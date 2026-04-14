#include <allegro5/allegro.h>
#include <allegro5/allegro_font.h>
#include <allegro5/allegro_image.h>
#include <allegro5/allegro_native_dialog.h>
#include <allegro5/allegro_primitives.h>
#include <allegro5/allegro_ttf.h>
#include <math.h>
#include <stdio.h>
#include <time.h>

#include <algorithm>
#include <cstring>
#include <filesystem>
#include <string>
#include <vector>

#define BLACK al_map_rgb(0, 0, 0)
#define WHITE al_map_rgb(255, 255, 255)
#define BLUE al_map_rgb(197, 214, 237)

namespace {

const float FPS = 60;
const int SW = 1600;
const int SH = 900;

static const char *ASSETS_UI = "assets/ui/";
static const char *ASSETS_CHAR = "assets/characters/";
static const char *ASSETS_FONTS = "assets/fonts/";

static const int MIN_CHARS_FOR_LEVEL[5] = {5, 8, 13, 18, 22};

static std::string g_slot_slug[25];

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

void destroyFaceBitmaps(ALLEGRO_BITMAP *faceBmps[25]) {
  for (int i = 0; i < 25; i++) {
    if (faceBmps[i]) {
      al_destroy_bitmap(faceBmps[i]);
      faceBmps[i] = nullptr;
    }
  }
}

bool loadFaceForLogicalId(int logicalId, ALLEGRO_BITMAP *faceBmps[25]) {
  if (logicalId < 0 || logicalId >= 25)
    return false;
  if (faceBmps[logicalId])
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
  faceBmps[logicalId] = b;
  return true;
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

void pickGameSlotSlugs(const std::vector<std::string> &all_slugs) {
  int N = (int)all_slugs.size();
  if (N == 0)
    return;
  if (N >= 25) {
    std::vector<std::string> copy = all_slugs;
    for (int i = 0; i < 25; i++) {
      int j = i + rand() % (N - i);
      std::swap(copy[i], copy[j]);
    }
    for (int i = 0; i < 25; i++)
      g_slot_slug[i] = copy[i];
  } else {
    for (int i = 0; i < 25; i++)
      g_slot_slug[i] = all_slugs[rand() % N];
  }
}

void shuffleDeck(int deck[25]) {
  for (int i = 0; i < 25; i++)
    deck[i] = i;
  for (int i = 0; i < 25; i++) {
    int j = rand() % 25;
    int temp = deck[i];
    deck[i] = deck[j];
    deck[j] = temp;
  }
}

ALLEGRO_BITMAP *loadUiBitmap(const char *filename) {
  char buf[512];
  snprintf(buf, sizeof buf, "%s%s", ASSETS_UI, filename);
  return al_load_bitmap(buf);
}

ALLEGRO_FONT *loadGameFont() {
  char buf[512];
  snprintf(buf, sizeof buf, "%scomic.ttf", ASSETS_FONTS);
  ALLEGRO_FONT *f = al_load_ttf_font(buf, 50, 0);
  if (!f)
    f = al_load_ttf_font("comic.ttf", 50, 0);
  if (!f)
    f = al_create_builtin_font();
  return f;
}

} // namespace

struct Button {

  float x1;
  float y1;
  float x2;
  float y2;
  int side;
  ALLEGRO_BITMAP *img;
  ALLEGRO_BITMAP *poker;

