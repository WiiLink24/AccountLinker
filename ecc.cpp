// Based off of Dolphin Emulator who based off of twintig http://git.infradead.org/?p=users/segher/wii.git
// Copyright 2007,2008  Segher Boessenkool  <segher@kernel.crashing.org>
// Licensed under the terms of the GNU GPL, version 2
// http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt

#include "ecc.h"
#include "bn.h"

#include <mbedtls/entropy.h>
#include <mbedtls/hmac_drbg.h>
#include <cstring>

namespace ec {
    std::array<u8, 60> PrivToPub(const u8* key)
    {
        const Point data = key * ec_G;
        std::array<u8, 60> result;
        std::copy_n(data.Data(), result.size(), result.begin());
        return result;
    }

    static void GenerateRandom(void* buffer, size_t size) {
        mbedtls_entropy_context entropy;
        mbedtls_hmac_drbg_context context;

        mbedtls_entropy_init(&entropy);
        mbedtls_hmac_drbg_init(&context);
        mbedtls_hmac_drbg_seed(&context, mbedtls_md_info_from_type(MBEDTLS_MD_SHA256), mbedtls_entropy_func, &entropy, nullptr, 0);
        mbedtls_hmac_drbg_random(&context, static_cast<u8*>(buffer), size);
        mbedtls_hmac_drbg_free(&context);
        mbedtls_entropy_free(&entropy);
    }

    std::array<u8, 60> Sign(const u8* key, const u8* hash)
    {
        u8 e[30]{};
        std::memcpy(e + 10, hash, 20);

        u8 m[30];
        do
        {
            // Generate 240 bits and keep 233.
            GenerateRandom(m, sizeof(m));
            m[0] &= 1;
        } while (bn_compare(m, ec_N, sizeof(m)) >= 0);

        Elt r = (m * ec_G).X();
        if (bn_compare(r.data.data(), ec_N, 30) >= 0)
            bn_sub_modulus(r.data.data(), ec_N, 30);

        //	S = m**-1*(e + Rk) (mod N)

        u8 kk[30];
        std::copy_n(key, sizeof(kk), kk);
        if (bn_compare(kk, ec_N, sizeof(kk)) >= 0)
            bn_sub_modulus(kk, ec_N, sizeof(kk));
        Elt s;
        bn_mul(s.data.data(), r.data.data(), kk, ec_N, 30);
        bn_add(kk, s.data.data(), e, ec_N, sizeof(kk));
        u8 minv[30];
        bn_inv(minv, m, ec_N, sizeof(minv));
        bn_mul(s.data.data(), minv, kk, ec_N, 30);

        std::array<u8, 60> signature{};
        std::ranges::copy(r.data, signature.begin());
        std::ranges::copy(s.data, signature.begin() + 30);
        return signature;
    }
}
