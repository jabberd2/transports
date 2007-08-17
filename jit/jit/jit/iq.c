/* --------------------------------------------------------------------------
   ICQ pthread jabber transport.

   Modified by Lukasz Karwacki <lukasm@wp-sa.pl>

   -------------------------------------------------------------------------- */

/** @file
 *  Handling of Jabber IQ packets */

#include "icqtransport.h"
#include <sys/utsname.h>

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

void it_iq_version(iti ti, jpacket jp);
void it_iq_vcard_server(iti ti, jpacket jp);
void it_iq_vcard(session s, jpacket jp);
void it_iq_last(session s, jpacket jp);
void it_iq_last_server(iti ti, jpacket jp);
void it_iq_time(iti ti, jpacket jp);
void it_iq_reg_get(session s, jpacket jp);
void it_iq_reg_remove(session s, jpacket jp);
void it_iq_search_get(session s, jpacket jp);
void it_iq_search_set(session s, jpacket jp);
void it_iq_gateway_get(session s, jpacket jp);
void it_iq_gateway_set(session s, jpacket jp);
void it_iq_browse_server(iti ti, jpacket jp);
void it_iq_browse_user(session s, jpacket jp);
void it_iq_disco_items_server(iti ti, jpacket jp);
void it_iq_disco_info_server(iti ti, jpacket jp);
void it_iq_disco_items_user(session s, jpacket jp);
void it_iq_disco_info_user(session s, jpacket jp);

void SendSearchUINRequest(session s,UIN_t uin);
void SendSearchUsersRequest(session s, 
                char *nick,
                char *first,
                char *last,
                char *email,
                char *city,
                int age_min, 
                int age_max,
                int sex_int,
                int online_only);

/** Process incoming IQ request */
void it_iq(session s, jpacket jp)
{
    char *ns;

    if (s->connected == 0)  {
      /* not yet connected, enqueue request */
      queue_elem queue;
      queue = pmalloco(jp->p,sizeof(_queue_elem));
      queue->elem = (void *)jp;
      
      QUEUE_PUT(s->queue,s->queue_last,queue);
      return; 
    } 
    
    ns = xmlnode_get_attrib(jp->iq,"xmlns");
    switch (jpacket_subtype(jp))
      {
      case JPACKET__GET:
        if (j_strcmp(ns,NS_REGISTER) == 0)
          it_iq_reg_get(s,jp);
        else if (j_strcmp(ns,NS_SEARCH) == 0)
          it_iq_search_get(s, jp);
        else if (j_strcmp(ns,NS_VERSION) == 0)
          it_iq_version(s->ti,jp);
        else if (j_strcmp(ns,NS_TIME) == 0)
          it_iq_time(s->ti,jp);
        else if (j_strcmp(ns,NS_GATEWAY) == 0)
          it_iq_gateway_get(s,jp);
        else if (j_strcmp(ns,NS_BROWSE) == 0)
          jp->to->user ? it_iq_browse_user(s,jp) : it_iq_browse_server(s->ti,jp);    
	else if (j_strcmp(ns,NS_DISCO_ITEMS) == 0)
	  jp->to->user ? it_iq_disco_items_user(s,jp) : it_iq_disco_items_server(s->ti,jp);
	else if (j_strcmp(ns,NS_DISCO_INFO) == 0)
	  jp->to->user ? it_iq_disco_info_user(s,jp) : it_iq_disco_info_server(s->ti,jp);
        else if (j_strcmp(ns,NS_VCARD) == 0)
          jp->to->user ? it_iq_vcard(s,jp) : it_iq_vcard_server(s->ti,jp);
        else if (j_strcmp(ns,NS_LAST) == 0)
          jp->to->user ? it_iq_last(s,jp) : it_iq_last_server(s->ti,jp);        
        else {
          jutil_error(jp->x,TERROR_NOTIMPL);
          it_deliver(s->ti,jp->x);
        }
        break;
        
    case JPACKET__SET:
      if (j_strcmp(ns,NS_REGISTER) == 0) {
        if(xmlnode_get_tag(jp->iq,"remove")) {
          it_iq_reg_remove(s,jp);
        }
        else {
          jutil_error(jp->x,TERROR_NOTIMPL);  
          it_deliver(s->ti,jp->x);
        }
      }      
      else if (j_strcmp(ns,NS_SEARCH) == 0)
        it_iq_search_set(s, jp);      
      else if (j_strcmp(ns,NS_GATEWAY) == 0)
        it_iq_gateway_set(s,jp);
      else {
        if (j_strcmp(ns,NS_VERSION) == 0 || j_strcmp(ns,NS_TIME) == 0)
          jutil_error(jp->x,TERROR_NOTALLOWED);
        else
          jutil_error(jp->x,TERROR_NOTIMPL);  
        
        it_deliver(s->ti,jp->x);
      }
      break;
      
    default:
      xmlnode_free(jp->x);
      break;
    }
}

