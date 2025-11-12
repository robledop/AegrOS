#include <wm/window.h>
#include <types.h>
#include <user.h>

#include "wm/desktop.h"

/**
 * @brief Allocate and initialise a window.
 *
 * @param x Left coordinate.
 * @param y Top coordinate.
 * @param width Window width in pixels.
 * @param height Window height in pixels.
 * @param flags Window flags (e.g. decorations).
 * @param context Drawing context backing the window.
 * @return Pointer to the new window or nullptr on failure.
 */
window_t *window_new(i16 x, i16 y, u16 width, u16 height, u16 flags, video_context_t *context)
{
    window_t *window = malloc(sizeof(window_t));
    if (!window) {
        return window;
    }

    if (!window_init(window, x, y, width, height, flags, context)) {
        free(window);
        return nullptr;
    }

    return window;
}

/**
 * @brief Initialise an already allocated window structure.
 *
 * @param window Window instance to initialise.
 * @param x Left coordinate.
 * @param y Top coordinate.
 * @param width Window width in pixels.
 * @param height Window height in pixels.
 * @param flags Window flags (e.g. decorations).
 * @param context Drawing context backing the window.
 * @return Non-zero on success, zero on failure.
 */
int window_init(window_t *window, i16 x, i16 y, u16 width, u16 height, u16 flags,
                video_context_t *context)
{
    window->children = list_new();
    if (!window->children) {
        return 0;
    }

    window->x                  = x;
    window->y                  = y;
    window->width              = width;
    window->height             = height;
    window->context            = context;
    window->flags              = flags;
    window->parent             = nullptr;
    window->drag_child         = nullptr;
    window->drag_off_x         = 0;
    window->drag_off_y         = 0;
    window->last_button_state  = 0;
    window->paint_function     = window_paint_handler;
    window->mousedown_function = window_mousedown_handler;
    window->active_child       = nullptr;
    window->title              = nullptr;

    return 1;
}

/**
 * @brief Recursively compute the window's absolute screen X coordinate.
 */
int window_screen_x(window_t *window)
{
    if (window->parent) {
        return window->x + window_screen_x(window->parent);
    }

    return window->x;
}

/**
 * @brief Recursively compute the window's absolute screen Y coordinate.
 */
int window_screen_y(window_t *window)
{
    if (window->parent) {
        return window->y + window_screen_y(window->parent);
    }

    return window->y;
}

/**
 * @brief Draw the decorative frame and title bar for a window.
 */
void window_draw_border(window_t *window)
{
    int screen_x = window_screen_x(window);
    int screen_y = window_screen_y(window);

    // Draw a border around the window. Concentric rectangles until the border width is achieved
    for (int i = 0; i < WIN_BORDER_WIDTH; i++) {
        context_draw_rect(window->context,
                          screen_x + i,
                          screen_y + i,
                          window->width - (2 * i),
                          window->height - (2 * i),
                          WIN_BORDER_COLOR);
    }

    // Draw a border line under the titlebar
    for (int i = 1; i <= WIN_BORDER_WIDTH; i++) {
        context_horizontal_line(window->context,
                                screen_x + WIN_BORDER_WIDTH,
                                screen_y + (WIN_TITLE_HEIGHT - i),
                                window->width - (2 * WIN_BORDER_WIDTH),
                                WIN_BORDER_COLOR);
    }

    // Fill in the titlebar background
    context_fill_rect(window->context,
                      screen_x + WIN_BORDER_WIDTH,
                      screen_y + WIN_BORDER_WIDTH,
                      window->width - (2 * WIN_BORDER_WIDTH),
                      (WIN_TITLE_HEIGHT - (2 * WIN_BORDER_WIDTH)),
                      window->parent->active_child == window ? WIN_TITLE_COLOR : WIN_TITLE_COLOR_INACTIVE);


    // Draw the window title
    context_draw_text(window->context,
                      window->title,
                      screen_x + WIN_TITLE_MARGIN,
                      screen_y + WIN_TITLE_MARGIN,
                      window->parent->active_child == window ? WIN_TITLE_TEXT_COLOR : WIN_TITLE_TEXT_COLOR_INACTIVE);
}

/**
 * @brief Apply clipping for the window bounds, optionally inheriting parent clips.
 *
 * @param window Window whose visible area is being computed.
 * @param in_recursion Non-zero when invoked from a recursive parent traversal.
 * @param dirty_regions Optional dirty region list used to limit repainting.
 */
