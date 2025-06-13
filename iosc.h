#pragma once

#include "ecc.h"

namespace IOSC {
    // I only need ng_priv.
    struct BootMiiKeyDump {
        std::array<char, 256> creator;
        std::array<u8, 20> boot1_hash;  // 0x100
        std::array<u8, 16> common_key;  // 0x114
        u32 ng_id;                      // 0x124
        union
        {
            struct
            {
                std::array<u8, 0x1e> ng_priv;  // 0x128
                std::array<u8, 0x12> pad1;
            };
            struct
            {
                std::array<u8, 0x1c> pad2;
                std::array<u8, 0x14> nand_hmac;  // 0x144
            };
        };
    };

    CertECC MakeBlankEccCert(std::string_view issuer, std::string_view name,
                                const u8* private_key, u32 key_id);

    void Sign(const u8* console_key, u8* sig_out, u8* ap_cert_out, u64 title_id, const u8* data, u32 data_size, u32 ca_id, u32 ms_id, u32 device_id);
}