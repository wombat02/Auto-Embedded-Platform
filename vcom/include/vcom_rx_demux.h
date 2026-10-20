/*
 * Fine-grained CAN RX demux: one wide vcom_rx_subscribe per bus → id handlers.
 * Avoids exhausting VCOM_RX_SUBSCRIBERS when many components need exact IDs.
 */
#ifndef VCOM_RX_DEMUX_H_
#define VCOM_RX_DEMUX_H_

#include "vcom_core.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef VCOM_RX_DEMUX_HANDLERS
#define VCOM_RX_DEMUX_HANDLERS 32u
#endif

typedef void (*vcom_rx_demux_fn)(uint8_t bus, const vcom_can_frame_t *fr, void *user);

typedef struct {
	uint8_t used;
	uint8_t bus;
	uint32_t id;
	uint32_t id_mask;
	vcom_rx_demux_fn fn;
	void *user;
} vcom_rx_demux_entry_t;

typedef struct {
	vcom_core_t *core;
	vcom_rx_demux_entry_t entry[VCOM_RX_DEMUX_HANDLERS];
	uint8_t bus_attached[VCOM_BUS_COUNT];
} vcom_rx_demux_t;

void vcom_rx_demux_init(vcom_rx_demux_t *d, vcom_core_t *core);

/* Subscribe once per bus with mask 0 (match all); safe to call repeatedly. */
bool vcom_rx_demux_attach_bus(vcom_rx_demux_t *d, uint8_t bus);

int vcom_rx_demux_register(vcom_rx_demux_t *d, uint8_t bus, uint32_t id, uint32_t id_mask,
						   vcom_rx_demux_fn fn, void *user);
void vcom_rx_demux_unregister_user(vcom_rx_demux_t *d, void *user);

#ifdef __cplusplus
}
#endif

#endif /* VCOM_RX_DEMUX_H_ */
