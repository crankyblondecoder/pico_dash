// #include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "pico/time.h"

/**
 * Program for Pi Pico that drives a BMW E36 Dash and instrument cluster.
 */

int main()
{
	// L293D Pin Allocation
	// (H Bridge Stepper Driver)

	const unsigned L293_PIN1_12EN = 17; // 1,2 Enable.
	const unsigned L293_PIN9_34EN = 16; // 3,4 Enable.
	const unsigned L293_PIN2_1A = 27; // Half bridge 1 input.
	const unsigned L293_PIN7_2A = 26; // Half bridge 2 input.
	const unsigned L293_PIN10_3A = 21; // Half bridge 3 input.
	const unsigned L293_PIN15_4A = 20; // Half bridge 4 input.

	const unsigned LEFT_SWITCH_PIN = 15;
	const unsigned RIGHT_SWITCH_PIN = 10;

	// Sleep times are in milliseconds.
	const unsigned SLEEP_TIME_INACTIVE = 500;
	const unsigned SLEEP_TIME_ACTIVE = 10;

	// Steps for bipolar stepper motor.
	struct stepper_step
	{
		unsigned coilA_1;
		unsigned coilA_2;
		unsigned coilB_1;
		unsigned coilB_2;
	};

	// The X27 Stepper that this targets is not a standard type stepper. It only has two coils and three pole positions
	// It requires partial steps to work properly.

	// Full step Table.
	struct stepper_step steps[6] = {

		{1,0,1,0},
		{1,0,0,0},
		{0,0,0,1},
		{0,1,0,1},
		{0,1,0,0},
		{0,0,1,0}
	};

	int curStep = 0;
	const unsigned MAX_STEP_INDEX = 5;

	gpio_init(LEFT_SWITCH_PIN);
	gpio_pull_up(LEFT_SWITCH_PIN);
	gpio_set_dir(LEFT_SWITCH_PIN, GPIO_IN);

	gpio_init(RIGHT_SWITCH_PIN);
	gpio_pull_up(RIGHT_SWITCH_PIN);
	gpio_set_dir(RIGHT_SWITCH_PIN, GPIO_IN);

	// Both H-Bridges are initially disabled.

	gpio_init(L293_PIN1_12EN);
    gpio_set_dir(L293_PIN1_12EN, GPIO_OUT);
	gpio_put(L293_PIN1_12EN, 0);

	gpio_init(L293_PIN9_34EN);
    gpio_set_dir(L293_PIN9_34EN, GPIO_OUT);
	gpio_put(L293_PIN9_34EN, 0);

	// Setup first step.

	gpio_init(L293_PIN2_1A);
    gpio_set_dir(L293_PIN2_1A, GPIO_OUT);
	gpio_put(L293_PIN2_1A, steps[0].coilA_1);

	gpio_init(L293_PIN7_2A);
    gpio_set_dir(L293_PIN7_2A, GPIO_OUT);
	gpio_put(L293_PIN7_2A, steps[0].coilA_2);

	gpio_init(L293_PIN10_3A);
    gpio_set_dir(L293_PIN10_3A, GPIO_OUT);
	gpio_put(L293_PIN10_3A, steps[0].coilB_1);

	gpio_init(L293_PIN15_4A);
    gpio_set_dir(L293_PIN15_4A, GPIO_OUT);
	gpio_put(L293_PIN15_4A, steps[0].coilB_2);

    while(true)
	{
		bool switchLeftState = gpio_get(LEFT_SWITCH_PIN);
		bool switchRightState = gpio_get(RIGHT_SWITCH_PIN);

		if(switchLeftState && switchRightState)
		{
			// Open switch should be pin pulled high.

			// Disable H-Bridges.
			gpio_put(L293_PIN1_12EN, 0);
			gpio_put(L293_PIN9_34EN, 0);

			sleep_ms(SLEEP_TIME_INACTIVE);
		}
		else
		{
			// Closed switch should be pin pulled low.

			// Enable H-Bridges.
			gpio_put(L293_PIN1_12EN, 1);
			gpio_put(L293_PIN9_34EN, 1);

			// Go to next step.

			if(!switchLeftState)
			{
				// Step left.
				if(curStep == 0)
				{
					curStep = MAX_STEP_INDEX;
				}
				else
				{
					curStep--;
				}

			}
			else
			{
				// Step right.
				if(curStep == MAX_STEP_INDEX)
				{
					curStep = 0;
				}
				else
				{
					curStep++;
				}
			}

			// Set coil pins.
			gpio_put(L293_PIN2_1A, steps[curStep].coilA_1);
			gpio_put(L293_PIN7_2A, steps[curStep].coilA_2);
			gpio_put(L293_PIN10_3A, steps[curStep].coilB_1);
			gpio_put(L293_PIN15_4A, steps[curStep].coilB_2);

			sleep_ms(SLEEP_TIME_ACTIVE);
		}
    }
}
