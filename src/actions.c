#include "TinyTile.h"

void killclient(const union Arg *arg)
{
    (void)arg;
    struct tiny_output *o = server.focused_output;
    if (o && o->focused)
        wlr_xdg_toplevel_send_close(o->focused->xdg_surface->toplevel);
}

void focusnext(const union Arg *arg)
{
    (void)arg;
    struct tiny_output *o = server.focused_output;
    if (!o)
        return;

    struct tiny_client *next = NULL;
    struct tiny_client *c;
    bool found = false;

    wl_list_for_each(c, &server.clients, link) {
        if (!visible(c) || c->monitor != o)
            continue;
        if (found) {
            next = c;
            break;
        }
        if (c == o->focused)
            found = true;
    }

    if (!next)
        wl_list_for_each(c, &server.clients, link) {
            if (visible(c) && c->monitor == o) {
                next = c;
                break;
            }
        }

    if (next)
        client_focus(next);
}

void focusprev(const union Arg *arg)
{
    (void)arg;
    struct tiny_output *o = server.focused_output;
    if (!o)
        return;

    struct tiny_client *prev = NULL;
    struct tiny_client *c;

    wl_list_for_each_reverse(c, &server.clients, link) {
        if (!visible(c) || c->monitor != o)
            continue;
        prev = c;
        if (c == o->focused)
            break;
    }

    if (prev && prev != o->focused)
        client_focus(prev);
}

void togglefloating(const union Arg *arg)
{
    (void)arg;
    struct tiny_output *o = server.focused_output;
    if (!o || !o->focused)
        return;

    o->focused->floating = !o->focused->floating;
    arrange(o);
}

void togglefullscreen(const union Arg *arg)
{
    (void)arg;
    struct tiny_output *o = server.focused_output;
    if (!o || !o->focused)
        return;

    bool fs = !o->focused->xdg_surface->toplevel->requested.fullscreen;
    wlr_xdg_toplevel_set_fullscreen(o->focused->xdg_surface->toplevel, fs);
}

void view(const union Arg *arg)
{
    tag_view(server.focused_output, arg->ui);
}

void toggleview(const union Arg *arg)
{
    tag_toggleview(server.focused_output, arg->ui);
}

void tag(const union Arg *arg)
{
    if (server.focused_output)
        tag_client(server.focused_output->focused, arg->ui);
}

void toggletag(const union Arg *arg)
{
    if (server.focused_output)
        tag_toggletag(server.focused_output->focused, arg->ui);
}