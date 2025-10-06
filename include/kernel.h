#pragma once

// number of elements in a fixed-size array
#define NELEM(x) (sizeof(x)/sizeof((x)[0]))

void panic(const char *msg);
