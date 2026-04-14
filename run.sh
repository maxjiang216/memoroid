#!/bin/sh
# Run with: ./run.sh   (or: bash run.sh)
# "run.sh" alone is not on PATH — use ./run.sh from this directory.
set -eu
cd "$(dirname "$0")"
TARGET="${TARGET:-memoroid}"

if ! command -v pkg-config >/dev/null 2>&1; then
  echo "pkg-config is not installed. On Debian/Ubuntu run:" >&2
  echo "  sudo apt install pkg-config" >&2
  exit 1
fi

if ! pkg-config --exists allegro-5 2>/dev/null; then
  echo "Allegro 5 development libraries were not found (no allegro-5.pc)." >&2
  echo "On Debian/Ubuntu (including 24.04 Noble), install:" >&2
  echo "  sudo apt install g++ pkg-config liballegro5-dev \\" >&2
  echo "    liballegro-ttf5-dev liballegro-image5-dev liballegro-dialog5-dev" >&2
  echo "(Older releases used the optional package liballegro5.2-plugins; on Noble that name does not exist.)" >&2
  exit 1
fi

if ! pkg-config --exists allegro_ttf-5 2>/dev/null; then
  echo "Allegro addon dev packages are missing (e.g. no allegro_ttf-5.pc)." >&2
  echo "Install:" >&2
  echo "  sudo apt install liballegro-ttf5-dev liballegro-image5-dev liballegro-dialog5-dev" >&2
  exit 1
fi

PKG_LIBS=$(pkg-config allegro-5 allegro_font-5 allegro_ttf-5 allegro_image-5 allegro_primitives-5 allegro_dialog-5 --cflags --libs)
if [ ! -f "$TARGET" ] || [ main.cpp -nt "$TARGET" ]; then
  if ! g++ -std=c++17 -O2 main.cpp -o "$TARGET" $PKG_LIBS; then
    g++ -std=c++17 -O2 main.cpp -o "$TARGET" $PKG_LIBS -lstdc++fs
  fi
fi
exec ./"$TARGET"
