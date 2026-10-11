#include "TinyTile.h"
#include <stdlib.h>
#include <wlr/types/wlr_layer_shell_v1.h>
#include <wlr/types/wlr_pointer.h>
#include <wlr/util/log.h>

static void output_frame(struct wl_listener *listener, void *data)
{
    struct tiny_output *output = wl_container_of(listener, output, frame);
    render_output(output);
}

static void output_destroy(struct wl_listener *listener, void *data)
{
    struct tiny_output *output = wl_container_of(listener, output, destroy);

    wl_list_remove(&output->frame.link);
    wl_list_remove(&output->destroy.link);
    wl_list_remove(&output->request_layout.link);
    wl_list_remove(&output->link);
    free(output);
}

static void output_request_layout(struct wl_listener *listener, void *data)
{
    struct tiny_output *output = wl_container_of(listener, output, request_layout);
    arrange(output);
}

static void new_output(struct wl_listener *listener, void *data)
{
    struct wlr_output *wlr_output = data;

    wlr_output_init_render(wlr_output, server.allocator, server.renderer);

    struct tiny_output *output = calloc(1, sizeof(struct tiny_output));
    output->server = &server;
    output->wlr_output = wlr_output;
    output->tagset = 1;

    for (int i = 0; i < 4; i++)
        wl_list_init(&output->layers[i]);

    output->frame.notify = output_frame;
    wl_signal_add(&wlr_output->events.frame, &output->frame);

    output->destroy.notify = output_destroy;
    wl_signal_add(&wlr_output->events.destroy, &output->destroy);

    output->request_layout.notify = output_request_layout;
    wl_signal_add(&wlr_output->events.request_state, &output->request_layout);

    wl_list_insert(&server.outputs, &output->link);

    wlr_output_layout_add_auto(server.output_layout, wlr_output);

    struct wlr_output_mode *mode = wlr_output_preferred_mode(wlr_output);
    if (mode)
        wlr_output_set_mode(wlr_output, mode);
    wlr_output_enable(wlr_output, true);
    wlr_output_commit(wlr_output);

    if (!server.focused_output)
        server.focused_output = output;

    schedule_frames();

    wlr_log(WLR_INFO, "new output: %s", wlr_output->name);
}

static void keyboard_modifiers(struct wl_listener *listener, void *data)
{
    struct tiny_keyboard *keyboard = wl_container_of(listener, keyboard, modifiers);
    wlr_seat_set_keyboard(server.seat, keyboard->wlr_keyboard);
    wlr_seat_keyboard_notify_modifiers(server.seat, &keyboard->wlr_keyboard->modifiers);
}

static void keyboard_key(struct wl_listener *listener, void *data)
{
    struct tiny_keyboard *keyboard = wl_container_of(listener, keyboard, key);
    struct wlr_keyboard_key_event *event = data;

    uint32_t keycode = event->keycode + 8;
    const xkb_keysym_t *syms;
    int nsyms = xkb_state_key_get_syms(keyboard->wlr_keyboard->xkb_state, keycode, &syms);

    bool handled = false;
    unsigned int mod = wlr_keyboard_get_modifiers(keyboard->wlr_keyboard);
    for (int i = 0; i < nsyms; i++)
        handled = handle_keybinding(syms[i], mod) || handled;

    if (!handled) {
        wlr_seat_set_keyboard(server.seat, keyboard->wlr_keyboard);
        wlr_seat_keyboard_notify_key(server.seat, event->time_msec, event->keycode, event->state);
    }
}

