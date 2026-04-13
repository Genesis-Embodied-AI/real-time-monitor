# File structure overview
```
+------------------------------------------------------------+
|                         HEADER                             |
+------------------------------------------------------------+
|                          DATA                              |
+------------------------------------------------------------+
|                   DATA_STREAM_END sentinel                 |
+------------------------------------------------------------+
|                    METADATA FOOTER                         |
+------------------------------------------------------------+

File.tick
│
├─ Header
│ ├─ header_major = 2, header_minor = 0
│ ├─ ...
│ ├─ source_name = "worker_01"
│ └─ metadata_footer_offset (0 if no metadata)
│
├─ Data (at data_offset, aligned to 8 bytes)
│ ├─ data_version = 1
│ ├─ control event: update period → u64 new_period_ns
│ ├─ control event: update priority → u32 new_priority
│ ├─ control event: update reference → u64 new_reference_ns
│ ├─ timestamp delta 1 (u32)
│ ├─ timestamp delta 2 (u32)
│ └─ ...
│
├─ DATA_STREAM_END sentinel (0x80000010)
│
└─ Metadata footer (at metadata_footer_offset)
  ├─ entry_count
  ├─ key: DISPLAY_NAME → "My custom name"
  └─ ...
```

## Header section

### Version 1.0 (legacy)

Legacy format. The monitor automatically migrates 1.0 files to 2.0 on first load.

| Offset | Size (bytes) | Type | Field Name | Description |
|:-------|:--------------|:------|:------------|:-------------|
| 0x0000 | 2  | `u16`  | **header_version** | Header format version (= 1) |
| 0x0002 | 6  | — | **padding** | Reserved / alignment |
| 0x0008 | 8  | `u64` | **data_offset** | Offset to data section (aligned on 8B) |
| 0x0010 | 16 | `bytes[16]` | **dataset_uuid** | Unique dataset identifier (UUID) |
| 0x0020 | 8  | `u64` | **process_start_timestamp** | Process start time since epoch |
| 0x0028 | 2  | `u16` | **Process name size** | Process name length (no '\0') |
| 0x002A | N₁ | `char[N₁]` | **Process name** | ASCII string |
| ...    | 2  | `u16` | **Source name size** | Thread name length (no '\0') |
| ...    | N₂ | `char[N₂]` | **Source name** | ASCII string |
| ...    | —  | — | (padding) | Align to 8-byte boundary before data section |

### Version 2.0

Major version 2 adds:
- A **`metadata_footer_offset`** pointer in the header (after the strings)
- A **`DATA_STREAM_END`** sentinel command that terminates the data stream
- A **metadata footer** after the sentinel

This is a **breaking change**: v1 parsers cannot correctly read v2 files.

| Offset | Size (bytes) | Type | Field Name | Description |
|:-------|:--------------|:------|:------------|:-------------|
| 0x0000 | 2  | `u16`  | **header_major** | Header format major version (= 2) |
| 0x0002 | 2  | `u16`  | **header_minor** | Header format minor version (= 0) |
| 0x0004 | 4  | — | **padding** | Reserved / alignment |
| 0x0008 | 8  | `u64` | **data_offset** | Offset to data section (aligned on 8B) |
| 0x0010 | 16 | `bytes[16]` | **dataset_uuid** | Unique dataset identifier (UUID) |
| 0x0020 | 8  | `u64` | **process_start_timestamp** | Process start time since epoch |
| 0x0028 | 2  | `u16` | **Process name size** | Process name length (no '\0') |
| 0x002A | N₁ | `char[N₁]` | **Process name** | ASCII string |
| ...    | 2  | `u16` | **Source name size** | Thread name length (no '\0') |
| ...    | N₂ | `char[N₂]` | **Source name** | ASCII string |
| ...    | 8  | `i64` | **metadata_footer_offset** | Byte offset to metadata footer (0 = none) |
| ...    | —  | — | (padding) | Align to 8-byte boundary before data section |

