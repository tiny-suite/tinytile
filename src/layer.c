#include "TinyTile.h"
#include <stdlib.h>

void layer_map(struct wl_listener *listener, void *data)
{
    struct tiny_layer *layer = wl_container_of(listener, layer, map);

    layer->layer_surface->surface->data = layer;
    arrange(layer->monitor);
}

void layer_unmap(struct wl_listener *listener, void *data)
{
    struct tiny_layer *layer = wl_container_of(listener, layer, unmap);
    arrange(layer->monitor);
}

void layer_commit(struct wl_listener *listener, void *data)
{
    struct tiny_layer *layer = wl_container_of(listener, layer, commit);
    struct wlr_layer_surface_v1 *layer_surface = layer->layer_surface;

    if (!layer_surface->initialized) {
        wlr_layer_surface_v1_configure(layer_surface, 0, 0);
        return;
    }

    arrange(layer->monitor);
}

void layer_destroy(struct wl_listener *listener, void *data)
{
    struct tiny_layer *layer = wl_container_of(listener, layer, destroy);

    wl_list_remove(&layer->map.link);
    wl_list_remove(&layer->unmap.link);
    wl_list_remove(&layer->commit.link);
    wl_list_remove(&layer->destroy.link);
    wl_list_remove(&layer->link);
    free(layer);
}