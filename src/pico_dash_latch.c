#include <stdio.h>
#include <stdbool.h>

#include "pico/multicore.h"
#include "pico/time.h"

#include "pico_dash_latch.h"

extern bool debugMsgActive;

/** Currently latched data. */
int _latchedData[MAX_LATCHED_INDEXES];

/** Sensors. Indexes match latched data indexes. */
struct Sensor _sensors[MAX_LATCHED_INDEXES];

/** Whether this is in test mode and is producing test data rather than reading actual live input. */
bool _testMode = false;

/** Flag to trigger sensor processing loop to exit. */
bool _exitSensorProcLoop = false;

/** Get an Analog to Digital value from the given channel. */
int _getAdcValue(int channel)
{
	// TODO ...

	// Just return 0 for now so that at least something is available before test mode is invoked.
	return 0;
}

void _initPulseSensor(int sensorIndex)
{
	_sensors[sensorIndex].pulseCount = 0;
	_sensors[sensorIndex].pulsePreScale = 1;
	_sensors[sensorIndex].pulsePostScale = 1;
	_sensors[sensorIndex].accumIntervalStartPulseTime = 0;
	_sensors[sensorIndex].accumIntervalEndPulseTime = 0;
}

/** Process a pulse sensor. */
void _procPulseSensor(int sensorIndex)
{
	absolute_time_t curTime = get_absolute_time();

	if(_sensors[sensorIndex].accumIntervalStartPulseTime == 0)
	{
		// First processing after init. Just set accumulation start time.
		_sensors[sensorIndex].accumIntervalStartPulseTime = curTime;
	}
	else if(_testMode)
	{
		int pulseTestCurDur = _sensors[sensorIndex].pulseTestCurDuration;

		if(pulseTestCurDur > 0)
		{
			// Maximum pulses that fit in time interval since last read.
			int pulseCountDelta = absolute_time_diff_us(curTime, _sensors[sensorIndex].accumIntervalEndPulseTime)
				/ _sensors[sensorIndex].pulseTestCurDuration;

			_sensors[sensorIndex].pulseCount += pulseCountDelta;

			// Update accumulation interval end time to be the rising edge of the last test pulse.
			_sensors[sensorIndex].accumIntervalEndPulseTime = delayed_by_us(_sensors[sensorIndex].accumIntervalEndPulseTime,
				pulseCountDelta * pulseTestCurDur);

			_sensors[sensorIndex].lastEdgeState = true;
		}
	}
	else
	{
		// TODO ... Get ADC data for channel and determine if pulse occurred.
		// Don't forget to store edge state so multiple samples of the same edge don't trigger multiple pulses.
	}

	absolute_time_t accumEndTime = delayed_by_us(_sensors[sensorIndex].accumIntervalStartPulseTime,
		_sensors[sensorIndex].pulseAccumulationInterval);

	if(accumEndTime < curTime)
	{
		// Accumulation interval has completed. Resolve pulses into a sensor output value.

		// Pre-scale reduces loss of precision.
		int latchedValue =  _sensors[sensorIndex].pulseCount * _sensors[sensorIndex].pulsePreScale;

		latchedValue /= absolute_time_diff_us(_sensors[sensorIndex].accumIntervalStartPulseTime,
			_sensors[sensorIndex].accumIntervalEndPulseTime);

		// Bring sensor output back to intended units and latch.
		_latchedData[sensorIndex] = latchedValue / _sensors[sensorIndex].pulsePostScale;

		// Reset interval start time to the end time so that accumulation interval resets.
		_sensors[sensorIndex].accumIntervalStartPulseTime = _sensors[sensorIndex].accumIntervalEndPulseTime;

		// Start accumulating pulses again.
		_sensors[sensorIndex].pulseCount = 0;
	}
}

/** Main sensor processing loop. */
void _sensorProcLoop()
{
	while(!_exitSensorProcLoop)
	{
		for(int index = 0; index < MAX_LATCHED_INDEXES; index++)
		{
			if(_sensors[index].active)
			{
				absolute_time_t curPollTime = get_absolute_time();

				if(absolute_time_diff_us(_sensors[index].lastStrobeTime, curPollTime) > _sensors[index].strobeInterval)
				{
					_sensors[index].lastStrobeTime = curPollTime;

					switch(_sensors[index].type)
					{
						case SCALED_VOLTAGE_SENSOR:

							break;

						case PULSE_SENSOR:

							_procPulseSensor(index);
							break;

						case ON_OFF_SENSOR:

							break;
					}
				}
			}
		}
	}
}

