#include "utils.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <format>
#include <gccore.h>
#include <mbedtls/sha1.h>
#include <mbedtls/sha512.h>
#include <mbedtls/base64.h>

static fstats stats ATTRIBUTE_ALIGN(32);
constexpr s32 ISFS_EEXIST = -105;
constexpr s32 ISFS_ENOENT = -106;

File* ISFS_GetFile(std::string_view path) {
  u32 size{};
  File* file = new File();

  s32 fd = ISFS_Open(path.data(), ISFS_OPEN_READ);
  if (fd < 0) {
    file->error = std::format("ISFS_GetFile: unable to open file {} (error {})\n", path, fd);
    file->error_code = fd;
    return file;
  }

  void *buf = nullptr;
  memset(&stats, 0, sizeof(fstats));

  s32 ret = ISFS_GetFileStats(fd, &stats);
  if (ret >= 0) {
    s32 length = stats.file_length;

    // We must align our length by 32.
    // memalign itself is dreadfully broken for unknown reasons.
    s32 aligned_length = length;
    s32 remainder = aligned_length % 32;
    if (remainder != 0) {
      aligned_length += 32 - remainder;
    }

    buf = aligned_alloc(32, aligned_length);

    if (buf != nullptr) {
      s32 tmp_size = ISFS_Read(fd, buf, length);
      if (tmp_size == length) {
        // We were successful.
        size = tmp_size;
      } else {
        // If positive, the file could not be fully read.
        // If negative, it is most likely an underlying /dev/fs
        // error.
        if (tmp_size >= 0) {
          file->error = std::format("ISFS_GetFile: only able to read {} out of {} bytes!", tmp_size, length);
          file->error_code = tmp_size;
        } else if (tmp_size == ISFS_ENOENT) {
          file->error = std::format("ISFS_GetFile: file not found (error {})", tmp_size);
          file->error_code = tmp_size;
        } else {
          file->error = std::format("ISFS_GetFile: ISFS_Open failed! (error {})", tmp_size);
          file->error_code = tmp_size;
        }

        free(buf);
      }
    } else {
      file->error = "ISFS_GetFile: failed to allocate buffer!";
      file->error_code = -1;
    }
  } else {
    file->error = std::format("ISFS_GetFile: unable to retrieve file stats (error {})", ret);
    file->error_code = ret;
  }
  ISFS_Close(fd);

  if (!file->error_code != 0) {
    file->data = buf;
    file->size = size;
  }

  return file;
}

inline std::string HexToString(const std::vector<u8> &hex) {
  static constexpr std::array lookup = {'0', '1', '2', '3', '4', '5',
                                                  '6', '7', '8', '9', 'a', 'b',
                                                  'c', 'd', 'e', 'f'};
  std::string str;
  str.reserve(hex.size() * 2);
  for (const unsigned char i : hex) {
    const u8 upper = static_cast<u8>((i >> 4) & 0xf);
    const u8 lower = static_cast<u8>(i & 0xf);
    str.push_back(lookup[upper]);
    str.push_back(lookup[lower]);
  }

  return str;
}

std::string Base64Encode(const unsigned char* data, size_t size) {
  size_t output_length;
  mbedtls_base64_encode(nullptr, 0, &output_length, data, size);

  std::vector<u8> vec(output_length);
  // Actually encode now that we have the length
  mbedtls_base64_encode(vec.data(), vec.size(), &output_length, data, size);

  return {reinterpret_cast<char*>(vec.data()), output_length};
}

std::array<u8, 160 / 8> SHA1Digest(const u8* msg, size_t len) {
  mbedtls_sha1_context ctx{};
  mbedtls_sha1_init(&ctx);
  mbedtls_sha1_starts(&ctx);
  mbedtls_sha1_update(&ctx, msg, len);

  std::array<u8, 160/8> vec{};
  mbedtls_sha1_finish(&ctx, vec.data());
  return vec;
}

std::string GetSerialNumber() {
  char serno[10];
  char code[4];
  __CONF_GetTxt("SERNO", serno, 10);
  __CONF_GetTxt("CODE", code, 4);

  return std::format("{}{}", code, serno);
}