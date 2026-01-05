#include "data/Database.h"

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <sqlite3.h>

#include "models/Message.h"
#include "utils/Logger.h"

namespace fs = std::filesystem;

namespace Data {

Database &Database::get() {
    static Database instance;
    return instance;
}

Database::~Database() {
    if (m_db) {
        sqlite3_close(m_db);
        m_db = nullptr;
    }
}

std::string Database::getDatabasePath() const {
    std::string dataDir;

#ifdef _WIN32
    char *appData = nullptr;
    size_t len = 0;
    if (_dupenv_s(&appData, &len, "LOCALAPPDATA") == 0 && appData != nullptr) {
        dataDir = std::string(appData) + "\\Discove";
        free(appData);
    } else {
        dataDir = "C:\\Temp\\Discove";
    }
#else
    const char *home = getenv("HOME");
    if (home) {
        dataDir = std::string(home) + "/.local/share/discove";
    } else {
        dataDir = "/tmp/discove";
    }
#endif

    try {
        fs::create_directories(dataDir);
    } catch (const std::exception &e) {
        Logger::error("Failed to create database directory: " + std::string(e.what()));
    }

    return dataDir + (std::string(1, fs::path::preferred_separator) + "discove.db");
}

bool Database::initialize() {
    std::scoped_lock lock(m_mutex);

    if (m_db) {
        return true;
    }

    std::string dbPath = getDatabasePath();
    Logger::info("Initializing database at: " + dbPath);

    int result = sqlite3_open(dbPath.c_str(), &m_db);
    if (result != SQLITE_OK) {
        Logger::error("Failed to open database: " + std::string(sqlite3_errmsg(m_db)));
        return false;
    }

    if (!createTables()) {
        Logger::error("Failed to create database tables");
        return false;
    }

    Logger::info("Database initialized successfully");
    return true;
}

bool Database::createTables() {
    const char *messagesSql = R"(
        CREATE TABLE IF NOT EXISTS messages (
            id TEXT PRIMARY KEY,
            channel_id TEXT NOT NULL,
            author_id TEXT NOT NULL,
            content TEXT NOT NULL,
            timestamp INTEGER NOT NULL,
            edited_timestamp INTEGER,
            type INTEGER NOT NULL,
            guild_id TEXT,
            tts INTEGER NOT NULL DEFAULT 0,
            mention_everyone INTEGER NOT NULL DEFAULT 0,
            pinned INTEGER NOT NULL DEFAULT 0,
            webhook_id TEXT,
            application_id TEXT,
            referenced_message_id TEXT,
            nonce TEXT,
            mention_ids TEXT,
            mention_role_ids TEXT,
            attachments TEXT,
            embeds TEXT
        )
    )";

    const char *messagesIndexSql = R"(
        CREATE INDEX IF NOT EXISTS idx_messages_channel_timestamp
        ON messages(channel_id, timestamp DESC)
    )";

    const char *channelsSql = R"(
        CREATE TABLE IF NOT EXISTS channels (
            id TEXT PRIMARY KEY,
            name TEXT,
            type TEXT,
            last_message_id TEXT,
            last_sync_timestamp INTEGER
        )
    )";

    return execute(messagesSql) && execute(messagesIndexSql) && execute(channelsSql);
}

bool Database::execute(const std::string &sql) {
    char *errMsg = nullptr;
    int result = sqlite3_exec(m_db, sql.c_str(), nullptr, nullptr, &errMsg);

    if (result != SQLITE_OK) {
        Logger::error("SQL error: " + std::string(errMsg ? errMsg : "unknown"));
        if (errMsg) {
            sqlite3_free(errMsg);
        }
        return false;
    }

    return true;
}

sqlite3_stmt *Database::prepare(const std::string &sql) {
    sqlite3_stmt *stmt = nullptr;
    int result = sqlite3_prepare_v2(m_db, sql.c_str(), -1, &stmt, nullptr);

    if (result != SQLITE_OK) {
        Logger::error("Failed to prepare statement: " + std::string(sqlite3_errmsg(m_db)));
        return nullptr;
    }

    return stmt;
}

