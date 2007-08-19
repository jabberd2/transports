#include <openssl/evp.h>	/* sha1 */

#include "jabber.h"
#include "config.h"
#include "debug.h"

void setnonblocking(int gniazdo) {
	int opts;
	opts = fcntl(gniazdo, F_GETFL);
	if (opts < 0) {
		my_debug(0, "fcntl(F_GETFL)");
		exit(EXIT_FAILURE);
	}
	opts = (opts | O_NONBLOCK);
	if (fcntl(gniazdo, F_SETFL, opts) < 0) {
		my_debug(0, "fcntl(F_SETFL)");
	exit(EXIT_FAILURE);
	}
	return;
}

int jabber_connect() {
	/* Ustawienie hosta docelowego */
	/*
	if ((sp = getservbyname("jabber-tlen", "tcp")) == NULL) (
		my_debug(0, "bfdr: tcp/jabber-tlen: unknown service");
		exit(1);
	}
	if ((hp = gethostbyaddr(config_connect_ip, sizeof(hp), AF_INET)) == NULL) {
		my_debug(0, "Nie znaleziono hosta");
		exit(2);
	}
	*/
	if ((hp = gethostbyname(config_connect_ip)) == NULL) {
		my_debug(0, "jabber: Nie znaleziono hosta");
		exit(2);
	}

	memset((char *)&server_addr, 0, sizeof(server_addr));
	/*
	server_addr.sin_family = hp->h_addrtype;
	*/
	server_addr.sin_family = AF_INET;
	memcpy((void *)(&server_addr.sin_addr), (void *)(hp->h_addr_list[0]), hp->h_length);
	inet_aton(config_connect_ip, &(server_addr.sin_addr.s_addr));
	/*
	server_addr.sin_port = sp->s_port;
	*/
	server_addr.sin_port = htons(config_connect_port);

	/* Utworzenie gniazda */
	if ((jabber_gniazdo = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0) {
		my_debug(0, "jabber: Blad tworzenia gniazda!");
		my_debug(0, "socket() failed");
		exit(1);
	} else {
		my_debug(0, "jabber: Utworzylem gniazdo");
	}

	/*
	setnonblocking(jabber_gniazdo);
	*/

	/*
	shutdown(gniazdo, 2);
	printf("Ustawilem parametry gniazda\n");
	*/

	/* Polaczenie */
	server_addrlen = sizeof(server_addr);
	if ((jabber_server = connect(jabber_gniazdo, (struct sockaddr *)&server_addr, server_addrlen)) < 0) {
		my_debug(0, "jabber: Blad polaczenia!");
		my_debug(0, "connect() failed");
		exit(1);
	} else {
		my_debug(0, "jabber: Polaczony z %s:%d!", config_connect_ip, config_connect_port);
	}

	/* Transmisja */
	my_debug(0, "jabber: Rozpoczecie transmisji!");
	return 0;
}

int jabber_login() {
	char *handshake;
	char *temp;
	int n = 0;

	EVP_MD_CTX mdctx;
	unsigned char md_value[EVP_MAX_MD_SIZE];
	const EVP_MD *md;
	int i, md_len;

	if ((js_buf = malloc(js_buf_len)) == NULL) {
		my_debug(0, "malloc()");
		return -2;
	}

	/* Inicjalizacja stream */
	js_p = xode_pool_new();
	js = xode_stream_new(js_p, jabber_stream_handler, NULL);

	/* Rozpoczynamy sesje */
	my_debug(1, "jabber: Otwieram stream:stream");

	n = strlen("<stream:stream xmlns='jabber:component:accept' xmlns:stream='http://etherx.jabber.org/streams' to=''>") + strlen(config_connect_id) + 1;
	if ((js_buf2 = malloc(n)) == NULL) {
		my_debug(0, "malloc()");
		return -2;
	}
	snprintf(js_buf2, n, "<stream:stream xmlns='jabber:component:accept' xmlns:stream='http://etherx.jabber.org/streams' to='%s'>", config_connect_id);
	jabber_send(js_buf2);
	my_debug(5, "jabber: Zapytanie:\n%s", js_buf2);
	t_free(js_buf2);
	jabber_recv(&js_buf);
	my_debug(5, "jabber: Odpowiedz:\n%s", js_buf);

	sid = NULL;
	xode_stream_eat(js, js_buf, -1);
	if (sid == NULL)
	    return -1;

	/* Obliczamy handshaka i go wyslamy */
	if (config_connect_secret == NULL)
	    config_connect_secret = "";

	n = strlen(config_connect_secret) + strlen(sid) + 1;
	if ((temp = malloc(n)) == NULL) {
		my_debug(0, "malloc()");
		return -2;
	}

	strcpy(temp, sid);
	strcat(temp, config_connect_secret);
	OpenSSL_add_all_digests();
	md = EVP_get_digestbyname("sha1");
	EVP_MD_CTX_init(&mdctx);
	EVP_DigestInit_ex(&mdctx, md, NULL);
	EVP_DigestUpdate(&mdctx, temp, strlen(temp));
	EVP_DigestFinal_ex(&mdctx, md_value, &md_len);
	EVP_MD_CTX_cleanup(&mdctx);

        /* wyczy¶æ temp! */
	t_free(temp);

	n = 41;
	if ((handshake = malloc(n)) == NULL) {
		my_debug(0, "malloc()");
		return -2;
	}
	temp = handshake;
	for(i = 0; i < md_len && i < 40; i++) {
		sprintf(temp, "%02x", md_value[i]);
		temp += 2;
	}
	temp = '\0';

	my_debug(1, "jabber: Wysylam handshake");
	n = strlen("<handshake></handshake>\n") + strlen(handshake) + 1;
	if ((js_buf2 = malloc(n)) == NULL) {
		my_debug(0, "malloc()");
		return -2;
	}
	snprintf(js_buf2, n, "<handshake>%s</handshake>", handshake);
	t_free(handshake);
	jabber_send(js_buf2);
	my_debug(5, "jabber: Zapytanie: (id=%s): %s", sid, js_buf2);
	t_free(js_buf2);
	jabber_recv(&js_buf);
	my_debug(5, "jabber: Odpowiedz: %s", js_buf);
	jabber_loged = 0;
	xode_stream_eat(js, js_buf, -1);
	if (jabber_loged != 1) {
		my_debug(0, "jabber: Nie udalo sie zalogowac");
		exit(1);
	}

	jabber_mypresence();

	return 0;
}
