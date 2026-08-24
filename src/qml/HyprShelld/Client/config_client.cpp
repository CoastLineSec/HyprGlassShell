#include "config_client.h"

#include "config/config_values.h"
#include "config1_interface.h"

#include <QDBusError>
#include <QDBusMessage>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDBusServiceWatcher>
#include <QMetaType>
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
    , shellBorderEnabled_(ConfigValues::defaultShellBorderEnabled)
    , shellBorderWidth_(ConfigValues::defaultShellBorderWidth)
    , shellBorderRadius_(ConfigValues::defaultShellBorderRadius)
    , syncHyprlandWindowBorders_(
          ConfigValues::defaultSyncHyprlandWindowBorders
      )
    , shellInnerSpacing_(ConfigValues::defaultShellInnerSpacing)
    , shellOuterSpacing_(ConfigValues::defaultShellOuterSpacing)
    , syncHyprlandWindowSpacing_(
          ConfigValues::defaultSyncHyprlandWindowSpacing
      )
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

bool ConfigClient::shellBorderEnabled() const
{
    return shellBorderEnabled_;
}

uint ConfigClient::shellBorderWidth() const
{
    return shellBorderWidth_;
}

uint ConfigClient::shellBorderRadius() const
{
    return shellBorderRadius_;
}

bool ConfigClient::syncHyprlandWindowBorders() const
{
    return syncHyprlandWindowBorders_;
}

uint ConfigClient::shellInnerSpacing() const
{
    return shellInnerSpacing_;
}

uint ConfigClient::shellOuterSpacing() const
{
    return shellOuterSpacing_;
}

bool ConfigClient::syncHyprlandWindowSpacing() const
{
    return syncHyprlandWindowSpacing_;
}

qulonglong ConfigClient::revision() const
{
    return revision_;
}

QString ConfigClient::revisionToken() const
{
    return QString::number(revision_);
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

bool ConfigClient::defaultShellBorderEnabled() const
{
    return ConfigValues::defaultShellBorderEnabled;
}

uint ConfigClient::minimumShellBorderWidth() const
{
    return ConfigValues::minimumShellBorderWidth;
}

uint ConfigClient::maximumShellBorderWidth() const
{
    return ConfigValues::maximumShellBorderWidth;
}

uint ConfigClient::defaultShellBorderWidth() const
{
    return ConfigValues::defaultShellBorderWidth;
}

uint ConfigClient::minimumShellBorderRadius() const
{
    return ConfigValues::minimumShellBorderRadius;
}

uint ConfigClient::maximumShellBorderRadius() const
{
    return ConfigValues::maximumShellBorderRadius;
}

uint ConfigClient::defaultShellBorderRadius() const
{
    return ConfigValues::defaultShellBorderRadius;
}

bool ConfigClient::defaultSyncHyprlandWindowBorders() const
{
    return ConfigValues::defaultSyncHyprlandWindowBorders;
}

uint ConfigClient::minimumShellSpacing() const
{
    return ConfigValues::minimumShellSpacing;
}

uint ConfigClient::maximumShellSpacing() const
{
    return ConfigValues::maximumShellSpacing;
}

uint ConfigClient::defaultShellInnerSpacing() const
{
    return ConfigValues::defaultShellInnerSpacing;
}

uint ConfigClient::defaultShellOuterSpacing() const
{
    return ConfigValues::defaultShellOuterSpacing;
}

bool ConfigClient::defaultSyncHyprlandWindowSpacing() const
{
    return ConfigValues::defaultSyncHyprlandWindowSpacing;
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

void ConfigClient::setSharedBorder(
    const bool enabled,
    const uint width,
    const uint radius,
    const bool syncHyprlandWindowBorders
)
{
    beginMutation(interface_->SetSharedBorder(
        enabled,
        width,
        radius,
        syncHyprlandWindowBorders
    ));
}

void ConfigClient::resetSharedBorder()
{
    beginMutation(interface_->ResetSharedBorder());
}

void ConfigClient::setSharedSpacing(
    const uint inner,
    const uint outer,
    const bool syncHyprlandWindowSpacing
)
{
    beginMutation(interface_->SetSharedSpacing(
        inner,
        outer,
        syncHyprlandWindowSpacing
    ));
}

void ConfigClient::resetSharedSpacing()
{
    beginMutation(interface_->ResetSharedSpacing());
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

    for (const auto &property : invalidated) {
        if (property == QStringLiteral("BarHeight")
            || property == QStringLiteral("ShellBorderEnabled")
            || property == QStringLiteral("ShellBorderWidth")
            || property == QStringLiteral("ShellBorderRadius")
            || property == QStringLiteral("SyncHyprlandWindowBorders")
            || property == QStringLiteral("ShellInnerSpacing")
            || property == QStringLiteral("ShellOuterSpacing")
            || property == QStringLiteral("SyncHyprlandWindowSpacing")
            || property == QStringLiteral("Revision")
            || property == QStringLiteral("RecoveryState")) {
            setAvailable(false);
            refresh();
            return;
        }
    }

    if (!available_ || !applyProperties(changed, false)) {
        setAvailable(false);
        refresh();
        return;
    }

    setAvailable(true);
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

            if (!applyProperties(reply.value(), true)) {
                setAvailable(false);
                return;
            }
            setAvailable(true);
        }
    );
}

