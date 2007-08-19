/*
 * Jabber AIM-Transport libfaim interface
 *
*/

#ifndef AT_IQ_H
#define AT_IQ_H

#ifdef __cplusplus
extern "C" {
#endif

extern HASHTABLE callbacks;

typedef int (*iqcb)(at_session s, jpacket jp);

/* iq.c */
int at_register_iqns(const char *ns, iqcb cb);
int at_run_iqcb(const char *ns, jpacket jp);
void at_init_iqcbs(void);

#ifdef __cplusplus
}
#endif

#endif /* AT_IQ_H */
