#include "TinyTile.h"
#include <string.h>

static bool tag_occupied(struct tiny_output *o, unsigned int tag)
{
    struct tiny_client *c;
    wl_list_for_each(c, &server.clients, link) {
        if (visible(c) && c->monitor == o && (c->tags & tag))
            return true;
    }

    return false;
}

static void tag_label(struct tiny_output *o, struct wlr_render_pass *pass,
                      int x, int y, int width, int index)
{
    unsigned int tag = 1u << index;
    bool active = (o->tagset & tag) != 0;
    bool occupied = tag_occupied(o, tag);

    float box[4];
    if (active)
        hex_to_color(config.sel_bg, box);
    else if (occupied)
        hex_to_color(config.sel_border, box);
    else
        hex_to_color(config.norm_border, box);

    fill_rect(pass, x, y + 4, width, BAR_HEIGHT - 8, box);

    float fg[4];
    if (active)
        hex_to_color(config.sel_fg, fg);
    else
        hex_to_color(config.norm_fg, fg);

    int scale = 2;
    int text_w = 4 * scale * (int)strlen(config.tags[index]) - scale;
    int tx = x + (width - text_w) / 2;
    int ty = y + (BAR_HEIGHT - 5 * scale) / 2;

    text_draw(pass, tx, ty, scale, fg, config.tags[index]);
}

void bar_draw(struct tiny_output *o, struct wlr_render_pass *pass)
{
    struct wlr_box full;
    wlr_output_layout_get_box(server.output_layout, o->wlr_output, &full);

    int y = config.topbar ? 0 : full.height - BAR_HEIGHT;

    float bg[4];
    hex_to_color(config.norm_bg, bg);
    fill_rect(pass, 0, y, full.width, BAR_HEIGHT, bg);

    int x = 4;
    int tag_w = 24;
    int pad = 4;

    for (int i = 0; i < config.num_tags; i++) {
        tag_label(o, pass, x, y, tag_w, i);
        x += tag_w + pad;
    }

    if (o->focused && o->focused->title[0]) {
        float fg[4];
        hex_to_color(config.sel_fg, fg);
        text_draw(pass, x + 8, y + (BAR_HEIGHT - 10) / 2, 2, fg, o->focused->title);
    }
}