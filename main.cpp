#include <allegro5/allegro.h>
#include <allegro5/allegro_font.h>
#include <allegro5/allegro_image.h>
#include <allegro5/allegro_native_dialog.h>
#include <allegro5/allegro_primitives.h>
#ifndef __EMSCRIPTEN__
#include <allegro5/allegro_ttf.h>
#endif
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include <algorithm>
#include <cstring>
#include <string>
#include <vector>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#else
#include <filesystem>
#endif

#define BLACK al_map_rgb(0, 0, 0)
#define WHITE al_map_rgb(255, 255, 255)
#define BLUE al_map_rgb(197, 214, 237)

// Step 5 (dialogs): on WASM, map fatal error messages to stderr instead of
// native dialog boxes, which behave as blocking browser alerts under Allegro's
// SDL backend.
#ifdef __EMSCRIPTEN__
#define FATAL_MSG(disp, msg)                                                   \
  fprintf(stderr, "MEMOROID ERROR: %s\n", (msg))
#else
#define FATAL_MSG(disp, msg)                                                   \
  al_show_native_message_box((disp), "Error", "Error", (msg), nullptr,        \
                             ALLEGRO_MESSAGEBOX_ERROR)
#endif

struct Button {
  float x1, y1, x2, y2;
  int side;
  ALLEGRO_BITMAP *img;
  ALLEGRO_BITMAP *poker;
  // side 0=inactive, 1=normal active, 2=hiscore active,
  //      3=card face-down, 4=card matched
};

