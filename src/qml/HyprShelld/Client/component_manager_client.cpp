#include "component_manager_client.h"

#include "component/component_configuration.h"
#include "component/component_contract.h"
#include "component/package_inspection_report.h"
#include "component/settings_schema.h"

#include <QCryptographicHash>
#include <QDBusMessage>
#include <QDBusPendingCallWatcher>
#include <QDBusServiceWatcher>
#include <QDBusUnixFileDescriptor>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMetaType>
#include <QRegularExpression>
#include <QTimer>
#include <QtEndian>

#include <algorithm>
#include <array>
#include <initializer_list>
#include <optional>
#include <utility>

#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

namespace HyprShelld {
namespace {

const QString serviceName = QStringLiteral("org.hyprshelld.ComponentManager1");
const QString objectPath = QStringLiteral("/org/hyprshelld/ComponentManager1");
const QString interfaceName = QStringLiteral("org.hyprshelld.ComponentManager1");
const QString propertiesInterface = QStringLiteral(
    "org.freedesktop.DBus.Properties"
);
constexpr int callTimeoutMs = 3000;
constexpr int packageBeginTimeoutMs = 30000;
constexpr int maximumRetryDelayMs = 5000;
constexpr qsizetype maximumComponents = 512;
constexpr qsizetype maximumSettingsSchemaBytes = 256 * 1024;

struct DecodedComponent final {
    QVariantMap presentation;
    QString packageDigest;
};

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
        && arguments.at(17).toByteArray().size() <= maximumSettingsSchemaBytes
        && boundedStrings(arguments.at(18).toStringList(), 64, 255)
        && boundedStrings(arguments.at(19).toStringList(), 64, 1024)
        && boundedStrings(arguments.at(20).toStringList(), 64, 255)
        && boundedStrings(arguments.at(21).toStringList(), 64, 256)
        && arguments.at(22).toString().size() <= 64
        && arguments.at(23).toString().size() <= 6;
}

bool isStrictlySortedUnique(const QStringList &values)
{
    for (qsizetype index = 1; index < values.size(); ++index) {
        if (values.at(index) <= values.at(index - 1)) {
            return false;
        }
    }
    return true;
}

void addDigestField(
    QCryptographicHash &hash,
    const QByteArray &name,
    const QByteArray &value
)
{
    std::array<uchar, sizeof(quint64)> length{};
    qToBigEndian<quint64>(static_cast<quint64>(name.size()), length.data());
    hash.addData(QByteArrayView(
        reinterpret_cast<const char *>(length.data()),
        length.size()
    ));
    hash.addData(name);
    qToBigEndian<quint64>(static_cast<quint64>(value.size()), length.data());
    hash.addData(QByteArrayView(
        reinterpret_cast<const char *>(length.data()),
        length.size()
    ));
    hash.addData(value);
}

QString deriveCatalogDigest(
    const QStringList &componentIds,
    const QHash<QString, QString> &packageDigests
)
{
    QCryptographicHash hash(QCryptographicHash::Sha256);
    for (const auto &componentId : componentIds) {
        addDigestField(
            hash,
            componentId.toUtf8(),
            packageDigests.value(componentId).toLatin1()
        );
    }
    return QString::fromLatin1(hash.result().toHex());
}

int compareNumericSemverIdentifier(const QString &left, const QString &right)
{
    if (left.size() != right.size()) {
        return left.size() < right.size() ? -1 : 1;
    }
    const auto comparison = QString::compare(left, right, Qt::CaseSensitive);
    return comparison < 0 ? -1 : comparison > 0 ? 1 : 0;
}

int compareSemanticVersions(const QString &left, const QString &right)
{
    const auto withoutBuild = [](const QString &version) {
        return version.left(version.indexOf(QLatin1Char('+')) >= 0
            ? version.indexOf(QLatin1Char('+')) : version.size());
    };
    const auto leftValue = withoutBuild(left);
    const auto rightValue = withoutBuild(right);
    const auto leftDash = leftValue.indexOf(QLatin1Char('-'));
    const auto rightDash = rightValue.indexOf(QLatin1Char('-'));
    const auto leftCore = leftValue.left(
        leftDash >= 0 ? leftDash : leftValue.size()
    ).split(QLatin1Char('.'));
    const auto rightCore = rightValue.left(
        rightDash >= 0 ? rightDash : rightValue.size()
    ).split(QLatin1Char('.'));
    for (qsizetype index = 0; index < 3; ++index) {
        const auto compared = compareNumericSemverIdentifier(
            leftCore.value(index), rightCore.value(index)
        );
        if (compared != 0) {
            return compared;
        }
    }

    const auto leftHasPrerelease = leftDash >= 0;
    const auto rightHasPrerelease = rightDash >= 0;
    if (leftHasPrerelease != rightHasPrerelease) {
        return leftHasPrerelease ? -1 : 1;
    }
    if (!leftHasPrerelease) {
        return 0;
    }
    const auto leftPrerelease = leftValue.mid(leftDash + 1)
                                    .split(QLatin1Char('.'));
    const auto rightPrerelease = rightValue.mid(rightDash + 1)
                                     .split(QLatin1Char('.'));
    const auto count = std::min(
        leftPrerelease.size(), rightPrerelease.size()
    );
    for (qsizetype index = 0; index < count; ++index) {
        const auto &leftPart = leftPrerelease.at(index);
        const auto &rightPart = rightPrerelease.at(index);
        const auto leftNumeric = std::ranges::all_of(
            leftPart, [](const QChar character) { return character.isDigit(); }
        );
        const auto rightNumeric = std::ranges::all_of(
            rightPart, [](const QChar character) { return character.isDigit(); }
        );
        if (leftNumeric != rightNumeric) {
            return leftNumeric ? -1 : 1;
        }
        const auto compared = leftNumeric
            ? compareNumericSemverIdentifier(leftPart, rightPart)
            : QString::compare(leftPart, rightPart, Qt::CaseSensitive);
        if (compared != 0) {
            return compared < 0 ? -1 : 1;
        }
    }
    if (leftPrerelease.size() == rightPrerelease.size()) {
        return 0;
    }
    return leftPrerelease.size() < rightPrerelease.size() ? -1 : 1;
}

QJsonArray stringArray(const QStringList &values)
{
    QJsonArray array;
    for (const auto &value : values) {
        array.append(value);
    }
    return array;
}

QVariantList settingsDefinitions(const Components::SettingsSchema &schema)
{
    QVariantList definitions;
    definitions.reserve(schema.settings.size());
    for (const auto &definition : schema.settings) {
        QVariantMap item{
            {QStringLiteral("key"), definition.key},
            {QStringLiteral("scope"), Components::toString(definition.scope)},
            {QStringLiteral("type"), Components::toString(definition.type)},
            {QStringLiteral("label"), definition.label},
            {QStringLiteral("description"), definition.description},
            {QStringLiteral("group"), definition.group},
            {QStringLiteral("order"), definition.order},
            {QStringLiteral("defaultValue"), definition.defaultValue.toVariant()},
        };
        if (definition.minimum.has_value()) {
            item.insert(QStringLiteral("minimum"), *definition.minimum);
        }
        if (definition.maximum.has_value()) {
            item.insert(QStringLiteral("maximum"), *definition.maximum);
        }
        if (definition.step.has_value()) {
            item.insert(QStringLiteral("step"), *definition.step);
        }
        if (definition.minimumLength.has_value()) {
            item.insert(
                QStringLiteral("minimumLength"),
                static_cast<qlonglong>(*definition.minimumLength)
            );
        }
        if (definition.maximumLength.has_value()) {
            item.insert(
                QStringLiteral("maximumLength"),
                static_cast<qlonglong>(*definition.maximumLength)
            );
        }
        QVariantList options;
        for (const auto &option : definition.options) {
            options.append(QVariantMap{
                {QStringLiteral("value"), option.value},
                {QStringLiteral("label"), option.label},
            });
        }
        item.insert(QStringLiteral("options"), options);
        if (definition.visibleWhen.has_value()) {
            item.insert(
                QStringLiteral("visibleWhen"),
                QVariantMap{
                    {QStringLiteral("key"), definition.visibleWhen->key},
                    {QStringLiteral("equals"),
                     definition.visibleWhen->equals.toVariant()},
                }
            );
        }
        definitions.append(std::move(item));
    }
    return definitions;
}

QVariantMap inspectionPresentation(
    const Components::PackageInspectionReport &report,
    const QVariantList &installedComponents
)
{
    const auto &manifest = report.manifest;
    QVariantList authors;
    for (const auto &author : manifest.authors) {
        authors.append(QVariantMap{
            {QStringLiteral("name"), author.name},
            {QStringLiteral("email"), author.email.value_or(QString())},
            {QStringLiteral("homepage"), author.homepage.value_or(QString())},
        });
    }
    QVariantList capabilities;
    for (const auto &capability : manifest.requestedCapabilities) {
        capabilities.append(QVariantMap{
            {QStringLiteral("id"), capability.id},
            {QStringLiteral("reason"), capability.reason},
        });
    }
    QVariantList dependencies;
    for (const auto &dependency : manifest.dependencies) {
        dependencies.append(QVariantMap{
            {QStringLiteral("id"), dependency.id},
            {QStringLiteral("version"), dependency.versionRequirement},
        });
    }
    QVariantList files;
    for (const auto &file : report.files) {
        files.append(QVariantMap{
            {QStringLiteral("path"), file.path},
            {QStringLiteral("size"), QVariant::fromValue<qulonglong>(file.size)},
            {QStringLiteral("sha256"), file.sha256},
        });
    }

    QString operation = QStringLiteral("install");
    for (const auto &installed : installedComponents) {
        const auto map = installed.toMap();
        if (map.value(QStringLiteral("id")).toString() != manifest.id) {
            continue;
        }
        operation = map.value(QStringLiteral("packageDigest")).toString()
                == report.packageDigest
            ? QStringLiteral("reinstall")
            : compareSemanticVersions(
                    manifest.version,
                    map.value(QStringLiteral("version")).toString()
                ) < 0
                ? QStringLiteral("downgrade")
                : QStringLiteral("update");
        break;
    }

    Components::SettingsSchema schema;
    if (report.normalizedSettingsSchema.has_value()) {
        const auto bytes = QJsonDocument(*report.normalizedSettingsSchema)
                               .toJson(QJsonDocument::Compact);
        const auto parsed = Components::parseSettingsSchema(QByteArrayView(bytes));
        if (parsed) {
            schema = *parsed.value;
        }
    }

    return {
        {QStringLiteral("operation"), operation},
        {QStringLiteral("id"), manifest.id},
        {QStringLiteral("name"), manifest.name},
        {QStringLiteral("description"), manifest.description},
        {QStringLiteral("version"), manifest.version},
        {QStringLiteral("type"), Components::toString(manifest.type)},
        {QStringLiteral("authors"), authors},
        {QStringLiteral("license"), manifest.license},
        {QStringLiteral("homepage"), manifest.homepage.value_or(QString())},
        {QStringLiteral("source"), manifest.source.value_or(QString())},
        {QStringLiteral("issues"), manifest.issues.value_or(QString())},
        {QStringLiteral("runtime"), QVariantMap{
             {QStringLiteral("kind"), Components::toString(manifest.runtime.kind)},
             {QStringLiteral("factory"), manifest.runtime.factory},
             {QStringLiteral("entrypoint"), manifest.runtime.entrypoint},
             {QStringLiteral("arguments"), manifest.runtime.arguments},
         }},
        {QStringLiteral("requestedCapabilities"), capabilities},
        {QStringLiteral("dependencies"), dependencies},
        {QStringLiteral("archiveDigest"), report.archiveSha256},
        {QStringLiteral("packageDigest"), report.packageDigest},
        {QStringLiteral("archiveSize"),
         QVariant::fromValue<qulonglong>(report.archiveSize)},
        {QStringLiteral("expandedSize"),
         QVariant::fromValue<qulonglong>(report.expandedSize)},
        {QStringLiteral("files"), files},
        {QStringLiteral("hasSettings"),
         report.normalizedSettingsSchema.has_value()},
        {QStringLiteral("settingsDefinitions"), settingsDefinitions(schema)},
        {QStringLiteral("activationSupported"), false},
        {QStringLiteral("compatibilityReason"), QStringLiteral(
             "Runtime activation for third-party components is not available yet."
         )},
    };
}

std::optional<DecodedComponent> decodeComponent(
    const QString &componentId,
    const QVariantList &arguments,
    QString &error
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
        || !rawFieldsAreBounded(arguments)) {
        error = QStringLiteral(
            "ComponentManager1.GetComponent returned malformed or oversized fields."
        );
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
        || dependencyIds.size() != dependencyVersions.size()
        || !isStrictlySortedUnique(capabilityIds)) {
        error = QStringLiteral(
            "ComponentManager1.GetComponent returned inconsistent metadata arrays."
        );
        return std::nullopt;
    }

