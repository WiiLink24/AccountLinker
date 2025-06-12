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

#include "utils.h"

static void *xfb = nullptr;
static GXRModeObj *rmode = nullptr;
static lwp_t oauth_poll_thread = LWP_THREAD_NULL;

[[noreturn]] void poll_home_button() {
    while (true) {
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
            std::cout << "Authenticated!!!" << std::endl;
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

    LWP_CreateThread(&oauth_poll_thread, PollOAuth, &oauth, nullptr, 0, 50);
    LWP_JoinThread(oauth_poll_thread, nullptr);

    auto ret = GetAuthTokenSignature();
    if (ret.second) {
        std::cout << ret.first << std::endl;
    }

    oauth.PerformLink(ret.first);

    poll_home_button();
    return 0;
}