/*
 * Jabber AIM-Transport libfaim interface
 *
*/

#include "aimtrans.h"
#include "utf8.h"

void *at_session_main(void *arg);

static int getaimdata(char *aimbinarypath, unsigned char **bufret, int *buflenret, unsigned long offset, unsigned long len, const char *modname)
{
  FILE *f;
  static const char defaultmod[] = "aim.exe";
  char *filename = NULL;
  struct stat st;
  unsigned char *buf;
  int invalid = 0;

  if(aimbinarypath == NULL) {
    log_error(ZONE, "Received aim.exe hash request from AOL servers but no aim.exe configured. We may get disconnected.\n");
    return -1;
  }

  if (modname) {

    if (!(filename = malloc(strlen(aimbinarypath)+1+strlen(modname)+4+1))) {
      return -1;
    }

    sprintf(filename, "%s/%s.ocm", aimbinarypath, modname);

  } else {

    if (!(filename = malloc(strlen(aimbinarypath)+1+strlen(defaultmod)+1))) {
      return -1;
    }

    sprintf(filename, "%s/%s", aimbinarypath, defaultmod);

  }

  if (stat(filename, &st) == -1) {
    if (!modname) {
      free(filename);
      return -1;
    }
    invalid = 1;
  }

  if (!invalid) {
    if ((offset > st.st_size) || (len > st.st_size))
      invalid = 1;
    else if ((st.st_size - offset) < len)
      len = st.st_size - offset;
    else if ((st.st_size - len) < len)
      len = st.st_size - len;
  }

  if (!invalid && len)
    len %= 4096;

  if (invalid) {
    int i;

    free(filename); /* not needed */

    log_debug(ZONE, "memrequest: received invalid request for 0x%08lx bytes at 0x%08lx (file %s)\n", len, offset, modname);

    i = 8;
    if (modname)
      i += strlen(modname);

    if (!(buf = malloc(i)))
      return -1;

    i = 0;

    if (modname) {
      memcpy(buf, modname, strlen(modname));
      i += strlen(modname);
    }

    /* Damn endianness. This must be little (LSB first) endian. */
    buf[i++] = offset & 0xff;
    buf[i++] = (offset >> 8) & 0xff;
    buf[i++] = (offset >> 16) & 0xff;
    buf[i++] = (offset >> 24) & 0xff;
    buf[i++] = len & 0xff;
    buf[i++] = (len >> 8) & 0xff;
    buf[i++] = (len >> 16) & 0xff;
    buf[i++] = (len >> 24) & 0xff;

    *bufret = buf;
    *buflenret = i;

  } else {

    if (!(buf = malloc(len))) {
      free(filename);
      return -1;
    }

    if (!(f = fopen(filename, "r"))) {
      free(filename);
      free(buf);
      return -1;
    }

    free(filename);

    if (fseek(f, offset, SEEK_SET) == -1) {
      fclose(f);
      free(buf);
      return -1;
    }

    if (fread(buf, len, 1, f) != 1) {
      fclose(f);
      free(buf);
      return -1;
    }

    fclose(f);

    *bufret = buf;
    *buflenret = len;

  }

  return 0; /* success! */
}

static int at_memrequest(aim_session_t *sess, aim_frame_t *command, ...)
{
  va_list ap;
  unsigned long offset, len;
  unsigned char *buf;
  char *modname;
  int buflen;
  int j;
  at_session s;
  ati ti;

  s = (at_session)sess->aux_data;
  ti = s->ti;
  
  va_start(ap, command);
  offset = va_arg(ap, unsigned long);
  len = va_arg(ap, unsigned long);
  modname = va_arg(ap, char *);
  va_end(ap);


  log_debug(ZONE, "We got a memrequest\n");
  if(ti->offset == offset && j_strcmp(ti->modname, modname) == 0 && 
          ti->send_buf)
  {
    aim_sendmemblock(sess, command->conn, offset, ti->read_len, ti->send_buf, AIM_SENDMEMBLOCK_FLAG_ISREQUEST);
  } else {
    if (getaimdata(ti->aimbinarydir, &buf, &buflen, offset, len, modname) == 0) {
      ti->offset = offset;
      ti->read_len = buflen;
      if(ti->modname != NULL)
         free(ti->modname);
      if(modname != NULL)
          ti->modname = strdup(modname);
      if(ti->send_buf != NULL)
         free(ti->send_buf);
      if(buflen > 0)
          ti->send_buf = strdup(buf);
      else
          ti->send_buf = NULL;
      aim_sendmemblock(sess, command->conn, offset, buflen, buf, AIM_SENDMEMBLOCK_FLAG_ISREQUEST);
    } else {
      ti->offset = offset;
      ti->read_len = buflen;
      if(ti->modname != NULL)
         free(ti->modname);
      ti->modname = NULL;
      if(ti->send_buf != NULL)
         free(ti->send_buf);
      ti->send_buf = NULL;
      aim_sendmemblock(sess, command->conn, offset, len, NULL, AIM_SENDMEMBLOCK_FLAG_ISREQUEST);
    }
  }

  return 1;
}

