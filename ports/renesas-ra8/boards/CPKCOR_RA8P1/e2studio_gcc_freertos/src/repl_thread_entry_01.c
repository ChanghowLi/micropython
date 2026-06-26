#include "console.h"
#include "repl_thread.h"

#include "SEGGER_RTT/SEGGER_RTT.h"
#include "utils/log.h"

void repl_thread_entry(void *pvParameters)
{
	FSP_PARAMETER_NOT_USED(pvParameters);

#if CONSOLE_CFG_USE_RTT == 0
	SEGGER_RTT_Init();
#endif
	CONSOLE_Init();

	while (1) {
		R_IOPORT_PinWrite(g_ioport.p_ctrl, USER_LED, BSP_IO_LEVEL_LOW);
		vTaskDelay(500);
		R_IOPORT_PinWrite(g_ioport.p_ctrl, USER_LED, BSP_IO_LEVEL_HIGH);
		vTaskDelay(500);
	}
}

#if LOG_CFG_EN_TIMESTAMP
void LOG_GetTime(uint32_t *s, uint32_t *ms)
{
	TickType_t t;

	if (__get_IPSR() == 0) {
		t = xTaskGetTickCount();
	}
	else {
		t = xTaskGetTickCountFromISR();
	}
	*s = (uint32_t)(t / 1000);
	*ms = (uint32_t)(t % 1000);
}
#endif
