#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include <libxode.h>

#include <unistd.h>		/* sleep */

#include "all.h"

#include "config.h"
#include "jabber.h"
#include "tlen.h"
#include "jid.h"
#include "encoding.h"

#include "jabber.h"

int jabber_gniazdo;
int jabber_server;

struct sockaddr_in server_addr;
socklen_t server_addrlen;

struct servent *sp;
struct hostent *hp;


/* Buffor dla polaczenia */
char *js_buf = NULL;
int js_buf_len = 1024;

char *js_buf2 = NULL;

/* XStream */
xode_stream js;
xode_pool js_p;

/* Zmienne uzywane przez stream handlera */
char *sid;

int jabber_loged;

int jabber_send(const char *buf) {
	int n;
	my_debug(2, "jabber: wysylam '%s'", buf);
	if ((n = send(jabber_gniazdo, buf, strlen(buf), 0)) < 0) {
		my_debug(0, "jabber: send() failed");
		exit(1);
		return -1;
	} else {
		my_debug(5, "jabber: Wyslane");
	}
	return 0;
}

int jabber_recv(char **buf) {
	char *buffor = *buf;
	int n;
/*	while (1) { */
		if ((n = recv(jabber_gniazdo, buffor, 1024, 0)) < 0) {
			my_debug(0, "recv() failed");
			return -1;
/*			break;
		} else {
			if (n == 0)
				break;
			buffor[n] = 0;
		}
*/	}
	buffor[n] = 0;
	return 0;
}

// co x sekund bombardujemy usera presence?!
int jabber_mypresence() {
	tt_user **user, *user0;
	/* Robimy sie dostepni */
/*	for (user = tt_users; *user; user++) {
		user0 = *user;
		if (user0->tlen_loged) {
			jabber_sndpresence(config_myjid, user0->jid, NULL, user0->jabber_status_type, user0->jabber_status);
		}
	}*/
	return 0;
}

int jabber_sendprobes() {
	tt_user **user, *user0;

	for (user = tt_users; *user; user++) {
		user0 = *user;
		jabber_sndpresence(config_myjid, user0->jid, "probe", NULL, NULL);
	}
	return 0;
}

int jabber_handler() {
	jabber_recv(&js_buf);
	my_debug(2, "jabber: Obsluga zdazen");
	my_debug(5, "jabber: Zawartosc buffora: %s", js_buf);
	xode_stream_eat(js, js_buf, -1);
	return 0;
}

int status_jabber_to_tlen(const char *show) {
	int tlen_type;
	if (show == NULL) {
		tlen_type = 2;
	} else if (strcmp(show, "") == 0) {
		tlen_type = 2;
	} else if (strcmp(show, "away") == 0) {
		tlen_type = 4;
	} else if (strcmp(show, "xa") == 0) {
		tlen_type = 3;
	} else if (strcmp(show, "dnd") == 0) {
		tlen_type = 5;
	} else if (strcmp(show, "chat") == 0) {
		tlen_type = 6;
	} else  {
		tlen_type = 2;
		my_debug(0, "FIXIT: Bladu typu statusu");
	}
	return tlen_type;
}

void jabber_stream_handler(int typ, xode x, void *arg) {

	char *mytag = NULL;
	char *char_temp = NULL;

	mytag = xode_get_name(x);

	if (strcmp(mytag, "handshake") == 0) {
		jabber_loged = 1;
		my_debug(2, "jabber: zdazenie handshake");
	} else if (strcmp(mytag, "stream:stream") == 0) {
		my_debug(2, "jabber: zdazenie stream:stream");
		if ((char_temp = xode_get_attrib(x, "id")) == NULL) {
			my_debug(0, "jabber: Blad w atrybucie id (wymagany)");
			exit(1);
		}
		my_strcpy(sid, char_temp);
	} else if (strcmp(mytag, "message") == 0) {
		jabber_message_handler(x);
	} else if (strcmp(mytag, "presence") == 0) {
		jabber_presence_handler(x);
	} else if (strcmp(mytag, "iq") == 0) {
		jabber_iq_handler(x);
	} else if (strcmp(mytag, "error") == 0) {
		my_debug(1, "jabber: zdazenie error");
		exit(1);
	} else if (strcmp(mytag, "stream:error") == 0) {
		my_debug(1, "jabber: blad stream");
		exit(1);
	}
	xode_free(x);
}

