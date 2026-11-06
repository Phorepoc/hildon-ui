/* libintl.h -- stand-in: Open C has no gettext; GLib is built without NLS, text passes through unchanged. */
#ifndef PHE_LIBINTL_H
#define PHE_LIBINTL_H
#define gettext(s) ((char *) (s))
#define dgettext(d, s) ((char *) (s))
#define dcgettext(d, s, c) ((char *) (s))
#define ngettext(s, p, n) ((char *) ((n) == 1 ? (s) : (p)))
#define dngettext(d, s, p, n) ((char *) ((n) == 1 ? (s) : (p)))
#define textdomain(d) ((char *) (d))
#define bindtextdomain(d, dir) ((char *) (dir))
#define bind_textdomain_codeset(d, c) ((char *) (c))
#endif
