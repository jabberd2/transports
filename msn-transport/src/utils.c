/* --------------------------------------------------------------------------
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

#include "utils.h"
#include "md5.h"


/*
   Contributed by David Carter

   MSN notifications (NOT messages) can contain illegal xml where ampersands 
   are not properly escaped using the &amp; entity. For example, here's such 
   an illegal piece of XML found in an actual MSn notification:

   <ACTION url="http://g.msn.com/3ALMSNTRACKING/111100500ToastAction?
   http://autos.msn.com/everyday/trafficreport.aspx?Metro=MIN&Incident=
   22868&src=edcp_toast&strela=1"/>

   as you can see, the url attribute contains illegal ampersands. These 
   ampersands should be written as &amp;. The effect of this illegal XML is 
   it cannot be parsed by the XML parsing utilities. Calling this function will 
   create a new string with the ampersands "fixed"...that is, it replaces the 
   ampersands with the string &amp;. The function will only operate on "stray" 
   ampersands; it won't try to fix legal entities (anything of the form 
   "&variable;" where "variable" can be any variable string containg text legal 
   for XML entities). 
*/
char *mt_fix_amps(pool p, char *strIn) 
{
    int x=0, iLen = 0;
    int iNumAmpers=0, iFirst=-1;
    char * strOut = NULL, * ptrOut = NULL, * ptrIn = NULL;
   
    iLen = strlen(strIn);
    
    if (iLen <= 0) return(strIn);
   
    //count # of ampersands in order to allocate enough memory for the new string 
    for (x=0; strIn[x] != '\0'; x++)
    {
        if (strIn[x] == '&')
        {
            if (iFirst == -1) iFirst=x;
            iNumAmpers++;
        } 

    } 
    if (iNumAmpers==0) return strIn;
    
    strOut = (char*) pmalloc(p,iLen+(iNumAmpers*4)+1);
    ptrOut = strOut;
    ptrIn = strIn;
    x = iFirst;
    while (ptrIn[x] != '\0')
    {
        if (ptrIn[x] == '&')
        {
            //copy everything upto and including the '&' to the new buffer
            strncpy(ptrOut,ptrIn, x+1);
            ptrOut += x+1;
            if (!mt_is_entity(ptrIn+x)) 
            {
                strcpy(ptrOut, "amp;");
                ptrOut += 4;
            }
            ptrIn  += x+1;
            x=0;
        } 
        else 
        {
            x++;
        }
    }
    strcpy(ptrOut, ptrIn);
    return(strOut);
}

/* 
   checks to see if the input string starts with a valid XML entity. 
   String checked should start with an &.

   evaluates to false if the string doesn't start with a valid XML entity
*/
int mt_is_entity(char * strIn) 
{
    int x=1;
    if (strIn[0] != '&' ) return(0);
    while (x < 31) //assume entities should be < 30 in length
    {
        switch(strIn[x]) 
        {
        case ';':
           return(1); 
        case '&':
	case ' ':
        case '<':
        case '>':
        case '\n':
        case '\r':
        case '\"':
        case '\'':
	case '\0':
            return(0); 
        }
	x++;
    }
    return (0);
}

void lowercase(char *string)
{
    int  i = 0;
    if(string == 0) {
        log_debug(ZONE, "lowercase(NULL) was called! Oops\n");
        return;
    }
    while(string[i] != 0)
    {
        string[i] = tolower(string[i]);
        i++;
    }
    return;
}

jid mt_mid2jid(pool p, char *mid, char *server)
{
    jid id;
    char *ptr;

    assert(mid && server);

    id = jid_new(p,server);
    id->user = pstrdup(p,mid);

    if ((ptr = strchr(id->user,'@')) == NULL)
        return NULL;

    *ptr = '%';

    return id;
}

char *mt_mid2jid_full(pool p, char *mid, char *server)
{
    char *ret;
    int len;

    len = strlen(mid) + strlen(server) + 2;
    ret = pmalloc(p,len);
    snprintf(ret,len,"%s@%s",mid,server);
    *(strchr(ret,'@')) = '%';

    return ret;
}

char *mt_jid2mid(pool p, jid id)
{
    char *user, *ptr;

    assert(id && id->user);

    user = pstrdup(p,id->user);
    if ((ptr = strchr(user,'%')) == NULL)
        return NULL;

    *ptr = '@';

    for (ptr = user; *ptr != '@'; ptr++)
        *ptr = tolower(*ptr);

    return user;
}

void mt_jpbuf_debug(void *arg)
{
    jpbuf buf = (jpbuf) arg;
    assert(buf->head == NULL);
}

jpbuf mt_jpbuf_new(pool p)
{
    jpbuf buf;

    buf = pmalloc(p,sizeof(_jpbuf));
    buf->head = buf->tail = NULL;
    pool_cleanup(p,&mt_jpbuf_debug,(void *) buf);
    return buf;
}

void mt_jpbuf_en(jpbuf buf, jpacket jp, jpbuf_cb cb, void *arg)
{
    jpnode n;

    log_debug(ZONE,"enqueue %X:%X",buf,jp);

    n = pmalloc(jp->p,sizeof(_jpnode));
    n->jp = jp;
    n->cb = cb;
    n->arg = arg;
    n->next = NULL;

    if (buf->head)
    {
        buf->tail->next = n;
        buf->tail = n;
    }
    else
        buf->head = buf->tail = n;
}

