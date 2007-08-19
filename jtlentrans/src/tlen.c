/* Komunikacja z serwerem tlena jako client (zmodyfikowany jabber) */
#include <libtlen/libtlen.h>

#include <stdio.h> /* fflush, perror */
#include <sys/select.h> /* selct */
#include <time.h> /* time */
#include <errno.h> /* perror */

#include "debug.h"

#include "config.h"
#include "tlen.h"
#include "jabber.h"
#include "jid.h"
#include "encoding.h"
#include "users.h"

#include "all.h"

int tlen_login_user(tt_user *user) {
	pid_t mypid;
	int last_ping_time;

	mypid = getpid();
	last_ping_time = time(NULL);

	tlen_setdebug(config_tlendebug);

	/* sprawdzamy czy nie trwa logowanie */
	if (user->tlen_loged != 0)
		return -1;

	my_debug(1, "tlen: Inicjuje logowanie %s...", user->jid);

	user->tlen_loged = 1;

	/* inicjujemy struktury */
	user->tlen_sesja = tlen_init();
	/* ustawiamy autoryzacje */
	tlen_set_auth(user->tlen_sesja, user->tid_short, user->password);
	/* nie blokowwane polaczenie */
	tlen_set_hub_blocking(user->tlen_sesja, 0);

	/* laczymy i logujemy sie... */
	tlen_login(user->tlen_sesja);
	return 0;
}

int tlen_presence_user(tt_user *user) {
	tlen_presence(user->tlen_sesja, user->status_type, user->status);
	jabber_sndpresence(config_myjid, user->jid, NULL, user->jabber_status_type, user->jabber_status);
	return 0;
}

