#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <ctype.h>

#include "encoding.h"
#include "debug.h"

/*
#include <iconv.h>
iconv_t konw, konw2;
*/

#include <errno.h>
#include <assert.h>
#include "encoding.h"
#include "enc_iso2uni.h"
#include "enc_uni2iso.h"

/* Fragmenty kodu zaporzyczone z gg tranportu */
/*
 *  (C) Copyright 2002 Jacek Konieczny <jajcus@pld.org.pl>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License Version 2 as
 *  published by the Free Software Foundation.
 */


static unsigned char *buf;
static int buf_len;

int enc_init(){
	/*
	buf_len = 0;
	buf = malloc(buf_len*sizeof(char));
	*/
	buf = NULL;
	return 0;
}

int encoding_done(){
	t_free(buf);
	return 0;
}

char *to_utf8(const char *str){
int o=0;
int i;
unsigned char c;
unsigned u;

	if (str == NULL)
		return NULL;

	/* This should always be enough */
/*	if (buf_len < (3*strlen(str) + 1)){
		buf_len = 3*strlen(str) + 1;
		buf = (char *)realloc(buf, buf_len);
		assert(buf != NULL);
	}
*/

	buf_len = strlen(str)*3 + 1;
	buf = (char *)malloc(buf_len*sizeof(char));

	for(i=0;str[i];i++){
		c=(unsigned char)str[i];
		if (c=='\r'){
			continue;
		}
		if (c=='\t' || c=='\n'){
			buf[o++]=c;
			continue;
		}
		if (c<32){
			buf[o++]='\xef';
			buf[o++]='\xbf';
			buf[o++]='\xbd';
			continue;
		}
		if (c<128){
			buf[o++]=c;
			continue;
		}
		u=iso88592_to_unicode[c-128];
		if (u==0||u>0x10000){ /* we don't need character > U+0x10000 */
			buf[o++]='\xef';
			buf[o++]='\xbf';
			buf[o++]='\xbd';
		}
		else if (u<0x800){
			buf[o++]=0xc0|(u>>6);
			buf[o++]=0x80|(u&0x3f);
		}
		else{
			buf[o++]=0xe0|(u>>12);
			buf[o++]=0x80|((u>>6)&0x3f);
			buf[o++]=0x80|(u&0x3f);
		}
	}
	buf[o]=0;
	return (char *)buf;
}

char *from_utf8(const char *str){
unsigned char b;
unsigned u;
int o=0;
int i;

	if (str == NULL)
		return NULL;

	/* This should always be enough */
/*	if (buf_len < (strlen(str) + 1)){
		buf_len = 2*strlen(str) + 1;
		buf = (char *)realloc(buf, buf_len);
		assert(buf != NULL);
	}
*/
	buf_len = 2*strlen(str) + 1;
	buf = (char *)malloc(buf_len*sizeof(char));

	for(i=0; str[i]; i++){
		b=(unsigned char)str[i];
		if (b=='\n'){
			buf[o++]='\r';
			buf[o++]='\n';
			continue;
		}
		if ((b&0x80)==0){ /* ASCII */
			buf[o++]=b;
			continue;
		}
		if ((b&0xc0)==0x80){ /* middle of UTF-8 char */
			continue;
		}
		if ((b&0xe0)==0xc0){
			u=b&0x1f;
			i++;
			b=(unsigned char)str[i];
			if (b==0){
				buf[o++]='?';
				break;
			}
			if ((b&0xc0)!=0x80){
				buf[o++]='?';
				continue;
			}
			u=(u<<6)|(b&0x3f);
		}
		else if ((b&0xf0)==0xe0){
			u=b&0x0f;
			b=(unsigned char)str[++i];
			if (b==0){
				buf[o++]='?';
				break;
			}
			if ((b&0xc0)!=0x80){
				buf[o++]='?';
				continue;
			}
			u=(u<<6)|(b&0x3f);
			b=(unsigned char)str[++i];
			if (b==0){
				buf[o++]='?';
				break;
			}
			if ((b&0xc0)!=0x80){
				buf[o++]='?';
				continue;
			}
			u=(u<<6)|(b&0x3f);
		}
		else{
			buf[o++]='?';
			continue;
		}
		if (u<0x00a0)
			buf[o++]='?';
		else if (u<0x0180)
			buf[o++]=unicode_to_iso88592_a0_17f[u-0x00a0];
		else if (u==0x02c7)
			buf[o++]=0xb7; /* a1 w cp1250, a5 ??? w iso?, jednak 0xb7 */
		else if (u<0x02d8)
			buf[o++]='?';
		else if (u<0x02de)
			buf[o++]=unicode_to_iso88592_2d8_2dd[u-0x02d8];
		else if (u<0x2013)
			buf[o++]='?';
		else if (u<0x203b)
			buf[o++]=unicode_to_iso88592_2013_203a[u-0x2013];
		else if (u==0x20ac)
			buf[o++]=0x80;
		else if (u==0x2122)
			buf[o++]=0x99;
		else
			buf[o++]='?';
	}
	buf[o]=0;
	return buf;
}


/* funkcje wziete ze zrodel libtlen */

/*
 * urlencode()
 *
 * Koduje tekst przy pomocy urlencode
 *
 * - what - tekst który chcemy zakodowaæ
 *
 * Dla zwracanego tekstu alokowana jest pamiêæ, trzeba go
 * zwolniæ przy u¿yciu free()
 *
 */

