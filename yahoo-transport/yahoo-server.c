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
 
    $Id: yahoo-server.c,v 1.23 2004/07/01 19:13:29 pcurtis Exp $
*/

#include "yahoo-transport.h"

int yahoo_xdb_set(yahoo_instance yi, char *host, jid owner, xmlnode data) {
   int ret;
   jid j;

   j = jid_new(owner->p, spools(owner->p, owner->user, "%", owner->server, "@", host, owner->p));
   xmlnode_put_attrib(data, "xmlns", "yahootrans:data");
   ret = xdb_set(yi->xc, j, "yahootrans:data", data);

   return ret;
}

xmlnode yahoo_xdb_get(yahoo_instance yi, char *host, jid owner) {
   xmlnode x;
   jid j;

   j = jid_new(owner->p, spools(owner->p, owner->user, "%", owner->server, "@", host, owner->p));
   x= xdb_get(yi->xc, j, "yahootrans:data");

   if(j_strcmp(xmlnode_get_name(x), "foo") == 0)
      x = xmlnode_get_firstchild(x);

   if(j_strcmp(xmlnode_get_name(x), "logon") == 0)
      x = xmlnode_get_firstchild(x);

   return x;
}

/* convert pre-2.1.2 XDB data to case insensitive clear text format */
void yahoo_xdb_convert(yahoo_instance yi, char *user, jid nid) {
   jid id, old, new;
   xmlnode x;
   pool p;

   if(user == NULL) return;

   p = pool_new();
   id = jid_new(p, user);
   old = jid_new(p, spools(p, shahash(jid_full(jid_user(id))),
                           "@", yi->i->id, p));

   new = jid_new(p, spools(p, nid->user, "%", nid->server,
                           "@", yi->i->id, p));

   x = xdb_get(yi->xc, old, "yahootrans:data");
   if(x != NULL)
      if(xdb_set(yi->xc, new, "yahootrans:data", x) == 0) {
         log_notice(ZONE, "[YAHOO]: Converted XDB for user %s",jid_full(jid_user(id)));
         xdb_set(yi->xc, old, "yahootrans:data", NULL);
      }

   pool_free(p);
}