int at_conninitdone_bos(aim_session_t *sess,
                        aim_frame_t *command, ...)
{
    char *buddylist;
    at_session s;
    /* this is the new profile */
    /* XXX Can we let people do this? */
    char profile[] = "";  

    s = sess->aux_data;
/*    sess->snacid_next = 0x00000001; */

    aim_reqpersonalinfo(sess, command->conn);
    aim_bos_reqlocaterights(sess, command->conn);
    aim_bos_setprofile(sess, command->conn, profile, NULL, AIM_CAPS_BUDDYICON | AIM_CAPS_CHAT | AIM_CAPS_GETFILE | AIM_CAPS_VOICE | AIM_CAPS_SENDFILE | AIM_CAPS_IMIMAGE);
    aim_bos_reqbuddyrights(sess, command->conn);

    /* send the buddy list and profile (required, even if empty) */
    buddylist = at_buddy_buildlist(s, s->cur);
    log_debug(ZONE, "[AIM] Setting buddylist: %s", buddylist);
    if(aim_bos_setbuddylist(sess, command->conn, buddylist)<0)
      log_debug(ZONE, "[AIM] Error setting buddylist!");
    if(buddylist != NULL)
      free(buddylist);

    /* dont really know what this does */
/*    aim_addicbmparam(sess, command->conn);
    aim_bos_reqicbmparaminfo(sess, command->conn); */
    aim_reqicbmparams(sess);
  
    aim_bos_reqrights(sess, command->conn);  
    /* set group permissions -- all user classes */
    /* XXX evaluate these! */
    aim_bos_setgroupperm(sess, command->conn, AIM_FLAG_ALLUSERS);
    aim_bos_setprivacyflags(sess, command->conn, 
                            AIM_PRIVFLAGS_ALLOWIDLE);
    return 1;
}

static int aim_icbmparaminfo(aim_session_t* sess, aim_frame_t* fr, ...)
{
    struct aim_icbmparameters *params;
    va_list ap;

	va_start(ap, fr);
	params = va_arg(ap, struct aim_icbmparameters *);
	va_end(ap);

	/*
	 * Set these to your taste, or client medium.  Setting minmsginterval
	 * higher is good for keeping yourself from getting flooded (esp
	 * if you're on a slow connection or something where that would be
	 * useful).
	 */
	params->maxmsglen = 8000;
	params->minmsginterval = 0; /* in milliseconds */

	aim_seticbmparam(sess, params);

	return 1;
}

int at_conninitdone_admin(aim_session_t *sess,
                          aim_frame_t *command, ...)
{
    aim_clientready(sess, command->conn);
    log_debug(ZONE, "[AT] connected to authorization/admin service");
    return 1;
}

int at_bosrights(aim_session_t *sess, 
                 aim_frame_t *command, ...)
{
    unsigned short maxpermits, maxdenies;
    va_list ap;
    xmlnode x;
    at_session s;
    ati ti;

    s = sess->aux_data;
    ti = s->ti;

    va_start(ap, command);
    maxpermits = va_arg(ap, u_int);
    maxdenies = va_arg(ap, u_int);
    va_end(ap);

    aim_clientready(sess, command->conn);

    aim_icq_reqofflinemsgs(sess);

    log_debug(ZONE, "[AIM] officially connected to BOS, sending pres.");

    s->loggedin = 1;

    // Send "connected" presence to all the attached resources of this JID
    x = jutil_presnew(JPACKET__AVAILABLE, jid_full(jid_user(s->cur)), "Connected");
    xmlnode_put_attrib(x, "from", jid_full(s->from));
    at_deliver(ti,x);

    return 1;
}

int at_parse_login(aim_session_t *sess, 
                   aim_frame_t *command, ...)
{
    struct client_info_s info = AIM_CLIENTINFO_KNOWNGOOD;
    
    unsigned char *key;
    va_list ap;
    at_session s;
  
    s = (at_session)sess->aux_data;
    va_start(ap, command);
    key = va_arg(ap, char *);
    va_end(ap);

    aim_send_login(sess,command->conn,s->screenname,s->password, &info, key);
 
    return 1;
}

