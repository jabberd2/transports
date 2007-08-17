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
 
    $Id: yahoo-stats.c,v 1.5 2003/09/27 22:39:52 pcurtis Exp $
*/

#include "yahoo-transport.h"

struct _yahoo_walk_session {
  xht users;
  xmlnode who;
};

void _yahoo_walk_session_hash(xht ht, const char *key, void *data, void *arg) {
   struct _yahoo_walk_session *s = (struct _yahoo_walk_session *)arg;

   xmlnode_insert_cdata(xmlnode_insert_tag(s->who, "user"), key, -1);
}

void _yahoo_get_sessions(struct _yahoo_walk_session *s) {
  xhash_walk(s->users, _yahoo_walk_session_hash, s);
}

void yahoo_stats(jpacket jp) {
   yahoo_instance yi = (yahoo_instance)jp->aux1;
   struct _yahoo_walk_session *s;
   xmlnode query, traffic;
   char *buffer;
   time_t t;

   jutil_iqresult(jp->x);
   query = xmlnode_insert_tag(jp->x, "query");
   xmlnode_put_attrib(query, "xmlns", NS_STATS);
   jpacket_reset(jp);

   log_notice(ZONE, "[YAHOO]: In the 'stats' function");
   s = (struct _yahoo_walk_session *)pmalloco(jp->p, sizeof(struct _yahoo_walk_session));
   s->users = yi->user;
   s->who = xmlnode_insert_tag(query, "who");
   _yahoo_get_sessions(s);

   log_notice(ZONE, "[YAHOO]: In the 'stats' function, users retrieved");
   traffic = xmlnode_insert_tag(query, "traffic");
   buffer = (char *)pmalloco(jp->p, 256);
   snprintf(buffer, 255, "%ld", yi->stats->packets_in);
   xmlnode_insert_cdata(xmlnode_insert_tag(traffic, "packets-in"), buffer, -1);
   snprintf(buffer, 255, "%ld", yi->stats->packets_out);
   xmlnode_insert_cdata(xmlnode_insert_tag(traffic, "packets-out"), buffer, -1);
   snprintf(buffer, 255, "%ld", yi->stats->bytes_in);
   xmlnode_insert_cdata(xmlnode_insert_tag(traffic, "bytes-in"), buffer, -1);
   snprintf(buffer, 255, "%ld", yi->stats->bytes_out);
   xmlnode_insert_cdata(xmlnode_insert_tag(traffic, "bytes-out"), buffer, -1);

   time(&t);
   snprintf(buffer, 255, "%ld", (t - yi->stats->start));
   xmlnode_insert_cdata(xmlnode_insert_tag(query, "uptime"), buffer, -1);

   yahoo_deliver(yi->i,jp->x);
   return;  
}
