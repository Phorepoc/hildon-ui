/* Copyright The Phorepoc Authors */
/* SPDX-License-Identifier: BSD-2-Clause */
/* phehomeclock.c -- OURS: a clock home widget for the Hildon Home (phehome): an HDHomePluginItem of libhildondesktop, loaded by hildon-home's applet manager like
 * every Maemo home widget. It shows the home time in the system's clock format (phehomeclock_sym.cpp: TTime, TLocale) on a translucent card, updated at each minute. */
#include <gtk/gtk.h>
#include <hildon/hildon.h>
#include <libhildondesktop/libhildondesktop.h>

/* phehomeclock_sym.cpp */
extern void phe_clk_now (int *hour, int *minute, int *h24, int *wday, int *day, int *month, int *year);

#define PHE_TYPE_HOME_CLOCK (phe_home_clock_get_type ())
typedef struct { HDHomePluginItem parent; } PheHomeClock;
typedef struct { HDHomePluginItemClass parent; } PheHomeClockClass;
typedef struct { GtkWidget *time, *date; guint timer; } PheHomeClockPrivate;
GType phe_home_clock_get_type (void);

HD_DEFINE_PLUGIN_MODULE_WITH_PRIVATE (PheHomeClock, phe_home_clock, HD_TYPE_HOME_PLUGIN_ITEM);

#define PRIV(o) ((PheHomeClockPrivate *) phe_home_clock_get_instance_private ((PheHomeClock *) (o)))

static const char *wdays[] = { "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday", "Sunday" };
static const char *months[] = { "January", "February", "March", "April", "May", "June", "July", "August", "September", "October", "November", "December" };

static gboolean
tick (gpointer data)
{
  PheHomeClockPrivate *p = PRIV (data);
  int h, m, h24, wd, d, mo, y, s;
  gchar *t;
  phe_clk_now (&h, &m, &h24, &wd, &d, &mo, &y);
  if (h24) t = g_strdup_printf ("<span size=\"xx-large\" weight=\"bold\">%02d:%02d</span>", h, m);
  else t = g_strdup_printf ("<span size=\"xx-large\" weight=\"bold\">%d:%02d</span> %s", h % 12 ? h % 12 : 12, m, h < 12 ? "am" : "pm");
  gtk_label_set_markup (GTK_LABEL (p->time), t); g_free (t);
  t = g_strdup_printf ("%s %d %s", wdays[wd % 7], d, months[mo % 12]);
  gtk_label_set_text (GTK_LABEL (p->date), t); g_free (t);
  (void) y;
  /* the next tick at the next minute */
  if (p->timer) g_source_remove (p->timer);
  s = 60 - (int) (g_get_real_time () / G_USEC_PER_SEC % 60);
  p->timer = g_timeout_add_seconds (s > 0 ? s : 60, tick, data);
  return FALSE;
}

static void
phe_home_clock_realize (GtkWidget *widget)
{
  gtk_widget_set_colormap (widget, gdk_screen_get_rgba_colormap (gtk_widget_get_screen (widget)));
  gtk_widget_set_app_paintable (widget, TRUE);
  GTK_WIDGET_CLASS (phe_home_clock_parent_class)->realize (widget);
}

static gboolean
phe_home_clock_expose_event (GtkWidget *widget, GdkEventExpose *event)
{
  cairo_t *cr = gdk_cairo_create (GDK_DRAWABLE (widget->window));
  double w = widget->allocation.width, h = widget->allocation.height, r = 14;
  gdk_cairo_region (cr, event->region); cairo_clip (cr);
  cairo_set_operator (cr, CAIRO_OPERATOR_SOURCE);
  cairo_set_source_rgba (cr, 0, 0, 0, 0); cairo_paint (cr);
  cairo_new_sub_path (cr);
  cairo_arc (cr, w - r, r, r, -G_PI / 2, 0); cairo_arc (cr, w - r, h - r, r, 0, G_PI / 2);
  cairo_arc (cr, r, h - r, r, G_PI / 2, G_PI); cairo_arc (cr, r, r, r, G_PI, 3 * G_PI / 2);
  cairo_close_path (cr);
  cairo_set_source_rgba (cr, 0, 0, 0, 0.55); cairo_fill (cr);
  cairo_destroy (cr);
  return GTK_WIDGET_CLASS (phe_home_clock_parent_class)->expose_event (widget, event);
}

static void
phe_home_clock_dispose (GObject *o)
{
  PheHomeClockPrivate *p = PRIV (o);
  if (p->timer) { g_source_remove (p->timer); p->timer = 0; }
  G_OBJECT_CLASS (phe_home_clock_parent_class)->dispose (o);
}

static void
phe_home_clock_init (PheHomeClock *c)
{
  PheHomeClockPrivate *p = PRIV (c);
  GtkWidget *box = gtk_vbox_new (FALSE, 2);
  GdkColor white = { 0, 0xffff, 0xffff, 0xffff };
  p->time = gtk_label_new (NULL); p->date = gtk_label_new (NULL);
  gtk_widget_modify_fg (p->time, GTK_STATE_NORMAL, &white); gtk_widget_modify_fg (p->date, GTK_STATE_NORMAL, &white);
  gtk_box_pack_start (GTK_BOX (box), p->time, FALSE, FALSE, 0);
  gtk_box_pack_start (GTK_BOX (box), p->date, FALSE, FALSE, 0);
  gtk_container_set_border_width (GTK_CONTAINER (box), 12);
  gtk_container_add (GTK_CONTAINER (c), box);
  gtk_widget_set_size_request (GTK_WIDGET (c), 300, 110);
  gtk_widget_show_all (box);
  tick (c);
}

static void
phe_home_clock_class_init (PheHomeClockClass *k)
{
  GTK_WIDGET_CLASS (k)->realize = phe_home_clock_realize;
  GTK_WIDGET_CLASS (k)->expose_event = phe_home_clock_expose_event;
  G_OBJECT_CLASS (k)->dispose = phe_home_clock_dispose;
}

static void
phe_home_clock_class_finalize (PheHomeClockClass *k)
{
  (void) k;
}
