# Farsi (Persian) support — RTL + Arabic shaping plan (Phase 2)

_Status: **planned, not started.** Next-session implementation plan. Farsi (`fa`) is
deliberately absent from `tools/i18n/supported_locales.json` today (it would render
as `.notdef` boxes). Companion to `docs/font-and-i18n-rendering.md` and
`docs/font-tiering-plan.md` (Farsi is the first **Phase 2** language pack)._

## TL;DR — it's feasible with the existing stack (no HarfBuzz)

Arabic/Persian is the first script that needs **two things the Latin/Greek/Cyrillic/
CJK tiers didn't**: (1) **cursive shaping** (each letter has isolated/initial/medial/
final forms), and (2) **RTL bidi** (right-to-left, with embedded LTR runs for Latin
technical terms + digits). Both are available in-tree:

- **Shaping:** LVGL's built-in `LV_USE_ARABIC_PERSIAN_CHARS` (`src/misc/lv_text_ap.c`)
  rewrites the logical Arabic/Persian code points into **presentation-form** code
  points (U+FE70–FEFF, a few U+FB50–FDFF) *before* glyph lookup. So tiny_ttf only
  ever has to rasterize a presentation-form code point by number — **no GSUB / no
  HarfBuzz**. This is "basic" shaping (the 4 positional forms + the common
  lam-alef ligatures), not full OpenType shaping, but it's correct for SeedSigner's
  UI strings.
- **Bidi:** LVGL's `LV_USE_BIDI` (Unicode bidi algorithm) handles the RTL paragraph
  direction and mixed LTR runs.
- **Font:** `components/seedsigner/assets/NotoSansAR-Regular.ttf` is already vendored
  and **contains the presentation-form glyphs the shaper emits** (verified:
  141 glyphs in U+FE70–FEFF, 631 in U+FB50–FDFF, plus the 256 base U+0600–06FF). So
  the same lv_tiny_ttf path used for CJK works for Farsi.

The hard/unknown part is **not the glyphs — it's the RTL layout** of the screens
(they were all built LTR). That's the bulk of the work; budget accordingly.

## Implementation steps

### 1. Turn on bidi + Arabic shaping in the LVGL build
The desktop tools build with `LV_CONF_SKIP` (no `lv_conf.h`), so features are enabled
as compile definitions (like `LV_USE_TINY_TTF=1`). Add to **every** desktop target's
`target_compile_definitions` (screenshot_generator, screen_runner, web_runner,
runner_core/test) and to the ESP32 component build:

```
LV_USE_BIDI=1
LV_USE_ARABIC_PERSIAN_CHARS=1
```

For the firmware (`raspi-lvgl`, `micropython-builder`) set the same in their
`lv_conf.h`. Note `LV_USE_BIDI` enables the bidi processor globally; the default base
direction stays auto/LTR unless a label/screen sets RTL (step 4).

### 2. Build a Farsi font pack from NotoSansAR
`tools/i18n/build_fontpacks.py`. Two viable approaches — **prefer block-range** (it's
corpus-independent, like the Greek/Cyrillic/Vietnamese packs, so a translation edit
never changes the pack):

