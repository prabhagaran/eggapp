# Button Mapping

!!! info "As-built — describes `task_buttons.cpp` (FW 2.0.0)"
    Replaces the design-phase *Encoder Mapping* page (rotary encoder was not
    fitted; see Design Archive).

## Hardware

| Button | GPIO | Wiring |
|--------|------|--------|
| UP | 32 | active-LOW, internal pull-up |
| DOWN | 33 | active-LOW, internal pull-up |
| OK | 25 | active-LOW, internal pull-up |

Polled every 10 ms by `task_buttons` (Core 1, priority 3) using **ezButton**
with 50 ms debounce. Each press posts one event (`UI_EVT_UP / DOWN / OK`) to
`uiEventQueue` (depth 10, non-blocking send).

## Meaning by context

| Context | UP | DOWN | OK |
|---------|----|------|----|
| Home screen | — | — | enter main menu |
| Menu list | move up (wraps) | move down (wraps) | select item |
| Value edit (temp/hum/hyst/fan/pump/turner) | increase | decrease | save & next/back |
| Date / time field editor | increment field | decrement field | next field → save |
| Factory reset confirm | toggle No/Yes | toggle No/Yes | confirm highlighted choice |
| Device info / sync result | — | dismiss | dismiss |
| **Fault screen** | ignored | ignored | **hold 3 s to reset latch** |

## Rate limiting and safety

- **OK rate limit:** the UI drops OK events arriving within 300 ms of the
  previous one (bounce / double-fire protection).
- **Fault mode:** while the over-temperature latch is active, the button task
  itself stops posting UI events and only measures the OK hold time
  (`FAULT_RESET_HOLD_MS = 3000`). On a valid 3 s hold it clears the latch,
  clears the persisted NVS fault flag, forces all relays off, and logs a
  `FAULT_RESET` event.
- Buttons never touch actuators directly — every action flows through the UI
  state machine and the settings/relay helpers.

## Known limitations / planned

- No auto-repeat yet: long edits (e.g. 0.1 °C steps) need repeated presses —
  hold-to-repeat is item 4.1 in `FEATURE_ROADMAP.md`.
- No long-press Back/Cancel outside the fault screen — item 4.3 in the roadmap.
