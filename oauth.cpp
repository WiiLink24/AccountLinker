#include "oauth.h"
#include "utils.h"
#include "tinyjson.h"
#include <curl/curl.h>
#include <sys/unistd.h>
#include "config.h"

bool OAuth::StartDeviceFlow() {
  const std::string post_data = std::format("client_id={}&scope=openid+profile+email+goauthentik.io/api", CLIENT_ID);

  std::vector<std::string> headers = {
    "User-Agent: WiiLink Account Linker v0.1",
    "Content-Type: application/x-www-form-urlencoded"
};

  m_response = http_post(DEVICE_PATH, post_data, headers);
  if (m_response.status_code != 200 || m_response.curl_code != CURLE_OK) {
    // JSON will probably be empty. Return and display message.
    return false;
  }

  m_device_code = m_response.j["device_code"];
  m_user_code = m_response.j["user_code"];
  m_interval = m_response.j["interval"];
  return true;
}

std::string OAuth::GetErrorMessage() const{
  // First check if we have a JSON error.
  if (!m_response.json_error.empty()) {
    return m_response.json_error;
  }

  // Server will respond with 400 Bad Request when the user has not completed the flow.
  // Only check if Curl succeeded.
  if (m_response.status_code != 200 && m_response.curl_code == CURLE_OK) {
    return std::format("failed with HTTP response code: {}", m_response.status_code);
  }

  // If we got here Curl failed.
  return std::format("failed with Curl code: {}", static_cast<int>(m_response.curl_code));
}


void OAuth::PollToken() {
  const std::string post_data = std::format("grant_type=urn:ietf:params:oauth:grant-type:device_code&client_id={}&device_code={}", CLIENT_ID, GetDeviceCode());
  std::vector<std::string> headers = {
          "User-Agent: WiiLink Account Linker v0.1",
          "Content-Type: application/x-www-form-urlencoded"
  };

  m_response = http_post(TOKEN_PATH, post_data, headers);
  if ((m_response.status_code != 200 && m_response.status_code != 400) || m_response.curl_code != CURLE_OK) {
    // JSON will probably be empty. Return and display message.
    return;
  }

  // Retrieve data
  if (!m_response.j.contains("error")) {
    m_access_token = m_response.j["access_token"];
    m_authenticated = true;
  }
}

bool OAuth::PerformLink(std::string_view wwfc_cert) {
  std::vector<std::string> headers = {
    "User-Agent: WiiLink Account Linker v0.1",
    "Accept: application/json",
    "Content-Type: application/x-www-form-urlencoded",
    std::format("Authorization: {}", m_access_token),
  };

  auto *config = new NWC24Config();
  if (!config->ReadConfig()) {
    return false;
  }

  // Create our payload
  std::cout << wwfc_cert.size() << std::endl;
  const std::string post_data = std::format("wii_num={}&cert={}", config->GetWiiNumber(), curl_easy_escape(nullptr, wwfc_cert.data(), wwfc_cert.length()));

  m_response = http_post(USER_UPDATE_PATH, post_data, headers);
  std::cout << m_response.status_code << std::endl;
  std::cout << m_response.curl_code << std::endl;
  std::cout << m_response.j.dump(4) << std::endl;
  return true;
}


std::string OAuth::GetDeviceCode() const {
  // curl_easy_escape destroys the entire POST body if we apply it. We will only escape the device code.
  return std::string{curl_easy_escape(nullptr, m_device_code.c_str(), m_device_code.length())};
}

std::string_view OAuth::GetUserCode() const {
  return m_user_code;
}

int OAuth::GetInterval() const {
  return m_interval;
}

bool OAuth::IsAuthenticated() const {
  return m_authenticated;
}