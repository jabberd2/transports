#ifndef __TLENOPT_H
#define __TLENOPT_H

int tlenopt_opt, tlenopt_read;
struct passwd *pw_user;

struct option tlenopt_longoptions[] = {
	{"help",	0, 0, 'h'},
	{"version",	0, 0, 'v'},
	{"user",	1, 0, 'u'},
	{"config",	1, 0, 'c'},
	{"debug",	0, 0, 'd'},
	{"tlendebug",	0, 0, 'D'},
	{"foreground",	0, 0, 'f'},
	{0, 0, 0, 0}
};						

const char tlenopt_optstring[] = "dDfhvu:c:";

#endif /* __TLENOPT_H */
