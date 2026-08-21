#include "sci.h"

#define MACHINE_SCI_CHANNEL_COUNT (10)

static machine_sci_owner_t machine_sci_owners[MACHINE_SCI_CHANNEL_COUNT];

bool machine_sci_take(uint8_t channel, machine_sci_owner_t owner)
{
    if (channel >= MACHINE_SCI_CHANNEL_COUNT || owner == MACHINE_SCI_OWNER_NONE) {
        return false;
    }

    if (machine_sci_owners[channel] != MACHINE_SCI_OWNER_NONE &&
        machine_sci_owners[channel] != owner) {
        return false;
    }

    machine_sci_owners[channel] = owner;
    return true;
}

void machine_sci_give(uint8_t channel, machine_sci_owner_t owner)
{
    if (channel < MACHINE_SCI_CHANNEL_COUNT && machine_sci_owners[channel] == owner) {
        machine_sci_owners[channel] = MACHINE_SCI_OWNER_NONE;
    }
}