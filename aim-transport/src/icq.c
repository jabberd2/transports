/*
 * Encapsulated ICQ.
 *
 */

#define FAIM_INTERNAL
#include <aim.h>

faim_export int aim_icq_reqofflinemsgs(aim_session_t *sess)
{
	aim_conn_t *conn;
	aim_frame_t *fr;
	aim_snacid_t snacid;
	int bslen;

	if (!sess || !(conn = aim_conn_findbygroup(sess, 0x0015)))
		return -EINVAL;

	bslen = 2 + 4 + 2 + 2;

	if (!(fr = aim_tx_new(sess, conn, AIM_FRAMETYPE_FLAP, 0x02, 10 + 4 + bslen)))
		return -ENOMEM;

	snacid = aim_cachesnac(sess, 0x0015, 0x0002, 0x0000, NULL, 0);
	aim_putsnac(&fr->data, 0x0015, 0x0002, 0x0000, snacid);

	/* For simplicity, don't bother using a tlvlist */
	aimbs_put16(&fr->data, 0x0001);
	aimbs_put16(&fr->data, bslen);

	aimbs_putle16(&fr->data, bslen - 2);
	aimbs_putle32(&fr->data, atoi(sess->sn));
	aimbs_putle16(&fr->data, 0x003c); /* I command thee. */
	aimbs_putle16(&fr->data, snacid); /* eh. */

	aim_tx_enqueue(sess, fr);

	return 0;
}

faim_export int aim_icq_ackofflinemsgs(aim_session_t *sess)
{
	aim_conn_t *conn;
	aim_frame_t *fr;
	aim_snacid_t snacid;
	int bslen;

	if (!sess || !(conn = aim_conn_findbygroup(sess, 0x0015)))
		return -EINVAL;

	bslen = 2 + 4 + 2 + 2;

	if (!(fr = aim_tx_new(sess, conn, AIM_FRAMETYPE_FLAP, 0x02, 10 + 4 + bslen)))
		return -ENOMEM;

	snacid = aim_cachesnac(sess, 0x0015, 0x0002, 0x0000, NULL, 0);
	aim_putsnac(&fr->data, 0x0015, 0x0002, 0x0000, snacid);

	/* For simplicity, don't bother using a tlvlist */
	aimbs_put16(&fr->data, 0x0001);
	aimbs_put16(&fr->data, bslen);

	aimbs_putle16(&fr->data, bslen - 2);
	aimbs_putle32(&fr->data, atoi(sess->sn));
	aimbs_putle16(&fr->data, 0x003e); /* I command thee. */
	aimbs_putle16(&fr->data, snacid); /* eh. */

	aim_tx_enqueue(sess, fr);

	return 0;
}

faim_export int aim_icq_sendxmlreq(aim_session_t *sess, const char *xml)
{
	aim_conn_t *conn;
	aim_frame_t *fr;
	aim_snacid_t snacid;
	int bslen;

	if (!xml || !strlen(xml))
		return -EINVAL;

	if (!sess || !(conn = aim_conn_findbygroup(sess, 0x0015)))
		return -EINVAL;

	bslen = 2 + 10 + 2 + strlen(xml) + 1;

	if (!(fr = aim_tx_new(sess, conn, AIM_FRAMETYPE_FLAP, 0x02, 10 + 4 + bslen)))
		return -ENOMEM;

	snacid = aim_cachesnac(sess, 0x0015, 0x0002, 0x0000, NULL, 0);
	aim_putsnac(&fr->data, 0x0015, 0x0002, 0x0000, snacid);

	/* For simplicity, don't bother using a tlvlist */
	aimbs_put16(&fr->data, 0x0001);
	aimbs_put16(&fr->data, bslen);

	aimbs_putle16(&fr->data, bslen - 2);
	aimbs_putle32(&fr->data, atoi(sess->sn));
	aimbs_putle16(&fr->data, 0x07d0); /* I command thee. */
	aimbs_putle16(&fr->data, snacid); /* eh. */
	aimbs_putle16(&fr->data, 0x0998); /* shrug. */
	aimbs_putle16(&fr->data, strlen(xml) + 1);
	aimbs_putraw(&fr->data, xml, strlen(xml) + 1);

	aim_tx_enqueue(sess, fr);

	return 0;
}