bool ConfigClient::applyProperties(
    const QVariantMap &properties,
    const bool requireComplete
)
{
    const QStringList required{
        QStringLiteral("BarHeight"),
        QStringLiteral("ShellBorderEnabled"),
        QStringLiteral("ShellBorderWidth"),
        QStringLiteral("ShellBorderRadius"),
        QStringLiteral("SyncHyprlandWindowBorders"),
        QStringLiteral("ShellInnerSpacing"),
        QStringLiteral("ShellOuterSpacing"),
        QStringLiteral("SyncHyprlandWindowSpacing"),
        QStringLiteral("Revision"),
        QStringLiteral("RecoveryState"),
    };
    if (requireComplete) {
        for (const auto &name : required) {
            if (!properties.contains(name)) {
                return false;
            }
        }
    }

    const QStringList sharedBorderProperties{
        QStringLiteral("ShellBorderEnabled"),
        QStringLiteral("ShellBorderWidth"),
        QStringLiteral("ShellBorderRadius"),
        QStringLiteral("SyncHyprlandWindowBorders"),
    };
    const QStringList sharedSpacingProperties{
        QStringLiteral("ShellInnerSpacing"),
        QStringLiteral("ShellOuterSpacing"),
        QStringLiteral("SyncHyprlandWindowSpacing"),
    };
    auto sharedBorderSupplied = false;
    for (const auto &name : sharedBorderProperties) {
        sharedBorderSupplied = sharedBorderSupplied
            || properties.contains(name);
    }
    if (sharedBorderSupplied) {
        for (const auto &name : sharedBorderProperties) {
            if (!properties.contains(name)) {
                return false;
            }
        }
    }
    auto sharedSpacingSupplied = false;
    for (const auto &name : sharedSpacingProperties) {
        sharedSpacingSupplied = sharedSpacingSupplied
            || properties.contains(name);
    }
    if (sharedSpacingSupplied) {
        for (const auto &name : sharedSpacingProperties) {
            if (!properties.contains(name)) {
                return false;
            }
        }
    }
    if ((properties.contains(QStringLiteral("BarHeight"))
         || sharedBorderSupplied || sharedSpacingSupplied)
        && !properties.contains(QStringLiteral("Revision"))) {
        return false;
    }

    auto nextBarHeight = barHeight_;
    auto nextShellBorderEnabled = shellBorderEnabled_;
    auto nextShellBorderWidth = shellBorderWidth_;
    auto nextShellBorderRadius = shellBorderRadius_;
    auto nextSyncHyprlandWindowBorders = syncHyprlandWindowBorders_;
    auto nextShellInnerSpacing = shellInnerSpacing_;
    auto nextShellOuterSpacing = shellOuterSpacing_;
    auto nextSyncHyprlandWindowSpacing = syncHyprlandWindowSpacing_;
    auto nextRevision = revision_;
    auto nextRecoveryState = recoveryState_;

    const auto barHeight = properties.constFind(QStringLiteral("BarHeight"));
    if (barHeight != properties.cend()) {
        if (barHeight->metaType().id() != QMetaType::UInt) {
            return false;
        }
        nextBarHeight = barHeight->toUInt();
        if (nextBarHeight < ConfigValues::minimumBarHeight
            || nextBarHeight > ConfigValues::maximumBarHeight) {
            return false;
        }
    }

    const auto shellBorderEnabled = properties.constFind(
        QStringLiteral("ShellBorderEnabled")
    );
    if (shellBorderEnabled != properties.cend()) {
        if (shellBorderEnabled->metaType().id() != QMetaType::Bool) {
            return false;
        }
        nextShellBorderEnabled = shellBorderEnabled->toBool();
    }

    const auto shellBorderWidth = properties.constFind(
        QStringLiteral("ShellBorderWidth")
    );
    if (shellBorderWidth != properties.cend()) {
        if (shellBorderWidth->metaType().id() != QMetaType::UInt) {
            return false;
        }
        nextShellBorderWidth = shellBorderWidth->toUInt();
        if (nextShellBorderWidth < ConfigValues::minimumShellBorderWidth
            || nextShellBorderWidth > ConfigValues::maximumShellBorderWidth) {
            return false;
        }
    }

    const auto shellBorderRadius = properties.constFind(
        QStringLiteral("ShellBorderRadius")
    );
    if (shellBorderRadius != properties.cend()) {
        if (shellBorderRadius->metaType().id() != QMetaType::UInt) {
            return false;
        }
        nextShellBorderRadius = shellBorderRadius->toUInt();
        if (nextShellBorderRadius < ConfigValues::minimumShellBorderRadius
            || nextShellBorderRadius
                > ConfigValues::maximumShellBorderRadius) {
            return false;
        }
    }

    const auto syncHyprlandWindowBorders = properties.constFind(
        QStringLiteral("SyncHyprlandWindowBorders")
    );
    if (syncHyprlandWindowBorders != properties.cend()) {
        if (syncHyprlandWindowBorders->metaType().id() != QMetaType::Bool) {
            return false;
        }
        nextSyncHyprlandWindowBorders = syncHyprlandWindowBorders->toBool();
    }

    const auto shellInnerSpacing = properties.constFind(
        QStringLiteral("ShellInnerSpacing")
    );
    if (shellInnerSpacing != properties.cend()) {
        if (shellInnerSpacing->metaType().id() != QMetaType::UInt) {
            return false;
        }
        nextShellInnerSpacing = shellInnerSpacing->toUInt();
        if (nextShellInnerSpacing < ConfigValues::minimumShellSpacing
            || nextShellInnerSpacing > ConfigValues::maximumShellSpacing) {
            return false;
        }
    }

    const auto shellOuterSpacing = properties.constFind(
        QStringLiteral("ShellOuterSpacing")
    );
    if (shellOuterSpacing != properties.cend()) {
        if (shellOuterSpacing->metaType().id() != QMetaType::UInt) {
            return false;
        }
        nextShellOuterSpacing = shellOuterSpacing->toUInt();
        if (nextShellOuterSpacing < ConfigValues::minimumShellSpacing
            || nextShellOuterSpacing > ConfigValues::maximumShellSpacing) {
            return false;
        }
    }

    const auto syncHyprlandWindowSpacing = properties.constFind(
        QStringLiteral("SyncHyprlandWindowSpacing")
    );
    if (syncHyprlandWindowSpacing != properties.cend()) {
        if (syncHyprlandWindowSpacing->metaType().id() != QMetaType::Bool) {
            return false;
        }
        nextSyncHyprlandWindowSpacing =
            syncHyprlandWindowSpacing->toBool();
    }

    const auto revision = properties.constFind(QStringLiteral("Revision"));
    if (revision != properties.cend()) {
        if (revision->metaType().id() != QMetaType::ULongLong) {
            return false;
        }
        nextRevision = revision->toULongLong();
    }

    const auto recoveryState = properties.constFind(
        QStringLiteral("RecoveryState")
    );
    if (recoveryState != properties.cend()) {
        if (recoveryState->metaType().id() != QMetaType::QString) {
            return false;
        }
        nextRecoveryState = recoveryState->toString();
    }

    const auto barHeightChanged = nextBarHeight != barHeight_;
    const auto sharedBorderChanged =
        nextShellBorderEnabled != shellBorderEnabled_
        || nextShellBorderWidth != shellBorderWidth_
        || nextShellBorderRadius != shellBorderRadius_
        || nextSyncHyprlandWindowBorders != syncHyprlandWindowBorders_;
    const auto sharedSpacingChanged =
        nextShellInnerSpacing != shellInnerSpacing_
        || nextShellOuterSpacing != shellOuterSpacing_
        || nextSyncHyprlandWindowSpacing != syncHyprlandWindowSpacing_;
    const auto revisionChanged = nextRevision != revision_;
    const auto recoveryStateChanged = nextRecoveryState != recoveryState_;

    if (projectionEstablished_
        && revision != properties.cend()
        && (nextRevision < revision_
            || (nextRevision == revision_
                && (barHeightChanged || sharedBorderChanged
                    || sharedSpacingChanged)))) {
        return false;
    }

    barHeight_ = nextBarHeight;
    shellBorderEnabled_ = nextShellBorderEnabled;
    shellBorderWidth_ = nextShellBorderWidth;
    shellBorderRadius_ = nextShellBorderRadius;
    syncHyprlandWindowBorders_ = nextSyncHyprlandWindowBorders;
    shellInnerSpacing_ = nextShellInnerSpacing;
    shellOuterSpacing_ = nextShellOuterSpacing;
    syncHyprlandWindowSpacing_ = nextSyncHyprlandWindowSpacing;
    revision_ = nextRevision;
    recoveryState_ = nextRecoveryState;
    projectionEstablished_ = true;

    if (barHeightChanged) {
        emit this->barHeightChanged();
    }
    if (sharedBorderChanged) {
        emit this->sharedBorderChanged();
    }
    if (sharedSpacingChanged) {
        emit this->sharedSpacingChanged();
    }
    if (revisionChanged) {
        emit this->revisionChanged();
    }
    if (recoveryStateChanged) {
        emit this->recoveryStateChanged();
    }

    return true;
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
                const auto error = reply.error();
                setError(error.name(), error.message());
                if (error.type() == QDBusError::ServiceUnknown) {
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

    if (available_) {
        clearError();
    }
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
