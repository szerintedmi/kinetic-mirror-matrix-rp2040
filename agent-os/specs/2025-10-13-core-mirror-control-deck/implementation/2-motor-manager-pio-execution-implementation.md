### Motor Coordination Engine
**Location:** `src/motion/MotorManager.cpp`

Implemented the RP2040-aware motor manager that maintains two-slot step queues per channel, enforces ±1200 step limits, coordinates autosleep through an SN74HC595-backed shift register facade, and now drives a three-stage homing sequence using relative moves that intentionally bypass soft limits before re-zeroing at travel midpoint. The manager owns deterministic timing estimates, exports command buffers for PIO consumption, and maps ESP32 FastAccelStepper behaviors (wake-before-motion, immediate post-move sleep, midpoint homing) to RP2040 semantics while documenting MCU-specific differences.

**Rationale:** Encapsulating motion policy in a dedicated manager keeps command parsing stateless, allows deterministic simulation in host tests, and mirrors the ESP32 prototype’s cadence while leveraging RP2040 PIO double buffering instead of timer ISRs.

### Serial Command Integration
**Location:** `src/CommandProcessor.cpp`

Refactored the command processor to delegate MOVE/HOME/SLEEP/WAKE verbs to the motor manager, added new response codes (limit, busy, driver fault), surfaced plan metadata in responses, and synchronized status reporting with manager-derived state and faults. Payload parsing now supports homing overrides and records per-channel response codes for STATUS rendering.

**Rationale:** Centralizing motion execution in the manager while keeping parsing guardrails ensures serial commands remain deterministic, prevent over-limit moves, and surface meaningful telemetry without duplicating state.

### Step/Dir PIO Program Assets
**Location:** `src/motion/StepperPioProgram.cpp`

Added a compile-time step/dir PIO program descriptor plus helper conversions that translate microsecond step periods into RP2040 clock ticks. The module publishes both encoded instructions and assembly listings so the double-buffered queues can feed deterministic pulses once bound to hardware.

**Rationale:** Providing a concrete program definition and conversion helpers enables the firmware to deploy RP2040 PIO state machines with compile-time buffers, satisfying determinism requirements while staying host-testable.

### Shift Register Sleep Control
**Location:** `include/motion/MotorManager.hpp`

Introduced a shift register abstraction that latches per-motor sleep bits, defaulting to host-mode no-ops but ready to drive SN74HC595 pins on hardware builds. Autosleep transitions now flow through this abstraction.

**Rationale:** Abstracting the SN74HC595 interface keeps GPIO preservation logic co-located with motion policy and allows clean host simulation while meeting the GPIO conservation requirement.

### Focused Motion Tests
**Location:** `test/test_motor_manager/test_main.cpp`

Authored six Unity tests that probe soft limit clipping, relative homing travel (including backoff and midpoint settling), autosleep recovery, trapezoidal timing math, and driver fault handling. Updated the command-surface tests to align with the richer response payloads exposed by the manager-backed processor.

**Rationale:** Host-native coverage protects the most error-prone motion behaviors without hardware, ensuring regressions are caught when command semantics or timing math evolve.

## Database Changes (if applicable)

None.

## Dependencies (if applicable)

### New Dependencies Added
None.

### Configuration Changes
- `Unity` configuration now compiles via `unity_config.c` for the new test suite; no additional environment variables required.

## Testing

### Test Files Created/Updated
- `test/test_motor_manager/test_main.cpp` – Validates limits, relative homing travel/backoff/midpoint behavior, autosleep, timing, and fault paths for the motor manager.
- `test/test_command_surface/test_main.cpp` – Adjusted expectations for MOVE and STATUS responses now driven by the motor manager.

### Test Coverage
- Unit tests: ✅ Complete
- Integration tests: ❌ None
- Edge cases covered: limit clipping, homing midpoint establishment, autosleep completion, timing math accuracy, injected driver fault recovery.

### Manual Testing Performed
Executed `platformio test -e native -f test_motor_manager` after implementation; no additional manual steps were required.

## User Standards & Preferences Compliance

### tech-stack.md
**File Reference:** `agent-os/product/tech-stack.md`

**How Your Implementation Complies:** Stayed within the PlatformIO + Unity toolchain, kept motion buffers static to respect RP2040 memory expectations, and reused the existing Arduino entrypoint without altering project scaffolding.

**Deviations (if any):** None.

## Integration Points (if applicable)

### Internal Dependencies
- `control::CommandProcessor` now depends on `motion::MotorManager` for all motion verbs and leverages `motion::pio` helpers to surface timing data.

## Known Issues & Limitations

### Issues
1. **PIO Waveform Unverified on Hardware**
   - Description: The new PIO program has not yet been run on physical hardware, so pulse timings remain host-simulated.
   - Impact: Potential tuning may be required once deployed to RP2040 silicon.
   - Workaround: None; schedule hardware validation during Task Group 3.
   - Tracking: Not yet ticketed.

### Limitations
1. **Homing Sequence Assumes Ideal Switch Behavior**
   - Description: Homing uses a deterministic three-stage plan without sensor feedback emulation.
   - Reason: Hardware integration for limit switch sensing is deferred to future tasks when physical rigs are available.