/** Send registration info to Jabber */
void it_iq_reg_get(session s, jpacket jp)
{
    iti ti = s->ti;
    xmlnode q, qq, reg, x;
    char *key;

    reg = xdb_get(ti->xc,it_xdb_id(xmlnode_pool(jp->x),s->id,s->from->server),NS_REGISTER);
    if (reg == NULL)
    {
        jutil_error(jp->x,TERROR_NOTFOUND);
        it_deliver(ti,jp->x);
        return;
    }

    jutil_iqresult(jp->x);
    q = xmlnode_insert_tag(jp->x,"query");
    xmlnode_put_attrib(q,"xmlns",NS_REGISTER);
    xmlnode_insert_node(q,xmlnode_get_firstchild(reg));
    xmlnode_free(reg);

    /* we don't need all NS_REGISTER tags here */
    xmlnode_hide(xmlnode_get_tag(q,"nick"));
    xmlnode_hide(xmlnode_get_tag(q,"first"));
    xmlnode_hide(xmlnode_get_tag(q,"last"));
    xmlnode_hide(xmlnode_get_tag(q,"email"));

    /* don't send the password to client */
    xmlnode_hide(xmlnode_get_tag(q,"password"));
    xmlnode_insert_tag(q,"password");

    while ((x = xmlnode_get_tag(q,"key")) != NULL) xmlnode_hide(x);

    key = jutil_regkey(NULL,jid_full(jp->from));
    xmlnode_insert_cdata(xmlnode_insert_tag(q,"key"),key,-1);
    xmlnode_insert_cdata(xmlnode_insert_tag(q,"instructions"),ti->registration_instructions,-1);
    xmlnode_insert_tag(q,"registered");

    /* create and fill in jabber:x:data form */
	if (!ti->no_x_data) {
	  qq = xdata_create(q, "form");
	  xmlnode_insert_cdata(xmlnode_insert_tag(qq,"title"),"Registration in JIT",-1);
	  xmlnode_insert_cdata(xmlnode_insert_tag(qq,"instructions"),ti->registration_instructions,-1);
	  
	  xdata_insert_field(qq,"text-single","username","UIN",xmlnode_get_tag_data(q,"username"));
	  xdata_insert_field(qq,"text-private","password","Password",xmlnode_get_tag_data(q,"password"));
	  xdata_insert_field(qq,"hidden","key",NULL,key);
	  xdata_insert_field(qq,"hidden","registered",NULL,NULL);
	}
    
    it_deliver(ti,jp->x);
}