- **Block-range (recommended):** subset NotoSansAR to the base block **plus the
  presentation forms the shaper actually emits** — start from `U+0600-06FF`
  (Arabic) + `U+FE70-FEFF` (Presentation Forms-B) + the Persian-specific forms in
  `U+FB50-FDFF` (e.g. peh/tcheh/jeh/gaf/farsi-yeh forms ≈ U+FB56-FB95, U+FBFC-FBFF —
  VERIFY against `lv_text_ap.c`'s `ap_chars_map` and a real Farsi `.po`). Drop GSUB
  (`--drop-tables+=GSUB,GPOS,GDEF`) — LVGL's basic shaper does the form selection, so
  the font's OpenType layout is unused. Two weights? Noto Sans Arabic ships a single
  Regular; like the CJK packs, one weight serves every role.
- **Corpus:** subset to the `.po` corpus, but you must also pull in each corpus
  char's presentation forms (run the same base→form mapping LVGL uses). More code,
  smaller pack. Only worth it if the block-range pack is too big for the SD tier.

The block-range pack should be a few tens of KB (the whole font is 194 KB). Wire it
the same way the other packs are produced (`lang-packs/fa/...` + manifest).

### 3. Add the `fa` entry to the render layer
`components/seedsigner/locale_fonts.{h,cpp}`:
- Add a `fa` → `NotoSansAR` entry. Chain: **`ChainRole::Primary`** like CJK (the
  Arabic font is primary; the baked OpenSans Western baseline stays as fallback so
  embedded Latin technical terms / digits still render). Pick per-role sizes (start
  at the CJK bump and tune for Arabic x-height legibility).
- Add an **RTL marker** to `LocaleFontEntry` (e.g. `bool rtl`) so the platform/
  screen layer knows to set base direction RTL for this locale. `locale_role_render_px`
  / the manifest are otherwise unchanged.
- Decide block-range vs corpus via the existing `unicode_range` field (block-range)
  or the corpus path (no `unicode_range`).

### 4. RTL layout (the big one)
With `LV_USE_BIDI` on, set the **base direction** to RTL for Farsi screens — per-label
`lv_obj_set_style_base_dir(obj, LV_BASE_DIR_RTL, 0)` or on the screen root. Then audit
each screen in `components/seedsigner/` for mirroring:
- `top_nav` (components.cpp): back-chevron and power button swap sides; title
  alignment flips. The scrolling-title marquee direction.
- Button lists / `button_list_screen`: text alignment (right), any leading icons.
- `large_icon_status_screen`, passphrase keyboard (the keyboard stays **LTR/ASCII** —
  passphrase input is not translated; only the chrome/labels flip).
- Right-align body text; check padding/edge constants that assume LTR.
This is invasive and the main risk/effort. Consider a single "is this locale RTL?"
switch feeding alignment + base_dir, rather than per-call flips.

### 5. Enroll the locale + verify
- Add `fa` to `tools/i18n/supported_locales.json` (`pack_locales` too). Display label
  "Persian (فارسی)" — note the native name itself is RTL; the picker `<select>` will
  show it fine.
- Regenerate localized scenarios (`gen_localized_scenarios.py` — the `fa` `.po` exists
  in the `seedsigner-translations` submodule) and the font pack.
- Render `fa` in the screenshot gallery (`gen_gallery.py`) and the web runner; verify:
  letters **connect** (shaping), the paragraph is **right-to-left**, embedded Latin/
  digits stay LTR (bidi), and nothing boxes.

## Open questions / risks for the next session
- **Exact presentation-form ranges** the shaper emits for the Farsi corpus — verify
  against `third_party/lvgl/src/misc/lv_text_ap.c` (`ap_chars_map`) and the actual
  `fa` `.po`, rather than trusting the ranges above. Persian adds پ چ ژ گ ک ی ه gaps
  vs Arabic.
- **RTL layout depth** — how much screen mirroring is "enough"? Text alignment + nav
  button sides is the minimum; full geometric mirroring is more.
- **tiny_ttf × bidi × shaper end-to-end** — confirm the pipeline order (bidi reorders
  → AP shaper substitutes presentation forms → tiny_ttf rasterizes by code point)
  actually holds in LVGL v9.5 with `cache_size=0`. Smoke-test early.
- **Bug #3 (tiny_ttf cache spin)** still applies on-device; desktop `cache_size=0` is
  fine for verification.
- Same `fa` work template applies to the other Phase-2 corpus scripts: **Thai** (`th`,
  no shaping but needs Noto Thai + the combining marks) and **Hindi/Devanagari**
  (`hi`, needs real GSUB shaping → likely HarfBuzz, a bigger lift than Arabic).

## Pointers
- Shaper: `third_party/lvgl/src/misc/lv_text_ap.{c,h}` (`_lv_text_ap_proc`, `ap_chars_map`).
- Font seam: `components/seedsigner/font_registry.cpp`, `locale_fonts.{h,cpp}`,
  `gui_constants.cpp` (`install_western_baseline` / `set_display`).
- Pack tooling: `tools/i18n/build_fontpacks.py` (corpus + block-range modes).
- Vendored font: `components/seedsigner/assets/NotoSansAR-Regular.ttf` (has the
  presentation forms).