faim_export int aim_icq_sendsms(aim_session_t *sess, const char *dest, const char *body)
{
	aim_conn_t *conn;
	aim_frame_t *fr;
	aim_snacid_t snacid;
	int bslen;
	int a;
	char *xml;
	int xmllen;
	char timestr[30];
	time_t t;
	struct tm *tm;

	if (!body || !strlen(body))
		return -EINVAL;

	if (!sess || !(conn = aim_conn_findbygroup(sess, 0x0015)))
		return -EINVAL;

	time(&t);
	tm = gmtime(&t);
	strftime(timestr, 30, "%a, %d %b %Y %T %Z", tm);
	xmllen = 17 + 13 + strlen(dest) + 14 + 6 + strlen(body) + 7 +
			25 + 13 + strlen(sess->sn) + 14 +
			14 + strlen(sess->sn /* should be nick */) + 15 +
			18 + 2 + 19 + 6 + strlen(timestr) + 7 + 18 + 1;

	bslen = 2 + 4 + 2 + 2 + 2 + 2 + 2 + 16 + 4 + xmllen;

	if (!(fr = aim_tx_new(sess, conn, AIM_FRAMETYPE_FLAP, 0x02, 10 + 4 + bslen)))
		return -ENOMEM;

	xml = (char *) malloc(xmllen);
	snprintf(xml, xmllen, "<icq_sms_message><destination>%s</destination>"\
			"<text>%s</text><codepage>1252</codepage>"\
			"<senders_UIN>%s</senders_UIN>"\
			"<senders_name>%s</senders_name>"\
			"<delivery_receipt>No</delivery_receipt>"\
			"<time>%s</time></icq_sms_message>",
			dest, body, sess->sn, sess->sn /* should be nick */, timestr);

	snacid = aim_cachesnac(sess, 0x0015, 0x0002, 0x0000, NULL, 0);
	aim_putsnac(&fr->data, 0x0015, 0x0002, 0x0000, snacid);

	/* For simplicity, don't bother using a tlvlist */
	aimbs_put16(&fr->data, 0x0001);
	aimbs_put16(&fr->data, bslen);

	aimbs_putle16(&fr->data, bslen - 2);
	aimbs_putle32(&fr->data, atoi(sess->sn));
	aimbs_putle16(&fr->data, 2000); /* I command thee. */
	aimbs_putle16(&fr->data, snacid); /* eh. */
	aimbs_put16(&fr->data, 0x8214); /* SMS send subtype */
	aimbs_put16(&fr->data, 0x0001);
	aimbs_put16(&fr->data, 0x0016);
	for(a = 0; a < 16; a++)
		aimbs_put8(&fr->data, 0x00);
	aimbs_put32(&fr->data, xmllen); /* yes, this has to be 32 bits */
	aimbs_putraw(&fr->data, xml, xmllen);

	aim_tx_enqueue(sess, fr);

	free(xml);

	return 0;
}

faim_export int aim_icq_getsimpleinfo(aim_session_t *sess, const char *uin)
{
	aim_conn_t *conn;
	aim_frame_t *fr;
	aim_snacid_t snacid;
	int bslen;

	if (!uin || uin[0] < '0' || uin[0] > '9')
		return -EINVAL;

	if (!sess || !(conn = aim_conn_findbygroup(sess, 0x0015)))
		return -EINVAL;

	bslen = 2 + 4 + 2 + 2 + 2 + 4;

	if (!(fr = aim_tx_new(sess, conn, AIM_FRAMETYPE_FLAP, 0x02, 10 + 4 + bslen)))
		return -ENOMEM;

	snacid = aim_cachesnac(sess, 0x0015, 0x0002, 0x0000, NULL, 0);
	aim_putsnac(&fr->data, 0x0015, 0x0002, 0x0000, snacid);

	/* For simplicity, don't bother using a tlvlist */
	aimbs_put16(&fr->data, 0x0001);
	aimbs_put16(&fr->data, bslen);

	aimbs_putle16(&fr->data, bslen - 2);
	aimbs_putle32(&fr->data, atoi(sess->sn));
	aimbs_putle16(&fr->data, 0x07d0); /* I command thee. */
	aimbs_putle16(&fr->data, snacid); /* eh. */
	aimbs_putle16(&fr->data, 0x051f); /* shrug. */
	aimbs_putle32(&fr->data, atoi(uin));

	aim_tx_enqueue(sess, fr);

	return 0;
}

/*
 * Response to 15/2, contains an ICQ packet.
 */
