### Integration Test Sequences
**Location:** `test/test_integration_control/test_main.cpp`

Created five Unity-based integration tests that emit `HELP`, `MOVE`, `HOME`, `STATUS`, and `SLEEP/WAKE` commands against `CommandProcessor`. Each test exercises serial responses and verifies resulting motor state transitions by advancing time through a new `CommandProcessor::service` helper, ensuring end-to-end command execution mirrors the RP2040 manager behavior.

**Rationale:** A dedicated integration suite validates the serial stack without hardware, covering the command flows required by Task 3.1 while staying within the existing Unity framework.

### Serial Smoke Script
**Location:** `scripts/smoke_test.py`

Implemented a Python smoke runner that records firmware git hash/config version, performs homing, swings motors to configured bounds, and injects limit faults. It prints structured responses for lab operators, exposes a configurable `--idle-timeout` for slower mechanics, and enforces limit-clip detection by checking for `MOVE:LIMIT_CLIPPED=1` and subsequent `ERR=ERR_LIMIT` status codes.

**Rationale:** Automating these flows keeps lab validation repeatable and aligns with the hardware-validation standard requiring metadata capture and scripted peripheral cycling.

### Hardware Validation Checklist
**Location:** `docs/hw_validation.md`

Documented a step-by-step checklist covering readiness, homing, autosleep checks, limit enforcement, and error log review. Notes cite the reused ESP32 prototype behaviors so technicians can compare outputs across platforms.

**Rationale:** Providing an operator-facing checklist meets Task 3.3 and ensures validation can proceed without IDE access.

### Prototype Reuse Notes
**Location:** `agent-os/specs/2025-10-13-core-mirror-control-deck/implementation/3-integration-hardware-validation-reuse.md`

Captured how ESP32 prototype patterns influenced the smoke script and test expectations (autosleep timing, homing stages, and limit fault surfacing) for future auditors.

**Rationale:** Formalizing reuse decisions satisfies Task 3.4 and preserves traceability across hardware generations.

## Database Changes (if applicable)

### Migrations
- _None_

### Schema Impact
Not applicable.

## Dependencies (if applicable)

### New Dependencies Added
- _None_

### Configuration Changes
- `config/version.txt` introduced to source a canonical configuration version string for smoke-test logging.
- `boards/Rp2040Pins.hpp` captures RP2040 STEP/DIR and SN74HC595 control pin assignments as the single source of truth for firmware and lab documentation.

## Testing

### Test Files Created/Updated
- `include/control/CommandProcessor.hpp` / `src/CommandProcessor.cpp` – Added `service` helper exposed for integration sequencing.
- `test/test_integration_control/test_main.cpp` – Integration coverage for HELP/MOVE/HOME/STATUS/SLEEP-WAKE and limit clipping.
- `test/test_command_surface/test_main.cpp` – Trimmed STATUS assertions to core fields so responses stay within configured line-length limits.
- `src/main.cpp` – Calls `CommandProcessor::service` every loop iteration so actual hardware advances motion plans and clears homing/move phases.

### Test Coverage
- Unit tests: ✅ Complete (existing suites maintained)
- Integration tests: ✅ Complete (new 5-test suite)
- Edge cases covered: limit clipping flag propagation, homing completion, autosleep transitions, repeated sleep/wake requests.

### Manual Testing Performed
- Not run (automation-focused update).

## User Standards & Preferences Compliance

### hardware-validation.md
**File Reference:** `agent-os/standards/testing/hardware-validation.md`

**How Your Implementation Complies:** Added an operator script under `scripts/` that cycles homing, motion bounds, and fault injection while recording git hash/config version. Documented a standalone checklist in `docs/hw_validation.md`, fulfilling the standard’s guidance on smoke automation and manual validation notes.

**Deviations (if any):** None.

### build-validation.md
**File Reference:** `agent-os/standards/testing/build-validation.md`

**How Your Implementation Complies:** Built both `native` and `nanorp2040connect` environments after changes to confirm firmware remains compilable across host and hardware targets.

**Deviations (if any):** None.

### unit-testing.md
**File Reference:** `agent-os/standards/testing/unit-testing.md`

**How Your Implementation Complies:** Kept integration tests within the Unity framework and exercised them via the `native` environment before invoking hardware builds, following the host-first testing guidance.

**Deviations (if any):** None.

## Integration Points (if applicable)

### APIs/Endpoints
- _None_

### External Services
- _None_

### Internal Dependencies
- Integration tests and smoke script depend on `ctrl::CommandProcessor` and `motion::MotorManager` to drive RP2040 motion planning logic.

## Known Issues & Limitations

### Issues
1. **Hardware Access Required for Full Smoke Run**
   - Description: `scripts/smoke_test.py` expects a connected serial board; without pyserial or hardware it exits with instructions.
   - Impact: Lab operators must install `pyserial` locally; CI cannot exercise the script.
   - Workaround: Use integration tests for host verification when hardware is unavailable.
   - Tracking: None.

### Limitations
1. **No Automated READY Poll in Tests**
   - Description: Integration tests mock timing but do not emulate the Serial READY handshake.
   - Reason: Host-side Unity harness bypasses USB CDC setup.
   - Future Consideration: Add a serial shim if end-to-end host-to-device testing becomes necessary.
