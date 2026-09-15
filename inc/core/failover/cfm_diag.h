#ifndef CFM_DIAG_H
#define CFM_DIAG_H

#include <stdbool.h>
#include <stdint.h>
#include <net/if.h>

struct app_config;

/* Packet-Parser-ne failover API — wan_dp is dataplane index (config_wan_cfg_to_dp). */
int cfm_init(const struct app_config *cfg);
/* 1 if CFM monitors wan_dp and link is DOWN; 0 if UP or not CFM-managed. */
int cfm_link_is_down(int wan_dp);
void cfm_cleanup(void);

int cfm_wan_status_by_name(const char *name);
void cfm_status_ipc_start(void);
int cfm_status_ipc_query(const char *name);

#endif
