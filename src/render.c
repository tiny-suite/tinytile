#include "TinyTile.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void hex_to_color(const char *hex, float out[4])
{
    unsigned int r = 0, g = 0, b = 0;

    if (hex && strlen(hex) >= 6)
        sscanf(hex, "%2x%2x%2x", &r, &g, &b);

    out[0] = r / 255.0f;
    out[1] = g / 255.0f;
    out[2] = b / 255.0f;
    out[3] = 1.0f;
}

static void fill(struct wlr_render_pass *pass, int x, int y, int w, int h, const float color[4])
{
    if (w <= 0 || h <= 0)
        return;

    struct wlr_render_rect_options opts = {
        .box = { .x = x, .y = y, .width = w, .height = h },
        .color = { .r = color[0], .g = color[1], .b = color[2], .a = color[3] },
    };

    wlr_render_pass_add_rect(pass, &opts);
}

void fill_rect(struct wlr_render_pass *pass, int x, int y, int w, int h, const float color[4])
{
    fill(pass, x, y, w, h, color);
}

struct render_data {
    struct wlr_render_pass *pass;
    struct tiny_output *output;
    struct tiny_client *client;
    struct timespec *when;
};

static void render_surface(struct wlr_surface *surface, int sx, int sy, void *data)
{
    struct render_data *rdata = data;
    struct tiny_client *c = rdata->client;

    struct wlr_texture *texture = wlr_surface_get_texture(surface);
    if (!texture)
        return;

    struct wlr_box box = {
        .x = c->geom.x + sx,
        .y = c->geom.y + sy,
        .width = surface->current.width,
        .height = surface->current.height,
    };

    struct wlr_render_texture_options opts = {
        .texture = texture,
        .dst_box = box,
        .alpha = 1.0f,
        .transform = rdata->output->wlr_output->transform,
    };

    wlr_render_pass_add_texture(rdata->pass, &opts);
    wlr_surface_send_frame_done(surface, rdata->when);
}

static void render_client(struct tiny_output *o, struct tiny_client *c,
                          struct wlr_render_pass *pass, struct timespec *when)
{
    const char *border = (o->focused == c) ? config.sel_border : config.norm_border;
    float color[4];
    hex_to_color(border, color);

    int b = config.border;
    fill(pass, c->geom.x - b, c->geom.y - b, c->geom.width + 2 * b, c->geom.height + 2 * b, color);

    struct render_data rdata = {
        .pass = pass,
        .output = o,
        .client = c,
        .when = when,
    };

    wlr_xdg_surface_for_each_surface(c->xdg_surface, render_surface, &rdata);
}

void layer_get_box(struct tiny_output *o, struct wlr_layer_surface_v1 *ls, struct wlr_box *box)
{
    int ow, oh;
    wlr_output_effective_resolution(o->wlr_output, &ow, &oh);

    int w = ls->surface->current.width;
    int h = ls->surface->current.height;
    uint32_t anchor = ls->current.anchor;

    int x = (anchor & ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT) ? ow - w : 0;
    int y = (anchor & ZWLR_LAYER_SURFACE_V1_ANCHOR_BOTTOM) ? oh - h : 0;

    if ((anchor & ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT) && (anchor & ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT))
        w = ow;
    if ((anchor & ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP) && (anchor & ZWLR_LAYER_SURFACE_V1_ANCHOR_BOTTOM))
        h = oh;

    box->x = x + ls->current.margin.left;
    box->y = y + ls->current.margin.top;
    box->width = w;
    box->height = h;
}

struct layer_render_data {
    struct wlr_render_pass *pass;
    struct tiny_output *output;
    struct wlr_box box;
    struct timespec *when;
};

static void render_layer_surface(struct wlr_surface *surface, int sx, int sy, void *data)
{
    struct layer_render_data *rdata = data;

    struct wlr_texture *texture = wlr_surface_get_texture(surface);
    if (!texture)
        return;

    struct wlr_box box = {
        .x = rdata->box.x + sx,
        .y = rdata->box.y + sy,
        .width = surface->current.width,
        .height = surface->current.height,
    };

    struct wlr_render_texture_options opts = {
        .texture = texture,
        .dst_box = box,
        .alpha = 1.0f,
        .transform = rdata->output->wlr_output->transform,
    };

    wlr_render_pass_add_texture(rdata->pass, &opts);
    wlr_surface_send_frame_done(surface, rdata->when);
}

static void render_layer_group(struct tiny_output *o, int layer,
                               struct wlr_render_pass *pass, struct timespec *when)
{
    struct tiny_layer *l;
    wl_list_for_each(l, &o->layers[layer], link) {
        if (!l->layer_surface->surface->mapped)
            continue;

        struct wlr_box box;
        layer_get_box(o, l->layer_surface, &box);

        struct layer_render_data rdata = {
            .pass = pass,
            .output = o,
            .box = box,
            .when = when,
        };

        wlr_layer_surface_v1_for_each_surface(l->layer_surface, render_layer_surface, &rdata);
    }
}

void render_output(struct tiny_output *o)
{
    struct wlr_output *wlr_output = o->wlr_output;

    struct wlr_render_pass *pass = wlr_output_begin_render_pass(wlr_output, NULL, NULL, NULL);
    if (!pass)
        return;

    int width, height;
    wlr_output_effective_resolution(wlr_output, &width, &height);

    float bg[4];
    hex_to_color(config.norm_bg, bg);
    fill(pass, 0, 0, width, height, bg);

    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);

    render_layer_group(o, ZWLR_LAYER_SHELL_V1_LAYER_BACKGROUND, pass, &now);
    render_layer_group(o, ZWLR_LAYER_SHELL_V1_LAYER_BOTTOM, pass, &now);

    struct tiny_client *c;
    wl_list_for_each_reverse(c, &server.clients, link) {
        if (visible(c) && c->monitor == o)
            render_client(o, c, pass, &now);
    }

    render_layer_group(o, ZWLR_LAYER_SHELL_V1_LAYER_TOP, pass, &now);
    render_layer_group(o, ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY, pass, &now);

    if (config.showbar)
        bar_draw(o, pass);

    wlr_render_pass_submit(pass);
    wlr_output_commit_state(wlr_output, NULL);
}