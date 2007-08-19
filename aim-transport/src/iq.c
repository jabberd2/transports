/*
 * Jabber AIM-Transport libfaim interface
 *
*/

#include "aimtrans.h"

int at_register_iqns(ati ti, const char *ns, iqcb cb)
{
    iqcb cur;

    log_debug(ZONE, "Registering callback for %s", ns);
    cur = xhash_get(ti->iq__callbacks, ns);
    if(cur)
    {
        xhash_zap(ti->iq__callbacks, ns);
    }
    xhash_put(ti->iq__callbacks, ns, cb);

    return;
}

int at_run_iqcb(ati ti, const char *ns, jpacket jp)
{
    iqcb cb;

    log_debug(ZONE, "Running callback for %s", ns);
    cb = xhash_get(ti->iq__callbacks, ns);
    if(cb == NULL)
        return -1;
    
    return cb(ti, jp);
}

void at_init_iqcbs(ati ti)
{
    /* Load up all the iq callbacks that we have */
    at_register_iqns(ti, NS_VCARD, &at_iq_vcard);
    at_register_iqns(ti, NS_LAST, &at_iq_last);
    at_register_iqns(ti, NS_GATEWAY, &at_iq_gateway);
    at_register_iqns(ti, NS_TIME, &at_iq_time);
    at_register_iqns(ti, NS_VERSION, &at_iq_version);
    at_register_iqns(ti, NS_SEARCH, &at_iq_search);
    at_register_iqns(ti, NS_BROWSE, &at_iq_browse);
    at_register_iqns(ti, NS_DISCO_ITEMS, &at_iq_disco_items);
    at_register_iqns(ti, NS_DISCO_INFO, &at_iq_disco_info);
}
