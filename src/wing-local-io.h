#pragma once

#include <array>
#include <cstdint>
#include <string>

namespace OMC::WingLocalIo {

using HeadampImage = std::array<uint8_t, 27>;
// Offline image edit only. Invalid indices/steps leave the image untouched.
bool EncodeHeadamp(HeadampImage& image, unsigned ha, unsigned step, bool phantom);

// Stock reference metadata, never a SPIBone routing or write capability.
const std::string& ReferenceJson();

}