void window_apply_bound_clipping(window_t *window, int in_recursion, list_t *dirty_regions)
{
    if (!window->context) {
        return;
    }

    // Build the visibility rectangle for this window
    // If the window is decorated and we're recursing, we want to limit
    // the window's drawable area to the area inside the window decoration.
    // If we're not recursing, however, it means we're about to paint
    // ourself and therefore we want to wait until we've finished painting
    // the window border to shrink the clipping area
    int screen_x = window_screen_x(window);
    int screen_y = window_screen_y(window);

    rect_t *temp_rect;
    if ((!(window->flags & WIN_NODECORATION)) && in_recursion) {
        // Limit client drawable area
        screen_x += WIN_BORDER_WIDTH;
        screen_y += WIN_TITLE_HEIGHT;
        temp_rect = rect_new(screen_y,
                             screen_x,
                             screen_y + window->height - WIN_TITLE_HEIGHT - WIN_BORDER_WIDTH - 1,
                             screen_x + window->width - (2 * WIN_BORDER_WIDTH) - 1);
    } else {
        temp_rect = rect_new(screen_y, screen_x, screen_y + window->height - 1, screen_x + window->width - 1);
    }

    // If there's no parent (meaning we're at the top of the window tree)
    // then we just add our rectangle and exit
    // Here's our change: If we were passed a dirty region list, we first
    // clone those dirty rects into the clipping region and then intersect
    // the top-level window bounds against it so that we're limited to the
    // dirty region from the outset
    if (!window->parent) {
        if (dirty_regions) {
            // Clone the dirty regions and put them into the clipping list
            for (unsigned int i = 0; i < dirty_regions->count; i++) {
                // Clone
                rect_t *current_dirty_rect = list_get_at(dirty_regions, i);
                rect_t *clone_dirty_rect   = rect_new(current_dirty_rect->top,
                                                    current_dirty_rect->left,
                                                    current_dirty_rect->bottom,
                                                    current_dirty_rect->right);

                // Add
                context_add_clip_rect(window->context, clone_dirty_rect);
            }

            // Finally, intersect this top level window against them
            context_intersect_clip_rect(window->context, temp_rect);

        } else {
            context_add_clip_rect(window->context, temp_rect);
        }

        return;
    }

    // Otherwise, we first reduce our clipping area to the visibility area of our parent
    window_apply_bound_clipping(window->parent, 1, dirty_regions);

    // Now that we've reduced our clipping area to our parent's clipping area, we can
    // intersect our own bounds rectangle to get our main visible area
    context_intersect_clip_rect(window->context, temp_rect);

    // And finally, we subtract the rectangles of any siblings that are occluding us
    list_t *clip_windows = window_get_windows_above(window->parent, window);

    while (clip_windows->count) {
        window_t *clipping_window = list_remove_at(clip_windows, 0);

        // Get a rectangle from the window, subtract it from the clipping
        // region, and dispose of it
        screen_x = window_screen_x(clipping_window);
        screen_y = window_screen_y(clipping_window);

        temp_rect =
            rect_new(screen_y, screen_x, screen_y + clipping_window->height - 1, screen_x + clipping_window->width - 1);
        context_subtract_clip_rect(window->context, temp_rect);
        free(temp_rect);
    }

    // Dispose of the used-up list
    free(clip_windows);
}

/**
 * @brief Redraw the window title bar to reflect changes in activation or text.
 */
void window_update_title(window_t *window)
{
    if (!window->context) {
        return;
    }

    if (window->flags & WIN_NODECORATION) {
        return;
    }

    // Start by limiting painting to the window's visible area
    window_apply_bound_clipping(window, 0, nullptr);

    // Draw border
    window_draw_border(window);

    context_clear_clip_rects(window->context);
}

/**
 * @brief Request a repaint of a window sub-region.
 *
 * Coordinates are provided in window-local space and converted to screen space before generating
 * a dirty rectangle.
 */
