/*
 * USB CDC ↔ ucp_session. Poll runs on the USBX device app thread.
 * See examples/stm32_h5/GLUE.md, BRINGUP.md, and ucp/docs/PORTING.md.
 */
#ifndef AEP_UCP_USB_H_
#define AEP_UCP_USB_H_

#include <stdbool.h>
#include <stdint.h>

/* 0 = session-only on CDC. 1 = ucp_tlm/apps_tick on the USB poll. */
#ifndef AEP_UCP_TLM_ON_USB
#define AEP_UCP_TLM_ON_USB 1
#endif

void aep_ucp_usb_init(void);
void aep_ucp_usb_poll(void);
bool aep_ucp_usb_ready(void);
/* True once host DTR — CDC must not carry ASCII logs (races forever OUT RX). */
bool aep_ucp_usb_owns_cdc(void);
bool aep_ucp_usb_send_frame(const uint8_t *frame, uint16_t len);
void aep_ucp_usb_log_stacks(void);

#endif /* AEP_UCP_USB_H_ */
