/*

    Yahoo! Transport 2 for the Jabber Server
    Copyright (c) 2002, Paul Curtis
    Portions of this program are derived from GAIM (yahoo.c)

    This file is part of the Jabber Yahoo! Transport. (yahoo-transport-2)

    The Yahoo! Transport is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    The Yahoo! Transport is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with the Yahoo! Transport; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

    $Id: yahoo-mail.c,v 1.2 2003/09/29 22:58:59 pcurtis Exp $
*/

#include "yahoo-transport.h"

void yahoo_send_jabber_mail_notify(struct yahoo_data *yd, int count, char *from, char *subj) {
   xmlnode m;
   char *s;
   pool p;

   if (count == 0)
     return;

   if (yd->last_mail_count == count)
     return;

   p = pool_new();
   yd->yi->stats->packets_out++;

   if (count == -1) {
     m = jutil_msgnew("normal", jid_full(yd->me), spools(p, "NEW MAIL from ", from, p), spools(p, "You have received new mail from ", from," with the subject '", subj, "'\n\n http://mail.yahoo.com\n",p));
   } else {
     s = (char *)pmalloc(p, 64);
     if (count == 1)
       sprintf(s, "You Have 1 Unread E-Mail in your Yahoo! Inbox");
     else
       sprintf(s, "You Have %d Unread E-Mails in your Yahoo! Inbox", count);
     m = jutil_msgnew("normal", jid_full(yd->me), s, "\nhttp://mail.yahoo.com\n");
     yd->last_mail_count = count;
   }

   xmlnode_put_attrib(m, "from", spools(p, yd->yi->i->id, p));

   yahoo_deliver(NULL,m);
   pool_free(p);
}
