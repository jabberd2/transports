#include "jabber.h"
#include "config.h"
#include "debug.h"

/*
    <message from=... to=... [id=...] [type=...] [lang=...]>
	[ <subject [lang=...]> ... </subject> ] (0-inf)
	[ <body [lang=...]> ... </body> ] (0-inf)
	[ <error type=... [code=...]> </error> ] (0-1)
	[ <thread> </thread> ]
    </message>

typy:

chat
error
groupchat
headline
normal

error typy:
cancel
auth
wait
continue
modify
*/

int jabber_sndmessage(const char *from, const char *to, const char *type, const char *subject, const char *body, const char *ex) {
	xode packet;

	if (from == NULL || to == NULL) {
		return -1;
		my_debug(0, "jabber: Niepoprawne parametry");
	}

	my_debug(2, "jabber: Wiadomosc od '%s' do '%s' o tresci: '%s'", from, to, body);

	packet = xode_new("message");
	xode_put_attrib(packet, "from", from);
	xode_put_attrib(packet, "to", to);
	
	if (type) {
		xode_put_attrib(packet, "type", type);
	}

	if (subject) {
		xode temp = xode_insert_tag(packet, "subject");
		xode_insert_cdata(temp, subject, -1);
	}

	if (body) {
		xode temp = xode_insert_tag(packet, "body");
		xode_insert_cdata(temp, body, -1);
	}

	if (ex) {
		xode temp = xode_from_str((char*)ex, -1);
		xode_insert_node(packet, temp);
		xode_free(temp);
	}

	my_strcpy(js_buf2, xode_to_str(packet));

	jabber_send(js_buf2);
	my_debug(5, "jabber: Zapytanie: %s", js_buf2);

	t_free(js_buf2);
	xode_free(packet);

	return 0;
}
