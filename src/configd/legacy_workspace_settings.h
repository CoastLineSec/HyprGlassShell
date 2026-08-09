#pragma once

#include <QString>
#include <QtTypes>

namespace HyprShelld {

struct LegacyWorkspaceSettings final {
    QString labelMode;
    bool showApplications = false;
    quint32 maximumApplications = 0;
    bool occupiedOnly = false;
    QString scrollMode;

    friend bool operator==(
        const LegacyWorkspaceSettings &,
        const LegacyWorkspaceSettings &
    ) = default;
};

} // namespace HyprShelld
