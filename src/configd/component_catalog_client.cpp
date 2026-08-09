#include "component_catalog_client.h"

#include "component/component_contract.h"
#include "component/settings_schema.h"

#include <QCryptographicHash>
#include <QDBusMessage>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDBusServiceWatcher>
#include <QMetaType>
#include <QTimer>
#include <QtEndian>

#include <array>
#include <algorithm>
#include <initializer_list>
#include <utility>

namespace HyprShelld {
namespace {

const QString serviceName = QStringLiteral("org.hyprshelld.ComponentManager1");
const QString objectPath = QStringLiteral("/org/hyprshelld/ComponentManager1");
const QString interfaceName = QStringLiteral("org.hyprshelld.ComponentManager1");
const QString propertiesInterface = QStringLiteral(
    "org.freedesktop.DBus.Properties"
);
constexpr int callTimeoutMs = 3000;
constexpr int maximumRetryDelayMs = 5000;

bool hasExactMetaTypes(
    const QVariantList &arguments,
    const std::initializer_list<int> expected
)
{
    if (arguments.size() != static_cast<qsizetype>(expected.size())) {
        return false;
    }
    qsizetype index = 0;
    for (const auto type : expected) {
        if (arguments.at(index++).metaType().id() != type) {
            return false;
        }
    }
    return true;
}

void addDigestFile(
    QCryptographicHash &hash,
    const QByteArray &name,
    const QByteArray &value
)
{
    std::array<uchar, sizeof(quint64)> length{};
    qToBigEndian<quint64>(static_cast<quint64>(name.size()), length.data());
    hash.addData(QByteArrayView(
        reinterpret_cast<const char *>(length.data()), length.size()
    ));
    hash.addData(name);
    qToBigEndian<quint64>(static_cast<quint64>(value.size()), length.data());
    hash.addData(QByteArrayView(
        reinterpret_cast<const char *>(length.data()), length.size()
    ));
    hash.addData(value);
}

QString deriveCatalogDigest(const Components::ConfigurationCatalog &catalog)
{
    QCryptographicHash hash(QCryptographicHash::Sha256);
    for (auto entry = catalog.entries.constBegin();
         entry != catalog.entries.constEnd(); ++entry) {
        addDigestFile(hash, entry.key().toUtf8(), entry->packageDigest.toLatin1());
    }
    return QString::fromLatin1(hash.result().toHex());
}

} // namespace

ComponentCatalogClient::ComponentCatalogClient(
    QDBusConnection connection,
    QObject *parent
)
    : QObject(parent)
    , connection_(std::move(connection))
{
    retryTimer_ = new QTimer(this);
    retryTimer_->setSingleShot(true);
    connect(retryTimer_, &QTimer::timeout, this, [this] { refresh(); });

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
        &ComponentCatalogClient::serviceOwnerChanged
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

bool ComponentCatalogClient::available() const
{
    return available_;
}

const Components::ConfigurationCatalog &ComponentCatalogClient::catalog() const
{
    return catalog_;
}

void ComponentCatalogClient::start()
{
    refresh();
}

void ComponentCatalogClient::serviceOwnerChanged(
    const QString &name,
    const QString &oldOwner,
    const QString &newOwner
)
{
    Q_UNUSED(name)
    Q_UNUSED(oldOwner)
    ++generation_;
    retryTimer_->stop();
    retryDelayMs_ = 250;
    if (available_) {
        available_ = false;
        emit catalogUnavailable();
    }
    if (!newOwner.isEmpty()) {
        refresh();
    }
}

void ComponentCatalogClient::propertiesChanged(
    const QString &changedInterface,
    const QVariantMap &changed,
    const QStringList &invalidated
)
{
    if (changedInterface == interfaceName
        && (changed.contains(QStringLiteral("CatalogDigest"))
            || invalidated.contains(QStringLiteral("CatalogDigest")))) {
        if (available_) {
            available_ = false;
            emit catalogUnavailable();
        }
        refresh();
    }
}

void ComponentCatalogClient::refresh()
{
    retryTimer_->stop();
    const auto generation = ++generation_;
    auto message = QDBusMessage::createMethodCall(
        serviceName,
        objectPath,
        interfaceName,
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
                || !hasExactMetaTypes(
                    arguments,
                    {QMetaType::QStringList, QMetaType::QString}
                )) {
                fail(generation);
                return;
            }
            const auto componentIds = arguments.at(0).toStringList();
            const auto digest = arguments.at(1).toString();
            auto sorted = componentIds;
            sorted.sort();
            sorted.removeDuplicates();
            if (componentIds.isEmpty() || componentIds.size() > 512
                || componentIds != sorted
                || !Components::isFullSha256Digest(digest)) {
                fail(generation);
                return;
            }
            for (const auto &componentId : componentIds) {
                if (!Components::isValidComponentId(componentId)) {
                    fail(generation);
                    return;
                }
            }
            Components::ConfigurationCatalog catalog;
            catalog.digest = digest;
            fetchNext(
                generation,
                componentIds,
                digest,
                0,
                std::move(catalog)
            );
        }
    );
}

