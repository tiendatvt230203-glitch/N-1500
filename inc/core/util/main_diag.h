#ifndef MAIN_DIAG_H
#define MAIN_DIAG_H

struct app_config;
struct forwarder;

enum main_diag_level {
    MAIN_DIAG_INFO = 0,
    MAIN_DIAG_WARN,
    MAIN_DIAG_ERROR,
    MAIN_DIAG_FATAL,
};

void main_diag_log(enum main_diag_level level, const char *component,
                   const char *fmt, ...)
#if defined(__GNUC__)
    __attribute__((format(printf, 3, 4)))
#endif
    ;

void main_diag_log_db_apply(const struct app_config *cfg, int trigger_profile_id,
                            const struct app_config *prev_cfg);
void main_diag_log_db_policy_apply(const struct app_config *cfg,
                                   int trigger_profile_id,
                                   const struct app_config *prev_cfg);
void main_diag_log_config_summary(struct app_config *cfg, int trigger_profile_id,
                                  int is_reload, int policy_only);
void main_diag_log_dataplane_ready(struct forwarder *fwd);

#endif