    const auto originText = arguments.at(23).toString();
    const auto origin = originText == QStringLiteral("system")
        ? std::optional(Components::ComponentOrigin::System)
        : originText == QStringLiteral("user")
            ? std::optional(Components::ComponentOrigin::User)
            : std::nullopt;
    const auto removable = arguments.at(24).toBool();
    if (!origin.has_value()
        || removable != (*origin == Components::ComponentOrigin::User)) {
        error = QStringLiteral(
            "ComponentManager1.GetComponent returned invalid origin metadata."
        );
        return std::nullopt;
    }

    const auto packageDigest = arguments.at(22).toString();
    if (!Components::isFullSha256Digest(packageDigest)) {
        error = QStringLiteral(
            "ComponentManager1.GetComponent returned an invalid package digest."
        );
        return std::nullopt;
    }

    const auto settingsSchema = arguments.at(17).toByteArray();
    Components::SettingsSchema parsedSettingsSchema;
    if (!settingsSchema.isEmpty()) {
        const auto parsed = Components::parseSettingsSchema(
            QByteArrayView(settingsSchema)
        );
        if (!parsed) {
            error = QStringLiteral(
                "ComponentManager1.GetComponent returned an invalid settings schema."
            );
            return std::nullopt;
        }
        parsedSettingsSchema = std::move(*parsed.value);
    }

