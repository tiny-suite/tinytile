#include "TinyTile.h"

static void focus_visible(struct tiny_output *o)
{
    struct tiny_client *c;
    wl_list_for_each(c, &server.clients, link) {
        if (visible(c) && c->monitor == o) {
            client_focus(c);
            return;
        }
    }

    client_focus(NULL);
}

void tag_view(struct tiny_output *o, unsigned int tags)
{
    o->tagset = tags & TAGMASK;
    focus_visible(o);
    arrange(o);
}

void tag_toggleview(struct tiny_output *o, unsigned int tags)
{
    unsigned int newset = o->tagset ^ (tags & TAGMASK);

    if (!newset)
        return;

    o->tagset = newset;
    focus_visible(o);
    arrange(o);
}

void tag_client(struct tiny_client *c, unsigned int tags)
{
    if (!c)
        return;

    c->tags = tags & TAGMASK;

    if (!(c->tags & c->monitor->tagset))
        client_hide(c);

    arrange(c->monitor);
}

void tag_toggletag(struct tiny_client *c, unsigned int tags)
{
    if (!c)
        return;

    c->tags ^= (tags & TAGMASK);
    if (!c->tags)
        c->tags = 1;

    arrange(c->monitor);
}