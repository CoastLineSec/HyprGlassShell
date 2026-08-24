#include "catalog.h"

#include "json_support.h"

#include <QCryptographicHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QMap>
#include <QRegularExpression>
#include <QSet>
#include <QUrl>

#include <algorithm>
#include <cmath>
#include <limits>

namespace HyprShelld::Hyprland {
namespace {

constexpr int maximumCatalogDepth = 64;
constexpr qsizetype maximumStringLength = 4096;

struct CatalogContract final {
    quint32 contractVersion;
    const char *reviewedVersion;
    const char *reviewedTag;
    const char *reviewedCommit;
    quint32 minimumPatch;
    std::optional<quint32> maximumPatch;
    const char *minimumSupported;
    const char *fullyQualified;
    bool requiresSourceManifestDigest;
    const char *sourceManifestDigest;
    const char *integrityDigest;
};

[[nodiscard]] const CatalogContract &activeCatalogContract()
{
    static const CatalogContract contract{
        .contractVersion = currentCatalogContractVersion,
        .reviewedVersion = "0.56.1",
        .reviewedTag = "v0.56.1",
        .reviewedCommit = "5c9377c15f85c50648f35ca5a213754f95b93ca0",
        .minimumPatch = 0,
        .maximumPatch = std::nullopt,
        .minimumSupported = "0.55.0",
        .fullyQualified = "0.56.x",
        .requiresSourceManifestDigest = false,
        .sourceManifestDigest = nullptr,
        .integrityDigest = reviewedCatalogDigest,
    };
    return contract;
}

[[nodiscard]] const CatalogContract &dormantCatalogContractV2()
{
    static const CatalogContract contract{
        .contractVersion = dormantCatalogV2ContractVersion,
        .reviewedVersion = "0.56.2",
        .reviewedTag = "v0.56.2",
        .reviewedCommit = "efb50993780079460b0cbed1363e2166a2de1d9f",
        .minimumPatch = 2,
        .maximumPatch = 2,
        .minimumSupported = "0.56.2",
        .fullyQualified = "0.56.2",
        .requiresSourceManifestDigest = true,
        .sourceManifestDigest = dormantReviewedSourceManifestDigest,
        .integrityDigest = dormantReviewedCatalogV2Digest,
    };
    return contract;
}

[[nodiscard]] bool isSha256(const QString &value)
{
    static const QRegularExpression expression(
        QStringLiteral("^[0-9a-f]{64}$")
    );
    return expression.match(value).hasMatch();
}

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

[[nodiscard]] QString qtCompatiblePattern(const QString &pattern)
{
    QString converted;
    converted.reserve(pattern.size() + 16);
    static const QRegularExpression hexQuad(QStringLiteral("^[0-9A-Fa-f]{4}$"));
    for (qsizetype index = 0; index < pattern.size(); ++index) {
        const auto character = pattern.at(index);
        if (character == QLatin1Char('\\') && index + 5 < pattern.size()
            && pattern.at(index + 1) == QLatin1Char('u')) {
            const auto digits = pattern.sliced(index + 2, 4);
            if (hexQuad.match(digits).hasMatch()) {
                converted.append(QStringLiteral("\\x{%1}").arg(digits));
                index += 5;
                continue;
            }
        }
        converted.append(character);
    }
    return converted;
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
                QStringLiteral("catalog.unknown-field"),
                QStringLiteral("Unknown catalog field: %1")
                    .arg(iterator.key())
            );
        }
    }
}

[[nodiscard]] bool hasDisallowedCharacter(const QString &value)
{
    for (const auto codePoint : value.toUcs4()) {
        const auto category = QChar::category(static_cast<char32_t>(codePoint));
        if (category == QChar::Other_Control
            || category == QChar::Other_Format) {
            return true;
        }
    }
    return false;
}

[[nodiscard]] QString readString(
    const QJsonObject &object,
    const QString &key,
    const QString &path,
    ValidationErrors &errors,
    const qsizetype maximumLength = maximumStringLength
)
{
    const auto valuePath = path + QLatin1Char('.') + key;
    const auto value = object.value(key);
    if (!value.isString()) {
        addError(
            errors,
            valuePath,
            QStringLiteral("catalog.string-required"),
            QStringLiteral("A string value is required.")
        );
        return {};
    }
    const auto text = value.toString();
    if (text.isEmpty() || text.size() > maximumLength
        || text != text.trimmed()
        || text != text.normalized(QString::NormalizationForm_C)
        || hasDisallowedCharacter(text)) {
        addError(
            errors,
            valuePath,
            QStringLiteral("catalog.invalid-string"),
            QStringLiteral("The string is empty, non-canonical, too long, or contains a disallowed character.")
        );
        return {};
    }
    return text;
}

[[nodiscard]] std::optional<quint32> readUnsigned(
    const QJsonObject &object,
    const QString &key,
    const QString &path,
    const quint32 maximum,
    ValidationErrors &errors
)
{
    const auto valuePath = path + QLatin1Char('.') + key;
    const auto value = object.value(key);
    if (!value.isDouble()) {
        addError(
            errors,
            valuePath,
            QStringLiteral("catalog.integer-required"),
            QStringLiteral("An unsigned integer is required.")
        );
        return std::nullopt;
    }
    const auto number = value.toDouble();
    if (!std::isfinite(number) || std::floor(number) != number || number < 0
        || number > maximum) {
        addError(
            errors,
            valuePath,
            QStringLiteral("catalog.integer-out-of-range"),
            QStringLiteral("The integer is outside the supported range.")
        );
        return std::nullopt;
    }
    return static_cast<quint32>(number);
}

[[nodiscard]] std::optional<double> readFiniteNumber(
    const QJsonObject &object,
    const QString &key,
    const QString &path,
    ValidationErrors &errors
)
{
    const auto valuePath = path + QLatin1Char('.') + key;
    const auto value = object.value(key);
    if (!value.isDouble() || !std::isfinite(value.toDouble())) {
        addError(
            errors,
            valuePath,
            QStringLiteral("catalog.number-required"),
            QStringLiteral("A finite number is required.")
        );
        return std::nullopt;
    }
    return value.toDouble();
}

[[nodiscard]] QJsonObject readObject(
    const QJsonObject &parent,
    const QString &key,
    const QString &path,
    ValidationErrors &errors
)
{
    const auto value = parent.value(key);
    if (!value.isObject()) {
        addError(
            errors,
            path + QLatin1Char('.') + key,
            QStringLiteral("catalog.object-required"),
            QStringLiteral("An object is required.")
        );
        return {};
    }
    return value.toObject();
}

[[nodiscard]] QJsonArray readArray(
    const QJsonObject &parent,
    const QString &key,
    const QString &path,
    ValidationErrors &errors
)
{
    const auto value = parent.value(key);
    if (!value.isArray()) {
        addError(
            errors,
            path + QLatin1Char('.') + key,
            QStringLiteral("catalog.array-required"),
            QStringLiteral("An array is required.")
        );
        return {};
    }
    return value.toArray();
}

[[nodiscard]] bool isOptionId(const QString &value)
{
    static const QRegularExpression expression(QStringLiteral(
        "^hyprland\\.[a-z][a-z0-9_-]*(?:\\.[a-z][a-z0-9_-]*)+$"
    ));
    return value.size() <= 255 && expression.match(value).hasMatch();
}

[[nodiscard]] bool isSurfaceToken(const QString &value)
{
    static const QRegularExpression expression(
        QStringLiteral("^[a-z][A-Za-z0-9]{0,63}$")
    );
    return expression.match(value).hasMatch();
}

[[nodiscard]] bool isHyprlandPath(const QString &value)
{
    static const QRegularExpression expression(QStringLiteral(
        "^[a-z][a-z0-9_-]*(?::[a-z][a-z0-9_-]*(?:\\.[a-z][a-z0-9_-]*)*)+$"
    ));
    return value.size() <= 255 && expression.match(value).hasMatch();
}

[[nodiscard]] bool isOfficialDocumentationUrl(const QString &text)
{
    const QUrl url(text, QUrl::StrictMode);
    return url.isValid() && url.scheme() == QStringLiteral("https")
        && url.host() == QStringLiteral("wiki.hypr.land")
        && url.path().startsWith(QStringLiteral("/0.56.0/"))
        && url.userInfo().isEmpty();
}

struct ExpectedSurface final {
    QString kind;
    QString module;
    QStringList luaPath;
    QString schemaReference;
};

