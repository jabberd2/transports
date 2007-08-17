/* --------------------------------------------------------------------------
   ICQ pthread jabber transport.

   Modified by Lukasz Karwacki <lukasm@wp-sa.pl>

   -------------------------------------------------------------------------- */

/** @file
 *  Charset conversion routines */

#include "jit/icqtransport.h"

extern iconv_t _ucs2utf;
extern iconv_t _win2utf;
extern iconv_t _utf2win;

char *it_convert_ucs2utf8(pool p, unsigned int len, const char ucs2_str[])
{
  size_t size_in, size_out;
  char *out_utf;
  char *in;
  char *out;
  size_t numconv;
  int q;

  if (!len)
    return NULL;

  size_in = len;
  size_out = 4*size_in+3;

  out_utf = pmalloco(p,size_out);

  in=(char*)ucs2_str;
  out=out_utf;

  q = 1;
  while (q) {
	numconv = iconv(_ucs2utf,&in,&size_in,&out,&size_out);

	if (numconv == (size_t)(-1)) {
	  switch (errno) {
		case EILSEQ:
		case EINVAL:
		  size_in--;
		  size_out--;
		  in++;
		  *out++ = '?';
		  break;
		case E2BIG:
		default:
		  q = 0;
		  break;
	  }
	}
	else {
	  q = 0;
	}
  }
	
  *out = '\0';
  return out_utf;
}


char *it_convert_windows2utf8(pool p, const char *windows_str)
{
  size_t size_in, size_out;
  char *out_utf;
  char *in;
  char *out;
  size_t numconv;
  int q;

  if (!windows_str)
    return NULL;

  size_in = strlen(windows_str);
  size_out = 4*size_in+3;

  out_utf = pmalloco(p,size_out);

  in=(char*)windows_str;
  out=out_utf;

  q = 1;
  while (q) {
	numconv = iconv(_win2utf,&in,&size_in,&out,&size_out);

	if (numconv == (size_t)(-1)) {
	  switch (errno) {
		case EILSEQ:
		case EINVAL:
		  size_in--;
		  size_out--;
		  in++;
		  *out++ = '?';
		  break;
		case E2BIG:
		default:
		  q = 0;
		  break;
	  }
	}
	else {
	  q = 0;
	}
  }
	
  *out = '\0';
  return out_utf;
}


char *it_convert_utf82windows(pool p, const char *utf8_str)
{
  size_t size_in, size_out;
  char *out_win;
  char *in;
  char *out;
  size_t numconv;
  int q;

  if (!utf8_str)
    return NULL;

  size_in = strlen(utf8_str);
  size_out = size_in+2;

  out_win = pmalloco(p,size_out);

  in=(char*)utf8_str;
  out=out_win;

  q = 1;
  while (q) {
	numconv = iconv(_utf2win,&in,&size_in,&out,&size_out);
        
	if (numconv == (size_t)(-1)) {
	  switch (errno) {
		case EILSEQ:
		case EINVAL:
		  size_in--;
		  size_out--;
		  in++;
		  *out++ = '?';
		  while ((*in & 0xC0) == 0x80) { /* Skip supplemental bytes of UTF-8 char */
			size_in--;
			in++;
		  }
		  break;
		case E2BIG:
		default:
		  q = 0;
		  break;
	  }
	}
	else {
	  q = 0;
	}
  }
  *out = '\0';
  return out_win;
}
