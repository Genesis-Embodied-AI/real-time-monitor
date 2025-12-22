import time
from real_time_monitor import Probe

p = Probe()
p.init(output_file="test.tick", process="python", task="my_task", period_ms=1, priority=42)

TARGET = 0.001  # 1 ms

for i in range(10000):
    p.log_start()
    start = time.perf_counter()

    # ---- dummy computation ----
    x = i * i

    # ---- adjust to hit 1 ms ----
    elapsed = time.perf_counter() - start
    remaining = TARGET - elapsed
    p.log_end()

    if remaining > 0:
        time.sleep(remaining)

    # print outside computation to not alter the timing
    print(f"Iteration {i}, x={x}, actual={time.perf_counter() - start:.6f}s")

