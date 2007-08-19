/*
* Jabber AIM-Transport libfaim interface
*
*/

#include "aimtrans.h"
#include "utf8.h"

int at_parse_incoming_im(aim_session_t *ass,
                         aim_frame_t *command, ...)
{
    int channel;
    va_list ap;
    aim_userinfo_t *userinfo;
    at_session s;
    ati ti;
    at_buddy buddy;
    unsigned int idle;
    char *utf8_str, *msg_plain, *msg_xhtml;
    utf8_str = malloc(8192);  // don't forget to free this!
    msg_plain = malloc(8192);
    msg_xhtml = malloc(8192);

    s = (at_session)ass->aux_data;
    ti = s->ti;

    va_start(ap, command);
    channel = va_arg(ap, int);
    userinfo = va_arg(ap, aim_userinfo_t *);

    buddy = xhash_get(s->buddies, at_normalize(userinfo->sn));

    //how long since buddy sent an im?
    if (buddy != NULL)
        idle = ((unsigned int)time(NULL) - (unsigned int)buddy->lastactivity);

    /*
    * Channel 1: Standard Message
    */
    if (channel == 1)
    {
        struct aim_incomingim_ch1_args *args;
        char *msg;
        char *fullmsg, *fullmsg_xhtml;
        int i;
        u_int icbmflags = 0;
        u_short flag1, flag2;
        jpacket jp;
        xmlnode par;
        xmlnode node, node_xhtml;
        jid from;

        args = va_arg(ap, struct aim_incomingim_ch1_args *);
        va_end(ap);

#if 0

        if (args->icbmflags & AIM_IMFLAG_HASICON)
        {
            if (args->iconstamp > buddy->iconstamp)
            {
                // Send the IM to req it?
            }
        }
#endif
        par = xmlnode_new_tag("message");
        xmlnode_put_attrib(par, "type", "chat");
        xmlnode_put_attrib(par, "to", jid_full(s->cur));
        xmlnode_put_attrib(par, "from", ti->i->id);
        jp = jpacket_new(par);

        msg = args->msg;
        icbmflags = args->icbmflags; // this seemed to be missing?

        // only convert if AIM
        if (!s->icq)
        {
            if (icbmflags & AIM_IMFLAGS_UNICODE)
            {
                unicode_to_utf8(msg, (args->msglen) / 2, utf8_str, 8192);
                msg = utf8_str;
            }
            else
            {
                msg = (char *)str_to_UTF8(jp->p, (unsigned char*)msg);
            }

            msgconv_aim2plain(msg, msg_plain, 8192);
            msgconv_aim2xhtml(msg, msg_xhtml, 8192);
        }
        else
        {
            msg = (char *)str_to_UTF8(jp->p, (unsigned char *)msg);
            strncpy(msg_plain, msg, 8191);
            strncpy(msg_xhtml, msg, 8191);
            msg_plain[8191] = 0;
            msg_xhtml[8191] = 0;
        }

        node = xmlnode_insert_tag(jp->x, "body");
        //node_xhtml = xmlnode_insert_tag(jp->x, "html");

        fullmsg = pmalloco(xmlnode_pool(node), strlen(msg_plain) + 30);
        //fullmsg_xhtml = pmalloco(xmlnode_pool(node_xhtml), strlen(msg_xhtml) + 30);
        fullmsg[0] = 0;
        //fullmsg_xhtml[0] = 0;

        // Autoreply?
        if (icbmflags & AIM_IMFLAGS_AWAY)
        {
            strcat(fullmsg, "(AutoReply) ");
            //strcat(fullmsg_xhtml, "(AutoReply) ");
        }
        strcat(fullmsg, msg_plain);
        //strcat(fullmsg_xhtml, msg_xhtml);

        // Sending autoreply?
        if (s->away && buddy != NULL &&
                ((s->awaysetat != buddy->sawawaymsg) || (idle > 300)) &&
                (!(icbmflags & AIM_IMFLAGS_AWAY)) &&
                !s->icq)
        {
            struct aim_sendimext_args args;
            unsigned char *unistr;
            unsigned short unilen;

            unistr = malloc(8192);

            args.destsn = userinfo->sn;
            args.flags = s->icq ? AIM_IMFLAGS_OFFLINE : 0;
            args.flags |= AIM_IMFLAGS_AWAY;

            //aim user will now see away msg:
            buddy->sawawaymsg = s->awaysetat;

            // simple enough to be ascii ?
            if (isAscii(s->status))
            {
                args.msg = s->status;
                args.msglen = strlen(s->status);
            }
            // unicode
            else
            {
                unilen = utf8_to_unicode(s->status, unistr, 8192);
                args.flags |= AIM_IMFLAGS_UNICODE;
                args.msg = unistr;
                args.msglen = unilen * 2;  // length has to be in bytes, not wchars
            }
            aim_send_im_ext(ass, &args);

            free(unistr);

            strcat(fullmsg, " (Sent AutoReply)");
            //strcat(fullmsg_xhtml, " (Sent AutoReply)");
        }


        //now it's been zero seconds since the last im from the aim user:
        if (buddy != NULL)
            buddy->lastactivity = time(NULL);

        // attach the body
        xmlnode_insert_cdata(node, fullmsg, strlen(fullmsg));

        // attach xhtml
        //xmlnode_put_attrib(node_xhtml, "xmlns", "http://www.w3.org/1999/xhtml");
        //xmlnode_insert_cdata(node_xhtml, fullmsg_xhtml, strlen(fullmsg_xhtml));

        jid_set(jp->from, at_normalize((char *)userinfo->sn), JID_USER);
        xmlnode_put_attrib(jp->x, "from", jid_full(jp->from));

        log_debug(ZONE, "[AIM] Sending: %s\n", xmlnode2str(jp->x));
        at_deliver(ti,jp->x);

        pth_wait(NULL);
    }
    else if (channel == 2)
    {
        /* XXX decide how to handle groupchat requests */
        /* XXX Actualy parse and use this */
        struct aim_incomingim_ch2_args *args;

        args = va_arg(ap, struct aim_incomingim_ch2_args *);
        va_end(ap);

#if 0

        switch (args->reqclass)
        {
        case AIM_CAPS_BUDDYICON:
            if (buddy->icon != NULL)
                free(buddy->icon);
            memcpy(buddy->icon, args->info.icon.icon);
            buddy->iconlen = args->info.icon.length;
            break;
        }
#endif

    }

    // free!
    free(utf8_str);
    free(msg_plain);
    free(msg_xhtml);

    return 1;
}

