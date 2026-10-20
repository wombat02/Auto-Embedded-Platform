# STM32H523 example bring-up

This project is an illustration of how glue wires the portable libraries. It is not a clone-and-build. CubeMX generates HAL, CMSIS, ThreadX, USBX, startup, and the `USER CODE` skeletons. This repo publishes `aep_*`, `log.*`, the `.ioc`, and these notes.

Firmware package: **STM32Cube FW_H5 V1.6.0** (set in `stm32_h5.ioc`).

## 1. Generate

1. Open `stm32_h5.ioc` in STM32CubeIDE or STM32CubeMX.
2. Generate code into this folder (overwrite Cube trees; keep user-code blocks).
3. Confirm `Drivers/`, `Middlewares/ST/threadx`, and `Middlewares/ST/usbx` appeared.

Do not enable USART2 in the `.ioc`. PD5/PD6 stay free; `aep_kline_phy.c` owns USART2 and its IRQ.

## 2. After-generate fixes Cube does not put in the `.ioc`

| File | Change |
| --- | --- |
| `USBX/App/app_usbx_device.h` | Set `UX_DEVICE_APP_THREAD_STACK_SIZE` to **4096** (1 KiB overflowed into the USBX pool during UCP poll + TX). |
| `.project` | Restore linked resources `portable/{aep_log,sig,ucp,ucp_tlm,ucp_cfg,comp,store,vcom,vcom_diag}` with `PARENT-2-PROJECT_LOC/...` (CubeMX often drops them). |
| Include paths (Debug and Release) | Add repo-root `*/include` trees and `vcom_diag/third_party/iso14229`. |
| Compiler defines | `-DUDS_SYS=0 -DUDS_TP_ISOTP_C`. Optional log knobs: `-DAEP_LOG_LEVEL=AEP_LOG_LEVEL_WARN` (default) or `-DAEP_LOG_ENABLE=0`. |

Include paths:

```
../../../aep_log/include
../../../sig/include
../../../ucp/include
../../../ucp_tlm/include
../../../ucp_cfg/include
../../../comp/include
../../../store/include
../../../vcom/include
../../../vcom_diag/include
../../../vcom_diag/third_party/iso14229
```

Link `aep_log/src/aep_log_port.c` only if glue does not provide a strong `aep_port_log` (the example does, in `aep_ucp_port.c`).

The `.ioc` already sets MSP stack `0x1000`, USBX app pool `24*1024`, FDCAN classic 500 kbit/s (prescaler 5, seg1 7, seg2 2), and `HSE_EN` (PE2) default high. Linker flash is the full 512 KiB device (no NVS region).

## 3. Paste into `USER CODE` blocks

### `Core/Src/main.c`

**Includes**

```c
/* USER CODE BEGIN Includes */
#include "aep_app.h"
/* USER CODE END Includes */
```

**Init** — enable the external oscillator before `SystemClock_Config`:

```c
/* USER CODE BEGIN Init */
  __HAL_RCC_GPIOE_CLK_ENABLE();

  GPIO_InitTypeDef GPIO_InitStruct = {0};
  GPIO_InitStruct.Pin = HSE_EN_Pin;           /* PE2 */
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(HSE_EN_GPIO_Port, &GPIO_InitStruct);

  HAL_GPIO_WritePin(HSE_EN_GPIO_Port, HSE_EN_Pin, GPIO_PIN_SET);
  for (volatile uint32_t i = 0u; i < 500000u; i++) { }
/* USER CODE END Init */
```

Leave Cube’s `MX_GPIO_Init`, `MX_FDCAN1_Init`, `MX_FDCAN2_Init`, and `MX_USB_PCD_Init` unconditional.

**Before ThreadX** — hang indicator if the LED sticks solid:

```c
/* USER CODE BEGIN 2 */
  for (uint32_t n = 0u; n < 4u; n++)
  {
    HAL_GPIO_TogglePin(USR_LED_GPIO_Port, USR_LED_Pin);
    for (volatile uint32_t d = 0u; d < 5000000u; d++) { }
  }
/* USER CODE END 2 */
```

