#include "http.h"

#include <sys/unistd.h>

struct UploadBuffer {
  const std::string* data;
  size_t read_pos = 0;
};

static size_t WriteCallback(void* contents, size_t size, size_t nmemb, std::string* output) {
  size_t total_size = size * nmemb;
  output->append(static_cast<char *>(contents), total_size);
  return total_size;
}

static size_t ReadCallback(void* ptr, size_t size, size_t nmemb, void* stream) {
  auto* upload = static_cast<UploadBuffer*>(stream);
  const size_t max = size * nmemb;
  const size_t remaining = upload->data->size() - upload->read_pos;
  const size_t to_copy = (remaining < max) ? remaining : max;

  if (to_copy > 0) {
    std::memcpy(ptr, upload->data->data() + upload->read_pos, to_copy);
    upload->read_pos += to_copy;
  }

  return to_copy;
}

static int SeekCallback(void* userdata, curl_off_t offset, int origin) {
  auto* upload = static_cast<UploadBuffer*>(userdata);

  if (origin == SEEK_SET && offset >= 0 && static_cast<size_t>(offset) <= upload->data->size()) {
    upload->read_pos = static_cast<size_t>(offset);
    return CURL_SEEKFUNC_OK;
  }

  return CURL_SEEKFUNC_FAIL;
}

HTTPResponse http_get(std::string_view url, const std::vector<std::string>& headers) {
  CURL* curl = curl_easy_init();
  CURLcode res{};
  std::string response{};
  long http_code{};

  if (curl) {
    curl_easy_setopt(curl, CURLOPT_URL, url.data());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);

    curl_slist* curl_headers{};
    for (const auto& header : headers) {
      curl_headers = curl_slist_append(curl_headers, header.c_str());
    }
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, curl_headers);

    res = curl_easy_perform(curl);
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);

    curl_slist_free_all(curl_headers);
    curl_easy_cleanup(curl);
  }

  // Attempt to parse JSON
  json j{};
  std::string json_error{};
  try {
    j = json::parse(response);
  } catch (std::exception& e) {
    json_error = e.what();
  }

  return HTTPResponse{
    .j = j,
    .json_error = json_error,
    .status_code = http_code,
    .curl_code = res
};
}

HTTPResponse http_post(std::string_view url, std::string_view post_data, const std::vector<std::string>& headers) {
  CURL* curl = curl_easy_init();
  CURLcode res{};
  std::string response{};
  long http_code{};

  if (curl) {
    curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "POST");
    curl_easy_setopt(curl, CURLOPT_URL, url.data());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, post_data.data());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);

    curl_slist* curl_headers{};
    for (const auto& header : headers) {
      curl_headers = curl_slist_append(curl_headers, header.c_str());
    }
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, curl_headers);

    res = curl_easy_perform(curl);
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);

    curl_slist_free_all(curl_headers);
    curl_easy_cleanup(curl);
  }

  // Attempt to parse JSON
  json j{};
  std::string json_error{};
  try {
    j = json::parse(response);
  } catch (std::exception& e) {
    json_error = e.what();
  }

  return HTTPResponse{
          .j = j,
          .json_error = json_error,
          .status_code = http_code,
          .curl_code = res,
          .resp = response,
  };
}
