#include <stdio.h>
#include <time.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/extensions/shape.h>
#include <X11/Xatom.h>
#include <X11/Xft/Xft.h>

#ifdef OSDCAL_DEBUG
#define DEBUG(...) fprintf(stderr, "[III] "__VA_ARGS__)

#define DEBUG_ENTER fprintf(stderr, "enter *%s*\n", __func__)
#define DEBUG_LEVEL fprintf(stderr, "level *%s*\n\n", __func__)
#else
#define DEBUG
#define DEBUG_ENTER
#define DEBUG_LEVEL
#endif

#define BOX_COLOR   "rgb:11/13/14"
#define LUNAR_COLOR "rgb:12/92/32"
#define SOLAR_COLOR "rgb:22/92/12"

#define OSDCAL_WIDTH  320
#define OSDCAL_HEIGHT 320

#define OSDCAL_CENTERX OSDCAL_WIDTH>>1
#define OSDCAL_CENTERY OSDCAL_HEIGHT>>1

#define OSDCAL_STYLE_CENTER 0x00
#define OSDCAL_STYLE_LEFT   0x01
#define OSDCAL_STYLE_RIGHT  0x02

/**reference book about Xlib
 * http://www.tronche.com/gui/x
 */

typedef struct {
  XColor solar_color, lunar_color;
  Visual *vis;
  Display *dsp;
  Pixmap mask_pixmap, back_pixmap;
  unsigned int depth;
  int scr, width, height;
  GC gc, mask_gc, back_gc;
  Window wnd;
} osdcal_t;

osdcal_t *osdcal_create(void) {
  osdcal_t *cal = NULL;
  int evt, err;

  cal = (osdcal_t *)calloc(1, sizeof(osdcal_t));
  cal->dsp = XOpenDisplay(NULL);

  if(!XShapeQueryExtension(cal->dsp, &evt, &err)) {
    DEBUG("X-Server cann't support shape extension.\n");
    XCloseDisplay(cal->dsp);

    return NULL;
  }
  return cal;
}

/* Tell window manager to put window topmost. copy from xosd */
void osdcal_stay_on_top(Display * dpy, Window win)
{
  Atom gnome, net_wm, type;
  int format;
  unsigned long nitems, bytesafter;
  unsigned char *args = NULL;
  Window root = DefaultRootWindow(dpy);

  /*
   * build atoms 
   */
  gnome = XInternAtom(dpy, "_WIN_SUPPORTING_WM_CHECK", False);
  net_wm = XInternAtom(dpy, "_NET_SUPPORTED", False);

  /*
   * gnome-compilant 
   * tested with icewm + WindowMaker 
   */
  if (Success == XGetWindowProperty
      (dpy, root, gnome, 0, (65536 / sizeof(long)), False,
       AnyPropertyType, &type, &format, &nitems, &bytesafter, &args) &&
      nitems > 0) {
    /*
     * FIXME: check capabilities 
     */
    XClientMessageEvent xev;
    Atom gnome_layer = XInternAtom(dpy, "_WIN_LAYER", False);

    memset(&xev, 0, sizeof(xev));
    xev.type = ClientMessage;
    xev.window = win;
    xev.message_type = gnome_layer;
    xev.format = 32;
    xev.data.l[0] = 6 /* WIN_LAYER_ONTOP */ ;

    XSendEvent(dpy, DefaultRootWindow(dpy), False, SubstructureNotifyMask,
               (XEvent *) & xev);
    XFree(args);
  }
  /*
   * netwm compliant.
   * tested with kde 
   */
  else if (Success == XGetWindowProperty
           (dpy, root, net_wm, 0, (65536 / sizeof(long)), False,
            AnyPropertyType, &type, &format, &nitems, &bytesafter, &args)
           && nitems > 0) {
    XEvent e;
    Atom net_wm_state = XInternAtom(dpy, "_NET_WM_STATE", False);
    Atom net_wm_top = XInternAtom(dpy, "_NET_WM_STATE_STAYS_ON_TOP", False);

    memset(&e, 0, sizeof(e));
    e.xclient.type = ClientMessage;
    e.xclient.message_type = net_wm_state;
    e.xclient.display = dpy;
    e.xclient.window = win;
    e.xclient.format = 32;
    e.xclient.data.l[0] = 1 /* _NET_WM_STATE_ADD */ ;
    e.xclient.data.l[1] = net_wm_top;
    e.xclient.data.l[2] = 0l;
    e.xclient.data.l[3] = 0l;
    e.xclient.data.l[4] = 0l;

    XSendEvent(dpy, DefaultRootWindow(dpy), False,
               SubstructureRedirectMask, &e);
    XFree(args);
  }
  XRaiseWindow(dpy, win);
}