[[nodiscard]] const QMap<QString, ExpectedSurface> &expectedSurfaces()
{
    static const QMap<QString, ExpectedSurface> values{
        {QStringLiteral("animations"), {QStringLiteral("animation"), QStringLiteral("animations"), {QStringLiteral("animation")}, QStringLiteral("config.schema.json#/$defs/animation")}},
        {QStringLiteral("bindings"), {QStringLiteral("binding"), QStringLiteral("bindings"), {QStringLiteral("bind")}, QStringLiteral("config.schema.json#/$defs/binding")}},
        {QStringLiteral("curves"), {QStringLiteral("curve"), QStringLiteral("animations"), {QStringLiteral("curve")}, QStringLiteral("config.schema.json#/$defs/curve")}},
        {QStringLiteral("devices"), {QStringLiteral("device"), QStringLiteral("input"), {QStringLiteral("device")}, QStringLiteral("config.schema.json#/$defs/device")}},
        {QStringLiteral("environment"), {QStringLiteral("environmentVariable"), QStringLiteral("environment"), {QStringLiteral("env")}, QStringLiteral("config.schema.json#/$defs/environmentVariable")}},
        {QStringLiteral("gestures"), {QStringLiteral("gesture"), QStringLiteral("input"), {QStringLiteral("gesture")}, QStringLiteral("config.schema.json#/$defs/gesture")}},
        {QStringLiteral("layerRules"), {QStringLiteral("layerRule"), QStringLiteral("rules"), {QStringLiteral("layer_rule")}, QStringLiteral("config.schema.json#/$defs/layerRule")}},
        {QStringLiteral("monitors"), {QStringLiteral("monitor"), QStringLiteral("monitors"), {QStringLiteral("monitor")}, QStringLiteral("config.schema.json#/$defs/monitor")}},
        {QStringLiteral("permissions"), {QStringLiteral("permission"), QStringLiteral("permissions"), {QStringLiteral("permission")}, QStringLiteral("config.schema.json#/$defs/permission")}},
        {QStringLiteral("submaps"), {QStringLiteral("submap"), QStringLiteral("bindings"), {QStringLiteral("define_submap")}, QStringLiteral("config.schema.json#/$defs/submap")}},
        {QStringLiteral("windowRules"), {QStringLiteral("windowRule"), QStringLiteral("rules"), {QStringLiteral("window_rule")}, QStringLiteral("config.schema.json#/$defs/windowRule")}},
        {QStringLiteral("workspaceRules"), {QStringLiteral("workspaceRule"), QStringLiteral("workspaces"), {QStringLiteral("workspace_rule")}, QStringLiteral("config.schema.json#/$defs/workspaceRule")}},
    };
    return values;
}

[[nodiscard]] const QMap<QString, QString> &expectedInheritedDefaults()
{
    static const QMap<QString, QString> values{
        {QStringLiteral("decoration:glow:color_inactive"), QStringLiteral("decoration:glow:color")},
        {QStringLiteral("decoration:shadow:color_inactive"), QStringLiteral("decoration:shadow:color")},
        {QStringLiteral("group:groupbar:text_color_inactive"), QStringLiteral("group:groupbar:text_color")},
        {QStringLiteral("group:groupbar:text_color_locked_active"), QStringLiteral("group:groupbar:text_color")},
        {QStringLiteral("group:groupbar:text_color_locked_inactive"), QStringLiteral("group:groupbar:text_color_inactive")},
    };
    return values;
}

[[nodiscard]] bool isIntegral(const QJsonValue &value)
{
    return value.isDouble() && std::isfinite(value.toDouble())
        && std::floor(value.toDouble()) == value.toDouble()
        && std::abs(value.toDouble()) <= 9007199254740991.0;
}

[[nodiscard]] bool isBoundedOptionString(const QJsonValue &value)
{
    return value.isString() && value.toString().size() <= 4096
        && !value.toString().contains(QChar::Null);
}

[[nodiscard]] std::optional<OptionType> optionTypeFromString(
    const QString &value
)
{
    if (value == QStringLiteral("boolean")) {
        return OptionType::Boolean;
    }
    if (value == QStringLiteral("integer")) {
        return OptionType::Integer;
    }
    if (value == QStringLiteral("number")) {
        return OptionType::Number;
    }
    if (value == QStringLiteral("string")) {
        return OptionType::String;
    }
    if (value == QStringLiteral("color")) {
        return OptionType::Color;
    }
    if (value == QStringLiteral("gradient")) {
        return OptionType::Gradient;
    }
    if (value == QStringLiteral("vector2")) {
        return OptionType::Vector2;
    }
    if (value == QStringLiteral("enum")) {
        return OptionType::Enumeration;
    }
    if (value == QStringLiteral("cssGap")) {
        return OptionType::CssGap;
    }
    if (value == QStringLiteral("fontWeight")) {
        return OptionType::FontWeight;
    }
    return std::nullopt;
}

[[nodiscard]] std::optional<DefaultPolicy> defaultPolicyFromString(
    const QString &value
)
{
    if (value == QStringLiteral("hyprland")) {
        return DefaultPolicy::Hyprland;
    }
    if (value == QStringLiteral("hyprshelld")) {
        return DefaultPolicy::HyprShelld;
    }
    return std::nullopt;
}

[[nodiscard]] std::optional<UiTier> uiTierFromString(const QString &value)
{
    if (value == QStringLiteral("common")) {
        return UiTier::Common;
    }
    if (value == QStringLiteral("advanced")) {
        return UiTier::Advanced;
    }
    if (value == QStringLiteral("expert")) {
        return UiTier::Expert;
    }
    if (value == QStringLiteral("external")) {
        return UiTier::External;
    }
    return std::nullopt;
}

[[nodiscard]] std::optional<ControlKind> controlFromString(
    const QString &value
)
{
    if (value == QStringLiteral("toggle")) {
        return ControlKind::Toggle;
    }
    if (value == QStringLiteral("spinBox")) {
        return ControlKind::SpinBox;
    }
    if (value == QStringLiteral("slider")) {
        return ControlKind::Slider;
    }
    if (value == QStringLiteral("text")) {
        return ControlKind::Text;
    }
    if (value == QStringLiteral("color")) {
        return ControlKind::Color;
    }
    if (value == QStringLiteral("gradient")) {
        return ControlKind::Gradient;
    }
    if (value == QStringLiteral("vector2")) {
        return ControlKind::Vector2;
    }
    if (value == QStringLiteral("select")) {
        return ControlKind::Select;
    }
    return std::nullopt;
}

[[nodiscard]] std::optional<ApplyMode> applyModeFromString(
    const QString &value
)
{
    if (value == QStringLiteral("reload")) {
        return ApplyMode::Reload;
    }
    if (value == QStringLiteral("restart")) {
        return ApplyMode::Restart;
    }
    if (value == QStringLiteral("session")) {
        return ApplyMode::Session;
    }
    return std::nullopt;
}

[[nodiscard]] std::optional<RiskLevel> riskFromString(const QString &value)
{
    if (value == QStringLiteral("safe")) {
        return RiskLevel::Safe;
    }
    if (value == QStringLiteral("caution")) {
        return RiskLevel::Caution;
    }
    if (value == QStringLiteral("dangerous")) {
        return RiskLevel::Dangerous;
    }
    return std::nullopt;
}

template<typename Enum, typename Converter>
[[nodiscard]] std::optional<Enum> readEnum(
    const QJsonObject &object,
    const QString &key,
    const QString &path,
    const QString &code,
    ValidationErrors &errors,
    Converter converter
)
{
    const auto text = readString(object, key, path, errors, 64);
    if (text.isEmpty()) {
        return std::nullopt;
    }
    const auto converted = converter(text);
    if (!converted.has_value()) {
        addError(
            errors,
            path + QLatin1Char('.') + key,
            code,
            QStringLiteral("The enum value is not part of this contract.")
        );
    }
    return converted;
}

[[nodiscard]] bool isCanonicalColor(const QJsonValue &value)
{
    static const QRegularExpression expression(
        QStringLiteral("^0x[0-9A-F]{8}$")
    );
    return value.isString() && expression.match(value.toString()).hasMatch();
}

[[nodiscard]] bool valueHasOptionType(
    const QJsonValue &value,
    const OptionType type,
    const QString &path,
    ValidationErrors &errors
)
{
    bool valid = false;
    switch (type) {
    case OptionType::Boolean:
        valid = value.isBool();
        break;
    case OptionType::Integer:
        valid = isIntegral(value);
        break;
    case OptionType::Number:
        valid = value.isDouble() && std::isfinite(value.toDouble());
        break;
    case OptionType::String:
        valid = isBoundedOptionString(value);
        break;
    case OptionType::Enumeration:
        valid = isBoundedOptionString(value) || isIntegral(value);
        break;
    case OptionType::Color:
        valid = isCanonicalColor(value);
        break;
    case OptionType::Vector2: {
        if (value.isArray()) {
            const auto array = value.toArray();
            valid = array.size() == 2 && array.at(0).isDouble()
                && array.at(1).isDouble()
                && std::isfinite(array.at(0).toDouble())
                && std::isfinite(array.at(1).toDouble());
        }
        break;
    }
    case OptionType::CssGap: {
        if (value.isArray()) {
            const auto array = value.toArray();
            valid = array.size() == 4;
            for (const auto &part : array) {
                valid = valid && isIntegral(part);
            }
        }
        break;
    }
    case OptionType::FontWeight:
        valid = isIntegral(value);
        break;
    case OptionType::Gradient: {
        if (value.isObject()) {
            const auto gradient = value.toObject();
            rejectUnknownFields(
                gradient,
                {QStringLiteral("colors"), QStringLiteral("angle")},
                path,
                errors
            );
            const auto colors = gradient.value(QStringLiteral("colors"));
            const auto angle = gradient.value(QStringLiteral("angle"));
            valid = colors.isArray() && !colors.toArray().isEmpty()
                && colors.toArray().size() <= 10 && angle.isDouble()
                && std::isfinite(angle.toDouble()) && angle.toDouble() >= -3600
                && angle.toDouble() <= 3600;
            if (colors.isArray()) {
                for (const auto &color : colors.toArray()) {
                    valid = valid && isCanonicalColor(color);
                }
            }
        }
        break;
    }
    }
    if (!valid) {
        addError(
            errors,
            path,
            QStringLiteral("option.type-mismatch"),
            QStringLiteral("The option value does not match its declared type.")
        );
    }
    return valid;
}

