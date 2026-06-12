#!/usr/bin/env python3
##############################################################################
#
#  Aggregate per-test timings comparing the AAD build (Real = xad::AReal<double>,
#  no tape recording) against the plain double build of QuantLib.
#
#  Inputs (produced by run_benchmarks.py in the results directory):
#    suite-<config>-<i>.xml   Boost.Test logs (JUNIT or Boost XML format)
#    bench-<config>-<i>.txt   quantlib-benchmark --verbose=2 stdout
#    metadata.json            platform / SHA / settings metadata
#    manifest.json            wall times and exit codes per run
#
#  Outputs:
#    results.csv              full per-test data with metadata header lines
#    markdown summary         appended to the file given via --summary
#                             (e.g. $GITHUB_STEP_SUMMARY)
#
#  Copyright (C) 2010-2026 Xcelerit Computing Limited
#  SPDX-License-Identifier: AGPL-3.0-or-later
#
##############################################################################

import argparse
import csv
import glob
import json
import math
import os
import re
import statistics
import sys
import xml.etree.ElementTree as ET

CONFIGS = ("double", "aad")

ANSI_RE = re.compile(r"\x1b\[[0-9;]*m")
BENCH_LINE_RE = re.compile(r"^(\S+)\s*:\s*([0-9]*\.?[0-9]+(?:[eE][+-]?[0-9]+)?)s\s*$")
BENCH_HEADER = "Total Runtime spent in each test"
THROUGHPUT_RE = re.compile(r"System Throughput\s*=\s*([0-9.eE+-]+)\s*tasks/s")
RUNTIME_RE = re.compile(r"Benchmark Runtime\s*=\s*([0-9.eE+-]+)s")


# ----------------------------------------------------------------------------
# Parsers
# ----------------------------------------------------------------------------

def parse_suite_xml(path):
    """Parse a Boost.Test log file into {test_key: {'time': sec, 'status': str}}.

    Auto-detects JUNIT format (<testsuite> root, time attribute in seconds)
    and Boost XML format (<TestLog> root, <TestingTime> in microseconds).
    Duplicate test keys are disambiguated with an occurrence suffix '#N'.
    """
    tree = ET.parse(path)
    root = tree.getroot()
    results = {}
    counts = {}

    def add(key, time_sec, status):
        n = counts.get(key, 0)
        counts[key] = n + 1
        if n:
            key = "%s#%d" % (key, n)
        results[key] = {"time": time_sec, "status": status}

    if root.tag in ("testsuite", "testsuites"):
        for tc in root.iter("testcase"):
            cls = (tc.get("classname") or "").strip()
            name = (tc.get("name") or "").strip()
            key = "%s/%s" % (cls, name) if cls else name
            status = "passed"
            for child in tc:
                tag = child.tag.lower()
                if tag in ("failure", "error"):
                    status = "failed"
                    break
                if tag == "skipped":
                    status = "skipped"
            add(key, float(tc.get("time") or 0.0), status)
    elif root.tag == "TestLog":
        def walk(node, prefix):
            for child in node:
                if child.tag == "TestSuite":
                    name = child.get("name") or ""
                    if name == "Master Test Suite":
                        walk(child, prefix)
                    else:
                        walk(child, prefix + "." + name if prefix else name)
                elif child.tag == "TestCase":
                    name = child.get("name") or ""
                    key = "%s/%s" % (prefix, name) if prefix else name
                    tt = child.find("TestingTime")
                    time_sec = float(tt.text) / 1e6 if tt is not None and tt.text else 0.0
                    status = "failed" if child.find("Error") is not None else "passed"
                    add(key, time_sec, status)
        walk(root, "")
    else:
        raise ValueError("%s: unrecognised root element <%s>" % (path, root.tag))
    return results


