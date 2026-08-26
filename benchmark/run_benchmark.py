"""Drives all three pipeline implementations over identical input data with
identical timing methodology, prints a summary table, and plots a histogram
of per-run wall times. See docs/ARCHITECTURE.md section 3.4 and
benchmark/RESULTS.md for the write-up.
"""
import argparse
import sys
import time
from pathlib import Path

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np
import pandas as pd

sys.path.insert(0, str(Path(__file__).parent / "python_pipeline"))
import pipeline as py_pipeline  # noqa: E402

import cpp_pipeline  # noqa: E402  (built via build_cpp_ext.py build_ext --inplace)


def load_data(csv_path, n_ticks):
    df = pd.read_csv(csv_path)
    df = df[df["type"] == "LIMIT"].head(n_ticks).reset_index(drop=True)
    return df


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", default="../data/sample_ticks.csv")
    parser.add_argument("--ticks", type=int, default=500_000)
    parser.add_argument("--runs", type=int, default=10)
    parser.add_argument("--window", type=int, default=20)
    parser.add_argument("--output", default="latency_histogram.png")
    args = parser.parse_args()

    df = load_data(args.input, args.ticks)
    n = len(df)
    if n < args.ticks:
        print(f"warning: only {n} LIMIT ticks available (requested {args.ticks})")

    sides_str = df["side"].to_numpy()
    prices = df["price"].to_numpy(dtype=np.float64)
    sides_int8 = np.where(sides_str == "B", 0, 1).astype(np.int8)

    results = {"python_row_loop": [], "python_vectorized": [], "cpp_row_loop": []}

    for run in range(args.runs):
        t0 = time.perf_counter()
        py_pipeline.row_loop(sides_str, prices, args.window)
        results["python_row_loop"].append(time.perf_counter() - t0)

        t0 = time.perf_counter()
        py_pipeline.vectorized(df, args.window)
        results["python_vectorized"].append(time.perf_counter() - t0)

        t0 = time.perf_counter()
        cpp_pipeline.row_loop(sides_int8, prices, args.window)
        results["cpp_row_loop"].append(time.perf_counter() - t0)

        print(f"run {run + 1}/{args.runs} done")

    print(f"\n{n} ticks, {args.runs} runs each\n")
    header = f"{'implementation':<20}{'mean (s)':<12}{'median (s)':<12}{'throughput (ticks/s)':<22}"
    print(header)
    print("-" * len(header))

    summary = {}
    for name, times in results.items():
        arr = np.array(times)
        mean_t = arr.mean()
        median_t = float(np.median(arr))
        throughput = n / mean_t
        summary[name] = {"mean": mean_t, "median": median_t, "throughput": throughput}
        print(f"{name:<20}{mean_t:<12.6f}{median_t:<12.6f}{throughput:<22.0f}")

    speedup_row = summary["python_row_loop"]["mean"] / summary["cpp_row_loop"]["mean"]
    speedup_vec = summary["python_vectorized"]["mean"] / summary["cpp_row_loop"]["mean"]
    print(f"\nC++ vs Python row-loop speedup:   {speedup_row:.1f}x  (fair: both are streaming)")
    print(f"C++ vs Python vectorized speedup: {speedup_vec:.2f}x  (NOT a fair streaming "
          f"comparison — see RESULTS.md)")

    plt.figure(figsize=(8, 5))
    for name, times in results.items():
        plt.hist(times, bins=10, alpha=0.6, label=name)
    plt.xlabel("wall time per run (s)")
    plt.ylabel("count")
    plt.title(f"Tick pipeline wall time: {n} ticks x {args.runs} runs")
    plt.legend()
    plt.tight_layout()
    plt.savefig(args.output, dpi=120)
    print(f"\nwrote {args.output}")


if __name__ == "__main__":
    main()
