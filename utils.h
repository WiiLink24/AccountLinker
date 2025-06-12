#pragma once

#include <ogcsys.h>
#include <string>
#include <array>
#include <vector>

void *ISFS_GetFile(const char *path, u32 *size);
std::string HexToString(const std::vector<u8> &hex);
std::string SHA512Encode(std::string_view str);
std::string Base64Encode(const unsigned char* data, size_t size);