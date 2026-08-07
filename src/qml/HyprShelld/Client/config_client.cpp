#include "config_client.h"

#include "config/config_values.h"
#include "config1_interface.h"

#include <QDBusError>
#include <QDBusMessage>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDBusServiceWatcher>
#include <QVariant>

#include <utility>

namespace HyprShelld {
namespace {

const QString serviceName = QStringLiteral("org.hyprshelld.Config1");
const QString objectPath = QStringLiteral("/org/hyprshelld/Config1");
const QString interfaceName = QStringLiteral("org.hyprshelld.Config1");
const QString propertiesInterface = QStringLiteral(
    "org.freedesktop.DBus.Properties"
);

} // namespace

ConfigClient::ConfigClient(QObject *parent)
    : ConfigClient(QDBusConnection::sessionBus(), parent)
{
}

ConfigClient::ConfigClient(QDBusConnection connection, QObject *parent)
    : QObject(parent)
    , connection_(std::move(connection))
    , barHeight_(ConfigValues::defaultBarHeight)
{
    interface_ = new OrgHyprshelldConfig1Interface(
        serviceName,
        objectPath,
        connection_,
        this
    );

    serviceWatcher_ = new QDBusServiceWatcher(
        serviceName,
        connection_,
        QDBusServiceWatcher::WatchForOwnerChange,
        this
    );
    connect(
        serviceWatcher_,
        &QDBusServiceWatcher::serviceOwnerChanged,
        this,
        &ConfigClient::serviceOwnerChanged
    );

    connection_.connect(
        serviceName,
        objectPath,
        propertiesInterface,
        QStringLiteral("PropertiesChanged"),
        this,
        SLOT(propertiesChanged(QString,QVariantMap,QStringList))
    );

    refresh();
}

bool ConfigClient::available() const
{
    return available_;
}

bool ConfigClient::busy() const
{
    return pendingOperations_ > 0;
}

uint ConfigClient::barHeight() const
{
    return barHeight_;
}

qulonglong ConfigClient::revision() const
{
    return revision_;
}

QString ConfigClient::recoveryState() const
{
    return recoveryState_;
}

uint ConfigClient::minimumBarHeight() const
{
    return ConfigValues::minimumBarHeight;
}

uint ConfigClient::maximumBarHeight() const
{
    return ConfigValues::maximumBarHeight;
}

uint ConfigClient::defaultBarHeight() const
{
    return ConfigValues::defaultBarHeight;
}

QString ConfigClient::lastErrorName() const
{
    return lastErrorName_;
}

QString ConfigClient::lastErrorMessage() const
{
    return lastErrorMessage_;
}

void ConfigClient::setBarHeight(uint height)
{
    beginMutation(interface_->SetBarHeight(height));
}

void ConfigClient::resetBarHeight()
{
    beginMutation(interface_->ResetBarHeight());
}

void ConfigClient::clearError()
{
    if (lastErrorName_.isEmpty() && lastErrorMessage_.isEmpty()) {
        return;
    }

    lastErrorName_.clear();
    lastErrorMessage_.clear();
    emit lastErrorChanged();
}

void ConfigClient::propertiesChanged(
    const QString &changedInterface,
    const QVariantMap &changed,
    const QStringList &invalidated
)
{
    if (changedInterface != interfaceName) {
        return;
    }

    applyProperties(changed);
    setAvailable(true);

    for (const auto &property : invalidated) {
        if (property == QStringLiteral("BarHeight")
            || property == QStringLiteral("Revision")
            || property == QStringLiteral("RecoveryState")) {
            refresh();
            break;
        }
    }
}

void ConfigClient::serviceOwnerChanged(
    const QString &name,
    const QString &oldOwner,
    const QString &newOwner
)
{
    Q_UNUSED(name)
    Q_UNUSED(oldOwner)

    ++ownerGeneration_;
    setAvailable(false);

    if (!newOwner.isEmpty()) {
        refresh();
    }
}

void ConfigClient::refresh()
{
    auto message = QDBusMessage::createMethodCall(
        serviceName,
        objectPath,
        propertiesInterface,
        QStringLiteral("GetAll")
    );
    message.setArguments({interfaceName});

    const auto generation = ownerGeneration_;
    auto *watcher = new QDBusPendingCallWatcher(
        connection_.asyncCall(message),
        this
    );
    connect(
        watcher,
        &QDBusPendingCallWatcher::finished,
        this,
        [this, watcher, generation] {
            const QDBusPendingReply<QVariantMap> reply = *watcher;
            watcher->deleteLater();

            if (generation != ownerGeneration_) {
                return;
            }

            if (reply.isError()) {
                setAvailable(false);
                return;
            }

            applyProperties(reply.value());
            setAvailable(true);
        }
    );
}

void ConfigClient::applyProperties(const QVariantMap &properties)
{
    bool barHeightChanged = false;
    bool revisionChanged = false;
    bool recoveryStateChanged = false;

    const auto barHeight = properties.constFind(QStringLiteral("BarHeight"));
    if (barHeight != properties.cend()) {
        const auto value = barHeight->toUInt();
        if (value != barHeight_) {
            barHeight_ = value;
            barHeightChanged = true;
        }
    }

    const auto revision = properties.constFind(QStringLiteral("Revision"));
    if (revision != properties.cend()) {
        const auto value = revision->toULongLong();
        if (value != revision_) {
            revision_ = value;
            revisionChanged = true;
        }
    }

    const auto recoveryState = properties.constFind(
        QStringLiteral("RecoveryState")
    );
    if (recoveryState != properties.cend()) {
        const auto value = recoveryState->toString();
        if (value != recoveryState_) {
            recoveryState_ = value;
            recoveryStateChanged = true;
        }
    }

    if (barHeightChanged) {
        emit this->barHeightChanged();
    }
    if (revisionChanged) {
        emit this->revisionChanged();
    }
    if (recoveryStateChanged) {
        emit this->recoveryStateChanged();
    }
}

void ConfigClient::beginMutation(const QDBusPendingCall &call)
{
    clearError();

    const auto wasBusy = busy();
    ++pendingOperations_;
    if (!wasBusy) {
        emit busyChanged();
    }

    auto *watcher = new QDBusPendingCallWatcher(call, this);
    connect(
        watcher,
        &QDBusPendingCallWatcher::finished,
        this,
        [this, watcher] {
            const QDBusPendingReply<qulonglong> reply = *watcher;
            watcher->deleteLater();

            if (reply.isError()) {
                setError(reply.error().name(), reply.error().message());
                if (reply.error().type() == QDBusError::ServiceUnknown) {
                    setAvailable(false);
                }
            } else if (!available_) {
                refresh();
            }

            --pendingOperations_;
            if (!busy()) {
                emit busyChanged();
            }
        }
    );
}

void ConfigClient::setAvailable(bool available)
{
    if (available == available_) {
        return;
    }

    available_ = available;
    emit availableChanged();
}

void ConfigClient::setError(const QString &name, const QString &message)
{
    if (name != lastErrorName_ || message != lastErrorMessage_) {
        lastErrorName_ = name;
        lastErrorMessage_ = message;
        emit lastErrorChanged();
    }

    emit operationFailed(name, message);
}

} // namespace HyprShelld
