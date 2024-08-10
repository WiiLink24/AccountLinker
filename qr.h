#pragma once

#include <array>
#include <cstring>
#include <gccore.h>
#include <mbedtls/sha512.h>
#include <png.h>
#include <qrencode.h>
#include <string>
#include <vector>

#include "config.h"

// URL for linking.
constexpr char URL[] = "https://demae.wiilink.ca?h=";

std::string HexToString(const std::vector<u8> &hex) {
  static constexpr std::array<char, 16> lookup = {'0', '1', '2', '3', '4', '5',
                                                  '6', '7', '8', '9', 'a', 'b',
                                                  'c', 'd', 'e', 'f'};
  std::string str;
  str.reserve(hex.size() * 2);
  for (unsigned char i : hex) {
    const u8 upper = static_cast<u8>((i >> 4) & 0xf);
    const u8 lower = static_cast<u8>(i & 0xf);
    str.push_back(lookup[upper]);
    str.push_back(lookup[lower]);
  }

  return str;
}

void encode_qr_code_to_png() {
  auto *config = new NWC24Config();
  std::string_view str = config->GetPassword();

  mbedtls_sha512_context ctx{};
  mbedtls_sha512_init(&ctx);
  mbedtls_sha512_starts(&ctx, 0);
  mbedtls_sha512_update(
      &ctx, reinterpret_cast<const unsigned char *>(str.data()), str.length());

  std::vector<u8> vec(64);
  mbedtls_sha512_finish(&ctx, vec.data());

  std::string string(URL);
  string.append(HexToString(vec));

  QRcode *qr =
      QRcode_encodeString(string.data(), 1, QR_ECLEVEL_L, QR_MODE_8, 0);
  if (qr == nullptr) {
    exit(0);
  }

  static FILE *fp;
  png_structp png_ptr;
  png_infop info_ptr;
  unsigned char *row, *p, *q;
  int x, y, xx, yy, bit;
  int realwidth;
  const int margin = 0;
  const int size = 1;

  realwidth = (qr->width + margin * 2) * size;
  row = (unsigned char *)malloc((realwidth + 7) / 8);
  if (row == NULL) {
    fprintf(stderr, "Failed to allocate memory.\n");
    exit(EXIT_FAILURE);
  }

  fp = fopen("fat:/qr.png", "wb");
  if (fp == NULL) {
    fprintf(stderr, "Failed to create file: %s\n", "tro");
    perror(NULL);
    exit(EXIT_FAILURE);
  }

  png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
  if (png_ptr == NULL) {
    fclose(fp);
    fprintf(stderr, "Failed to initialize PNG writer.\n");
    exit(EXIT_FAILURE);
  }

  info_ptr = png_create_info_struct(png_ptr);
  if (info_ptr == NULL) {
    fclose(fp);
    fprintf(stderr, "Failed to initialize PNG write.\n");
    exit(EXIT_FAILURE);
  }

  if (setjmp(png_jmpbuf(png_ptr))) {
    png_destroy_write_struct(&png_ptr, &info_ptr);
    fclose(fp);
    fprintf(stderr, "Failed to write PNG image.\n");
    exit(EXIT_FAILURE);
  }

  png_init_io(png_ptr, fp);
  png_set_IHDR(png_ptr, info_ptr, realwidth, realwidth, 1, PNG_COLOR_TYPE_GRAY,
               PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT,
               PNG_FILTER_TYPE_DEFAULT);
  png_write_info(png_ptr, info_ptr);

  /* top margin */
  memset(row, 0xff, (realwidth + 7) / 8);
  for (y = 0; y < margin * size; y++) {
    png_write_row(png_ptr, row);
  }

  /* data */
  p = qr->data;
  for (y = 0; y < qr->width; y++) {
    bit = 7;
    memset(row, 0xff, (realwidth + 7) / 8);
    q = row;
    q += margin * size / 8;
    bit = 7 - (margin * size % 8);
    for (x = 0; x < qr->width; x++) {
      for (xx = 0; xx < size; xx++) {
        *q ^= (*p & 1) << bit;
        bit--;
        if (bit < 0) {
          q++;
          bit = 7;
        }
      }
      p++;
    }
    for (yy = 0; yy < size; yy++) {
      png_write_row(png_ptr, row);
    }
  }
  /* bottom margin */
  memset(row, 0xff, (realwidth + 7) / 8);
  for (y = 0; y < margin * size; y++) {
    png_write_row(png_ptr, row);
  }

  png_write_end(png_ptr, info_ptr);
  png_destroy_write_struct(&png_ptr, &info_ptr);

  fclose(fp);
  free(row);
}

