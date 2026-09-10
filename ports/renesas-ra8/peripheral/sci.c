#include "sci.h"

#define MACHINE_SCI_CHANNEL_COUNT (10U)

static machine_sci_owner_t machine_sci_owners[MACHINE_SCI_CHANNEL_COUNT];

machine_sci_owner_t machine_sci_get_owner(uint8_t channel)
{
    if (channel >= MACHINE_SCI_CHANNEL_COUNT) {
        return MACHINE_SCI_OWNER_NONE;
    }

    return machine_sci_owners[channel];
}

void machine_sci_give(uint8_t channel, machine_sci_owner_t owner)
{
    if (channel < MACHINE_SCI_CHANNEL_COUNT && machine_sci_owners[channel] == owner) {
        machine_sci_owners[channel] = MACHINE_SCI_OWNER_NONE;
    }
}

bool machine_sci_take(uint8_t channel, machine_sci_owner_t owner)
{
    if (channel >= MACHINE_SCI_CHANNEL_COUNT || owner == MACHINE_SCI_OWNER_NONE) {
        return false;
    }

    if (machine_sci_owners[channel] != MACHINE_SCI_OWNER_NONE) {
        return false;
    }

    machine_sci_owners[channel] = owner;
    return true;
}