[[nodiscard]] OptionConstraints parseConstraints(
    const QJsonObject &object,
    const QString &path,
    ValidationErrors &errors
)
{
    OptionConstraints result;
    rejectUnknownFields(
        object,
        {
            QStringLiteral("min"),
            QStringLiteral("max"),
            QStringLiteral("step"),
            QStringLiteral("choices"),
            QStringLiteral("maxLength"),
            QStringLiteral("pattern"),
        },
        path,
        errors
    );

    const auto readBound = [&object, &path, &errors](const QString &key)
        -> std::optional<QJsonValue> {
        const auto value = object.value(key);
        const auto valuePath = path + QLatin1Char('.') + key;
        if (value.isDouble() && std::isfinite(value.toDouble())) {
            return value;
        }
        if (value.isArray()) {
            const auto array = value.toArray();
            if (array.size() == 2 && array.at(0).isDouble()
                && array.at(1).isDouble()
                && std::isfinite(array.at(0).toDouble())
                && std::isfinite(array.at(1).toDouble())) {
                return value;
            }
        }
        addError(
            errors,
            valuePath,
            QStringLiteral("catalog.invalid-bound"),
            QStringLiteral("A finite scalar or two-component numeric bound is required.")
        );
        return std::nullopt;
    };
    if (object.contains(QStringLiteral("min"))) {
        result.minimum = readBound(QStringLiteral("min"));
    }
    if (object.contains(QStringLiteral("max"))) {
        result.maximum = readBound(QStringLiteral("max"));
    }
    if (object.contains(QStringLiteral("step"))) {
        result.step = readFiniteNumber(
            object, QStringLiteral("step"), path, errors
        );
    }
    if (result.minimum && result.maximum) {
        const auto minimum = *result.minimum;
        const auto maximum = *result.maximum;
        bool validRange = minimum.type() == maximum.type();
        if (minimum.isDouble() && maximum.isDouble()) {
            validRange = minimum.toDouble() <= maximum.toDouble();
        } else if (minimum.isArray() && maximum.isArray()) {
            validRange = minimum.toArray().at(0).toDouble()
                    <= maximum.toArray().at(0).toDouble()
                && minimum.toArray().at(1).toDouble()
                    <= maximum.toArray().at(1).toDouble();
        }
        if (!validRange) {
            addError(
                errors,
                path,
                QStringLiteral("catalog.invalid-range"),
                QStringLiteral("The minimum exceeds the maximum or has a different shape.")
            );
        }
    }
    if (result.step && *result.step <= 0) {
        addError(
            errors,
            path + QStringLiteral(".step"),
            QStringLiteral("catalog.invalid-step"),
            QStringLiteral("The step must be greater than zero.")
        );
    }

    if (object.contains(QStringLiteral("maxLength"))) {
        result.maximumLength = readUnsigned(
            object,
            QStringLiteral("maxLength"),
            path,
            65536,
            errors
        );
    }
    if (object.contains(QStringLiteral("pattern"))) {
        const auto pattern = readString(
            object, QStringLiteral("pattern"), path, errors, 1024
        );
        if (!pattern.isEmpty()) {
            const auto converted = qtCompatiblePattern(pattern);
            const QRegularExpression expression(converted);
            if (!expression.isValid()) {
                addError(
                    errors,
                    path + QStringLiteral(".pattern"),
                    QStringLiteral("catalog.invalid-pattern"),
                    QStringLiteral("The regular expression is invalid.")
                );
            } else {
                result.pattern = converted;
            }
        }
    }
    if (object.contains(QStringLiteral("choices"))) {
        const auto choicesValue = object.value(QStringLiteral("choices"));
        if (!choicesValue.isArray()) {
            addError(
                errors,
                path + QStringLiteral(".choices"),
                QStringLiteral("catalog.array-required"),
                QStringLiteral("Choices must be an array.")
            );
        } else if (choicesValue.toArray().isEmpty()
                   || choicesValue.toArray().size() > 256) {
            addError(
                errors,
                path + QStringLiteral(".choices"),
                QStringLiteral("catalog.collection-limit"),
                QStringLiteral("Choices must contain between one and 256 entries.")
            );
        } else {
            QSet<QByteArray> seenValues;
            const auto choices = choicesValue.toArray();
            for (qsizetype index = 0; index < choices.size(); ++index) {
                const auto choicePath = path + QStringLiteral(".choices[")
                    + QString::number(index) + QLatin1Char(']');
                if (!choices.at(index).isObject()) {
                    addError(
                        errors,
                        choicePath,
                        QStringLiteral("catalog.object-required"),
                        QStringLiteral("A choice object is required.")
                    );
                    continue;
                }
                const auto choice = choices.at(index).toObject();
                rejectUnknownFields(
                    choice,
                    {QStringLiteral("label"), QStringLiteral("value")},
                    choicePath,
                    errors
                );
                const auto choiceLabel = readString(
                    choice,
                    QStringLiteral("label"),
                    choicePath,
                    errors,
                    128
                );
                Q_UNUSED(choiceLabel);
                const auto selectedValue = choice.value(QStringLiteral("value"));
                if (!(selectedValue.isString() || isIntegral(selectedValue))) {
                    addError(
                        errors,
                        choicePath + QStringLiteral(".value"),
                        QStringLiteral("catalog.invalid-choice"),
                        QStringLiteral("A choice value must be a string or integer.")
                    );
                    continue;
                }
                const auto encoded = JsonSupport::canonicalJson(selectedValue);
                if (seenValues.contains(encoded)) {
                    addError(
                        errors,
                        choicePath + QStringLiteral(".value"),
                        QStringLiteral("catalog.duplicate-choice"),
                        QStringLiteral("Choice values must be unique.")
                    );
                }
                seenValues.insert(encoded);
                result.choices.append(choice);
            }
        }
    }
    return result;
}

[[nodiscard]] bool constraintAllowsValue(
    const OptionDefinition &option,
    const QJsonValue &value
)
{
    const auto componentBound = [](const QJsonValue &bound, const qsizetype index)
        -> std::optional<double> {
        if (bound.isDouble()) {
            return bound.toDouble();
        }
        if (bound.isArray() && index < bound.toArray().size()) {
            return bound.toArray().at(index).toDouble();
        }
        return std::nullopt;
    };
    const auto componentAllowed = [&option, &componentBound](
        const double number,
        const qsizetype index
    ) {
        if (option.constraints.minimum) {
            const auto minimum = componentBound(*option.constraints.minimum, index);
            if (minimum && number < *minimum) {
                return false;
            }
        }
        if (option.constraints.maximum) {
            const auto maximum = componentBound(*option.constraints.maximum, index);
            if (maximum && number > *maximum) {
                return false;
            }
        }
        return true;
    };
    if (value.isDouble() && !componentAllowed(value.toDouble(), 0)) {
        return false;
    }
    if (value.isArray()) {
        const auto values = value.toArray();
        for (qsizetype index = 0; index < values.size(); ++index) {
            if (values.at(index).isDouble()
                && !componentAllowed(values.at(index).toDouble(), index)) {
                return false;
            }
        }
    }
    if (value.isString()) {
        const auto text = value.toString();
        if (option.constraints.maximumLength
            && text.size() > *option.constraints.maximumLength) {
            return false;
        }
        if (option.constraints.pattern
            && !QRegularExpression(*option.constraints.pattern)
                    .match(text)
                    .hasMatch()) {
            return false;
        }
    }
    if (!option.constraints.choices.isEmpty()) {
        const auto expected = JsonSupport::canonicalJson(value);
        const auto found = std::ranges::any_of(
            option.constraints.choices,
            [&expected](const QJsonValue &choice) {
                return JsonSupport::canonicalJson(
                           choice.toObject().value(QStringLiteral("value"))
                       ) == expected;
            }
        );
        if (!found) {
            return false;
        }
    }
    return true;
}

