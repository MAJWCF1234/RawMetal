# Reactor service-bay revision — local verification

Release build: 18,587,648 bytes, below the 19,800,000-byte limit.
The root ZIP contains the same executable. No new GitHub release was published.

Passed: reactor disk/computer/valve interaction tests; wrong-order recovery;
chunk persistence; restart; equipment collision on both floors; fixed lamp
mounting; lift boarding/stairs/exit route; 30/60/120 Hz passenger containment;
audio mix transitions; developer console; full software and Vulkan smoke suites;
Vulkan depth, clipping, emission dimming and parallel-animation checks.
Vulkan synchronization validation produced no output/errors.

Visual inspection covered the cab, power-loss phase, core, maintenance disk,
computer, pump bay and return station. Corrected a workbench above eye height,
upper-wall grating misuse, oversized support UVs and occluded equipment labels.

Latest full-resolution 640×360 Vulkan benchmark on Intel Graphics: 1,560 measured
frames, zero above 50 ms, all six culling/reference comparisons identical.
Lift/reactor scene averages were 13.9–16.0 ms; worst measured frame was 25.6 ms.
First-frame initialization is reported separately by the benchmark.

Two earlier repeat runs each recorded one 66–74 ms rendering hitch during the
fall. A later instrumented run did not reproduce it; its cause is not established.
This is not a guarantee of minimum FPS on every run or device. Slow frames now
record the lift phase, height, CPU scene work and GPU submission/readback time.
The earlier report is retained at diagnostics/reactor-performance-hitch.txt.

Play: run RawMetal.exe, open the backtick console, use `map lift` for the full
sequence or `map reactor` for the puzzle. E interacts and closes terminal logs.
