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
    Q_PROPERTY(bool ShellBorderEnabled READ shellBorderEnabled)
    Q_PROPERTY(uint ShellBorderWidth READ shellBorderWidth)
    Q_PROPERTY(uint ShellBorderRadius READ shellBorderRadius)
    Q_PROPERTY(bool SyncHyprlandWindowBorders READ syncHyprlandWindowBorders)
    Q_PROPERTY(uint ShellInnerSpacing READ shellInnerSpacing)
    Q_PROPERTY(uint ShellOuterSpacing READ shellOuterSpacing)
    Q_PROPERTY(bool SyncHyprlandWindowSpacing READ syncHyprlandWindowSpacing)
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
    [[nodiscard]] bool shellBorderEnabled() const;
    [[nodiscard]] uint shellBorderWidth() const;
    [[nodiscard]] uint shellBorderRadius() const;
    [[nodiscard]] bool syncHyprlandWindowBorders() const;
    [[nodiscard]] uint shellInnerSpacing() const;
    [[nodiscard]] uint shellOuterSpacing() const;
    [[nodiscard]] bool syncHyprlandWindowSpacing() const;
    [[nodiscard]] qulonglong revision() const;
    [[nodiscard]] QString recoveryState() const;

    void authorizeLegacyWorkspaceRetirement();

public slots:
    qulonglong SetBarHeight(uint height);
    qulonglong ResetBarHeight();
    qulonglong SetSharedBorder(
        bool enabled,
        uint width,
        uint radius,
        bool syncHyprlandWindowBorders
    );
    qulonglong ResetSharedBorder();
    qulonglong SetSharedSpacing(
        uint inner,
        uint outer,
        bool syncHyprlandWindowSpacing
    );
    qulonglong ResetSharedSpacing();

private:
    qulonglong setBarHeight(uint height);
    qulonglong setSharedBorder(
        bool enabled,
        uint width,
        uint radius,
        bool syncHyprlandWindowBorders
    );
    qulonglong setSharedSpacing(
        uint inner,
        uint outer,
        bool syncHyprlandWindowSpacing
    );
    void attemptLegacyWorkspaceRetirement();
    void reportError(const QString &name, const QString &message) const;
    void publishChange(const ConfigState &previous) const;

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
