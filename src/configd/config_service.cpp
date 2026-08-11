#include "config_service.h"

#include "config/config_values.h"

#include <QDBusMessage>
#include <QDebug>
#include <QVariantMap>

#include <limits>
#include <utility>

namespace HyprShelld {
namespace {

const QString configInterface = QStringLiteral("org.hyprshelld.Config1");
const QString configPath = QStringLiteral("/org/hyprshelld/Config1");
const QString invalidBarHeightError = QStringLiteral(
    "org.hyprshelld.Config1.Error.InvalidBarHeight"
);
const QString invalidSharedBorderError = QStringLiteral(
    "org.hyprshelld.Config1.Error.InvalidSharedBorder"
);
const QString persistenceError = QStringLiteral(
    "org.hyprshelld.Config1.Error.PersistenceFailed"
);

QString recoveryStateName(ConfigRecoveryState state)
{
    switch (state) {
    case ConfigRecoveryState::Normal:
        return QStringLiteral("normal");
    case ConfigRecoveryState::Recovered:
        return QStringLiteral("recovered");
    case ConfigRecoveryState::Defaulted:
        return QStringLiteral("defaulted");
    }

    Q_UNREACHABLE_RETURN(QString());
}

} // namespace

ConfigService::ConfigService(
    ConfigStore store,
    const ConfigLoadResult &loaded,
    QDBusConnection connection,
    QObject *parent
)
    : QObject(parent)
    , store_(std::move(store))
    , state_(loaded.state)
    , recoveryState_(recoveryStateName(loaded.recoveryState))
    , connection_(std::move(connection))
    , legacyWorkspaceSettings_(loaded.legacyWorkspaceSettings)
    , legacyWorkspaceRetirementPending_(
          loaded.legacyWorkspaceRetirementPending
      )
{
    legacyWorkspaceRetirementTimer_.setSingleShot(true);
    legacyWorkspaceRetirementTimer_.setInterval(1000);
    connect(
        &legacyWorkspaceRetirementTimer_,
        &QTimer::timeout,
        this,
        &ConfigService::attemptLegacyWorkspaceRetirement
    );
}

uint ConfigService::barHeight() const
{
    return state_.barHeight;
}

bool ConfigService::shellBorderEnabled() const
{
    return state_.shellBorderEnabled;
}

uint ConfigService::shellBorderWidth() const
{
    return state_.shellBorderWidth;
}

uint ConfigService::shellBorderRadius() const
{
    return state_.shellBorderRadius;
}

bool ConfigService::syncHyprlandWindowBorders() const
{
    return state_.syncHyprlandWindowBorders;
}

qulonglong ConfigService::revision() const
{
    return state_.revision;
}

QString ConfigService::recoveryState() const
{
    return recoveryState_;
}

void ConfigService::authorizeLegacyWorkspaceRetirement()
{
    legacyWorkspaceRetirementAuthorized_ = true;
    attemptLegacyWorkspaceRetirement();
}

qulonglong ConfigService::SetBarHeight(uint height)
{
    return setBarHeight(height);
}

qulonglong ConfigService::ResetBarHeight()
{
    return setBarHeight(ConfigValues::defaultBarHeight);
}

qulonglong ConfigService::SetSharedBorder(
    const bool enabled,
    const uint width,
    const uint radius,
    const bool syncHyprlandWindowBorders
)
{
    return setSharedBorder(
        enabled,
        width,
        radius,
        syncHyprlandWindowBorders
    );
}

qulonglong ConfigService::ResetSharedBorder()
{
    return setSharedBorder(
        ConfigValues::defaultShellBorderEnabled,
        ConfigValues::defaultShellBorderWidth,
        ConfigValues::defaultShellBorderRadius,
        ConfigValues::defaultSyncHyprlandWindowBorders
    );
}

qulonglong ConfigService::setBarHeight(uint height)
{
    if (height < ConfigValues::minimumBarHeight
        || height > ConfigValues::maximumBarHeight) {
        reportError(
            invalidBarHeightError,
            QStringLiteral("Bar height must be between %1 and %2 logical pixels")
                .arg(ConfigValues::minimumBarHeight)
                .arg(ConfigValues::maximumBarHeight)
        );
        return state_.revision;
    }

    if (height == state_.barHeight) {
        return state_.revision;
    }

    if (state_.revision == std::numeric_limits<quint64>::max()) {
        reportError(persistenceError, QStringLiteral("Configuration revision is exhausted"));
        return state_.revision;
    }

    auto next = state_;
    next.barHeight = height;
    next.revision = state_.revision + 1;

    QString error;
    if (!store_.persist(
            state_,
            next,
            legacyWorkspaceSettings_,
            error
        )) {
        reportError(persistenceError, error);
        return state_.revision;
    }

    const auto previous = state_;
    state_ = next;
    publishChange(previous);
    return state_.revision;
}

qulonglong ConfigService::setSharedBorder(
    const bool enabled,
    const uint width,
    const uint radius,
    const bool syncHyprlandWindowBorders
)
{
    if (width < ConfigValues::minimumShellBorderWidth
        || width > ConfigValues::maximumShellBorderWidth
        || radius < ConfigValues::minimumShellBorderRadius
        || radius > ConfigValues::maximumShellBorderRadius) {
        reportError(
            invalidSharedBorderError,
            QStringLiteral(
                "Shared border width and radius must each be between %1 and %2 logical pixels"
            )
                .arg(ConfigValues::minimumShellBorderWidth)
                .arg(ConfigValues::maximumShellBorderWidth)
        );
        return state_.revision;
    }

    if (enabled == state_.shellBorderEnabled
        && width == state_.shellBorderWidth
        && radius == state_.shellBorderRadius
        && syncHyprlandWindowBorders
            == state_.syncHyprlandWindowBorders) {
        return state_.revision;
    }

    if (state_.revision == std::numeric_limits<quint64>::max()) {
        reportError(
            persistenceError,
            QStringLiteral("Configuration revision is exhausted")
        );
        return state_.revision;
    }

    auto next = state_;
    next.shellBorderEnabled = enabled;
    next.shellBorderWidth = width;
    next.shellBorderRadius = radius;
    next.syncHyprlandWindowBorders = syncHyprlandWindowBorders;
    next.revision = state_.revision + 1;

    QString error;
    if (!store_.persist(state_, next, legacyWorkspaceSettings_, error)) {
        reportError(persistenceError, error);
        return state_.revision;
    }

    const auto previous = state_;
    state_ = next;
    publishChange(previous);
    return state_.revision;
}

void ConfigService::attemptLegacyWorkspaceRetirement()
{
    if (!legacyWorkspaceRetirementAuthorized_
        || !legacyWorkspaceRetirementPending_) {
        return;
    }

    QString error;
    if (store_.retireLegacyWorkspaceSettings(state_, error)) {
        legacyWorkspaceRetirementPending_ = false;
        legacyWorkspaceSettings_.reset();
        legacyWorkspaceRetirementTimer_.stop();
        return;
    }

    qWarning().noquote()
        << QStringLiteral("Failed to retire migrated workspace settings: %1")
               .arg(error);
    if (!legacyWorkspaceRetirementTimer_.isActive()) {
        legacyWorkspaceRetirementTimer_.start();
    }
}

void ConfigService::reportError(const QString &name, const QString &message) const
{
    if (calledFromDBus()) {
        sendErrorReply(name, message);
    }
}

void ConfigService::publishChange(const ConfigState &previous) const
{
    QVariantMap changed;
    if (state_.barHeight != previous.barHeight) {
        changed.insert(QStringLiteral("BarHeight"), state_.barHeight);
    }
    if (state_.shellBorderEnabled != previous.shellBorderEnabled
        || state_.shellBorderWidth != previous.shellBorderWidth
        || state_.shellBorderRadius != previous.shellBorderRadius
        || state_.syncHyprlandWindowBorders
            != previous.syncHyprlandWindowBorders) {
        changed.insert(
            QStringLiteral("ShellBorderEnabled"),
            state_.shellBorderEnabled
        );
        changed.insert(
            QStringLiteral("ShellBorderWidth"),
            state_.shellBorderWidth
        );
        changed.insert(
            QStringLiteral("ShellBorderRadius"),
            state_.shellBorderRadius
        );
        changed.insert(
            QStringLiteral("SyncHyprlandWindowBorders"),
            state_.syncHyprlandWindowBorders
        );
    }
    changed.insert(
        QStringLiteral("Revision"),
        QVariant::fromValue<qulonglong>(state_.revision)
    );

    auto signal = QDBusMessage::createSignal(
        configPath,
        QStringLiteral("org.freedesktop.DBus.Properties"),
        QStringLiteral("PropertiesChanged")
    );
    signal.setArguments({configInterface, changed, QStringList()});

    if (!connection_.send(signal)) {
        qWarning() << "Failed to publish configuration change";
    }
}

} // namespace HyprShelld