int at_parse_authresp(aim_session_t *sess, 
                      aim_frame_t *command, ...)
{
    va_list ap;
    struct aim_authresp_info *info;
    aim_conn_t *bosconn = NULL;
    at_mio am;
    at_session s;
    aim_conn_t *conn = NULL;
    ati ti;

    s = (at_session)sess->aux_data;
    ti = s->ti;

    va_start(ap, command);
    info = va_arg(ap, struct aim_authresp_info *);
    conn = command->conn;
    va_end(ap);

    log_debug(ZONE, "Auth Response for Screen name: %s\n", info->sn);

    /*
    * Check for error.
    */
    if (info->errorcode || !info->bosip || !info->cookie)
    {
        spool sp;
        xmlnode x;
        xmlnode body;
        char *errorstr;

        x = xmlnode_new_tag("presence");
        xmlnode_put_attrib(x, "to", jid_full(s->cur));
        xmlnode_put_attrib(x, "from", jid_full(s->from));
        xmlnode_put_attrib(x, "type", "error");
        body = xmlnode_insert_tag(x, "error");
        xmlnode_put_attrib(body, "code", "401");

        sp = spool_new(xmlnode_pool(x));
        spooler(sp, "Error Code #%04x While Logging in to AIM\n",
                "Error URL: %s\0", sp);
        errorstr = pmalloc(xmlnode_pool(x), 200);
        switch (info->errorcode) {
            case 0x05:
                sprintf(errorstr, "Incorrect nick/password.");
                break;
            case 0x11:
                sprintf(errorstr, "Your account is currently suspended.");
                break;
            case 0x18:
                sprintf(errorstr, "Connecting too frequently. Try again in ten minutes.");
                break;
            case 0x1c:
                sprintf(errorstr, "Server software is out of date.");
                break;
            default:
                snprintf(errorstr, 200, spool_print(sp), info->errorcode,
                         info->errorurl);
                break;
        }
        xmlnode_insert_cdata(body, errorstr, strlen(errorstr));
        at_deliver(ti,x);

        s->loggedin = 0;
        /* XXX End the Stuff */
        aim_conn_kill(sess, &command->conn);
        return 1;
    }

    aim_conn_kill(sess, &command->conn);

    bosconn = aim_newconn(sess, AIM_CONN_TYPE_BOS, info->bosip);
    /* XXX Handle these better so we actually return and free the threads */
    if (bosconn == NULL) {
        fprintf(stderr, "at: could not connect to BOS: internal error\n");
    } else if (bosconn->status & AIM_CONN_STATUS_CONNERR) {	
        fprintf(stderr, "at: could not connect to BOS\n");
        aim_conn_kill(sess, &bosconn);
    } else {

        aim_conn_setlatency(bosconn, 0);
        aim_conn_addhandler(sess, bosconn, AIM_CB_FAM_SPECIAL, 
                            AIM_CB_SPECIAL_CONNCOMPLETE, at_conncomplete, 0);
        aim_conn_addhandler(sess, bosconn, AIM_CB_FAM_SPECIAL,
                            AIM_CB_SPECIAL_CONNINITDONE, at_conninitdone_bos, 0);
        aim_conn_addhandler(sess, bosconn, 0x0009, 0x0003, 
                            at_bosrights, 0);
        aim_conn_addhandler(sess, bosconn, AIM_CB_FAM_ACK, 
                            AIM_CB_ACK_ACK, NULL, 0);
        aim_conn_addhandler(sess, bosconn, AIM_CB_FAM_GEN, 
                            AIM_CB_GEN_REDIRECT, at_handleredirect, 0);
        aim_conn_addhandler(sess, bosconn, AIM_CB_FAM_BUD, 
                            AIM_CB_BUD_ONCOMING, at_parse_oncoming, 0);
        aim_conn_addhandler(sess, bosconn, AIM_CB_FAM_BUD, 
                            AIM_CB_BUD_OFFGOING, at_parse_offgoing, 0);
        aim_conn_addhandler(sess, bosconn, AIM_CB_FAM_MSG, 
                            AIM_CB_MSG_INCOMING, at_parse_incoming_im, 0);
        aim_conn_addhandler(sess, bosconn, AIM_CB_FAM_LOC, 
                            AIM_CB_LOC_ERROR, at_parse_locerr, 0);
        aim_conn_addhandler(sess, bosconn, AIM_CB_FAM_MSG, 
                            AIM_CB_MSG_MISSEDCALL, at_parse_misses, 0);
        aim_conn_addhandler(sess, bosconn, AIM_CB_FAM_GEN, 
                            AIM_CB_GEN_RATECHANGE, at_parse_ratechange, 0);
        aim_conn_addhandler(sess, bosconn, AIM_CB_FAM_GEN, 
                            AIM_CB_GEN_EVIL, at_parse_evilnotify, 0);
        aim_conn_addhandler(sess, bosconn, AIM_CB_FAM_MSG, 
                            AIM_CB_MSG_ERROR, at_parse_msgerr, 0);
        aim_conn_addhandler(sess, bosconn, AIM_CB_FAM_LOC, 
                            AIM_CB_LOC_USERINFO, at_parse_userinfo, 0);
        aim_conn_addhandler(sess, bosconn, AIM_CB_FAM_ICQ,
                            AIM_CB_ICQ_OFFLINEMSG, at_offlinemsg, 0);
        aim_conn_addhandler(sess, bosconn, AIM_CB_FAM_ICQ,
                            AIM_CB_ICQ_OFFLINEMSGCOMPLETE, at_offlinemsgdone, 0);
        aim_conn_addhandler(sess, bosconn, AIM_CB_FAM_ICQ,
                            AIM_CB_ICQ_SMSRESPONSE, at_icq_smsresponse, 0);
        aim_conn_addhandler(sess, bosconn, AIM_CB_FAM_ICQ,
                            AIM_CB_ICQ_SIMPLEINFO, at_parse_icq_simpleinfo, 0);
        /* XXX Future?
         * aim_conn_addhandler(sess, bosconn, AIM_CB_FAM_MSG, 
                            AIM_CB_MSG_ACK, at_parse_msgack, 0);
         */

        /*
        aim_conn_addhandler(sess, bosconn, AIM_CB_FAM_CTN, 
                            AIM_CB_CTN_DEFAULT, aim_parse_unknown, 0);
        */
        /*
        aim_conn_addhandler(sess, bosconn, AIM_CB_FAM_SPECIAL, 
                            AIM_CB_SPECIAL_DEFAULT, aim_parse_unknown, 0);
        */
        aim_conn_addhandler(sess, bosconn, AIM_CB_FAM_GEN, 
                            AIM_CB_GEN_MOTD, at_parse_motd, 0);
        
        aim_conn_addhandler(sess, bosconn, AIM_CB_FAM_SPECIAL, 
                            AIM_CB_SPECIAL_CONNERR, at_parse_connerr, 0);
        aim_conn_addhandler(sess, bosconn, 0x0001, 0x001f, 
                            at_memrequest, 0);
        aim_conn_addhandler(sess, bosconn, 0x0004, 0x0005, 
                            aim_icbmparaminfo, 0);
        
    
        am = pmalloco(s->p, sizeof(_at_mio));
        am->p = s->p;
        am->s = s;
        am->ass = sess;
        am->conn = bosconn;

        //m = mio_new(bosconn->fd, &at_handle_mio, am, mio_handlers_new(at_session_handle_read, at_session_handle_write, NULL));

        //s->m = m;

        aim_sendcookie(sess, bosconn, info->cookie);

    }
    
    return 1;
}