void window_invalidate(window_t *window, int top, int left, int bottom, int right)
{
    // This function takes coordinates in terms of window coordinates
    // So we need to convert them to screen space
    int origin_x = window_screen_x(window);
    int origin_y = window_screen_y(window);
    top += origin_y;
    bottom += origin_y;
    left += origin_x;
    right += origin_x;

    list_t *dirty_regions = list_new();
    if (!dirty_regions) {
        return;
    }

    rect_t *dirty_rect = rect_new(top, left, bottom, right);
    if (!dirty_rect) {
        free(dirty_regions);
        return;
    }

    if (!list_add(dirty_regions, dirty_rect)) {
        free(dirty_regions);
        free(dirty_rect);
        return;
    }

    window_paint(window, dirty_regions, 0);

    // Clean up the dirty rect list
    list_remove_at(dirty_regions, 0);
    free(dirty_regions);
    free(dirty_rect);
}

/**
 * @brief Paint a window and, optionally, its descendants.
 *
 * @param window Window to paint.
 * @param dirty_regions Optional dirty rectangle list limiting repaint areas.
 * @param paint_children Whether to recurse into child windows.
 */
void window_paint(window_t *window, list_t *dirty_regions, u8 paint_children)
{
    window_t *current_child;
    rect_t *temp_rect;

    if (!window->context) {
        return;
    }

    // Start by limiting painting to the window's visible area
    window_apply_bound_clipping(window, 0, dirty_regions);

    // Set the context translation
    int screen_x = window_screen_x(window);
    int screen_y = window_screen_y(window);

    // If we have window decorations turned on, draw them and then further
    // limit the clipping area to the inner drawable area of the window
    if (!(window->flags & WIN_NODECORATION)) {
        // Draw border
        window_draw_border(window);

        // Limit client drawable area
        screen_x += WIN_BORDER_WIDTH;
        screen_y += WIN_TITLE_HEIGHT;
        temp_rect = rect_new(screen_y,
                             screen_x,
                             screen_y + window->height - WIN_TITLE_HEIGHT - WIN_BORDER_WIDTH - 1,
                             screen_x + window->width - (2 * WIN_BORDER_WIDTH) - 1);
        context_intersect_clip_rect(window->context, temp_rect);
    }

    // Then subtract the screen rectangles of any children
    // NOTE: We don't do this in window_apply_bound_clipping because, due to
    // its recursive nature, it would cause the screen rectangles of all of
    // our parent's children to be subtracted from the clipping area -- which
    // would eliminate this window.
    for (unsigned int i = 0; i < window->children->count; i++) {
        current_child = (window_t *)list_get_at(window->children, i);

        int child_screen_x = window_screen_x(current_child);
        int child_screen_y = window_screen_y(current_child);

        temp_rect = rect_new(child_screen_y,
                             child_screen_x,
                             child_screen_y + current_child->height - 1,
                             child_screen_x + current_child->width - 1);
        context_subtract_clip_rect(window->context, temp_rect);
        free(temp_rect);
    }

    // Finally, with all the clipping set up, we can set the context's 0,0 to the top-left corner
    // of the window's drawable area, and call the window's final paint function
    window->context->translate_x = screen_x;
    window->context->translate_y = screen_y;
    window->paint_function(window);

    // Now that we're done drawing this window, we can clear the changes we made to the context
    context_clear_clip_rects(window->context);
    window->context->translate_x = 0;
    window->context->translate_y = 0;

    // Even though we're no longer having all mouse events cause a redraw from the desktop
    // down, we still need to call paint on our children in the case that we were called with
    // a dirty region list since each window needs to be responsible for recursively checking
    // if its children were dirtied
    if (!paint_children) {
        return;
    }

    for (unsigned int i = 0; i < window->children->count; i++) {

        current_child = (window_t *)list_get_at(window->children, i);

        if (dirty_regions) {
            // Check to see if the child is affected by any of the
            // dirty region rectangles
            unsigned int j;
            for (j = 0; j < dirty_regions->count; j++) {
                temp_rect = (rect_t *)list_get_at(dirty_regions, j);

                screen_x = window_screen_x(current_child);
                screen_y = window_screen_y(current_child);

                if (temp_rect->left <= (screen_x + current_child->width - 1) && temp_rect->right >= screen_x &&
                    temp_rect->top <= (screen_y + current_child->height - 1) && temp_rect->bottom >= screen_y) {
                    break;
                }
            }

            // Skip drawing this child if no intersection was found
            if (j == dirty_regions->count) {
                continue;
            }
        }

        // Otherwise, recursively request the child to redraw its dirty areas
        window_paint(current_child, dirty_regions, 1);
    }
}

/**
 * @brief Default paint routine filling the window with the background colour.
 */