jpacket mt_jpbuf_de(jpbuf buf)
{
    if (buf->head)
    {
        jpacket ret;
        ret = buf->head->jp;
        buf->head = buf->head->next;
        return ret;
    }
    return NULL;
}

void mt_jpbuf_flush(jpbuf buf)
{
    jpnode tmp, cur = buf->head;

    buf->head = NULL;
    while (cur != NULL)
    {
        tmp = cur;
        cur = cur->next;
        (tmp->cb)(tmp->jp,tmp->arg);
    }
}

void mt_append_char(spool sp, char ch)
{
    char buf[2];
    snprintf(buf,2,"%c",ch);
    spool_add(sp,buf);
}

char *mt_encode(pool p, char *s)
{
    spool sp = spool_new(p);
    int i, len = strlen(s);
    unsigned char ch;

    for (i = 0; i < len; i++)
    {
        ch = s[i];

        if (should_escape(ch))
        {
            char esc[4], c;

            esc[0] = '%';
            c = ch / 16;
            c += c > 9 ? 'A' - 10 : '0';
            esc[1] = c;

            c = ch % 16;
            c += c > 9 ? 'A' - 10 : '0';
            esc[2] = c;
            esc[3] = '\0';

            spool_add(sp,esc);
        }
        else
            mt_append_char(sp,ch);
    }

    return spool_print(sp);
}

int mt_hex2int(char c)
{
    if (c >= 'A' && c <= 'F') return (c - 'A' + 10);
    if (c >= 'a' && c <= 'f') return (c - 'a' + 10);
    if (c >= '0' && c <= '9') return (c - '0');
    return 0;
}

char *mt_decode(pool p, char *s)
{
    spool sp = spool_new(p);
    int l, i = 0;

    l = strlen(s);
    while (i < l)
    {
        int c = s[i++];

        if (c == '%' && (i + 2 <= l))
        {
            int hb, lb;
            hb = mt_hex2int(s[i++]);
            lb = mt_hex2int(s[i++]);
            c = hb * 16 + lb;
        }

        mt_append_char(sp,c);
    }

    return spool_print(sp);
}

ustate mt_show2state(char *show)
{
    ustate ret;

    if (show == NULL)
        ret = ustate_nln;
    else if (strcmp(show,"dnd") == 0)
        ret = ustate_bsy;
    else if (strcmp(show,"xa") == 0)
        ret = ustate_awy;
    else if (strcmp(show,"away") == 0)
        ret = ustate_awy;
    else
        ret = ustate_nln;

    return ret;
}

char *mt_state2char(ustate state)
{
    static const char *states[] =
    {
        "NLN",
        "FLN",
        "BSY",
        "AWY",
        "PHN",
        "BRB",
        "IDL",
        "LUN",
    };

    return (char *) states[state];
}

ustate mt_char2state(char *state)
{
    ustate ret;

    if (j_strcmp(state,"NLN") == 0)
        ret = ustate_nln;
    else if (j_strcmp(state,"BSY") == 0)
        ret = ustate_bsy;
    else if (j_strcmp(state,"IDL") == 0)
        ret = ustate_idl;
    else if (j_strcmp(state,"BRB") == 0)
        ret = ustate_brb;
    else if (j_strcmp(state,"AWY") == 0)
        ret = ustate_awy;
    else if (j_strcmp(state,"PHN") == 0)
        ret = ustate_phn;
    else if (j_strcmp(state,"LUN") == 0)
        ret = ustate_lun;
    else
        ret = ustate_fln;

    return ret;
}

void mt_replace_newline(spool sp, char *str)
{
    char *ptr, *pos = str;

    ptr = strchr(pos,'\n');
    if (ptr != NULL)
    {
        do
        {
            if (*ptr - 1 != '\r')
            {
                *ptr = '\0';
                spooler(sp,pos,"\r\n",sp);
                *ptr = '\n';
            }
            pos = ptr + 1;
        }
        while ((ptr = strchr(pos,'\n')));
    }

    spool_add(sp,pos);
}

int mt_safe_user(char *user)
{
    char *str;
    int len, flag = 0;

    for (len = 0, str = user; *str != '\0'; len++, str++)
    {
        if (*str <= 32 || *str == '\r' || *str == '\n'||
            *str == ':' || *str == '<' || *str == '>' ||
            *str == '\'' || *str == '"' || *str == '&')
            return 0;
        if (*str == '@')
            flag++;
    }

    return (len > 0 && len < 129 && flag == 1);
}

xmlnode mt_presnew(int type, char *to, char *from)
{
    xmlnode x = jutil_presnew(type,to,NULL);
    xmlnode_put_attrib(x,"from",from);
    return x;
}

jid  mt_xdb_id(pool p, jid id, char *server)
{
    jid ret;

    ret = jid_new(p,server);
    jid_set(ret,spools(p,id->user,"%",id->server,p),JID_USER);

    return ret;
}

void mt_md5hash(char *a, char *b, char result[33])
{
    md5_state_t state;
    unsigned char digest[16];
    int i;

    md5_init(&state);
    md5_append(&state,a,strlen(a));
    md5_append(&state,b,strlen(b));
    md5_finish(&state,(md5_byte_t *) digest);

    for (i = 0; i < 16; i++)
        snprintf(result + (i * 2),3,"%02x",digest[i]);
}