unsigned char *readPNG(const char *filename, int &width, int &height) {
  FILE *fp = fopen(filename, "rb");
  if (!fp) {
    perror("File opening failed");
    return nullptr;
  }

  png_structp png =
      png_create_read_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);
  if (!png)
    return nullptr;

  png_infop info = png_create_info_struct(png);
  if (!info)
    return nullptr;

  if (setjmp(png_jmpbuf(png)))
    return nullptr;

  png_init_io(png, fp);
  png_read_info(png, info);

  width = png_get_image_width(png, info);
  height = png_get_image_height(png, info);
  png_byte color_type = png_get_color_type(png, info);
  png_byte bit_depth = png_get_bit_depth(png, info);

  if (bit_depth == 16)
    png_set_strip_16(png);

  if (color_type == PNG_COLOR_TYPE_PALETTE)
    png_set_palette_to_rgb(png);

  if (color_type == PNG_COLOR_TYPE_GRAY && bit_depth < 8)
    png_set_expand_gray_1_2_4_to_8(png);

  if (png_get_valid(png, info, PNG_INFO_tRNS))
    png_set_tRNS_to_alpha(png);

  if (color_type == PNG_COLOR_TYPE_RGB || color_type == PNG_COLOR_TYPE_GRAY ||
      color_type == PNG_COLOR_TYPE_PALETTE)
    png_set_filler(png, 0xFF, PNG_FILLER_AFTER);

  if (color_type == PNG_COLOR_TYPE_GRAY ||
      color_type == PNG_COLOR_TYPE_GRAY_ALPHA)
    png_set_gray_to_rgb(png);

  png_read_update_info(png, info);

  unsigned char *image_data =
      (unsigned char *)malloc(png_get_rowbytes(png, info) * height);
  png_bytep *row_pointers = (png_bytep *)malloc(sizeof(png_bytep) * height);

  for (int y = 0; y < height; y++) {
    row_pointers[y] = (png_byte *)image_data + y * png_get_rowbytes(png, info);
  }

  png_read_image(png, row_pointers);
  fclose(fp);

  if (png && info)
    png_destroy_read_struct(&png, &info, nullptr);
  if (row_pointers)
    free(row_pointers);

  return image_data;
}

// Function to write a PNG file
void writePNG(const char *filename, unsigned char *image_data, int width,
              int height) {
  FILE *fp = fopen(filename, "wb");
  if (!fp) {
    perror("File opening failed");
    return;
  }

  png_structp png =
      png_create_write_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);
  if (!png)
    return;

  png_infop info = png_create_info_struct(png);
  if (!info)
    return;

  if (setjmp(png_jmpbuf(png)))
    return;

  png_init_io(png, fp);

  png_set_IHDR(png, info, width, height, 8, PNG_COLOR_TYPE_RGBA,
               PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT,
               PNG_FILTER_TYPE_DEFAULT);
  png_write_info(png, info);

  png_bytep row_pointers[height];
  for (int y = 0; y < height; y++) {
    row_pointers[y] = (png_bytep)(image_data + y * width * 4);
  }

  png_write_image(png, row_pointers);
  png_write_end(png, nullptr);

  fclose(fp);

  if (png && info)
    png_destroy_write_struct(&png, &info);
}

// Function to resize an image using nearest neighbor algorithm
unsigned char *resizeImage(unsigned char *image_data, int width, int height,
                           int new_width, int new_height) {
  unsigned char *resized_data =
      (unsigned char *)malloc(new_width * new_height * 4);

  for (int y = 0; y < new_height; y++) {
    for (int x = 0; x < new_width; x++) {
      int src_x = x * width / new_width;
      int src_y = y * height / new_height;
      for (int c = 0; c < 4; c++) {
        resized_data[(y * new_width + x) * 4 + c] =
            image_data[(src_y * width + src_x) * 4 + c];
      }
    }
  }

  return resized_data;
}
