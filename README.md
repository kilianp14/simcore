# Simcore

[![CI](https://github.com/kilianp14/simcore/actions/workflows/ci.yml/badge.svg)](https://github.com/kilianp14/simcore/actions/workflows/ci.yml)

A small deterministic discrete-event simulation kernel written in C11. It provides an explicit API for scheduling events on a logical timeline and executing their callbacks in a predictable order.

## Features

- **Deterministic ordering:** events execute by ascending simulation time, with event ID as the tie-breaker.
- **Flexible scheduling:** schedule at an absolute time or after a delay from the current time.
- **Cancellation:** cancel pending events by ID.
- **Bounded execution:** run until the queue is empty or until an inclusive simulation-time limit.
- **Callback-driven behavior:** callbacks can schedule or cancel events while a simulation is running.
- **Callback-context cleanup:** optionally register a cleanup callback for event data.

## Quick start

### Requirements

- C11-compatible compiler (Clang or GCC)
- CMake 3.31 or newer
- Ninja, used by the included CMake presets

### Build and run the tests

```sh
cmake --preset clang-debug
cmake --build --preset clang-debug
ctest --preset clang-debug
```

## API overview

| Function | Description |
| --- | --- |
| `sim_create` | Create a simulator with its clock initialized to `0`. |
| `sim_destroy` | Destroy the simulator and clean up pending event contexts. |
| `sim_now` | Read the current simulation time. |
| `sim_schedule_at` | Schedule an event at an absolute simulation time. |
| `sim_schedule_after` | Schedule an event after a delay from the current simulation time. |
| `sim_cancel` | Cancel a pending event by ID. |
| `sim_run` | Execute events until the queue is empty. |
| `sim_run_until` | Execute events whose times are less than or equal to a specified time. |
| `sim_status_string` | Get a human-readable description of a status code. |

See [`include/sim/sim.h`](include/sim/sim.h) for the full API contract.