// -----------------------------------------------------------------------
// Step 2 (globals): all state that was local to main() is lifted here so
// the emscripten_set_main_loop callback (tick) can access it.
// -----------------------------------------------------------------------
namespace {

const float FPS = 60;
const int SW = 1600;
const int SH = 900;

static const char *ASSETS_UI = "assets/ui/";
static const char *ASSETS_CHAR = "assets/characters/";
static const char *ASSETS_FONTS = "assets/fonts/";

static const int MIN_CHARS_FOR_LEVEL[5] = {5, 8, 13, 18, 22};

static std::string g_slot_slug[25];

// Allegro objects
static ALLEGRO_DISPLAY *display = nullptr;
static ALLEGRO_DISPLAY_MODE disp_data;
static ALLEGRO_EVENT_QUEUE *event_queue = nullptr;
static ALLEGRO_TIMER *timer = nullptr;
static ALLEGRO_FONT *font = nullptr;

// UI bitmaps
static ALLEGRO_BITMAP *ititle = nullptr;
static ALLEGRO_BITMAP *iplay = nullptr;
static ALLEGRO_BITMAP *ihi = nullptr;
static ALLEGRO_BITMAP *ilevels[5] = {};
static ALLEGRO_BITMAP *iback = nullptr;
static ALLEGRO_BITMAP *ipoker = nullptr;
static ALLEGRO_BITMAP *ireplay = nullptr;
static ALLEGRO_BITMAP *faceBmps[25] = {};

// Game state
static bool doexit = false;
static int deck[25];
static int deck3[9], deck4[16], deck5[25], deck6[36], deck7[49];
static int ind, temp;
static int cardholder = -1;
static int game = 0;
static int check = 0;
static int t = 0;
static int hiscore[5][10];
static int stop = 0;
static int last_level = -1;
static int current_level = 0;
static bool touched_card_this_game = false;
static std::vector<std::string> all_slugs;

// Buttons
static Button title, play, hi, replay, levels[5], bback;
static Button card3[9], card4[16], card5[25], card6[36], card7[49];

// -----------------------------------------------------------------------
// Step 3 (filesystem): on WASM, directory_iterator cannot scan the preloaded
// virtual FS reliably, so we replace both helpers with hardcoded manifests
// derived from the actual assets/ tree in this repo.
// -----------------------------------------------------------------------
#ifdef __EMSCRIPTEN__

static std::vector<std::string> getBmpsForSlug(const std::string &slug) {
  if (slug == "asuna")
    return {"asuna.bmp", "asunab.bmp", "asunac.bmp", "asunad.bmp",
            "asunae.bmp"};
  if (slug == "aoba")    return {"aoba.bmp"};
  if (slug == "azusa")   return {"azusa.bmp"};
  if (slug == "chiyo")   return {"chiyo.bmp", "chiyob.bmp"};
  if (slug == "chitoge") return {"chitoge.bmp", "chitogeb.bmp"};
  if (slug == "emilia")  return {"emilia.bmp"};
  if (slug == "hifumin") return {"hifumin.bmp"};
  if (slug == "kanade")  return {"kanade.bmp"};
  if (slug == "katou")   return {"katou.bmp", "katoub.bmp"};
  if (slug == "kosaki")  return {"kosaki.bmp"};
  if (slug == "mashiron")
    return {"mashiron.bmp", "mashironb.bmp", "mashironc.bmp", "mashirond.bmp"};
  if (slug == "megumin") return {"megumin.bmp"};
  if (slug == "menmu")   return {"menmu.bmp"};
  if (slug == "mio")     return {"mio.bmp", "miob.bmp", "mioc.bmp"};
  if (slug == "misaka")
    return {"misaka.bmp", "misakab.bmp", "misakac.bmp", "misakad.bmp"};
  if (slug == "mizuki")  return {"mizuki.bmp"};
  if (slug == "nanamin") return {"nanamin.bmp"};
  if (slug == "nao")     return {"nao.bmp", "naob.bmp"};
  if (slug == "rem")     return {"rem.bmp", "remb.bmp", "remc.bmp"};
  if (slug == "rikka")   return {"rikka.bmp", "rikkab.bmp"};
  if (slug == "ritsu")   return {"ritsu.bmp"};
  if (slug == "shiro")   return {"shiro.bmp"};
  if (slug == "taiga")   return {"taiga.bmp"};
  if (slug == "yuichan") return {"yuichan.bmp"};
  if (slug == "yukinon") return {"yukinon.bmp"};
  if (slug == "yui")     return {"yui.bmp"};
  return {};
}

void collectBmpPaths(const std::string &dir, std::vector<std::string> &out) {
  out.clear();
  // Extract the slug (last path component) from the directory path
  std::string slug = dir;
  if (!slug.empty() && slug.back() == '/')
    slug.pop_back();
  auto pos = slug.rfind('/');
  if (pos != std::string::npos)
    slug = slug.substr(pos + 1);
  for (const auto &f : getBmpsForSlug(slug))
    out.push_back(dir + "/" + f);
}

void scanCharacterSlugs(std::vector<std::string> &out) {
  // Hardcoded list matching assets/characters/ folders that contain at least
  // one .bmp file (yun, vigne, kaede, extras contain only .gitkeep).
  out = {"asuna",   "aoba",    "azusa",   "chiyo",   "chitoge", "emilia",
         "hifumin", "kanade",  "katou",   "kosaki",  "mashiron","megumin",
         "menmu",   "mio",     "misaka",  "mizuki",  "nanamin", "nao",
         "rem",     "rikka",   "ritsu",   "shiro",   "taiga",   "yuichan",
         "yukinon", "yui"};
}

#else // native desktop

void collectBmpPaths(const std::string &dir, std::vector<std::string> &out) {
  out.clear();
  try {
    namespace fs = std::filesystem;
    fs::path p(dir);
    if (!fs::exists(p) || !fs::is_directory(p))
      return;
    for (const auto &e : fs::directory_iterator(p)) {
      if (!e.is_regular_file())
        continue;
      auto ext = e.path().extension().string();
      if (ext == ".bmp" || ext == ".BMP")
        out.push_back(e.path().string());
    }
  } catch (...) {
  }
}

void scanCharacterSlugs(std::vector<std::string> &out) {
  out.clear();
  try {
    namespace fs = std::filesystem;
    fs::path root(ASSETS_CHAR);
    if (!fs::exists(root) || !fs::is_directory(root))
      return;
    for (const auto &e : fs::directory_iterator(root)) {
      if (!e.is_directory())
        continue;
      std::string name = e.path().filename().string();
      if (!name.empty() && name[0] == '.')
        continue;
      std::vector<std::string> bmps;
      collectBmpPaths(e.path().string(), bmps);
      if (!bmps.empty())
        out.push_back(name);
    }
    std::sort(out.begin(), out.end());
  } catch (...) {
  }
}

#endif // __EMSCRIPTEN__

void destroyFaceBitmaps(ALLEGRO_BITMAP *fb[25]) {
  for (int i = 0; i < 25; i++) {
    if (fb[i]) {
      al_destroy_bitmap(fb[i]);
      fb[i] = nullptr;
    }
  }
}

bool loadFaceForLogicalId(int logicalId, ALLEGRO_BITMAP *fb[25]) {
  if (logicalId < 0 || logicalId >= 25)
    return false;
  if (fb[logicalId])
    return true;
  std::string dirpath = std::string(ASSETS_CHAR) + g_slot_slug[logicalId];
  std::vector<std::string> bmps;
  collectBmpPaths(dirpath, bmps);
  if (bmps.empty())
    return false;
  int pick = rand() % (int)bmps.size();
  ALLEGRO_BITMAP *b = al_load_bitmap(bmps[pick].c_str());
  if (!b)
    return false;
  fb[logicalId] = b;
  return true;
}

void pickGameSlotSlugs(const std::vector<std::string> &sl) {
  int N = (int)sl.size();
  if (N == 0)
    return;
  if (N >= 25) {
    std::vector<std::string> copy = sl;
    for (int i = 0; i < 25; i++) {
      int j = i + rand() % (N - i);
      std::swap(copy[i], copy[j]);
    }
    for (int i = 0; i < 25; i++)
      g_slot_slug[i] = copy[i];
  } else {
    for (int i = 0; i < 25; i++)
      g_slot_slug[i] = sl[rand() % N];
  }
}

void shuffleDeck(int d[25]) {
  for (int i = 0; i < 25; i++)
    d[i] = i;
  for (int i = 0; i < 25; i++) {
    int j = rand() % 25;
    int tmp = d[i];
    d[i] = d[j];
    d[j] = tmp;
  }
}

ALLEGRO_BITMAP *loadUiBitmap(const char *filename) {
  char buf[512];
  snprintf(buf, sizeof buf, "%s%s", ASSETS_UI, filename);
  return al_load_bitmap(buf);
}

ALLEGRO_FONT *loadGameFont() {
#ifndef __EMSCRIPTEN__
  char buf[512];
  snprintf(buf, sizeof buf, "%scomic.ttf", ASSETS_FONTS);
  ALLEGRO_FONT *f = al_load_ttf_font(buf, 50, 0);
  if (!f)
    f = al_load_ttf_font("comic.ttf", 50, 0);
  if (!f)
    f = al_create_builtin_font();
  return f;
#else
  // TTF addon not compiled into the WASM monolith; use builtin font.
  return al_create_builtin_font();
#endif
}

void wireCardImages(int *deckvals, int n, Button *cards, ALLEGRO_BITMAP *fb[25]) {
  for (int i = 0; i < n; i++) {
    int id = deckvals[i];
    loadFaceForLogicalId(id, fb);
    cards[i].img = fb[id];
  }
}

} // namespace

