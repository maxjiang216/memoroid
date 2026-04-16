#!/usr/bin/env bash
# Build memoroid for the web using Emscripten.
#
# Prerequisites
# =============
# 1. Install and activate emsdk:
#
#       git clone https://github.com/emscripten-core/emsdk.git
#       cd emsdk
#       ./emsdk install latest
#       ./emsdk activate latest
#       source ./emsdk_env.sh          # or add to ~/.bashrc
#
# 2. Build Allegro 5 for Emscripten.
#    Allegro's web target uses its SDL-based backend.
#    A minimal recipe (run once, outside this repo):
#
#       git clone https://github.com/liballeg/allegro5.git
#       cd allegro5
#       # Pre-build SDL2 and FreeType Emscripten ports first:
#       embuilder build sdl2 freetype
#
#       EMCACHE=$(em-config CACHE)
#       emcmake cmake -B build-wasm \
#           -DCMAKE_BUILD_TYPE=Release \
#           -DALLEGRO_SDL=ON \
#           -DSDL2_INCLUDE_DIR="${EMCACHE}/sysroot/include" \
#           -DSDL2_LIBRARY="${EMCACHE}/sysroot/lib/wasm32-emscripten/libSDL2.a" \
#           -DFREETYPE_INCLUDE_DIRS="${EMCACHE}/sysroot/include/freetype2" \
#           -DFREETYPE_LIBRARY="${EMCACHE}/sysroot/lib/wasm32-emscripten/libfreetype.a" \
#           -DWANT_MONOLITH=ON \
#           -DWANT_TTF=ON \
#           -DWANT_IMAGE=ON \
#           -DWANT_PRIMITIVES=ON \
#           -DWANT_FONT=ON \
#           -DWANT_NATIVE_DIALOG=OFF \
#           -DWANT_AUDIO=OFF \
#           -DWANT_ACODEC=OFF \
#           -DWANT_SHADERS_GL=OFF \
#           -DWANT_EXAMPLES=OFF \
#           -DWANT_TESTS=OFF \
#           -DWANT_DOCS=OFF \
#           -S .
#       cmake --build build-wasm -j$(nproc)
#
#    The important output is build-wasm/lib/liballegro_monolith-static.a
#    (exact name may vary; adjust ALLEGRO_LIB below accordingly).
#
# Usage
# =====
#   ALLEGRO_INCLUDE=/path/to/allegro5/include \
#   ALLEGRO_LIB=/path/to/allegro5/build-wasm/lib \
#   bash build-web.sh
#
# Output goes to public/ (index.html, index.js, index.wasm, index.data).
# Commit that directory and push; Vercel serves it as a static site.
#
# Before linking, this script:
#   1. Converts character BMPs → PNG in a temp dir (web-assets/).
#      PNG is lossless but ~70-80% smaller than BMP for anime-style art,
#      bringing ~155 MB of character BMPs down to ~30-45 MB without any
#      quality loss.  Allegro loads PNGs via libpng (compiled in with
#      -sUSE_LIBPNG=1 / embuilder build libpng — see deploy.yml).
#   2. Runs scripts/generate_wasm_character_manifest.py with --ext png.
#   3. Packages assets via Emscripten's file_packager.py.  We call the
#      packager directly (not via emcc --preload-file) so we can point it
#      at web-assets/ instead of assets/.
# Desktop builds are unaffected and continue to load .bmp via the filesystem.

set -euo pipefail

ALLEGRO_INCLUDE="${ALLEGRO_INCLUDE:-./allegro5/include}"
ALLEGRO_LIB="${ALLEGRO_LIB:-./allegro5/build-wasm/lib}"
OUT_DIR="${OUT_DIR:-public}"

if ! command -v emcc &>/dev/null; then
  echo "ERROR: emcc not found. Activate emsdk first:" >&2
  echo "  source /path/to/emsdk/emsdk_env.sh" >&2
  exit 1
fi

if [ ! -d "${ALLEGRO_INCLUDE}/allegro5" ]; then
  echo "ERROR: Allegro headers not found at ${ALLEGRO_INCLUDE}/allegro5" >&2
  echo "  Set ALLEGRO_INCLUDE to the allegro5/include directory." >&2
  exit 1
fi

mkdir -p "${OUT_DIR}"

# Locate the monolith static library (name varies: -static suffix or not)
ALLEGRO_LIB_FILE=$(find "${ALLEGRO_LIB}" -name "liballegro_monolith*.a" 2>/dev/null | head -1)
if [ -z "${ALLEGRO_LIB_FILE}" ]; then
  echo "ERROR: Could not find liballegro_monolith*.a under ${ALLEGRO_LIB}" >&2
  echo "  Make sure Allegro was built with WANT_MONOLITH=ON for Emscripten." >&2
  exit 1
