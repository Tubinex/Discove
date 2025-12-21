#pragma once

#include <optional>
#include <string>

class Secrets {
  public:
    /**
     * Store a secret securely in the OS credential store
     * @param key The key to identify the secret
     * @param value The secret value to store
     * @return true if successful, false otherwise
     */
    static bool set(const std::string &key, const std::string &value);

    /**
     * Retrieve a secret from the OS credential store
     * @param key The key to identify the secret
     * @return The secret value if found, std::nullopt otherwise
     */
    static std::optional<std::string> get(const std::string &key);

    /**
     * Delete a secret from the OS credential store
     * @param key The key to identify the secret
     * @return true if successful, false otherwise
     */
    static bool remove(const std::string &key);

  private:
    static constexpr const char *SERVICE_NAME = "Discove";
};
