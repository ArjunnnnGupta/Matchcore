#!/usr/bin/env bash
# 2-3 minute terminal walkthrough: build from scratch, run the test suite,
# run the matching engine benchmark, run the C++ vs Python benchmark suite.
#
# This is the script to record for the README demo link, e.g.:
#   asciinema rec demo.cast -c ./demo.sh
# (asciinema records the terminal session to a .cast file locally; nothing is
# uploaded anywhere unless you separately run `asciinema upload`.)
#
# WARNING: deletes and recreates build/ and benchmark/build — intentional,
# so the recording shows a real build happening, not a cached no-op.
set -euo pipefail
cd "$(dirname "${BASH_SOURCE[0]}")"

echo "=== MatchCore demo: build -> test -> benchmark ==="

rm -rf build
mkdir build && cd build

echo "--- configuring (Release) ---"
cmake .. -DCMAKE_BUILD_TYPE=Release

echo "--- building ---"
make -j

echo "--- running unit tests ---"
ctest

echo "--- running the matching engine benchmark (1M synthetic orders) ---"
./engine/matchcore_bench --input ../data/sample_ticks.csv --orders 1000000

cd ../benchmark
echo "--- building the pybind11 C++ pipeline ---"
python3 build_cpp_ext.py build_ext --inplace >/dev/null

echo "--- running the C++ vs Python benchmark suite (500K ticks x 10 runs) ---"
python3 run_benchmark.py --input ../data/sample_ticks.csv --ticks 500000 --runs 10

echo "=== done ==="
