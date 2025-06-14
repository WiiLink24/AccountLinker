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
        auth->PollToken();
        if (auth->IsAuthenticated()) {
            std::cout << "Account authenticated." << std::endl;
            authenticated = true;
            return nullptr;
        }
    }
}

static void DisplayError(std::string_view message) {
    std::cerr << message << std::endl;
    std::cout << "Please join the WiiLink Discord for support." << std::endl;
    std::cout << "Server Link: https://discord.gg/reqUMqxu8D" << std::endl << std::endl ;
    std::cout << "Press the HOME Button to exit." << std::endl;
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

    std::cout << std::endl;
    std::cout << std::endl;
    std::cout << std::endl;
    std::cout << "WiiLink Account Linker - (c) 2025 WiiLink" << std::endl;
    std::cout << "v2.0" << std::endl;
    std::cout << std::endl;
    curl_global_init(CURL_GLOBAL_DEFAULT);
    wiisocket_init();

    OAuth oauth{};
    if (!oauth.StartDeviceFlow()) {
        const std::string msg = "Starting the device flow " + oauth.GetErrorMessage();
        DisplayError(msg);
        poll_home_button();
    }

    std::cout << "Please visit https://sso.riiconnect24.net/device" << std::endl << "and enter the following code: " << oauth.GetUserCode() << std::endl;

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
            // If the file doesn't exist, we can assume this NAND is a default one.
            DisplayError(keys->error);
        }

        auto* dump = static_cast<IOSC::BootMiiKeyDump*>(keys->data);
        ret = GetAuthTokenSignature(dump->ng_priv.data());
    }

    if (ret.second) {
        DisplayError(ret.first);
    }

    oauth.PerformLink(ret.first);

    poll_home_button();
    return 0;
}