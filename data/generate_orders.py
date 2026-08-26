#!/usr/bin/env python3
"""Synthetic order generator for MatchCore replay/benchmarking.

Simulates a random-walk mid price and emits a CSV of orders clustered
around it, so a replay produces realistic matching activity (crosses,
partial fills, resting liquidity) rather than either "never matches" or
"always matches" degenerate input.

CSV columns: order_id,owner_id,timestamp_ns,side,type,price,quantity
"""
import argparse
import random


def generate(num_orders, seed, start_price, tick_size, num_owners,
             ioc_fraction, market_fraction):
    rng = random.Random(seed)
    mid = start_price
    timestamp_ns = 0

    yield "order_id,owner_id,timestamp_ns,side,type,price,quantity"

    for order_id in range(1, num_orders + 1):
        # Random walk the mid price by a small number of ticks per order.
        mid += rng.choice([-2, -1, -1, 0, 0, 0, 1, 1, 2]) * tick_size
        mid = max(mid, tick_size)

        side = rng.choice(["B", "S"])
        roll = rng.random()
        if roll < ioc_fraction:
            order_type = "IOC"
        elif roll < ioc_fraction + market_fraction:
            order_type = "MARKET"
        else:
            order_type = "LIMIT"

        # Limit price offset from mid: aggressive enough that a meaningful
        # fraction of orders actually cross the book, not just rest forever.
        offset_ticks = rng.randint(-3, 3)
        price = mid + (offset_ticks * tick_size if side == "B" else -offset_ticks * tick_size)
        price = max(price, tick_size)

        quantity = rng.randint(1, 50)
        owner_id = rng.randint(1, num_owners)
        timestamp_ns += rng.randint(100, 5000)

        price_str = f"{price:.4f}" if order_type != "MARKET" else "0.0000"

        yield f"{order_id},{owner_id},{timestamp_ns},{side},{order_type},{price_str},{quantity}"


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", default="sample_ticks.csv")
    parser.add_argument("--orders", type=int, default=500_000)
    parser.add_argument("--seed", type=int, default=42)
    parser.add_argument("--start-price", type=float, default=100.0)
    parser.add_argument("--tick-size", type=float, default=0.01)
    parser.add_argument("--owners", type=int, default=200,
                         help="number of distinct trading accounts (owner_id pool)")
    parser.add_argument("--ioc-fraction", type=float, default=0.1)
    parser.add_argument("--market-fraction", type=float, default=0.05)
    args = parser.parse_args()

    with open(args.output, "w") as f:
        for line in generate(args.orders, args.seed, args.start_price, args.tick_size,
                              args.owners, args.ioc_fraction, args.market_fraction):
            f.write(line + "\n")

    print(f"wrote {args.orders} orders to {args.output}")


if __name__ == "__main__":
    main()
