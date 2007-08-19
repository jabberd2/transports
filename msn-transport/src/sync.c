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

result mt_sync_chg(mpacket mp, void *arg)
{
    session s = (session) arg;
    char *cmd;

    cmd = mt_packet_data(mp,0);

    if (j_strcmp(cmd,"CHG") == 0)
    {
        mtq_send(s->q,s->p,&mt_session_connected,(void *) s);
        return r_DONE;
    }

    if (j_strcmp(cmd,"ILN") == 0)
    {
        mt_user_update(s,mt_packet_data(mp,3),
                       mt_packet_data(mp,2),
                       mt_packet_data(mp,4));
        return r_PASS;
    }

    return r_ERR;
}

void mt_user_sync_walk(xht h, const char *key, void *data, void *arg)
{
    xmlnode roster = (xmlnode) arg;
    muser u = (muser) data;
    session s = (session) xmlnode_get_vattrib(roster,"s");
    xmlnode x;
    int chg = 0;

    if (!(u->list_old & LIST_FL))
    {
        if (u->list & LIST_FL)
        {
            chg = 1;
            x = jutil_presnew(JPACKET__SUBSCRIBED,jid_full(s->id),NULL);
            xmlnode_put_attrib(x,"from",mt_mid2jid_full(x->p,u->mid,s->host));
            mt_deliver(s->ti,x);
        }
    }
    else
    {
        if (!(u->list & LIST_FL))
        {
            chg = 1;
            x = jutil_presnew(JPACKET__UNSUBSCRIBED,jid_full(s->id),NULL);
            xmlnode_put_attrib(x,"from",mt_mid2jid_full(x->p,u->mid,s->host));
            mt_deliver(s->ti,x);
        }
    }

    if (!(u->list_old & LIST_RL))
    {
        if (u->list & LIST_RL)
        {
            chg = 1;
            if ((u->list & LIST_BL) == 0)
            {
                x = jutil_presnew(JPACKET__SUBSCRIBE,jid_full(s->id),NULL);
                xmlnode_put_attrib(x,"from",mt_mid2jid_full(x->p,u->mid,s->host));
                mt_deliver(s->ti,x);
            }
        }
    }
    else
    {
        if (!(u->list & LIST_RL))
        {
            chg = 1;
            x = jutil_presnew(JPACKET__UNSUBSCRIBE,jid_full(s->id),NULL);
            xmlnode_put_attrib(x,"from",mt_mid2jid_full(x->p,u->mid,s->host));
            mt_deliver(s->ti,x);
        }
    }

    if (chg)
    {
        xmlnode item = xmlnode_get_tag(roster,spools(roster->p,"?jid=",u->mid,roster->p));

        if ((u->list & LIST_FL) || (u->list & LIST_RL))
        {
            if (item == NULL)
            {
                item = xmlnode_insert_tag(roster,"item");
                xmlnode_put_attrib(item,"jid",u->mid);
            }

            if (u->list & LIST_RL)
            {
                if (u->list & LIST_FL)
                    xmlnode_put_attrib(item,"subscription","both");
                else
                    xmlnode_put_attrib(item,"subscription","from");
            }
            else
                xmlnode_put_attrib(item,"subscription","to");
        }
        else if (item)
            xmlnode_hide(item);
    }
    u->list_old = 0;
}

void mt_user_sync_final(void *arg)
{
    session s = (session) arg;
    mti ti = s->ti;
    xmlnode roster, cur;
    muser u;
    jid xid;
    char *id, *s10n;

    s->currentcontact = 0;
    s->numcontacts = 0;

    xid = mt_xdb_id(s->p,s->id,s->host);
    roster = xdb_get(ti->xc,xid,NS_ROSTER);

    if (roster == NULL)
    {
        roster = xmlnode_new_tag("query");
        xmlnode_put_attrib(roster,"xmlns",NS_ROSTER);
    }

    for_each_node(cur,roster)
    {
        if ((id = xmlnode_get_attrib(cur,"jid")) == NULL ||
            (s10n = xmlnode_get_attrib(cur,"subscription")) == NULL)
            continue;

        u = mt_user(s,id);
        if (strcmp(s10n,"to") == 0)
            u->list_old = LIST_FL;
        else if (strcmp(s10n,"from") == 0)
            u->list_old = LIST_RL;
        else if (strcmp(s10n,"both") == 0)
            u->list_old = (LIST_RL | LIST_FL);
    }

    xmlnode_put_vattrib(roster,"s",(void *) s);
    xhash_walk(s->users,&mt_user_sync_walk,(void *) roster);
    xmlnode_hide_attrib(roster,"s");

    xdb_set(ti->xc,xid,NS_ROSTER,roster);
    xmlnode_free(roster);

    if (s->exit_flag == 0)
    {
        /* send our initial status */
        mt_stream_register(s->st,&mt_sync_chg,(void *) s);
        mt_cmd_chg(s->st,mt_state2char(s->state));
        mt_send_saved_friendly(s);
    }
}