void window_paint_handler(window_t *window)
{
    context_fill_rect(window->context, 0, 0, window->width, window->height, WIN_BGCOLOR);
}

/**
 * @brief Collect child windows stacked above the specified window and overlapping it.
 *
 * @param parent Parent window owning the children.
 * @param child Reference child window.
 * @return List of overlapping windows higher in Z-order (caller frees).
 */
list_t *window_get_windows_above(window_t *parent, window_t *child)
{
    list_t *return_list = list_new();
    if (!return_list) {
        return return_list;
    }

    // We just need to get a list of all items in the
    // child list at higher indexes than the passed window
    // We start by finding the passed child in the list
    int i = list_find(parent->children, child);
    if (i == -1) {
        panic("window_get_windows_above: Couldn't find child window in parent's child list");
    }

    // Now we just need to add the remaining items in the list
    // to the output (IF they overlap, of course)
    // NOTE: As a bonus, this will also automatically fall through
    // if the window wasn't found
    for (i++; i < (int)parent->children->count; i++) {
        window_t *current_window = list_get_at(parent->children, i);

        // Our good old rectangle intersection logic
        if (current_window->x <= (child->x + child->width - 1) &&
            (current_window->x + current_window->width - 1) >= child->x &&
            current_window->y <= (child->y + child->height - 1) &&
            (current_window->y + current_window->height - 1) >= child->y) {
            list_add(return_list, current_window); // Insert the overlapping window
        }
    }

    return return_list;
}

/**
 * @brief Collect child windows stacked below the specified window and overlapping it.
 *
 * @param parent Parent window owning the children.
 * @param child Reference child window.
 * @return List of overlapping windows lower in Z-order (caller frees).
 */
list_t *window_get_windows_below(window_t *parent, window_t *child)
{
    list_t *return_list = list_new();
    if (!return_list) {
        return return_list;
    }

    // We just need to get a list of all items in the
    // child list at higher indexes than the passed window
    // We start by finding the passed child in the list
    int i = list_find(parent->children, child);
    if (i == -1) {
        panic("window_get_windows_below: Couldn't find child window in parent's child list");
    }

    // Now we just need to add the remaining items in the list
    // to the output (IF they overlap, of course)
    // NOTE: As a bonus, this will also automatically fall through
    // if the window wasn't found
    for (i--; i > -1; i--) {
        window_t *current_window = list_get_at(parent->children, i);

        // Our good old rectangle intersection logic
        if (current_window->x <= (child->x + child->width - 1) &&
            (current_window->x + current_window->width - 1) >= child->x &&
            current_window->y <= (child->y + child->height - 1) &&
            (current_window->y + current_window->height - 1) >= child->y) {
            list_add(return_list, current_window); // Insert the overlapping window
        }
    }

    return return_list;
}

/**
 * @brief Bring a window to the front of its siblings optionally repainting it.
 *
 * @param window Window to raise.
 * @param do_draw Non-zero to repaint the window after raising.
 */
void window_raise(window_t *window, u8 do_draw)
{
    if (!window->parent) {
        return;
    }

    window_t *parent = window->parent;

    if (parent->active_child == window) {
        return;
    }

    window_t *last_active = parent->active_child;

    int i = list_find(parent->children, window);
    if (i == -1) {
        panic("window_raise: Couldn't find child window in parent's child list");
    }

    list_remove_at(parent->children, i); // Pull window out of list
    list_add(parent->children, window);  // Insert at the top

    // Make it active
    parent->active_child = window;

    // Do a redraw if it was requested
    if (!do_draw) {
        return;
    }

    window_paint(window, nullptr, 1);

    // Make sure the old active window gets an updated title color
    window_update_title(last_active);
}

/**
 * @brief Move a window to a new position and repaint affected regions.
 *
 * @param window Window to move.
 * @param new_x New left coordinate.
 * @param new_y New top coordinate.
 */