    QJsonArray authors;
    for (qsizetype index = 0; index < authorNames.size(); ++index) {
        QJsonObject author{{QStringLiteral("name"), authorNames.at(index)}};
        if (!authorEmails.at(index).isEmpty()) {
            author.insert(QStringLiteral("email"), authorEmails.at(index));
        }
        if (!authorHomepages.at(index).isEmpty()) {
            author.insert(
                QStringLiteral("homepage"),
                authorHomepages.at(index)
            );
        }
        authors.append(author);
    }

    const auto runtimeKindText = arguments.at(13).toString();
    const auto runtimeKind = Components::runtimeKindFromString(runtimeKindText);
    if (!runtimeKind.has_value()) {
        error = QStringLiteral(
            "ComponentManager1.GetComponent returned an unknown runtime kind."
        );
        return std::nullopt;
    }
    QJsonObject runtime{{QStringLiteral("kind"), runtimeKindText}};
    switch (*runtimeKind) {
    case Components::RuntimeKind::BuiltinV1:
        if (!arguments.at(15).toString().isEmpty()
            || !arguments.at(16).toStringList().isEmpty()) {
            error = QStringLiteral(
                "ComponentManager1.GetComponent returned invalid built-in runtime fields."
            );
            return std::nullopt;
        }
        runtime.insert(QStringLiteral("factory"), arguments.at(14).toString());
        break;
    case Components::RuntimeKind::DeclarativeV1:
    case Components::RuntimeKind::QmlFullTrustV1:
        if (!arguments.at(14).toString().isEmpty()
            || !arguments.at(16).toStringList().isEmpty()) {
            error = QStringLiteral(
                "ComponentManager1.GetComponent returned invalid declarative runtime fields."
            );
            return std::nullopt;
        }
        runtime.insert(
            QStringLiteral("entrypoint"),
            arguments.at(15).toString()
        );
        break;
    case Components::RuntimeKind::ProcessV1:
        if (!arguments.at(14).toString().isEmpty()) {
            error = QStringLiteral(
                "ComponentManager1.GetComponent returned invalid process runtime fields."
            );
            return std::nullopt;
        }
        runtime.insert(
            QStringLiteral("entrypoint"),
            arguments.at(15).toString()
        );
        runtime.insert(
            QStringLiteral("arguments"),
            stringArray(arguments.at(16).toStringList())
        );
        break;
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

    QJsonObject manifest{
        {QStringLiteral("manifestVersion"),
         static_cast<qint64>(arguments.at(0).toUInt())},
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
    for (const auto &[key, value] : {
             std::pair{QStringLiteral("homepage"), arguments.at(9).toString()},
             std::pair{QStringLiteral("source"), arguments.at(10).toString()},
             std::pair{QStringLiteral("issues"), arguments.at(11).toString()},
         }) {
        if (!value.isEmpty()) {
            manifest.insert(key, value);
        }
    }
    if (!settingsSchema.isEmpty()) {
        manifest.insert(
            QStringLiteral("settingsSchema"),
            QStringLiteral("settings.schema.json")
        );
    }

    const auto parsedManifest = Components::parseComponentManifest(
        QByteArrayView(QJsonDocument(manifest).toJson(QJsonDocument::Compact)),
        *origin
    );
    if (!parsedManifest) {
        error = QStringLiteral(
            "ComponentManager1.GetComponent returned invalid manifest metadata."
        );
        return std::nullopt;
    }

    const auto &parsed = *parsedManifest.value;
    QVariantList presentationAuthors;
    presentationAuthors.reserve(parsed.authors.size());
    for (const auto &author : parsed.authors) {
        presentationAuthors.append(QVariantMap{
            {QStringLiteral("name"), author.name},
            {QStringLiteral("email"), author.email.value_or(QString())},
            {QStringLiteral("homepage"), author.homepage.value_or(QString())},
        });
    }
    QVariantList presentationCapabilities;
    presentationCapabilities.reserve(parsed.requestedCapabilities.size());
    for (const auto &capability : parsed.requestedCapabilities) {
        presentationCapabilities.append(QVariantMap{
            {QStringLiteral("id"), capability.id},
            {QStringLiteral("reason"), capability.reason},
        });
    }
    QVariantList presentationDependencies;
    presentationDependencies.reserve(parsed.dependencies.size());
    for (const auto &dependency : parsed.dependencies) {
        presentationDependencies.append(QVariantMap{
            {QStringLiteral("id"), dependency.id},
            {QStringLiteral("version"), dependency.versionRequirement},
        });
    }

    return DecodedComponent{
        .presentation = QVariantMap{
            {QStringLiteral("id"), parsed.id},
            {QStringLiteral("type"), Components::toString(parsed.type)},
            {QStringLiteral("version"), parsed.version},
            {QStringLiteral("name"), parsed.name},
            {QStringLiteral("description"), parsed.description},
            {QStringLiteral("authors"), presentationAuthors},
            {QStringLiteral("license"), parsed.license},
            {QStringLiteral("homepage"), parsed.homepage.value_or(QString())},
            {QStringLiteral("source"), parsed.source.value_or(QString())},
            {QStringLiteral("issues"), parsed.issues.value_or(QString())},
            {QStringLiteral("componentApiVersion"),
             parsed.componentApiVersion},
            {QStringLiteral("runtime"),
             QVariantMap{
                 {QStringLiteral("kind"), Components::toString(parsed.runtime.kind)},
                 {QStringLiteral("factory"), parsed.runtime.factory},
                 {QStringLiteral("entrypoint"), parsed.runtime.entrypoint},
                 {QStringLiteral("arguments"), parsed.runtime.arguments},
             }},
            {QStringLiteral("packageDigest"), packageDigest},
            {QStringLiteral("origin"), Components::toString(parsed.origin)},
            {QStringLiteral("removable"), removable},
            {QStringLiteral("hasSettings"), !settingsSchema.isEmpty()},
            {QStringLiteral("settingsDefinitions"),
             settingsDefinitions(parsedSettingsSchema)},
            {QStringLiteral("activationSupported"),
             parsed.origin == Components::ComponentOrigin::System
                 && Components::validateCurrentHostSupport(parsed).isEmpty()},
            {QStringLiteral("compatibilityReason"),
             parsed.origin == Components::ComponentOrigin::System
                 ? QString()
                 : QStringLiteral(
                       "Runtime activation for third-party components is not available yet."
                   )},
            {QStringLiteral("requestedCapabilities"),
             presentationCapabilities},
            {QStringLiteral("dependencies"), presentationDependencies},
        },
        .packageDigest = packageDigest,
    };
}

QString replyError(const QDBusMessage &reply, const QString &operation)
{
    if (reply.errorName().isEmpty()) {
        return operation + QStringLiteral(" failed.");
    }
    auto message = reply.errorMessage().left(1024)
                       .normalized(QString::NormalizationForm_C);
    for (auto &character : message) {
        if (character.category() == QChar::Other_Control) {
            character = QLatin1Char(' ');
        }
    }
    return QStringLiteral("%1 failed: %2: %3")
        .arg(operation, reply.errorName().left(255), message);
}

} // namespace

struct ComponentManagerClient::HydrationState final {
    QStringList componentIds;
    QString catalogDigest;
    QVariantList components;
    QHash<QString, QString> packageDigests;
    qsizetype index = 0;
};

ComponentManagerClient::ComponentManagerClient(QObject *parent)
    : ComponentManagerClient(QDBusConnection::sessionBus(), parent)
{
}

ComponentManagerClient::ComponentManagerClient(
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
        &ComponentManagerClient::serviceOwnerChanged
    );
    connection_.connect(
        serviceName,
        objectPath,
        propertiesInterface,
        QStringLiteral("PropertiesChanged"),
        this,
        SLOT(propertiesChanged(QString,QVariantMap,QStringList))
    );
    connection_.connect(
        serviceName,
        objectPath,
        interfaceName,
        QStringLiteral("PackageInspectionFinished"),
        this,
        SLOT(packageInspectionFinished(QString,QByteArray,QString,QString))
    );
    refresh();
}

