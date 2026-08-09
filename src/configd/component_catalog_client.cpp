#include "component_catalog_client.h"

#include "component/component_contract.h"
#include "component/declarative_document.h"
#include "component/settings_schema.h"

#include <QCryptographicHash>
#include <QDBusMessage>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDBusServiceWatcher>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
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

bool boundedStrings(
    const QStringList &values,
    const qsizetype maximumCount,
    const qsizetype maximumLength
)
{
    return values.size() <= maximumCount
        && std::ranges::all_of(values, [maximumLength](const QString &value) {
               return value.size() <= maximumLength;
           });
}

bool rawFieldsAreBounded(const QVariantList &arguments)
{
    return arguments.at(1).toString().size() <= 32
        && arguments.at(2).toString().size() <= 64
        && arguments.at(3).toString().size() <= 128
        && arguments.at(4).toString().size() <= 4096
        && boundedStrings(arguments.at(5).toStringList(), 16, 128)
        && boundedStrings(arguments.at(6).toStringList(), 16, 254)
        && boundedStrings(arguments.at(7).toStringList(), 16, 2048)
        && arguments.at(8).toString().size() <= 128
        && arguments.at(9).toString().size() <= 2048
        && arguments.at(10).toString().size() <= 2048
        && arguments.at(11).toString().size() <= 2048
        && arguments.at(12).toString().size() <= 32
        && arguments.at(13).toString().size() <= 32
        && arguments.at(14).toString().size() <= 64
        && arguments.at(15).toString().size() <= 255
        && boundedStrings(arguments.at(16).toStringList(), 32, 1024)
        && arguments.at(17).toByteArray().size() <= 256 * 1024
        && boundedStrings(arguments.at(18).toStringList(), 64, 255)
        && boundedStrings(arguments.at(19).toStringList(), 64, 1024)
        && boundedStrings(arguments.at(20).toStringList(), 64, 255)
        && boundedStrings(arguments.at(21).toStringList(), 64, 256)
        && arguments.at(22).toString().size() <= 64
        && arguments.at(23).toString().size() <= 6;
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

QJsonArray stringArray(const QStringList &values)
{
    QJsonArray array;
    for (const auto &value : values) {
        array.append(value);
    }
    return array;
}

std::optional<Components::ConfigurationCatalogEntry> decodeCatalogEntry(
    const QString &componentId,
    const QVariantList &arguments
)
{
    if (!hasExactMetaTypes(
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
        || arguments.at(0).toUInt() != 1
        || !rawFieldsAreBounded(arguments)) {
        return std::nullopt;
    }

    const auto authorNames = arguments.at(5).toStringList();
    const auto authorEmails = arguments.at(6).toStringList();
    const auto authorHomepages = arguments.at(7).toStringList();
    const auto capabilityIds = arguments.at(18).toStringList();
    const auto capabilityReasons = arguments.at(19).toStringList();
    const auto dependencyIds = arguments.at(20).toStringList();
    const auto dependencyVersions = arguments.at(21).toStringList();
    if (authorNames.size() != authorEmails.size()
        || authorNames.size() != authorHomepages.size()
        || capabilityIds.size() != capabilityReasons.size()
        || dependencyIds.size() != dependencyVersions.size()) {
        return std::nullopt;
    }
    auto sortedCapabilities = capabilityIds;
    sortedCapabilities.sort();
    sortedCapabilities.removeDuplicates();
    if (capabilityIds != sortedCapabilities) {
        return std::nullopt;
    }

    const auto originText = arguments.at(23).toString();
    const auto origin = originText == QStringLiteral("system")
        ? std::optional(Components::ComponentOrigin::System)
        : originText == QStringLiteral("user")
            ? std::optional(Components::ComponentOrigin::User)
            : std::nullopt;
    if (!origin.has_value()
        || arguments.at(24).toBool()
            != (*origin == Components::ComponentOrigin::User)) {
        return std::nullopt;
    }

    const auto settingsBytes = arguments.at(17).toByteArray();
    Components::SettingsSchema settingsSchema;
    if (!settingsBytes.isEmpty()) {
        auto parsed = Components::parseSettingsSchema(
            QByteArrayView(settingsBytes)
        );
        if (!parsed) {
            return std::nullopt;
        }
        settingsSchema = std::move(*parsed.value);
    }

    QJsonArray authors;
    for (qsizetype index = 0; index < authorNames.size(); ++index) {
        QJsonObject author{{QStringLiteral("name"), authorNames.at(index)}};
        if (!authorEmails.at(index).isEmpty()) {
            author.insert(QStringLiteral("email"), authorEmails.at(index));
        }
        if (!authorHomepages.at(index).isEmpty()) {
            author.insert(
                QStringLiteral("homepage"), authorHomepages.at(index)
            );
        }
        authors.append(author);
    }

    QJsonArray capabilities;
    for (qsizetype index = 0; index < capabilityIds.size(); ++index) {
        capabilities.append(QJsonObject{
            {QStringLiteral("id"), capabilityIds.at(index)},
            {QStringLiteral("reason"), capabilityReasons.at(index)},
        });
    }
    QJsonArray dependencies;
    for (qsizetype index = 0; index < dependencyIds.size(); ++index) {
        dependencies.append(QJsonObject{
            {QStringLiteral("id"), dependencyIds.at(index)},
            {QStringLiteral("version"), dependencyVersions.at(index)},
        });
    }

    const auto runtimeKind = Components::runtimeKindFromString(
        arguments.at(13).toString()
    );
    if (!runtimeKind.has_value()) {
        return std::nullopt;
    }
    QJsonObject runtime{
        {QStringLiteral("kind"), arguments.at(13).toString()},
    };
    switch (*runtimeKind) {
    case Components::RuntimeKind::BuiltinV1:
        if (!arguments.at(15).toString().isEmpty()
            || !arguments.at(16).toStringList().isEmpty()) {
            return std::nullopt;
        }
        runtime.insert(QStringLiteral("factory"), arguments.at(14).toString());
        break;
    case Components::RuntimeKind::DeclarativeV1:
    case Components::RuntimeKind::QmlFullTrustV1:
        if (!arguments.at(14).toString().isEmpty()
            || !arguments.at(16).toStringList().isEmpty()) {
            return std::nullopt;
        }
        runtime.insert(
            QStringLiteral("entrypoint"), arguments.at(15).toString()
        );
        break;
    case Components::RuntimeKind::ProcessV1:
        if (!arguments.at(14).toString().isEmpty()) {
            return std::nullopt;
        }
        runtime.insert(
            QStringLiteral("entrypoint"), arguments.at(15).toString()
        );
        runtime.insert(
            QStringLiteral("arguments"),
            stringArray(arguments.at(16).toStringList())
        );
        break;
    }

    QJsonObject manifest{
        {QStringLiteral("manifestVersion"), 1},
        {QStringLiteral("id"), componentId},
        {QStringLiteral("version"), arguments.at(2).toString()},
        {QStringLiteral("type"), arguments.at(1).toString()},
        {QStringLiteral("name"), arguments.at(3).toString()},
        {QStringLiteral("description"), arguments.at(4).toString()},
        {QStringLiteral("authors"), authors},
        {QStringLiteral("license"), arguments.at(8).toString()},
        {QStringLiteral("componentApiVersion"), arguments.at(12).toString()},
        {QStringLiteral("runtime"), runtime},
        {QStringLiteral("requestedCapabilities"), capabilities},
        {QStringLiteral("dependencies"), dependencies},
    };
    for (const auto &[name, value] : {
             std::pair{QStringLiteral("homepage"), arguments.at(9).toString()},
             std::pair{QStringLiteral("source"), arguments.at(10).toString()},
             std::pair{QStringLiteral("issues"), arguments.at(11).toString()},
         }) {
        if (!value.isEmpty()) {
            manifest.insert(name, value);
        }
    }
    if (!settingsBytes.isEmpty()) {
        manifest.insert(
            QStringLiteral("settingsSchema"),
            QStringLiteral("settings.schema.json")
        );
    }

    const auto parsedManifest = Components::parseComponentManifest(
        QByteArrayView(QJsonDocument(manifest).toJson(QJsonDocument::Compact)),
        *origin
    );
    const auto packageDigest = arguments.at(22).toString();
    if (!parsedManifest || !Components::isFullSha256Digest(packageDigest)) {
        return std::nullopt;
    }

    QSet<QString> requestedCapabilities;
    for (const auto &request : parsedManifest.value->requestedCapabilities) {
        requestedCapabilities.insert(request.id);
    }
    QStringList parsedDependencyIds;
    parsedDependencyIds.reserve(parsedManifest.value->dependencies.size());
    for (const auto &dependency : parsedManifest.value->dependencies) {
        parsedDependencyIds.append(dependency.id);
    }

    return Components::ConfigurationCatalogEntry{
        .packageDigest = packageDigest,
        .type = parsedManifest.value->type,
        .origin = parsedManifest.value->origin,
        .settingsSchema = std::move(settingsSchema),
        .requestedCapabilities = std::move(requestedCapabilities),
        .componentApiVersion = parsedManifest.value->componentApiVersion,
        .runtimeKind = parsedManifest.value->runtime.kind,
        .dependencyIds = std::move(parsedDependencyIds),
        .activationSupported = Components::validateCurrentHostSupport(
            *parsedManifest.value
        ).isEmpty(),
    };
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
            if (reply.type() == QDBusMessage::ErrorMessage) {
                fail(generation);
                return;
            }
            auto entry = decodeCatalogEntry(
                componentIds.at(index), arguments
            );
            if (!entry.has_value()) {
                fail(generation);
                return;
            }
            if (entry->activationSupported
                && entry->origin == Components::ComponentOrigin::User
                && entry->runtimeKind
                    == Components::RuntimeKind::DeclarativeV1) {
                fetchDeclarativeRuntime(
                    generation,
                    std::move(componentIds),
                    std::move(catalogDigest),
                    index,
                    std::move(catalog),
                    std::move(*entry)
                );
                return;
            }
            catalog.entries.insert(
                componentIds.at(index), std::move(*entry)
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

void ComponentCatalogClient::fetchDeclarativeRuntime(
    const quint64 generation,
    QStringList componentIds,
    QString catalogDigest,
    const qsizetype index,
    Components::ConfigurationCatalog catalog,
    Components::ConfigurationCatalogEntry entry
)
{
    if (generation != generation_ || index >= componentIds.size()) {
        return;
    }
    auto message = QDBusMessage::createMethodCall(
        serviceName,
        objectPath,
        interfaceName,
        QStringLiteral("GetDeclarativeRuntime")
    );
    message.setArguments({
        componentIds.at(index), entry.packageDigest, catalogDigest,
    });
    auto *watcher = new QDBusPendingCallWatcher(
        connection_.asyncCall(message, callTimeoutMs), this
    );
    connect(
        watcher,
        &QDBusPendingCallWatcher::finished,
        this,
        [this, watcher, generation, componentIds = std::move(componentIds),
         catalogDigest = std::move(catalogDigest), index,
         catalog = std::move(catalog), entry = std::move(entry)]() mutable {
            const auto reply = watcher->reply();
            watcher->deleteLater();
            if (generation != generation_) {
                return;
            }
            const auto arguments = reply.arguments();
            if (reply.type() == QDBusMessage::ErrorMessage
                || !hasExactMetaTypes(arguments, {QMetaType::QByteArray})) {
                fail(generation);
                return;
            }
            const auto bytes = arguments.first().toByteArray();
            const auto parsed = Components::parseDeclarativeDocument(
                QByteArrayView(bytes), &entry.settingsSchema
            );
            if (!parsed
                || Components::serializeDeclarativeDocument(*parsed.value)
                    != bytes) {
                fail(generation);
                return;
            }
            entry.declarativeRuntime = bytes;
            catalog.entries.insert(
                componentIds.at(index), std::move(entry)
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
