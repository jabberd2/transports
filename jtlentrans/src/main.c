#include "all.h"

#include <sys/select.h>
#include <signal.h>
#include <getopt.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/stat.h>


#include "config.h"
#include "tlen.h"
#include "jabber.h"
#include "users.h"
#include "jid.h"
#include "tlenopt.h"

#include "encoding.h"

char *tlen_version = PACKAGE_VERSION;

time_t start_time;

struct sigaction obsluga_term;

int daemonize() {
	int ii, noctty, noblock, fd[3];
	long parent_pid, child_pid;

	parent_pid = (long)getpid();

	if (0L > (child_pid = (long)fork())) { /* failed */
		return -1;
	} else if (child_pid) { /* parent */
		exit(0);
	}

	/* child */
	for(ii = 0; ii < 3; ii++) { /* detach terminal */
		if (isatty(ii)) {
			fd[ii] = -1;
			close(ii);
		} else {
			fd[ii] = ii;
		}
	}

	setsid(); /* become session leader */

	if (0 > (child_pid=(long)fork())) /* failed */
		return -1;
	else if (child_pid) /* parent */
		exit(0);

	umask(000); /* allow all users access to files created */

	noctty = noblock = 0; /* reset file mode mask */
#ifdef O_NOCTTY
	noctty |= O_NOCTTY;
#endif
#ifdef O_NDELAY
	noblock |= O_NDELAY;
#endif
#ifdef O_NONBLOCK
	noblock |= O_NONBLOCK;
#endif

	/* open handles to nul device */
	for(ii = 0; ii < 3; ii++) { /* detach terminal */
		if (ii >= 0)
			continue;
		switch(ii) {
			case 0:
				open("/dev/null",O_RDONLY|noctty); /* stdin */
				break;
			case 1:
				open("/dev/null",O_APPEND|noctty); /* stdout */
				break;
			case 2:
				open("/dev/null",O_APPEND|noblock|noctty); /* stderr */
				break;
		}
	}
	return 0;
}

