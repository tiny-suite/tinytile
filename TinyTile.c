#include "TinyTile.h"
#include "toml.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <wlr/util/log.h>

struct tiny_server server;
struct tiny_config config;

static Key keys[64];
static int num_keys;

static void split_list(const char *src, char out[][256], int max, int *count)
{
    if (!src)
        return;

    int n = 0;
    const char *p = src;

    while (*p && n < max) {
        while (*p == ' ' || *p == ',' || *p == '"')
            p++;

        if (!*p)
            break;

        int i = 0;
        while (*p && *p != ',' && *p != '"' && i < 255)
            out[n][i++] = *p++;

        while (i > 0 && out[n][i - 1] == ' ')
            i--;

        out[n][i] = '\0';

        if (i > 0)
            n++;
    }

    *count = n;
}

static void split_tags(const char *src, char out[][16], int max, int *count)
{
    if (!src)
        return;

    int n = 0;
    const char *p = src;

    while (*p && n < max) {
        while (*p == ' ' || *p == ',' || *p == '"')
            p++;

        if (!*p)
            break;

        int i = 0;
        while (*p && *p != ',' && *p != '"' && i < 15)
            out[n][i++] = *p++;

        while (i > 0 && out[n][i - 1] == ' ')
            i--;

        out[n][i] = '\0';

        if (i > 0)
            n++;
    }

    *count = n;
}

void load_config(void)
{
    TomlConfig toml;
    toml_init(&toml);

    const char *path = "config/config.toml";
    if (toml_parse(&toml, path) != 0) {
        const char *home = getenv("HOME");
        static char userpath[512];
        if (home) {
            snprintf(userpath, sizeof(userpath), "%s/.config/tinytile/config.toml", home);
            toml_parse(&toml, userpath);
        }
    }

    const char *modkey = toml_get(&toml, "modkey");
    config.modkey = (modkey && strcmp(modkey, "Alt") == 0)
        ? WLR_MODIFIER_ALT : WLR_MODIFIER_LOGO;

    const char *v;

    v = toml_get(&toml, "terminal");
    snprintf(config.terminal, sizeof(config.terminal), "%s", v ? v : "st");

    v = toml_get(&toml, "menu");
    snprintf(config.menu, sizeof(config.menu), "%s", v ? v : "rofi -show drun");

    v = toml_get(&toml, "browser");
    snprintf(config.browser, sizeof(config.browser), "%s", v ? v : "firefox");

    v = toml_get(&toml, "showbar");
    config.showbar = v ? (strcmp(v, "true") == 0) : true;

    v = toml_get(&toml, "topbar");
    config.topbar = v ? (strcmp(v, "true") == 0) : true;

    v = toml_get(&toml, "focus_follows_mouse");
    config.focus_follows_mouse = v ? (strcmp(v, "true") == 0) : false;

    config.border = toml_geti(&toml, "border");
    config.gaps = toml_geti(&toml, "gaps");
    config.snap = toml_geti(&toml, "snap");
    config.mfact = toml_getf(&toml, "mfact");
    config.nmaster = toml_geti(&toml, "nmaster");

    if (config.border == 0)
        config.border = 2;
    if (config.gaps == 0)
        config.gaps = 5;
    if (config.mfact == 0.0f)
        config.mfact = 0.55f;
    if (config.nmaster == 0)
        config.nmaster = 1;

    v = toml_get(&toml, "norm_bg");
    snprintf(config.norm_bg, sizeof(config.norm_bg), "%s", v ? v : "222222");
    v = toml_get(&toml, "norm_border");
    snprintf(config.norm_border, sizeof(config.norm_border), "%s", v ? v : "444444");
    v = toml_get(&toml, "norm_fg");
    snprintf(config.norm_fg, sizeof(config.norm_fg), "%s", v ? v : "bbbbbb");
    v = toml_get(&toml, "sel_fg");
    snprintf(config.sel_fg, sizeof(config.sel_fg), "%s", v ? v : "eeeeee");
    v = toml_get(&toml, "sel_border");
    snprintf(config.sel_border, sizeof(config.sel_border), "%s", v ? v : "88c0d0");
    v = toml_get(&toml, "sel_bg");
    snprintf(config.sel_bg, sizeof(config.sel_bg), "%s", v ? v : "005577");

    config.num_tags = MAX_TAGS;
    for (int i = 0; i < MAX_TAGS; i++)
        snprintf(config.tags[i], sizeof(config.tags[i]), "%d", i + 1);

    v = toml_get(&toml, "tags");
    if (v) {
        split_tags(v, config.tags, MAX_TAGS, &config.num_tags);
        if (config.num_tags == 0) {
            config.num_tags = MAX_TAGS;
            for (int i = 0; i < MAX_TAGS; i++)
                snprintf(config.tags[i], sizeof(config.tags[i]), "%d", i + 1);
        }
    }

    v = toml_get(&toml, "autostart");
    config.num_autostart = 0;
    if (v)
        split_list(v, config.autostart, 8, &config.num_autostart);

    toml_free(&toml);
}

