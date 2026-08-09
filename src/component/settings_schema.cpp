#include "settings_schema.h"

#include "strict_json.h"

#include <QJsonArray>
#include <QRegularExpression>
#include <QSet>

#include <algorithm>
#include <cmath>

namespace HyprShelld::Components {
namespace {

constexpr qsizetype maximumSchemaBytes = 256 * 1024;
constexpr int maximumSchemaDepth = 32;
constexpr qsizetype maximumSettingCount = 128;
constexpr double maximumExactJsonInteger = 9007199254740991.0;

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
                QStringLiteral("settings-schema.unknown-field"),
                QStringLiteral("Unknown field: %1").arg(iterator.key())
            );
        }
    }
}

[[nodiscard]] bool hasDisallowedControl(const QString &value)
{
    return std::ranges::any_of(value.toUcs4(), [](const auto codePoint) {
        const auto category = QChar::category(
            static_cast<char32_t>(codePoint)
        );
        return category == QChar::Other_Control
            || category == QChar::Other_Format;
    });
}

[[nodiscard]] bool hasControl(const QString &value)
{
    return std::ranges::any_of(value, [](const QChar character) {
        return character.category() == QChar::Other_Control;
    });
}

[[nodiscard]] bool isExactJsonInteger(const double value)
{
    return std::isfinite(value) && std::floor(value) == value
        && std::abs(value) <= maximumExactJsonInteger;
}

[[nodiscard]] QString requiredText(
    const QJsonObject &object,
    const QString &key,
    const QString &path,
    const qsizetype maximumLength,
    ValidationErrors &errors
)
{
    const auto value = object.value(key);
    const auto valuePath = path + QLatin1Char('.') + key;
    if (!value.isString()) {
        addError(
            errors,
            valuePath,
            QStringLiteral("settings-schema.string-required"),
            QStringLiteral("A string value is required.")
        );
        return {};
    }
    const auto normalized = value.toString().normalized(
        QString::NormalizationForm_C
    );
    if (normalized.isEmpty() || normalized.size() > maximumLength
        || normalized != normalized.trimmed()
        || hasDisallowedControl(normalized)) {
        addError(
            errors,
            valuePath,
            QStringLiteral("settings-schema.invalid-string"),
            QStringLiteral(
                "The string is empty, too long, or contains disallowed "
                "control or format characters."
            )
        );
        return {};
    }
    return normalized;
}

[[nodiscard]] std::optional<double> requiredFiniteNumber(
    const QJsonObject &object,
    const QString &key,
    const QString &path,
    ValidationErrors &errors
)
{
    const auto value = object.value(key);
    if (!value.isDouble() || !std::isfinite(value.toDouble())) {
        addError(
            errors,
            path + QLatin1Char('.') + key,
            QStringLiteral("settings-schema.finite-number-required"),
            QStringLiteral("A finite number is required.")
        );
        return std::nullopt;
    }
    return value.toDouble();
}

[[nodiscard]] std::optional<qint64> requiredInteger(
    const QJsonObject &object,
    const QString &key,
    const QString &path,
    const qint64 minimum,
    const qint64 maximum,
    ValidationErrors &errors
)
{
    const auto number = requiredFiniteNumber(object, key, path, errors);
    if (!number.has_value()) {
        return std::nullopt;
    }
    if (!isExactJsonInteger(*number) || *number < minimum || *number > maximum) {
        addError(
            errors,
            path + QLatin1Char('.') + key,
            QStringLiteral("settings-schema.integer-out-of-range"),
            QStringLiteral("The value must be a bounded integer.")
        );
        return std::nullopt;
    }
    return static_cast<qint64>(*number);
}

[[nodiscard]] bool isValidSettingKey(const QString &key)
{
    static const QRegularExpression expression(QStringLiteral(
        "^[a-z][A-Za-z0-9]{0,63}$"
    ));
    return expression.match(key).hasMatch();
}

[[nodiscard]] bool isValidGroup(const QString &group)
{
    static const QRegularExpression expression(QStringLiteral(
        "^[a-z][a-z0-9]*(?:-[a-z0-9]+)*$"
    ));
    return group.size() <= 64 && expression.match(group).hasMatch();
}

[[nodiscard]] bool isColor(const QString &color)
{
    static const QRegularExpression expression(QStringLiteral(
        "^#[0-9A-Fa-f]{6}(?:[0-9A-Fa-f]{2})?$"
    ));
    return expression.match(color).hasMatch();
}

