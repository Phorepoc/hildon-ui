/* Copyright The Phorepoc Authors */
/* SPDX-License-Identifier: BSD-2-Clause */
/* phehomedate.c -- OURS: a date home widget for the Hildon Home (phehome), a calendar sheet: the month on a coloured band, the day of the month large, the
 * weekday below. An HDHomePluginItem of libhildondesktop, loaded by hildon-home's applet manager like every Maemo home widget; the date comes from the
 * home time (phehomedate_sym.cpp: TTime). Drawn entirely in code (cairo + pango), no image files. */
#include <gtk/gtk.h>
#include <hildon/hildon.h>
#include <libhildondesktop/libhildondesktop.h>

/* phehomedate_sym.cpp */
extern void phe_date_now (int *wday, int *day, int *month, int *year);

#define PHE_TYPE_HOME_DATE (phe_home_date_get_type ())
typedef struct { HDHomePluginItem parent; } PheHomeDate;
typedef struct { HDHomePluginItemClass parent; } PheHomeDateClass;
typedef struct { int wday, day, month, year; guint timer; } PheHomeDatePrivate;
GType phe_home_date_get_type (void);

HD_DEFINE_PLUGIN_MODULE_WITH_PRIVATE (PheHomeDate, phe_home_date, HD_TYPE_HOME_PLUGIN_ITEM);

#define PRIV(o) ((PheHomeDatePrivate *) phe_home_date_get_instance_private ((PheHomeDate *) (o)))
#define W 150
#define H 160
#define BAND 40

static const char *wdays[] = { "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday", "Sunday" };
static const char *months[] = { "January", "February", "March", "April", "May", "June", "July", "August", "September", "October", "November", "December" };

/* once a minute: redraw when the date has changed (midnight, or the user set the date) */
static gboolean
tick (gpointer data)
{
  PheHomeDatePrivate *p = PRIV (data);
  int wd, d, mo, y;
  phe_date_now (&wd, &d, &mo, &y);
  if (wd != p->wday || d != p->day || mo != p->month || y != p->year) {
    p->wday = wd; p->day = d; p->month = mo; p->year = y;
    gtk_widget_queue_draw (GTK_WIDGET (data));
  }
  return TRUE;
}

static void
phe_home_date_realize (GtkWidget *widget)
{
  gtk_widget_set_colormap (widget, gdk_screen_get_rgba_colormap (gtk_widget_get_screen (widget)));
  gtk_widget_set_app_paintable (widget, TRUE);
  GTK_WIDGET_CLASS (phe_home_date_parent_class)->realize (widget);
}

static void
rounded (cairo_t *cr, double x, double y, double w, double h, double r)
{
  cairo_new_sub_path (cr);
  cairo_arc (cr, x + w - r, y + r, r, -G_PI / 2, 0); cairo_arc (cr, x + w - r, y + h - r, r, 0, G_PI / 2);
  cairo_arc (cr, x + r, y + h - r, r, G_PI / 2, G_PI); cairo_arc (cr, x + r, y + r, r, G_PI, 3 * G_PI / 2);
  cairo_close_path (cr);
}

static void
text (cairo_t *cr, GtkWidget *w, const char *markup, double cx, double y)
{
  PangoLayout *l = gtk_widget_create_pango_layout (w, NULL);
  int tw, th;
  pango_layout_set_markup (l, markup, -1);
  pango_layout_get_pixel_size (l, &tw, &th);
  cairo_move_to (cr, cx - tw / 2.0, y);
  pango_cairo_show_layout (cr, l);
  g_object_unref (l);
}

static gboolean
phe_home_date_expose_event (GtkWidget *widget, GdkEventExpose *event)
{
  PheHomeDatePrivate *p = PRIV (widget);
  cairo_t *cr = gdk_cairo_create (GDK_DRAWABLE (widget->window));
  double w = widget->allocation.width, h = widget->allocation.height, r = 14;
  gchar *s;
  gdk_cairo_region (cr, event->region); cairo_clip (cr);
  cairo_set_operator (cr, CAIRO_OPERATOR_SOURCE);
  cairo_set_source_rgba (cr, 0, 0, 0, 0); cairo_paint (cr);
  cairo_set_operator (cr, CAIRO_OPERATOR_OVER);
  /* the sheet: a translucent dark card like the clock's, with the month band in the accent blue clipped to its top corners */
  rounded (cr, 0, 0, w, h, r);
  cairo_set_source_rgba (cr, 0, 0, 0, 0.55); cairo_fill_preserve (cr);
  cairo_save (cr); cairo_clip (cr);
  cairo_rectangle (cr, 0, 0, w, BAND); cairo_set_source_rgba (cr, 0.16, 0.45, 0.93, 0.95); cairo_fill (cr);
  cairo_restore (cr);
  cairo_set_source_rgb (cr, 1, 1, 1);
  s = g_strdup_printf ("<span weight=\"bold\">%s</span>", months[p->month % 12]);
  text (cr, widget, s, w / 2, 9); g_free (s);
  s = g_strdup_printf ("<span size=\"xx-large\" weight=\"bold\"><span size=\"x-large\">%d</span></span>", p->day);
  text (cr, widget, s, w / 2, BAND + 8); g_free (s);
  text (cr, widget, wdays[p->wday % 7], w / 2, h - 34);
  cairo_destroy (cr);
  return FALSE;
}

static void
phe_home_date_dispose (GObject *o)
{
  PheHomeDatePrivate *p = PRIV (o);
  if (p->timer) { g_source_remove (p->timer); p->timer = 0; }
  G_OBJECT_CLASS (phe_home_date_parent_class)->dispose (o);
}

static void
phe_home_date_init (PheHomeDate *c)
{
  PheHomeDatePrivate *p = PRIV (c);
  gtk_widget_set_size_request (GTK_WIDGET (c), W, H);
  p->wday = -1;
  tick (c);
  p->timer = g_timeout_add_seconds (60, tick, c);
}

static void
phe_home_date_class_init (PheHomeDateClass *k)
{
  GTK_WIDGET_CLASS (k)->realize = phe_home_date_realize;
  GTK_WIDGET_CLASS (k)->expose_event = phe_home_date_expose_event;
  G_OBJECT_CLASS (k)->dispose = phe_home_date_dispose;
}

static void
phe_home_date_class_finalize (PheHomeDateClass *k)
{
  (void) k;
}
