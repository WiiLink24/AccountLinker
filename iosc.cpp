//
// Created by Noah Pistilli on 2025-06-12.
//

#include "iosc.h"

#include <cstring>
#include <format>

#include "utils.h"

namespace IOSC {
    // https://github.com/dolphin-emu/dolphin/blob/a16387741383ca00524f78f9854c55be7089cf93/Source/Core/Core/IOS/IOSC.cpp#L508
    CertECC MakeBlankEccCert(std::string_view issuer, std::string_view name, const u8 *private_key, u32 key_id) {
        CertECC cert{};
        cert.signature.type = 0x00010002;
        issuer.copy(cert.signature.issuer, sizeof(cert.signature.issuer) - 1);
        cert.header.public_key_type = 2;
        name.copy(cert.header.name, sizeof(cert.header.name) - 1);
        cert.header.id = key_id;
        cert.public_key = ec::PrivToPub(private_key);
        return cert;
    }

    // https://github.com/dolphin-emu/dolphin/blob/a16387741383ca00524f78f9854c55be7089cf93/Source/Core/Core/IOS/IOSC.cpp#L530
    void Sign(const u8* console_key, u8 *sig_out, u8 *ap_cert_out, u64 title_id, const u8 *data, u32 data_size, u32 ca_id, u32 ms_id, u32 device_id) {
        std::array<u8, 30> ap_priv{};

        ap_priv[0x1d] = 1;

        const std::string signer =
            std::format("Root-CA{:08x}-MS{:08x}-NG{:08x}", ca_id, ms_id, device_id);
        const std::string name = std::format("AP{:016x}", title_id);
        CertECC cert = MakeBlankEccCert(signer, name, ap_priv.data(), 0);
        // Sign the AP cert.
        const size_t skip = offsetof(CertECC, signature.issuer);
        const auto ap_cert_digest =
            SHA1Digest(reinterpret_cast<const u8*>(&cert) + skip, sizeof(cert) - skip);
        cert.signature.sig =
            ec::Sign(console_key, ap_cert_digest.data());
        std::memcpy(ap_cert_out, &cert, sizeof(cert));

        // Sign the data.
        const auto data_digest = SHA1Digest(data, data_size);
        const auto signature = ec::Sign(ap_priv.data(), data_digest.data());
        std::ranges::copy(signature, sig_out);
    }

}
