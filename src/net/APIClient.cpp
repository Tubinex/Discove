#include "net/APIClient.h"

#include <FL/Fl.H>
#include <curl/curl.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <thread>

#include "utils/Logger.h"

namespace {
    bool isMessageEndpoint(const std::string& endpoint) {
        return endpoint.find("/channels/") != std::string::npos && endpoint.find("/messages") != std::string::npos;
    }

    std::string extractChannelId(const std::string& endpoint) {
        const std::string marker = "/channels/";
        size_t start = endpoint.find(marker);
        if (start == std::string::npos) {
            return "unknown";
        }
        start += marker.size();
        size_t end = endpoint.find('/', start);
        if (end == std::string::npos || end <= start) {
            return "unknown";
        }
        return endpoint.substr(start, end - start);
    }
} // namespace

namespace Discord {

    APIClient& APIClient::get() {
        static APIClient instance;
        return instance;
    }

    void APIClient::setToken(const std::string& token) {
        std::scoped_lock lock(m_tokenMutex);
        m_token = token;
    }

    void APIClient::getChannelMessages(const std::string& channelId, int limit, const std::optional<std::string>& before,
        SuccessCallback onSuccess, ErrorCallback onError) {
        std::ostringstream endpoint;
        endpoint << "/channels/" << channelId << "/messages";
        endpoint << "?limit=" << limit;

        if (before.has_value()) {
            endpoint << "&before=" << *before;
        }

        Logger::debug("API: Requesting " + std::to_string(limit) + " messages from channel " + channelId);
        performGet(endpoint.str(), onSuccess, onError);
    }

    void APIClient::performGet(const std::string& endpoint, SuccessCallback onSuccess, ErrorCallback onError) {
        std::string token;
        {
            std::scoped_lock lock(m_tokenMutex);
            token = m_token;
        }

        std::thread([=]() {
            CURL* curl = curl_easy_init();
            if (!curl) {
                auto* heapFn = new std::function<void()>([onError]() { onError(-1, "Failed to initialize CURL"); });
                Fl::awake(
                    [](void* p) {
                        std::unique_ptr<std::function<void()>> fnPtr(static_cast<std::function<void()> *>(p));
                        (*fnPtr)();
                    },
                    heapFn);
                return;
            }

            std::string url = buildUrl(endpoint);
            std::string response;

            Logger::debug("API: Requesting URL: " + url);

            curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
            curl_easy_setopt(curl, CURLOPT_USERAGENT, USER_AGENT);
            curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 10L);
            curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);
            curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
            curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
            curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);

            struct curl_slist* headers = nullptr;
            std::string authHeader = "Authorization: " + token;
            headers = curl_slist_append(headers, authHeader.c_str());
            headers = curl_slist_append(headers, "Content-Type: application/json");
            curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

            CURLcode res = curl_easy_perform(curl);
            long httpCode = 0;
            curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode);

            Logger::debug("API: Response HTTP " + std::to_string(httpCode) +
                ", body length: " + std::to_string(response.length()));

            curl_slist_free_all(headers);
            curl_easy_cleanup(curl);

            if (res != CURLE_OK) {
                Logger::error("CURL error: " + std::string(curl_easy_strerror(res)));
                auto* heapFn = new std::function<void()>(
                    [onError, res]() { onError(-1, "Network error: " + std::string(curl_easy_strerror(res))); });
                Fl::awake(
                    [](void* p) {
                        std::unique_ptr<std::function<void()>> fnPtr(static_cast<std::function<void()> *>(p));
                        (*fnPtr)();
                    },
                    heapFn);
                return;
            }

            if (httpCode >= 200 && httpCode < 300) {
                try {
                    Json json = Json::parse(response);
                    if (json.is_array()) {
                        Logger::debug("API: Received " + std::to_string(json.size()) + " items in response");
                    }
                    else {
                        Logger::debug("API: Received non-array response");
                    }
                    auto* heapFn = new std::function<void()>([onSuccess, json]() { onSuccess(json); });
                    Fl::awake(
                        [](void* p) {
                            std::unique_ptr<std::function<void()>> fnPtr(static_cast<std::function<void()> *>(p));
                            (*fnPtr)();
                        },
                        heapFn);
                }
                catch (const std::exception& e) {
                    Logger::error("JSON parse error: " + std::string(e.what()));
                    auto* heapFn =
                        new std::function<void()>([onError, httpCode, e]() { onError(httpCode, "JSON parse error"); });
                    Fl::awake(
                        [](void* p) {
                            std::unique_ptr<std::function<void()>> fnPtr(static_cast<std::function<void()> *>(p));
                            (*fnPtr)();
                        },
                        heapFn);
                }
            }
            else {
                Logger::error("HTTP error " + std::to_string(httpCode) + ": " + response);
                auto* heapFn = new std::function<void()>([onError, httpCode, response]() {
                    onError(httpCode, "HTTP error " + std::to_string(httpCode) + ": " + response);
                    });
                Fl::awake(
                    [](void* p) {
                        std::unique_ptr<std::function<void()>> fnPtr(static_cast<std::function<void()> *>(p));
                        (*fnPtr)();
                    },
                    heapFn);
            }
            }).detach();
    }

    size_t APIClient::writeCallback(void* contents, size_t size, size_t nmemb, void* userp) {
        size_t totalSize = size * nmemb;
        std::string* response = static_cast<std::string*>(userp);
        response->append(static_cast<char*>(contents), totalSize);
        return totalSize;
    }

    std::string APIClient::buildUrl(const std::string& endpoint) const { return std::string(BASE_URL) + endpoint; }

} // namespace Discord
