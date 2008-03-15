#include "jabber.h"
#include "config.h"
#include "debug.h"

int jabber_snddisco(const char *id, const char *to) {
	xode packet, temp, temp1;

	packet = xode_new("iq");
	xode_put_attrib(packet, "type", "result");
	xode_put_attrib(packet, "from", config_myjid);
	xode_put_attrib(packet, "to", to);

	if (id) {
		xode_put_attrib(packet, "id", id);
	}

	temp = xode_insert_tag(packet, "query");
	xode_put_attrib(temp, "xmlns", "http://jabber.org/protocol/disco#info");

	temp1 = xode_insert_tag(temp, "identity");
	xode_put_attrib(temp1, "category", "gateway");
	xode_put_attrib(temp1, "type", "tlen");
	xode_put_attrib(temp1, "name", "Tlen Transport");

	temp1 = xode_insert_tag(temp, "feature");
	xode_put_attrib(temp1, "var", "jabber:iq:register");

	temp1 = xode_insert_tag(temp, "feature");
	xode_put_attrib(temp1, "var", "jabber:iq:search");
	
	temp1 = xode_insert_tag(temp, "feature");
	xode_put_attrib(temp1, "var", "jabber:iq:gateway");

	temp1 = xode_insert_tag(temp, "feature");
	xode_put_attrib(temp1, "var", "jabber:iq:version");

	temp1 = xode_insert_tag(temp, "feature");
	xode_put_attrib(temp1, "var", "vcard-temp");

	temp1 = xode_insert_tag(temp, "feature");
	xode_put_attrib(temp1, "var", "http://jabber.org/protocol/disco#info");

/*	jabber:iq:agent
	jabber:iq:browse
	http://jabber.org/protocol/stats
	http://jabber.org/protocol/disco#items
*/

	my_strcpy(js_buf2, xode_to_str(packet));

	jabber_send(js_buf2);
	my_debug(5, "jabber: Zapytanie: %s", js_buf2);

	t_free(js_buf2);
	xode_free(packet);

	return 0;
}
