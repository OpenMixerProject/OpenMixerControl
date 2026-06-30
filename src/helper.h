#pragma once

#include <stdint.h>
#include <stdarg.h>

#include <string>
#include <vector>
#include <map>

#include "../lib/plf_nanotimer.h"
#include "../lib/WString.h"

#include "enum.h"
#include "types.h"

#define DEBUGLEVEL_OFF -1 // No Debug
#define DEBUGLEVEL_NORMAL 0 // General highlevel debug messages
#define DEBUGLEVEL_VERBOSE 1 // Function calls, Elements, ...
#define DEBUGLEVEL_TRACE 2 // Very verbose down to the last bit

using namespace std;
using namespace WString;

namespace OMC
{

class Helper
{
    private:
        uint32_t debug_;
        uint8_t debuglevel_;
        map<uint8_t, plf::nanotimer*> timers;

        // LUT von 6 dBFS bis -60 dBFS (Größe: 67)
        const uint32_t dbfs_lut[67] = {
            4285031804 /* 6 dBFS (Index 0) */,
            3818987154 /* 5 dBFS (Index 1) */,
            3403606992 /* 4 dBFS (Index 2) */,
            3033430349 /* 3 dBFS (Index 3) */,
            2703517173 /* 2 dBFS (Index 4) */,
            2409477610 /* 1 dBFS (Index 5) */,
            2147483648 /* 0 dBFS (Index 6) */,
            1913143336 /* -1 dBFS (Index 7) */,
            1704403666 /* -2 dBFS (Index 8) */,
            1518428514 /* -3 dBFS (Index 9) */,
            1352758156 /* -4 dBFS (Index 10) */,
            1205156645 /* -5 dBFS (Index 11) */,
            1073656113 /* -6 dBFS (Index 12) */,
            956501250  /* -7 dBFS (Index 13) */,
            852136450  /* -8 dBFS (Index 14) */,
            759164679  /* -9 dBFS (Index 15) */,
            676326102  /* -10 dBFS (Index 16) */,
            602528766  /* -11 dBFS (Index 17) */,
            536814324  /* -12 dBFS (Index 18) */,
            478241517  /* -13 dBFS (Index 19) */,
            426058097  /* -14 dBFS (Index 20) */,
            379564639  /* -15 dBFS (Index 21) */,
            338148805  /* -16 dBFS (Index 22) */,
            301254308  /* -17 dBFS (Index 23) */,
            268383842  /* -18 dBFS (Index 24) */,
            239099499  /* -19 dBFS (Index 25) */,
            213012165  /* -20 dBFS (Index 26) */,
            189769395  /* -21 dBFS (Index 27) */,
            169062828  /* -22 dBFS (Index 28) */,
            150616147  /* -23 dBFS (Index 29) */,
            134181970  /* -24 dBFS (Index 30) */,
            119541178  /* -25 dBFS (Index 31) */,
            106497285  /* -26 dBFS (Index 32) */,
            94877112   /* -27 dBFS (Index 33) */,
            84524276   /* -28 dBFS (Index 34) */,
            75299446   /* -29 dBFS (Index 35) */,
            67083161   /* -30 dBFS (Index 36) */,
            59762145   /* -31 dBFS (Index 37) */,
            53241193   /* -32 dBFS (Index 38) */,
            47431415   /* -33 dBFS (Index 39) */,
            42255743   /* -34 dBFS (Index 40) */,
            37644919   /* -35 dBFS (Index 41) */,
            33537233   /* -36 dBFS (Index 42) */,
            29877717   /* -37 dBFS (Index 43) */,
            26617650   /* -38 dBFS (Index 44) */,
            23713210   /* -39 dBFS (Index 45) */,
            21126135   /* -40 dBFS (Index 46) */,
            18820925   /* -41 dBFS (Index 47) */,
            16767355   /* -42 dBFS (Index 48) */,
            14937748   /* -43 dBFS (Index 49) */,
            13307750   /* -44 dBFS (Index 50) */,
            11855639   /* -45 dBFS (Index 51) */,
            10562016   /* -46 dBFS (Index 52) */,
            9410118    /* -47 dBFS (Index 53) */,
            8383321    /* -48 dBFS (Index 54) */,
            7468551    /* -49 dBFS (Index 55) */,
            6653609    /* -50 dBFS (Index 56) */,
            5927606    /* -51 dBFS (Index 57) */,
            5280784    /* -52 dBFS (Index 58) */,
            4704537    /* -53 dBFS (Index 59) */,
            4191195    /* -54 dBFS (Index 60) */,
            3733854    /* -55 dBFS (Index 61) */,
            3326425    /* -56 dBFS (Index 62) */,
            2963451    /* -57 dBFS (Index 63) */,
            2640078    /* -58 dBFS (Index 64) */,
            2352011    /* -59 dBFS (Index 65) */,
            2095383    /* -60 dBFS (Index 66) */
        };

        const int8_t main_led_lut[24] = {
            -57, -54, -51, // LED 1 bis 3
            -48, -45, -42, // LED 4 bis 6
            -39, -36, -33, // LED 7 bis 9
            -30, -27, -24, // LED 10 bis 12
            -21, -18, -15, // LED 13 bis 15
            -12, -10,  -8, // LED 16 bis 18
            -6,   -4,  -3, // LED 19 bis 21 
            -2,   -1,   0  // LED 22 bis 24
        };

    public:

