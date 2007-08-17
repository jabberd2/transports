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

    $Id: yahoo-transport.c,v 1.10 2004/01/01 03:04:35 pcurtis Exp $
*/

#include "yahoo-transport.h"

void yahoo_transport(instance i, xmlnode x) {
    yahoo_instance yi = pmalloco(i->p, sizeof(_yahoo_instance));
    yi->i  = i;
#ifndef _JCOMP
    yi->xc = xdb_cache(i);
    yi->cfg  = xdb_get(yi->xc,jid_new(xmlnode_pool(x),"config@-internal"),"jabber:config:yahoo");
#else
    yi->cfg = xmlnode_get_tag(jcr->config, "config");
#endif
    if (!(xmlnode_get_tag_data(yi->cfg,"instructions"))) {
      fprintf(stderr, "%s Configuration Error: There are no registration instructions (<instructions/>) in the configuration file.\n\n", i->id);
#ifdef _JCOMP
      exit(1);
#else
      _jabberd_shutdown();
#endif
    }

    if (!(xmlnode_get_tag_data(yi->cfg,"vCard/NAME"))) {
      fprintf(stderr, "%s Configuration Error: The vCard/NAME element is not defined in the configuration file.\n\n", i->id);
#ifdef _JCOMP
      exit(1);
#else
      _jabberd_shutdown();
#endif
    }

    log_notice(i->id, "Yahoo! Transport v%s [%s] starting.", YAHOO_VERSION, YAHOO_SUB_VERSION);
    yi->stats = pmalloco(i->p, sizeof(_yahoo_transport_stats));
    yi->stats->start = time(NULL);
    yi->stats->packets_in = 0;
    yi->stats->packets_out = 0;
    yi->stats->bytes_in = 0;
    yi->stats->bytes_out = 0;
    
    yi->user = xhash_new(31);
#ifdef _JCOMP
    yi->lock = g_mutex_new();
    yi->q = g_thread_pool_new((void *)yahoo_jabber_handler, NULL, 10, FALSE, NULL);
#else
    pth_mutex_init(&yi->lock);
#endif
    yi->charmap = pstrdup(yi->i->p, xmlnode_get_data(xmlnode_get_tag(yi->cfg,"charmap")));
    yi->server = pstrdup(yi->i->p, xmlnode_get_data(xmlnode_get_tag(yi->cfg,"server")));
    yi->port = j_atoi(xmlnode_get_data(xmlnode_get_tag(yi->cfg,"port")), 5050);
    yi->mail = (xmlnode_get_type(xmlnode_get_tag(yi->cfg,"newmail")) == NTYPE_TAG);
    yi->timer_interval = j_atoi(xmlnode_get_data(xmlnode_get_tag(yi->cfg,"interval")), 50);
    yi->timeout = j_atoi(xmlnode_get_data(xmlnode_get_tag(yi->cfg,"timeout")), 30);
    yi->timeout = (yi->timeout * 1000) / yi->timer_interval;
//  ji->debug = (xmlnode_get_type(xmlnode_get_tag(yi->cfg,"debug")) == NTYPE_TAG);




//  log_debug(ZONE, "[YAHOO]: yi = 0x%X  yi->user = 0x%X", (unsigned long)yi,
//      (unsigned long)yi->user);

   /* Register the Jabber packet handler -- All Jabber packets go here  */
   register_phandler(i, o_DELIVER, yahoo_phandler, (void *)yi);
   register_shutdown(yahoo_transport_shutdown, (void *)yi->user);
}
