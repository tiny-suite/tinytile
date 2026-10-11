#ifndef TINYTILE_H
#define TINYTILE_H

#include <stdbool.h>
#include <time.h>
#include <wayland-server-core.h>
#include <wlr/backend.h>
#include <wlr/backend/session.h>
#include <wlr/render/allocator.h>
#include <wlr/render/pass.h>
#include <wlr/render/wlr_renderer.h>
#include <wlr/types/wlr_cursor.h>
#include <wlr/types/wlr_input_device.h>
#include <wlr/types/wlr_keyboard.h>
#include <wlr/types/wlr_layer_shell_v1.h>
#include <wlr/types/wlr_output.h>
#include <wlr/types/wlr_output_layout.h>
#include <wlr/types/wlr_seat.h>
#include <wlr/types/wlr_xcursor_manager.h>
#include <wlr/types/wlr_xdg_shell.h>
#include <xkbcommon/xkbcommon.h>

#define MAX_TAGS 9
#define TAGMASK ((1u << MAX_TAGS) - 1)
#define BAR_HEIGHT 20
#define LENGTH(x) (sizeof(x) / sizeof(x[0]))

union Arg {
    int i;
    unsigned int ui;
    float f;
    const void *v;
};

struct tiny_server;
struct tiny_output;
struct tiny_client;

struct tiny_config {
    unsigned int modkey;
    char terminal[128];
    char menu[128];
    char browser[128];
    bool showbar;
    bool topbar;
    bool focus_follows_mouse;
    int border;
    int gaps;
    int snap;
    float mfact;
    int nmaster;
    char norm_bg[8];
    char norm_border[8];
    char norm_fg[8];
    char sel_fg[8];
    char sel_border[8];
    char sel_bg[8];
    int num_tags;
    char tags[MAX_TAGS][16];
    char autostart[8][256];
    int num_autostart;
};

struct tiny_client {
    struct wl_list link;
    struct tiny_server *server;
    struct wlr_xdg_surface *xdg_surface;
    struct tiny_output *monitor;
    unsigned int tags;
    bool floating;
    bool mapped;
    char title[256];
    struct wlr_box geom;
    struct wlr_box float_geom;
    struct wl_listener map;
    struct wl_listener unmap;
    struct wl_listener commit;
    struct wl_listener destroy;
    struct wl_listener set_title;
};

struct tiny_layer {
    struct wl_list link;
    struct tiny_server *server;
    struct wlr_layer_surface_v1 *layer_surface;
    struct tiny_output *monitor;
    struct wl_listener map;
    struct wl_listener unmap;
    struct wl_listener commit;
    struct wl_listener destroy;
};

struct tiny_output {
    struct wl_list link;
    struct tiny_server *server;
    struct wlr_output *wlr_output;
    struct wl_list layers[4];
    struct wlr_box usable_area;
    unsigned int tagset;
    struct tiny_client *focused;
    struct wl_listener frame;
    struct wl_listener destroy;
    struct wl_listener request_layout;
};

struct tiny_keyboard {
    struct wl_list link;
    struct tiny_server *server;
    struct wlr_keyboard *wlr_keyboard;
    struct wl_listener modifiers;
    struct wl_listener key;
};

typedef struct {
    unsigned int mod;
    bool use_modkey;
    xkb_keysym_t keysym;
    void (*func)(const union Arg *arg);
    union Arg arg;
} Key;

struct tiny_server {
    struct wl_display *display;
    struct wlr_backend *backend;
    struct wlr_session *session;
    struct wlr_renderer *renderer;
    struct wlr_allocator *allocator;
    struct wlr_seat *seat;
    struct wlr_cursor *cursor;
    struct wlr_xcursor_manager *cursor_mgr;
    struct wlr_output_layout *output_layout;
    struct wlr_xdg_shell *xdg_shell;
    struct wlr_layer_shell_v1 *layer_shell;

    struct wl_listener new_output;
    struct wl_listener new_input;
    struct wl_listener new_xdg_surface;
    struct wl_listener new_layer_surface;
    struct wl_listener cursor_motion;
    struct wl_listener cursor_motion_absolute;
    struct wl_listener cursor_button;
    struct wl_listener cursor_axis;
    struct wl_listener cursor_frame;

    struct wl_list outputs;
    struct wl_list keyboards;
    struct wl_list clients;
    struct tiny_output *focused_output;
};

extern struct tiny_server server;
extern struct tiny_config config;

void server_init(void);
void server_run(void);
void server_cleanup(void);

void load_config(void);
void run_autostart(void);
void setup_keybinds(void);
bool handle_keybinding(xkb_keysym_t sym, unsigned int mod);

struct tiny_client *client_create(struct wlr_xdg_surface *surface);
void client_focus(struct tiny_client *c);
void client_activate(struct tiny_client *c, bool active);
void client_show(struct tiny_client *c);
void client_hide(struct tiny_client *c);
struct tiny_client *client_at(struct tiny_output *o, double lx, double ly);
struct wlr_surface *surface_at(double lx, double ly, double *sx, double *sy, struct tiny_client **out);

void tag_view(struct tiny_output *o, unsigned int tags);
void tag_toggleview(struct tiny_output *o, unsigned int tags);
void tag_client(struct tiny_client *c, unsigned int tags);
void tag_toggletag(struct tiny_client *c, unsigned int tags);

void arrange(struct tiny_output *o);
void dwindle_layout(struct tiny_output *o);
bool visible(struct tiny_client *c);

void render_output(struct tiny_output *o);
void layer_get_box(struct tiny_output *o, struct wlr_layer_surface_v1 *ls, struct wlr_box *box);
void bar_draw(struct tiny_output *o, struct wlr_render_pass *pass);
void fill_rect(struct wlr_render_pass *pass, int x, int y, int w, int h, const float color[4]);
void text_draw(struct wlr_render_pass *pass, int x, int y, int scale, const float color[4], const char *str);
void schedule_frames(void);
void hex_to_color(const char *hex, float out[4]);

void layer_map(struct wl_listener *listener, void *data);
void layer_unmap(struct wl_listener *listener, void *data);
void layer_commit(struct wl_listener *listener, void *data);
void layer_destroy(struct wl_listener *listener, void *data);

void spawn_command(const char *command);
void spawn_terminal(const union Arg *arg);
void spawn_menu(const union Arg *arg);
void spawn_browser(const union Arg *arg);
void quit(const union Arg *arg);
void killclient(const union Arg *arg);
void focusnext(const union Arg *arg);
void focusprev(const union Arg *arg);
void togglefloating(const union Arg *arg);
void togglefullscreen(const union Arg *arg);
void view(const union Arg *arg);
void toggleview(const union Arg *arg);
void tag(const union Arg *arg);
void toggletag(const union Arg *arg);

#endif