#pragma once

#include <list.h>

typedef struct rect {
    int top;
    int left;
    int bottom;
    int right;

} rect_t;

rect_t *rect_create(int top, int left, int bottom, int right);
list_t *rect_split(rect_t *subject_rect, rect_t *cutting_rect);
