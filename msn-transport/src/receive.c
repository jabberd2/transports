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

/* bounces packet for unknown users with the appropriate error */
void mt_unknown_bounce(void *arg)
{
    jpacket jp = (jpacket) arg;
    mti ti = (mti) jp->aux1;
    xmlnode reg;

    lowercase(jp->from->user);
    lowercase(jp->from->server);

    if ((reg = xdb_get(ti->xc,mt_xdb_id(jp->p,jp->from,jp->to->server),NS_REGISTER)) != NULL)
    {
        xmlnode p = xmlnode_new_tag("presence");
        xmlnode_put_attrib(p,"to",jid_full(jp->from));
        xmlnode_put_attrib(p,"from",jp->to->server);
        xmlnode_put_attrib(p,"type","probe");

        mt_deliver(ti,p);

        jutil_error(jp->x,TERROR_NOTFOUND);
        xmlnode_free(reg);
    }
    else
        jutil_error(jp->x,TERROR_REGISTER);

    mt_deliver(ti,jp->x);
}

void mt_unknown_process(mti ti, jpacket jp)
{
    switch (jp->type)
    {
    case JPACKET_IQ:
        if (jp->to->user != NULL || (ti->con && j_strcmp(ti->con_id,jp->to->server) == 0))
        {
            jp->aux1 = (void *) ti;
            mtq_send(NULL,jp->p,&mt_unknown_bounce,(void *) jp);
        }
        else
        {
            if (j_strcmp(jp->iqns,NS_REGISTER) == 0)
            {
                jp->aux1 = (void *) ti;
                mtq_send(NULL,jp->p,&mt_reg_unknown,(void *) jp);
            }
            else
                mt_iq_server(ti,jp);
        }
        break;

    case JPACKET_MESSAGE:
    case JPACKET_S10N:
        /* message and s10n aren't allowed without a valid session */
        if (jp->to->user != NULL || jpacket_subtype(jp) != JPACKET__SUBSCRIBED)
        {
            jp->aux1 = (void *) ti;
            mtq_send(NULL,jp->p,&mt_unknown_bounce,(void *) jp);
            break;
        }
        /* fall through here to free the node */

    case JPACKET_PRESENCE:
        if (jpacket_subtype(jp) == JPACKET__AVAILABLE && jp->to->user == NULL)
        {
            jp->aux1 = (void *) ti;
            mtq_send(NULL,jp->p,&mt_presence_unknown,(void *) jp);
            break;
        }
        xmlnode_free(jp->x);
    }
}

result mt_receive(instance i, dpacket d, void *arg)
{
    jpacket jp;
    mti ti;
    session s;

    switch(d->type)
    {
    case p_NONE:
    case p_NORM:
        jp = jpacket_new(d->x);
        if (jp->from && jp->from->user && jp->type != JPACKET_UNKNOWN &&
            jpacket_subtype(jp) != JPACKET__ERROR)
        {
            mti ti = (mti) arg;
            session s = mt_session_find(ti,jp->from);

            lowercase(jp->from->server);
            lowercase(jp->from->user);


            if (s != NULL)
                mt_session_process(s,jp);
            else
                mt_unknown_process(ti,jp);
        }
        else
        {
            log_warn(NULL,"Invalid packet");
            xmlnode_free(d->x);
        }
        break;

    default:
        return r_ERR;

    }

    return r_DONE;
}