/* Future expansion
int at_conninitdone_chatnav(aim_session_t *sess, aim_frame_t *command, ...)
{
  fprintf(stderr, "faimtest: chatnav: got server ready\n");
  aim_conn_addhandler(sess, command->conn, AIM_CB_FAM_CTN, AIM_CB_CTN_INFO, faimtest_chatnav_info, 0);
  aim_bos_reqrate(sess, command->conn);
  aim_chatnav_clientready(sess, command->conn);
  aim_chatnav_reqrights(sess, command->conn);
  return 1;
}

int at_conninitdone_chat(aim_session_t *sess, aim_frame_t *command, ...)
{
  aim_conn_addhandler(sess, command->conn, AIM_CB_FAM_CHT, AIM_CB_CHT_USERJOIN, faimtest_chat_join, 0);
  aim_conn_addhandler(sess, command->conn, AIM_CB_FAM_CHT, AIM_CB_CHT_USERLEAVE, faimtest_chat_leave, 0);
  aim_conn_addhandler(sess, command->conn, AIM_CB_FAM_CHT, AIM_CB_CHT_ROOMINFOUPDATE, faimtest_chat_infoupdate, 0);
  aim_conn_addhandler(sess, command->conn, AIM_CB_FAM_CHT, AIM_CB_CHT_INCOMINGMSG, faimtest_chat_incomingmsg, 0);
  aim_bos_reqrate(sess, command->conn);
  aim_chat_clientready(sess, command->conn);
  return 1;
} */

int at_handleredirect(aim_session_t *sess, aim_frame_t *command, ...)
{
  va_list ap;
  int serviceid;
  char *ip;
  unsigned char *cookie;
  at_session s;
  at_mio am;

  va_start(ap, command);
  serviceid = va_arg(ap, int);
  ip = va_arg(ap, char *);
  cookie = va_arg(ap, unsigned char *);

  s = (at_session)sess->aux_data;
 
  switch(serviceid)
  {
    case 0x0005:
    {
    	aim_conn_t *tstconn;
            
        tstconn = aim_newconn(sess, AIM_CONN_TYPE_ADS, ip);
	    if ((tstconn==NULL) || (tstconn->status & AIM_CONN_STATUS_RESOLVERR)) {
        	  log_debug(ZONE, "[AT] unable to reconnect with authorizer");
      	} else {
	      aim_conn_addhandler(sess, tstconn, AIM_CB_FAM_SPECIAL, AIM_CB_SPECIAL_FLAPVER, at_flapversion, 0);
    	  aim_conn_addhandler(sess, tstconn, AIM_CB_FAM_SPECIAL, AIM_CB_SPECIAL_CONNCOMPLETE, at_conncomplete, 0);
	      aim_conn_addhandler(sess, tstconn, AIM_CB_FAM_SPECIAL, AIM_CB_SPECIAL_CONNINITDONE, at_conninitdone_admin, 0);
    	  //aim_conn_addhandler(sess, tstconn, AIM_CB_FAM_GEN, 0x0018, faimtest_hostversions, 0);
	      aim_sendcookie(sess, tstconn, cookie);
    	  log_debug(ZONE, "sent cookie to adverts host");
       }
	}
    break;
    case 0x0007: /* Authorizer */
      {
	aim_conn_t *tstconn;
	/* Open a connection to the Auth */
	tstconn = aim_newconn(sess, AIM_CONN_TYPE_AUTH, ip);
	if ( (tstconn==NULL) || (tstconn->status >= AIM_CONN_STATUS_RESOLVERR) )
    {
	    log_debug(ZONE, "[AT] unable to reconnect with authorizer\n");
    }
	else
    {
        aim_conn_addhandler(sess, tstconn, AIM_CB_FAM_SPECIAL, AIM_CB_SPECIAL_FLAPVER, at_flapversion, 0);
        aim_conn_addhandler(sess, tstconn, AIM_CB_FAM_SPECIAL, AIM_CB_SPECIAL_CONNCOMPLETE, at_conncomplete, 0);
        aim_conn_addhandler(sess, tstconn, AIM_CB_FAM_SPECIAL, AIM_CB_SPECIAL_CONNINITDONE, at_conninitdone_admin, 0);
	  /* Send the cookie to the Auth */
/*
        am = pmalloco(s->p, sizeof(_at_mio));
        am->p = s->p;
        am->s = s;
        am->conn = tstconn;
        am->ass = sess;
*/
        
        //mio_new(tstconn->fd, &at_handle_mio, tstconn, mio_handlers_new(at_session_handle_read, at_session_handle_write, NULL));

	    aim_sendcookie(sess, tstconn, cookie);
    }

      }  
      break;
    case 0x000d: /* XXX ChatNav */
      {
	/*aim_conn_t *tstconn = NULL;
	tstconn = aim_newconn(sess, AIM_CONN_TYPE_CHATNAV, ip);
	if ( (tstconn==NULL) || (tstconn->status >= AIM_CONN_STATUS_RESOLVERR)) {
	  fprintf(stderr, "faimtest: unable to connect to chatnav server\n");
	  if (tstconn) aim_conn_kill(sess, &tstconn);
	  return 1;
	}
	aim_conn_addhandler(sess, tstconn, 0x0001, 0x0003, faimtest_serverready, 0);
	aim_sendcookie(sess, tstconn, cookie);
	fprintf(stderr, "\achatnav: connected\n");*/
      }
      break;
    case 0x000e: /* Chat */
      {
	/*char *roomname = NULL;
	aim_conn_t *tstconn = NULL;

	roomname = va_arg(ap, char *);

	tstconn = aim_newconn(sess, AIM_CONN_TYPE_CHAT, ip);
	if ( (tstconn==NULL) || (tstconn->status >= AIM_CONN_STATUS_RESOLVERR))
	  {
	    fprintf(stderr, "faimtest: unable to connect to chat server\n");
	    if (tstconn) aim_conn_kill(sess, &tstconn);
	    return 1;
	  }		
	printf("faimtest: chat: connected\n");

	aim_chat_attachname(tstconn, roomname);

	aim_conn_addhandler(sess, tstconn, 0x0001, 0x0003, faimtest_serverready, 0);
	aim_sendcookie(sess, tstconn, cookie);*/
      }
      break;
    default:
      log_debug(ZONE, "uh oh... got redirect for unknown service 0x%04x!!\n", serviceid);
      /* dunno */
    }

  va_end(ap);

  return 1;
}

