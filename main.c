#include "banner.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

// ── Configuration ──────────────────────────────────────────────────────────

const char *news[] = {"Desarrollando en C sobre Debian 🚀",
                      "Sígueme en github.com/xeland314 👨‍💻",
                      "Proyecto actual: Side Panel Refactor 🛠️"};
#define NEWS_COUNT (sizeof(news) / sizeof(news[0]))

// ── Metrics & Helpers ───────────────────────────────────────────────────────

static float get_ram_usage(void) {
  FILE *fp = fopen("/proc/meminfo", "r");
  if (!fp)
    return 0.0f;
  long total = 0, avail = 0;
  char line[128];
  while (fgets(line, sizeof(line), fp)) {
    if (sscanf(line, "MemTotal: %ld kB", &total) == 1)
      continue;
    if (sscanf(line, "MemAvailable: %ld kB", &avail) == 1)
      continue;
  }
  fclose(fp);
  return (total == 0) ? 0.0f : 1.0f - (float)avail / (float)total;
}

static float get_cpu_usage(void) {
  static long p_idle = 0, p_total = 0;
  FILE *fp = fopen("/proc/stat", "r");
  if (!fp)
    return 0.0f;
  long u, n, s, id, wa, hi, si, st;
  if (fscanf(fp, "cpu %ld %ld %ld %ld %ld %ld %ld %ld", &u, &n, &s, &id, &wa,
             &hi, &si, &st) < 8) {
    fclose(fp);
    return 0.0f;
  }
  fclose(fp);
  long idle = id + wa, total = u + n + s + id + wa + hi + si + st;
  long d_idle = idle - p_idle, d_total = total - p_total;
  p_idle = idle;
  p_total = total;
  return (d_total == 0) ? 0.0f : 1.0f - (float)d_idle / (float)d_total;
}

static PangoLayout *make_layout(cairo_t *cr, const char *family, int size_pt,
                                int bold) {
  PangoLayout *l = pango_cairo_create_layout(cr);
  char desc[128];
  snprintf(desc, sizeof(desc), "%s %s %d", family, bold ? "Bold" : "", size_pt);
  PangoFontDescription *fd = pango_font_description_from_string(desc);
  pango_layout_set_font_description(l, fd);
  pango_font_description_free(fd);
  return l;
}

static void draw_pango(cairo_t *cr, PangoLayout *layout, const char *text,
                       int markup, double x, double y, double r, double g,
                       double b, int center) {
  if (!text || !text[0])
    return;
  if (markup)
    pango_layout_set_markup(layout, text, -1);
  else
    pango_layout_set_text(layout, text, -1);
  if (center) {
    pango_layout_set_width(layout, PANEL_WIDTH * PANGO_SCALE);
    pango_layout_set_alignment(layout, PANGO_ALIGN_CENTER);
    x = 0;
  } else {
    pango_layout_set_width(layout, (PANEL_WIDTH - MARGIN * 2) * PANGO_SCALE);
    pango_layout_set_alignment(layout, PANGO_ALIGN_LEFT);
  }
  cairo_set_source_rgb(cr, r, g, b);
  cairo_move_to(cr, x, y);
  pango_cairo_show_layout(cr, layout);
}

static void draw_bar(cairo_t *cr, double x, double y, double w, double h,
                     float val, double br, double bg, double bb, double fr,
                     double fg, double fb) {
  cairo_set_source_rgb(cr, br, bg, bb);
  cairo_rectangle(cr, x, y, w, h);
  cairo_fill(cr);
  if (val > 0.90f)
    cairo_set_source_rgb(cr, 0.9, 0.2, 0.2);
  else
    cairo_set_source_rgb(cr, fr, fg, fb);
  cairo_rectangle(cr, x, y, w * val, h);
  cairo_fill(cr);
}

static void draw_sep(cairo_t *cr, double y) {
  cairo_set_source_rgba(cr, 1, 1, 1, 0.12);
  cairo_rectangle(cr, MARGIN, y, PANEL_WIDTH - MARGIN * 2, 1);
  cairo_fill(cr);
}

// ── Implementation of Banner Hooks ──────────────────────────────────────────

void *news_thread_fn(void *arg) {
  AppState *s = (AppState *)arg;
  char weather[256] = "⏳ Cargando clima...";
  int idx = 0, count = 0;

  while (1) {
    if (count % 10 == 0) {
      FILE *p =
          popen("curl -sf --max-time 8 \"wttr.in/Quito?format=%c+%t\"", "r");
      if (p) {
        char tmp[128];
        if (fgets(tmp, sizeof(tmp), p)) {
          tmp[strcspn(tmp, "\n")] = '\0';
          snprintf(weather, sizeof(weather),
                   "<span foreground='#10b981'> %s </span>", tmp);
        }
        pclose(p);
      }
    }
    time_t t = time(NULL);
    struct tm *tm = localtime(&t);
    char date[128];
    strftime(date, sizeof(date), "%A, %d de %B | %H:%M", tm);

    pthread_mutex_lock(&s->news_mutex);
    snprintf(
        s->news_msg, sizeof(s->news_msg),
        "<span foreground='#ffffff'>[ %s ]</span> <span foreground='#fbbf24'> "
        "@xeland314 </span> • %s • <span foreground='#d1d5db'> %s </span>",
        date, weather, news[idx]);
    pthread_mutex_unlock(&s->news_mutex);

    idx = (idx + 1) % NEWS_COUNT;
    count++;
    sleep(60);
  }
  return NULL;
}