[[nodiscard]] ValidationResult<QJsonValue> invalidValue(
    const QString &path,
    const QString &code,
    const QString &message
)
{
    ValidationResult<QJsonValue> result;
    addError(result.errors, path, code, message);
    return result;
}

[[nodiscard]] ValidationResult<QJsonValue> normalizeKeybinding(
    const QJsonValue &value,
    const QString &path
)
{
    if (value.isNull()) {
        return {.value = QJsonValue(QJsonValue::Null), .errors = {}};
    }
    if (!value.isObject()) {
        return invalidValue(
            path,
            QStringLiteral("settings-value.keybinding-required"),
            QStringLiteral("A keybinding object or null is required.")
        );
    }
    const auto object = value.toObject();
    ValidationErrors errors;
    rejectUnknownFields(
        object,
        {QStringLiteral("modifiers"), QStringLiteral("key")},
        path,
        errors
    );
    const auto modifiersValue = object.value(QStringLiteral("modifiers"));
    const auto keyValue = object.value(QStringLiteral("key"));
    if (!modifiersValue.isArray() || modifiersValue.toArray().size() > 5) {
        addError(
            errors,
            path + QStringLiteral(".modifiers"),
            QStringLiteral("settings-value.invalid-modifiers"),
            QStringLiteral("modifiers must be an array of at most five names.")
        );
    }
    if (!keyValue.isString() || keyValue.toString().isEmpty()
        || keyValue.toString().size() > 64 || hasControl(keyValue.toString())) {
        addError(
            errors,
            path + QStringLiteral(".key"),
            QStringLiteral("settings-value.invalid-key"),
            QStringLiteral("A bounded key name is required.")
        );
    }

    static const QStringList canonicalModifiers{
        QStringLiteral("ctrl"),
        QStringLiteral("alt"),
        QStringLiteral("shift"),
        QStringLiteral("super"),
        QStringLiteral("hyper"),
    };
    QSet<QString> modifiers;
    if (modifiersValue.isArray()) {
        const auto array = modifiersValue.toArray();
        for (qsizetype index = 0; index < array.size(); ++index) {
            if (!array.at(index).isString()) {
                addError(
                    errors,
                    path + QStringLiteral(".modifiers[%1]").arg(index),
                    QStringLiteral("settings-value.invalid-modifier"),
                    QStringLiteral("Modifier names must be strings.")
                );
                continue;
            }
            const auto modifier = array.at(index).toString();
            if (!canonicalModifiers.contains(modifier)) {
                addError(
                    errors,
                    path + QStringLiteral(".modifiers[%1]").arg(index),
                    QStringLiteral("settings-value.invalid-modifier"),
                    QStringLiteral("Unknown keybinding modifier.")
                );
            } else if (modifiers.contains(modifier)) {
                addError(
                    errors,
                    path + QStringLiteral(".modifiers[%1]").arg(index),
                    QStringLiteral("settings-value.duplicate-modifier"),
                    QStringLiteral("A keybinding modifier may appear only once.")
                );
            }
            modifiers.insert(modifier);
        }
    }
    if (!errors.isEmpty()) {
        return {.value = std::nullopt, .errors = std::move(errors)};
    }

    QJsonArray normalizedModifiers;
    for (const auto &modifier : canonicalModifiers) {
        if (modifiers.contains(modifier)) {
            normalizedModifiers.append(modifier);
        }
    }
    QJsonObject normalized{
        {QStringLiteral("modifiers"), normalizedModifiers},
        {QStringLiteral("key"),
         keyValue.toString().normalized(QString::NormalizationForm_C)},
    };
    return {.value = QJsonValue(normalized), .errors = {}};
}