void yahoo_transport_packets(jpacket jp) {
   yahoo_instance yi = (yahoo_instance)jp->aux1;
   struct yahoo_data *yd = NULL;
   xmlnode os, query, body;
   char *tstr;
   time_t t;
   struct utsname un;
   int status, show;

   log_debug(ZONE, "transport packet handling");
   if (NSCHECK(jp->iq, NS_REGISTER)) {

      if (jp->subtype == JPACKET__GET) {
         xmlnode x = yahoo_xdb_get(yi, jp->to->server, jp->from);
         jutil_iqresult(jp->x);
         query = xmlnode_insert_tag(jp->x, "query");
         xmlnode_put_attrib(query,"xmlns",NS_REGISTER);
         jpacket_reset(jp);
         xmlnode_insert_tag(query, "username");
         xmlnode_insert_tag(query, "password");
         xmlnode_insert_cdata(xmlnode_insert_tag(query, "key"), jutil_regkey(NULL, jid_full(jp->to)), -1);
         xmlnode_insert_cdata(xmlnode_insert_tag(query, "instructions"), xmlnode_get_tag_data(yi->cfg,"instructions"), -1);

         /* we have XDB data for this user */
         if(x != NULL) {
            // We place the username we have in the packet, but no password
            xmlnode_insert_cdata(xmlnode_get_tag(query, "username"), xmlnode_get_data(xmlnode_get_tag(x, "username")), -1);
            // If the username and password exist, then we can only assume the
            // user has registered with the transport.
            if ((xmlnode_get_data(xmlnode_get_tag(x, "username")) != NULL) && (xmlnode_get_data(xmlnode_get_tag(x, "password")) != NULL))
               xmlnode_insert_tag(query, "registered");
            xmlnode_free(x);
         }

         yahoo_deliver(NULL,jp->x);
         return;
      } // JPACKET__GET

      if (jp->subtype == JPACKET__SET) {
         xmlnode p;
         jid jnew;

         if (xmlnode_get_tag(jp->iq, "remove")) {
            //          xmlnode logon = xmlnode_new_tag("logon");
            //          yahoo_xdb_set(yi, jp->to->server, jp->from, logon);
            log_notice(ZONE, "[YAHOO]: Deleting Registration for '%s'", jid_full(jp->from));

            yd = yahoo_get_session("unregister", jp, yi);
            if (yd) {
               yahoo_unsubscribe_contacts(yd);
               yahoo_transport_presence_offline(yd);
               yahoo_close(yd);
            }
            p = xmlnode_new_tag("presence");
            jnew = jid_new(xmlnode_pool(p), yi->i->id);
            xmlnode_put_attrib(p, "to", jid_full(jp->from));
            xmlnode_put_attrib(p, "from", jid_full(jnew));
            xmlnode_put_attrib(p, "type", "unsubscribe");
            yahoo_deliver(NULL,p);

            jutil_iqresult(jp->x);
            query = xmlnode_insert_tag(jp->x, "query");
            xmlnode_put_attrib(query,"xmlns",NS_REGISTER);
            jpacket_reset(jp);
            yahoo_deliver(NULL,jp->x);
            return;
         }

         if (yahoo_xdb_set(yi, jp->to->server, jp->from, jp->iq) == 0) {
            log_notice(ZONE, "[YAHOO]: Creating Registration for '%s'", jid_full(jp->from));
            p = xmlnode_new_tag("presence");
            jnew = jid_new(xmlnode_pool(p), yi->i->id);
            xmlnode_put_attrib(p, "to", jid_full(jp->from));
            xmlnode_put_attrib(p, "from", jid_full(jnew));
            xmlnode_put_attrib(p, "type", "subscribe");
            yahoo_deliver(NULL,p);


            yd = yahoo_get_session("register", jp, yi);
            if (yd) {
               yahoo_remove_session(jp, yi);
               yahoo_new_session("register", jp, yi);
            }
            jutil_iqresult(jp->x);
            query = xmlnode_insert_tag(jp->x, "query");
            xmlnode_put_attrib(query,"xmlns",NS_REGISTER);
            jpacket_reset(jp);
            yahoo_deliver(NULL,jp->x);

            return;
         } else {
            jutil_error(jp->x, TERROR_INTERNAL);
            jpacket_reset(jp);
            yahoo_deliver(NULL,jp->x);
            return;
         }
      } // JPACKET__SET

   } // NS_REGISTER




   /*
     The presence packets from the user are delivered here. This is where
     we either change the presence to Yahoo, or disconnect if the user's
     presence is 'unavailable'
   */
   if ((jp->type == JPACKET_PRESENCE) && (jp->subtype == JPACKET__UNAVAILABLE)) {
      log_debug(ZONE, "[YAHOO]: Closing session for '%s'", jid_full(jp->from));
      yd = yahoo_get_session("close", jp, yi);
      if (yd) {
         yahoo_remove_session(jp, yi);
         yahoo_transport_presence_offline(yd);
         yahoo_close(yd);
      }
      return;
   } // unavailable presence

   if ((jp->type == JPACKET_PRESENCE) && (jp->subtype == JPACKET__AVAILABLE)) {
      yd = yahoo_get_session("presence", jp, yi);

      if (!(yd)) {
         jutil_error(jp->x, TERROR_EXTERNAL);
         jpacket_reset(jp);
         yahoo_deliver(NULL,jp->x);
         return;
      }

      status = (xmlnode_get_type(xmlnode_get_tag(jp->x,"status")) == NTYPE_TAG);
      show = (xmlnode_get_type(xmlnode_get_tag(jp->x,"show")) == NTYPE_TAG);

      log_notice(ZONE, "[YAHOO] Available: status=%d, show=%d for '%s'", status, show, jid_full(jp->from));

      if (!show) {
         yahoo_set_away(yd, 0, "Available");
      } else {
         if (status) {
            body = xmlnode_get_tag(jp->x, "status");
         } else {
            body = xmlnode_get_tag(jp->x, "show");
         }
         yahoo_set_away(yd, 1, xmlnode_get_data(body));
      }
      jutil_tofrom(jp->x);
      yahoo_deliver(NULL,jp->x);
      return;
   } // available presence



   /*
     Everything from here on down are "server" packets. These are packets
     that are directed specifically to the transport, rather than a user
     or Yahoo. Mostly, these are informational packets like stats,
     version, time, etc.
   */

   if (NSCHECK(jp->iq, NS_STATS)) {
      yahoo_stats(jp);
      return;
   }

   if (NSCHECK(jp->iq, NS_VERSION)) {
      jutil_iqresult(jp->x);
      query = xmlnode_insert_tag(jp->x, "query");
      xmlnode_put_attrib(query, "xmlns", NS_VERSION);
      jpacket_reset(jp);

      xmlnode_insert_cdata(xmlnode_insert_tag(query, "name"), xmlnode_get_tag_data(yi->cfg,"vCard/NAME"), -1);
      uname(&un);
      os = xmlnode_insert_tag(query,"os");
      xmlnode_insert_cdata(os,un.sysname,-1);
      xmlnode_insert_cdata(os," ",1);
      xmlnode_insert_cdata(os,un.release,-1);

      xmlnode_insert_cdata(xmlnode_insert_tag(query, "version"),
                           spools(jp->p, YAHOO_VERSION, " [", YAHOO_SUB_VERSION, "] (Gaim yahoo.c ",YAHOO_GAIM_VERSION,")", jp->p), -1);
      yahoo_deliver(NULL,jp->x);
      return;
   }


   if (NSCHECK(jp->iq, NS_TIME)) {
      jutil_iqresult(jp->x);
      query = xmlnode_insert_tag(jp->x, "query");
      xmlnode_put_attrib(query, "xmlns", NS_TIME);
      jpacket_reset(jp);
      xmlnode_insert_cdata(xmlnode_insert_tag(query,"utc"),jutil_timestamp(),-1);
      xmlnode_insert_cdata(xmlnode_insert_tag(query,"tz"),tzname[0],-1);

      /* create nice display time */
      t = time(NULL);
      tstr = ctime(&t);
      tstr[strlen(tstr) - 1] = '\0'; /* cut off newline */
      xmlnode_insert_cdata(xmlnode_insert_tag(query,"display"),tstr,-1);
      yahoo_deliver(NULL,jp->x);
      return;
   }

   if (NSCHECK(jp->iq, NS_BROWSE)) {
      jutil_iqresult(jp->x);
      query = xmlnode_insert_tag(jp->x, "service");
      xmlnode_put_attrib(query, "xmlns", "jabber:iq:browse");
      xmlnode_put_attrib(query, "type", "jabber");
      xmlnode_put_attrib(query, "jid", yi->i->id);
      xmlnode_put_attrib(query, "name",xmlnode_get_tag_data(yi->cfg,"vCard/NAME")) ;
      xmlnode_insert_cdata(xmlnode_insert_tag(query, "ns"), "jabber:iq:register", -1);
      xmlnode_insert_cdata(xmlnode_insert_tag(query, "ns"), "jabber:iq:version", -1);
      xmlnode_insert_cdata(xmlnode_insert_tag(query, "ns"), "jabber:iq:time", -1);

      jpacket_reset(jp);
      xmlnode_put_attrib(jp->iq,"type","yahoo");
      xmlnode_put_attrib(jp->iq,"name",xmlnode_get_tag_data(yi->cfg,"vCard/NAME")); /* pull name from the server vCard */
      yahoo_deliver(NULL,jp->x);
      return;
   }

   if (NSCHECK(jp->iq, NS_DISCO_ITEMS)) {
      jutil_iqresult(jp->x);
      query = xmlnode_insert_tag(jp->x, "query");
      xmlnode_put_attrib(query, "xmlns", NS_DISCO_ITEMS);
      jpacket_reset(jp);
      yahoo_deliver(NULL,jp->x);
      return;
   }

   if (NSCHECK(jp->iq, NS_DISCO_INFO)) {
      jutil_iqresult(jp->x);
      query = xmlnode_insert_tag(jp->x, "query");
      xmlnode_put_attrib(query, "xmlns", NS_DISCO_INFO);

      body = xmlnode_insert_tag(query, "identity");
      xmlnode_put_attrib(body, "category", "gateway");
      xmlnode_put_attrib(body, "type", "yahoo");
      xmlnode_put_attrib(body, "name", xmlnode_get_tag_data(yi->cfg,"vCard/NAME"));

      body = xmlnode_insert_tag(query, "feature");
      xmlnode_put_attrib(body, "var", "jabber:iq:register");
      body = xmlnode_insert_tag(query, "feature");
      xmlnode_put_attrib(body, "var", "jabber:iq:version");
      body = xmlnode_insert_tag(query, "feature");
      xmlnode_put_attrib(body, "var", "jabber:iq:time");

      jpacket_reset(jp);
      yahoo_deliver(NULL, jp->x);
      return;
   }
   /*
     If we get here, the packet is requesting something we do not do.
   */
   jutil_error(jp->x, TERROR_NOTIMPL);
   yahoo_deliver(NULL,jp->x);
   return;
}
