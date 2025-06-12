#pragma once

#include <string>

constexpr char AUTH_TOKEN[] = "WIILINK_ACCOUNT_LINKER";
constexpr int GP_AUTHTOKEN_LEN = 256;

std::pair<std::string, bool> GetAuthTokenSignature();