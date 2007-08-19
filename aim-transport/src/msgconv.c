/*
 *  msgconv.c - convert AIM HTML messages to<->from XHTML and Plaintext
 *
 *  Date:   December 1st, 2001
 *
 *  Author: Justin Karneges
 *          infiniti@affinix.com
 */

#include<string.h>
#include<stdlib.h>
#include<stdio.h>
#include<ctype.h>

/*
 *  msgconv_aim2plain -- converts an AIM HTML message into plain text
 *
 *   input:  in     = pointer to a UTF-8 zero-terminated string
 *           out    = pointer to where the resulting UTF-8 zero-terminated
 *                    string should be saved
 *           maxlen = maximum number of bytes to save
 */
void msgconv_aim2plain(const unsigned char *in, unsigned char *out, int maxlen)
{
	int i, j;
	int len, x;
	const char *p1, *p2;

	len = strlen(in);
	--maxlen; // don't count the zero

	for(i = j = 0; i < len && j < maxlen; ++i) {
		if(in[i] == '<') {
			// br tag?
			if(!strncasecmp(in + i, "<br>", 4)) {
				out[j++] = '\n';
				i += 4-1;
				continue;
			}
			if(!strncasecmp(in + i, "<br/>", 5)) {
				out[j++] = '\n';
				i += 5-1;
				continue;
			}

			// look for ending '>'
			p1 = in + i;
			p2 = strchr(p1, '>');
			if(!p2)
				break;
			x = p2-p1;

			// skip over it
			i += x;
		}
		else if(in[i] == '&') {
			// find semicolon
			p1 = in + i;
			p2 = strchr(p1, ';');
			if(!p2)
				break;
			x = p2-p1;

			++p1;
			if(!strncmp(p1, "lt;", 3)) {
				out[j++] = '<';
				i += x;
				continue;
			}
			if(!strncmp(p1, "gt;", 3)) {
				out[j++] = '>';
				i += x;
				continue;
			}
			if(!strncmp(p1, "amp;", 4)) {
				out[j++] = '&';
				i += x;
				continue;
			}
			if(!strncmp(p1, "quot;", 5)) {
				out[j++] = '"';
				i += x;
				continue;
			}
			if(!strncmp(p1, "nbsp;", 5)) {
				out[j++] = ' ';
				i += x;
				continue;
			}
		}
		else if(isspace(in[i])) {
			// go until you find no space
			for(; i < len && isspace(in[i]); ++i);

			// only apply the space if the previous char wasn't a space
			if(j > 0 && !isspace(out[j-1]))
				out[j++] = ' ';

			--i; // loop will undo this
		}
		else {
			out[j++] = in[i];
		}
	}

	out[j] = 0;
}

/*
 *  msgconv_plain2aim -- converts a plain text message into AIM HTML
 *
 *   input:  in     = pointer to a UTF-8 zero-terminated string
 *           out    = pointer to where the resulting UTF-8 zero-terminated
 *                    string should be saved
 *           maxlen = maximum number of bytes to save
 */
void msgconv_plain2aim(const unsigned char *in, unsigned char *out, int maxlen)
{
	int i, j;
	int len;

	len = strlen(in);
	--maxlen; // don't count the zero

	for(i = j = 0; i < len && j < maxlen; ++i) {
		if(in[i] == '\n') {
			if(j + 4 >= maxlen)
				break;
			strncpy(out + j, "<br>", 4);
			j += 4;
		}
		else if(in[i] == '<') {
			if(j + 4 >= maxlen)
				break;
			strncpy(out + j, "&lt;", 4);
			j += 4;
		}
		else if(in[i] == '>') {
			if(j + 4 >= maxlen)
				break;
			strncpy(out + j, "&gt;", 4);
			j += 4;
		}
		else if(in[i] == '&') {
			if(j + 5 >= maxlen)
				break;
			strncpy(out + j, "&amp;", 5);
			j += 5;
		}
		else if(in[i] == '"') {
			if(j + 6 >= maxlen)
				break;
			strncpy(out + j, "&quot;", 6);
			j += 6;
		}
		else if(in[i] == ' ') {
			if(i > 0 && in[i-1] == ' ') {
				if(j + 6 >= maxlen)
					break;
				strncpy(out + j, "&nbsp;", 6);
				j += 6;
			}
			else {
				out[j++] = in[i];
			}
		}
		else {
			out[j++] = in[i];
		}
	}

	out[j] = 0;
}

/*
 *  msgconv_aim2xhtml -- converts an AIM HTML message into XHTML
 *
 *   input:  in     = pointer to a UTF-8 zero-terminated string
 *           out    = pointer to where the resulting UTF-8 zero-terminated
 *                    string should be saved
 *           maxlen = maximum number of bytes to save
 */
void msgconv_aim2xhtml(const unsigned char *in, unsigned char *out, int maxlen)
{
	int i, j;
	int len, x;
	const char *p1, *p2;

	len = strlen(in);
	--maxlen; // don't count the zero

	// convert all tag data to lowercase
	for(i = j = 0; i < len && j < maxlen; ++i) {
		if(in[i] == '<') {
			// look for ending '>'
			p1 = in + i;
			p2 = strchr(p1, '>');
			if(!p2)
				break;
			x = p2-p1;

			for(; i < i + x && i < len && j < maxlen; ++i)
				out[j++] = tolower(in[i]);
		}
		else {
			out[j++] = in[i];
		}
	}

	out[j] = 0;
}

/*
 *  msgconv_xhtml2aim -- converts an XHTML message into AIM HTML
 *
 *   input:  in     = pointer to a UTF-8 zero-terminated string
 *           out    = pointer to where the resulting UTF-8 zero-terminated
 *                    string should be saved
 *           maxlen = maximum number of bytes to save
 */
void msgconv_xhtml2aim(const unsigned char *in, unsigned char *out, int maxlen)
{
	// no conversion necessary, xhtml is directly compatible to html
	int len;

	len = strlen(in);
	--maxlen; // don't count the zero

	if(len > maxlen)
		len = maxlen;

	strncpy(out, in, len);
	out[len] = 0;
}

