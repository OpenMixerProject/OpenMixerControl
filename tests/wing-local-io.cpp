#include "../src/wing-local-io.h"
#include <cassert>
#include <charconv>
#include <string_view>
#include <tuple>

using namespace OMC::WingLocalIo;

static HeadampImage Hex(std::string_view text)
{
    HeadampImage bytes{};
    assert(text.size() == 2 * bytes.size());
    for (unsigned i = 0; i < bytes.size(); ++i) {
        const auto* start = text.data() + 2 * i;
        const auto [end, error] = std::from_chars(start, start + 2, bytes[i], 16);
        assert(error == std::errc{} && end == start + 2);
    }
    return bytes;
}

int main()
{
    // Independent SS3 payloads captured from emulated stock 3.1.1 configure_ha.
    HeadampImage off{};
    for (unsigned ha = 0; ha < 24; ++ha) assert(EncodeHeadamp(off, ha, 0, false));
    assert(off == Hex("9595f7dfbdffdf7bdf9595f7dfbdffdf7bdf9595f7dfbdffdf7bdf"));
    for (const auto& [ha, step, phantom, expected] : {
        std::tuple<unsigned, unsigned, bool, const char*>{0, 19, false, "9595f7dfbdffdf7bdf9595f7dfbdffdf7bdf9596f7dfbdffdf7b80"},
        {8, 5, true, "9595f7dfbdffdf7bdf9597f7dfbdffdf7bfe9595f7dfbdffdf7bdf"},
        {23, 19, true, "55950fdcbdffdf7bdf9595f7dfbdffdf7bdf9595f7dfbdffdf7bdf"},
    }) {
        auto image = off;
        assert(EncodeHeadamp(image, ha, step, phantom));
        assert(image == Hex(expected));
        assert(!EncodeHeadamp(image, 24, step, phantom));
        assert(!EncodeHeadamp(image, ha, 20, phantom));
        assert(image == Hex(expected));
    }
}
