// #include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "pico/time.h"

#include "pico_dash_gpio.h"
#include "pico_dash_latch.h"
#include "pico_dash_spi.h"

/** Currently latched data. */
int latchedData[MAX_LATCHED_INDEXES];

/**
 * Program for Pi Pico that interfaces to cars electrical signals and can either provide data to another display system
 * or drive physical gauges directly.
 */
int main()
{
	// Must be done before SPI start.
	initGpioIrqSubsystem();

	// For now run the SPI comms on core 0.
	startSpiSubsystem();

	// TODO ...
}