static void new_input(struct wl_listener *listener, void *data)
{
    struct wlr_input_device *device = data;

    switch (device->type) {
    case WLR_INPUT_DEVICE_KEYBOARD: {
        struct tiny_keyboard *keyboard = calloc(1, sizeof(struct tiny_keyboard));
        keyboard->server = &server;
        keyboard->wlr_keyboard = wlr_keyboard_from_input_device(device);

        struct xkb_context *context = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
        struct xkb_keymap *keymap = xkb_keymap_new_from_names(context, NULL,
            XKB_KEYMAP_COMPILE_NO_FLAGS);

        wlr_keyboard_set_keymap(keyboard->wlr_keyboard, keymap);
        xkb_keymap_unref(keymap);
        xkb_context_unref(context);
        wlr_keyboard_set_repeat_info(keyboard->wlr_keyboard, 25, 600);

        keyboard->modifiers.notify = keyboard_modifiers;
        wl_signal_add(&keyboard->wlr_keyboard->events.modifiers, &keyboard->modifiers);
        keyboard->key.notify = keyboard_key;
        wl_signal_add(&keyboard->wlr_keyboard->events.key, &keyboard->key);

        wl_list_insert(&server.keyboards, &keyboard->link);
        break;
    }
    case WLR_INPUT_DEVICE_POINTER:
        wlr_cursor_attach_input_device(server.cursor, device);
        break;
    default:
        break;
    }

    uint32_t caps = WL_SEAT_CAPABILITY_POINTER;
    if (!wl_list_empty(&server.keyboards))
        caps |= WL_SEAT_CAPABILITY_KEYBOARD;
    wlr_seat_set_capabilities(server.seat, caps);
}

static void new_xdg_surface(struct wl_listener *listener, void *data)
{
    struct wlr_xdg_surface *surface = data;

    if (surface->role != WLR_XDG_SURFACE_ROLE_TOPLEVEL)
        return;

    client_create(surface);
}

static void new_layer_surface(struct wl_listener *listener, void *data)
{
    struct wlr_layer_surface_v1 *layer_surface = data;

    if (!layer_surface->output) {
        layer_surface->output = server.focused_output
            ? server.focused_output->wlr_output : NULL;
    }

    struct tiny_output *output = server.focused_output;
    if (!output) {
        wlr_layer_surface_v1_destroy(layer_surface);
        return;
    }

    struct tiny_layer *layer = calloc(1, sizeof(struct tiny_layer));
    layer->server = &server;
    layer->layer_surface = layer_surface;
    layer->monitor = output;

    layer->map.notify = layer_map;
    wl_signal_add(&layer_surface->surface->events.map, &layer->map);
    layer->unmap.notify = layer_unmap;
    wl_signal_add(&layer_surface->surface->events.unmap, &layer->unmap);
    layer->commit.notify = layer_commit;
    wl_signal_add(&layer_surface->surface->events.commit, &layer->commit);
    layer->destroy.notify = layer_destroy;
    wl_signal_add(&layer_surface->events.destroy, &layer->destroy);

    wl_list_insert(&output->layers[layer_surface->pending.layer], &layer->link);
}

static void process_cursor_motion(uint32_t time)
{
    double sx, sy;
    struct tiny_client *c = NULL;
    struct wlr_surface *surface = surface_at(server.cursor->x, server.cursor->y, &sx, &sy, &c);

    if (!surface) {
        wlr_seat_pointer_clear_focus(server.seat);
        return;
    }

    if (config.focus_follows_mouse && c)
        client_focus(c);

    wlr_seat_pointer_notify_enter(server.seat, surface, sx, sy);
    wlr_seat_pointer_notify_motion(server.seat, time, sx, sy);
}

static void cursor_motion(struct wl_listener *listener, void *data)
{
    struct wlr_pointer_motion_event *event = data;
    wlr_cursor_move(server.cursor, &event->pointer->base, event->delta_x, event->delta_y);
    process_cursor_motion(event->time_msec);
}

static void cursor_motion_absolute(struct wl_listener *listener, void *data)
{
    struct wlr_pointer_motion_absolute_event *event = data;
    wlr_cursor_warp_absolute(server.cursor, &event->pointer->base, event->x, event->y);
    process_cursor_motion(event->time_msec);
}