```
┌───────────────────────────────────────────────────────────────┐
│ HEADER                                                        │
│ ┌──────────┬──────────┬───────────┬────────────────────────┐ │
│ │ u16      │ u16      │ 4B padding│ u64 data_offset        │ │
│ │ major    │ minor    │           │ (aligned to 8 bytes)   │ │
│ └──────────┴──────────┴───────────┴────────────────────────┘ │
│ ┌───────────────────────────────────────────────────────────┐ │
│ │ UUID (16B)                                                │ │
│ └───────────────────────────────────────────────────────────┘ │
│ ┌──────────────────────────────┬────────────────────────────┐ │
│ │ u64 start_timestamp          │ u16 process_name_size      │ │
│ └──────────────────────────────┴────────────────────────────┘ │
│ ┌───────────────────────────────────────────────────────────┐ │
│ │ ASCII process_name (N₁ bytes)                             │ │
│ ├───────────────────────────────────────────────────────────┤ │
│ │ u16 source_name_size                                      │ │
│ ├───────────────────────────────────────────────────────────┤ │
│ │ ASCII source_name (N₂ bytes)                              │ │
│ ├───────────────────────────────────────────────────────────┤ │
│ │ i64 metadata_footer_offset   (major >= 2 only)             │ │
│ └───────────────────────────────────────────────────────────┘ │
│ (Padding → align to 8B)                                       │
└───────────────────────────────────────────────────────────────┘
```
## Data section


| Offset (relative to `data_offset`) | Size | Type | Field | Description |
|:----------------------------------|:------|:------|:--------|:-------------|
| 0x00 | 2 | `u16` | **data_version** | Data format version |
| 0x02 | 6 | — | **padding** | Reserved |
| 0x08 | variable | `u32[]` | **timestamps / events** | Series of timestamp deltas or control events

### Timestamp series

Each entry represents time delta or control event:
```
┌──────────────────────────────────────┐
│ Bit 31 │ Bits 30–0                   │
├────────┼─────────────────────────────┤
│  1 bit │ 31 bits                     │
│  Flag  │ Δ time in ns or OOB type    │
└──────────────────────────────────────┘
```

| Bit 31 | Meaning           | Description                                        |
| :----- | :---------------- | :------------------------------------------------- |
| 0      | Regular timestamp | Bits [30:0] = delta time (ns) since last reference |
| 1      | Control event     | Bits [30:0] = type of control  (see below)         |

Their shall be two timestamps per loop call.
Note: an update reference control replace a timestamp.

### Control event message format

| Control Type (`u32` value) | Followed by | Description |
|:------------------------|:-------------|:-------------|
| `0x00000001` | `u64 new_period_ns` | Update task period |
| `0x00000002` | `u32 new_priority` | Update task priority |
| `0x00000004` | `u64 new_reference_ns` | Update reference point (ns since epoch)|
| `0x00000008` | `u64 threshold_ns` | Set blackbox threshold (0 = disabled) |
| `0x00000010` | *(nothing)* | End of data stream (sentinel) |

#### Example — Update Period (`0x00000001`)
```
┌───────────────────────────────┐
│ 0x80000001 (flag + OOB type)  │ ← u32 (bit31=1)
├───────────────────────────────┤
│ 0x000000012A05F200            │ ← u64 (new_period_ns)
└───────────────────────────────┘
```

#### Example — Update Priority (`0x00000002`)
```
┌───────────────────────────────┐
│ 0x80000002 (flag + OOB type)  │ ← u32 (bit31=1)
├───────────────────────────────┤
│ 0x00000005                    │ ← u32 (new_priority)
└───────────────────────────────┘
```

#### Example — Update Reference (`0x00000004`)
```
┌───────────────────────────────┐
│ 0x80000004 (flag + OOB type)  │ ← u32 (bit31=1)
├───────────────────────────────┤
│ 0x1875be46ebe41a00            │ ← u64 (new_reference_ns)
└───────────────────────────────┘
```
#### Example — Set Threshold (`0x00000008`)
```
┌───────────────────────────────┐
│ 0x80000008 (flag + OOB type)  │ ← u32 (bit31=1)
├───────────────────────────────┤
│ 0x00000000004C4B40            │ ← u64 (threshold_ns = 5ms)
└───────────────────────────────┘
```
When set to a non-zero value, the recorder enters blackbox mode for this probe:
it maintains a rolling buffer and only writes to disk when the time diff between
consecutive loop starts exceeds the threshold. A value of 0 disables blackbox mode.

