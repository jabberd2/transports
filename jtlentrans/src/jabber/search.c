#include "jabber.h"
#include "config.h"
#include "debug.h"
#include "users.h"
#include "jid.h"
#include "encoding.h"


void jabber_iq_search_set(tt_user **user, char *from, char *to, char *id, xode query) {

    xode z, z1, z2;
    tt_user *user0;
    char *char_temp = NULL, *bare_from = NULL;
	my_debug(3, "jabber: Wyszukiwanie...");
	bare_from = jid_jidr2jid(from);
	user = jid_jid2user(bare_from);
	t_free(bare_from);

	if (user) {
		user0 = *user;
		if (user0) {
			if (user0->tlen_loged == 2) {
				struct tlen_pubdir *search;
				my_strcpy(user0->search_id, id);
				my_strcpy(user0->search_jid, from);
				search = tlen_new_pubdir();
				if ((z = xode_get_tag(query, "x?xmlns=jabber:x:data")) != NULL) {
					my_debug(4, "Dane przekazane przez x");
					if ((char_temp = xode_get_attrib(z, "type")) == NULL) {
						my_debug(4, "Brak `type`");
					} else {
						if (strcmp(char_temp, "submit") != 0) {
							my_debug(4, "Nieznany typ");
						} else {
                            search->id = get_x_field(z, "tid");
                            search->nick = get_x_field(z, "nick");
                            search->firstname = get_x_field(z, "firstname");
                            search->lastname = get_x_field(z, "lastname");
                            search->city = get_x_field(z, "city");
                            tlen_search(user0->tlen_sesja, search);
							tlen_free_pubdir(search);
						}
					}
				} else {
					my_debug(4, "Dane przekazane przez tagi");
                    search->city = get_field(query, "city");
                    search->firstname = get_field(query, "first");
                    search->lastname = get_field(query, "last");
                    search->nick = get_field(query, "nick");
                    /* sprawdzic czy sa jakies pola */
                    tlen_search(user0->tlen_sesja, search);
                    tlen_free_pubdir(search);
                }
			} else {
				jabber_sndiq(config_myjid, from, "error", id, "<error code='409'>Not loged</error>");
			}
		}
	} else {
		my_debug(0, "jabber: Nie zarejstrowany");
		jabber_sndiq(to, from, "error", id, "<error code='409'>Not registred</error>");
	}
}

