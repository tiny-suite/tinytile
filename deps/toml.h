#ifndef TOML_H
#define TOML_H

typedef struct {
    char *key;
    char *value;
} TomlEntry;

typedef struct {
    int count;
    int capacity;
    TomlEntry *entries;
} TomlConfig;

void toml_init(TomlConfig *cfg);
void toml_free(TomlConfig *cfg);
int toml_parse(TomlConfig *cfg, const char *filepath);
int toml_add(TomlConfig *cfg, const char *key, const char *value);
const char *toml_get(TomlConfig *cfg, const char *key);
int toml_geti(TomlConfig *cfg, const char *key);
float toml_getf(TomlConfig *cfg, const char *key);

#endif