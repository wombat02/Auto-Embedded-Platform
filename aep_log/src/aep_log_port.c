#include "aep_log.h"

#if defined(__GNUC__) || defined(__clang__)
#define AEP_LOG_WEAK __attribute__((weak))
#else
#define AEP_LOG_WEAK
#endif

AEP_LOG_WEAK void aep_port_log(aep_log_level_t level, const char *msg, size_t len)
{
	(void)level;
	(void)msg;
	(void)len;
}