fi
echo "Using Allegro library: ${ALLEGRO_LIB_FILE}"

echo "Building memoroid.wasm ..."

# Derive Allegro build and source roots from the library path.
# Layout: allegro5-src/build-wasm/lib/liballegro_monolith.a
#              => build dir:  allegro5-src/build-wasm/
#              => source dir: allegro5-src/
ALLEGRO_BUILD_DIR="$(cd "$(dirname "${ALLEGRO_LIB_FILE}")/.." && pwd)"
ALLEGRO_SRC_DIR="$(cd "${ALLEGRO_BUILD_DIR}/.." && pwd)"

# Generated headers (alplatf.h etc.) land in the build include tree.
ALLEGRO_BUILD_INC="${ALLEGRO_BUILD_DIR}/include"

# Addon headers (allegro_font.h, allegro_image.h, allegro_primitives.h, …)
# live in the source addon subdirectories. They are NOT copied to the build
# tree during cmake build (only on cmake --install), so we add each one.
ADDON_INCS=""
for addon_dir in "${ALLEGRO_SRC_DIR}/addons"/*/; do
    [ -d "${addon_dir}" ] && ADDON_INCS="${ADDON_INCS} -I${addon_dir%/}"
done

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "${SCRIPT_DIR}"

# Convert character BMPs → PNG for the web bundle.
# BMPs are ~911 KB each; PNG is ~150-250 KB — a ~4-6× lossless reduction.
WEB_ASSETS="${SCRIPT_DIR}/web-assets"
echo "Creating web asset bundle (BMP → PNG for character art) …"
rm -rf "${WEB_ASSETS}"
cp -r "${SCRIPT_DIR}/assets" "${WEB_ASSETS}"
if command -v magick &>/dev/null; then CONV="magick"; else CONV="convert"; fi
find "${WEB_ASSETS}/characters" -name '*.bmp' | while IFS= read -r bmp; do
  "${CONV}" "${bmp}" "${bmp%.bmp}.png"
  rm "${bmp}"
done
echo "  $(find "${WEB_ASSETS}/characters" -name '*.png' | wc -l) PNG(s) ready"
# Remove temp dir on exit (success or failure).
trap 'rm -rf "${WEB_ASSETS}"' EXIT

GEN_CPP="${SCRIPT_DIR}/generated/wasm_character_manifest.cpp"
echo "Generating WASM character manifest (ext=png) …"
python3 "${SCRIPT_DIR}/scripts/generate_wasm_character_manifest.py" \
  --characters "${WEB_ASSETS}/characters" \
  --ext png \
  --out "${GEN_CPP}"

# Package assets using file_packager.py directly (avoids emcc flag-forwarding
# issues) and point it at web-assets/ where the PNGs live.
EMSCRIPTEN_ROOT=$(em-config EMSCRIPTEN_ROOT)
echo "Packaging assets …"
python3 "${EMSCRIPTEN_ROOT}/tools/file_packager.py" \
  "${OUT_DIR}/index.data" \
  --preload "${WEB_ASSETS}@assets" \
  --js-output="${OUT_DIR}/index.data.js"
echo "  index.data: $(du -sh "${OUT_DIR}/index.data" | cut -f1)"

echo "Compiling and linking …"
emcc main.cpp \
  "${GEN_CPP}" \
  -std=c++17 \
  -O2 \
  -I. \
  -I"${ALLEGRO_INCLUDE}" \
  -I"${ALLEGRO_BUILD_INC}" \
  ${ADDON_INCS} \
  "${ALLEGRO_LIB_FILE}" \
  -sUSE_SDL=2 \
  -sUSE_LIBPNG=1 \
  -sFULL_ES2=1 \
  -sALLOW_MEMORY_GROWTH=1 \
  -sEXPORTED_RUNTIME_METHODS='["UTF8ToString","lengthBytesUTF8","stringToUTF8"]' \
  --pre-js "${OUT_DIR}/index.data.js" \
  --shell-file "${SCRIPT_DIR}/memoroid-shell.html" \
  -o "${OUT_DIR}/index.html"

echo ""
echo "Build complete. Output in ${OUT_DIR}/:"
ls -lh "${OUT_DIR}"/index.*
echo ""
echo "To preview locally:"
echo "  cd ${OUT_DIR} && python3 -m http.server 8080"
echo "  Then open http://localhost:8080/index.html"
