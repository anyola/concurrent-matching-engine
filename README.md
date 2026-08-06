# Concurrent Matching Engine

A thread-safe, in-memory limit order matching engine written in C++20.
Implements price-time priority matching, partial fills, self-trade prevention,
market orders, and a blocking trade-feed API for real-time subscriptions —
all verified race-free under ThreadSanitizer and AddressSanitizer.

## Overview

This project implements the core of a trading exchange: a limit order book
that matches buy and sell orders concurrently and safely across multiple
threads. It's built from the ground up around explicit, documented decisions
about locking granularity, exception safety, and lock-free-friendly data
structures — the kind of design trade-offs that come up in real order-matching
systems (exchanges, brokerages, internal trading tools).

**Core capabilities:**

- **Price-time priority matching** — best price first, FIFO within a price level.
- **Partial fills** — a single incoming order can sweep multiple price levels
  and match against multiple resting orders.
- **Self-trade prevention** — an order never matches against the same
  trader's resting orders; matching skips them and continues with the next
  eligible counterparty instead of failing the whole operation.
- **Market and limit orders** — market orders execute against whatever
  liquidity is available and never rest in the book.
- **O(1) order cancellation** — a hash index maps order IDs directly to their
  position in the book, avoiding a linear scan.
- **Blocking trade feed** — subscribe to a live stream of trades with an
  atomic "snapshot + subscribe" guarantee, so no trade can be missed between
  reading current state and starting to listen for new ones.
- **Multi-symbol exchange** — independent order books per symbol, with
  per-symbol locking so that trading on one symbol never blocks trading on
  another.

## Architecture

```
Exchange
  └─ owns one OrderBook per symbol, created lazily and lock-protected
       OrderBook (one per symbol)
         ├─ bid/ask price levels: std::map<price, std::list<Order>>
         ├─ order_id -> location index: std::unordered_map (O(1) cancel)
         ├─ single std::mutex guarding all book state
         ├─ append-only trade log + condition_variable
         └─ TradeFeed: a per-subscriber cursor into the trade log,
            blocks on the condition_variable until new trades arrive
```

Each `OrderBook` is independently locked, so concurrent operations on
different symbols never contend with each other — only operations on the
*same* symbol serialize. This was a deliberate design choice over a single
global lock, made specifically to allow the engine to scale across symbols.

## Key design decisions

A few choices here are intentional trade-offs, not oversights — documented
here so they read as decisions rather than gaps:

- **Self-trade prevention skips, it doesn't reject.** If the best available
  counterparty belongs to the same trader, the matcher skips it and keeps
  looking — it does not throw and does not roll back partial progress. This
  keeps the operation exception-safe: once matching starts consuming
  liquidity, nothing after that point can fail.
- **Market order remainders are dropped, not queued.** If a market order
  can't be fully filled, the unfilled quantity is reported back via
  `PlaceResult::remaining_quantity` but is never added to the book — market
  orders are never resting orders by definition.
- **The trade log grows unbounded for the lifetime of the process.** Fine
  for a process with a bounded lifetime (tests, demos); a long-running
  service would need log rotation or a persistent store for history that
  outgrows memory.
- **Order IDs are assigned by the engine, not the caller**, via an atomic
  counter — this removes an entire class of bugs where two orders could
  collide on the same ID and silently corrupt the index.

## Concurrency model

- Each `OrderBook` has a single `std::mutex` guarding its price levels, its
  order index, and its trade log. All public methods (`place_order`,
  `cancel_order`, `snapshot_depth`, `subscribe`) take this lock for their
  full duration.
- `subscribe()` and `snapshot_depth()` are atomic with respect to trading
  activity: a subscription is guaranteed to observe every trade from the
  moment it was created onward, with no possibility of a trade slipping
  through the gap between reading state and starting to listen.
- `TradeFeed::wait_next_trade()` blocks on a `condition_variable` (never a
  busy-wait) until a trade the subscriber hasn't seen yet is available.
  Multiple independent subscribers can wait on the same book simultaneously;
  each tracks its own read position into the trade log.
- `Exchange::get_or_create_book()` is guarded by its own separate mutex,
  independent from any individual `OrderBook`'s mutex — so looking up or
  creating a book for one symbol never blocks trading on another.
- No blocking I/O, and no `sleep_for`-based synchronization, anywhere in the
  engine itself.

## Tooling and verification

**ThreadSanitizer** — used to validate multi-threaded code paths and detect
potential data races, including concurrent order placement, concurrent symbol
creation, and trade-feed subscription stress scenarios.

**AddressSanitizer + UndefinedBehaviorSanitizer** — used to detect memory
safety issues and undefined behavior such as invalid memory access and
lifetime errors.

**clang-tidy** — static analysis with checks from:
`bugprone-*`, `cppcoreguidelines-*`, `performance-*`,
`modernize-*`, and `clang-analyzer-*`.

**Compiler warnings enabled:**
```text
-Wall
-Wextra
-Wpedantic
-Wconversion
-Wshadow
-Wold-style-cast
-Wnon-virtual-dtor
```

## Building

```bash
cmake -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

## Testing

The test suite covers single- and multi-threaded:

- Core matching: full fills, partial fills, multi-level sweeps, price-time
  priority ordering.
- Self-trade prevention across several scenarios (only own liquidity
  available, own orders interleaved with others', own liquidity spread
  across multiple price levels).
- Market orders: full execution, partial execution with dropped remainder,
  and zero available liquidity.
- Input validation and the exception hierarchy.
- Order cancellation, including cancelling partially-filled and
  already-fully-executed orders.
- Order book depth aggregation.
- `TradeFeed`: immediate delivery, genuine blocking across threads
  (verified via `std::future`/`std::promise`, not timing-based sleeps), and
  a dedicated stress test for the atomicity of `subscribe()`.
- `Exchange`: identity of concurrently-created books for the same symbol,
  and independence between different symbols.
- A concurrency invariant test: under sustained multi-threaded load, total
  submitted volume always equals `2 × executed volume + resting volume` —
  the factor of two accounts for the fact that every trade consumes
  quantity from both the taker and the maker side.

## Project structure

```
concurrent-matching-engine/
├── CMakeLists.txt
├── include/
│   ├── errors.hpp       # exception hierarchy
│   ├── order.hpp        # plain data types: Order, Trade, Depth, PlaceResult
│   └── order_book.hpp   # OrderBook, TradeFeed, Exchange
├── src/
│   └── order_book.cpp
├── tests/
│   └── test_matching.cpp
└── third_party/
    └── doctest.h
```

## Known limitations

- `int` price/quantity fields instead of fixed-point arithmetic.
- Floating-point arithmetic is intentionally avoided; production systems
  would typically use fixed-point integer representation for prices and
  quantities to guarantee exact financial calculations.
- Unbounded trade log growth over process lifetime.
- No persistence — the book exists only in memory for the process lifetime.
- `std::mutex` rather than `std::shared_mutex`: since `snapshot_depth` is
  read-only and likely called far more often than `place_order`, a
  reader/writer lock is a plausible follow-up optimization, ideally
  validated with a before/after throughput benchmark.
