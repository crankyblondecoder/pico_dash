#ifndef PICO_DASH_GPIO_H
#define PICO_DASH_GPIO_H

#include "hardware/gpio.h"
#include "platform.h"

// Contains any code to shared GPIO resources like interupts.

// REMEMBER: IRQ's are set _per core_.

/**
 * GPIO subsystem initialisation.
 * Only do this _once_.
 */
void initGpioIrqSubsystem();

/**
 * Set a specific GPIO callback for the current core.
 * @note This doesn't enable the GPIO irq. That must be done with gpio_set_irq_enabled.
 */
void setGpioIrqCallBack(uint gpio, gpio_irq_callback_t callback);

#endif