int at_parse_ratechange(aim_session_t *sess,
                        aim_frame_t *fr, ...)
{
	static const char *codes[5] = {
		"invalid",
		 "change",
		 "warning",
		 "limit",
		 "limit cleared",
	};
	va_list ap;
	fu16_t code, rateclass;
	fu32_t windowsize, clear, alert, limit, disconnect, currentavg, maxavg;
	at_session s;
	ati ti;

	s = (at_session)sess->aux_data;
	ti = s->ti;

	va_start(ap, fr); 
	code = (fu16_t)va_arg(ap, unsigned int);
	rateclass= (fu16_t)va_arg(ap, unsigned int);
	windowsize = (fu32_t)va_arg(ap, unsigned long);
	clear = (fu32_t)va_arg(ap, unsigned long);
	alert = (fu32_t)va_arg(ap, unsigned long);
	limit = (fu32_t)va_arg(ap, unsigned long);
	disconnect = (fu32_t)va_arg(ap, unsigned long);
	currentavg = (fu32_t)va_arg(ap, unsigned long);
	maxavg = (fu32_t)va_arg(ap, unsigned long);
	va_end(ap);

	log_debug(ZONE, "[AIM] rate %s (paramid 0x%04lx): curavg = %ld, maxavg = %ld, alert at %ld, "
		     "clear warning at %ld, limit at %ld, disconnect at %ld (window size = %ld)\n",
		     (code < 5) ? codes[code] : codes[0],
		     rateclass,
		     currentavg, maxavg,
		     alert, clear,
		     limit, disconnect,
		     windowsize);

	/* XXX fix these values */
	if (code == AIM_RATE_CODE_CHANGE) {
		if (currentavg >= clear)
			aim_conn_setlatency(fr->conn, 0);
	} else if (code == AIM_RATE_CODE_WARNING) {
		aim_conn_setlatency(fr->conn, 500);
	} else if (code == AIM_RATE_CODE_LIMIT) {
	        /* send rate warning message */
	        xmlnode err,error;
	        err=xmlnode_new_tag("message");
	        xmlnode_put_attrib(err,"type","error");
	        xmlnode_put_attrib(err,"from",ti->i->id);
	        xmlnode_put_attrib(err,"to",jid_full(s->cur));
	        error=xmlnode_insert_tag(err,"error");
	        xmlnode_insert_cdata(error,
	          "You are talking too fast. Message has been dropped.",-1);
	        at_deliver(ti,err);

		aim_conn_setlatency(fr->conn, 1000);
	} else if (code == AIM_RATE_CODE_CLEARLIMIT) {
		aim_conn_setlatency(fr->conn, 0);
	}

	return 1;
}

int at_parse_motd(aim_session_t *sess, 
                  aim_frame_t *command, ...)
{
/* XXX Should we do the headline?
  static char *codes[] = {
    "Unknown",
    "Mandatory upgrade",
    "Advisory upgrade",
    "System bulletin",
    "Top o' the world!"};
  static int codeslen = 5;

  char *amsg;
  unsigned short id;
  va_list ap;
  char msg[1024];
  
  va_start(ap, command);
  id = va_arg(ap, u_int);
  msg = va_arg(ap, char *);
  va_end(ap);

  printf("faimtest: motd: %s (%d / %s)\n", msg, id, 
	 (id < codeslen)?codes[id]:"unknown");
*/
  //aim_0001_0020(sess, command->conn);

  return 1;
}

int at_parse_connerr(aim_session_t *sess, 
                     aim_frame_t *command, ...)
{
  va_list ap;
  unsigned short code;
  char *msg = NULL;
  at_session s;

  s = (at_session)sess->aux_data;
  va_start(ap, command);
  code = va_arg(ap, u_int);
  msg = va_arg(ap, char *);
  va_end(ap);

  /* XXX End the session */
  aim_conn_kill(sess, &command->conn); /* this will break the main loop */

  return 1;
}

int at_flapversion(aim_session_t *sess, 
                   aim_frame_t *command, ...)
{

  return 1;
}

int at_conncomplete(aim_session_t *sess, 
                    aim_frame_t *command, ...)
{
  va_list ap;
  aim_conn_t *conn;

  va_start(ap, command);
  conn = va_arg(ap, aim_conn_t *);
  va_end(ap);
  
  return 1;
}

#if 0
at_session at_session_find_aim(int fd)
{
    aim_conn_t *cur;

    for (cur = sess->connlist; cur; cur = cur->next) {
        if (cur->status & AIM_CONN_STATUS_INPROGRESS) {
            FD_SET(cur->fd, &wfds);
            haveconnecting++;
        }
        FD_SET(cur->fd, &fds);
        if (cur->fd > maxfd)
            maxfd = cur->fd;
    }
}
#endif

#if 0
ssize_t at_session_handle_read(mio m, void* buf, size_t count)
{
    at_mio am;
    at_session s;
    aim_conn_t *conn;
    int flags;

    am = (at_mio)m->cb_arg;

    flags = fcntl(m->fd, F_GETFL, 0);
    if(flags & O_NONBLOCK)
    {
        flags &= ~O_NONBLOCK;
        if(fcntl(m->fd, F_SETFL, flags)<0)
            log_debug(ZONE, "[AT] Error calling F_SETFL to block");
    }


    /*log_debug(ZONE, "[AT] Session MIO handle read");*/
    errno = 0;
    if (aim_get_command(am->ass, am->conn) >= 0)
    {
        aim_rxdispatch(am->ass);
        return 1;
    } else {
        log_debug(ZONE, "[AT] Error handling read!");
        aim_conn_kill(am->ass, &am->conn);
        mio_close(m);
    //    at_session_end(s);
        return -1;
    }
}

