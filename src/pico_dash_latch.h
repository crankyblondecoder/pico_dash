#ifndef PICO_DASH_LATCH_H
#define PICO_DASH_LATCH_H

#include <string.h>

#define MAX_LATCHED_INDEXES 64
#define MAX_LATCH_DATA_INDEX_NAME_SIZE 3

// Anything to do with latching.

/**
 * Indexes used to store and retrieve latched data.
 * @note Index 0 is never used because SPI comms needs to avoid zeros if possible.
 */
enum LatchedDataIndex
{
	/** Engine RPM. */
	ENGINE_RPM = 1,

	/** Speed km/h. */
	SPEED_KMH,

	/** Engine temperature degrees celsius. */
	ENGINE_TEMP_C
};

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
int getLatchedDataResolution(enum LatchedDataIndex index);

#endif