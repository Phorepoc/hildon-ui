/* Copyright The Phorepoc Authors */
/* SPDX-License-Identifier: BSD-2-Clause */
/* dlfcn.h -- OURS: dlopen / dlsym for hildon-control-panel's applet loader (hcp-app.c), over GModule (glib-symbian). The .desktop entry's
 * X-control-panel-plugin "phehcpdatetime.so" is the DLL phehcpdatetime.dll; its names resolve through its symbol table (resource/gmodule/<dll>.sym). */
#ifndef PHE_HCP_DLFCN_H
#define PHE_HCP_DLFCN_H
#include <gmodule.h>
#define RTLD_LAZY 1
#define RTLD_NOW 2
extern void phe_log (const char *msg);
/* each applet load is logged (an applet that does not open: hildon-port.md 7.113) */
static inline void *dlopen (const char *path, int flags)
{
  GModule *m = g_module_open (path, G_MODULE_BIND_LAZY); gchar *t;
  (void) flags;
  t = g_strdup_printf ("[phehcp] applet %s: %s", path, m ? "loaded" : g_module_error ()); phe_log (t); g_free (t);
  return m;
}
static inline void *dlsym (void *h, const char *name) { gpointer p = NULL; return g_module_symbol ((GModule *) h, name, &p) ? p : NULL; }
static inline int dlclose (void *h) { return g_module_close ((GModule *) h) ? 0 : 1; }
static inline const char *dlerror (void) { return g_module_error (); }
#endif
