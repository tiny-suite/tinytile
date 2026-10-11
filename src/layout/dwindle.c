#include "TinyTile.h"
#include <stdlib.h>

static void apply_geom(struct tiny_client *c, struct wlr_box area)
{
    int inset = config.gaps + config.border;

    c->geom.x = area.x + inset;
    c->geom.y = area.y + inset;
    c->geom.width = area.width - 2 * inset;
    c->geom.height = area.height - 2 * inset;

    if (c->geom.width < 1)
        c->geom.width = 1;
    if (c->geom.height < 1)
        c->geom.height = 1;

    wlr_xdg_toplevel_set_size(c->xdg_surface->toplevel, c->geom.width, c->geom.height);
}

static void dwindle(struct tiny_client **clients, int count, struct wlr_box area, bool vertical)
{
    if (count <= 0)
        return;

    if (count == 1) {
        apply_geom(clients[0], area);
        return;
    }

    struct wlr_box first, rest;

    if (vertical) {
        int half = area.width / 2;
        first = (struct wlr_box){area.x, area.y, half, area.height};
        rest = (struct wlr_box){area.x + half, area.y, area.width - half, area.height};
    } else {
        int half = area.height / 2;
        first = (struct wlr_box){area.x, area.y, area.width, half};
        rest = (struct wlr_box){area.x, area.y + half, area.width, area.height - half};
    }

    apply_geom(clients[0], first);
    dwindle(clients + 1, count - 1, rest, !vertical);
}

static void reserve_layer_zones(struct tiny_output *o)
{
    for (int i = 0; i < 4; i++) {
        struct tiny_layer *l;
        wl_list_for_each(l, &o->layers[i], link) {
            struct wlr_layer_surface_v1 *ls = l->layer_surface;

            if (!ls->surface->mapped || ls->current.exclusive_zone <= 0)
                continue;

            int zone = ls->current.exclusive_zone;
            uint32_t anchor = ls->current.anchor;

            if (anchor & ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP) {
                o->usable_area.y += zone;
                o->usable_area.height -= zone;
            } else if (anchor & ZWLR_LAYER_SURFACE_V1_ANCHOR_BOTTOM) {
                o->usable_area.height -= zone;
            }
        }
    }
}

static void update_usable_area(struct tiny_output *o)
{
    struct wlr_box full;
    wlr_output_layout_get_box(server.output_layout, o->wlr_output, &full);

    o->usable_area = full;

    if (config.showbar) {
        if (config.topbar) {
            o->usable_area.y += BAR_HEIGHT;
            o->usable_area.height -= BAR_HEIGHT;
        } else {
            o->usable_area.height -= BAR_HEIGHT;
        }
    }

    reserve_layer_zones(o);
}

void dwindle_layout(struct tiny_output *o)
{
    struct wlr_box area = o->usable_area;

    int tiled = 0;
    struct tiny_client *c;
    wl_list_for_each(c, &server.clients, link) {
        if (visible(c) && c->monitor == o && !c->floating)
            tiled++;
    }

    if (tiled == 0)
        return;

    struct tiny_client **clients = calloc(tiled, sizeof(struct tiny_client *));
    int n = 0;

    wl_list_for_each(c, &server.clients, link) {
        if (!visible(c) || c->monitor != o || c->floating)
            continue;
        clients[n++] = c;
    }

    dwindle(clients, tiled, area, true);

    free(clients);
}

static void float_client(struct tiny_output *o, struct tiny_client *c)
{
    if (c->float_geom.width == 0) {
        c->float_geom.width = o->usable_area.width / 2;
        c->float_geom.height = o->usable_area.height / 2;
        c->float_geom.x = o->usable_area.x + (o->usable_area.width - c->float_geom.width) / 2;
        c->float_geom.y = o->usable_area.y + (o->usable_area.height - c->float_geom.height) / 2;
    }

    c->geom = c->float_geom;
    wlr_xdg_toplevel_set_size(c->xdg_surface->toplevel, c->geom.width, c->geom.height);
}

void arrange(struct tiny_output *o)
{
    if (!o)
        return;

    update_usable_area(o);
    dwindle_layout(o);

    struct tiny_client *c;
    wl_list_for_each(c, &server.clients, link) {
        if (visible(c) && c->monitor == o && c->floating)
            float_client(o, c);
    }

    schedule_frames();
}