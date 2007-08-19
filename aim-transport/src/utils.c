/*
 * Jabber AIM-Transport libfaim interface
 *
*/

#include "aimtrans.h"
#include <ctype.h>

iconv_t fromutf8, toutf8;

/* Thanks to the gaim dudes for this func */
char *strip_html(char * text,pool p)
{
	int i, j;
	int visible = 1;
	char *text2 = pmalloc(p, strlen(text) + 1);
	
    if(text == NULL)
        return NULL;

	strcpy(text2, text);
	for (i = 0, j = 0;text2[i]; i++)
	{
		if(text2[i]=='<')
		{	
			visible = 0;
			continue;
		}
		else if(text2[i]=='>')
		{
			visible = 1;
			continue;
		}
		if(visible)
		{
			text2[j++] = text2[i];
		}
	}
	text2[j] = '\0';
	return text2;
}

char *str_to_UTF8(pool p,unsigned char *in)
{
    int n, i = 0;
    char *result = NULL;
	char *aux = NULL;
	int q = 1;
	size_t numconv;
	size_t inbytesleft, outbytesleft;
	char *inbuf, *outbuf;
    
	if(in==NULL) return NULL;

	aux = (char*)pmalloc(p,strlen(in)+1);
	for(n = 0; n < strlen(in); n++)
	{
		long c = (long)in[n];
		/*strip out color codes*/
		if(c == 27)
		{
			n += 2;
			if(in[n] == 'x') n++;
			if(in[n] == '3') n++;
			n++;
			continue;
		}
		if(c == '\r')
			continue;
		aux[i++] = (char)c;
	}
	aux[i] = '\0';
	
	return it_convert_windows2utf8(p, aux);
}

unsigned char *UTF8_to_str(pool p,unsigned char *in)
{
	return it_convert_utf82windows(p, in);
}

char *at_normalize(char *s)
{
    char *new, *old;

    if(s == NULL)
        return;

    new = old = s;
    while(*old)
    {
        if(*old == ' ')
        {
            old++;
            continue;
        }
        *new++ = tolower(*old++);
    }
    *new = '\0';

    return s;
}

int at_xdb_set(ati ti, char *host, jid owner, xmlnode data, char *ns)
{
    xmlnode x = xmlnode_wrap(data, "aimtrans");
    int ret;
    jid j;
    char *res;

    res = owner->resource;
    jid_set(owner, NULL, JID_RESOURCE);

    log_debug(ZONE,"[AT] Setting XDB for user %s", jid_full(owner));

    j = jid_new(owner->p, spools(owner->p, owner->user, "%", owner->server,
                "@", host, owner->p));
    ret = xdb_set(ti->xc, j, ns, x);

    jid_set(owner, res, JID_RESOURCE);

    return ret;
}

xmlnode at_xdb_get(ati ti, jid owner, char *ns)
{
    xmlnode x;
    jid j;
    char *res;

    log_debug(ZONE, "[AT] Getting XDB for user %s",jid_full(owner));
    
    res = owner->resource;
    jid_set(owner, NULL, JID_RESOURCE);

    j = jid_new(owner->p, spools(owner->p, owner->user, "%", owner->server,
                "@", ti->i->id, owner->p));
    x= xdb_get(ti->xc, j, ns);

    jid_set(owner, res, JID_RESOURCE);

    return xmlnode_get_firstchild(x);
}

/* convert pre-2003 XDB data to new format */
void at_xdb_convert(ati ti, char *user, jid nid)
{
    jid id, old, new;
    xmlnode x;
    pool p;

    if(user == NULL) return;

    p = pool_new();
    id = jid_new(p, user);

    old = jid_new(p, spools(p, shahash(jid_full(jid_user(id))),
                  "@", ti->i->id, p));
    new = jid_new(p, spools(p, nid->user, "%", nid->server,
                  "@", ti->i->id, p));

    x = xdb_get(ti->xc, old, AT_NS_AUTH);
    if(x != NULL)
      if(xdb_set(ti->xc, new, AT_NS_AUTH, x) == 0)
      {
        log_error(ZONE, "[AT] Converted XDB for user %s",jid_full(jid_user(id)));
        xdb_set(ti->xc, old, AT_NS_AUTH, NULL);
      }

    x = xdb_get(ti->xc, old, AT_NS_ROSTER);
    if(x != NULL)
      if(xdb_set(ti->xc, new, AT_NS_ROSTER, x) == 0)
        xdb_set(ti->xc, old, AT_NS_ROSTER, NULL);

    pool_free(p);
}

void at_psend(pth_msgport_t mp, jpacket p)
{
    static pth_msgport_t unknown_mp = NULL;
    jpq q;

    if(p == NULL || mp == NULL)
        return;

    log_debug(ZONE,"psending to %X packet %X",mp,p);

    q = pmalloc(p->p, sizeof(_jpq));
    q->p = p;

    q->head.m_replyport = unknown_mp;
    pth_msgport_put(mp, (pth_message_t *)q);
}
