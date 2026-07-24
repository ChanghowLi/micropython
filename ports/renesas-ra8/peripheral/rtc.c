#include "rtc.h"

#define RTC_INSTANCE	g_rtc0
#define RTC_CALLBACK	RTC_Callback

#define TAG	__FUNCTION__

#ifndef __RTC_DEBUG
#define __RTC_DEBUG	1
#endif

#if __RTC_DEBUG
#include "utils/log.h"
#define LIKE_RETURN(v, t, msg, ...)		if (v == t) { LOG_E(TAG, msg, ##__VA_ARGS__); return v; }
#define UNLIKE_RETURN(v, t, msg, ...)	if (v != t) { LOG_E(TAG, msg, ##__VA_ARGS__); return v; }
#else
#define LIKE_RETURN(v, t, msg, ...)		if (v == t) { return v; }
#define UNLIKE_RETURN(v, t, msg, ...)	if (v != t) { return v; }
#endif

uint32_t RTC_GetCalendarTime(rtc_time_t *const p_time)
{
	uint32_t err;

	err = R_RTC_CalendarTimeGet(RTC_INSTANCE.p_ctrl, p_time);
	UNLIKE_RETURN(err, 0, "CalendarTimeGet failed: %lu", err);

	return 0;
}

uint32_t RTC_Init(void)
{
	uint32_t err;

	err = R_RTC_Open(RTC_INSTANCE.p_ctrl, RTC_INSTANCE.p_cfg);
	UNLIKE_RETURN(err, 0, "Open failed: %lu", err);

	return 0;
}

void RTC_CALLBACK(rtc_callback_args_t *p_args)
{
	switch (p_args->event) {
	case RTC_EVENT_ALARM_IRQ:
		break;
	case RTC_EVENT_ALARM1_IRQ:
		break;
	case RTC_EVENT_PERIODIC_IRQ:
		break;
	default:
		break;
	}
}