void window_move(window_t *window, int new_x, int new_y)
{
    int old_x = window->x;
    int old_y = window->y;
    rect_t new_window_rect;

    // To make life a little bit easier, we'll make the not-unreasonable
    // rule that if a window is moved, it must become the top-most window
    window_raise(window, 0); // Raise it, but don't repaint it yet

    // We'll hijack our dirty rect collection from our existing clipping operations
    // So, first we'll get the visible regions of the original window position
    window_apply_bound_clipping(window, 0, nullptr);

    // Temporarily update the window position
    window->x = new_x;
    window->y = new_y;

    // Calculate the new bounds
    new_window_rect.top    = window_screen_y(window);
    new_window_rect.left   = window_screen_x(window);
    new_window_rect.bottom = new_window_rect.top + window->height - 1;
    new_window_rect.right  = new_window_rect.left + window->width - 1;

    // Reset the window position
    window->x = old_x;
    window->y = old_y;

    // Now, we'll get the *actual* dirty area by subtracting the new location of
    // the window
    context_subtract_clip_rect(window->context, &new_window_rect);

    // Now that the context clipping tools made the list of dirty rects for us,
    // we can go ahead and steal the list it made for our own purposes
    //(yes, it would be cleaner to spin off our boolean rect functions so that
    // they can be used both here and by the clipping region tools, but I ain't
    // got time for that junk)
    list_t *replacement_list = list_new();
    if (!replacement_list) {
        context_clear_clip_rects(window->context);
        return;
    }

    list_t *dirty_list          = window->context->clip_rects;
    window->context->clip_rects = replacement_list;

    // Now, let's get all of the siblings that we overlap before the move
    list_t *dirty_windows = window_get_windows_below(window->parent, window);

    window->x = new_x;
    window->y = new_y;

    // And we'll repaint all of them using the dirty rects
    //(removing them from the list as we go for convenience)
    while (dirty_windows->count)
        window_paint(list_remove_at(dirty_windows, 0), dirty_list, 1);

    // The one thing that might still be dirty is the parent we're inside of
    window_paint(window->parent, dirty_list, 0);

    // We're done with the lists, so we can dump them
    while (dirty_list->count)
        free(list_remove_at(dirty_list, 0));

    free(dirty_list);
    free(dirty_windows);

    // With the dirtied siblings redrawn, we can do the final update of
    // the window location and paint it at that new position
    window_paint(window, nullptr, 1);
}

// Interface between windowing system and mouse device
/**
 * @brief Dispatch mouse events to windows, handling dragging and focus changes.
 *
 * @param window Window receiving the event.
 * @param mouse_x Mouse X coordinate.
 * @param mouse_y Mouse Y coordinate.
 * @param mouse_buttons Mouse button state bitmask.
 */
void window_process_mouse(window_t *window, u16 mouse_x, u16 mouse_y, u8 mouse_buttons)
{
    // auto left_click     = mouse_buttons & MOUSE_LEFT;
    auto left_click = mouse_buttons;
    // auto was_left_click = window->last_button_state & MOUSE_LEFT;
    auto was_left_click = window->last_button_state;

    // If we had a button depressed, then we need to see if the mouse was
    // over any of the child windows
    // We go front-to-back in terms of the window stack for kfree occlusion
    for (int i = (int)window->children->count - 1; i >= 0; i--) {

        window_t *child = list_get_at(window->children, i);

        // If mouse isn't window bounds, we can't possibly be interacting with it
        if (!(mouse_x >= child->x && mouse_x < (child->x + child->width) && mouse_y >= child->y &&
            mouse_y < (child->y + child->height))) {
            continue;
        }

        // Now we'll check to see if we're dragging a titlebar
        if (left_click && !was_left_click) {

            // Let's adjust things so that a raise happens whenever we click inside a
            // child, to be more consistent with most other GUIs
            window_raise(child, 1);

            // See if the mouse position lies within the bounds of the current
            // window's 31 px tall titlebar
            // We check the decoration flag since we can't drag a window without a titlebar
            if (!(child->flags & WIN_NODECORATION) && mouse_y >= child->y && mouse_y < (child->y + 31)) {

                // We'll also set this window as the window being dragged
                // until such a time as the mouse is released
                window->drag_off_x = mouse_x - child->x;
                window->drag_off_y = mouse_y - child->y;
                window->drag_child = child;

                // We break without setting target_child if we're doing a drag since
                // that shouldn't trigger a mouse event in the child
                break;
            }
        }

        // Found a target, so forward the mouse event to that window and quit looking
        window_process_mouse(child, mouse_x - child->x, mouse_y - child->y, mouse_buttons);
        break;
    }

    // Moving this outside of the mouse-in-child detection since it doesn't really
    // have anything to do with it
    if (!left_click) {
        window->drag_child = nullptr;
    }

    // Update drag window to match the mouse if we have an active drag window
    if (window->drag_child) {
        // Changed to use
        window_move(window->drag_child, mouse_x - window->drag_off_x, mouse_y - window->drag_off_y);
    }

    // If we didn't find a target in the search, then we ourselves are the target of any clicks
    if (window->mousedown_function && left_click && !was_left_click) {
        window->mousedown_function(window, mouse_x, mouse_y);
    }

    // Update the stored mouse button state to match the current state
    window->last_button_state = mouse_buttons;
}

