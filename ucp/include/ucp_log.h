#ifndef UCP_LOG_H_
#define UCP_LOG_H_

#include <stdint.h>

#ifndef UCP_LOG_ENABLE
#define UCP_LOG_ENABLE 1
#endif

#define UCP_LOG_LEVEL_DEBUG 0
#define UCP_LOG_LEVEL_INFO 1

#ifndef UCP_DEBUG_VERBOSE
#define UCP_DEBUG_VERBOSE 0
#endif

#ifndef UCP_LOG_LEVEL
#define UCP_LOG_LEVEL UCP_LOG_LEVEL_INFO
#endif

typedef enum
{
	UCP_LOG_DEBUG = 0,
	UCP_LOG_INFO = 1
} ucp_log_level_t;

void ucp_port_log(ucp_log_level_t level, const char *msg);

#if (UCP_LOG_ENABLE) && (UCP_LOG_LEVEL <= UCP_LOG_LEVEL_DEBUG)
#define UCP_LOGD(msg) ucp_port_log(UCP_LOG_DEBUG, (msg))
#else
#define UCP_LOGD(msg) ((void)0)
#endif

#if (UCP_LOG_ENABLE) && (UCP_LOG_LEVEL <= UCP_LOG_LEVEL_INFO)
#define UCP_LOGI(msg) ucp_port_log(UCP_LOG_INFO, (msg))
#else
#define UCP_LOGI(msg) ((void)0)
#endif

#endif /* UCP_LOG_H_ */
