/*
 * Example overrides for ucp_port_* (time, USB send, app registry).
 * Portable ucp calls these weak symbols; aep_ucp_port.c defines them.
 * See examples/stm32_h5/GLUE.md and ucp/docs/PORTING.md.
 */
#ifndef AEP_UCP_PORT_H_
#define AEP_UCP_PORT_H_

#include <stdint.h>

/* ThreadX tick as milliseconds (TX_TIMER_TICKS_PER_SECOND == 1000). */
uint32_t aep_now_ms(void);

#endif /* AEP_UCP_PORT_H_ */
