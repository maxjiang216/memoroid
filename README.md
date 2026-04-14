# memoroid

Anime-themed memory matching game (high school project). Built with Allegro 5.

## Dependencies

- C++ compiler with C++17 (`g++` or Clang)
- Allegro 5 development libraries: core, font, TTF, image, primitives, native dialog

On Debian/Ubuntu:

```bash
sudo apt install g++ liballegro5-dev liballegro5.2-plugins
```

## Build

From the repository root:

```bash
g++ -std=c++17 -O2 main.cpp -o memoroid \
  $(pkg-config allegro-5 allegro_font-5 allegro_ttf-5 allegro_image-5 allegro_primitives-5 allegro_dialog-5 --cflags --libs)
```

If your distro needs it, link the C++ filesystem library:

```bash
g++ -std=c++17 -O2 main.cpp -o memoroid ... $(pkg-config ...) -lstdc++fs
```

## Build and run (one command)

[`run.sh`](run.sh) rebuilds the binary when `main.cpp` is newer than `memoroid` (or when the binary is missing), then starts the game:

```bash
./run.sh
```

## Run

The game loads assets with paths relative to the **current working directory**. From the repo root after building:

```bash
./memoroid
```

The game runs **fullscreen** on the primary display. Press **Escape** or close the window to quit.

## Assets layout

| Path | Contents |
|------|-----------|
| `assets/ui/` | Menu and board chrome: `title.bmp`, `play.bmp`, `hiscore.bmp`, `replay.bmp`, `three.bmp`–`seven.bmp`, `back.bmp`, `poker.bmp` |
| `assets/characters/<name>/` | One folder per character. Put any number of `.bmp` face images there; **one is chosen at random** when that character appears in a deal. At the **start of each round**, the game picks **25** character folders for that round: if you have **more than 25** folders with art, it chooses **25 at random** (without replacement); if you have **fewer**, it samples with replacement to fill the 25 card slots. |
| `assets/fonts/` (optional) | Optional `comic.ttf`; if missing, the game uses Allegro’s built-in font for the timer and scores |

Folders under `assets/characters/` without at least one `.bmp` are ignored. Minimum **number of distinct characters with art** (folder count) for each level: 3×3 → 5, 4×4 → 8, 5×5 → 13, 6×6 → 18, 7×7 → 22.

## How to play

1. **Main menu**: Click **Play** to choose a grid size (3×3 through 7×7), or **High scores** to open the scoreboard mode.
2. **Replay**: On the main menu, **Replay** starts the **last grid size you played** (after you have finished at least one round). During a round, **Replay** restarts that grid with a **new shuffle** and new random character art for the 25 slots.
3. **Memory**: Cards start face-down (poker back). Click two cards to flip them. If they show the same character, they stay matched; otherwise they flip back. Clear the board as fast as you can.
4. **Timer**: A timer runs during play; lower times are better. When you finish a board, your time may be added to that difficulty’s top-10 list (stored in `save.txt` in the working directory).
5. **High scores**: From the main menu, choose **High scores**, then click a level button to view times for that grid size.
6. **Back**: Returns toward the menu from sub-screens; face art for the last round is unloaded when you go back.

## License / credits

Character artwork is fan art from the original project; rights remain with their respective owners.
