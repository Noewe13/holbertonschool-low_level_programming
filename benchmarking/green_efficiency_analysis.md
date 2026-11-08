# Green Efficiency Analysis

## Measurement Methodology

The program `instrumentation_lab.c` was instrumented with `clock()` from `<time.h>`. Four `clock_t` values are captured: one before `build_dataset()`, one after it, one after `process_dataset()`, and one after `reduce_checksum()`. Each phase is the difference between two consecutive timestamps, and `TOTAL` is the difference between the first and the last one. Every interval is converted to seconds by dividing by `CLOCKS_PER_SEC` and printed with `%.6f`.

These boundaries were chosen so that the phases are contiguous: the end of one phase is exactly the start of the next. Because of that, `TOTAL` always covers the three phases and is greater than or equal to each of them. The `printf` calls and the checksum test are kept outside the measured region, so output does not pollute the timings. The algorithms were not modified.

The program was compiled with `gcc -Wall -Werror -Wextra -pedantic -std=gnu89 -Wno-long-long` and run five times. It was also compiled with `-O2` and run three times for comparison.

## Observed Performance Differences

Recorded outputs without optimisation (5 runs):

| Run | TOTAL | BUILD_DATA | PROCESS | REDUCE |
|-----|-------|------------|---------|--------|
| 1 | 0.000445 | 0.000210 | 0.000159 | 0.000076 |
| 2 | 0.000485 | 0.000212 | 0.000191 | 0.000082 |
| 3 | 0.000485 | 0.000195 | 0.000207 | 0.000083 |
| 4 | 0.000435 | 0.000169 | 0.000181 | 0.000085 |
| 5 | 0.000360 | 0.000149 | 0.000135 | 0.000076 |

On average, `BUILD_DATA` took about 0.000187 s and `PROCESS` about 0.000175 s, while `REDUCE` took about 0.000080 s. Build and process together represent roughly 80% of the total. `BUILD_DATA` was usually the most expensive phase, but in run 3 `PROCESS` was slower, which shows that the two are close and that noise can change their order.

With `-O2`, `TOTAL` dropped to 0.000278–0.000306 s. `PROCESS` fell the most, from around 0.000175 s to between 0.000071 and 0.000083 s. `REDUCE` stayed near 0.000076–0.000100 s, which suggests that its sequential dependency (each sum depends on the previous one) limits what the compiler can speed up.

## Relation Between Runtime and Energy Consumption

`clock()` measures CPU time consumed by the process. For a single-threaded program like this one, energy used by the CPU is roughly power multiplied by the time the CPU is busy. Under that assumption, the `-O2` build, which used about 35% less CPU time (about 0.000291 s versus 0.000442 s on average), would consume proportionally less CPU energy for the same result. No energy meter was used, so this is an estimate based on CPU time, not a direct energy measurement.

## Limitations of the Experiment

- The durations are below one millisecond, close to the resolution of `clock()`. Small differences between phases are therefore uncertain.
- Run-to-run variation is significant: `TOTAL` ranged from 0.000360 to 0.000485 s on the same binary.
- Only five and three runs were recorded, which is not enough for strong statistics.
- `clock()` reports CPU time, not wall-clock time, and the experiment did not measure real power or energy.
- Results depend on the machine, compiler version and system load at the time of the runs.

## Practical Engineering Takeaway

Instrumenting each phase separately shows where time is actually spent instead of guessing. Here, data generation and processing dominate, so they are the first places to optimise. The `-O2` results also show that compiler flags can reduce CPU time, and likely energy, without touching the algorithm. Because the measured times are so small, a reliable conclusion needs more runs or a larger dataset before making engineering decisions.
