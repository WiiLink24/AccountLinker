#include "wwfc.h"

#include <cstring>
#include <format>
#include <iostream>
#include <ogcsys.h>

#include "iosc.h"
#include "utils.h"

// From: https://github.com/WiiLink24/wfc-patcher-wii/blob/main/payload%2FwwfcLogin.cpp
static u64 DecodeUintString(const char* str, u32 bitCount)
{
    u64 value = 0;

    for (u32 n = 0; n < (bitCount >> 2); n++) {
        u8 nybble = 0;
        if (str[n] >= '0' && str[n] <= '9') {
            nybble = str[n] - '0';
        } else if (str[n] >= 'a' && str[n] <= 'f') {
            nybble = (str[n] - 'a') + 0xA;
        } else {
            return 0;
        }

        value |= u64(nybble) << ((bitCount - 4) - n * 4);
    }

    return value;
}

static bool IsZeroBlock(const void* block, u32 size)
{
    const u8* blockU8 = static_cast<const u8*>(block);

    for (u32 i = 0; i < size; i++) {
        if (blockU8[i] != 0) {
            return false;
        }
    }

    return true;
}

std::pair<std::string, bool> GetAuthTokenSignature(const u8* key) {
    struct IOSCECCCert {
        u32 signatureType;
        u8 signature[0x3C];
        u8 signaturePad[0x40];

        char issuer[0x40];
        u32 publicKeyType;
        char name[0x40];
        u32 timestamp;

        u8 publicKey[0x3C];
        u8 publicKeyPad[0x3C];
    };

    struct __attribute__((packed)) WWFCAuthSignature {
        /* 0x000:0x004 */ u32 deviceId;
        /* 0x004:0x008 */ u32 deviceTimestamp;
        /* 0x008:0x00C */ u32 caId;
        /* 0x00C:0x010 */ u32 msId;
        /* 0x010:0x018 */ u64 appTitleId;
        /* 0x018:0x054 */ u8 msSignature[0x3C];
        /* 0x054:0x090 */ u8 devicePublicKey[0x3C];
        /* 0x090:0x0CC */ u8 deviceSignature[0x3C];
        /* 0x0CC:0x108 */ u8 appPublicKey[0x3C];
        /* 0x108:0x144 */ u8 appSignature[0x3C];
        /* 0x144:0x148 */ u32 appTimestamp;
    };

    alignas(32) IOSCECCCert eccCert{};
    s32 ret = ES_GetDeviceCert(reinterpret_cast<u8*>(&eccCert));
    if (ret < 0) {
        return {std::format("ES_GetDeviceCert failed with: {}", ret), true};
    }

    WWFCAuthSignature authSig = {};
    authSig.deviceId = DecodeUintString(eccCert.name + 2, 32);
    authSig.deviceTimestamp = eccCert.timestamp;

    if (authSig.deviceId == 0 || eccCert.signatureType != 0x00010002 ||
        eccCert.publicKeyType != 2 ||
        !IsZeroBlock(eccCert.signaturePad, sizeof(eccCert.signaturePad)) ||
        !IsZeroBlock(eccCert.publicKeyPad, sizeof(eccCert.publicKeyPad)) ||
        std::memcmp(eccCert.name, "NG", 2) != 0 ||
        !IsZeroBlock(eccCert.name + 0xA, 0x40 - 0xA)) {
        return {"Invalid device certificate", true};
        }

    authSig.caId = DecodeUintString(eccCert.issuer + 7, 32);
    authSig.msId = DecodeUintString(eccCert.issuer + 18, 32);

    if (authSig.caId == 0 || authSig.msId == 0 ||
    std::memcmp(eccCert.issuer, "Root-CA", 7) != 0 ||
    std::memcmp(eccCert.issuer + 0xF, "-MS", 3) != 0) {
        return {"Invalid device certificate issuer", true};
    }

    std::memcpy(authSig.msSignature, eccCert.signature, 0x3C);
    std::memcpy(authSig.devicePublicKey, eccCert.publicKey, 0x3C);

    // Setup app issuer for sanity checking later
    char appIssuer[0x40];
    std::memcpy(appIssuer, eccCert.issuer, 0x1A);
    appIssuer[0x1A] = '-';
    std::memcpy(appIssuer + 0x1B, eccCert.name, 0xA);

    // Now call ES sign
    alignas(32) u8 eccSignature[0x3C + 0x4] = {};
    alignas(64) char authTokenAligned[GP_AUTHTOKEN_LEN] = {};

    s32 authTokenSize = std::strlen(AUTH_TOKEN);
    if (authTokenSize > GP_AUTHTOKEN_LEN) {
        authTokenSize = GP_AUTHTOKEN_LEN;
    }
    std::memcpy(authTokenAligned, AUTH_TOKEN, authTokenSize);

    eccCert = {};


    IOSC::Sign(key, eccSignature, reinterpret_cast<u8*>(&eccCert), 0, reinterpret_cast<const u8 *>(&authTokenAligned), authTokenSize, authSig.caId, authSig.msId, authSig.deviceId);
    authSig.appTitleId = DecodeUintString(eccCert.name + 2, 64);
    authSig.appTimestamp = eccCert.timestamp;

    if (
        !IsZeroBlock(eccCert.signaturePad, sizeof(eccCert.signaturePad)) ||
        !IsZeroBlock(eccCert.publicKeyPad, sizeof(eccCert.publicKeyPad)) ||
        std::memcmp(eccCert.name, "AP", 2) != 0 ||
        !IsZeroBlock(eccCert.name + 0x12, 0x40 - 0x12)) {
        return {"Invalid app certificate", true};
        }

    if (std::memcmp(eccCert.issuer, appIssuer, 0x25) != 0 ||
        !IsZeroBlock(eccCert.issuer + 0x25, 0x40 - 0x25)) {
        return {"Invalid app certificate issuer", true};
        }

    std::memcpy(authSig.deviceSignature, eccCert.signature, 0x3C);
    std::memcpy(authSig.appPublicKey, eccCert.publicKey, 0x3C);
    std::memcpy(authSig.appSignature, eccSignature, 0x3C);

    std::string b64 = Base64Encode(reinterpret_cast<const unsigned char*>(&authSig), sizeof(authSig));
    return {b64, false};
}