bool ComponentManagerClient::available() const { return available_; }
bool ComponentManagerClient::busy() const { return busy_; }
QString ComponentManagerClient::catalogDigest() const { return catalogDigest_; }
QVariantList ComponentManagerClient::components() const { return components_; }
QString ComponentManagerClient::lastError() const { return lastError_; }
bool ComponentManagerClient::inspectionBusy() const { return inspectionBusy_; }
bool ComponentManagerClient::packageOperationBusy() const
{
    return packageOperationBusy_;
}
QVariantMap ComponentManagerClient::inspectionReview() const
{
    return inspectionReview_;
}
QString ComponentManagerClient::inspectionToken() const
{
    return inspectionToken_;
}
QString ComponentManagerClient::packageError() const { return packageError_; }

void ComponentManagerClient::inspectPackage(const QUrl &packageUrl)
{
    if (inspectionBusy_ || packageOperationBusy_) {
        return;
    }
    if (!inspectionToken_.isEmpty()) {
        auto cancel = QDBusMessage::createMethodCall(
            serviceName,
            objectPath,
            interfaceName,
            QStringLiteral("CancelPackageInspection")
        );
        cancel.setArguments({inspectionToken_});
        connection_.asyncCall(cancel, callTimeoutMs);
    }
    setPackageError(QString());
    setInspectionReview({});
    setInspectionToken({});
    inspectionArchiveDigest_.clear();
    inspectionCatalogDigest_.clear();

    if (!packageUrl.isValid() || !packageUrl.isLocalFile()) {
        setPackageError(QStringLiteral("Choose a local HyprShelld component file."));
        return;
    }

    const auto encodedPath = QFile::encodeName(packageUrl.toLocalFile());
    const auto descriptor = ::open(
        encodedPath.constData(),
        O_RDONLY | O_CLOEXEC | O_NOFOLLOW | O_NONBLOCK
    );
    if (descriptor < 0) {
        setPackageError(
            QStringLiteral("The component file could not be opened: %1")
                .arg(QString::fromLocal8Bit(std::strerror(errno)))
        );
        return;
    }

    struct stat status {};
    constexpr off_t maximumArchiveBytes = 32 * 1024 * 1024;
    if (::fstat(descriptor, &status) != 0 || !S_ISREG(status.st_mode)
        || status.st_size <= 0 || status.st_size > maximumArchiveBytes) {
        ::close(descriptor);
        setPackageError(
            QStringLiteral("The component must be a nonempty regular file no larger than 32 MiB.")
        );
        return;
    }

    QDBusUnixFileDescriptor dbusDescriptor;
    dbusDescriptor.giveFileDescriptor(descriptor);
    if (!dbusDescriptor.isValid()) {
        setPackageError(QStringLiteral("The component file descriptor is unavailable."));
        return;
    }

    const auto operation = ++packageGeneration_;
    setInspectionBusy(true);
    auto message = QDBusMessage::createMethodCall(
        serviceName,
        objectPath,
        interfaceName,
        QStringLiteral("BeginPackageInspection")
    );
    message.setArguments({QVariant::fromValue(dbusDescriptor)});
    auto *watcher = new QDBusPendingCallWatcher(
        connection_.asyncCall(message, packageBeginTimeoutMs),
        this
    );
    connect(
        watcher,
        &QDBusPendingCallWatcher::finished,
        this,
        [this, watcher, operation] {
            const auto reply = watcher->reply();
            watcher->deleteLater();
            if (operation != packageGeneration_) {
                const auto arguments = reply.arguments();
                if (reply.type() == QDBusMessage::ReplyMessage
                    && hasExactMetaTypes(
                        arguments,
                        {QMetaType::QString}
                    )) {
                    const auto staleToken = arguments.at(0).toString();
                    static const QRegularExpression tokenPattern(
                        QStringLiteral("^[0-9a-f]{32}$")
                    );
                    if (tokenPattern.match(staleToken).hasMatch()) {
                        auto cancel = QDBusMessage::createMethodCall(
                            serviceName,
                            objectPath,
                            interfaceName,
                            QStringLiteral("CancelPackageInspection")
                        );
                        cancel.setArguments({staleToken});
                        connection_.asyncCall(cancel, callTimeoutMs);
                    }
                }
                return;
            }
            const auto arguments = reply.arguments();
            if (reply.type() == QDBusMessage::ErrorMessage
                || !hasExactMetaTypes(arguments, {QMetaType::QString})) {
                setInspectionBusy(false);
                setPackageError(
                    replyError(reply, QStringLiteral("Package inspection"))
                );
                return;
            }
            const auto token = arguments.at(0).toString();
            static const QRegularExpression tokenPattern(
                QStringLiteral("^[0-9a-f]{32}$")
            );
            if (!tokenPattern.match(token).hasMatch()) {
                setInspectionBusy(false);
                setPackageError(
                    QStringLiteral("The component manager returned an invalid inspection token.")
                );
                return;
            }
            setInspectionToken(token);
        }
    );
}

