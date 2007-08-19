#include "jabber.h"
#include "config.h"
#include "debug.h"

/*
    <iq from=... to=... type= [id=...]/>
typy:
get
set
result
error

*/
int jabber_sndiq(const char *from, const char *to, const char *type, const char *id, const char *ex) {
	xode packet;

	my_debug(2, "jabber: Wysylam iq");

	if (from  == NULL || to == NULL || type == NULL || id == NULL) {
	    my_debug(0, "jabber: Niepoprawne parametry");
	    return -1;

	}

	packet = xode_new("iq");
	xode_put_attrib(packet, "from", from);
	xode_put_attrib(packet, "to", to);
	xode_put_attrib(packet, "type", type);	
	xode_put_attrib(packet, "id", id);

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
