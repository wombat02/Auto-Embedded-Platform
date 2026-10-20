/*
 * USART2 + LIN_NSLP GPIO -> vcom_kline_phy_t.
 * Owns USART2 IRQ and HAL UART RX callback for the example board.
 * See examples/stm32_h5/GLUE.md and BRINGUP.md.
 */
#ifndef AEP_KLINE_PHY_H_
#define AEP_KLINE_PHY_H_

#include "vcom_types.h"

void aep_kline_phy_init(void);
void aep_kline_phy_bind(vcom_kline_phy_t *phy);


#endif /* AEP_KLINE_PHY_H_ */
