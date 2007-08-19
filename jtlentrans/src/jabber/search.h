#ifndef __JABBERSEARCH_H
#define __JABBERSEARCH_H

extern void jabber_iq_search_set(tt_user **user, char *from, char *to, char *id, xode query);
extern int jabber_sndsearch_form(const char *id, const char *to);
extern int jabber_sndsearch_result(const char *id, const char *to, const tx_search **tx_results);

#endif