ssize_t at_session_handle_write(mio m, const void* buf, size_t count)
{
    /*log_debug(ZONE, "[AT] Session MIO handle write");*/
    errno = 0;
    return count;
}

void at_handle_mio(mio m, int flags, void *arg, char *buffer, int bufsz)
{
    aim_conn_t *cur;
    at_mio am;
    at_session s;
    int sflags;

    am = (at_mio)arg;

    if(flags == MIO_NEW)
        return;

    if(flags == MIO_CLOSED || flags == MIO_ERROR)
    {
        log_debug(ZONE, "[AT] socket %d closed", m->fd);
        pool_free(am->p);
        if(am->s->loggedin == 1)
            at_session_end(am->s);
        return;
    }
}
#endif

void at_debugcb(aim_session_t *sess, int level, const char *format, va_list va)
{
    vfprintf(stderr, format, va);
    
    return;
}

at_session at_session_create(ati ti, xmlnode aim_data, jpacket jp)
{
    char *user, *pword;
    jid j, from;
    pool p;
    mio m = NULL;
    at_session new;
    aim_session_t *ass;
    aim_conn_t *authconn = NULL;
    jid jput;
    pth_attr_t attr;
    xmlnode fpres;

    user = xmlnode_get_attrib(aim_data, "id");
    pword = xmlnode_get_attrib(aim_data, "pass");

    j = jp->from;
    from = jp->to;

    new = at_session_find_by_jid(ti, j);
    if(new!=NULL)
    {
        log_debug(ZONE, "[AIM] The goober is already online");
        return NULL;
    }
    log_debug(ZONE, "[AT] Going to start session for %s", jid_full(j));
    printf("New session for %s\n", jid_full(j));
    ass = malloc(sizeof(aim_session_t));
    aim_session_init(ass, AIM_SESS_FLAGS_NONBLOCKCONNECT, 0 /* faim debug level */);
    aim_setdebuggingcb(ass, at_debugcb);

    /* Set queue mode */
    aim_tx_setenqueue(ass, AIM_TX_QUEUED, NULL);

    authconn = aim_newconn(ass, AIM_CONN_TYPE_AUTH, FAIM_LOGIN_SERVER);
    if(authconn == NULL)
    {
        xmlnode err;
        
        /* Let the user know */
        err = xmlnode_new_tag("message");
        xmlnode_put_attrib(err, "to", jid_full(j));
        xmlnode_put_attrib(err, "type", "error");
        xmlnode_put_attrib(err, "from", jid_full(from));
        jutil_error(err, (terror){500, "Error connecting to the AIM authentication server"});
        at_deliver(ti,err);

        /* Break it down */
        log_error("[AIM]", "Error connecting to aims authentication server.\n");
        return NULL;
    } 
    else if (authconn->fd == -1)
    {
        xmlnode err;
        
        /* Let the user know */
        err = xmlnode_new_tag("message");
        xmlnode_put_attrib(err, "to", jid_full(j));
        xmlnode_put_attrib(err, "type", "error");
        xmlnode_put_attrib(err, "from", jid_full(from));
        
        if (authconn->status & AIM_CONN_STATUS_RESOLVERR)
        {
            jutil_error(err, (terror){500, "Could not resolve AIM authorizer"});
            log_error("[AIM]", "at: could not resolve authorizer name");
        }
        else if (authconn->status & AIM_CONN_STATUS_CONNERR)
        {
            jutil_error(err, (terror){500, "Could not connect to AIM authorizer"});
            log_error("[AIM]", "at: could not connect to authorizer");
        }
        at_deliver(ti,err);

        /* Clean up and get out */
        aim_conn_kill(ass, &authconn);
        return NULL;
    }
  	else
    {


        aim_conn_addhandler(ass, authconn, AIM_CB_FAM_SPECIAL, 
                            AIM_CB_SPECIAL_FLAPVER, at_flapversion, 0);
        aim_conn_addhandler(ass, authconn, AIM_CB_FAM_SPECIAL, 
                            AIM_CB_SPECIAL_CONNCOMPLETE, at_conncomplete, 0);
        aim_conn_addhandler(ass, authconn, 0x0017, 0x0007, 
                            at_parse_login, 0);
        aim_conn_addhandler(ass, authconn, 0x0017, 0x0003, 
                            at_parse_authresp, 0);
    
        //m = mio_new(authconn->fd, &at_handle_mio, authconn, mio_handlers_new(at_session_handle_read, at_session_handle_write, NULL));
        //m->rated = 0;

        p = pool_new();
        new = pmalloc(p, sizeof(_at_session));
        new->m = NULL;
        new->p = p;
        new->ti = ti;
    	new->mp_to = pth_msgport_create("aimsession_to");
        new->cur=jid_new(new->p, jid_full(j));
        new->from=jid_new(new->p, jid_full(from));
        jid_set(new->from, "registered", JID_RESOURCE);
        new->ass = ass;
        new->exit_flag = 0;
        new->loggedin = 0;
        new->status = NULL;
        new->away = 0;
        new->icq = isdigit(user[0]);
        new->buddies = xhash_new(137);
        new->profile = NULL;
        new->at_PPDB = NULL;
        new->screenname = pstrdup(new->p, user);
        new->password = pstrdup(new->p, pword);
        new->icq_vcard_response = NULL;
        new->ass->aux_data = (void *)new;

        fpres = jutil_presnew(JPACKET__AVAILABLE, ti->i->id, "Online");
        xmlnode_put_attrib(fpres, "from", jid_full(new->cur));
        new->at_PPDB = ppdb_insert(new->at_PPDB, new->cur, fpres);
        xmlnode_free(fpres);
        /*
        am = pmalloco(new->p, sizeof(_at_mio));
        am->p = new->p;
        am->s = new;
        am->conn = authconn;
        am->ass = ass;
        */

        /* mio_reset(m, &at_handle_mio, am); */

        /* Ok everything is good, put this in the session hash */
        jput = jid_new(new->p, jid_full(new->cur));
        jid_set(jput, NULL, JID_RESOURCE);
        xhash_put(ti->session__list, jid_full(jput), new);
    
        log_debug(ZONE, "[AT] User Login: %s", jid_full(new->cur));

        aim_request_login(ass, authconn, user);
        
        attr = pth_attr_new();
        new->tid = pth_spawn(attr, at_session_main, (void *)new);
    	return new;
    }

    return NULL;
}