  // Side 0 - inactive
  // Side 1 - normal active
  // Side 2 - high scores active
  // Side 3 - Reverse card active
  // Side 4 - Complete card active
};

namespace {

void wireCardImages(int *deckvals, int n, Button *cards, ALLEGRO_BITMAP *faceBmps[25]) {
  for (int i = 0; i < n; i++) {
    int id = deckvals[i];
    loadFaceForLogicalId(id, faceBmps);
    cards[i].img = faceBmps[id];
  }
}

} // namespace

void layoutReplayGame(Button &r, int w, int h) {
  /* Mirror of the Back button (which is at w/100 .. 3w/25) on the right side. */
  r.x1 = (float)w - 3.f * (float)w / 25.f;
  r.x2 = (float)w - (float)w / 100.f;
  r.y1 = (float)(791 * h) / 900.f;
  r.y2 = (float)(99 * h) / 100.f;
}

int click(Button button, float clickx, float clicky, int stop);
void draw(Button button);
void make(Button &button, float x1, float y1, float x2, float y2, int side,
          ALLEGRO_BITMAP *img);
void exportSave(int hiscores[5][10]);

int compare(const void *a, const void *b) { return (*(int *)a - *(int *)b); }

int main(int argc, char *argv[]) {

  ALLEGRO_DISPLAY *display = nullptr;
  ALLEGRO_DISPLAY_MODE disp_data;
  ALLEGRO_EVENT_QUEUE *event_queue = nullptr;
  ALLEGRO_TIMER *timer = nullptr;

  al_init();
  /* Windowed: avoids fullscreen resolution/scaling weirdness and mouse grab. */
  al_set_new_display_flags(ALLEGRO_WINDOWED);
  disp_data.width = SW;
  disp_data.height = SH;
  display = al_create_display(SW, SH);
  if (!display) {
    al_show_native_message_box(display, "Error", "Error",
                               "Failed to initialize display!", nullptr,
                               ALLEGRO_MESSAGEBOX_ERROR);
    return -1;
  }
  al_set_window_title(display, "MEMOROID");
  if (!al_install_keyboard()) {
    al_show_native_message_box(display, "Error", "Error",
                               "Failed to initialize the keyboard!", nullptr,
                               ALLEGRO_MESSAGEBOX_ERROR);
    return -1;
  }
  if (!al_install_mouse()) {
    al_show_native_message_box(display, "Error", "Error",
                               "Failed to initialize the mouse!", nullptr,
                               ALLEGRO_MESSAGEBOX_ERROR);
    return -1;
  }
  if (!al_init_image_addon()) {
    al_show_native_message_box(display, "Error", "Error",
                               "Failed to initialize the image add-on!",
                               nullptr, ALLEGRO_MESSAGEBOX_ERROR);
    return -1;
  }
  timer = al_create_timer(1.0 / FPS);
  if (!timer) {
    al_show_native_message_box(display, "Error", "Error",
                               "Failed to create the timer!", nullptr,
                               ALLEGRO_MESSAGEBOX_ERROR);
    return -1;
  }
  event_queue = al_create_event_queue();
  if (!event_queue) {
    al_show_native_message_box(display, "Error", "Error",
                               "Failed to create the event queue!", nullptr,
                               ALLEGRO_MESSAGEBOX_ERROR);
    al_destroy_display(display);
    return -1;
  }
  if (!al_init_primitives_addon()) {
    al_show_native_message_box(display, "Error", "Error",
                               "Failed to initialize primatives add-on!",
                               nullptr, ALLEGRO_MESSAGEBOX_ERROR);
    return -1;
  }
  al_init_font_addon();
  al_init_ttf_addon();
  ALLEGRO_FONT *font = loadGameFont();
  if (!font) {
    al_show_native_message_box(display, "Error", "Error",
                               "Could not create font!", nullptr,
                               ALLEGRO_MESSAGEBOX_ERROR);
    return -1;
  }

  ALLEGRO_BITMAP *ititle, *iplay, *ihi, *ilevels[5], *iback, *ipoker, *ireplay;
  ALLEGRO_BITMAP *faceBmps[25] = {};

  ititle = loadUiBitmap("title.bmp");
  if (!ititle) {
    al_show_native_message_box(display, "Error", "Error",
                               "Failed to load assets/ui/title.bmp!", nullptr,
                               ALLEGRO_MESSAGEBOX_ERROR);
    al_destroy_display(display);
    return -1;
  }
  iplay = loadUiBitmap("play.bmp");
  if (!iplay) {
    al_show_native_message_box(display, "Error", "Error",
                               "Failed to load assets/ui/play.bmp!", nullptr,
                               ALLEGRO_MESSAGEBOX_ERROR);
    al_destroy_display(display);
    return -1;
  }
  ihi = loadUiBitmap("hiscore.bmp");
  if (!ihi) {
    al_show_native_message_box(display, "Error", "Error",
                               "Failed to load assets/ui/hiscore.bmp!", nullptr,
                               ALLEGRO_MESSAGEBOX_ERROR);
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
      al_show_native_message_box(display, "Error", "Error",
                                 "Failed to load level graphics!", nullptr,
                                 ALLEGRO_MESSAGEBOX_ERROR);
      al_destroy_display(display);
      return -1;
    }
  }
  iback = loadUiBitmap("back.bmp");
  ipoker = loadUiBitmap("poker.bmp");
  if (!iback || !ipoker) {
    al_show_native_message_box(display, "Error", "Error",
                               "Failed to load assets/ui/back.bmp or poker.bmp!",
                               nullptr, ALLEGRO_MESSAGEBOX_ERROR);
    al_destroy_display(display);
    return -1;
  }
  ireplay = loadUiBitmap("replay.bmp");
  if (!ireplay) {
    al_show_native_message_box(display, "Error", "Error",
                               "Failed to load assets/ui/replay.bmp!", nullptr,
                               ALLEGRO_MESSAGEBOX_ERROR);
    al_destroy_display(display);
    return -1;
  }

