/*

    Yahoo! Transport 2 for the Jabber Server
    Copyright (c) 2002, Paul Curtis
    Portions of this program are derived from GAIM (yahoo.c)

    This file is part of the Jabber Yahoo! Transport. (yahoo-transport-2)

    The Yahoo! Transport is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    The Yahoo! Transport is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with the Yahoo! Transport; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

    $Id: yahoo-message.c,v 1.9 2004/07/01 16:06:34 pcurtis Exp $
*/

#include "yahoo-transport.h"


char *str_to_UTF8(pool p,unsigned char *in, const char *charmap, int isUTF8) {
   int n, i = 0;
   char *tmp = NULL, *tmp2 = NULL;
   char *result = NULL;
   size_t inbytes, outbytes;
   iconv_t iconv_obj;

   /* for now, allocate more than enough space... */
   if (in == NULL)
      return NULL;

   tmp = (char*)pmalloco(p, strlen(in) + 1);

   for (n = 0; n < strlen(in); n++) {
      char *font;
      long c = (long)in[n];
      /*check for <font ....> html codes*/
      font = strstr(&in[n],"<font ");
      if (strlen(&in[n]) > 7 && strncasecmp(&in[n],"<font ",6) == 0) {
         font = strchr(font,'>');
         if (font != NULL) {
            n = ((long)font) - ((long)in);
            continue;
         }
      }
      /*strip out color codes*/
      if (c == 27) {
         n += 2;
         if (in[n] == 'x')
            n++;
         if (in[n] == '3')
            n++;
         n++;
         continue;
      }
      if (c == 0x0D || c == 0x0a)
         continue;
      tmp[i++] = (char)c;
   }
   tmp[i] = '\0';
   inbytes = strlen(tmp)+1;
   outbytes = inbytes * 2;
   tmp2 = result = (char*)pmalloco(p, outbytes);

   iconv_obj = iconv_open("UTF-8", charmap);
   if ((iconv_obj == (iconv_t) -1) || (isUTF8)) {
//   log_notice(ZONE, "[YAHOO]: Error - iconv_open returned %d",errno);
     strncpy(result, tmp, outbytes);
   } else {
     iconv(iconv_obj, &tmp, &inbytes, &tmp2, &outbytes);
     iconv_close(iconv_obj);
   }
   return result;
}


void yahoo_send_jabber_message(struct yahoo_data *yd, char *type, char *from, char *msg, int isUTF8) {
   xmlnode m;
   pool p;

   p = pool_new();
   yd->yi->stats->packets_out++;
   msg = str_to_UTF8(p, msg, yd->yi->charmap, isUTF8);
   m = jutil_msgnew(type, jid_full(yd->me), NULL, msg);
   xmlnode_put_attrib(m, "from", spools(p, from, "@", yd->yi->i->id, p));

   yahoo_deliver(NULL,m);
   pool_free(p);
}
