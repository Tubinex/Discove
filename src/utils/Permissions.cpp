#include "utils/Permissions.h"

namespace PermissionUtils {

bool hasPermission(uint64_t permissions, uint64_t permission) {
    if ((permissions & Permissions::ADMINISTRATOR) == Permissions::ADMINISTRATOR) {
        return true;
    }

    return (permissions & permission) == permission;
}

bool hasPermission(const std::string &permissionsStr, uint64_t permission) {
    uint64_t permissions = parsePermissions(permissionsStr);
    return hasPermission(permissions, permission);
}

uint64_t parsePermissions(const std::string &permissionsStr) {
    if (permissionsStr.empty()) {
        return 0;
    }
    try {
        return std::stoull(permissionsStr);
    } catch (...) {
        return 0;
    }
}

std::string permissionsToString(uint64_t permissions) { return std::to_string(permissions); }

} // namespace PermissionUtils