[[nodiscard]] std::optional<OptionDefinition> parseOption(
    const QJsonObject &object,
    const QString &path,
    ValidationErrors &errors
)
{
    rejectUnknownFields(
        object,
        {
            QStringLiteral("id"),
            QStringLiteral("path"),
            QStringLiteral("luaPath"),
            QStringLiteral("module"),
            QStringLiteral("type"),
            QStringLiteral("defaultPolicy"),
            QStringLiteral("writable"),
            QStringLiteral("default"),
            QStringLiteral("uiTier"),
            QStringLiteral("control"),
            QStringLiteral("constraints"),
            QStringLiteral("applyMode"),
            QStringLiteral("risk"),
            QStringLiteral("since"),
            QStringLiteral("until"),
            QStringLiteral("description"),
            QStringLiteral("documentation"),
        },
        path,
        errors
    );

    OptionDefinition option;
    option.id = readString(object, QStringLiteral("id"), path, errors, 255);
    if (!option.id.isEmpty() && !isOptionId(option.id)) {
        addError(
            errors,
            path + QStringLiteral(".id"),
            QStringLiteral("catalog.invalid-id"),
            QStringLiteral("The option ID is not a stable identifier.")
        );
    }
    option.path = readString(
        object, QStringLiteral("path"), path, errors, 255
    );
    if (!option.path.isEmpty() && !isHyprlandPath(option.path)) {
        addError(
            errors,
            path + QStringLiteral(".path"),
            QStringLiteral("catalog.invalid-path"),
            QStringLiteral("The option path is not a Hyprland scalar path.")
        );
    }
    const auto luaPath = readArray(
        object, QStringLiteral("luaPath"), path, errors
    );
    static const QRegularExpression luaIdentifier(
        QStringLiteral("^[A-Za-z_][A-Za-z0-9_]*$")
    );
    if (luaPath.size() < 2 || luaPath.size() > 8) {
        addError(
            errors,
            path + QStringLiteral(".luaPath"),
            QStringLiteral("catalog.invalid-lua-path"),
            QStringLiteral("The Lua path must contain two to eight identifier segments.")
        );
    }
    for (qsizetype index = 0; index < luaPath.size() && index < 8; ++index) {
        const auto segmentPath = path + QStringLiteral(".luaPath[")
            + QString::number(index) + QLatin1Char(']');
        if (!luaPath.at(index).isString()
            || luaPath.at(index).toString().size() > 64
            || !luaIdentifier.match(luaPath.at(index).toString()).hasMatch()) {
            addError(
                errors,
                segmentPath,
                QStringLiteral("catalog.invalid-lua-identifier"),
                QStringLiteral("A canonical Lua identifier is required.")
            );
            continue;
        }
        option.luaPath.append(luaPath.at(index).toString());
    }
    option.module = readString(
        object, QStringLiteral("module"), path, errors, 64
    );
    static const QRegularExpression moduleName(
        QStringLiteral("^[a-z][a-z0-9_]{0,63}$")
    );
    if (!option.module.isEmpty()
        && !moduleName.match(option.module).hasMatch()) {
        addError(
            errors,
            path + QStringLiteral(".module"),
            QStringLiteral("catalog.invalid-module"),
            QStringLiteral("The managed destination module is invalid.")
        );
    }

    const auto type = readEnum<OptionType>(
        object,
        QStringLiteral("type"),
        path,
        QStringLiteral("catalog.invalid-option-type"),
        errors,
        optionTypeFromString
    );
    if (type) {
        option.type = *type;
    }
    const auto defaultPolicy = readEnum<DefaultPolicy>(
        object,
        QStringLiteral("defaultPolicy"),
        path,
        QStringLiteral("catalog.invalid-default-policy"),
        errors,
        defaultPolicyFromString
    );
    if (defaultPolicy) {
        option.defaultPolicy = *defaultPolicy;
    }
    const auto writable = object.value(QStringLiteral("writable"));
    if (!writable.isBool()) {
        addError(
            errors,
            path + QStringLiteral(".writable"),
            QStringLiteral("catalog.boolean-required"),
            QStringLiteral("Every scalar option must explicitly declare whether managed state may override it.")
        );
    } else {
        option.writable = writable.toBool();
    }
    if (!object.contains(QStringLiteral("default"))) {
        addError(
            errors,
            path + QStringLiteral(".default"),
            QStringLiteral("catalog.default-required"),
            QStringLiteral("Every option must declare its exact default.")
        );
    } else {
        option.defaultValue = object.value(QStringLiteral("default"));
        const auto defaultObject = option.defaultValue.isObject()
            ? option.defaultValue.toObject() : QJsonObject{};
        const auto inheritanceAuthored = defaultObject.contains(
                                             QStringLiteral("kind")
                                         )
            || defaultObject.contains(QStringLiteral("from"));
        if (inheritanceAuthored) {
            const auto defaultPath = path + QStringLiteral(".default");
            rejectUnknownFields(
                defaultObject,
                {QStringLiteral("kind"), QStringLiteral("from")},
                defaultPath,
                errors
            );
            const auto kind = readString(
                defaultObject,
                QStringLiteral("kind"),
                defaultPath,
                errors,
                16
            );
            const auto from = readString(
                defaultObject,
                QStringLiteral("from"),
                defaultPath,
                errors,
                255
            );
            if (kind != QStringLiteral("inherit")) {
                addError(errors, defaultPath + QStringLiteral(".kind"), QStringLiteral("catalog.invalid-inherited-default"), QStringLiteral("Inherited defaults require the exact 'inherit' discriminator."));
            }
            if (!from.isEmpty() && !isHyprlandPath(from)) {
                addError(errors, defaultPath + QStringLiteral(".from"), QStringLiteral("catalog.invalid-inherited-default"), QStringLiteral("The inherited source must be a canonical upstream option path."));
            }
            if (type && *type != OptionType::Color
                && *type != OptionType::Gradient) {
                addError(errors, defaultPath, QStringLiteral("catalog.invalid-inherited-default"), QStringLiteral("Only color and gradient defaults may inherit another upstream option."));
            }
            if (defaultPolicy && *defaultPolicy != DefaultPolicy::Hyprland) {
                addError(errors, defaultPath, QStringLiteral("catalog.invalid-inherited-default"), QStringLiteral("Inherited defaults must retain Hyprland's default policy."));
            }
            if (kind == QStringLiteral("inherit") && !from.isEmpty()) {
                option.inheritedDefaultFrom = from;
            }
        } else if (type) {
            const auto defaultTypeMatches = valueHasOptionType(
                option.defaultValue,
                option.type,
                path + QStringLiteral(".default"),
                errors
            );
            if (!defaultTypeMatches) {
                addError(
                    errors,
                    path + QStringLiteral(".default"),
                    QStringLiteral("catalog.default-type"),
                    QStringLiteral("The option default does not match its declared type.")
                );
            }
            Q_UNUSED(defaultTypeMatches);
        }
    }

    const auto uiTier = readEnum<UiTier>(
        object,
        QStringLiteral("uiTier"),
        path,
        QStringLiteral("catalog.invalid-ui-tier"),
        errors,
        uiTierFromString
    );
    if (uiTier) {
        option.uiTier = *uiTier;
    }
    const auto control = readEnum<ControlKind>(
        object,
        QStringLiteral("control"),
        path,
        QStringLiteral("catalog.invalid-control"),
        errors,
        controlFromString
    );
    if (control) {
        option.control = *control;
    }
    option.constraints = parseConstraints(
        readObject(object, QStringLiteral("constraints"), path, errors),
        path + QStringLiteral(".constraints"),
        errors
    );

    const auto applyMode = readEnum<ApplyMode>(
        object,
        QStringLiteral("applyMode"),
        path,
        QStringLiteral("catalog.invalid-apply-mode"),
        errors,
        applyModeFromString
    );
    if (applyMode) {
        option.applyMode = *applyMode;
    }
    const auto risk = readEnum<RiskLevel>(
        object,
        QStringLiteral("risk"),
        path,
        QStringLiteral("catalog.invalid-risk"),
        errors,
        riskFromString
    );
    if (risk) {
        option.risk = *risk;
    }

    const auto sinceText = readString(
        object, QStringLiteral("since"), path, errors, 32
    );
    if (const auto since = semanticVersionFromString(sinceText)) {
        option.since = *since;
    } else if (!sinceText.isEmpty()) {
        addError(
            errors,
            path + QStringLiteral(".since"),
            QStringLiteral("catalog.invalid-version"),
            QStringLiteral("A strict major.minor.patch version is required.")
        );
    }
    if (object.contains(QStringLiteral("until"))) {
        const auto untilText = readString(
            object, QStringLiteral("until"), path, errors, 32
        );
        if (const auto until = semanticVersionFromString(untilText)) {
            option.until = *until;
            if (*until < option.since) {
                addError(
                    errors,
                    path + QStringLiteral(".until"),
                    QStringLiteral("catalog.invalid-version-range"),
                    QStringLiteral("The end version precedes the start version.")
                );
            }
        } else if (!untilText.isEmpty()) {
            addError(
                errors,
                path + QStringLiteral(".until"),
                QStringLiteral("catalog.invalid-version"),
                QStringLiteral("A strict major.minor.patch version is required.")
            );
        }
    }
    option.description = readString(
        object, QStringLiteral("description"), path, errors, 4096
    );
    option.documentation = readString(
        object, QStringLiteral("documentation"), path, errors, 2048
    );
    if (!option.documentation.isEmpty()) {
        if (!isOfficialDocumentationUrl(option.documentation)) {
            addError(
                errors,
                path + QStringLiteral(".documentation"),
                QStringLiteral("catalog.invalid-documentation-url"),
                QStringLiteral("Documentation must be an official absolute Hyprland wiki URL.")
            );
        }
    }

    if (type && !option.inheritedDefaultFrom
        && !constraintAllowsValue(option, option.defaultValue)) {
        addError(
            errors,
            path + QStringLiteral(".default"),
            QStringLiteral("catalog.default-out-of-range"),
            QStringLiteral("The default violates the declared constraints.")
        );
    }
    if (type && *type == OptionType::Enumeration
        && option.constraints.choices.isEmpty()) {
        addError(
            errors,
            path + QStringLiteral(".constraints.choices"),
            QStringLiteral("catalog.enum-choices-required"),
            QStringLiteral("An enum option must declare at least one choice.")
        );
    }
    if (type && control) {
        const auto incompatible =
            (*type == OptionType::Boolean
                && *control != ControlKind::Toggle
                && *control != ControlKind::Select)
            || (*type == OptionType::Enumeration
                && *control != ControlKind::Select)
            || (*type == OptionType::Color
                && *control != ControlKind::Color
                && *control != ControlKind::Text)
            || (*type == OptionType::Gradient
                && *control != ControlKind::Gradient)
            || (*type == OptionType::Vector2
                && *control != ControlKind::Vector2);
        if (incompatible) {
            addError(
                errors,
                path + QStringLiteral(".control"),
                QStringLiteral("catalog.incompatible-control"),
                QStringLiteral("The control cannot edit the declared option type.")
            );
        }
    }

    return option;
}

