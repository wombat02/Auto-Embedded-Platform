#ifndef UCP_LOG_H_
#define UCP_LOG_H_

/*
 * Compatibility shim. New code should include aep_log.h and use AEP_LOG*.
 * Logging is owned by the shared aep_log package (aep_port_log).
 */

#include "aep_log.h"

#ifndef UCP_DEBUG_VERBOSE
#define UCP_DEBUG_VERBOSE 0
#endif

/* Legacy names → aep_log */
#ifndef UCP_LOG_ENABLE
#define UCP_LOG_ENABLE AEP_LOG_ENABLE
#endif

#define UCP_LOG_LEVEL_DEBUG AEP_LOG_LEVEL_DEBUG
#define UCP_LOG_LEVEL_INFO AEP_LOG_LEVEL_INFO
#define UCP_LOG_LEVEL_WARN AEP_LOG_LEVEL_WARN
#define UCP_LOG_LEVEL_ERROR AEP_LOG_LEVEL_ERROR

#ifndef UCP_LOG_LEVEL
#define UCP_LOG_LEVEL AEP_LOG_LEVEL
#endif

typedef aep_log_level_t ucp_log_level_t;

#define UCP_LOG_DEBUG AEP_LOG_DEBUG
#define UCP_LOG_INFO AEP_LOG_INFO
#define UCP_LOG_WARN AEP_LOG_WARN
#define UCP_LOG_ERROR AEP_LOG_ERROR

#define UCP_LOGD(msg) AEP_LOGD(msg)
#define UCP_LOGI(msg) AEP_LOGI(msg)
#define UCP_LOGW(msg) AEP_LOGW(msg)
#define UCP_LOGE(msg) AEP_LOGE(msg)

#endif /* UCP_LOG_H_ */
