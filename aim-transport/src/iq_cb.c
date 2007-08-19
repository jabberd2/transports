/*
 * Jabber AIM-Transport libfaim interface
 *
*/

#include "aimtrans.h"
#include "config.h"

int at_iq_vcard(ati ti, jpacket jp)
{
    xmlnode data;
    at_session s;

    s = at_session_find_by_jid(ti, jp->from);

    if(jpacket_subtype(jp) != JPACKET__GET ||
      (s && ((!s->icq && jp->to->user) || (s->icq && s->icq_vcard_response))))
    {
        at_bounce(ti, jp, TERROR_BAD);
        return 1;
    }

    if(!jp->to->user)
    {
        xmlnode_insert_node(jutil_iqresult(jp->x),ti->vcard);
        at_deliver(ti,jp->x);
        return 1;
    }

    if(!s)
        return 0;

    jutil_iqresult(jp->x);
    jp->iq = data = xmlnode_insert_tag(jp->x,"vCard");
    xmlnode_put_attrib(data,"xmlns",NS_VCARD);
    xmlnode_put_attrib(data,"version","3.0");
    xmlnode_put_attrib(data,"prodid","-//HandGen//NONSGML vGen v1.0//EN");
    s->icq_vcard_response = jp;

    aim_icq_getsimpleinfo(s->ass,
                          jp->to->user);
    return 1;
}

int at_iq_last(ati ti, jpacket jp)
{
    xmlnode last;
    xmlnode q;
    char str[10];

    /* XXX I can do last if I track logouts in the XDB... not too hard */
    if(jpacket_subtype(jp) != JPACKET__GET)
    {
        at_bounce(ti, jp, TERROR_BAD);
        return 1;
    }

    if(jp->to->user != NULL)
    {
        at_session s;
        at_buddy buddy;
        char *res;

        s = at_session_find_by_jid(ti, jp->from);
        if(s == NULL)
        {
            at_bounce(ti, jp, TERROR_REGISTER);
            return 1;
        }
        buddy = xhash_get(s->buddies, jp->to->user);
        if(buddy == NULL)
        {
            at_bounce(ti, jp, TERROR_BAD);
            return 1;
        }
         
        jutil_iqresult(jp->x);
        last = xmlnode_insert_tag(jp->x, "query");
        xmlnode_put_attrib(last,"xmlns",NS_LAST);
        sprintf(str, "%d", buddy->idle_time);
        xmlnode_put_attrib(last, "seconds", str);
        at_deliver(ti,jp->x);

        return 1;
    }

    jutil_iqresult(jp->x);
    last = xmlnode_insert_tag(jp->x, "query");
    xmlnode_put_attrib(last,"xmlns",NS_LAST);
    sprintf(str, "%d", time(NULL) - ti->start_time);
    xmlnode_put_attrib(last,"seconds", str);
    at_deliver(ti,jp->x);

    return 1;
}

int at_iq_gateway(ati ti, jpacket jp)
{
    if(jp->to->user != NULL)
    {
        at_bounce(ti, jp, TERROR_NOTALLOWED);
        return 1;
    }

    
    switch(jpacket_subtype(jp))
    {
        case JPACKET__GET:
        {
            xmlnode q;

            jutil_iqresult(jp->x);

            q = xmlnode_insert_tag(jp->x,"query");
            xmlnode_put_attrib(q,"xmlns",NS_GATEWAY);
            xmlnode_insert_cdata(xmlnode_insert_tag(q,"desc"),"Enter the user's screenname",-1);
            xmlnode_insert_tag(q,"prompt");

            break;
        }

        case JPACKET__SET:
        {
            char *user, *id;
    
            user = xmlnode_get_tag_data(jp->iq,"prompt");
            id = user ? spools(jp->p,at_normalize(user),"@",jp->to->server,jp->p) : NULL;
            if (id)
            {
                xmlnode q;

                jutil_iqresult(jp->x);
        
                q = xmlnode_insert_tag(jp->x,"query");
                xmlnode_put_attrib(q,"xmlns",NS_GATEWAY);
                xmlnode_insert_cdata(xmlnode_insert_tag(q,"prompt"),id,-1);
            }
            else
                jutil_error(jp->x,TERROR_BAD);
            break;
        }
        default:
            jutil_error(jp->x,TERROR_BAD);
            break;
    }
    
    at_deliver(ti,jp->x);
    return 1;
}

