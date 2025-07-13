#include <iostream>
#include <gccore.h>
#include <unistd.h>
#include "oauth.h"
#include "wwfc.h"
#include "otp.h"
#include <curl/curl.h>
#include <wiisocket.h>
#include <wiiuse/wpad.h>
#include <ogcsys.h>

#include "iosc.h"
#include "utils.h"

static void *xfb = nullptr;
static GXRModeObj *rmode = nullptr;
static bool authenticated = false;
static lwp_t oauth_poll_thread = LWP_THREAD_NULL;
static lwp_t wii_remote_poll_thread = LWP_THREAD_NULL;

static void DisplayError(std::string_view message) {
    std::cout << "An error has occurred:" << std::endl;
    std::cerr << message << std::endl;
    std::cout << "Please join the WiiLink Discord for support." << std::endl;
    std::cout << "Server Link: https://discord.gg/wiilink" << std::endl << std::endl ;
    std::cout << "Press the HOME Button to exit." << std::endl;
}

[[noreturn]] void poll_home_button() {
    while (true) {
        WPAD_ScanPads();
        const u32 pressed = WPAD_ButtonsDown(0);
        if (pressed & WPAD_BUTTON_HOME)
            exit(0);
        VIDEO_WaitVSync();
    }
}

static void* ThreadedPollHomeButton(void *arg) {
    while (true) {
        if (authenticated) {
            return nullptr;
        }

        WPAD_ScanPads();
        const u32 pressed = WPAD_ButtonsDown(0);
        if (pressed & WPAD_BUTTON_HOME)
            exit(0);
        VIDEO_WaitVSync();
    }
}

static void* PollOAuth(void *arg) {
    OAuth* auth = static_cast<OAuth*>(arg);
    while (true) {
        sleep(auth->GetInterval());
        bool ret = auth->PollToken();
        if (!ret) {
            const std::string msg = "Polling the device flow " + auth->GetErrorMessage();
            DisplayError(msg);
            while (true) {}
        }

        if (auth->IsAuthenticated()) {
            std::cout << "Account authenticated." << std::endl << std::endl;
            authenticated = true;
            return nullptr;
        }
    }
}

int main() {
    VIDEO_Init();
    WPAD_Init();
    CONF_Init();
    ISFS_Initialize();

    rmode = VIDEO_GetPreferredMode(nullptr);
    xfb = MEM_K0_TO_K1(SYS_AllocateFramebuffer(rmode));
    console_init(xfb,20,20,rmode->fbWidth-20,rmode->xfbHeight-20,rmode->fbWidth*VI_DISPLAY_PIX_SZ);
    VIDEO_Configure(rmode);
    VIDEO_SetNextFramebuffer(xfb);
    VIDEO_ClearFrameBuffer(rmode, xfb, COLOR_BLACK);
    VIDEO_SetBlack(false);
    VIDEO_Flush();
    VIDEO_WaitVSync();
    if(rmode->viTVMode&VI_NON_INTERLACE) VIDEO_WaitVSync();

    PrintHeader();
    std::cout << "Please wait a moment while we initialize..." << std::endl;

    curl_global_init(CURL_GLOBAL_DEFAULT);
    wiisocket_init();

    OAuth oauth{};
    bool success = oauth.StartDeviceFlow();
    ClearScreen();
    PrintHeader();
    if (!success) {
        const std::string msg = "Starting the device flow " + oauth.GetErrorMessage();
        DisplayError(msg);
        poll_home_button();
    }

    std::cout << "Please visit https://accounts.wiilink.ca/link" << std::endl << "and enter the following code: " << oauth.GetUserCode() << std::endl;

    // Spawn a thread for polling OAuth, and one for polling the Wii Remote.
    LWP_CreateThread(&oauth_poll_thread, PollOAuth, &oauth, nullptr, 0, 80);
    LWP_CreateThread(&wii_remote_poll_thread, ThreadedPollHomeButton, nullptr, nullptr, 0, 30);
    LWP_JoinThread(oauth_poll_thread, nullptr);

    // We will now get the keys.
    // First we try if this is a real Wii.
    std::pair<std::string, bool> ret{};

    WiiOTP otp{};
    otp_read(0, OTP_WORD_COUNT, otp.data);
    if (otp.device_id != 0) {
        // Is a Wii.
        ret = GetAuthTokenSignature(otp.device_private_key);
    } else {
        // We are on Dolphin.
        File* keys = ISFS_GetFile("/keys.bin");
        if (keys->error_code != 0) {
            if (keys->error_code == -106) {
                // If the file doesn't exist, we can assume this NAND is a default one.
                DisplayError("This is a default Dolphin NAND. Please use a NAND dump from your Wii.\n");
            } else {
                // Some other underlying ISFS error
                DisplayError(keys->error);
            }

            poll_home_button();
        }

        auto* dump = static_cast<IOSC::BootMiiKeyDump*>(keys->data);
        ret = GetAuthTokenSignature(dump->ng_priv.data());
    }

    if (ret.second) {
        DisplayError(ret.first);
        poll_home_button();
    }

    success = oauth.PerformLink(ret.first);
    if (!success) {
        const std::string msg = "Linking the Wii " + oauth.GetErrorMessage();
        DisplayError(msg);
        poll_home_button();
    }

    std::cout << "Wii successfully linked! Enjoy WiiLink Accounts!" << std::endl << std::endl;
    std::cout << "Press the HOME button to exit." << std::endl;
    poll_home_button();
    return 0;
}