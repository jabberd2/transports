#include "jabber.h"
#include "config.h"
#include "debug.h"
#include "users.h"
#include "jid.h"

int jabber_sndregister_form(const char *id, const char *to) {
	xode iq, query, temp;

	iq = xode_new("iq");
	xode_put_attrib(iq, "type", "result");
	xode_put_attrib(iq, "from", config_myjid);
	xode_put_attrib(iq, "to", to);
	xode_put_attrib(iq, "id", id);

	query = xode_insert_tag(iq, "query");
	xode_put_attrib(query, "xmlns", "jabber:iq:register");
    
	temp = xode_insert_tag(query, "instructions");
	xode_insert_cdata(temp, config_register_instructions, -1);

	xode_insert_tag(query, "username");
	xode_insert_tag(query, "password");

	/* TODO: jabber:x:data dla formularza rejestracyjnego...
	x = 	"<x xmlns='jabber:x:data' type='form'>"
		"	<title>Formularz rejestracyjny Jabber Tlen Transport</title>"
		"	<instructions>Wype?nij ten formularz aby zarejestrowa? si? w transporcie. Mo?esz u?y? trybu rejestracji w przysz?o?ci aby zmieni? swoje ustawienia, has?o, informacje w katalogu albo wyrejestrowa? si?.</instructions>"
		"	<field type='text-single' label='Login Tlen' var='tin'><required/></field>"
		"	<field type='text-private' label='Haslo' var='password'><required/></field>"
		"</x>";
	*/   

	my_strcpy(js_buf2, xode_to_str(iq));
	jabber_send(js_buf2);
	my_debug(5, "jabber: Zapytanie: %s", js_buf2);

	xode_free(iq);
	t_free(js_buf2);
	return 0;

    
}

int jabber_register(const char *jid0, const char *tid_short, const char *password) {
	tt_user **user, *user0;
	char *jid, *tid_short0;
	int n;

	jid = jid_jidr2jid(jid0);

	tid_short0 = jid_tid2tid_short(tid_short);

	my_debug(0, "Rejestruje %s jako %s@tlen.pl...", jid, tid_short0);

	if (jid == NULL)
		return -1;
	if (tid_short == NULL)
		return -1;
	if (password == NULL)
		return -1;

	if ((user = jid_jid2user(jid)) != NULL) {
		my_debug(0, "Juz jestes zarejstrowany jako %s", (*user)->tid);
		return 1;
	}
	if ((user = jid_tid2user(tid_short0)) != NULL) {
		my_debug(0, "Juz %s zarejstrowany na tym loginie tlen", (*user)->jid);
		//return 2;
	}

	n = users_size();
	tt_users = (tt_user**)realloc(tt_users, (n+2)*sizeof(tt_user*));
	tt_users[n+1] = NULL;
	tt_users[n] = NULL;

	user0 = (tt_user*)malloc(sizeof(tt_user));

	my_strcpy(user0->jid, jid);
	my_strcpy(user0->tid_short, tid_short0);

	user0->tid = malloc(strlen(user0->tid_short) + strlen("@tlen.pl") + 1);
	sprintf(user0->tid, "%s@tlen.pl", user0->tid_short);

	my_strcpy(user0->password, password);

	user0->tlen_sesja = NULL;
	user0->tlen_loged = 0;
	user0->last_ping_time = 0;

	user0->roster = NULL;
	user0->search = NULL;
	user0->search_id = NULL;

	user0->mailnotify = 1;
	user0->typingnotify = 1;
	user0->alarmnotify = 1;

	user0->status = NULL;
	user0->status_type = 0;

	user0->jabber_status = NULL;
	user0->jabber_status_type = NULL;

	tt_users[n] = user0; /* dodajemy na koncu */

	users_save(jid);
	t_free(jid);

	return 0;
}

int jabber_unregister(const char *jid0) {
	tt_user **user, *user0;
	char *jid;
	int n;

	jid = jid_jidr2jid(jid0);
	my_debug(0, "Wyrejestruje %s...", jid);

	if (jid == NULL)
		return -1;

	if ((user = jid_jid2user(jid)) == NULL) {
		my_debug(0, "Nie jestes zarejstrowany");
		return 1;
	}

	user0 = *user;

	my_debug(0, "Usuwanie %s...", user0->tid);

	n = users_size();

	tlen_logout_user(user0, NULL);

	t_free(user0->jid);
	t_free(user0->tid_short);
	t_free(user0->tid);
	t_free(user0->password);

	t_free(user0->status);
	t_free(user0->jabber_status);
	t_free(user0->jabber_status_type);

	/* FIXIT: zwolnic reszte */

	t_free(user0);

	/* FIXIT: n = 0 ? */
	*user = tt_users[n-1]; /* ostatni w miejsce usuwanego */
	tt_users[n-1] = NULL; /* ostatni znika */

	tt_users = (tt_user**)realloc(tt_users, (n+1)*sizeof(tt_user*));

	users_unlink(jid);
	t_free(jid);

	return 0;
}
