#ifndef VCOM_TYPES_H_
#define VCOM_TYPES_H_

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
	uint32_t id;
	uint8_t ide; /* 0 standard, 1 extended */
	uint8_t dlc; /* 0..8 */
	uint8_t data[8];
} vcom_can_frame_t;

enum {
	VCOM_BUS_CAN0 = 0,
	VCOM_BUS_CAN1 = 1,
	VCOM_BUS_COUNT = 2
};

typedef struct {
	void *hw;
	bool (*set_standby)(void *hw, bool standby);
	bool (*set_bitrate)(void *hw, uint32_t bit_s);
	bool (*start)(void *hw);
	void (*stop)(void *hw);
	bool (*send)(void *hw, const vcom_can_frame_t *fr);
	bool (*rx_pop)(void *hw, vcom_can_frame_t *fr);
} vcom_can_phy_t;

typedef struct {
	void *hw;
	void (*set_awake)(void *hw, bool awake);
	bool (*start)(void *hw);
	void (*stop)(void *hw);
	bool (*send)(void *hw, const uint8_t *data, uint8_t len);
	bool (*rx_pop)(void *hw, uint8_t *byte);
} vcom_kline_phy_t;

typedef struct {
	uint32_t (*now_ms)(void);
} vcom_time_t;

#ifdef __cplusplus
}
#endif

#endif /* VCOM_TYPES_H_ */