def parse_bench_txt(path):
    """Parse quantlib-benchmark --verbose=2 stdout.

    Returns (per_test: {name: seconds}, throughput_tasks_per_s, runtime_s).
    Per-test lines only appear after the 'Total Runtime spent in each test'
    header, in the form '<Fixture>/<testName>   : <float>s'.
    """
    per_test = {}
    throughput = None
    runtime = None
    in_table = False
    with open(path, encoding="utf-8", errors="replace") as fh:
        for line in fh:
            line = ANSI_RE.sub("", line).rstrip()
            m = THROUGHPUT_RE.search(line)
            if m:
                throughput = float(m.group(1))
                continue
            m = RUNTIME_RE.search(line)
            if m:
                runtime = float(m.group(1))
                continue
            if BENCH_HEADER in line:
                in_table = True
                continue
            if in_table:
                m = BENCH_LINE_RE.match(line.strip())
                if m:
                    per_test[m.group(1)] = float(m.group(2))
    return per_test, throughput, runtime


# ----------------------------------------------------------------------------
# Loading and statistics
# ----------------------------------------------------------------------------

def load_results(results_dir):
    """Load all run files. Returns (suite_runs, bench_runs, metadata, manifest)
    where suite_runs[config] is a list of per-run dicts and bench_runs[config]
    is a list of (per_test, throughput, runtime) tuples.
    """
    suite_runs = {c: [] for c in CONFIGS}
    bench_runs = {c: [] for c in CONFIGS}
    for config in CONFIGS:
        for path in sorted(glob.glob(os.path.join(results_dir, "suite-%s-*.xml" % config))):
            try:
                suite_runs[config].append(parse_suite_xml(path))
            except ET.ParseError as exc:
                print("warning: skipping unparseable %s: %s" % (path, exc), file=sys.stderr)
        for path in sorted(glob.glob(os.path.join(results_dir, "bench-%s-*.txt" % config))):
            bench_runs[config].append(parse_bench_txt(path))

    metadata = {}
    meta_path = os.path.join(results_dir, "metadata.json")
    if os.path.exists(meta_path):
        with open(meta_path, encoding="utf-8") as fh:
            metadata = json.load(fh)
    manifest = {}
    manifest_path = os.path.join(results_dir, "manifest.json")
    if os.path.exists(manifest_path):
        with open(manifest_path, encoding="utf-8") as fh:
            manifest = json.load(fh)
    return suite_runs, bench_runs, metadata, manifest


def per_test_stats(runs):
    """Aggregate a list of per-run dicts into {key: stats}."""
    keys = set()
    for run in runs:
        keys.update(run)
    out = {}
    for key in keys:
        times = [r[key]["time"] for r in runs if key in r and r[key]["status"] == "passed"]
        statuses = [r[key]["status"] for r in runs if key in r]
        status = "passed"
        if any(s == "failed" for s in statuses):
            status = "failed"
        elif all(s == "skipped" for s in statuses):
            status = "skipped"
        out[key] = {
            "median": statistics.median(times) if times else None,
            "mean": statistics.fmean(times) if times else None,
            "stddev": statistics.stdev(times) if len(times) > 1 else 0.0,
            "min": min(times) if times else None,
            "n": len(times),
            "status": status,
        }
    return out


def join_stats(double_stats, aad_stats, min_time, source):
    """Full outer join of the two configs into a list of row dicts."""
    rows = []
    for key in sorted(set(double_stats) | set(aad_stats)):
        d = double_stats.get(key)
        a = aad_stats.get(key)
        ratio = None
        if d and a and d["median"] and a["median"] is not None and d["median"] > 0:
            ratio = a["median"] / d["median"]
        rows.append({
            "test": key,
            "source": source,
            "double": d,
            "aad": a,
            "ratio": ratio,
            "below_threshold": bool(d and d["median"] is not None and d["median"] < min_time),
        })
    return rows


def ratio_sample(rows):
    """Ratios eligible for aggregate statistics: both configs passed and the
    double-side median is above the noise threshold."""
    return [
        r["ratio"] for r in rows
        if r["ratio"] is not None and not r["below_threshold"]
        and r["double"]["status"] == "passed"
        and r["aad"] and r["aad"]["status"] == "passed"
    ]


def geomean(values):
    if not values:
        return None
    return math.exp(sum(math.log(v) for v in values) / len(values))


def percentile(sorted_values, p):
    if not sorted_values:
        return None
    idx = (len(sorted_values) - 1) * p
    lo, hi = int(math.floor(idx)), int(math.ceil(idx))
    if lo == hi:
        return sorted_values[lo]
    frac = idx - lo
    return sorted_values[lo] * (1 - frac) + sorted_values[hi] * frac


