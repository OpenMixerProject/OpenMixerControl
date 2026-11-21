// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#ifdef TARGET_WING
namespace OMC {

// Throws unless the requested core is boot-qualified and answers WDSP Identify.
void IdentifyWingDsp(unsigned core);

} // namespace OMC
#endif
