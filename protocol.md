# File structure overview
```
+------------------------------------------------------------+
|                         HEADER                             |
+------------------------------------------------------------+
|                          DATA                              |
+------------------------------------------------------------+

File.tick
│
├─ Header
│ ├─ header_version = 1
│ ├─ ...
│ └─ source_name = "worker_01"
│
└─ Data (aligned to 8 bytes)
├─ data_version = 1
├─ control event: update period → u64 new_period_ns
├─ control event: update priority → u32 new_priority
├─ control event: update reference → u64 new_reference_ns
├─ timestamp delta 1 (u32)
├─ timestamp delta 2 (u32)
└─ ...
```

## Header section

| Offset | Size (bytes) | Type | Field Name | Description |
|:-------|:--------------|:------|:------------|:-------------|
| 0x0000 | 2  | `u16`  | **header_version** | Header format version |
| 0x0002 | 6  | — | **padding** | Reserved / alignment |
| 0x0008 | 8  | `u64` | **data_offset** | Offset to data section (aligned on 8B) |
| 0x0010 | 16 | `bytes[16]` | **dataset_uuid** | Unique dataset identifier (UUID) |
| 0x0020 | 8  | `u64` | **process_start_timestamp** | Process start time since epoch |
| 0x0028 | 2  | `u16` | **Process name size** | Process name length (no '\0') |
| 0x002A | N₁ | `char[N₁]` | **Process name** | ASCII string |
| ...    | 2  | `u16` | **Source name size** | Thread name length (no '\0') |
| ...    | N₂ | `char[N₂]` | **Source name** | ASCII string |
| ...    | —  | — | (padding) | Align to 8-byte boundary before data section |


```
┌───────────────────────────────────────────────────────────────┐
│ HEADER                                                        │
│ ┌────────────┬────────────┬───────────────────────────────┐   │
│ │ u16        │ 6B padding │ u64 data_offset               │   │
│ │ version    │            │ (aligned to 8 bytes)          │   │
│ └────────────┴────────────┴───────────────────────────────┘   │
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
At the start of the data section, there must always be three OOB messages:
update period    (0x00000001) → u64 new_period
update priority  (0x00000002) → u32 new_priority
update reference (0x00000004) → u64 new_reference_time

Their order does not matter, but all must appear before any normal timestamp deltas.
