/*
 * aep_kline_phy.c — USART2 K-Line PHY glue for portable vcom.
 *
 * Pins (example board): PD5 TX, PD6 RX, PD3 LIN_NSLP (awake high).
 * Owns USART2_IRQHandler and HAL_UART_RxCpltCallback for this UART.
 *
 * See examples/stm32_h5/GLUE.md.
 */
#include "aep_kline_phy.h"


#include "main.h"
#include "stm32h5xx_hal.h"

/******************************************************************************/
/* 1. Includes / config — above                                               */
/******************************************************************************/

/******************************************************************************/
/* 2. Types and static state                                                  */
/******************************************************************************/

static UART_HandleTypeDef s_huart2;
static uint8_t s_kline_rx_byte;
static volatile uint8_t s_kline_have_rx;

/******************************************************************************/
/* 3. HAL / HW helpers                                                        */
/******************************************************************************/

void USART2_IRQHandler(void)
{
	HAL_UART_IRQHandler(&s_huart2);
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
	if (huart == &s_huart2)
	{
		s_kline_have_rx = 1u;
	}
}

static void usart2_init(void)
{
	GPIO_InitTypeDef gpio = {0};
	RCC_PeriphCLKInitTypeDef pclk = {0};

	pclk.PeriphClockSelection = RCC_PERIPHCLK_USART2;
	pclk.Usart2ClockSelection = RCC_USART2CLKSOURCE_PCLK1;
	(void)HAL_RCCEx_PeriphCLKConfig(&pclk);
	__HAL_RCC_USART2_CLK_ENABLE();
	__HAL_RCC_GPIOD_CLK_ENABLE();

	gpio.Pin = GPIO_PIN_5 | GPIO_PIN_6;
	gpio.Mode = GPIO_MODE_AF_PP;
	gpio.Pull = GPIO_PULLUP;
	gpio.Speed = GPIO_SPEED_FREQ_LOW;
	gpio.Alternate = GPIO_AF7_USART2;
	HAL_GPIO_Init(GPIOD, &gpio);

	s_huart2.Instance = USART2;
	s_huart2.Init.BaudRate = 10400;
	s_huart2.Init.WordLength = UART_WORDLENGTH_8B;
	s_huart2.Init.StopBits = UART_STOPBITS_1;
	s_huart2.Init.Parity = UART_PARITY_NONE;
	s_huart2.Init.Mode = UART_MODE_TX_RX;
	s_huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
	s_huart2.Init.OverSampling = UART_OVERSAMPLING_16;
	s_huart2.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
	s_huart2.Init.ClockPrescaler = UART_PRESCALER_DIV1;
	s_huart2.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
	(void)HAL_UART_Init(&s_huart2);
	HAL_NVIC_SetPriority(USART2_IRQn, 5, 0);
	HAL_NVIC_EnableIRQ(USART2_IRQn);
}

/******************************************************************************/
/* 4. Portable vtable implementations (vcom_kline_phy_t)                      */
/******************************************************************************/

static void kline_set_awake(void *hw, bool awake)
{
	(void)hw;
	HAL_GPIO_WritePin(LIN_NSLP_GPIO_Port, LIN_NSLP_Pin, awake ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

static bool kline_start(void *hw)
{
	UART_HandleTypeDef *hu = (UART_HandleTypeDef *)hw;
	kline_set_awake(hw, true);
	s_kline_have_rx = 0u;
	return HAL_UART_Receive_IT(hu, &s_kline_rx_byte, 1) == HAL_OK;
}

static void kline_stop(void *hw)
{
	(void)HAL_UART_AbortReceive((UART_HandleTypeDef *)hw);
	kline_set_awake(hw, false);
}

static bool kline_send(void *hw, const uint8_t *data, uint8_t len)
{
	return HAL_UART_Transmit((UART_HandleTypeDef *)hw, (uint8_t *)data, len, 50) == HAL_OK;
}

static bool kline_rx_pop(void *hw, uint8_t *byte)
{
	(void)hw;
	if (s_kline_have_rx == 0u)
	{
		return false;
	}
	*byte = s_kline_rx_byte;
	s_kline_have_rx = 0u;
	(void)HAL_UART_Receive_IT(&s_huart2, &s_kline_rx_byte, 1);
	return true;
}

/******************************************************************************/
/* 5. Module / demo app logic — n/a                                           */
/******************************************************************************/

/******************************************************************************/
/* 6. Glue init                                                               */
/******************************************************************************/

void aep_kline_phy_init(void)
{
	usart2_init();
}

void aep_kline_phy_bind(vcom_kline_phy_t *phy)
{
	if (phy == NULL)
	{
		return;
	}
	phy->hw = &s_huart2;
	phy->set_awake = kline_set_awake;
	phy->start = kline_start;
	phy->stop = kline_stop;
	phy->send = kline_send;
	phy->rx_pop = kline_rx_pop;
}

/******************************************************************************/
/* 7. Public API — aep_kline_phy_init / aep_kline_phy_bind                    */
/******************************************************************************/

