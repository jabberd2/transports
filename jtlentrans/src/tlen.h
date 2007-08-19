#include "users.h"

#ifndef __TLEN_H
#define __TLEN_H

#include <libtlen/libtlen.h>


extern int tlen_login_user(tt_user *user);
extern int tlen_presence_user(tt_user *user);
extern int tlen_logout_user(tt_user *user, const char *status);

extern int tlen_handler(tt_user *user);

#endif /* __TLEN_H */