void ComponentManagerClient::cancelInspection()
{
    ++packageGeneration_;
    if (!inspectionToken_.isEmpty()) {
        auto message = QDBusMessage::createMethodCall(
            serviceName,
            objectPath,
            interfaceName,
            QStringLiteral("CancelPackageInspection")
        );
        message.setArguments({inspectionToken_});
        connection_.asyncCall(message, callTimeoutMs);
    }
    setInspectionBusy(false);
    setInspectionReview({});
    setInspectionToken({});
    inspectionArchiveDigest_.clear();
    inspectionCatalogDigest_.clear();
}

void ComponentManagerClient::installInspectedPackage()
{
    if (inspectionToken_.isEmpty() || inspectionArchiveDigest_.isEmpty()
        || !Components::isFullSha256Digest(inspectionCatalogDigest_)
        || inspectionBusy_ || packageOperationBusy_) {
        return;
    }

    const auto operation = ++packageGeneration_;
    const auto token = inspectionToken_;
    const auto archiveDigest = inspectionArchiveDigest_;
    setPackageError(QString());
    setPackageOperationBusy(true);
    auto message = QDBusMessage::createMethodCall(
        serviceName,
        objectPath,
        interfaceName,
        QStringLiteral("InstallInspectedPackage")
    );
    message.setArguments({token, archiveDigest, inspectionCatalogDigest_});
    auto *watcher = new QDBusPendingCallWatcher(
        connection_.asyncCall(message, 30000),
        this
    );
    connect(
        watcher,
        &QDBusPendingCallWatcher::finished,
        this,
        [this, watcher, operation, token] {
            const auto reply = watcher->reply();
            watcher->deleteLater();
            if (operation != packageGeneration_) {
                return;
            }
            const auto arguments = reply.arguments();
            if (reply.type() == QDBusMessage::ErrorMessage
                || !hasExactMetaTypes(
                    arguments,
                    {QMetaType::QString, QMetaType::QString,
                     QMetaType::QString}
                )
                || !Components::isValidComponentId(
                    arguments.value(0).toString()
                )
                || !Components::isFullSha256Digest(
                    arguments.value(1).toString()
                )
                || !Components::isFullSha256Digest(
                    arguments.value(2).toString()
                )) {
                auto cancel = QDBusMessage::createMethodCall(
                    serviceName,
                    objectPath,
                    interfaceName,
                    QStringLiteral("CancelPackageInspection")
                );
                cancel.setArguments({token});
                connection_.asyncCall(cancel, callTimeoutMs);
                setPackageOperationBusy(false);
                setInspectionReview({});
                setInspectionToken({});
                inspectionArchiveDigest_.clear();
                inspectionCatalogDigest_.clear();
                setPackageError(
                    replyError(reply, QStringLiteral("Package installation"))
                );
                return;
            }
            const auto componentId = arguments.at(0).toString();
            setPackageOperationBusy(false);
            setInspectionReview({});
            setInspectionToken({});
            inspectionArchiveDigest_.clear();
            inspectionCatalogDigest_.clear();
            emit packageInstalled(componentId);
            refresh();
        }
    );
}

