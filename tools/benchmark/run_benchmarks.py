#!/usr/bin/env python3
##############################################################################
#
#  Run the QuantLib test suite and benchmark binaries from a plain-double
#  build and an AAD build (Real = xad::AReal<double>, no tape recording),
#  interleaved on the same machine, capturing per-test timings for
#  aggregate.py.
#
#  Produces in --out:
#    suite-<config>-<i>.xml   Boost.Test JUNIT logs (per-test times)
#    bench-<config>-<i>.txt   quantlib-benchmark --verbose=2 stdout
#    metadata.json            platform / compiler / SHA / settings
#    manifest.json            wall times and exit codes per run
#
#  Copyright (C) 2010-2026 Xcelerit Computing Limited
#  SPDX-License-Identifier: AGPL-3.0-or-later
#
##############################################################################

import argparse
import datetime
import glob
import json
import os
import platform
import re
import subprocess
import sys
import time

CONFIGS = ("double", "aad")
EXE = ".exe" if os.name == "nt" else ""


def find_exe(build_dir, name):
    path = os.path.join(build_dir, "test-suite", name + EXE)
    if not os.path.exists(path):
        raise FileNotFoundError("%s not found - was the build configured with "
                                "QL_BUILD_TEST_SUITE=ON?" % path)
    return os.path.abspath(path)


def run_logged(cmd, cwd, log_path=None):
    """Run a command, returning (exit_code, wall_seconds). Nonzero exit is
    tolerated (e.g. a failing test) so a multi-hour job is never killed by
    one bad test; the code is recorded in the manifest."""
    print("+ " + " ".join(cmd), flush=True)
    start = time.monotonic()
    if log_path:
        with open(log_path, "w", encoding="utf-8", errors="replace") as log:
            proc = subprocess.run(cmd, cwd=cwd, stdout=log,
                                  stderr=subprocess.STDOUT)
    else:
        proc = subprocess.run(cmd, cwd=cwd)
    wall = time.monotonic() - start
    print("  -> exit %d in %.1fs" % (proc.returncode, wall), flush=True)
    return proc.returncode, wall


def run_suite(exe, sink_name, run_test, out_dir):
    # The --logger argument is colon/comma separated, so the JUNIT sink must
    # be a relative path (an absolute Windows path's drive colon breaks it).
    # cwd is the output directory so the relative sink lands there.
    cmd = [exe, "--logger=HRF,message,stdout:JUNIT,message," + sink_name]
    if run_test:
        cmd.append("--run_test=" + run_test)
    return run_logged(cmd, cwd=out_dir)


def run_bench(exe, size, log_name, out_dir):
    cmd = [exe, "--verbose=2", "--size=" + size]
    return run_logged(cmd, cwd=out_dir, log_path=os.path.join(out_dir, log_name))


def cpu_model():
    system = platform.system()
    try:
        if system == "Linux":
            with open("/proc/cpuinfo", encoding="utf-8") as fh:
                for line in fh:
                    if line.lower().startswith("model name"):
                        return line.split(":", 1)[1].strip()
        elif system == "Darwin":
            return subprocess.check_output(
                ["sysctl", "-n", "machdep.cpu.brand_string"], text=True).strip()
        elif system == "Windows":
            out = subprocess.check_output(
                ["powershell", "-NoProfile", "-Command",
                 "(Get-CimInstance Win32_Processor).Name"], text=True).strip()
            return out.splitlines()[0] if out else ""
    except Exception:
        pass
    return platform.processor() or "unknown"


def compiler_info(build_dir):
    """Extract compiler id/version from CMake's generated compiler file."""
    info = {}
    for path in glob.glob(os.path.join(build_dir, "CMakeFiles", "*",
                                       "CMakeCXXCompiler.cmake")):
        with open(path, encoding="utf-8", errors="replace") as fh:
            text = fh.read()
        m = re.search(r'set\(CMAKE_CXX_COMPILER_ID "([^"]+)"\)', text)
        if m:
            info["id"] = m.group(1)
        m = re.search(r'set\(CMAKE_CXX_COMPILER_VERSION "([^"]+)"\)', text)
        if m:
            info["version"] = m.group(1)
        break
    return info


