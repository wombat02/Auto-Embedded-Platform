/*
 * STM32 FDCAN HAL -> vcom_can_phy_t.
 * Portable vcom must not see FDCAN_HandleTypeDef; this file is example-only glue.
 * See examples/stm32_h5/GLUE.md and BRINGUP.md.
 */
#ifndef AEP_FDCAN_PHY_H_
#define AEP_FDCAN_PHY_H_

#include "vcom_types.h"
#include "fdcan.h"

void aep_fdcan_phy_bind(vcom_can_phy_t *phy, FDCAN_HandleTypeDef *hw);


#endif /* AEP_FDCAN_PHY_H_ */