void ComponentManagerClient::removeComponent(
    const QString &componentId,
    const QString &packageDigest,
    const QString &expectedCatalogDigest
)
{
    if (packageOperationBusy_ || inspectionBusy_
        || !Components::isValidComponentId(componentId)
        || !Components::isFullSha256Digest(packageDigest)
        || !Components::isFullSha256Digest(expectedCatalogDigest)) {
        return;
    }

    const auto operation = ++packageGeneration_;
    setPackageError(QString());
    setPackageOperationBusy(true);
    auto message = QDBusMessage::createMethodCall(
        serviceName,
        objectPath,
        interfaceName,
        QStringLiteral("RemovePackage")
    );
    message.setArguments({componentId, packageDigest, expectedCatalogDigest});
    auto *watcher = new QDBusPendingCallWatcher(
        connection_.asyncCall(message, 30000),
        this
    );
    connect(
        watcher,
        &QDBusPendingCallWatcher::finished,
        this,
        [this, watcher, operation, componentId] {
            const auto reply = watcher->reply();
            watcher->deleteLater();
            if (operation != packageGeneration_) {
                return;
            }
            const auto arguments = reply.arguments();
            if (reply.type() == QDBusMessage::ErrorMessage
                || !hasExactMetaTypes(arguments, {QMetaType::QString})
                || !Components::isFullSha256Digest(
                    arguments.value(0).toString()
                )) {
                setPackageOperationBusy(false);
                setPackageError(
                    replyError(reply, QStringLiteral("Package removal"))
                );
                return;
            }
            setPackageOperationBusy(false);
            emit packageRemoved(componentId);
            refresh();
        }
    );
}

