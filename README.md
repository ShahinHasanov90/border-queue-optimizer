# Border Queue Optimizer

![C11](https://img.shields.io/badge/C-C11-blue.svg)
![CMake](https://img.shields.io/badge/build-CMake-064F8C.svg)
![License](https://img.shields.io/badge/license-Apache%202.0-green.svg)

Queue management and optimization engine for customs border crossings. Discrete event simulation of vehicle arrivals, inspection queues, and processing lanes. Optimizes lane allocation, predicts wait times, and recommends staffing levels based on historical patterns and real-time queue state.

---

## Features

- **Discrete Event Simulation (DES)** -- Simulates vehicle arrivals, queue formation, and lane processing with configurable time-varying arrival rates, rush-hour patterns, and seasonal adjustments.
- **Queue Theory Models (M/M/c)** -- Analytical M/M/1 and M/M/c implementations with Erlang-C formula for wait probability, Little's Law, and tail probability calculations.
- **Wait Time Prediction** -- Exponential smoothing on historical data with 15-minute time-of-day granularity and confidence intervals.
- **Lane Allocation Optimizer** -- Finds optimal lane count to minimize total cost (staffing + waiting) subject to utilization and wait-time constraints.
- **Arrival Pattern Generator** -- Non-homogeneous Poisson process via Lewis-Shedler thinning with rush-hour peaks, seasonal factors, and weekend adjustments.
- **Statistics Collector** -- Online mean, variance (Welford's algorithm), percentiles via sorted reservoir sampling, and utilization tracking.

## Architecture

```
src/
  main.c           CLI entry point (simulate, optimize, predict)
  simulator.c/h    Discrete event simulation engine
  queue_model.c/h  M/M/1, M/M/c, Erlang-C, Little's Law
  optimizer.c/h    Lane allocation cost optimizer
  predictor.c/h    Exponential smoothing wait-time predictor
  arrival.c/h      NHPP arrival pattern generator
  heap.c/h         Min-heap priority queue for event scheduling
  stats.c/h        Online statistics (Welford, reservoir percentiles)
include/
  common.h         Shared types, constants, error codes
tests/
  test_main.c      Test runner with assertion macros
  test_queue_model.c   11 tests for queue theory models
  test_heap.c          7 tests for min-heap
  test_simulator.c     7 tests for DES engine
  test_optimizer.c     6 tests for lane optimizer
config/
  simulation.conf  Default simulation parameters
```

## Build

### CMake

```bash
mkdir build && cd build
cmake ..
make
```

### Make

```bash
make            # build the CLI binary
make test       # build and run the test suite
make clean      # remove build artifacts
```

### Requirements

- C11-compliant compiler (GCC, Clang, MSVC)
- CMake 3.14+ (optional, Makefile also provided)
- POSIX math library (`-lm`)

## Usage

### Run a simulation

```bash
./build/border-queue-optimizer simulate \
    --lanes 4 \
    --duration 28800 \
    --rate 120 \
    --service-rate 40
```

### Optimize lane allocation

```bash
./build/border-queue-optimizer optimize \
    --rate 120 \
    --service-rate 40 \
    --max-lanes 16 \
    --max-wait 300
```

### Predict wait time

```bash
./build/border-queue-optimizer predict \
    --hour 8 \
    --minute 30
```

## Performance

| Metric | Value |
|--------|-------|
| 8-hour simulation (4 lanes, 120 veh/hr) | < 50 ms |
| Heap push/pop | O(log n) |
| Statistics update | O(1) amortized |
| M/M/c analytical solve | O(c) |
| Optimizer search | O(max_lanes * c) |
| Memory footprint | < 2 MB typical |

All core data structures use stack or arena allocation where possible. The heap grows dynamically but only when capacity is exceeded. The statistics reservoir uses a fixed-size sorted array for approximate percentile computation.

## Queue Theory Reference

The M/M/c model assumes:
- **M**: Poisson arrivals (memoryless inter-arrival times)
- **M**: Exponential service times (memoryless)
- **c**: Number of parallel identical servers

Key formulas implemented:
- **Utilization**: `rho = lambda / (c * mu)`
- **Erlang-C**: Probability an arriving customer must wait
- **Little's Law**: `L = lambda * W`
- **Average wait**: `Wq = C(c, a) * rho / (c * mu * (1 - rho)^2)` simplified via `Wq = Lq / lambda`

## License

Copyright 2022 Shahin Hasanov. Licensed under the [Apache License 2.0](LICENSE).
