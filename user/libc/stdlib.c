#include <ctype.h>
#include <errno.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "status.h"

#define MAX_ENV_VARS 32

static char *env_vars[MAX_ENV_VARS];

static int env_name_matches(const char *entry, const char *name, size_t len)
{
    return entry != nullptr && strncmp(entry, name, len) == 0 && entry[len] == '=';
}

char *getenv(const char *name)
{
    if (name == nullptr || *name == '\0') {
        return nullptr;
    }
    size_t len = strlen(name);
    for (int i = 0; i < MAX_ENV_VARS; ++i) {
        if (env_name_matches(env_vars[i], name, len)) {
            return env_vars[i] + len + 1;
        }
    }
    return nullptr;
}

int putenv(char *string)
{
    if (string == nullptr) {
        errno = -EINVARG;
        return -1;
    }
    char *sep = strchr(string, '=');
    if (sep == nullptr) {
        errno = -EINVARG;
        return -1;
    }
    size_t len = (size_t)(sep - string);
    for (int i = 0; i < MAX_ENV_VARS; ++i) {
        if (env_name_matches(env_vars[i], string, len)) {
            env_vars[i] = string;
            return 0;
        }
    }
    for (int i = 0; i < MAX_ENV_VARS; ++i) {
        if (env_vars[i] == nullptr) {
            env_vars[i] = string;
            return 0;
        }
    }
    errno = -ENOMEM;
    return -1;
}

int system(const char *command)
{
    (void)command;
    errno = -ENOTSUP;
    return -1;
}

static double pow10(int exponent)
{
    double result = 1.0;
    if (exponent >= 0) {
        for (int i = 0; i < exponent; ++i) {
            result *= 10.0;
        }
    } else {
        for (int i = 0; i < -exponent; ++i) {
            result /= 10.0;
        }
    }
    return result;
}

double atof(const char *str)
{
    if (str == nullptr) {
        return 0.0;
    }

    while (isspace((unsigned char)*str)) {
        str++;
    }

    int sign = 1;
    if (*str == '-') {
        sign = -1;
        str++;
    } else if (*str == '+') {
        str++;
    }

    double value = 0.0;
    while (isdigit((unsigned char)*str)) {
        value = value * 10.0 + (*str - '0');
        str++;
    }

    if (*str == '.') {
        str++;
        double factor = 0.1;
        while (isdigit((unsigned char)*str)) {
            value += factor * (*str - '0');
            factor *= 0.1;
            str++;
        }
    }

    if (*str == 'e' || *str == 'E') {
        str++;
        int exp_sign = 1;
        if (*str == '-') {
            exp_sign = -1;
            str++;
        } else if (*str == '+') {
            str++;
        }
        int exponent = 0;
        while (isdigit((unsigned char)*str)) {
            exponent = exponent * 10 + (*str - '0');
            str++;
        }
        value *= pow10(exp_sign * exponent);
    }

    return sign * value;
}
