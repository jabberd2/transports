#ifndef __ALL_H
#define __ALL_H

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <netdb.h>
#include <errno.h>
#include <time.h>
#include <signal.h>
#include <limits.h>
#include <errno.h>
#include <pwd.h>
#include <grp.h>
#include <ctype.h>
#include <locale.h>

#if HAVE_LIBDMALLOC 
#include <dmalloc.h>
#endif

#include "debug.h"

extern char *tlen_version;
extern time_t start_time;

#ifndef VERSION
#define VERSION "0.3.6"
#endif

#endif /* __ALL_H */
