#include "../../../inc/core/util/cpu_map.h"
#include "../../../inc/core/util/main_diag.h"
#include "../../../inc/core/util/config.h"

#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
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

static int append_cpu_list(char *out, size_t out_size, size_t *used,
                           const uint8_t *cpus, uint32_t count,
                           uint8_t seen[CPU_SETSIZE])
{
    for (uint32_t i = 0; i < count; i++) {
        unsigned int cpu = cpus[i];
        int n;

        if (cpu >= CPU_SETSIZE || seen[cpu])
            continue;
        n = snprintf(out + *used, out_size - *used, "%s%u",
                     *used ? "," : "", cpu);
        if (n < 0 || (size_t)n >= out_size - *used)
            return -1;
        *used += (size_t)n;
        seen[cpu] = 1u;
    }
    return 0;
}

static int irq_line_matches_config(const char *line,
                                   const struct app_config *cfg)
{
    if (strstr(line, "ixgbe"))
        return 1;
    if (!cfg)
        return 0;
    for (int i = 0; i < cfg->local_count && i < MAX_INTERFACES; i++) {
        if (cfg->locals[i].ifname[0] && strstr(line, cfg->locals[i].ifname))
            return 1;
    }
    for (int i = 0; i < cfg->wan_count && i < MAX_INTERFACES; i++) {
        if (cfg->wans[i].ifname[0] && strstr(line, cfg->wans[i].ifname))
            return 1;
    }
    return 0;
}

int ne_cpu_map_configure_irq_affinity(const struct app_config *cfg)
{
    uint8_t seen[CPU_SETSIZE] = {0};
    char cpu_list[256] = {0};
    char line[2048];
    size_t used = 0;
    FILE *interrupts;
    int matched = 0;
    int updated = 0;
    int failed = 0;

    /* IRQ/NAPI work belongs only on the RX/TX cores. Crypto workers are
     * intentionally excluded so NIC interrupts cannot steal AES/PQC time. */
    for (uint32_t i = 0; i < NE_CRYPTO_WORKERS; i++)
        seen[NE_CPU_CRYPTO[i]] = 1u;
    if (append_cpu_list(cpu_list, sizeof(cpu_list), &used,
                        NE_CPU_RX_LAN, NE_RX_LAN_SLOTS, seen) != 0 ||
        append_cpu_list(cpu_list, sizeof(cpu_list), &used,
                        NE_CPU_TX, NE_TX_SLOTS, seen) != 0 ||
        append_cpu_list(cpu_list, sizeof(cpu_list), &used,
                        NE_CPU_RX_WAN, NE_RX_WAN_SLOTS, seen) != 0 ||
        used == 0)
        return -1;

    interrupts = fopen("/proc/interrupts", "r");
    if (!interrupts) {
        main_diag_log(MAIN_DIAG_WARN, "DP-CONF",
                      "cannot read /proc/interrupts: %s", strerror(errno));
        return -1;
    }
    while (fgets(line, sizeof(line), interrupts)) {
        char *cursor = line;
        char *end = NULL;
        long irq;
        char path[128];
        FILE *affinity;

        while (*cursor == ' ' || *cursor == '\t')
            cursor++;
        errno = 0;
        irq = strtol(cursor, &end, 10);
        if (errno || end == cursor || *end != ':' || irq < 0 ||
            !irq_line_matches_config(line, cfg))
            continue;
        matched++;
        snprintf(path, sizeof(path), "/proc/irq/%ld/smp_affinity_list", irq);
        affinity = fopen(path, "w");
        if (!affinity) {
            failed++;
            continue;
        }
        if (fprintf(affinity, "%s\n", cpu_list) < 0) {
            (void)fclose(affinity);
            failed++;
            continue;
        }
        if (fclose(affinity) != 0) {
            failed++;
            continue;
        }
        updated++;
    }
    fclose(interrupts);

    if (failed) {
        main_diag_log(MAIN_DIAG_WARN, "DP-CONF",
                      "NIC IRQ affinity cpus=%s updated=%d/%d (need root; irqbalance may override)",
                      cpu_list, updated, matched);
        return -1;
    }
    main_diag_log(MAIN_DIAG_INFO, "DP-CONF",
                  "NIC IRQ affinity cpus=%s updated=%d", cpu_list, updated);
    return updated > 0 ? 0 : -1;
}
