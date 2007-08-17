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

    $Id: yahoo-phandler.c,v 1.28 2004/07/01 16:06:34 pcurtis Exp $
*/

#include "yahoo-transport.h"

/*   Used for initial debugging
void _yahoo_walk_hash(xht ht, const char *key, void *data, void *arg) {
   log_debug(ZONE, "[YAHOO]: Key = '%s'", key);
}

void _yahoo_get_hash(xht hash, char *what) {
  log_debug(ZONE, "[YAHOO]: -------------------------");
  log_debug(ZONE, "[YAHOO]: Reading keys from hash '%s'", what);
  xhash_walk(hash, _yahoo_walk_hash, NULL);
  log_debug(ZONE, "[YAHOO]: -------------------------");
}
*/

void _yahoo_shutdown_session(xht ht, const char *key, void *data, void *arg) {
  struct yahoo_data *yd = (struct yahoo_data *)data;
  yahoo_transport_presence_offline(yd);
}

void yahoo_transport_shutdown(void *arg) {
  xht session = (xht)arg;

  log_notice(ZONE, "[YAHOO]: Transport shutting down");
  xhash_walk(session, _yahoo_shutdown_session, NULL);
}

/*
  All this function does is read the packet headers and decide which
  "real" function to call.
 
  This is nothing more than a big "switch" statement ....
*/

void yahoo_jabber_user_packet(void *arg) {
   jpacket jp = (jpacket)arg;
   yahoo_instance yi = (yahoo_instance)jp->aux1;
   struct yahoo_data *yd = NULL;
   xmlnode body, x;
   char *buffer;
   int i;

/* These functions handle cases where Yahoo sessions are created, but not connected.
   They mostly are situations of long timeouts, and other cases
*/

/* If the user has not registered with the transport, return a 
   registration error to the client.
*/   
   if (yahoo_get_session_connection_state(jp) == YAHOO_NOT_REGISTERED) {
      if (yahoo_get_registration_required(jp)) {
         jutil_error(jp->x, TERROR_REGISTER);
         yahoo_deliver(NULL,jp->x);
         return;
      }
   }

/* Next, if there is a session around, and the client sends an unavailable presence
   mark the session cancelled. This will force a close of the session and a 
   disconnect from Yahoo. Essentially, this is a clean-up in case of bad connections
*/   

   if ((yahoo_get_session_connection_state(jp) != YAHOO_CONNECTED) &&
       (jp->type == JPACKET_PRESENCE) &&
       (jp->subtype == JPACKET__UNAVAILABLE)) {
      
      log_notice(ZONE, "[YAHOO]: Yahoo session for '%s' cancelled by user.", jid_full(jp->from));
      yd = yahoo_get_session("unavailable-presence", jp, yi);
      if (yd)
         yahoo_update_session_state(yd, YAHOO_CANCEL, "Unavailable Presence Rcv'd");
      return;
   }    


   i = 0;
   while ((yahoo_get_session_connection_state(jp) != YAHOO_CONNECTED) 
           && (i < yi->timeout) 
           && (yahoo_get_session_connection_state(jp) != YAHOO_CANCEL)) {
#ifdef _JCOMP
     g_usleep(yi->timer_interval * 1000);
#else
     pth_usleep(yi->timer_interval * 1000);
#endif
     i++;
   }

   if (yahoo_get_session_connection_state(jp) == YAHOO_CANCEL) {
      log_notice(ZONE, "[YAHOO]: Yahoo session for '%s' cancelled by user.", jid_full(jp->from));
      yd = yahoo_get_session("connection-cancelled", jp, yi);
      if (yd)
         yahoo_close(yd);
      jutil_error(jp->x, TERROR_DISCONNECTED);
      yahoo_deliver(NULL,jp->x);
      return;
      
   }

   if (i >= yi->timeout) {
      log_notice(ZONE, "[YAHOO]: Yahoo session for '%s' timed out.", jid_full(jp->from));
      yd = yahoo_get_session("connection-timeout", jp, yi);
      if (yd)
         yahoo_close(yd);
      jutil_error(jp->x, TERROR_EXTTIMEOUT);
      yahoo_deliver(NULL,jp->x);
      return;
   }
/* From here on, we have a connected Yahoo session, and we handle the packets
   normally.
*/

/* 
   Even though these are server packets, they require a user to have a
   session. These packets are mostly presence packets that have to be
   sent to Yahoo from the user.
*/
   if(jp->to->user == NULL) {
      yahoo_transport_packets(jp);
      return;
   }

   yd = yahoo_get_session("user-packet", jp, yi);

   if (!(yd)) {
      jutil_error(jp->x, TERROR_EXTERNAL);
      jpacket_reset(jp);
      yahoo_deliver(NULL,jp->x);
      return;
   }

   switch (jp->type) {

   case JPACKET_MESSAGE:

      switch (jp->subtype) {

      case JPACKET__NONE:
      case JPACKET__CHAT:
         body = xmlnode_get_tag(jp->x, "body");

         buffer = xmlnode_get_data(body);
         if (buffer)
            yahoo_send_im(yd, jp->to->user, buffer, strlen(buffer), 0);
         xmlnode_free(body);
         break;

      case JPACKET__GROUPCHAT:
         break;

      } //jp->subtype

      break;

   case JPACKET_PRESENCE:

      switch (jp->subtype) {

      case JPACKET__AVAILABLE:
/*
         show = xmlnode_get_tag(jp->x, "show");
         if (show) {
            body = xmlnode_get_tag(jp->x, "status");
            if (body) {
               yahoo_set_away(yd, 1, xmlnode_get_data(body));
               log_debug(ZONE, "[YAHOO]: '%s' set presence to '%s'", jp->from->user, xmlnode_get_data(body));
            } else {
               yahoo_set_away(yd, 1, xmlnode_get_data(show));
               log_debug(ZONE, "[YAHOO]: '%s' set presence to '%s'", jp->from->user, xmlnode_get_data(body));
            }
         } else {
            yahoo_set_away(yd, 0, "Available");
            log_debug(ZONE, "[YAHOO]: '%s' set presence to 'Available'", jp->from->user);
         }
*/
         break;

      case JPACKET__UNAVAILABLE:
         break;

      case JPACKET__PROBE:
         break;

      } //jp->subtype

      break;

   case JPACKET_S10N:

      switch (jp->subtype) {

      case JPACKET__SUBSCRIBE:

         log_debug(ZONE, "[YAHOO]: Adding '%s' to group '%s'",
            jp->to->user, "Buddies");
         xhash_put(yd->contacts, jp->to->user, (void *)"Buddies");
//       _yahoo_get_hash(yd->contacts, "contacts -- add buddy");
         yahoo_add_buddy(yd, jp->to->user, "Buddies");
         x = jutil_presnew(JPACKET__SUBSCRIBED, jid_full(jp->from), NULL);
         jutil_tofrom(x);
         xmlnode_put_attrib(x, "from", jid_full(jp->to));
         yahoo_deliver(NULL,x);
    
         break;

      case JPACKET__UNSUBSCRIBE:

//       _yahoo_get_hash(yd->contacts, "contacts -- remove buddy");
         buffer = (char *)xhash_get(yd->contacts, jp->to->user);
         if (buffer != NULL) {
            log_debug(ZONE, "[YAHOO]: Removing '%s' from group '%s'",
               jp->to->user, buffer);
            yahoo_remove_buddy(yd, jp->to->user, buffer);
            xhash_zap(yd->contacts, jp->to->user);
//          _yahoo_get_hash(yd->contacts, "contacts -- remove buddy, after zap");
         }
         break;

      } //jp->subtype

      break;

   case JPACKET_UNKNOWN:
   default:
      break;

   } // jp->type
}