[[nodiscard]] QVector<EnumOption> parseEnumOptions(
    const QJsonValue &value,
    const QString &path,
    ValidationErrors &errors
)
{
    QVector<EnumOption> options;
    if (!value.isArray() || value.toArray().isEmpty()
        || value.toArray().size() > 64) {
        addError(
            errors,
            path,
            QStringLiteral("settings-schema.invalid-options"),
            QStringLiteral("An enum requires one to sixty-four options.")
        );
        return options;
    }
    QSet<QString> values;
    const auto array = value.toArray();
    for (qsizetype index = 0; index < array.size(); ++index) {
        const auto optionPath = path + QStringLiteral("[%1]").arg(index);
        if (!array.at(index).isObject()) {
            addError(
                errors,
                optionPath,
                QStringLiteral("settings-schema.object-required"),
                QStringLiteral("Each enum option must be an object.")
            );
            continue;
        }
        const auto object = array.at(index).toObject();
        rejectUnknownFields(
            object,
            {QStringLiteral("value"), QStringLiteral("label")},
            optionPath,
            errors
        );
        EnumOption option{
            .value = requiredText(
                object,
                QStringLiteral("value"),
                optionPath,
                128,
                errors
            ),
            .label = requiredText(
                object,
                QStringLiteral("label"),
                optionPath,
                128,
                errors
            ),
        };
        if (values.contains(option.value)) {
            addError(
                errors,
                optionPath + QStringLiteral(".value"),
                QStringLiteral("settings-schema.duplicate-option"),
                QStringLiteral("Enum option values must be unique.")
            );
        }
        values.insert(option.value);
        options.append(std::move(option));
    }
    return options;
}