void Database::clearAllMessages() {
    std::scoped_lock lock(m_mutex);

    if (!m_db) {
        return;
    }

    if (execute("DELETE FROM messages")) {
        Logger::info("Cleared all messages from database");
    } else {
        Logger::error("Failed to clear messages from database");
    }
}

bool Database::insertMessage(const Message &msg) {
    std::scoped_lock lock(m_mutex);

    if (!m_db) {
        return false;
    }

    const char *sql = R"(
        INSERT OR REPLACE INTO messages (
            id, channel_id, author_id, content, timestamp, edited_timestamp,
            type, guild_id, tts, mention_everyone, pinned, webhook_id,
            application_id, referenced_message_id, nonce,
            mention_ids, mention_role_ids, attachments, embeds
        ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
    )";

    sqlite3_stmt *stmt = prepare(sql);
    if (!stmt) {
        return false;
    }

    bool success = bindMessageToStatement(stmt, msg);
    if (success) {
        int result = sqlite3_step(stmt);
        success = (result == SQLITE_DONE);
    }

    sqlite3_finalize(stmt);
    return success;
}

int Database::insertMessages(const std::vector<Message> &messages) {
    std::scoped_lock lock(m_mutex);

    if (!m_db || messages.empty()) {
        return 0;
    }

    execute("BEGIN TRANSACTION");

    const char *sql = R"(
        INSERT OR REPLACE INTO messages (
            id, channel_id, author_id, content, timestamp, edited_timestamp,
            type, guild_id, tts, mention_everyone, pinned, webhook_id,
            application_id, referenced_message_id, nonce,
            mention_ids, mention_role_ids, attachments, embeds
        ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
    )";

    int successCount = 0;

    for (const auto &msg : messages) {
        sqlite3_stmt *stmt = prepare(sql);
        if (!stmt) {
            continue;
        }

        if (bindMessageToStatement(stmt, msg)) {
            int result = sqlite3_step(stmt);
            if (result == SQLITE_DONE) {
                successCount++;
            }
        }

        sqlite3_finalize(stmt);
    }

    execute("COMMIT");

    return successCount;
}

