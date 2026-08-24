#include "shared_visual_source.h"

#include "config/config_values.h"

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

std::optional<SharedVisualProjection> decode(const QVariantMap &properties)
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
    const auto innerSpacing = unwrapped(properties.value(
        QStringLiteral("ShellInnerSpacing")
    ));
    const auto outerSpacing = unwrapped(properties.value(
        QStringLiteral("ShellOuterSpacing")
    ));
    const auto syncSpacing = unwrapped(properties.value(
        QStringLiteral("SyncHyprlandWindowSpacing")
    ));
    const auto revision = unwrapped(properties.value(
        QStringLiteral("Revision")
    ));
    if (enabled.metaType().id() != QMetaType::Bool
        || width.metaType().id() != QMetaType::UInt
        || radius.metaType().id() != QMetaType::UInt
        || sync.metaType().id() != QMetaType::Bool
        || innerSpacing.metaType().id() != QMetaType::UInt
        || outerSpacing.metaType().id() != QMetaType::UInt
        || syncSpacing.metaType().id() != QMetaType::Bool
        || revision.metaType().id() != QMetaType::ULongLong
        || width.toUInt() > ConfigValues::maximumShellBorderWidth
        || radius.toUInt() > ConfigValues::maximumShellBorderRadius
        || innerSpacing.toUInt() > ConfigValues::maximumShellSpacing
        || outerSpacing.toUInt() > ConfigValues::maximumShellSpacing) {
        return std::nullopt;
    }
    return SharedVisualProjection{
        .borderEnabled = enabled.toBool(),
        .borderWidth = width.toUInt(),
        .borderRadius = radius.toUInt(),
        .syncWindowBorders = sync.toBool(),
        .innerSpacing = innerSpacing.toUInt(),
        .outerSpacing = outerSpacing.toUInt(),
        .syncWindowSpacing = syncSpacing.toBool(),
        .revision = revision.toULongLong(),
    };
}

const QStringList borderProperties{
    QStringLiteral("ShellBorderEnabled"),
    QStringLiteral("ShellBorderWidth"),
    QStringLiteral("ShellBorderRadius"),
    QStringLiteral("SyncHyprlandWindowBorders"),
};

const QStringList spacingProperties{
    QStringLiteral("ShellInnerSpacing"),
    QStringLiteral("ShellOuterSpacing"),
    QStringLiteral("SyncHyprlandWindowSpacing"),
};

bool includesSharedVisualProperty(
    const QVariantMap &changed,
    const QStringList &invalidated
)
{
    auto names = borderProperties;
    names.append(spacingProperties);
    names.append(QStringLiteral("Revision"));
    for (const auto &name : names) {
        if (changed.contains(name) || invalidated.contains(name)) {
            return true;
        }
    }
    return false;
}

bool invalidatesSharedVisualProperty(const QStringList &invalidated)
{
    auto names = borderProperties;
    names.append(spacingProperties);
    names.append(QStringLiteral("Revision"));
    for (const auto &name : names) {
        if (invalidated.contains(name)) {
            return true;
        }
    }
    return false;
}

qsizetype suppliedValues(
    const QVariantMap &changed,
    const QStringList &names
)
{
    qsizetype supplied = 0;
    for (const auto &name : names) {
        if (changed.contains(name)) {
            ++supplied;
        }
    }
    return supplied;
}

