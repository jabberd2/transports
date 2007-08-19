#include "all.h"

#include <stdio.h>
#include <sys/types.h>
#include <dirent.h>

#include <libxode.h>

#include "config.h"
#include "users.h"
#include "jid.h"

tt_user **tt_users;

tt_user *users_read(const char *jid) {
	tt_user *user;
	xode cfg, x;
	char *file, *temp;

	if (jid == NULL)
		return NULL;

	if ((file = malloc(strlen(config_dir) + strlen(jid) + 2)) == NULL) {
		perror("malloc()");
		exit(1);
	}
	sprintf(file, "%s/%s", config_dir, jid);

	my_debug(0, "users: Wczytuje plik %s", file);

	if ((cfg = xode_from_file(file)) == NULL) {
		my_debug(0, "users: Nie moge wczytac usera");
		return NULL;
	}

	user = (tt_user*)malloc(sizeof(tt_user));
	memset(user, 0, sizeof(tt_user));

	x = xode_get_tag(cfg, "jid");
	temp = xode_get_data(x);
	my_strcpy(user->jid, temp);

	x = xode_get_tag(cfg, "tid");
	temp = xode_get_data(x);
	my_strcpy(user->tid_short, temp);

	user->tid = malloc(strlen(user->tid_short) + strlen("@tlen.pl") + 1);
	sprintf(user->tid, "%s@tlen.pl", user->tid_short);

	x = xode_get_tag(cfg, "password");
	temp = xode_get_data(x);
	my_strcpy(user->password, temp);

	user->tlen_sesja = NULL;
	user->tlen_loged = 0;
	user->last_ping_time = 0;

	user->roster = NULL;
	user->search = NULL;
	user->search_id = NULL;

	x = xode_get_tag(cfg, "notify");

	temp = xode_get_attrib(x, "mail");
	if (temp != NULL) {
		if (strcmp(temp, "1") == 0) {
			user->mailnotify = 1;
		} else if (strcmp(temp, "0") == 0) {
			user->mailnotify = 0;
		} else {
			user->mailnotify = 0;
			my_debug(0, "Blad w pliku");
		}
	} else {
		user->mailnotify = 0;
	}

	temp = xode_get_attrib(x, "typing");
	if (temp != NULL) {
		if (strcmp(temp, "1") == 0) {
			user->typingnotify = 1;
		} else if (strcmp(temp, "0") == 0) {
			user->typingnotify = 0;
		} else {
			user->typingnotify = 0;
			my_debug(0, "Blad w pliku");
		}
	} else {
		user->typingnotify = 0;
	}

	temp = xode_get_attrib(x, "alarm");
	if (temp != NULL) {
		if (strcmp(temp, "1") == 0) {
			user->alarmnotify = 1;
		} else if (strcmp(temp, "0") == 0) {
			user->alarmnotify = 0;
		} else {
			user->alarmnotify = 0;
			my_debug(0, "Blad w pliku");
		}
	} else {
		user->alarmnotify = 0;
	}

	user->status = NULL;
	user->status_type = 0;

	user->jabber_status = NULL;
	user->jabber_status_type = NULL;

	xode_free(cfg);
	t_free(file);

	return user;
}

tt_user *users_save(const char *jid) {
	tt_user **user, *user0;
	xode cfg, x;
	char *file;

	if (jid == NULL)
		return NULL;

	if ((file = malloc(strlen(config_dir) + strlen(jid) + 2)) == NULL) {
		perror("malloc()");
		exit(1);
	}
	sprintf(file, "%s/%s", config_dir, jid);

	my_debug(0, "users: Zapisuje plik %s", file);

	user = jid_jid2user(jid);
	user0 = *user;

	cfg = xode_new("user");

	x = xode_insert_tag(cfg, "jid");
	xode_insert_cdata(x, user0->jid, -1);

	x = xode_insert_tag(cfg, "tid");
	xode_insert_cdata(x, user0->tid_short, -1);

	x = xode_insert_tag(cfg, "password");
	xode_insert_cdata(x, user0->password, -1);

	x = xode_insert_tag(cfg, "notify");
	xode_put_attrib(x, "mail", (user0->mailnotify ? "1" : "0"));
	xode_put_attrib(x, "typing", (user0->typingnotify ? "1" : "0"));
	xode_put_attrib(x, "alarm", (user0->alarmnotify ? "1" : "0"));

	xode_to_file(file, cfg);
	xode_free(cfg);
	t_free(file);

	return user0;
}

int users_unlink(const char *jid) {
	char *file;

	if (jid == NULL)
		return -1;

	if ((file = malloc(strlen(config_dir) + strlen(jid) + 2)) == NULL) {
		perror("malloc()");
		exit(1);
	}
	sprintf(file, "%s/%s", config_dir, jid);

	my_debug(0, "users: Usuwam plik %s!!", file);

	unlink(file);
	t_free(file);

	return 0;
}

int users_size() {
	tt_user **user;
	int n;

	for (n = 0, user = tt_users; *user; user++, n++)
		;

	return n;
}

int users_init() {
	DIR *katalog;
	struct dirent *pozycja;

	tt_user *user;
	int i;

	i = 0;
	if ((tt_users = (tt_user**)malloc((i+1)*sizeof(tt_user*))) == NULL) {
		my_debug(0, "mallloc failed");
		exit(1);
	}

	if (!(katalog = opendir(config_dir))) {
		perror("opendir()");
		exit(1);
	}

	while((pozycja = readdir(katalog)) != NULL) {
		if (strcmp(pozycja->d_name, ".") == 0 || strcmp(pozycja->d_name, "..") == 0)
			continue;

		if ((user = users_read(pozycja->d_name)) == NULL) {
			my_debug(0, "users_read failed");
			exit(1);
		}

		if ((tt_users = (tt_user**)realloc(tt_users, (i+2)*sizeof(tt_user*))) == NULL) {
			my_debug(0, "reallloc failed");
			exit(1);
		}

		tt_users[i] = user;
		i++;
	}
	closedir(katalog);
	tt_users[i] = NULL;

	my_debug(0, "Wczytano %d userow", users_size());

	return 0;
}

int users_close() {
	tt_user **user, *user0;
	tx_user **tuser, *tuser0;

	for (user = tt_users; *user; user++) {
		user0 = *user;
		t_free(user0->jid);
		t_free(user0->password);
		t_free(user0->tid);
		t_free(user0->tid_short);
		t_free(user0->status);
		t_free(user0->jabber_status);
		t_free(user0->jabber_status_type);
		t_free(user0->search_id);
		t_free(user0->search_jid);
		if (user0->roster) {
			for (tuser = user0->roster; *tuser; tuser++) {
				tuser0 = *tuser;
				t_free(tuser0->tid);
				t_free(tuser0->status);
				t_free(tuser0);
			}
			t_free(user0->roster);
		}
		/* FIXIT: wyniki wyszukiwania, vcard */
		t_free(user0);
	}
	t_free(tt_users);

	return 0;
}

int users_sizet(tx_user **tx_users) {
	tx_user **tuser;
	int n;

	if (tx_users == NULL)
		return 0;

	for (n = 0, tuser = tx_users; *tuser; tuser++, n++)
		;

	return n;
}

int users_sizes(tx_search **tx_results) {
	tx_search **item;
	int n;

	if (tx_results == NULL)
		return 0;

	for (n = 0, item = tx_results; *item; item++, n++)
		;

	return n;
}
