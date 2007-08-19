/*
 *  utf8.c - simple UTF8 <-> Unicode conversion functions
 *
 *  Date:   October 20th, 2001
 *
 *  Author: Justin Karneges
 *          infiniti@affinix.com
 *
 *  (Adapted from the Qt library)
 */

#include<string.h>
#include<stdlib.h>
#include<stdio.h>

/*
 *  isAscii -- tells if a UTF-8 string is just ASCII
 *
 *   input:  in = char pointer to a UTF-8 zero-terminated string
 *
 *   return: 1 if ASCII, 0 if not
 */
int isAscii(const unsigned char *in)
{
	int n;
	int len;

	len = strlen(in); 
	for(n = 0; n < len; ++n) {
		if(in[n] & 0x80)
			return 0;
	}
 
	return 1;
}

/*
 *  utf8_to_unicode -- converts a UTF-8 string to Unicode
 *
 *   input:  in = pointer to the input UTF-8 zero-terminated string
 *           out = pointer to where the resulting Unicode string should be saved
 *           maxlen = max number of bytes to save
 *
 *   return: size of Unicode string (in wchars)
 */
int utf8_to_unicode(const unsigned char *in, unsigned char *out, unsigned short maxlen)
{
	int n;
	int len;
	int size;
	int need = 0;
	unsigned char ch;
	unsigned short uc;

	len = strlen(in);
	size = 0;

	for(n = 0; n < len; ++n) {
		ch = in[n];
		if(need) {
			if((ch&0xc0) == 0x80) {
				uc = (uc << 6) | (ch & 0x3f);
				need--;
				if(!need) {
					if(size + 2 > maxlen)
						return size;

					out[size++] = ((uc >> 8) & 0xff);
					out[size++] = (uc & 0xff);
				}
			}
			else {
				need = 0;
			}
		}
		else {
			if(ch < 128) {
				if(size + 2 > maxlen)
					return size;

				out[size++] = 0;
				out[size++] = ch;
			}
			else if( (ch&0xe0) == 0xc0 ) {
				uc = ch & 0x1f;
				need = 1;
			}
			else if( (ch&0xf0) == 0xe0 ) {
				uc = ch & 0x0f;
				need = 2;
			}
		}
	}

	return (size >> 1);
}

/*
 *  unicode_to_utf8 -- converts a Unicode string to UTF-8
 *
 *   input:  in = pointer to the input Unicode string
 *           len = length of Unicode string (in wchars)
 *           out = pointer to where the resulting UTF-8 zero-terminated string should be saved
 *           maxlen = max number of bytes to save
 *
 *   return: none
 */
void unicode_to_utf8(const unsigned char *in, int len, unsigned char *out, int maxlen)
{
	int n, size;
	unsigned short uc;
	unsigned char hi, lo, b;
 
	size = 0;
 
	for(n = 0; n < len; ++n) {
		lo = in[n*2+1];
		hi = in[n*2];
		uc = ((unsigned short)hi << 8) + lo;
		if(!hi && lo < 0x80) {
			if(size + 1 >= maxlen)
				break;
			out[size++] = lo;
		}
		else {
			b = (hi << 2) | (lo >> 6);
			if(hi < 0x08) {
				if(size + 1 >= maxlen)
					break;
				out[size++] = 0xc0 | b;
			}
			else {
				if(size + 2 >= maxlen)
					break;
				out[size++] = 0xe0 | (hi >> 4);
				out[size++] = 0x80 | (b & 0x3f);
			}
			if(size + 1 >= maxlen)
				break;
			out[size++] = 0x80 | (lo & 0x3f);
		}
	}
 
	out[size] = 0;
}

