/* Copyright The Phorepoc Authors */
/* SPDX-License-Identifier: BSD-2-Clause */
/* shim: <libintl.h> for a system that ships no message catalogs: a lookup falls back to the msgid, as GNU gettext does when it finds no catalog for the domain (ngettext: the
 * English plural rule), EXCEPT for the Maemo logical ids below. libhildon and the control panel use logical ids ("wdgt_ti_date") as msgids and rely on a catalog for every
 * visible string, and some of them are strftime formats ("wdgt_va_date_long"): this small English table is ours (Phorepoc), written from what each id is used for.
 * bindtextdomain / textdomain / bind_textdomain_codeset record nothing and report the request as accepted. BSD-2-Clause (Phorepoc). */
#ifndef PHE_LIBINTL_H
#define PHE_LIBINTL_H
#include <string.h>
static inline char *
phe_intl_english (const char *msgid)
{
  static const char *const table[][2] = {
    { "wdgt_ti_date", "Date" }, { "wdgt_ti_time", "Time" }, { "wdgt_bd_done", "Done" },
    { "wdgt_va_date_long", "%A %e %B %Y" }, { "wdgt_va_12h_time_am", "%l:%M am" }, { "wdgt_va_12h_time_pm", "%l:%M pm" }, { "wdgt_va_24h_time", "%H:%M" },
    { "wdgt_va_am", "am" }, { "wdgt_va_pm", "pm" }, { "wdgt_va_12h_hours", "%l" }, { "wdgt_va_24h_hours", "%H" }, { "wdgt_va_minutes", "%M" },
    { "wdgt_va_month", "%B" }, { "wdgt_va_day_numeric", "%d" }, { "wdgt_va_year", "%Y" },
    { "copa_ia_extras", "Extras" }, { "copa_ia_personalisation", "Personalisation" }, { "copa_ia_connectivity", "Connectivity" }, { "copa_ia_general", "General" },
  };
  unsigned i;
  if (msgid && (msgid[0] == 'w' || msgid[0] == 'c'))
    for (i = 0; i < sizeof table / sizeof table[0]; i++)
      if (strcmp (msgid, table[i][0]) == 0) return (char *) table[i][1];
  return (char *) msgid;
}
static inline char *gettext (const char *msgid) { return phe_intl_english (msgid); }
static inline char *dgettext (const char *domain, const char *msgid) { (void) domain; return phe_intl_english (msgid); }
static inline char *dcgettext (const char *domain, const char *msgid, int category) { (void) domain; (void) category; return phe_intl_english (msgid); }
static inline char *ngettext (const char *msgid1, const char *msgid2, unsigned long n) { return (char *) (n == 1 ? msgid1 : msgid2); }
static inline char *dngettext (const char *domain, const char *msgid1, const char *msgid2, unsigned long n) { (void) domain; return (char *) (n == 1 ? msgid1 : msgid2); }
static inline char *textdomain (const char *domain) { return (char *) (domain ? domain : "messages"); }
static inline char *bindtextdomain (const char *domain, const char *dirname) { (void) domain; return (char *) dirname; }
static inline char *bind_textdomain_codeset (const char *domain, const char *codeset) { (void) domain; return (char *) codeset; }
#endif
