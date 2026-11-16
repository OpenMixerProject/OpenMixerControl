#pragma once

#include "../lib_ext/small-osc/small-osc.h"
#include <cstring>

namespace OMC
{
    // SmallOSC's parser scans unboundedly for NUL/comma and its argument
    // readers assume complete data. Validate our single-argument control
    // protocol before calling it. Bundles/multiple arguments are not supported.
    inline bool ParseControlPacket(tosc_message* message, char* data, size_t size)
    {
        if (size < 8 || data[0] != '/' || size % 4) { return false; }
        auto* end = static_cast<char*>(memchr(data, 0, size));
        if (!end) { return false; }
        size_t format = (end - data + 4) & ~size_t(3);
        if (format + 4 > size || data[format] != ',') { return false; }
        end = static_cast<char*>(memchr(data + format + 1, 0, size - format - 1));
        if (!end) { return false; }
        size_t count = end - (data + format + 1);
        if (count > 1) { return false; }
        size_t argument = (end - data + 4) & ~size_t(3);
        if (!count) { if (argument != size) { return false; } }
        else if (data[format + 1] == 'f' || data[format + 1] == 'i')
        { if (argument + 4 != size) { return false; } }
        else if (data[format + 1] == 's')
        { if (argument >= size || !memchr(data + argument, 0, size - argument)) { return false; } }
        else { return false; }
        return tosc_parseMessage(message, data, size) == 0;
    }
}
