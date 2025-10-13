# Mirror Array — Requirements v0.2.1

Date: 2025‑10‑10

## Scope & Definitions

 • System: Modular mirror array producing controllable light reflections on a wall/surface.
 • Island: 4×4 mirrors (16 mirrors), each mirror with 2 stepper motors → 32 motors per island.
 • Global Master: ESP32 coordinating multiple islands (introduced in Phase 2).
 • Island Master: ESP32 per island; in Phase 1 it hosts the web UI and scheduler and directly orchestrates its RP2040 nodes.
 • Motor Node: RP2040 controlling up to 8 motors via DRV8825 (Step/Dir), with per‑motor SLEEP via 74HC595.
 • Canvas: Target wall/surface coordinate space for placing “pixels” (reflections).

Assumptions (A):
 • Motors: bipolar 4‑wire, 9 V, ~100 mA peak (70 mA typical), 3–4 k steps/s.
 • Mechanics: non‑gimbal linkage, ≈13° max per axis, ~2300 steps full range, ~50 steps (~0.26°) minimum useful increment.
 • Operating distance: 2–10 m; centroid control (controlling only the center point of the reflected light spot, not its full shape or intensity distribution).
 • No keep‑out zones for MVP; system is supervised.

⸻

## Architecture Baseline (prioritized by phase)

SYS‑001. Phase 1 (MVP): Implement Island Master (ESP32) + RP2040 nodes only; no Global Master. The Island Master hosts the web UI and scheduler and controls its island end‑to‑end.
SYS‑002. Phase 2: Introduce a Global Master ESP32 to coordinate one or more Island Masters over ESP‑NOW. Island/firmware protocols shall be structured for a clean refactor to accept external master commands without breaking Phase 1 behavior.
SYS‑003. Each Island Master shall control its local RP2040 motor nodes over short intra‑island links on a custom PCB (no long cables).
SYS‑004. Each RP2040 shall generate step pulses using PIO and control DRV8825 drivers (Step/Dir).
SYS‑005. Per‑motor SLEEP control shall be provided via SN74HC595 (or equivalent) on each RP2040.

Phase 1 Acceptance (A1): End‑to‑end (single island): UI → Island Master → RP2040 → DRV8825 → motors reach commanded positions.
Phase 2 Acceptance (A2): Global Master → (ESP‑NOW) → Island Masters → RP2040 → DRV8825; multi‑island orchestration.

⸻

## Mechanical & Optics (P0 unless noted)

MECH‑001. Each mirror axis shall support commanded motion within ±13° (or realized safe mechanical limit as a single global parameter).
MECH‑002. Minimum settable increment shall be ≥ 50 steps (~0.26°) initially; finer control may be explored later.
MECH‑003. From any in‑range position, a full‑range move shall complete in <1 s at nominal 3–4 k steps/s when unconstrained by power scheduling.
OPT‑001. The system shall aim for centroid placement only; angle‑dependent distortion is accepted and mirrored in the simulator.
OPT‑002 (P1). The simulator should visualize spot growth/distortion with incidence angle.

⸻

## Electrical & Power

PWR‑001. Phase 1: The Island Master shall enforce a configurable concurrency limit (max simultaneously moving motors) to stay within the island power budget.
PWR‑002. Phase 2: The Global Master shall enforce an overall power cap (e.g., bench PSU ≈ 300 W) and derive per‑island caps; Island Masters shall respect those caps locally.
PWR‑003. Per‑motor SLEEP shall be asserted whenever a motor is idle (no torque holding required).
PWR‑004. Nominal supply shall be 9 V (12 V permissible); actual limits recorded in config.

⸻

## Communication & Addressing

COMM‑001. Phase 1: No Global↔Island link; all control remains within the island.
COMM‑002. Phase 2: Global→Island transport shall be ESP‑NOW (unicast/broadcast) with an island registry at the Global Master.
COMM‑003. Intra‑island control (Island→RP2040) shall use short PCB traces (I²C/SPI/PIO‑sideband) sized to avoid bus‑length/EMI issues.
ADDR‑001. RP2040 addressing shall be deterministic per‑island (fixed mapping by design or strapping); no runtime auto‑addressing is required for MVP.
ADDR‑002 (P1). If software assignment is introduced, the assigned ID should be persisted in RP2040 flash; a re‑assignment flow should exist.

⸻

## Firmware Responsibilities & Command Model