void jabber_message_handler(xode x)
{
	char *to = NULL, *from = NULL, *type = NULL;
	xode xode_temp;
	char *char_temp = NULL;

	char *body = NULL, *bare_from = NULL;
	tt_user **user, *user0;
	char *bare_to = NULL, *tid_to = NULL;
	tx_user **item, *item0;

	int tlen_type;

	my_debug(2, "jabber: zdazenie message");

	if ((to = xode_get_attrib(x, "to")) == NULL) {
		my_debug(5, "jabber: Blad w atrybucie to (wymagany)");
		return;
	}
	if ((from = xode_get_attrib(x, "from")) == NULL) {
		my_debug(5, "jabber: Blad w atrybucie from (wymagany)");
		return;
	}
	if ((type = xode_get_attrib(x, "type")) == NULL) {
		my_debug(5, "jabber: Blad w atrybucie type (opcjonalny)");
	}

	if ((xode_temp = xode_get_tag(x, "body")) == NULL) {
		my_debug(5, "jabber: Blad w tagu body (opcjonalny, zalecany)");
	} else {
		if ((char_temp = xode_get_data(xode_temp)) == NULL) {
			my_debug(5, "jabber: Blad w danych body");
			//return; <- jabber:x:event; hmm?
		} else {
			if ((body = unicode_utf8_2_iso88592((const char *)char_temp)) == NULL) {
				my_debug(5, "jabber: Blad w konwersji body");
				return;
			}
		}
	}

	bare_from = jid_jidr2jid(from);
	user = jid_jid2user(bare_from);
	t_free(bare_from);

	my_debug(3, "jabber: analizowanie wiadomosci i usera");
	if (!user) {
		my_debug(0, "jabber: User nie jest zarejstrowanym uzytkownikiem");
        jabber_snderror(config_myjid, from, "message", "409", "Nie jestes zarejestrowany", NULL);
		t_free(body);
		return;
	}

	if (!*user) {
        jabber_snderror(config_myjid, from, "message", "406", "Nie jestes zalogowany", NULL);
		t_free(body);
		return;
	}

	user0 = *user;
	bare_to = jid_jidr2jid(to);

	if (strcmp(bare_to, config_myjid) == 0) {
		jabber_chatcommands_handler(user0, from, body);
	} else if (user0->tlen_sesja && user0->tlen_loged == 2) {
		int notify;
		notify = 0;
		tid_to = jid_jid2tid(bare_to);
		/* TODO: obluga http://jabber.org/protocol/xhtml-im */
		if ((xode_temp = xode_get_tag(x, "x?xmlns=jabber:x:event")) == NULL) {
			my_debug(5, "jabber: Brak tagu x (opcjonalny)");
		} else {
			my_debug(2, "jabber: message zawiera jabber:x:event");
			notify = ((xode_temp = xode_get_tag(xode_temp, "composing")) == NULL) ? 2 : 1;
		}
        
		if (body) {
            tlen_type = ((type == NULL) || (strcmp(type, "message") == 0)) ? TLEN_MESSAGE : TLEN_CHAT;
			tlen_sendmsg(user0->tlen_sesja, tid_to, body, tlen_type);
		}
        
		if (notify == 1) {
			my_debug(3, "jabber: wysylam do tlena: TLEN_NOTIFY_TYPING");
			tlen_sendnotify(user0->tlen_sesja, tid_to, TLEN_NOTIFY_TYPING);
		} else if (notify == 2) {
			my_debug(3, "jabber: wysylam do tlena: TLEN_NOTIFY_NOTTYPING");
			tlen_sendnotify(user0->tlen_sesja, tid_to, TLEN_NOTIFY_NOTTYPING);
		}
		t_free(tid_to);
	} else {
        jabber_snderror(config_myjid, from, "message", "409", "Nie jestes zalogowany", NULL);
	}
	t_free(bare_to)
	t_free(body);
}

