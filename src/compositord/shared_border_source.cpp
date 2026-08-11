#include "shared_border_source.h"

#include <QDBusMessage>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDBusServiceWatcher>
#include <QDBusVariant>
#include <QMetaType>

#include <optional>
#include <utility>

namespace HyprShelld::Compositor {
namespace {

const QString serviceName = QStringLiteral("org.hyprshelld.Config1");
const QString objectPath = QStringLiteral("/org/hyprshelld/Config1");
const QString interfaceName = QStringLiteral("org.hyprshelld.Config1");
const QString propertiesInterface = QStringLiteral(
    "org.freedesktop.DBus.Properties"
);

QVariant unwrapped(const QVariant &value)
{
    if (value.metaType().id() == qMetaTypeId<QDBusVariant>()) {
        return value.value<QDBusVariant>().variant();
    }
    return value;
}

std::optional<SharedBorderProjection> decode(const QVariantMap &properties)
{
    const auto enabled = unwrapped(properties.value(
        QStringLiteral("ShellBorderEnabled")
    ));
    const auto width = unwrapped(properties.value(
        QStringLiteral("ShellBorderWidth")
    ));
    const auto radius = unwrapped(properties.value(
        QStringLiteral("ShellBorderRadius")
    ));
    const auto sync = unwrapped(properties.value(
        QStringLiteral("SyncHyprlandWindowBorders")
    ));
    const auto revision = unwrapped(properties.value(
        QStringLiteral("Revision")
    ));
    if (enabled.metaType().id() != QMetaType::Bool
        || width.metaType().id() != QMetaType::UInt
        || radius.metaType().id() != QMetaType::UInt
        || sync.metaType().id() != QMetaType::Bool
        || revision.metaType().id() != QMetaType::ULongLong
        || width.toUInt() > 20U
        || radius.toUInt() > 20U) {
        return std::nullopt;
    }
    return SharedBorderProjection{
        .borderEnabled = enabled.toBool(),
        .borderWidth = width.toUInt(),
        .borderRadius = radius.toUInt(),
        .syncWindowBorders = sync.toBool(),
        .revision = revision.toULongLong(),
    };
}

bool includesSharedBorderProperty(
    const QVariantMap &changed,
    const QStringList &invalidated
)
{
    for (const auto &name : {
             QStringLiteral("ShellBorderEnabled"),
             QStringLiteral("ShellBorderWidth"),
             QStringLiteral("ShellBorderRadius"),
             QStringLiteral("SyncHyprlandWindowBorders"),
             QStringLiteral("Revision"),
         }) {
        if (changed.contains(name) || invalidated.contains(name)) {
            return true;
        }
    }
    return false;
}

bool invalidatesSharedBorderProperty(const QStringList &invalidated)
{
    for (const auto &name : {
             QStringLiteral("ShellBorderEnabled"),
             QStringLiteral("ShellBorderWidth"),
             QStringLiteral("ShellBorderRadius"),
             QStringLiteral("SyncHyprlandWindowBorders"),
             QStringLiteral("Revision"),
         }) {
        if (invalidated.contains(name)) {
            return true;
        }
    }
    return false;
}

qsizetype suppliedSharedBorderValues(const QVariantMap &changed)
{
    qsizetype supplied = 0;
    for (const auto &name : {
             QStringLiteral("ShellBorderEnabled"),
             QStringLiteral("ShellBorderWidth"),
             QStringLiteral("ShellBorderRadius"),
             QStringLiteral("SyncHyprlandWindowBorders"),
         }) {
        if (changed.contains(name)) {
            ++supplied;
        }
    }
    return supplied;
}

QString boundedError(QString error)
{
    constexpr qsizetype maximumErrorLength = 1024;
    if (error.isEmpty()) {
        return QStringLiteral("Shared visual settings are unavailable");
    }
    if (error.size() > maximumErrorLength) {
        error.truncate(maximumErrorLength);
    }
    return error;
}

} // namespace

SharedBorderSource::SharedBorderSource(QObject *parent)
    : QObject(parent)
{
}

bool SharedBorderSource::available() const
{
    return available_;
}

const SharedBorderProjection &SharedBorderSource::projection() const
{
    return projection_;
}

QString SharedBorderSource::error() const
{
    return error_;
}

void SharedBorderSource::publishProjection(
    const SharedBorderProjection &projection
)
{
    const auto changed = !available_ || projection != projection_
        || !error_.isEmpty();
    available_ = true;
    projection_ = projection;
    error_.clear();
    if (changed) {
        emit this->changed();
    }
}

void SharedBorderSource::publishUnavailable(const QString &error)
{
    const auto nextError = boundedError(error);
    const auto changed = available_ || nextError != error_;
    available_ = false;
    error_ = nextError;
    if (changed) {
        emit this->changed();
    }
}

DbusSharedBorderSource::DbusSharedBorderSource(
    QDBusConnection connection,
    QObject *parent
)
    : SharedBorderSource(parent)
    , connection_(std::move(connection))
{
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
        &DbusSharedBorderSource::serviceOwnerChanged
    );
    connection_.connect(
        serviceName,
        objectPath,
        propertiesInterface,
        QStringLiteral("PropertiesChanged"),
        this,
        SLOT(propertiesChanged(QString,QVariantMap,QStringList))
    );
}

