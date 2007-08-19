/* --------------------------------------------------------------------------
 *
 * License
 *
 * The contents of this file are subject to the Jabber Open Source License
 * Version 1.0 (the "License").  You may not copy or use this file, in either
 * source code or executable form, except in compliance with the License.  You
 * may obtain a copy of the License at http://www.jabber.com/license/ or at
 * http://www.opensource.org/.
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied.  See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * Copyright (c) 2000-2001 Schuyler Heath <sheath@jabber.org>
 *
 * Acknowledgements
 *
 * Special thanks to the Jabber Open Source Contributors for their
 * suggestions and support of Jabber.
 *
 * -------------------------------------------------------------------------- */

#ifndef MSNTRANS_H
#define MSNTRANS_H

#include <assert.h>
#include "jabberd.h"
#undef VERSION
#include "config.h"

/* Size of session xhash */
#define SESSION_TABLE_SZ 500

/* Dumps packets
#define PACKET_DEBUG
*/

/* namespaces for service discovery */
#define NS_DISCO_ITEMS "http://jabber.org/protocol/disco#items"
#define NS_DISCO_INFO "http://jabber.org/protocol/disco#info"

/* MSN Transport instance */
typedef struct mti_struct
{
    instance i;
    pool p;
    xdbcache xc;
    xht sessions;
    int sessions_count;
    xmlnode vcard, admin;
    char *reg;
    time_t start;
    int attempts_max;
//    char **servers;
//    int cur_server;
    int con;
    char *con_id;
    char *join, *leave;
    char *invite_msg;
    char *proxyhost, *proxypass;
    int is_insecure, socksproxy, inbox_headlines, fancy_friendly; /* Option flags */
    xht iq_handlers;
} *mti, _mti;

result mt_receive(instance i, dpacket d, void *arg);

#endif
