/* generated configuration header file - do not edit */
#ifndef BSP_PIN_CFG_H_
#define BSP_PIN_CFG_H_
#include "r_ioport.h"

/* Common macro for FSP header files. There is also a corresponding FSP_FOOTER macro at the end of this file. */
FSP_HEADER

#define MIPI_FPC_LCD_BL (BSP_IO_PORT_00_PIN_12)
#define MIPI_FPC_TP_RST (BSP_IO_PORT_00_PIN_13)
#define USER_LED (BSP_IO_PORT_01_PIN_10)
#define FUSB_EN_N (BSP_IO_PORT_06_PIN_15)

extern const ioport_cfg_t g_bsp_pin_cfg; /* RA8P1_CPKCOR.pincfg */

void BSP_PinConfigSecurityInit();

/* Common macro for FSP header files. There is also a corresponding FSP_HEADER macro at the top of this file. */
FSP_FOOTER
#endif /* BSP_PIN_CFG_H_ */