void *at_session_main(void *arg)
{
    at_session s;
    ati ti;
	pth_event_t to;
	jpq q;
	xmlnode x;
	aim_conn_t *waitingconn = NULL;
    int status;

    s = (at_session)arg;
    ti = s->ti;
	to = pth_event(PTH_EVENT_MSG, s->mp_to);

	/* Do stuff */
	log_debug(ZONE, "[AIM] In our main session");
	while ((waitingconn = _aim_select(s->ass, to, &status)) >= 0)
    {
		if(s->exit_flag)
			break;

        switch(status)
        {
            case -1:
                /* There was an error */
                s->exit_flag = 1;
                break;
            case 0:
                /* Nothing to do */
                break;
            case 1:
			    log_debug(ZONE, "[AIM] Flushing outgoing queue");
    			aim_tx_flushqueue(s->ass);
                break;
            case 2:
    		    if (aim_get_command(s->ass, waitingconn) < 0)
                {
	    			/* XXX do something like send an error */
		    		log_debug(ZONE,"[AIM] connection error (type 0x%04x:0x%04x)", waitingconn->type, waitingconn->subtype);
                    aim_conn_kill(s->ass, &waitingconn);
                    if (!aim_getconn_type(s->ass, AIM_CONN_TYPE_BOS))
	                      log_debug(ZONE,"major connection error");
			    	s->exit_flag = 1;
    			}
	        	else
		    	{
			    	aim_rxdispatch(s->ass);
    			}
                break;
            case 3:
		        while(1)
        		{
		        	q = (jpq)pth_msgport_get(s->mp_to);
        			if(q == NULL)
                    {
		        		break;
                    }
                    at_aim_session_parser(s, q->p);
                    if(s->exit_flag)
                        break;
        		} 
                break;
        }

		if(s->exit_flag)
			break;
    }

	/* Close up */
	log_debug(ZONE,"[AIM] Closing up a main thread");

	pth_event_free(to, PTH_FREE_ALL);

	at_session_end(s);

    pth_exit(NULL);

	return NULL;
}
	
void at_aim_session_parser(at_session s, jpacket jp)
{
    ati ti;
    int freed;
    char *ns;

    ti = s->ti;
    log_debug(ZONE, "[AIM] Parsing Packet on sessions");
        
    if(s->exit_flag > 0)
    {
        xmlnode_free(jp->x);
        return;
    }
    switch(jp->type)
    {
        case JPACKET_IQ:
            if(NSCHECK(xmlnode_get_tag(jp->x, "query"),NS_REGISTER))
            {
                /* Register the user */
                freed = at_register(ti, jp);
                break;
            }
            ns = xmlnode_get_attrib(jp->iq,"xmlns");
            freed = at_run_iqcb(ti, ns, jp);
            if(freed < 0)
            {
                /* XXX We could dup here and then return 0... */
                jutil_error(jp->x, TERROR_NOTFOUND);
                at_deliver(ti,jp->x);
                freed = 1;
            }
            break;
	    case JPACKET_MESSAGE:
            /* Put the message on the line for the session to handle */
            at_session_deliver(s, jp->x, jp->to);
            freed = 1;
            break;
        case JPACKET_S10N:
            /* Handle a S10N packet based on their subtype */
            log_debug(ZONE, "[AT] We got a s10n packet");
            freed = at_session_s10n(s, jp);
            break;
		case JPACKET_PRESENCE:
            freed = at_session_pres(s, jp);
            break;
        default:
            /* XXX do more!? */
            /*jutil_error(jp->x, TERROR_NOTACCEPTABLE);
            at_deliver(ti,jp->x);
            */
            xmlnode_free(jp->x);
            freed = 1;
            break;
	}

    if(freed==0)
        xmlnode_free(jp->x);
}

void at_session_deliver(at_session s, xmlnode x, jid from)
{
    /* XXX We need to check for the XHTML version of the msg */
    char *body;
    char *dest;
    struct aim_sendimext_args args;
    unsigned char *utf8_str;
    unsigned char *unistr;
    unsigned short unilen;

    if(s->icq)
        body = UTF8_to_str(xmlnode_pool(x),xmlnode_get_tag_data(x, "body"));
    else
        body = xmlnode_get_tag_data(x, "body");

    if(body == NULL || from->user == NULL)
        return;

    // don't forget to free!
    utf8_str = malloc(8192);
    unistr = malloc(8192);

    // only convert if AIM
    if(!s->icq) {
       msgconv_plain2aim(body, utf8_str, 8192);
       body = utf8_str;
    }

    if(!s->icq || (strstr(body, "SEND-SMS") != body)) {
        log_debug(ZONE, "[AIM] Sending a Message");

        args.destsn = from->user;
        args.flags = s->icq ? AIM_IMFLAGS_OFFLINE : 0;

        // simple enough to be ascii ?
        if(isAscii(body) || s->icq) {
            args.msg = body;
            args.msglen = strlen(body);
        }
        // unicode
        else {
            unilen = utf8_to_unicode(body, unistr, 8192);
            args.flags |= AIM_IMFLAGS_UNICODE;
            args.msg = unistr;
            args.msglen = unilen * 2;  // length has to be in bytes, not wchars
        }
        aim_send_im_ext(s->ass, &args);

        //aim_send_im(s->ass, from->user, s->icq ? AIM_IMFLAGS_OFFLINE : 0, body);
        /* XXX set offline flag only if destination is really offline */
    } else {
        log_debug(ZONE, "[AIM] Sending a SMS");
        aim_strsep(&body, ":");
        dest = aim_strsep(&body, ":");
        aim_icq_sendsms(s->ass,dest,body);
    }

    xmlnode_free(x);
    free(utf8_str);
    free(unistr);
    return;
}