void jabber_chatcommands_handler(tt_user *user0, char *from, char *body)
{

	xode xode_temp;
	char *char_temp = NULL;
	tx_user **item, *item0;

	if (strcmp(body, "gr") == 0) {
		jabber_sndmessage(config_myjid, from, "chat", NULL, "Importowanie rostra:", NULL);
		for (item = user0->roster; *item; item++) {
			item0 = *item;
			char_temp = jid_tid2jid(item0->tid);
			jabber_sndmessage(config_myjid, from, "chat", NULL, char_temp, NULL);
			jabber_sndpresence(char_temp, user0->jid, "subscribed", NULL, NULL);
			jabber_sndpresence(char_temp, user0->jid, "subscribe", NULL, NULL);
			t_free(char_temp);
		}
		jabber_sndmessage(config_myjid, from, "chat", NULL, "Koniec importu.", NULL);
	} else if (strcmp(body, "gp") == 0) {
		char_temp = malloc(1024);
		strcpy(char_temp, "Twoje haslo tlena to: ");
		strcat(char_temp, user0->password);
		jabber_sndmessage(config_myjid, from, "chat", NULL, char_temp, NULL);
		t_free(char_temp);
	} else if (strcmp(body, "sr") == 0) {
		char *char_temp2;
		char_temp2 = malloc(10240);
		strcpy(char_temp2, "\nWyswietlanie rostra:\n");
		for (item = user0->roster; *item; item++) {
			item0 = *item;
			char_temp = jid_tid2jid(item0->tid);
			strcat(char_temp2, char_temp);
			strcat(char_temp2, "\n");
			t_free(char_temp);
		}
		strcat(char_temp2, "Koniec rostra.");
		jabber_sndmessage(config_myjid, from, "chat", NULL, char_temp2, NULL);
		t_free(char_temp2);
	//} else if (strcmp(body, "lm") == 0) {
	} else if (strcmp(body, "sm") == 0) {
		if (user0->mailnotify == 1)
			user0->mailnotify = 0;
		else if (user0->mailnotify == 0)
			user0->mailnotify = 1;
		users_save(user0->jid);
	} else if (strcmp(body, "sn") == 0) {
		if (user0->typingnotify == 1)
			user0->typingnotify = 0;
		else if (user0->typingnotify == 0)
			user0->typingnotify = 1;
		users_save(user0->jid);
	} else if (strcmp(body, "tt") == 0) {
		//char *xode_temp;
		char_temp = malloc(1024);
		snprintf(char_temp, 1023, "\nZarejstrowanych uzytkownikow: %d\n"
			"Zalogowanych uzytkownikow: %d", 0, 0);
		jabber_sndmessage(config_myjid, from, "chat", NULL, char_temp, NULL);
		t_free(char_temp);
	//} else if (strcmp(body, "cp") == 0) {
	} else {
		my_debug(3, "jabber: commandhandler - nieznane polecenie");
		char *temp = NULL;
		char *tf;
		int n;
		tf = "\nDostepne komendy:\n"
			"gr - Wczytuje roster tlenowy do jabbera (MOZE POWODOWAC USZKODZENIA ROSTERA TLENOWEGO)\n"
			"gp - Wyswietla haslo tlenowe\n"
			"sr - Wyswietla roster tlena\n"
			//"lm - Loguje sie na serwer pocztowy i wyswietla spis poczty (NIEZAIMPLEMENTOWANE)\n"
			"sn - Wlacza/Wylacza powiadamianie u pisaniu\n"
			"sm - Wlacza/Wylacza powiadamianie o poczcie w tlen.pl\n"
			"tt - Informacje administracyjne\n"
			//"st - Statystyki (NIEZAIMPLEMENTOWANE)\n"
			//"cp - Zmien haslo (NIEZAIMPLEMENTOWANE)\n\n"
			"Jestes zarejstrowany jako: %s\n\n"
			"Powiadamianie o poczcie: %s\n"
			"Powiadamianie o pisaniu: %s\n\n"
			"Uptime transportu: %d sek.\n"
			"Wersja transportu: "PACKAGE_VERSION"\n"
			"W razie problemow prosze pisac do JID: movax@jabber.mpi.int.pl\n"
			"Aktualnosci na stronie http://sourceforge.net/projects/jtlentrans/";
		n = strlen(tf) + + strlen(user0->tid_short) +
			strlen((user0->mailnotify ? "Wlaczone" : "Wylaczone")) +
			strlen((user0->typingnotify ? "Wlaczone" : "Wylaczone")) + 12 + 1;
		my_debug(3, "jabber: commandhandler - :|");
		temp = malloc(n);
		snprintf(temp, n, tf, user0->tid_short, (user0->mailnotify ? "Wlaczone" : "Wylaczone"),
			(user0->typingnotify ? "Wlaczone" : "Wylaczone"), (time(NULL) - start_time));
		jabber_sndmessage(config_myjid, from, "chat", NULL, temp, NULL);
		t_free(temp);
	}
}

