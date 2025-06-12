#pragma once

#include "ecc.h"

namespace IOSC {
    CertECC MakeBlankEccCert(std::string_view issuer, std::string_view name,
                                const u8* private_key, u32 key_id);

    void Sign(u8* sig_out, u8* ap_cert_out, u64 title_id, const u8* data, u32 data_size, u32 ca_id, u32 ms_id, u32 device_id);
}