int at_offlinemsg(aim_session_t *sess,
                  aim_frame_t *command, ...)
{
    va_list ap;
    struct aim_icq_offlinemsg *msg;
    struct gaim_connection *gc = sess->aux_data;
    at_session s;
    ati ti;

    va_start(ap, command);
    msg = va_arg(ap, struct aim_icq_offlinemsg *);
    va_end(ap);

    s = (at_session)sess->aux_data;
    ti = s->ti;

    if (msg->type == 0x0001)
    {
        xmlnode x;
        xmlnode body;
        jpacket jp;
        char *cmsg;
        char *fmsg;
        char sender[128];

        snprintf(sender, sizeof(sender), "%lu@%s", msg->sender, ti->i->id);
        x = xmlnode_new_tag("message");
        xmlnode_put_attrib(x, "to", jid_full(s->cur));
        xmlnode_put_attrib(x, "from", sender);

        jp = jpacket_new(x);
        body = xmlnode_insert_tag(jp->x, "body");
        cmsg = strip_html(msg->msg, jp->p);
        cmsg = (char *)str_to_UTF8(jp->p, (unsigned char *)cmsg);
        fmsg = pmalloco(xmlnode_pool(body), strlen(cmsg) + 3);
        strcat(fmsg, cmsg);
        xmlnode_insert_cdata(body, fmsg, strlen(fmsg));

        at_deliver(ti,x);
    }
    else
    {
        log_debug(ZONE, "[AIM] unknown offline message type 0x%04x\n", msg->type);
    }

    return 1;
}

