#pragma once

#include "config/config_values.h"

#include <QString>
#include <QtTypes>

namespace HyprShelld {

struct ConfigState final {
    quint32 barHeight = ConfigValues::defaultBarHeight;
    quint64 revision = 0;

    friend bool operator==(const ConfigState &, const ConfigState &) = default;
};

enum class ConfigRecoveryState {
    Normal,
    Recovered,
    Defaulted,
};

struct ConfigPaths final {
    QString activeFile;
    QString recoveryFile;

    [[nodiscard]] static ConfigPaths standard();
};

struct ConfigLoadResult final {
    bool success = false;
    ConfigState state;
    ConfigRecoveryState recoveryState = ConfigRecoveryState::Normal;
    QString error;
};

class ConfigStore final {
public:
    explicit ConfigStore(ConfigPaths paths);

    [[nodiscard]] ConfigLoadResult load() const;
    [[nodiscard]] bool persist(
        const ConfigState &current,
        const ConfigState &next,
        QString &error
    ) const;

private:
    ConfigPaths paths_;
};

} // namespace HyprShelld
