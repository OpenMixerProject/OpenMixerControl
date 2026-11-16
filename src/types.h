#pragma once

#include <stdint.h>

#include "defines.h"
#include "enum.h"
#include "../lib/WString.h"

using namespace std;

typedef unsigned int uint;

// define own datatypes
typedef union {
    uint64_t u64;
    int64_t s64;
    uint32_t u32[2];
    int32_t s32[2];
    uint16_t u16[4];
    int16_t s16[4];
    uint8_t u8[8];
    int8_t s8[8];
    double d;
} data_64b;

typedef union {
    uint32_t u32;
    int32_t s32;
    uint16_t u16[2];
    int16_t s16[2];
    uint8_t u8[4];
    int8_t s8[4];
    float f;
} data_32b;

typedef union {
    uint16_t u16;
    int16_t s16;
    uint8_t u8[2];
    int8_t s8[2];
} data_16b;


// TouchControl Basic Plus - one fader at a time is touchcontrolled
typedef struct{
    uint8_t value;
    OMC_BOARD board;
    uint8_t faderIndex;
} sTouchControl;

typedef struct {
	// user-settings
	float fc; // cutoff-frequency for high- or lowpass
	bool isHighpass; // choose if Highpass or Lowpass

	// filter-coefficients
	float a[3];
	float b[3];
} sLR12;

typedef struct {
	// user-settings
	float fc; // cutoff-frequency for high- or lowpass
	bool isHighpass; // choose if Highpass or Lowpass

	// filter-coefficients
	float a[5];
	float b[5];
} sLR24;

typedef struct {
	// user-settings
	int type; // 0=allpass, 1=peak, 2=low-shelf, 3=high-shelf, 4=bandpass, 5=notch, 6=lowpass, 7=highpass
	float fc; // center-frequency of PEQ
	float Q; // Quality of PEQ (bandwidth)
	float gain; // gain of PEQ

	// filter-coefficients
	float a[3];
	float b[3];
} sPEQ;

typedef struct
{
	float meterPu[2]; // meter information in p.u.
	uint32_t meter[2];
	uint32_t meterDecay[2]; // meter information with decay
} sMainChannel;

// values only for runtime use
typedef struct
{
	sPEQ peq[MAX_CHAN_EQS];

	uint32_t meter;
	uint32_t meterDecay; // meter information with decay
} srDspChannel;

typedef struct {
	float frequency;
	float volume;
} sDsp2Oscillator;

typedef struct {
	WString::String label;
	WString::String value;
} sDisplayEncoder;

typedef void (*SurfaceCallback)(void* arg, OMC_BOARD board, char command, uint8_t index, uint16_t value);

typedef void (*OscSendToServerCallbackSet)(void* arg, MP_ID parameterId, WString::String strValue, float floatValue, uint index);
typedef void (*OscSendToServerCallbackChange)(void* arg, MP_ID parameterId, int amount, uint index);
typedef void (*OscSendToServerCallbackToogle)(void* arg, MP_ID parameterId, uint index);
typedef void (*OscSendToServerCallbackReset)(void* arg, MP_ID parameterId, uint index);