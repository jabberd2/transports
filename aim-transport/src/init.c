/*
 * Jabber AIM-Transport libfaim interface
 *
*/

#include "aimtrans.h"

/* handle a packet from jabberd */
result at_phandler(instance i,dpacket p,void *arg)
{
    int ret;
    ati ti;
    at_mtq_data amd;

    if(i == NULL || p == NULL) 
        return r_ERR;

    ti = (ati)arg;

    switch(p->type)
    {
    case p_NONE:
    case p_NORM:
        log_debug(ZONE, "[AT] we got a packet from jabberd: %s", xmlnode2str(p->x));
        amd = pmalloco(p->p, sizeof(_at_mtq_data));
        amd->jp = jpacket_new(p->x);
        amd->p = p->p;
        amd->ti = ti;

        mtq_send(NULL, p->p, at_parse_packet, (void *)amd);
        return r_DONE;
    default:
        log_debug(ZONE, "[AT] ignoring packet from jabberd: %s", xmlnode2str(p->x));
        return r_PASS;
    }
}

void at_bounce(ati ti, jpacket p, terror terr)
{
    char *to, *from;
    xmlnode err, x = p->x;
    char code[4];

    to = xmlnode_get_attrib(x, "to");
    from = xmlnode_get_attrib(x, "from");
    xmlnode_put_attrib(x, "from", to);
    xmlnode_put_attrib(x, "to", from);

    if(p->type == JPACKET_S10N && jpacket_subtype(p) == JPACKET__SUBSCRIBE)
    { /* bounce subscriptions only */
        xmlnode_put_attrib(x, "type", "unsubscribed");
        xmlnode_insert_cdata(xmlnode_insert_tag(x, "status"), terr.msg, -1);

        at_deliver(ti,x);
        return;
    }

    if(jpacket_subtype(p) == JPACKET__ERROR || p->type == JPACKET_PRESENCE || p->type == JPACKET_S10N)
    { /* presence/error packets are just dropped */
        xmlnode_free(x);
        return;
    }

    xmlnode_put_attrib(x,"type", "error");
    err = xmlnode_insert_tag(x, "error");

    snprintf(code, 4, "%d", terr.code);
    xmlnode_put_attrib(err, "code", code);
    if(terr.msg != NULL)
        xmlnode_insert_cdata(err, terr.msg, strlen(terr.msg));

    at_deliver(ti,x);
}

void _at_shutdown(xht x, const char *key, void *data, void *arg)
{
    ati ti = (ati)arg;
    at_session s = (at_session)data;
    xmlnode message;

    message = xmlnode_new_tag("message");
    xmlnode_put_attrib(message, "from", jid_full(s->from));
    xmlnode_put_attrib(message,"type", "error");
    xmlnode_put_attrib(message, "to", jid_full(s->cur));
    xmlnode_insert_cdata(xmlnode_insert_tag(message, "error"), "AIM Transport shutting down.", -1);
    xmlnode_put_attrib(xmlnode_get_tag(message, "error"), "code", "502");

    at_deliver(ti,message);

    at_session_end(s);

	iconv_close( fromutf8 );
	iconv_close( toutf8 );

    return;
}

void at_shutdown(void *arg)
{
    ati ti = (ati)arg;

    xhash_walk(ti->session__list, &_at_shutdown, (void*)ti);
    xhash_free(ti->session__list);
}

void aim_transport(instance i, xmlnode x)
{
    ati ti;
    xmlnode config;
	char *eightbitcode, *utf8code = "UTF-8", *latin1code = "CP1252";
    
    ti = pmalloco(i->p, sizeof(_ati));

    ti->i = i;
    ti->xc = xdb_cache(i);
    log_notice(i->id, "AIM-Transport starting up for instance %s...", i->id);
    
    config = xdb_get(ti->xc, jid_new(xmlnode_pool(x), "config@-internal"), "jabber:config:aimtrans");
    
    ti->vcard = xmlnode_new_tag_pool(i->p,"vCard");
    xmlnode_put_attrib(ti->vcard,"xmlns",NS_VCARD);
    xmlnode_insert_node(ti->vcard,xmlnode_get_firstchild(xmlnode_get_tag(config,"vCard")));

    ti->start_time = time(NULL);
    

    ti->session__list=xhash_new(101);
    ti->iq__callbacks = xhash_new(23);
    ti->pending__buddies = xhash_new(101);

    /* The aim.exe binary should not be necessary any more. */
    ti->aimbinarydir = pstrdup(i->p, xmlnode_get_tag_data(config, "aimbinarydir"));

	eightbitcode = pstrdup(i->p, xmlnode_get_tag_data(config, "charset"));
	if( eightbitcode == NULL )
	{
		log_notice( i->id, "Charset is not specified, using CP1252" );
		eightbitcode = latin1code;
	}

    xmlnode_free(config);

	fromutf8 = iconv_open(eightbitcode, utf8code);
	if(fromutf8 == (iconv_t)(-1))
	{
		log_error(i->id, "Conversion from %s to %s is not supported", utf8code, eightbitcode);
		raise(SIGINT);
	}
	toutf8 = iconv_open(utf8code, eightbitcode);
	if(toutf8 == (iconv_t)(-1))
	{
		log_error(i->id, "Conversion from %s to %s is not supported", eightbitcode, utf8code);
		raise(SIGINT);
	}

    ti->send_buf = NULL;
    ti->modname = NULL;

    pth_mutex_init(&ti->buddies_mutex);

    at_init_iqcbs(ti);

    register_phandler(i, o_DELIVER, at_phandler, ti);
    pool_cleanup(i->p, at_shutdown, (void*)i);
}
