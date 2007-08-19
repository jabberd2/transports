#include "jabber.h"
#include "config.h"
#include "debug.h"

/*
    <presence from=...  to=... [id=...] [type=...] [lang=...]>
	[ <show> { ... | away | chat | dnd | xa } </show> ]
	[ <status [lang=...]> ... </status> ]
	[ <priority>...</priority> ]
    </presence>

typy:

subscribe
subscribed
unsubscribe
unsubscribed
unavailable
probe
error    
*/
int jabber_sndpresence(const char *from, const char *to, const char *type, const char *show, const char *status) {
	xode packet, error;
	char *from_res = NULL;

	if (from == NULL || to == NULL) {
		return -1;
		my_debug(0, "jabber: Niepoprawne parametry");
	}

	my_debug(2, "jabber: Wysylam info o statusie '%s' do '%s' (type=%s, show=%s, status=%s)", from, to, type, show, status);

    
	if (((type == NULL) || (strstr(type, "subscribe") == NULL)) && (strchr(from, '/') == NULL)) {
        if (strchr(from, '@') == NULL)
            my_strcat(from_res, from, "/registred")
        else
            my_strcat(from_res, from, "/Tlen.pl");
	} else {
		my_strcpy(from_res, from);
	}

	packet = xode_new("presence");
    xode_put_attrib(packet, "from", from_res);
	xode_put_attrib(packet, "to", to);
	
	if (type) {
		xode_put_attrib(packet, "type", type);
	}
	
    if ((type == NULL) || (strstr(type, "error") == NULL)) {
        if (show) {
            xode temp = xode_insert_tag(packet, "show");
            xode_insert_cdata(temp, show, -1);
        }

        if (status) {
            xode temp = xode_insert_tag(packet, "status");
            xode_insert_cdata(temp, status, -1);
        }
    } else {
        error = xode_insert_tag(packet, "error"); 
        xode_put_attrib(error, "code", show);
        xode_insert_cdata(error, status, -1);            
        
    }

	my_strcpy(js_buf2, xode_to_str(packet));

	jabber_send(js_buf2);
	my_debug(5, "jabber: Zapytanie: %s", js_buf2);

	t_free(js_buf2);
	t_free(from_res);
	xode_free(packet);

	return 0;
}
