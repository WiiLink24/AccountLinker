#include "config.h"
#include "utils.h"
#include <cstring>
#include <iostream>
#include <string_view>

constexpr char CONFIG_PATH[] = "/shared2/wc24/nwc24msg.cfg";


bool NWC24Config::ReadConfig() {
  File* file = ISFS_GetFile(CONFIG_PATH);
  if (file->error_code != 0) {
    std::cout << file->error << std::endl;
    return false;
  }

  m_data = *(static_cast<ConfigData *>(file->data));
  return true;
}

u64 NWC24Config::GetWiiNumber() const { return {m_data.nwc24_id}; }