FW‑001. Master‑scheduled model: RP2040 nodes shall not maintain multi‑command queues. Each RP2040 executes one active command; a new command cancels the active move.
FW‑002. RP2040 shall expose primitives:
 • SET_ABS(axis, steps, speed, accel) – absolute target.
 • SET_REL(axis, dsteps, speed, accel) – relative move.
 • HOME(axis, dir, v_fast, v_slow, timeout_ms) – bump‑stop homing sequence.
 • SLEEP(mask) – per‑motor sleep via 595.
FW‑003. Commands are one‑way in normal ops; RP2040 does not send unsolicited status.
FW‑004 (P1). Island/Global Masters should support a polled diagnostic word (e.g., busy/idle bitfield + thermal‑throttle latch) on request.
FW‑005. All time/velocity/accel parameters shall be specified in the command (thin node).

⸻

## Scheduling & Power‑Aware Movement

SCH‑001. Phase 1: The Island Master shall estimate duration from (steps, speed, accel) and schedule batches to respect concurrency and SLEEP grouping.
SCH‑002. If per‑RP2040 SLEEP grouping is used (fallback), the scheduler shall batch moves per RP2040 to minimize wake time.
SCH‑003. Initial heuristic shall prioritize mirrors with longest remaining travel first, then shorter moves.
SCH‑004 (P1). Add inrush guard bands and simple rate‑limits for new starts.

⸻

## Homing & Calibration

HOM‑001. Homing shall use the mechanical bump stop: fast approach → back‑off → slow re‑touch, with safe current/velocity; timeout per axis required.
HOM‑002. Homing shall be master‑initiated (on boot or on demand).
CAL‑001 (P1). Support per‑mirror zero offset constants (persisted) to compensate manufacturing variance.
CAL‑002 (P2). Optional orthogonality/coupling corrections.

⸻

## Geometry Model & Targeting

GEO‑001. The system shall maintain three frames: world (wall plane), panel (island frame), mirror (per‑mirror axes).
GEO‑002. The UI shall accept manual azimuth/elevation for the incoming light and wall distance + wall plane angle.
GEO‑003. The master shall compute the required mirror normal vector (bisector of incoming/outgoing rays) and convert to axis angles respecting mechanical limits.
GEO‑004. MVP targeting tolerance: place the centroid within ±3 cm at 3 m and ±10 cm at 10 m (assuming current hardware).
GEO‑005. Out‑of‑reach targets shall be detected and clipped or flagged in the UI.
GEO‑006 (P1). Multi‑island layout transforms.

⸻

## UI & Usability

UI‑001. Phase 1: The Island Master shall host an async HTTP server with a responsive web UI (mobile‑friendly).
UI‑002. UI shall provide Wi‑Fi setup via Wi‑Fi Portal (AP→STA) and persist credentials.
UI‑003. UI shall present a canvas to place desired “pixels”; grid extent driven by geometry limits.
UI‑004. UI shall allow manual entry of: wall distance, wall orientation, light azimuth/elevation, and mechanical angle limit (global).
UI‑005 (P1). Grouping + simple motion scripts for “play” patterns (phase, frequency, amplitude).
UI‑006. Phase 2: When a Global Master exists, the UI may run there; Island UI remains available for local control/diagnostics.

⸻

## Safety & Thermal Protection

SAFE‑001. A rolling‑window step/time limit per motor shall be enforced by the scheduler to prevent overheating (configurable window and cap).
SAFE‑002. RP2040 shall implement a local hard cap using the same metric; on violation it blocks motion until cool‑down elapses.
SAFE‑003. During a local block, RP2040 shall ignore new motion commands for that motor until cool‑down expires (silent fail‑safe acceptable in MVP).
SAFE‑004 (P1). Diagnostic polling exposes a thermal‑throttle/blocked bit per motor.

⸻

## Diagnostics & Operations

DIAG‑001. Phase 1: The Island Master shall maintain a local registry (node IDs, versions).
DIAG‑002. Phase 1: A master‑initiated ping (poll) shall return a minimal status word from RP2040 nodes on demand (reachable/busy).
OPS‑001. If a node is unreachable, the island shall continue with remaining nodes (degraded mode).
DIAG‑003. Phase 2: The Global Master maintains an island registry (ID, version, layout) and can poll islands.
LOG‑001 (P1). Optional telemetry logs: command timestamps, durations, and power‑scheduling decisions.

⸻

## Simulator (separate project; shared math)

