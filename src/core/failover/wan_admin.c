#include "../../../inc/core/failover/wan_admin.h"
#include "../../../inc/core/util/config.h"
#include "../../../inc/core/forwarder/forwarder.h"
#include "../../../inc/core/forwarder/forwarder_wan.h"
#include "../../../inc/core/iface/interface.h"
#include "../../../inc/core/util/main_diag.h"

#include <net/if.h>
#include <string.h>

static int find_cfg_wan_idx(const struct app_config *cfg, const char *ifname)
{
    if (!cfg || !ifname)
        return -1;
    for (int i = 0; i < cfg->wan_count; i++) {
        if (strcmp(cfg->wans[i].ifname, ifname) == 0)
            return i;
    }
    return -1;
}

static int find_live_dp_by_ifname(struct forwarder *fwd, const char *ifname)
{
    int n;

    if (!fwd || !ifname)
        return -1;
    n = fwd->pair.wan_count;
    if (n < fwd->wan_count)
        n = fwd->wan_count;
    if (n > MAX_INTERFACES)
        n = MAX_INTERFACES;
    for (int i = 0; i < n; i++) {
        if (!ne_pair_wan_live(&fwd->pair, i))
            continue;
        if (fwd->wans[i].ifname[0] && strcmp(fwd->wans[i].ifname, ifname) == 0)
            return i;
        if (fwd->pair.wans[i].ifname[0] &&
            strcmp(fwd->pair.wans[i].ifname, ifname) == 0)
            return i;
    }
    return -1;
}

int wan_admin_kick(struct forwarder *fwd, const char *ifname)
{
    int di;
    int ci;

    if (!fwd || !ifname || !ifname[0])
        return -1;
    if (!fwd->threads_started) {
        main_diag_log(MAIN_DIAG_WARN, "WAN-ADMIN",
                      "dataplane is not running");
        return -1;
    }

    forwarder_runtime_lock();
    di = find_live_dp_by_ifname(fwd, ifname);
    if (di < 0) {
        ci = find_cfg_wan_idx(fwd->cfg, ifname);
        if (ci >= 0) {
            di = fwd_wan_live_dp_for_cfg(fwd, ci);
            if (di < 0) {
                for (int i = 0; i < MAX_INTERFACES; i++) {
                    if (fwd->wan_cfg_idx[i] == ci) {
                        di = i;
                        break;
                    }
                }
            }
        }
    }
    if (di < 0) {
        forwarder_runtime_unlock();
        main_diag_log(MAIN_DIAG_WARN, "WAN-ADMIN",
                      "cannot disable %s: interface not found", ifname);
        return -1;
    }

    if (fwd_wan_admin_is_held(di)) {
        forwarder_runtime_unlock();
        return 0;
    }

    (void)fwd_wan_flush_queue(fwd, di);
    fwd_wan_admin_hold_set(di, 1);
    forwarder_runtime_unlock();

    main_diag_log(MAIN_DIAG_INFO, "WAN-ADMIN",
                  "%s disabled (dataplane slot %d)", ifname, di);
    return 0;
}

int wan_admin_restore(struct forwarder *fwd, const char *ifname)
{
    int di = -1;
    int ci;
    int target_w = 1;

    if (!fwd || !fwd->cfg || !ifname || !ifname[0])
        return -1;
    if (!fwd->threads_started) {
        main_diag_log(MAIN_DIAG_WARN, "WAN-ADMIN",
                      "dataplane is not running");
        return -1;
    }

    ci = find_cfg_wan_idx(fwd->cfg, ifname);
    if (ci < 0 || !config_wan_live(fwd->cfg, ci)) {
        main_diag_log(MAIN_DIAG_WARN, "WAN-ADMIN",
                      "cannot restore %s: not active in configuration", ifname);
        return -1;
    }

    forwarder_runtime_lock();
    di = find_live_dp_by_ifname(fwd, ifname);
    if (di < 0) {
        for (int i = 0; i < MAX_INTERFACES; i++) {
            if (fwd->wan_cfg_idx[i] == ci) {
                di = i;
                break;
            }
        }
    }
    if (di < 0 || !ne_pair_wan_live(&fwd->pair, di)) {
        forwarder_runtime_unlock();
        main_diag_log(MAIN_DIAG_WARN, "WAN-ADMIN",
                      "cannot restore %s: dataplane is not plumbed", ifname);
        return -1;
    }

    fwd_wan_admin_hold_set(di, 0);
    target_w = config_wan_profile_weight(fwd->cfg, ci);
    if (target_w <= 0)
        target_w = 1;
    fwd_wan_join_ramp_begin(ci, target_w);
    forwarder_runtime_unlock();

    main_diag_log(MAIN_DIAG_INFO, "WAN-ADMIN",
                  "%s restored (dataplane slot %d)", ifname, di);
    return 0;
}
