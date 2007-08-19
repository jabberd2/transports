#include "jabber.h"
#include "config.h"
#include "debug.h"
#include "jid.h"

int jabber_sndgate_form(const char *id, const char *to) {
	xode iq, query, temp;

	iq = xode_new("iq");
	xode_put_attrib(iq, "type", "result");
	xode_put_attrib(iq, "from", config_myjid);
	xode_put_attrib(iq, "to", to);
	xode_put_attrib(iq, "id", id);

	query = xode_insert_tag(iq, "query");
	xode_put_attrib(query, "xmlns", "jabber:iq:gateway");
    
	temp = xode_insert_tag(query, "desc");
	xode_insert_cdata(temp, config_gateway_desc, -1);

	temp = xode_insert_tag(query, "prompt");
	xode_insert_cdata(temp, config_gateway_prompt, -1);

	my_strcpy(js_buf2, xode_to_str(iq));
	jabber_send(js_buf2);
	my_debug(5, "jabber: Zapytanie: %s", js_buf2);

	xode_free(iq);
	t_free(js_buf2);
	return 0;
}

int jabber_sndgate_result(const char *id, const char *to, const char *prompt) {
	xode iq, query, temp;
	char *tlen_jid;

	tlen_jid = jid_tid2jid(prompt);

	iq = xode_new("iq");
	xode_put_attrib(iq, "type", "result");
	xode_put_attrib(iq, "from", config_myjid);
	xode_put_attrib(iq, "to", to);
	xode_put_attrib(iq, "id", id);

	query = xode_insert_tag(iq, "query");
	xode_put_attrib(query, "xmlns", "jabber:iq:gateway");
    
	temp = xode_insert_tag(query, "prompt");
	xode_insert_cdata(temp, tlen_jid, -1);

	temp = xode_insert_tag(query, "jid");
	xode_insert_cdata(temp, tlen_jid, -1);

	my_strcpy(js_buf2, xode_to_str(iq));
	jabber_send(js_buf2);
	my_debug(5, "jabber: Zapytanie: %s", js_buf2);

	xode_free(iq);
	t_free(tlen_jid);
	t_free(js_buf2);
	return 0;
}
