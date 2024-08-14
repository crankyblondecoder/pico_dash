// #include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "pico/time.h"

#include "pico_dash_latch.h"

/** Currently latched data. */
int latchedData[MAX_LATCHED_INDEXES];

/**
 * Program for Pi Pico that interfaces to cars electrical signals and can either provide data to another display system
 * or drive physical gauges directly.
 */
int main()
{
	// TODO ...
}