void jabber_presence_handler(xode x)
{
	char *bare_to = NULL, *to = NULL, *bare_from = NULL, *from = NULL, *type = NULL;
    char *tid = NULL;
    xode xode_temp;
	tt_user **user, *user0;
	char *show = NULL, *status = NULL, *status_iso = NULL;
	int tlen_type = 0;
    char *tid_short;

	my_debug(3, "jabber: zdazenie presence");

	if ((to = xode_get_attrib(x, "to")) == NULL) {
		my_debug(4, "jabber: Brak atrybutu to (wymagany");
		return;
	}
	if ((from = xode_get_attrib(x, "from")) == NULL) {
		my_debug(4, "jabber: Brak atrybutu from (wymagany)");
		return;
	}
	if ((type = xode_get_attrib(x, "type")) == NULL) {
		my_debug(5, "jabber: Blad w atrybucie type (opcjonalny)");
	}


	my_debug(2, "jabber: Otrzymalem info o statusie '%s' do '%s' (type=%s)", from, to, type);

	bare_to = jid_jidr2jid(to);
	bare_from = jid_jidr2jid(from);
	user = jid_jid2user(bare_from);
	t_free(bare_from);

	if (!user) {
		my_debug(0, "jabber: User nie jest zarejstrowanym uzytkownikiem");
        jabber_snderror(to, from, "presence", "406", "Nie jestes zarejestrowany", NULL);
		return;
	}

	if (!*user) {
        jabber_snderror(to, from, "presence", "406", "Nie jestes zarejestrowany", NULL);
		return;
	}

	user0 = *user;
	if (user0) {
		if (strcmp(bare_to, config_myjid) == 0) {
			if ((xode_temp = xode_get_tag(x, "show")) == NULL) {
				my_debug(5, "jabber: Brak tagu show (opcjonalny)");
			} else {
				if ((show = xode_get_data(xode_temp)) == NULL) {
					my_debug(5, "jabber: Blad w danych show");
					return;
				}
			}

			if ((xode_temp = xode_get_tag(x, "status")) == NULL) {
				my_debug(5, "jabber: Brak tagu status (opcjonalny)");
			} else {
				if ((status = xode_get_data(xode_temp)) == NULL) {
					my_debug(5, "jabber: Blad w danych status (opcjonalny)");
					return;
				} else {
					if ((status_iso = unicode_utf8d_2_iso88592e((const char *)status)) == NULL) {
						my_debug(5, "jabber: Blad w konwersji status");
						return;
					}
				}
			}
			if (type != NULL) {
				if (strcmp(type, "unavailable") == 0) {
					if (user0->tlen_loged == 2) {
						my_debug(2, "jabber: Wylogowywanie usera: %s, o jid: %s", user0->tid, user0->jid);
						tlen_logout_user(user0, status_iso);
					} else {
						jabber_sndpresence(to, user0->jid, "unavailable", NULL, NULL);
					}
				} else if (strcmp(type, "subscribe") == 0) {
					jabber_sndpresence(to, from, "subscribed", NULL, NULL);
					jabber_sndpresence(to, from, "subscribe", NULL, NULL);
                }
			} else {
				user0->status_type = status_jabber_to_tlen(show);
				my_strcpy(user0->status, status_iso); /* FIXIT, sprawdzic pamiec */
				my_strcpy(user0->jabber_status, status);
				my_strcpy(user0->jabber_status_type, show);
				if (user0->tlen_loged == 0) {
					my_debug(2, "jabber: laczenie z tlenem dla usera: %s, o jid: %s", user0->tid, user0->jid);
					tlen_login_user(user0);
				} else if (user0->tlen_loged == 2) { /* FIXIT */
					my_debug(2, "k");
					tlen_presence_user(user0);
				}
			}
			t_free(status_iso);
		} else {
            tid = jid_jid2tid(bare_to);
			if (type == NULL) {}
			else if (strcmp(type, "subscribe") == 0) {
				// dodalismy usera do rostera i prosimy o mozliwosc ogladania jego statusu
				my_debug(3, "jabber: subscribe");
				if (user0->tlen_loged == 2) {
					my_debug(1, "jabber: Wysylam request subscribe do %s", tid);
					tid_short = jid_jid2tid_short(tid);
					tlen_addcontact(user0->tlen_sesja, tid_short, tid, "TlenTransport");
					t_free(tid_short);
					tlen_request_subscribe(user0->tlen_sesja, tid);
				}
			} else if (strcmp(type, "subscribed") == 0) {
				// zgodzilismy sie na ogladanie naszego statusu przez kogos
				my_debug(3, "jabber: subscribed");
				if (user0->tlen_loged == 2) {
					my_debug(1, "jabber: Wysylam accept subscribe do %s", tid);
					tid_short = jid_jid2tid_short(tid);
					tlen_addcontact(user0->tlen_sesja, tid_short, tid, "TlenTransport");
					t_free(tid_short);
					tlen_accept_subscribe(user0->tlen_sesja, tid);
				}
			} else if (strcmp(type, "unsubscribe") == 0) {
				// prosimy kogos o usuniecie nas z listy
				my_debug(3, "jabber: unsubscribe");
				if (user0->tlen_loged == 2) {
					my_debug(1, "jabber: Wysylam request unsubscribe do %s", tid);
					tlen_request_unsubscribe(user0->tlen_sesja, tid);
					//tlen_removecontact(user0->tlen_sesja, to0); //hmmm?
				}
			} else if (strcmp(type, "unsubscribed") == 0) {
				// jakis user zostaje wyrzucony z listy..
				my_debug(3, "jabber: unsubscribed");
				if (user0->tlen_loged == 2) {
					my_debug(1, "jabber: Wysylam accept unsubscribe do %s", tid);
					tlen_accept_unsubscribe(user0->tlen_sesja, tid);
					tlen_removecontact(user0->tlen_sesja, tid);
				}
			} else if (strcmp(type, "probe") == 0) {
				my_debug(3, "jabber: Sprawdzam status %s", tid);
				/* TODO */
			} 
            t_free(tid);
		}
	}
}

