/*
 * aep_fdcan_phy.c — STM32 FDCAN driver glue for portable vcom.
 *
 * Implements vcom_can_phy_t over hfdcan1/hfdcan2 and CAN*_STB GPIO.
 * Bit timing assumes FDCAN kernel clock = HSE 25 MHz.
 * Do not put FDCAN_HandleTypeDef into portable/ trees.
 *
 * See examples/stm32_h5/GLUE.md.
 */
#include "aep_fdcan_phy.h"


#include "main.h"
#include "stm32h5xx_hal.h"

/******************************************************************************/
/* 1. Includes / config — above                                               */
/******************************************************************************/

/******************************************************************************/
/* 2. Types and static state                                                  */
/******************************************************************************/

extern FDCAN_HandleTypeDef hfdcan1;
extern FDCAN_HandleTypeDef hfdcan2;

/******************************************************************************/
/* 3. HAL / HW helpers                                                        */
/******************************************************************************/

/* (bitrate/filter setup lives with the vtable ops below) */

/******************************************************************************/
/* 4. Portable vtable implementations (vcom_can_phy_t)                        */
/******************************************************************************/

static bool phy_set_standby(void *hw, bool standby)
{
	GPIO_TypeDef *port = (hw == &hfdcan1) ? CAN1_STB_GPIO_Port : CAN2_STB_GPIO_Port;
	uint16_t pin = (hw == &hfdcan1) ? CAN1_STB_Pin : CAN2_STB_Pin;
	HAL_GPIO_WritePin(port, pin, standby ? GPIO_PIN_SET : GPIO_PIN_RESET);
	return true;
}

static bool phy_set_bitrate(void *hw, uint32_t bit_s)
{
	FDCAN_HandleTypeDef *h = (FDCAN_HandleTypeDef *)hw;
	uint32_t presc = 5u;

	if (bit_s == 250000u)
	{
		presc = 10u;
	}
	else if (bit_s == 125000u)
	{
		presc = 20u;
	}
	else if (bit_s != 500000u)
	{
		return false;
	}

	(void)HAL_FDCAN_Stop(h);
	h->Init.NominalPrescaler = presc;
	h->Init.NominalTimeSeg1 = 7u;
	h->Init.NominalTimeSeg2 = 2u;
	h->Init.NominalSyncJumpWidth = 1u;
	h->Init.StdFiltersNbr = 1u;
	/* Keep retrying until ACK — one-shot hello must not leave the controller quiet. */
	h->Init.AutoRetransmission = ENABLE;
	return HAL_FDCAN_Init(h) == HAL_OK;
}

static bool phy_start(void *hw)
{
	FDCAN_HandleTypeDef *h = (FDCAN_HandleTypeDef *)hw;
	FDCAN_FilterTypeDef flt = {0};

	flt.IdType = FDCAN_STANDARD_ID;
	flt.FilterIndex = 0;
	flt.FilterType = FDCAN_FILTER_MASK;
	flt.FilterConfig = FDCAN_FILTER_TO_RXFIFO0;
	flt.FilterID1 = 0x000;
	flt.FilterID2 = 0x000;
	if (HAL_FDCAN_ConfigFilter(h, &flt) != HAL_OK)
	{
		return false;
	}
	(void)HAL_FDCAN_ConfigGlobalFilter(h, FDCAN_ACCEPT_IN_RX_FIFO0, FDCAN_ACCEPT_IN_RX_FIFO0,
									   FDCAN_FILTER_REMOTE, FDCAN_FILTER_REMOTE);
	return HAL_FDCAN_Start(h) == HAL_OK;
}

static void phy_stop(void *hw)
{
	(void)HAL_FDCAN_Stop((FDCAN_HandleTypeDef *)hw);
}

static bool phy_send(void *hw, const vcom_can_frame_t *fr)
{
	FDCAN_HandleTypeDef *h = (FDCAN_HandleTypeDef *)hw;
	FDCAN_TxHeaderTypeDef hdr = {0};
	static const uint32_t dlc_map[9] = {
		FDCAN_DLC_BYTES_0, FDCAN_DLC_BYTES_1, FDCAN_DLC_BYTES_2, FDCAN_DLC_BYTES_3,
		FDCAN_DLC_BYTES_4, FDCAN_DLC_BYTES_5, FDCAN_DLC_BYTES_6, FDCAN_DLC_BYTES_7,
		FDCAN_DLC_BYTES_8
	};

	if ((fr == NULL) || (fr->dlc > 8u))
	{
		return false;
	}
	hdr.Identifier = fr->id;
	hdr.IdType = (fr->ide != 0u) ? FDCAN_EXTENDED_ID : FDCAN_STANDARD_ID;
	hdr.TxFrameType = FDCAN_DATA_FRAME;
	hdr.DataLength = dlc_map[fr->dlc];
	hdr.ErrorStateIndicator = FDCAN_ESI_ACTIVE;
	hdr.BitRateSwitch = FDCAN_BRS_OFF;
	hdr.FDFormat = FDCAN_CLASSIC_CAN;
	hdr.TxEventFifoControl = FDCAN_NO_TX_EVENTS;
	hdr.MessageMarker = 0;
	return HAL_FDCAN_AddMessageToTxFifoQ(h, &hdr, (uint8_t *)fr->data) == HAL_OK;
}

static bool phy_rx_pop(void *hw, vcom_can_frame_t *fr)
{
	FDCAN_HandleTypeDef *h = (FDCAN_HandleTypeDef *)hw;
	FDCAN_RxHeaderTypeDef hdr;

	if (HAL_FDCAN_GetRxFifoFillLevel(h, FDCAN_RX_FIFO0) == 0u)
	{
		return false;
	}
	if (HAL_FDCAN_GetRxMessage(h, FDCAN_RX_FIFO0, &hdr, fr->data) != HAL_OK)
	{
		return false;
	}
	fr->id = hdr.Identifier;
	fr->ide = (hdr.IdType == FDCAN_EXTENDED_ID) ? 1u : 0u;
	switch (hdr.DataLength)
	{
	case FDCAN_DLC_BYTES_0: fr->dlc = 0; break;
	case FDCAN_DLC_BYTES_1: fr->dlc = 1; break;
	case FDCAN_DLC_BYTES_2: fr->dlc = 2; break;
	case FDCAN_DLC_BYTES_3: fr->dlc = 3; break;
	case FDCAN_DLC_BYTES_4: fr->dlc = 4; break;
	case FDCAN_DLC_BYTES_5: fr->dlc = 5; break;
	case FDCAN_DLC_BYTES_6: fr->dlc = 6; break;
	case FDCAN_DLC_BYTES_7: fr->dlc = 7; break;
	default: fr->dlc = 8; break;
	}
	return true;
}

/******************************************************************************/
/* 5. Module / demo app logic — n/a                                           */
/******************************************************************************/

/******************************************************************************/
/* 6. Glue init                                                               */
/******************************************************************************/

void aep_fdcan_phy_bind(vcom_can_phy_t *phy, FDCAN_HandleTypeDef *hw)
{
	if (phy == NULL)
	{
		return;
	}
	phy->hw = hw;
	phy->set_standby = phy_set_standby;
	phy->set_bitrate = phy_set_bitrate;
	phy->start = phy_start;
	phy->stop = phy_stop;
	phy->send = phy_send;
	phy->rx_pop = phy_rx_pop;
}

/******************************************************************************/
/* 7. Public API — aep_fdcan_phy_bind                                         */
/******************************************************************************/