# ----------------------------------------------------------------------------
# Output
# ----------------------------------------------------------------------------

CSV_FIELDS = [
    "test", "source",
    "double_median_s", "double_mean_s", "double_stddev_s", "double_runs", "double_status",
    "aad_median_s", "aad_mean_s", "aad_stddev_s", "aad_runs", "aad_status",
    "ratio", "below_threshold",
]


def fmt(value, digits=6):
    if value is None:
        return ""
    return ("%." + str(digits) + "g") % value


def write_csv(rows, metadata, path):
    with open(path, "w", newline="", encoding="utf-8") as fh:
        for key in sorted(metadata):
            fh.write("# %s=%s\n" % (key, metadata[key]))
        writer = csv.writer(fh)
        writer.writerow(CSV_FIELDS)
        for r in rows:
            d = r["double"] or {}
            a = r["aad"] or {}
            writer.writerow([
                r["test"], r["source"],
                fmt(d.get("median")), fmt(d.get("mean")), fmt(d.get("stddev")),
                d.get("n", ""), d.get("status", ""),
                fmt(a.get("median")), fmt(a.get("mean")), fmt(a.get("stddev")),
                a.get("n", ""), a.get("status", ""),
                fmt(r["ratio"], 4), int(r["below_threshold"]),
            ])


def md_row(cells):
    return "| " + " | ".join(str(c) for c in cells) + " |"


def pct(ratio):
    """Format a ratio as overhead percent, bold when extreme (>= 6x)."""
    if ratio is None:
        return "n/a"
    text = "+%.0f%%" % ((ratio - 1) * 100)
    return "**%s**" % text if ratio >= 6.0 else text


def write_summary(suite_rows, bench_rows, bench_runs, metadata, manifest, path, top_n, min_time):
    lines = []
    platform = metadata.get("platform", "benchmark")
    compiler = metadata.get("compiler", "unknown compiler")
    cpu = metadata.get("cpu", "unknown CPU")
    lines.append("## %s — %s (%s)" % (platform, compiler, cpu))
    lines.append("")

    # Overall headline as overhead percentages
    suite_sample = sorted(ratio_sample(suite_rows))
    bench_sample = sorted(ratio_sample(bench_rows))
    parts = []
    walls = manifest.get("suite_wall_s", {})
    if walls.get("double") and walls.get("aad"):
        wd = statistics.median(walls["double"])
        wa = statistics.median(walls["aad"])
        if wd:
            parts.append("suite wall %s" % pct(wa / wd))
    bwalls = manifest.get("bench_wall_s", {})
    if bwalls.get("double") and bwalls.get("aad"):
        bd = statistics.median(bwalls["double"])
        ba = statistics.median(bwalls["aad"])
        if bd:
            parts.append("benchmark wall %s" % pct(ba / bd))
    gm = geomean(suite_sample)
    gm_bench = geomean(bench_sample)
    geo = []
    if gm is not None:
        geo.append("suite %s" % pct(gm))
    if gm_bench is not None:
        geo.append("bench %s" % pct(gm_bench))
    if geo:
        parts.append("per-test geomean: " + ", ".join(geo))
    if parts:
        lines.append("**Overall: " + " · ".join(parts) + "**")
        lines.append("")

    # Combined per-test table: top suite rows then top bench rows, by overhead
    def eligible(rows):
        out = [r for r in rows if r["ratio"] is not None and not r["below_threshold"]]
        out.sort(key=lambda r: r["ratio"], reverse=True)
        return out

    lines.append(md_row(["test", "double (s)", "AAD (s)", "overhead"]))
    lines.append(md_row(["---"] * 4))
    for r in eligible(suite_rows)[:top_n]:
        name = r["test"].split("/")[-1]
        lines.append(md_row(["suite: " + name,
                             "%.3f" % r["double"]["median"],
                             "%.3f" % r["aad"]["median"],
                             pct(r["ratio"])]))
    for r in eligible(bench_rows)[:top_n]:
        lines.append(md_row(["bench: " + r["test"],
                             "%.2f" % r["double"]["median"],
                             "%.2f" % r["aad"]["median"],
                             pct(r["ratio"])]))
    lines.append("")

    # Distribution of suite overhead
    if suite_sample:
        lines.append(md_row(["suite overhead", "p10", "p25", "median", "p75", "p90"]))
        lines.append(md_row(["---"] * 6))
        lines.append(md_row(["distribution"] +
                            [pct(percentile(suite_sample, p))
                             for p in (0.10, 0.25, 0.50, 0.75, 0.90)]))
        lines.append("")

    # Counts and full metadata, collapsed
    n_both = sum(1 for r in suite_rows if r["double"] and r["aad"])
    n_double_only = sum(1 for r in suite_rows if r["double"] and not r["aad"])
    n_aad_only = sum(1 for r in suite_rows if r["aad"] and not r["double"])
    n_failed = sum(1 for r in suite_rows
                   if (r["double"] and r["double"]["status"] == "failed")
                   or (r["aad"] and r["aad"]["status"] == "failed"))
    n_below = sum(1 for r in suite_rows if r["below_threshold"])
    lines.append("<details><summary>Details: %d suite tests in both configs, "
                 "%d failed, %d below %.3gs threshold; full metadata</summary>"
                 % (n_both, n_failed, n_below, min_time))
    lines.append("")
    lines.append("Suite tests: %d in both configs, %d double-only, %d aad-only, "
                 "%d failed, %d below threshold (excluded from geomean). "
                 "Full per-test data in the results.csv artifact."
                 % (n_both, n_double_only, n_aad_only, n_failed, n_below))
    lines.append("")
    lines.append("| | |")
    lines.append("|---|---|")
    for key in sorted(metadata):
        lines.append(md_row([key, metadata[key]]))
    lines.append("")
    lines.append("</details>")
    lines.append("")

    text = "\n".join(lines) + "\n"
    if len(text.encode("utf-8")) > 950_000:  # stay under GitHub's 1 MiB limit
        text = text.encode("utf-8")[:950_000].decode("utf-8", errors="ignore") \
            + "\n\n*(summary truncated; see results.csv artifact)*\n"
    with open(path, "a", encoding="utf-8") as fh:
        fh.write(text)


