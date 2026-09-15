#ifndef VAULT_H
#define VAULT_H

#define NE_VAULT_SECRET_PATH "kv/secret"
int ne_vault_unseal_and_login(void);

int ne_vault_load_secrets(void);

#endif
