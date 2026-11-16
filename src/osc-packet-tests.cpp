#include "osc-packet.h"
#include "../lib_ext/doctest/doctest/doctest.h"

TEST_CASE("OSC control packet validates bounds before SmallOSC")
{
    char data[128]{};
    tosc_message message;
    int size = tosc_writeMessage(data, sizeof(data), "/get/state/1", "");
    CHECK(OMC::ParseControlPacket(&message, data, size));
    size = tosc_writeMessage(data, sizeof(data), "/subscribe/state/1", "f", 1.0f);
    CHECK(OMC::ParseControlPacket(&message, data, size));
    CHECK_FALSE(OMC::ParseControlPacket(&message, data, size - 4));
    memset(data, 'x', sizeof(data));
    data[0] = '/';
    CHECK_FALSE(OMC::ParseControlPacket(&message, data, sizeof(data)));
    data[4] = 0;
    CHECK_FALSE(OMC::ParseControlPacket(&message, data, sizeof(data)));
    size = tosc_writeMessage(data, sizeof(data), "/set/name/1", "s", "Vocal");
    CHECK(OMC::ParseControlPacket(&message, data, size));
    size = tosc_writeMessage(data, sizeof(data), "/set/debug/1", "ff", 1.0f, 2.0f);
    CHECK_FALSE(OMC::ParseControlPacket(&message, data, size));
}
