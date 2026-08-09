#pragma once

#include "config_store.h"

#include <QDBusConnection>
#include <QDBusContext>
#include <QObject>
#include <QString>
#include <QTimer>

namespace HyprShelld {

class ConfigService final : public QObject, protected QDBusContext {
    Q_OBJECT
    Q_PROPERTY(uint BarHeight READ barHeight)
    Q_PROPERTY(qulonglong Revision READ revision)
    Q_PROPERTY(QString RecoveryState READ recoveryState)

public:
    ConfigService(
        ConfigStore store,
        const ConfigLoadResult &loaded,
        QDBusConnection connection,
        QObject *parent = nullptr
    );

    [[nodiscard]] uint barHeight() const;
    [[nodiscard]] qulonglong revision() const;
    [[nodiscard]] QString recoveryState() const;

    void authorizeLegacyWorkspaceRetirement();

public slots:
    qulonglong SetBarHeight(uint height);
    qulonglong ResetBarHeight();

private:
    qulonglong setBarHeight(uint height);
    void attemptLegacyWorkspaceRetirement();
    void reportError(const QString &name, const QString &message) const;
    void publishChange() const;

    ConfigStore store_;
    ConfigState state_;
    QString recoveryState_;
    QDBusConnection connection_;
    std::optional<LegacyWorkspaceSettings> legacyWorkspaceSettings_;
    bool legacyWorkspaceRetirementPending_ = false;
    bool legacyWorkspaceRetirementAuthorized_ = false;
    QTimer legacyWorkspaceRetirementTimer_;
};

} // namespace HyprShelld