void DbusSharedBorderSource::start()
{
    if (started_) {
        return;
    }
    started_ = true;
    refresh();
}

void DbusSharedBorderSource::requestRefresh()
{
    if (started_) {
        refresh();
    }
}

void DbusSharedBorderSource::serviceOwnerChanged(
    const QString &name,
    const QString &oldOwner,
    const QString &newOwner
)
{
    Q_UNUSED(name)
    Q_UNUSED(oldOwner)
    if (!started_) {
        return;
    }
    ++generation_;
    publishUnavailable(QStringLiteral("Shared visual settings are unavailable"));
    if (!newOwner.isEmpty()) {
        refresh();
    }
}

void DbusSharedBorderSource::propertiesChanged(
    const QString &changedInterface,
    const QVariantMap &changed,
    const QStringList &invalidated
)
{
    if (!started_ || changedInterface != interfaceName
        || !includesSharedBorderProperty(changed, invalidated)) {
        return;
    }

    const auto markStaleAndRefresh = [this] {
        publishUnavailable(QStringLiteral(
            "Shared visual settings changed with an invalid or incomplete projection"
        ));
        refresh();
    };
    if (invalidatesSharedBorderProperty(invalidated)) {
        markStaleAndRefresh();
        return;
    }

    const auto suppliedValues = suppliedSharedBorderValues(changed);
    const auto suppliesRevision = changed.contains(QStringLiteral("Revision"));
    if (suppliedValues == 4 && suppliesRevision) {
        const auto next = decode(changed);
        if (!next
            || (available() && next->revision < projection().revision)) {
            markStaleAndRefresh();
            return;
        }
        ++generation_;
        publishProjection(*next);
        return;
    }

    if (suppliedValues == 0 && suppliesRevision && available()) {
        const auto revision = unwrapped(changed.value(
            QStringLiteral("Revision")
        ));
        if (revision.metaType().id() == QMetaType::ULongLong
            && revision.toULongLong() >= projection().revision) {
            auto next = projection();
            next.revision = revision.toULongLong();
            ++generation_;
            publishProjection(next);
            return;
        }
    }

    markStaleAndRefresh();
}

void DbusSharedBorderSource::refresh()
{
    auto message = QDBusMessage::createMethodCall(
        serviceName,
        objectPath,
        propertiesInterface,
        QStringLiteral("GetAll")
    );
    message.setArguments({interfaceName});

    const auto generation = ++generation_;
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
            if (generation != generation_) {
                return;
            }
            if (reply.isError()) {
                publishUnavailable(reply.error().message());
                return;
            }
            const auto projection = decode(reply.value());
            if (!projection) {
                publishUnavailable(QStringLiteral(
                    "Shared visual settings returned an invalid projection"
                ));
                return;
            }
            publishProjection(*projection);
        }
    );
}

} // namespace HyprShelld::Compositor
