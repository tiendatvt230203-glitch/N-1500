#include "../../../inc/core/util/cpu_map.h"
#include "../../../inc/core/util/main_diag.h"

#include <sched.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

static int validate_group(const char *name, const uint8_t *cpus, uint32_t count,
                          const cpu_set_t *allowed, uint8_t seen[CPU_SETSIZE])
{
    for (uint32_t i = 0; i < count; i++) {
        unsigned int cpu = cpus[i];

        if (cpu >= CPU_SETSIZE || !CPU_ISSET(cpu, allowed)) {
            main_diag_log(MAIN_DIAG_ERROR, "DP-CONF",
                          "%s[%u]=CPU%u is offline or outside cpuset",
                          name, i, cpu);
            return -1;
        }
        if (seen[cpu]) {
            main_diag_log(MAIN_DIAG_ERROR, "DP-CONF",
                          "CPU%u is assigned to more than one dataplane role",
                          cpu);
            return -1;
        }
        seen[cpu] = 1;
    }
    return 0;
}

int ne_cpu_map_validate(void)
{
    cpu_set_t allowed;
    uint8_t seen[CPU_SETSIZE];

    CPU_ZERO(&allowed);
    memset(seen, 0, sizeof(seen));
    if (sched_getaffinity(0, sizeof(allowed), &allowed) != 0) {
        main_diag_log(MAIN_DIAG_ERROR, "DP-CONF",
                      "sched_getaffinity failed: %s", strerror(errno));
        return -1;
    }
    if (validate_group("RX_LAN", NE_CPU_RX_LAN, NE_RX_LAN_SLOTS,
                       &allowed, seen) != 0 ||
        validate_group("TX", NE_CPU_TX, NE_TX_SLOTS, &allowed, seen) != 0 ||
        validate_group("CRYPTO", NE_CPU_CRYPTO, NE_CRYPTO_WORKERS,
                       &allowed, seen) != 0 ||
        validate_group("RX_WAN", NE_CPU_RX_WAN, NE_RX_WAN_SLOTS,
                       &allowed, seen) != 0)
        return -1;
    return 0;
}
