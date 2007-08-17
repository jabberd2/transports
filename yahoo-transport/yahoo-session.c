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

    $Id: yahoo-session.c,v 1.20 2004/06/25 18:33:56 pcurtis Exp $
*/

#include "yahoo-transport.h"

/* 
   User sessions are keyed on the JID of the user (without the resource)
   We examine the packet to get the user information, then look up to see
   if the user has a Yahoo session currently.

   If there isn't a session we create one and fill in the information.
*/
   


void yahoo_new_session(char *who, jpacket jp, yahoo_instance yi) {
   struct yahoo_data *yd = NULL;
   jid u;
   xmlnode x;

#ifdef _JCOMP
   g_mutex_lock(yi->lock);
#else
   pth_mutex_acquire(&yi->lock, 0, NULL);
#endif
   u = jid_new(jp->p, jid_full(jid_user(jp->from)));
   x = yahoo_xdb_get(yi, jp->to->server, jp->from);
   if(x == NULL) {
      yahoo_xdb_convert(yi, xmlnode_get_attrib(jp->x,"origfrom"), jp->from);
      x = yahoo_xdb_get(yi, jp->to->server, jp->from);
   }
   yd = (struct yahoo_data *)pmalloco(yi->i->p, sizeof(struct yahoo_data));
   yd->fd = NULL;
   yd->username = xmlnode_get_data(xmlnode_get_tag(x, "username"));
   yd->password = xmlnode_get_data(xmlnode_get_tag(x, "password"));
   yd->me = jid_new(yi->i->p, jid_full(jp->from));
   yd->connection_state = YAHOO_NEW_SESSION;
   yd->yi = yi;
   yd->registration_required = 0;
   yd->last_mail_count = 0;
   yd->contacts = xhash_new(31);
   yd->session_key = j_strdup(jid_full(u));
   xhash_put(yi->user, yd->session_key, (void *)yd);

   log_debug(ZONE, "[YAHOO]: New session for '%s' from '%s'", yd->session_key, who);
   if ((yd->username != NULL) && (yd->password != NULL)) {
      yd->connection_state = YAHOO_CONNECTING;
      xhash_put(yi->user, j_strdup(jid_full(u)), (void *)yd);
#ifdef _JCOMP
      g_mutex_unlock(yi->lock);
#else
      pth_mutex_release(&yi->lock);
#endif
      log_debug(ZONE, "[YAHOO]: New connection to '%s:%d' for '%s'", 
         yi->server, yi->port, yd->session_key);
      mio_connect(yi->server, yi->port, yahoo_pending, (void*)yd, 30, MIO_CONNECT_RAW);
   } else {
      yd->registration_required = 1;
      yd->connection_state = YAHOO_NOT_REGISTERED;
      xhash_put(yi->user, j_strdup(jid_full(u)), (void *)yd);
   }
#ifdef _JCOMP
   g_mutex_unlock(yi->lock);
#else
   pth_mutex_release(&yi->lock);
#endif
}



struct yahoo_data *yahoo_get_session(char *who, jpacket jp, yahoo_instance yi) {
   struct yahoo_data *yd = NULL;
   jid u;

#ifdef _JCOMP
   g_mutex_lock(yi->lock);
#else
   pth_mutex_acquire(&yi->lock, 0, NULL);
#endif
// log_debug(ZONE, "[YAHOO]: yi = 0x%X  yi->user = 0x%X", (unsigned long)yi,
//      (unsigned long)yi->user);

   u = jid_new(jp->p, jid_full(jid_user(jp->from)));

   yd = (struct yahoo_data *)xhash_get(yi->user, jid_full(u));

// if (yd)
//    log_debug(ZONE, "[YAHOO]: Got session (0x%X) for '%s' from '%s' %d", (unsigned long)yd, jid_full(u), who, yd->connection_state);
// yahoo_debug_sessions(yi->user);
#ifdef _JCOMP
   g_mutex_unlock(yi->lock);
#else
   pth_mutex_release(&yi->lock);
#endif
   return yd;
}


/*
  This is the reverse of the previous function. When a user disconnects, we want to
  log him off Yahoo, and remove his session
*/

void yahoo_remove_session(jpacket jp, yahoo_instance yi) {
   struct yahoo_data *session;
   jid u;

#ifdef _JCOMP
   g_mutex_lock(yi->lock);
#else
   pth_mutex_acquire(&yi->lock, 0, NULL);
#endif
   u = jid_new(jp->p, jid_full(jid_user(jp->from)));
   session = (struct yahoo_data *)xhash_get(yi->user, jid_full(u));

   if (session != NULL) {
      yahoo_transport_presence_offline(session);
//    pool_free(session->p);
      xhash_zap(yi->user, jid_full(u));
      log_notice(ZONE, "Zapped Yahoo! session for '%s'", jid_full(u));
   }
// yahoo_debug_sessions(yi->user);
#ifdef _JCOMP
   g_mutex_unlock(yi->lock);
#else
   pth_mutex_release(&yi->lock);
#endif
}

void yahoo_remove_session_yd(struct yahoo_data *yd) {
   yahoo_instance yi = yd->yi;

#ifdef _JCOMP
   g_mutex_lock(yi->lock);
#else
   pth_mutex_acquire(&yi->lock, 0, NULL);
#endif

   if (yd != NULL) {
      yahoo_transport_presence_offline(yd);
//    pool_free(yd->p);
      log_notice(ZONE, "Ending Yahoo! session (yd) for '%s'", yd->session_key);
      xhash_zap(yi->user, yd->session_key);
   }
// yahoo_debug_sessions(yi->user);
#ifdef _JCOMP
   g_mutex_unlock(yi->lock);
#else
   pth_mutex_release(&yi->lock);
#endif
}

void yahoo_update_session_state(struct yahoo_data *yd, int state, char *msg) {
   yahoo_instance yi = yd->yi;

#ifdef _JCOMP
   g_mutex_lock(yi->lock);
#else
   pth_mutex_acquire(&yd->yi->lock, 0, NULL);
#endif
// log_debug(ZONE, "[YAHOO]: Updating session state (%d) for '%s' from '%s'", state, jid_full(u), msg);
   yd->connection_state = state;
   xhash_put(yd->yi->user, yd->session_key, (void *)yd);
#ifdef _JCOMP
   g_mutex_unlock(yi->lock);
#else
   pth_mutex_release(&yd->yi->lock);
#endif
   if (yd->connection_state == YAHOO_CONNECTED)
      yahoo_transport_presence_online(yd);
   else
      yahoo_transport_presence_offline(yd);
}

int yahoo_get_session_connection_state(jpacket jp) {
   yahoo_instance yi = (yahoo_instance)jp->aux1;
   struct yahoo_data *yd = NULL;

   yd = yahoo_get_session("conn-state", jp, yi);
   if (yd)
      return yd->connection_state;
   else
      return 0;
}

int yahoo_get_registration_required(jpacket jp) {
   yahoo_instance yi = (yahoo_instance)jp->aux1;
   struct yahoo_data *yd = NULL;

   yd = yahoo_get_session("reg-reqd", jp, yi);
   if (yd) {
      if (yd->registration_required) {
         yd->registration_required = 0;
         return 1;
      } else
         return 0;
   } else
      return 0;
}
