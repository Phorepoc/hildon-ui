/* gdkinput-symbian.c -- GdkDevice of the Symbian backend: the single core pointer (WSERV mouse or touch) and the default colormap bookkeeping of the screen. BSD-2-Clause (Phorepoc). */
#include "config.h"
#include "gdksymbian.h"
#include "gdkinput.h"
#include "gdkprivate.h"
#include "gdkinternals.h"

struct _GdkDeviceClass { GObjectClass parent_class; };
G_DEFINE_TYPE (GdkDevice, gdk_device, G_TYPE_OBJECT)
static void gdk_device_init (GdkDevice *d) { d->name = NULL; d->axes = NULL; d->keys = NULL; }
static void gdk_device_finalize (GObject *object) { G_OBJECT_CLASS (gdk_device_parent_class)->finalize (object); }   /* name/axes of the core pointer are static */
static void gdk_device_class_init (GdkDeviceClass *c) { G_OBJECT_CLASS (c)->finalize = gdk_device_finalize; }

/* The device state: the pointer position the window server reported last, relative to @window, and the modifier/button mask that goes with it. */
void
gdk_device_get_state (GdkDevice *device, GdkWindow *window, gdouble *axes, GdkModifierType *mask)
{
  GdkDisplaySymbian *d = GDK_DISPLAY_SYMBIAN (_gdk_display); gint ox = 0, oy = 0;
  g_return_if_fail (device != NULL); g_return_if_fail (GDK_IS_WINDOW (window));
  gdk_window_get_origin (window, &ox, &oy);
  if (axes) { axes[0] = d->pointer_x - ox; axes[1] = d->pointer_y - oy; }
  if (mask) *mask = d->pointer_state;
}

gboolean
gdk_device_get_axis (GdkDevice *device, gdouble *axes, GdkAxisUse use, gdouble *value)
{
  gint i;
  g_return_val_if_fail (device != NULL, FALSE);
  if (axes == NULL) return FALSE;
  for (i = 0; i < device->num_axes; i++)
    if (device->axes[i].use == use) { if (value) *value = axes[i]; return TRUE; }
  return FALSE;
}

/* XInput extension devices do not exist here: the core pointer is the only device, so the request has nothing to enable */
void gdk_input_set_extension_events (GdkWindow *window, gint mask, GdkExtensionMode mode) { g_return_if_fail (GDK_IS_WINDOW (window)); (void) mask; (void) mode; }

/* ---- the screen's default colormap ---- */
static GdkColormap *default_colormap = NULL;
/* until an application sets one, the default colormap of a screen is its system colormap */
GdkColormap *
gdk_screen_get_default_colormap (GdkScreen *screen)
{
  g_return_val_if_fail (GDK_IS_SCREEN (screen), NULL);
  if (default_colormap == NULL) default_colormap = g_object_ref (gdk_screen_get_system_colormap (screen));
  return default_colormap;
}
void
gdk_screen_set_default_colormap (GdkScreen *screen, GdkColormap *colormap)
{
  GdkColormap *old = default_colormap;
  g_return_if_fail (GDK_IS_SCREEN (screen)); g_return_if_fail (GDK_IS_COLORMAP (colormap));
  default_colormap = g_object_ref (colormap);
  if (old) g_object_unref (old);
}
