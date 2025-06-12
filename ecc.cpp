// Based off of Dolphin Emulator who based off of twintig http://git.infradead.org/?p=users/segher/wii.git
// Copyright 2007,2008  Segher Boessenkool  <segher@kernel.crashing.org>
// Licensed under the terms of the GNU GPL, version 2
// http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt

#include "ecc.h"

namespace ec {
    std::array<u8, 60> PrivToPub(const u8* key)
    {
        const Point data = key * ec_G;
        std::array<u8, 60> result;
        std::copy_n(data.Data(), result.size(), result.begin());
        return result;
    }
}