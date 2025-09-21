#include <gui/rect.h>
#include <kernel_heap.h>

/**
 * @brief Allocate a rectangle descriptor.
 *
 * @param top Top edge coordinate.
 * @param left Left edge coordinate.
 * @param bottom Bottom edge coordinate.
 * @param right Right edge coordinate.
 * @return Pointer to the newly allocated rectangle or nullptr on failure.
 */
rect_t *rect_new(int top, int left, int bottom, int right)
{
    auto rect = (rect_t *)kzalloc(sizeof(rect_t));
    if (!rect) {
        return rect;
    }

    rect->top    = top;
    rect->left   = left;
    rect->bottom = bottom;
    rect->right  = right;

    return rect;
}

// Explode subject_rect into a list of contiguous rects which are
// not occluded by cutting_rect
//  ________                ____ ___
//|s    ___|____          |o   |o__|
//|____|___|   c|   --->  |____|
//      |________|
/**
 * @brief Split a rectangle into non-overlapping regions after subtracting another.
 *
 * Returns a list of rectangles representing the visible portions of @p subject_rect after
 * removing the area covered by @p cutting_rect.
 *
 * @param subject_rect Rectangle to be split (remains unchanged).
 * @param cutting_rect Rectangle occluding part of the subject.
 * @return List of allocated rectangles or nullptr on failure.
 */
list_t *rect_split(rect_t *subject_rect, rect_t *cutting_rect)
{
    list_t *output_rects = list_new();
    if (!output_rects) {
        return output_rects;
    }

    // We're going to modify the subject rect as we go,
    // so we'll clone it so as to not upset the object
    // we were passed
    rect_t subject_copy;
    subject_copy.top    = subject_rect->top;
    subject_copy.left   = subject_rect->left;
    subject_copy.bottom = subject_rect->bottom;
    subject_copy.right  = subject_rect->right;

    // We need a rectangle to hold new rectangles before
    // they get pushed into the output list
    rect_t *temp_rect;

    // Begin splitting
    // 1 -Split by left edge if that edge is between the subject's left and right edges
    if (cutting_rect->left > subject_copy.left && cutting_rect->left <= subject_copy.right) {

        // Try to make a new rectangle spanning from the subject rectangle's left and stopping before
        // the cutting rectangle's left
        temp_rect = rect_new(subject_copy.top, subject_copy.left, subject_copy.bottom, cutting_rect->left - 1);
        if (!temp_rect) {
            kfree(output_rects);

            return nullptr;
        }

        // Add the new rectangle to the output list
        list_add(output_rects, temp_rect);

        // Shrink the subject rectangle to exclude the split portion
        subject_copy.left = cutting_rect->left;
    }

    // 2 -Split by top edge if that edge is between the subject's top and bottom edges
    if (cutting_rect->top > subject_copy.top && cutting_rect->top <= subject_copy.bottom) {

        // Try to make a new rectangle spanning from the subject rectangle's top and stopping before
        // the cutting rectangle's top
        temp_rect = rect_new(subject_copy.top, subject_copy.left, cutting_rect->top - 1, subject_copy.right);
        if (!temp_rect) {
            // If the object creation failed, we need to delete the list and exit failed
            // This time, also delete any previously allocated rectangles
            for (; output_rects->count; temp_rect = list_remove_at(output_rects, 0))
                kfree(temp_rect);

            kfree(output_rects);

            return nullptr;
        }

        // Add the new rectangle to the output list
        list_add(output_rects, temp_rect);

        // Shrink the subject rectangle to exclude the split portion
        subject_copy.top = cutting_rect->top;
    }

    // 3 -Split by right edge if that edge is between the subject's left and right edges
    if (cutting_rect->right >= subject_copy.left && cutting_rect->right < subject_copy.right) {

        // Try to make a new rectangle spanning from the subject rectangle's right and stopping before
        // the cutting rectangle's right
        temp_rect = rect_new(subject_copy.top, cutting_rect->right + 1, subject_copy.bottom, subject_copy.right);
        if (!temp_rect) {
            for (; output_rects->count; temp_rect = list_remove_at(output_rects, 0))
                kfree(temp_rect);

            kfree(output_rects);

            return nullptr;
        }

        // Add the new rectangle to the output list
        list_add(output_rects, temp_rect);

        // Shrink the subject rectangle to exclude the split portion
        subject_copy.right = cutting_rect->right;
    }

    // 4 -Split by bottom edge if that edge is between the subject's top and bottom edges
    if (cutting_rect->bottom >= subject_copy.top && cutting_rect->bottom < subject_copy.bottom) {

        // Try to make a new rectangle spanning from the subject rectangle's bottom and stopping before
        // the cutting rectangle's bottom
        temp_rect = rect_new(cutting_rect->bottom + 1, subject_copy.left, subject_copy.bottom, subject_copy.right);
        if (!temp_rect) {
            for (; output_rects->count; temp_rect = list_remove_at(output_rects, 0))
                kfree(temp_rect);

            kfree(output_rects);

            return nullptr;
        }

        // Add the new rectangle to the output list
        list_add(output_rects, temp_rect);

        // Shrink the subject rectangle to exclude the split portion
        subject_copy.bottom = cutting_rect->bottom;
    }

    // Finally, after all that, we can return the output rectangles
    return output_rects;
}

/**
 * @brief Compute the intersection of two rectangles.
 *
 * @param rect_a First rectangle.
 * @param rect_b Second rectangle.
 * @return Newly allocated rectangle describing the overlap, or nullptr if they do not intersect.
 */
rect_t *rect_intersect(rect_t *rect_a, rect_t *rect_b)
{
    if (!(rect_a->left <= rect_b->right && rect_a->right >= rect_b->left && rect_a->top <= rect_b->bottom &&
          rect_a->bottom >= rect_b->top)) {
        return nullptr;
    }

    rect_t *result_rect = rect_new(rect_a->top, rect_a->left, rect_a->bottom, rect_a->right);
    if (!result_rect) {
        return nullptr;
    }

    if (rect_b->left > result_rect->left && rect_b->left <= result_rect->right) {
        result_rect->left = rect_b->left;
    }

    if (rect_b->top > result_rect->top && rect_b->top <= result_rect->bottom) {
        result_rect->top = rect_b->top;
    }

    if (rect_b->right >= result_rect->left && rect_b->right < result_rect->right) {
        result_rect->right = rect_b->right;
    }

    if (rect_b->bottom >= result_rect->top && rect_b->bottom < result_rect->bottom) {
        result_rect->bottom = rect_b->bottom;
    }

    return result_rect;
}