/** Unregister, remove registration from XDB */
void it_iq_reg_remove(session s, jpacket jp)
{
    iti ti = s->ti;
    jid id;
    contact c;
    xmlnode x, pres;

    log_debug(ZONE,"Unregistering user '%s'",jid_full(s->id));

    id = it_xdb_id(jp->p,s->id,s->from->server);
    if (xdb_set(ti->xc,id,NS_REGISTER,NULL)) 
    {
        jutil_error(jp->x,(terror){500,"XDB troubles"});
        it_deliver(ti,jp->x);
        return;
    }

    x = jutil_presnew(JPACKET__UNSUBSCRIBE,jid_full(s->id),NULL);
    /* XXX icq contacts ??? */
    for (c = s->contacts; c != NULL; c = c->next)
    {
        pres = xmlnode_dup(x);
        xmlnode_put_attrib(pres,"from",jid_full(it_uin2jid(c->p,c->uin,s->from->server)));
        it_deliver(ti,pres);
    }

    /* our self */
    xmlnode_put_attrib(x,"from",jid_full(s->from));
    it_deliver(s->ti,x);

    jutil_iqresult(jp->x);
    it_deliver(s->ti,jp->x);

    EndClient(s); // will call it_session_end later
}

/** Send search information to Jabber */
void it_iq_search_get(session s, jpacket jp) 
{ 
  iti ti = s->ti; 
  xmlnode q, qq; 
  char *key;

  if (ti->search_instructions == NULL || jp->to->user != NULL) { 
    jutil_error(jp->x,TERROR_NOTALLOWED); 
    it_deliver(s->ti,jp->x); 
    return; 
  } 

  jutil_iqresult(jp->x); 
  q = xmlnode_insert_tag(jp->x,"query"); 
  xmlnode_put_attrib(q,"xmlns",NS_SEARCH); 

  xmlnode_insert_tag(q,"username");
  xmlnode_insert_tag(q,"email");
  xmlnode_insert_tag(q,"nick");
  xmlnode_insert_tag(q,"first");
  xmlnode_insert_tag(q,"last");
  xmlnode_insert_tag(q,"age_min");
  xmlnode_insert_tag(q,"age_max");
  xmlnode_insert_tag(q,"city");
  xmlnode_insert_tag(q,"sex");
  xmlnode_insert_tag(q,"online");
    
  xmlnode_insert_cdata(xmlnode_insert_tag(q,"instructions"),ti->search_instructions,-1); 
  key = jutil_regkey(NULL,jid_full(jp->from));
  xmlnode_insert_cdata(xmlnode_insert_tag(q,"key"),key,-1); 
  
  if (!s->ti->no_x_data) {
	q = xdata_create(q,"form");
	xmlnode_insert_cdata(xmlnode_insert_tag(q,"title"),"Search in JIT",-1);
	xmlnode_insert_cdata(xmlnode_insert_tag(q,"instructions"),
						 ti->search_instructions,-1);
	
	xdata_insert_field(q,"text-single","username","UIN",NULL);
	xdata_insert_field(q,"text-single","email","E-mail",NULL);
	xdata_insert_field(q,"text-single","nick",
					   it_convert_windows2utf8(jp->p,LNG_SEARCH_NICKNAME),
					   NULL);
	xdata_insert_field(q,"text-single","first",
					   it_convert_windows2utf8(jp->p,LNG_SEARCH_NICKNAME_FIRST)
					   ,NULL);
	xdata_insert_field(q,"text-single","last",
					   it_convert_windows2utf8(jp->p,LNG_SEARCH_NICKNAME_LAST),
					   NULL);
	xdata_insert_field(q,"text-single","age_min",
					   it_convert_windows2utf8(jp->p,LNG_SEARCH_AGE_MIN),
					   NULL);
	xdata_insert_field(q,"text-single","age_max",
					   it_convert_windows2utf8(jp->p,LNG_SEARCH_AGE_MAX),
					   NULL);
	xdata_insert_field(q,"text-single","city",
					   it_convert_windows2utf8(jp->p,LNG_SEARCH_CITY),
					   NULL);
	
	qq = xdata_insert_field(q,"list-single","sex",
							it_convert_windows2utf8(jp->p,LNG_SEARCH_GENDER),
							"0");
	xdata_insert_option(qq,"-","0");
	xdata_insert_option(qq,
						it_convert_windows2utf8(jp->p,LNG_SEARCH_FEMALE),
						"1");
	xdata_insert_option(qq,
						it_convert_windows2utf8(jp->p,LNG_SEARCH_MALE),
						"2");
	xdata_insert_field(q,"boolean","online",
					   it_convert_windows2utf8(jp->p,LNG_SEARCH_ONLINE),
					   NULL);
	
	xdata_insert_field(q,"hidden","key",NULL,key);
  }	
  it_deliver(ti,jp->x); 
}

