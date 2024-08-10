#include "utils.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <gccore.h>

static fstats stats ATTRIBUTE_ALIGN(32);
constexpr s32 ISFS_EEXIST = -105;
constexpr s32 ISFS_ENOENT = -106;

void *ISFS_GetFile(const char *path, u32 *size) {
  *size = 0;

  s32 fd = ISFS_Open(path, ISFS_OPEN_READ);
  if (fd < 0) {
    printf("ISFS_GetFile: unable to open file (error %d)\n", fd);
    return nullptr;
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
        *size = tmp_size;
      } else {
        // If positive, the file could not be fully read.
        // If negative, it is most likely an underlying /dev/fs
        // error.
        if (tmp_size >= 0) {
          printf("ISFS_GetFile: only able to read %d out of "
                 "%d bytes!\n",
                 tmp_size, length);
        } else if (tmp_size == ISFS_ENOENT) {
          // We ignore logging errors about files that do not exist.
        } else {
          printf("ISFS_GetFile: ISFS_Open failed! (error %d)\n", tmp_size);
        }

        free(buf);
      }
    } else {
      printf("ISFS_GetFile: failed to allocate buffer!\n");
    }
  } else {
    printf("ISFS_GetFile: unable to retrieve file stats (error %d)\n", ret);
  }
  ISFS_Close(fd);

  return buf;
}