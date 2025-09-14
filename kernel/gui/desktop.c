#include <desktop.h>
#include <kernel_heap.h>
#include <list.h>
#include <mouse.h>
#include <printf.h>
#include <rect.h>
#include <string.h>
#include <vesa.h>
#include <window.h>

desktop_t desktop_ = {};
desktop_t *desktop = &desktop_;
extern struct ps2_mouse mouse_device;

window_t *desktop_window_create(char *name, int x, int y, int width, int height)
{
    window_t *desktop_window = (window_t *)desktop;

    if (desktop_window->children_count >= MAX_CHILDREN) {
        printf("Cannot create more than %d windows\n", MAX_CHILDREN);
        return nullptr;
    }

    auto w = (window_t *)kzalloc(sizeof(window_t));
    if (w == nullptr) {
        printf("Failed to allocate memory for window\n");
        return nullptr;
    }

    strncpy(w->name, name, 20);
    w->x      = x;
    w->y      = y;
    w->width  = width;
    w->height = height;

    desktop_window->children[desktop_window->children_count] = w;
    desktop_window->children_count++;

    return w;
}

void desktop_window_destroy(window_t *window)
{
    if (window == nullptr) {
        return;
    }

    window_t *desktop_window = (window_t *)desktop;

    for (int i = 0; i < desktop_window->children_count; i++) {
        if (desktop_window->children[i] == window) {
            kfree(window);
            desktop_window->children[i] = nullptr;
            // Shift remaining windows
            for (int j = i; j < desktop_window->children_count - 1; j++) {
                desktop_window->children[j] = desktop_window->children[j + 1];
            }
            desktop_window->children[desktop_window->children_count - 1] = nullptr;
            desktop_window->children_count--;
            return;
        }
    }
}


// Used to get a list of windows overlapping the passed window
list_t *desktop_get_windows_above(window_t *window)
{
    window_t *desktop_window = (window_t *)desktop;
    int i;
    list_t *return_list = list_new();

    // Attempt to allocate the output list
    if (!return_list) {
        return return_list;
    }

    // We just need to get a list of all items in the
    // child list at higher indexes than the passed window
    // We start by finding the passed child in the list
    for (i = 0; i < desktop_window->children_count; i++)
        if (window == (window_t *)desktop_window->children[i]) {
            break;
        }

    // Now we just need to add the remaining items in the list
    // to the output (IF they overlap, of course)
    // NOTE: As a bonus, this will also automatically fall through
    // if the window wasn't found
    for (; i < desktop_window->children_count; i++) {

        window_t *current_window = desktop_window->children[i];

        // Our good old rectangle intersection logic
        if (current_window->x <= (window->x + window->width - 1) &&
            (current_window->x + current_window->width - 1) >= window->x &&
            current_window->y <= (window->y + window->height - 1) && (window->y + window->height - 1) >= window->y) {
            list_add(return_list, current_window); // Insert the overlapping window
        }
    }

    return return_list;
}

void desktop_draw_windows()
{
    window_t *desktop_window = (window_t *)desktop;

    // Loop through all of the children and call paint on each of them
    window_t *current_window;

    // Do the clipping for the desktop just like before
    // Add a rect for the desktop
    rect_t *temp_rect = rect_create(0, 0, vbe_info->height - 1, vbe_info->width - 1);
    vesa_add_clip_rect(temp_rect);

    // Now subtract each of the window rects from the desktop rect
    for (int i = 0; i < desktop_window->children_count; i++) {

        current_window = (window_t *)desktop_window->children[i];

        temp_rect = rect_create(current_window->y,
                                current_window->x,
                                current_window->y + current_window->height - 1,
                                current_window->x + current_window->width - 1);
        vesa_subtract_clip_rect(temp_rect);
        kfree(temp_rect); // rect_t doesn't end up in the clipping list
                          // during a subtract, so we need to get rid of it
    }

    // Fill the desktop
    vesa_fill_rect(0, 0, vbe_info->width, vbe_info->height, 0x113399);

    // Reset the context clipping
    vesa_clear_clip_rects();

    // Now we do a similar process to draw each window
    for (int i = 0; i < desktop_window->children_count; i++) {

        current_window = (window_t *)desktop_window->children[i];

        // Create and add a base rectangle for the current window
        temp_rect = rect_create(current_window->y,
                                current_window->x,
                                current_window->y + current_window->height - 1,
                                current_window->x + current_window->width - 1);
        vesa_add_clip_rect(temp_rect);

        // Now, we need to get and clip any windows overlapping this one
        list_t *clip_windows = desktop_get_windows_above(current_window);

        while (clip_windows->count) {

            // We do the different loop above and use List_remove_at because
            // we want to empty and destroy the list of clipping widows
            window_t *clipping_window = (window_t *)list_remove_at(clip_windows, 0);

            // Make sure we don't try and clip the window from itself
            if (clipping_window == current_window) {
                continue;
            }

            // Get a rectangle from the window, subtract it from the clipping
            // region, and dispose of it
            temp_rect = rect_create(clipping_window->y,
                                    clipping_window->x,
                                    clipping_window->y + clipping_window->height - 1,
                                    clipping_window->x + clipping_window->width - 1);
            vesa_subtract_clip_rect(temp_rect);
            kfree(temp_rect);
        }

        // Now that we've set up the clipping, we can do the
        // normal (but now clipped) window painting
        window_draw(current_window);

        // Dispose of the used-up list and clear the clipping we used to draw the window
        kfree(clip_windows);
        vesa_clear_clip_rects();
    }
}


/**
 * @brief Move window to the top of the list
 * @param window
 */
void desktop_raise_window(window_t *window)
{
    if (window == nullptr) {
        return;
    }

    window_t *desktop_window = (window_t *)desktop;

    for (int i = 0; i < desktop_window->children_count; i++) {
        if (desktop_window->children[i] == window) {
            // Shift remaining windows
            for (int j = i; j < desktop_window->children_count - 1; j++) {
                desktop_window->children[j] = desktop_window->children[j + 1];
            }
            desktop_window->children[desktop_window->children_count - 1] = window;
        }
    }
}

void desktop_process_mouse_event()
{
    window_t *desktop_window = (window_t *)desktop;

    auto mouse_x = mouse_device.x;
    auto mouse_y = mouse_device.y;

    bool left_button      = (mouse_device.flags & MOUSE_LEFT) != 0;
    bool prev_left_button = (mouse_device.prev_flags & MOUSE_LEFT) != 0;

    if (left_button) {
        if (!prev_left_button) {
            // Left button pressed
            for (int i = 0; i < desktop_window->children_count; i++) {
                auto window = desktop_window->children[i];
                if (window_was_clicked(window, mouse_x, mouse_y)) {
                    desktop_raise_window(window);
                    desktop_window->drag_x     = mouse_x - window->x;
                    desktop_window->drag_y     = mouse_y - window->y;
                    desktop_window->drag_child = window;

                    break;
                }
            }
        }
    } else {
        desktop_window->drag_child = nullptr;
    }

    if (desktop_window->drag_child != nullptr) {
        desktop_window->drag_child->x = mouse_x - desktop_window->drag_x;
        desktop_window->drag_child->y = mouse_y - desktop_window->drag_y;
    }

    if (desktop_window->drag_child != nullptr) {
        desktop_draw_windows();
    }
}
