Select a language | 选择语言

[**简体中文**](README_cn.md)  
[**English**](README.md)  

# Debug Text

A **minimalist and lightweight debug text (bitmap font) drawing library** based on **SDL3**, **supporting UTF-8 character sets** (including CJK)

## Advantages
- No need to **pre-load any font files** at runtime
- **Header-Only design**. Just include `DebugText.hpp` plus the font headers you want and **you're ready to compile**, **no extra configuration needed**
- **Cross-platform**
- **Multiple fonts can coexist** and be selected by FontID (string), e.g. 8/10/12px variants

## Documentation

- [**API reference (v2.0, Chinese)**](docs/API_cn.md): every public function, type, macro, return-value convention and behavioral detail

## Project Structure

```
├── src/
│   └── generator/              # One-stop font generator (TTF -> font header)
│       ├── main.cpp            # CLI entry (--all / --ttf ... / --preview)
│       ├── FontBuilder.*       # Rasterization + v2 packing
│       ├── HeaderEmitter.*     # Emits header (constexpr arrays + auto registration)
│       └── Preview.*           # Optional SDL preview window
├── test/
│   ├── loadtest.cpp            # Multi-font rendering demo (switch with 1/2/3)
│   └── odr_compat.cpp          # Second TU, verifies ODR and shared registry
├── include/                    # Public headers (other helper headers live in ../RecentlyC++Programs)
│   ├── SDL_WindowIM.hpp        # SDL window wrapper (Windows DWM support)
│   ├── DebugText.hpp           # Bitmap font rendering engine (font driven, v2 format)
│   ├── decode.hpp              # UTF-8 decoder
│   ├── fonts/                  # Generated font headers (commit them)
│   │   ├── fusion_pixel_8px_mono_zh_hans.hpp
│   │   ├── fusion_pixel_10px_mono_zh_hans.hpp
│   │   └── fusion_pixel_12px_mono_zh_hans.hpp
│   ├── GL_Commdlg.hpp          # Windows common dialog wrapper
│   ├── GL_DWMoperations.hpp    # DWM rounded window preference settings
│   ├── SDL_EventPlus.hpp       # SDL event polling wrapper
│   ├── SmoothCpp.hpp           # Numeric smoothing / EMA filter
│   ├── StopWatch.hpp           # High-precision timer
│   ├── Times.hpp               # Frame rate control (Timer)
│   ├── TimeCounter.h           # Simple time counter
│   └── UTF8toWide.hpp          # UTF-8 to wide string conversion
├── font/
│   └── fusion-pixel-font-12px-monospaced-ttf-v2026.10.25/  # Fusion Pixel CJK font (with OFL)
├── ../RecentlyC++Programs/     # SDL3 / SDL3_ttf development libraries and helper headers
├── CMakeLists.txt              # CMake build script
├── CMakePresets.json           # CMake presets (debug / release)
├── docs/
│   └── API_cn.md               # API reference (Chinese)
└── .vscode/
    ├── launch.json             # VS Code debug configuration
    └── tasks.json              # VS Code build tasks
```

The extra hpp libraries are for the font generator and demo program. For actual usage, you only need `DebugText.hpp` plus the font headers you want to use (`include/fonts/*.hpp`).

## Dependencies

### Library itself:

- **Compiler:** Any g++ will do
- **C++ Standard:** C++17 or later
- **Graphics Library:** SDL3
- **Link Libraries:** SDL3
- **Platform:** Any platform supported by SDL3

### Generator and test program:

- **Compiler:** MinGW-w64 (g++)
- **C++ Standard:** C++20
- **Graphics Library:** SDL3, SDL3_ttf
- **Link Libraries:** SDL3, SDL3_ttf, dwmapi, imm32, comctl32, comdlg32, shell32, gdi32, ole32, uuid
- **Platform:** Windows only

> SDL3 and SDL3_ttf development libraries live in the sibling folder `../RecentlyC++Programs/` (`SDL3-3.4.14/`, `SDL3_ttf-3.2.2/`), no additional download required.

## Building

This project is built with CMake + CMakePresets (requires CMake 3.21+, Ninja and MinGW-w64 g++).

```bash
# Configure and build the Debug version (output: bin/debug/)
cmake --preset debug
cmake --build --preset debug

# Configure and build the Release version (output: bin/release/)
cmake --preset release
cmake --build --preset release

# Build a single target
cmake --build --preset debug --target generator
cmake --build --preset debug --target loadtest

# Regenerate the font headers (output: include/fonts/)
cmake --build --preset debug --target fonts

# Clean build artifacts
cmake --build --preset debug --target clean
```

Build output goes to `bin/debug/` or `bin/release/`; the SDL3 runtime DLLs are copied next to the executables automatically.
`generator` is a console program; it looks for the preset fonts under `font/` relative to the repository root, or pass `--root`.

