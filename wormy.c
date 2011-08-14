#include <X11/Xcursor/Xcursor.h>
#include <X11/extensions/Xfixes.h>
#include <X11/cursorfont.h>
#include <time.h>
#include <cairo/cairo.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>

struct {
  Display *disp;
  XcursorImage *xci;
  Cursor cursor, original;
  XcursorPixel *cache[360];
  /* image specs */
  float width, height, radius, angle;
  /* rotation center */
  float piv_x, piv_y, tail_d;
  /* hotspot location (polar coords) */
  float hot_a, hot_d;
} wormy;

void cleanup () {
  if (wormy.cursor) {
    XFreeCursor (wormy.disp, wormy.cursor);
    wormy.cursor = 0;
  }
  if (wormy.xci) {
    XcursorImageDestroy(wormy.xci);
    wormy.xci = NULL;
  }
  int a;
  for (a = 0; a < 360; a++)
    if (wormy.cache[a]) {
      free(wormy.cache[a]);
      wormy.cache[a] = NULL;
    }
  memset(&wormy, 0, sizeof(wormy));
}

int buildCache(const char *path) {
  cairo_surface_t *img = cairo_image_surface_create_from_png (path);
  if (CAIRO_STATUS_SUCCESS != cairo_surface_status (img)) {
    fprintf(stderr, "Failed to load cusor image [%s]\n", path);
    return 1;
  }

  /*calculate dimensions needed for rotate image*/
  wormy.width = cairo_image_surface_get_width(img);
  wormy.height = cairo_image_surface_get_height(img);
  wormy.radius = sqrt(wormy.width*wormy.width/4.0 +
                      wormy.height*wormy.height/4.0);
  /*create a destination canvas*/
  cairo_surface_t *work = cairo_image_surface_create(
                          CAIRO_FORMAT_ARGB32, 2*wormy.radius, 2*wormy.radius);
  cairo_t *cr = cairo_create(work);

  int a;
  for (a = 0; a < 360; a++) {
    /*clear background*/
    cairo_identity_matrix (cr);
    cairo_set_operator (cr, CAIRO_OPERATOR_CLEAR);
    cairo_paint(cr);

    /*move to the center, rotate, translate back an to the final place*/
    cairo_translate(cr, wormy.radius, wormy.radius);
    cairo_rotate(cr, a * M_PI / 180.0);
    cairo_translate(cr, -wormy.width/2.0, -wormy.height/2.0);

    /*paint rotate image*/
    cairo_set_operator(cr, CAIRO_OPERATOR_OVER);
    cairo_set_source_surface(cr, img, 0, 0);
    cairo_paint(cr);

    if (wormy.cache[a]) free(wormy.cache[a]);
    /*cache the image*/
    int stride = cairo_image_surface_get_stride(work);
    void *data = cairo_image_surface_get_data(work);
    wormy.cache[a] = (XcursorPixel *)malloc((int)(
                      wormy.radius * wormy.radius * stride));
    memcpy(wormy.cache[a], data, wormy.radius * wormy.radius * stride);
  }

  cairo_destroy(cr);
  cairo_surface_destroy(img);
  cairo_surface_destroy(work);

  return 0;
}


int loadCursor(char *path, int hotx, int hoty, float tail_dist, float angle) {
  if (buildCache(path) != 0) return 1;

  // create a cursor image and set it
  wormy.xci = XcursorImageCreate (2*wormy.radius, 2*wormy.radius);
  wormy.xci->xhot = (2.0*wormy.radius-wormy.width)/2 + hotx;
  wormy.xci->yhot = (2.0*wormy.radius-wormy.height)/2 + hoty;
  wormy.xci->pixels = wormy.cache[0];

  wormy.original = XCreateFontCursor(wormy.disp, XC_left_ptr);
  XDefineCursor(wormy.disp, DefaultRootWindow(wormy.disp), wormy.original);
  wormy.cursor = XcursorImageLoadCursor(wormy.disp, wormy.xci);
  XFixesChangeCursor(wormy.disp, wormy.cursor, wormy.original);

  // using cairo coordinates (inverted y axis)
  wormy.hot_a = atan2f(wormy.xci->yhot - wormy.radius,
                       wormy.xci->xhot - wormy.radius);
  wormy.hot_d = sqrt((wormy.xci->xhot - wormy.radius) *
                     (wormy.xci->xhot - wormy.radius) +
                     (wormy.xci->yhot - wormy.radius) *
                     (wormy.xci->yhot - wormy.radius));
  wormy.tail_d = tail_dist;
  wormy.angle = angle * M_PI / 180.0;

  return 0;
}


void mouseMove(int posx, int posy) {
  /*get the new angle we're traveling*/
  /*needent invert y axis since cairo uses same coord system*/
  float a = atan2f(posy - wormy.piv_y, posx - wormy.piv_x);
  /*update pivoting position*/
  wormy.piv_x = posx - wormy.tail_d * cosf(a);
  wormy.piv_y = posy - wormy.tail_d * sinf(a);

  /*compensate image orientation*/
  a += wormy.angle;

  // create a cursor image and set it
  wormy.xci->xhot = wormy.radius + wormy.hot_d * cosf(wormy.hot_a + a);
  wormy.xci->yhot = wormy.radius + wormy.hot_d * sinf(wormy.hot_a + a);
  wormy.xci->pixels = wormy.cache[ (int)(360 + a * 180.0 / M_PI) % 360 ];

  Cursor newcur = XcursorImageLoadCursor(wormy.disp, wormy.xci);
  XFixesChangeCursor(wormy.disp, newcur, wormy.cursor);
  XFreeCursor(wormy.disp, wormy.cursor);
  wormy.cursor = newcur;
}


int main(int argc, char *argv[]){
  wormy.disp = XOpenDisplay(NULL);
  if (loadCursor("mouse.png", 3, 5, 45.0, 90.0) != 0) return 1;

  struct timespec tm = { 0, 1e+7 };
  int x, y, cx, cy;
  unsigned int mask;
  Window root, child;

  while(nanosleep(&tm, NULL) == 0){
    XQueryPointer(wormy.disp, DefaultRootWindow(wormy.disp),
                  &root, &child, &x, &y, &cx, &cy, &mask);
    /*fprintf(stderr, "x: %d, y: %d\n", x, y);*/
    mouseMove(x, y);
    XFlush(wormy.disp);
  }

  cleanup();
  return 0;
}
