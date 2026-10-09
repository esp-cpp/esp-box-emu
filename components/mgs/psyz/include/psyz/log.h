#ifndef PSYZ_LOG_H
#define PSYZ_LOG_H

typedef enum {
    LOG_LEVEL_D,
    LOG_LEVEL_I,
    LOG_LEVEL_W,
    LOG_LEVEL_E,
} LOG_LEVEL;

#define NAMEOF(var) #var
#define NOT_IMPLEMENTED LOG_ONCE("not implemented")

#ifndef NO_LOGS

// current logging level
// set to LOG_LEVEL_D to enable all logging
// set to LOG_LEVEL_E+1 to disable all logging
extern LOG_LEVEL psyz_logLevel;

#define LOG_ONCE(...)                                                          \
    do {                                                                       \
        static int was_logged = 0;                                             \
        if (LOG_LEVEL_D >= psyz_logLevel && !was_logged) {                     \
            was_logged = 1;                                                    \
            psyz_log(LOG_LEVEL_D, __FILE__, __LINE__, __func__, __VA_ARGS__);  \
        }                                                                      \
    } while (0)
#define DEBUGF(...)                                                            \
    do {                                                                       \
        if (LOG_LEVEL_D >= psyz_logLevel)                                      \
            psyz_log(LOG_LEVEL_D, __FILE__, __LINE__, __func__, __VA_ARGS__);  \
    } while (0)
#define INFOF(...)                                                             \
    do {                                                                       \
        if (LOG_LEVEL_I >= psyz_logLevel)                                      \
            psyz_log(LOG_LEVEL_I, __FILE__, __LINE__, __func__, __VA_ARGS__);  \
    } while (0)
#define WARNF(...)                                                             \
    do {                                                                       \
        if (LOG_LEVEL_W >= psyz_logLevel)                                      \
            psyz_log(LOG_LEVEL_W, __FILE__, __LINE__, __func__, __VA_ARGS__);  \
    } while (0)
#define ERRORF(...)                                                            \
    do {                                                                       \
        if (LOG_LEVEL_E >= psyz_logLevel)                                      \
            psyz_log(LOG_LEVEL_E, __FILE__, __LINE__, __func__, __VA_ARGS__);  \
    } while (0)

#if defined(__GNUC__) || defined(__clang__)
__attribute__((format(printf, 5, 6)))
#endif
void psyz_log(unsigned int level, const char* file, unsigned int line,
              const char* func, const char* fmt, ...);

#else

#define DEBUGF(...) ((void)0)
#define INFOF(...) ((void)0)
#define WARNF(...) ((void)0)
#define ERRORF(...) ((void)0)

#endif

#endif
