#ifndef VCOM_CORE_H_
#define VCOM_CORE_H_

#include "vcom_types.h"

#ifdef __cplusplus
extern "C" {
#endif

#define VCOM_TX_ONCE_DEPTH 8u /* per bus; 16 total across VCOM_BUS_COUNT */
#define VCOM_TX_PERIODIC_SLOTS 24u
#define VCOM_RX_SUBSCRIBERS 12u

typedef void (*vcom_tx_fill_fn)(vcom_can_frame_t *fr, void *user);
typedef void (*vcom_rx_fn)(const vcom_can_frame_t *fr, void *user);

typedef struct {
	uint8_t used;
	vcom_can_frame_t fr;
} vcom_tx_once_t;

typedef struct {
	uint8_t used;
	uint8_t bus;
	uint32_t period_ms;
	uint32_t next_ms;
	vcom_can_frame_t tmpl;
	vcom_tx_fill_fn fill;
	void *user;
} vcom_tx_periodic_t;

typedef struct {
	uint8_t used;
	uint8_t bus;
	uint32_t id;
	uint32_t id_mask;
	vcom_rx_fn fn;
	void *user;
} vcom_rx_sub_t;

typedef struct {
	vcom_tx_once_t q[VCOM_TX_ONCE_DEPTH];
	uint8_t head;
	uint8_t tail;
	uint8_t count;
} vcom_tx_once_q_t;

typedef struct vcom_core {
	const vcom_time_t *time;
	const vcom_can_phy_t *can_phy[VCOM_BUS_COUNT];
	const vcom_kline_phy_t *kline_phy;
	uint8_t can_started[VCOM_BUS_COUNT];
	uint8_t kline_started;

	vcom_tx_once_q_t once[VCOM_BUS_COUNT];
	vcom_tx_periodic_t periodic[VCOM_TX_PERIODIC_SLOTS];
	vcom_rx_sub_t rx_sub[VCOM_RX_SUBSCRIBERS];
} vcom_core_t;

void vcom_port_lock(void);
void vcom_port_unlock(void);

void vcom_core_init(vcom_core_t *c, const vcom_time_t *time);
void vcom_core_set_can_phy(vcom_core_t *c, uint8_t bus, const vcom_can_phy_t *phy);
void vcom_core_set_kline_phy(vcom_core_t *c, const vcom_kline_phy_t *phy);

bool vcom_core_start_bus(vcom_core_t *c, uint8_t bus, uint32_t bitrate);
void vcom_core_stop_bus(vcom_core_t *c, uint8_t bus);
void vcom_core_start_kline(vcom_core_t *c);
void vcom_core_stop_kline(vcom_core_t *c);

bool vcom_tx_post(vcom_core_t *c, uint8_t bus, const vcom_can_frame_t *fr);
bool vcom_tx_post_local(vcom_core_t *c, uint8_t bus, const vcom_can_frame_t *fr);
int vcom_tx_periodic(vcom_core_t *c, uint8_t bus, uint32_t period_ms,
					 const vcom_can_frame_t *tmpl,
					 vcom_tx_fill_fn fill, void *user);
void vcom_tx_cancel(vcom_core_t *c, int slot);
void vcom_tx_cancel_bus(vcom_core_t *c, uint8_t bus);

bool vcom_rx_subscribe(vcom_core_t *c, uint8_t bus,
					   uint32_t id, uint32_t id_mask,
					   vcom_rx_fn fn, void *user);
void vcom_rx_unsubscribe_user(vcom_core_t *c, void *user);

void vcom_core_drain_rx(vcom_core_t *c, uint8_t bus);
void vcom_core_tick(vcom_core_t *c, uint32_t now_ms);

#ifdef __cplusplus
}
#endif

#endif /* VCOM_CORE_H_ */
