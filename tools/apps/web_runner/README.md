# web_runner — Browser (WASM) Screen Playground

An interactive, browser-based playground for the SeedSigner LVGL screens. It
compiles the **same** platform-agnostic screen code (`components/seedsigner/`)
to WebAssembly via Emscripten and renders the real LVGL screens to an HTML
`<canvas>`. You can:

- pick a **language**, a **screen**, and a **scenario** (the prebuilt scenarios
  from `tools/scenarios/scenarios.json`, localized per language),
- **live-edit the JSON** that drives the screen and watch it re-render,
- drive it with **keyboard nav + mouse/touch**, plus an **on-screen joystick**
  (D-pad, center-select, KEY1/2/3) so hardware-input mode works with no keyboard,
- switch **resolution** (240×240 / 320×240 / 480×320 / 800×480), toggle **input
  mode**, **zoom**, and go **fullscreen**.

The current selections (**language, screen, scenario, resolution, input mode,
zoom**) are mirrored into the URL query string, so a refresh — or a shared link —
restores exactly where you were.

It shares its LVGL/display/input plumbing with the native `screen_runner` via
`tools/apps/runner_core/runner_core` (SDL-free) and `tools/apps/runner_core/runner_sdl`.

## Architecture: engine + runtime assets

The bundle is **multi-file**: the `.wasm`/`.js`/`.html` is the *engine*, and the
fonts + scenarios are plain static files **fetched at runtime**:

```
build-wasm/
  index.html  index.js  index.wasm        # the engine (rebuild only on C++/shell change)
  assets/
    scenarios/<locale>.json               # raw localized catalogs (one per locale)
    scenarios/locales.json                # the language picker's ordered index
    lang-packs/<locale>/*.ttf             # script/CJK font packs (Greek/Cyrillic/Vietnamese/CJK)
```

This means a **translation / scenario / font edit is just a file swap** — no WASM
rebuild. The page fetches `assets/scenarios/<locale>.json` and expands its
variations client-side (RFC 7396 merge-patch); on a language switch it fetches the
locale's font pack(s) and hands the bytes to the engine to register over the baked
baseline. Latin-baseline languages (English, Spanish, German, …) need no pack — the
compiled-in OpenSans Western floor covers them.

## Build

Emscripten is provided by a Docker image (the official `emscripten/emsdk`
extended with **ccache** — see `Dockerfile`), so no host toolchain is needed:

```bash
bash tools/apps/web_runner/build.sh
# or a different default resolution:
DISPLAY_WIDTH=480 DISPLAY_HEIGHT=320 bash tools/apps/web_runner/build.sh
```

The first run pulls the base image and compiles the SDL2 port + LVGL; ccache and
the Emscripten port cache are persisted under `tools/apps/web_runner/.ccache` and
`.emcache`, so subsequent builds are fast. The build emits the engine
(`index.{html,js,wasm}`) and then stages the runtime assets next to it
(`stage_assets.py` — scenario catalogs + font packs + the `locales.json` index).
The font packs must exist under `lang-packs/` first; regenerate them with
`tools/i18n/build_fontpacks.py`, or pass `REGEN_PACKS=1 bash …/build.sh` to rebuild
them as part of the build.

## Run (serve + live-edit)

`serve.sh` does two things at once: it serves the bundle over HTTP **and** watches
`tools/scenarios/scenarios.json`, auto-regenerating the localized catalogs +
re-staging the assets on every save. Because assets are fetched at runtime, the
loop is just **edit `scenarios.json` → hard-refresh the browser** — no rebuild.

```bash
bash tools/apps/web_runner/serve.sh          # port 8000 (or: serve.sh 9000)
#   Local:   http://localhost:8000/index.html
#   LAN:     http://192.168.x.y:8000/index.html
#   Tailnet: http://<machine>.<tailnet>.ts.net:8000/index.html
```