int tlen_handler(tt_user *user) {
	struct tlen_event *event = NULL;
	char *from = NULL, *to = NULL;
	char *temp = NULL, *body = NULL;

	my_debug(3, "tlen: Obsluga zdazen %s", user->jid);

	if (user->tlen_sesja == NULL) {
		my_debug(0, "tlen: Bledny parametr user");
		return -1;
	}

	tlen_watch_fd(user->tlen_sesja);
	if ((user->tlen_sesja)->error) {
		my_debug(0, "tlen: Blad tlen %d", (user->tlen_sesja)->error);
		switch ((user->tlen_sesja)->error) {
			case TLEN_ERROR_UNAUTHORIZED:
				my_debug(0, "tlen: UNAUTHORIZED", (user->tlen_sesja)->error);
				break;
			case TLEN_ERROR_MALLOC:
				my_debug(0, "tlen: MALLOC", (user->tlen_sesja)->error);
				break;
			case TLEN_ERROR_OTHER:
				my_debug(0, "tlen: OTHER", (user->tlen_sesja)->error);
				break;
			case TLEN_ERROR_NETWORK: /* Czy aby nie spowoduje to jakichs niechcianych zachowan? */
				my_debug(0, "tlen: NETWORK", (user->tlen_sesja)->error);
				tlen_logout_user(user, NULL);
				my_debug(0, "tlen: sprobuje zalogowac");
				tlen_login_user(user);
				return 1;
				break;
			default:
				my_debug(0, "tlen: NIEZNANY BLAD", (user->tlen_sesja)->error);
				break;
		}
		tlen_logout_user(user, NULL);
		user->tlen_loged = 0;
		/* jabber_sndmessage(config_myjid, user->jid, "error", NULL, "Blad w transporcie (blad autoryzacji, lub blad programu)");
		*/
		return 1;
	}

	/* i nam o tym powiedzial w eventach, sprawdzamy po koleji */
	while ((event = tlen_getevent(user->tlen_sesja)) != NULL) {
		my_debug(3, "tlen: Zdazenie %d", event->type);
		switch (event->type) {
			/*
			case TLEN_EVENT_NOWGETID:
				tlen_getid(user->tlen_sesja);
				break;
			case TLEN_EVENT_GOTID:
				tlen_authorize(user->tlen_sesja);
				break;
			*/
			case TLEN_EVENT_AUTHORIZED:
				my_debug(4, "tlen: Zalogowalismy wiec pobieramy roster");
				tlen_getroster(user->tlen_sesja);
				break;
			/*
			case TLEN_EVENT_UNAUTHORIZED:
				my_debug(4, "tlen: Nie udalo sie zalogowac, sio");
				user->tlen_loged = 0;
				break;
			*/
			case TLEN_EVENT_GOTROSTERITEM:
				my_debug(4, "tlen: Got user: %s", event->roster->jid);
				{
				tx_user *tx;
				int i;
                my_debug(1, "tlen: otrzymalismy usera (jid=%s, subscription=%s, ask=%s)", event->roster->jid, event->roster->subscription, event->roster->ask);
				tx = malloc(sizeof(tx_user));
				my_strcpy(tx->tid, event->roster->jid);
				my_strcpy(tx->status, ""); /* FIXIT */
                if (strcmp(event->roster->subscription, "both") == 0) {
                    tx->subs = sub_both;
                } else if (strcmp(event->roster->subscription, "to") == 0) {
                    tx->subs = sub_to;
                } else if (strcmp(event->roster->subscription, "from") == 0) {
                    tx->subs = sub_from;
                } else {
                    tx->subs = sub_none;
                }
                tx->ask = event->roster->ask != NULL;
				i = users_sizet(user->roster);
				user->roster = realloc(user->roster, (i+2)*sizeof(tx_user*));
				(user->roster)[i] = tx;
				(user->roster)[i+1] = NULL;
				}
				break;
			case TLEN_EVENT_ENDROSTER:
				my_debug(4, "tlen: Otrzymali¶my ca³y roster");
				/* FIXIT: zrobic presence_user */
				tlen_presence(user->tlen_sesja, user->status_type, user->status);
				user->tlen_loged = 2;
				jabber_sndpresence(config_myjid, user->jid, NULL, user->jabber_status_type, user->jabber_status);
				break;
			case TLEN_EVENT_SUBSCRIBE:
				my_debug(1, "tlen: User %s chce nas dodac do swojego rostra, przekazujemy do jabbera", event->subscribe->jid);
				from = jid_tid2jid(event->subscribe->jid);
				jabber_sndpresence(from, user->jid, "subscribe", NULL, NULL);
				t_free(from);
				break;
			case TLEN_EVENT_SUBSCRIBED:
				my_debug(1, "tlen: User %s pozwolil nam dodac siebie do rostra, wiec dodajemy", event->subscribe->jid);
				/* Dodajemy w Jabberze */
				from = jid_tid2jid(event->subscribe->jid);
				jabber_sndpresence(from, user->jid, "subscribed", NULL, NULL);
				t_free(from);
                //from = jid_jid2tid_short(event->subscribe->jid);
                //tlen_addcontact(user->tlen_sesja, from, event->subscribe->jid, "TlenTransport");
                //t_free(from);
				/* Dodajemy w Tlenie */
				/* tlen_addcontact(user->tlen_sesja, event->subscribe->jid, event->subscribe->jid, "grupa");
				*/
				break;
			case TLEN_EVENT_UNSUBSCRIBE:
				my_debug(1, "tlen: User %s usunal nas z rostra, wiec pozwalamy mu na usuniecie siebie, i prosimy o pozwolenie usuniecia go ze swojego", event->subscribe->jid);
                //tlen_removecontact(user->tlen_sesja, event->subscribe->jid);
				from = jid_tid2jid(event->subscribe->jid);
                jabber_sndpresence(from, user->jid, "unsubscribe", NULL, NULL);
				//jabber_sndpresence(from, user->jid, "unsubscribe", NULL, NULL);
				t_free(from);
				break;
			case TLEN_EVENT_UNSUBSCRIBED:
				my_debug(1, "tlen: User %s pozwolil/kaza³ siebie usunac z naszego rostra, wiec usuwamy", event->subscribe->jid);
                //tlen_removecontact(user->tlen_sesja, event->subscribe->jid);
				from = jid_tid2jid(event->subscribe->jid);
				jabber_sndpresence(from, user->jid, "unsubscribed", NULL, NULL);
                //jabber_sndpresence(from, user->jid, "unavailable", NULL, NULL);
				t_free(from);				
				break;
			case TLEN_EVENT_PRESENCE:
				my_debug(4, "tlen: User %s zmieni³ stan na '%d' z opisem '%s'", event->presence->from, event->presence->status, event->presence->description);
				{
				char *show = NULL, *typ = NULL, *status = NULL;
				/* '2' - 'Dostpny'
				   '3' z opisem 'Wroce pozniej
				   '4' z opisem 'Zaraz wracam'
				   '5' z opisem 'Jestem zajty'
				   '6' z opisem 'Porozmawiajmy'
				   '8' z opisem '(null)'
				*/
				switch (event->presence->status) {
				/*	case TLEN_STATUS_AVAILABLE: presence = "available"; break; 
	    				case TLEN_STATUS_AWAY: presence = "away"; break; 
	    				case TLEN_STATUS_CHATTY: presence = "chat"; break; 
	    				case TLEN_STATUS_EXT_AWAY: presence = "xa"; break; 
	    				case TLEN_STATUS_DND: presence = "dnd"; break; 
	    				case TLEN_STATUS_INVISIBLE: tlen_presence_invisible(sesja); return 1; break; 
	    				case TLEN_STATUS_UNAVAILABLE: tlen_presence_disconnect(sesja); return 1; break; 
	    				default: presence = "available"; break;
				*/
					case 1:
						break; /* FIXIT: ? */
					case 2:
						show = NULL; break;
					case 3:
						show = "xa"; break;
					case 4:
						show = "away"; break;
					case 5:
						show = "dnd"; break;
					case 6:
						show = "chat"; break;
					case 7:
						break; /* FIXIT: rozlaczony? */
					case 8:
						typ = "unavailable"; break;
					default:
						break;
				}
				from = jid_tid2jid(event->presence->from);
				status = unicode_iso88592_2_utf8(event->presence->description);
				jabber_sndpresence(from, user->jid, typ, show, status);
				t_free(from);
				t_free(status);
				}
				break;
			case TLEN_EVENT_MESSAGE:
				my_debug(4, "tlen: Wiadomosc od '%s' o tresci: '%s'", event->message->from, event->message->body);
				{
				char *typ = NULL;
				switch (event->message->type) {
					case TLEN_MESSAGE:
						typ = "normal";
						break;
					case TLEN_CHAT:
						typ = "chat";
						break;
					default:
						typ = "chat";
						break;
				}
				body = unicode_iso88592_2_utf8(event->message->body);
				from = jid_tid2jid(event->message->from);
				jabber_sndmessage(from, user->jid, typ, NULL, body, NULL);
				t_free(body);
				t_free(from);
				}
				break;
			case TLEN_EVENT_NEWMAIL:
				if (user->mailnotify) {
				my_debug(4, "tlen: Masz now± pocztê od %s o temacie '%s'!\n", event->newmail->from, event->newmail->subject);
                
				temp = malloc(strlen("Masz now± pocztê na koncie @tlen.pl od  o temacie ''!") + strlen(user->tlen_sesja->username) + strlen(event->newmail->from) + strlen(event->newmail->subject) + 1);
				sprintf(temp, "Masz now± pocztê na koncie %s@tlen.pl od %s o temacie '%s'!", user->tlen_sesja->username, event->newmail->from, event->newmail->subject);
				body = unicode_iso88592_2_utf8(temp);
				t_free(temp);
                
                char *subject;
				temp = malloc(strlen("Masz now± pocztê!") + 1);
				sprintf(temp, "Masz now± pocztê!");
                subject = unicode_iso88592_2_utf8(temp);
                t_free(temp);
                
				jabber_sndmessage(config_myjid, user->jid, "message", subject, body, "<x xmlns='jabber:x:oob'><url>http://poczta.o2.pl/</url><desc>WebMail</desc></x>");
				t_free(subject);
                t_free(body);
				}
				break;
			case TLEN_EVENT_GOTSEARCHITEM:
				my_debug(4, "tlen: Element wyniku wyszukiwania");
				{
				tx_search *tx;
				int i;
				tx = malloc(sizeof(tx_search));
				memset(tx, 0, sizeof(tx_search));
				my_strcpy(tx->tid, event->pubdir->id);
				my_strcpy(tx->firstname, event->pubdir->firstname);
				my_strcpy(tx->lastname, event->pubdir->lastname);
				my_strcpy(tx->city, event->pubdir->city);
				my_strcpy(tx->email, event->pubdir->email);
				tx->age = NULL;
				i = users_sizes(user->search);
				user->search = realloc(user->search, (i+2)*sizeof(tx_search*));
				(user->search)[i] = tx;
				(user->search)[i+1] = NULL;
				}
/*                if (strcmp(user->search_id, "vCardsearch") == 0) {
                    jabber_sndvCard_result(user->search_jid, (const tx_search**)user->search);
                }*/
				break;
			case TLEN_EVENT_ENDSEARCH:
				my_debug(4, "tlen: Wyszukiwanie sie skonczylo");
				{
				tx_search **item, *item0;
				if (strcmp(user->search_id, "vCardsearch") == 0) {
//					jabber_sndvCard_result(user->search_jid, (const tx_search**)user->search);
				} else {
					jabber_sndsearch_result(user->search_id, user->search_jid, (const tx_search**)user->search);
				}
				if (user->search) {
					for (item = user->search; *item; item++) {
						item0 = *item;
						t_free(item0->tid);
						t_free(item0->age);
						t_free(item0->city);
						t_free(item0->firstname);
						t_free(item0->lastname);
						t_free(item0->email);
						t_free(item0);
					}
					t_free(user->search);
					user->search = NULL;
				}
				}
				break;
			case TLEN_EVENT_PUBDIR_GOTDATA:
				my_debug(4, "tlen: Pobralismy swoje dane");
				break;
			case TLEN_EVENT_PUBDIR_GOTUPDATEOK:
				my_debug(4, "tlen: Nasze dane zaktualizowane");
				break;
			case TLEN_EVENT_ADVERT:
				my_debug(4, "tlen: Otrzyma³em od %s reklamê o adresie %s i wymiarach %d na %d pikseli\n", event->advert->name, event->advert->url, event->advert->width, event->advert->height);
				break;
			case TLEN_EVENT_NOTIFY:
				if (user->typingnotify) {
				my_debug(4, "tlen: Obsluga powiadomien od %s", event->notify->from);
				switch (event->notify->type) {
					case TLEN_NOTIFY_TYPING:
						my_debug(4, "tlen: Powiadomienie o pisaniu");
						from = jid_tid2jid(event->notify->from);
						jabber_sndevent(NULL, from, user->jid, "<composing/>");
						t_free(from);
						break;
					case TLEN_NOTIFY_NOTTYPING:
						my_debug(4, "tlen: Powiadomienie o zaprzestaniu pisania (od 30s)");
						from = jid_tid2jid(event->notify->from);
						jabber_sndevent(NULL, from, user->jid, "");
						t_free(from);
						break;
					case TLEN_NOTIFY_SOUNDALERT:
						my_debug(4, "tlen: Powiadomienie o alarmie od usera");
						{
						char *temp = NULL, *body = NULL;
						temp = malloc(strlen("User %s wysyla ci alarm!") + strlen(event->notify->from) + 1);
						sprintf(temp, "User %s wysyla ci alarm!", event->notify->from);
						body = unicode_iso88592_2_utf8(temp);
						t_free(temp);
						jabber_sndmessage(config_myjid, user->jid, "message", NULL, body, NULL);
						t_free(body);
						}
						break;
					default:
						break;
				}
				}
				break;
			/* wiadomosc od msg.tlen.pl; np. z ludzie.tlen.pl */
			case TLEN_EVENT_WEBMSG: 
				if (event->webmsg) {
					char *temp = NULL, *body = NULL, *subject = NULL;
					temp = malloc(strlen("Wiadomo¶æ ze strony %s") + strlen(event->webmsg->site) + 1);
					sprintf(temp, "Wiadomo¶æ ze strony %s", event->webmsg->site);
					subject = unicode_iso88592_2_utf8(temp);
					t_free(temp);

					/* FIXME tutaj sie gdzies robia &amp;gt; i &amp;lt; - juz mi sie nie chcialo sledzic */
					temp = malloc(strlen("U¿ytkownik %s <%s> przysy³a Ci wiadomo¶æ ze strony %s:\n-----\n\n%s") + strlen(event->webmsg->from) + strlen(event->webmsg->email) + strlen(event->webmsg->site) + strlen(event->webmsg->body) +1);
					sprintf(temp, "U¿ytkownik %s <%s> przysy³a Ci wiadomo¶æ ze strony %s:\n-----\n\n%s", event->webmsg->from, event->webmsg->email, event->webmsg->site, event->webmsg->body);
					body = unicode_iso88592_2_utf8(temp);
					t_free(temp);
					jabber_sndmessage(config_myjid, user->jid, "normal", subject, body, NULL);
					t_free(subject);
					t_free(body);
				}
				break;
			default:
				my_debug(3, "Nieznany event %d", event->type);
				break;
		} /* switch zdazenie */
		/* zwolnij pamiec zwiazana ze zdaezniem */
		tlen_freeevent(event);
	} /* while zdazenia */
	my_debug(3, "tlen: Koniec obslugi zdazen %s", user->jid);
	return 0;
}

int tlen_logout_user(tt_user *user, const char *status) {
	char *temp;
	tx_user **tuser, *tuser0;

	if (user == NULL)
		return -1;

	if (user->tlen_sesja == NULL || user->tlen_loged == 0) {
		/* jabber_sndpresence(config_myjid, user->jid, "unavailable", NULL, NULL);
		*/
		return -1;
	}

	my_debug(1, "tlen: Wylogowywuje...");
	/* tlen_presence(user->tlen_sesja, TLEN_STATUS_UNAVAILABLE, ""); */
	if (user->roster) {
		for (tuser = user->roster; *tuser; tuser++) {
			tuser0 = *tuser;
			temp = jid_tid2jid(tuser0->tid);
			jabber_sndpresence(temp, user->jid, "unavailable", NULL, NULL);
			t_free(temp);
			t_free(tuser0->tid);
			t_free(tuser0->status);
			t_free(tuser0);
		}
		t_free(user->roster);
	}
	tlen_presence_disconnect(user->tlen_sesja);
	tlen_freesession(user->tlen_sesja);
	jabber_sndpresence(config_myjid, user->jid, "unavailable", NULL, NULL);
	user->tlen_loged = 0;
	user->tlen_sesja = NULL;
	return 0;
}