int osdcal_get_color(osdcal_t *cal, char *name, XColor *clr) {
  Colormap clrmap;

  clrmap = DefaultColormap(cal->dsp, cal->scr);
  if(XParseColor(cal->dsp, clrmap, name, clr)) {
    if(XAllocColor(cal->dsp, clrmap, clr)) {
      DEBUG("get color successful\n");
      return 0;
    }
  }

  return -1;
}

void osdcal_read_config(osdcal_t *cal) {
  osdcal_get_color(cal, SOLAR_COLOR, &cal->solar_color);
  osdcal_get_color(cal, LUNAR_COLOR, &cal->lunar_color);
}

void osdcal_init(osdcal_t *cal) {
  XSetWindowAttributes attr = {0};
  unsigned long mask = 0L;
  int x, y;

  DEBUG_ENTER;
  if(cal) {
    cal->scr = DefaultScreen(cal->dsp);
    cal->vis = DefaultVisual(cal->dsp, cal->scr);
    cal->depth = DefaultDepth(cal->dsp, cal->scr);

    cal->width = XDisplayWidth(cal->dsp, cal->scr);
    cal->height = XDisplayHeight(cal->dsp, cal->scr);
    DEBUG("screen.width = %d, screen.height = %d, depth = %d\n", cal->width, cal->height, cal->depth);

    mask = CWEventMask|CWOverrideRedirect|CWBorderPixel;
    attr.override_redirect = 1;
    attr.border_pixel = 0;
    attr.event_mask = KeyPressMask | KeyReleaseMask | ButtonPressMask |
                      ButtonReleaseMask| ExposureMask | VisibilityChangeMask |
                      StructureNotifyMask | /* ResizeRedirectMask | */
                      SubstructureNotifyMask | SubstructureRedirectMask;

    x = (cal->width-OSDCAL_WIDTH)>>1;
    y = (cal->height-OSDCAL_HEIGHT)>>1;
    cal->wnd = XCreateWindow(cal->dsp, 
        RootWindow(cal->dsp, cal->scr), x, y, OSDCAL_WIDTH, OSDCAL_HEIGHT, 0,
        cal->depth, InputOutput, cal->vis, mask, &attr);
    if(! cal->wnd) {
      return;
    }
    XStoreName(cal->dsp, cal->wnd, "OSDCAL");

    cal->mask_pixmap = XCreatePixmap(cal->dsp, cal->wnd, OSDCAL_WIDTH, OSDCAL_HEIGHT, 1);
    cal->back_pixmap = XCreatePixmap(cal->dsp, cal->wnd, OSDCAL_WIDTH, OSDCAL_HEIGHT, cal->depth);

    cal->gc = XCreateGC(cal->dsp, cal->wnd, 0, NULL);
    cal->mask_gc = XCreateGC(cal->dsp, cal->mask_pixmap, 0, NULL);
    cal->back_gc = XCreateGC(cal->dsp, cal->mask_pixmap, 0, NULL);

    XSetBackground(cal->dsp, cal->gc, WhitePixel(cal->dsp, cal->scr));

    XSetForeground(cal->dsp, cal->mask_gc, WhitePixel(cal->dsp, cal->scr));
    XSetBackground(cal->dsp, cal->mask_gc, BlackPixel(cal->dsp, cal->scr));

    XSetForeground(cal->dsp, cal->back_gc, BlackPixel(cal->dsp, cal->scr));
    XSetBackground(cal->dsp, cal->back_gc, WhitePixel(cal->dsp, cal->scr));

    osdcal_stay_on_top(cal->dsp, cal->wnd);

    XSelectInput(cal->dsp, cal->wnd, ExposureMask|KeyPressMask|StructureNotifyMask);
    XMapWindow(cal->dsp, cal->wnd);
  }
  DEBUG_LEVEL;
}

