# Core Mirror Control Deck Hardware Validation Checklist

Follow these steps after flashing new firmware builds. All checks can be executed from a terminal without opening the IDE.

## 1. Record Metadata
- Run `scripts/smoke_test.py --port <tty>` to capture the firmware git hash and config version in the session log.
- Note the test operator, board serial, and power supply voltage in the lab logbook.
- Increase the command’s `--idle-timeout` flag (default 25 s) if homing or long moves require additional settle time.

## 2. Serial Readiness
- Power cycle the RP2040 deck and confirm `CTRL:READY` appears within 2 seconds of reset.
- Issue `HELP` and verify each verb (MOVE, HOME, STATUS, SLEEP, WAKE) is listed; compare against ESP32 prototype cheat-sheet for consistency.

## 3. Homing Verification
- Execute the smoke script homing pass or run `HOME:0` manually.
- Confirm the channel reports `STATE=IDLE` and `POS=0` after completion.
- Cross-check that autosleep re-engages (`SLEEP=1`) within one status poll.

## 4. Motion Bounds & Autosleep
- Run the scripted +/- limit sweep (default ±1200 steps).
- Observe that motors wake before motion (`SLEEP=0`) and return to sleep immediately once the move finishes.
- Listen/feel for driver silence after the move to confirm SN74HC595 sleep gating matches the ESP32 prototype behavior.
- If wiring needs inspection, consult `boards/Rp2040Pins.hpp` for the authoritative STEP/DIR and shift-register GPIO assignments.

## 5. Fault / Limit Handling
- Trigger an out-of-range move (e.g., `MOVE:0,2000`) and verify:
  - Response includes `MOVE:LIMIT_CLIPPED=1`.
  - Subsequent `STATUS` reports `ERR=ERR_LIMIT`.
- Clear the condition with `WAKE` + in-range move; ensure `ERR=OK` is restored.

## 6. Error Report Review
- Run `STATUS` for all channels and confirm last-error codes reset to `ERR=OK` after successful moves.
- Document any deviations or unexpected error codes in the lab tracker before proceeding.

## 7. Final Autosleep Confirmation
- After the above flows, leave the system idle for 60 seconds and verify all channels report `SLEEP=1`.
- Compare idle current draw to the ESP32 prototype baseline (<30 mA increase when asleep).
