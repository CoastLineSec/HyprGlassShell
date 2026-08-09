#include "component_contract.h"

#include "strict_json.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSet>
#include <QUrl>

#include <cmath>
#include <limits>

namespace HyprShelld::Components {
namespace {

constexpr qsizetype maximumManifestBytes = 128 * 1024;
constexpr int maximumManifestDepth = 32;

void addError(
    ValidationErrors &errors,
    QString path,
    QString code,
    QString message
)
{
    errors.append({
        .path = std::move(path),
        .code = std::move(code),
        .message = std::move(message),
    });
}

void rejectUnknownFields(
    const QJsonObject &object,
    const QSet<QString> &allowed,
    const QString &path,
    ValidationErrors &errors
)
{
    for (auto iterator = object.constBegin(); iterator != object.constEnd();
         ++iterator) {
        if (!allowed.contains(iterator.key())) {
            addError(
                errors,
                path + QLatin1Char('.') + iterator.key(),
                QStringLiteral("manifest.unknown-field"),
                QStringLiteral("Unknown field: %1").arg(iterator.key())
            );
        }
    }
}

[[nodiscard]] QString normalizedText(const QString &value)
{
    return value.normalized(QString::NormalizationForm_C);
}

[[nodiscard]] bool hasDisallowedControl(
    const QString &value,
    const bool allowNewlines
)
{
    for (const auto codePoint : value.toUcs4()) {
        const auto category = QChar::category(
            static_cast<char32_t>(codePoint)
        );
        if ((category == QChar::Other_Control
                || category == QChar::Other_Format)
            && !(allowNewlines && codePoint == '\n')) {
            return true;
        }
    }
    return false;
}

[[nodiscard]] QString requiredString(
    const QJsonObject &object,
    const QString &key,
    const QString &path,
    const qsizetype maximumLength,
    ValidationErrors &errors,
    const bool allowNewlines = false
)
{
    const auto value = object.value(key);
    const auto valuePath = path + QLatin1Char('.') + key;
    if (!value.isString()) {
        addError(
            errors,
            valuePath,
            QStringLiteral("manifest.string-required"),
            QStringLiteral("A string value is required.")
        );
        return {};
    }

    const auto normalized = normalizedText(value.toString());
    if (normalized.isEmpty() || normalized.size() > maximumLength
        || normalized != normalized.trimmed()
        || hasDisallowedControl(normalized, allowNewlines)) {
        addError(
            errors,
            valuePath,
            QStringLiteral("manifest.invalid-string"),
            QStringLiteral("The string is empty, too long, or contains disallowed characters.")
        );
        return {};
    }
    return normalized;
}

[[nodiscard]] std::optional<QString> optionalString(
    const QJsonObject &object,
    const QString &key,
    const QString &path,
    const qsizetype maximumLength,
    ValidationErrors &errors
)
{
    if (!object.contains(key)) {
        return std::nullopt;
    }
    const auto value = requiredString(
        object,
        key,
        path,
        maximumLength,
        errors
    );
    if (value.isEmpty()) {
        return std::nullopt;
    }
    return value;
}

[[nodiscard]] std::optional<quint32> requiredUnsignedInteger(
    const QJsonObject &object,
    const QString &key,
    const QString &path,
    const quint32 minimum,
    const quint32 maximum,
    ValidationErrors &errors
)
{
    const auto value = object.value(key);
    const auto valuePath = path + QLatin1Char('.') + key;
    if (!value.isDouble()) {
        addError(
            errors,
            valuePath,
            QStringLiteral("manifest.integer-required"),
            QStringLiteral("An integer value is required.")
        );
        return std::nullopt;
    }
    const auto number = value.toDouble();
    if (!std::isfinite(number) || std::floor(number) != number
        || number < minimum || number > maximum) {
        addError(
            errors,
            valuePath,
            QStringLiteral("manifest.integer-out-of-range"),
            QStringLiteral("The integer is outside the accepted range.")
        );
        return std::nullopt;
    }
    return static_cast<quint32>(number);
}

[[nodiscard]] bool isWebUrl(const QString &value)
{
    if (value.size() > 2048) {
        return false;
    }
    const QUrl url(value, QUrl::StrictMode);
    return url.isValid() && !url.host().isEmpty()
        && (url.scheme() == QStringLiteral("https")
            || url.scheme() == QStringLiteral("http"))
        && url.userInfo().isEmpty();
}

void validateOptionalUrl(
    const std::optional<QString> &value,
    const QString &path,
    ValidationErrors &errors
)
{
    if (value.has_value() && !isWebUrl(*value)) {
        addError(
            errors,
            path,
            QStringLiteral("manifest.invalid-url"),
            QStringLiteral("Only a valid HTTP or HTTPS URL without user information is accepted.")
        );
    }
}

[[nodiscard]] bool hasValidCapabilityIdSyntax(const QString &id)
{
    static const QRegularExpression expression(QStringLiteral(
        "^[a-z](?:[a-z0-9-]{0,61}[a-z0-9])?"
        "(?:\\.[a-z](?:[a-z0-9-]{0,61}[a-z0-9])?)+$"
    ));
    return id.size() <= 255 && expression.match(id).hasMatch();
}

[[nodiscard]] bool isValidFactory(const QString &factory)
{
    static const QRegularExpression expression(QStringLiteral(
        "^[a-z][a-z0-9]*(?:-[a-z0-9]+)*$"
    ));
    return factory.size() <= 64 && expression.match(factory).hasMatch();
}

[[nodiscard]] bool isValidPackagePath(const QString &path)
{
    if (path.size() > 255 || !path.startsWith(QStringLiteral("payload/"))
        || path.contains(QLatin1Char('\\'))
        || path.contains(QLatin1Char(':'))) {
        return false;
    }
    static const QRegularExpression segmentExpression(QStringLiteral(
        "^[A-Za-z0-9][A-Za-z0-9._-]{0,127}$"
    ));
    const auto segments = path.split(QLatin1Char('/'));
    if (segments.size() < 2) {
        return false;
    }
    for (const auto &segment : segments) {
        if (segment == QStringLiteral(".") || segment == QStringLiteral("..")
            || !segmentExpression.match(segment).hasMatch()) {
            return false;
        }
    }
    return true;
}

[[nodiscard]] bool isComponentApiVersion(const QString &version)
{
    static const QRegularExpression expression(QStringLiteral(
        "^(0|[1-9][0-9]*)\\.(0|[1-9][0-9]*)$"
    ));
    return version.size() <= 32 && expression.match(version).hasMatch();
}

[[nodiscard]] bool isVersionRequirement(const QString &requirement)
{
    if (requirement == QStringLiteral("*")) {
        return true;
    }
    if (requirement.isEmpty() || requirement.size() > 256
        || requirement != requirement.trimmed()) {
        return false;
    }
    const auto terms = requirement.split(QLatin1Char(' '), Qt::SkipEmptyParts);
    if (terms.isEmpty() || terms.size() > 4) {
        return false;
    }
    for (auto term : terms) {
        for (const auto &prefix : {
                 QStringLiteral(">="),
                 QStringLiteral("<="),
                 QStringLiteral("^"),
                 QStringLiteral("~"),
                 QStringLiteral(">"),
                 QStringLiteral("<"),
                 QStringLiteral("="),
             }) {
            if (term.startsWith(prefix)) {
                term.remove(0, prefix.size());
                break;
            }
        }
        if (!isStrictSemanticVersion(term)) {
            return false;
        }
    }
    return true;
}

[[nodiscard]] QVector<ComponentAuthor> parseAuthors(
    const QJsonValue &value,
    ValidationErrors &errors
)
{
    QVector<ComponentAuthor> authors;
    if (!value.isArray()) {
        addError(
            errors,
            QStringLiteral("$.authors"),
            QStringLiteral("manifest.array-required"),
            QStringLiteral("authors must be an array.")
        );
        return authors;
    }
    const auto array = value.toArray();
    if (array.isEmpty() || array.size() > 16) {
        addError(
            errors,
            QStringLiteral("$.authors"),
            QStringLiteral("manifest.invalid-author-count"),
            QStringLiteral("One to sixteen authors are required.")
        );
        return authors;
    }

    QSet<QString> identities;
    for (qsizetype index = 0; index < array.size(); ++index) {
        const auto path = QStringLiteral("$.authors[%1]").arg(index);
        if (!array.at(index).isObject()) {
            addError(
                errors,
                path,
                QStringLiteral("manifest.object-required"),
                QStringLiteral("Each author must be an object.")
            );
            continue;
        }
        const auto object = array.at(index).toObject();
        rejectUnknownFields(
            object,
            {QStringLiteral("name"), QStringLiteral("email"),
             QStringLiteral("homepage")},
            path,
            errors
        );
        ComponentAuthor author{
            .name = requiredString(
                object,
                QStringLiteral("name"),
                path,
                128,
                errors
            ),
            .email = optionalString(
                object,
                QStringLiteral("email"),
                path,
                254,
                errors
            ),
            .homepage = optionalString(
                object,
                QStringLiteral("homepage"),
                path,
                2048,
                errors
            ),
        };
        if (author.email.has_value()
            && (!author.email->contains(QLatin1Char('@'))
                || author.email->contains(QLatin1Char(' ')))) {
            addError(
                errors,
                path + QStringLiteral(".email"),
                QStringLiteral("manifest.invalid-email"),
                QStringLiteral("The author email address is malformed.")
            );
        }
        validateOptionalUrl(author.homepage, path + QStringLiteral(".homepage"), errors);

        const auto identity = author.name + QLatin1Char('\n')
            + author.email.value_or(QString());
        if (identities.contains(identity)) {
            addError(
                errors,
                path,
                QStringLiteral("manifest.duplicate-author"),
                QStringLiteral("Duplicate author entry.")
            );
        }
        identities.insert(identity);
        authors.append(std::move(author));
    }
    return authors;
}

[[nodiscard]] ComponentRuntime parseRuntime(
    const QJsonValue &value,
    ValidationErrors &errors
)
{
    ComponentRuntime runtime;
    if (!value.isObject()) {
        addError(
            errors,
            QStringLiteral("$.runtime"),
            QStringLiteral("manifest.object-required"),
            QStringLiteral("runtime must be an object.")
        );
        return runtime;
    }
    const auto object = value.toObject();
    const auto kindText = requiredString(
        object,
        QStringLiteral("kind"),
        QStringLiteral("$.runtime"),
        32,
        errors
    );
    const auto kind = runtimeKindFromString(kindText);
    if (!kind.has_value()) {
        addError(
            errors,
            QStringLiteral("$.runtime.kind"),
            QStringLiteral("manifest.unknown-runtime-kind"),
            QStringLiteral("Unknown runtime kind.")
        );
        rejectUnknownFields(
            object,
            {QStringLiteral("kind"), QStringLiteral("factory"),
             QStringLiteral("entrypoint"), QStringLiteral("arguments")},
            QStringLiteral("$.runtime"),
            errors
        );
        return runtime;
    }
    runtime.kind = *kind;

    switch (*kind) {
    case RuntimeKind::BuiltinV1:
        rejectUnknownFields(
            object,
            {QStringLiteral("kind"), QStringLiteral("factory")},
            QStringLiteral("$.runtime"),
            errors
        );
        runtime.factory = requiredString(
            object,
            QStringLiteral("factory"),
            QStringLiteral("$.runtime"),
            64,
            errors
        );
        if (!runtime.factory.isEmpty() && !isValidFactory(runtime.factory)) {
            addError(
                errors,
                QStringLiteral("$.runtime.factory"),
                QStringLiteral("manifest.invalid-factory"),
                QStringLiteral("The built-in factory key is malformed.")
            );
        }
        break;
    case RuntimeKind::DeclarativeV1:
    case RuntimeKind::QmlFullTrustV1:
        rejectUnknownFields(
            object,
            {QStringLiteral("kind"), QStringLiteral("entrypoint")},
            QStringLiteral("$.runtime"),
            errors
        );
        runtime.entrypoint = requiredString(
            object,
            QStringLiteral("entrypoint"),
            QStringLiteral("$.runtime"),
            255,
            errors
        );
        if (!runtime.entrypoint.isEmpty()
            && !isValidPackagePath(runtime.entrypoint)) {
            addError(
                errors,
                QStringLiteral("$.runtime.entrypoint"),
                QStringLiteral("manifest.invalid-entrypoint"),
                QStringLiteral("The entry point must be a safe path below payload/.")
            );
        }
        break;
    case RuntimeKind::ProcessV1: {
        rejectUnknownFields(
            object,
            {QStringLiteral("kind"), QStringLiteral("entrypoint"),
             QStringLiteral("arguments")},
            QStringLiteral("$.runtime"),
            errors
        );
        runtime.entrypoint = requiredString(
            object,
            QStringLiteral("entrypoint"),
            QStringLiteral("$.runtime"),
            255,
            errors
        );
        if (!runtime.entrypoint.isEmpty()
            && !isValidPackagePath(runtime.entrypoint)) {
            addError(
                errors,
                QStringLiteral("$.runtime.entrypoint"),
                QStringLiteral("manifest.invalid-entrypoint"),
                QStringLiteral("The entry point must be a safe path below payload/.")
            );
        }
        if (object.contains(QStringLiteral("arguments"))) {
            const auto arguments = object.value(QStringLiteral("arguments"));
            if (!arguments.isArray() || arguments.toArray().size() > 32) {
                addError(
                    errors,
                    QStringLiteral("$.runtime.arguments"),
                    QStringLiteral("manifest.invalid-arguments"),
                    QStringLiteral("arguments must be an array of at most 32 strings.")
                );
            } else {
                const auto array = arguments.toArray();
                for (qsizetype index = 0; index < array.size(); ++index) {
                    const auto argument = array.at(index);
                    if (!argument.isString() || argument.toString().size() > 1024
                        || argument.toString().contains(QChar::Null)) {
                        addError(
                            errors,
                            QStringLiteral("$.runtime.arguments[%1]").arg(index),
                            QStringLiteral("manifest.invalid-argument"),
                            QStringLiteral("Each argument must be a bounded literal string.")
                        );
                    } else {
                        runtime.arguments.append(normalizedText(argument.toString()));
                    }
                }
            }
        }
        break;
    }
    }
    return runtime;
}

[[nodiscard]] QVector<CapabilityRequest> parseCapabilities(
    const QJsonValue &value,
    ValidationErrors &errors
)
{
    QVector<CapabilityRequest> capabilities;
    if (value.isUndefined()) {
        return capabilities;
    }
    if (!value.isArray() || value.toArray().size() > 64) {
        addError(
            errors,
            QStringLiteral("$.requestedCapabilities"),
            QStringLiteral("manifest.invalid-capabilities"),
            QStringLiteral("requestedCapabilities must contain at most 64 entries.")
        );
        return capabilities;
    }

    QSet<QString> identifiers;
    const auto array = value.toArray();
    for (qsizetype index = 0; index < array.size(); ++index) {
        const auto path = QStringLiteral("$.requestedCapabilities[%1]").arg(index);
        if (!array.at(index).isObject()) {
            addError(
                errors,
                path,
                QStringLiteral("manifest.object-required"),
                QStringLiteral("Each capability request must be an object.")
            );
            continue;
        }
        const auto object = array.at(index).toObject();
        rejectUnknownFields(
            object,
            {QStringLiteral("id"), QStringLiteral("reason")},
            path,
            errors
        );
        CapabilityRequest request{
            .id = requiredString(
                object,
                QStringLiteral("id"),
                path,
                255,
                errors
            ),
            .reason = requiredString(
                object,
                QStringLiteral("reason"),
                path,
                1024,
                errors,
                true
            ),
        };
        if (!request.id.isEmpty() && !hasValidCapabilityIdSyntax(request.id)) {
            addError(
                errors,
                path + QStringLiteral(".id"),
                QStringLiteral("manifest.invalid-capability-id"),
                QStringLiteral("The capability ID is malformed.")
            );
        }
        if (identifiers.contains(request.id)) {
            addError(
                errors,
                path + QStringLiteral(".id"),
                QStringLiteral("manifest.duplicate-capability"),
                QStringLiteral("A capability may be requested only once.")
            );
        }
        identifiers.insert(request.id);
        capabilities.append(std::move(request));
    }
    return capabilities;
}

[[nodiscard]] QVector<ComponentDependency> parseDependencies(
    const QJsonValue &value,
    const QString &componentId,
    ValidationErrors &errors
)
{
    QVector<ComponentDependency> dependencies;
    if (value.isUndefined()) {
        return dependencies;
    }
    if (!value.isArray() || value.toArray().size() > 64) {
        addError(
            errors,
            QStringLiteral("$.dependencies"),
            QStringLiteral("manifest.invalid-dependencies"),
            QStringLiteral("dependencies must contain at most 64 entries.")
        );
        return dependencies;
    }

    QSet<QString> identifiers;
    const auto array = value.toArray();
    for (qsizetype index = 0; index < array.size(); ++index) {
        const auto path = QStringLiteral("$.dependencies[%1]").arg(index);
        if (!array.at(index).isObject()) {
            addError(
                errors,
                path,
                QStringLiteral("manifest.object-required"),
                QStringLiteral("Each dependency must be an object.")
            );
            continue;
        }
        const auto object = array.at(index).toObject();
        rejectUnknownFields(
            object,
            {QStringLiteral("id"), QStringLiteral("version")},
            path,
            errors
        );
        ComponentDependency dependency{
            .id = requiredString(
                object,
                QStringLiteral("id"),
                path,
                255,
                errors
            ),
            .versionRequirement = requiredString(
                object,
                QStringLiteral("version"),
                path,
                256,
                errors
            ),
        };
        if (!dependency.id.isEmpty() && !isValidComponentId(dependency.id)) {
            addError(
                errors,
                path + QStringLiteral(".id"),
                QStringLiteral("manifest.invalid-dependency-id"),
                QStringLiteral("The dependency ID is malformed.")
            );
        }
        if (!dependency.versionRequirement.isEmpty()
            && !isVersionRequirement(dependency.versionRequirement)) {
            addError(
                errors,
                path + QStringLiteral(".version"),
                QStringLiteral("manifest.invalid-version-requirement"),
                QStringLiteral("The dependency version requirement is malformed.")
            );
        }
        if (dependency.id == componentId) {
            addError(
                errors,
                path + QStringLiteral(".id"),
                QStringLiteral("manifest.self-dependency"),
                QStringLiteral("A component cannot depend on itself.")
            );
        }
        if (identifiers.contains(dependency.id)) {
            addError(
                errors,
                path + QStringLiteral(".id"),
                QStringLiteral("manifest.duplicate-dependency"),
                QStringLiteral("A dependency may be declared only once.")
            );
        }
        identifiers.insert(dependency.id);
        dependencies.append(std::move(dependency));
    }
    return dependencies;
}

} // namespace

QString toString(const ComponentOrigin origin)
{
    switch (origin) {
    case ComponentOrigin::System: return QStringLiteral("system");
    case ComponentOrigin::User: return QStringLiteral("user");
    }
    return {};
}

QString toString(const ComponentType type)
{
    switch (type) {
    case ComponentType::BarWidget: return QStringLiteral("bar-widget");
    case ComponentType::DesktopWidget: return QStringLiteral("desktop-widget");
    case ComponentType::ShellApplication:
        return QStringLiteral("shell-application");
    case ComponentType::ShellService: return QStringLiteral("shell-service");
    }
    return {};
}

QString toString(const RuntimeKind kind)
{
    switch (kind) {
    case RuntimeKind::BuiltinV1: return QStringLiteral("builtin-v1");
    case RuntimeKind::DeclarativeV1: return QStringLiteral("declarative-v1");
    case RuntimeKind::QmlFullTrustV1:
        return QStringLiteral("qml-full-trust-v1");
    case RuntimeKind::ProcessV1: return QStringLiteral("process-v1");
    }
    return {};
}

std::optional<ComponentType> componentTypeFromString(const QString &value)
{
    if (value == QStringLiteral("bar-widget")) {
        return ComponentType::BarWidget;
    }
    if (value == QStringLiteral("desktop-widget")) {
        return ComponentType::DesktopWidget;
    }
    if (value == QStringLiteral("shell-application")) {
        return ComponentType::ShellApplication;
    }
    if (value == QStringLiteral("shell-service")) {
        return ComponentType::ShellService;
    }
    return std::nullopt;
}

std::optional<RuntimeKind> runtimeKindFromString(const QString &value)
{
    if (value == QStringLiteral("builtin-v1")) {
        return RuntimeKind::BuiltinV1;
    }
    if (value == QStringLiteral("declarative-v1")) {
        return RuntimeKind::DeclarativeV1;
    }
    if (value == QStringLiteral("qml-full-trust-v1")) {
        return RuntimeKind::QmlFullTrustV1;
    }
    if (value == QStringLiteral("process-v1")) {
        return RuntimeKind::ProcessV1;
    }
    return std::nullopt;
}

bool isValidComponentId(const QString &id)
{
    if (id.size() > 255) {
        return false;
    }
    const auto labels = id.split(QLatin1Char('.'));
    if (labels.size() < 3) {
        return false;
    }
    static const QRegularExpression labelExpression(QStringLiteral(
        "^[a-z](?:[a-z0-9-]{0,61}[a-z0-9])?$"
    ));
    for (const auto &label : labels) {
        if (label.size() > 63 || !labelExpression.match(label).hasMatch()) {
            return false;
        }
    }
    return true;
}

bool isValidCapabilityId(const QString &id)
{
    return hasValidCapabilityIdSyntax(id);
}

bool isReservedBuiltinId(const QString &id)
{
    return id.startsWith(QLatin1StringView(builtinIdPrefix));
}

bool isStrictSemanticVersion(const QString &version)
{
    static const QRegularExpression expression(QStringLiteral(
        "^(0|[1-9][0-9]*)\\.(0|[1-9][0-9]*)\\.(0|[1-9][0-9]*)"
        "(?:-(?:0|[1-9][0-9]*|[0-9]*[A-Za-z-][0-9A-Za-z-]*)"
        "(?:\\.(?:0|[1-9][0-9]*|[0-9]*[A-Za-z-][0-9A-Za-z-]*))*)?"
        "(?:\\+[0-9A-Za-z-]+(?:\\.[0-9A-Za-z-]+)*)?$"
    ));
    return version.size() <= 64 && expression.match(version).hasMatch();
}

ValidationResult<ComponentManifest> parseComponentManifest(
    const QByteArrayView bytes,
    const ComponentOrigin origin
)
{
    ValidationResult<ComponentManifest> result;
    const auto json = parseStrictJsonObject(
        bytes,
        {.maximumBytes = maximumManifestBytes,
         .maximumDepth = maximumManifestDepth}
    );
    if (!json) {
        result.errors = json.errors;
        return result;
    }
    const auto &object = *json.value;
    rejectUnknownFields(
        object,
        {
            QStringLiteral("manifestVersion"),
            QStringLiteral("id"),
            QStringLiteral("version"),
            QStringLiteral("type"),
            QStringLiteral("name"),
            QStringLiteral("description"),
            QStringLiteral("authors"),
            QStringLiteral("license"),
            QStringLiteral("homepage"),
            QStringLiteral("source"),
            QStringLiteral("issues"),
            QStringLiteral("componentApiVersion"),
            QStringLiteral("runtime"),
            QStringLiteral("settingsSchema"),
            QStringLiteral("requestedCapabilities"),
            QStringLiteral("dependencies"),
        },
        QStringLiteral("$"),
        result.errors
    );

    ComponentManifest manifest;
    manifest.origin = origin;
    const auto manifestVersion = requiredUnsignedInteger(
        object,
        QStringLiteral("manifestVersion"),
        QStringLiteral("$"),
        1,
        1,
        result.errors
    );
    if (manifestVersion.has_value()) {
        manifest.manifestVersion = *manifestVersion;
    }
    manifest.id = requiredString(
        object,
        QStringLiteral("id"),
        QStringLiteral("$"),
        255,
        result.errors
    );
    manifest.version = requiredString(
        object,
        QStringLiteral("version"),
        QStringLiteral("$"),
        64,
        result.errors
    );
    const auto typeText = requiredString(
        object,
        QStringLiteral("type"),
        QStringLiteral("$"),
        32,
        result.errors
    );
    const auto type = componentTypeFromString(typeText);
    if (type.has_value()) {
        manifest.type = *type;
    } else {
        addError(
            result.errors,
            QStringLiteral("$.type"),
            QStringLiteral("manifest.unknown-component-type"),
            QStringLiteral("Unknown component type.")
        );
    }
    manifest.name = requiredString(
        object,
        QStringLiteral("name"),
        QStringLiteral("$"),
        128,
        result.errors
    );
    manifest.description = requiredString(
        object,
        QStringLiteral("description"),
        QStringLiteral("$"),
        4096,
        result.errors,
        true
    );
    manifest.authors = parseAuthors(
        object.value(QStringLiteral("authors")),
        result.errors
    );
    manifest.license = requiredString(
        object,
        QStringLiteral("license"),
        QStringLiteral("$"),
        128,
        result.errors
    );
    static const QRegularExpression licenseExpression(QStringLiteral(
        "^[A-Za-z0-9.+:() -]+$"
    ));
    if (!manifest.license.isEmpty()
        && !licenseExpression.match(manifest.license).hasMatch()) {
        addError(
            result.errors,
            QStringLiteral("$.license"),
            QStringLiteral("manifest.invalid-license"),
            QStringLiteral("The license expression contains unsupported characters.")
        );
    }
    manifest.homepage = optionalString(
        object,
        QStringLiteral("homepage"),
        QStringLiteral("$"),
        2048,
        result.errors
    );
    manifest.source = optionalString(
        object,
        QStringLiteral("source"),
        QStringLiteral("$"),
        2048,
        result.errors
    );
    manifest.issues = optionalString(
        object,
        QStringLiteral("issues"),
        QStringLiteral("$"),
        2048,
        result.errors
    );
    validateOptionalUrl(manifest.homepage, QStringLiteral("$.homepage"), result.errors);
    validateOptionalUrl(manifest.source, QStringLiteral("$.source"), result.errors);
    validateOptionalUrl(manifest.issues, QStringLiteral("$.issues"), result.errors);
    manifest.componentApiVersion = requiredString(
        object,
        QStringLiteral("componentApiVersion"),
        QStringLiteral("$"),
        32,
        result.errors
    );
    if (!manifest.componentApiVersion.isEmpty()
        && !isComponentApiVersion(manifest.componentApiVersion)) {
        addError(
            result.errors,
            QStringLiteral("$.componentApiVersion"),
            QStringLiteral("manifest.invalid-component-api-version"),
            QStringLiteral("componentApiVersion must be a strict major.minor version.")
        );
    }
    manifest.runtime = parseRuntime(
        object.value(QStringLiteral("runtime")),
        result.errors
    );
    manifest.settingsSchema = optionalString(
        object,
        QStringLiteral("settingsSchema"),
        QStringLiteral("$"),
        64,
        result.errors
    );
    if (manifest.settingsSchema.has_value()
        && *manifest.settingsSchema != QStringLiteral("settings.schema.json")) {
        addError(
            result.errors,
            QStringLiteral("$.settingsSchema"),
            QStringLiteral("manifest.invalid-settings-schema-path"),
            QStringLiteral("Version one accepts only settings.schema.json at the package root.")
        );
    }
    manifest.requestedCapabilities = parseCapabilities(
        object.value(QStringLiteral("requestedCapabilities")),
        result.errors
    );
    manifest.dependencies = parseDependencies(
        object.value(QStringLiteral("dependencies")),
        manifest.id,
        result.errors
    );

    if (!manifest.id.isEmpty() && !isValidComponentId(manifest.id)) {
        addError(
            result.errors,
            QStringLiteral("$.id"),
            QStringLiteral("manifest.invalid-component-id"),
            QStringLiteral("The ID must be a strict lowercase reverse-DNS identifier.")
        );
    }
    if (origin == ComponentOrigin::System
        && !isReservedBuiltinId(manifest.id)) {
        addError(
            result.errors,
            QStringLiteral("$.id"),
            QStringLiteral("manifest.system-id-prefix-required"),
            QStringLiteral("System components must use the protected built-in prefix.")
        );
    }
    if (origin == ComponentOrigin::User && isReservedBuiltinId(manifest.id)) {
        addError(
            result.errors,
            QStringLiteral("$.id"),
            QStringLiteral("manifest.reserved-component-id"),
            QStringLiteral("User packages cannot use the protected built-in prefix.")
        );
    }
    if (!manifest.version.isEmpty()
        && !isStrictSemanticVersion(manifest.version)) {
        addError(
            result.errors,
            QStringLiteral("$.version"),
            QStringLiteral("manifest.invalid-semantic-version"),
            QStringLiteral("The component version must be strict Semantic Versioning.")
        );
    }
    if (manifest.runtime.kind == RuntimeKind::BuiltinV1
        && origin != ComponentOrigin::System) {
        addError(
            result.errors,
            QStringLiteral("$.runtime.kind"),
            QStringLiteral("manifest.builtin-runtime-forbidden"),
            QStringLiteral("Only protected system components may use builtin-v1.")
        );
    }

    const auto visualType = manifest.type == ComponentType::BarWidget
        || manifest.type == ComponentType::DesktopWidget;
    const auto visualRuntime = manifest.runtime.kind == RuntimeKind::BuiltinV1
        || manifest.runtime.kind == RuntimeKind::DeclarativeV1
        || manifest.runtime.kind == RuntimeKind::QmlFullTrustV1;
    const auto processType = manifest.type == ComponentType::ShellApplication
        || manifest.type == ComponentType::ShellService;
    if (!((visualType && visualRuntime)
          || (processType && manifest.runtime.kind == RuntimeKind::ProcessV1))) {
        addError(
            result.errors,
            QStringLiteral("$.runtime.kind"),
            QStringLiteral("manifest.invalid-type-runtime-combination"),
            QStringLiteral("The component type and runtime kind cannot be combined.")
        );
    }

    if (result.errors.isEmpty()) {
        result.value = std::move(manifest);
    }
    return result;
}

ValidationErrors validateCurrentHostSupport(const ComponentManifest &manifest)
{
    ValidationErrors errors;
    if (manifest.origin != ComponentOrigin::System
        || manifest.id != QLatin1StringView(workspaceSwitcherId)
        || manifest.type != ComponentType::BarWidget
        || manifest.componentApiVersion
            != QLatin1StringView(currentComponentApiVersion)
        || manifest.runtime.kind != RuntimeKind::BuiltinV1
        || manifest.runtime.factory
            != QLatin1StringView(workspaceSwitcherFactory)) {
        addError(
            errors,
            QStringLiteral("$"),
            QStringLiteral("component.unsupported-by-host"),
            QStringLiteral("The current component host supports only the protected workspace-switcher factory.")
        );
    }
    if (!manifest.dependencies.isEmpty()) {
        addError(
            errors,
            QStringLiteral("$.dependencies"),
            QStringLiteral("component.unsupported-dependencies"),
            QStringLiteral("The built-in workspace switcher has no component dependencies.")
        );
    }

    const QSet<QString> requiredCapabilities{
        QLatin1StringView(workspacesReadCapability),
        QLatin1StringView(workspacesActivateCapability),
    };
    QSet<QString> declaredCapabilities;
    for (qsizetype index = 0; index < manifest.requestedCapabilities.size();
         ++index) {
        const auto &capability = manifest.requestedCapabilities.at(index);
        declaredCapabilities.insert(capability.id);
        if (!requiredCapabilities.contains(capability.id)) {
            addError(
                errors,
                QStringLiteral("$.requestedCapabilities[%1].id").arg(index),
                QStringLiteral("component.unsupported-capability"),
                QStringLiteral("The built-in workspace switcher requested an unknown capability.")
            );
        }
    }
    if (declaredCapabilities != requiredCapabilities) {
        addError(
            errors,
            QStringLiteral("$.requestedCapabilities"),
            QStringLiteral("component.required-capabilities-missing"),
            QStringLiteral("The built-in workspace switcher must declare the exact capabilities supported by its host.")
        );
    }
    return errors;
}

} // namespace HyprShelld::Components
