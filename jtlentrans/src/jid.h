#ifndef __JID_H
#define __JID_H

/* funkcje dostepne z zewnatrz */
extern char *jid_getusername(const char *src);
extern char *jid_getdomainname(const char *src);

extern char *jid_tid2jid(const char *tid);
extern char *jid_jid2tid(const char *jid);
extern char *jid_jid2tid_short(const char *jid);
extern char *jid_tid2tid_short(const char *tid);
extern char *jid_my_jid2my_tid(const char *my_jid);
extern char *jid_my_tid2my_jid(const char *my_tid);

extern char *jid_jidr2jid(const char *jidr);

extern tt_user **jid_jid2user(const char *jid); 
extern tt_user **jid_tid2user(const char *tid); 

#endif /* __JID_H */
