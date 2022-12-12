#include <allegro5/allegro.h>
#include <allegro5/allegro_font.h>
#include <allegro5/allegro_image.h>
#include <allegro5/allegro_native_dialog.h>
#include <allegro5/allegro_primitives.h>
#include <allegro5/allegro_ttf.h>
#include <math.h>
#include <stdio.h>
#include <time.h>

#define BLACK al_map_rgb(0, 0, 0);
#define WHITE al_map_rgb(255, 255, 255);
#define BLUE al_map_rgb(197, 214, 237);

// Initialize constants
const float FPS = 200;
// Screen dimensions
const int SW = 1600;
const int SH = 900;

// Declare structs
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

// Prototype functions
int click(Button button, float clickx, float clicky, int stop);
void draw(Button button);
void make(Button &button, float x1, float y1, float x2, float y2, int side,
          ALLEGRO_BITMAP *img);
void exportSave(int hiscores[5][10]);

int compare(const void *a, const void *b) { return (*(int *)a - *(int *)b); }

int main(int argc, char *argv[]) {

  ALLEGRO_DISPLAY *display = nullptr;
  ALLEGRO_DISPLAY_MODE disp_data;
  // Add event queue object
  ALLEGRO_EVENT_QUEUE *event_queue = nullptr;
  // Add timer object
  ALLEGRO_TIMER *timer = nullptr;

  // Initialize Allegro
  al_init();
  // Initialize display
  al_get_display_mode(al_get_num_display_modes() - 1, &disp_data);
  al_set_new_display_flags(ALLEGRO_FULLSCREEN);
  display = al_create_display(disp_data.width, disp_data.height);
  if (!display) {
    al_show_native_message_box(display, "Error", "Error",
                               "Failed to initialize display!", nullptr,
                               ALLEGRO_MESSAGEBOX_ERROR);
    return -1;
  }
  al_set_window_title(display, "MEMOROID");
  // Initialize keyboard routines
  if (!al_install_keyboard()) {
    al_show_native_message_box(display, "Error", "Error",
                               "Failed to initialize the keyboard!", nullptr,
                               ALLEGRO_MESSAGEBOX_ERROR);
    return -1;
  }
  // Initialize mouse routines
  if (!al_install_mouse()) {
    al_show_native_message_box(display, "Error", "Error",
                               "Failed to initialize the mouse!", nullptr,
                               ALLEGRO_MESSAGEBOX_ERROR);
    return -1;
  }
  // Initialize the image processor
  if (!al_init_image_addon()) {
    al_show_native_message_box(display, "Error", "Error",
                               "Failed to initialize the image add-on!",
                               nullptr, ALLEGRO_MESSAGEBOX_ERROR);
    return -1;
  }
  // Set up the timer
  timer = al_create_timer(1.0 / FPS);
  if (!timer) {
    al_show_native_message_box(display, "Error", "Error",
                               "Failed to create the timer!", nullptr,
                               ALLEGRO_MESSAGEBOX_ERROR);
    return -1;
  }
  // Set up the event queue
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
  al_init_font_addon(); // initialize the font add on
  al_init_ttf_addon();  // initialize the ttf (True Type Font) add on
  ALLEGRO_FONT *font = al_load_ttf_font("comic.ttf", 50, 0);
  if (!font) {
    al_show_native_message_box(display, "Error", "Error",
                               "Could not load comic.ttf!", nullptr,
                               ALLEGRO_MESSAGEBOX_ERROR);
    return -1;
  }

  // Images
  ALLEGRO_BITMAP *ititle, *iplay, *ihi, *ilevels[5], *iback, *ipoker,
      *icards[25];
  ititle = al_load_bitmap("title.bmp");
  if (!ititle) {
    al_show_native_message_box(display, "Error", "Error",
                               "Failed to load graphics!", nullptr,
                               ALLEGRO_MESSAGEBOX_ERROR);
    al_destroy_display(display);
    return -1;
  }
  iplay = al_load_bitmap("play.bmp");
  if (!iplay) {
    al_show_native_message_box(display, "Error", "Error",
                               "Failed to load graphics!", nullptr,
                               ALLEGRO_MESSAGEBOX_ERROR);
    al_destroy_display(display);
    return -1;
  }
  ihi = al_load_bitmap("hiscore.bmp");
  if (!ihi) {
    al_show_native_message_box(display, "Error", "Error",
                               "Failed to load graphics!", nullptr,
                               ALLEGRO_MESSAGEBOX_ERROR);
    al_destroy_display(display);
    return -1;
  }
  ilevels[0] = al_load_bitmap("three.bmp");
  ilevels[1] = al_load_bitmap("four.bmp");
  ilevels[2] = al_load_bitmap("five.bmp");
  ilevels[3] = al_load_bitmap("six.bmp");
  ilevels[4] = al_load_bitmap("seven.bmp");
  for (int i = 0; i < 5; i++) {
    if (!ilevels[i]) {
      al_show_native_message_box(display, "Error", "Error",
                                 "Failed to load graphics!", nullptr,
                                 ALLEGRO_MESSAGEBOX_ERROR);
      al_destroy_display(display);
      return -1;
    }
  }
  iback = al_load_bitmap("back.bmp");
  ipoker = al_load_bitmap("poker.bmp");
  icards[0] = al_load_bitmap("mizuki.bmp");
  icards[1] = al_load_bitmap("aoba.bmp");
  icards[2] = al_load_bitmap("asuna.bmp");
  icards[3] = al_load_bitmap("chitoge.bmp");
  icards[4] = al_load_bitmap("chiyo.bmp");
  icards[5] = al_load_bitmap("emilia.bmp");
  icards[6] = al_load_bitmap("hifumin.bmp");
  icards[7] = al_load_bitmap("kosaki.bmp");
  icards[8] = al_load_bitmap("mashiron.bmp");
  icards[9] = al_load_bitmap("megumin.bmp");
  icards[10] = al_load_bitmap("mio.bmp");
  icards[11] = al_load_bitmap("misaka.bmp");
  icards[12] = al_load_bitmap("nao.bmp");
  icards[13] = al_load_bitmap("shiro.bmp");
  icards[14] = al_load_bitmap("taiga.bmp");
  icards[15] = al_load_bitmap("yui.bmp");
  icards[16] = al_load_bitmap("yuichan.bmp");
  icards[17] = al_load_bitmap("rem.bmp");
  icards[18] = al_load_bitmap("azusa.bmp");
  icards[19] = al_load_bitmap("yun.bmp");
  icards[20] = al_load_bitmap("ritsu.bmp");
  icards[21] = al_load_bitmap("nanamin.bmp");
  icards[22] = al_load_bitmap("yukinon.bmp");
  icards[23] = al_load_bitmap("vigne.bmp");
  icards[24] = al_load_bitmap("kaede.bmp");
  for (int i = 0; i < 25; i++) {
    if (!icards[i]) {
      al_show_native_message_box(display, "Error", "Error",
                                 "Failed to load graphics!", nullptr,
                                 ALLEGRO_MESSAGEBOX_ERROR);
      al_destroy_display(display);
      return -1;
    }
  }

  // Declare and initialize variables
  bool doexit = false;
  ALLEGRO_EVENT ev;
  srand(time(0));

  int deck[25], deck3[9], deck4[16], deck5[25], deck6[36], deck7[49], ind, temp,
      cardholder = -1, game = 0, check = 0, t = 0, hiscore[5][10], stop = 0;
  for (int i = 0; i < 25; i++) {
    deck[i] = i;
  }
  for (int i = 0; i < 5; i++) {
    for (int j = 0; j < 10; j++) {
      hiscore[i][j] = 600 * FPS;
    }
  }

  // Buttons
  Button title, play, hi, levels[5], bback, card3[9], card4[16], card5[25],
      card6[36], card7[49];
  make(title, disp_data.width / 4, disp_data.height / 100,
       3 * disp_data.width / 4, 109 * disp_data.height / 900, 1, ititle);
  make(play, 3 * disp_data.width / 8, 4 * disp_data.height / 9,
       5 * disp_data.width / 8, 2 * disp_data.height / 3, 1, iplay);
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
         icards[i % 5]);
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
         icards[i % 8]);
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
         icards[i % 10]);
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
         icards[i % 12]);
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
         icards[i % 25]);
    card7[i].poker = ipoker;
  }

  FILE *fptr;
  fptr = fopen("save.txt", "r");
  if (fptr != NULL) {
    for (int i = 0; i < 5; i++) {
      for (int j = 0; j < 10; j++) {
        fscanf(fptr, "%d", &hiscore[i][j]);
      }
    }
  }
  fclose(fptr);

  // Register timer events
  al_register_event_source(event_queue, al_get_display_event_source(display));
  al_register_event_source(event_queue, al_get_keyboard_event_source());
  al_register_event_source(event_queue, al_get_mouse_event_source());
  al_register_event_source(event_queue, al_get_timer_event_source(timer));

  // Start the timer
  al_start_timer(timer);

  // Main game

  while (!doexit) {

    al_wait_for_event(event_queue, &ev);
    ALLEGRO_MOUSE_STATE state;
    al_get_mouse_state(&state);
    ALLEGRO_MOUSE_STATE mouse;
    al_get_mouse_state(&mouse);

    // Close the game
    if (ev.type == ALLEGRO_EVENT_DISPLAY_CLOSE) {
      doexit = true;
    }

    else if (ev.type == ALLEGRO_EVENT_KEY_DOWN) {
      if (ev.keyboard.keycode == ALLEGRO_KEY_ESCAPE) {
        doexit = true;
      }
    }

    // Update the screen

    else if (ev.type == ALLEGRO_EVENT_TIMER) {

      al_clear_to_color(BLUE);

      // On-screen text
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

      // Timing the games

      if (game == 1 && !check) {
        t++;
      } else if (!check) {
        t = 0;
      }

      // Timing temporary disabling
      if (stop > 0) {
        stop--;
      }
    }

    // On click

    if (ev.type == ALLEGRO_EVENT_MOUSE_BUTTON_DOWN) {

      if (click(play, mouse.x, mouse.y, stop)) {
        play.side = 0;
        hi.side = 0;
        for (int i = 0; i < 5; i++) {
          levels[i].side = 1;
        }
        bback.side = 1;
        stop = FPS / 10;
      }

      if (click(hi, mouse.x, mouse.y, stop)) {
        play.side = 0;
        hi.side = 0;
        for (int i = 0; i < 5; i++) {
          levels[i].side = 2;
        }
        bback.side = 1;
        stop = FPS / 10;
      }

      if (click(bback, mouse.x, mouse.y, stop)) {
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
        stop = FPS / 10;
      }

      if (click(levels[0], mouse.x, mouse.y, stop) && levels[0].side == 1) {
        title.side = 0;
        bback.side = 0;
        for (int i = 0; i < 5; i++) {
          levels[i].side = 0;
        }
        for (int i = 0; i < 9; i++) {
          card3[i].side = 3;
        }
        for (int i = 0; i < 25; i++) {
          ind = rand() % 25;
          temp = deck[ind];
          deck[ind] = deck[i];
          deck[i] = temp;
        }
        for (int i = 0; i < 9; i++) {
          deck3[i] = deck[i % 5];
        }
        for (int i = 0; i < 9; i++) {
          ind = rand() % 9;
          temp = deck3[ind];
          deck3[ind] = deck3[i];
          deck3[i] = temp;
        }
        for (int i = 0; i < 9; i++) {
          card3[i].img = icards[deck3[i]];
        }
        game = 1;
        cardholder = -1;
        stop = FPS / 10;
      }

      for (int i = 0; i < 9; i++) {
        if (click(card3[i], mouse.x, mouse.y, stop) && card3[i].side % 2) {
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

      if (click(levels[0], mouse.x, mouse.y, stop) && levels[0].side == 2) {
        title.side = 0;
        for (int i = 0; i < 5; i++) {
          levels[i].side = 0;
        }
        game = 3;
      }

      if (click(levels[1], mouse.x, mouse.y, stop) && levels[1].side == 1) {
        title.side = 0;
        bback.side = 0;
        for (int i = 0; i < 5; i++) {
          levels[i].side = 0;
        }
        for (int i = 0; i < 16; i++) {
          card4[i].side = 3;
        }
        for (int i = 0; i < 25; i++) {
          ind = rand() % 25;
          temp = deck[ind];
          deck[ind] = deck[i];
          deck[i] = temp;
        }
        for (int i = 0; i < 16; i++) {
          deck4[i] = deck[i % 8];
        }
        for (int i = 0; i < 16; i++) {
          ind = rand() % 16;
          temp = deck4[ind];
          deck4[ind] = deck4[i];
          deck4[i] = temp;
        }
        for (int i = 0; i < 16; i++) {
          card4[i].img = icards[deck4[i]];
        }
        game = 1;
        cardholder = -1;
        stop = FPS / 10;
      }

      for (int i = 0; i < 16; i++) {
        if (click(card4[i], mouse.x, mouse.y, stop) && card4[i].side % 2) {
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

      if (click(levels[1], mouse.x, mouse.y, stop) && levels[1].side == 2) {
        title.side = 0;
        for (int i = 0; i < 5; i++) {
          levels[i].side = 0;
        }
        game = 4;
      }

      if (click(levels[2], mouse.x, mouse.y, stop) && levels[2].side == 1) {
        title.side = 0;
        bback.side = 0;
        for (int i = 0; i < 5; i++) {
          levels[i].side = 0;
        }
        for (int i = 0; i < 25; i++) {
          card5[i].side = 3;
        }
        for (int i = 0; i < 25; i++) {
          ind = rand() % 25;
          temp = deck[ind];
          deck[ind] = deck[i];
          deck[i] = temp;
        }
        for (int i = 0; i < 25; i++) {
          deck5[i] = deck[i % 13];
        }
        for (int i = 0; i < 25; i++) {
          ind = rand() % 25;
          temp = deck5[ind];
          deck5[ind] = deck5[i];
          deck5[i] = temp;
        }
        for (int i = 0; i < 25; i++) {
          card5[i].img = icards[deck5[i]];
        }
        game = 1;
        cardholder = -1;
        stop = FPS / 10;
      }

      for (int i = 0; i < 25; i++) {
        if (click(card5[i], mouse.x, mouse.y, stop) && card5[i].side % 2) {
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

      if (click(levels[2], mouse.x, mouse.y, stop) && levels[2].side == 2) {
        title.side = 0;
        for (int i = 0; i < 5; i++) {
          levels[i].side = 0;
        }
        game = 5;
      }

      if (click(levels[3], mouse.x, mouse.y, stop) && levels[3].side == 1) {
        title.side = 0;
        bback.side = 0;
        for (int i = 0; i < 5; i++) {
          levels[i].side = 0;
        }
        for (int i = 0; i < 36; i++) {
          card6[i].side = 3;
        }
        for (int i = 0; i < 25; i++) {
          ind = rand() % 25;
          temp = deck[ind];
          deck[ind] = deck[i];
          deck[i] = temp;
        }
        for (int i = 0; i < 36; i++) {
          deck6[i] = deck[i % 18];
        }
        for (int i = 0; i < 36; i++) {
          ind = rand() % 36;
          temp = deck6[ind];
          deck6[ind] = deck6[i];
          deck6[i] = temp;
        }
        for (int i = 0; i < 36; i++) {
          card6[i].img = icards[deck6[i]];
        }
        game = 1;
        cardholder = -1;
        stop = FPS / 10;
      }

      for (int i = 0; i < 36; i++) {
        if (click(card6[i], mouse.x, mouse.y, stop) && card6[i].side % 2) {
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

      if (click(levels[3], mouse.x, mouse.y, stop) && levels[3].side == 2) {
        title.side = 0;
        for (int i = 0; i < 5; i++) {
          levels[i].side = 0;
        }
        game = 6;
      }

      if (click(levels[4], mouse.x, mouse.y, stop) && levels[4].side == 1) {
        title.side = 0;
        bback.side = 0;
        for (int i = 0; i < 5; i++) {
          levels[i].side = 0;
        }
        for (int i = 0; i < 49; i++) {
          card7[i].side = 3;
        }
        for (int i = 0; i < 25; i++) {
          ind = rand() % 25;
          temp = deck[ind];
          deck[ind] = deck[i];
          deck[i] = temp;
        }
        for (int i = 0; i < 49; i++) {
          deck7[i] = deck[i % 25];
        }
        for (int i = 0; i < 49; i++) {
          ind = rand() % 25;
          temp = deck7[ind];
          deck7[ind] = deck7[i];
          deck7[i] = temp;
        }
        for (int i = 0; i < 49; i++) {
          card7[i].img = icards[deck7[i]];
        }
        game = 1;
        cardholder = -1;
        stop = FPS / 10;
      }

      for (int i = 0; i < 49; i++) {
        if (click(card7[i], mouse.x, mouse.y, stop) && card7[i].side % 2) {
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

      if (click(levels[4], mouse.x, mouse.y, stop) && levels[4].side == 2) {
        title.side = 0;
        for (int i = 0; i < 5; i++) {
          levels[i].side = 0;
        }
        game = 7;
      }
    }

    al_flip_display();
  }

  al_destroy_display(display);
  return 0;
}

// Functions

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

  FILE *fptr;

  fptr = fopen("save.txt", "w");

  for (int i = 0; i < 5; i++) {
    for (int j = 0; j < 10; j++) {
      fprintf(fptr, "%d\n", hiscores[i][j]);
    }
  }

  fclose(fptr);
}