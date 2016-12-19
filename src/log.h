#ifndef __LOG_H__
#define __LOG_H__

#include <syslog.h>

typedef enum {
	LOG_LEVEL_ALL,
	LOG_LEVEL_DEBUG,
	LOG_LEVEL_INFO,
	LOG_LEVEL_WARN,
	LOG_LEVEL_ERROR,
	LOG_LEVEL_FATAL,
	LOG_LEVEL_OFF,
} LOG_LEVEL;

/*
 * Priority defined in syslog.h
 *
 * Reuse syslog priority 1~7 as log level and define 0 as LOG_OFF
 */
// #define	LOG_EMERG	0	/* system is unusable */
// #define	LOG_ALERT	1	/* action must be taken immediately */
// #define	LOG_CRIT	2	/* critical conditions */
// #define	LOG_ERR		3	/* error conditions */
// #define	LOG_WARNING	4	/* warning conditions */
// #define	LOG_NOTICE	5	/* normal but significant condition */
// #define	LOG_INFO	6	/* informational */
// #define	LOG_DEBUG	7	/* debug-level messages */

#define LOG_OFF     0

#ifndef LOG_ALERT
#define	LOG_ALERT	1
#endif
#ifndef LOG_CRIT
#define	LOG_CRIT	2
#endif
#ifndef LOG_ERR
#define	LOG_ERR		3
#endif
#ifndef LOG_WARNING
#define	LOG_WARNING	4
#endif
#ifndef LOG_NOTICE
#define	LOG_NOTICE	5
#endif
#ifndef LOG_INFO
#define	LOG_INFO	6
#endif
#ifndef LOG_DEBUG
#define	LOG_DEBUG	7
#endif

#define LOG_CHANNEL_CONSOLE		(1 << 0)
#define LOG_CHANNEL_SYSLOG		(1 << 1)
#define LOG_CHANNEL_FILE		(1 << 2)
#define LOG_CHANNEL_REMOTE		(1 << 3)

#define LOG_SYMBOL_ALERT        "A"
#define LOG_SYMBOL_CRIT         "C"
#define LOG_SYMBOL_ERR          "E"
#define LOG_SYMBOL_WARN         "W"
#define LOG_SYMBOL_NOTICE       "N"
#define LOG_SYMBOL_INFO         "I"
#define LOG_SYMBOL_DEBUG        "D"

#define log_alert(fmt, arg...)         log_output(LOG_ALERT  , LOG_SYMBOL_ALERT  " " fmt , ## arg)
#define log_crit(fmt, arg...)          log_output(LOG_CRIT   , LOG_SYMBOL_CRIT   " " fmt , ## arg)
#define log_err(fmt, arg...)           log_output(LOG_ERR    , LOG_SYMBOL_ERR    " " fmt , ## arg)
#define log_warn(fmt, arg...)          log_output(LOG_WARNING, LOG_SYMBOL_WARN   " " fmt , ## arg)
#define log_notice(fmt, arg...)        log_output(LOG_NOTICE , LOG_SYMBOL_NOTICE " " fmt , ## arg)
#define log_info(fmt, arg...)          log_output(LOG_INFO   , LOG_SYMBOL_INFO   " " fmt , ## arg)
#define log_debug(fmt, arg...)         log_output(LOG_DEBUG  , LOG_SYMBOL_DEBUG  " " fmt , ## arg)

#define log(fmt, arg...)               log_output(LOG_NOTICE , LOG_SYMBOL_NOTICE " " fmt , ## arg)


/**
 * Log Level:
 * - VERBOSE
 * - INFO
 * - DEBUG
 * - WARN
 * - ERROR
 *
 * Priority increase from VERBOSE to ERROR<br>
 * Lower priority log level will include higher priority log<br>
 */

#define LOG_LEVEL_DEBUG

/* WARN/ERROR log default ON */
#ifndef LOG_LEVEL_WARN
#define LOG_LEVEL_WARN
#endif
#ifndef LOG_LEVEL_ERROR
#define LOG_LEVEL_ERROR
#endif

