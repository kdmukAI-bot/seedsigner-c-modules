# Input Navigation + Desktop Runner Implementation Plan

Status: **active tracker** (delete after completion)
Owner: SeedSigner C modules workstream
Canonical repo: `/home/keith/.openclaw/workspace-mp-project-lead/dev/seedsigner-c-modules`

## Purpose

Provide a restart-safe implementation checklist for:
1) hardware-input-aware C-module screens (joystick + KEY1/2/3), and
2) a fast desktop interactive runner to avoid slow ARMv6 emulation loops.

This plan is execution-oriented and intended to be checked off as work progresses.

---

## Behavioral contract (authoritative intent)

- Focus zones: `TOP_NAV` and `BODY`.
- `UP/DOWN` handle zone transitions where appropriate.
- BODY navigation depends on layout:
  - vertical: `UP/DOWN` traverse, `LEFT/RIGHT` no-op
  - grid (e.g., main menu 2x2): all directions active via neighbor mapping
- KEY1/2/3 defaults to ENTER behavior.
- KEY1/2/3 may be overridden per screen (`enter|noop|emit|custom`), without global hardwired ESC/HOME assumptions.
- Default selection policy:
  - touch mode: no default active button unless only one button exists
  - hardware mode: always default-select a BODY control (never TOP_NAV)

Reference behavior spec (hardware-facing):
- `../seedsigner-raspi-lvgl/docs/input-button-behavior.md`

---

## Architecture decisions (locked)

- Input mode is **runtime** profile, not build flag.
- Global input profile is set once at init in production (touch vs hardware).
- Optional test override can switch mode live in desktop runner.
- Screen implementations should read a shared profile/helper and avoid duplicated mode branching.
- **Keypad sink pattern**: in hardware mode, a single 1×1 transparent object is the sole LVGL
  group member. All keypad indev key events route to it via `nav_key_handler`. Body and top-nav
  items are never in the group, preventing LVGL auto-focus interference.
- Top-nav has at most one button visible at a time (back XOR power, never both).

---

## Work phases

## Phase 1 — Shared navigation layer foundation

- [x] Add reusable nav module (`components/seedsigner/navigation.h/.cpp`)
- [x] Add zone/body-layout model and KEY1/2/3 policy scaffolding
- [x] Wire `button_list_screen` as first implementation target
- [x] API documented in headers; lifecycle cleanup stable under repeated screen swaps

---

## Phase 2 — Input mode/profile plumbing

- [x] Add global runtime input profile API (set/get) in seedsigner component layer
- [x] Enforce default-selection policy from profile (touch vs hardware)
- [x] Add optional `initial_selected_index` support for screens that need explicit initial focus
  - Note: applies in hardware mode; touch mode pre-selection is a pending TODO
- [x] Keep per-screen config override optional and minimal (no pollution)

---

## Phase 3 — Layout-specific navigation rollout

- [x] `button_list_screen` vertical behavior + top-nav transitions
- [x] `main_menu_screen` grid (2x2) directional neighbor behavior
- [x] BODY default focus in BODY for hardware mode
- [x] Top-nav enter/exit behaves predictably from each layout
- [x] Scrollable list auto-scrolls to keep focused item visible (`lv_obj_scroll_to_view`)

---

## Phase 4 — Desktop interactive runner (fast loop)

Goal: fast host iteration without ARMv6 emulation.

- [x] Desktop runner target (`tools/screen_runner/`) — SDL2 + LVGL host build
- [x] Loads `tools/scenarios.json` as scenario source
- [x] PageUp/PageDown (and [/] ,/.) cycle scenarios
- [x] Keyboard input mapped: arrows → UP/DOWN/LEFT/RIGHT, Return → ENTER, 1/2/3 → KEY1/KEY2/KEY3
- [x] Hardware mode active by default on runner start
- [x] `seedsigner_lvgl_on_button_selected` override logs selection to terminal
- [ ] Runner chrome: scenario dropdown selector (mouse-first, keyboard supported)
- [ ] Hotkey to open scenario selector

Exit criteria (revised):
- Basic interactive runner is functional. Chrome dropdown is a convenience enhancement, not a
  blocker for hardware-nav validation.

---

## Phase 5 — Validation + regression

- [ ] Manual nav validation matrix (vertical + grid + top-nav transitions)
- [ ] KEY1/2/3 default + override validation
- [ ] Touch behavior regression check
- [ ] Optional scripted key-sequence replay for deterministic checks

---

## Completion / cleanup policy

When all phases are done:
1) confirm behavior against spec,
2) move any lasting rules into durable docs,
3) delete this implementation-plan file.
