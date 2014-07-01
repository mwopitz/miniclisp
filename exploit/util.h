#ifndef UTIL_H
#define UTIL_H

#define print_err(fmt, ...) \
        do { fprintf(stderr, "[%s:%d] Error. " fmt, __func__, \
                        __LINE__, __VA_ARGS__); } while (0)

#define print_warn(fmt, ...) \
        do { fprintf(stderr, "[%s:%d] Warning. " fmt, __func__, \
                        __LINE__, __VA_ARGS__); } while (0)

#define debug_info(fmt, ...) \
        do { if (DEBUG) fprintf(stdout, "[%s:%d] " fmt, __func__, \
                        __LINE__, __VA_ARGS__); } while (0)

#define debug_err(fmt, ...) \
        do { if (DEBUG) fprintf(stderr, "[%s:%d] " fmt, __func__, \
                        __LINE__, __VA_ARGS__); } while (0)

#endif
