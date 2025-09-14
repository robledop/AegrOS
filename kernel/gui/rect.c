#include <kernel_heap.h>
#include <rect.h>

rect_t *rect_create(int top, int left, int bottom, int right)
{
    auto rect = (rect_t *)kzalloc(sizeof(rect_t));
    if (rect == nullptr) {
        return nullptr;
    }

    rect->top    = top;
    rect->left   = left;
    rect->bottom = bottom;
    rect->right  = right;

    return rect;
}

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
        temp_rect = rect_create(subject_copy.top, subject_copy.left, subject_copy.bottom, cutting_rect->left - 1);
        if (!temp_rect) {
            // If the object creation failed, we need to delete the list and exit failed
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
        temp_rect = rect_create(subject_copy.top, subject_copy.left, cutting_rect->top - 1, subject_copy.right);
        if (!temp_rect) {

            // If the object creation failed, we need to delete the list and exit failed
            // This time, also delete any previously allocated rectangles
            while (output_rects->count) {

                temp_rect = list_remove_at(output_rects, 0);
                kfree(temp_rect);
            }

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
        temp_rect = rect_create(subject_copy.top, cutting_rect->right + 1, subject_copy.bottom, subject_copy.right);
        if (!temp_rect) {
            // Free on fail
            while (output_rects->count) {

                temp_rect = list_remove_at(output_rects, 0);
                kfree(temp_rect);
            }

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
        temp_rect = rect_create(cutting_rect->bottom + 1, subject_copy.left, subject_copy.bottom, subject_copy.right);
        if (!temp_rect) {

            // Free on fail
            while (output_rects->count) {

                temp_rect = list_remove_at(output_rects, 0);
                kfree(temp_rect);
            }

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
