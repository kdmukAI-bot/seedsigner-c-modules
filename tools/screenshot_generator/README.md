# screenshot_gen (LVGL screen screenshot generator)

Generates deterministic screenshots from real LVGL screen render paths using JSON-defined scenarios.

## Build (CMake)

From repo root:

```bash
cmake -S tools/screenshot_generator \
      -B tools/screenshot_generator/build
cmake --build tools/screenshot_generator/build -j
```

Executable:

- `tools/screenshot_generator/build/screenshot_gen`

## Usage

```bash
tools/screenshot_generator/build/screenshot_gen [options]
```

Options:

- `--out-dir <path>` output root (default `tools/screenshot_generator/screenshots`)
- `--width <px>` render width (default `480`)
- `--height <px>` render height (default `320`)
- `--scenarios-file <path>` scenario config file
  - default from repo root: `tools/scenarios.json`

## Scenario configuration

Scenario definitions are loaded from JSON.

Schema:

- top-level keys are screen function names
- each screen contains:
  - `context` (base input context)
  - optional `variations` array
- each variation contains:
  - `name`
  - optional `context` (merged onto base context)

`animated` is optional in context and defaults to `false`.

## Output model

The generator runs in **single-output overwrite mode**:

- output directory is cleared before each run
- image assets are written to `img/`
- `manifest.json` is regenerated each run with dynamic presentation data


## Animated GIF behavior

For scenarios marked animated in context:

- if ImageMagick is available (`magick` or `convert`), a GIF is generated
- temporary frame files are cleaned up after GIF generation

## Dependencies

- `nlohmann/json` is fetched automatically by CMake (`FetchContent`)
- `libpng` is required for PNG output (`sudo apt install -y libpng-dev` on Ubuntu/Debian)