void osdcal_destroy(osdcal_t *cal) {
  if(cal) {
    if(cal->mask_gc)
      XFreeGC(cal->dsp, cal->mask_gc);
    if(cal->back_gc)
      XFreeGC(cal->dsp, cal->back_gc);

    if(cal->gc)
      XFreeGC(cal->dsp, cal->gc);
    if(cal->dsp)
      XCloseDisplay(cal->dsp);
  }
}

void osdcal_draw_utf8(osdcal_t *cal, char *utf8, int x, int y, XColor *c) {
  XftFont *font = NULL;
  XftDraw *draw = NULL, *mask_draw = NULL;
  char *xft = "Microsoft YaHei:bold";
  XftColor xftcolor;
  XGlyphInfo info;

  font = XftFontOpenName(cal->dsp, cal->scr, xft);
  if(font == NULL) {
    DEBUG("open font name = %s failure.\n", xft);
  }

  XftTextExtentsUtf8(cal->dsp, font, utf8, strlen(utf8), &info);
  y += (info.height>>1); x -= (info.width>>1);

  draw = XftDrawCreate(cal->dsp, cal->back_pixmap, cal->vis, DefaultColormap(cal->dsp, cal->scr));
  mask_draw = XftDrawCreate(cal->dsp, cal->mask_pixmap, NULL, DefaultColormap(cal->dsp, cal->scr));

  xftcolor.pixel = c->pixel;
  xftcolor.color.red   = c->red;
  xftcolor.color.green = c->green;
  xftcolor.color.blue  = c->blue;
  xftcolor.color.alpha = 0xffff;

  XftDrawStringUtf8(draw, &xftcolor, font, x, y, utf8, strlen(utf8));
  XftDrawStringUtf8(mask_draw, &xftcolor, font, x, y, utf8, strlen(utf8));

  XftDrawDestroy(draw);
  XftDrawDestroy(mask_draw);

  if(font) {
    XftFontClose(cal->dsp, font);
  }
}

void osdcal_draw_string8(osdcal_t *cal, char *str, int x, int y, XColor *c) {
  XftFont *font = NULL;
  XftDraw *draw = NULL, *mask_draw = NULL;
  char *xft = "AR PL SungtiL GB-18:bold";
  XftColor xftcolor;
  XGlyphInfo info;

  font = XftFontOpenName(cal->dsp, cal->scr, xft);
  if(font == NULL) {
    DEBUG("open font name = %s failure.\n", xft);
  }

  XftTextExtents8(cal->dsp, font, str, strlen(str), &info);
  y += (info.height>>1); x -= (info.width>>1);

  draw = XftDrawCreate(cal->dsp, cal->back_pixmap, cal->vis, DefaultColormap(cal->dsp, cal->scr));
  mask_draw = XftDrawCreate(cal->dsp, cal->mask_pixmap, NULL, DefaultColormap(cal->dsp, cal->scr));

  xftcolor.pixel = c->pixel;
  xftcolor.color.red   = c->red;
  xftcolor.color.green = c->green;
  xftcolor.color.blue  = c->blue;
  xftcolor.color.alpha = 0xffff;

  XftDrawString8(draw, &xftcolor, font, x, y, str, strlen(str));
  XftDrawString8(mask_draw, &xftcolor, font, x, y, str, strlen(str));

  XftDrawDestroy(draw);
  XftDrawDestroy(mask_draw);

  if(font) {
    XftFontClose(cal->dsp, font);
  }
}

