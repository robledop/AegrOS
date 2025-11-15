#pragma once

#include <types.h>

#define time_t long long int

struct tm
{
    int tm_sec;   // seconds after the minute - [0, 60] including leap second
    int tm_min;   // minutes after the hour - [0, 59]
    int tm_hour;  // hours since midnight - [0, 23]
    int tm_mday;  // day of the month - [1, 31]
    int tm_mon;   // months since January - [0, 11]
    int tm_year;  // years since 1900
    int tm_wday;  // days since Sunday - [0, 6]
    int tm_yday;  // days since January 1 - [0, 365]
    int tm_isdst; // daylight saving time flag
};

time_t mktime(const struct tm *tm);
void unix_timestamp_to_tm(time_t timestamp, struct tm *tm);
int days_in_month(int year, int month);
int is_leap_year(int year);
u32 strftime(const char *format, const struct tm *tm, char *buffer, u32 max);
time_t time(time_t *timer);
struct tm *localtime(const time_t *timer);