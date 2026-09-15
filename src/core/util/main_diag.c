#include "../../../inc/core/util/main_diag.h"

#include "../../../inc/core/forwarder/forwarder.h"
#include "../../../inc/core/flow/mac_learn.h"
#include "../../../inc/core/util/config.h"

#include <pthread.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

static pthread_mutex_t main_diag_lock = PTHREAD_MUTEX_INITIALIZER;

static const char *main_diag_level_name(enum main_diag_level level)
{
    switch (level) {
    case MAIN_DIAG_INFO:
        return "INFO";
    case MAIN_DIAG_WARN:
        return "WARN";
    case MAIN_DIAG_ERROR:
        return "ERROR";
    case MAIN_DIAG_FATAL:
        return "FATAL";
    }
    return "ERROR";
}

void main_diag_log(enum main_diag_level level, const char *component,
                   const char *fmt, ...)
{
    va_list ap;
    size_t len;

    if (!fmt)
        return;

    pthread_mutex_lock(&main_diag_lock);
    fprintf(stderr, "[%s] [%s] ", main_diag_level_name(level),
            component && component[0] ? component : "MAIN");
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    len = strlen(fmt);
    if (len == 0 || fmt[len - 1] != '\n')
        fputc('\n', stderr);
    fflush(stderr);
    pthread_mutex_unlock(&main_diag_lock);
}

void main_diag_log_db_apply(const struct app_config *cfg, int profile_id,
                            const struct app_config *prev_cfg)
{
    if (!cfg)
        return;
    if (prev_cfg) {
        main_diag_log(MAIN_DIAG_INFO, "DB",
                      "profile %d applied: LAN %d->%d, WAN %d->%d, policies %d->%d",
                      profile_id, prev_cfg->local_count, cfg->local_count,
                      prev_cfg->wan_count, cfg->wan_count,
                      prev_cfg->policy_count, cfg->policy_count);
        return;
    }
    main_diag_log(MAIN_DIAG_INFO, "DB",
                  "profile %d loaded: LAN=%d, WAN=%d, policies=%d",
                  profile_id, cfg->local_count, cfg->wan_count,
                  cfg->policy_count);
}

void main_diag_log_db_policy_apply(const struct app_config *cfg, int profile_id,
                                   const struct app_config *prev_cfg)
{
    if (!cfg)
        return;
    main_diag_log(MAIN_DIAG_INFO, "DB",
                  "profile %d policy update applied: %d->%d",
                  profile_id, prev_cfg ? prev_cfg->policy_count : 0,
                  cfg->policy_count);
}

void main_diag_log_config_summary(struct app_config *cfg, int profile_id,
                                  int is_reload, int policy_only)
{
    if (!cfg)
        return;
    main_diag_log(MAIN_DIAG_INFO, "CONFIG",
                  "profile %d %s: profiles=%d, policies=%d%s",
                  profile_id, is_reload ? "reloaded" : "loaded",
                  cfg->profile_count, cfg->policy_count,
                  policy_only ? ", policy-only" : "");
}

void main_diag_log_dataplane_ready(struct forwarder *fwd)
{
    if (!fwd || !fwd->cfg)
        return;

    /* Refresh is operational state initialization, not diagnostic output. */
    mac_learn_refresh_iface_macs(fwd);
    main_diag_log(MAIN_DIAG_INFO, "DATAPLANE",
                  "ready: LAN=%d, WAN=%d, policies=%d",
                  fwd->local_count, fwd->wan_count,
                  fwd->cfg->policy_count);
}
