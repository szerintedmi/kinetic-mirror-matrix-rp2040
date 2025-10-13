# Core Mirror Control Deck Firmware

## Serial Command Surface

The RP2040 firmware exposes a text-based serial control deck using `<VERB>[:payload]\n` framing. Each command returns acknowledgements prefixed with `CTRL:` and optional detail lines.

The built-in `HELP` verb emits the following documentation:

```
CTRL:OK
HELP:HELP|HELP|List supported verbs and payload formats.
HELP:MOVE|MOVE:<channel>,<position>[,<speed>[,<accel>]]|Queue an absolute move with optional speed/accel overrides.
HELP:HOME|HOME:<channel>|Initiate the homing routine for the provided channel.
HELP:STATUS|STATUS[:<channel>]|Report state, position, and last error for one or all motors.
HELP:SLEEP|SLEEP:<channel>|Force a motor channel into low-power sleep.
HELP:WAKE|WAKE:<channel>|Wake a motor channel before additional commands.
```

### Command Summary

| Verb   | Payload Format                                     | Description                                                                 |
| ------ | -------------------------------------------------- | --------------------------------------------------------------------------- |
| `HELP` | _none_                                             | Lists the supported verbs along with payload formatting guidance.           |
| `MOVE` | `<channel>,<position>[,<speed>[,<accel>]]`         | Queues an absolute move and optionally overrides speed (Hz) and acceleration.|
| `HOME` | `<channel>`                                        | Reserved for Task Group 2 implementation; currently returns `CTRL:ERR_NOT_READY`. |
| `STATUS` | optional `<channel>`                            | With no payload returns an entry per motor. With a channel reports a single motor. |
| `SLEEP` | `<channel>`                                       | Forces the requested channel into driver sleep, reporting the resulting state. |
| `WAKE` | `<channel>`                                        | Wakes the requested channel and clears sleep state prior to motion commands. |

### Response Codes

All responses are prefixed with `CTRL:` followed by a status code. Available codes include `OK`, `ERR_UNKNOWN_VERB`, `ERR_PAYLOAD_TOO_LONG`, `ERR_EMPTY`, `ERR_VERB_TOO_LONG`, `ERR_MISSING_PAYLOAD`, `ERR_INVALID_CHANNEL`, `ERR_PARSE`, `ERR_INVALID_ARGUMENT`, and `ERR_NOT_READY`.

### Defaults

- `MOVE` defaults to `4000 Hz` speed and `16000 Hz/s` acceleration when optional fields are omitted.
- Motors boot in sleep and report positions of `0` until moves update state.
- Structured `STATUS` output emits `STATUS:CH=<id> POS=<pos> TARGET=<target> STATE=<state> SLEEP=<0|1> ERR=<code> SPEED=<hz> ACC=<hz_per_s>`.

### Future Integration Points

- Homing routines (`HOME`) will be implemented alongside the motion manager work in Task Group 2.
- Status reporting will incorporate live motion state once the motion engine and autosleep routines are connected.