SIM‑001. A standalone simulator shall render the wall with rays and a heatmap of intended mirror→pixel mapping using the same geometry inputs as the control UI.
SIM‑002. The simulator shall overlay reachable regions based on mechanical angle limits.
SIM‑003. The simulator shall use the same calculation logic as the real control by linking a shared C++ library that implements geometry, reachability, and angle conversion; this library is compiled for both firmware and simulator.
SIM‑004 (P1). The simulator should display a time/power timeline for scheduled moves given concurrency caps.
SIM‑005 (P1). The simulator should support simple assignment optimization (reduce crossed rays, over‑travel).

Implementation location considerations (non‑binding DDR to resolve early):
 • Frontend‑heavy sim (JS/Web): Pros: Offloads device/backend; easy local runs; good dev velocity. Cons: Requires a transpiled/ported math layer or WebAssembly build of the shared C++ library; risk of logic drift if not using the shared lib; harder to ensure deterministic parity.
 • Backend‑heavy sim (C++ service): Pros: Reuses the same compiled C++ math directly; easier to keep logic identical and deterministic; simpler CI parity. Cons: Running without hardware needs a lightweight local server process; a bit more setup for pure‑browser demos.
SIM‑006. Decision (opinionated): Adopt a shared C++ core as a portable library and ship both a native build (firmware/backend) and a WASM build for the browser. Visualization remains frontend; scheduling/power previews can run in the frontend by calling the shared lib. This gives near‑zero logic drift with good developer ergonomics.

⸻

## Testing

TEST‑001. Core math/scheduling shall be packaged as host‑runnable C++ with PlatformIO (native) unit tests (CI‑friendly).
TEST‑002. Unit tests shall cover: bisector/angle conversion, reachability, duration estimation, concurrency enforcement, and homing sequences (logic).
TEST‑003 (P1). On‑target tests validate PIO pulse timing and DRV8825 interfacing on a dev fixture.
TEST‑004. Simulator CI shall run the same math tests against the shared C++ core.

⸻

## Data & Persistence

DATA‑001. Phase 1: Island Master shall persist configuration (Wi‑Fi, canvas params, mechanical limit, local node layout).
DATA‑002. Phase 2: Global Master shall persist multi‑island layout/config.
DATA‑003 (P1). If per‑mirror offsets are introduced, they should be persisted and versioned.

⸻

## Prioritized MVP Feature Set — Phase 1 (P0)

 1. Single‑island end‑to‑end control path (UI → Island Master → RP2040 → DRV8825 → motors).
 2. Geometry & targeting with manual light/wall inputs; canvas placement; reachability checks.
 3. Island‑level scheduling with concurrency caps and per‑motor SLEEP; longest‑travel‑first heuristic.
 4. Homing by bump stop with safe speeds and timeouts.
 5. Thermal step/time limiter (Island scheduler) + simple local fail‑safe (RP2040).
 6. Simulator (basic): rays + reachable overlay + centroid preview using the shared C++ math.
 7. Host unit tests for math/scheduling (shared core) and firmware logic where applicable.

⸻

## Next Priorities — Phase 2 (P1)

 • Introduce Global Master and ESP‑NOW transport; multi‑island orchestration.
 • Global power‑cap enforcement and per‑island budgeting.
 • Multi‑island layout config & composition on canvas.
 • Minimal diagnostics polling (busy/idle, thermal latch).
 • Scheduler guard bands; per‑RP2040 batching awareness.
 • Per‑mirror zero offsets.
 • UI scripting for “play” patterns.
 • Simulator timeline/power view + simple routing optimization.
 • Telemetry logging.

## Later (P2)

 • Orthogonality/coupling calibration; hysteresis compensation.
 • Advanced safety (keep‑out masks, dwell limits).
 • Enhanced routing/assignment optimization.
 • Alternate drivers (e.g., quieter/sensor‑assisted) evaluation.

⸻

## Open / Tracked Decisions

 • Intra‑island bus choice (I²C vs SPI control for 595 and RP2040 mgmt) — implement on PCB and validate (P0).
 • ESP‑NOW message format (seq numbers/ids to drop stale commands) — minimal framing (P1).
 • Exact acceptance tolerances per distance beyond 3 m/10 m — refine after initial calibration (P1).
 • Island ID & discovery (static vs soft provisioning) — static for MVP, tool later (P1).
 • SIM logic location: confirm WASM build feasibility/perf for the shared C++ core (P0 decision checkpoint).