[[nodiscard]] SettingDefinition parseDefinition(
    const QJsonValue &value,
    const qsizetype index,
    ValidationErrors &errors
)
{
    SettingDefinition definition;
    const auto path = QStringLiteral("$.settings[%1]").arg(index);
    if (!value.isObject()) {
        addError(
            errors,
            path,
            QStringLiteral("settings-schema.object-required"),
            QStringLiteral("Each setting definition must be an object.")
        );
        return definition;
    }
    const auto object = value.toObject();
    const QSet<QString> baseFields{
        QStringLiteral("key"),
        QStringLiteral("scope"),
        QStringLiteral("type"),
        QStringLiteral("label"),
        QStringLiteral("description"),
        QStringLiteral("group"),
        QStringLiteral("order"),
        QStringLiteral("default"),
        QStringLiteral("visibleWhen"),
    };

    definition.key = requiredText(
        object,
        QStringLiteral("key"),
        path,
        64,
        errors
    );
    if (!definition.key.isEmpty() && !isValidSettingKey(definition.key)) {
        addError(
            errors,
            path + QStringLiteral(".key"),
            QStringLiteral("settings-schema.invalid-key"),
            QStringLiteral("Setting keys must use lower camel-case ASCII.")
        );
    }

    const auto scopeText = requiredText(
        object,
        QStringLiteral("scope"),
        path,
        16,
        errors
    );
    const auto scope = settingScopeFromString(scopeText);
    if (scope.has_value()) {
        definition.scope = *scope;
    } else {
        addError(
            errors,
            path + QStringLiteral(".scope"),
            QStringLiteral("settings-schema.invalid-scope"),
            QStringLiteral("Unknown setting scope.")
        );
    }

    const auto typeText = requiredText(
        object,
        QStringLiteral("type"),
        path,
        16,
        errors
    );
    const auto type = settingTypeFromString(typeText);
    if (type.has_value()) {
        definition.type = *type;
    } else {
        addError(
            errors,
            path + QStringLiteral(".type"),
            QStringLiteral("settings-schema.invalid-type"),
            QStringLiteral("Unknown setting type.")
        );
    }

    definition.label = requiredText(
        object,
        QStringLiteral("label"),
        path,
        128,
        errors
    );
    definition.description = requiredText(
        object,
        QStringLiteral("description"),
        path,
        1024,
        errors
    );
    definition.group = requiredText(
        object,
        QStringLiteral("group"),
        path,
        64,
        errors
    );
    if (!definition.group.isEmpty() && !isValidGroup(definition.group)) {
        addError(
            errors,
            path + QStringLiteral(".group"),
            QStringLiteral("settings-schema.invalid-group"),
            QStringLiteral("Setting groups must use lowercase kebab-case ASCII.")
        );
    }
    const auto order = requiredInteger(
        object,
        QStringLiteral("order"),
        path,
        0,
        100000,
        errors
    );
    if (order.has_value()) {
        definition.order = static_cast<qint32>(*order);
    }
    if (!object.contains(QStringLiteral("default"))) {
        addError(
            errors,
            path + QStringLiteral(".default"),
            QStringLiteral("settings-schema.default-required"),
            QStringLiteral("Every setting requires an explicit default.")
        );
    } else {
        definition.defaultValue = object.value(QStringLiteral("default"));
    }

    auto allowed = baseFields;
    if (type.has_value()) {
        switch (*type) {
        case SettingType::Integer:
        case SettingType::Number: {
            allowed.insert(QStringLiteral("minimum"));
            allowed.insert(QStringLiteral("maximum"));
            allowed.insert(QStringLiteral("step"));
            definition.minimum = requiredFiniteNumber(
                object,
                QStringLiteral("minimum"),
                path,
                errors
            );
            definition.maximum = requiredFiniteNumber(
                object,
                QStringLiteral("maximum"),
                path,
                errors
            );
            if (definition.minimum.has_value()
                && definition.maximum.has_value()
                && *definition.minimum > *definition.maximum) {
                addError(
                    errors,
                    path + QStringLiteral(".maximum"),
                    QStringLiteral("settings-schema.invalid-range"),
                    QStringLiteral("maximum must be greater than or equal to minimum.")
                );
            }
            if (*type == SettingType::Integer
                && ((definition.minimum.has_value()
                     && !isExactJsonInteger(*definition.minimum))
                    || (definition.maximum.has_value()
                        && !isExactJsonInteger(*definition.maximum)))) {
                addError(
                    errors,
                    path,
                    QStringLiteral("settings-schema.integer-bounds-required"),
                    QStringLiteral("Integer settings require exact JSON-safe integer bounds.")
                );
            }
            if (object.contains(QStringLiteral("step"))) {
                definition.step = requiredFiniteNumber(
                    object,
                    QStringLiteral("step"),
                    path,
                    errors
                );
                if (definition.step.has_value()
                    && (*definition.step <= 0
                        || (*type == SettingType::Integer
                            && !isExactJsonInteger(*definition.step)))) {
                    addError(
                        errors,
                        path + QStringLiteral(".step"),
                        QStringLiteral("settings-schema.invalid-step"),
                        QStringLiteral("step must be positive and match the numeric type.")
                    );
                }
            }
            break;
        }
        case SettingType::String: {
            allowed.insert(QStringLiteral("minimumLength"));
            allowed.insert(QStringLiteral("maximumLength"));
            if (object.contains(QStringLiteral("minimumLength"))) {
                const auto minimum = requiredInteger(
                    object,
                    QStringLiteral("minimumLength"),
                    path,
                    0,
                    4096,
                    errors
                );
                if (minimum.has_value()) {
                    definition.minimumLength = static_cast<qsizetype>(*minimum);
                }
            } else {
                definition.minimumLength = 0;
            }
            const auto maximum = requiredInteger(
                object,
                QStringLiteral("maximumLength"),
                path,
                1,
                4096,
                errors
            );
            if (maximum.has_value()) {
                definition.maximumLength = static_cast<qsizetype>(*maximum);
            }
            if (definition.minimumLength.has_value()
                && definition.maximumLength.has_value()
                && *definition.minimumLength > *definition.maximumLength) {
                addError(
                    errors,
                    path,
                    QStringLiteral("settings-schema.invalid-length-range"),
                    QStringLiteral("minimumLength cannot exceed maximumLength.")
                );
            }
            break;
        }
        case SettingType::Enumeration:
            allowed.insert(QStringLiteral("options"));
            definition.options = parseEnumOptions(
                object.value(QStringLiteral("options")),
                path + QStringLiteral(".options"),
                errors
            );
            break;
        case SettingType::Boolean:
        case SettingType::Color:
        case SettingType::Keybinding:
        case SettingType::File:
        case SettingType::Directory:
            break;
        }
    }
    rejectUnknownFields(object, allowed, path, errors);

    if (object.contains(QStringLiteral("visibleWhen"))) {
        const auto conditionValue = object.value(QStringLiteral("visibleWhen"));
        const auto conditionPath = path + QStringLiteral(".visibleWhen");
        if (!conditionValue.isObject()) {
            addError(
                errors,
                conditionPath,
                QStringLiteral("settings-schema.invalid-condition"),
                QStringLiteral("visibleWhen must be an object.")
            );
        } else {
            const auto condition = conditionValue.toObject();
            rejectUnknownFields(
                condition,
                {QStringLiteral("key"), QStringLiteral("equals")},
                conditionPath,
                errors
            );
            if (!condition.contains(QStringLiteral("equals"))) {
                addError(
                    errors,
                    conditionPath + QStringLiteral(".equals"),
                    QStringLiteral("settings-schema.condition-value-required"),
                    QStringLiteral("visibleWhen requires an equality value.")
                );
            }
            definition.visibleWhen = VisibilityCondition{
                .key = requiredText(
                    condition,
                    QStringLiteral("key"),
                    conditionPath,
                    64,
                    errors
                ),
                .equals = condition.value(QStringLiteral("equals")),
            };
        }
    }

    if (object.contains(QStringLiteral("default")) && type.has_value()) {
        const auto normalized = normalizeSettingValue(
            definition,
            definition.defaultValue,
            path + QStringLiteral(".default")
        );
        if (!normalized) {
            errors += normalized.errors;
        } else {
            definition.defaultValue = *normalized.value;
        }
        if ((*type == SettingType::File || *type == SettingType::Directory)
            && !definition.defaultValue.isNull()) {
            addError(
                errors,
                path + QStringLiteral(".default"),
                QStringLiteral("settings-schema.portal-default-must-be-null"),
                QStringLiteral("File and directory settings must default to null.")
            );
        }
    }
    return definition;
}

} // namespace

