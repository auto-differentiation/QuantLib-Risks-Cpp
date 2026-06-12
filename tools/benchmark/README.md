# Overhead Benchmarking

Measures the overhead of building QuantLib with `Real = xad::AReal<double>`
(AAD build, **no tape recording** — passive type overhead only) against a
plain `double` build, per test, using:

- the **full QuantLib test suite** (per-test timings via Boost.Test JUNIT logs), and
- the **QuantLib benchmark binary** (`quantlib-benchmark`, ~80 representative
  performance tests with built-in repetition).

Both configurations are built and run interleaved **on the same machine** so
that overhead ratios are meaningful despite machine noise. Repeated runs are
aggregated per test (median, mean, stddev) and the AAD/double ratio reported.

## Running via GitHub Actions

Trigger the **Benchmark** workflow (`workflow_dispatch` only):

```
gh workflow run benchmark.yaml \
  -f ql_branch=master -f xad_branch=main \
  -f suite_repeats=2 -f bench_repeats=3
```

Inputs: `ql_repo`/`ql_branch`/`xad_repo`/`xad_branch` (which sources to
benchmark), `suite_repeats` (default 2), `bench_repeats` (default 3),
`bench_size` (`quantlib-benchmark --size`, default `3`), `run_test`
(optional Boost.Test filter for cheap smoke runs, e.g.
`QuantLibTests/AmericanOptionTests`), `cxx_standard` (default 17).

Each matrix leg (linux-gcc, linux-clang, windows-msvc, macos-appleclang)
uploads a `benchmark-<platform>` artifact containing `results.csv`, the raw
Boost.Test XML logs, benchmark stdout, and `metadata.json` (CPU, compiler,
repo SHAs) — everything needed to compare runs across XAD versions. A
summary table is written to the job step summary.

Note: the first dispatch per platform builds with a cold compiler cache and
is the slow one; subsequent runs reuse the `bench-<platform>` ccache.

## Running locally

Build both configurations (only the two needed targets):

```
cmake -S QuantLib -B QuantLib/build-double -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DQLAAD_DISABLE_AAD=ON \
  -DQL_EXTERNAL_SUBDIRECTORIES="$PWD/xad;$PWD/QuantLibAAD" \
  -DQL_EXTRA_LINK_LIBRARIES=QuantLibAAD -DQL_NULL_AS_FUNCTIONS=ON
cmake --build QuantLib/build-double --target ql_test_suite ql_benchmark
# repeat with -B QuantLib/build-aad -DQLAAD_DISABLE_AAD=OFF
```

Then:

```
python3 QuantLibAAD/tools/benchmark/run_benchmarks.py \
  --build-double QuantLib/build-double --build-aad QuantLib/build-aad \
  --suite-repeats 2 --bench-repeats 3 --out results
python3 QuantLibAAD/tools/benchmark/aggregate.py results --summary results/summary.md
```

`results/results.csv` holds the full per-test data with `# key=value`
metadata header lines; `summary.md` holds the headline tables.

## Methodology notes

- Per-test suite timings come from Boost.Test's JUNIT logger
  (`--logger=...:JUNIT,...`) — no QuantLib code changes are needed.
- Runs are interleaved (double, aad, double, aad, ...) so machine drift over
  a multi-hour session affects both configurations equally.
- Tests whose double-build median is below `--min-time` (default 0.05 s) are
  excluded from ratio statistics: at millisecond resolution the ratio is
  dominated by timer noise. They remain in the CSV flagged
  `below_threshold=1`.
- Failed or skipped tests are excluded from timing statistics and counted in
  the summary; tests present in only one configuration are reported as
  `double_only` / `aad_only`.
- The headline `geomean overhead` is the geometric mean of per-test
  AAD/double median ratios — the right average for ratios.
