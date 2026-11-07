/* shim: <libintl.h> for a system that ships no message catalogs: every lookup falls back to the msgid, as GNU gettext does when it finds no catalog for the domain (ngettext: the
 * English plural rule). bindtextdomain / textdomain / bind_textdomain_codeset record nothing and report the request as accepted. BSD-2-Clause (Phorepoc). */
#ifndef PHE_LIBINTL_H
#define PHE_LIBINTL_H
static inline char *gettext (const char *msgid) { return (char *) msgid; }
static inline char *dgettext (const char *domain, const char *msgid) { (void) domain; return (char *) msgid; }
static inline char *dcgettext (const char *domain, const char *msgid, int category) { (void) domain; (void) category; return (char *) msgid; }
static inline char *ngettext (const char *msgid1, const char *msgid2, unsigned long n) { return (char *) (n == 1 ? msgid1 : msgid2); }
static inline char *dngettext (const char *domain, const char *msgid1, const char *msgid2, unsigned long n) { (void) domain; return (char *) (n == 1 ? msgid1 : msgid2); }
static inline char *textdomain (const char *domain) { return (char *) (domain ? domain : "messages"); }
static inline char *bindtextdomain (const char *domain, const char *dirname) { (void) domain; return (char *) dirname; }
static inline char *bind_textdomain_codeset (const char *domain, const char *codeset) { (void) domain; return (char *) codeset; }
#endif
