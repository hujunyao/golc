#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include "labar-types.h"

#define DEBUG(...) fprintf(stdout, "[labar.main] "__VA_ARGS__)

enum {
  LA_BAR_CFG_DOCK          = (1 << 0),
  LA_BAR_CFG_SKIP          = (1 << 1),
  LA_BAR_CFG_STICK         = (1 << 2),
  LA_BAR_CFG_BOTTOM_LAYER  = (1 << 3),
  LA_BAR_CFG_NO_DECORATION = (1 << 4)
};

typedef struct {
  gint x, y, w, h;
} la_bar_rect_t;

typedef struct {
  Window window;
  gint x, y;
} la_bar_render_context_t;

typedef struct {
  Display *display;
  Window window;
  gint screen_width, screen_height;
  long mask;
} la_bar_t;

static gboolean g_is_quit = FALSE;

#define LA_BAR_EVENT_MASK \
  (PointerMotionMask | ExposureMask | ButtonPressMask| \
  ButtonReleaseMask | LeaveWindowMask | EnterWindowMask)

void la_bar_main_quit() {
  g_is_quit = TRUE;
}

gboolean la_bar_get_event(la_bar_t *bar, XEvent *ev) {
  if(XQLength(bar->display)) {
    if(!XCheckWindowEvent(bar->display, bar->window, bar->mask, ev)) {
      XSync(bar->display, True);
      return FALSE;
    }
  } else {
    XWindowEvent(bar->display, bar->window, bar->mask,ev);
  }

  return TRUE;
}

gboolean la_bar_init(la_bar_t *bar) {
  #define LA_BAR_X 1
  #define LA_BAR_Y 1
  #define LA_BAR_W 1
  #define LA_BAR_H 1
  gint scrn;

  bar->display = XOpenDisplay(NULL);
  scrn = DefaultScreen(bar->display);
  gdk_pixbuf_xlib_init(bar->display, scrn);
  bar->window = XCreateSimpleWindow(bar->display, DefaultRootWindow(bar->display),
                LA_BAR_X, LA_BAR_Y, LA_BAR_W, LA_BAR_H, 0, 0, 0);
  if(bar->window) {
    XClassHint ch = {"labar.name", "labar.class"};

    //bar->screen_width = WidthOfScreen(scrn);
    //bar->screen_height = HeightOfScreen(scrn);
    XSetClassHint(bar->display, bar->window, &ch);
    XSelectInput(bar->display, bar->window, LA_BAR_EVENT_MASK);

    return TRUE;
  }

  return FALSE;
}

void _render_icon_cb(gpointer data, gpointer userdata) {
  la_item_t *app = (la_item_t *)data; gint w, h;
  la_bar_render_context_t *cxt = (la_bar_render_context_t *)userdata;

  w = gdk_pixbuf_get_width(app->nr_icon);
  h = gdk_pixbuf_get_height(app->nr_icon);

  DEBUG("x = %d, y = %d, w = %d, h =%d\n", cxt->x, cxt->y, w, h);
  gdk_pixbuf_xlib_render_to_drawable_alpha(app->nr_icon, cxt->window,
    0, 0, cxt->x, cxt->y, w, h, 0, 0.5, 0, 0);
  cxt->x += w;
}

void _calc_size_cb(gpointer data, gpointer userdata) {
  la_item_t *app = (la_item_t *)data;
  la_bar_rect_t *pr = (la_bar_rect_t *)userdata;
  gint height;

  pr->w += gdk_pixbuf_get_width(app->nr_icon);
  height = gdk_pixbuf_get_height(app->nr_icon);
  pr->h = pr->h > height ? pr->h : height;
}

void la_bar_resize(la_bar_t *bar, la_conf_t *c) {
  la_bar_rect_t rt = {0};

  g_slist_foreach(c->apps, _calc_size_cb, &rt);
  DEBUG("Total size = %d, %d, resize window\n", rt.w , rt.h);
  XMoveResizeWindow(bar->display, bar->window, rt.x, rt.y, rt.w, rt.h);
}

void la_bar_startup(la_bar_t *bar, la_conf_t *c) {
  la_bar_render_context_t cxt;
  cxt.x = 0, cxt.y = 0, cxt.window = bar->window;

  la_bar_resize(bar, c);
  XMapWindow(bar->display, bar->window);
  g_slist_foreach(c->apps, _render_icon_cb, &cxt);
  XSync(bar->display, True);
}

void la_bar_draw_icons(la_bar_t *bar, GSList *apps) {
  la_bar_render_context_t cxt;
  cxt.x = 0, cxt.y = 0, cxt.window = bar->window;
  g_slist_foreach(apps, _render_icon_cb, &cxt);
}

