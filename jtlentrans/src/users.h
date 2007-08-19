#ifndef __USERS_H
#define __USERS_H

#include <libtlen/libtlen.h>

typedef enum  {sub_none, sub_from, sub_to, sub_both} subs_enum;

typedef struct tx_user_s {
	char *tid;
	char *status;
    subs_enum subs;
    short int ask;
} tx_user;


typedef struct tx_search_s {
	char *firstname;
	char *lastname;
	char *tid;
	char *city;
	char *sex;
	char *age;
	char *email;
} tx_search;

typedef struct rousource_s {
	char *resource;
	int priority;
} resource;

typedef struct tt_user_s {
	char *jid;
	resource **jabber_resources;
	char *tid;
	char *tid_short;
	char *password;
	int tlen_loged;
	char status_type;
	char *status;
	char *jabber_status_type;
	char *jabber_status;
	struct tlen_session *tlen_sesja;
	int last_ping_time;
	tx_user **roster;
	tx_search **search;
	char *search_id;
	char *search_jid;
	char mailnotify;
	char typingnotify;
	char alarmnotify;
	int connect_trys;
	int connect_next_try; /* czas */
} tt_user;

/* NULL na koncu oznacza koniec listy */
extern tt_user **tt_users;

extern int users_init();
extern int users_close();
extern int users_size();
extern int users_sizet(tx_user **tx_users);
extern int users_sizes(tx_search **tx_results);
extern tt_user *users_save(const char *jid);
extern tt_user *users_read(const char *jid);
extern int users_unlink(const char *jid);

#endif /* __USERS_H */