void jabber_iq_handler(xode x)
{
	char *id = NULL, *from = NULL; //, *type = NULL;
	xode query;
	tt_user **user, *user0;
	char *xmlns = NULL, *bare_from = NULL;
	char *to = NULL;
	xode xode_temp;
	char *char_temp = NULL;//, char_temp;
	char *to0 = NULL, *to1 = NULL;
	int retval;
	enum {set, get, unknown} type = unknown;

	my_debug(3, "jabber: zdazenie iq");

	if ((id = xode_get_attrib(x, "id")) == NULL) {
		my_debug(5, "jabber: Blad w atrybucie id (opcjonalny)");
	}
	if ((from = xode_get_attrib(x, "from")) == NULL) {
		my_debug(5, "jabber: Blad w atrybucie from (wymagany)");
		return;
	} else {
		bare_from = jid_jidr2jid(from);
		user = jid_jid2user(bare_from);
		t_free(bare_from);
	}
	if ((char_temp = xode_get_attrib(x, "type")) == NULL) {
		my_debug(5, "jabber: Blad w atrybucie type (wymagany)");
		return;
	}
	if ((to = xode_get_attrib(x, "to")) == NULL) {
		my_debug(5, "jabber: Blad w atrybucie to");
		return;
	}
	if (strcmp(char_temp, "get") == 0) {
		type = get;
	} else if (strcmp(char_temp, "set") == 0) {
		type = set;
	}


	if ((query = xode_get_tag(x, "query")) == NULL) {
	// sytuacja gdy iq nie zawiera query... blad lub zapytanie o vCarda
		my_debug(5, "jabber: Blad w tagu query");
		if ((query = xode_get_tag(x, "vCard")) == NULL) {
			my_debug(5, "jabber: Blad w tagu vCard");
			//jabber_sndiq(to, from, "error", id, "<error code='501'>Not implemented</error>");
            jabber_snderror(to, from, "iq", "501", "Not implemented", id);
			return;
		}
		//jabber_sndiq(config_myjid, from, "error", id, "<error code='501'>Not implemented</error>");
        jabber_snderror(to, from, "iq", "501", "Not implemented", id);
		return;
		{
/*
		// FIXME: to trza przepisac innaczej.
		bare_from = jid_jidr2jid(from);
		user = jid_jid2user(bare_from);
		t_free(bare_from);
		if (user) {
			user0 = *user;
			if (user0) {
				if (user0->tlen_loged == 2) {
					struct tlen_pubdir *search;
					my_strcpy(user0->search_id, "vCardsearch");
					my_strcpy(user0->search_jid, from);
					if ((body = xode_get_attrib(query, "xmlns")) == NULL) {
						my_debug(5, "jabber: Blad w atrybucie xmlns");
						return;
					}
					if (strcmp(body, "vcard-temp") != 0) {
						my_debug(5, "jabber: Blad w atrybucie xmlns2");
						return;
					}
					if ((to = xode_get_attrib(x, "to")) == NULL) {
						my_debug(5, "jabber: Blad w atrybucie to");
						return;
					}
					to0 = jid_jidr2jid(to);
					to1 = jid_jid2tid(to0);

					search = tlen_new_pubdir();
					search->id = to1;
					tlen_search(user0->tlen_sesja, search);
					tlen_free_pubdir(search);
					t_free(to0);
				}
			}
		}
*/
		}
	}

	if ((xmlns = xode_get_attrib(query, "xmlns")) == NULL) {
		my_debug(5, "jabber: Blad w atrybucie xmlns (wymagany)");
		return;
	}

	if (strcmp(xmlns, "jabber:iq:register") == 0 && type == get) {
		jabber_sndregister_form(id, from);
	} else if (strcmp(xmlns, "jabber:iq:register") == 0 && type == set) {
		char *username, *password;
		/*
		<iq id='reg2' type='set'><query xmlns='jabber:iq:register'>
		<username>tlenuser</username>
		<password>secret</password>
		</query></iq>
		*/
		if ((xode_temp = xode_get_tag(query, "remove")) != NULL) {
			my_debug(5, "jabber: Tag remove");
			retval = jabber_unregister(from);
			switch (retval) {
				case 0:
					jabber_sndiq(config_myjid, from, "result", id, NULL);
					break;
				case 1:
                    jabber_snderror(to, from, "iq", "406", "Not registred", id);
					break;
			}
		} else {
			my_debug(5, "jabber: Blad w tagu remove (alternatywny)");
			if ((xode_temp = xode_get_tag(query, "username")) == NULL) {
				my_debug(3, "jabber: Blad w tagu username (wymagany)");
				return;
			}
			username = xode_get_data(xode_temp);
			if ((xode_temp = xode_get_tag(query, "password")) == NULL) {
				my_debug(3, "jabber: Blad w tagu password (wymagany)");
				return;
			}
			password = xode_get_data(xode_temp);
			if (username != NULL && password != NULL && strlen(username) > 0) {
				retval = jabber_register(from, username, password);
				switch (retval) {
					case 0:
						jabber_sndiq(to, from, "result", id, NULL);
						jabber_sndpresence(config_myjid, from, "subscribed", NULL, NULL);
						jabber_sndpresence(config_myjid, from, "subscribe", NULL, NULL);
						break;
					case 1:
                        jabber_snderror(to, from, "iq", "409", "Already registred", id);
						break;
					case 3:
                        jabber_snderror(to, from, "iq", "401", "Unauthorized", id);
						break;
					case 2:
                        jabber_snderror(to, from, "iq", "409", "Username Not Avaible", id);
						break;
					case 4:
						break;
						/* <error code='500'>Password Storage Failed</error></iq> */
				}
			} else if ((username != NULL && password != NULL && strlen(username) == 0 && strlen(password) == 0) || (username == NULL && password == NULL)) {
				retval = jabber_unregister(from);
				switch (retval) {
					case 0:
						jabber_sndiq(to, from, "result", id, NULL);
						break;
					case 1:
                        jabber_snderror(to, from, "iq", "401", "Not registred", id);
						break;
				}
			} else {
				my_debug(3, "jabber: Bledny tid lub/i haslo w czasie rejstracji");
                jabber_snderror(to, from, "iq", "406", "Bad syntax of username and/or password", id);
			}
		}
	} else if (strcmp(xmlns, "jabber:iq:version") == 0 && type == get) {
		if ((to = xode_get_attrib(x, "to")) == NULL) {
			my_debug(4, "Blad w atrybucie to (wymagany)");
			return;
		}
		to0 = jid_jid2tid(to);
		jabber_sndversion_result(id, to, from);
	} else if (strcmp(xmlns, "jabber:iq:gateway") == 0 && type == get) {
		jabber_sndgate_form(id, from);
	} else if (strcmp(xmlns, "jabber:iq:gateway") == 0 && type == set) {
		char *prompt;
		if ((xode_temp = xode_get_tag(query, "prompt")) == NULL) {
			my_debug(3, "jabber: Blad w tagu prompt");
		}
		if ((prompt = xode_get_data(xode_temp)) == NULL) {
			my_debug(3, "jabber: Blad w danych prompt");
		}
		if (prompt != NULL && strlen(prompt) > 0) {
			jabber_sndgate_result(id, from, prompt);
		}
		/* jabber_sndiq(config_myjid, from, "error", id, "<error code='406'>Bad syntax of username</error>");
		*/
	} else if (strcmp(xmlns, "jabber:iq:search") == 0 && type == get) {
		bare_from = jid_jidr2jid(from);
		user = jid_jid2user(bare_from);
		t_free(bare_from);
		if (user) {
			user0 = *user;
			if (user0) {
				if (user0->tlen_loged == 2) {
					jabber_sndsearch_form(id, from);
				} else {
				    //jabber_sndiq(config_myjid, from, "error", id, "<error code='409'>Not loged</error>");
                    jabber_snderror(to, from, "iq", "409", "Not logged", id);
				}
			}
		} else {
			my_debug(0, "jabber: User nie jest zarejstrowanym uzytkownikiem");
			//jabber_sndiq(config_myjid, from, "error", id, "<error code='409'>Not registred</error>");
            jabber_snderror(to, from, "iq", "409", "Not registred", id);
		}
	} else if (strcmp(xmlns, "http://jabber.org/protocol/disco#info") == 0 && type == get) {
		jabber_snddisco(id, from);
	} else if (strcmp(xmlns, "jabber:iq:search") == 0 && type == set) {
        jabber_iq_search_set(user, from, to, id, query);
	} else {
		my_debug(3, "jabber: Nieznane polecenie iq!");
		//jabber_sndiq(to, from, "error", id, "<error code='501'>Not implemented</error>");
        jabber_snderror(to, from, "iq", "501", "Not implemented", id);
	}
}


