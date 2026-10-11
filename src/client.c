#include "TinyTile.h"
#include <stdio.h>
#include <stdlib.h>

bool visible(struct tiny_client *c)
{
    return c->mapped && (c->tags & c->monitor->tagset);
}

static void client_map(struct wl_listener *listener, void *data)
{
    struct tiny_client *c = wl_container_of(listener, c, map);

    c->mapped = true;

    if (!c->monitor->focused)
        client_focus(c);

    client_show(c);
    arrange(c->monitor);
    schedule_frames();
}

static void client_unmap(struct wl_listener *listener, void *data)
{
    struct tiny_client *c = wl_container_of(listener, c, unmap);

    c->mapped = false;

    if (c->monitor->focused == c)
        client_focus(NULL);

    arrange(c->monitor);
    schedule_frames();
}

static void client_commit(struct wl_listener *listener, void *data)
{
    struct tiny_client *c = wl_container_of(listener, c, commit);

    if (!c->mapped)
        return;

    arrange(c->monitor);
}

static void client_set_title(struct wl_listener *listener, void *data)
{
    struct tiny_client *c = wl_container_of(listener, c, set_title);

    const char *title = c->xdg_surface->toplevel->title;
    if (title)
        snprintf(c->title, sizeof(c->title), "%s", title);
    else
        c->title[0] = '\0';

    schedule_frames();
}

static void client_destroy(struct wl_listener *listener, void *data)
{
    struct tiny_client *c = wl_container_of(listener, c, destroy);

    wl_list_remove(&c->map.link);
    wl_list_remove(&c->unmap.link);
    wl_list_remove(&c->commit.link);
    wl_list_remove(&c->set_title.link);
    wl_list_remove(&c->destroy.link);
    wl_list_remove(&c->link);

    if (c->monitor->focused == c)
        c->monitor->focused = NULL;

    free(c);
}

struct tiny_client *client_create(struct wlr_xdg_surface *surface)
{
    struct tiny_output *monitor = server.focused_output;
    if (!monitor)
        return NULL;

    struct tiny_client *c = calloc(1, sizeof(struct tiny_client));
    c->server = &server;
    c->xdg_surface = surface;
    c->monitor = monitor;
    c->tags = monitor->tagset ? monitor->tagset : 1;
    c->floating = false;
    c->mapped = false;

    c->map.notify = client_map;
    wl_signal_add(&surface->surface->events.map, &c->map);
    c->unmap.notify = client_unmap;
    wl_signal_add(&surface->surface->events.unmap, &c->unmap);
    c->commit.notify = client_commit;
    wl_signal_add(&surface->surface->events.commit, &c->commit);
    c->destroy.notify = client_destroy;
    wl_signal_add(&surface->events.destroy, &c->destroy);
    c->set_title.notify = client_set_title;
    wl_signal_add(&surface->toplevel->events.set_title, &c->set_title);

    wl_list_insert(&server.clients, &c->link);

    surface->data = c;

    return c;
}

void client_activate(struct tiny_client *c, bool active)
{
    wlr_xdg_toplevel_set_activated(c->xdg_surface->toplevel, active);
}

void client_show(struct tiny_client *c)
{
    wlr_xdg_surface_schedule_configure(c->xdg_surface);
}

void client_hide(struct tiny_client *c)
{
    arrange(c->monitor);
}

void client_focus(struct tiny_client *c)
{
    if (c == NULL) {
        wlr_seat_keyboard_notify_enter(server.seat, NULL, NULL, 0, NULL);
        schedule_frames();
        return;
    }

    struct tiny_output *monitor = c->monitor;

    if (monitor->focused && monitor->focused != c)
        client_activate(monitor->focused, false);

    monitor->focused = c;
    wl_list_remove(&c->link);
    wl_list_insert(&server.clients, &c->link);

    client_activate(c, true);

    struct wlr_keyboard *kb = wlr_seat_get_keyboard(server.seat);
    if (kb)
        wlr_seat_keyboard_notify_enter(server.seat, c->xdg_surface->surface,
            kb->keycodes, kb->num_keycodes, &kb->modifiers);

    schedule_frames();
}

struct wlr_surface *surface_at(double lx, double ly, double *sx, double *sy, struct tiny_client **out)
{
    if (out)
        *out = NULL;

    for (int i = 3; i >= 0; i--) {
        struct tiny_output *o;
        wl_list_for_each(o, &server.outputs, link) {
            struct tiny_layer *l;
            wl_list_for_each(l, &o->layers[i], link) {
                struct wlr_layer_surface_v1 *ls = l->layer_surface;

                if (!ls->surface->mapped)
                    continue;

                struct wlr_box box;
                layer_get_box(o, ls, &box);

                struct wlr_surface *s = wlr_layer_surface_v1_surface_at(ls,
                    lx - box.x, ly - box.y, sx, sy);
                if (s)
                    return s;
            }
        }
    }

    struct tiny_client *c;
    wl_list_for_each(c, &server.clients, link) {
        if (!visible(c))
            continue;

        struct wlr_surface *s = wlr_xdg_surface_surface_at(c->xdg_surface,
            lx - c->geom.x, ly - c->geom.y, sx, sy);
        if (s) {
            if (out)
                *out = c;
            return s;
        }
    }

    return NULL;
}

struct tiny_client *client_at(struct tiny_output *o, double lx, double ly)
{
    struct tiny_client *c;
    wl_list_for_each(c, &server.clients, link) {
        if (!visible(c) || c->monitor != o)
            continue;

        double sx = lx - c->geom.x;
        double sy = ly - c->geom.y;
        if (sx >= 0 && sy >= 0 && sx < c->geom.width && sy < c->geom.height)
            return c;
    }

    return NULL;
}