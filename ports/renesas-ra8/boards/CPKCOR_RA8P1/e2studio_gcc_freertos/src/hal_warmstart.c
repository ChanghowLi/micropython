/*
* Copyright (c) 2020 - 2026 Renesas Electronics Corporation and/or its affiliates
*
* SPDX-License-Identifier: BSD-3-Clause
*/

#include "hal_data.h"
#include "modmachine.h"
#include "FreeRTOS.h"
#include "portable.h"

#define SRAM_HEAP_SIZE  (32U * 1024U)
#define SDRAM_HEAP_SIZE (16U * 1024U * 1024U)

static uint8_t s_sram_heap[SRAM_HEAP_SIZE] __attribute__((aligned(32)));
static uint8_t s_sdram_heap[SDRAM_HEAP_SIZE]
    __attribute__((section(".sdram_noinit_nocache"), aligned(64)));

FSP_CPP_HEADER
void R_BSP_WarmStart(bsp_warm_start_event_t event);

FSP_CPP_FOOTER

/*******************************************************************************************************************//**
 * This function is called at various points during the startup process.  This implementation uses the event that is
 * called right before main() to set up the pins.
 *
 * @param[in]  event    Where at in the start up process the code is currently at
 **********************************************************************************************************************/
void R_BSP_WarmStart (bsp_warm_start_event_t event)
{
    if (BSP_WARM_START_RESET == event)
    {
        machine_reset_flags.rstsr0 = R_SYSTEM->RSTSR0;
        machine_reset_flags.rstsr1 = R_SYSTEM->RSTSR1;
        machine_reset_flags.rstsr2 = R_SYSTEM->RSTSR2;
        machine_reset_flags.rstsr3 = R_SYSTEM->RSTSR3;
        
        /* Clear reset-source flags after saving them. */
         R_SYSTEM->RSTSR0 = 0U;
         R_SYSTEM->RSTSR1 = 0U;
         R_SYSTEM->RSTSR3 = 0U;

        /* Mark subsequent resets as warm starts. */
        R_SYSTEM->RSTSR2_b.CWSF = 1U;

#if BSP_FEATURE_FLASH_LP_VERSION != 0

        /* Enable reading from data flash. */
        R_FACI_LP->DFLCTL = 1U;

        /* Would normally have to wait tDSTOP(6us) for data flash recovery. Placing the enable here, before clock and
         * C runtime initialization, should negate the need for a delay since the initialization will typically take more than 6us. */
#endif
    }

#if BSP_CFG_OSPI_B_STARTUP_ENABLED && defined(BSP_CFG_OSPI_B_STARTUP_FN)
    if (BSP_WARM_START_POST_CLOCK == event)
    {
        /* Setup OSPI_B SiP flash and initialize it. */
        R_BSP_OspiBInit(BSP_CFG_OSPI_B_STARTUP_FN, true);
    }
#endif

    if (BSP_WARM_START_POST_C == event)
    {
        /* C runtime environment and system clocks are setup. */

        /* Configure pins. */
        R_IOPORT_Open(&IOPORT_CFG_CTRL, &IOPORT_CFG_NAME);

#if BSP_CFG_SDRAM_ENABLED

        /* Setup SDRAM and initialize it. Must configure pins first. */
        R_BSP_SdramInit(true);

        const HeapRegion_t heap_regions[] = {
            { s_sram_heap, sizeof(s_sram_heap) },
            { s_sdram_heap, sizeof(s_sdram_heap) },
            { NULL, 0 }
        };
        vPortDefineHeapRegions(heap_regions);
#endif
    }
}