int at_iq_time(ati ti, jpacket jp)
{
    xmlnode x, q;
    time_t t;
    char *tstr;

    if(jpacket_subtype(jp) != JPACKET__GET)
    {
        at_bounce(ti, jp, TERROR_BAD);
        return 1;
    }
    x = jutil_iqresult(jp->x);

    q = xmlnode_insert_tag(x,"query");
    xmlnode_put_attrib(q,"xmlns",NS_TIME);

    xmlnode_insert_cdata(xmlnode_insert_tag(q,"utc"),jutil_timestamp(),-1);
    xmlnode_insert_cdata(xmlnode_insert_tag(q,"tz"),tzname[0],-1);

    /* create nice display time */
    t = time(NULL);
    tstr = ctime(&t);
    tstr[strlen(tstr) - 1] = '\0'; /* cut off newline */
    xmlnode_insert_cdata(xmlnode_insert_tag(q,"display"),tstr,-1);

    at_deliver(ti,x);
    return 1;
}

int at_iq_version(ati ti, jpacket jp)
{
    xmlnode os, x, q;
    struct utsname un;

    if(jpacket_subtype(jp) != JPACKET__GET)
    {
        at_bounce(ti, jp, TERROR_BAD);
        return 1;
    }

    x = jutil_iqresult(jp->x);

    q = xmlnode_insert_tag(x,"query");
    xmlnode_put_attrib(q,"xmlns",NS_VERSION);

    xmlnode_insert_cdata(xmlnode_insert_tag(q,"name"),"AIM Transport",-1);
    xmlnode_insert_cdata(xmlnode_insert_tag(q,"version"), VERSION,-1);

    uname(&un);
    os = xmlnode_insert_tag(q,"os");
    xmlnode_insert_cdata(os,un.sysname,-1);
    xmlnode_insert_cdata(os," ",1);
    xmlnode_insert_cdata(os,un.release,-1);

    at_deliver(ti,x);
    // AGAIN!?!
    return 1;
}

int at_iq_search(ati ti, jpacket jp)
{
    at_bounce(ti, jp, TERROR_NOTIMPL);
    return 1;
}

int at_iq_browse(ati ti, jpacket jp)
{
    xmlnode q;

    if(jpacket_subtype(jp) != JPACKET__GET)
    {
        at_bounce(ti, jp, TERROR_BAD);
        return 1;
    }

    if(jp->to->user != NULL)
    {
        q = xmlnode_insert_tag(jutil_iqresult(jp->x),"item");
        xmlnode_put_attrib(q,"xmlns",NS_BROWSE);

        xmlnode_put_attrib(q,"category","user");
        xmlnode_put_attrib(q,"type","client");
        xmlnode_put_attrib(q,"jid",jid_full(jp->to));
        xmlnode_put_attrib(q,"name", jp->to->user);

        at_deliver(ti,jp->x);
        return 1;
    }
    
    q = xmlnode_insert_tag(jutil_iqresult(jp->x),"item");
    xmlnode_put_attrib(q,"xmlns",NS_BROWSE);

    xmlnode_put_attrib(q,"category","service");
    xmlnode_put_attrib(q,"type","aim");
    xmlnode_put_attrib(q,"jid",jp->to->server);
    xmlnode_put_attrib(q,"name",xmlnode_get_tag_data(ti->vcard,"FN"));

    xmlnode_insert_cdata(xmlnode_insert_tag(q,"ns"),NS_DISCO_INFO,-1);
    xmlnode_insert_cdata(xmlnode_insert_tag(q,"ns"),NS_DISCO_ITEMS,-1);
    xmlnode_insert_cdata(xmlnode_insert_tag(q,"ns"),NS_REGISTER,-1);
    xmlnode_insert_cdata(xmlnode_insert_tag(q,"ns"),NS_GATEWAY,-1);

    at_deliver(ti,jp->x);

    // WHY! DAMNIT! WHY?
    return 1;
}