/**
 * @brief Default no-op mouse down handler for windows.
 */
void window_mousedown_handler([[maybe_unused]] window_t *window, [[maybe_unused]] i16 x, [[maybe_unused]] i16 y)
{
}

/**
 * @brief Propagate a new drawing context to a window and its children.
 *
 * @param window Root of the subtree to update.
 * @param context New video context.
 */
void window_update_context(window_t *window, video_context_t *context)
{
    window->context = context;

    for (unsigned int i = 0; i < window->children->count; i++)
        window_update_context(list_get_at(window->children, i), context);
}

/**
 * @brief Attach a child window to a parent and update its context.
 *
 * @param window Parent window.
 * @param child Child window to insert.
 */
void window_insert_child(window_t *window, window_t *child)
{

    child->parent = window;
    list_add(window->children, child);
    child->parent->active_child = child;

    window_update_context(child, window->context);
}

/**
 * @brief Convenience helper to create and attach a child window.
 *
 * @param window Parent window owning the child.
 * @param x Child left coordinate.
 * @param y Child top coordinate.
 * @param width Child width.
 * @param height Child height.
 * @param flags Child window flags.
 * @return Newly created child window or nullptr on failure.
 */
window_t *window_create_window(window_t *window, i16 x, i16 y, u16 width, i16 height, u16 flags)
{
    window_t *new_window = window_new(x, y, width, height, flags, window->context);
    if (!new_window) {
        return new_window;
    }

    // Attempt to add the window to the end of the parent's children list
    // If we fail, make sure to clean up all of our allocations so far
    if (!list_add(window->children, new_window)) {

        free(new_window);
        return nullptr;
    }

    // Set the new child's parent
    new_window->parent               = window;
    new_window->parent->active_child = new_window;

    return new_window;
}

/**
 * @brief Assign a new title string to a window, redrawing as necessary.
 *
 * @param window Window to update.
 * @param new_title Null-terminated string to set as the title.
 */
void window_set_title(window_t *window, char *new_title)
{
    // Make sure to kfree any preexisting title
    if (window->title) {
        free(window->title);
    }

    int len = (int)strlen(new_title);

    // Try to allocate new memory to clone the string
    //(+1 because of the trailing zero in a c-string)
    window->title = (char *)malloc((len + 1) * sizeof(char));
    if (!window->title) {
        return;
    }

    memcpy(window->title, new_title, len + 1);

    // Make sure the change is reflected on-screen
    if (window->flags & WIN_NODECORATION) {
        window_invalidate(window, 0, 0, window->height - 1, window->width - 1);
    } else {
        window_update_title(window);
    }
}

/**
 * @brief Append characters to the existing window title, redrawing if needed.
 *
 * @param window Window whose title will be extended.
 * @param additional_chars Null-terminated string to append.
 */
void window_append_title(window_t *window, char *additional_chars)
{
    // Set the title if there isn't already one
    if (!window->title) {
        window_set_title(window, additional_chars);
        return;
    }

    int original_length   = (int)strlen(window->title);
    int additional_length = (int)strlen(additional_chars);

    char *new_string = malloc(sizeof(char) * (original_length + additional_length + 1));
    if (!new_string) {
        return;
    }

    // Copy the base string into the new string
    int i;
    for (i            = 0; window->title[i]; i++)
        new_string[i] = window->title[i];

    // Copy the appended chars into the new string
    for (i                              = 0; additional_chars[i]; i++)
        new_string[original_length + i] = additional_chars[i];

    // Add the final zero char
    new_string[original_length + i] = 0;

    // And swap the string pointers
    free(window->title);
    window->title = new_string;

    // Make sure the change is reflected on-screen
    if (window->flags & WIN_NODECORATION) {
        window_invalidate(window, 0, 0, window->height - 1, window->width - 1);
    } else {
        window_update_title(window);
    }
}
