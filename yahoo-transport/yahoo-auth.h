/*
 * yahoo_auth.h: Header for Yahoo Messenger authentication schemes.  Eew.
 *
 * Copyright(c) 2003 Cerulean Studios
 */

/*
    $Id: yahoo-auth.h,v 1.2 2004/06/25 18:45:09 pcurtis Exp $
*/


#ifndef _YAHOO_AUTH_H_
#define _YAHOO_AUTH_H_ 

unsigned int yahoo_auth_finalCountdown(unsigned int challenge, int divisor, int inner_loop, int outer_loop);


#endif /* _YAHOO_AUTH_H_ */