result mt_user_lst(session s, mpacket mp)
{
		int i = 0;

		s->currentcontact++;

// Example packet
// LST someuser@hotmail.com Friendly%20Name 11 0

		if(mp->count > 4)
		{
			muser u = mt_user(s, mt_packet_data(mp,1));
			char *h = mt_packet_data(mp,2);

			switch(j_atoi(mt_packet_data(mp, 3), 0))
			{
					/*
						The lists have these values
							AL 2
							BL 4
							FL 1
							RL 8
					*/

				case 15:
					u->list |= LIST_FL;
					u->list |= LIST_AL;
					u->list |= LIST_BL;
					u->list |= LIST_RL;
					break;

				case 11:
					u->list |= LIST_FL;
					u->list |= LIST_AL;
					//u->list |= LIST_BL;
					u->list |= LIST_RL;
					break;

				case 3:
						u->list |= LIST_FL;
						u->list |= LIST_AL;
						//u->list |= LIST_BL;
						//u->list |= LIST_RL;
						break;

				case 10:
						//u->list |= LIST_FL;
						u->list |= LIST_AL;
						//u->list |= LIST_BL;
						u->list |= LIST_RL;
						break;

				case 13:
						u->list |= LIST_FL;
						//u->list |= LIST_AL;
						u->list |= LIST_BL;
						u->list |= LIST_RL;
						break;

				case 12:
						//u->list |= LIST_FL;
						//u->list |= LIST_AL;
						u->list |= LIST_BL;
						u->list |= LIST_RL;
						break;

				case 5:
						u->list |= LIST_FL;
						//u->list |= LIST_AL;
						u->list |= LIST_BL;
						//u->list |= LIST_RL;
						break;

				case 2:
						//u->list |= LIST_FL;
						u->list |= LIST_AL;
						//u->list |= LIST_BL;
						//u->list |= LIST_RL;
						break;

				case 4:
						//u->list |= LIST_FL;
						//u->list |= LIST_AL;
						u->list |= LIST_BL;
						//u->list |= LIST_RL;
						break;

				default:
					log_debug(ZONE,"User %s recieved unknown value for LST: %d",s->user, j_atoi(mt_packet_data(mp, 3), 0));
			}

			if (strcmp(h,u->handle))
			{
					mt_free(u->handle);
					u->handle = mt_strdup(h);
			}
		}

		if(s->numcontacts == s->currentcontact && s->numcontacts > 0) {
			// all the lists have been received
			mtq_send(s->q,s->p,&mt_user_sync_final,(void *) s);
		}

    return r_DONE;
}

result mt_user_syn(mpacket mp, void *arg)
{
    session s = (session) arg;
    char *cmd;

    cmd = mt_packet_data(mp,0);


    if (j_strcmp(cmd,"LST") == 0)
    {
        return mt_user_lst(s,mp);
    }
    else if (j_strcmp(cmd,"SYN") == 0)
    {
        // Set the number of contacts
        s->currentcontact = 0;
        if(mp->count > 3)
          s->numcontacts = j_atoi(mt_packet_data(mp, 3), 1);
        if (j_atoi(mt_packet_data(mp,2),-1) == 0)
        {
            // No LST commands will be received because the Seq number is zero
            mt_stream_register(s->st,&mt_sync_chg,(void *) s);
            mt_cmd_chg(s->st,mt_state2char(s->state));
            return r_DONE;
        }
    }
    else if (j_strcmp(cmd,"GTC") && j_strcmp(cmd,"BLP"))
        return r_ERR;

    return r_PASS;
}

void mt_user_sync(session s)
{
    if (s->users == NULL)
        s->users = xhash_new(25);

    s->currentcontact = 0;
    s->numcontacts = 0;

    mt_stream_register(s->st,&mt_user_syn,(void *) s);
    mt_cmd_syn(s->st);
}


