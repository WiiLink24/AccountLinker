#include "gui/gui.h"
#include "menu.h"
#include "qr.h"
#include "noto_sans_jp_regular.otf.h"
#include <fat.h>
#include <gccore.h>
#include <iostream>
#include <sdcard/wiisd_io.h>
#include <unistd.h>

const DISC_INTERFACE *sd_slot = &__io_wiisd;
const DISC_INTERFACE *usb = &__io_usbstorage;

s32 init_fat() {
  // Initialize IO
  usb->startup();
  sd_slot->startup();

  // Check if the SD Card is inserted
  bool isInserted = __io_wiisd.isInserted();

  // Try to mount the SD Card before the USB
  if (isInserted) {
    fatMountSimple("fat", sd_slot);
  } else {
    // Since the SD Card is not inserted, we will attempt to mount the USB.
    bool USB = __io_usbstorage.isInserted();
    if (USB) {
      fatMountSimple("fat", usb);
    } else {
      __io_usbstorage.shutdown();
      __io_wiisd.shutdown();
      printf("No SD Card or USB is inserted");
      sleep(5);
      return -1;
    }
  }

  return 0;
}

static void power_cb() {
  ExitRequested = true;
  // We don't exit here at a risk of the threads being desynced, causing crashes
  exitType = ExitType::POWER;
}

static void reset_cb(u32 level, void *unk) {
  ExitRequested = true;
  exitType = ExitType::WII_MENU;
}

int main() {
  // Make hardware buttons functional.
  SYS_SetPowerCallback(power_cb);
  SYS_SetResetCallback(reset_cb);

  InitVideo();
  ISFS_Initialize();
  CONF_Init();
  SetupPads();

  InitAudio();
  InitGUIThreads();
  InitFreeType((u8 *)noto_sans_jp_regular_otf, noto_sans_jp_regular_otf_size);
  init_fat();

  // We first encode our URL to a PNG.
  encode_qr_code_to_png();

  // We are unable to encode the PNG in a dimension that we want, so we must
  // resize ourselves.
  FILE *fp = fopen("fat:/qr.png", "rb");
  fseek(fp, 0L, SEEK_END);
  size_t sz = ftell(fp);
  fseek(fp, 0L, SEEK_SET);

  u8 *buf = static_cast<u8 *>(malloc(sz + 1));
  fread(buf, 1, sz, fp);
  int width, height;

  u8 *png_data = readPNG("fat:/qr.png", width, height);
  u8 *resized = resizeImage(png_data, width, height, 200, 200);
  writePNG("fat:/qr.png", resized, 200, 200);

  MainMenu();
  return 0;
}