int jabber_sndsearch_form(const char *id, const char *to) {
	xode iq, query, x, temp, gender;
	char *to_jid;
	tt_user **user;

	to_jid = jid_jidr2jid(to);
	if ((user = jid_jid2user(to_jid)) == NULL) {
		my_debug(0, "Nie jestes zarejstrowany");
		return -1;
	}
	t_free(to_jid);


	/* iq */
	iq = xode_new("iq");
	xode_put_attrib(iq, "type", "result");
	xode_put_attrib(iq, "from", config_myjid);
	xode_put_attrib(iq, "to", to);
	xode_put_attrib(iq, "id", id);

	/* query */
	query = xode_insert_tag(iq, "query");
	xode_put_attrib(query, "xmlns", "jabber:iq:search");

	temp = xode_insert_tag(query, "instructions");
	xode_insert_cdata(temp, config_search_instructions, -1); 
	xode_insert_tag(query, "active");
	xode_insert_tag(query, "first");
	xode_insert_tag(query, "last");
	xode_insert_tag(query, "nick");
	xode_insert_tag(query, "city");
	xode_insert_tag(query, "gender");
	xode_insert_tag(query, "born");
	xode_insert_tag(query, "username");

	/* jabber:x:data */
	x = xode_insert_tag(query, "x");
	xode_put_attrib(x, "xmlns", "jabber:x:data");
	xode_put_attrib(x, "type", "form");
	/* naglowek */
	temp = xode_insert_tag(x, "title");
	xode_insert_cdata(temp, "Wyszukiwanie w katalogu publicznym Tlena", -1); 
	temp = xode_insert_tag(x, "instructions");
	xode_insert_cdata(temp, config_search_instructions, -1); 

	/* pola */
	temp = xode_insert_tag(x, "field");
	xode_put_attrib(temp, "type", "text-single");
	xode_put_attrib(temp, "label", "Login Tlen");
	xode_put_attrib(temp, "var", "tid");

	temp = xode_insert_tag(x, "field");
	xode_put_attrib(temp, "type", "text-single");
	xode_put_attrib(temp, "label", "Pseudonim");
	xode_put_attrib(temp, "var", "nick");

	temp = xode_insert_tag(x, "field");
	xode_put_attrib(temp, "type", "text-single");
	xode_put_attrib(temp, "label", "Imi?");
	xode_put_attrib(temp, "var", "firstname");

	temp = xode_insert_tag(x, "field");
	xode_put_attrib(temp, "type", "text-single");
	xode_put_attrib(temp, "label", "Nazwisko");
	xode_put_attrib(temp, "var", "lastname");

	temp = xode_insert_tag(x, "field");
	xode_put_attrib(temp, "type", "text-single");
	xode_put_attrib(temp, "label", "Rok urodzenia");
	xode_put_attrib(temp, "var", "birthyear");

	temp = xode_insert_tag(x, "field");
	xode_put_attrib(temp, "type", "text-single");
	xode_put_attrib(temp, "label", "Miasto");
	xode_put_attrib(temp, "var", "city");

	gender = xode_insert_tag(x, "field");
	xode_put_attrib(gender, "type", "list-single");
	xode_put_attrib(gender, "label", "P?e?");
	xode_put_attrib(gender, "var", "gender");

	temp = xode_insert_tag(gender, "value");
	xode_insert_cdata(temp, "_any_", -1);

	temp = xode_insert_tag(gender, "option");
	xode_put_attrib(temp, "label", "Dowolna");
	temp = xode_insert_tag(temp, "value");
	xode_insert_cdata(temp, "_any_", -1);

	temp = xode_insert_tag(gender, "option");
	xode_put_attrib(temp, "label", "Kobieta");
	temp = xode_insert_tag(temp, "value");
	xode_insert_cdata(temp, "1", -1);

	temp = xode_insert_tag(gender, "option");
	xode_put_attrib(temp, "label", "M??czyzna");
	temp = xode_insert_tag(temp, "value");
	xode_insert_cdata(temp, "2", -1);

	temp = xode_insert_tag(x, "field");
	xode_put_attrib(temp, "type", "boolean");
	xode_put_attrib(temp, "label", "Tylko dost?pne");
	xode_put_attrib(temp, "var", "active");
	temp = xode_insert_tag(temp, "value");
	xode_insert_cdata(temp, "1", -1);

	temp = xode_insert_tag(x, "field");
	xode_put_attrib(temp, "type", "text-single");
	xode_put_attrib(temp, "label", "Nazwisko rodowe");
	xode_put_attrib(temp, "var", "familyname");

	temp = xode_insert_tag(x, "field");
	xode_put_attrib(temp, "type", "text-single");
	xode_put_attrib(temp, "label", "Miasto pochodzenia");
	xode_put_attrib(temp, "var", "familycity");

	temp = xode_insert_tag(x, "field");
	xode_put_attrib(temp, "type", "text-single");
	xode_put_attrib(temp, "label", "Ile grup wynik?w");
	xode_put_attrib(temp, "var", "maxgroups");
	xode_insert_tag(temp, "required");
	temp = xode_insert_tag(temp, "value");
	xode_insert_cdata(temp, "1", -1);

	my_strcpy(js_buf2, xode_to_str(iq));

	jabber_send(js_buf2);
	my_debug(5, "jabber: Zapytanie: %s", js_buf2);

	t_free(js_buf2);
	xode_free(iq);

	return 0;
}

