#include "jabber.h"
#include "config.h"
#include "debug.h"
#include <sys/utsname.h>

int jabber_sndversion_result(const char *id, const char *from, const char *to) {
	struct utsname un;
	xode packet, query, qname, qver, qos;

	if (to == NULL || from == NULL || id == NULL)
		return -1;

	my_debug(2, "jabber: Wysylam wyniki zapytania version do %s, id %s", to, id);

	packet = xode_new("iq");
	xode_put_attrib(packet, "from", from);
	xode_put_attrib(packet, "to", to);
	xode_put_attrib(packet, "type", "result");
	xode_put_attrib(packet, "id", id);
	
	query = xode_insert_tag(packet, "query");
	xode_put_attrib(query, "xmlns", "jabber:iq:version");
	
	qname = xode_insert_tag(query, "name");
	xode_insert_cdata(qname, "Tlen Transport", -1);
	
	qver = xode_insert_tag(query, "version");
	xode_insert_cdata(qver, PACKAGE_VERSION, -1);
	
	uname(&un);
	qos = xode_insert_tag(query, "os");
	xode_insert_cdata(qos, un.sysname, -1);
	
	my_strcpy(js_buf2, xode_to_str(packet));

	jabber_send(js_buf2);
	my_debug(5, "jabber: Zapytanie: %s", js_buf2);

	t_free(js_buf2);
	xode_free(packet);

	return 0;
}
