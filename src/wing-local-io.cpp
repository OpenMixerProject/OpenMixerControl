#include "wing-local-io.h"

#ifdef TARGET_WING
#include <iterator>

namespace OMC::WingLocalIo {
namespace {

// WING Compact stock 3.1.1 APP (ui-ha-table.json, ha-map.json).
constexpr uint8_t gainCode[] = {0x01, 0x00, 0x03, 0x02, 0x05, 0x04, 0x07, 0x06, 0x0d, 0x0c,
    0x1d, 0x1c, 0x3d, 0x3c, 0x7d, 0x7c, 0xfd, 0xfc, 0xff, 0xfe};
constexpr uint8_t B(unsigned byte, unsigned bit) { return 8 * byte + bit; }
// Each row: phantom, then inverted gain code bits 7 through 0. Bytes go out MSB first.
constexpr uint8_t haBits[][9] = {
    {B(26,5), B(26,3), B(26,2), B(26,4), B(26,1), B(26,6), B(26,0), B(19,0), B(19,1)},
    {B(25,2), B(25,0), B(26,7), B(25,1), B(25,5), B(25,3), B(25,4), B(19,2), B(19,3)},
    {B(25,7), B(24,4), B(24,3), B(25,6), B(24,2), B(24,0), B(24,1), B(19,4), B(19,5)},
    {B(24,5), B(23,2), B(23,1), B(23,3), B(23,0), B(24,6), B(24,7), B(19,7), B(19,6)},
    {B(22,1), B(23,7), B(23,6), B(22,0), B(23,5), B(22,2), B(23,4), B(18,0), B(18,1)},
    {B(22,6), B(22,4), B(22,3), B(22,5), B(21,3), B(22,7), B(21,2), B(18,2), B(18,3)},
    {B(21,5), B(20,2), B(20,1), B(21,4), B(20,0), B(21,6), B(21,7), B(18,4), B(18,5)},
    {B(20,3), B(21,0), B(20,7), B(21,1), B(20,6), B(20,4), B(20,5), B(18,7), B(18,6)},
    {B(17,5), B(17,3), B(17,2), B(17,4), B(17,1), B(17,6), B(17,0), B(10,0), B(10,1)},
    {B(16,2), B(16,0), B(17,7), B(16,1), B(16,5), B(16,3), B(16,4), B(10,2), B(10,3)},
    {B(16,7), B(15,4), B(15,3), B(16,6), B(15,2), B(15,0), B(15,1), B(10,4), B(10,5)},
    {B(15,5), B(14,2), B(14,1), B(14,3), B(14,0), B(15,6), B(15,7), B(10,7), B(10,6)},
    {B(13,1), B(14,7), B(14,6), B(13,0), B(14,5), B(13,2), B(14,4), B(9,0), B(9,1)},
    {B(13,6), B(13,4), B(13,3), B(13,5), B(12,3), B(13,7), B(12,2), B(9,2), B(9,3)},
    {B(12,5), B(11,2), B(11,1), B(12,4), B(11,0), B(12,6), B(12,7), B(9,4), B(9,5)},
    {B(11,3), B(12,0), B(11,7), B(12,1), B(11,6), B(11,4), B(11,5), B(9,7), B(9,6)},
    {B(8,5), B(8,3), B(8,2), B(8,4), B(8,1), B(8,6), B(8,0), B(1,0), B(1,1)},
    {B(7,2), B(7,0), B(8,7), B(7,1), B(7,5), B(7,3), B(7,4), B(1,2), B(1,3)},
    {B(7,7), B(6,4), B(6,3), B(7,6), B(6,2), B(6,0), B(6,1), B(1,4), B(1,5)},
    {B(6,5), B(5,2), B(5,1), B(5,3), B(5,0), B(6,6), B(6,7), B(1,7), B(1,6)},
    {B(4,1), B(5,7), B(5,6), B(4,0), B(5,5), B(4,2), B(5,4), B(0,0), B(0,1)},
    {B(4,6), B(4,4), B(4,3), B(4,5), B(3,3), B(4,7), B(3,2), B(0,2), B(0,3)},
    {B(3,5), B(2,2), B(2,1), B(3,4), B(2,0), B(3,6), B(3,7), B(0,4), B(0,5)},
    {B(2,3), B(3,0), B(2,7), B(3,1), B(2,6), B(2,4), B(2,5), B(0,7), B(0,6)},
};
static_assert([] {
    bool used[27 * 8]{};
    for (const auto& row : haBits) for (auto bit : row) {
        if (bit >= std::size(used) || used[bit]) return false;
        used[bit] = true;
    }
    for (bool bit : used) if (!bit) return false;
    return true;
}());

struct Lane { const char* pad; const char* net; };
// Stock router simulation (ui-ha-table.json, out-dac-slot.json, io-assignment.json).
constexpr Lane adc[] = {{"G13", "AD0_DATA0"}, {"V11", "AD_AUX_D0"}, {"U10", "DA_AUX_D0"}};
constexpr Lane dac[] = {{"V10", "DA_I2S_D0"}, {"V14", "DA_I2S_D1"}, {"H14", "DA_I2S_D2"},
    {"U11", "DA_I2S_D3"}, {"G15", "DA_FSYNC"}};

}

bool EncodeHeadamp(HeadampImage& image, unsigned ha, unsigned step, bool phantom)
{
    if (ha >= std::size(haBits) || step >= std::size(gainCode)) return false;
    const auto put = [&](unsigned bit, bool high) {
        const unsigned mask = 1u << (bit % 8);
        auto& byte = image[bit / 8];
        byte = high ? byte | mask : byte & ~mask;
    };
    put(haBits[ha][0], phantom);
    for (unsigned i = 0; i < 8; ++i) put(haBits[ha][i + 1], !(gainCode[step] & (0x80u >> i)));
    return true;
}

const std::string& ReferenceJson()
{
    static const auto json = [] {
        std::string out = R"({"transport_available":false,"reason":"Current gateware has no ADDA transport","requires":"ADDA gateware (clocks, lanes and control CSRs), kernel SS3 and verified codec init","gain_step_max":)";
        out += std::to_string(std::size(gainCode) - 1);
        out += R"(,"inputs":[)";
        const auto lane = [&](const Lane& l, unsigned slot) {
            out += std::string(R"(,"pad":")") + l.pad + R"(","net":")" + l.net + R"(","slot":)" + std::to_string(slot) + '}';
        };
        for (unsigned ha = 0; ha < std::size(haBits); ++ha) {
            if (ha) out += ',';
            out += R"({"name":"LOCAL IN )" + std::to_string(ha + 1) + R"(","ha_index":)" + std::to_string(ha)
                + R"(,"stock_ae_source":)" + std::to_string(ha < 16 ? 0x1f0 + ha : ha - 16);
            lane(adc[ha / 8], ha % 8);
        }
        out += R"(],"outputs":[)";
        for (unsigned i = 0; i < 2 * std::size(dac); ++i) {
            if (i) out += ',';
            const bool aux = i >= 8;
            out += (aux ? R"({"name":"AUX OUT )" : R"({"name":"LOCAL OUT )")
                + std::to_string(aux ? 7 + (i - 8) : 1 + i) + R"(","stock_router_destination":)"
                + std::to_string(aux ? 0x1ee + (i - 8) : 0x1e0 + i);
            lane(dac[i / 2], i % 2);
        }
        return out + "]}";
    }();
    return json;
}

}
#endif