char *get_field(xode query, const char *name)
{
    xode xode_temp;
    char *char_temp = NULL;
	if ((xode_temp = xode_get_tag(query, "nick")) == NULL) {
		my_debug(5, "jabber: Blad w tagu nick (opcjonalny)");
        return NULL;
	}
	char_temp = xode_get_data(xode_temp);
    my_debug(4, "jabber: %s = %s", name, char_temp);
	return unicode_utf8d_2_iso88592e(char_temp);
}

char *get_x_field(xode xdata, const char *name)
{
    xode xdata1, xdata2;
    char *temp1 = NULL, *temp2 = NULL;

    my_strcat(temp1, "field?var=", name);
	if ((xdata1 = xode_get_tag(xdata, temp1)) != NULL) {
		if ((xdata2 = xode_get_tag(xdata1, "value")) != NULL) {
			temp2 = xode_get_data(xdata2);
            t_free(temp1);
            my_debug(4, "jabber: %s = %s", name, temp2);
			return unicode_utf8d_2_iso88592e(temp2);
		}
	}
    t_free(temp1);
    return NULL;
}

void jabber_snderror(const char *from, const char *to, const char *type, const char *code, const char *msg, const char *id) {

    xode packet, error;
	char *from_res = NULL;

	if (from == NULL || to == NULL) {
		return;
		my_debug(0, "jabber: Niepoprawne parametry");
	}

	my_debug(2, "jabber: Wysylam info o bledzie '%s' od '%s' do '%s' (code=%s, msg=%s)", type, from, to, code, msg);

	packet = xode_new(type);
    xode_put_attrib(packet, "from", from);
	xode_put_attrib(packet, "to", to);
	xode_put_attrib(packet, "type", "error");
	
	if (id) {
		xode_put_attrib(packet, "id", id);
	}
	
    error = xode_insert_tag(packet, "error"); 
    xode_put_attrib(error, "code", code);
    xode_insert_cdata(error, msg, -1);            

	my_strcpy(js_buf2, xode_to_str(packet));

	jabber_send(js_buf2);
	my_debug(5, "jabber: Zapytanie: %s", js_buf2);

	t_free(js_buf2);
	t_free(from_res);
	xode_free(packet);

	return;
}


/* JEP-0114 */

int jabber_ping() {
	return 0;
}
