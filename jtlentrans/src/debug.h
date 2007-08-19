#ifndef __DEBUG_H
#define __DEBUG_H

#include <stdio.h> /* fprintf */

#include "config.h"

extern int mypid;
extern int config_daemon;
extern int config_tlendebug;

extern int syslogowac;
extern int stderrorowac;

extern int debug_init();
extern int debug_close();

extern int my_debug0(int level, const char *file, const int line, const char *msg, ...);

#define my_debug(level, msg, args...) my_debug0(level, __FILE__, __LINE__, msg, ## args )

#define t_free(what) \
	{ if ((what) != NULL) { free((what)); (what) = NULL; } else { my_debug(0, "free warning: ptr is null!"); }; }

					/* t_free(dst); */
#define my_strcpy(dst, src)	{ \
					if (dst != NULL) { \
						my_debug(0, "my_strcpy: dst is not null"); \
					} \
					if (src == NULL) { \
						my_debug(0, "my_strcpy: src is null"); \
						dst = NULL; \
					} else { \
						if ((dst = malloc((strlen(src)+1)*sizeof(char))) == NULL) { \
							perror("malloc()"); \
							exit(1); \
						} \
						strcpy(dst, src); \
					} \
				}

#define my_strcat(dst, src1, src2)	{ \
					if (dst != NULL) { \
						my_debug(0, "my_strcpy: dst is not null"); \
					} \
					if (src1 == NULL || src2 == NULL) { \
						my_debug(0, "my_strcpy: src? is null"); \
						dst = NULL; \
					} else { \
						if ((dst = malloc((strlen(src1)+strlen(src2)+1)*sizeof(char))) == NULL) { \
							perror("malloc()"); \
							exit(1); \
						} \
						strcpy(dst, src1); \
						strcat(dst, src2); \
					} \
				}

#endif /* __DEBUG_H */
