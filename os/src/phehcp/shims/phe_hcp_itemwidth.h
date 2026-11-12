/* Copyright The Phorepoc Authors */
/* SPDX-License-Identifier: BSD-2-Clause */
/* phe_hcp_itemwidth.h -- included (-include) into hcp-app-view.c: its item width is the N900's (480 - 3 margins = 432 px in
   portrait), wider than a 360 px screen, so the grid overflowed and the pannable area scrolled it sideways on a tap.
   phe_hcp_set_item_width (phehcp_osso.c) keeps the width inside the screen. */
#ifndef PHE_HCP_ITEMWIDTH_H
#define PHE_HCP_ITEMWIDTH_H
struct _GtkIconView;
void phe_hcp_set_item_width (struct _GtkIconView *view, int width);
#define gtk_icon_view_set_item_width phe_hcp_set_item_width
#endif
