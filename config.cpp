#include "config.h"
#include "utils.h"
#include <cstring>
#include <iostream>
#include <string_view>

constexpr const char CONFIG_PATH[] = "/shared2/wc24/nwc24msg.cfg";

NWC24Config::NWC24Config() { ReadConfig(); }

void NWC24Config::ReadConfig() {
  u32 readSize;
  void *fileBuffer = ISFS_GetFile(CONFIG_PATH, &readSize);
  m_data = *(reinterpret_cast<ConfigData *>(fileBuffer));
}

std::string_view NWC24Config::GetPassword() const { return {m_data.paswd}; }