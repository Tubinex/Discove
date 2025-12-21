#include "utils/Secrets.h"
#include "utils/Logger.h"

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <wincred.h>
#include <vector>

bool Secrets::set(const std::string &key, const std::string &value) {
    std::wstring targetName;
    std::string fullKey = std::string(SERVICE_NAME) + ":" + key;
    targetName.resize(fullKey.size());
    MultiByteToWideChar(CP_UTF8, 0, fullKey.c_str(), -1, &targetName[0], fullKey.size() + 1);

    CREDENTIALW cred = {};
    cred.Type = CRED_TYPE_GENERIC;
    cred.TargetName = &targetName[0];
    cred.CredentialBlobSize = static_cast<DWORD>(value.size());
    cred.CredentialBlob = (LPBYTE)value.data();
    cred.Persist = CRED_PERSIST_LOCAL_MACHINE;

    if (CredWriteW(&cred, 0)) {
        Logger::debug("Secret stored successfully for key: " + key);
        return true;
    }

    Logger::error("Failed to store secret for key: " + key);
    return false;
}

std::optional<std::string> Secrets::get(const std::string &key) {
    std::wstring targetName;
    std::string fullKey = std::string(SERVICE_NAME) + ":" + key;
    targetName.resize(fullKey.size());
    MultiByteToWideChar(CP_UTF8, 0, fullKey.c_str(), -1, &targetName[0], fullKey.size() + 1);

    PCREDENTIALW cred = nullptr;
    if (CredReadW(&targetName[0], CRED_TYPE_GENERIC, 0, &cred)) {
        std::string value((char *)cred->CredentialBlob, cred->CredentialBlobSize);
        CredFree(cred);
        Logger::debug("Secret retrieved successfully for key: " + key);
        return value;
    }

    Logger::debug("Secret not found for key: " + key);
    return std::nullopt;
}

bool Secrets::remove(const std::string &key) {
    std::wstring targetName;
    std::string fullKey = std::string(SERVICE_NAME) + ":" + key;
    targetName.resize(fullKey.size());
    MultiByteToWideChar(CP_UTF8, 0, fullKey.c_str(), -1, &targetName[0], fullKey.size() + 1);

    if (CredDeleteW(&targetName[0], CRED_TYPE_GENERIC, 0)) {
        Logger::debug("Secret removed successfully for key: " + key);
        return true;
    }

    Logger::error("Failed to remove secret for key: " + key);
    return false;
}

#elif __APPLE__
#include <Security/Security.h>

bool Secrets::set(const std::string &key, const std::string &value) {
    remove(key);

    OSStatus status = SecKeychainAddGenericPassword(nullptr, strlen(SERVICE_NAME), SERVICE_NAME, key.length(),
                                                     key.c_str(), value.length(), value.c_str(), nullptr);

    if (status == errSecSuccess) {
        Logger::debug("Secret stored successfully for key: " + key);
        return true;
    }

    Logger::error("Failed to store secret for key: " + key);
    return false;
}

std::optional<std::string> Secrets::get(const std::string &key) {
    void *passwordData = nullptr;
    UInt32 passwordLength = 0;

    OSStatus status = SecKeychainFindGenericPassword(nullptr, strlen(SERVICE_NAME), SERVICE_NAME, key.length(),
                                                      key.c_str(), &passwordLength, &passwordData, nullptr);

    if (status == errSecSuccess && passwordData != nullptr) {
        std::string value((char *)passwordData, passwordLength);
        SecKeychainItemFreeContent(nullptr, passwordData);
        Logger::debug("Secret retrieved successfully for key: " + key);
        return value;
    }

    Logger::debug("Secret not found for key: " + key);
    return std::nullopt;
}

bool Secrets::remove(const std::string &key) {
    SecKeychainItemRef itemRef = nullptr;
    OSStatus status = SecKeychainFindGenericPassword(nullptr, strlen(SERVICE_NAME), SERVICE_NAME, key.length(),
                                                      key.c_str(), nullptr, nullptr, &itemRef);

    if (status == errSecSuccess && itemRef != nullptr) {
        status = SecKeychainItemDelete(itemRef);
        CFRelease(itemRef);

        if (status == errSecSuccess) {
            Logger::debug("Secret removed successfully for key: " + key);
            return true;
        }
    }

    Logger::error("Failed to remove secret for key: " + key);
    return false;
}

#elif __linux__
#include <libsecret/secret.h>

static const SecretSchema *get_schema() {
    static const SecretSchema schema = {"com.discove.secrets",
                                        SECRET_SCHEMA_NONE,
                                        {
                                            {"service", SECRET_SCHEMA_ATTRIBUTE_STRING},
                                            {"key", SECRET_SCHEMA_ATTRIBUTE_STRING},
                                            {nullptr, SECRET_SCHEMA_ATTRIBUTE_STRING},
                                        }};
    return &schema;
}

bool Secrets::set(const std::string &key, const std::string &value) {
    GError *error = nullptr;
    std::string label = std::string(SERVICE_NAME) + " - " + key;

    secret_password_store_sync(get_schema(), SECRET_COLLECTION_DEFAULT, label.c_str(), value.c_str(), nullptr, &error,
                                "service", SERVICE_NAME, "key", key.c_str(), nullptr);

    if (error != nullptr) {
        Logger::error("Failed to store secret for key: " + key + " - " + std::string(error->message));
        g_error_free(error);
        return false;
    }

    Logger::debug("Secret stored successfully for key: " + key);
    return true;
}

std::optional<std::string> Secrets::get(const std::string &key) {
    GError *error = nullptr;

    gchar *password = secret_password_lookup_sync(get_schema(), nullptr, &error, "service", SERVICE_NAME, "key",
                                                   key.c_str(), nullptr);

    if (error != nullptr) {
        Logger::debug("Secret not found for key: " + key);
        g_error_free(error);
        return std::nullopt;
    }

    if (password == nullptr) {
        Logger::debug("Secret not found for key: " + key);
        return std::nullopt;
    }

    std::string value(password);
    secret_password_free(password);
    Logger::debug("Secret retrieved successfully for key: " + key);
    return value;
}

bool Secrets::remove(const std::string &key) {
    GError *error = nullptr;

    gboolean removed =
        secret_password_clear_sync(get_schema(), nullptr, &error, "service", SERVICE_NAME, "key", key.c_str(), nullptr);

    if (error != nullptr) {
        Logger::error("Failed to remove secret for key: " + key + " - " + std::string(error->message));
        g_error_free(error);
        return false;
    }

    if (removed) {
        Logger::debug("Secret removed successfully for key: " + key);
        return true;
    }

    Logger::debug("Secret not found for key: " + key);
    return false;
}

#else
#error "Unsupported platform"
#endif