const SettingDefinition *SettingsSchema::find(const QString &key) const
{
    const auto iterator = std::ranges::find(
        settings,
        key,
        &SettingDefinition::key
    );
    return iterator == settings.cend() ? nullptr : &*iterator;
}

QString toString(const SettingScope scope)
{
    switch (scope) {
    case SettingScope::Component: return QStringLiteral("component");
    case SettingScope::Instance: return QStringLiteral("instance");
    }
    return {};
}

QString toString(const SettingType type)
{
    switch (type) {
    case SettingType::Boolean: return QStringLiteral("boolean");
    case SettingType::Integer: return QStringLiteral("integer");
    case SettingType::Number: return QStringLiteral("number");
    case SettingType::String: return QStringLiteral("string");
    case SettingType::Enumeration: return QStringLiteral("enum");
    case SettingType::Color: return QStringLiteral("color");
    case SettingType::Keybinding: return QStringLiteral("keybinding");
    case SettingType::File: return QStringLiteral("file");
    case SettingType::Directory: return QStringLiteral("directory");
    }
    return {};
}

std::optional<SettingScope> settingScopeFromString(const QString &value)
{
    if (value == QStringLiteral("component")) {
        return SettingScope::Component;
    }
    if (value == QStringLiteral("instance")) {
        return SettingScope::Instance;
    }
    return std::nullopt;
}

std::optional<SettingType> settingTypeFromString(const QString &value)
{
    if (value == QStringLiteral("boolean")) {
        return SettingType::Boolean;
    }
    if (value == QStringLiteral("integer")) {
        return SettingType::Integer;
    }
    if (value == QStringLiteral("number")) {
        return SettingType::Number;
    }
    if (value == QStringLiteral("string")) {
        return SettingType::String;
    }
    if (value == QStringLiteral("enum")) {
        return SettingType::Enumeration;
    }
    if (value == QStringLiteral("color")) {
        return SettingType::Color;
    }
    if (value == QStringLiteral("keybinding")) {
        return SettingType::Keybinding;
    }
    if (value == QStringLiteral("file")) {
        return SettingType::File;
    }
    if (value == QStringLiteral("directory")) {
        return SettingType::Directory;
    }
    return std::nullopt;
}