void run_autostart(void)
{
    for (int i = 0; i < config.num_autostart; i++)
        spawn_command(config.autostart[i]);
}

static void add_key(unsigned int mod, bool use_modkey, xkb_keysym_t sym,
                    void (*func)(const union Arg *arg), union Arg arg)
{
    Key *k = &keys[num_keys++];
    k->mod = mod;
    k->use_modkey = use_modkey;
    k->keysym = sym;
    k->func = func;
    k->arg = arg;
}

void setup_keybinds(void)
{
    num_keys = 0;

    add_key(0, true, XKB_KEY_Return, spawn_terminal, (union Arg){0});
    add_key(0, true, XKB_KEY_d, spawn_menu, (union Arg){0});
    add_key(0, true, XKB_KEY_w, spawn_browser, (union Arg){0});
    add_key(WLR_MODIFIER_SHIFT, true, XKB_KEY_q, killclient, (union Arg){0});
    add_key(WLR_MODIFIER_SHIFT, true, XKB_KEY_e, quit, (union Arg){0});
    add_key(0, true, XKB_KEY_j, focusnext, (union Arg){0});
    add_key(0, true, XKB_KEY_k, focusprev, (union Arg){0});
    add_key(0, true, XKB_KEY_f, togglefloating, (union Arg){0});
    add_key(0, true, XKB_KEY_m, togglefullscreen, (union Arg){0});

    for (int i = 0; i < config.num_tags && i < MAX_TAGS; i++) {
        add_key(0, true, XKB_KEY_1 + i, view, (union Arg){.ui = 1u << i});
        add_key(WLR_MODIFIER_SHIFT, true, XKB_KEY_1 + i, tag, (union Arg){.ui = 1u << i});
        add_key(WLR_MODIFIER_CTRL, true, XKB_KEY_1 + i, toggleview, (union Arg){.ui = 1u << i});
        add_key(WLR_MODIFIER_CTRL | WLR_MODIFIER_SHIFT, true, XKB_KEY_1 + i,
                toggletag, (union Arg){.ui = 1u << i});
    }
}

bool handle_keybinding(xkb_keysym_t sym, unsigned int mod)
{
    for (int i = 0; i < num_keys; i++) {
        unsigned int required = keys[i].mod;
        if (keys[i].use_modkey)
            required |= config.modkey;

        if (keys[i].keysym == sym && (mod & required) == required) {
            keys[i].func(&keys[i].arg);
            return true;
        }
    }

    return false;
}

int main(int argc, char *argv[])
{
    wlr_log_init(WLR_DEBUG, NULL);

    load_config();
    setup_keybinds();

    server_init();
    run_autostart();
    server_run();
    server_cleanup();

    return 0;
}