void osdcal_draw_line(osdcal_t *cal, int width, int x0, int y0, int x1, int y1) {
  XSetLineAttributes(cal->dsp, cal->gc, width, LineSolid, CapButt, JoinBevel);
  XSetLineAttributes(cal->dsp, cal->mask_gc, width, LineSolid, CapButt, JoinBevel);

  XDrawLine(cal->dsp, cal->back_pixmap, cal->gc, x0, y0, x1, y1);
  XDrawLine(cal->dsp, cal->mask_pixmap, cal->mask_gc, x0, y0, x1, y1);
}


void osdcal_draw_box(osdcal_t *cal) {
  XColor c;

  osdcal_get_color(cal, BOX_COLOR, &c);
  XSetForeground(cal->dsp, cal->gc, c.pixel);
  XSetLineAttributes(cal->dsp, cal->gc, 2, LineDoubleDash, CapButt, JoinBevel);
  XSetLineAttributes(cal->dsp, cal->mask_gc, 2, LineDoubleDash, CapButt, JoinBevel);

  XDrawRectangle(cal->dsp, cal->back_pixmap, cal->gc, 0, 0, OSDCAL_WIDTH, OSDCAL_HEIGHT);
  XDrawRectangle(cal->dsp, cal->mask_pixmap, cal->mask_gc, 0, 0, OSDCAL_WIDTH, OSDCAL_HEIGHT);
}

void osdcal_draw_all(osdcal_t *cal) {
  /**draw dash-line box rectangle for debug*/
#ifdef OSDCAL_DEBUG
  osdcal_draw_box(cal);
#endif

  osdcal_draw_utf8(cal, "\xe6\x97\xa5", OSDCAL_CENTERX, 10, &cal->solar_color);
  osdcal_draw_utf8(cal, "\xe6\x98\xa5\xe5\x88\x86", OSDCAL_CENTERX, 40, &cal->solar_color);
  //osdcal_draw_utf8(cal, "\xe6\xb8\x85\xe6\x98\x8e", OSDCAL_CENTERX, 60, &cal->solar_color);
  //osdcal_draw_string8(cal, "Chinese Calendar", OSDCAL_CENTERX, 10, &cal->solar_color);
  //osdcal_draw_line(cal, 6, 0, 10, 430, 10);
}

void osdcal_expose(osdcal_t *cal, XEvent *evt) {
  XExposeEvent *e = (XExposeEvent *)evt;

  DEBUG_ENTER;
  if(cal != NULL) {
    DEBUG("EXPOSE: x = %d, y = %d, w = %d, h = %d\n", e->x, e->y, e->width, e->height);
    XFillRectangle(cal->dsp, cal->mask_pixmap, cal->back_gc, 0, 0, OSDCAL_WIDTH, OSDCAL_HEIGHT);
    osdcal_draw_all(cal);
    XShapeCombineMask(cal->dsp, cal->wnd, ShapeBounding, 0, 0, cal->mask_pixmap, ShapeSet);
    XCopyArea(cal->dsp, cal->back_pixmap, cal->wnd, cal->gc, e->x, e->y, e->width, e->height, e->x, e->y);
  }
  DEBUG_LEVEL;
}

int main(int argc, char *argv[]) {
  osdcal_t *mycal = NULL;
  XEvent evt;
  int quit = 0;

  mycal = osdcal_create();
  osdcal_read_config(mycal);
  osdcal_init(mycal);

  while(!quit) {
    XNextEvent(mycal->dsp, &evt);

    switch(evt.type) {
      case Expose:
        osdcal_expose(mycal, &evt);
        break;

      case KeyPress:
        quit = 1;
        break;
    }
  }

  osdcal_destroy(mycal);

  return 0;
}
