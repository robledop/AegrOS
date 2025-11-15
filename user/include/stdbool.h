#pragma once

#ifndef __cplusplus
#ifndef __bool_true_false_are_defined
typedef _Bool bool;
#define true 1
#define false 0
#define __bool_true_false_are_defined 1
#endif
#else
#define __bool_true_false_are_defined 1
#endif