/*
  This routine is called for every packet received from the jabber
  server core. If it looks somewhat redundant to the phandler routine,
  you are right. The difference between the two is this: the phandler
  is called by the jabberd core. It determines if a packet needs to
  get passed into the transport.

  This routine takes those packets and moves them into one or two
  places. The first is packets for registration. The second determines
  which user this packet is from. This allows us to create a 'session'
  for the user, and to create a queue specifically for the user. That
  means that all packets for a specific user are queued for them and
  them alone. This allows for high volume packet processing, as the
  transport does not wait for a particular user's packet to be
  processed.
*/
#ifdef _JCOMP
void yahoo_jabber_handler(void *arg, void *user_data) {
#else
void yahoo_jabber_handler(void *arg) {
#endif
   jpacket jp = (jpacket)arg;
   yahoo_instance yi = (yahoo_instance)jp->aux1;
   struct yahoo_data *yd = NULL;
   unsigned char *user;

   yi->stats->packets_in++;
/* 
   All error packets are returned
*/
   if (jpacket_subtype(jp) == JPACKET__ERROR) {
      xmlnode_free(jp->x);
      return;
   }

/*
   If a presence packet is sent from the client, we do not have to handle it
   as the transport handles the presence to Yahoo. Meaning: Yahoo does not want
   a presence from all of your Yahoo contacts. Yahoo wants one presence from 
   *you*
*/
   if ((jp->type == JPACKET_PRESENCE) && (jp->to->user != NULL)) {
      xmlnode_free(jp->x);
      return;
   }

/*
   Normally, subscribe/unsubscribe & chat/message packets directly to the 
   transport are meaningless. We do not handle them, and pass them off
*/
   if ((jp->type == JPACKET_S10N) && (jp->to->user == NULL)) {
      xmlnode_free(jp->x);
      return;
   }

   log_debug(ZONE, "[YAHOO] Packet type=%d subtype=%d iq=%s iq_type=%s", jp->type, jp->subtype, NS_PRINT(jp->iq), xmlnode_get_attrib(jp->x, "type"));
   log_debug(ZONE, "[YAHOO] Packet to '%s'", jid_full(jp->to));
   log_debug(ZONE, "[YAHOO] Packet from '%s'", jid_full(jp->from));

/*
   JID user part should be case insensitive
   convert user part of from JID to lower case
   Mangle "from" JID, save original attribute for migration
*/
   if(jp->from->user != NULL)
     for(user = jp->from->user; *user != '\0'; user++)
       if(*user < 128)
         *user = tolower(*user);
   xmlnode_put_attrib(jp->x, "origfrom", xmlnode_get_attrib(jp->x, "from"));
   xmlnode_put_attrib(jp->x, "from", jid_full(jp->from));

/*
   All IQ packets are handled at the server level, not the user level, so
   ship them off for processing.
*/
   if (jp->type ==  JPACKET_IQ) {
      yahoo_transport_packets(jp);
      return;
   }


/*
   There is one more type of packet the transport receives, and
   basically does nothing with. If the client sends an "unsubscribed"
   packet to us, we just acknowledge it.
*/

   if ((jp->type == JPACKET_S10N) && (jp->subtype == JPACKET__UNSUBSCRIBED)) {
      jutil_tofrom(jp->x);
      jpacket_reset(jp);
      yahoo_deliver(NULL,jp->x);
      return;
   }

/* At this point, all packets not handled above will require the user
   to have a 'session'. In addition, the session may establish a
   connection to Yahoo, IF the user has registered with the transport.
   If he has not, the transport will send a 'registration required'
   error to the client.
*/

   if (yahoo_get_session("initial-check", jp, yi) == NULL) {
      if ((jp->type == JPACKET_PRESENCE) && (jp->subtype == JPACKET__UNAVAILABLE)) {
         log_notice(ZONE, "[YAHOO] No active session, unavailable presence from '%s' ignored", jid_full(jp->from));
         return;
      }
      yahoo_new_session("initial-check", jp, yi);
   }

   yd = yahoo_get_session("phandler", jp, yi);
   if (yd)
      yahoo_jabber_user_packet(jp);
// xmlnode_free(jp->x);
   return;
}





/*
 All packets received from the Jabber server are processed through
 this function. The function adds the 'instance' structure to the
 packet for use by the functions that follow.
 
 This function also grabs the user's session, and adds it to the
 packet. The user 'session' is distinct for each user, and contains
 user specific information.
 
 After all data is attached to the packet, this function queues up the
 packet for handling by the above function.
*/

result yahoo_phandler(instance id, dpacket dp, void *arg) {
   _yahoo_instance *yi = (_yahoo_instance *)arg;
   jpacket jp;
   char *val;

   /* strip the route off */
   if(dp->type == p_ROUTE)
      jp = jpacket_new(xmlnode_get_firstchild(dp->x));
   else
      jp = jpacket_new(dp->x);

   if(jp == NULL || jp->type == JPACKET_UNKNOWN) {
      xmlnode_free(jp->x);
      return r_DONE;
   }

   val = xmlnode_get_attrib(jp->x,"to");
   if (val == NULL) {
      jutil_error(jp->x, TERROR_BAD);
      yahoo_deliver(NULL,jp->x);
      return r_DONE;
   }

   val = xmlnode_get_attrib(jp->x,"from");
   if (val == NULL) {
      jutil_error(jp->x, TERROR_BAD);
      yahoo_deliver(NULL,jp->x);
      return r_DONE;
   }



   jp->aux1 = (void *)yi;
// log_debug(ZONE, "[YAHOO]: yi = 0x%X  yi->user = 0x%X", (unsigned long)yi,
//      (unsigned long)yi->user);
#ifdef _JCOMP
   g_thread_pool_push(yi->q, (void *)jp, NULL);
#else
   mtq_send(yi->q, jp->p, yahoo_jabber_handler, (void*)jp);
#endif
   return r_DONE;
}