char *urlencode (const char *what)
{
	if (what)
	{
		const unsigned char *s = what;
		unsigned char *ptr, *str;

		/* przydalo by sie dokladniej allkowac pamiec, a nie na zapas */
		str = ptr = (unsigned char *) calloc (3 * strlen (s) + 1,1);
		if (!str)
			return 0;
		while (*s)
		{
			if (*s == ' ')
				*ptr++ = '+';
			else if ((*s < '0' && *s != '-' && *s != '.')
				 || (*s < 'A' && *s > '9') || (*s > 'Z'
							       && *s < 'a'
							       && *s != '_')
				 || (*s > 'z'))
			{
				sprintf (ptr, "%%%02X", *s);
				ptr += 3;
			}
			else
				*ptr++ = *s;
			s++;
		}
		return str;
	}
	return 0;
}

/*
 * urldecode()
 *
 * Dekoduje tekst przy pomocy urldecode
 *
 * - what - tekst który chcemy rozkodowaæ
 *
 * Dla zwracanego tekstu alokowana jest pamiêæ, trzeba go
 * zwolniæ przy u¿yciu free()
 *
 */

char *urldecode(const char *what)
{
	if (what)
	{
		unsigned char *dest, *data, *retval;
		dest = data = retval = strdup (what);
		if (!data)
			return 0;

		while (*data)
		{
			if (*data == '+')
				*dest++ = ' ';
			else if ((*data == '%') && isxdigit ((int) data[1])
				 && isxdigit ((int) data[2]))
			{
				int code;
				sscanf (data + 1, "%2x", &code);
				if (code != '\r')
					*dest++ = (unsigned char) code;
				data += 2;
			}
			else
				*dest++ = *data;
			data++;
		}
		*dest = '\0';
		return retval;
	}
	return 0;
}

typedef struct enc_t {
    char c;
    char *l;
} enc;

enc a[] = {
    {'>', "&gt;"},
    {'<', "&lt;"},
    {'&', "&amp;"},
#if 0
    {177 /* ± */, "a"},
    {234 /* ê */, "e"},
    {192 /* ¿ */, "z"},
    {188 /* ¼ */, "z"},
    {243 /* ó */, "o"},
    {179 /* ³ */, "l"},
    {241 /* ñ */, "n"},
    {230 /* æ */, "c"},
    {161 /* ¡ */, "A"},
    {202 /* Ê */, "E"},
    {175 /* ¯ */, "Z"},
    {172 /* ¬ */, "Z"},
    {211 /* Ó */, "O"},
    {163 /* £ */, "L"},
    {209 /* Ñ */, "N"},
    {198 /* Æ */, "C"},
#endif
    {0, NULL}
};

/* Moje funkcje */
char *urlencode2 (const char *what) {
	int i;
	const unsigned char *s = what;
	unsigned char *ptr, *str;

	if (what == NULL) return NULL;

	/* przydalo by sie dokladniej allkowac pamiec, a nie na zapas */
	if ((str = ptr = (unsigned char *) calloc (3 * strlen (s) + 1, 1)) == NULL) {
		return 0;
	}

	while (*s) {
		int kodowany = 0;
		for (i = 0; a[i].c; i++) {
			if (a[i].c == *s) {
				sprintf (ptr, "%s", a[i].l);
				ptr += strlen(a[i].l);
				kodowany = 1;
			}
		}
		if (!kodowany)
			*ptr++ = *s;
		s++;
	}
	return str;
}

char *unicode_utf8_2_iso88592(const char *src) {
	return from_utf8(src);
}

char *unicode_iso88592_2_utf8(const char *src) {
	return to_utf8(src);
}

char *unicode_utf8d_2_iso88592e(const char *src) {
	return unicode_utf8_2_iso88592(src);
}

char *unicode_iso88592e_2_utf8d(const char *src) {
	char *temp = NULL, *temp2 = NULL;
	temp = urlencode2(src);
	temp2 = unicode_iso88592_2_utf8(temp);
	t_free(temp);
	return temp2;
}

int encoding_init() {
	char *temp, *temp2, *src;

	enc_init();

	src =  "[]:#/';\\\"}{|=+-_~!`$%^*()<>&@ ZA¯Ó£Æ GÊ¦L¡ JA¬Ñ za¿ó³æ gê¶l± ja¼ñ";
	my_debug(4, "encoding: Test konwersji...");
	my_debug(5, "encoding: A: %s", src);
	temp2 = unicode_iso88592e_2_utf8d(src);
	my_debug(5, "encoding: A->B: %s", temp2);
	temp = unicode_utf8d_2_iso88592e(temp2);
	my_debug(5, "encoding: B->A': %s", temp);
	if (strcmp(src, temp2) != 0) {
		my_debug(0, "encoding: Blad inicializacji kodowan");
		/* exit(1); */
	}
	return 0;
}

/*	if (konw == (iconv_t) -1) {
		if ((konw = iconv_open("ISO-8859-2", "UTF-8")) == (iconv_t) -1) {
			my_debug(0, "Nie moge przekonwertowac");
			return -1;
		}
	}
	if (konw2 == (iconv_t) -1) {
		if ((konw2 = iconv_open("UTF-8", "ISO-8859-2")) == (iconv_t) -1) {
			my_debug(0, "Nie moge przekonwertowac");
			return -1;
		}
	}
*/

/*	char *temp;
	size_t n, l;

	my_debug(0, "Przekonwertowywanie %s", src);

	if (src == NULL)
		return NULL;

	l = strlen(src);
	n = 2*l+1;
	temp = malloc(n);
	iconv(konw2, &src, &l, &temp, &n);

	my_debug(0, "Przekonwertowane %s", temp);

	return temp;
*/
