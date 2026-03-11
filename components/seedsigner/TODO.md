# TODO (seedsigner C modules)

## Screen behavior / parity

- [ ] `button_list_screen`: `initial_selected_index` JSON config applies the initial highlight in
  hardware mode only. Touch mode does not apply it (no button is pre-highlighted on load).
  If touch-mode pre-selection is needed for specific flows, extend `nav_bind` to call
  `update_visual_focus` (or directly `button_set_active`) when a valid `initial_body_index`
  is provided, regardless of input mode.

## Architecture separation (long-term)

- [ ] Migrate desktop/tooling LVGL dependency to standalone pinned clone workflow (e.g.,
  `third_party/lvgl`) rather than ESP-IDF managed component discovery.
- [ ] Separate LVGL screen/core modules from ESP-specific integration so screen layer can
  compile/run without ESP target dependencies.
