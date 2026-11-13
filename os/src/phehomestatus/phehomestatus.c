/* Copyright The Phorepoc Authors */
/* SPDX-License-Identifier: BSD-2-Clause */
/* phehomestatus.c -- OURS: a device status home widget for the Hildon Home (phehome): free memory and free storage on C:, each as a
 * number and a bar, refreshed every 30 s. An HDHomePluginItem of libhildondesktop like every Maemo home widget; the values come from HAL and
 * the file server (phehomestatus_sym.cpp). Drawn in code (cairo + pango). */
#include <gtk/gtk.h>
#include <hildon/hildon.h>
#include <libhildondesktop/libhildondesktop.h>

/* phehomestatus_sym.cpp: MB, -1 = not known */
extern void phe_st_now (int *ram_total, int *ram_free, int *disk_total, int *disk_free);

#define PHE_TYPE_HOME_STATUS (phe_home_status_get_type ())
typedef struct { HDHomePluginItem parent; } PheHomeStatus;
typedef struct { HDHomePluginItemClass parent; } PheHomeStatusClass;
typedef struct { int rt, rf, dt, df; guint timer; } PheHomeStatusPrivate;
GType phe_home_status_get_type (void);

HD_DEFINE_PLUGIN_MODULE_WITH_PRIVATE (PheHomeStatus, phe_home_status, HD_TYPE_HOME_PLUGIN_ITEM);

#define PRIV(o) ((PheHomeStatusPrivate *) phe_home_status_get_instance_private ((PheHomeStatus *) (o)))
#define W 230
#define H 152

/* every 30 s: redraw when a value has changed */
static gboolean
tick (gpointer data)
{
  PheHomeStatusPrivate *p = PRIV (data);
  int rt, rf, dt, df;
  phe_st_now (&rt, &rf, &dt, &df);
  if (rt != p->rt || rf != p->rf || dt != p->dt || df != p->df) {
    p->rt = rt; p->rf = rf; p->dt = dt; p->df = df;
    gtk_widget_queue_draw (GTK_WIDGET (data));
  }
  return TRUE;
}

static void
phe_home_status_realize (GtkWidget *widget)
{
  gtk_widget_set_colormap (widget, gdk_screen_get_rgba_colormap (gtk_widget_get_screen (widget)));
  gtk_widget_set_app_paintable (widget, TRUE);
  GTK_WIDGET_CLASS (phe_home_status_parent_class)->realize (widget);
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
text (cairo_t *cr, GtkWidget *w, const char *markup, double x, double y)
{
  PangoLayout *l = gtk_widget_create_pango_layout (w, NULL);
  int tw, th;
  pango_layout_set_markup (l, markup, -1);
  pango_layout_get_pixel_size (l, &tw, &th);
  cairo_move_to (cr, x, y);
  pango_cairo_show_layout (cr, l);
  g_object_unref (l);
}

static void
bar (cairo_t *cr, GtkWidget *w, const char *what, int free_mb, int total_mb, double y)
{
  double bw = W - 28, used = total_mb > 0 ? 1.0 - (double) free_mb / total_mb : 0;
  gchar *s = total_mb > 0 ? g_strdup_printf ("<span size=\"small\">%d MB free of %d MB</span>", free_mb, total_mb)
                          : g_strdup ("<span size=\"small\">not known</span>");
  gchar *t = g_strdup_printf ("<span weight=\"bold\">%s</span>", what);
  cairo_set_source_rgb (cr, 1, 1, 1);
  text (cr, w, t, 14, y); g_free (t);
  cairo_set_source_rgba (cr, 1, 1, 1, 0.8);
  text (cr, w, s, 14, y + 24); g_free (s);
  rounded (cr, 14, y + 50, bw, 10, 5); cairo_set_source_rgba (cr, 1, 1, 1, 0.25); cairo_fill (cr);
  if (used > 0.02) {
    rounded (cr, 14, y + 50, bw * MIN (used, 1.0), 10, 5);
    if (used > 0.9) cairo_set_source_rgba (cr, 0.93, 0.33, 0.23, 0.95); else cairo_set_source_rgba (cr, 0.16, 0.45, 0.93, 0.95);
    cairo_fill (cr);
  }
}

static gboolean
phe_home_status_expose_event (GtkWidget *widget, GdkEventExpose *event)
{
  PheHomeStatusPrivate *p = PRIV (widget);
  cairo_t *cr = gdk_cairo_create (GDK_DRAWABLE (widget->window));
  double w = widget->allocation.width, h = widget->allocation.height, r = 14;
  gdk_cairo_region (cr, event->region); cairo_clip (cr);
  cairo_set_operator (cr, CAIRO_OPERATOR_SOURCE);
  cairo_set_source_rgba (cr, 0, 0, 0, 0); cairo_paint (cr);
  cairo_set_operator (cr, CAIRO_OPERATOR_OVER);
  /* a translucent dark card like the clock's and the date's; per value a label, "free of total", and a bar of the used part in the accent blue */
  rounded (cr, 0, 0, w, h, r);
  cairo_set_source_rgba (cr, 0, 0, 0, 0.55); cairo_fill (cr);
  bar (cr, widget, "Memory", p->rf, p->rt, 12);
  bar (cr, widget, "Storage", p->df, p->dt, 80);
  cairo_destroy (cr);
  return FALSE;
}

static void
phe_home_status_dispose (GObject *o)
{
  PheHomeStatusPrivate *p = PRIV (o);
  if (p->timer) { g_source_remove (p->timer); p->timer = 0; }
  G_OBJECT_CLASS (phe_home_status_parent_class)->dispose (o);
}

static void
phe_home_status_init (PheHomeStatus *c)
{
  PheHomeStatusPrivate *p = PRIV (c);
  gtk_widget_set_size_request (GTK_WIDGET (c), W, H);
  p->rt = -2;
  tick (c);
  p->timer = g_timeout_add_seconds (30, tick, c);
}

static void
phe_home_status_class_init (PheHomeStatusClass *k)
{
  GTK_WIDGET_CLASS (k)->realize = phe_home_status_realize;
  GTK_WIDGET_CLASS (k)->expose_event = phe_home_status_expose_event;
  G_OBJECT_CLASS (k)->dispose = phe_home_status_dispose;
}

static void
phe_home_status_class_finalize (PheHomeStatusClass *k)
{
  (void) k;
}