#### Data Stream End (`0x00000010`)
```
┌───────────────────────────────┐
│ 0x80000010 (flag + OOB type)  │ ← u32 (bit31=1), no payload
└───────────────────────────────┘
```
Marks the end of the sample data. The parser stops reading samples when it
encounters this sentinel. Any bytes after it belong to the metadata footer.

The sentinel may appear more than once (e.g. the probe writes one on shutdown,
and the recorder appends another). The parser stops at the first occurrence;
duplicates are harmless.

If the sentinel is missing (e.g. power failure), the parser falls back to
reading until EOF, same as v1 files. Saving metadata requires the sentinel;
the monitor prompts the user to repair the file (append the sentinel) before
proceeding.

At the start of the data section, there must always be three OOB messages:
update period    (0x00000001) → u64 new_period
update priority  (0x00000002) → u32 new_priority
update reference (0x00000004) → u64 new_reference_time

An optional fourth message may follow:
set threshold    (0x00000008) → u64 threshold_ns

Their order does not matter, but all must appear before any normal timestamp deltas.

## Metadata footer

The metadata footer sits after the `DATA_STREAM_END` sentinel, at the byte offset
stored in `metadata_footer_offset` in the header. If `metadata_footer_offset` is 0,
no metadata has been written yet.

The footer starts with:

| Field | Type | Description |
|:------|:-----|:------------|
| **entry_count** | `u32` | Number of metadata entries that follow |

Followed by `entry_count` entries, concatenated back-to-back.
Each entry is: `key_id` (`u16`) | `payload_size` (`u32`) | `payload` (`payload_size` bytes).
The `payload_size` field allows skipping unknown keys.

Well-known keys:

| key_id | Name | Payload | Description |
|:-------|:-----|:--------|:------------|
| 1 | `DISPLAY_NAME` | UTF-8 string (no NUL) | Custom display name for the curve |
| 2 | `DEFAULT_VISIBILITY` | `u8` (0 or 1) | Initial show/hide state |
| 3 | `DISPLAY_WEIGHT` | `i32` | Sort weight (heavier values sink to the end) |
| 0xFFFF | `USER_INFO` | see below | Generic user metadata *(future)* |

Unknown keys are skipped using `payload_size`.

### USER_INFO payload (key_id = 0xFFFF)

A generic typed key-value pair for arbitrary user metadata (notes, tags,
calibration data, etc.). Multiple `USER_INFO` entries can coexist in the
same footer, each with a different user key.

The payload is: `value_type` (`u8`) | `key` (`u16` length + UTF-8 bytes) | `value` (type-dependent).

| value_type | Name | Value encoding |
|:-----------|:-----|:---------------|
| 0 | string | `u16` length + UTF-8 bytes (no NUL) |
| 1 | int32 | `i32` (4 bytes, little-endian) |
| 2 | float32 | `f32` (4 bytes, IEEE 754 LE) |
| 3 | bool | `u8` (0 or 1) |

Example: a `USER_INFO` entry with key `"sensor"`, value `"left_arm"` (string):
```
key_id       = 0xFFFF          (u16)
payload_size = 1 + 2 + 6 + 2 + 8 = 19   (u32)
─── payload ───
value_type   = 0               (u8, string)
key_length   = 6               (u16)
key          = "sensor"        (6 bytes)
value_length = 8               (u16)
value        = "left_arm"      (8 bytes)
```

```
┌───────────────────────────────────────────────────────────────┐
│ DATA STREAM                                                    │
│ ┌───────────────────────────────────────────────────────────┐ │
│ │ ... samples and control events ...                        │ │
│ └───────────────────────────────────────────────────────────┘ │
│ ┌───────────────────────────────────────────────────────────┐ │
│ │ 0x80000010  DATA_STREAM_END sentinel                      │ │
│ └───────────────────────────────────────────────────────────┘ │
├─────────────────── metadata_footer_offset ───────────────────┤
│ METADATA FOOTER                                                │
│ ┌───────────────────────────────────────────────────────────┐ │
│ │ u32 entry_count                                           │ │
│ ├───────────────────────────────────────────────────────────┤ │
│ │ Metadata entries (key_id + payload_size + payload) × N     │ │
│ └───────────────────────────────────────────────────────────┘ │
└───────────────────────────────────────────────────────────────┘
```
