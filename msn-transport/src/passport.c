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
 * Copyright (c) 2003 James Bunton <james@delx.cjb.net>
 *
 * Acknowledgements
 *
 * Special thanks to the Jabber Open Source Contributors for their
 * suggestions and support of Jabber.
 * Thanks to http://www.hypothetic.org/docs/msn/ for the information to get this working
 *
 * -------------------------------------------------------------------------- */

#include "sb.h"
#include <curl/curl.h>
#include <curl/types.h>


struct MemoryStruct {
	char *memory;
	size_t size;
};

int mt_findkey(char *str, char *key, char *value, int valuelength, int flag) {
	char *val = 0;
	char lower = '0';
	char upper = '9';
	int i = 0;
	int skip = strlen(key);

	// If flag is 0, then normal
	// For all other values of flag, it will be the end char that we look for, and there will be no check for numeracy

	if(flag != 0) {
		lower = 0;
		upper = 127;
	}

	val = strstr(str, key);
	if(val == 0) return -1;

	if(skip >= valuelength) return -1;
	strncpy(value, val, skip);

	for(i = skip; val[i] >= lower && val[i] <= upper; i++) {
		if(i >= valuelength)
			return -1;

		value[i] = val[i];

		if(value[i] == flag) {
			value[i + 1] = 0;
			return 0;
		}
	}
	return -2; /* We only ever get here if a character is out of ASCII range*/
}

size_t WriteMemoryCallback(void *ptr, size_t size, size_t nmemb, void *data)
{
	register int realsize = size * nmemb;
	struct MemoryStruct *mem = (struct MemoryStruct *)data;

//	mt_free(mem->memory);
//	mem->memory = mt_malloc(mem->size + realsize + 1);
	mem->memory = (char *)realloc(mem->memory, mem->size + realsize + 1);
	if (mem->memory) {
		memcpy(&(mem->memory[mem->size]), ptr, realsize);
		mem->size += realsize;
		mem->memory[mem->size] = 0;
	}
	return realsize;
}


// Curl handles, speeds things up if we don't need to recreate them each time
CURL *curl;
CURLcode res;
struct MemoryStruct chunk;
char errorbuffer[CURL_ERROR_SIZE+1];


char *mt_nexus(session s)
{
	static char *passportlogin = 0;

	if(passportlogin != 0)
		return passportlogin;

	chunk.memory = 0;
	chunk.size = 0;

	// Connect to the nexus server and get the login location
	curl = curl_easy_init();
	if(!curl) {
		log_debug(ZONE,"Session[%s], Curl init failed, bailing out",jid_full(s->id));
		return 0;
	}

	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteMemoryCallback);
	if(s->ti->proxyhost) {
		curl_easy_setopt(curl, CURLOPT_PROXY, s->ti->proxyhost);
		if(s->ti->proxypass) {
			curl_easy_setopt(curl, CURLOPT_PROXYUSERPWD, s->ti->proxypass);
		}
		if(s->ti->socksproxy) {
			curl_easy_setopt(curl, CURLOPT_PROXYTYPE, CURLPROXY_SOCKS5);
		}
	}
	curl_easy_setopt(curl, CURLOPT_FILE, (void *)&chunk);
	curl_easy_setopt(curl, CURLOPT_HEADER, TRUE);
	curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1);
	curl_easy_setopt(curl, CURLOPT_UNRESTRICTED_AUTH, 1);
	curl_easy_setopt(curl, CURLOPT_URL, "https://nexus.passport.com/rdr/pprdr.asp");
	curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, errorbuffer);
	if (s->ti->is_insecure == 1) {
		curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0);
	}
	log_debug(ZONE,"Session[%s], Retrieving data from nexus\nIf this is the last message you see, you have a problem with Curl",jid_full(s->id));
	res = curl_easy_perform(curl);
	log_debug(ZONE,"Session[%s], Finished Curl call",jid_full(s->id));
	if(res != CURLE_OK) {
		log_warn(ZONE,"CURL result=%d, CURL error message=%s",res,errorbuffer);
	}

	if(chunk.memory == 0 || strcmp(chunk.memory, "") == 0) {
		log_debug(ZONE,"Session[%s], No data for Nexus, bailing out",jid_full(s->id));
		return 0;
	}
	log_debug(ZONE,"----Start Nexus-----\nRetrieved data nexus for session: %s\n%s\n-----End Nexus----",jid_full(s->id),chunk.memory);

	passportlogin = mt_malloc(100);
	if(mt_findkey(chunk.memory, "DALogin=", passportlogin, 100, ',') != 0) {
		mt_free(passportlogin);
		passportlogin = 0;
		return 0;
	}
	strncpy(passportlogin, "https://", 8); // DALogin= is overwritten exactly by this.. =)
	passportlogin[strlen(passportlogin) - 1] = 0; // Chops off the , on the end

	// Free the memory again...
	mt_free(chunk.memory);
	chunk.memory = 0;
	chunk.size = 0;

	return passportlogin;
}


