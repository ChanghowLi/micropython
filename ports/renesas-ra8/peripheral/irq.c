#include "hal_data.h"
#include "irq.h"

uint32_t query_irq(void)
{
    return __get_PRIMASK();
}

uint32_t disable_irq(void)
{
    uint32_t state = __get_PRIMASK();
    __disable_irq();
    return state;
}

void enable_irq(uint32_t state)
{
    __set_PRIMASK(state);
}