int tt_main_loop() {
	int wyjsc = 0;
	int maxfd = 0;

	struct timeval tv;

	int last_ping_time;

	int co_ile_pingac = 40;
	int ile_max_czekac = 35;

	/* zestawy gniazd ktore bedziemy monitorowac na nowe dane */
	fd_set rd;
	fd_set wr;
	int retval;

	tt_user **user;
	tt_user *user0;

	last_ping_time = time(NULL);


	while (!wyjsc) {
		tv.tv_sec = ile_max_czekac; tv.tv_usec = 0;

		/* inicjalizacja zestawow */
		FD_ZERO(&rd);
		FD_ZERO(&wr);

		maxfd = 0;

		/* ustawiamy zestawy do monitorowania jabbera */
		FD_SET(jabber_gniazdo, &rd);
		/* FD_SET(jabber_gniazdo, &wr); */

		if (maxfd < jabber_gniazdo)
			maxfd = jabber_gniazdo;

		/* ustawiamy zestawy do monitorowania tlena */
		for (user = tt_users; *user; user++) {
			user0 = *user;
			if (user0->tlen_loged) {
				my_debug(3, "loop: Dodaje obsluge %s", user0->tid_short);
				if (((user0->tlen_sesja)->check & TLEN_CHECK_READ))
					FD_SET((user0->tlen_sesja)->fd, &rd);
				if (user0->tlen_loged == 1) {
					if (((user0->tlen_sesja)->check & TLEN_CHECK_WRITE))
						FD_SET((user0->tlen_sesja)->fd, &wr);
				}
				if (maxfd < (user0->tlen_sesja)->fd)
					maxfd = (user0->tlen_sesja)->fd;
			}
		}

		/* czekamy az cos sie wydazy (z blokowaniem, max 60sek)... */
		my_debug(2, "loop: Czekam...");
		if ((retval = select(maxfd + 1, &rd, &wr /*NULL*/, NULL, &tv)) < 0) {
			my_debug(0, "loop: select()");
			return -3;
		};
		if (retval == 0) {
			my_debug(3, "loop: Nic sie nie wydazylo");
			;
		} else {
			my_debug(3, "loop: Cos sie wydazylo");
			/* sprawdzac czy cos sie dzieje na dyskryptorach ... */

			/*
			if (jabber_gniazdo && (FD_ISSET(jabber_gniazdo, &rd) || FD_ISSET(jabber_gniazdo, &wr))) {
			*/
			if (FD_ISSET(jabber_gniazdo, &rd)) {
				my_debug(2, "loop: Zdazenie z Jabbera");
				jabber_handler();
			};
			/* jesli z jabbera:

				przyjdzie info ze ktos zarejstrowany zmienil status na dostepny
				zalogowac, jesli nie byl
	
				przyjdzie info ze ktos jest nie dostepny
				wylogowac go, jesli jest
			
				tlen_logout_user(user);

				w przypadku zmiany statusu
				zmienic status, ewentualnie zalogowac

				w przypadku nadejscia message od usera jabberowego
				i skierowaniu jej do usera tlenowego
				wyslac ja

				obsluga autoryzacji....

				obsluga rejstracji...

				wyszukiwanie...
			*/
			for (user = tt_users; *user; user++) {
				user0 = *user;
				if (user0->tlen_loged) {
					if (user0->tlen_sesja) {
						if ((user0->tlen_sesja)->fd >= 0) {
							if (FD_ISSET((user0->tlen_sesja)->fd, &rd) || FD_ISSET((user0->tlen_sesja)->fd, &wr)) {
								my_debug(2, "loop: Zdazenie z Tlena dla %s", user0->tid_short);
								tlen_handler(user0);
							}
						}
					}
				}
			}
			/* jesli z tlena w konkretnej sesji skojarzonej z userem jabberowy:

				przyjdzie info ze ktos zmienil status
				wyslac do jabbera o tym info

				nadeszla wiadomosc od usera tlenowego
				wyslac to do odpowiedniego usera jabberowego
	
				obsluga autoryzacji...

				wyszukiwanie...

			Reagowac na takie rzeczy jak rozlaczenie

			Zrobic obsluge sygnalow
			*/
		}

		/* Pingujmy co minute serwery */
		if (time(NULL) - last_ping_time > co_ile_pingac) {
			my_debug(2, "loop: tlen: Puszczam ping do serwera (z kadego usera)");
			for (user = tt_users; *user; user++) {
				user0 = *user;
				if (user0->tlen_loged == 2) {
					my_debug(3, "tlen: Puszczam ping do serwera z user '%s'", user0->tid_short);
					tlen_ping(user0->tlen_sesja);
					user0->last_ping_time = time(NULL);
				}
			}
			my_debug(2, "loop: jabber: Puszczam ping do serwera");
			jabber_ping();
			jabber_mypresence();
			last_ping_time = time(NULL);
		}
	}
	return 0;
}

void sighand_term(int sygnal, siginfo_t *info, void *kontekst) {
	tt_user **user, *user0;
	my_debug(0, "sighand: Otrzymalem sygnal, wylaczam sie...");
	my_debug(2, "sighand: Wylogowywanie z Tlena");
	for (user = tt_users; *user; user++) {
		user0 = *user;
		if (user0->tlen_loged) {
			my_debug(3, "sighand: Wylogowywanie z Tlena usera '%s'", user0->tid_short);
			tlen_logout_user(user0, NULL);
		}
	}
	my_debug(2, "sighand: Wylogowywanie z Jabbera");
	jabber_logout();
	jabber_close();
	users_close();
	config_close();
	encoding_done();
	debug_close();
	my_debug(0, "sighand: Exiting...");
	exit(0);
}

void print_help() {
	fprintf(stderr, "tlen-transport help:\n"
	"-f, --foreground\tDon't fork, run in foreground, prints only critical errors on console\n"
	"-d, --debug\tDon't fork, run in foreground, and turn on logging to console\n"
	"-D, --tlendebug\tEnable libtlen debuging. Implies --foreground\n"
	"-u, --user\tRun as specified user\n"
	"-v, --version\tPrint version and exit\n"
	"-c, --config\tUse specified config file\n"
	"-h, --help\tPrint help and exit\n");
}

