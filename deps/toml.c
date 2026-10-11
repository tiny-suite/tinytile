#define _GNU_SOURCE
#include "toml.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

static void trim(char *str)
{
    char *start = str;
    while (isspace((unsigned char)*start))
        start++;

    if (*start == '\0') {
        *str = '\0';
        return;
    }

    char *end = start + strlen(start) - 1;
    while (end > start && isspace((unsigned char)*end))
        end--;

    end[1] = '\0';

    memmove(str, start, end - start + 2);
}

static int is_comment(const char *line)
{
    while (isspace((unsigned char)*line))
        line++;

    return *line == '#' || *line == ';';
}

static void strip_quotes(char *str)
{
    if (*str != '"')
        return;

    char *inner = str + 1;
    char *end = strchr(inner, '"');

    size_t len = end ? (size_t)(end - inner) : strlen(inner);
    memmove(str, inner, len);
    str[len] = '\0';
}

static void strip_braces(char *str)
{
    if (*str != '[')
        return;

    char *inner = str + 1;
    char *end = strrchr(inner, ']');

    size_t len = end ? (size_t)(end - inner) : strlen(inner);
    memmove(str, inner, len);
    str[len] = '\0';
}

static int grow(TomlConfig *cfg)
{
    if (cfg->count < cfg->capacity)
        return 0;

    cfg->capacity = cfg->capacity ? cfg->capacity * 2 : 8;
    cfg->entries = realloc(cfg->entries, sizeof(TomlEntry) * cfg->capacity);

    if (!cfg->entries)
        return -1;

    return 0;
}

void toml_init(TomlConfig *cfg)
{
    cfg->count = 0;
    cfg->capacity = 0;
    cfg->entries = NULL;
}

void toml_free(TomlConfig *cfg)
{
    for (int i = 0; i < cfg->count; i++) {
        free(cfg->entries[i].key);
        free(cfg->entries[i].value);
    }

    free(cfg->entries);
    cfg->entries = NULL;
    cfg->count = 0;
    cfg->capacity = 0;
}

int toml_add(TomlConfig *cfg, const char *key, const char *value)
{
    if (grow(cfg) != 0)
        return -1;

    TomlEntry *entry = &cfg->entries[cfg->count];
    entry->key = strdup(key);
    entry->value = strdup(value ? value : "");
    cfg->count++;

    return 0;
}

int toml_parse(TomlConfig *cfg, const char *filepath)
{
    FILE *file = fopen(filepath, "r");
    if (!file)
        return -1;

    char line[1024];

    while (fgets(line, sizeof(line), file)) {
        trim(line);

        if (*line == '\0')
            continue;

        if (is_comment(line))
            continue;

        if (line[0] == '[')
            continue;

        char *equals = strchr(line, '=');
        if (!equals)
            continue;

        *equals = '\0';

        char *key = line;
        char *value = equals + 1;

        trim(key);
        trim(value);

        strip_quotes(value);
        strip_braces(value);

        if (toml_add(cfg, key, value) != 0) {
            fclose(file);
            return -1;
        }
    }

    fclose(file);
    return 0;
}

const char *toml_get(TomlConfig *cfg, const char *key)
{
    for (int i = 0; i < cfg->count; i++) {
        if (strcmp(cfg->entries[i].key, key) == 0)
            return cfg->entries[i].value;
    }

    return NULL;
}

int toml_geti(TomlConfig *cfg, const char *key)
{
    const char *value = toml_get(cfg, key);
    if (!value)
        return 0;

    return (int)strtol(value, NULL, 10);
}

float toml_getf(TomlConfig *cfg, const char *key)
{
    const char *value = toml_get(cfg, key);
    if (!value)
        return 0.0f;

    return (float)strtod(value, NULL);
}