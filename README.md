# Electronic-Limit-Order-Book

C++ order-book and cache benchmarking project.

## Repository layout

- `include/` – public headers used across the matching engine, snapshot logic, and cache clients.
- `tests/` – executable validation programs for ladder, snapshot, and cache behavior.
- `benchmarks/` – benchmark harnesses for cache comparison workloads.
- `data/` – serialized dump artifacts produced during analysis runs.
- `docs/` – project notes and comparison docs.
- `build/` – generated CMake build outputs.

## Build

```bash
cmake -S . -B build
cmake --build build
```