/** Send search result to Jabber */
void it_iq_search_result(session s, UIN_t uin, meta_gen *info, void *arg) 
{ 
    jpacket jp = (jpacket) arg;

    if (info != NULL)
    {
        xmlnode item;
        pool p = jp->p;
	
	if (xdata_test(jp->iq,"result")) {
	    item = xdata_insert_node(jp->iq,"item");

	    xdata_insert_field(item,"jid-single","jid",NULL,jid_full(it_uin2jid(p,uin,s->from->server)));
	    xdata_insert_field(item,NULL,"email",NULL,it_convert_windows2utf8(p,info->email));
	    xdata_insert_field(item,NULL,"nick",NULL,it_convert_windows2utf8(p,info->nick));
	    xdata_insert_field(item,NULL,"first",NULL,it_convert_windows2utf8(p,info->first));
	    xdata_insert_field(item,NULL,"last",NULL,it_convert_windows2utf8(p,info->last));

	    xdata_insert_field(item,NULL,"status",NULL,
						   it_convert_windows2utf8(p,jit_status2fullinfo(info->status))
						   );

	    xdata_insert_field(item,NULL,"authreq",NULL,info->auth?"yes":"no");
	}
	else {
	    item = xmlnode_insert_tag(jp->iq,"item");

	    xmlnode_put_attrib(item,"jid",jid_full(it_uin2jid(p,uin,s->from->server)));

	    xmlnode_insert_cdata(xmlnode_insert_tag(item,"email"),
                             it_convert_windows2utf8(p,info->email),(unsigned int)-1);

	    xmlnode_insert_cdata(xmlnode_insert_tag(item,"nick"),
                             it_convert_windows2utf8(p,info->nick),(unsigned int)-1);

	    xmlnode_insert_cdata(xmlnode_insert_tag(item,"first"),
                             it_convert_windows2utf8(p,info->first),(unsigned int)-1);

	    xmlnode_insert_cdata(xmlnode_insert_tag(item,"last"),
                             it_convert_windows2utf8(p,info->last),(unsigned int)-1);
	
	    xmlnode_insert_cdata(xmlnode_insert_tag(item,"status"),
							 jit_status2fullinfo(info->status),
							 (unsigned int)-1);

           xmlnode_insert_cdata(xmlnode_insert_tag(item,"authreq"),
           info->auth?"yes":"no",(unsigned int)-1);
	}
    }
    else
        it_deliver(s->ti,jp->x);
}