QVariantMap propertiesFor(const SharedVisualProjection &projection)
{
    return {
        {QStringLiteral("ShellBorderEnabled"), projection.borderEnabled},
        {QStringLiteral("ShellBorderWidth"), projection.borderWidth},
        {QStringLiteral("ShellBorderRadius"), projection.borderRadius},
        {
            QStringLiteral("SyncHyprlandWindowBorders"),
            projection.syncWindowBorders,
        },
        {QStringLiteral("ShellInnerSpacing"), projection.innerSpacing},
        {QStringLiteral("ShellOuterSpacing"), projection.outerSpacing},
        {
            QStringLiteral("SyncHyprlandWindowSpacing"),
            projection.syncWindowSpacing,
        },
        {
            QStringLiteral("Revision"),
            QVariant::fromValue<qulonglong>(projection.revision),
        },
    };
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

SharedVisualSource::SharedVisualSource(QObject *parent)
    : QObject(parent)
{
}

bool SharedVisualSource::available() const
{
    return available_;
}

const SharedVisualProjection &SharedVisualSource::projection() const
{
    return projection_;
}

QString SharedVisualSource::error() const
{
    return error_;
}

void SharedVisualSource::publishProjection(
    const SharedVisualProjection &projection
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

void SharedVisualSource::publishUnavailable(const QString &error)
{
    const auto nextError = boundedError(error);
    const auto changed = available_ || nextError != error_;
    available_ = false;
    error_ = nextError;
    if (changed) {
        emit this->changed();
    }
}

DbusSharedVisualSource::DbusSharedVisualSource(
    QDBusConnection connection,
    QObject *parent
)
    : SharedVisualSource(parent)
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
        &DbusSharedVisualSource::serviceOwnerChanged
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

void DbusSharedVisualSource::start()
{
    if (started_) {
        return;
    }
    started_ = true;
    refresh();
}

void DbusSharedVisualSource::requestRefresh()
{
    if (started_) {
        refresh();
    }
}

void DbusSharedVisualSource::serviceOwnerChanged(
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

void DbusSharedVisualSource::propertiesChanged(
    const QString &changedInterface,
    const QVariantMap &changed,
    const QStringList &invalidated
)
{
    if (!started_ || changedInterface != interfaceName
        || !includesSharedVisualProperty(changed, invalidated)) {
        return;
    }

    const auto markStaleAndRefresh = [this] {
        publishUnavailable(QStringLiteral(
            "Shared visual settings changed with an invalid or incomplete projection"
        ));
        refresh();
    };
    if (invalidatesSharedVisualProperty(invalidated)) {
        markStaleAndRefresh();
        return;
    }

    const auto suppliedBorder = suppliedValues(changed, borderProperties);
    const auto suppliedSpacing = suppliedValues(changed, spacingProperties);
    const auto suppliesRevision = changed.contains(QStringLiteral("Revision"));
    const auto completeBorder = suppliedBorder == borderProperties.size();
    const auto completeSpacing = suppliedSpacing == spacingProperties.size();
    const auto partialBorder = suppliedBorder > 0 && !completeBorder;
    const auto partialSpacing = suppliedSpacing > 0 && !completeSpacing;
    if (partialBorder || partialSpacing
        || ((completeBorder || completeSpacing) && !suppliesRevision)) {
        markStaleAndRefresh();
        return;
    }

    if ((completeBorder || completeSpacing) && suppliesRevision) {
        if (!available() && !(completeBorder && completeSpacing)) {
            markStaleAndRefresh();
            return;
        }
        auto merged = available() ? propertiesFor(projection()) : QVariantMap{};
        for (auto iterator = changed.constBegin(); iterator != changed.constEnd();
             ++iterator) {
            merged.insert(iterator.key(), iterator.value());
        }
        const auto next = decode(merged);
        if (!next || (projectionEstablished_
                && (next->revision < projection().revision
                    || (next->revision == projection().revision
                        && *next != projection())))) {
            markStaleAndRefresh();
            return;
        }
        ++generation_;
        projectionEstablished_ = true;
        publishProjection(*next);
        return;
    }

    if (suppliedBorder == 0 && suppliedSpacing == 0
        && suppliesRevision && available()) {
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

void DbusSharedVisualSource::refresh()
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
            if (projectionEstablished_
                && (projection->revision < this->projection().revision
                    || (projection->revision == this->projection().revision
                        && *projection != this->projection()))) {
                publishUnavailable(QStringLiteral(
                    "Shared visual settings returned a stale projection"
                ));
                return;
            }
            projectionEstablished_ = true;
            publishProjection(*projection);
        }
    );
}

} // namespace HyprShelld::Compositor
