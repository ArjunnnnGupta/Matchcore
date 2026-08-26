"""Python reference implementation of the shared 3-stage benchmark pipeline:
parse tick -> update best-bid/ask -> emit a signal (mid vs its own moving
average). See docs/ARCHITECTURE.md section 3 for why this pipeline (not the
full matching engine) is what's being compared here.

Two variants, on purpose (see docs/ARCHITECTURE.md 3.2): a row-loop version,
which is what a real streaming feed handler looks like, and a vectorized
pandas version, which is the idiomatic "fast pandas" approach but is NOT a
faithful streaming proxy (it can see the whole column at once via ffill,
which a real handler cannot). Comparing C++ only against the vectorized
version would be an unfair/misleading benchmark; both are reported.
"""
from collections import deque

import numpy as np


def row_loop(sides, prices, window=20):
    """Pure-Python, one tick at a time. sides/prices are equal-length
    sequences; sides[i] is 'B' or 'S'. Returns a list of {-1, 0, 1} signals.
    """
    best_bid = float("nan")
    best_ask = float("nan")
    window_vals = deque(maxlen=window)
    window_sum = 0.0

    n = len(prices)
    signals = [0] * n
    for i in range(n):
        if sides[i] == "B":
            best_bid = prices[i]
        else:
            best_ask = prices[i]

        if best_bid == best_bid and best_ask == best_ask:  # neither is NaN
            mid = (best_bid + best_ask) / 2.0
            if len(window_vals) == window:
                window_sum -= window_vals[0]
            window_vals.append(mid)
            window_sum += mid
            moving_avg = window_sum / len(window_vals)
            if mid > moving_avg:
                signals[i] = 1
            elif mid < moving_avg:
                signals[i] = -1
    return signals


def vectorized(df, window=20):
    """Pandas/numpy vectorized version of the same conceptual pipeline.
    `df` must have 'side' ('B'/'S') and 'price' columns.
    """
    bid = df["price"].where(df["side"] == "B").ffill()
    ask = df["price"].where(df["side"] == "S").ffill()
    mid = (bid + ask) / 2.0
    moving_avg = mid.rolling(window=window, min_periods=1).mean()
    signal = np.sign(mid - moving_avg).fillna(0).astype(int)
    return signal.to_numpy()