void mt_ssl_auth(session s, char *authdata, char *tp)
{
	struct curl_slist *headerlist = 0;
	spool authorisation = spool_new(s->p);
	char *authorisation_str = 0;
	char *passportlogin;
	int i = 0;

	chunk.memory = 0;
	chunk.size = 0;

	lowercase(s->user);

	strcpy(tp, "");

	// Get the passport login address, also ensuring that our handle is setup
	passportlogin = mt_nexus(s);
	if(passportlogin == 0)
		return;

	// Now we authenticate with the login server we were given

	spool_add(authorisation, "Authorization: Passport1.4 OrgVerb=GET,OrgURL=http%3A%2F%2Fmessenger%2Emsn%2Ecom,sign-in=");
	spool_add(authorisation, mt_encode(s->p, s->user));
	spool_add(authorisation, ",pwd=");
	spool_add(authorisation, mt_encode(s->p, s->pass));
	spool_add(authorisation, ",");
	spool_add(authorisation, authdata);
	spool_add(authorisation, "\r\n");
	authorisation_str = spool_print(authorisation);
/*	strcpy(authorisation, "Authorization: Passport1.4 OrgVerb=GET,OrgURL=http%3A%2F%2Fmessenger%2Emsn%2Ecom,sign-in=");
	strcat(authorisation, mt_encode(s->p,s->user));
	strcat(authorisation, ",pwd=");
	strcat(authorisation, mt_encode(s->p,s->pass));
	strcat(authorisation, ",");
	strcat(authorisation, authdata);
	strcat(authorisation, "\r\n"); */
	headerlist = curl_slist_append(headerlist, authorisation_str);

	curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headerlist);

	curl_easy_setopt(curl, CURLOPT_URL, passportlogin);

	log_debug(ZONE,"Session[%s], Retrieving data for passport login\nIf this is the last message you see, you have a problem with Curl",jid_full(s->id));
	res = curl_easy_perform(curl);
	if(res != CURLE_OK) {
		log_warn(ZONE,"CURL result=%d, CURL error message=%s",res,errorbuffer);
	}

	if(chunk.memory == 0 || strcmp(chunk.memory, "") == 0) {
		log_debug(ZONE,"Session[%s], No data for second SSL Auth, bailing out",jid_full(s->id));
		return;
	}

	log_debug(ZONE,"----Second SSL Auth\nRetrieved data from: %s\nWith authorisation: %s\nFor session: %s\n%s\nSecond SSL Auth----",passportlogin,authorisation_str,jid_full(s->id),chunk.memory);

	// Now we need to search for the ticket
	if(mt_findkey(chunk.memory, "PP='t=", tp, 500, '\'') != 0) {
		strcpy(tp, "");
		return;
	}
	for(i = 0; i < strlen(tp) - 5; i++) /* Shift everything down 4 spaces to remove the PP=' */
		tp[i] = tp[i + 4];
	tp[i + 1] = 0;

	curl_slist_free_all(headerlist);
}