void ComponentManagerClient::clearPackageError()
{
    setPackageError(QString());
}

void ComponentManagerClient::propertiesChanged(
    const QString &changedInterface,
    const QVariantMap &changed,
    const QStringList &invalidated
)
{
    if (changedInterface == interfaceName
        && (changed.contains(QStringLiteral("CatalogDigest"))
            || invalidated.contains(QStringLiteral("CatalogDigest")))) {
        refresh();
    }
}

void ComponentManagerClient::serviceOwnerChanged(
    const QString &name,
    const QString &oldOwner,
    const QString &newOwner
)
{
    Q_UNUSED(name)
    Q_UNUSED(oldOwner)
    ++generation_;
    ++packageGeneration_;
    retryTimer_->stop();
    retryDelayMs_ = 250;
    setBusy(false);
    setAvailable(false);
    setInspectionBusy(false);
    setPackageOperationBusy(false);
    setInspectionReview({});
    setInspectionToken({});
    inspectionArchiveDigest_.clear();
    inspectionCatalogDigest_.clear();
    if (newOwner.isEmpty()) {
        setLastError(QStringLiteral("The component manager is unavailable."));
        scheduleRetry();
    } else {
        refresh();
    }
}

void ComponentManagerClient::packageInspectionFinished(
    const QString &token,
    const QByteArray &review,
    const QString &errorCode,
    const QString &errorMessage
)
{
    if (token != inspectionToken_) {
        return;
    }
    setInspectionBusy(false);
    if (!errorCode.isEmpty()) {
        setPackageError(
            QStringLiteral("Package inspection failed: %1: %2")
                .arg(errorCode.left(255), errorMessage.left(1024))
        );
        setInspectionReview({});
        setInspectionToken({});
        inspectionArchiveDigest_.clear();
        inspectionCatalogDigest_.clear();
        return;
    }

    auto parsed = Components::parsePackageInspectionReport(
        QByteArrayView(review)
    );
    if (!parsed || parsed.value->inspectionToken != token) {
        setPackageError(
            QStringLiteral("The component manager returned an invalid inspection report.")
        );
        setInspectionReview({});
        setInspectionToken({});
        inspectionArchiveDigest_.clear();
        inspectionCatalogDigest_.clear();
        return;
    }
    inspectionArchiveDigest_ = parsed.value->archiveSha256;
    inspectionCatalogDigest_ = catalogDigest_;
    setInspectionReview(
        inspectionPresentation(*parsed.value, components_)
    );
    setPackageError(QString());
}

void ComponentManagerClient::refresh()
{
    retryTimer_->stop();
    const auto generation = ++generation_;
    setAvailable(false);
    setBusy(true);

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
            if (reply.type() == QDBusMessage::ErrorMessage) {
                fail(
                    generation,
                    replyError(reply, QStringLiteral("ListComponents"))
                );
                return;
            }
            if (!hasExactMetaTypes(
                    arguments,
                    {QMetaType::QStringList, QMetaType::QString}
                )) {
                fail(
                    generation,
                    QStringLiteral(
                        "ComponentManager1.ListComponents returned malformed fields."
                    )
                );
                return;
            }

            const auto componentIds = arguments.at(0).toStringList();
            const auto digest = arguments.at(1).toString();
            if (componentIds.isEmpty()
                || componentIds.size() > maximumComponents
                || !isStrictlySortedUnique(componentIds)
                || !Components::isFullSha256Digest(digest)
                || !std::ranges::all_of(
                    componentIds,
                    [](const QString &componentId) {
                        return Components::isValidComponentId(componentId);
                    }
                )) {
                fail(
                    generation,
                    QStringLiteral(
                        "ComponentManager1.ListComponents returned an invalid catalog generation."
                    )
                );
                return;
            }

            auto state = std::make_shared<HydrationState>();
            state->componentIds = componentIds;
            state->catalogDigest = digest;
            state->components.reserve(componentIds.size());
            fetchNext(generation, std::move(state));
        }
    );
}