ValidationResult<QJsonValue> normalizeSettingValue(
    const SettingDefinition &definition,
    const QJsonValue &value,
    const QString &path
)
{
    switch (definition.type) {
    case SettingType::Boolean:
        if (!value.isBool()) {
            return invalidValue(
                path,
                QStringLiteral("settings-value.boolean-required"),
                QStringLiteral("A boolean value is required.")
            );
        }
        return {.value = value, .errors = {}};
    case SettingType::Integer:
    case SettingType::Number: {
        if (!value.isDouble() || !std::isfinite(value.toDouble())
            || (definition.type == SettingType::Integer
                && !isExactJsonInteger(value.toDouble()))) {
            return invalidValue(
                path,
                QStringLiteral("settings-value.invalid-number"),
                QStringLiteral("A finite value matching the numeric type is required.")
            );
        }
        const auto number = value.toDouble();
        if (!definition.minimum.has_value()
            || !definition.maximum.has_value()
            || number < *definition.minimum || number > *definition.maximum) {
            return invalidValue(
                path,
                QStringLiteral("settings-value.number-out-of-range"),
                QStringLiteral("The number is outside its declared bounds.")
            );
        }
        if (definition.step.has_value()) {
            bool stepMismatch = false;
            if (definition.type == SettingType::Integer
                && isExactJsonInteger(*definition.minimum)
                && isExactJsonInteger(*definition.step)
                && *definition.step > 0) {
                const auto integer = static_cast<qint64>(number);
                const auto minimum = static_cast<qint64>(*definition.minimum);
                const auto step = static_cast<qint64>(*definition.step);
                stepMismatch = (integer - minimum) % step != 0;
            } else {
                const auto offset = (number - *definition.minimum)
                    / *definition.step;
                stepMismatch = !std::isfinite(offset)
                    || std::abs(offset - std::round(offset)) > 1e-9;
            }
            if (stepMismatch) {
                return invalidValue(
                    path,
                    QStringLiteral("settings-value.step-mismatch"),
                    QStringLiteral("The number does not align with its declared step.")
                );
            }
        }
        return {.value = QJsonValue(number), .errors = {}};
    }
    case SettingType::String: {
        if (!value.isString()) {
            return invalidValue(
                path,
                QStringLiteral("settings-value.string-required"),
                QStringLiteral("A string value is required.")
            );
        }
        const auto normalized = value.toString().normalized(
            QString::NormalizationForm_C
        );
        if (hasControl(normalized) || !definition.minimumLength.has_value()
            || !definition.maximumLength.has_value()
            || normalized.size() < *definition.minimumLength
            || normalized.size() > *definition.maximumLength) {
            return invalidValue(
                path,
                QStringLiteral("settings-value.invalid-string"),
                QStringLiteral("The string violates its declared length or character bounds.")
            );
        }
        return {.value = QJsonValue(normalized), .errors = {}};
    }
    case SettingType::Enumeration: {
        if (!value.isString()) {
            return invalidValue(
                path,
                QStringLiteral("settings-value.enum-required"),
                QStringLiteral("An enum option string is required.")
            );
        }
        const auto normalized = value.toString().normalized(
            QString::NormalizationForm_C
        );
        const auto present = std::ranges::any_of(
            definition.options,
            [&normalized](const EnumOption &option) {
                return option.value == normalized;
            }
        );
        if (!present) {
            return invalidValue(
                path,
                QStringLiteral("settings-value.unknown-enum-option"),
                QStringLiteral("The value is not one of the declared enum options.")
            );
        }
        return {.value = QJsonValue(normalized), .errors = {}};
    }
    case SettingType::Color:
        if (!value.isString() || !isColor(value.toString())) {
            return invalidValue(
                path,
                QStringLiteral("settings-value.invalid-color"),
                QStringLiteral("A #RRGGBB or #RRGGBBAA color is required.")
            );
        }
        return {
            .value = QJsonValue(value.toString().toUpper()),
            .errors = {},
        };
    case SettingType::Keybinding:
        return normalizeKeybinding(value, path);
    case SettingType::File:
    case SettingType::Directory:
        if (value.isNull()) {
            return {.value = QJsonValue(QJsonValue::Null), .errors = {}};
        }
        if (!value.isString() || value.toString().size() > 4096
            || hasControl(value.toString())) {
            return invalidValue(
                path,
                QStringLiteral("settings-value.invalid-portal-selection"),
                QStringLiteral("A bounded portal selection string or null is required.")
            );
        }
        return {
            .value = QJsonValue(value.toString().normalized(
                QString::NormalizationForm_C
            )),
            .errors = {},
        };
    }
    return invalidValue(
        path,
        QStringLiteral("settings-value.unknown-type"),
        QStringLiteral("Unknown setting type.")
    );
}