bool Database::bindMessageToStatement(sqlite3_stmt *stmt, const Message &msg) {
    int idx = 1;

    sqlite3_bind_text(stmt, idx++, msg.id.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, idx++, msg.channelId.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, idx++, msg.authorId.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, idx++, msg.content.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt, idx++, toUnixMs(msg.timestamp));

    if (msg.editedTimestamp.has_value()) {
        sqlite3_bind_int64(stmt, idx++, toUnixMs(*msg.editedTimestamp));
    } else {
        sqlite3_bind_null(stmt, idx++);
    }

    sqlite3_bind_int(stmt, idx++, static_cast<int>(msg.type));

    if (msg.guildId.has_value()) {
        sqlite3_bind_text(stmt, idx++, msg.guildId->c_str(), -1, SQLITE_TRANSIENT);
    } else {
        sqlite3_bind_null(stmt, idx++);
    }

    sqlite3_bind_int(stmt, idx++, msg.tts ? 1 : 0);
    sqlite3_bind_int(stmt, idx++, msg.mentionEveryone ? 1 : 0);
    sqlite3_bind_int(stmt, idx++, msg.pinned ? 1 : 0);

    if (msg.webhookId.has_value()) {
        sqlite3_bind_text(stmt, idx++, msg.webhookId->c_str(), -1, SQLITE_TRANSIENT);
    } else {
        sqlite3_bind_null(stmt, idx++);
    }

    if (msg.applicationId.has_value()) {
        sqlite3_bind_text(stmt, idx++, msg.applicationId->c_str(), -1, SQLITE_TRANSIENT);
    } else {
        sqlite3_bind_null(stmt, idx++);
    }

    if (msg.referencedMessageId.has_value()) {
        sqlite3_bind_text(stmt, idx++, msg.referencedMessageId->c_str(), -1, SQLITE_TRANSIENT);
    } else {
        sqlite3_bind_null(stmt, idx++);
    }

    if (msg.nonce.has_value()) {
        sqlite3_bind_text(stmt, idx++, msg.nonce->c_str(), -1, SQLITE_TRANSIENT);
    } else {
        sqlite3_bind_null(stmt, idx++);
    }

    nlohmann::json mentionIdsJson = msg.mentionIds;
    std::string mentionIdsStr = mentionIdsJson.dump();
    sqlite3_bind_text(stmt, idx++, mentionIdsStr.c_str(), -1, SQLITE_TRANSIENT);

    nlohmann::json mentionRoleIdsJson = msg.mentionRoleIds;
    std::string mentionRoleIdsStr = mentionRoleIdsJson.dump();
    sqlite3_bind_text(stmt, idx++, mentionRoleIdsStr.c_str(), -1, SQLITE_TRANSIENT);

    nlohmann::json attachmentsJson = nlohmann::json::array();
    for (const auto &attachment : msg.attachments) {
        nlohmann::json attachmentJson;
        attachmentJson["id"] = attachment.id;
        attachmentJson["filename"] = attachment.filename;
        attachmentJson["size"] = attachment.size;
        attachmentJson["url"] = attachment.url;
        attachmentJson["proxy_url"] = attachment.proxyUrl;
        if (attachment.height.has_value()) {
            attachmentJson["height"] = *attachment.height;
        }
        if (attachment.width.has_value()) {
            attachmentJson["width"] = *attachment.width;
        }
        attachmentsJson.push_back(attachmentJson);
    }
    std::string attachmentsStr = attachmentsJson.dump();
    sqlite3_bind_text(stmt, idx++, attachmentsStr.c_str(), -1, SQLITE_TRANSIENT);

    nlohmann::json embedsJson = nlohmann::json::array();
    for (const auto &embed : msg.embeds) {
        embedsJson.push_back(nlohmann::json{});
    }
    std::string embedsStr = embedsJson.dump();
    sqlite3_bind_text(stmt, idx++, embedsStr.c_str(), -1, SQLITE_TRANSIENT);

    return true;
}

std::vector<Message> Database::getChannelMessages(const std::string &channelId, int limit) {
    std::scoped_lock lock(m_mutex);

    std::vector<Message> messages;

    if (!m_db) {
        return messages;
    }

    const char *sql = R"(
        SELECT id, channel_id, author_id, content, timestamp, edited_timestamp,
               type, guild_id, tts, mention_everyone, pinned, webhook_id,
               application_id, referenced_message_id, nonce,
               mention_ids, mention_role_ids, attachments, embeds
        FROM messages
        WHERE channel_id = ?
        ORDER BY timestamp ASC
        LIMIT ?
    )";

    sqlite3_stmt *stmt = prepare(sql);
    if (!stmt) {
        return messages;
    }

    sqlite3_bind_text(stmt, 1, channelId.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 2, limit);

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        messages.push_back(messageFromStatement(stmt));
    }

    sqlite3_finalize(stmt);
    return messages;
}