int at_iq_disco_items(ati ti, jpacket jp)
{
    xmlnode q;

    if(jpacket_subtype(jp) != JPACKET__GET)
    {
        at_bounce(ti, jp, TERROR_BAD);
        return 1;
    }

    if(xmlnode_get_attrib(xmlnode_get_tag(jp->x, "query"),"node") != NULL)
    {
        at_bounce(ti, jp, TERROR_NOTALLOWED);
        return 1;
    }

    jutil_iqresult(jp->x);
    q = xmlnode_insert_tag(jp->x, "query");
    xmlnode_put_attrib(q, "xmlns", NS_DISCO_ITEMS);

    at_deliver(ti,jp->x);

    return 1;

}

int at_iq_disco_info(ati ti, jpacket jp)
{
    xmlnode q, info;

    if(jpacket_subtype(jp) != JPACKET__GET)
    {
        at_bounce(ti, jp, TERROR_BAD);
        return 1;
    }

    if(xmlnode_get_attrib(xmlnode_get_tag(jp->x, "query"),"node") != NULL)
    {
        at_bounce(ti, jp, TERROR_NOTALLOWED);
        return 1;
    }

    if(jp->to->user != NULL)
    {
        q = xmlnode_insert_tag(jutil_iqresult(jp->x),"query");
        xmlnode_put_attrib(q,"xmlns",NS_DISCO_INFO);

	info = xmlnode_insert_tag(q, "identity");
	xmlnode_put_attrib(info, "category", "client");
	xmlnode_put_attrib(info, "type", "pc");
	xmlnode_put_attrib(info, "name", jp->to->user);

	info = xmlnode_insert_tag(q, "feature");
	xmlnode_put_attrib(info, "var", NS_VCARD);

	info = xmlnode_insert_tag(q, "feature");
	xmlnode_put_attrib(info, "var", NS_LAST);

	info = xmlnode_insert_tag(q, "feature");
	xmlnode_put_attrib(info, "var", NS_TIME);

	info = xmlnode_insert_tag(q, "feature");
	xmlnode_put_attrib(info, "var", NS_VERSION);
	
        at_deliver(ti,jp->x);
        return 1;
    }
 
    q = xmlnode_insert_tag(jutil_iqresult(jp->x),"query");
    xmlnode_put_attrib(q,"xmlns",NS_DISCO_INFO);

    info = xmlnode_insert_tag(q, "identity");
    xmlnode_put_attrib(info, "category", "gateway");
    xmlnode_put_attrib(info, "type", "aim");
    xmlnode_put_attrib(info,"name",xmlnode_get_tag_data(ti->vcard,"FN"));

    info = xmlnode_insert_tag(q, "feature");
    xmlnode_put_attrib(info, "var", NS_VCARD);

    info = xmlnode_insert_tag(q, "feature");
    xmlnode_put_attrib(info, "var", NS_LAST);

    info = xmlnode_insert_tag(q, "feature");
    xmlnode_put_attrib(info, "var", NS_TIME);

    info = xmlnode_insert_tag(q, "feature");
    xmlnode_put_attrib(info, "var", NS_VERSION);

    info = xmlnode_insert_tag(q, "feature");
    xmlnode_put_attrib(info, "var", NS_GATEWAY);

    info = xmlnode_insert_tag(q, "feature");
    xmlnode_put_attrib(info, "var", NS_REGISTER);
    
    at_deliver(ti,jp->x);
    return 1;
}