// Global-scope helpers called from both the namespace (tick) and main()
void layoutReplayGame(Button &r, int w, int h);
int click(Button button, float clickx, float clicky, int s);
void draw(Button button);
void make(Button &button, float x1, float y1, float x2, float y2, int side,
          ALLEGRO_BITMAP *img);
void exportSave(int hiscores[5][10]);

int compare(const void *a, const void *b) { return (*(int *)a - *(int *)b); }

void layoutReplayGame(Button &r, int w, int h) {
  r.x1 = (float)w - 3.f * (float)w / 25.f;
  r.x2 = (float)w - (float)w / 100.f;
  r.y1 = (float)(791 * h) / 900.f;
  r.y2 = (float)(99 * h) / 100.f;
}

namespace {

// -----------------------------------------------------------------------
// beginLevel and returnToMainMenu: converted from main()-local lambdas to
// regular functions now that all captured state is in the global namespace.
// -----------------------------------------------------------------------
void beginLevel(int L) {
  if ((int)all_slugs.size() < MIN_CHARS_FOR_LEVEL[L]) {
    FATAL_MSG(display, "Add more character folders under assets/characters/.");
    stop = FPS / 10;
    return;
  }
  destroyFaceBitmaps(faceBmps);
  pickGameSlotSlugs(all_slugs);
  shuffleDeck(deck);
  last_level = L;
  current_level = L;
  title.side = 0;
  bback.side = 1;
  for (int i = 0; i < 5; i++)
    levels[i].side = 0;
  t = 0;
  check = 0;
  if (L == 0) {
    for (int i = 0; i < 9; i++)
      card3[i].side = 3;
    for (int i = 0; i < 9; i++)
      deck3[i] = deck[i % 5];
    for (int i = 0; i < 9; i++) {
      ind = rand() % 9;
      temp = deck3[ind];
      deck3[ind] = deck3[i];
      deck3[i] = temp;
    }
    wireCardImages(deck3, 9, card3, faceBmps);
  } else if (L == 1) {
    for (int i = 0; i < 16; i++)
      card4[i].side = 3;
    for (int i = 0; i < 16; i++)
      deck4[i] = deck[i % 8];
    for (int i = 0; i < 16; i++) {
      ind = rand() % 16;
      temp = deck4[ind];
      deck4[ind] = deck4[i];
      deck4[i] = temp;
    }
    wireCardImages(deck4, 16, card4, faceBmps);
  } else if (L == 2) {
    for (int i = 0; i < 25; i++)
      card5[i].side = 3;
    for (int i = 0; i < 25; i++)
      deck5[i] = deck[i % 13];
    for (int i = 0; i < 25; i++) {
      ind = rand() % 25;
      temp = deck5[ind];
      deck5[ind] = deck5[i];
      deck5[i] = temp;
    }
    wireCardImages(deck5, 25, card5, faceBmps);
  } else if (L == 3) {
    for (int i = 0; i < 36; i++)
      card6[i].side = 3;
    for (int i = 0; i < 36; i++)
      deck6[i] = deck[i % 18];
    for (int i = 0; i < 36; i++) {
      ind = rand() % 36;
      temp = deck6[ind];
      deck6[ind] = deck6[i];
      deck6[i] = temp;
    }
    wireCardImages(deck6, 36, card6, faceBmps);
  } else if (L == 4) {
    for (int i = 0; i < 49; i++)
      card7[i].side = 3;
    for (int i = 0; i < 49; i++)
      deck7[i] = deck[i % 25];
    for (int i = 0; i < 49; i++) {
      ind = rand() % 25; // preserved from original
      temp = deck7[ind];
      deck7[ind] = deck7[i];
      deck7[i] = temp;
    }
    wireCardImages(deck7, 49, card7, faceBmps);
  }
  game = 1;
  cardholder = -1;
  stop = FPS / 10;
  touched_card_this_game = false;
  replay.side = 0;
}

void returnToMainMenu() {
  destroyFaceBitmaps(faceBmps);
  replay.side = 0;
  bback.side = 0;
  for (int i = 0; i < 5; i++)
    levels[i].side = 0;
  t = 0;
  check = 0;
  game = 0;
  title.side = 1;
  play.side = 1;
  hi.side = 1;
  for (int i = 0; i < 9; i++)
    card3[i].side = 0;
  for (int i = 0; i < 16; i++)
    card4[i].side = 0;
  for (int i = 0; i < 25; i++)
    card5[i].side = 0;
  for (int i = 0; i < 36; i++)
    card6[i].side = 0;
  for (int i = 0; i < 49; i++)
    card7[i].side = 0;
  touched_card_this_game = false;
  stop = FPS / 10;
}

// -----------------------------------------------------------------------
// Step 2 (loop): the original while(!doexit) body is extracted here.
// Desktop: tick() is called by the while loop in main(); the blocking
// al_wait_for_event keeps CPU idle between frames.
// WASM: tick() is the emscripten_set_main_loop callback; events are polled
// non-blocking so the browser's scheduler can continue running.
// -----------------------------------------------------------------------
void tick() {
  ALLEGRO_EVENT ev;
  int timerTicks = 0;
  bool mouseHandled = false;
  bool keyHandled = false;

#ifndef __EMSCRIPTEN__
  // Block until at least one event arrives, then drain the rest.
  al_wait_for_event(event_queue, &ev);
  bool hasEvent = true;
#else
  // Non-blocking poll: browser calls tick() at ~60 fps via rAF.
  bool hasEvent = al_get_next_event(event_queue, &ev);
#endif

  while (hasEvent) {
    if (ev.type == ALLEGRO_EVENT_DISPLAY_CLOSE) {
#ifdef __EMSCRIPTEN__
      emscripten_cancel_main_loop();
      return;
#else
      doexit = true;
#endif
    } else if (ev.type == ALLEGRO_EVENT_KEY_DOWN) {
      if (ev.keyboard.keycode == ALLEGRO_KEY_ESCAPE) {
        if (game == 1 || (game >= 3 && game <= 7)) {
          returnToMainMenu();
          keyHandled = true;
        } else {
#ifdef __EMSCRIPTEN__
          // Cannot close a browser tab; Escape at main menu is a no-op.
          (void)0;
#else
          doexit = true;
#endif
        }
      }
    } else if (ev.type == ALLEGRO_EVENT_TIMER) {
      timerTicks++;
    } else if (ev.type == ALLEGRO_EVENT_MOUSE_BUTTON_DOWN) {
      mouseHandled = true;
      const float mx = (float)ev.mouse.x;
      const float my = (float)ev.mouse.y;

      if (click(play, mx, my, stop)) {
        play.side = 0;
        hi.side = 0;
        for (int i = 0; i < 5; i++)
          levels[i].side = 1;
        bback.side = 1;
        stop = FPS / 10;
      }

      if (click(hi, mx, my, stop)) {
        play.side = 0;
        hi.side = 0;
        for (int i = 0; i < 5; i++)
          levels[i].side = 2;
        bback.side = 1;
        stop = FPS / 10;
      }

      if (click(replay, mx, my, stop)) {
        if (game == 1)
          beginLevel(current_level);
      }

      if (click(bback, mx, my, stop))
        returnToMainMenu();

      if (click(levels[0], mx, my, stop) && levels[0].side == 1)
        beginLevel(0);

      for (int i = 0; i < 9; i++) {
        if (click(card3[i], mx, my, stop) && card3[i].side % 2) {
          touched_card_this_game = true;
          card3[i].side = 1;
          if (cardholder != -1 && i != cardholder) {
            if (deck3[i] == deck3[cardholder]) {
              card3[i].side = 4;
              card3[cardholder].side = 4;
            }
            check = 1;
            for (int i = 0; i < 9; i++) {
              if (card3[i].side != 4) {
                if (check == 1) {
                  check = 2;
                } else {
                  check = 0;
                }
              }
            }
            if (check == 2) {
              for (int k = 0; k < 9; k++) {
                if (card3[k].side != 4) {
                  card3[k].side = 4;
                  break;
                }
              }
              check = 1;
            }
            if (check) {
              for (int i = 9; i >= 0; i--) {
                if (hiscore[0][i] > t) {
                  hiscore[0][i] = t;
                  qsort(hiscore[0], 10, sizeof(int), compare);
                  exportSave(hiscore);
                  break;
                }
              }
              bback.side = 1;
            }
            cardholder = -1;
          } else {
            for (int j = 0; j < 9; j++) {
              if (card3[j].side == 1 && j != i)
                card3[j].side = 3;
            }
            cardholder = i;
          }
        }
      }

      if (click(levels[0], mx, my, stop) && levels[0].side == 2) {
        title.side = 0;
        for (int i = 0; i < 5; i++)
          levels[i].side = 0;
        game = 3;
      }

      if (click(levels[1], mx, my, stop) && levels[1].side == 1)
        beginLevel(1);

      for (int i = 0; i < 16; i++) {
        if (click(card4[i], mx, my, stop) && card4[i].side % 2) {
          touched_card_this_game = true;
          card4[i].side = 1;
          if (cardholder != -1 && i != cardholder) {
            if (deck4[i] == deck4[cardholder]) {
              card4[i].side = 4;
              card4[cardholder].side = 4;
            }
            check = 1;
            for (int i = 0; i < 16; i++) {
              if (card4[i].side != 4)
                check = 0;
            }
            if (check) {
              for (int i = 9; i >= 0; i--) {
                if (hiscore[1][i] > t) {
                  hiscore[1][i] = t;
                  qsort(hiscore[1], 10, sizeof(int), compare);
                  exportSave(hiscore);
                  break;
                }
              }
              bback.side = 1;
            }
            cardholder = -1;
          } else {
            for (int j = 0; j < 16; j++) {
              if (card4[j].side == 1 && j != i)
                card4[j].side = 3;
            }
            cardholder = i;
          }
        }
      }

      if (click(levels[1], mx, my, stop) && levels[1].side == 2) {
        title.side = 0;
        for (int i = 0; i < 5; i++)
          levels[i].side = 0;
        game = 4;
      }

      if (click(levels[2], mx, my, stop) && levels[2].side == 1)
        beginLevel(2);

      for (int i = 0; i < 25; i++) {
        if (click(card5[i], mx, my, stop) && card5[i].side % 2) {
          touched_card_this_game = true;
          card5[i].side = 1;
          if (cardholder != -1 && i != cardholder) {
            if (deck5[i] == deck5[cardholder]) {
              card5[i].side = 4;
              card5[cardholder].side = 4;
            }
            check = 1;
            for (int i = 0; i < 25; i++) {
              if (card5[i].side != 4) {
                if (check == 1) {
                  check = 2;
                } else {
                  check = 0;
                }
              }
            }
            if (check == 2) {
              for (int k = 0; k < 25; k++) {
                if (card5[k].side != 4) {
                  card5[k].side = 4;
                  break;
                }
              }
              check = 1;
            }
            if (check) {
              for (int i = 9; i >= 0; i--) {
                if (hiscore[2][i] > t) {
                  hiscore[2][i] = t;
                  qsort(hiscore[2], 10, sizeof(int), compare);
                  exportSave(hiscore);
                  break;
                }
              }
              bback.side = 1;
            }
            cardholder = -1;
          } else {
            for (int j = 0; j < 25; j++) {
              if (card5[j].side == 1 && j != i)
                card5[j].side = 3;
            }
            cardholder = i;
          }
        }
      }

      if (click(levels[2], mx, my, stop) && levels[2].side == 2) {
        title.side = 0;
        for (int i = 0; i < 5; i++)
          levels[i].side = 0;
        game = 5;
      }

      if (click(levels[3], mx, my, stop) && levels[3].side == 1)
        beginLevel(3);

      for (int i = 0; i < 36; i++) {
        if (click(card6[i], mx, my, stop) && card6[i].side % 2) {
          touched_card_this_game = true;
          card6[i].side = 1;
          if (cardholder != -1 && i != cardholder) {
            if (deck6[i] == deck6[cardholder]) {
              card6[i].side = 4;
              card6[cardholder].side = 4;
            }
            check = 1;
            for (int i = 0; i < 36; i++) {
              if (card6[i].side != 4)
                check = 0;
            }
            if (check) {
              for (int i = 9; i >= 0; i--) {
                if (hiscore[3][i] > t) {
                  hiscore[3][i] = t;
                  qsort(hiscore[3], 10, sizeof(int), compare);
                  exportSave(hiscore);
                  break;
                }
              }
              bback.side = 1;
            }
            cardholder = -1;
          } else {
            for (int j = 0; j < 36; j++) {
              if (card6[j].side == 1 && j != i)
                card6[j].side = 3;
            }
            cardholder = i;
          }
        }
      }

      if (click(levels[3], mx, my, stop) && levels[3].side == 2) {
        title.side = 0;
        for (int i = 0; i < 5; i++)
          levels[i].side = 0;
        game = 6;
      }

      if (click(levels[4], mx, my, stop) && levels[4].side == 1)
        beginLevel(4);

      for (int i = 0; i < 49; i++) {
        if (click(card7[i], mx, my, stop) && card7[i].side % 2) {
          touched_card_this_game = true;
          card7[i].side = 1;
          if (cardholder != -1 && i != cardholder) {
            if (deck7[i] == deck7[cardholder]) {
              card7[i].side = 4;
              card7[cardholder].side = 4;
            }
            check = 1;
            for (int i = 0; i < 49; i++) {
              if (card7[i].side != 4) {
                if (check == 1) {
                  check = 2;
                } else {
                  check = 0;
                }
              }
            }
            if (check == 2) {
              for (int k = 0; k < 49; k++) {
                if (card7[k].side != 4) {
                  card7[k].side = 4;
                  break;
                }
              }
              check = 1;
            }
            if (check) {
              for (int i = 9; i >= 0; i--) {
                if (hiscore[4][i] > t) {
                  hiscore[4][i] = t;
                  qsort(hiscore[4], 10, sizeof(int), compare);
                  exportSave(hiscore);
                  break;
                }
              }
              bback.side = 1;
            }
            cardholder = -1;
          } else {
            for (int j = 0; j < 49; j++) {
              if (card7[j].side == 1 && j != i)
                card7[j].side = 3;
            }
            cardholder = i;
          }
        }
      }

      if (click(levels[4], mx, my, stop) && levels[4].side == 2) {
        title.side = 0;
        for (int i = 0; i < 5; i++)
          levels[i].side = 0;
        game = 7;
      }
    }

    hasEvent = al_get_next_event(event_queue, &ev);
  }

  if (timerTicks > 0) {
    if (game == 1 && !check && touched_card_this_game) {
      t += timerTicks;
    } else if (!check) {
      t = 0;
    }
    if (stop > 0) {
      stop -= timerTicks;
      if (stop < 0)
        stop = 0;
    }
  }

  if (timerTicks > 0 || mouseHandled || keyHandled) {
    if (game == 1) {
      replay.side = touched_card_this_game ? 1 : 0;
      if (replay.side)
        layoutReplayGame(replay, disp_data.width, disp_data.height);
    } else {
      replay.side = 0;
    }

    al_clear_to_color(BLUE);
    draw(title);
    draw(play);
    draw(hi);
    for (int i = 0; i < 5; i++)
      draw(levels[i]);
    draw(bback);
    for (int i = 0; i < 9; i++)
      draw(card3[i]);
    for (int i = 0; i < 16; i++)
      draw(card4[i]);
    for (int i = 0; i < 25; i++)
      draw(card5[i]);
    for (int i = 0; i < 36; i++)
      draw(card6[i]);
    for (int i = 0; i < 49; i++)
      draw(card7[i]);
    if (game == 1)
      draw(replay);
    if (t) {
      al_draw_textf(font, BLACK, disp_data.width / 100,
                    disp_data.height / 100, 0, "%.2f", (float)t / FPS);
    }
    if (game > 2) {
      for (int i = 0; i < 10; i++) {
        if (hiscore[game - 3][i] < 600 * FPS) {
          al_draw_textf(font, BLACK, disp_data.width / 2,
                        (0.12 + (float)(i - 1) / 10) * disp_data.height,
                        ALLEGRO_ALIGN_CENTER, "%d. %10.2f", i + 1,
                        (float)hiscore[game - 3][i] / FPS);
        }
      }
    }
    al_flip_display();
  }
}

} // namespace

