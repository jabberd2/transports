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

    $Id: yahoo-composing.c,v 1.1 2003/09/29 00:40:44 pcurtis Exp $
*/

#include "yahoo-transport.h"

void yahoo_send_jabber_composing_start(struct yahoo_data *yd, char *from) {
   xmlnode msg,x;
   pool p;

   p = pool_new();
   yd->yi->stats->packets_out++;

   msg = xmlnode_new_tag("message");
   xmlnode_put_attrib(msg, "to", jid_full(yd->me));
   xmlnode_put_attrib(msg, "from", spools(p, from, "@", yd->yi->i->id, p));

   x = xmlnode_new_tag("x");
   xmlnode_put_attrib(x, "xmlns", "jabber:x:event");
   xmlnode_insert_tag(x, "composing");
   xmlnode_insert_cdata (xmlnode_insert_tag (x, "id"), "ABCD", 4);

   xmlnode_insert_node(msg, x);
   yahoo_deliver(NULL,msg);
   pool_free(p);
}


void yahoo_send_jabber_composing_stop(struct yahoo_data *yd, char *from) {
   xmlnode msg,x;
   pool p;

   p = pool_new();
   yd->yi->stats->packets_out++;

   msg = xmlnode_new_tag("message");
   xmlnode_put_attrib(msg, "to", jid_full(yd->me));
   xmlnode_put_attrib(msg, "from", spools(p, from, "@", yd->yi->i->id, p));

   x = xmlnode_new_tag("x");
   xmlnode_put_attrib(x, "xmlns", "jabber:x:event");
   xmlnode_insert_cdata (xmlnode_insert_tag (x, "id"), "ABCD", 4);

   xmlnode_insert_node(msg, x);
   yahoo_deliver(NULL,msg);
   pool_free(p);
}