### Building in VS Code

- Press `Ctrl+Shift+B` to select a build task
- Press `F5` to automatically compile and start debugging

## Program Descriptions

### generator — One-stop Font Generator

Reads a TTF and writes the bitmap data straight into a C++ font header (no intermediate `font.bin`, no extra conversion script).

```bash
# Regenerate all presets (8/10/12px mono zh_hans) -> include/fonts/*.hpp
cmake --build --preset debug --target fonts

# Or call the CLI directly
bin/debug/generator.exe --all --root .
bin/debug/generator.exe --ttf path/to/font.ttf --id my-font --out include/fonts/my_font.hpp --preview
```

- Uses `TTF_FontHasGlyph()` to auto-detect the highest covered codepoint (astral CJK extension glyphs included)
- Bitmaps are normalized to `cellWidth x cellHeight`; half/full width comes from the advance; rare oversized glyphs go to an `extra` side table
- Deterministic output (no timestamps); unchanged content is not rewritten, so nothing recompiles needlessly
- `--preview` opens an SDL preview window (NEAREST scaling) with live glyph and progress

**v2 font format (all constexpr arrays)**

| Section | Description |
|---------|-------------|
| `presenceBits` + `presenceBlockCounts` | codepoint -> glyph index (bitset + per-256 cumulative counts, popcount gives the index) |
| `fullWidthBits` + `fullWidthBlockCounts` | glyph index -> half/full width (1 bit per glyph) |
| `atlas` | bitmaps packed in codepoint order, record size = ceil(w*h/8) (12px: 9B half / 18B full) |
| `extra*` | side table for glyphs taller/wider than the cell |

Bit meaning: **1 = ink**. Compared with the old `font.bin` format it drops the 65536-entry int32 offset table and the 2-byte per-glyph record header: measured size drop **75.7% / 75.1% / 62.1%** for 8/10/12px.

### loadtest — Multi-font Rendering Test

Draws all three fonts in the same frame by FontID; press `1`/`2`/`3` or `Tab` to switch the current font, `Up`/`Down` to change the scale, `Esc` to quit.

## Usage

```cpp
#include "DebugText.hpp"
#include "fonts/fusion_pixel_10px_mono_zh_hans.hpp"
#include "fonts/fusion_pixel_12px_mono_zh_hans.hpp"

// 1) Select the current font; paintDBText* will use it
dbtext::setFont("fusion-pixel-12px-mono-zh_hans");
paintDBTextFormat(renderer, 10, 10, {0, 0, 0, 255}, 2, {0, 0}, "FPS: %.0f", fps);

// 2) Pick a font per call via FontID
dbtext::paintTextFormat(renderer, dbtext::FontID("fusion-pixel-10px-mono-zh_hans"),
                        10, 40, {0, 0, 0, 255}, 3, {0, 0}, "small font");

// 2b) Foreground + background color (background alpha = 0 means transparent)
dbtext::paintTextFormat(renderer, dbtext::FontID("fusion-pixel-10px-mono-zh_hans"),
                        10, 60, {0, 0, 0, 255}, {255, 236, 179, 255}, 3, {0, 0}, "with background");

// 3) RAII temporary switch
{
    dbtext::FontScope scope("fusion-pixel-10px-mono-zh_hans");
    paintDBText(renderer, 10, 80, {0, 0, 0, 255}, 2, "temporarily 10px");
}

// 4) Measure only / need an SDL_Surface
dbtext::TextSize size = dbtext::measureText(*dbtext::currentFont(), "Hello 中文");
SDL_Surface* surface = dbtext::createTextSurface(dbtext::currentFont(), "Hello", {0, 0, 0, 255});
```

Font headers register themselves when included (`dbtext::AutoRegister`), so including several headers makes them all selectable. Without `setFont`, the current font is `DBTEXT_DEFAULT_FONT_ID` (12px by default, overridable by macro).

## License

- This project is licensed under the MIT License
- The font `fusion-pixel-*-monospaced-zh_hans.ttf` is from [Fusion Pixel](https://github.com/TakWolf/fusion-pixel-font), licensed under OFL (see `font/fusion-pixel-font-*/OFL.txt` and the per-directory `LICENSES/`).
- The generated bitmap data is derived from those fonts; when exporting it back to TTF, do not reuse the reserved font name (the built-in export is named `DebugBitmapFont`).

## Backstory

Originally, I wanted to render text on an STM32 LCD screen, but the default library only supports English, so I decided to make a font bitmap myself.  
After finishing it, I realized this approach could also be useful in everyday development — not having to load font files in advance is really convenient!  
And thus this library was born.

## Using with ImGui

Theoretically possible, but I'm not a fan of ImGui, so if anyone is interested, feel free to write an adapter.