static void cursor_button(struct wl_listener *listener, void *data)
{
    struct wlr_pointer_button_event *event = data;

    if (event->state == WL_POINTER_BUTTON_STATE_PRESSED) {
        double sx, sy;
        struct tiny_client *c = NULL;
        struct wlr_surface *surface = surface_at(server.cursor->x, server.cursor->y, &sx, &sy, &c);
        if (surface && c)
            client_focus(c);
    }

    wlr_seat_pointer_notify_button(server.seat, event->time_msec, event->button, event->state);
}

static void cursor_axis(struct wl_listener *listener, void *data)
{
    struct wlr_pointer_axis_event *event = data;
    wlr_seat_pointer_notify_axis(server.seat, event->time_msec, event->orientation,
        event->delta, event->delta_discrete, event->source, event->relative_direction);
}

static void cursor_frame(struct wl_listener *listener, void *data)
{
    wlr_seat_pointer_notify_frame(server.seat);
}

void server_init(void)
{
    server.display = wl_display_create();
    server.backend = wlr_backend_autocreate(server.display, &server.session);
    server.renderer = wlr_renderer_autocreate(server.backend);
    server.allocator = wlr_allocator_autocreate(server.backend, server.renderer);

    wlr_renderer_init_wl_display(server.renderer, server.display);

    server.xdg_shell = wlr_xdg_shell_create(server.display, 3);
    server.layer_shell = wlr_layer_shell_v1_create(server.display, 4);
    server.seat = wlr_seat_create(server.display, "seat0");

    server.output_layout = wlr_output_layout_create(server.display);
    server.cursor = wlr_cursor_create();
    wlr_cursor_attach_output_layout(server.cursor, server.output_layout);
    server.cursor_mgr = wlr_xcursor_manager_create(NULL, 24);
    wlr_xcursor_manager_load(server.cursor_mgr, 1);
    wlr_cursor_set_xcursor(server.cursor, server.cursor_mgr, "default");

    wl_list_init(&server.outputs);
    wl_list_init(&server.keyboards);
    wl_list_init(&server.clients);

    server.new_output.notify = new_output;
    wl_signal_add(&server.backend->events.new_output, &server.new_output);

    server.new_input.notify = new_input;
    wl_signal_add(&server.backend->events.new_input, &server.new_input);

    server.new_xdg_surface.notify = new_xdg_surface;
    wl_signal_add(&server.xdg_shell->events.new_surface, &server.new_xdg_surface);

    server.new_layer_surface.notify = new_layer_surface;
    wl_signal_add(&server.layer_shell->events.new_surface, &server.new_layer_surface);

    server.cursor_motion.notify = cursor_motion;
    wl_signal_add(&server.cursor->events.motion, &server.cursor_motion);
    server.cursor_motion_absolute.notify = cursor_motion_absolute;
    wl_signal_add(&server.cursor->events.motion_absolute, &server.cursor_motion_absolute);
    server.cursor_button.notify = cursor_button;
    wl_signal_add(&server.cursor->events.button, &server.cursor_button);
    server.cursor_axis.notify = cursor_axis;
    wl_signal_add(&server.cursor->events.axis, &server.cursor_axis);
    server.cursor_frame.notify = cursor_frame;
    wl_signal_add(&server.cursor->events.frame, &server.cursor_frame);
}

void server_run(void)
{
    const char *socket = wl_display_add_socket_auto(server.display);
    if (!socket) {
        wlr_log(WLR_ERROR, "unable to open wayland socket");
        return;
    }

    if (!wlr_backend_start(server.backend)) {
        wlr_log(WLR_ERROR, "unable to start backend");
        wl_display_destroy(server.display);
        return;
    }

    setenv("WAYLAND_DISPLAY", socket, true);
    wl_display_run(server.display);
}

void server_cleanup(void)
{
    wl_display_destroy_clients(server.display);
    wl_display_destroy(server.display);
}