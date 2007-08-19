/*
 * Jabber AIM-Transport libfaim interface
 *
*/

#include "aimtrans.h"

void at_parse_packet(void *arg)
{
    at_mtq_data amd;
    at_session s;
    xmlnode dupx;
    char *ns;
    unsigned char *user;
    int ret=0;
    jpacket jp;
    ati ti;

    amd = (at_mtq_data)arg;
    ti = amd->ti;
    jp = amd->jp;

    /* JID user part should be case insensitive */
    /* convert user part of from JID to lower case */
    if(jp->from->user != NULL)
        for(user = jp->from->user; *user != '\0'; user++)
            if(*user < 128)
                *user = tolower(*user);
    /* Mangle "from" JID, save original attribute for migration */
    xmlnode_put_attrib(jp->x, "origfrom", xmlnode_get_attrib(jp->x, "from"));
    xmlnode_put_attrib(jp->x, "from", jid_full(jp->from));

    log_debug(ZONE, "[AT] parsing packet for %s", jid_full(jp->from));
    s = at_session_find_by_jid(ti, jp->from);
    if(s != NULL)
    {
        log_debug(ZONE, "Packet sent to session parser");
        at_psend(s->mp_to, jp);
        return;
    }

    switch(jp->type)
    {
        case JPACKET_IQ:
            if(NSCHECK(xmlnode_get_tag(jp->x, "query"),NS_REGISTER))
            {
                /* Register the user */
                ret =  at_register(ti, jp);
                break;
            }
            ns = xmlnode_get_attrib(xmlnode_get_tag(jp->x, "query"), "xmlns");
            ret = at_run_iqcb(ti, ns, jp);
            if(ret < 0)
            {
                /* No IQ callback of that type */
                jutil_error(jp->x, TERROR_NOTFOUND);
                at_deliver(ti,jp->x);
                ret = 1;
            }
            break;
        case JPACKET_S10N:
            ret = at_server_s10n(ti, jp);
            break;
        case JPACKET_PRESENCE:
            ret = at_server_pres(ti, jp);
            break;
        case JPACKET_MESSAGE:
            log_debug(ZONE, "[AT] Got message bound to the server: %s", xmlnode2str(jp->x));
            xmlnode_free(jp->x);
            ret = 1;
            break;
        default:
            jutil_error(jp->x, TERROR_NOTACCEPTABLE);
            at_deliver(ti,jp->x);
            ret = 1;
            break;
    }

    if(ret == 0)
    {
        xmlnode_free(jp->x);
    }
}
