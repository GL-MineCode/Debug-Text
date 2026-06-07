Select a language | 选择语言

[**简体中文**](README.md)  
[**English**](README.en.md)  

# Debug Text

A **minimalist and lightweight debug text (bitmap font) drawing library** based on **SDL2**, **supporting UTF-8 character sets** (including CJK)

## Advantages
- No need to **pre-load any font files** at runtime
- **Header-Only design**. Just include `DebugText.hpp` and **you're ready to compile**, **no extra configuration needed**
- **Cross-platform**

## Project Structure

```
├── src/
│   └── generator.cpp          # Glyph file generator main program
├── test/
│   └── loadtest.cpp            # Bitmap font rendering test program
├── include/                    # Public headers
│   ├── SDLWindowStuff.hpp      # SDL window wrapper (Windows DWM support)
│   ├── AdvancedSDLWindow.hpp   # Advanced SDL window Win32 API wrapper
│   ├── DebugText.hpp           # Bitmap font rendering engine
│   ├── decode.hpp              # UTF-8 decoder
│   ├── fusionpixel_12px_mono.hpp # Embedded 12px bitmap font data
│   ├── GL_Commdlg.hpp          # Windows common dialog wrapper
│   ├── GL_DWMoperations.hpp    # DWM rounded window preference settings
│   ├── SDL_EventPlus.hpp       # SDL event polling wrapper
│   ├── SmoothCpp.hpp           # Numeric smoothing / EMA filter
│   ├── StopWatch.hpp           # High-precision timer
│   ├── Times.hpp               # Frame rate control (Timer)
│   ├── TimeCounter.h           # Simple time counter
│   └── UTF8toWide.hpp          # UTF-8 to wide string conversion
├── font/
│   └── fusion-pixel-12px-monospaced-zh_hans.ttf  # Fusion Pixel CJK font
├── tools/
│   └── toarray.py              # Arbitrary file → C++ byte array (.hpp) converter
├── SDL2-2.24.0/                # SDL2 development libraries (include / lib)
├── SDL2_ttf-2.20.1/            # SDL2_ttf development libraries (include / lib)
├── Makefile                    # Build script
└── .vscode/
    ├── launch.json             # VS Code debug configuration
    └── tasks.json              # VS Code build tasks
```

The extra hpp libraries are for the font generator and demo program. For actual usage, you only need `fusionpixel_12px_mono.hpp` and `DebugText.hpp`.

## Dependencies

### Library itself:

- **Compiler:** Any g++ will do
- **C++ Standard:** C++17 or later
- **Graphics Library:** SDL2
- **Link Libraries:** SDL2main, SDL2
- **Platform:** Any platform supported by SDL2

### Generator and test program:

- **Compiler:** MinGW-w64 (g++)
- **C++ Standard:** C++20
- **Graphics Library:** SDL2, SDL2_ttf
- **Link Libraries:** SDL2main, SDL2, SDL2_ttf, dwmapi, imm32, comctl32, comdlg32, ole32, uuid
- **Platform:** Windows only

> SDL2 and SDL2_ttf development libraries are included in the repository (`SDL2-2.24.0/`, `SDL2_ttf-2.20.1/`), no additional download required.

## Building

```bash
# Build all targets
make

# Build only the generator
make generator

# Build only the loadtest
make loadtest

# Clean build artifacts
make clean
```

Build output goes to the `build/` directory.

### Building in VS Code

- Press `Ctrl+Shift+B` to select a build task
- Press `F5` to automatically compile and start debugging

## Program Descriptions

### generator — Glyph File Generator

Reads `font/fusion-pixel-12px-monospaced-zh_hans.ttf`, iterates over all codepoints in the Unicode BMP range (U+0000 ~ U+FFFF), renders them as 1-bit bitmaps, and packs the output into `font.bin`.

**Output format (`font.bin`):**

| Offset | Size | Description |
|--------|------|-------------|
| 0 | 4 bytes | Total number of glyphs (int32) |
| 4 | glyph_count × 4 bytes | Glyph index table (int32 array, each entry is the offset of the corresponding glyph in the file) |
| ... | Variable | Glyph data, each containing: width (uint8), height (uint8), bitmap compressed data |

Bitmap compression: 1 bit per pixel, row-major order, white background is `0`, non-white is `1`.

### loadtest — Bitmap Font Rendering Test

Uses the embedded 12px bitmap font data (`fusionpixel_12px_mono.hpp`) to render text, demonstrating the rendering capabilities of the `DebugText.hpp` engine.

### tools/toarray.py — File to C++ Byte Array Converter

Converts any binary file into a C++ `const uint8_t` array definition for embedding resource files into code.

```bash
python tools/toarray.py
```

## License

- This project is licensed under the MIT License
- The font `fusion-pixel-12px-monospaced-zh_hans.ttf` is from [Fusion Pixel](https://github.com/TakWolf/fusion-pixel-font), licensed under OFL (see `font/LICENSE-OFL`).

## Backstory

Originally, I wanted to render text on an STM32 LCD screen, but the default library only supports English, so I decided to make a font bitmap myself.  
After finishing it, I realized this approach could also be useful in everyday development — not having to load font files in advance is really convenient!  
And thus this library was born.

## Using with ImGui

Theoretically possible, but I'm not a fan of ImGui, so if anyone is interested, feel free to write an adapter.
