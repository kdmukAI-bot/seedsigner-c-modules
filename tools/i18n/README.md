# tools/i18n — offline internationalization tooling

Offline "Produce" tooling for SeedSigner's multi-language support. Self-contained (no Babel): it parses
`.po` catalogs directly and shells out only to `fontTools`. It never duplicates the render layer's
locale table — it reads that from the binary via `screenshot_gen --dump-locales`.

See `docs/font-and-i18n-rendering.md` (design) and `docs/font-and-i18n-implementation-plan.md` (status).

## Two independent steps

The font pack (production) and the localized scenarios (test-only) are produced by **separate** tools —
each writes to where its consumer expects it:

- **`build_fontpacks.py`** — the font-pack builder. Derives the locale set from
  `screenshot_gen --dump-locales` (the render layer's source of truth; `--locale` restricts it), and
  subsets the vendored source TTF (`components/seedsigner/assets/`) with `fontTools` per locale. It runs in
  one of two modes, chosen by whether the manifest entry carries a `unicode_range`:
  - **corpus mode** (CJK, "primary" chain) — extracts the locale's glyph corpus from its `.po` and subsets
    the source Noto TTF to exactly those glyphs. Output → **`lang-packs/<locale>/<locale>.ttf`** (one
    single-weight `.ttf`). Drops GSUB/GPOS/GDEF (CJK needs no shaping); Arabic/Thai (Phase 2) must keep
    layout tables + presentation forms.
  - **block-range mode** (Greek/Cyrillic/Vietnamese, "fallback" chain) — subsets OpenSans to a *fixed
    Unicode block* (e.g. `U+0400-04FF` for Cyrillic) declared by the render layer, **not** the `.po` corpus,
    so a translation edit can never change the pack or its signature. Produces two weights to match the
    compiled-in Western baseline these chain under. Output →
    **`lang-packs/<locale>/<locale>_{regular,semibold}.ttf`**. Keeps layout tables (Vietnamese mark
    positioning may need GPOS).

  Every pack also gets a `manifest.json` with sha256s — production-ready. One `.ttf` serves all
  sizes/resolutions (Tiny TTF rasterizes on demand). Subsets **exclude ASCII** (the compiled-in OpenSans
  Western baseline covers it; embedded English defers to it via the fallback chain — needs
  `third_party/patches/lv_tiny_ttf-fallback-chain.patch`).
- **`gen_localized_scenarios.py`** — translates the `title`/`text`/`status_headline`/`button_list` leaves
  of `tools/scenarios/scenarios.json` via a locale's catalog (English passthrough for non-msgids). Output →
  **`tools/scenarios/localized/<locale>.json`**, which the desktop apps load unchanged. (`en` is the
  identity passthrough.) This is **test-only** output and is *not* part of a deployable font pack.
- **`po_catalog.py`** — the shared `.po` reader. `parse_catalog()` → `{msgid: msgstr}` (non-empty only);
  `corpus_chars()` → the unique glyph set used by a locale's translations (non-ASCII by default). Imported
  by both tools above.
- **`supported_locales.json`** — the shared canonical locale list: each entry is `{code, english, native}`,
  plus `pack_locales` (the subset that ships a fetched/registered font pack — the script + CJK packs; the
  rest are covered by the compiled-in OpenSans Western baseline). Consumed by the multi-language gallery
  (`tools/apps/screenshot_generator/gen_gallery.py`) and the web playground's language list
  (`tools/apps/web_runner/stage_assets.py`).

`lang-packs/` and `tools/scenarios/localized/` are gitignored — reproducible from the committed source
(`tools/scenarios/scenarios.json` + the catalogs + the vendored fonts).

## Prerequisites

- `tools/i18n/seedsigner-translations` submodule initialized (`.po` catalogs).
- `fontTools` (`pip install fonttools`).
- A built `screenshot_gen` (provides `--dump-locales`).

## Typical run

```bash
git submodule update --init tools/i18n/seedsigner-translations
# build screenshot_gen first (any resolution; it compiles all profiles)
cmake -S tools/apps/screenshot_generator -B tools/apps/screenshot_generator/build \
      -DDISPLAY_WIDTH=240 -DDISPLAY_HEIGHT=240 && cmake --build tools/apps/screenshot_generator/build -j

# font packs → repo-root lang-packs/ (production-ready)
python3 tools/i18n/build_fontpacks.py
# localized scenarios → tools/scenarios/localized/ (test input for the apps)
python3 tools/i18n/gen_localized_scenarios.py

# then render a locale (the consumer step):
./tools/apps/screenshot_generator/build/screenshot_gen \
      --locale zh_Hans_CN --scenarios-file tools/scenarios/localized/zh_Hans_CN.json \
      --font-dir lang-packs --out-dir tools/apps/screenshot_generator/screenshots/i18n/zh_Hans_CN
```

`--locale` is repeatable / omittable. `build_fontpacks.py` defaults to all locales the manifest declares;
`gen_localized_scenarios.py` defaults to `en` + every available catalog. Build just one font pack:
`build_fontpacks.py --locale ja`.