void setup_windows(AppState *s) {
  s->disp = XOpenDisplay(NULL);
  if (!s->disp)
    exit(1);
  s->scr = DefaultScreen(s->disp);
  s->screen_w = DisplayWidth(s->disp, s->scr);
  s->screen_h = DisplayHeight(s->disp, s->scr);

  int bar_w = s->screen_w - PANEL_WIDTH;
  s->banner_win = XCreateSimpleWindow(s->disp, RootWindow(s->disp, s->scr), 0,
                                      0, bar_w, BAR_HEIGHT, 0, 0, 0x000000);
  s->panel_win = XCreateSimpleWindow(s->disp, RootWindow(s->disp, s->scr),
                                     s->screen_w - PANEL_WIDTH, 0, PANEL_WIDTH,
                                     s->screen_h, 0, 0, 0x1e3a8a);

  Atom wtype = XInternAtom(s->disp, "_NET_WM_WINDOW_TYPE", False);
  Atom dock = XInternAtom(s->disp, "_NET_WM_WINDOW_TYPE_DOCK", False);
  Atom strut = XInternAtom(s->disp, "_NET_WM_STRUT_PARTIAL", False);

  XChangeProperty(s->disp, s->banner_win, wtype, XA_ATOM, 32, PropModeReplace,
                  (unsigned char *)&dock, 1);
  long b_sv[12] = {0, 0, BAR_HEIGHT, 0, 0, 0, 0, 0, 0, bar_w, 0, 0};
  XChangeProperty(s->disp, s->banner_win, strut, XA_CARDINAL, 32,
                  PropModeReplace, (unsigned char *)b_sv, 12);

  XChangeProperty(s->disp, s->panel_win, wtype, XA_ATOM, 32, PropModeReplace,
                  (unsigned char *)&dock, 1);
  long p_sv[12] = {0, PANEL_WIDTH, 0, 0, 0, 0, 0, s->screen_h, 0, 0, 0, 0};
  XChangeProperty(s->disp, s->panel_win, strut, XA_CARDINAL, 32,
                  PropModeReplace, (unsigned char *)p_sv, 12);

  XMapWindow(s->disp, s->banner_win);
  XMapWindow(s->disp, s->panel_win);

  s->banner_cr = cairo_create(cairo_xlib_surface_create(
      s->disp, s->banner_win, DefaultVisual(s->disp, s->scr), bar_w,
      BAR_HEIGHT));
  s->panel_cr = cairo_create(cairo_xlib_surface_create(
      s->disp, s->panel_win, DefaultVisual(s->disp, s->scr), PANEL_WIDTH,
      s->screen_h));
  s->banner_layout = pango_cairo_create_layout(s->banner_cr);
  PangoFontDescription *fd = pango_font_description_from_string("Inter 14");
  pango_layout_set_font_description(s->banner_layout, fd);
  pango_font_description_free(fd);

  pthread_mutex_init(&s->news_mutex, NULL);
  strcpy(s->news_msg, "Cargando...");
}

// ── Main Entry ──────────────────────────────────────────────────────────────

