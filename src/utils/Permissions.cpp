#include "utils/Permissions.h"
#include "models/PermissionOverwrite.h"
#include "models/Role.h"

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

uint64_t computeBasePermissions(const std::string &guildId, const std::vector<std::string> &userRoleIds,
                                const std::vector<Role> &guildRoles) {
    uint64_t permissions = 0;
    for (const auto &role : guildRoles) {
        if (role.id == guildId) {
            permissions |= role.getPermissionsInt();
            break;
        }
    }

    for (const auto &userRoleId : userRoleIds) {
        for (const auto &role : guildRoles) {
            if (role.id == userRoleId) {
                permissions |= role.getPermissionsInt();
                if (permissions & Permissions::ADMINISTRATOR) {
                    return permissions;
                }
                break;
            }
        }
    }

    return permissions;
}

bool canViewChannel(const std::string &guildId, const std::vector<std::string> &userRoleIds,
                    const std::vector<PermissionOverwrite> &permissionOverwrites, uint64_t basePermissions) {
    if (basePermissions & Permissions::ADMINISTRATOR) {
        return true;
    }
    bool canView = true;

    for (const auto &overwrite : permissionOverwrites) {
        if (overwrite.type == 0 && overwrite.id == guildId) {
            if (overwrite.deny & Permissions::VIEW_CHANNEL) {
                canView = false;
            }
            if (overwrite.allow & Permissions::VIEW_CHANNEL) {
                canView = true;
            }
            break;
        }
    }

    for (const auto &overwrite : permissionOverwrites) {
        if (overwrite.type == 0 && overwrite.id != guildId) {
            for (const auto &userRoleId : userRoleIds) {
                if (userRoleId == overwrite.id) {
                    if (overwrite.allow & Permissions::VIEW_CHANNEL) {
                        canView = true;
                    }
                    break;
                }
            }
        }
    }

    for (const auto &overwrite : permissionOverwrites) {
        if (overwrite.type == 0 && overwrite.id != guildId) {
            for (const auto &userRoleId : userRoleIds) {
                if (userRoleId == overwrite.id) {
                    if (overwrite.deny & Permissions::VIEW_CHANNEL) {
                        canView = false;
                    }
                    break;
                }
            }
        }
    }

    return canView;
}

uint64_t computeChannelPermissions(const std::string &userId, const std::string &guildId,
                                   const std::vector<std::string> &userRoleIds,
                                   const std::vector<PermissionOverwrite> &permissionOverwrites,
                                   uint64_t basePermissions) {
    if (basePermissions & Permissions::ADMINISTRATOR) {
        return basePermissions;
    }

    uint64_t permissions = basePermissions;
    for (const auto &overwrite : permissionOverwrites) {
        if (overwrite.type == 0 && overwrite.id == guildId) {
            permissions &= ~overwrite.deny;
            permissions |= overwrite.allow;
            break;
        }
    }

    uint64_t allow = 0;
    uint64_t deny = 0;
    for (const auto &overwrite : permissionOverwrites) {
        if (overwrite.type == 0 && overwrite.id != guildId) {
            for (const auto &userRoleId : userRoleIds) {
                if (userRoleId == overwrite.id) {
                    allow |= overwrite.allow;
                    deny |= overwrite.deny;
                    break;
                }
            }
        }
    }
    permissions &= ~deny;
    permissions |= allow;

    for (const auto &overwrite : permissionOverwrites) {
        if (overwrite.type == 1 && overwrite.id == userId) {
            permissions &= ~overwrite.deny;
            permissions |= overwrite.allow;
            break;
        }
    }

    return permissions;
}

} // namespace PermissionUtils