void la_bar_config(la_bar_t *bar, gint cfgmask) {
  Atom am = None;

  if(cfgmask & LA_BAR_CFG_NO_DECORATION) {
    long prop[5] = {2, 0, 0, 0, 0};
    am = XInternAtom(bar->display, "_MOTIF_WM_HINTS", True);
    if(am != None)
      XChangeProperty(bar->display, bar->window, am, am, 32, PropModeReplace, (unsigned char *) prop, 5);
  }

  if(cfgmask & LA_BAR_CFG_STICK) {
    long prop = 0xFFFFFFFF;

    am = XInternAtom(bar->display, "_NET_WM_DESKTOP", True);
    if(am != None)
      XChangeProperty(bar->display, bar->window, am, XA_CARDINAL, 32, PropModeAppend, (unsigned char *) &prop, 1);
    am = XInternAtom(bar->display, "_NET_WM_STATE", True);
    if(am != None) {
      Atom _prop = XInternAtom(bar->display, "_NET_WM_STATE_STICKY", True);
      XChangeProperty(bar->display, bar->window, am, XA_ATOM, 32, PropModeAppend, (unsigned char *) &_prop, 1);
    }
  }

  if(cfgmask & LA_BAR_CFG_SKIP) {
    am = XInternAtom(bar->display, "_NET_WM_STATE", True);
    if(am != None) {
      Atom _prop = XInternAtom(bar->display, "_NET_WM_STATE_SKIP_TASKBAR", True);
      XChangeProperty(bar->display, bar->window, am, XA_ATOM, 32, PropModeAppend, (unsigned char *) &_prop, 1);
    }
    am = XInternAtom(bar->display, "_NET_WM_STATE", True);
    if(am != None) {
      Atom _prop = XInternAtom(bar->display, "_NET_WM_STATE_SKIP_PAGER", True);
      XChangeProperty(bar->display, bar->window, am, XA_ATOM, 32, PropModeAppend, (unsigned char *) &_prop, 1);
    }
  }

  if(cfgmask & LA_BAR_CFG_DOCK) {
    am = XInternAtom(bar->display, "_NET_WM_WINDOW_TYPE", True);
    if(am != None) {
      Atom _prop = XInternAtom(bar->display, "_NET_WM_WINDOW_TYPE_DOCK", True);
      XChangeProperty(bar->display, bar->window, am, XA_ATOM, 32, PropModeAppend, (unsigned char *) &_prop, 1);
    }
  }

  if(cfgmask & LA_BAR_CFG_BOTTOM_LAYER) {
    am = XInternAtom(bar->display, "_WIN_LAYER", True);
    if(am != None) {
      long _prop = 0;
      XChangeProperty(bar->display, bar->window, am, XA_CARDINAL, 32, PropModeAppend, (unsigned char *) &_prop, 1);
    }

    am = XInternAtom(bar->display, "_NET_WM_STATE", True);
    if(am != None) {
      Atom _prop = XInternAtom(bar->display, "_NET_WM_STATE_BELOW", True);
      XChangeProperty(bar->display, bar->window, am, XA_ATOM, 32, PropModeAppend, (unsigned char *) &_prop, 1);
    }
  }
}

void la_bar_free(la_bar_t *bar) {
  if(bar->window) {
    XDestroyWindow(bar->display, bar->window);
  }
  if(bar->display) {
    XCloseDisplay(bar->display);
  }
}

int main(int argc, char *argv[]) {
  la_conf_t c;
  la_bar_t bar;
  XEvent ev;
 
  g_type_init();
  la_conf_init(&c, "labar.conf");
  if(la_bar_init(&bar)) {
    gint cfg = /*LA_BAR_CFG_BOTTOM_LAYER |*/ LA_BAR_CFG_DOCK | LA_BAR_CFG_SKIP | LA_BAR_CFG_STICK | LA_BAR_CFG_NO_DECORATION;
    la_bar_config(&bar, cfg);
    la_bar_startup(&bar, &c);
  } else {
    g_is_quit = TRUE;
  }

  while(g_is_quit != TRUE) {
    if(la_bar_get_event(&bar, &ev)) {
      switch(ev.type) {
        case Expose:
        DEBUG("Expose Event\n");
        la_bar_draw_icons(&bar, c.apps);
        break;

        case ButtonPress:
        break;

        case ButtonRelease:
        break;

        case MotionNotify:
        break;

        case LeaveNotify:
        break;

        case EnterNotify:
        break;
      }
    }
  }

  la_bar_free(&bar);
  la_conf_free(&c);

  return 0;
}