/** Perform search, propagate to C++ backend */
void it_iq_search_set(session s, jpacket jp) 
{ 
  xmlnode query = jp->iq; 
  pool p; 
  UIN_t uin;
  char * tmp;
  char *first, *last, *nick, *email, *country, *city, *sex;
  int age_min,age_max,online_only,sex_int;
  int xdata;
  xmlnode q;

  if (s->ti->search_instructions == NULL || s->pend_search != NULL || jp->to->user != NULL) 
     { 
         jutil_error(jp->x,TERROR_NOTALLOWED); 
         it_deliver(s->ti,jp->x); 
         return; 
     } 

  p = jp->p; 

  xdata = xdata_test(query,"submit");

  if (xdata) {
	uin = it_strtouin(xdata_get_data(query,"username")); 
	nick = it_convert_utf82windows(p,xdata_get_data(query,"nick")); 
	first = it_convert_utf82windows(p,xdata_get_data(query,"first")); 
	last = it_convert_utf82windows(p,xdata_get_data(query,"last"));
	email = it_convert_utf82windows(p,xdata_get_data(query,"email"));
  
	country = it_convert_utf82windows(p,xdata_get_data(query,"country"));
	city = it_convert_utf82windows(p,xdata_get_data(query,"city"));
	sex_int = j_atoi(it_convert_utf82windows(p,xdata_get_data(query,"sex")),0);
	age_min = j_atoi(it_convert_utf82windows(p,xdata_get_data(query,"age_min")),0);
	age_max = j_atoi(it_convert_utf82windows(p,xdata_get_data(query,"age_max")),0);
	online_only = j_atoi(it_convert_utf82windows(p,xdata_get_data(query,"online")),0);
  }
  else {
	uin = it_strtouin(xmlnode_get_tag_data(query,"username")); 
	nick = it_convert_utf82windows(p,xmlnode_get_tag_data(query,"nick")); 
	first = it_convert_utf82windows(p,xmlnode_get_tag_data(query,"first")); 
	last = it_convert_utf82windows(p,xmlnode_get_tag_data(query,"last"));
	email = it_convert_utf82windows(p,xmlnode_get_tag_data(query,"email"));
  
	country = it_convert_utf82windows(p,xmlnode_get_tag_data(query,"country"));
	city = it_convert_utf82windows(p,xmlnode_get_tag_data(query,"city"));
	sex = it_convert_utf82windows(p,xmlnode_get_tag_data(query,"sex"));
	age_min = j_atoi(it_convert_utf82windows(p,xmlnode_get_tag_data(query,"age_min")),0);
	age_max = j_atoi(it_convert_utf82windows(p,xmlnode_get_tag_data(query,"age_max")),0);
	tmp = it_convert_utf82windows(p,xmlnode_get_tag_data(query,"online"));
	online_only=(tmp && tmp[0] && tmp[0]!='0');
  
	if (sex==NULL)
	  sex_int = 0;
	else
	  if ((j_strncasecmp(sex,"W",1)==0)||(j_strncasecmp(sex,"F",1)==0)||
		    (j_strncasecmp(sex,"K",1)==0))
		sex_int = 1; /* FEMALE */
	  else
		sex_int = 2;/* MALE */
  }

  /* Make sure at least ONE of the possible search parameters is specified */
  if (uin == 0 && nick == NULL && first == NULL && last == NULL && email == NULL
   && city == NULL && age_min ==0 && age_max == 0) 
    { 
      jutil_error(jp->x,(terror) {406,"No valid search parameters specified"}); 
      it_deliver(s->ti,jp->x); 
      return; 
    } 
  
  jutil_iqresult(jp->x); 
  q = xmlnode_insert_tag(jp->x,"query"); 
  xmlnode_put_attrib(q,"xmlns",NS_SEARCH); 
  
  if (xdata)
  {
	q = xdata_create(q,"result");
	xmlnode_insert_cdata(xmlnode_insert_tag(q,"title"),"JIT Search Results",-1);

	q = xmlnode_insert_tag(q, "reported");
	xdata_insert_field(q,"jid-single","jid","JID",NULL);
	xdata_insert_field(q,NULL,"email","E-mail",NULL);
	xdata_insert_field(q,NULL,"nick","Nickname",NULL);
	xdata_insert_field(q,NULL,"first","First Name",NULL);
	xdata_insert_field(q,NULL,"last","Last Name",NULL);
	xdata_insert_field(q,NULL,"status","Status",NULL);
	xdata_insert_field(q,NULL,"authreq","Auth. required",NULL);
  }
  
  jpacket_reset(jp); 

  s->pend_search = pmalloco(jp->p,sizeof(_pendmeta));
  s->pend_search->p = jp->p;
  s->pend_search->cb = (void *) &it_iq_search_result;
  s->pend_search->arg = (void *) jp;
    
  if (uin != 0)
    SendSearchUINRequest(s,uin);
  else
    SendSearchUsersRequest(s, 
               nick ? nick : "",
               first ? first : "",
               last ? last : "",
               email ? email : "", 
               city ? city : "", 
               age_min, 
               age_max,
               sex_int,
               online_only);
} 