int at_offlinemsgdone(aim_session_t *sess,
                      aim_frame_t *command, ...)
{
    aim_icq_ackofflinemsgs(sess);
    return 1;
}

int at_icq_smsresponse(aim_session_t *sess,
                       aim_frame_t *command, ...)
{
    va_list ap;
    struct aim_icq_smsresponse *msg;
    struct gaim_connection *gc = sess->aux_data;
    at_session s;
    ati ti;

    va_start(ap, command);
    msg = va_arg(ap, struct aim_icq_smsresponse *);
    va_end(ap);

    s = (at_session)sess->aux_data;
    ti = s->ti;

    if (msg->type == 150)
    {
        xmlnode x;
        xmlnode body;
        jpacket jp;
        char *smsg = "SMS has been sent.";
        char *fmsg;

        x = xmlnode_new_tag("message");
        xmlnode_put_attrib(x, "to", jid_full(s->cur));
        xmlnode_put_attrib(x, "from", ti->i->id);

        jp = jpacket_new(x);
        body = xmlnode_insert_tag(jp->x, "body");
        fmsg = pmalloco(xmlnode_pool(body), strlen(smsg) + 3);
        strcat(fmsg, smsg);
        xmlnode_insert_cdata(body, fmsg, strlen(fmsg));

        at_deliver(ti,x);
    }

    return 1;
}

int at_parse_misses(aim_session_t *sess,
                    aim_frame_t *command, ...)
{
    xmlnode x;
    xmlnode body;
    at_session s;
    ati ti;

    static char *missedreasons[] = {
                                       "Unknown",
                                       "Message too large",
                                       "Rate limit exceeded",
                                       "Sender is too evil",
                                       "You are too evil"};
    static int missedreasonslen = 5;

    va_list ap;
    unsigned short chan, nummissed, reason;
    aim_userinfo_t *userinfo;
    char msg[1024];

    s = (at_session)sess->aux_data;
    ti = s->ti;

    x = xmlnode_new_tag("message");
    xmlnode_put_attrib(x, "to", jid_full(s->cur));
    xmlnode_put_attrib(x, "from", jid_full(s->from));
    xmlnode_put_attrib(x, "type", "error");
    body = xmlnode_insert_tag(x, "error");

    va_start(ap, command);
    chan = va_arg(ap, u_int);
    userinfo = va_arg(ap, aim_userinfo_t *);
    nummissed = va_arg(ap, u_int);
    reason = va_arg(ap, u_int);
    va_end(ap);

    memset(&msg, '\0', 1024);
    snprintf((char *)&msg, 1024, "Missed %d messages from %s (reason %d: %s)",
             nummissed, userinfo->sn, reason,
             (reason < missedreasonslen) ? missedreasons[reason] : "unknown");

    xmlnode_insert_cdata(body, (char *)&msg, strlen((char *)&msg));

    at_deliver(ti,x);
    return 1;
}

int at_parse_msgerr(aim_session_t *sess, aim_frame_t *command, ...)
{
    xmlnode x;
    xmlnode body;
    jpacket jp;
    at_session s;
    ati ti;
    char msg[1024];

    va_list ap;
    char *destsn;
    unsigned short reason;

    va_start(ap, command);
    reason = va_arg(ap, u_int);
    destsn = va_arg(ap, char *);
    va_end(ap);

    memset((char *)&msg, '\0', 1024);
    snprintf((char *)&msg, 1024,
             "faimtest: message to %s bounced (reason 0x%04x: %s)\n",
             destsn, reason,
             (reason < msgerrreasonslen) ? msgerrreasons[reason] : "unknown");

    s = (at_session)sess->aux_data;
    ti = s->ti;

    x = xmlnode_new_tag("message");
    xmlnode_put_attrib(x, "to", jid_full(s->cur));
    xmlnode_put_attrib(x, "from", jid_full(s->from));
    xmlnode_put_attrib(x, "type", "error");
    body = xmlnode_insert_tag(x, "error");
    xmlnode_insert_cdata(body, (char *)&msg, strlen((char *)&msg));

    at_deliver(ti,x);

    return 1;
}