int main(int argc, char *argv[]) {
  (void)argc;
  (void)argv;

  al_init();
  al_set_new_display_flags(ALLEGRO_WINDOWED);
  disp_data.width = SW;
  disp_data.height = SH;
  display = al_create_display(SW, SH);
  if (!display) {
    FATAL_MSG(nullptr, "Failed to initialize display!");
    return -1;
  }
  al_set_window_title(display, "MEMOROID");
  if (!al_install_keyboard()) {
    FATAL_MSG(display, "Failed to initialize the keyboard!");
    return -1;
  }
  if (!al_install_mouse()) {
    FATAL_MSG(display, "Failed to initialize the mouse!");
    return -1;
  }
  if (!al_init_image_addon()) {
    FATAL_MSG(display, "Failed to initialize the image add-on!");
    return -1;
  }
  timer = al_create_timer(1.0 / FPS);
  if (!timer) {
    FATAL_MSG(display, "Failed to create the timer!");
    return -1;
  }
  event_queue = al_create_event_queue();
  if (!event_queue) {
    FATAL_MSG(display, "Failed to create the event queue!");
    al_destroy_display(display);
    return -1;
  }
  if (!al_init_primitives_addon()) {
    FATAL_MSG(display, "Failed to initialize primitives add-on!");
    return -1;
  }
  al_init_font_addon();
#ifndef __EMSCRIPTEN__
  al_init_ttf_addon();
#endif
  font = loadGameFont();
  if (!font) {
    FATAL_MSG(display, "Could not create font!");
    return -1;
  }

  ititle = loadUiBitmap("title.bmp");
  if (!ititle) {
    FATAL_MSG(display, "Failed to load assets/ui/title.bmp!");
    al_destroy_display(display);
    return -1;
  }
  iplay = loadUiBitmap("play.bmp");
  if (!iplay) {
    FATAL_MSG(display, "Failed to load assets/ui/play.bmp!");
    al_destroy_display(display);
    return -1;
  }
  ihi = loadUiBitmap("hiscore.bmp");
  if (!ihi) {
    FATAL_MSG(display, "Failed to load assets/ui/hiscore.bmp!");
    al_destroy_display(display);
    return -1;
  }
  ilevels[0] = loadUiBitmap("three.bmp");
  ilevels[1] = loadUiBitmap("four.bmp");
  ilevels[2] = loadUiBitmap("five.bmp");
  ilevels[3] = loadUiBitmap("six.bmp");
  ilevels[4] = loadUiBitmap("seven.bmp");
  for (int i = 0; i < 5; i++) {
    if (!ilevels[i]) {
      FATAL_MSG(display, "Failed to load level graphics!");
      al_destroy_display(display);
      return -1;
    }
  }
  iback = loadUiBitmap("back.bmp");
  ipoker = loadUiBitmap("poker.bmp");
  if (!iback || !ipoker) {
    FATAL_MSG(display, "Failed to load assets/ui/back.bmp or poker.bmp!");
    al_destroy_display(display);
    return -1;
  }
  ireplay = loadUiBitmap("replay.bmp");
  if (!ireplay) {
    FATAL_MSG(display, "Failed to load assets/ui/replay.bmp!");
    al_destroy_display(display);
    return -1;
  }

  scanCharacterSlugs(all_slugs);
  if (all_slugs.empty()) {
    FATAL_MSG(display, "No character art found under assets/characters/. "
                       "Add at least one .bmp per folder.");
    al_destroy_display(display);
    return -1;
  }

  srand(time(0));
  for (int i = 0; i < 25; i++)
    deck[i] = i;
  for (int i = 0; i < 5; i++)
    for (int j = 0; j < 10; j++)
      hiscore[i][j] = (int)(600 * FPS);

  // Step 4 (save): load hiscores from localStorage on WASM, save.txt on desktop.
#ifdef __EMSCRIPTEN__
  {
    char *saved = reinterpret_cast<char *>((intptr_t)EM_ASM_PTR({
      var s = localStorage.getItem('memoroid_scores');
      if (!s) return 0;
      var len = lengthBytesUTF8(s) + 1;
      var buf = _malloc(len);
      stringToUTF8(s, buf, len);
      return buf;
    }));
    if (saved) {
      char *p = saved;
      for (int i = 0; i < 5 && p; i++) {
        for (int j = 0; j < 10 && p; j++) {
          hiscore[i][j] = atoi(p);
          p = strchr(p, ',');
          if (p) p++;
        }
      }
      free(saved);
    }
  }
#else
  {
    FILE *fptr = fopen("save.txt", "r");
    if (fptr != NULL) {
      for (int i = 0; i < 5; i++)
        for (int j = 0; j < 10; j++)
          (void)fscanf(fptr, "%d", &hiscore[i][j]);
      fclose(fptr);
    }
  }
#endif

  make(title, disp_data.width / 4, disp_data.height / 100,
       3 * disp_data.width / 4, 109 * disp_data.height / 900, 1, ititle);
  make(play, 3 * disp_data.width / 8, 4 * disp_data.height / 9,
       5 * disp_data.width / 8, 2 * disp_data.height / 3, 1, iplay);
  make(replay, 0, 0, 0, 0, 0, ireplay);
  layoutReplayGame(replay, disp_data.width, disp_data.height);
  make(hi, disp_data.width / 3, 791 * disp_data.height / 900,
       2 * disp_data.width / 3, 99 * disp_data.height / 100, 1, ihi);
  for (int i = 0; i < 5; i++) {
    make(levels[i], (1 + (float)i * 3 / 2) * disp_data.width / 9,
         4 * disp_data.height / 9,
         (2 + (float)i * 3 / 2) * disp_data.width / 9,
         2 * disp_data.height / 3, 0, ilevels[i]);
  }
  make(bback, disp_data.width / 100, 791 * disp_data.height / 900,
       3 * disp_data.width / 25, 99 * disp_data.height / 100, 0, iback);
  for (int i = 0; i < 9; i++) {
    make(card3[i],
         disp_data.width / 2 +
             disp_data.height * ((float)(i % 3) * 166 / 459 - (float)8 / 17),
         (float)(floor(i / 3) * 11 / 10 + 0.1) * disp_data.height * 8 / 27,
         disp_data.width / 2 +
             disp_data.height *
                 ((float)(i % 3) * 166 / 459 - (float)116 / 459),
         (float)(floor(i / 3) * 11 / 10 + 1.1) * disp_data.height * 8 / 27,
         0, ipoker);
    card3[i].poker = ipoker;
  }
  for (int i = 0; i < 16; i++) {
    make(card4[i],
         disp_data.width / 2 +
             disp_data.height *
                 ((float)(i % 4) * 961 / 3645 - (float)43 / 90),
         (float)(floor(i / 4) * 11 / 10 + 0.1) * disp_data.height * 2 / 9,
         disp_data.width / 2 +
             disp_data.height *
                 ((float)(i % 4) * 961 / 3645 - (float)761 / 2430),
         (float)(floor(i / 4) * 11 / 10 + 1.1) * disp_data.height * 2 / 9,
         0, ipoker);
    card4[i].poker = ipoker;
  }
  for (int i = 0; i < 25; i++) {
    make(card5[i],
         disp_data.width / 2 +
             disp_data.height *
                 ((float)(i % 5) * 629 / 3024 - (float)27 / 56),
         (float)(floor(i / 5) * 11 / 10 + 0.1) * disp_data.height * 8 / 45,
         disp_data.width / 2 +
             disp_data.height *
                 ((float)(i % 5) * 629 / 3024 - (float)529 / 1512),
         (float)(floor(i / 5) * 11 / 10 + 1.1) * disp_data.height * 8 / 45,
         0, ipoker);
    card5[i].poker = ipoker;
  }
  for (int i = 0; i < 36; i++) {
    make(card6[i],
         disp_data.width / 2 +
             disp_data.height *
                 ((float)(i % 6) * 311 / 1809 - (float)65 / 134),
         (float)(floor(i / 6) * 11 / 10 + 0.1) * disp_data.height * 4 / 27,
         disp_data.width / 2 +
             disp_data.height *
                 ((float)(i % 6) * 311 / 1809 - (float)1355 / 3618),
         (float)(floor(i / 6) * 11 / 10 + 1.1) * disp_data.height * 4 / 27,
         0, ipoker);
    card6[i].poker = ipoker;
  }
  for (int i = 0; i < 49; i++) {
    make(card7[i],
         disp_data.width / 2 +
             disp_data.height *
                 ((float)(i % 7) * 463 / 3159 - (float)19 / 39),
         (float)(floor(i / 7) * 11 / 10 + 0.1) * disp_data.height * 8 / 63,
         disp_data.width / 2 +
             disp_data.height *
                 ((float)(i % 7) * 463 / 3159 - (float)413 / 1053),
         (float)(floor(i / 7) * 11 / 10 + 1.1) * disp_data.height * 8 / 63,
         0, ipoker);
    card7[i].poker = ipoker;
  }

  al_register_event_source(event_queue, al_get_display_event_source(display));
  al_register_event_source(event_queue, al_get_keyboard_event_source());
  al_register_event_source(event_queue, al_get_mouse_event_source());
  al_register_event_source(event_queue, al_get_timer_event_source(timer));
  al_start_timer(timer);

#ifdef __EMSCRIPTEN__
  // fps=0 lets the browser control the frame rate via requestAnimationFrame.
  // simulate_infinite_loop=1 means emscripten_set_main_loop does not return.
  emscripten_set_main_loop(tick, 0, 1);
#else
  while (!doexit) {
    tick();
  }
  destroyFaceBitmaps(faceBmps);
  al_destroy_bitmap(ititle);
  al_destroy_bitmap(iplay);
  al_destroy_bitmap(ihi);
  for (int i = 0; i < 5; i++)
    al_destroy_bitmap(ilevels[i]);
  al_destroy_bitmap(iback);
  al_destroy_bitmap(ipoker);
  al_destroy_bitmap(ireplay);
  al_destroy_font(font);
  al_destroy_timer(timer);
  al_destroy_event_queue(event_queue);
  al_destroy_display(display);
  return 0;
#endif
}