Message Database::messageFromStatement(sqlite3_stmt *stmt) {
    Message msg;

    int idx = 0;

    msg.id = reinterpret_cast<const char *>(sqlite3_column_text(stmt, idx++));
    msg.channelId = reinterpret_cast<const char *>(sqlite3_column_text(stmt, idx++));
    msg.authorId = reinterpret_cast<const char *>(sqlite3_column_text(stmt, idx++));
    msg.content = reinterpret_cast<const char *>(sqlite3_column_text(stmt, idx++));
    msg.timestamp = fromUnixMs(sqlite3_column_int64(stmt, idx++));

    if (sqlite3_column_type(stmt, idx) != SQLITE_NULL) {
        msg.editedTimestamp = fromUnixMs(sqlite3_column_int64(stmt, idx));
    }
    idx++;

    msg.type = static_cast<MessageType>(sqlite3_column_int(stmt, idx++));

    if (sqlite3_column_type(stmt, idx) != SQLITE_NULL) {
        msg.guildId = reinterpret_cast<const char *>(sqlite3_column_text(stmt, idx));
    }
    idx++;

    msg.tts = sqlite3_column_int(stmt, idx++) != 0;
    msg.mentionEveryone = sqlite3_column_int(stmt, idx++) != 0;
    msg.pinned = sqlite3_column_int(stmt, idx++) != 0;

    if (sqlite3_column_type(stmt, idx) != SQLITE_NULL) {
        msg.webhookId = reinterpret_cast<const char *>(sqlite3_column_text(stmt, idx));
    }
    idx++;

    if (sqlite3_column_type(stmt, idx) != SQLITE_NULL) {
        msg.applicationId = reinterpret_cast<const char *>(sqlite3_column_text(stmt, idx));
    }
    idx++;

    if (sqlite3_column_type(stmt, idx) != SQLITE_NULL) {
        msg.referencedMessageId = reinterpret_cast<const char *>(sqlite3_column_text(stmt, idx));
    }
    idx++;

    if (sqlite3_column_type(stmt, idx) != SQLITE_NULL) {
        msg.nonce = reinterpret_cast<const char *>(sqlite3_column_text(stmt, idx));
    }
    idx++;

    try {
        const char *mentionIdsStr = reinterpret_cast<const char *>(sqlite3_column_text(stmt, idx++));
        if (mentionIdsStr) {
            nlohmann::json mentionIdsJson = nlohmann::json::parse(mentionIdsStr);
            msg.mentionIds = mentionIdsJson.get<std::vector<std::string>>();
        }
    } catch (...) {
    }

    try {
        const char *mentionRoleIdsStr = reinterpret_cast<const char *>(sqlite3_column_text(stmt, idx++));
        if (mentionRoleIdsStr) {
            nlohmann::json mentionRoleIdsJson = nlohmann::json::parse(mentionRoleIdsStr);
            msg.mentionRoleIds = mentionRoleIdsJson.get<std::vector<std::string>>();
        }
    } catch (...) {
    }

    idx++;
    idx++;

    return msg;
}

bool Database::messageExists(const std::string &messageId) {
    std::scoped_lock lock(m_mutex);

    if (!m_db) {
        return false;
    }

    const char *sql = "SELECT 1 FROM messages WHERE id = ? LIMIT 1";

    sqlite3_stmt *stmt = prepare(sql);
    if (!stmt) {
        return false;
    }

    sqlite3_bind_text(stmt, 1, messageId.c_str(), -1, SQLITE_TRANSIENT);

    bool exists = (sqlite3_step(stmt) == SQLITE_ROW);

    sqlite3_finalize(stmt);
    return exists;
}

bool Database::insertChannel(const std::string &channelId, const std::string &name, const std::string &type) {
    std::scoped_lock lock(m_mutex);

    if (!m_db) {
        return false;
    }

    const char *sql = R"(
        INSERT OR REPLACE INTO channels (id, name, type)
        VALUES (?, ?, ?)
    )";

    sqlite3_stmt *stmt = prepare(sql);
    if (!stmt) {
        return false;
    }

    sqlite3_bind_text(stmt, 1, channelId.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, name.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, type.c_str(), -1, SQLITE_TRANSIENT);

    int result = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    return result == SQLITE_DONE;
}

int64_t Database::toUnixMs(const std::chrono::system_clock::time_point &tp) const {
    return std::chrono::duration_cast<std::chrono::milliseconds>(tp.time_since_epoch()).count();
}

std::chrono::system_clock::time_point Database::fromUnixMs(int64_t ms) const {
    return std::chrono::system_clock::time_point(std::chrono::milliseconds(ms));
}

} // namespace Data
