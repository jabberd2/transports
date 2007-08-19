#include "jabber.h"
#include "config.h"
#include "debug.h"

int jabber_sndevent(const char *id, const char *from, const char *to, const char *data) {
	int n;

	if (to == NULL || from == NULL)
		return -1;

	my_debug(2, "jabber: Wysylam event do %s, id %s", to, (id ? id : ""));

	n = strlen("<message from='' to=''><x xmlns='jabber:x:event'><id></id></x></message>") + strlen(from) + strlen(to) + (data ? strlen(data) : 0) + (id ? strlen(id) : 0) + 1;
	if ((js_buf2 = malloc(n)) == NULL) {
		my_debug(0, "malloc()");
		return -2;
	}
	snprintf(js_buf2, n, "<message from='%s' to='%s'><x xmlns='jabber:x:event'>%s<id>%s</id></x></message>", from, to, (data ? data : ""), (id ? id : ""));
	jabber_send(js_buf2);
	my_debug(5, "jabber: Zapytanie: %s", js_buf2);
	t_free(js_buf2);

	return 0;
}
