/* Copyright The Phorepoc Authors */
/* SPDX-License-Identifier: BSD-2-Clause */
/* phe_hcp_show.h -- included (-include) into the control panel's sources: its gtk_widget_show_all goes through phe_hcp_show_all (phehcp_osso.c), which sizes a stackable window to the screen. */
#ifndef PHE_HCP_SHOW_H
#define PHE_HCP_SHOW_H
#define gtk_widget_show_all phe_hcp_show_all
#endif
