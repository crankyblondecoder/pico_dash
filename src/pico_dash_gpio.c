#include "pico_dash_gpio.h"

#define NUM_GPIOS 29

// There are 29 GPIO pins exposed on the pico.
gpio_irq_callback_t core0_callbacks[NUM_GPIOS];
gpio_irq_callback_t core1_callbacks[NUM_GPIOS];

bool core_irq_enabled[2];

void __not_in_flash_func(gpio_callback)(uint gpio, uint32_t event_mask)
{
	uint curCoreNum = get_core_num();

	gpio_irq_callback_t callback;

	if(curCoreNum == 0)
	{
		callback = core0_callbacks[gpio];
	}
	else
	{
		callback = core1_callbacks[gpio];
	}

	if(callback) callback(gpio, event_mask);
}

void initGpioIrqSubsystem()
{
	int index = 0;

	while(index < NUM_GPIOS)
	{
		core0_callbacks[index] = 0;
		core1_callbacks[index] = 0;
		index++;
	}

	core_irq_enabled[0] = false;
	core_irq_enabled[1] = false;
}

void setGpioCallBack(uint gpio, gpio_irq_callback_t callback)
{
	uint curCoreNum = get_core_num();

	// NOTE: This function only affects the NVIC of the current core.
	// NOTE: IO_IRQ_BANK0 is correct for enabling the GPIO subsystem interupts.
	// NOTE: SIO_IRQ_PROCx are NOT related to GPIO but are used for SIO FIFO.

	if(!core_irq_enabled[curCoreNum])
	{
		core_irq_enabled[curCoreNum] = true;
		gpio_set_irq_callback(callback);
		irq_set_enabled(IO_IRQ_BANK0, true);
	}

	if(curCoreNum == 0)
	{
		core0_callbacks[gpio] = callback;
	}
	else
	{
		core1_callbacks[gpio] = callback;
	}
}