int jabber_sndsearch_result(const char *id, const char *to, const tx_search **tx_results) {
	xode iq, query, x, x_item, temp, reported;
	tx_search **item, *item0;
	tt_user **user;
	char *jidt;
	char *to_jid;

	if (to == NULL)
		return -1;

	to_jid = jid_jidr2jid(to);
	if ((user = jid_jid2user(to_jid)) == NULL) {
		my_debug(0, "Nie jestes zarejstrowany");
		return -1;
	}
	t_free(to_jid);

	my_debug(2, "Wysylam wyniki wyszukiwanie do %s, id %s", to, id);

	/* iq */
	iq = xode_new("iq");
	xode_put_attrib(iq, "type", "result");
	xode_put_attrib(iq, "from", config_myjid);
	xode_put_attrib(iq, "to", to);
	xode_put_attrib(iq, "id", id);

	/* query */
	query = xode_insert_tag(iq, "query");
	xode_put_attrib(query, "xmlns", "jabber:iq:search");

	/* jabber:x:data */
	x = xode_insert_tag(query, "x");
	xode_put_attrib(x, "xmlns", "jabber:x:data");
	xode_put_attrib(x, "type", "result");

	/* naglowek */
	temp = xode_insert_tag(x, "title");
	xode_insert_cdata(temp, "Wynik przeszukiwania katalogu publicznego Tlena", -1);

	reported = xode_insert_tag(x, "reported");

	temp = xode_insert_tag(reported, "field");
	xode_put_attrib(temp, "type", "jid-single");
	xode_put_attrib(temp, "label", "JID");
	xode_put_attrib(temp, "var", "jid");

	temp = xode_insert_tag(reported, "field");
	xode_put_attrib(temp, "type", "text-single");
	xode_put_attrib(temp, "label", "Imi?");
	xode_put_attrib(temp, "var", "first");

	temp = xode_insert_tag(reported, "field");
	xode_put_attrib(temp, "type", "text-single");
	xode_put_attrib(temp, "label", "Nazwisko");
	xode_put_attrib(temp, "var", "last");

	temp = xode_insert_tag(reported, "field");
	xode_put_attrib(temp, "type", "text-single");
	xode_put_attrib(temp, "label", "Nick");
	xode_put_attrib(temp, "var", "nick");

	temp = xode_insert_tag(reported, "field");
	xode_put_attrib(temp, "type", "text-single");
	xode_put_attrib(temp, "label", "Email");
	xode_put_attrib(temp, "var", "email");

	if (tx_results != NULL) {

	for (item = (tx_search**)tx_results; *item; item++) {
		char *email0, *firstname0, *lastname0;
		item0 = *item;

		jidt = jid_tid2jid(item0->tid);

		email0 = unicode_iso88592_2_utf8(item0->email);
		firstname0 = unicode_iso88592_2_utf8(item0->firstname);
		lastname0 = unicode_iso88592_2_utf8(item0->lastname);

		/* potrzebne to? */
		x_item = xode_insert_tag(query, "item");
		xode_put_attrib(x_item, "jid", jidt);

		temp = xode_insert_tag(x_item, "first");
		xode_insert_cdata(temp, firstname0, -1);

		temp = xode_insert_tag(x_item, "last");
		xode_insert_cdata(temp, lastname0, -1);

		temp = xode_insert_tag(x_item, "nick");
		xode_insert_cdata(temp, item0->tid, -1);

		temp = xode_insert_tag(x_item, "email");
		xode_insert_cdata(temp, email0, -1);

		/* jabber:x:data */
		x_item = xode_insert_tag(x, "item");

		temp = xode_insert_tag(x_item, "field");
		xode_put_attrib(temp, "var", "jid");
		temp = xode_insert_tag(temp, "value");
		xode_insert_cdata(temp, jidt, -1); 

		temp = xode_insert_tag(x_item, "field");
		xode_put_attrib(temp, "var", "first");
		temp = xode_insert_tag(temp, "value");
		xode_insert_cdata(temp, firstname0, -1);

		temp = xode_insert_tag(x_item, "field");
		xode_put_attrib(temp, "var", "last");
		temp = xode_insert_tag(temp, "value");
		xode_insert_cdata(temp, lastname0, -1);

		temp = xode_insert_tag(x_item, "field");
		xode_put_attrib(temp, "var", "nick");
		temp = xode_insert_tag(temp, "value");
		xode_insert_cdata(temp, item0->tid, -1);

		temp = xode_insert_tag(x_item, "field");
		xode_put_attrib(temp, "var", "email");
		temp = xode_insert_tag(temp, "value");
		xode_insert_cdata(temp, email0, -1);

		t_free(email0);
		t_free(firstname0);
		t_free(lastname0);
		t_free(jidt);
	}
	}

	my_strcpy(js_buf2, xode_to_str(iq));

	jabber_send(js_buf2);
	my_debug(5, "jabber: Zapytanie: %s", js_buf2);

	t_free(js_buf2);
	xode_free(iq);

	return 0;
}
