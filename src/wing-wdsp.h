// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#ifdef TARGET_WING
#include <array>

namespace OMC {

// Throws unless the requested core is boot-qualified and answers WDSP Identify.
void IdentifyWingDsp(unsigned core);

// DSP4 pair 0 only. Values have WDSP units; nullptr requests bypass.
void SetWingOverdrive(const std::array<float, 6>* values);

} // namespace OMC
#endif
