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

#include "utils.h"

/* MSN and XHTML don't use the same "endian" for encoded hex colors */
char *mt_xhtml_flip(pool p, char *color)
{
    char *ret;
    int left = 0, right;

    right = strlen(color);
    /* Allocate one extra, as eg "0" becomes "00" */
    ret = pmalloc(p,(right + 2) * sizeof(char));

    while (right > 0)
    {
        right -= 2;
        ret[left++] = right != -1 ? color[right] : '0';
        ret[left++] = color[right + 1];
    }

    ret[left] = '\0';

    return ret;
}

char *mt_xhtml_style(pool p, char *style, char *attr, int len)
{
    char *ret, *ptr;

    ret = strstr(style,attr);
    if (ret != NULL)
    {
        ret += (len + 1);
        while (isspace(*ret)) ret++;

        ptr = strchr(ret,';');
        if (ptr != NULL)
        {
            *ptr = '\0';
            ret = pstrdup(p,ret);
            *ptr = ';';
        }
        else
            ret = NULL;
    }

    return ret;
}

void mt_xhtml_span(xmlnode span, xhtml_msn *xm)
{
    char *style, *font, *color;
    pool p;

    style = xmlnode_get_attrib(span,"style");
    if (style != NULL)
    {
        p = xmlnode_pool(span);
        if (xm->font == NULL)
        {
            font = mt_xhtml_style(p,style,"font-family",11);
            if (font != NULL)
                xm->font = mt_encode(p,font);
        }

        if (xm->color == NULL)
        {
            color = mt_xhtml_style(p,style,"color",5);
            if (j_strlen(color) > 2)
                xm->color = color + 1;
        }
    }
}

void mt_xhtml_tag(xmlnode cur, xhtml_msn *xm)
{
    if (cur->type == NTYPE_TAG)
    {
        char *name = xmlnode_get_name(cur);

        if (strcmp(name,"span") == 0)
            mt_xhtml_span(cur,xm);
        else if (strcmp(name,"strong") == 0)
            xm->b = 1;
        else if (strcmp(name,"em") == 0)
            xm->i = 1;
        else if (strcmp(name,"u") == 0)
            xm->u = 1;
     /* else if (strcmp(name,"br") == 0)
            spool_add(xm->body,"\r\n"); */
    }
    else if (cur->type == NTYPE_CDATA)
        mt_replace_newline(xm->body,xmlnode_get_data(cur));
}

void mt_xhtml_traverse(xmlnode cur, xhtml_msn *xm)
{
    for_each_node(cur,cur)
    {
        mt_xhtml_tag(cur,xm);

        if (xmlnode_has_children(cur))
            mt_xhtml_traverse(cur,xm);
    }
}

char *mt_xhtml_format(xmlnode html)
{
    xhtml_msn xm;
    spool sp;
    pool p = html->p;

    xm.body = spool_new(p);
    xm.i = xm.b = xm.u = 0;
    xm.font = NULL;
    xm.color = NULL;

    mt_xhtml_traverse(html,&xm);

    sp = spool_new(p);
    spooler(sp,"MIME-Version: 1.0\r\nContent-Type: text/plain; charset=UTF-8\r\nX-MMS-IM-Format: FN=",
            xm.font != NULL ? xm.font : "MS%20Sans%20Serif",
            "; EF=",sp);

    if (xm.i) spool_add(sp,"I");
    if (xm.b) spool_add(sp,"B");
    if (xm.u) spool_add(sp,"U");

    spooler(sp,"; CO=",xm.color == NULL ? "0" : mt_xhtml_flip(p,xm.color),
               "; CS=0; PF=0\r\n\r\n",spool_print(xm.body),sp);

    return spool_print(sp);
}

char *mt_xhtml_get_fmt(pool p, char *fmt, char *type)
{
    char *val, *ptr;

    val = strstr(fmt,type);
    if (val != NULL)
    {
        val += 3;
        if ((ptr = strchr(val,';')) == NULL)
            return NULL;

        *ptr = '\0';
        val = pstrdup(p,val);
        *ptr = ';';
    }

    return val;
}

void mt_xhtml_message(xmlnode message, char *fmt, char *msg)
{
    pool p = message->p;
    char *font, *ef, *color;

    font = mt_xhtml_get_fmt(p,fmt,"FN");
    ef = mt_xhtml_get_fmt(p,fmt,"EF");
    color = mt_xhtml_get_fmt(p,fmt,"CO");

    if (font != NULL && ef != NULL && color != NULL)
    {
        xmlnode h, span, x;

        h = xmlnode_insert_tag(message,"html");
        xmlnode_put_attrib(h,"xmlns",NS_XHTML);
        span = xmlnode_insert_tag(xmlnode_insert_tag(h,"body"),"span");

        xmlnode_put_attrib(span,"style",spools(p,"font-family: ",mt_decode(p,font),
                                               "; color: #",mt_xhtml_flip(p,color),
                                               "; margin:0; padding:0; font-size: 10pt",p));

        x = span;
        if (strchr(ef,'B'))
            x = xmlnode_insert_tag(x,"strong");
        if (strchr(ef,'I'))
            x = xmlnode_insert_tag(x,"em");
        if (strchr(ef,'U'))
            x = xmlnode_insert_tag(x,"u");

        xmlnode_insert_cdata(x,msg,-1);
        /* xmlnode_insert_tag(span,"br"); */
    }
}