[[nodiscard]] std::optional<ComplexSurfaceDefinition> parseSurface(
    const QJsonObject &object,
    const QString &path,
    ValidationErrors &errors
)
{
    rejectUnknownFields(
        object,
        {
            QStringLiteral("id"),
            QStringLiteral("kind"),
            QStringLiteral("module"),
            QStringLiteral("luaPath"),
            QStringLiteral("ordered"),
            QStringLiteral("identityField"),
            QStringLiteral("applyMode"),
            QStringLiteral("risk"),
            QStringLiteral("description"),
            QStringLiteral("schemaRef"),
            QStringLiteral("documentation"),
        },
        path,
        errors
    );
    ComplexSurfaceDefinition surface;
    surface.id = readString(object, QStringLiteral("id"), path, errors, 64);
    if (!surface.id.isEmpty() && !isSurfaceToken(surface.id)) {
        addError(
            errors,
            path + QStringLiteral(".id"),
            QStringLiteral("catalog.invalid-id"),
            QStringLiteral("The surface ID is not stable.")
        );
    }
    surface.kind = readString(
        object, QStringLiteral("kind"), path, errors, 64
    );
    if (!isSurfaceToken(surface.kind)) {
        addError(
            errors,
            path + QStringLiteral(".kind"),
            QStringLiteral("catalog.invalid-kind"),
            QStringLiteral("The surface kind is invalid.")
        );
    }
    surface.module = readString(
        object, QStringLiteral("module"), path, errors, 64
    );
    static const QRegularExpression moduleName(
        QStringLiteral("^[a-z][a-z0-9_]{0,63}$")
    );
    if (!surface.module.isEmpty()
        && !moduleName.match(surface.module).hasMatch()) {
        addError(errors, path + QStringLiteral(".module"), QStringLiteral("catalog.invalid-module"), QStringLiteral("The managed destination module is invalid."));
    }
    const auto luaPath = readArray(
        object, QStringLiteral("luaPath"), path, errors
    );
    static const QRegularExpression luaIdentifier(
        QStringLiteral("^[A-Za-z_][A-Za-z0-9_]*$")
    );
    if (luaPath.isEmpty() || luaPath.size() > 4) {
        addError(errors, path + QStringLiteral(".luaPath"), QStringLiteral("catalog.invalid-lua-path"), QStringLiteral("A one-to-four segment Lua call path is required."));
    }
    for (qsizetype index = 0; index < luaPath.size() && index < 4; ++index) {
        if (!luaPath.at(index).isString()
            || luaPath.at(index).toString().size() > 64
            || !luaIdentifier.match(luaPath.at(index).toString()).hasMatch()) {
            addError(errors, path + QStringLiteral(".luaPath[") + QString::number(index) + QLatin1Char(']'), QStringLiteral("catalog.invalid-lua-identifier"), QStringLiteral("A canonical Lua identifier is required."));
        } else {
            surface.luaPath.append(luaPath.at(index).toString());
        }
    }
    const auto orderedValue = object.value(QStringLiteral("ordered"));
    if (!orderedValue.isBool()) {
        addError(
            errors,
            path + QStringLiteral(".ordered"),
            QStringLiteral("catalog.boolean-required"),
            QStringLiteral("A boolean is required.")
        );
    } else {
        surface.ordered = orderedValue.toBool();
        if (!surface.ordered) {
            addError(
                errors,
                path + QStringLiteral(".ordered"),
                QStringLiteral("catalog.ordered-surface-required"),
                QStringLiteral("Slice 1 complex surfaces must preserve order.")
            );
        }
    }
    surface.identityField = readString(
        object, QStringLiteral("identityField"), path, errors, 64
    );
    if (surface.identityField != QStringLiteral("id")) {
        addError(
            errors,
            path + QStringLiteral(".identityField"),
            QStringLiteral("catalog.unsupported-identity"),
            QStringLiteral("The stable identity field must be id.")
        );
    }
    if (const auto apply = readEnum<ApplyMode>(
            object,
            QStringLiteral("applyMode"),
            path,
            QStringLiteral("catalog.invalid-apply-mode"),
            errors,
            applyModeFromString
        )) {
        surface.applyMode = *apply;
    }
    if (const auto risk = readEnum<RiskLevel>(
            object,
            QStringLiteral("risk"),
            path,
            QStringLiteral("catalog.invalid-risk"),
            errors,
            riskFromString
        )) {
        surface.risk = *risk;
    }
    surface.description = readString(
        object, QStringLiteral("description"), path, errors, 4096
    );
    surface.schemaReference = readString(
        object, QStringLiteral("schemaRef"), path, errors, 256
    );
    static const QRegularExpression schemaReference(
        QStringLiteral("^config\\.schema\\.json#/\\$defs/[A-Za-z][A-Za-z0-9]*$")
    );
    if (!schemaReference.match(surface.schemaReference).hasMatch()) {
        addError(
            errors,
            path + QStringLiteral(".schemaRef"),
            QStringLiteral("catalog.invalid-schema-reference"),
            QStringLiteral("A local config schema definition reference is required.")
        );
    }
    surface.documentation = readString(
        object, QStringLiteral("documentation"), path, errors, 2048
    );
    if (!surface.documentation.isEmpty()) {
        if (!isOfficialDocumentationUrl(surface.documentation)) {
            addError(
                errors,
                path + QStringLiteral(".documentation"),
                QStringLiteral("catalog.invalid-documentation-url"),
                QStringLiteral("Documentation must be an official absolute Hyprland wiki URL.")
            );
        }
    }
    return surface;
}

