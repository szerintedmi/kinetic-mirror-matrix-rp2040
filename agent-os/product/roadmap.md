# Product Roadmap

1. [ ] Core Mirror Control Deck — Bring up the RP2040 command set over serial with reliable step/dir output and per-motor sleep so makers can send repeatable moves from a laptop. `M`
2. [ ] Homing & Baseline Calibration — Add bump-stop homing cycles, safety timeouts, and saved zero offsets so every panel snaps back to a known pose before experiments. `S`
3. [ ] Targeting Geometry Core — Ship a shared math library plus a CLI harness that converts wall coordinates into mirror angles with reachability checks, feeding the control deck. `M`
4. [ ] Power-Aware Motion Scheduler — Layer in concurrency caps and thermal throttling that queue commands intelligently, keeping motion smooth without cooking the rig. `M`
5. [ ] Quick Health Readouts — Surface node status, homing state, and recent faults via a lightweight dashboard or CLI summary to catch wobbly mirrors early. `S`
6. [ ] Pattern Sequencer & Presets — Let users script, store, and replay reflection cues from disk so meetups can share their favorite light routines. `M`
7. [ ] Live Play Hooks — Expose a minimal API for controllers, sensors, or creative-coding tools to nudge mirrors in real time while still honoring scheduler limits. `M`
8. [ ] Scene Sandbox Preview — Provide a desktop visualization that replays planned cues, timing, and reach limits so teams can iterate before hauling gear onsite. `M`
9. [ ] Multi-Island Sync — Orchestrate two or more panels with synchronized timelines and shared power budgets for larger wall coverage. `L`