int file_exists(char * filename) {
	FILE *fp;
	if ((fp = fopen(filename, "r"))) {
		fclose(fp);
		return 1;
	} else {
		return 0;
	}
}

void print_version(char *verstring) {
    fprintf(stderr, "tlen-transport ver. %s\n", verstring);
}


int main(int argc, char **argv) {
	mypid = getpid();
	start_time = time(NULL);
	my_debug(1, "main: Ustawiam wstepne debugowanie...");
	debug_init();

	my_debug(1, "main: Start");

	config_daemon = 1;
	syslogowac = 0;
	stderrorowac = 0;

	while ((tlenopt_read = getopt_long(argc, argv, tlenopt_optstring, tlenopt_longoptions, &tlenopt_opt)) != EOF) {
		switch(tlenopt_read) {
			case 'h':
				print_help();
				exit(EXIT_SUCCESS);
				break;
			case 'u':
				my_debug(1, "main: Zmieniam grupe i uzytkownika...");
                    		pw_user = getpwnam(optarg);
                    		if (setregid(pw_user->pw_gid,pw_user->pw_gid) < 0) {
					my_debug(0, "setregid");
				}
				if (setreuid(pw_user->pw_uid,pw_user->pw_uid) < 0) {
					my_debug(0, "setreuid");
				}															                                                                                                                    									
				break;
			case 'c':
				if (file_exists(optarg)) {
					my_debug(1, "Pobieram sciezke dla pliku konfiguracyjnego..");
					config_file = optarg;
				} else {
					stderrorowac = 1;
					my_debug(0, "Podany plik konfiguracyjny nie istnieje!");
					exit(EXIT_FAILURE);
				}
				break;
			case 'v':
				print_version(tlen_version);
				exit(EXIT_SUCCESS);
				break;
			case 'f':
				config_daemon = 0;
				break;
			case 'd':
				stderrorowac = 1;
				config_daemon = 0;
				break;
			case 'D':
				my_debug(1, "main: Debugowanie libtlen na stderr.");
				config_tlendebug = 1;
				config_daemon = 0;
				break;
		}
	}

	if (config_file == NULL || !file_exists(config_file)) {
		stderrorowac = 1;
		my_debug(0, "main: Podaj plik konfiguracyjny (opcja -c)...");
		exit(EXIT_FAILURE);
	}

	if (config_daemon == 0) {
		my_debug(1, "main: Zostaje w trybie foreground");
	} else {
		stderrorowac = 1;
		my_debug(1, "main: Daemonising...");
		stderrorowac = 0;
		if (daemonize()) {
			my_debug(0, "main: Blad daemonising...");
			exit(1);
		}
	}

	my_debug(1, "main: Wlaczam obsluge sygnalow...");
	obsluga_term.sa_sigaction = sighand_term;
	obsluga_term.sa_flags = SA_SIGINFO;
	sigaction(SIGINT, &obsluga_term, NULL);
	sigaction(SIGTERM, &obsluga_term, NULL);

	my_debug(1, "main: Wczytuje konwersje...");
	encoding_init();

	my_debug(1, "main: Wczytuje config...");
	if (config_init() < 0) {
		my_debug(0, "main: Blad przy wczytywaniu configu...");
		return 1;
	}

	my_debug(1, "main: Ustawiam debugowanie...");
	debug_init();

	my_debug(1, "main: Wczytuje liste userow...");
	if (users_init() < 0) {
		my_debug(0, "main: Blad przy wczytywaniu listy userow...");
		return 1;
	}

	my_debug(1, "main: Polaczenie z serwerem jabbera...");
	jabber_connect();
	jabber_login();

	my_debug(1, "main: Wysylam zapytania o stan zarejstrowanych userow...");
	jabber_sendprobes();

	my_debug(1, "main: Wchodze w glowna petle...");
	tt_main_loop();

	return 0;
}
