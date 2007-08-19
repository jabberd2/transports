/*
 * Jabber AIM-Transport libfaim interface
 *
*/

#include "aimtrans.h"

int at_server_s10n(ati ti, jpacket jp)
{
    /* These are basically functions to tell you that you're stupid */
    log_debug(ZONE, "Handling server subscription.");
    switch(jpacket_subtype(jp))
    {
        case JPACKET__SUBSCRIBED:
        case JPACKET__UNSUBSCRIBED:
            /* XXX: Do what for these? */
        case JPACKET__UNSUBSCRIBE:
        case JPACKET__SUBSCRIBE:
            /* XXX:  You must have a session to do both of these */
            jutil_error(jp->x, TERROR_REGISTER);
            at_deliver(ti,jp->x);
            return 1;
        default:
            jutil_error(jp->x, TERROR_NOTACCEPTABLE);
            at_deliver(ti,jp->x);
            return 1;
    }
    return 0;
}

int at_session_s10n(at_session s, jpacket jp)
{
    ati ti;
    xmlnode x;

    ti = s->ti;

    log_debug(ZONE, "Handling session subscription");
    switch(jpacket_subtype(jp))
    {
        case JPACKET__SUBSCRIBE:
            if(jp->to->user != NULL)
            {
                return at_buddy_add(ti, jp);
            } else {
                x = jutil_presnew(JPACKET__SUBSCRIBED, jid_full(jp->from), NULL);
                jutil_tofrom(x);
                xmlnode_put_attrib(x, "from", jid_full(jp->to));
                at_deliver(ti,x);
                return 0;
            }
        case JPACKET__UNSUBSCRIBE:
            /*XXX Do more with this 
            * (Actually take the user off the aim roster)
            * should we check for a subscription as well? 
            */
            xhash_zap(s->buddies, jp->to->user);
            aim_remove_buddy(s->ass, aim_getconn_type(s->ass,
                            AIM_CONN_TYPE_BOS), jp->to->user);
            log_debug(ZONE, "[AIM] Unsubscribing\n");
            x = jutil_presnew(JPACKET__UNSUBSCRIBED, jid_full(jp->from), "Unsubscribed");
            xmlnode_put_attrib(x, "from", jid_full(jp->to));
            at_deliver(ti,x);
            xmlnode_free(jp->x);
            return 1;
        /* Do I need to do anything? */
        case JPACKET__SUBSCRIBED:
        case JPACKET__UNSUBSCRIBED:
        default:
            xmlnode_free(jp->x);
            return 1;
    }
    xmlnode_free(jp->x);
    return 1;
}
