#ifndef BANNER_H
#define BANNER_H

#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <cairo/cairo-xlib.h>
#include <cairo/cairo.h>
#include <pango/pangocairo.h>
#include <pthread.h>

// UI Constants
#define BAR_HEIGHT 35
#define PANEL_WIDTH 300
#define MARGIN 20
#define LOGO_SIZE 180
#define LOGO_PATH "logo.png"

// App State
typedef struct {
  Display *disp;
  int scr;
  int screen_w;
  int screen_h;

  Window banner_win;
  Window panel_win;

  cairo_t *banner_cr;
  cairo_t *panel_cr;

  PangoLayout *banner_layout;
  char news_msg[2048];
  pthread_mutex_t news_mutex;
} AppState;

// Core setup for the UI
void setup_windows(AppState *state);
void *news_thread_fn(void *arg);

#endif