int click(Button button, float clickx, float clicky, int s) {
  if (clickx >= button.x1 && clicky >= button.y1 && clickx <= button.x2 &&
      clicky <= button.y2 && button.side && s <= 0) {
    return 1;
  } else {
    return 0;
  }
}

void draw(Button button) {
  if (button.side == 3) {
    al_draw_scaled_bitmap(button.poker, 0, 0,
                          al_get_bitmap_width(button.poker),
                          al_get_bitmap_height(button.poker), button.x1,
                          button.y1, button.x2 - button.x1,
                          button.y2 - button.y1, 0);
  } else if (button.side) {
    al_draw_scaled_bitmap(button.img, 0, 0, al_get_bitmap_width(button.img),
                          al_get_bitmap_height(button.img), button.x1,
                          button.y1, button.x2 - button.x1,
                          button.y2 - button.y1, 0);
  }
}

void make(Button &button, float x1, float y1, float x2, float y2, int side,
          ALLEGRO_BITMAP *img) {
  button.x1 = x1;
  button.y1 = y1;
  button.x2 = x2;
  button.y2 = y2;
  button.side = side;
  button.img = img;
}

// Step 4 (save): persist hiscores to localStorage on WASM, save.txt on desktop.
void exportSave(int hiscores[5][10]) {
#ifdef __EMSCRIPTEN__
  char buf[512];
  int pos = 0;
  for (int i = 0; i < 5; i++) {
    for (int j = 0; j < 10; j++) {
      bool last = (i == 4 && j == 9);
      int written = snprintf(buf + pos, (int)sizeof(buf) - pos,
                             last ? "%d" : "%d,", hiscores[i][j]);
      if (written > 0 && pos + written < (int)sizeof(buf))
        pos += written;
    }
  }
  buf[pos] = '\0';
  EM_ASM({ localStorage.setItem('memoroid_scores', UTF8ToString($0)); }, buf);
#else
  FILE *fptr = fopen("save.txt", "w");
  if (!fptr)
    return;
  for (int i = 0; i < 5; i++)
    for (int j = 0; j < 10; j++)
      fprintf(fptr, "%d\n", hiscores[i][j]);
  fclose(fptr);
#endif
}
