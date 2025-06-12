#pragma once

#include <string>
#include "http.h"

class OAuth {
public:
    OAuth() = default;
    bool StartDeviceFlow();
    void PollToken();
    void PerformLink(std::string_view wwfc_cert);
    std::string GetErrorMessage() const;
    std::string_view GetUserCode() const;
    std::string GetDeviceCode() const;
    int GetInterval() const;
    bool IsAuthenticated() const;

private:
    std::string m_user_code{};
    std::string m_device_code{};
    HTTPResponse m_response{};
    int m_interval;
    std::string m_access_token{};
    bool m_authenticated = false;

    // We need these values for the user update endpoint.
    int m_user_id{};
    std::string m_username{};
    std::string m_name{};

    static constexpr char TOKEN_PATH[] = "https://sso.riiconnect24.net/application/o/token/";
    static constexpr char DEVICE_PATH[] = "https://sso.riiconnect24.net/application/o/device/";
    static constexpr char USER_ID_PATH[] = "https://sso.riiconnect24.net/api/v3/core/users/me/";
    static constexpr char USER_UPDATE_PATH[] = "http://localhost:9011/link/wii";

    // Production is ChGKaNcTcArxLCWSxAbvXXtbWKsM1xcy6x7k8ssn
    static constexpr char CLIENT_ID[] = "fBBy1hJf5Ay9JFYkQpHmeExgE1Kr7KUb4f6Ngzar";
};