ValidationResult<SettingsSchema> parseSettingsSchema(const QByteArrayView bytes)
{
    ValidationResult<SettingsSchema> result;
    const auto json = parseStrictJsonObject(
        bytes,
        {.maximumBytes = maximumSchemaBytes,
         .maximumDepth = maximumSchemaDepth}
    );
    if (!json) {
        result.errors = json.errors;
        return result;
    }
    const auto &object = *json.value;
    rejectUnknownFields(
        object,
        {QStringLiteral("schemaVersion"), QStringLiteral("settings")},
        QStringLiteral("$"),
        result.errors
    );

    SettingsSchema schema;
    const auto schemaVersion = requiredInteger(
        object,
        QStringLiteral("schemaVersion"),
        QStringLiteral("$"),
        1,
        1,
        result.errors
    );
    if (schemaVersion.has_value()) {
        schema.schemaVersion = static_cast<quint32>(*schemaVersion);
    }

    const auto settingsValue = object.value(QStringLiteral("settings"));
    if (!settingsValue.isArray()
        || settingsValue.toArray().size() > maximumSettingCount) {
        addError(
            result.errors,
            QStringLiteral("$.settings"),
            QStringLiteral("settings-schema.invalid-settings-array"),
            QStringLiteral("settings must be an array of at most 128 definitions.")
        );
        return result;
    }

    QSet<QString> keys;
    const auto array = settingsValue.toArray();
    for (qsizetype index = 0; index < array.size(); ++index) {
        auto definition = parseDefinition(array.at(index), index, result.errors);
        if (keys.contains(definition.key)) {
            addError(
                result.errors,
                QStringLiteral("$.settings[%1].key").arg(index),
                QStringLiteral("settings-schema.duplicate-key"),
                QStringLiteral("Setting keys must be unique.")
            );
        }
        keys.insert(definition.key);

        if (definition.visibleWhen.has_value()) {
            const auto &condition = *definition.visibleWhen;
            const auto previous = std::ranges::find(
                schema.settings,
                condition.key,
                &SettingDefinition::key
            );
            if (previous == schema.settings.cend()
                || previous->scope != definition.scope) {
                addError(
                    result.errors,
                    QStringLiteral("$.settings[%1].visibleWhen.key").arg(index),
                    QStringLiteral("settings-schema.invalid-condition-reference"),
                    QStringLiteral("visibleWhen must reference an earlier setting in the same scope.")
                );
            } else if (previous->type == SettingType::Keybinding
                       || previous->type == SettingType::File
                       || previous->type == SettingType::Directory) {
                addError(
                    result.errors,
                    QStringLiteral("$.settings[%1].visibleWhen.key").arg(index),
                    QStringLiteral("settings-schema.unsupported-condition-type"),
                    QStringLiteral("This setting type cannot control visibility.")
                );
            } else {
                const auto normalized = normalizeSettingValue(
                    *previous,
                    condition.equals,
                    QStringLiteral("$.settings[%1].visibleWhen.equals").arg(index)
                );
                if (!normalized) {
                    result.errors += normalized.errors;
                } else {
                    definition.visibleWhen->equals = *normalized.value;
                }
            }
        }
        schema.settings.append(std::move(definition));
    }

    if (result.errors.isEmpty()) {
        result.value = std::move(schema);
    }
    return result;
}

ValidationResult<QJsonObject> normalizeSettings(
    const SettingsSchema &schema,
    const SettingScope scope,
    const QJsonObject &values,
    const bool applyDefaults
)
{
    ValidationResult<QJsonObject> result;
    QJsonObject normalized;

    for (auto iterator = values.constBegin(); iterator != values.constEnd();
         ++iterator) {
        const auto *definition = schema.find(iterator.key());
        if (definition == nullptr || definition->scope != scope) {
            addError(
                result.errors,
                QStringLiteral("$.") + iterator.key(),
                QStringLiteral("settings-value.unknown-or-wrong-scope"),
                QStringLiteral("The setting key is unknown or belongs to another scope.")
            );
        }
    }

    for (const auto &definition : schema.settings) {
        if (definition.scope != scope) {
            continue;
        }
        const auto present = values.contains(definition.key);
        if (!present && !applyDefaults) {
            continue;
        }
        const auto value = present ? values.value(definition.key)
                                   : definition.defaultValue;
        const auto normalizedValue = normalizeSettingValue(
            definition,
            value,
            QStringLiteral("$.") + definition.key
        );
        if (!normalizedValue) {
            result.errors += normalizedValue.errors;
        } else {
            normalized.insert(definition.key, *normalizedValue.value);
        }
    }

    if (result.errors.isEmpty()) {
        result.value = std::move(normalized);
    }
    return result;
}

} // namespace HyprShelld::Components