void initLatcher()
{
	for(int index = 0; index < MAX_LATCHED_INDEXES; index++)
	{
		_sensors[index].active = false;

		_sensors[index].lastStrobeTime = 0;

		switch(_sensors[index].type)
		{
			case SCALED_VOLTAGE_SENSOR:

				// TODO ...
				break;

			case PULSE_SENSOR:

				_initPulseSensor(index);
				break;

			case ON_OFF_SENSOR:

				// TODO ...
				break;
		}
	}
}

void _coreEntry()
{
	_sensorProcLoop();
}

void startLatcher()
{
	// Zero the latched data.
	for(int index = 0; index < MAX_LATCHED_INDEXES; index++)
	{
		_latchedData[index] = 0;
	}

	multicore_launch_core1(_coreEntry);
}

int getLatchedDataIndex(const char* latchedDataIndexName)
{
	int retVal = -1;

	if(!strncmp("ERM", latchedDataIndexName, MAX_LATCH_DATA_INDEX_NAME_SIZE))
	{
		retVal = ENGINE_RPM;
	}
	else if(!strncmp("SKH", latchedDataIndexName, MAX_LATCH_DATA_INDEX_NAME_SIZE))
	{
		retVal = SPEED_KMH;
	}
	else if(!strncmp("ETC", latchedDataIndexName, MAX_LATCH_DATA_INDEX_NAME_SIZE))
	{
		retVal = ENGINE_TEMP_C;
	}

	return retVal;
}

int getLatchedDataResolution(LatchedDataIndex index)
{
	int retVal = 0;

	switch (index)
	{
		case ENGINE_RPM:

			retVal = 1;
			break;

		case SPEED_KMH:

			retVal = 1;
			break;

		case ENGINE_TEMP_C:

			retVal = 10;
			break;
	}

	return retVal;
}

int getLatchedData(LatchedDataIndex index)
{

	// NOTE: Because latched values are a single word and the Cortex M0+ doesn't have a data cache and the AHB-lite crossbar
	//       will stall any of the cores while the other is accessing a particular data location, it should be perfectly
	//       safe _not_ to have synchronisation here.

	if(index < MAX_LATCHED_INDEXES)
	{
		return _latchedData[index];
	}
	else if(debugMsgActive)
	{
		printf("Latched data index %i out of bounds.\n", index);
	}

	return 0;
}

bool setSensorData(LatchedDataIndex sensorIndex, SensorData sensorVar, int varVal)
{
	bool retVal = false;

	if(sensorIndex < MAX_LATCHED_INDEXES)
	{
		if(sensorVar < MAX_SENSOR_DATA)
		{
			switch(sensorVar)
			{
				case ACTIVE:

					_sensors[sensorIndex].active = varVal > 0;
					break;

				case STROBE_INTERVAL:

					_sensors[sensorIndex].strobeInterval = varVal;
					break;

				case ADC_CHANNEL:

					_sensors[sensorIndex].adcChannel = varVal;
					break;

				case PULSE_ACCUMULATION_INTERVAL:

					_sensors[sensorIndex].pulseAccumulationInterval = varVal;
					break;

				case PULSE_PRE_SCALE:

					_sensors[sensorIndex].pulsePreScale = varVal;
					break;

				case PULSE_POST_SCALE:

					_sensors[sensorIndex].pulsePostScale = varVal;
					break;

				case PULSE_TEST_DURATION_START:

					_sensors[sensorIndex].pulseTestDurationStart = varVal;
					break;

				case PULSE_TEST_DURATION_END:

					_sensors[sensorIndex].pulseTestDurationEnd = varVal;
					break;

				case PULSE_TEST_DURATION_STEP:

					_sensors[sensorIndex].pulseTestDurationStep = varVal;
					break;

				case PULSE_TEST_STEP_TIME_INTERVAL:

					_sensors[sensorIndex].pulseTestStepTimeInterval = varVal;
					break;
			}

			retVal = true;
		}
		else if(debugMsgActive)
		{
			printf("Sensor data variable index %i out of bounds.\n", sensorVar);
		}
	}
	else if(debugMsgActive)
	{
		printf("Sensor index %i out of bounds.\n", sensorIndex);
	}

	return retVal;
}

void setTestMode(bool testMode)
{
	_testMode = testMode;
}