[[nodiscard]] HyprlandReleaseRange parseReleaseRange(
    const QJsonObject &object,
    const QString &path,
    ValidationErrors &errors,
    const CatalogContract &contract
)
{
    rejectUnknownFields(
        object,
        {
            QStringLiteral("major"),
            QStringLiteral("minor"),
            QStringLiteral("reviewedVersion"),
            QStringLiteral("reviewedTag"),
            QStringLiteral("reviewedCommit"),
            QStringLiteral("repository"),
            QStringLiteral("minimumPatch"),
            QStringLiteral("maximumPatch"),
        },
        path,
        errors
    );
    HyprlandReleaseRange result;
    if (const auto value = readUnsigned(
            object, QStringLiteral("major"), path, 65535, errors
        )) {
        result.major = *value;
        if (*value != 0) {
            addError(errors, path + QStringLiteral(".major"), QStringLiteral("catalog.invalid-release-range"), QStringLiteral("The authority is pinned to Hyprland major 0."));
        }
    }
    if (const auto value = readUnsigned(
            object, QStringLiteral("minor"), path, 65535, errors
        )) {
        result.minor = *value;
        if (*value != 56) {
            addError(errors, path + QStringLiteral(".minor"), QStringLiteral("catalog.invalid-release-range"), QStringLiteral("The authority is pinned to Hyprland minor 56."));
        }
    }
    const auto reviewedText = readString(
        object, QStringLiteral("reviewedVersion"), path, errors, 32
    );
    if (const auto reviewed = semanticVersionFromString(reviewedText)) {
        result.reviewedVersion = *reviewed;
    } else if (!reviewedText.isEmpty()) {
        addError(
            errors,
            path + QStringLiteral(".reviewedVersion"),
            QStringLiteral("catalog.invalid-version"),
            QStringLiteral("A strict reviewed major.minor.patch version is required.")
        );
    }
    if (!reviewedText.isEmpty()
        && reviewedText != QLatin1String(contract.reviewedVersion)) {
        addError(errors, path + QStringLiteral(".reviewedVersion"), QStringLiteral("catalog.invalid-version"), QStringLiteral("The catalog must use the exact reviewed Hyprland version."));
    }
    result.reviewedTag = readString(
        object, QStringLiteral("reviewedTag"), path, errors, 64
    );
    if (result.reviewedTag != QLatin1String(contract.reviewedTag)) {
        addError(errors, path + QStringLiteral(".reviewedTag"), QStringLiteral("catalog.invalid-reviewed-tag"), QStringLiteral("The catalog must use the exact reviewed tag."));
    }
    result.reviewedCommit = readString(
        object, QStringLiteral("reviewedCommit"), path, errors, 40
    );
    if (result.reviewedCommit != QLatin1String(contract.reviewedCommit)) {
        addError(errors, path + QStringLiteral(".reviewedCommit"), QStringLiteral("catalog.invalid-reviewed-commit"), QStringLiteral("The catalog must use the reviewed Hyprland commit."));
    }
    result.repository = readString(
        object, QStringLiteral("repository"), path, errors, 256
    );
    if (result.repository != QStringLiteral("https://github.com/hyprwm/Hyprland")) {
        addError(errors, path + QStringLiteral(".repository"), QStringLiteral("catalog.invalid-repository"), QStringLiteral("The pinned official Hyprland repository is required."));
    }
    if (const auto value = readUnsigned(
            object, QStringLiteral("minimumPatch"), path, 65535, errors
        )) {
        result.minimumPatch = *value;
        if (*value != contract.minimumPatch) {
            addError(errors, path + QStringLiteral(".minimumPatch"), QStringLiteral("catalog.invalid-release-range"), QStringLiteral("The catalog has the wrong exact minimum patch."));
        }
    }
    const auto maximum = object.value(QStringLiteral("maximumPatch"));
    if (!contract.maximumPatch) {
        if (maximum.isNull()) {
            result.maximumPatch = std::nullopt;
        } else if (const auto value = readUnsigned(
                       object,
                       QStringLiteral("maximumPatch"),
                       path,
                       65535,
                       errors
                   )) {
            result.maximumPatch = *value;
            addError(errors, path + QStringLiteral(".maximumPatch"), QStringLiteral("catalog.invalid-release-range"), QStringLiteral("This authority requires a null maximumPatch."));
        }
    } else if (const auto value = readUnsigned(
                   object,
                   QStringLiteral("maximumPatch"),
                   path,
                   65535,
                   errors
               )) {
        result.maximumPatch = *value;
        if (*value != *contract.maximumPatch) {
            addError(errors, path + QStringLiteral(".maximumPatch"), QStringLiteral("catalog.invalid-release-range"), QStringLiteral("The catalog has the wrong exact maximum patch."));
        }
    }
    if (result.maximumPatch
        && *result.maximumPatch < result.minimumPatch) {
        addError(
            errors,
            path + QStringLiteral(".maximumPatch"),
            QStringLiteral("catalog.invalid-version-range"),
            QStringLiteral("The maximum patch precedes the minimum patch.")
        );
    }
    if (result.reviewedVersion.major != result.major
        || result.reviewedVersion.minor != result.minor
        || result.reviewedVersion.patch < result.minimumPatch
        || (result.maximumPatch
            && result.reviewedVersion.patch > *result.maximumPatch)) {
        addError(
            errors,
            path + QStringLiteral(".reviewedVersion"),
            QStringLiteral("catalog.reviewed-version-out-of-range"),
            QStringLiteral("The reviewed source version must lie inside the supported release range.")
        );
    }
    return result;
}

[[nodiscard]] CompatibilityPolicy parseCompatibility(
    const QJsonObject &object,
    const QString &path,
    ValidationErrors &errors,
    const CatalogContract &contract
)
{
    rejectUnknownFields(
        object,
        {
            QStringLiteral("minimumSupported"),
            QStringLiteral("fullyQualified"),
            QStringLiteral("olderMinor"),
            QStringLiteral("newerMinor"),
            QStringLiteral("unknownMajor"),
        },
        path,
        errors
    );
    CompatibilityPolicy result;
    const auto minimum = readString(
        object, QStringLiteral("minimumSupported"), path, errors, 32
    );
    if (const auto version = semanticVersionFromString(minimum)) {
        result.minimumSupported = *version;
        if (minimum != QLatin1String(contract.minimumSupported)) {
            addError(errors, path + QStringLiteral(".minimumSupported"), QStringLiteral("catalog.invalid-compatibility-policy"), QStringLiteral("The authority has the wrong exact migration floor."));
        }
    } else if (!minimum.isEmpty()) {
        addError(
            errors,
            path + QStringLiteral(".minimumSupported"),
            QStringLiteral("catalog.invalid-version"),
            QStringLiteral("A strict major.minor.patch version is required.")
        );
    }

    const auto qualified = readArray(
        object, QStringLiteral("fullyQualified"), path, errors
    );
    if (qualified.isEmpty()) {
        addError(
            errors,
            path + QStringLiteral(".fullyQualified"),
            QStringLiteral("catalog.qualified-version-required"),
            QStringLiteral("At least one qualified minor is required.")
        );
    }
    if (qualified.size() > 32) {
        addError(
            errors,
            path + QStringLiteral(".fullyQualified"),
            QStringLiteral("catalog.collection-limit"),
            QStringLiteral("Too many qualified versions are declared.")
        );
    }
    static const QRegularExpression wildcardPattern(
        QStringLiteral("^(0|[1-9][0-9]*)\\.(0|[1-9][0-9]*)\\.x$")
    );
    QSet<QString> seen;
    for (qsizetype index = 0; index < qualified.size(); ++index) {
        const auto valuePath = path + QStringLiteral(".fullyQualified[")
            + QString::number(index) + QLatin1Char(']');
        const auto text = qualified.at(index).toString();
        const auto validPattern = contract.contractVersion
                == currentCatalogContractVersion
            ? wildcardPattern.match(text).hasMatch()
            : semanticVersionFromString(text).has_value();
        if (!qualified.at(index).isString() || !validPattern) {
            addError(
                errors,
                valuePath,
                QStringLiteral("catalog.invalid-version-pattern"),
                QStringLiteral("The authority requires its exact qualified-version syntax.")
            );
            continue;
        }
        const auto value = text;
        if (seen.contains(value)) {
            addError(
                errors,
                valuePath,
                QStringLiteral("catalog.duplicate-version-pattern"),
                QStringLiteral("Qualified version patterns must be unique.")
            );
        }
        seen.insert(value);
        result.fullyQualified.append(value);
    }
    if (result.fullyQualified
        != QStringList{QLatin1String(contract.fullyQualified)}) {
        addError(errors, path + QStringLiteral(".fullyQualified"), QStringLiteral("catalog.invalid-compatibility-policy"), QStringLiteral("Only the exact reviewed release policy is fully qualified."));
    }

    const auto older = readString(
        object, QStringLiteral("olderMinor"), path, errors, 32
    );
    if (older == QStringLiteral("migration")) {
        result.olderMinor = OlderMinorPolicy::Migration;
    } else if (!older.isEmpty()) {
        addError(
            errors,
            path + QStringLiteral(".olderMinor"),
            QStringLiteral("catalog.invalid-compatibility-policy"),
            QStringLiteral("The older-minor policy is unsupported.")
        );
    }
    const auto newer = readString(
        object, QStringLiteral("newerMinor"), path, errors, 32
    );
    if (newer == QStringLiteral("read-only")) {
        result.newerMinor = NewerMinorPolicy::ReadOnly;
    } else if (!newer.isEmpty()) {
        addError(
            errors,
            path + QStringLiteral(".newerMinor"),
            QStringLiteral("catalog.invalid-compatibility-policy"),
            QStringLiteral("The newer-minor policy is unsupported.")
        );
    }
    const auto major = readString(
        object, QStringLiteral("unknownMajor"), path, errors, 32
    );
    if (major != QStringLiteral("unsupported") && !major.isEmpty()) {
        addError(
            errors,
            path + QStringLiteral(".unknownMajor"),
            QStringLiteral("catalog.invalid-compatibility-policy"),
            QStringLiteral("Unknown major versions must be unsupported.")
        );
    }
    return result;
}

} // namespace

QString toString(const OptionType value)
{
    switch (value) {
    case OptionType::Boolean: return QStringLiteral("boolean");
    case OptionType::Integer: return QStringLiteral("integer");
    case OptionType::Number: return QStringLiteral("number");
    case OptionType::String: return QStringLiteral("string");
    case OptionType::Color: return QStringLiteral("color");
    case OptionType::Gradient: return QStringLiteral("gradient");
    case OptionType::Vector2: return QStringLiteral("vector2");
    case OptionType::Enumeration: return QStringLiteral("enum");
    case OptionType::CssGap: return QStringLiteral("cssGap");
    case OptionType::FontWeight: return QStringLiteral("fontWeight");
    }
    return {};
}

QString toString(const DefaultPolicy value)
{
    return value == DefaultPolicy::Hyprland ? QStringLiteral("hyprland")
                                            : QStringLiteral("hyprshelld");
}

QString toString(const UiTier value)
{
    switch (value) {
    case UiTier::Common: return QStringLiteral("common");
    case UiTier::Advanced: return QStringLiteral("advanced");
    case UiTier::Expert: return QStringLiteral("expert");
    case UiTier::External: return QStringLiteral("external");
    }
    return {};
}