void ComponentManagerClient::fetchNext(
    const quint64 generation,
    std::shared_ptr<HydrationState> state
)
{
    if (generation != generation_) {
        return;
    }
    if (state->index == state->componentIds.size()) {
        accept(generation, *state);
        return;
    }

    const auto componentId = state->componentIds.at(state->index);
    auto message = QDBusMessage::createMethodCall(
        serviceName,
        objectPath,
        interfaceName,
        QStringLiteral("GetComponent")
    );
    message.setArguments({componentId, state->catalogDigest});
    auto *watcher = new QDBusPendingCallWatcher(
        connection_.asyncCall(message, callTimeoutMs),
        this
    );
    connect(
        watcher,
        &QDBusPendingCallWatcher::finished,
        this,
        [this, watcher, generation, state = std::move(state), componentId] {
            const auto reply = watcher->reply();
            watcher->deleteLater();
            if (generation != generation_) {
                return;
            }
            if (reply.type() == QDBusMessage::ErrorMessage) {
                fail(
                    generation,
                    replyError(reply, QStringLiteral("GetComponent"))
                );
                return;
            }

            QString error;
            auto decoded = decodeComponent(componentId, reply.arguments(), error);
            if (!decoded.has_value()) {
                fail(generation, error);
                return;
            }
            state->components.append(std::move(decoded->presentation));
            state->packageDigests.insert(
                componentId,
                std::move(decoded->packageDigest)
            );
            ++state->index;
            fetchNext(generation, std::move(state));
        }
    );
}

void ComponentManagerClient::accept(
    const quint64 generation,
    const HydrationState &state
)
{
    if (generation != generation_) {
        return;
    }
    if (state.packageDigests.size() != state.componentIds.size()
        || deriveCatalogDigest(state.componentIds, state.packageDigests)
            != state.catalogDigest) {
        fail(
            generation,
            QStringLiteral(
                "The hydrated component records do not match the catalog digest."
            )
        );
        return;
    }

    if (catalogDigest_ != state.catalogDigest) {
        catalogDigest_ = state.catalogDigest;
        emit catalogDigestChanged();
    }
    if (components_ != state.components) {
        components_ = state.components;
        emit componentsChanged();
    }
    retryDelayMs_ = 250;
    setLastError(QString());
    setBusy(false);
    setAvailable(true);
}

void ComponentManagerClient::fail(
    const quint64 generation,
    const QString &error
)
{
    if (generation != generation_) {
        return;
    }
    setBusy(false);
    setAvailable(false);
    setLastError(
        error.isEmpty()
            ? QStringLiteral("The component catalog could not be loaded.")
            : error
    );
    scheduleRetry();
}

void ComponentManagerClient::scheduleRetry()
{
    if (retryTimer_->isActive()) {
        return;
    }
    retryTimer_->start(retryDelayMs_);
    retryDelayMs_ = std::min(retryDelayMs_ * 2, maximumRetryDelayMs);
}

void ComponentManagerClient::setAvailable(const bool available)
{
    if (available_ == available) {
        return;
    }
    available_ = available;
    emit availableChanged();
}

void ComponentManagerClient::setBusy(const bool busy)
{
    if (busy_ == busy) {
        return;
    }
    busy_ = busy;
    emit busyChanged();
}

void ComponentManagerClient::setLastError(const QString &error)
{
    if (lastError_ == error) {
        return;
    }
    lastError_ = error;
    emit lastErrorChanged();
}

void ComponentManagerClient::setInspectionBusy(const bool busy)
{
    if (inspectionBusy_ == busy) {
        return;
    }
    inspectionBusy_ = busy;
    emit inspectionBusyChanged();
}

void ComponentManagerClient::setPackageOperationBusy(const bool busy)
{
    if (packageOperationBusy_ == busy) {
        return;
    }
    packageOperationBusy_ = busy;
    emit packageOperationBusyChanged();
}

void ComponentManagerClient::setInspectionReview(QVariantMap review)
{
    if (inspectionReview_ == review) {
        return;
    }
    inspectionReview_ = std::move(review);
    emit inspectionReviewChanged();
}

void ComponentManagerClient::setInspectionToken(QString token)
{
    if (inspectionToken_ == token) {
        return;
    }
    inspectionToken_ = std::move(token);
    emit inspectionTokenChanged();
}

void ComponentManagerClient::setPackageError(QString error)
{
    error = error.left(2048).normalized(QString::NormalizationForm_C);
    for (auto &character : error) {
        if (character.category() == QChar::Other_Control) {
            character = QLatin1Char(' ');
        }
    }
    if (packageError_ == error) {
        return;
    }
    packageError_ = std::move(error);
    emit packageErrorChanged();
}

} // namespace HyprShelld
