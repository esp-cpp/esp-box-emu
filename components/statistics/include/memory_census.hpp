#pragma once

#include <cstddef>

/// Snapshot of the heap state that matters for the emulators: how much internal
/// RAM / PSRAM is free and, more importantly, the largest contiguous block of
/// each (the cores allocate a few large hot buffers and fall back to PSRAM when
/// the internal block is too small -- which is where most "it got slower"
/// regressions come from).
struct MemSnapshot {
  size_t internal_free{0};
  size_t internal_largest{0};
  size_t internal_min_free{0}; ///< low-water mark since boot
  size_t psram_free{0};
  size_t psram_largest{0};
  size_t dma_free{0};
  size_t dma_largest{0};
};

/// Take a snapshot of the current heap state.
MemSnapshot mem_snapshot();

/// Print a single greppable `[mem] phase=<phase> ...` line describing the
/// current heap state, plus a `[mem] delta ...` line against \p baseline when
/// one is given (free/largest deltas for internal RAM and PSRAM). The phase
/// names are what tools/perf_report.py keys on, so keep them stable:
/// boot, menu, launch, quit.
/// \param phase short phase name (no spaces)
/// \param baseline optional earlier snapshot to diff against
/// \return the snapshot that was printed
MemSnapshot print_mem_census(const char *phase, const MemSnapshot *baseline = nullptr);