QString toString(const ControlKind value)
{
    switch (value) {
    case ControlKind::Toggle: return QStringLiteral("toggle");
    case ControlKind::SpinBox: return QStringLiteral("spinBox");
    case ControlKind::Slider: return QStringLiteral("slider");
    case ControlKind::Text: return QStringLiteral("text");
    case ControlKind::Color: return QStringLiteral("color");
    case ControlKind::Gradient: return QStringLiteral("gradient");
    case ControlKind::Vector2: return QStringLiteral("vector2");
    case ControlKind::Select: return QStringLiteral("select");
    }
    return {};
}

QString toString(const ApplyMode value)
{
    switch (value) {
    case ApplyMode::Reload: return QStringLiteral("reload");
    case ApplyMode::Restart: return QStringLiteral("restart");
    case ApplyMode::Session: return QStringLiteral("session");
    }
    return {};
}

QString toString(const RiskLevel value)
{
    switch (value) {
    case RiskLevel::Safe: return QStringLiteral("safe");
    case RiskLevel::Caution: return QStringLiteral("caution");
    case RiskLevel::Dangerous: return QStringLiteral("dangerous");
    }
    return {};
}

QString toString(const SemanticVersion value)
{
    return QStringLiteral("%1.%2.%3")
        .arg(value.major)
        .arg(value.minor)
        .arg(value.patch);
}

std::optional<SemanticVersion> semanticVersionFromString(const QString &value)
{
    static const QRegularExpression expression(QStringLiteral(
        "^(0|[1-9][0-9]*)\\.(0|[1-9][0-9]*)\\.(0|[1-9][0-9]*)$"
    ));
    const auto match = expression.match(value);
    if (!match.hasMatch()) {
        return std::nullopt;
    }
    bool majorOk = false;
    bool minorOk = false;
    bool patchOk = false;
    const auto major = match.captured(1).toUInt(&majorOk);
    const auto minor = match.captured(2).toUInt(&minorOk);
    const auto patch = match.captured(3).toUInt(&patchOk);
    if (!majorOk || !minorOk || !patchOk) {
        return std::nullopt;
    }
    return SemanticVersion{major, minor, patch};
}

[[nodiscard]] static ValidationResult<Catalog> parseCatalogContract(
    const QByteArrayView bytes,
    const CatalogContract &contract
)
{
    ValidationResult<Catalog> result;
    const auto parsed = JsonSupport::parseStrictObject(
        bytes, maximumCatalogBytes, maximumCatalogDepth
    );
    if (!parsed) {
        result.errors = parsed.errors;
        return result;
    }
    const auto &root = *parsed.value;
    QSet<QString> rootFields{
        QStringLiteral("contractVersion"),
        QStringLiteral("hyprland"),
        QStringLiteral("options"),
        QStringLiteral("complexSurfaces"),
        QStringLiteral("compatibility"),
    };
    if (contract.requiresSourceManifestDigest) {
        rootFields.insert(QStringLiteral("sourceManifestDigest"));
    }
    rejectUnknownFields(
        root,
        rootFields,
        QStringLiteral("$"),
        result.errors
    );

    Catalog catalog;
    if (const auto version = readUnsigned(
            root,
            QStringLiteral("contractVersion"),
            QStringLiteral("$"),
            std::numeric_limits<quint32>::max(),
            result.errors
        )) {
        catalog.contractVersion = *version;
        if (*version != contract.contractVersion) {
            addError(
                result.errors,
                QStringLiteral("$.contractVersion"),
                QStringLiteral("catalog.unsupported-contract-version"),
                QStringLiteral("Only catalog contract v%1 is accepted by this parser.")
                    .arg(contract.contractVersion)
            );
        }
    }

    if (contract.requiresSourceManifestDigest) {
        catalog.sourceManifestDigest = readString(
            root,
            QStringLiteral("sourceManifestDigest"),
            QStringLiteral("$"),
            result.errors,
            64
        );
        if (!catalog.sourceManifestDigest.isEmpty()
            && !isSha256(catalog.sourceManifestDigest)) {
            addError(
                result.errors,
                QStringLiteral("$.sourceManifestDigest"),
                QStringLiteral("catalog.invalid-source-manifest-digest"),
                QStringLiteral("A lowercase SHA-256 source-manifest digest is required.")
            );
        } else if (catalog.sourceManifestDigest
            != QLatin1String(contract.sourceManifestDigest)) {
            addError(
                result.errors,
                QStringLiteral("$.sourceManifestDigest"),
                QStringLiteral("catalog.source-manifest-digest-mismatch"),
                QStringLiteral("The catalog is not bound to the exact reviewed source manifest.")
            );
        }
    }

    catalog.hyprland = parseReleaseRange(
        readObject(root, QStringLiteral("hyprland"), QStringLiteral("$"), result.errors),
        QStringLiteral("$.hyprland"),
        result.errors,
        contract
    );

    const auto options = readArray(
        root, QStringLiteral("options"), QStringLiteral("$"), result.errors
    );
    if (options.isEmpty() || options.size() > maximumCatalogOptions) {
        addError(
            result.errors,
            QStringLiteral("$.options"),
            QStringLiteral("catalog.collection-limit"),
            QStringLiteral("The catalog must contain between one and 1024 scalar options.")
        );
    }
    QSet<QString> optionIds;
    QSet<QString> optionPaths;
    QSet<QString> luaPaths;
    QString previousPath;
    for (qsizetype index = 0;
         index < std::min(options.size(), maximumCatalogOptions);
         ++index) {
        const auto path = QStringLiteral("$.options[") + QString::number(index)
            + QLatin1Char(']');
        if (!options.at(index).isObject()) {
            addError(
                result.errors,
                path,
                QStringLiteral("catalog.object-required"),
                QStringLiteral("An option object is required.")
            );
            continue;
        }
        auto option = parseOption(options.at(index).toObject(), path, result.errors);
        if (!option) {
            continue;
        }
        if (optionIds.contains(option->id)) {
            addError(
                result.errors,
                path + QStringLiteral(".id"),
                QStringLiteral("catalog.duplicate-id"),
                QStringLiteral("Option IDs must be unique.")
            );
        }
        if (optionPaths.contains(option->path)) {
            addError(
                result.errors,
                path + QStringLiteral(".path"),
                QStringLiteral("catalog.duplicate-path"),
                QStringLiteral("Option paths must be unique.")
            );
        }
        const auto luaPath = option->luaPath.join(QLatin1Char('.'));
        if (!luaPath.isEmpty() && luaPaths.contains(luaPath)) {
            addError(
                result.errors,
                path + QStringLiteral(".luaPath"),
                QStringLiteral("catalog.duplicate-lua-path"),
                QStringLiteral("Lua emission paths must be unique.")
            );
        }
        if (!previousPath.isEmpty() && option->path <= previousPath) {
            addError(
                result.errors,
                path + QStringLiteral(".path"),
                QStringLiteral("catalog.non-canonical-order"),
                QStringLiteral("Scalar options must be strictly ordered by upstream path.")
            );
        }
        optionIds.insert(option->id);
        optionPaths.insert(option->path);
        luaPaths.insert(luaPath);
        previousPath = option->path;
        catalog.options.append(std::move(*option));
    }

    QMap<QString, qsizetype> optionIndexByPath;
    for (qsizetype index = 0; index < catalog.options.size(); ++index) {
        optionIndexByPath.insert(catalog.options.at(index).path, index);
    }
    for (qsizetype index = 0; index < catalog.options.size(); ++index) {
        const auto &option = catalog.options.at(index);
        const auto defaultPath = QStringLiteral("$.options[")
            + QString::number(index) + QStringLiteral("].default");
        const auto expected = expectedInheritedDefaults().constFind(option.path);
        if (!option.inheritedDefaultFrom) {
            if (expected != expectedInheritedDefaults().constEnd()) {
                addError(result.errors, defaultPath, QStringLiteral("catalog.inherited-default-required"), QStringLiteral("The reviewed upstream option requires its explicit inherited-default sentinel."));
            }
            continue;
        }
        if (expected == expectedInheritedDefaults().constEnd()
            || *option.inheritedDefaultFrom != expected.value()) {
            addError(result.errors, defaultPath, QStringLiteral("catalog.unreviewed-inherited-default"), QStringLiteral("The inherited-default relationship is not part of the reviewed 0.56.1 authority."));
        }
        if (*option.inheritedDefaultFrom == option.path) {
            addError(result.errors, defaultPath + QStringLiteral(".from"), QStringLiteral("catalog.inherited-default-cycle"), QStringLiteral("An option cannot inherit its own default."));
            continue;
        }
        const auto sourceIndex = optionIndexByPath.constFind(
            *option.inheritedDefaultFrom
        );
        if (sourceIndex == optionIndexByPath.constEnd()) {
            addError(result.errors, defaultPath + QStringLiteral(".from"), QStringLiteral("catalog.inherited-default-missing-source"), QStringLiteral("The inherited default source does not exist in this catalog."));
            continue;
        }
        if (catalog.options.at(*sourceIndex).type != option.type) {
            addError(result.errors, defaultPath + QStringLiteral(".from"), QStringLiteral("catalog.inherited-default-type-mismatch"), QStringLiteral("Inherited defaults must reference an option of the same declared type."));
        }
    }
    for (auto expected = expectedInheritedDefaults().constBegin();
         expected != expectedInheritedDefaults().constEnd(); ++expected) {
        if (!optionIndexByPath.contains(expected.key())) {
            addError(result.errors, QStringLiteral("$.options"), QStringLiteral("catalog.inherited-default-required"), QStringLiteral("A reviewed inherited-default option is missing from the catalog."));
        }
    }
    for (qsizetype index = 0; index < catalog.options.size(); ++index) {
        const auto &option = catalog.options.at(index);
        if (!option.inheritedDefaultFrom) continue;
        QSet<QString> chain;
        auto currentPath = option.path;
        while (true) {
            if (chain.contains(currentPath)) {
                addError(
                    result.errors,
                    QStringLiteral("$.options[") + QString::number(index)
                        + QStringLiteral("].default.from"),
                    QStringLiteral("catalog.inherited-default-cycle"),
                    QStringLiteral("Inherited default relationships must be acyclic.")
                );
                break;
            }
            chain.insert(currentPath);
            const auto currentIndex = optionIndexByPath.constFind(currentPath);
            if (currentIndex == optionIndexByPath.constEnd()) break;
            const auto &current = catalog.options.at(*currentIndex);
            if (!current.inheritedDefaultFrom) break;
            currentPath = *current.inheritedDefaultFrom;
        }
    }

    const auto surfaces = readArray(
        root,
        QStringLiteral("complexSurfaces"),
        QStringLiteral("$"),
        result.errors
    );
    if (surfaces.size() != maximumComplexSurfaces) {
        addError(
            result.errors,
            QStringLiteral("$.complexSurfaces"),
            QStringLiteral("catalog.collection-limit"),
            QStringLiteral("The catalog must contain exactly 12 complex surfaces.")
        );
    }
    QSet<QString> surfaceIds;
    QString previousSurfaceId;
    for (qsizetype index = 0;
         index < std::min(surfaces.size(), maximumComplexSurfaces);
         ++index) {
        const auto path = QStringLiteral("$.complexSurfaces[")
            + QString::number(index) + QLatin1Char(']');
        if (!surfaces.at(index).isObject()) {
            addError(
                result.errors,
                path,
                QStringLiteral("catalog.object-required"),
                QStringLiteral("A complex surface object is required.")
            );
            continue;
        }
        auto surface = parseSurface(
            surfaces.at(index).toObject(), path, result.errors
        );
        if (!surface) {
            continue;
        }
        const auto expected = expectedSurfaces().constFind(surface->id);
        if (expected == expectedSurfaces().constEnd()) {
            addError(
                result.errors,
                path + QStringLiteral(".id"),
                QStringLiteral("catalog.unknown-surface"),
                QStringLiteral("The v1 catalog contains an unsupported complex surface.")
            );
        } else {
            if (surface->kind != expected->kind) {
                addError(result.errors, path + QStringLiteral(".kind"), QStringLiteral("catalog.surface-contract-mismatch"), QStringLiteral("The surface kind does not match its stable ID."));
            }
            if (surface->module != expected->module) {
                addError(result.errors, path + QStringLiteral(".module"), QStringLiteral("catalog.surface-contract-mismatch"), QStringLiteral("The surface module does not match its stable ID."));
            }
            if (surface->luaPath != expected->luaPath) {
                addError(result.errors, path + QStringLiteral(".luaPath"), QStringLiteral("catalog.surface-contract-mismatch"), QStringLiteral("The surface Lua destination does not match its stable ID."));
            }
            if (surface->schemaReference != expected->schemaReference) {
                addError(result.errors, path + QStringLiteral(".schemaRef"), QStringLiteral("catalog.surface-contract-mismatch"), QStringLiteral("The surface schema definition does not match its stable ID."));
            }
        }
        if (surfaceIds.contains(surface->id)) {
            addError(
                result.errors,
                path + QStringLiteral(".id"),
                QStringLiteral("catalog.duplicate-id"),
                QStringLiteral("Complex-surface IDs must be unique.")
            );
        }
        if (!previousSurfaceId.isEmpty()
            && surface->id <= previousSurfaceId) {
            addError(
                result.errors,
                path + QStringLiteral(".id"),
                QStringLiteral("catalog.non-canonical-order"),
                QStringLiteral("Complex surfaces must be strictly ordered by ID.")
            );
        }
        surfaceIds.insert(surface->id);
        previousSurfaceId = surface->id;
        catalog.complexSurfaces.append(std::move(*surface));
    }
    if (surfaceIds != QSet<QString>(expectedSurfaces().keyBegin(), expectedSurfaces().keyEnd())) {
        addError(
            result.errors,
            QStringLiteral("$.complexSurfaces"),
            QStringLiteral("catalog.surface-inventory-mismatch"),
            QStringLiteral("The v1 catalog must contain the exact reviewed complex-surface inventory.")
        );
    }

    catalog.compatibility = parseCompatibility(
        readObject(
            root,
            QStringLiteral("compatibility"),
            QStringLiteral("$"),
            result.errors
        ),
        QStringLiteral("$.compatibility"),
        result.errors,
        contract
    );

    if (!catalog.compatibility.fullyQualified.contains(
            QLatin1String(contract.fullyQualified)
        )) {
        addError(
            result.errors,
            QStringLiteral("$.compatibility.fullyQualified"),
            QStringLiteral("catalog.target-not-qualified"),
            QStringLiteral("The catalog's own target minor must be fully qualified.")
        );
    }

    catalog.canonicalDocument = root;
    catalog.digest = QString::fromLatin1(
        QCryptographicHash::hash(
            JsonSupport::canonicalJson(root),
            QCryptographicHash::Sha256
        ).toHex()
    );
    if (catalog.digest != QLatin1String(contract.integrityDigest)) {
        addError(
            result.errors,
            QStringLiteral("$"),
            QStringLiteral("catalog.integrity-mismatch"),
            QStringLiteral("The canonical catalog does not match the compiled reviewed v%1 authority.")
                .arg(contract.contractVersion)
        );
    }

    if (result.errors.isEmpty()) {
        result.value = std::move(catalog);
    }
    return result;
}

