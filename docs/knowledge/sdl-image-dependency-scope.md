# SDL_image is fetched deliberately narrow — do not "restore the defaults"

Decided 2026-11-20 (Sprint 7, Windows port). Declared once here; cite this
topic rather than re-explaining the option list somewhere else.

## The decision

`CMakeLists.txt` fetches SDL_image with `GIT_SUBMODULES ""` and with
`SDLIMAGE_VENDORED`, `SDLIMAGE_PNG_LIBPNG`, `SDLIMAGE_AVIF`, `SDLIMAGE_JXL`,
`SDLIMAGE_TIF` and `SDLIMAGE_WEBP` all forced **OFF**. That is not tidying and
it is not a Windows workaround to be unwound later. It is load-bearing.

## Why

The project's entire use of SDL_image is **one call**:
`IMG_LoadTexture()` at `src/assets/asset_manager.c:85`, on PNGs this repo
generates itself (`tools/gen_assets.py`) as flat RGBA pixel art.

Left at its defaults, SDL_image brings in vastly more than that. FetchContent
clones submodules by default, and SDL_image's submodule tree vendors `aom`,
`dav1d`, `libavif` and `libjxl` — and `libjxl` in turn registers `brotli`,
`googletest`, `highway`, `lcms`, `libpng`, `sjpeg`, `skcms`, `zlib` and a
`testdata` repository — plus `libtiff` and `libwebp`. Gigabytes of AV1 and
JPEG-XL codec source to decode sprites that have no codec worth the name.

On Windows that was not merely wasteful, it was **fatal**: the nested paths
(`external/libjxl/third_party/skcms/...`) run past `MAX_PATH`, so the populate
step failed with `MSB8066` / exit `-1` and **no Windows configure could
complete at all**. This is the specific reason the project had no Windows build.

`SDLIMAGE_PNG_LIBPNG OFF` is the piece that makes the submodule-free build
self-sufficient: it selects SDL_image's built-in PNG decoder instead of vendored
libpng, so nothing needs `external/libpng` to exist. The format options are off
because every one of them requires an external library this project never uses.

## Effect measured

- `sdl_image-src` went from gigabytes to **78 MB**.
- Windows configure went from *never completing* to **99 s**.
- Linux loses the same waste. PNG decoding of our generated sprites is
  unchanged either way — the pixels are the same whichever decoder reads them.

## What would make this wrong

Only one thing: the game starting to load an image format that is not PNG. If
that ever happens, enable **that one format** and whatever it needs, and say so
here. Do not re-enable `SDLIMAGE_VENDORED` or drop `GIT_SUBMODULES ""` to get
it — that reintroduces the MAX_PATH failure and with it the Windows build.

Related: [[build-shared-dep-cache]] (why every build dir shares one
`FETCHCONTENT_BASE_DIR`). On this Windows machine that shared cache is
`D:/Development/.cmake-deps-msvc`, populated for the Visual Studio 17 2022
generator; a populated cache is bound to one generator, so a Ninja build needs
its own.
