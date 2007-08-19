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

#include "session.h"
#include "sb.h"
#include <curl/curl.h>

/*char *mt_default_servers[] = {
    "messenger.hotmail.com",
    NULL
};*/

void mt_init_curl(mti ti, xmlnode cfg) {
    curl_global_init(CURL_GLOBAL_ALL);

    if (cfg == NULL) {
        log_debug(ZONE, "No curl options configured");
        return;
    }
    ti->is_insecure=0;
    if (xmlnode_get_tag(cfg,"insecureSSL")) {
        // Thanks to David Carter <carter@carter.to> for this fix
        ti->is_insecure=1;
        log_warn(ZONE, "Curl will use insecure SSL mode");
    }

    if (xmlnode_get_tag(cfg,"proxyhost")) {
        ti->proxyhost = pstrdup(ti->p,xmlnode_get_tag_data(cfg,"proxyhost"));
        if (ti->proxyhost != NULL) {
            log_debug(ZONE,"Using proxy host and port: %s", ti->proxyhost);
        }
	else {
            log_debug(ZONE,"No proxy configured");
        }
    }
    else {
        ti->proxyhost = 0;
        return;
    }

    if (xmlnode_get_tag(cfg,"proxypass")) {
        ti->proxypass = pstrdup(ti->p,xmlnode_get_tag_data(cfg,"proxypass"));
        if (ti->proxypass != NULL) {
            log_debug(ZONE,"Using proxy user/pass of: %s", ti->proxypass);
	}
        else {
            log_debug(ZONE,"No proxy user/pass configured");
	}
    }
    else {
        ti->proxypass = 0;
    }

    if (xmlnode_get_tag(cfg,"socksproxy")) {
        ti->socksproxy = 1;
        log_debug(ZONE,"Using a SOCKS5 proxy");
    }
    else {
        ti->socksproxy = 0;
        log_debug(ZONE,"Using an HTTP proxy");
    }
}

int mt_init_conference(mti ti, xmlnode cfg)
{
    if (cfg == NULL)
    {
        return 0;
    }

    ti->con_id = pstrdup(ti->p,xmlnode_get_attrib(cfg,"id"));
    if (ti->con_id == NULL)
    {
        log_error(ti->i->id,"No conference ID configured");
        return 1;
    }

    ti->join = pstrdup(ti->p,xmlnode_get_tag_data(cfg,"notice/join"));
    ti->leave = pstrdup(ti->p,xmlnode_get_tag_data(cfg,"notice/leave"));
    ti->con = 1;

    if (xmlnode_get_tag(cfg,"invite"))
    {
        ti->invite_msg = pstrdup(ti->p,xmlnode_get_tag_data(cfg,"invite"));
        if (ti->invite_msg == NULL)
        {
            log_error(ti->i->id,"invite tag must contain the invitation messages to be displayed.");
            return 1;
        }
    }

    return 0;
}
/*
int mt_init_servers(mti ti, xmlnode cfg)
{
    xmlnode cur;
    char *server;
    int c = 0;

    ti->cur_server = 0;

    if (cfg == NULL)
    {
        ti->attempts_max = 5;
        ti->servers = mt_default_servers;
        return 0;
    }

    ti->attempts_max = j_atoi(xmlnode_get_tag_data(cfg,"attempts"),5);

    for_each_node(cur,cfg)
        if (j_strcmp(xmlnode_get_name(cur),"ip") == 0)
            c++;

    if (c == 0)
    {
        ti->servers = mt_default_servers;
        return 0;
    }

    ti->servers = pmalloco(ti->p,sizeof(char *) * c + 1);
    c = 0;

    for_each_node(cur,cfg)
    {
        if (j_strcmp(xmlnode_get_name(cur),"ip") == 0)
        {
            server = xmlnode_get_data(cur);
            if (server == NULL)
            {
                log_error(ti->i->id,"An <ip/> tag must contain the IP address of a MSN Dispatch Server");
                return 1;
            }

            ti->servers[c++] = pstrdup(ti->p,server);
        }
    }

    ti->servers[c + 1] = NULL;

    return 0;
}*/

void _mt_debug(xht h, const char *key, void *val, void *arg)
{
    session s = (session) val;
    int *total = (int *) arg;
    log_debug(ZONE,"SESSION[%s:%d] %d, size %d",jid_full(s->id),s->exit_flag,s->ref,pool_size(s->p));
    *total++;
}

result mt_debug(void *arg)
{
    mti ti = (mti) arg;
    int total = 0;
    xhash_walk(ti->sessions,&_mt_debug,&total);
    log_debug(ZONE,"SESSION TOTAL %d/%d",ti->sessions_count,total);
    return r_DONE;
}

void mt_shutdown_sessions(xht h, const char *key, void *val, void *arg)
{
    session s = (session) val;
    mt_session_end(s);
}

void mt_shutdown(void *arg)
{
    mti ti = (mti) arg;

    log_debug(ZONE,"Shutting down MSN Transport");

    xhash_walk(ti->sessions,&mt_shutdown_sessions,NULL);
    xhash_free(ti->sessions);
    ti->sessions = NULL;
    xhash_free(ti->iq_handlers);
    xmlnode_free(ti->admin);
    xmlnode_free(ti->vcard);
}

int mt_init(mti ti)
{
    xmlnode cfg;

    ti->xc = xdb_cache(ti->i);
    cfg = xdb_get(ti->xc,jid_new(ti->p,"config@-internal"),"jabber:config:msntrans");

    if (cfg == NULL)
    {
        log_error(ti->i->id,"Configuration not found!");
        return 1;
    }

    if (/*mt_init_servers(ti,xmlnode_get_tag(cfg,"servers"))||*/
        mt_init_conference(ti,xmlnode_get_tag(cfg,"conference")))
        return 1;
    ti->attempts_max = 5;

    ti->reg = pstrdup(ti->p,xmlnode_get_tag_data(cfg,"instructions"));
    if (ti->reg == NULL)
    {
        log_error(ti->i->id,"No instructions configured");
        return 1;
    }

    mt_init_curl(ti, xmlnode_get_tag(cfg, "curl"));

    ti->fancy_friendly = xmlnode_get_tag(cfg,"fancy_friendly") ? 1 : 0;
    ti->inbox_headlines = xmlnode_get_tag(cfg,"headlines") ? 1 : 0;
    ti->vcard = xmlnode_new_tag_pool(ti->p,"vCard");
    xmlnode_put_attrib(ti->vcard,"xmlns",NS_VCARD);
    xmlnode_insert_node(ti->vcard,xmlnode_get_firstchild(xmlnode_get_tag(cfg,"vCard")));
    ti->admin = xmlnode_dup(xmlnode_get_tag(cfg,"admin"));
    ti->sessions = xhash_new(SESSION_TABLE_SZ);
    ti->start = time(NULL);

    mt_iq_init(ti);
    mt_stream_init();

    xmlnode_free(cfg);

    return 0;
}

void msntrans(instance i, xmlnode unused)
{
    mti ti;

    log_debug(ZONE,"MSN Transport loading section '%s'",i->id);

    /* create a new msn-t instance */
    ti = pmalloco(i->p,sizeof(_mti));
    ti->i = i;
    ti->p = i->p;

    if (mt_init(ti) == 0)
    {
        register_phandler(i,o_DELIVER,&mt_receive,(void *) ti);
        register_shutdown(&mt_shutdown,(void *) ti);
        register_beat(5,&mt_chat_docomposing, (void *)ti);

        if (debug_flag)
            register_beat(60,&mt_debug,(void *) ti);
    }
}
