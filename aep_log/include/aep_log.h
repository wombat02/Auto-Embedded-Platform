#ifndef AEP_LOG_H_
#define AEP_LOG_H_

#include <stddef.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Portable log facade. AEP modules emit prebuilt messages; glue implements
 * aep_port_log. Compile out with AEP_LOG_ENABLE=0 or raise AEP_LOG_LEVEL.
 *
 * Default level is WARN: Info/Debug calls compile out unless overridden.
 */

#ifndef AEP_LOG_ENABLE
#define AEP_LOG_ENABLE 1
#endif

#define AEP_LOG_LEVEL_DEBUG 0
#define AEP_LOG_LEVEL_INFO 1
#define AEP_LOG_LEVEL_WARN 2
#define AEP_LOG_LEVEL_ERROR 3

#ifndef AEP_LOG_LEVEL
#define AEP_LOG_LEVEL AEP_LOG_LEVEL_WARN
#endif

typedef enum
{
	AEP_LOG_DEBUG = 0,
	AEP_LOG_INFO = 1,
	AEP_LOG_WARN = 2,
	AEP_LOG_ERROR = 3
} aep_log_level_t;

/* Glue implements this (strong). Weak stub is a no-op. */
void aep_port_log(aep_log_level_t level, const char *msg, size_t len);

#if (AEP_LOG_ENABLE) && (AEP_LOG_LEVEL <= AEP_LOG_LEVEL_DEBUG)
#define AEP_LOGD(msg)                                                          \
	do                                                                         \
	{                                                                          \
		const char *_aep_log_msg = (msg);                                      \
		aep_port_log(AEP_LOG_DEBUG, _aep_log_msg, strlen(_aep_log_msg));       \
	} while (0)
#else
#define AEP_LOGD(msg) ((void)0)
#endif

#if (AEP_LOG_ENABLE) && (AEP_LOG_LEVEL <= AEP_LOG_LEVEL_INFO)
#define AEP_LOGI(msg)                                                          \
	do                                                                         \
	{                                                                          \
		const char *_aep_log_msg = (msg);                                      \
		aep_port_log(AEP_LOG_INFO, _aep_log_msg, strlen(_aep_log_msg));        \
	} while (0)
#else
#define AEP_LOGI(msg) ((void)0)
#endif

#if (AEP_LOG_ENABLE) && (AEP_LOG_LEVEL <= AEP_LOG_LEVEL_WARN)
#define AEP_LOGW(msg)                                                          \
	do                                                                         \
	{                                                                          \
		const char *_aep_log_msg = (msg);                                      \
		aep_port_log(AEP_LOG_WARN, _aep_log_msg, strlen(_aep_log_msg));        \
	} while (0)
#else
#define AEP_LOGW(msg) ((void)0)
#endif

#if (AEP_LOG_ENABLE) && (AEP_LOG_LEVEL <= AEP_LOG_LEVEL_ERROR)
#define AEP_LOGE(msg)                                                          \
	do                                                                         \
	{                                                                          \
		const char *_aep_log_msg = (msg);                                      \
		aep_port_log(AEP_LOG_ERROR, _aep_log_msg, strlen(_aep_log_msg));       \
	} while (0)
#else
#define AEP_LOGE(msg) ((void)0)
#endif

#ifdef __cplusplus
}
#endif

#endif /* AEP_LOG_H_ */
