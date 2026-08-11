# Deterministic Lockstep Simulation (C++)

A deterministic lockstep simulation engine for reproducible real-time systems.
Same input -> Same result. On any machine, in any execution.

**Core Principle**: Never send state over the network. Only send inputs.
If all peers apply the same inputs on the same tick, the simulation proceeds identically.

---

## Phase 0 — MVP: Single Process Determinism (No Network)

> Goal: Running the same input sequence twice results in a 100% state hash match every tick.
> There is no network yet. Prove determinism first.

- [ ] Project setup (CMake, C++20, warnings-as-errors, sanitizers)
- [ ] Fixed timestep simulation loop (`tick()` — completely decoupled from render/real-time)
- [ ] Deterministic numerical computation selection & implementation
  - [ ] Fixed-point arithmetic (Q16.16 or Q32.32) — Recommended
  - [ ] (Alternative) IEEE754 strict mode verification (`-ffp-contract=off`, enforce SSE2, disable FMA)
- [ ] Deterministic RNG (PCG/xoshiro, explicit seed management, system rand prohibited)
- [ ] Isolate simulation state into a single struct (No global state, use index/handle instead of pointers)
- [ ] Establish rules for deterministic containers (no unordered_map traversal, only sorted traversal)
- [ ] State hash function (hash entire state every tick — xxHash/FNV)
- [ ] Demo simulation (Simple physics: N units moving/colliding)
- [ ] **Verification**: Run same input for 10,000 ticks x 2 times -> hash sequence diff == 0
- [ ] **Verification**: Hash match between Debug and Release builds

## Phase 1 — Input System & Replay

> Goal: Record/playback input stream. Replay = free bonus of determinism and the best testing tool.

- [ ] Define input command structure (tick number + player id + serialized payload)
- [ ] Input buffer (input queue per tick, schedule future tick inputs)
- [ ] Replay format (header: seed/version/start state hash + input stream)
- [ ] Record / Playback replays
- [ ] **Verification**: Live execution hash == Replay playback hash
- [ ] Fuzz replays (generate thousands of random inputs -> verify playback, include in CI)

## Phase 2 — Local Lockstep (Multi-peer, Same Machine)

> Goal: 2 processes exchange only inputs via loopback and proceed with identical simulations.

- [ ] Peer abstraction (transport interface — initially loopback/in-memory)
- [ ] Input delay model (Input at tick N applied at tick N+d)
- [ ] Lockstep barrier: Proceed tick only when all peer inputs arrive (handle stalls)
- [ ] Exchange state hash every tick -> **Detect desync immediately**
- [ ] Desync dump (serialize state of both sides + diff tool)
- [ ] **Verification**: 2~4 peers, 100,000 ticks with no desync

## Phase 3 — Real Network

> Goal: Lockstep over UDP that withstands packet loss/latency/jitter.

- [ ] UDP transport + reliability layer (inputs must never be lost — redundant send or ack/resend)
- [ ] Include redundant inputs of recent k ticks in packets (loss tolerance)
- [ ] Network simulator (inject artificial loss/latency/jitter — for testing)
- [ ] Adaptive input delay (adjust d based on RTT)
- [ ] Late peer handling policy (stall vs timeout vs drop)
- [ ] Connection/Startup handshake (consensus on seed, start tick, peer list)
- [ ] **Verification**: Long duration with no desync under 5% loss + 100ms jitter

## Phase 4 — Production Quality

- [ ] State serialization/snapshot (save/load, basis for late join)
- [ ] Cross-platform determinism verification (Linux/macOS/Windows, x86-64 vs ARM64 — passes if fixed-point, hell if float)
- [ ] Cross-compiler verification (gcc vs clang vs msvc)
- [ ] Desync debugging tools (tick-by-tick hash logs, state diff viewer, binary search for desync tick)
- [ ] Performance: Profile tick budget, separate simulation thread
- [ ] Documentation (Determinism rules checklist — pitfalls easy for contributors to violate)

## Phase 5 — Expansion (Optional)

- [ ] Rollback support (Snapshot + re-simulation — GGPO style, eliminates perceived input delay)
- [ ] Late join (Transmit snapshot + catch up)
- [ ] Spectator mode (implemented solely by broadcasting input stream)
- [ ] Headless server verification node (Anti-cheat — server runs same simulation to verify hash)

---

## Determinism Rules (Things you must absolutely never violate)

- Prohibited in simulation code: `float`/`double` (if fixed-point is chosen), `std::unordered_*` traversals, pointer address-based sorting/hashing, `rand()`, system time, thread races, uninitialized memory
- Simulation inputs are only: Previous state + Input commands for the current tick
- Rendering/Audio/UI **read-only** access to the simulation state

## Milestone Summary

| Phase | What It Proves |
|---|---|
| P0 | Determinism itself |
| P1 | Replay = Regression test infrastructure |
| P2 | Lockstep protocol integrity |
| P3 | Real-world network tolerance |
| P4 | Cross-platform + debuggability |
| P5 | Advanced features like rollback |