#if   defined LOG_LEVEL_NONE    // NONE
#undef  LOG_LEVEL_VERBOSE
#undef  LOG_LEVEL_INFO
#undef  LOG_LEVEL_DEBUG
#undef  LOG_LEVEL_WARN
#undef  LOG_LEVEL_ERROR
#elif defined LOG_LEVEL_VERBOSE // VERBOSE
#define LOG_LEVEL_INFO
#define LOG_LEVEL_DEBUG
#elif defined LOG_LEVEL_INFO    // INFO
#undef  LOG_LEVEL_VERBOSE
#define LOG_LEVEL_DEBUG
#elif defined LOG_LEVEL_DEBUG   // DEBUG
#undef  LOG_LEVEL_VERBOSE
#undef  LOG_LEVEL_INFO
#elif defined LOG_LEVEL_WARN    // WARN
#undef  LOG_LEVEL_VERBOSE
#undef  LOG_LEVEL_INFO
#undef  LOG_LEVEL_DEBUG
#elif defined LOG_LEVEL_ERROR   // ERROR
#undef  LOG_LEVEL_VERBOSE
#undef  LOG_LEVEL_INFO
#undef  LOG_LEVEL_DEBUG
#undef  LOG_LEVEL_WARN
#else                           // DEFAULT
#undef  LOG_LEVEL_VERBOSE
#undef  LOG_LEVEL_INFO
#undef  LOG_LEVEL_DEBUG
#endif

#define LOG_LEVEL_SYMBOL_VERBOSE        "V"
#define LOG_LEVEL_SYMBOL_INFO           "I"
#define LOG_LEVEL_SYMBOL_DEBUG          "D"
#define LOG_LEVEL_SYMBOL_WARN           "W"
#define LOG_LEVEL_SYMBOL_ERROR          "E"

#ifdef LOG_LEVEL_NONE
#define LOG(fmt, arg...)
#else
#define LOG(fmt, arg...)                log_output(fmt, ##arg)
#endif

#ifdef LOG_LEVEL_VERBOSE
#define LOGV(tag, fmt, arg...)          log_output(LOG_NOTICE   , LOG_LEVEL_SYMBOL_VERBOSE "\t%s\t" fmt, tag, ##arg)
#else
#define LOGV(tag, fmt, arg...)
#endif

#ifdef LOG_LEVEL_INFO
#define LOGI(tag, fmt, arg...)          log_output(LOG_INFO     , LOG_LEVEL_SYMBOL_INFO    "\t%s\t" fmt, tag, ## arg)
#else
#define LOGI(tag, fmt, arg...)
#endif

#ifdef LOG_LEVEL_DEBUG
#define LOGD(tag, fmt, arg...)          log_output(LOG_DEBUG    , LOG_LEVEL_SYMBOL_DEBUG   "\t%s\t" fmt, tag, ## arg)
#else
#define LOGD(tag, fmt, arg...)
#endif

#ifdef LOG_LEVEL_WARN
#define LOGW(tag, fmt, arg...)          log_output(LOG_WARNING  , LOG_LEVEL_SYMBOL_WARN    "\t%s\t" fmt, tag, ## arg)
#else
#define LOGW(tag, fmt, arg...)
#endif

#ifdef LOG_LEVEL_ERROR
#define LOGE(tag, fmt, arg...)          log_output(LOG_ERR      , LOG_LEVEL_SYMBOL_ERROR   "\t%s\t" fmt, tag, ## arg)
#else
#define LOGE(tag, fmt, arg...)
#endif

/*
 * Another API for disable log in code level.
 * It will not output log in trace.
 */
#define __LOG(fmt, arg...)
#define __LOGV(tag, fmt, arg...)
#define __LOGI(tag, fmt, arg...)
#define __LOGD(tag, fmt, arg...)
#define __LOGW(tag, fmt, arg...)
#define __LOGE(tag, fmt, arg...)

/**
 * Another API for quick debug
 * It will output line and function name
 */
#define D()                             printf("* L%d, %s\r\n", __LINE__, __FUNCTION__)
#define DD(fmt, arg...)                 printf("* L%d, %s: " fmt, __LINE__, __FUNCTION__, ##arg)
#define DDD(fmt, arg...)                printf("* L%d: " fmt, __LINE__, ##arg)

/* Public API for log */
void log_output(int level, const char * fmt, ...);

void log_set_level(int level);
void log_set_channel(int channel);
void log_open(const char *ident, int channels);
void log_close(void);

#endif /* __LOG_H__ */

/* vim: set ts=4 sw=4 tw=0 noexpandtab nolist : */
