/* Copyright The Phorepoc Authors */
/* SPDX-License-Identifier: BSD-2-Clause */
/* phe_xlib.h -- the part of Xlib / gdkx.h that libhildon uses, implemented over what the Symbian GDK backend really has: GDK atoms (the X predefined atoms 0-68 are the same numbers),
 * window properties, client messages, window groups. A window id (XID) is the native window id of this backend (the GdkWindow address, gdk_window_lookup () finds it again); a "Display" is the GdkDisplay.
 * Everything here is semantics of an X server whose only client is this process, and whose window manager is the backend itself. Implemented in phehildon_x11.c. BSD-2-Clause (Phorepoc). */
#ifndef PHE_XLIB_H
#define PHE_XLIB_H
#include <glib.h>
#include <gdk/gdk.h>
G_BEGIN_DECLS

typedef struct _PheXDisplay Display;                  /* really the GdkDisplay */
typedef gulong XID;
typedef XID Window;
typedef XID Drawable;
typedef gulong Atom;
typedef gulong Time;
typedef char *XPointer;
typedef int Bool;
typedef int Status;

#define True 1
#define False 0
#define None 0L
#define AnyPropertyType 0L
#define Success 0
#define BadWindow 3
#define BadAtom 5
#define PropModeReplace 0
#define PropModePrepend 1
#define PropModeAppend 2
#define ClientMessage 33
#define PropertyNotify 28
#define SubstructureNotifyMask (1L << 19)
#define SubstructureRedirectMask (1L << 20)
#define WindowGroupHint (1L << 6)

/* the predefined atoms (X11/Xatom.h) */
#define XA_PRIMARY ((Atom) 1)
#define XA_SECONDARY ((Atom) 2)
#define XA_ARC ((Atom) 3)
#define XA_ATOM ((Atom) 4)
#define XA_BITMAP ((Atom) 5)
#define XA_CARDINAL ((Atom) 6)
#define XA_COLORMAP ((Atom) 7)
#define XA_CURSOR ((Atom) 8)
#define XA_DRAWABLE ((Atom) 17)
#define XA_FONT ((Atom) 18)
#define XA_INTEGER ((Atom) 19)
#define XA_PIXMAP ((Atom) 20)
#define XA_POINT ((Atom) 21)
#define XA_RECTANGLE ((Atom) 22)
#define XA_STRING ((Atom) 31)
#define XA_VISUALID ((Atom) 32)
#define XA_WINDOW ((Atom) 33)
#define XA_WM_COMMAND ((Atom) 34)
#define XA_WM_TRANSIENT_FOR ((Atom) 68)
#define XA_WM_HINTS ((Atom) 35)
#define XA_WM_CLIENT_MACHINE ((Atom) 36)
#define XA_WM_ICON_NAME ((Atom) 37)
#define XA_WM_NAME ((Atom) 39)
#define XA_WM_NORMAL_HINTS ((Atom) 40)

typedef struct {
  int type; gulong serial; Bool send_event; Display *display; Window window; Atom message_type; int format;
  union { char b[20]; short s[10]; glong l[5]; } data;
} XClientMessageEvent;

typedef struct { int type; gulong serial; Bool send_event; Display *display; Window window; } XAnyEvent;
typedef struct { int type; gulong serial; Bool send_event; Display *display; Window window; Atom atom; Time time; int state; } XPropertyEvent;
typedef union _XEvent { int type; XAnyEvent xany; XPropertyEvent xproperty; XClientMessageEvent xclient; glong pad[24]; } XEvent;

typedef struct { glong flags; Bool input; int initial_state; XID icon_pixmap; Window icon_window; int icon_x, icon_y; XID icon_mask; XID window_group; } XWMHints;

#define GDK_WINDOW_XID(win)          ((Window) (gsize) (win))
#define GDK_WINDOW_XDISPLAY(win)     ((Display *) gdk_window_get_display (win))
#define GDK_DISPLAY_XDISPLAY(dpy)    ((Display *) (dpy))
#define GDK_DISPLAY()                ((Display *) gdk_display_get_default ())
#define GDK_ROOT_WINDOW()            ((Window) (gsize) gdk_get_default_root_window ())
#define GDK_SCREEN_XDISPLAY(screen)  ((Display *) gdk_screen_get_display (screen))

Atom   XInternAtom (Display *display, const char *atom_name, Bool only_if_exists);
int    XChangeProperty (Display *display, Window w, Atom property, Atom type, int format, int mode, const unsigned char *data, int nelements);
int    XGetWindowProperty (Display *display, Window w, Atom property, glong long_offset, glong long_length, Bool delete_prop, Atom req_type,
                           Atom *actual_type_return, int *actual_format_return, gulong *nitems_return, gulong *bytes_after_return, unsigned char **prop_return);
int    XDeleteProperty (Display *display, Window w, Atom property);
int    XSetTransientForHint (Display *display, Window w, Window prop_window);
Status XSetWMProtocols (Display *display, Window w, Atom *protocols, int count);
Status XGetWMProtocols (Display *display, Window w, Atom **protocols_return, int *count_return);
XWMHints *XGetWMHints (Display *display, Window w);
Status XSendEvent (Display *display, Window w, Bool propagate, glong event_mask, XEvent *event_send);
int    XIfEvent (Display *display, XEvent *event_return, Bool (*predicate) (Display *, XEvent *, char *), char *arg);
int    XSync (Display *display, Bool discard);
int    XFlush (Display *display);
int    XFree (void *data);
Window XDefaultRootWindow (Display *display);
#define RootWindow(display, screen)      XDefaultRootWindow (display)       /* one screen */
#define GDK_SCREEN_XNUMBER(screen)       0

/* gdkx.h */
Atom    gdk_x11_get_xatom_by_name_for_display (GdkDisplay *display, const gchar *atom_name);
Atom    gdk_x11_get_xatom_by_name (const gchar *atom_name);
GdkAtom gdk_x11_xatom_to_atom (Atom xatom);
Atom    gdk_x11_atom_to_xatom (GdkAtom atom);
gboolean gdk_x11_screen_supports_net_wm_hint (GdkScreen *screen, GdkAtom property);

G_END_DECLS
#endif