def collect_metadata(build_double, build_aad, extra_meta, args):
    comp = compiler_info(build_double)
    meta = {
        "timestamp_utc": datetime.datetime.now(datetime.timezone.utc)
            .strftime("%Y-%m-%dT%H:%M:%SZ"),
        "os": "%s %s" % (platform.system(), platform.release()),
        "cpu": cpu_model(),
        "cpu_count": os.cpu_count(),
        "compiler": "%s %s" % (comp.get("id", "unknown"), comp.get("version", "")),
        "suite_repeats": args.suite_repeats,
        "bench_repeats": args.bench_repeats,
        "bench_size": args.bench_size,
        "run_test_filter": args.run_test or "(all)",
    }
    for item in extra_meta:
        key, _, value = item.partition("=")
        meta[key] = value
    return meta


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build-double", required=True,
                        help="build dir configured with QLAAD_DISABLE_AAD=ON")
    parser.add_argument("--build-aad", required=True,
                        help="build dir configured with QLAAD_DISABLE_AAD=OFF")
    parser.add_argument("--suite-repeats", type=int, default=2)
    parser.add_argument("--bench-repeats", type=int, default=3)
    parser.add_argument("--bench-size", default="3",
                        help="quantlib-benchmark --size value (XXS..L or integer)")
    parser.add_argument("--run-test", default="",
                        help="optional Boost.Test --run_test filter (smoke runs)")
    parser.add_argument("--out", default="results")
    parser.add_argument("--meta", nargs="*", default=[],
                        help="extra key=value metadata entries (e.g. ql_sha=...)")
    args = parser.parse_args(argv)

    out_dir = os.path.abspath(args.out)
    os.makedirs(out_dir, exist_ok=True)

    builds = {"double": os.path.abspath(args.build_double),
              "aad": os.path.abspath(args.build_aad)}
    suite_exes = {c: find_exe(builds[c], "quantlib-test-suite") for c in CONFIGS}
    bench_exes = {c: find_exe(builds[c], "quantlib-benchmark") for c in CONFIGS}

    meta = collect_metadata(builds["double"], builds["aad"], args.meta, args)
    with open(os.path.join(out_dir, "metadata.json"), "w", encoding="utf-8") as fh:
        json.dump(meta, fh, indent=2)
    print("metadata: " + json.dumps(meta))

    manifest = {"suite_wall_s": {c: [] for c in CONFIGS},
                "suite_exit": {c: [] for c in CONFIGS},
                "bench_wall_s": {c: [] for c in CONFIGS},
                "bench_exit": {c: [] for c in CONFIGS}}

    # Interleave configs (D1, A1, D2, A2, ...) so that machine drift over a
    # multi-hour job affects both configurations equally.
    for i in range(1, args.suite_repeats + 1):
        for config in CONFIGS:
            sink = "suite-%s-%d.xml" % (config, i)
            code, wall = run_suite(suite_exes[config], sink, args.run_test, out_dir)
            manifest["suite_exit"][config].append(code)
            manifest["suite_wall_s"][config].append(wall)

    for i in range(1, args.bench_repeats + 1):
        for config in CONFIGS:
            log = "bench-%s-%d.txt" % (config, i)
            code, wall = run_bench(bench_exes[config], args.bench_size, log, out_dir)
            manifest["bench_exit"][config].append(code)
            manifest["bench_wall_s"][config].append(wall)

    with open(os.path.join(out_dir, "manifest.json"), "w", encoding="utf-8") as fh:
        json.dump(manifest, fh, indent=2)

    failures = [c for code_list in (manifest["suite_exit"], manifest["bench_exit"])
                for c, codes in code_list.items() if any(codes)]
    if failures:
        print("note: nonzero exit codes recorded (see manifest.json); "
              "failed tests are excluded from timing stats by aggregate.py")
    print("done: results in " + out_dir)
    return 0


if __name__ == "__main__":
    sys.exit(main())
