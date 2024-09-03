#include "pico_dash_latch.h"

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

int getLatchedDataResolution(enum LatchedDataIndex index)
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