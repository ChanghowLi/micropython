#include "app_thread.h"
#include "console.h"

void app_thread_entry(void *pvParameters)
{
	FSP_PARAMETER_NOT_USED(pvParameters);

	CONSOLE_Init();

	while (1) {
		vTaskDelay(1);
	}
}
