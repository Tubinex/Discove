#pragma once

#include <chrono>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

struct sqlite3;
struct sqlite3_stmt;

class Message;

namespace Data {

class Database {
  public:
    static Database &get();

    bool initialize();

    void clearAllMessages();

    bool insertMessage(const Message &msg);
    int insertMessages(const std::vector<Message> &messages);

    std::vector<Message> getChannelMessages(const std::string &channelId, int limit = 50);

    bool messageExists(const std::string &messageId);

    bool insertChannel(const std::string &channelId, const std::string &name, const std::string &type);

  private:
    Database() = default;
    ~Database();
    Database(const Database &) = delete;
    Database &operator=(const Database &) = delete;

    std::string getDatabasePath() const;
    bool createTables();
    bool execute(const std::string &sql);
    sqlite3_stmt *prepare(const std::string &sql);

    bool bindMessageToStatement(sqlite3_stmt *stmt, const Message &msg);
    Message messageFromStatement(sqlite3_stmt *stmt);

    int64_t toUnixMs(const std::chrono::system_clock::time_point &tp) const;
    std::chrono::system_clock::time_point fromUnixMs(int64_t ms) const;

  private:
    sqlite3 *m_db = nullptr;
    mutable std::mutex m_mutex;
};

} // namespace Data