  std::vector<std::string> all_slugs;
  scanCharacterSlugs(all_slugs);
  if (all_slugs.empty()) {
    al_show_native_message_box(
        display, "Error", "Error",
        "No character art found under assets/characters/. Add at least one "
        ".bmp per folder.",
        nullptr, ALLEGRO_MESSAGEBOX_ERROR);
    al_destroy_display(display);
    return -1;
  }

  bool doexit = false;
  ALLEGRO_EVENT ev;
  srand(time(0));

  int deck[25], deck3[9], deck4[16], deck5[25], deck6[36], deck7[49], ind, temp,
      cardholder = -1, game = 0, check = 0, t = 0, hiscore[5][10], stop = 0;
  int last_level = -1;
  int current_level = 0;
  bool touched_card_this_game = false;
  for (int i = 0; i < 25; i++) {
    deck[i] = i;
  }
  for (int i = 0; i < 5; i++) {
    for (int j = 0; j < 10; j++) {
      hiscore[i][j] = 600 * FPS;
    }
  }

  Button title, play, hi, replay, levels[5], bback, card3[9], card4[16],
      card5[25], card6[36], card7[49];
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
         4 * disp_data.height / 9, (2 + (float)i * 3 / 2) * disp_data.width / 9,
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
             disp_data.height * ((float)(i % 3) * 166 / 459 - (float)116 / 459),
         (float)(floor(i / 3) * 11 / 10 + 1.1) * disp_data.height * 8 / 27, 0,
         ipoker);
    card3[i].poker = ipoker;
  }
  for (int i = 0; i < 16; i++) {
    make(card4[i],
         disp_data.width / 2 +
             disp_data.height * ((float)(i % 4) * 961 / 3645 - (float)43 / 90),
         (float)(floor(i / 4) * 11 / 10 + 0.1) * disp_data.height * 2 / 9,
         disp_data.width / 2 + disp_data.height * ((float)(i % 4) * 961 / 3645 -
                                                   (float)761 / 2430),
         (float)(floor(i / 4) * 11 / 10 + 1.1) * disp_data.height * 2 / 9, 0,
         ipoker);
    card4[i].poker = ipoker;
  }
  for (int i = 0; i < 25; i++) {
    make(card5[i],
         disp_data.width / 2 +
             disp_data.height * ((float)(i % 5) * 629 / 3024 - (float)27 / 56),
         (float)(floor(i / 5) * 11 / 10 + 0.1) * disp_data.height * 8 / 45,
         disp_data.width / 2 + disp_data.height * ((float)(i % 5) * 629 / 3024 -
                                                   (float)529 / 1512),
         (float)(floor(i / 5) * 11 / 10 + 1.1) * disp_data.height * 8 / 45, 0,
         ipoker);
    card5[i].poker = ipoker;
  }
  for (int i = 0; i < 36; i++) {
    make(card6[i],
         disp_data.width / 2 +
             disp_data.height * ((float)(i % 6) * 311 / 1809 - (float)65 / 134),
         (float)(floor(i / 6) * 11 / 10 + 0.1) * disp_data.height * 4 / 27,
         disp_data.width / 2 + disp_data.height * ((float)(i % 6) * 311 / 1809 -
                                                   (float)1355 / 3618),
         (float)(floor(i / 6) * 11 / 10 + 1.1) * disp_data.height * 4 / 27, 0,
         ipoker);
    card6[i].poker = ipoker;
  }
  for (int i = 0; i < 49; i++) {
    make(card7[i],
         disp_data.width / 2 +
             disp_data.height * ((float)(i % 7) * 463 / 3159 - (float)19 / 39),
         (float)(floor(i / 7) * 11 / 10 + 0.1) * disp_data.height * 8 / 63,
         disp_data.width / 2 + disp_data.height * ((float)(i % 7) * 463 / 3159 -
                                                   (float)413 / 1053),
         (float)(floor(i / 7) * 11 / 10 + 1.1) * disp_data.height * 8 / 63, 0,
         ipoker);
    card7[i].poker = ipoker;
  }

  FILE *fptr = fopen("save.txt", "r");
  if (fptr != NULL) {
    for (int i = 0; i < 5; i++) {
      for (int j = 0; j < 10; j++) {
        (void)fscanf(fptr, "%d", &hiscore[i][j]);
      }
    }
    fclose(fptr);
  }

  al_register_event_source(event_queue, al_get_display_event_source(display));
  al_register_event_source(event_queue, al_get_keyboard_event_source());
  al_register_event_source(event_queue, al_get_mouse_event_source());
  al_register_event_source(event_queue, al_get_timer_event_source(timer));

  al_start_timer(timer);

  auto beginLevel = [&](int L) {
    if ((int)all_slugs.size() < MIN_CHARS_FOR_LEVEL[L]) {
      al_show_native_message_box(
          display, "Memoroid", "Not enough characters",
          "Add more character folders under assets/characters/.", nullptr,
          ALLEGRO_MESSAGEBOX_ERROR);
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
        ind = rand() % 25;
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
  };

  auto returnToMainMenu = [&]() {
    destroyFaceBitmaps(faceBmps);
    replay.side = 0;
    bback.side = 0;
    for (int i = 0; i < 5; i++) {
      levels[i].side = 0;
    }
    t = 0;
    check = 0;
    game = 0;
    title.side = 1;
    play.side = 1;
    hi.side = 1;
    for (int i = 0; i < 9; i++) {
      card3[i].side = 0;
    }
    for (int i = 0; i < 16; i++) {
      card4[i].side = 0;
    }
    for (int i = 0; i < 25; i++) {
      card5[i].side = 0;
    }
    for (int i = 0; i < 36; i++) {
      card6[i].side = 0;
    }
    for (int i = 0; i < 49; i++) {
      card7[i].side = 0;
    }
    touched_card_this_game = false;
    stop = FPS / 10;
  };

  while (!doexit) {

    al_wait_for_event(event_queue, &ev);
    int timerTicks = 0;
    bool mouseHandled = false;
    bool keyHandled = false;
    do {
      if (ev.type == ALLEGRO_EVENT_DISPLAY_CLOSE) {
        doexit = true;
      } else if (ev.type == ALLEGRO_EVENT_KEY_DOWN) {
        if (ev.keyboard.keycode == ALLEGRO_KEY_ESCAPE) {
          if (game == 1 || (game >= 3 && game <= 7)) {
            returnToMainMenu();
            keyHandled = true;
          } else {
            doexit = true;
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
        for (int i = 0; i < 5; i++) {
          levels[i].side = 1;
        }
        bback.side = 1;
        stop = FPS / 10;
      }

      if (click(hi, mx, my, stop)) {
        play.side = 0;
        hi.side = 0;
        for (int i = 0; i < 5; i++) {
          levels[i].side = 2;
        }
        bback.side = 1;
        stop = FPS / 10;
      }

      if (click(replay, mx, my, stop)) {
        if (game == 1)
          beginLevel(current_level);
      }

      if (click(bback, mx, my, stop)) {
        returnToMainMenu();
      }

      if (click(levels[0], mx, my, stop) && levels[0].side == 1) {
        beginLevel(0);
      }

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
                if (card3[k].side != 4) { card3[k].side = 4; break; }
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
              if (card3[j].side == 1 && j != i) {
                card3[j].side = 3;
              }
            }
            cardholder = i;
          }
        }
      }

      if (click(levels[0], mx, my, stop) && levels[0].side == 2) {
        title.side = 0;
        for (int i = 0; i < 5; i++) {
          levels[i].side = 0;
        }
        game = 3;
      }

      if (click(levels[1], mx, my, stop) && levels[1].side == 1) {
        beginLevel(1);
      }

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
              if (card4[i].side != 4) {
                check = 0;
              }
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
              if (card4[j].side == 1 && j != i) {
                card4[j].side = 3;
              }
            }
            cardholder = i;
          }
        }
      }

      if (click(levels[1], mx, my, stop) && levels[1].side == 2) {
        title.side = 0;
        for (int i = 0; i < 5; i++) {
          levels[i].side = 0;
        }
        game = 4;
      }

      if (click(levels[2], mx, my, stop) && levels[2].side == 1) {
        beginLevel(2);
      }

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
                if (card5[k].side != 4) { card5[k].side = 4; break; }
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
              if (card5[j].side == 1 && j != i) {
                card5[j].side = 3;
              }
            }
            cardholder = i;
          }
        }
      }

      if (click(levels[2], mx, my, stop) && levels[2].side == 2) {
        title.side = 0;
        for (int i = 0; i < 5; i++) {
          levels[i].side = 0;
        }
        game = 5;
      }

      if (click(levels[3], mx, my, stop) && levels[3].side == 1) {
        beginLevel(3);
      }

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
              if (card6[i].side != 4) {
                check = 0;
              }
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
              if (card6[j].side == 1 && j != i) {
                card6[j].side = 3;
              }
            }
            cardholder = i;
          }
        }
      }

      if (click(levels[3], mx, my, stop) && levels[3].side == 2) {
        title.side = 0;
        for (int i = 0; i < 5; i++) {
          levels[i].side = 0;
        }
        game = 6;
      }

      if (click(levels[4], mx, my, stop) && levels[4].side == 1) {
        beginLevel(4);
      }

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
                if (card7[k].side != 4) { card7[k].side = 4; break; }
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
              if (card7[j].side == 1 && j != i) {
                card7[j].side = 3;
              }
            }
            cardholder = i;
          }
        }
      }

      if (click(levels[4], mx, my, stop) && levels[4].side == 2) {
        title.side = 0;
        for (int i = 0; i < 5; i++) {
          levels[i].side = 0;
        }
        game = 7;
      }
      }
    } while (al_get_next_event(event_queue, &ev));

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
      for (int i = 0; i < 5; i++) {
        draw(levels[i]);
      }
      draw(bback);
      for (int i = 0; i < 9; i++) {
        draw(card3[i]);
      }
      for (int i = 0; i < 16; i++) {
        draw(card4[i]);
      }
      for (int i = 0; i < 25; i++) {
        draw(card5[i]);
      }
      for (int i = 0; i < 36; i++) {
        draw(card6[i]);
      }
      for (int i = 0; i < 49; i++) {
        draw(card7[i]);
      }
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
}

int click(Button button, float clickx, float clicky, int stop) {
  if (clickx >= button.x1 && clicky >= button.y1 && clickx <= button.x2 &&
      clicky <= button.y2 && button.side && stop <= 0) {
    return 1;
  } else {
    return 0;
  }
}

void draw(Button button) {

  if (button.side == 3) {
    al_draw_scaled_bitmap(button.poker, 0, 0, al_get_bitmap_width(button.poker),
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

void exportSave(int hiscores[5][10]) {

  FILE *fptr = fopen("save.txt", "w");
  if (!fptr)
    return;

  for (int i = 0; i < 5; i++) {
    for (int j = 0; j < 10; j++) {
      fprintf(fptr, "%d\n", hiscores[i][j]);
    }
  }

  fclose(fptr);
}
