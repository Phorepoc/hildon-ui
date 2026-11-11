/* Copyright The Phorepoc Authors */
/* SPDX-License-Identifier: BSD-2-Clause */
/* phe_theme.h -- load the Hildon theme (hildon-theme-alpha, the Maemo Leste default, drawn by the sapwood and pixmap engines): BSD-2-Clause (Phorepoc).
 * The theme's gtkrc is read from wherever its package is installed (phe_res), its pixmap_path is rewritten to the drive-less /resource/gtk/themes/alpha/images, and the engines are named by absolute
 * path (engine "sapwood" -> engine "/resource/gtk/engines/sapwood"; GTK uses an absolute name as it is, GModule maps it to sapwood.dll). GTK path lists are split at the colon, so no path of ours can
 * hold a drive letter: the drive of the theme becomes the current drive instead. Returns TRUE when a theme was found and parsed. Needs phe_respath.h and gtk. */
#ifndef PHE_THEME_H
#define PHE_THEME_H
#include <string.h>
#include <unistd.h>
#include <gtk/gtk.h>
#include "phe_respath.h"

static inline gchar *
phe_theme_replace (gchar *text, const char *from, const char *to)
{
  gchar **parts = g_strsplit (text, from, -1), *out = g_strjoinv (to, parts);
  g_strfreev (parts); g_free (text);
  return out;
}

static inline gboolean
phe_theme_load (void (*log) (const char *))
{
  const char *rc = phe_res ("resource/gtk/themes/alpha/gtk-2.0/gtkrc"); gchar *text = NULL, *m;
  char root[4];
  if (rc[0] == 'z' && access (rc, F_OK) != 0) return FALSE;
  if (!g_file_get_contents (rc, &text, NULL, NULL)) return FALSE;
  root[0] = rc[0]; root[1] = 58; root[2] = 47; root[3] = 0;
  chdir (root);
  text = phe_theme_replace (text, "/usr/share/themes/alpha/images", "/resource/gtk/themes/alpha/images");
  text = phe_theme_replace (text, "engine \"sapwood\"", "engine \"/resource/gtk/engines/sapwood\"");
  text = phe_theme_replace (text, "engine \"pixmap\"", "engine \"/resource/gtk/engines/pixmap\"");
  gtk_rc_parse_string (text);
  m = g_strdup_printf ("[theme] alpha parsed from %s (%d bytes)", rc, (int) strlen (text)); if (log) log (m); g_free (m);
  g_free (text);
  return TRUE;
}
#endif
