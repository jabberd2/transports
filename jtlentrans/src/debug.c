#include "all.h"
#include "debug.h"
#include <stdarg.h>
#include <time.h>
#include <syslog.h>

int mypid;
int config_daemon = 1;
int config_tlendebug = 0;

int syslogowac = 0;
int stderrorowac = 0;

FILE* logfile = NULL;

int debug_init() {
	my_debug(2, "debug: Otwieram pliki logow");
	if (config_log_file) {
		my_debug(3, "debug: Otwieram plik logow %s", config_log_file);
		if (!(logfile = fopen(config_log_file, "a"))) {
			perror("fopen");
			my_debug(0, "debug: Blad otwierania pliku logow %s", config_log_file);
		}
	}
	if (config_syslog) {
		my_debug(3, "debug: Otwieram logowanie syslog");
		openlog("tt", LOG_PID | LOG_CONS, LOG_DAEMON);
		syslogowac = 1;
	}
	return 0;
}

int debug_close() {
	my_debug(2, "debug: Zamkyam pliki logow");
	if (logfile) {
		my_debug(3, "debug: Zamykam plik logow %s", config_log_file);
		fclose(logfile);
		logfile = NULL;
	}
	if (syslogowac) {
		my_debug(3, "debug: Zamykam logowanie syslog");
		syslogowac = 0;
		closelog();
	}
	return 0;
}

int my_debug0(int level, const char *file, const int line, const char *msg, ...) {
	int n;
	va_list ap;
	char *temp;
	char *s;
	char c;
	int d;
	void *p;
	struct tm *czas;
	time_t czas0;
	char data_mod[30];
	int i;
	char *debugbuf0, *debugbuf1, *debugbuf0_end;

	if (config_debug_level < level)
		return 0;

	czas0 = time(NULL);

	/* FIXIT: Nie allokowac co chwile tyle pamieci */
	debugbuf0 = malloc(32*1024);
	debugbuf0_end = debugbuf0;

	va_start(ap, msg);
	temp = (char *)msg;
	while (*temp) {
		if ((*temp) == '%') {
			temp++;
			switch(*temp) {
				case 's':
					s = va_arg(ap, char *);
					if (s == NULL) {
					    strcat(debugbuf0_end,  "(null)");
					} else {
					    strcat(debugbuf0_end,  s);
					}
					break;
				case 'd':
					d = va_arg(ap, int);
					sprintf(debugbuf0_end, "%d", d);
					break;
				case 'c':
					c = (char) va_arg(ap, int);
					sprintf(debugbuf0_end, "%c", c);
					break;
				case 'p':
					p = va_arg(ap, void *);
					sprintf(debugbuf0_end, "%p", p);
					break;
				case '%':
					sprintf(debugbuf0_end, "%%");
					break;
				case '\0':
				default:
					temp--;
					break;
			}
			temp++;
		} else {
			debugbuf0_end[1] = '\0';
			debugbuf0_end[0] = *temp;
			temp++;
		}
		debugbuf0_end = debugbuf0 + strlen(debugbuf0);
	}
	va_end(ap);

	czas = localtime(&czas0);
	sprintf(data_mod, "%4d-%02d-%02d %02d:%02d:%02d",
			1900+czas->tm_year, czas->tm_mon+1, czas->tm_mday,
			czas->tm_hour, czas->tm_min, czas->tm_sec);

	n = strlen("tlentrans (%d) %s %s:%d: %s\n") + strlen(data_mod) + strlen(file) + strlen(debugbuf0) + 6 + 6 + 1;
	debugbuf1 = malloc(n*sizeof(char));
	snprintf(debugbuf1, n, "tlentrans (%d) %s %s:%d: %s\n", mypid, data_mod, file, line, debugbuf0);

	for (i = 0; i < 3; i++) {
		switch (i) {
		case 0:
			if (stderrorowac) {
				fprintf(stderr, "%s", debugbuf1);
				fflush(stderr);
			}
			break;
		case 1:
			if (logfile != NULL) {
				fprintf(logfile, "%s", debugbuf1);
				fflush(logfile);
			}
			break;
		case 2:
			if (syslogowac) {
				syslog(LOG_DEBUG, "%s", debugbuf0);
			}
			break;
		default:
			if (stderrorowac) {
				fprintf(stderr, "%s", debugbuf1);
				fflush(stderr);
			}
			break;
		}
	}

	t_free(debugbuf1);
	t_free(debugbuf0);

	return 0;
}