void ComponentCatalogClient::fetchNext(
    const quint64 generation,
    QStringList componentIds,
    QString catalogDigest,
    const qsizetype index,
    Components::ConfigurationCatalog catalog
)
{
    if (generation != generation_) {
        return;
    }
    if (index == componentIds.size()) {
        if (deriveCatalogDigest(catalog) != catalogDigest) {
            fail(generation);
            return;
        }
        catalog_ = std::move(catalog);
        available_ = true;
        retryDelayMs_ = 250;
        emit catalogChanged();
        return;
    }

    auto message = QDBusMessage::createMethodCall(
        serviceName,
        objectPath,
        interfaceName,
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
        [this, watcher, generation, componentIds = std::move(componentIds),
         catalogDigest = std::move(catalogDigest), index,
         catalog = std::move(catalog)]() mutable {
            const auto reply = watcher->reply();
            watcher->deleteLater();
            if (generation != generation_) {
                return;
            }
            const auto arguments = reply.arguments();
            if (reply.type() == QDBusMessage::ErrorMessage
                || !hasExactMetaTypes(
                    arguments,
                    {
                        QMetaType::UInt,
                        QMetaType::QString,
                        QMetaType::QString,
                        QMetaType::QString,
                        QMetaType::QString,
                        QMetaType::QStringList,
                        QMetaType::QStringList,
                        QMetaType::QStringList,
                        QMetaType::QString,
                        QMetaType::QString,
                        QMetaType::QString,
                        QMetaType::QString,
                        QMetaType::QString,
                        QMetaType::QString,
                        QMetaType::QString,
                        QMetaType::QString,
                        QMetaType::QStringList,
                        QMetaType::QByteArray,
                        QMetaType::QStringList,
                        QMetaType::QStringList,
                        QMetaType::QStringList,
                        QMetaType::QStringList,
                        QMetaType::QString,
                        QMetaType::QString,
                        QMetaType::Bool,
                    }
                )
                || arguments.at(0).toUInt() != 1) {
                fail(generation);
                return;
            }

            const auto type = Components::componentTypeFromString(
                arguments.at(1).toString()
            );
            const auto settingsBytes = arguments.at(17).toByteArray();
            const auto capabilityIds = arguments.at(18).toStringList();
            const auto capabilityReasons = arguments.at(19).toStringList();
            const auto packageDigest = arguments.at(22).toString();
            const auto originText = arguments.at(23).toString();
            auto sortedCapabilities = capabilityIds;
            sortedCapabilities.sort();
            sortedCapabilities.removeDuplicates();
            if (!type.has_value()
                || capabilityIds.size() != capabilityReasons.size()
                || capabilityIds != sortedCapabilities
                || !Components::isFullSha256Digest(packageDigest)
                || (originText != QStringLiteral("system")
                    && originText != QStringLiteral("user"))) {
                fail(generation);
                return;
            }

            Components::SettingsSchema schema;
            if (!settingsBytes.isEmpty()) {
                auto parsedSchema = Components::parseSettingsSchema(
                    QByteArrayView(settingsBytes)
                );
                if (!parsedSchema) {
                    fail(generation);
                    return;
                }
                schema = std::move(*parsedSchema.value);
            }
            QSet<QString> requested;
            for (const auto &capabilityId : capabilityIds) {
                if (!Components::isValidCapabilityId(capabilityId)
                    || requested.contains(capabilityId)) {
                    fail(generation);
                    return;
                }
                requested.insert(capabilityId);
            }
            catalog.entries.insert(
                componentIds.at(index),
                {
                    .packageDigest = packageDigest,
                    .type = *type,
                    .origin = originText == QStringLiteral("system")
                        ? Components::ComponentOrigin::System
                        : Components::ComponentOrigin::User,
                    .settingsSchema = std::move(schema),
                    .requestedCapabilities = std::move(requested),
                }
            );
            fetchNext(
                generation,
                std::move(componentIds),
                std::move(catalogDigest),
                index + 1,
                std::move(catalog)
            );
        }
    );
}

void ComponentCatalogClient::fail(const quint64 generation)
{
    if (generation != generation_) {
        return;
    }
    if (available_) {
        available_ = false;
        emit catalogUnavailable();
    }
    if (!retryTimer_->isActive()) {
        retryTimer_->start(retryDelayMs_);
        retryDelayMs_ = std::min(retryDelayMs_ * 2, maximumRetryDelayMs);
    }
}

} // namespace HyprShelld