        void Log(const char* format, ...);
        void Error(const char* format, ...);
        unsigned int Checksum(char* str);
        int ReadConfig(const char* filename, const char* key, char* value_buffer, size_t buffer_size);
        
        float Fadervalue2DMX(uint16_t fadervalue);
        float Fadervalue2dBfs(uint16_t faderValue);
        uint16_t Oscvalue2Fadervalue(float oscValue);
        float Fadervalue2Oscvalue(uint16_t faderValue);
        uint16_t Dbfs2Fader(float dbfsValue);
        uint16_t DMX2Fadervalue(float DMXValue);
        float Dbfs2Oscvalue(float dbfsValue);
        float samplePu2Dbfs(float samplePu);
        float sample2Dbfs(uint32_t sample);
        int get_dbfs_from_peak_arm_opt(uint32_t raw_sample);
        uint8_t GetMeter6Info(int dbfs);
        uint8_t GetMeter8Info(int dbfs);
        uint32_t GetMeter24Info(int dbfs);

        uint8_t value2percent(float value, float value_min, float value_max);
        uint8_t value2percent(uint8_t value, uint8_t value_min, uint8_t value_max);
        uint8_t value2percent(int8_t value, int8_t value_min, int8_t value_max);
        
        long GetFileSize(const char* filename);
        String FormatFileSize(uint sizeByte, uint digits);
        void ReverseBitOrderArray(uint8_t* data, uint32_t len);
        uint32_t ReverseBitOrder_uint32(uint32_t n);
        float Saturate(float value, float min, float max);
        int CheckBoundaries(int value, int amount, int lowerbound, int upperbound);

        String MixerparameterAction2String(MixerparameterAction action);
        String MixerparameterCategoryToString(MP_CAT cat);

        String getIpAddress();

        bool IsInChannelBlock(uint8_t index, X32_VCHANNEL_BLOCK block);

        vector<string> split(string s, string delimiter);

        void starttimer(uint8_t timer);
        void stoptimer(uint8_t timer, const char* description);


        // Only show debug messages up to DEBUGLEVEL_..., e.g. DEBUGLEVEL_NORMAL
        void SetDebugLevel(uint8_t debuglevel)
        {
            debuglevel_ = debuglevel;
        }

        void SetDebugAll()
        {
            debug_ = 0b1111111111111111;            
        }

        #if BUILD_DEBUG
        #define DEBUG_DEF(name, bitvalue) \
            void name(int debuglevel, const char* format, ...) { \
                if (debuglevel <= debuglevel_ && ((debug_ & bitvalue) == bitvalue)) { \
                    va_list args; \
                    va_start(args, format); \
                    vprintf((String(#name) + String(": ") + String(format) + String("\n")).c_str(), args); \
                    fflush(stdout); \
                    va_end(args); \
                } \
            } \
            \
            /* Check if this Debugflag is enabled */ \
            bool name(int debuglevel=0) { \
                return (debuglevel <= debuglevel_ && ((debug_ & bitvalue) == bitvalue));   \
            } \
            \
            /* Enable or Disable Debugflag */ \
            void name(bool enabled) { \
                if(enabled){ \
                    debug_ |= bitvalue; \
                } else { \
                    debug_ &= ~bitvalue; \
                } \
            } 
        #else
        #define DEBUG_DEF(name, bitvalue) \
        void name(int debuglevel, const char* format, ...) { } \
        bool name(int debuglevel=0) { return false; } \
        void name(bool enabled) { } 
        #endif

        DEBUG_DEF(DEBUG_OSC,      0b0000000000000001);
        DEBUG_DEF(DEBUG_SURFACE,  0b0000000000000010);
        DEBUG_DEF(DEBUG_DMX,      0b0000000000000100);
        DEBUG_DEF(DEBUG_X32CTRL,  0b0000000000001000);
        DEBUG_DEF(DEBUG_ADDA,     0b0000000000010000);
        DEBUG_DEF(DEBUG_MIXER,    0b0000000000100000);
        DEBUG_DEF(DEBUG_GUI,      0b0000000001000000);
        DEBUG_DEF(DEBUG_SPI,      0b0000000010000000);
        DEBUG_DEF(DEBUG_DSP1,     0b0000000100000000);
        DEBUG_DEF(DEBUG_DSP2,     0b0000001000000000);
        DEBUG_DEF(DEBUG_FPGA,     0b0000010000000000);
        DEBUG_DEF(DEBUG_UART,     0b0000100000000000);
        DEBUG_DEF(DEBUG_INI,      0b0001000000000000);
        DEBUG_DEF(DEBUG_STATE,    0b0010000000000000);
        DEBUG_DEF(DEBUG_TIMER,    0b0100000000000000);
        DEBUG_DEF(DEBUG_FX,       0b1000000000000000);

        String intToHex(uint32_t val, uint8_t outputLength);
        uint32_t hexToInt(String hexString);
        String split(String s, char parser, int index);
        uint getNumberOfEntries(String s, char separator);
        String secondsToHmsHuman(uint32_t seconds);
        String secondsToHmsTechnical(uint32_t seconds, bool withDots);
        String intToStringTwoDigits(int value);
        String intToHexString(int value);
        float rescale(float input, float inputMin, float inputMax, float outputMin, float outputMax);
        int32_t rescale(int32_t input, int32_t inputMin, int32_t inputMax, int32_t outputMin, int32_t outputMax);
        float math_log(float number, float base);

        uint8_t CalculateWingChecksum(const uint8_t *payload, size_t len);
        String RoutingGetOutputNameByIndex(uint8_t index);
};

}