### `Core/Src/usb.c` — CRS + USB IRQ priority

In `HAL_PCD_MspInit`, after Cube enables the USB IRQ:

```c
/* USER CODE BEGIN USB_DRD_FS_MspInit 1 */
    {
      RCC_CRSInitTypeDef crs = {0};
      __HAL_RCC_CRS_CLK_ENABLE();
      crs.Prescaler = RCC_CRS_SYNC_DIV1;
      crs.Source = RCC_CRS_SYNC_SOURCE_USB;
      crs.Polarity = RCC_CRS_SYNC_POLARITY_RISING;
      crs.ReloadValue = __HAL_RCC_CRS_RELOADVALUE_CALCULATE(48000000U, 1000U);
      crs.ErrorLimitValue = 34;
      crs.HSI48CalibrationValue = 32;
      HAL_RCCEx_CRSConfig(&crs);
    }
    HAL_NVIC_SetPriority(USB_DRD_FS_IRQn, 5, 0);
/* USER CODE END USB_DRD_FS_MspInit 1 */
```

### `Core/Src/app_threadx.c`

**Includes**

```c
/* USER CODE BEGIN Includes */
#include "main.h"
#include "log.h"
#include "aep_app.h"
/* USER CODE END Includes */
```

**Init** — start glue when not in minimal bring-up:

```c
/* USER CODE BEGIN App_ThreadX_Init */
#if (AEP_BRINGUP_MINIMAL == 0)
  ret = aep_app_init(byte_pool);
  if (ret != TX_SUCCESS)
  {
    return ret;
  }
#else
  (void)byte_pool;
#endif
/* USER CODE END App_ThreadX_Init */
```

**App thread** — LED only (no demo signals):

```c
/* USER CODE BEGIN tx_app_thread_entry */
  TX_PARAMETER_NOT_USED(thread_input);

  while (1)
  {
    HAL_GPIO_TogglePin(USR_LED_GPIO_Port, USR_LED_Pin);
    tx_thread_sleep(TX_TIMER_TICKS_PER_SECOND / 2u);
  }
/* USER CODE END tx_app_thread_entry */
```

`AEP_BRINGUP_MINIMAL` is defined in `Core/Inc/aep_app.h` (default `0`). Set it to `1` to skip `aep_app_init` and run LED + USB CDC only.

### `USBX/App/app_usbx_device.c` — UCP owns CDC

Include `aep_ucp_usb.h`. In the device app thread loop, after CDC is ready and **before** the ASCII log-ring drain:

```c
    if (aep_ucp_usb_owns_cdc())
    {
      aep_ucp_usb_poll();
      /* Skip ASCII only while READY — keep draining for PuTTY bring-up. */
      if (aep_ucp_usb_ready())
      {
        tx_thread_sleep(1);
        continue;
      }
    }
    /* … existing log_ring → cdc_acm_write drain … */
```

Also keep the PMA layout and `HAL_PCD_Start` that this board needs (EP0, CDC CMD, bulk IN/OUT). See the working `app_ux_device_thread_entry` in a local generate for the full block.

`aep_ucp_usb_init()` (from the com thread) sets `owns_cdc` so this thread runs `aep_ucp_usb_poll()`. ASCII log drain stays on this same thread and is skipped only while the UCP session is READY (so `ucp_host` / TUI see a clean binary stream). Do not ASCII-write CDC from any other thread. `USBX/` is gitignored — keep this paste in your local generate.

## 4. Build and smoke

1. Build Debug, flash the STM32H523 board.
2. Expect ~1 Hz LED blink and a CDC ACM COM port.
3. Host unit tests (no board): `./tools/run_host_tests.ps1`
4. On-target dual-bus demo: [DEMO_PT.md](DEMO_PT.md)

## Related

| Doc | Role |
| --- | --- |
| [README.md](README.md) | Example map: glue file → portable module |
| [GLUE.md](GLUE.md) | File ownership and runtime sequence |
| [INTEGRATION.md](INTEGRATION.md) | Pins and include paths |
| [docs/LICENSING.md](../../docs/LICENSING.md) | MIT repo vs Cube packs |