static int icqresponse(aim_session_t *sess, aim_module_t *mod, aim_frame_t *rx, aim_modsnac_t *snac, aim_bstream_t *bs)
{
	int ret = 0;
	aim_tlvlist_t *tl;
	aim_tlv_t *datatlv;
	aim_bstream_t qbs;
	fu32_t ouruin;
	fu16_t cmdlen, cmd, reqid;

	if (!(tl = aim_readtlvchain(bs)) || !(datatlv = aim_gettlv(tl, 0x0001, 1))) {
		aim_freetlvchain(&tl);
		faimdprintf(sess, 0, "corrupt ICQ response\n");
		return 0;
	}

	aim_bstream_init(&qbs, datatlv->value, datatlv->length);

	cmdlen = aimbs_getle16(&qbs);
	ouruin = aimbs_getle32(&qbs);
	cmd = aimbs_getle16(&qbs);
	reqid = aimbs_getle16(&qbs);

	faimdprintf(sess, 1, "icq response: %d bytes, %ld, 0x%04x, 0x%04x\n", cmdlen, ouruin, cmd, reqid);

	if (cmd == 0x0041) {
		fu16_t msglen;
		struct aim_icq_offlinemsg msg;
		aim_rxcallback_t userfunc;

		memset(&msg, 0, sizeof(msg));

		msg.sender = aimbs_getle32(&qbs);
		msg.year = aimbs_getle16(&qbs);
		msg.month = aimbs_getle8(&qbs);
		msg.day = aimbs_getle8(&qbs);
		msg.hour = aimbs_getle8(&qbs);
		msg.minute = aimbs_getle8(&qbs);
		msg.type = aimbs_getle16(&qbs);
		msglen = aimbs_getle16(&qbs);
		msg.msg = aimbs_getstr(&qbs, msglen);

		if ((userfunc = aim_callhandler(sess, rx->conn, AIM_CB_FAM_ICQ, AIM_CB_ICQ_OFFLINEMSG)))
			ret = userfunc(sess, rx, &msg);

		free(msg.msg);

	} else if (cmd == 0x0042) {
		aim_rxcallback_t userfunc;

		if ((userfunc = aim_callhandler(sess, rx->conn, AIM_CB_FAM_ICQ, AIM_CB_ICQ_OFFLINEMSGCOMPLETE)))
			ret = userfunc(sess, rx);
	} else if (cmd == 0x07da) {
		fu16_t subtype;

		subtype = aimbs_getle16(&qbs);

		if (subtype == 0x019a) {
			fu16_t tlen;
			struct aim_icq_simpleinfo info;
			aim_rxcallback_t userfunc;

			memset(&info, 0, sizeof(info));

			aimbs_getle8(&qbs); /* no clue */
			aimbs_getle16(&qbs); /* no clue */
			info.uin = aimbs_getle32(&qbs);
			tlen = aimbs_getle16(&qbs);
			info.nick = aimbs_getstr(&qbs, tlen);
			tlen = aimbs_getle16(&qbs);
			info.first = aimbs_getstr(&qbs, tlen);
			tlen = aimbs_getle16(&qbs);
			info.last = aimbs_getstr(&qbs, tlen);
			tlen = aimbs_getle16(&qbs);
			info.email = aimbs_getstr(&qbs, tlen);
			/* no clue what the rest of it is */

			if ((userfunc = aim_callhandler(sess, rx->conn, AIM_CB_FAM_ICQ, AIM_CB_ICQ_SIMPLEINFO)))
				ret = userfunc(sess, rx, &info);

			free(info.nick);
			free(info.first);
			free(info.last);
			free(info.email);
		}
		else if (subtype == 100 || subtype == 150) {
			fu16_t tlen;
			struct aim_icq_smsresponse response;
			aim_rxcallback_t userfunc;

			memset(&response, 0, sizeof(response));

			for(tlen = 0; tlen < 7; tlen++)
				aimbs_getle8(&qbs); /* no clue */

			response.type = subtype;
			tlen = aimbs_getle16(&qbs);
			response.tag = aimbs_getstr(&qbs, tlen);
			tlen = aimbs_getle16(&qbs);
			response.xml = aimbs_getstr(&qbs, tlen);

			if ((userfunc = aim_callhandler(sess, rx->conn, AIM_CB_FAM_ICQ, AIM_CB_ICQ_SMSRESPONSE)))
				ret = userfunc(sess, rx, &response);

			free(response.tag);
			free(response.xml);
		}
		else faimdprintf(sess, 1, "unknown subtype 0x%04x\n", subtype);
	}

	aim_freetlvchain(&tl);

	return ret;
}

static int snachandler(aim_session_t *sess, aim_module_t *mod, aim_frame_t *rx, aim_modsnac_t *snac, aim_bstream_t *bs)
{

	if (snac->subtype == 0x0003)
		return icqresponse(sess, mod, rx, snac, bs);

	return 0;
}

faim_internal int icq_modfirst(aim_session_t *sess, aim_module_t *mod)
{

	mod->family = 0x0015;
	mod->version = 0x0001;
	mod->toolid = 0x0110;
	mod->toolversion = 0x047b;
	mod->flags = 0;
	strncpy(mod->name, "icq", sizeof(mod->name));
	mod->snachandler = snachandler;

	return 0;
}