/** Jabber wants to know the JID associated to a UIN when using this transport */
void it_iq_gateway_get(session s, jpacket jp)
{
    if (jp->to->user == NULL)
    {
        xmlnode q;

        jutil_iqresult(jp->x);
        q = xmlnode_insert_tag(jp->x,"query");
        xmlnode_put_attrib(q,"xmlns",NS_GATEWAY);
        xmlnode_insert_cdata(xmlnode_insert_tag(q,"desc"),"Enter the user's UIN",-1);
        xmlnode_insert_tag(q,"prompt");
    }
    else
        jutil_error(jp->x,TERROR_NOTALLOWED);

    it_deliver(s->ti,jp->x);
}

/** Return JID corresponding to UIN */
void it_iq_gateway_set(session s, jpacket jp)
{
    char *user, *id;

    user = xmlnode_get_tag_data(jp->iq,"prompt");
    id = user ? spools(jp->p,user,"@",jp->to->server,jp->p) : NULL;
    if (id && it_strtouin(user))
    {
        xmlnode q;

        jutil_iqresult(jp->x);
        q = xmlnode_insert_tag(jp->x,"query");
        xmlnode_put_attrib(q,"xmlns",NS_GATEWAY);
        xmlnode_insert_cdata(xmlnode_insert_tag(q,"prompt"),id,-1);
    }
    else
        jutil_error(jp->x,TERROR_BAD);

    it_deliver(s->ti,jp->x);
}

/** Present namespaces supported by this transport */
void it_iq_browse_server(iti ti, jpacket jp)
{
    xmlnode q;

    q = xmlnode_insert_tag(jutil_iqresult(jp->x),"service");
    xmlnode_put_attrib(q,"xmlns",NS_BROWSE);

    xmlnode_put_attrib(q,"type","icq");
    xmlnode_put_attrib(q,"jid",jp->to->server);
    if (ti->sms_id && ti->sms_name && j_strcasecmp(jp->to->server, ti->sms_id)==0)
	xmlnode_put_attrib(q,"name",ti->sms_name);
    else
	xmlnode_put_attrib(q,"name",xmlnode_get_tag_data(ti->vcard,"FN"));

    xmlnode_insert_cdata(xmlnode_insert_tag(q,"ns"),NS_REGISTER,-1);
    xmlnode_insert_cdata(xmlnode_insert_tag(q,"ns"),NS_SEARCH,-1);
    xmlnode_insert_cdata(xmlnode_insert_tag(q,"ns"),NS_GATEWAY,-1);

    it_deliver(ti,jp->x);
}

void it_iq_disco_items_server(iti ti, jpacket jp)
{
    xmlnode q;

    q = xmlnode_insert_tag(jutil_iqresult(jp->x),"query");
    xmlnode_put_attrib(q,"xmlns",NS_DISCO_ITEMS);

    it_deliver(ti,jp->x);
}

void it_iq_disco_info_server(iti ti, jpacket jp)
{
    xmlnode q, info;

    q = xmlnode_insert_tag(jutil_iqresult(jp->x),"query");
    xmlnode_put_attrib(q,"xmlns",NS_DISCO_INFO);

    info = xmlnode_insert_tag(q,"identity");
    xmlnode_put_attrib(info,"category", "gateway");
    xmlnode_put_attrib(info,"type", "icq");
    if (ti->sms_id && ti->sms_name && j_strcasecmp(jp->to->server, ti->sms_id)==0)
	xmlnode_put_attrib(info,"name",ti->sms_name);
    else
	xmlnode_put_attrib(info,"name",xmlnode_get_tag_data(ti->vcard,"FN"));

    info = xmlnode_insert_tag(q,"feature");
    xmlnode_put_attrib(info,"var",NS_REGISTER);

    info = xmlnode_insert_tag(q,"feature");
    xmlnode_put_attrib(info,"var",NS_SEARCH);

    info = xmlnode_insert_tag(q,"feature");
    xmlnode_put_attrib(info,"var",NS_VERSION);

    info = xmlnode_insert_tag(q,"feature");
    xmlnode_put_attrib(info,"var",NS_TIME);

    info = xmlnode_insert_tag(q,"feature");
    xmlnode_put_attrib(info,"var",NS_GATEWAY);

    info = xmlnode_insert_tag(q,"feature");
    xmlnode_put_attrib(info,"var",NS_VCARD);

    info = xmlnode_insert_tag(q,"feature");
    xmlnode_put_attrib(info,"var",NS_LAST);

    it_deliver(ti,jp->x);
}

