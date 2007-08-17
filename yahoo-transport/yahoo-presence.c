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

    $Id: yahoo-presence.c,v 1.11 2004/01/17 00:07:12 pcurtis Exp $
*/

#include "yahoo-transport.h"

void yahoo_set_jabber_presence(struct yahoo_data *yd, char *contact_name, int state, char *msg) {
   xmlnode x = NULL;
   pool p;

   p = pool_new();
   yd->yi->stats->packets_out++;
   switch (state) {

   case YAHOO_STATUS_AVAILABLE:	// Available
      x = jutil_presnew(JPACKET__AVAILABLE, jid_full(yd->me), msg);
      xmlnode_put_attrib(x, "from", spools(p, contact_name, "@", yd->yi->i->id, p));
      log_debug(ZONE, "[YAHOO]:   Presence for '%s' = available", xmlnode2str(x));
      break;


   case YAHOO_STATUS_BRB:	// Away
   case YAHOO_STATUS_OUTTOLUNCH:
   case YAHOO_STATUS_STEPPEDOUT:
      x = jutil_presnew(JPACKET__AVAILABLE, jid_full(yd->me), msg);
      xmlnode_put_attrib(x, "from", spools(p, contact_name, "@", yd->yi->i->id, p));
      xmlnode_insert_cdata(xmlnode_insert_tag(x,"show"), "away", -1);
      log_debug(ZONE, "[YAHOO]:   Presence for '%s' = away", xmlnode2str(x));
      break;
   
   case YAHOO_STATUS_BUSY: // Busy
   case YAHOO_STATUS_ONPHONE:
      x = jutil_presnew(JPACKET__AVAILABLE , jid_full(yd->me) , msg);
      xmlnode_put_attrib(x , " from " , spools(p , contact_name , " @ " , yd->yi->i->id , p));
      xmlnode_insert_cdata(xmlnode_insert_tag(x , " show " ) , " dnd " , -1);
      log_debug(ZONE , " [YAHOO]: Presence for ' %s ' = dnd " , xmlnode2str(x));
      break;
   
   case YAHOO_STATUS_NOTATHOME: // XA
   case YAHOO_STATUS_NOTATDESK:
   case YAHOO_STATUS_NOTINOFFICE:
   case YAHOO_STATUS_ONVACATION:
      x = jutil_presnew(JPACKET__AVAILABLE , jid_full(yd->me) , msg);
      xmlnode_put_attrib(x , " from " , spools(p , contact_name , " @ " , yd->yi->i->id , p));
      xmlnode_insert_cdata(xmlnode_insert_tag(x , " show " ) , " xa " , -1);
      log_debug(ZONE , " [YAHOO]: Presence for ' %s ' = xa " , xmlnode2str(x));
      break;

   case YAHOO_STATUS_OFFLINE:	// Logoff
      x = jutil_presnew(JPACKET__UNAVAILABLE, jid_full(yd->me), "Logged Off");
      xmlnode_put_attrib(x, "from", spools(p, contact_name, "@", yd->yi->i->id, p));
      log_debug(ZONE, "[YAHOO]:   Presence for '%s' = logged off", xmlnode2str(x));
      break;

   }
   yahoo_deliver(NULL,x);
   pool_free(p);
}

void yahoo_set_jabber_buddy(struct yahoo_data *yd, char *contact_name, char *group) {
   xmlnode x;
   pool p;

   log_debug(ZONE, "[YAHOO]: Adding contact '%s' to '%s'", contact_name, group);
   xhash_put(yd->contacts, j_strdup(contact_name), (void *)j_strdup(group));
   p = pool_new();
   yd->yi->stats->packets_out++;

   x = jutil_presnew(JPACKET__SUBSCRIBED, jid_full(yd->me), NULL);
   xmlnode_put_attrib(x, "from", spools(p, contact_name, "@", yd->yi->i->id, p));
   xmlnode_put_attrib(x, "name", contact_name);
   xmlnode_insert_cdata(xmlnode_insert_tag(x, "group"), group, -1);
   yahoo_deliver(NULL,x);
   pool_free(p);
}

void yahoo_transport_presence_offline(struct yahoo_data *yd) {
   xmlnode x;

   yahoo_unavailable_contacts(yd);
   yd->yi->stats->packets_out++;
   x = jutil_presnew(JPACKET__UNAVAILABLE, jid_full(yd->me), NULL);
   xmlnode_put_attrib(x, "from", yd->yi->i->id);
   yahoo_deliver(NULL,x);
}

void yahoo_transport_presence_online(struct yahoo_data *yd) {
  xmlnode x;

   yd->yi->stats->packets_out++;
   x = jutil_presnew(JPACKET__AVAILABLE, jid_full(yd->me), NULL);
   xmlnode_put_attrib(x, "from", yd->yi->i->id);
   xmlnode_insert_cdata(xmlnode_insert_tag(x, "status"), "Connected", -1);
   yahoo_deliver(NULL,x);
}


void yahoo_unsubscribe_contact(xht ht, const char *key, void *data, void *arg) {
   xmlnode x;
   struct yahoo_data *yd = (struct yahoo_data *)arg;
   pool p;

   p = pool_new();
   yd->yi->stats->packets_out++;

   log_debug(ZONE, "[YAHOO]: Unsubscribing '%s'", key);
   x = jutil_presnew(JPACKET__UNSUBSCRIBE, jid_full(yd->me), NULL);
   xmlnode_put_attrib(x, "from", spools(p, key, "@", yd->yi->i->id, p));
// x = jutil_presnew(JPACKET__UNSUBSCRIBE, spools(p, key, "@", yd->yi->i->id, p), NULL);
// xmlnode_put_attrib(x, "from", jid_full(yd->me));
   yahoo_deliver(NULL,x);
   pool_free(p);
}

void yahoo_unsubscribe_contacts(struct yahoo_data *yd) {
  xhash_walk(yd->contacts, yahoo_unsubscribe_contact, (void *)yd);
}



void yahoo_unavailable_contact(xht ht, const char *key, void *data, void *arg) {
   xmlnode x;
   struct yahoo_data *yd = (struct yahoo_data *)arg;
   pool p;

   p = pool_new();
   yd->yi->stats->packets_out++;

   x = jutil_presnew(JPACKET__UNAVAILABLE, jid_full(yd->me), NULL);
   xmlnode_put_attrib(x, "from", spools(p, key, "@", yd->yi->i->id, p));
   yahoo_deliver(NULL,x);
   pool_free(p);
}

void yahoo_unavailable_contacts(struct yahoo_data *yd) {
  xhash_walk(yd->contacts, yahoo_unavailable_contact, (void *)yd);
}