int main(void) {
  AppState s;
  setup_windows(&s);

  pthread_t t;
  pthread_create(&t, NULL, news_thread_fn, &s);

  cairo_surface_t *logo = cairo_image_surface_create_from_png(LOGO_PATH);
  int has_logo = (cairo_surface_status(logo) == CAIRO_STATUS_SUCCESS);

  PangoLayout *lay_name = make_layout(s.panel_cr, "Inter", 24, 1);
  PangoLayout *lay_role = make_layout(s.panel_cr, "Inter", 13, 0);
  PangoLayout *lay_section = make_layout(s.panel_cr, "Inter", 11, 1);
  PangoLayout *lay_item = make_layout(s.panel_cr, "Inter", 12, 0);
  PangoLayout *lay_metric = make_layout(s.panel_cr, "Inter", 8, 0);
  PangoLayout *lay_link = make_layout(s.panel_cr, "Inter", 11, 0);

  pango_layout_set_width(lay_item, (PANEL_WIDTH - MARGIN * 2) * PANGO_SCALE);
  pango_layout_set_wrap(lay_item, PANGO_WRAP_WORD_CHAR);

  double x_ban = s.screen_w - PANEL_WIDTH;
  int tick = 0;

  while (1) {
    while (XPending(s.disp)) {
      XEvent ev;
      XNextEvent(s.disp, &ev);
    }

    // Banner
    cairo_set_source_rgb(s.banner_cr, 0.05, 0.1, 0.3);
    cairo_paint(s.banner_cr);
    pthread_mutex_lock(&s.news_mutex);
    pango_layout_set_markup(s.banner_layout, s.news_msg, -1);
    pthread_mutex_unlock(&s.news_mutex);
    int tw, th;
    pango_layout_get_pixel_size(s.banner_layout, &tw, &th);
    cairo_set_source_rgb(s.banner_cr, 1, 1, 1);
    cairo_move_to(s.banner_cr, x_ban, (BAR_HEIGHT - th) / 2.0);
    pango_cairo_show_layout(s.banner_cr, s.banner_layout);
    x_ban -= 2;
    if (x_ban < -tw)
      x_ban = s.screen_w - PANEL_WIDTH;

    // Panel (30fps render)
    if (tick % 30 == 0) {
      cairo_set_source_rgb(s.panel_cr, 0.074, 0.196, 0.541);
      cairo_paint(s.panel_cr);
      double py = 30.0;
      if (has_logo) {
        cairo_save(s.panel_cr);
        cairo_translate(s.panel_cr, (PANEL_WIDTH - LOGO_SIZE) / 2.0, py);
        cairo_scale(s.panel_cr,
                    (double)LOGO_SIZE / cairo_image_surface_get_width(logo),
                    (double)LOGO_SIZE / cairo_image_surface_get_height(logo));
        cairo_set_source_surface(s.panel_cr, logo, 0, 0);
        cairo_paint(s.panel_cr);
        cairo_restore(s.panel_cr);
      }
      py += LOGO_SIZE + 18;
      draw_pango(s.panel_cr, lay_name, "@xeland314", 0, MARGIN, py, 1, 1, 1, 1);
      py += 40;
      draw_pango(s.panel_cr, lay_role, "Backend Developer", 0, MARGIN, py, 0.75,
                 0.85, 1.0, 1);
      py += 32;
      draw_sep(s.panel_cr, py);
      py += 24;
      cairo_set_source_rgba(s.panel_cr, 1, 1, 1, 0.07);
      cairo_rectangle(s.panel_cr, MARGIN, py - 8, PANEL_WIDTH - MARGIN * 2,
                      120);
      cairo_fill(s.panel_cr);
      draw_pango(s.panel_cr, lay_section, "PROYECTO ACTUAL", 0, MARGIN + 6, py,
                 0.98, 0.82, 0.25, 1);
      py += 32;
      draw_pango(s.panel_cr, lay_item, "› Side-Panel en C + Xlib + Cairo", 0,
                 MARGIN + 6, py, 0.88, 0.92, 1.0, 0);
      py += 20;
      draw_pango(s.panel_cr, lay_item, "› Ticker bar con Pango markup", 0,
                 MARGIN + 6, py, 0.88, 0.92, 1.0, 0);
      py += 20;
      draw_pango(s.panel_cr, lay_item, "› Deploy: compile & run 🚀", 0,
                 MARGIN + 6, py, 0.88, 0.92, 1.0, 0);
      py = s.screen_h - 144;
      float cpu = get_cpu_usage(), ram = get_ram_usage();
      char mb[64];
      snprintf(mb, sizeof(mb), "CPU  %.1f%%", cpu * 100);
      draw_pango(s.panel_cr, lay_metric, mb, 0, MARGIN, py, 1, 1, 1, 1);
      py += 20;
      draw_bar(s.panel_cr, MARGIN, py, PANEL_WIDTH - MARGIN * 2, 10, cpu, 0.05,
               0.12, 0.35, 0.30, 0.70, 1.00);
      py += 20;
      snprintf(mb, sizeof(mb), "RAM  %.1f%%", ram * 100);
      draw_pango(s.panel_cr, lay_metric, mb, 0, MARGIN, py, 1, 1, 1, 1);
      py += 20;
      draw_bar(s.panel_cr, MARGIN, py, PANEL_WIDTH - MARGIN * 2, 10, ram, 0.05,
               0.12, 0.35, 0.30, 0.80, 0.30);
      draw_sep(s.panel_cr, py + 24);
      double yb = s.screen_h - 50.0;
      draw_pango(s.panel_cr, lay_link, "github.com/xeland314", 0, MARGIN, yb,
                 0.7, 0.8, 1.0, 1);
      yb += 20;
      draw_pango(s.panel_cr, lay_link, "xeland314.github.io", 0, MARGIN, yb,
                 0.7, 0.8, 1.0, 1);
    }
    XFlush(s.disp);
    tick++;
    usleep(30000);
  }
  return 0;
}