void it_iq_browse_user(session s, jpacket jp)
{
    xmlnode browse;

    if (s->type == stype_register)
    {
      queue_elem queue;

      queue = pmalloco(jp->p,sizeof(_queue_elem));
      queue->elem = (void *)jp;
      
      QUEUE_PUT(s->queue,s->queue_last,queue);
      return;
    }

    if (it_jid2uin(jp->from) == 0)
    {
        jutil_error(jp->x,TERROR_BAD);
        it_deliver(s->ti,jp->x);
        return;
    }

    jutil_iqresult(jp->x);
    browse = xmlnode_insert_tag(jp->x,"user");
    xmlnode_put_attrib(browse,"xmlns",NS_BROWSE);
    xmlnode_put_attrib(browse,"jid",jid_full(jid_user(jp->to)));
    xmlnode_put_attrib(browse,"type","user");

    it_deliver(s->ti,jp->x);
}

void it_iq_disco_items_user(session s, jpacket jp)
{
    xmlnode q;
    UIN_t uin;

    if (s->type == stype_register)
    {
	queue_elem queue;

	queue = pmalloco(jp->p,sizeof(_queue_elem));
	queue->elem = (void *)jp;
      
	QUEUE_PUT(s->queue,s->queue_last,queue);
	return;
    }

    uin = it_jid2uin(jp->from);
    if (uin == 0)
    {
	jutil_error(jp->x,TERROR_BAD);
	it_deliver(s->ti,jp->x);
	return;
    }

    jutil_iqresult(jp->x);
    q = xmlnode_insert_tag(jp->x,"query");
    xmlnode_put_attrib(q,"xmlns",NS_DISCO_ITEMS);

    it_deliver(s->ti,jp->x);
}

void it_iq_disco_info_user(session s, jpacket jp)
{
    xmlnode q,info;
    UIN_t uin;
    char uinstr[21];

    if (s->type == stype_register)
    {
	queue_elem queue;

	queue = pmalloco(jp->p,sizeof(_queue_elem));
	queue->elem = (void *)jp;
      
	QUEUE_PUT(s->queue,s->queue_last,queue);
	return;
    }

    uin = it_jid2uin(jp->from);
    if (uin == 0)
    {
	jutil_error(jp->x,TERROR_BAD);
	it_deliver(s->ti,jp->x);
	return;
    }

    jutil_iqresult(jp->x);
    q = xmlnode_insert_tag(jp->x,"query");
    xmlnode_put_attrib(q,"xmlns",NS_DISCO_INFO);
    
    info = xmlnode_insert_tag(q,"identity");
    xmlnode_put_attrib(info,"category","client");
    xmlnode_put_attrib(info,"type","pc");
    snprintf(uinstr,21,"%d",uin);
    xmlnode_put_attrib(info,"name",uinstr);

    info = xmlnode_insert_tag(q,"feature");
    xmlnode_put_attrib(info,"var",NS_VCARD);

    info = xmlnode_insert_tag(q,"feature");
    xmlnode_put_attrib(info,"var",NS_LAST);

    it_deliver(s->ti,jp->x);
}

