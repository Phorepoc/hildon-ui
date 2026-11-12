/* Copyright The Phorepoc Authors */
/* SPDX-License-Identifier: BSD-2-Clause */
/* phe_picker_center.h -- included (-include) into hildon-picker-dialog.c only. The dialog centres its selector on the selected rows in
   its show handler; there the columns have no size yet (the pannable areas' adjustments are empty), hildon_pannable_area_scroll_to
   returns early, and the date picker opened at 1970-01-01. On Maemo the window manager's configure round trip comes first. The centring
   is done again once every column's pannable area has a scroll range (checked every 50 ms, for 1 s at most) (hildon-port.md 7.114). */
#ifndef PHE_PICKER_CENTER_H
#define PHE_PICKER_CENTER_H
#include <gtk/gtk.h>
struct _HildonTouchSelector;
void hildon_touch_selector_center_on_selected (struct _HildonTouchSelector *selector);
typedef struct { GtkWidget *sel; gint tries; } PhePickerCenter;
/* a column's pannable area is the scrolled parent of its tree view: its vertical adjustment has a range once the rows are laid out */
static void
phe_picker_ranges (GtkWidget *w, gpointer data)
{
  gint *state = data;
  if (GTK_IS_TREE_VIEW (w)) {
    GtkAdjustment *v = gtk_tree_view_get_vadjustment (GTK_TREE_VIEW (w));
    state[0]++;
    if (v && v->upper > v->page_size && v->page_size > 0) state[1]++;
    if (v) state[2] = (gint) v->upper, state[3] = (gint) v->page_size;
  } else if (GTK_IS_CONTAINER (w)) gtk_container_forall (GTK_CONTAINER (w), phe_picker_ranges, data);
}
static gboolean
phe_picker_center_tick (gpointer data)
{
  PhePickerCenter *c = data; gint st[4] = { 0, 0, 0, 0 };
  phe_picker_ranges (c->sel, st);
  if (st[0] > 0 && st[1] < st[0] && ++c->tries < 20) return TRUE;
  g_message ("[phehildon] picker centred after %d tick(s): %d of %d columns laid out (last upper %d, page %d)", c->tries, st[1], st[0], st[2], st[3]);
  hildon_touch_selector_center_on_selected ((struct _HildonTouchSelector *) c->sel);
  g_object_unref (c->sel); g_free (c);
  return FALSE;
}
void
phe_picker_center_later (struct _HildonTouchSelector *s)
{
  PhePickerCenter *c = g_new0 (PhePickerCenter, 1);
  hildon_touch_selector_center_on_selected (s);
  c->sel = g_object_ref (s);
  g_timeout_add (50, phe_picker_center_tick, c);
}
#define hildon_touch_selector_center_on_selected phe_picker_center_later
#endif
