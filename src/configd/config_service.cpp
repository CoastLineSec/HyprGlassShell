#include "config_service.h"

#include <QDBusMessage>
#include <QDebug>
#include <QVariantMap>

#include <limits>
#include <utility>

namespace HyprShelld {
namespace {

constexpr auto defaultBarHeight = 48U;
constexpr auto minimumBarHeight = 32U;
constexpr auto maximumBarHeight = 96U;

const QString configInterface = QStringLiteral("org.hyprshelld.Config1");
const QString configPath = QStringLiteral("/org/hyprshelld/Config1");
const QString invalidBarHeightError = QStringLiteral(
    "org.hyprshelld.Config1.Error.InvalidBarHeight"
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
{
}

uint ConfigService::barHeight() const
{
    return state_.barHeight;
}

qulonglong ConfigService::revision() const
{
    return state_.revision;
}

QString ConfigService::recoveryState() const
{
    return recoveryState_;
}

qulonglong ConfigService::SetBarHeight(uint height)
{
    return setBarHeight(height);
}

qulonglong ConfigService::ResetBarHeight()
{
    return setBarHeight(defaultBarHeight);
}

qulonglong ConfigService::setBarHeight(uint height)
{
    if (height < minimumBarHeight || height > maximumBarHeight) {
        reportError(
            invalidBarHeightError,
            QStringLiteral("Bar height must be between 32 and 96 logical pixels")
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

    const ConfigState next {
        .barHeight = height,
        .revision = state_.revision + 1,
    };

    QString error;
    if (!store_.persist(state_, next, error)) {
        reportError(persistenceError, error);
        return state_.revision;
    }

    state_ = next;
    publishChange();
    return state_.revision;
}

void ConfigService::reportError(const QString &name, const QString &message) const
{
    if (calledFromDBus()) {
        sendErrorReply(name, message);
    }
}

void ConfigService::publishChange() const
{
    QVariantMap changed;
    changed.insert(QStringLiteral("BarHeight"), state_.barHeight);
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
