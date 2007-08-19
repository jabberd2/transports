#include "jabber.h"
#include "config.h"
#include "debug.h"
#include "users.h"

int jabber_close() {
	my_debug(0, "jabber: Zakonczenie transmisji!");
	close(jabber_server);
	my_debug(0, "jabber: Polaczenie zamkniete!");

	close(jabber_gniazdo);
	my_debug(0, "jabber: Gniazdo zamkniete!");
	return 0;
}

int jabber_logout() {
	tt_user **user, *user0;
	/* Robimy sie niedostepni */
	for (user = tt_users; *user; user++) {
		user0 = *user;
		if (user0->tlen_loged) {
			/* jabber_sndpresence(config_myjid, user0->jid, NULL, "unavailable", NULL); */
			jabber_sndpresence(config_myjid, user0->jid, "unavailable", NULL, NULL);
		}
	}

	my_debug(2, "jabber: Zamkniecie stream!");
	jabber_send("</stream:stream>\n");
	jabber_loged = 0;
	t_free(js_buf);
	t_free(sid);
	return 0;
}
