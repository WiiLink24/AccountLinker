#include "menu.h"
#include "gui/gui.h"
#include <ogcsys.h>
#include <unistd.h>

#include "data/bg_stripes.png.h"
#include "data/channel_gradient_bottom.png.h"
#include "data/channel_gradient_top.png.h"

#define THREAD_SLEEP 100
// 48 KiB was chosen after many days of testing.
// It horrifies the author.
#define GUI_STACK_SIZE (48 * 1024)

static GuiImage *bgImg = nullptr;
static GuiWindow *mainWindow = nullptr;
static lwp_t guithread = LWP_THREAD_NULL;
static bool guiHalt = true;
static bool startApp = false;
bool ExitRequested = false;
ExitType exitType = ExitType::WII_MENU;

/****************************************************************************
 * ResumeGui
 *
 * Signals the GUI thread to start, and resumes the thread. This is called
 * after finishing the removal/insertion of new elements, and after initial
 * GUI setup.
 ***************************************************************************/
static void ResumeGui() {
  guiHalt = false;
  LWP_ResumeThread(guithread);
}

/****************************************************************************
 * HaltGui
 *
 * Signals the GUI thread to stop, and waits for GUI thread to stop
 * This is necessary whenever removing/inserting new elements into the GUI.
 * This eliminates the possibility that the GUI is in the middle of accessing
 * an element that is being changed.
 ***************************************************************************/
static void HaltGui() {
  guiHalt = true;

  // wait for thread to finish
  while (!LWP_ThreadIsSuspended(guithread))
    usleep(THREAD_SLEEP);
}

[[noreturn]] static void ExitApp() {
  if (exitType == WII_MENU)
    WII_ReturnToMenu();

  STM_ShutdownToIdle();

  __builtin_unreachable();
}

/****************************************************************************
 * UpdateGUI
 *
 * Primary thread to allow GUI to respond to state changes, and draws GUI
 ***************************************************************************/

[[noreturn]] static void *UpdateGUI(void *arg) {
  int i;

  while (true) {
    if (guiHalt) {
      LWP_SuspendThread(guithread);
    } else {
      mainWindow->Draw();

      // Run this for loop only once
      if (!startApp) {
        for (i = 255; i >= 0; i -= 15) {
          mainWindow->Draw();
          Menu_DrawRectangle(0, 0, static_cast<float>(screenwidth),
                             static_cast<float>(screenheight),
                             (GXColor){0, 0, 0, (u8)i}, 1);
          Menu_Render();
        }

        startApp = true;
      }

      Menu_Render();

      if (ExitRequested) {
        for (i = 0; i <= 255; i += 15) {
          mainWindow->Draw();
          Menu_DrawRectangle(0, 0, static_cast<float>(screenwidth),
                             static_cast<float>(screenheight),
                             (GXColor){0, 0, 0, (u8)i}, 1);
          Menu_Render();
        }

        ExitApp();
      }
    }
  }
}

static u8 *_gui_stack[GUI_STACK_SIZE] ATTRIBUTE_ALIGN(8);
void InitGUIThreads() {
  LWP_CreateThread(&guithread, UpdateGUI, nullptr, _gui_stack, GUI_STACK_SIZE,
                   70);
}

[[noreturn]] void MainMenu() {
  // int currentMenu = menu;

  mainWindow = new GuiWindow(screenwidth, screenheight);

  bgImg = new GuiImage(screenwidth, screenheight, (GXColor){0, 0, 0, 255});

  // Create a white stripe beneath the title and above actionable buttons.
  bgImg->ColorStripe(75, (GXColor){0xff, 0xff, 0xff, 255});
  bgImg->ColorStripe(76, (GXColor){0xff, 0xff, 0xff, 255});
  bgImg->ColorStripe(77, (GXColor){0xff, 0xff, 0xff, 255});

  bgImg->ColorStripe(screenheight - 90, (GXColor){0xff, 0xff, 0xff, 255});
  bgImg->ColorStripe(screenheight - 91, (GXColor){0xff, 0xff, 0xff, 255});
  bgImg->ColorStripe(screenheight - 92, (GXColor){0xff, 0xff, 0xff, 255});

  GuiImage *topChannelGradient =
      new GuiImage(new GuiImageData(channel_gradient_top_png));
  topChannelGradient->SetAlignment(ALIGN_LEFT, ALIGN_BOTTOM);

  GuiImage *bottomChannelGradient =
      new GuiImage(new GuiImageData(channel_gradient_bottom_png));
  bottomChannelGradient->SetAlignment(ALIGN_LEFT, ALIGN_TOP);

  // Create a tilable grey stripes between both stripes.
  GuiImage *bgstripes = new GuiImage(new GuiImageData(bg_stripes_png));
  bgstripes->SetAlignment(ALIGN_LEFT, ALIGN_TOP);
  bgstripes->SetPosition(0, 78);

  mainWindow->Append(bgImg);
  mainWindow->Append(bgstripes);
  mainWindow->Append(topChannelGradient);
  mainWindow->Append(bottomChannelGradient);

  GuiTrigger trigA;
  trigA.SetSimpleTrigger(-1, WPAD_BUTTON_A | WPAD_CLASSIC_BUTTON_A,
                         PAD_BUTTON_A);

  ResumeGui();

  // DO the funny and read
  FILE *fp = fopen("fat:/qr.png", "rb");
  fseek(fp, 0L, SEEK_END);
  size_t sz = ftell(fp);
  fseek(fp, 0L, SEEK_SET);

  u8 *buf = static_cast<u8 *>(malloc(sz + 1));
  fread(buf, 1, sz, fp);

  GuiText titleTxt("WiiLink Account Linker", 28, {70, 187, 255, 255});
  titleTxt.SetAlignment(ALIGN_CENTRE, ALIGN_TOP);
  titleTxt.SetPosition(0, 25);

  GuiText msgTxt("To finish creating your WiiLink Account, scan this QR code with a mobile device and follow the instructions there.", 20, (GXColor){0xff, 0xff, 0xff, 255});
  msgTxt.SetAlignment(ALIGN_CENTRE, ALIGN_BOTTOM);
  msgTxt.SetWrap(true, 500);
  msgTxt.SetPosition(0, -150);

  GuiText homeTxt("Press HOME to return to the Wii Menu", 30, {70, 187, 255, 255});
  homeTxt.SetAlignment(ALIGN_CENTRE, ALIGN_BOTTOM);
  homeTxt.SetPosition(0, -30);

  GuiImage *qr = new GuiImage(new GuiImageData(buf));
  qr->SetAlignment(ALIGN_CENTRE, ALIGN_CENTRE);
  qr->SetPosition(0, 90);

  mainWindow->Append(&titleTxt);
  mainWindow->Append(&msgTxt);
  mainWindow->Append(&homeTxt);
  mainWindow->Append(qr);

  while (true) {
    usleep(THREAD_SLEEP);
    WPAD_ScanPads();
    u32 pressed = WPAD_ButtonsDown(0);
    if (pressed & WPAD_BUTTON_HOME) {
      ExitRequested = true;
      exitType = WII_MENU;
    }
  }
}