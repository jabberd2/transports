#include "jabber.h"
#include "config.h"
#include "debug.h"
#include "jid.h"
#include "users.h"

/*
<iq from='%jid_czyjs' type="result" id="ac2fa" to="movax@jabber.mpi.int.pl/Psi">
<vCard xmlns="vcard-temp" prodid="-//HandGen//NONSGML vGen v1.0//EN" version="2.0">
<FN>Edek</FN>
<FN>Irish M. Kochlewski</FN>
<FN>kolargol</FN>
<NICKNAME>kolargol</NICKNAME>
<NICKNAME>bee</NICKNAME>
<NICKNAME>ninchxxx</NICKNAME>
<ADR>
    <HOME/>
    <LOCALITY>Komorow</LOCALITY>
    <REGION>Mazowieckie</REGION>
    <CTRY>Poland</CTRY>
</ADR>
<URL>http://kolargol.com</URL>
<EMAIL></EMAIL>
<BDAY>1985</BDAY>
<BDAY>17 kwiecin 1978</BDAY>
<ORG>
    <ORGNAME>RTK Autocom</ORGNAME>
    <ORGUNIT>Dzia? IT</ORGUNIT>
</ORG>
<DESC>To jest GaduGadu Transport - bramka pomi?dzy Jabberem, a GaduGadu.</DESC>
</vCard>
</iq>
*/
//int jabber_sndvCard_result(const char *to, tlen_pubdir *pubdir) {
int jabber_sndvCard_result(const char *to, struct tlen_pubdir *pubdir) {
	int n;
	tx_search **item, *item0;
	tt_user **user;
	char *to0;

    xode iq, query, temp;

	if (to == NULL)
		return -1;

	my_debug(2, "jabber: Wysylam wyniki vCard do %s", to);

	to0 = jid_jidr2jid(to);
	if ((user = jid_jid2user(to0)) == NULL) {
		my_debug(0, "Nie jestes zarejstrowany");
		return -1;
	}

    //jid_tid2jid(pubdir->
    
	/* iq */
	iq = xode_new("iq");
	xode_put_attrib(iq, "type", "result");
    xode_put_attrib(iq, "from", item0->tid);
	xode_put_attrib(iq, "to", to);
	//xode_put_attrib(iq, "id", id);

	/* vCard */
	query = xode_insert_tag(iq, "vCard");
	xode_put_attrib(query, "xmlns", "vcard-temp");
	xode_put_attrib(query, "prodid", "-//HandGen//NONSGML vGen v1.0//EN");
	xode_put_attrib(query, "version", "2.0");

    temp = xode_insert_tag(query, "FN");
    xode_insert_cdata(temp, pubdir->firstname, -1); 
/*
	temp = xode_insert_tag(query, "instructions");
	xode_insert_cdata(temp, config_search_instructions, -1); 
	xode_insert_tag(query, "active");
	xode_insert_tag(query, "first");
	xode_insert_tag(query, "last");
	xode_insert_tag(query, "nick");
	xode_insert_tag(query, "city");
	xode_insert_tag(query, "gender");
	xode_insert_tag(query, "born");
	xode_insert_tag(query, "username");
*/
    
	t_free(to0);

	my_strcpy(js_buf2, xode_to_str(iq));

	jabber_send(js_buf2);
	my_debug(5, "jabber: Zapytanie: %s", js_buf2);

	t_free(js_buf2);
	xode_free(iq);

	return 0;
}
