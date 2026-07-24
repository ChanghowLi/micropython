#ifndef __RTC_H
#define __RTC_H

#include <stdint.h>
#include "hal_data.h"

#ifdef __cplusplus
extern "C" {
#endif

uint32_t RTC_Init(void);
uint32_t RTC_GetCalendarTime(rtc_time_t *const p_time);

#ifdef __cplusplus
}
#endif

#endif