/** Send reply to a version inquiry to Jabber */
void it_iq_version(iti ti, jpacket jp)
{
    char buf[1000];
    xmlnode x, q;
    struct utsname un;

    x = jutil_iqresult(jp->x);

    q = xmlnode_insert_tag(x,"query");
    xmlnode_put_attrib(q,"xmlns",NS_VERSION);
	
    xmlnode_insert_cdata(xmlnode_insert_tag(q,"name"),"JIT - Jabber ICQ Transport by Lukas",-1);

    snprintf(buf,1000,"Jabber: %s \n ICQ: %s",VERSION,MOD_VERSION);
    xmlnode_insert_cdata(xmlnode_insert_tag(q,"version"),buf,-1);

    uname(&un);
    snprintf(buf,1000,"%s %s",un.sysname,un.release);
    xmlnode_insert_cdata(xmlnode_insert_tag(q,"os"),buf,-1);

    it_deliver(ti,x);
}

/** Send our time */
void it_iq_time(iti ti, jpacket jp)
{
    xmlnode x, q;

    x = jutil_iqresult(jp->x);

    q = xmlnode_insert_tag(x,"query");
    xmlnode_put_attrib(q,"xmlns",NS_TIME);

    xmlnode_insert_cdata(xmlnode_insert_tag(q,"utc"),jutil_timestamp(),-1);
    xmlnode_insert_cdata(xmlnode_insert_tag(q,"tz"),tzname[0],-1);

    it_deliver(ti,x);
}

/** Send transport's uptime */
void it_iq_last_server(iti ti, jpacket jp)
{
    xmlnode x, q;
    char str[10];

    x = jutil_iqresult(jp->x);

    snprintf(str,10,"%d",time(NULL) - ti->start);
    q = xmlnode_insert_tag(x,"query");
    xmlnode_put_attrib(q,"xmlns",NS_LAST);
    xmlnode_put_attrib(q,"seconds",str);

    it_deliver(ti,x);
}

unsigned long GetLast(session s,UIN_t uin);

/** Send contact's last online time to Jabber */
void it_iq_last(session s, jpacket jp)
{
    UIN_t uin;
    unsigned long t;

    uin = it_jid2uin(jp->to);
    if (uin == 0)
    {
        jutil_error(jp->x,TERROR_BAD);
        it_deliver(s->ti,jp->x);
        return;
    }

    t = GetLast(s,uin);

    if (t > 0) {
      xmlnode x, q;
      char str[20];

      x = jutil_iqresult(jp->x);      
      snprintf(str,20,"%d",time(NULL) - t);
      q = xmlnode_insert_tag(x,"query");
      xmlnode_put_attrib(q,"xmlns",NS_LAST);
      xmlnode_put_attrib(q,"seconds",str);      
      it_deliver(s->ti,x);
    }
    else
      xmlnode_free(jp->x);
}

/** Send transport's vCard */
void it_iq_vcard_server(iti ti, jpacket jp)
{
    xmlnode_insert_node(jutil_iqresult(jp->x),ti->vcard);
    it_deliver(ti,jp->x);
}

void GetVcard(session s,jpacket jp,UIN_t uin);

/** Get vCard for contact. */
void it_iq_vcard(session s, jpacket jp)
{
    UIN_t uin;
    xmlnode data;

    uin = it_jid2uin(jp->to);
    if (uin == 0)
    {
        jutil_error(jp->x,TERROR_BAD);
        it_deliver(s->ti,jp->x);
        return;
    }

    if (s->vcard_get) {
      jutil_error(jp->x,TERROR_NOTALLOWED); 
      it_deliver(s->ti,jp->x); 
      return; 
    }

    jutil_iqresult(jp->x);
    jp->iq = data = xmlnode_insert_tag(jp->x,"vCard");
    xmlnode_put_attrib(data,"xmlns",NS_VCARD);
    xmlnode_put_attrib(data,"version","3.0");
    xmlnode_put_attrib(data,"prodid","-//HandGen//NONSGML vGen v1.0//EN");

    /* Request meta-info for the user, propagate to C++ backend. */
    GetVcard(s,jp,uin);
}
