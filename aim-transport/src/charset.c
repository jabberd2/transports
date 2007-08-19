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
 * Copyright (c) 1999-2000 Schuyler Heath <sheath@jabber.org>
 *
 * Acknowledgements
 *
 * Special thanks to the Jabber Open Source Contributors for their
 * suggestions and support of Jabber.
 *
 * --------------------------------------------------------------------------*/

#include "aimtrans.h"

/* Constants for character set conversion. */
#define BYTEMASK_3   0xFFFFF800UL   /* mask to test for 3-byte UTF-8 representation required */
#define BYTEMASK_2   0xFFFFFF80UL   /* mask to test for 2-byte UTF-8 representation required */

#define BYTEU8_LEN6  0xFC           /* length prefix for 6-byte UTF-8 sequence */
#define BYTEU8_LEN5  0xF8           /* length prefix for 5-byte UTF-8 sequence */
#define BYTEU8_LEN4  0xF0           /* length prefix for 4-byte UTF-8 sequence */
#define BYTEU8_LEN3  0xE0           /* length prefix for 3-byte UTF-8 sequence */
#define BYTEU8_LEN2  0xC0           /* length prefix for 2-byte UTF-8 sequence */
#define BYTEU8_PFIX  0x80           /* prefix for supplemental bytes of a multibyte sequence */

#define BYTETEST_1   0x80           /* test mask for 1-byte UTF-8 sequence */
#define BYTETEST_PF  0xC0           /* test mask for second/subsequent bytes of multibyte */
#define BYTETEST_2   0xE0           /* test mask for 2-byte UTF-8 sequence */
#define BYTETEST_3   0xF0           /* test mask for 3-byte UTF-8 sequence */
#define BYTETEST_4   0xF8           /* test mask for 4-byte UTF-8 sequence */
#define BYTETEST_5   0xFC           /* test mask for 5-byte UTF-8 sequence */
#define BYTETEST_6   0xFE           /* test mask for 6-byte UTF-8 sequence */

#define LOBITS_1     0x7F           /* low-bit mask for 1-byte UTF-8 sequence */
#define LOBITS_PFX   0x3F           /* low-bit mask for supp. bytes of multibyte sequence */
#define LOBITS_2     0x1F           /* low-bit mask for 2-byte UTF-8 sequence */
#define LOBITS_3     0x0F           /* low-bit mask for 3-byte UTF-8 sequence */
#define LOBITS_4     0x07           /* low-bit mask for 4-byte UTF-8 sequence */
#define LOBITS_5     0x03           /* low-bit mask for 5-byte UTF-8 sequence */
#define LOBITS_6     0x01           /* low-bit mask for 6-byte UTF-8 sequence */

#define SAFE2_MASK   0xFE           /* test for safe encoding of 2-byte sequences (mask) */
#define SAFE2_TEST   0xC0           /* test for safe encoding of 2-byte sequences (value) */
#define SAFE3A_TEST  0xE0           /* test for safe encoding of 3-byte sequences (value 1) */
#define SAFE3B_MASK  0xE0           /* test for safe encoding of 3-byte sequences (mask 2) */
#define SAFE3B_TEST  0x80           /* test for safe encoding of 3-byte sequences (value 2) */
#define SAFE4A_TEST  0xF0           /* test for safe encoding of 4-byte sequences (value 1) */
#define SAFE4B_MASK  0xF0           /* test for safe encoding of 4-byte sequences (mask 2) */
#define SAFE4B_TEST  0x80           /* test for safe encoding of 4-byte sequences (value 2) */
#define SAFE5A_TEST  0xF8           /* test for safe encoding of 5-byte sequences (value 1) */
#define SAFE5B_MASK  0xF8           /* test for safe encoding of 5-byte sequences (mask 2) */
#define SAFE5B_TEST  0x80           /* test for safe encoding of 5-byte sequences (value 2) */
#define SAFE6A_TEST  0xFC           /* test for safe encoding of 6-byte sequences (value 1) */
#define SAFE6B_MASK  0xFC           /* test for safe encoding of 6-byte sequences (mask 2) */
#define SAFE6B_TEST  0x80           /* test for safe encoding of 6-byte sequences (value 2) */

#define BITOFS       6              /* bit offset for fragments of UTF-8 data */
#define BITSOFS(x)   ((x) * BITOFS) /* multiples of the bit offset */

#define UTF8SUB      '?'            /* UCS-4 substitute character */
#define WINSUB       '?'            /* Windows substitute character */
#define NULCHR       '\0'           /* null character for termination */

/* Conversion descriptors */
iconv_t fromutf8, toutf8;

/*  Converts a string encoded in the eight-bit character set 
	to the UTF-8 encoding of UCS-4.  Characters that are
	"undefined" in Windows-1252 are replaced with question-mark character. */
char *it_convert_windows2utf8(pool p, const char *windows_str)
{
    int n, i = 0;
    char *result = NULL;
    int q;
    size_t numconv;
    size_t inbytesleft, outbytesleft;
    char *inbuf, *outbuf;

    log_debug( ZONE, "it_convert_windows2utf8");
    if(windows_str==NULL) return NULL;

    /* for now, allocate more than enough space... */
    result = (char*)pmalloc(p,strlen(windows_str)*4+1);

    inbuf = (char *)windows_str;
    outbuf = result;
    inbytesleft = strlen(windows_str);
    outbytesleft = strlen(windows_str)*4;

    q = 1;
    while(q)
    {
        numconv = iconv(toutf8, &inbuf, &inbytesleft, &outbuf, &outbytesleft);
        if(numconv == (size_t)(-1))
        {
            switch(errno)
            {
                case EILSEQ:
                case EINVAL:
                    inbytesleft--;
                    outbytesleft--;
                    inbuf++;
                    *outbuf++ = UTF8SUB;
                    break;
                case E2BIG:
                default:
                    q = 0;
                    break;
            }
        }
        else
        {
            q = 0;
        }
    }
    *outbuf = NULCHR;
    return result;

}

/*  Converts a string encoded in the UTF-8 encoding of UCS-4 to the eight-bit
	encoding.  Characters that cannot be
	converted to this encoding are replaced with the question-mark
	character. */
char *it_convert_utf82windows(pool p, const char *utf8_str)
{
    int q = 1;
    size_t numconv;
    size_t inbytesleft, outbytesleft;
    unsigned char *result = NULL;
    char *inbuf, *outbuf;

    log_debug( ZONE, "it_convert_utf82windows");
    if(utf8_str==NULL) return NULL;

    result=(unsigned char*)pmalloc(p,strlen(utf8_str)+1);
    inbuf = (char *)utf8_str;
    outbuf = result;
    inbytesleft = outbytesleft = strlen(utf8_str);

    while(q)
    {
        numconv = iconv(fromutf8, &inbuf, &inbytesleft, &outbuf, &outbytesleft);
        if(numconv == (size_t)(-1))
        {
            switch(errno)
            {
                case EILSEQ:
                case EINVAL:
                    inbytesleft--;
                    outbytesleft--;
                    inbuf++;
                    *outbuf++ = WINSUB;
                    while((*inbuf & BYTETEST_PF) == BYTEU8_PFIX)
                    {
                        inbytesleft--;
                        inbuf++;
                    }
                    break;
                case E2BIG:
                default:
                    q = 0;
                    break;
            }
        }
        else
        {
            q = 0;
        }
    }
    *outbuf = NULCHR;
    return result;

}
