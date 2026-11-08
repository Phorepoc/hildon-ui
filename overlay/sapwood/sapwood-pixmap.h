/* GTK+ Sapwood Engine -- in-process image tiles for Phorepoc (replaces sapwood-pixmap.h of the upstream engine, which gets X pixmaps from the separate sapwood-server).
 * Copyright (C) 2005 Nokia Corporation (the engine); this file Copyright The Phorepoc Authors. LGPL-2.1-or-later (as the engine).
 * An image is cut at its borders into 3x3 tiles (GdkPixbufs with their own 8-bit alpha); rendering tiles them with cairo. */
#ifndef SAPWOOD_PIXMAP_H
#define SAPWOOD_PIXMAP_H 1

#include <gtk/gtk.h>

G_BEGIN_DECLS

typedef struct _SapwoodPixmap SapwoodPixmap;

/* the fields keep the upstream names (pixmap = the tile, pixmask unused) so theme-pixbuf.c needs no change for them */
typedef struct {
    gpointer pixmap;
    gpointer pixmask;
    GdkRectangle dest;
} SapwoodRect;

SapwoodPixmap *sapwood_pixmap_get_for_file (const char *filename, int border_left, int border_right, int border_top, int border_bottom, GError **err) G_GNUC_INTERNAL;
void      sapwood_pixmap_free         (SapwoodPixmap *self) G_GNUC_INTERNAL;
gboolean  sapwood_pixmap_get_geometry (SapwoodPixmap *self, gint *width, gint *height) G_GNUC_INTERNAL;
void      sapwood_pixmap_get_pixmap   (SapwoodPixmap *self, gint x, gint y, gpointer *ret_pixmap, gpointer *ret_pixmask) G_GNUC_INTERNAL;
void      sapwood_pixmap_render_rects (SapwoodPixmap *self, GtkWidget *widget, GdkDrawable *draw, gint draw_x, gint draw_y, gint width, gint height,
                                       GdkBitmap *mask, gint mask_x, gint mask_y, gboolean mask_required, GdkRectangle *clip_rect, gint n_rects, SapwoodRect *rects) G_GNUC_INTERNAL;

G_GNUC_INTERNAL extern gboolean sapwood_debug_scaling;
G_GNUC_INTERNAL extern gboolean sapwood_debug_xtraps;

G_END_DECLS

#endif
