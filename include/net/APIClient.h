#pragma once

#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>

#include <nlohmann/json.hpp>

typedef void CURL;

namespace Discord {

class APIClient {
  public:
    using Json = nlohmann::json;

    using SuccessCallback = std::function<void(const Json &)>;
    using ErrorCallback = std::function<void(int httpCode, const std::string &error)>;

    static APIClient &get();

    void setToken(const std::string &token);

    void getChannelMessages(const std::string &channelId, int limit, const std::optional<std::string> &before,
                            SuccessCallback onSuccess, ErrorCallback onError);
    void sendChannelMessage(const std::string &channelId, const std::string &content, const std::string &nonce,
                            SuccessCallback onSuccess, ErrorCallback onError);
    void getUser(const std::string &userId, SuccessCallback onSuccess, ErrorCallback onError);

  private:
    APIClient() = default;
    ~APIClient() = default;
    APIClient(const APIClient &) = delete;
    APIClient &operator=(const APIClient &) = delete;

    void performGet(const std::string &endpoint, SuccessCallback onSuccess, ErrorCallback onError);
    void performPost(const std::string &endpoint, const std::string &body, SuccessCallback onSuccess,
                     ErrorCallback onError);

    static size_t writeCallback(void *contents, size_t size, size_t nmemb, void *userp);

    void setupCurlOptions(CURL *curl, const std::string &url, std::string *responseBuffer);

    std::string buildUrl(const std::string &endpoint) const;

  private:
    static constexpr const char *BASE_URL = "https://discord.com/api/v10";
    static constexpr const char *USER_AGENT = "Discord/1.0";

    std::string m_token;
    mutable std::mutex m_tokenMutex;
};

} // namespace Discord