ValidationResult<Catalog> parseCatalog(const QByteArrayView bytes)
{
    return parseCatalogContract(bytes, activeCatalogContract());
}

ValidationResult<Catalog> parseDormantCatalogV2(const QByteArrayView bytes)
{
    return parseCatalogContract(bytes, dormantCatalogContractV2());
}

QByteArray canonicalCatalogJson(const Catalog &catalog)
{
    return JsonSupport::canonicalJson(catalog.canonicalDocument);
}

QString catalogDigest(const Catalog &catalog)
{
    if (!catalog.digest.isEmpty()) {
        return catalog.digest;
    }
    return QString::fromLatin1(
        QCryptographicHash::hash(
            canonicalCatalogJson(catalog), QCryptographicHash::Sha256
        ).toHex()
    );
}

CompatibilityDecision compatibilityForVersion(
    const Catalog &catalog,
    const SemanticVersion version
)
{
    if (version.major != catalog.hyprland.major) {
        return CompatibilityDecision::UnsupportedMajor;
    }
    if (version.minor == catalog.hyprland.minor) {
        if (version == catalog.hyprland.reviewedVersion) {
            return CompatibilityDecision::Exact;
        }
        if (version.patch >= catalog.hyprland.minimumPatch
            && (!catalog.hyprland.maximumPatch
                || version.patch <= *catalog.hyprland.maximumPatch)) {
            return CompatibilityDecision::SupportedMinor;
        }
        return version.patch < catalog.hyprland.minimumPatch
            ? CompatibilityDecision::UnsupportedOlder
            : CompatibilityDecision::UnsupportedFuture;
    }
    const auto pattern = QStringLiteral("%1.%2.x")
        .arg(version.major)
        .arg(version.minor);
    if (catalog.compatibility.fullyQualified.contains(pattern)) {
        return CompatibilityDecision::SupportedMinor;
    }
    const SemanticVersion target{
        catalog.hyprland.major,
        catalog.hyprland.minor,
        catalog.hyprland.minimumPatch,
    };
    if (version < target) {
        return CompatibilityDecision::UnsupportedOlder;
    }
    return CompatibilityDecision::UnsupportedFuture;
}

const OptionDefinition *findOption(
    const Catalog &catalog,
    const QString &id
)
{
    const auto iterator = std::ranges::find(
        catalog.options, id, &OptionDefinition::id
    );
    return iterator == catalog.options.end() ? nullptr : &*iterator;
}

ValidationErrors validateOptionValue(
    const OptionDefinition &option,
    const QJsonValue &value,
    const QString &path
)
{
    ValidationErrors errors;
    const auto typeMatches = valueHasOptionType(
        value, option.type, path, errors
    );
    if (typeMatches && !constraintAllowsValue(option, value)) {
        addError(
            errors,
            path,
            QStringLiteral("option.constraint-violation"),
            QStringLiteral("The value violates the option constraints.")
        );
    }
    return errors;
}

} // namespace HyprShelld::Hyprland
