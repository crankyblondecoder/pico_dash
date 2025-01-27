#ifndef PICO_DASH_LATCH_H
#define PICO_DASH_LATCH_H

#include <string.h>

#include "pico/time.h"

// Anything to do with latching.

#define MAX_LATCH_DATA_INDEX_NAME_SIZE 3

/** Maximum number of Analog to Digital converter channels. */
#define MAX_ADC_CHANNELS 16

/**
 * Types of sensors.
 */
typedef enum
{
	/** Voltage is read and scaled to produce the latched data value. */
	SCALED_VOLTAGE_SENSOR,

	/** Pulses are detected and counted. */
	PULSE_SENSOR,

	/** Simple on/off state. */
	ON_OFF_SENSOR

} SensorType;

/**
 * Used to populate sensor data.
 * All enums match a variable name in Sensor.
 */
typedef enum
{
	/** The sensor must be explicitly set to active at some point to start processing input. */
	ACTIVE = 1,
	STROBE_INTERVAL,
	ADC_CHANNEL,
	PULSE_ACCUMULATION_INTERVAL,
	PULSE_PRE_SCALE,
	PULSE_POST_SCALE,
	PULSE_TEST_DURATION_START,
	PULSE_TEST_DURATION_END,
	PULSE_TEST_DURATION_STEP,
	PULSE_TEST_STEP_TIME_INTERVAL,
	/** Must always be last to indicate the end of the enum. */
	MAX_SENSOR_DATA

} SensorData;

/**
 * Description of a sensor, used to read and interpret latched data.
 */
struct Sensor
{
	/** Type of sensor. */
	SensorType type;

	/** Whether this sensor is active. */
	bool active;

	/** Number of microseconds between each strobe. A strobe is the ideal time duration between sensor processing passes. */
	int strobeInterval;

	/** Time of last strobe. */
	absolute_time_t lastStrobeTime;

	/** Analog to digital converter channel to use. */
	int adcChannel;

	union
	{
		/** Pulse sensor data. */
		struct
		{
			/** The state of the last edge. True for rising, false for falling. */
			bool lastEdgeState;

			/** Current number of unresolved pulses. A pulse is considered rising edge to rising edge. */
			int pulseCount;

			/** Time interval, in microseconds, over which pulses are accumulated before resolution. */
			int pulseAccumulationInterval;

			/**
			 * This value is multiplied by the pulse count then divided by the time interval between the first and last rising
			 * edge to get the current resolved sensor reading. It is necessary so that accurate integer arithmetic can be
			 * used instead of floating point.
			 */
			int pulsePreScale;

			/**
			 * The resolved sensor reading is divided by this value to get the desired final required units that the sensor
			 * ultimate outputs. ie Sensor Output Value.
			 */
			int pulsePostScale;

			/** Start time of accumulating interval. ie First rising edge, pulse count 0. */
			absolute_time_t accumIntervalStartPulseTime;

			/** Time of last pulse rising edge. */
			absolute_time_t accumIntervalEndPulseTime;

			/** Duration of test pulse, in microseconds, at test start value. */
			int pulseTestDurationStart;

			/** Duration of test pulse, in microseconds, at test end value. */
			int pulseTestDurationEnd;

			/** The current duration of the test pulse, in microseconds. */
			int pulseTestCurDuration;

			/**
			 * Amount to step the test duration at the end of each test interval.
			 * May be negated from its original value to indicate test direction.
			 */
			int pulseTestDurationStep;

			/** Amount of time to wait, in microseconds, before stepping (increasing or decreasing) the test pulse duration. */
			int pulseTestStepTimeInterval;
		};
	};
};

/**
 * Indexes used to store and retrieve latched data.
 * Also applies to sensor descriptors.
 * @note Index 0 is never used because SPI comms needs to avoid zeros if possible.
 */
typedef enum
{
	/** Engine RPM. */
	ENGINE_RPM = 1,

	/** Speed km/h. */
	SPEED_KMH,

	/** Engine temperature degrees celsius. */
	ENGINE_TEMP_C,

	/** Marker to indicate the size of the latched data indexes. Must _always_ be last in enum. */
	MAX_LATCHED_INDEXES

} LatchedDataIndex;

/**
 * Initialise the latcher. Must be done before it is started.
 */
void initLatcher();

/**
 * Initialise and start the latcher on the core this function was called from.
 * @note Starts and uses the second core, then immediately returns.
 */
void startLatcher();

/**
 * Get the latched data index given a name.
 * @returns Positive index if found, -1 otherwise.
 */
int getLatchedDataIndex(const char* latchedDataIndexName);

/**
 * Get the resolution of the given latched data index.
 * The resolution is the number of integer steps allocated per graduation.
 * For example, engine temperature might have 10 integer steps per degree celsius giving it a resolution of 0.1.
 */
int getLatchedDataResolution(LatchedDataIndex index);

/**
 * Get the currently latched data for the given index.
 */
int getLatchedData(LatchedDataIndex index);

/**
 * Set the data for a paricular sensor.
 * @param sensorIndex Sensor to set data for.
 * @param sensorVar Sensor variable to set data for.
 * @param varVal Variable value to set.
 * @returns True for success, false for could not be set.
 */
bool setSensorData(LatchedDataIndex sensorIndex, SensorData sensorVar, int varVal);

/**
 * Set whether latcher is in test mode.
 */
void setTestMode(bool testMode);

#endif