# ----------------------------------------------------------------------------
# Main
# ----------------------------------------------------------------------------

def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("results_dir", help="directory produced by run_benchmarks.py")
    parser.add_argument("--csv", default=None, help="output CSV path "
                        "(default: <results_dir>/results.csv)")
    parser.add_argument("--summary", default=None,
                        help="markdown summary file to append to (e.g. $GITHUB_STEP_SUMMARY)")
    parser.add_argument("--top-n", type=int, default=25)
    parser.add_argument("--min-time", type=float, default=0.05,
                        help="exclude tests whose double-config median is below this "
                             "many seconds from ratio statistics (default 0.05)")
    args = parser.parse_args(argv)

    suite_runs, bench_runs, metadata, manifest = load_results(args.results_dir)
    for config in CONFIGS:
        if not suite_runs[config] and not bench_runs[config]:
            print("error: no results found for config '%s' in %s"
                  % (config, args.results_dir), file=sys.stderr)
            return 1

    suite_rows = join_stats(per_test_stats(suite_runs["double"]),
                            per_test_stats(suite_runs["aad"]),
                            args.min_time, "suite")
    bench_rows = join_stats(per_test_stats(
                                [{k: {"time": v, "status": "passed"}
                                  for k, v in run[0].items()} for run in bench_runs["double"]]),
                            per_test_stats(
                                [{k: {"time": v, "status": "passed"}
                                  for k, v in run[0].items()} for run in bench_runs["aad"]]),
                            args.min_time, "bench")

    csv_path = args.csv or os.path.join(args.results_dir, "results.csv")
    write_csv(suite_rows + bench_rows, metadata, csv_path)
    print("wrote %s (%d suite rows, %d bench rows)"
          % (csv_path, len(suite_rows), len(bench_rows)))

    sample = ratio_sample(suite_rows)
    gm = geomean(sample)
    if gm is not None:
        print("suite per-test geomean overhead: %.2fx (n=%d)" % (gm, len(sample)))

    if args.summary:
        write_summary(suite_rows, bench_rows, bench_runs, metadata, manifest,
                      args.summary, args.top_n, args.min_time)
        print("appended summary to %s" % args.summary)
    return 0


if __name__ == "__main__":
    sys.exit(main())