It binds all interfaces (`0.0.0.0`), so the page is reachable from external clients
on the **LAN** and the **tailnet** — the intended loop when building on a remote
machine. (If a host firewall is active, allow the port, e.g. `sudo ufw allow
8000/tcp`.) The multi-file bundle is served over HTTP and cannot be opened from
`file://` (the asset fetches would be blocked).

Rebuild (`build.sh`) only when the C++ or `shell.html` changes. Font pack changes
need `tools/i18n/build_fontpacks.py`; scenario/translation changes need nothing but
the running `serve.sh`.

## CI / GitHub Pages

The screenshot gallery and this playground are published together by a single
workflow, `.github/workflows/pages.yml`, using the **official** GitHub Pages
action (`actions/upload-pages-artifact` + `actions/deploy-pages`):

- Combined site: gallery at `https://<owner>.github.io/<repo>/`, playground at `…/play/`.
- `ci.sh build-web-runner` builds the engine and stages the assets (regenerating
  the font packs from the `seedsigner-translations` submodule + fontTools);
  `ci.sh assemble-site` copies `index.{html,js,wasm}` **and** `assets/` to `…/play/`.
- **Deploys only on push to `main` / manual dispatch** (trusted, post-merge).
- **`pull_request` runs are read-only**: they build the site (uploaded as the
  `site` artifact) and run a read-only screenshot regression diff (base vs PR)
  surfaced in the job summary + a `screenshot-diff` artifact. No PR holds a write
  token.

Security notes: the top-level token is `contents: read`; only the deploy job takes
the minimal `pages: write` + `id-token: write` scopes (no `contents: write`
anywhere, nothing pushes to a branch). All actions are pinned to commit SHAs.
**One-time setup:** Settings → Pages → Source = "GitHub Actions"; optionally
restrict the `github-pages` environment to `main`. See
`docs/knowledge/fork-pr-ci-token-permissions.md`.

## How it fits together

- `web_runner.cpp` — Emscripten entry point: sets up the SDL canvas + LVGL via
  `runner_core`, runs the cooperative main loop, and exposes the exported C API.
  Screen control: `ss_load_screen`, `ss_set_resolution` (resize only — the host
  re-registers fonts + reloads, so a resize never flashes the wrong font),
  `ss_set_input_mode`, `ss_send_key`, `ss_scroll`, `ss_get_width/height`. Locale
  fonts: `ss_locale_font_plan` (the render layer's per-resolution `{role,px,file}`
  plan), `ss_begin_locale`, `ss_register_font` (copies JS-fetched `.ttf` bytes and
  keeps them alive for tiny_ttf's lazy reads). Screen results forward to
  `window.ssOnResult`.
- `shell.html` — the page chrome + JS glue: language/screen/scenario selectors,
  JSON editor, on-screen joystick, results log; fetches + expands the locale
  catalogs (client-side merge-patch); fetches + registers font packs (fetch-first,
  then a synchronous swap, so switching between scrolling-title locales can't
  use-after-free a freed font); mirrors state into the URL and restores it on load.
- `stage_assets.py` — stages the supported locales' scenario catalogs + font packs
  + `locales.json` next to the bundle (used by both `build.sh`/`serve.sh` and CI).
- `CMakeLists.txt` — reuses the LVGL + seedsigner sources (incl. `font_registry` +
  `locale_fonts`) and the shared `runner_core`/`runner_sdl`; links with
  `-sUSE_SDL=2 -fexceptions` and exports the locale fns + `_malloc`/`_free`. No
  `-sSINGLE_FILE` (multi-file) and no embedded scenarios (fetched at runtime).

## Notes

- C++ exceptions are enabled (`-fexceptions`) because the screens throw on
  malformed JSON; `ss_load_screen` catches and keeps the last good render, and
  the editor pre-validates with `JSON.parse`.
- Touch works on phones/tablets via SDL2 touch→mouse emulation; the page sets
  `touch-action: none` and disables pinch-zoom so gestures drive the screen.
- `fa`/`hi`/`th` (Arabic/Persian, Devanagari, Thai) are intentionally absent from
  the language list — no font packs yet (Phase 2); they would render as boxes.
