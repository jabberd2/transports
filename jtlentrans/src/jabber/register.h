#ifndef __JABBERREGISTER_H
#define __JABBERREGISTER_H

extern int jabber_sndregister_form(const char *id, const char *to);
extern int jabber_register(const char *jid0, const char *tid, const char *password);
extern int jabber_unregister(const char *jid0);

#endif