void _at_session_save_buddies(xht ht, const char *key, void *data, void *arg)
{
    xmlnode item;
    xmlnode roster;
    char *user;

    roster = (xmlnode)arg;
    user = (char *)key;

    item = xmlnode_insert_tag(roster, "item");
    xmlnode_put_attrib(item, "name", user);

    return;
}

void at_session_end(at_session s)
{
    ati ti;
    xmlnode x;
    xmlnode xset;
    int ret;
    jid jend;

    if(s == NULL)
        log_warn(ZONE,"NULL Session told to end!");
    else
    {
        ti = s->ti;

        log_debug(ZONE, "[AT] Session (%s) told to end.", jid_full(s->cur));
        printf("Ending session for %s\n", jid_full(s->cur));
        jend = jid_new(s->p, jid_full(s->cur));
        jid_set(jend, NULL, JID_RESOURCE);
        xhash_zap(ti->session__list, jid_full(jend));

	// Signal all resources with the Disconnected" presence
	x = jutil_presnew(JPACKET__UNAVAILABLE, jid_full(jid_user(s->cur)), "Disconnected");

        xmlnode_put_attrib(x, "from", jid_full(s->from));
        at_deliver(ti,x);

        xset = xmlnode_new_tag("buddies");
        xhash_walk(s->buddies, &_at_session_save_buddies, xset);
        log_debug(ZONE, "Saving buddies: %s", xmlnode2str(xset));
        ret = at_xdb_set(ti, ti->i->id, s->cur, xset, AT_NS_ROSTER);
        if(ret == 1)
            xmlnode_free(xset);

        pth_msgport_destroy(s->mp_to);
        aim_logoff(s->ass);
        aim_session_kill(s->ass);
        log_debug(ZONE,"[AT] unlinking session");
        at_session_unlink_buddies(s);
        xhash_free(s->buddies);
        log_warn(ZONE, "[AT] Closing down session for %s", jid_full(s->cur));
        ppdb_free(s->at_PPDB);
        free(s->ass);
        pool_free(s->p);
    }
    return;
}

void _at_session_unlink_buddies(xht ht, const char *key, void *data, void *arg)
{
    ati ti;
    at_session s;
    xmlnode p;
    at_buddy buddy;

    s = (at_session)arg;
    buddy = (at_buddy)data;
    ti = s->ti;

    p = jutil_presnew(JPACKET__UNAVAILABLE, jid_full(s->cur), "Transport Disconnected");
    xmlnode_put_attrib(p, "from", jid_full(buddy->full));

    at_deliver(ti,p);

    return;
}
void at_session_unlink_buddies(at_session s)
{
    at_buddy cur, next;

    xhash_walk(s->buddies, &_at_session_unlink_buddies, s);
}

int session_walk(void *user_data, const void *key, void *data)
{
    log_debug(ZONE, "[AT] Walking key: %s data: %x", key, data);
    return 0;
}

at_session at_session_find_by_jid(ati ti, jid j)
{
    at_session s;
    char *res;

    if(j == NULL)
        return NULL;

    res = j->resource;
    jid_set(j, NULL, JID_RESOURCE);
    log_debug(ZONE, "[AT] Finding session for %s", jid_full(j));
    s = (at_session)xhash_get(ti->session__list, jid_full(j));
    j->resource = res;
    /* XXX LEAK ALERT! This is a jid handling problem that we're working on */
    j->full = NULL;

    return s;
}

aim_conn_t *_aim_select(aim_session_t *sess, 
                               pth_event_t ev, 
                               int *status)
{
    aim_conn_t *cur;
    fd_set fds, wfds;
    int maxfd = 0;
    int i, haveconnecting = 0;

    if (sess->connlist == NULL) {
        *status = -1;
        return NULL;
    }

    FD_ZERO(&fds);
    FD_ZERO(&wfds);
    maxfd = 0;

    for (cur = sess->connlist; cur; cur = cur->next) {
        if (cur->status & AIM_CONN_STATUS_INPROGRESS) {
            FD_SET(cur->fd, &wfds);
            haveconnecting++;
        }
        FD_SET(cur->fd, &fds);
        if (cur->fd > maxfd)
            maxfd = cur->fd;
    }
    
    /* 
    * If we have data waiting to be sent, return immediatly
    */
    if (!haveconnecting && (sess->queue_outgoing != NULL)) {
        *status = 1;
        return NULL;
    } 

    if ((i = pth_select_ev(maxfd+1, &fds, &wfds, NULL, NULL, ev))>=1) {
        for (cur = sess->connlist; cur; cur = cur->next) {
            if (FD_ISSET(cur->fd, &fds) || 
                ((cur->status & AIM_CONN_STATUS_INPROGRESS) && 
                FD_ISSET(cur->fd, &wfds))) 
            {
	            *status = 2;
	            return cur;
            }
        }
        *status = 0; /* shouldn't happen */
    } else if ((i == -1) && (errno == EINTR))/* treat inerpts as a timeout */
        *status = 0;
    else
        *status = i; /* can be 0 or -1 */

    if(pth_event_occurred(ev))
    {
        /* Possible msgport event (should be, no timeout) */
        *status = 3;
        return (aim_conn_t *)1;
    }

    return NULL;  /* no waiting or error, return */
}
