#include "component_plan_hydrator.h"

#include "component/component_configuration.h"
#include "component/component_contract.h"
#include "component_plan_controller.h"

#include <QDBusMessage>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDBusServiceWatcher>
#include <QMetaType>
#include <QTimer>

#include <algorithm>
#include <utility>

namespace HyprShelld {
namespace {

const QString managerService = QStringLiteral(
    "org.hyprshelld.ComponentManager1"
);
const QString managerPath = QStringLiteral(
    "/org/hyprshelld/ComponentManager1"
);
const QString managerInterface = QStringLiteral(
    "org.hyprshelld.ComponentManager1"
);
const QString configService = QStringLiteral("org.hyprshelld.Config1");
const QString configPath = QStringLiteral(
    "/org/hyprshelld/Config1/Components"
);
const QString configInterface = QStringLiteral(
    "org.hyprshelld.ComponentConfig1"
);
const QString propertiesInterface = QStringLiteral(
    "org.freedesktop.DBus.Properties"
);
constexpr int callTimeoutMs = 3000;
constexpr int maximumRetryDelayMs = 5000;

QString describeErrors(const Components::ValidationErrors &errors)
{
    QStringList details;
    details.reserve(errors.size());
    for (const auto &error : errors) {
        details.append(
            QStringLiteral("%1 [%2]: %3")
                .arg(error.path, error.code, error.message)
        );
    }
    return details.join(QStringLiteral("; "));
}

bool exactType(const QVariant &value, const QMetaType type)
{
    return value.metaType() == type;
}

bool decodeCatalogRecord(
    const QString &componentId,
    const QDBusMessage &reply,
    Components::RuntimeCatalogComponentRecord &record
)
{
    const auto arguments = reply.arguments();
    if (reply.type() == QDBusMessage::ErrorMessage
        || arguments.size() != 25) {
        return false;
    }

    const auto stringType = QMetaType::fromType<QString>();
    const auto stringListType = QMetaType::fromType<QStringList>();
    if (!exactType(arguments.at(0), QMetaType::fromType<uint>())
        || !exactType(arguments.at(1), stringType)
        || !exactType(arguments.at(2), stringType)
        || !exactType(arguments.at(3), stringType)
        || !exactType(arguments.at(4), stringType)
        || !exactType(arguments.at(5), stringListType)
        || !exactType(arguments.at(6), stringListType)
        || !exactType(arguments.at(7), stringListType)
        || !exactType(arguments.at(8), stringType)
        || !exactType(arguments.at(9), stringType)
        || !exactType(arguments.at(10), stringType)
        || !exactType(arguments.at(11), stringType)
        || !exactType(arguments.at(12), stringType)
        || !exactType(arguments.at(13), stringType)
        || !exactType(arguments.at(14), stringType)
        || !exactType(arguments.at(15), stringType)
        || !exactType(arguments.at(16), stringListType)
        || !exactType(arguments.at(17), QMetaType::fromType<QByteArray>())
        || !exactType(arguments.at(18), stringListType)
        || !exactType(arguments.at(19), stringListType)
        || !exactType(arguments.at(20), stringListType)
        || !exactType(arguments.at(21), stringListType)
        || !exactType(arguments.at(22), stringType)
        || !exactType(arguments.at(23), stringType)
        || !exactType(arguments.at(24), QMetaType::fromType<bool>())) {
        return false;
    }

    record = {
        .componentId = componentId,
        .manifestVersion = arguments.at(0).toUInt(),
        .componentType = arguments.at(1).toString(),
        .version = arguments.at(2).toString(),
        .name = arguments.at(3).toString(),
        .description = arguments.at(4).toString(),
        .authorNames = arguments.at(5).toStringList(),
        .authorEmails = arguments.at(6).toStringList(),
        .authorHomepages = arguments.at(7).toStringList(),
        .license = arguments.at(8).toString(),
        .homepage = arguments.at(9).toString(),
        .source = arguments.at(10).toString(),
        .issues = arguments.at(11).toString(),
        .componentApiVersion = arguments.at(12).toString(),
        .runtimeKind = arguments.at(13).toString(),
        .runtimeFactory = arguments.at(14).toString(),
        .runtimeEntryPoint = arguments.at(15).toString(),
        .runtimeArguments = arguments.at(16).toStringList(),
        .settingsSchema = arguments.at(17).toByteArray(),
        .capabilityIds = arguments.at(18).toStringList(),
        .capabilityReasons = arguments.at(19).toStringList(),
        .dependencyIds = arguments.at(20).toStringList(),
        .dependencyVersionRequirements = arguments.at(21).toStringList(),
        .packageDigest = arguments.at(22).toString(),
        .origin = arguments.at(23).toString(),
        .removable = arguments.at(24).toBool(),
    };
    return true;
}

bool touchesAny(
    const QVariantMap &changed,
    const QStringList &invalidated,
    const QStringList &names
)
{
    for (const auto &name : names) {
        if (changed.contains(name) || invalidated.contains(name)) {
            return true;
        }
    }
    return false;
}

} // namespace

ComponentPlanHydrator::ComponentPlanHydrator(
    ComponentPlanController *controller,
    QDBusConnection connection,
    QObject *parent
)
    : QObject(parent)
    , controller_(controller)
    , connection_(std::move(connection))
{
    Q_ASSERT(controller_ != nullptr);

    managerWatcher_ = new QDBusServiceWatcher(
        managerService,
        connection_,
        QDBusServiceWatcher::WatchForOwnerChange,
        this
    );
    configWatcher_ = new QDBusServiceWatcher(
        configService,
        connection_,
        QDBusServiceWatcher::WatchForOwnerChange,
        this
    );
    connect(
        managerWatcher_,
        &QDBusServiceWatcher::serviceOwnerChanged,
        this,
        &ComponentPlanHydrator::sourceOwnerChanged
    );
    connect(
        configWatcher_,
        &QDBusServiceWatcher::serviceOwnerChanged,
        this,
        &ComponentPlanHydrator::sourceOwnerChanged
    );

    connection_.connect(
        managerService,
        managerPath,
        propertiesInterface,
        QStringLiteral("PropertiesChanged"),
        this,
        SLOT(managerPropertiesChanged(QString,QVariantMap,QStringList))
    );
    connection_.connect(
        configService,
        configPath,
        propertiesInterface,
        QStringLiteral("PropertiesChanged"),
        this,
        SLOT(configPropertiesChanged(QString,QVariantMap,QStringList))
    );

    retryTimer_ = new QTimer(this);
    retryTimer_->setSingleShot(true);
    connect(retryTimer_, &QTimer::timeout, this, [this] { refresh(); });
}

void ComponentPlanHydrator::start()
{
    if (started_) {
        return;
    }
    started_ = true;
    refresh();
}

void ComponentPlanHydrator::sourceOwnerChanged(
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
    retryTimer_->stop();
    retryDelayMs_ = 250;
    controller_->sourceUnavailable(
        QStringLiteral("A component runtime authority changed ownership.")
    );
    if (!newOwner.isEmpty()) {
        refresh();
    } else {
        scheduleRetry();
    }
}

void ComponentPlanHydrator::managerPropertiesChanged(
    const QString &changedInterface,
    const QVariantMap &changed,
    const QStringList &invalidated
)
{
    if (started_ && changedInterface == managerInterface
        && touchesAny(
            changed,
            invalidated,
            {QStringLiteral("CatalogDigest")}
        )) {
        refresh();
    }
}

void ComponentPlanHydrator::configPropertiesChanged(
    const QString &changedInterface,
    const QVariantMap &changed,
    const QStringList &invalidated
)
{
    if (started_ && changedInterface == configInterface
        && touchesAny(
            changed,
            invalidated,
            {
                QStringLiteral("Available"),
                QStringLiteral("CatalogAvailable"),
                QStringLiteral("Revision"),
                QStringLiteral("CatalogDigest"),
                QStringLiteral("LoadState"),
            }
        )) {
        refresh();
    }
}

void ComponentPlanHydrator::refresh()
{
    if (!started_) {
        return;
    }
    retryTimer_->stop();
    controller_->beginHydration();
    const auto generation = ++generation_;

    auto message = QDBusMessage::createMethodCall(
        managerService,
        managerPath,
        managerInterface,
        QStringLiteral("ListComponents")
    );
    auto *watcher = new QDBusPendingCallWatcher(
        connection_.asyncCall(message, callTimeoutMs),
        this
    );
    connect(
        watcher,
        &QDBusPendingCallWatcher::finished,
        this,
        [this, watcher, generation] {
            const auto reply = watcher->reply();
            watcher->deleteLater();
            if (generation != generation_) {
                return;
            }
            const auto arguments = reply.arguments();
            if (reply.type() == QDBusMessage::ErrorMessage
                || arguments.size() != 2
                || !exactType(
                    arguments.at(0),
                    QMetaType::fromType<QStringList>()
                )
                || !exactType(
                    arguments.at(1),
                    QMetaType::fromType<QString>()
                )) {
                fail(
                    generation,
                    QStringLiteral("ComponentManager1.ListComponents failed or returned malformed data.")
                );
                return;
            }

            auto componentIds = arguments.at(0).toStringList();
            auto catalogDigest = arguments.at(1).toString();
            const auto listing = Components::validateRuntimeCatalogListing(
                componentIds,
                catalogDigest
            );
            if (!listing) {
                fail(generation, describeErrors(listing.errors));
                return;
            }

            fetchCatalogRecord(
                generation,
                std::move(componentIds),
                std::move(catalogDigest),
                0,
                {}
            );
        }
    );
}

void ComponentPlanHydrator::fetchCatalogRecord(
    const quint64 generation,
    QStringList componentIds,
    QString catalogDigest,
    const qsizetype index,
    QVector<Components::RuntimeCatalogComponentRecord> records
)
{
    if (generation != generation_) {
        return;
    }
    if (index == componentIds.size()) {
        auto hydrated = Components::hydrateRuntimeCatalog(
            componentIds,
            catalogDigest,
            records
        );
        if (!hydrated) {
            fail(generation, describeErrors(hydrated.errors));
            return;
        }
        fetchConfigurationProperties(
            generation,
            std::move(*hydrated.value)
        );
        return;
    }

    auto message = QDBusMessage::createMethodCall(
        managerService,
        managerPath,
        managerInterface,
        QStringLiteral("GetComponent")
    );
    message.setArguments({componentIds.at(index), catalogDigest});
    auto *watcher = new QDBusPendingCallWatcher(
        connection_.asyncCall(message, callTimeoutMs),
        this
    );
    connect(
        watcher,
        &QDBusPendingCallWatcher::finished,
        this,
        [
            this,
            watcher,
            generation,
            componentIds = std::move(componentIds),
            catalogDigest = std::move(catalogDigest),
            index,
            records = std::move(records)
        ]() mutable {
            const auto reply = watcher->reply();
            watcher->deleteLater();
            if (generation != generation_) {
                return;
            }

            Components::RuntimeCatalogComponentRecord record;
            if (!decodeCatalogRecord(
                    componentIds.at(index),
                    reply,
                    record
                )) {
                fail(
                    generation,
                    QStringLiteral("ComponentManager1.GetComponent returned malformed data.")
                );
                return;
            }
            const auto validated =
                Components::validateRuntimeCatalogRecord(record);
            if (!validated) {
                fail(generation, describeErrors(validated.errors));
                return;
            }
            records.append(std::move(record));
            fetchCatalogRecord(
                generation,
                std::move(componentIds),
                std::move(catalogDigest),
                index + 1,
                std::move(records)
            );
        }
    );
}

void ComponentPlanHydrator::fetchConfigurationProperties(
    const quint64 generation,
    Components::HydratedRuntimeCatalog catalog
)
{
    if (generation != generation_) {
        return;
    }

    auto message = QDBusMessage::createMethodCall(
        configService,
        configPath,
        propertiesInterface,
        QStringLiteral("GetAll")
    );
    message.setArguments({configInterface});
    auto *watcher = new QDBusPendingCallWatcher(
        connection_.asyncCall(message, callTimeoutMs),
        this
    );
    connect(
        watcher,
        &QDBusPendingCallWatcher::finished,
        this,
        [this, watcher, generation, catalog = std::move(catalog)]() mutable {
            const QDBusPendingReply<QVariantMap> reply = *watcher;
            watcher->deleteLater();
            if (generation != generation_) {
                return;
            }
            if (reply.isError()) {
                fail(generation, reply.error().message());
                return;
            }

            const auto properties = reply.value();
            const auto available = properties.constFind(
                QStringLiteral("Available")
            );
            const auto catalogAvailable = properties.constFind(
                QStringLiteral("CatalogAvailable")
            );
            const auto revision = properties.constFind(
                QStringLiteral("Revision")
            );
            const auto digest = properties.constFind(
                QStringLiteral("CatalogDigest")
            );
            const auto loadState = properties.constFind(
                QStringLiteral("LoadState")
            );
            if (available == properties.cend()
                || catalogAvailable == properties.cend()
                || revision == properties.cend()
                || digest == properties.cend()
                || loadState == properties.cend()
                || !exactType(*available, QMetaType::fromType<bool>())
                || !exactType(
                    *catalogAvailable,
                    QMetaType::fromType<bool>()
                )
                || !exactType(
                    *revision,
                    QMetaType::fromType<qulonglong>()
                )
                || !exactType(*digest, QMetaType::fromType<QString>())
                || !exactType(*loadState, QMetaType::fromType<QString>())
                || !available->toBool()
                || !catalogAvailable->toBool()
                || !Components::isFullSha256Digest(digest->toString())
                || digest->toString()
                    != catalog.configurationCatalog.digest) {
                fail(
                    generation,
                    QStringLiteral("ComponentConfig1 is unavailable or published malformed authority metadata.")
                );
                return;
            }

            fetchConfigurationSnapshot(
                generation,
                std::move(catalog),
                revision->toULongLong(),
                digest->toString()
            );
        }
    );
}

void ComponentPlanHydrator::fetchConfigurationSnapshot(
    const quint64 generation,
    Components::HydratedRuntimeCatalog catalog,
    const quint64 expectedRevision,
    QString expectedCatalogDigest
)
{
    if (generation != generation_) {
        return;
    }

    auto message = QDBusMessage::createMethodCall(
        configService,
        configPath,
        configInterface,
        QStringLiteral("GetSnapshot")
    );
    auto *watcher = new QDBusPendingCallWatcher(
        connection_.asyncCall(message, callTimeoutMs),
        this
    );
    connect(
        watcher,
        &QDBusPendingCallWatcher::finished,
        this,
        [
            this,
            watcher,
            generation,
            catalog = std::move(catalog),
            expectedRevision,
            expectedCatalogDigest = std::move(expectedCatalogDigest)
        ]() mutable {
            const QDBusPendingReply<QByteArray, qulonglong, QString> reply =
                *watcher;
            watcher->deleteLater();
            if (generation != generation_) {
                return;
            }
            if (reply.isError()
                || reply.argumentAt<1>() != expectedRevision
                || reply.argumentAt<2>() != expectedCatalogDigest) {
                fail(
                    generation,
                    reply.isError()
                        ? reply.error().message()
                        : QStringLiteral("ComponentConfig1 changed during hydration.")
                );
                return;
            }

            auto configuration = Components::hydrateRuntimeConfiguration(
                QByteArrayView(reply.argumentAt<0>()),
                reply.argumentAt<1>(),
                reply.argumentAt<2>(),
                catalog.configurationCatalog
            );
            if (!configuration) {
                fail(generation, describeErrors(configuration.errors));
                return;
            }
            if (!controller_->acceptSnapshots(
                    catalog.runtimeCatalog,
                    *configuration.value
                )) {
                scheduleRetry();
                return;
            }
            retryDelayMs_ = 250;
        }
    );
}

void ComponentPlanHydrator::fail(
    const quint64 generation,
    const QString &error
)
{
    if (generation != generation_) {
        return;
    }
    controller_->hydrationFailed(
        error.isEmpty()
            ? QStringLiteral("Component plan hydration failed.")
            : error
    );
    scheduleRetry();
}

void ComponentPlanHydrator::scheduleRetry()
{
    if (!started_ || retryTimer_->isActive()) {
        return;
    }
    retryTimer_->start(retryDelayMs_);
    retryDelayMs_ = std::min(
        retryDelayMs_ * 2,
        maximumRetryDelayMs
    );
}

} // namespace HyprShelld
