#include <strings.h>

#include "all.h"

#include "config.h"
#include "users.h"
#include "jid.h"

#include "debug.h"

char *jid_getusername(const char *src) {
	int i;
	char *temp;

	if (src == NULL)
		return NULL;

	temp = strchr(src, '@');
	if (temp == NULL)
	    i = strlen(src);
	else
	    i = temp - src;

	if ((temp = (char*)malloc((i+1)*sizeof(char))) == NULL) {
		my_debug(0, "malloc()");
		exit(1);
	}
	strncpy(temp, src, i);
	temp[i] = '\0';
	return temp;
}

char *jid_getdomainname(const char *src) {
	int i, l;
	char *temp;

	if (src == NULL)
	    return NULL;

	temp = NULL;

	i = strchr(src, '@')  - src;
	l = strlen(src);
	if ((temp = (char*)malloc((l-i)*sizeof(char))) == NULL) {
		my_debug(0, "malloc()");
		exit(1);
	}
	strncpy(temp, src+i+1, l-i);
	return temp;
}

/* misiek@tlen.pl -> misiek@tlen.jabber.mpi.int.pl
   misiek -> misiek@tlen.jabber.mpi.int.pl */
char *jid_tid2jid(const char *tid) {
	char *temp, *temp2;
	int n;

	if (tid == NULL)
		return NULL;

	temp = jid_getusername(tid);

	n = strlen(temp) + strlen(config_myjid) + 2;
	if ((temp2 = malloc(n)) == NULL) {
		my_debug(0, "malloc()");
		exit(1);
	}
	snprintf(temp2, n, "%s@%s", temp, config_myjid);
	/*
	strcpy(temp2, temp);
	strcat(temp2, "@");
	strcat(temp2, config_myjid);
	*/
	t_free(temp);

	return temp2;
}

/* misiek@tlen.jabber.mpi.int.pl -> misiek@tlen.pl */
char *jid_jid2tid(const char *jid) {
	char *temp, *temp2;
	int n;

	if (jid == NULL)
		return NULL;

	temp = jid_getusername(jid);

	n = strlen(temp) + strlen("@tlen.pl") + 1;
	if ((temp2 = malloc(n)) == NULL) {
		my_debug(0, "malloc()");
		exit(1);
	}
	snprintf(temp2, n, "%s@tlen.pl", temp, config_myjid);
	/* strcpy(temp2, temp); */
	t_free(temp);
	/* strcat(temp2, "@tlen.pl"); */
	return temp2;
}

/* misiek@tlen.jabber.mpi.int.pl -> misiek */
char *jid_jid2tid_short(const char *jid) {
	char *temp, *temp2;

	if (jid == NULL)
		return NULL;

	temp = jid_getusername(jid);

	if ((temp2 = malloc((strlen(temp)+1)*sizeof(char))) == NULL) {
		my_debug(0, "malloc()");
		exit(1);
	}
	strcpy(temp2, temp);
	t_free(temp);

	return temp2;
}

/* misie@tlen.pl -> misiek
   misiek -> misiek */
char *jid_tid2tid_short(const char *tid) {
	return jid_jid2tid_short(tid);
}

/* witek@jabber.mpi.int.pl -> movax@tlen.pl
   FIXIT: uwzglednic resurce */
char *jid_my_jid2my_tid(const char *my_jid) {
	tt_user **user;

	if (my_jid == NULL)
	    return NULL;
	
	for(user = tt_users; *user; user++) {
		if (strcmp((*user)->jid, my_jid) == 0)
			return ((*user)->tid);
	}
	return NULL;
}

/* movax@tlen.pl -> witek@jabber.mpi.int.pl
   movax -> witek@jabber.mpi.int.pl */
char *jid_my_tid2my_jid(const char *my_tid) {
	tt_user **user, *user0;

	if (my_tid == NULL)
	    return NULL;

	for (user = tt_users; *user; user++) {
		user0 = *user;
		if (strcmp(user0->tid, my_tid) == 0 || strcmp(user0->tid_short, my_tid) == 0)
			return (user0->jid);
	}
	return NULL;
}

/* movax@jabber.mpi.int.pl/Psi -> movax@jabber.mpi.int.pl */
char *jid_jidr2jid(const char *jidr) {
	int i;
	char *temp;

	if (jidr == NULL)
		return NULL;

	temp = malloc(1);
	temp = strchr(jidr, '/');
	if (temp == NULL)
	    i = strlen(jidr);
	else
	    i = temp - jidr;

	if ((temp = (char*)malloc((i+1)*sizeof(char))) == NULL) {
		my_debug(0, "malloc()");
		exit(1);
	}

	strncpy(temp, jidr, i);
	temp[i] = '\0';

	return temp;
}

tt_user **jid_jid2user(const char *jid) {
	tt_user **user, *user0;

	if (jid == NULL)
		return NULL;

	for (user = tt_users; *user; user++) {
		user0 = *user;
		if (user0->jid) {
			if (strcmp(jid, user0->jid) == 0) {
				return user;
			}
		}
	}

	return NULL;
}

tt_user **jid_tid2user(const char *tid_short) {
	tt_user **user, *user0;

	if (tid_short == NULL)
		return NULL;

	for (user = tt_users; *user; user++) {
		user0 = *user;
		if (user0->tid_short) {
			if (strcmp(tid_short, user0->tid_short) == 0) {
				return user;
			}
		}
	}

	return NULL;
}
