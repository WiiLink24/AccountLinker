#pragma once

#include <ogcsys.h>
#include <string>
#include <array>
#include <vector>
#include <iostream>

const std::string version = "v2.0.1";

struct File {
    void* data;
    size_t size;
    std::string error;
    s32 error_code;
};

File* ISFS_GetFile(std::string_view path);
std::string HexToString(const std::vector<u8> &hex);
std::string Base64Encode(const unsigned char* data, size_t size);
std::array<u8, 160 / 8> SHA1Digest(const u8* msg, size_t len);

inline void PrintHeader() {
    std::cout << std::endl;
    std::cout << std::endl;
    std::cout << std::endl;
    std::cout << "WiiLink Account Linker - (c) 2025 WiiLink" << std::endl;
    std::cout << version << std::endl;
    std::cout << std::endl;
}

inline void ClearScreen() {
    std::cout << "\x1b[2J";
}