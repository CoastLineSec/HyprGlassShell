#include "desired_state.h"

#include "default_keybindings.h"
#include "json_support.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QRegularExpression>
#include <QSet>

#include <re2/re2.h>
#include <xkbcommon/xkbcommon.h>

#include <algorithm>
#include <cmath>
#include <limits>

namespace HyprShelld::Hyprland {
namespace {

constexpr int maximumDesiredStateDepth = 64;
constexpr qint64 maximumSafeJsonIntegerValue = 9007199254740991LL;
constexpr double maximumSafeJsonInteger =
    static_cast<double>(maximumSafeJsonIntegerValue);

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
                QStringLiteral("state.unknown-field"),
                QStringLiteral("Unknown desired-state field: %1")
                    .arg(iterator.key())
            );
        }
    }
}

[[nodiscard]] bool hasDisallowedCharacter(
    const QString &value,
    const bool allowNewlines
)
{
    for (const auto codePoint : value.toUcs4()) {
        const auto category = QChar::category(static_cast<char32_t>(codePoint));
        if ((category == QChar::Other_Control
                || category == QChar::Other_Format)
            && !(allowNewlines && (codePoint == '\n' || codePoint == '\r'
                || codePoint == '\t'))) {
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
    const qsizetype maximumLength = maximumStateStringLength,
    const bool allowEmpty = false,
    const bool allowNewlines = false
)
{
    const auto valuePath = path + QLatin1Char('.') + key;
    const auto value = object.value(key);
    if (!value.isString()) {
        addError(
            errors,
            valuePath,
            QStringLiteral("state.string-required"),
            QStringLiteral("A string value is required.")
        );
        return {};
    }
    const auto text = value.toString();
    if ((!allowEmpty && text.isEmpty()) || text.size() > maximumLength
        || text != text.normalized(QString::NormalizationForm_C)
        || hasDisallowedCharacter(text, allowNewlines)) {
        addError(
            errors,
            valuePath,
            QStringLiteral("state.invalid-string"),
            QStringLiteral("The string is empty, non-canonical, too long, or contains a disallowed character.")
        );
        return {};
    }
    return text;
}

[[nodiscard]] bool readBoolean(
    const QJsonObject &object,
    const QString &key,
    const QString &path,
    ValidationErrors &errors,
    const bool fallback = false
)
{
    const auto value = object.value(key);
    if (!value.isBool()) {
        addError(
            errors,
            path + QLatin1Char('.') + key,
            QStringLiteral("state.boolean-required"),
            QStringLiteral("A boolean value is required.")
        );
        return fallback;
    }
    return value.toBool();
}

[[nodiscard]] std::optional<double> readNumber(
    const QJsonObject &object,
    const QString &key,
    const QString &path,
    ValidationErrors &errors,
    const double minimum,
    const double maximum
)
{
    const auto value = object.value(key);
    const auto valuePath = path + QLatin1Char('.') + key;
    if (!value.isDouble() || !std::isfinite(value.toDouble())) {
        addError(
            errors,
            valuePath,
            QStringLiteral("state.number-required"),
            QStringLiteral("A finite number is required.")
        );
        return std::nullopt;
    }
    const auto number = value.toDouble();
    if (number < minimum || number > maximum) {
        addError(
            errors,
            valuePath,
            QStringLiteral("state.number-out-of-range"),
            QStringLiteral("The number is outside the accepted range.")
        );
        return std::nullopt;
    }
    return number;
}

[[nodiscard]] std::optional<qint64> readInteger(
    const QJsonObject &object,
    const QString &key,
    const QString &path,
    ValidationErrors &errors,
    const qint64 minimum,
    const qint64 maximum
)
{
    const auto value = object.value(key);
    const auto valuePath = path + QLatin1Char('.') + key;
    if (!value.isDouble() || !std::isfinite(value.toDouble())
        || std::floor(value.toDouble()) != value.toDouble()
        || std::abs(value.toDouble()) > maximumSafeJsonInteger
        || value.toDouble() < static_cast<double>(minimum)
        || value.toDouble() > static_cast<double>(maximum)) {
        addError(
            errors,
            valuePath,
            QStringLiteral("state.integer-out-of-range"),
            QStringLiteral("A bounded exactly representable integer is required.")
        );
        return std::nullopt;
    }
    return static_cast<qint64>(value.toDouble());
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
            QStringLiteral("state.object-required"),
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
            QStringLiteral("state.array-required"),
            QStringLiteral("An array is required.")
        );
        return {};
    }
    return value.toArray();
}

[[nodiscard]] bool isStableId(const QString &value)
{
    static const QRegularExpression expression(
        QStringLiteral("^[A-Za-z0-9][A-Za-z0-9._:-]{0,127}$")
    );
    return expression.match(value).hasMatch();
}

void validateStableId(
    const QString &id,
    const QString &path,
    ValidationErrors &errors
)
{
    if (!isStableId(id)) {
        addError(
            errors,
            path,
            QStringLiteral("state.invalid-id"),
            QStringLiteral("A stable one-to-128-character record ID is required.")
        );
    }
}

[[nodiscard]] QStringList readStringArray(
    const QJsonObject &object,
    const QString &key,
    const QString &path,
    const qsizetype maximumItems,
    ValidationErrors &errors,
    const bool allowEmptyStrings = true
)
{
    QStringList result;
    const auto array = readArray(object, key, path, errors);
    const auto arrayPath = path + QLatin1Char('.') + key;
    if (array.size() > maximumItems) {
        addError(
            errors,
            arrayPath,
            QStringLiteral("state.collection-limit"),
            QStringLiteral("The array exceeds its item limit.")
        );
    }
    for (qsizetype index = 0;
         index < std::min(array.size(), maximumItems);
         ++index) {
        const auto itemPath = arrayPath + QLatin1Char('[')
            + QString::number(index) + QLatin1Char(']');
        if (!array.at(index).isString()) {
            addError(
                errors,
                itemPath,
                QStringLiteral("state.string-required"),
                QStringLiteral("A string array item is required.")
            );
            continue;
        }
        const auto item = array.at(index).toString();
        if ((!allowEmptyStrings && item.isEmpty())
            || item.size() > maximumStateStringLength
            || item != item.normalized(QString::NormalizationForm_C)
            || hasDisallowedCharacter(item, true)) {
            addError(
                errors,
                itemPath,
                QStringLiteral("state.invalid-string"),
                QStringLiteral("The string array item is invalid.")
            );
            continue;
        }
        result.append(item);
    }
    return result;
}

void validatePersistedModifiers(
    const QStringList &modifiers,
    const QString &path,
    ValidationErrors &errors
)
{
    static const QSet<QString> canonicalModifiers{
        QStringLiteral("ctrl"), QStringLiteral("alt"),
        QStringLiteral("shift"), QStringLiteral("super"),
        QStringLiteral("caps"), QStringLiteral("mod2"),
        QStringLiteral("mod3"), QStringLiteral("mod5"),
    };
    QSet<QString> seen;
    for (qsizetype index = 0; index < modifiers.size(); ++index) {
        const auto itemPath = path + QLatin1Char('[')
            + QString::number(index) + QLatin1Char(']');
        const auto &modifier = modifiers.at(index);
        if (!canonicalModifiers.contains(modifier)) {
            addError(
                errors,
                itemPath,
                QStringLiteral("state.invalid-modifier"),
                QStringLiteral("Persisted modifiers must use the canonical lowercase contract spelling.")
            );
        }
        if (seen.contains(modifier)) {
            addError(
                errors,
                itemPath,
                QStringLiteral("state.duplicate-modifier"),
                QStringLiteral("Persisted modifiers must be unique.")
            );
        }
        seen.insert(modifier);
    }
}

void validateNonEmptySchemaStringList(
    const QStringList &values,
    const QString &path,
    ValidationErrors &errors
)
{
    for (qsizetype index = 0; index < values.size(); ++index) {
        const auto &value = values.at(index);
        if (value.isEmpty() || value.size() > 256
            || value != value.normalized(QString::NormalizationForm_C)
            || hasDisallowedCharacter(value, false)) {
            addError(
                errors,
                path + QLatin1Char('[') + QString::number(index)
                    + QLatin1Char(']'),
                QStringLiteral("state.invalid-string"),
                QStringLiteral("The list item must satisfy the non-empty 256-character schema contract.")
            );
        }
    }
}

[[nodiscard]] bool isBooleanValue(const QJsonValue &value)
{
    return value.isBool();
}

[[nodiscard]] bool isSchemaString(
    const QJsonValue &value,
    const qsizetype maximumLength,
    const bool allowEmpty
)
{
    return value.isString()
        && (allowEmpty || !value.toString().isEmpty())
        && value.toString().size() <= maximumLength
        && value.toString()
            == value.toString().normalized(QString::NormalizationForm_C)
        && !hasDisallowedCharacter(value.toString(), false);
}

[[nodiscard]] bool isNumberValue(const QJsonValue &value)
{
    return value.isDouble() && std::isfinite(value.toDouble());
}

[[nodiscard]] bool isIntegerValue(const QJsonValue &value)
{
    return isNumberValue(value)
        && std::floor(value.toDouble()) == value.toDouble()
        && std::abs(value.toDouble()) <= maximumSafeJsonInteger;
}

[[nodiscard]] bool isCssGapValue(const QJsonValue &value)
{
    if (!value.isArray() || value.toArray().size() != 4) {
        return false;
    }
    return std::ranges::all_of(value.toArray(), isIntegerValue);
}

enum class NestedValueKind {
    Boolean,
    Integer,
    Number,
    ShortString,
    NonEmptyString,
    RegexString,
    CssGap,
    Vector2,
    ExpressionVector2,
    Gradient,
    LayoutOptions,
    WindowFullscreenState,
    WindowOpacity,
    SuppressEvents,
    Object,
};

[[nodiscard]] bool isBoundedNumberValue(
    const QJsonValue &value,
    const double minimum,
    const double maximum
)
{
    return isNumberValue(value) && value.toDouble() >= minimum
        && value.toDouble() <= maximum;
}

[[nodiscard]] bool isBoundedIntegerValue(
    const QJsonValue &value,
    const qint64 minimum,
    const qint64 maximum
)
{
    return isIntegerValue(value)
        && value.toDouble() >= static_cast<double>(minimum)
        && value.toDouble() <= static_cast<double>(maximum);
}

[[nodiscard]] bool nestedValueMatches(
    const QJsonValue &value,
    const NestedValueKind kind
)
{
    switch (kind) {
    case NestedValueKind::Boolean: return isBooleanValue(value);
    case NestedValueKind::Integer: return isIntegerValue(value);
    case NestedValueKind::Number: return isNumberValue(value);
    case NestedValueKind::ShortString:
        return isSchemaString(value, 256, true);
    case NestedValueKind::NonEmptyString:
        return isSchemaString(value, 256, false);
    case NestedValueKind::RegexString:
        return isSchemaString(value, 512, false);
    case NestedValueKind::CssGap: return isCssGapValue(value);
    case NestedValueKind::Vector2:
        return value.isArray() && value.toArray().size() == 2
            && std::ranges::all_of(value.toArray(), isNumberValue);
    case NestedValueKind::ExpressionVector2:
        return value.isArray() && value.toArray().size() == 2
            && std::ranges::all_of(value.toArray(), [](const QJsonValue &item) {
                return isBoundedNumberValue(item, -1000000.0, 1000000.0);
            });
    case NestedValueKind::Gradient: {
        if (!value.isObject()) return false;
        const auto gradient = value.toObject();
        if (gradient.size() != 2 || !gradient.contains(QStringLiteral("colors"))
            || !gradient.contains(QStringLiteral("angle"))
            || !gradient.value(QStringLiteral("colors")).isArray()
            || gradient.value(QStringLiteral("colors")).toArray().isEmpty()
            || gradient.value(QStringLiteral("colors")).toArray().size() > 10
            || !isNumberValue(gradient.value(QStringLiteral("angle"))
            ) || gradient.value(QStringLiteral("angle")).toDouble() < -3600
            || gradient.value(QStringLiteral("angle")).toDouble() > 3600) return false;
        static const QRegularExpression color(QStringLiteral("^0x[0-9A-F]{8}$"));
        return std::ranges::all_of(
            gradient.value(QStringLiteral("colors")).toArray(),
            [](const QJsonValue &item) {
                return item.isString() && color.match(item.toString()).hasMatch();
            }
        );
    }
    case NestedValueKind::LayoutOptions: {
        if (!value.isObject()) return false;
        const auto options = value.toObject();
        static const QSet<QString> allowed{QStringLiteral("orientation"), QStringLiteral("direction")};
        static const QSet<QString> orientations{QStringLiteral("left"), QStringLiteral("right"), QStringLiteral("top"), QStringLiteral("bottom"), QStringLiteral("center")};
        static const QSet<QString> directions{QStringLiteral("left"), QStringLiteral("right"), QStringLiteral("up"), QStringLiteral("down")};
        for (auto iterator = options.constBegin(); iterator != options.constEnd(); ++iterator) {
            if (!allowed.contains(iterator.key()) || !iterator.value().isString()) return false;
            if (iterator.key() == QStringLiteral("orientation") && !orientations.contains(iterator.value().toString())) return false;
            if (iterator.key() == QStringLiteral("direction") && !directions.contains(iterator.value().toString())) return false;
        }
        return true;
    }
    case NestedValueKind::WindowFullscreenState: {
        if (!value.isObject()) return false;
        const auto state = value.toObject();
        static const QSet<QString> fields{
            QStringLiteral("internal"), QStringLiteral("client")
        };
        if (!state.contains(QStringLiteral("internal"))) return false;
        for (auto iterator = state.constBegin(); iterator != state.constEnd();
             ++iterator) {
            if (!fields.contains(iterator.key())
                || !isBoundedIntegerValue(iterator.value(), 0, 2)) {
                return false;
            }
        }
        return true;
    }
    case NestedValueKind::WindowOpacity: {
        if (!value.isObject()) return false;
        const auto opacity = value.toObject();
        static const QSet<QString> fields{
            QStringLiteral("active"), QStringLiteral("inactive"),
            QStringLiteral("fullscreen"), QStringLiteral("overrideActive"),
            QStringLiteral("overrideInactive"),
            QStringLiteral("overrideFullscreen")
        };
        static const QSet<QString> required{
            QStringLiteral("active"), QStringLiteral("overrideActive"),
            QStringLiteral("overrideInactive"),
            QStringLiteral("overrideFullscreen")
        };
        for (const auto &field : required) {
            if (!opacity.contains(field)) return false;
        }
        for (auto iterator = opacity.constBegin(); iterator != opacity.constEnd();
             ++iterator) {
            if (!fields.contains(iterator.key())) return false;
            if (iterator.key().startsWith(QStringLiteral("override"))) {
                if (!iterator.value().isBool()) return false;
            } else if (!isBoundedNumberValue(iterator.value(), 0.0, 1.0)) {
                return false;
            }
        }
        return true;
    }
    case NestedValueKind::SuppressEvents: {
        if (!value.isArray()) return false;
        const auto events = value.toArray();
        if (events.isEmpty() || events.size() > 6) return false;
        static const QSet<QString> allowed{
            QStringLiteral("fullscreen"), QStringLiteral("maximize"),
            QStringLiteral("activate"), QStringLiteral("activatefocus"),
            QStringLiteral("fullscreenoutput"),
            QStringLiteral("x11configurerequest")
        };
        QSet<QString> seen;
        for (const auto &event : events) {
            if (!event.isString() || !allowed.contains(event.toString())
                || seen.contains(event.toString())) {
                return false;
            }
            seen.insert(event.toString());
        }
        return true;
    }
    case NestedValueKind::Object: return value.isObject();
    }
    return false;
}

using NestedFieldTable = QMap<QString, NestedValueKind>;

[[nodiscard]] QJsonObject readClosedMap(
    const QJsonObject &parent,
    const QString &key,
    const QString &path,
    const NestedFieldTable &fields,
    ValidationErrors &errors
)
{
    const auto object = readObject(parent, key, path, errors);
    const auto objectPath = path + QLatin1Char('.') + key;
    if (object.size() > maximumGenericMapEntries) {
        addError(
            errors,
            objectPath,
            QStringLiteral("state.collection-limit"),
            QStringLiteral("The nested object exceeds its field limit.")
        );
    }
    for (auto iterator = object.constBegin(); iterator != object.constEnd();
         ++iterator) {
        const auto definition = fields.constFind(iterator.key());
        const auto fieldPath = objectPath + QLatin1Char('.') + iterator.key();
        if (definition == fields.constEnd()) {
            addError(
                errors,
                fieldPath,
                QStringLiteral("state.unknown-field"),
                QStringLiteral("The nested field is not supported by the pinned Hyprland contract.")
            );
        } else if (!nestedValueMatches(iterator.value(), *definition)) {
            addError(
                errors,
                fieldPath,
                QStringLiteral("state.nested-value-type"),
                QStringLiteral("The nested value has the wrong contract type.")
            );
        }
    }
    return object;
}

void validateNumericField(
    const QJsonObject &object,
    const QString &key,
    const double minimum,
    const double maximum,
    const QString &path,
    ValidationErrors &errors
)
{
    if (!object.contains(key)) return;
    const auto value = object.value(key);
    if (!value.isDouble() || value.toDouble() < minimum
        || value.toDouble() > maximum) {
        addError(errors, path + QLatin1Char('.') + key, QStringLiteral("state.nested-value-range"), QStringLiteral("The nested value is outside the pinned Hyprland range."));
    }
}

void validateChoiceField(
    const QJsonObject &object,
    const QString &key,
    const QSet<QString> &choices,
    const QString &path,
    ValidationErrors &errors
)
{
    if (object.contains(key)
        && (!object.value(key).isString()
            || !choices.contains(object.value(key).toString()))) {
        addError(errors, path + QLatin1Char('.') + key, QStringLiteral("state.nested-value-choice"), QStringLiteral("The nested value is not in the pinned Hyprland enum."));
    }
}

void validateRe2Expression(
    const QJsonObject &object,
    const QString &key,
    const QString &path,
    ValidationErrors &errors
)
{
    if (!object.contains(key) || !object.value(key).isString()) return;
    auto expression = object.value(key).toString();
    if (expression.startsWith(QStringLiteral("negative:"))) {
        expression.remove(0, QStringLiteral("negative:").size());
    }
    if (expression.isEmpty()) {
        addError(
            errors,
            path + QLatin1Char('.') + key,
            QStringLiteral("state.invalid-regex"),
            QStringLiteral("A rule matcher must contain a non-empty RE2 expression.")
        );
        return;
    }
    const auto encoded = expression.toUtf8();
    re2::RE2::Options options;
    options.set_log_errors(false);
    const re2::RE2 compiled(encoded.constData(), options);
    if (!compiled.ok()) {
        addError(
            errors,
            path + QLatin1Char('.') + key,
            QStringLiteral("state.invalid-regex"),
            QStringLiteral("The matcher is not valid RE2 syntax.")
        );
    }
}

void validateRe2Matchers(
    const QJsonObject &object,
    const QStringList &keys,
    const QString &path,
    ValidationErrors &errors
)
{
    for (const auto &key : keys) {
        validateRe2Expression(object, key, path, errors);
    }
}

void validateWorkspaceRuleSelector(
    const QString &selector,
    const QString &recordId,
    const QString &path,
    ValidationErrors &errors
);
void validateWorkspaceSpec(
    const QString &specification,
    const QString &path,
    ValidationErrors &errors,
    bool requireExisting
);
void validateMonitorSpec(
    const QString &specification,
    const QString &path,
    ValidationErrors &errors
);

[[nodiscard]] bool isWindowAnimationStyle(const QString &style)
{
    static const QRegularExpression expression(
        QStringLiteral("^(?:|slide(?: (?:top|bottom|left|right))?|gnome|gnomed|popin(?: (?:0|[1-9][0-9]?|100)%)?)$")
    );
    return expression.match(style).hasMatch();
}

void validateStaticMonitorSelector(
    const QString &selector,
    const QString &path,
    ValidationErrors &errors
)
{
    static const QRegularExpression output(
        QStringLiteral("^[A-Za-z][A-Za-z0-9_.-]{0,127}$")
    );
    static const QSet<QString> dynamic{
        QStringLiteral("current"), QStringLiteral("left"),
        QStringLiteral("right"), QStringLiteral("up"),
        QStringLiteral("down")
    };
    if (output.match(selector).hasMatch() && !dynamic.contains(selector)) return;
    if (selector.startsWith(QStringLiteral("desc:"))) {
        const auto description = selector.sliced(5);
        if (!description.isEmpty() && description.size() <= 256
            && description == description.trimmed()
            && !hasDisallowedCharacter(description, false)) {
            return;
        }
    }
    addError(errors, path, QStringLiteral("state.invalid-static-monitor-selector"), QStringLiteral("A static monitor selector must be an exact output token or a nonempty canonical description selector."));
}

[[nodiscard]] bool isWorkspaceAnimationStyle(const QString &style)
{
    static const QRegularExpression expression(
        QStringLiteral("^(?:|fade|(?:slide|slidevert|slidefade|slidefadevert)(?: (?:top|bottom|left|right))?(?: (?:0|[1-9][0-9]?|100)%)?)$")
    );
    return expression.match(style).hasMatch();
}

[[nodiscard]] bool isLayerAnimationStyle(const QString &style)
{
    static const QRegularExpression expression(
        QStringLiteral("^(?:|fade|slide(?: (?:top|bottom|left|right))?|popin(?: (?:0|[1-9][0-9]?|100)%)?)$")
    );
    return expression.match(style).hasMatch();
}

void validateDeviceOverrides(
    const QJsonObject &object,
    const QString &path,
    ValidationErrors &errors
)
{
    validateNumericField(object, QStringLiteral("sensitivity"), -1, 1, path, errors);
    validateNumericField(object, QStringLiteral("rotation"), 0, 359, path, errors);
    validateNumericField(object, QStringLiteral("repeat_rate"), 0, 200, path, errors);
    validateNumericField(object, QStringLiteral("repeat_delay"), 0, 2000, path, errors);
    validateNumericField(object, QStringLiteral("drag_lock"), 0, 2, path, errors);
    validateNumericField(object, QStringLiteral("scroll_button"), 0, 300, path, errors);
    validateNumericField(object, QStringLiteral("scroll_factor"), 0, 100, path, errors);
    validateNumericField(object, QStringLiteral("drag_3fg"), 0, 2, path, errors);
    validateNumericField(object, QStringLiteral("share_states"), 0, 2, path, errors);
    validateNumericField(object, QStringLiteral("transform"), -1, 7, path, errors);
    validateChoiceField(object, QStringLiteral("accel_profile"), {QString(), QStringLiteral("adaptive"), QStringLiteral("flat")}, path, errors);
    validateChoiceField(object, QStringLiteral("tap_button_map"), {QString(), QStringLiteral("lrm"), QStringLiteral("lmr")}, path, errors);
    validateChoiceField(object, QStringLiteral("scroll_method"), {QString(), QStringLiteral("2fg"), QStringLiteral("edge"), QStringLiteral("on_button_down"), QStringLiteral("no_scroll")}, path, errors);
}

void validateWorkspaceOverrides(
    const QJsonObject &object,
    const QString &path,
    ValidationErrors &errors
)
{
    if (object.value(QStringLiteral("animation")).isString()
        && !isWorkspaceAnimationStyle(
            object.value(QStringLiteral("animation")).toString())) {
        addError(errors, path + QStringLiteral(".animation"), QStringLiteral("state.invalid-animation-style"), QStringLiteral("The workspace rule animation style is invalid."));
    }
}

void validateWindowEffects(
    const QJsonObject &object,
    const QString &path,
    ValidationErrors &errors
)
{
    validateNumericField(object, QStringLiteral("rounding"), 0, 20, path, errors);
    validateNumericField(object, QStringLiteral("rounding_power"), 1, 10, path, errors);
    validateNumericField(object, QStringLiteral("scroll_mouse"), 0.01, 10, path, errors);
    validateNumericField(object, QStringLiteral("scroll_touchpad"), 0.01, 10, path, errors);
    validateNumericField(object, QStringLiteral("scrolling_width"), 0, 1, path, errors);
    validateNumericField(object, QStringLiteral("no_close_for"), 0, std::numeric_limits<qint32>::max(), path, errors);
    validateChoiceField(object, QStringLiteral("content"), {QStringLiteral("none"), QStringLiteral("photo"), QStringLiteral("video"), QStringLiteral("game")}, path, errors);
    validateChoiceField(object, QStringLiteral("idle_inhibit"), {QStringLiteral("none"), QStringLiteral("always"), QStringLiteral("focus"), QStringLiteral("fullscreen")}, path, errors);
    validateChoiceField(object, QStringLiteral("tonemap"), {QStringLiteral("on"), QStringLiteral("off"), QStringLiteral("clamp"), QStringLiteral("limited")}, path, errors);
    if (object.value(QStringLiteral("animation")).isString()
        && !isWindowAnimationStyle(
            object.value(QStringLiteral("animation")).toString())) {
        addError(errors, path + QStringLiteral(".animation"), QStringLiteral("state.invalid-animation-style"), QStringLiteral("The window rule animation style is invalid."));
    }
    if (object.value(QStringLiteral("tag")).isString()) {
        static const QRegularExpression tag(
            QStringLiteral("^[+-]?[A-Za-z0-9][A-Za-z0-9._:-]{0,127}$")
        );
        if (!tag.match(object.value(QStringLiteral("tag")).toString()).hasMatch()) {
            addError(errors, path + QStringLiteral(".tag"), QStringLiteral("state.invalid-window-tag"), QStringLiteral("A window rule tag requires a nonempty canonical base name."));
        }
    }
    const auto validateTarget = [&](const QString &field, const bool monitor) {
        if (!object.contains(field) || !object.value(field).isObject()) return;
        const auto targetObject = object.value(field).toObject();
        const auto targetPath = path + QLatin1Char('.') + field;
        if (targetObject.size() != 2
            || !targetObject.value(QStringLiteral("target")).isString()
            || !targetObject.value(QStringLiteral("silent")).isBool()) {
            addError(errors, targetPath, QStringLiteral("state.nested-value-type"), QStringLiteral("A target effect requires exact target and silent fields."));
            return;
        }
        for (auto iterator = targetObject.constBegin();
             iterator != targetObject.constEnd(); ++iterator) {
            if (iterator.key() != QStringLiteral("target")
                && iterator.key() != QStringLiteral("silent")) {
                addError(errors, targetPath + QLatin1Char('.') + iterator.key(), QStringLiteral("state.unknown-field"), QStringLiteral("The target effect field is not supported."));
            }
        }
        const auto specification =
            targetObject.value(QStringLiteral("target")).toString();
        if (specification == QStringLiteral("unset")) return;
        if (monitor) {
            validateMonitorSpec(
                specification, targetPath + QStringLiteral(".target"), errors
            );
        } else if (specification.isEmpty()) {
            addError(errors, targetPath + QStringLiteral(".target"), QStringLiteral("state.invalid-workspace-spec"), QStringLiteral("A workspace target cannot be empty."));
        } else {
            validateWorkspaceSpec(
                specification, targetPath + QStringLiteral(".target"),
                errors, false
            );
        }
    };
    validateTarget(QStringLiteral("monitor"), true);
    validateTarget(QStringLiteral("workspace"), false);
}

void validateWindowMatch(
    const QJsonObject &object,
    const QString &path,
    ValidationErrors &errors
)
{
    validateNumericField(
        object, QStringLiteral("fullscreen_state_internal"), 0, 2,
        path, errors
    );
    validateNumericField(
        object, QStringLiteral("fullscreen_state_client"), 0, 2,
        path, errors
    );
    validateRe2Matchers(
        object,
        {
            QStringLiteral("class"), QStringLiteral("title"),
            QStringLiteral("initial_class"),
            QStringLiteral("initial_title"), QStringLiteral("content"),
            QStringLiteral("xdg_tag"), QStringLiteral("namespace")
        },
        path,
        errors
    );
    if (object.value(QStringLiteral("workspace")).isString()) {
        validateWorkspaceRuleSelector(
            object.value(QStringLiteral("workspace")).toString(),
            {},
            path + QStringLiteral(".workspace"), errors
        );
    }
}

void validateLayerEffects(
    const QJsonObject &object,
    const QString &path,
    ValidationErrors &errors
)
{
    validateNumericField(object, QStringLiteral("ignore_alpha"), 0, 1, path, errors);
    validateNumericField(object, QStringLiteral("above_lock"), 0, 2, path, errors);
    if (object.value(QStringLiteral("animation")).isString()
        && !isLayerAnimationStyle(
            object.value(QStringLiteral("animation")).toString())) {
        addError(errors, path + QStringLiteral(".animation"), QStringLiteral("state.invalid-animation-style"), QStringLiteral("The layer rule animation style is invalid."));
    }
}

void validateLayerMatch(
    const QJsonObject &object,
    const QString &path,
    ValidationErrors &errors
)
{
    validateRe2Expression(
        object, QStringLiteral("namespace"), path, errors
    );
}

[[nodiscard]] const NestedFieldTable &deviceFields()
{
    static const NestedFieldTable fields{
        {QStringLiteral("sensitivity"), NestedValueKind::Number},
        {QStringLiteral("accel_profile"), NestedValueKind::ShortString},
        {QStringLiteral("rotation"), NestedValueKind::Integer},
        {QStringLiteral("kb_file"), NestedValueKind::ShortString},
        {QStringLiteral("kb_layout"), NestedValueKind::ShortString},
        {QStringLiteral("kb_variant"), NestedValueKind::ShortString},
        {QStringLiteral("kb_options"), NestedValueKind::ShortString},
        {QStringLiteral("kb_rules"), NestedValueKind::ShortString},
        {QStringLiteral("kb_model"), NestedValueKind::ShortString},
        {QStringLiteral("repeat_rate"), NestedValueKind::Integer},
        {QStringLiteral("repeat_delay"), NestedValueKind::Integer},
        {QStringLiteral("natural_scroll"), NestedValueKind::Boolean},
        {QStringLiteral("tap_button_map"), NestedValueKind::ShortString},
        {QStringLiteral("numlock_by_default"), NestedValueKind::Boolean},
        {QStringLiteral("resolve_binds_by_sym"), NestedValueKind::Boolean},
        {QStringLiteral("disable_while_typing"), NestedValueKind::Boolean},
        {QStringLiteral("clickfinger_behavior"), NestedValueKind::Boolean},
        {QStringLiteral("middle_button_emulation"), NestedValueKind::Boolean},
        {QStringLiteral("tap_to_click"), NestedValueKind::Boolean},
        {QStringLiteral("tap_and_drag"), NestedValueKind::Boolean},
        {QStringLiteral("drag_lock"), NestedValueKind::Integer},
        {QStringLiteral("left_handed"), NestedValueKind::Boolean},
        {QStringLiteral("scroll_method"), NestedValueKind::ShortString},
        {QStringLiteral("scroll_button"), NestedValueKind::Integer},
        {QStringLiteral("scroll_button_lock"), NestedValueKind::Boolean},
        {QStringLiteral("scroll_factor"), NestedValueKind::Number},
        {QStringLiteral("transform"), NestedValueKind::Integer},
        {QStringLiteral("region_position"), NestedValueKind::Vector2},
        {QStringLiteral("absolute_region_position"), NestedValueKind::Boolean},
        {QStringLiteral("region_size"), NestedValueKind::Vector2},
        {QStringLiteral("relative_input"), NestedValueKind::Boolean},
        {QStringLiteral("active_area_position"), NestedValueKind::Vector2},
        {QStringLiteral("active_area_size"), NestedValueKind::Vector2},
        {QStringLiteral("flip_x"), NestedValueKind::Boolean},
        {QStringLiteral("flip_y"), NestedValueKind::Boolean},
        {QStringLiteral("drag_3fg"), NestedValueKind::Integer},
        {QStringLiteral("keybinds"), NestedValueKind::Boolean},
        {QStringLiteral("share_states"), NestedValueKind::Integer},
        {QStringLiteral("release_pressed_on_close"), NestedValueKind::Boolean},
    };
    return fields;
}

[[nodiscard]] const NestedFieldTable &workspaceFields()
{
    static const NestedFieldTable fields{
        {QStringLiteral("gaps_in"), NestedValueKind::CssGap},
        {QStringLiteral("gaps_out"), NestedValueKind::CssGap},
        {QStringLiteral("float_gaps"), NestedValueKind::CssGap},
        {QStringLiteral("border_size"), NestedValueKind::Integer},
        {QStringLiteral("no_border"), NestedValueKind::Boolean},
        {QStringLiteral("no_rounding"), NestedValueKind::Boolean},
        {QStringLiteral("decorate"), NestedValueKind::Boolean},
        {QStringLiteral("no_shadow"), NestedValueKind::Boolean},
        {QStringLiteral("default_name"), NestedValueKind::ShortString},
        {QStringLiteral("animation"), NestedValueKind::ShortString},
        {QStringLiteral("layout_opts"), NestedValueKind::LayoutOptions},
    };
    return fields;
}

[[nodiscard]] const NestedFieldTable &windowMatchFields()
{
    static const NestedFieldTable fields{
        {QStringLiteral("class"), NestedValueKind::RegexString},
        {QStringLiteral("title"), NestedValueKind::RegexString},
        {QStringLiteral("initial_class"), NestedValueKind::RegexString},
        {QStringLiteral("initial_title"), NestedValueKind::RegexString},
        {QStringLiteral("tag"), NestedValueKind::NonEmptyString},
        {QStringLiteral("xwayland"), NestedValueKind::Boolean},
        {QStringLiteral("float"), NestedValueKind::Boolean},
        {QStringLiteral("fullscreen"), NestedValueKind::Boolean},
        {QStringLiteral("pin"), NestedValueKind::Boolean},
        {QStringLiteral("focus"), NestedValueKind::Boolean},
        {QStringLiteral("group"), NestedValueKind::Boolean},
        {QStringLiteral("modal"), NestedValueKind::Boolean},
        {QStringLiteral("fullscreen_state_internal"), NestedValueKind::Integer},
        {QStringLiteral("fullscreen_state_client"), NestedValueKind::Integer},
        {QStringLiteral("workspace"), NestedValueKind::NonEmptyString},
        {QStringLiteral("content"), NestedValueKind::RegexString},
        {QStringLiteral("xdg_tag"), NestedValueKind::RegexString},
        {QStringLiteral("namespace"), NestedValueKind::RegexString},
    };
    return fields;
}

[[nodiscard]] const NestedFieldTable &windowEffectFields()
{
    static const NestedFieldTable fields{
        {QStringLiteral("float"), NestedValueKind::Boolean},
        {QStringLiteral("tile"), NestedValueKind::Boolean},
        {QStringLiteral("fullscreen"), NestedValueKind::Boolean},
        {QStringLiteral("maximize"), NestedValueKind::Boolean},
        {QStringLiteral("center"), NestedValueKind::Boolean},
        {QStringLiteral("pseudo"), NestedValueKind::Boolean},
        {QStringLiteral("no_initial_focus"), NestedValueKind::Boolean},
        {QStringLiteral("pin"), NestedValueKind::Boolean},
        {QStringLiteral("fullscreen_state"), NestedValueKind::WindowFullscreenState},
        {QStringLiteral("move"), NestedValueKind::ExpressionVector2},
        {QStringLiteral("size"), NestedValueKind::ExpressionVector2},
        {QStringLiteral("monitor"), NestedValueKind::Object},
        {QStringLiteral("workspace"), NestedValueKind::Object},
        {QStringLiteral("suppress_event"), NestedValueKind::SuppressEvents},
        {QStringLiteral("content"), NestedValueKind::ShortString},
        {QStringLiteral("no_close_for"), NestedValueKind::Integer},
        {QStringLiteral("scrolling_width"), NestedValueKind::Number},
        {QStringLiteral("rounding"), NestedValueKind::Integer},
        {QStringLiteral("border_size"), NestedValueKind::Integer},
        {QStringLiteral("rounding_power"), NestedValueKind::Number},
        {QStringLiteral("scroll_mouse"), NestedValueKind::Number},
        {QStringLiteral("scroll_touchpad"), NestedValueKind::Number},
        {QStringLiteral("animation"), NestedValueKind::ShortString},
        {QStringLiteral("idle_inhibit"), NestedValueKind::ShortString},
        {QStringLiteral("opacity"), NestedValueKind::WindowOpacity},
        {QStringLiteral("tag"), NestedValueKind::ShortString},
        {QStringLiteral("max_size"), NestedValueKind::ExpressionVector2},
        {QStringLiteral("min_size"), NestedValueKind::ExpressionVector2},
        {QStringLiteral("border_color"), NestedValueKind::Gradient},
        {QStringLiteral("persistent_size"), NestedValueKind::Boolean},
        {QStringLiteral("allows_input"), NestedValueKind::Boolean},
        {QStringLiteral("dim_around"), NestedValueKind::Boolean},
        {QStringLiteral("decorate"), NestedValueKind::Boolean},
        {QStringLiteral("focus_on_activate"), NestedValueKind::Boolean},
        {QStringLiteral("keep_aspect_ratio"), NestedValueKind::Boolean},
        {QStringLiteral("nearest_neighbor"), NestedValueKind::Boolean},
        {QStringLiteral("no_anim"), NestedValueKind::Boolean},
        {QStringLiteral("no_blur"), NestedValueKind::Boolean},
        {QStringLiteral("no_dim"), NestedValueKind::Boolean},
        {QStringLiteral("no_focus"), NestedValueKind::Boolean},
        {QStringLiteral("no_follow_mouse"), NestedValueKind::Boolean},
        {QStringLiteral("no_max_size"), NestedValueKind::Boolean},
        {QStringLiteral("no_shadow"), NestedValueKind::Boolean},
        {QStringLiteral("no_shortcuts_inhibit"), NestedValueKind::Boolean},
        {QStringLiteral("opaque"), NestedValueKind::Boolean},
        {QStringLiteral("force_rgbx"), NestedValueKind::Boolean},
        {QStringLiteral("sync_fullscreen"), NestedValueKind::Boolean},
        {QStringLiteral("immediate"), NestedValueKind::Boolean},
        {QStringLiteral("xray"), NestedValueKind::Boolean},
        {QStringLiteral("render_unfocused"), NestedValueKind::Boolean},
        {QStringLiteral("no_screen_share"), NestedValueKind::Boolean},
        {QStringLiteral("no_vrr"), NestedValueKind::Boolean},
        {QStringLiteral("no_auto_hdr"), NestedValueKind::Boolean},
        {QStringLiteral("stay_focused"), NestedValueKind::Boolean},
        {QStringLiteral("confine_pointer"), NestedValueKind::Boolean},
        {QStringLiteral("tonemap"), NestedValueKind::ShortString},
    };
    return fields;
}

[[nodiscard]] const NestedFieldTable &layerMatchFields()
{
    static const NestedFieldTable fields{
        {QStringLiteral("namespace"), NestedValueKind::RegexString},
    };
    return fields;
}

[[nodiscard]] const NestedFieldTable &layerEffectFields()
{
    static const NestedFieldTable fields{
        {QStringLiteral("no_anim"), NestedValueKind::Boolean},
        {QStringLiteral("blur"), NestedValueKind::Boolean},
        {QStringLiteral("blur_popups"), NestedValueKind::Boolean},
        {QStringLiteral("ignore_alpha"), NestedValueKind::Number},
        {QStringLiteral("dim_around"), NestedValueKind::Boolean},
        {QStringLiteral("xray"), NestedValueKind::Boolean},
        {QStringLiteral("animation"), NestedValueKind::ShortString},
        {QStringLiteral("order"), NestedValueKind::Integer},
        {QStringLiteral("above_lock"), NestedValueKind::Integer},
        {QStringLiteral("no_screen_share"), NestedValueKind::Boolean},
    };
    return fields;
}

template<typename T, typename Parser>
[[nodiscard]] QVector<T> parseRecordArray(
    const QJsonObject &root,
    const QString &key,
    const qsizetype maximumItems,
    ValidationErrors &errors,
    Parser parser
)
{
    QVector<T> result;
    const auto array = readArray(root, key, QStringLiteral("$"), errors);
    const auto arrayPath = QStringLiteral("$.") + key;
    if (array.size() > maximumItems) {
        addError(
            errors,
            arrayPath,
            QStringLiteral("state.collection-limit"),
            QStringLiteral("The complex collection exceeds its item limit.")
        );
    }
    QSet<QString> ids;
    for (qsizetype index = 0;
         index < std::min(array.size(), maximumItems);
         ++index) {
        const auto recordPath = arrayPath + QLatin1Char('[')
            + QString::number(index) + QLatin1Char(']');
        if (!array.at(index).isObject()) {
            addError(
                errors,
                recordPath,
                QStringLiteral("state.object-required"),
                QStringLiteral("A complex record object is required.")
            );
            continue;
        }
        auto record = parser(array.at(index).toObject(), recordPath, errors);
        if (ids.contains(record.id)) {
            addError(
                errors,
                recordPath + QStringLiteral(".id"),
                QStringLiteral("state.duplicate-id"),
                QStringLiteral("Record IDs must be unique inside a collection.")
            );
        }
        ids.insert(record.id);
        result.append(std::move(record));
    }
    return result;
}

struct ParsedTargetVersion final {
    SemanticVersion version;
    bool patchSpecified = false;
};

[[nodiscard]] std::optional<ParsedTargetVersion> parseTargetVersion(
    const QString &text
)
{
    static const QRegularExpression expression(QStringLiteral(
        "^(0|[1-9][0-9]*)\\.(0|[1-9][0-9]*)(?:\\.(0|[1-9][0-9]*))?$"
    ));
    const auto match = expression.match(text);
    if (!match.hasMatch()) {
        return std::nullopt;
    }
    bool majorOk = false;
    bool minorOk = false;
    bool patchOk = true;
    const auto major = match.captured(1).toUInt(&majorOk);
    const auto minor = match.captured(2).toUInt(&minorOk);
    quint32 patch = 0;
    const auto patchSpecified = !match.captured(3).isEmpty();
    if (patchSpecified) {
        patch = match.captured(3).toUInt(&patchOk);
    }
    if (!majorOk || !minorOk || !patchOk) {
        return std::nullopt;
    }
    return ParsedTargetVersion{
        .version = {major, minor, patch},
        .patchSpecified = patchSpecified,
    };
}

[[nodiscard]] std::optional<quint64> parseRevision(const QString &text)
{
    static const QRegularExpression expression(
        QStringLiteral("^(0|[1-9][0-9]{0,19})$")
    );
    if (!expression.match(text).hasMatch()) {
        return std::nullopt;
    }
    bool ok = false;
    const auto value = text.toULongLong(&ok, 10);
    return ok ? std::optional<quint64>(value) : std::nullopt;
}

[[nodiscard]] QJsonArray stringArray(const QStringList &values)
{
    QJsonArray result;
    for (const auto &value : values) {
        result.append(value);
    }
    return result;
}

[[nodiscard]] std::optional<QString> canonicalBindingKey(
    const QString &key,
    const QString &path,
    ValidationErrors &errors,
    const bool requireCanonical,
    const bool allowBindingSpecials = true
)
{
    static const QRegularExpression lexical(
        QStringLiteral("^[^\\x{0000}-\\x{0020}\\x{007f}+,]{1,64}$")
    );
    if (!lexical.match(key).hasMatch()) {
        addError(errors, path, QStringLiteral("state.invalid-key"), QStringLiteral("A binding key must be one canonical single-key token without whitespace or chord separators."));
        return std::nullopt;
    }
    static const QSet<QString> wheelTokens{
        QStringLiteral("mouse_down"), QStringLiteral("mouse_up"),
        QStringLiteral("mouse_left"), QStringLiteral("mouse_right"),
    };
    if (wheelTokens.contains(key) || key == QStringLiteral("catchall")) {
        if (!allowBindingSpecials) {
            addError(errors, path, QStringLiteral("state.invalid-action-key"), QStringLiteral("Action key payloads do not accept binding-only wheel or catchall tokens."));
            return std::nullopt;
        }
        return key;
    }
    static const QRegularExpression numericToken(
        QStringLiteral("^(code|mouse):(0|[1-9][0-9]{0,9})$")
    );
    const auto numeric = numericToken.match(key);
    if (numeric.hasMatch()) {
        bool converted = false;
        const auto value = numeric.captured(2).toULongLong(&converted, 10);
        const auto codeMaximum = allowBindingSpecials
            ? 4294967294ULL
            : static_cast<quint64>(std::numeric_limits<qint32>::max());
        const auto valid = converted
            && ((numeric.captured(1) == QStringLiteral("code")
                    && value <= codeMaximum)
                || (numeric.captured(1) == QStringLiteral("mouse")
                    && value >= 272 && value <= 767));
        if (!valid) {
            addError(errors, path, QStringLiteral("state.invalid-key"), QStringLiteral("The numeric binding key is outside its reviewed domain."));
            return std::nullopt;
        }
        return key;
    }

    const auto encoded = key.toUtf8();
    const auto symbol = xkb_keysym_from_name(
        encoded.constData(), XKB_KEYSYM_CASE_INSENSITIVE
    );
    if (symbol == XKB_KEY_NoSymbol) {
        addError(errors, path, QStringLiteral("state.invalid-key"), QStringLiteral("The binding key is not a recognized XKB keysym name."));
        return std::nullopt;
    }
    char canonicalBuffer[128]{};
    const auto length = xkb_keysym_get_name(
        symbol, canonicalBuffer, sizeof(canonicalBuffer)
    );
    if (length <= 0 || length >= static_cast<int>(sizeof(canonicalBuffer))) {
        addError(errors, path, QStringLiteral("state.invalid-key"), QStringLiteral("The binding key has no bounded canonical XKB name."));
        return std::nullopt;
    }
    const auto canonical = QString::fromLatin1(canonicalBuffer, length);
    if (requireCanonical && key != canonical) {
        addError(errors, path, QStringLiteral("state.non-canonical-key"), QStringLiteral("Persisted binding keys must use xkbcommon's canonical keysym name."));
        return std::nullopt;
    }
    return canonical;
}

void validateCanonicalActionModifiers(
    const QJsonObject &arguments,
    const QString &path,
    ValidationErrors &errors
)
{
    const auto value = arguments.value(QStringLiteral("mods"));
    if (!value.isString()) return;
    const auto authored = value.toString();
    static const QStringList canonicalOrder{
        QStringLiteral("SHIFT"), QStringLiteral("CAPS"),
        QStringLiteral("CTRL"), QStringLiteral("ALT"),
        QStringLiteral("MOD2"), QStringLiteral("MOD3"),
        QStringLiteral("SUPER"), QStringLiteral("MOD5")
    };
    const auto tokens = authored.isEmpty()
        ? QStringList{}
        : authored.split(QLatin1Char(' '), Qt::KeepEmptyParts);
    QSet<QString> selected;
    bool valid = tokens.size() <= canonicalOrder.size();
    qsizetype previous = -1;
    for (const auto &token : tokens) {
        const auto position = canonicalOrder.indexOf(token);
        if (position < 0 || position <= previous || selected.contains(token)) {
            valid = false;
            break;
        }
        previous = position;
        selected.insert(token);
    }
    QStringList canonical;
    for (const auto &token : canonicalOrder) {
        if (selected.contains(token)) canonical.append(token);
    }
    if (!valid || authored != canonical.join(QLatin1Char(' '))) {
        addError(
            errors,
            path + QStringLiteral(".mods"),
            QStringLiteral("state.non-canonical-action-modifiers"),
            QStringLiteral("Action modifiers must be unique canonical tokens in the pinned order.")
        );
    }
}

void validateWindowSelector(
    const QString &selector,
    const QString &path,
    ValidationErrors &errors
)
{
    static const QSet<QString> exact{
        QStringLiteral("active"), QStringLiteral("floating"),
        QStringLiteral("tiled")
    };
    if (exact.contains(selector)) return;

    static const QStringList regexPrefixes{
        QStringLiteral("class:"), QStringLiteral("initialclass:"),
        QStringLiteral("title:"), QStringLiteral("initialtitle:"),
        QStringLiteral("tag:")
    };
    for (const auto &prefix : regexPrefixes) {
        if (!selector.startsWith(prefix)) continue;
        const auto expression = selector.sliced(prefix.size());
        const auto encoded = expression.toUtf8();
        re2::RE2::Options options;
        options.set_log_errors(false);
        const re2::RE2 compiled(encoded.constData(), options);
        if (!expression.isEmpty() && compiled.ok()) return;
        addError(errors, path, QStringLiteral("state.invalid-window-selector"), QStringLiteral("The action window selector contains invalid or empty RE2 syntax."));
        return;
    }

    static const QRegularExpression stableId(
        QStringLiteral("^stableid:[1-9a-f][0-9a-f]{0,15}$")
    );
    static const QRegularExpression address(
        QStringLiteral("^address:0x[1-9a-f][0-9a-f]{0,15}$")
    );
    static const QRegularExpression pid(
        QStringLiteral("^pid:[1-9][0-9]{0,9}$")
    );
    bool validPid = false;
    if (pid.match(selector).hasMatch()) {
        const auto value = selector.sliced(QStringLiteral("pid:").size())
                               .toLongLong(&validPid);
        validPid = validPid && value <= std::numeric_limits<qint32>::max();
    }
    if (stableId.match(selector).hasMatch()
        || address.match(selector).hasMatch() || validPid) {
        return;
    }
    addError(errors, path, QStringLiteral("state.invalid-window-selector"), QStringLiteral("The action window selector is outside the pinned ViewQuery grammar."));
}

void validateWorkspaceRuleSelector(
    const QString &selector,
    const QString &recordId,
    const QString &path,
    ValidationErrors &errors
)
{
    if (selector == QLatin1String(sharedSpacingWorkspaceRuleSelector)) {
        if (recordId == QLatin1String(sharedSpacingWorkspaceRuleId)) {
            return;
        }
        addError(
            errors,
            path,
            QStringLiteral("state.reserved-workspace-selector"),
            QStringLiteral(
                "The maximized-workspace selector is reserved for HyprShelld."
            )
        );
        return;
    }
    static const QRegularExpression decimal(
        QStringLiteral("^[1-9][0-9]{0,9}$")
    );
    if (decimal.match(selector).hasMatch()) {
        bool converted = false;
        const auto id = selector.toLongLong(&converted, 10);
        if (converted && id <= std::numeric_limits<qint32>::max()) return;
    }
    static const QRegularExpression named(
        QStringLiteral("^(?:name:[A-Za-z0-9_][A-Za-z0-9_.-]{0,127}|special(?::[A-Za-z0-9_][A-Za-z0-9_.-]{0,127})?)$")
    );
    if (named.match(selector).hasMatch()) return;
    addError(
        errors,
        path,
        QStringLiteral("state.invalid-workspace-selector"),
        QStringLiteral("The workspace rule selector is outside the pinned safe v1 subset.")
    );
}

[[nodiscard]] bool isPinnedWorkspaceName(const QString &name)
{
    static const QRegularExpression token(
        QStringLiteral("^[A-Za-z0-9_][A-Za-z0-9_.-]{0,127}$")
    );
    return token.match(name).hasMatch();
}

void validateWorkspaceSpec(
    const QString &specification,
    const QString &path,
    ValidationErrors &errors,
    const bool requireExisting
)
{
    if (!requireExisting
        && (specification == QStringLiteral("previous")
            || specification == QStringLiteral("previous_per_monitor")
            || specification == QStringLiteral("next")
            || specification == QStringLiteral("empty"))) {
        return;
    }
    if (specification == QStringLiteral("special")) return;
    if (specification.startsWith(QStringLiteral("name:"))
        && isPinnedWorkspaceName(
            specification.sliced(QStringLiteral("name:").size()))) {
        return;
    }
    if (specification.startsWith(QStringLiteral("special:"))
        && isPinnedWorkspaceName(
            specification.sliced(QStringLiteral("special:").size()))) {
        return;
    }
    static const QRegularExpression decimal(
        QStringLiteral("^[1-9][0-9]{0,9}$")
    );
    bool converted = false;
    const auto id = specification.toLongLong(&converted, 10);
    if (decimal.match(specification).hasMatch() && converted
        && id <= std::numeric_limits<qint32>::max()) {
        return;
    }
    addError(errors, path, QStringLiteral("state.invalid-workspace-spec"), QStringLiteral("The action workspace specification is outside the safe pinned subset."));
}

void validateMonitorSpec(
    const QString &specification,
    const QString &path,
    ValidationErrors &errors
)
{
    static const QSet<QString> relativeNames{
        QStringLiteral("current"), QStringLiteral("left"),
        QStringLiteral("right"), QStringLiteral("up"),
        QStringLiteral("down")
    };
    if (relativeNames.contains(specification)) return;

    static const QRegularExpression numeric(
        QStringLiteral("^(?:0|[1-9][0-9]{0,9}|[+-][1-9][0-9]{0,9})$")
    );
    if (numeric.match(specification).hasMatch()) {
        bool converted = false;
        const auto value = specification.toLongLong(&converted, 10);
        if (converted && value >= -std::numeric_limits<qint32>::max()
            && value <= std::numeric_limits<qint32>::max()) {
            return;
        }
    }
    static const QRegularExpression output(
        QStringLiteral("^[A-Za-z][A-Za-z0-9_.-]{0,127}$")
    );
    if (output.match(specification).hasMatch()) return;
    if (specification.startsWith(QStringLiteral("desc:"))) {
        const auto description = specification.sliced(5);
        if (!description.trimmed().isEmpty() && description.size() <= 256
            && description == description.trimmed()
            && !hasDisallowedCharacter(description, false)) {
            return;
        }
    }
    addError(errors, path, QStringLiteral("state.invalid-monitor-spec"), QStringLiteral("The action monitor specification is outside the canonical pinned subset."));
}

void validateDispatcherPayloadSemantics(
    const QString &action,
    const QJsonObject &arguments,
    const QString &path,
    ValidationErrors &errors
)
{
    for (const auto &field : {QStringLiteral("window"), QStringLiteral("target")}) {
        if (arguments.value(field).isString()) {
            validateWindowSelector(
                arguments.value(field).toString(),
                path + QLatin1Char('.') + field,
                errors
            );
        }
    }
    if (arguments.value(QStringLiteral("workspace")).isString()) {
        validateWorkspaceSpec(
            arguments.value(QStringLiteral("workspace")).toString(),
            path + QStringLiteral(".workspace"), errors,
            action == QStringLiteral("workspace.rename")
                || action == QStringLiteral("workspace.change_id")
                || action == QStringLiteral("workspace.move")
        );
    }
    for (const auto &field : {
             QStringLiteral("monitor"), QStringLiteral("monitor1"),
             QStringLiteral("monitor2")}) {
        if (arguments.value(field).isString()) {
            validateMonitorSpec(
                arguments.value(field).toString(),
                path + QLatin1Char('.') + field,
                errors
            );
        }
    }
    if (action == QStringLiteral("window.tag")
        && arguments.value(QStringLiteral("tag")).isString()) {
        static const QRegularExpression tag(
            QStringLiteral("^[+-]?[A-Za-z0-9][A-Za-z0-9._:-]{0,127}$")
        );
        if (!tag.match(arguments.value(QStringLiteral("tag")).toString())
                 .hasMatch()) {
            addError(errors, path + QStringLiteral(".tag"), QStringLiteral("state.invalid-window-tag"), QStringLiteral("A managed window tag requires a nonempty canonical base name."));
        }
    }
    if (action == QStringLiteral("send_shortcut")
        || action == QStringLiteral("send_key_state")) {
        validateCanonicalActionModifiers(arguments, path, errors);
        if (arguments.value(QStringLiteral("key")).isString()) {
            Q_UNUSED(canonicalBindingKey(
                arguments.value(QStringLiteral("key")).toString(),
                path + QStringLiteral(".key"), errors, true, false
            ));
        }
    }
    if (action == QStringLiteral("global")) {
        const auto value = arguments.value(QStringLiteral("name"));
        if (!value.isString()) return;
        static const QRegularExpression globalName(
            QStringLiteral("^[^:\\x{0000}-\\x{0020}\\x{007f}]{1,128}:[^:\\x{0000}-\\x{0020}\\x{007f}]{1,128}$")
        );
        if (!globalName.match(value.toString()).hasMatch()) {
            addError(errors, path + QStringLiteral(".name"), QStringLiteral("state.invalid-global-name"), QStringLiteral("A global shortcut name must be canonical appid:name with one non-empty colon-separated pair."));
        }
    }
}

void insertFields(QJsonObject &target, const QJsonObject &source)
{
    for (auto iterator = source.constBegin(); iterator != source.constEnd();
         ++iterator) {
        target.insert(iterator.key(), iterator.value());
    }
}

void validateActiveV1Authorities(
    const Catalog &catalog,
    const ActionCatalog &actionCatalog,
    ValidationErrors &errors
)
{
    const auto exactCatalog = catalog.contractVersion
            == currentCatalogContractVersion
        && catalog.digest == QLatin1String(reviewedCatalogDigest)
        && catalog.sourceManifestDigest.isEmpty()
        && catalog.hyprland.reviewedVersion == SemanticVersion{0, 56, 1}
        && catalog.hyprland.reviewedTag == QStringLiteral("v0.56.1")
        && catalog.hyprland.reviewedCommit
            == QStringLiteral("5c9377c15f85c50648f35ca5a213754f95b93ca0")
        && catalog.hyprland.minimumPatch == 0
        && !catalog.hyprland.maximumPatch.has_value();
    if (!exactCatalog) {
        addError(
            errors,
            QStringLiteral("$.catalogDigest"),
            QStringLiteral("state.active-v1-catalog-authority-required"),
            QStringLiteral(
                "Production desired-state parsing requires the exact active v1 scalar authority."
            )
        );
    }

    const auto exactActions = actionCatalog.contractVersion
            == currentActionCatalogContractVersion
        && actionCatalog.digest == QLatin1String(reviewedActionCatalogDigest)
        && actionCatalog.sourceManifestDigest.isEmpty()
        && actionCatalog.reviewedVersion == SemanticVersion{0, 56, 1}
        && actionCatalog.reviewedTag == QStringLiteral("v0.56.1")
        && actionCatalog.reviewedCommit
            == QStringLiteral("5c9377c15f85c50648f35ca5a213754f95b93ca0")
        && actionCatalog.minimumPatch == 0
        && !actionCatalog.maximumPatch.has_value();
    if (!exactActions) {
        addError(
            errors,
            QStringLiteral("$.actionCatalogDigest"),
            QStringLiteral("state.active-v1-action-authority-required"),
            QStringLiteral(
                "Production desired-state parsing requires the exact active v1 action authority."
            )
        );
    }
}

void validateDormantV2Authorities(
    const Catalog &catalog,
    const ActionCatalog &actionCatalog,
    ValidationErrors &errors
)
{
    const auto exactCatalog = catalog.contractVersion
            == dormantCatalogV2ContractVersion
        && catalog.digest == QLatin1String(dormantReviewedCatalogV2Digest)
        && catalog.sourceManifestDigest
            == QLatin1String(dormantReviewedSourceManifestDigest)
        && catalog.hyprland.reviewedVersion == SemanticVersion{0, 56, 2}
        && catalog.hyprland.reviewedTag == QStringLiteral("v0.56.2")
        && catalog.hyprland.reviewedCommit
            == QStringLiteral("efb50993780079460b0cbed1363e2166a2de1d9f")
        && catalog.hyprland.minimumPatch == 2
        && catalog.hyprland.maximumPatch == std::optional<quint32>{2};
    if (!exactCatalog) {
        addError(
            errors,
            QStringLiteral("$.catalogDigest"),
            QStringLiteral("state.dormant-v2-catalog-authority-required"),
            QStringLiteral(
                "The dormant v2 envelope requires the exact reviewed v2 scalar authority."
            )
        );
    }

    const auto exactActions = actionCatalog.contractVersion
            == dormantActionCatalogV2ContractVersion
        && actionCatalog.digest
            == QLatin1String(dormantReviewedActionCatalogV2Digest)
        && actionCatalog.sourceManifestDigest
            == QLatin1String(dormantReviewedSourceManifestDigest)
        && actionCatalog.reviewedVersion == SemanticVersion{0, 56, 2}
        && actionCatalog.reviewedTag == QStringLiteral("v0.56.2")
        && actionCatalog.reviewedCommit
            == QStringLiteral("efb50993780079460b0cbed1363e2166a2de1d9f")
        && actionCatalog.minimumPatch == 2
        && actionCatalog.maximumPatch == std::optional<quint32>{2};
    if (!exactActions) {
        addError(
            errors,
            QStringLiteral("$.actionCatalogDigest"),
            QStringLiteral("state.dormant-v2-action-authority-required"),
            QStringLiteral(
                "The dormant v2 envelope requires its exact reviewed v2 action authority."
            )
        );
    }

    if (catalog.sourceManifestDigest != actionCatalog.sourceManifestDigest) {
        addError(
            errors,
            QStringLiteral("$.sourceManifestDigest"),
            QStringLiteral("state.dormant-v2-source-authority-mismatch"),
            QStringLiteral(
                "The dormant scalar and action authorities must bind the same exact source manifest."
            )
        );
    }
}

void validateDormantV2ProtectedWorkspaceRuleOrder(
    const QVector<WorkspaceRule> &workspaceRules,
    ValidationErrors &errors
)
{
    for (qsizetype index = 0; index + 1 < workspaceRules.size(); ++index) {
        if (workspaceRules.at(index).id
            == QLatin1String(sharedSpacingWorkspaceRuleId)) {
            addError(
                errors,
                QStringLiteral("$.workspaceRules"),
                QStringLiteral(
                    "state.dormant-v2-protected-workspace-rule-not-final"
                ),
                QStringLiteral(
                    "The protected HyprShelld workspace rule must be the final dormant v2 workspaceRules entry."
                )
            );
            return;
        }
    }
}

} // namespace

bool isCanonicalAuthorityId(const QString &authorityId)
{
    if (authorityId.size() != authorityIdHexLength) {
        return false;
    }

    bool hasNonzeroNibble = false;
    for (const auto character : authorityId) {
        const auto isDigit = character >= QLatin1Char('0')
            && character <= QLatin1Char('9');
        const auto isLowerHex = character >= QLatin1Char('a')
            && character <= QLatin1Char('f');
        if (!isDigit && !isLowerHex) {
            return false;
        }
        hasNonzeroNibble = hasNonzeroNibble
            || character != QLatin1Char('0');
    }
    return hasNonzeroNibble;
}

ValidationResult<QString> normalizeBindingChord(
    const QStringList &modifiers,
    const QString &key
)
{
    ValidationResult<QString> result;
    if (modifiers.size() > maximumBindingModifiers) {
        addError(
            result.errors,
            QStringLiteral("$.modifiers"),
            QStringLiteral("state.collection-limit"),
            QStringLiteral("Too many binding modifiers were provided.")
        );
        return result;
    }
    const QMap<QString, QString> aliases{
        {QStringLiteral("CTRL"), QStringLiteral("CTRL")},
        {QStringLiteral("CONTROL"), QStringLiteral("CTRL")},
        {QStringLiteral("ALT"), QStringLiteral("ALT")},
        {QStringLiteral("MOD1"), QStringLiteral("ALT")},
        {QStringLiteral("SHIFT"), QStringLiteral("SHIFT")},
        {QStringLiteral("CAPS"), QStringLiteral("CAPS")},
        {QStringLiteral("SUPER"), QStringLiteral("SUPER")},
        {QStringLiteral("MOD4"), QStringLiteral("SUPER")},
        {QStringLiteral("WIN"), QStringLiteral("SUPER")},
        {QStringLiteral("LOGO"), QStringLiteral("SUPER")},
        {QStringLiteral("META"), QStringLiteral("SUPER")},
        {QStringLiteral("MOD2"), QStringLiteral("MOD2")},
        {QStringLiteral("MOD3"), QStringLiteral("MOD3")},
        {QStringLiteral("MOD5"), QStringLiteral("MOD5")},
    };
    const QStringList order{
        QStringLiteral("SUPER"),
        QStringLiteral("CTRL"),
        QStringLiteral("ALT"),
        QStringLiteral("SHIFT"),
        QStringLiteral("CAPS"),
        QStringLiteral("MOD2"),
        QStringLiteral("MOD3"),
        QStringLiteral("MOD5"),
    };
    QSet<QString> normalizedModifiers;
    for (qsizetype index = 0; index < modifiers.size(); ++index) {
        const auto authored = modifiers.at(index).trimmed().toUpper();
        const auto alias = aliases.constFind(authored);
        const auto path = QStringLiteral("$.modifiers[")
            + QString::number(index) + QLatin1Char(']');
        if (alias == aliases.constEnd()) {
            addError(
                result.errors,
                path,
                QStringLiteral("state.invalid-modifier"),
                QStringLiteral("The modifier is not supported by Hyprland's Lua binding API.")
            );
            continue;
        }
        if (normalizedModifiers.contains(*alias)) {
            addError(
                result.errors,
                path,
                QStringLiteral("state.duplicate-modifier"),
                QStringLiteral("A modifier is repeated in the chord.")
            );
        }
        normalizedModifiers.insert(*alias);
    }
    const auto canonicalKey = canonicalBindingKey(
        key, QStringLiteral("$.key"), result.errors, false
    );
    if (!result.errors.isEmpty()) {
        return result;
    }
    QStringList parts;
    for (const auto &modifier : order) {
        if (normalizedModifiers.contains(modifier)) {
            parts.append(modifier);
        }
    }
    auto escapedKey = canonicalKey->toUpper();
    escapedKey.replace(QLatin1Char('\\'), QStringLiteral("\\\\"));
    escapedKey.replace(QLatin1Char('+'), QStringLiteral("\\+"));
    parts.append(escapedKey);
    result.value = parts.join(QLatin1Char('+'));
    return result;
}

ValidationResult<QVector<DeviceConfiguration>> parseDesiredInputDevices(
    const QJsonObject &snapshot
)
{
    ValidationResult<QVector<DeviceConfiguration>> result;
    auto devices = parseRecordArray<DeviceConfiguration>(
        snapshot,
        QStringLiteral("devices"),
        maximumDevices,
        result.errors,
        [](const QJsonObject &object,
           const QString &path,
           ValidationErrors &errors) {
            rejectUnknownFields(
                object,
                {
                    QStringLiteral("id"),
                    QStringLiteral("selector"),
                    QStringLiteral("kind"),
                    QStringLiteral("enabled"),
                    QStringLiteral("overrides"),
                },
                path,
                errors
            );
            DeviceConfiguration record;
            record.id = readString(
                object, QStringLiteral("id"), path, errors, 128
            );
            validateStableId(
                record.id, path + QStringLiteral(".id"), errors
            );
            record.selector = readString(
                object, QStringLiteral("selector"), path, errors, 256
            );
            record.kind = readString(
                object, QStringLiteral("kind"), path, errors, 64
            );
            static const QSet<QString> deviceKinds{
                QStringLiteral("keyboard"),
                QStringLiteral("pointer"),
                QStringLiteral("touchpad"),
                QStringLiteral("touch"),
                QStringLiteral("tablet"),
                QStringLiteral("tabletTool"),
                QStringLiteral("switch"),
                QStringLiteral("other"),
            };
            if (!record.kind.isEmpty()
                && !deviceKinds.contains(record.kind)) {
                addError(
                    errors,
                    path + QStringLiteral(".kind"),
                    QStringLiteral("state.invalid-device-kind"),
                    QStringLiteral(
                        "The device kind is not supported by the pinned contract."
                    )
                );
            }
            record.enabled = readBoolean(
                object, QStringLiteral("enabled"), path, errors, true
            );
            record.overrides = readClosedMap(
                object,
                QStringLiteral("overrides"),
                path,
                deviceFields(),
                errors
            );
            validateDeviceOverrides(
                record.overrides,
                path + QStringLiteral(".overrides"),
                errors
            );
            return record;
        }
    );

    QSet<QString> selectors;
    for (qsizetype index = 0; index < devices.size(); ++index) {
        auto selector = devices.at(index).selector;
        selector.replace(QLatin1Char(' '), QLatin1Char('-'));
        if (selector.isEmpty()) continue;
        if (selectors.contains(selector)) {
            addError(
                result.errors,
                QStringLiteral("$.devices[") + QString::number(index)
                    + QStringLiteral("].selector"),
                QStringLiteral("state.duplicate-natural-identity"),
                QStringLiteral(
                    "The managed collection contains a duplicate natural identity."
                )
            );
        }
        selectors.insert(selector);
    }

    if (result.errors.isEmpty()) result.value = std::move(devices);
    return result;
}

[[nodiscard]] static ValidationResult<DesiredState>
parseDesiredStateForAuthorities(
    const QByteArrayView bytes,
    const Catalog &catalog,
    const ActionCatalog &actionCatalog
)
{
    ValidationResult<DesiredState> result;
    const auto parsed = JsonSupport::parseStrictObject(
        bytes, maximumDesiredStateBytes, maximumDesiredStateDepth
    );
    if (!parsed) {
        result.errors = parsed.errors;
        return result;
    }
    const auto &root = *parsed.value;

    DesiredState state;
    if (actionCatalog.reviewedVersion != catalog.hyprland.reviewedVersion
        || actionCatalog.reviewedTag != catalog.hyprland.reviewedTag
        || actionCatalog.reviewedCommit != catalog.hyprland.reviewedCommit) {
        addError(
            result.errors,
            QStringLiteral("$"),
            QStringLiteral("state.authority-version-mismatch"),
            QStringLiteral("The scalar and action authorities were reviewed against different Hyprland sources.")
        );
    }
    if (const auto format = readInteger(
            root,
            QStringLiteral("formatVersion"),
            QStringLiteral("$"),
            result.errors,
            0,
            std::numeric_limits<quint32>::max()
        )) {
        state.formatVersion = static_cast<quint32>(*format);
        if (state.formatVersion != currentDesiredStateFormatVersion) {
            addError(
                result.errors,
                QStringLiteral("$.formatVersion"),
                QStringLiteral("state.unsupported-format-version"),
                QStringLiteral("This desired-state format version is not supported.")
            );
        }
    }
    const auto revisionText = readString(
        root,
        QStringLiteral("revision"),
        QStringLiteral("$"),
        result.errors,
        20
    );
    if (const auto revision = parseRevision(revisionText)) {
        state.revision = *revision;
    } else if (!revisionText.isEmpty()) {
        addError(
            result.errors,
            QStringLiteral("$.revision"),
            QStringLiteral("state.invalid-revision"),
            QStringLiteral("Revision must be a canonical unsigned 64-bit decimal string.")
        );
    }
    state.targetHyprland = readString(
        root,
        QStringLiteral("targetHyprland"),
        QStringLiteral("$"),
        result.errors,
        32
    );
    const auto target = parseTargetVersion(state.targetHyprland);
    if (!target && !state.targetHyprland.isEmpty()) {
        addError(
            result.errors,
            QStringLiteral("$.targetHyprland"),
            QStringLiteral("state.invalid-target-version"),
            QStringLiteral("A strict major.minor or major.minor.patch version is required.")
        );
    }
    if (target) {
        state.compatibility = compatibilityForVersion(
            catalog, target->version
        );
        if (state.compatibility == CompatibilityDecision::UnsupportedOlder
            || state.compatibility == CompatibilityDecision::UnsupportedMajor) {
            addError(
                result.errors,
                QStringLiteral("$.targetHyprland"),
                QStringLiteral("state.unsupported-target-version"),
                QStringLiteral("Use the matching migration/catalog contract for this target.")
            );
        }
        if (state.compatibility == CompatibilityDecision::UnsupportedFuture) {
            if (catalog.compatibility.newerMinor != NewerMinorPolicy::ReadOnly) {
                addError(
                    result.errors,
                    QStringLiteral("$.targetHyprland"),
                    QStringLiteral("state.unsupported-target-version"),
                    QStringLiteral("The future target is not accepted by this catalog policy.")
                );
            } else {
                state.readOnly = true;
            }
        }
    }
    state.catalogDigest = readString(
        root,
        QStringLiteral("catalogDigest"),
        QStringLiteral("$"),
        result.errors,
        64
    );
    static const QRegularExpression digestExpression(
        QStringLiteral("^[0-9a-f]{64}$")
    );
    if (!state.catalogDigest.isEmpty()
        && !digestExpression.match(state.catalogDigest).hasMatch()) {
        addError(
            result.errors,
            QStringLiteral("$.catalogDigest"),
            QStringLiteral("state.invalid-catalog-digest"),
            QStringLiteral("A lowercase SHA-256 digest is required.")
        );
    }
    if (!state.readOnly && !state.catalogDigest.isEmpty()
        && state.catalogDigest != catalogDigest(catalog)) {
        addError(
            result.errors,
            QStringLiteral("$.catalogDigest"),
            QStringLiteral("state.catalog-digest-mismatch"),
            QStringLiteral("The state was not authored against this catalog.")
        );
    }
    state.actionCatalogDigest = readString(
        root,
        QStringLiteral("actionCatalogDigest"),
        QStringLiteral("$"),
        result.errors,
        64
    );
    if (!state.actionCatalogDigest.isEmpty()
        && !digestExpression.match(state.actionCatalogDigest).hasMatch()) {
        addError(
            result.errors,
            QStringLiteral("$.actionCatalogDigest"),
            QStringLiteral("state.invalid-action-catalog-digest"),
            QStringLiteral("A lowercase SHA-256 digest is required.")
        );
    }
    if (!state.readOnly && !state.actionCatalogDigest.isEmpty()
        && state.actionCatalogDigest != actionCatalogDigest(actionCatalog)) {
        addError(
            result.errors,
            QStringLiteral("$.actionCatalogDigest"),
            QStringLiteral("state.action-catalog-digest-mismatch"),
            QStringLiteral("The state was not authored against this action catalog and schema authority.")
        );
    }

    // A future Hyprland minor may extend every nested record. Preserve the
    // parsed strict-JSON semantics as an opaque, non-editable envelope rather
    // than accidentally treating unknown fields as the current v1 authority.
    // This is canonical semantic preservation, not byte preservation: the
    // strict reader has already rejected numbers QJsonValue cannot re-emit
    // exactly, duplicate keys, excessive depth, and other lossy input.
    if (state.readOnly) {
        if (result.errors.isEmpty()) {
            state.opaqueFutureDocument = root;
            result.value = std::move(state);
        }
        return result;
    }

    rejectUnknownFields(
        root,
        {
            QStringLiteral("formatVersion"),
            QStringLiteral("revision"),
            QStringLiteral("targetHyprland"),
            QStringLiteral("catalogDigest"),
            QStringLiteral("actionCatalogDigest"),
            QStringLiteral("overrides"),
            QStringLiteral("monitors"),
            QStringLiteral("devices"),
            QStringLiteral("curves"),
            QStringLiteral("animations"),
            QStringLiteral("gestures"),
            QStringLiteral("workspaceRules"),
            QStringLiteral("windowRules"),
            QStringLiteral("layerRules"),
            QStringLiteral("submaps"),
            QStringLiteral("bindings"),
            QStringLiteral("permissions"),
            QStringLiteral("environment"),
        },
        QStringLiteral("$"),
        result.errors
    );

    const QStringList requiredSurfaces{
        QStringLiteral("monitors"),
        QStringLiteral("devices"),
        QStringLiteral("curves"),
        QStringLiteral("animations"),
        QStringLiteral("gestures"),
        QStringLiteral("workspaceRules"),
        QStringLiteral("windowRules"),
        QStringLiteral("layerRules"),
        QStringLiteral("submaps"),
        QStringLiteral("bindings"),
        QStringLiteral("permissions"),
        QStringLiteral("environment"),
    };
    for (const auto &surfaceId : requiredSurfaces) {
        const auto found = std::ranges::find(
            catalog.complexSurfaces,
            surfaceId,
            &ComplexSurfaceDefinition::id
        );
        if (found == catalog.complexSurfaces.end() || !found->ordered) {
            addError(
                result.errors,
                QStringLiteral("$.catalogDigest"),
                QStringLiteral("state.incomplete-catalog"),
                QStringLiteral("The catalog does not declare ordered surface %1.")
                    .arg(surfaceId)
            );
        }
    }

    const auto overrides = readObject(
        root, QStringLiteral("overrides"), QStringLiteral("$"), result.errors
    );
    if (overrides.size() > maximumOverrides) {
        addError(
            result.errors,
            QStringLiteral("$.overrides"),
            QStringLiteral("state.collection-limit"),
            QStringLiteral("The override map exceeds its catalog bound.")
        );
    }
    qsizetype overrideCount = 0;
    for (auto iterator = overrides.constBegin();
         iterator != overrides.constEnd() && overrideCount < maximumOverrides;
         ++iterator, ++overrideCount) {
        const auto path = QStringLiteral("$.overrides.") + iterator.key();
        const auto *option = findOption(catalog, iterator.key());
        if (!option) {
            addError(
                result.errors,
                path,
                QStringLiteral("state.unknown-option"),
                QStringLiteral("The override ID is not in the exact catalog.")
            );
            continue;
        }
        if (!option->writable) {
            addError(
                result.errors,
                path,
                QStringLiteral("state.read-only-option"),
                QStringLiteral("This complete catalog entry is not writable through managed desired state.")
            );
            continue;
        }
        const auto valueErrors = validateOptionValue(
            *option, iterator.value(), path
        );
        result.errors.append(valueErrors);
        if ((option->path == QStringLiteral("misc:swallow_regex")
                || option->path
                    == QStringLiteral("misc:swallow_exception_regex"))
            && iterator.value().isString()) {
            const auto expression = iterator.value().toString();
            const auto encoded = expression.toUtf8();
            re2::RE2::Options options;
            options.set_log_errors(false);
            const re2::RE2 compiled(encoded.constData(), options);
            if (expression.isEmpty() || !compiled.ok()) {
                addError(
                    result.errors,
                    path,
                    QStringLiteral("state.invalid-regex"),
                    QStringLiteral("Swallow overrides require nonempty valid RE2 syntax.")
                );
            }
        }
        if (option->path == QStringLiteral("cursor:default_monitor")
            && iterator.value().isString()
            && !iterator.value().toString().isEmpty()) {
            validateStaticMonitorSelector(
                iterator.value().toString(), path, result.errors
            );
        }
        if (JsonSupport::canonicalJson(iterator.value())
            == JsonSupport::canonicalJson(option->defaultValue)) {
            addError(
                result.errors,
                path,
                QStringLiteral("state.redundant-override"),
                QStringLiteral("Explicit overrides must differ from the catalog default.")
            );
        }
        state.overrides.insert(iterator.key(), iterator.value());
    }

    state.monitors = parseRecordArray<MonitorConfiguration>(
        root,
        QStringLiteral("monitors"),
        maximumMonitors,
        result.errors,
        [](const QJsonObject &object, const QString &path, ValidationErrors &errors) {
            rejectUnknownFields(
                object,
                {
                    QStringLiteral("id"), QStringLiteral("selector"),
                    QStringLiteral("enabled"), QStringLiteral("mode"),
                    QStringLiteral("position"), QStringLiteral("scale"),
                    QStringLiteral("reserved"),
                    QStringLiteral("transform"), QStringLiteral("mirror"),
                    QStringLiteral("bitdepth"), QStringLiteral("cm"),
                    QStringLiteral("sdrEotf"), QStringLiteral("sdrBrightness"),
                    QStringLiteral("sdrSaturation"), QStringLiteral("vrr"),
                    QStringLiteral("icc"), QStringLiteral("supportsWideColor"),
                    QStringLiteral("supportsHdr"), QStringLiteral("sdrMinLuminance"),
                    QStringLiteral("sdrMaxLuminance"), QStringLiteral("minLuminance"),
                    QStringLiteral("maxLuminance"), QStringLiteral("maxAvgLuminance"),
                },
                path,
                errors
            );
            MonitorConfiguration record;
            record.id = readString(object, QStringLiteral("id"), path, errors, 128);
            validateStableId(record.id, path + QStringLiteral(".id"), errors);
            record.selector = readString(object, QStringLiteral("selector"), path, errors, 261);
            validateStaticMonitorSelector(
                record.selector, path + QStringLiteral(".selector"), errors
            );
            record.enabled = readBoolean(object, QStringLiteral("enabled"), path, errors, true);
            record.mode = readString(object, QStringLiteral("mode"), path, errors, 24);
            static const QSet<QString> automaticModes{
                QStringLiteral("preferred"), QStringLiteral("highrr"),
                QStringLiteral("highres"), QStringLiteral("maxwidth"),
            };
            static const QRegularExpression explicitMode(
                QStringLiteral("^[1-9][0-9]{0,4}x[1-9][0-9]{0,4}(?:@(?:[1-9][0-9]{0,3}(?:\\.[0-9]{1,3})?|0\\.[0-9]{0,2}[1-9]))?$")
            );
            if (!record.mode.isEmpty() && !automaticModes.contains(record.mode)
                && !explicitMode.match(record.mode).hasMatch()) {
                addError(errors, path + QStringLiteral(".mode"), QStringLiteral("state.invalid-monitor-mode"), QStringLiteral("The monitor mode is outside the managed mode grammar."));
            }
            record.position = readString(object, QStringLiteral("position"), path, errors, 17);
            static const QSet<QString> automaticPositions{
                QStringLiteral("auto"), QStringLiteral("auto-right"),
                QStringLiteral("auto-left"), QStringLiteral("auto-up"),
                QStringLiteral("auto-down"), QStringLiteral("auto-center-right"),
                QStringLiteral("auto-center-left"), QStringLiteral("auto-center-up"),
                QStringLiteral("auto-center-down"),
            };
            static const QRegularExpression explicitPosition(
                QStringLiteral("^(?:0|[+-]?(?:[1-9][0-9]{0,5}|1000000))x(?:0|[+-]?(?:[1-9][0-9]{0,5}|1000000))$")
            );
            if (!record.position.isEmpty()
                && !automaticPositions.contains(record.position)
                && !explicitPosition.match(record.position).hasMatch()) {
                addError(errors, path + QStringLiteral(".position"), QStringLiteral("state.invalid-monitor-position"), QStringLiteral("The monitor position is outside the managed coordinate grammar."));
            }
            const auto scale = object.value(QStringLiteral("scale"));
            if (scale.isDouble() && std::isfinite(scale.toDouble())
                && scale.toDouble() >= 0.25
                && scale.toDouble() <= std::numeric_limits<float>::max()) {
                record.scale = scale.toDouble();
            } else if (scale.isString()
                && scale.toString() == QStringLiteral("auto")) {
                record.scale = scale.toString();
            } else {
                addError(errors, path + QStringLiteral(".scale"), QStringLiteral("state.invalid-monitor-scale"), QStringLiteral("Scale must be at least 0.25 or exactly auto."));
            }
            const auto reserved = object.value(QStringLiteral("reserved"));
            if (!reserved.isArray() || !isCssGapValue(reserved)) {
                addError(errors, path + QStringLiteral(".reserved"), QStringLiteral("state.invalid-css-gap"), QStringLiteral("Reserved area must contain four bounded integers."));
            } else {
                for (qsizetype index = 0; index < 4; ++index) {
                    const auto number = reserved.toArray().at(index).toDouble();
                    record.reserved.at(index) = static_cast<qint64>(number);
                }
            }
            if (const auto value = readInteger(object, QStringLiteral("transform"), path, errors, 0, 7)) {
                record.transform = static_cast<qint32>(*value);
            }
            record.mirror = readString(object, QStringLiteral("mirror"), path, errors, 261, true);
            if (!record.mirror.isEmpty()) {
                validateStaticMonitorSelector(
                    record.mirror, path + QStringLiteral(".mirror"), errors
                );
            }
            if (const auto value = readInteger(object, QStringLiteral("bitdepth"), path, errors, 8, 10)) {
                record.bitdepth = static_cast<qint32>(*value);
                if (*value != 8 && *value != 10) addError(errors, path + QStringLiteral(".bitdepth"), QStringLiteral("state.invalid-monitor-bitdepth"), QStringLiteral("Only 8-bit and 10-bit outputs are supported."));
            }
            record.colorManagement = readString(object, QStringLiteral("cm"), path, errors, 32);
            static const QSet<QString> colorModes{QStringLiteral("auto"), QStringLiteral("srgb"), QStringLiteral("wide"), QStringLiteral("edid"), QStringLiteral("hdr"), QStringLiteral("hdredid"), QStringLiteral("dcip3"), QStringLiteral("dp3"), QStringLiteral("adobe")};
            if (!record.colorManagement.isEmpty() && !colorModes.contains(record.colorManagement)) addError(errors, path + QStringLiteral(".cm"), QStringLiteral("state.invalid-monitor-color-mode"), QStringLiteral("The color-management mode is unsupported."));
            record.sdrEotf = readString(object, QStringLiteral("sdrEotf"), path, errors, 32);
            static const QSet<QString> eotfs{QStringLiteral("default"), QStringLiteral("auto"), QStringLiteral("srgb"), QStringLiteral("gamma22"), QStringLiteral("gamma22force")};
            if (!record.sdrEotf.isEmpty() && !eotfs.contains(record.sdrEotf)) addError(errors, path + QStringLiteral(".sdrEotf"), QStringLiteral("state.invalid-monitor-eotf"), QStringLiteral("The SDR transfer function is unsupported."));
            if (const auto value = readNumber(object, QStringLiteral("sdrBrightness"), path, errors, 0.0, 10.0)) record.sdrBrightness = *value;
            if (const auto value = readNumber(object, QStringLiteral("sdrSaturation"), path, errors, 0.0, 10.0)) record.sdrSaturation = *value;
            if (const auto value = readInteger(object, QStringLiteral("vrr"), path, errors, -1, 3)) {
                record.vrr = static_cast<qint32>(*value);
            }
            record.icc = readString(object, QStringLiteral("icc"), path, errors, 256, true);
            if (const auto value = readInteger(object, QStringLiteral("supportsWideColor"), path, errors, -1, 1)) record.supportsWideColor = static_cast<qint32>(*value);
            if (const auto value = readInteger(object, QStringLiteral("supportsHdr"), path, errors, -1, 1)) record.supportsHdr = static_cast<qint32>(*value);
            if (const auto value = readNumber(object, QStringLiteral("sdrMinLuminance"), path, errors, 0.0, 10000.0)) record.sdrMinLuminance = *value;
            if (const auto value = readInteger(object, QStringLiteral("sdrMaxLuminance"), path, errors, -1, std::numeric_limits<qint32>::max())) record.sdrMaxLuminance = *value;
            if (const auto value = readNumber(object, QStringLiteral("minLuminance"), path, errors, -1.0, 10000.0)) record.minLuminance = *value;
            if (const auto value = readInteger(object, QStringLiteral("maxLuminance"), path, errors, -1, std::numeric_limits<qint32>::max())) record.maxLuminance = *value;
            if (const auto value = readInteger(object, QStringLiteral("maxAvgLuminance"), path, errors, -1, std::numeric_limits<qint32>::max())) record.maxAvgLuminance = *value;
            return record;
        }
    );

    const auto devices = parseDesiredInputDevices(root);
    result.errors.append(devices.errors);
    if (devices) state.devices = *devices.value;

    state.curves = parseRecordArray<AnimationCurve>(
        root,
        QStringLiteral("curves"),
        maximumCurves,
        result.errors,
        [](const QJsonObject &object, const QString &path, ValidationErrors &errors) {
            AnimationCurve record;
            record.id = readString(object, QStringLiteral("id"), path, errors, 128);
            validateStableId(record.id, path + QStringLiteral(".id"), errors);
            record.name = readString(object, QStringLiteral("name"), path, errors, 256);
            const auto type = readString(object, QStringLiteral("type"), path, errors, 16);
            if (type == QStringLiteral("bezier")) {
                rejectUnknownFields(
                    object,
                    {QStringLiteral("id"), QStringLiteral("name"),
                     QStringLiteral("type"), QStringLiteral("points")},
                    path,
                    errors
                );
                BezierCurveParameters parameters;
                const auto points = readArray(object, QStringLiteral("points"), path, errors);
                if (points.size() != 2) {
                    addError(errors, path + QStringLiteral(".points"), QStringLiteral("state.invalid-curve"), QStringLiteral("Bezier curves require exactly two points."));
                }
                for (qsizetype point = 0; point < std::min<qsizetype>(2, points.size()); ++point) {
                    const auto pointPath = path + QStringLiteral(".points[") + QString::number(point) + QLatin1Char(']');
                    if (!points.at(point).isArray() || points.at(point).toArray().size() != 2) {
                        addError(errors, pointPath, QStringLiteral("state.invalid-curve"), QStringLiteral("A curve point must contain exactly two coordinates."));
                        continue;
                    }
                    const auto coordinates = points.at(point).toArray();
                    for (qsizetype component = 0; component < 2; ++component) {
                        const auto value = coordinates.at(component);
                        if (!value.isDouble() || !std::isfinite(value.toDouble()) || value.toDouble() < -1.0 || value.toDouble() > 2.0) {
                            addError(errors, pointPath + QLatin1Char('[') + QString::number(component) + QLatin1Char(']'), QStringLiteral("state.invalid-curve"), QStringLiteral("Bezier coordinates must lie between -1 and 2."));
                        } else {
                            parameters.points.at(point).at(component) = value.toDouble();
                        }
                    }
                }
                record.parameters = parameters;
            } else if (type == QStringLiteral("spring")) {
                rejectUnknownFields(
                    object,
                    {QStringLiteral("id"), QStringLiteral("name"),
                     QStringLiteral("type"), QStringLiteral("stiffness"),
                     QStringLiteral("dampening"), QStringLiteral("mass")},
                    path,
                    errors
                );
                SpringCurveParameters parameters;
                const auto readSpring = [&object, &path, &errors](const QString &key) {
                    const auto value = readNumber(object, key, path, errors, 0.0, 1000000.0);
                    if (value && *value <= 0.5) {
                        addError(errors, path + QLatin1Char('.') + key, QStringLiteral("state.invalid-curve"), QStringLiteral("Spring parameters must be greater than 0.5."));
                        return std::optional<double>{};
                    }
                    return value;
                };
                if (const auto value = readSpring(QStringLiteral("stiffness"))) parameters.stiffness = *value;
                if (const auto value = readSpring(QStringLiteral("dampening"))) parameters.dampening = *value;
                if (const auto value = readSpring(QStringLiteral("mass"))) parameters.mass = *value;
                record.parameters = parameters;
            } else if (!type.isEmpty()) {
                addError(errors, path + QStringLiteral(".type"), QStringLiteral("state.invalid-curve-type"), QStringLiteral("Only bezier and spring curves are supported."));
            }
            return record;
        }
    );

    state.animations = parseRecordArray<AnimationConfiguration>(
        root,
        QStringLiteral("animations"),
        maximumAnimations,
        result.errors,
        [](const QJsonObject &object, const QString &path, ValidationErrors &errors) {
            rejectUnknownFields(
                object,
                {QStringLiteral("id"), QStringLiteral("name"),
                 QStringLiteral("enabled"), QStringLiteral("speed"),
                 QStringLiteral("curve"), QStringLiteral("style")},
                path,
                errors
            );
            AnimationConfiguration record;
            record.id = readString(object, QStringLiteral("id"), path, errors, 128);
            validateStableId(record.id, path + QStringLiteral(".id"), errors);
            record.name = readString(object, QStringLiteral("name"), path, errors, 128);
            static const QSet<QString> animationLeaves{
                QStringLiteral("global"), QStringLiteral("windows"),
                QStringLiteral("layers"), QStringLiteral("fade"),
                QStringLiteral("border"), QStringLiteral("borderangle"),
                QStringLiteral("shadowangle"), QStringLiteral("glowangle"),
                QStringLiteral("workspaces"), QStringLiteral("zoomFactor"),
                QStringLiteral("monitorAdded"), QStringLiteral("layersIn"),
                QStringLiteral("layersOut"), QStringLiteral("windowsIn"),
                QStringLiteral("windowsOut"), QStringLiteral("windowsMove"),
                QStringLiteral("fadeIn"), QStringLiteral("fadeOut"),
                QStringLiteral("fadeSwitch"), QStringLiteral("fadeShadow"),
                QStringLiteral("fadeGlow"), QStringLiteral("fadeDim"),
                QStringLiteral("fadeLayers"), QStringLiteral("fadeLayersIn"),
                QStringLiteral("fadeLayersOut"), QStringLiteral("fadePopups"),
                QStringLiteral("fadePopupsIn"), QStringLiteral("fadePopupsOut"),
                QStringLiteral("fadeDpms"), QStringLiteral("workspacesIn"),
                QStringLiteral("workspacesOut"), QStringLiteral("specialWorkspace"),
                QStringLiteral("specialWorkspaceIn"),
                QStringLiteral("specialWorkspaceOut"),
            };
            if (!record.name.isEmpty() && !animationLeaves.contains(record.name)) {
                addError(errors, path + QStringLiteral(".name"), QStringLiteral("state.invalid-animation-name"), QStringLiteral("The animation name is not a pinned Hyprland animation leaf."));
            }
            record.enabled = readBoolean(object, QStringLiteral("enabled"), path, errors, true);
            if (const auto value = readNumber(object, QStringLiteral("speed"), path, errors, 0.0, 100.0)) record.speed = *value;
            if (record.speed <= 0) addError(errors, path + QStringLiteral(".speed"), QStringLiteral("state.animation-speed"), QStringLiteral("Animation speed must be greater than zero."));
            record.curve = readString(object, QStringLiteral("curve"), path, errors, 256);
            record.style = readString(object, QStringLiteral("style"), path, errors, 128, true);
            static const QSet<QString> windowLeaves{
                QStringLiteral("windows"), QStringLiteral("windowsIn"),
                QStringLiteral("windowsOut"), QStringLiteral("windowsMove"),
            };
            static const QSet<QString> workspaceLeaves{
                QStringLiteral("workspaces"), QStringLiteral("workspacesIn"),
                QStringLiteral("workspacesOut"), QStringLiteral("specialWorkspace"),
                QStringLiteral("specialWorkspaceIn"),
                QStringLiteral("specialWorkspaceOut"),
            };
            static const QSet<QString> angleLeaves{
                QStringLiteral("borderangle"), QStringLiteral("shadowangle"),
                QStringLiteral("glowangle"),
            };
            static const QSet<QString> layerLeaves{
                QStringLiteral("layers"), QStringLiteral("layersIn"),
                QStringLiteral("layersOut"),
            };
            static const QRegularExpression windowStyle(
                QStringLiteral("^(?:slide(?: (?:top|bottom|left|right))?|gnome|gnomed|popin(?: (?:0|[1-9][0-9]?|100)%)?)$")
            );
            static const QRegularExpression workspaceStyle(
                QStringLiteral("^(?:fade|(?:slide|slidevert|slidefade|slidefadevert)(?: (?:top|bottom|left|right))?(?: (?:0|[1-9][0-9]?|100)%)?)$")
            );
            static const QRegularExpression layerStyle(
                QStringLiteral("^(?:|fade|slide(?: (?:top|bottom|left|right))?|popin(?: (?:0|[1-9][0-9]?|100)%)?)$")
            );
            bool styleValid = record.style.isEmpty();
            if (!record.style.isEmpty() && windowLeaves.contains(record.name)) {
                styleValid = windowStyle.match(record.style).hasMatch();
            } else if (!record.style.isEmpty()
                       && workspaceLeaves.contains(record.name)) {
                styleValid = workspaceStyle.match(record.style).hasMatch();
            } else if (!record.style.isEmpty()
                       && angleLeaves.contains(record.name)) {
                styleValid = record.style == QStringLiteral("loop")
                    || record.style == QStringLiteral("once");
            } else if (!record.style.isEmpty()
                       && layerLeaves.contains(record.name)) {
                styleValid = layerStyle.match(record.style).hasMatch();
            } else if (!record.style.isEmpty()
                       && animationLeaves.contains(record.name)) {
                styleValid = record.style.isEmpty();
            }
            if (!styleValid) {
                addError(errors, path + QStringLiteral(".style"), QStringLiteral("state.invalid-animation-style"), QStringLiteral("The style is not valid for this animation leaf."));
            }
            return record;
        }
    );

    state.gestures = parseRecordArray<GestureConfiguration>(
        root,
        QStringLiteral("gestures"),
        maximumGestures,
        result.errors,
        [&actionCatalog](const QJsonObject &object, const QString &path, ValidationErrors &errors) {
            rejectUnknownFields(
                object,
                {QStringLiteral("id"), QStringLiteral("fingers"),
                 QStringLiteral("direction"), QStringLiteral("modifiers"),
                 QStringLiteral("scale"), QStringLiteral("disableInhibit"),
                 QStringLiteral("action")},
                path,
                errors
            );
            GestureConfiguration record;
            record.id = readString(object, QStringLiteral("id"), path, errors, 128);
            validateStableId(record.id, path + QStringLiteral(".id"), errors);
            if (const auto value = readInteger(object, QStringLiteral("fingers"), path, errors, 2, 9)) record.fingers = static_cast<quint32>(*value);
            record.direction = readString(object, QStringLiteral("direction"), path, errors, 32);
            static const QSet<QString> directions{
                QStringLiteral("swipe"), QStringLiteral("pinch"),
                QStringLiteral("pinchIn"), QStringLiteral("pinchOut"),
                QStringLiteral("horizontal"), QStringLiteral("vertical"),
                QStringLiteral("left"), QStringLiteral("right"),
                QStringLiteral("up"), QStringLiteral("down"),
            };
            if (!record.direction.isEmpty() && !directions.contains(record.direction)) addError(errors, path + QStringLiteral(".direction"), QStringLiteral("state.invalid-gesture-direction"), QStringLiteral("The gesture direction is not supported."));
            record.modifiers = readStringArray(object, QStringLiteral("modifiers"), path, maximumBindingModifiers, errors, false);
            validatePersistedModifiers(
                record.modifiers,
                path + QStringLiteral(".modifiers"),
                errors
            );
            const auto modifierValidation = normalizeBindingChord(record.modifiers, QStringLiteral("A"));
            for (const auto &error : modifierValidation.errors) addError(errors, path + QStringLiteral(".modifiers") + error.path.sliced(QStringLiteral("$.modifiers").size()), error.code, error.message);
            if (const auto value = readNumber(object, QStringLiteral("scale"), path, errors, 0.1, 10)) record.scale = *value;
            record.disableInhibit = readBoolean(object, QStringLiteral("disableInhibit"), path, errors);
            const auto action = readObject(object, QStringLiteral("action"), path, errors);
            const auto actionPath = path + QStringLiteral(".action");
            const auto type = readString(action, QStringLiteral("type"), actionPath, errors, 32);
            record.action.id = type;
            record.action.payload = action;
            if (const auto *definition = findAction(
                    actionCatalog, ActionKind::Gesture, type
                )) {
                errors.append(validateActionPayload(
                    *definition, action, actionPath
                ));
            } else if (!type.isEmpty()) {
                addError(errors, actionPath + QStringLiteral(".type"), QStringLiteral("state.unknown-gesture-action"), QStringLiteral("Only pinned declarative gesture actions are accepted."));
            }
            return record;
        }
    );

    state.workspaceRules = parseRecordArray<WorkspaceRule>(
        root,
        QStringLiteral("workspaceRules"),
        maximumWorkspaceRules,
        result.errors,
        [](const QJsonObject &object, const QString &path, ValidationErrors &errors) {
            rejectUnknownFields(object, {QStringLiteral("id"), QStringLiteral("selector"), QStringLiteral("enabled"), QStringLiteral("monitor"), QStringLiteral("persistent"), QStringLiteral("isDefault"), QStringLiteral("layout"), QStringLiteral("overrides")}, path, errors);
            WorkspaceRule record;
            record.id = readString(object, QStringLiteral("id"), path, errors, 128);
            validateStableId(record.id, path + QStringLiteral(".id"), errors);
            record.selector = readString(object, QStringLiteral("selector"), path, errors, 256);
            validateWorkspaceRuleSelector(
                record.selector,
                record.id,
                path + QStringLiteral(".selector"),
                errors
            );
            record.enabled = readBoolean(object, QStringLiteral("enabled"), path, errors, true);
            record.monitor = readString(object, QStringLiteral("monitor"), path, errors, 261, true);
            if (!record.monitor.isEmpty()) {
                validateStaticMonitorSelector(
                    record.monitor, path + QStringLiteral(".monitor"), errors
                );
            }
            record.persistent = readBoolean(object, QStringLiteral("persistent"), path, errors);
            record.isDefault = readBoolean(object, QStringLiteral("isDefault"), path, errors);
            record.layout = readString(object, QStringLiteral("layout"), path, errors, 256, true);
            static const QSet<QString> layouts{
                QString(), QStringLiteral("dwindle"), QStringLiteral("master"),
                QStringLiteral("scrolling"), QStringLiteral("monocle")
            };
            if (!layouts.contains(record.layout)) {
                addError(errors, path + QStringLiteral(".layout"), QStringLiteral("state.invalid-workspace-layout"), QStringLiteral("The layout is not a pinned built-in Hyprland layout."));
            }
            record.overrides = readClosedMap(object, QStringLiteral("overrides"), path, workspaceFields(), errors);
            validateWorkspaceOverrides(
                record.overrides, path + QStringLiteral(".overrides"), errors
            );
            if (record.id == QLatin1String(sharedSpacingWorkspaceRuleId)) {
                const QJsonObject expectedOverrides{
                    {
                        QStringLiteral("gaps_out"),
                        QJsonArray{0, 0, 0, 0},
                    },
                };
                if (record.selector
                        != QLatin1String(sharedSpacingWorkspaceRuleSelector)
                    || !record.enabled || !record.monitor.isEmpty()
                    || record.persistent || record.isDefault
                    || !record.layout.isEmpty()
                    || record.overrides != expectedOverrides) {
                    addError(
                        errors,
                        path,
                        QStringLiteral("state.invalid-protected-workspace-rule"),
                        QStringLiteral(
                            "The protected maximized-workspace rule must match the exact HyprShelld contract."
                        )
                    );
                }
            }
            return record;
        }
    );
    const auto userWorkspaceRuleCount = std::ranges::count_if(
        state.workspaceRules,
        [](const WorkspaceRule &record) {
            return record.id
                != QLatin1String(sharedSpacingWorkspaceRuleId);
        }
    );
    if (userWorkspaceRuleCount > maximumUserWorkspaceRules) {
        addError(
            result.errors,
            QStringLiteral("$.workspaceRules"),
            QStringLiteral("state.too-many-workspace-rules"),
            QStringLiteral(
                "At most 1024 user workspace rules are accepted."
            )
        );
    }

    state.windowRules = parseRecordArray<WindowRule>(
        root,
        QStringLiteral("windowRules"),
        maximumWindowRules,
        result.errors,
        [](const QJsonObject &object, const QString &path, ValidationErrors &errors) {
            rejectUnknownFields(object, {QStringLiteral("id"), QStringLiteral("name"), QStringLiteral("enabled"), QStringLiteral("match"), QStringLiteral("effects")}, path, errors);
            WindowRule record;
            record.id = readString(object, QStringLiteral("id"), path, errors, 128);
            validateStableId(record.id, path + QStringLiteral(".id"), errors);
            record.name = readString(object, QStringLiteral("name"), path, errors, 256);
            record.enabled = readBoolean(object, QStringLiteral("enabled"), path, errors, true);
            record.match = readClosedMap(object, QStringLiteral("match"), path, windowMatchFields(), errors);
            validateWindowMatch(
                record.match, path + QStringLiteral(".match"), errors
            );
            record.effects = readClosedMap(object, QStringLiteral("effects"), path, windowEffectFields(), errors);
            validateWindowEffects(record.effects, path + QStringLiteral(".effects"), errors);
            if (record.match.isEmpty()) addError(errors, path + QStringLiteral(".match"), QStringLiteral("state.empty-rule-match"), QStringLiteral("A window rule requires at least one matcher."));
            if (record.effects.isEmpty()) addError(errors, path + QStringLiteral(".effects"), QStringLiteral("state.empty-rule-effects"), QStringLiteral("A window rule requires at least one effect."));
            return record;
        }
    );

    state.layerRules = parseRecordArray<LayerRule>(
        root,
        QStringLiteral("layerRules"),
        maximumLayerRules,
        result.errors,
        [](const QJsonObject &object, const QString &path, ValidationErrors &errors) {
            rejectUnknownFields(object, {QStringLiteral("id"), QStringLiteral("name"), QStringLiteral("enabled"), QStringLiteral("match"), QStringLiteral("effects")}, path, errors);
            LayerRule record;
            record.id = readString(object, QStringLiteral("id"), path, errors, 128);
            validateStableId(record.id, path + QStringLiteral(".id"), errors);
            record.name = readString(object, QStringLiteral("name"), path, errors, 256);
            record.enabled = readBoolean(object, QStringLiteral("enabled"), path, errors, true);
            record.match = readClosedMap(object, QStringLiteral("match"), path, layerMatchFields(), errors);
            validateLayerMatch(
                record.match, path + QStringLiteral(".match"), errors
            );
            record.effects = readClosedMap(object, QStringLiteral("effects"), path, layerEffectFields(), errors);
            validateLayerEffects(record.effects, path + QStringLiteral(".effects"), errors);
            if (record.match.isEmpty()) addError(errors, path + QStringLiteral(".match"), QStringLiteral("state.empty-rule-match"), QStringLiteral("A layer rule requires at least one matcher."));
            if (record.effects.isEmpty()) addError(errors, path + QStringLiteral(".effects"), QStringLiteral("state.empty-rule-effects"), QStringLiteral("A layer rule requires at least one effect."));
            return record;
        }
    );

    state.submaps = parseRecordArray<SubmapConfiguration>(
        root,
        QStringLiteral("submaps"),
        maximumSubmaps,
        result.errors,
        [](const QJsonObject &object, const QString &path, ValidationErrors &errors) {
            rejectUnknownFields(object, {QStringLiteral("id"), QStringLiteral("name"), QStringLiteral("reset"), QStringLiteral("enabled")}, path, errors);
            SubmapConfiguration record;
            record.id = readString(object, QStringLiteral("id"), path, errors, 128);
            validateStableId(record.id, path + QStringLiteral(".id"), errors);
            record.name = readString(object, QStringLiteral("name"), path, errors, 256);
            record.reset = readString(object, QStringLiteral("reset"), path, errors, 256, true);
            record.enabled = readBoolean(object, QStringLiteral("enabled"), path, errors, true);
            return record;
        }
    );

    state.bindings = parseRecordArray<BindingConfiguration>(
        root,
        QStringLiteral("bindings"),
        maximumBindings,
        result.errors,
        [&actionCatalog](const QJsonObject &object, const QString &path, ValidationErrors &errors) {
            rejectUnknownFields(object, {QStringLiteral("id"), QStringLiteral("modifiers"), QStringLiteral("key"), QStringLiteral("actionType"), QStringLiteral("action"), QStringLiteral("arguments"), QStringLiteral("description"), QStringLiteral("enabled"), QStringLiteral("submap"), QStringLiteral("options")}, path, errors);
            BindingConfiguration record;
            record.id = readString(object, QStringLiteral("id"), path, errors, 128);
            validateStableId(record.id, path + QStringLiteral(".id"), errors);
            record.modifiers = readStringArray(object, QStringLiteral("modifiers"), path, maximumBindingModifiers, errors, false);
            validatePersistedModifiers(
                record.modifiers,
                path + QStringLiteral(".modifiers"),
                errors
            );
            record.key = readString(object, QStringLiteral("key"), path, errors, 64);
            if (!record.key.isEmpty()) {
                Q_UNUSED(canonicalBindingKey(
                    record.key,
                    path + QStringLiteral(".key"),
                    errors,
                    true
                ));
            }
            const auto actionType = readString(object, QStringLiteral("actionType"), path, errors, 32);
            if (actionType == QStringLiteral("dispatcher")) record.actionType = BindingActionType::Dispatcher;
            else if (actionType == QStringLiteral("defaultApp")) record.actionType = BindingActionType::DefaultApp;
            else if (actionType == QStringLiteral("hyprshelld")) record.actionType = BindingActionType::HyprShelld;
            else if (!actionType.isEmpty()) addError(errors, path + QStringLiteral(".actionType"), QStringLiteral("state.invalid-action-type"), QStringLiteral("The binding action type is not closed by this contract."));
            record.action = readString(object, QStringLiteral("action"), path, errors, 128);
            record.arguments = readObject(object, QStringLiteral("arguments"), path, errors);
            const auto actionKind = record.actionType == BindingActionType::Dispatcher
                ? ActionKind::Dispatcher
                : record.actionType == BindingActionType::DefaultApp
                    ? ActionKind::DefaultApp
                    : ActionKind::HyprShelld;
            if (const auto *definition = findAction(
                    actionCatalog, actionKind, record.action
                )) {
                errors.append(validateActionPayload(
                    *definition,
                    record.arguments,
                    path + QStringLiteral(".arguments")
                ));
                if (actionKind == ActionKind::Dispatcher) {
                    validateDispatcherPayloadSemantics(
                        record.action,
                        record.arguments,
                        path + QStringLiteral(".arguments"),
                        errors
                    );
                }
                if (actionKind != ActionKind::Dispatcher
                    && !record.arguments.isEmpty()) {
                    addError(errors, path + QStringLiteral(".arguments"), QStringLiteral("state.unexpected-action-arguments"), QStringLiteral("Semantic actions do not accept arguments in this contract."));
                }
            } else if (!record.action.isEmpty()) {
                addError(errors, path + QStringLiteral(".action"), QStringLiteral("state.unknown-binding-action"), QStringLiteral("The action is not in the compiled action catalog."));
            }
            record.description = readString(object, QStringLiteral("description"), path, errors, 512, false, true);
            record.enabled = readBoolean(object, QStringLiteral("enabled"), path, errors, true);
            record.submap = readString(object, QStringLiteral("submap"), path, errors, 256, true);

            const auto options = readObject(object, QStringLiteral("options"), path, errors);
            const auto optionsPath = path + QStringLiteral(".options");
            rejectUnknownFields(options, {QStringLiteral("repeating"), QStringLiteral("locked"), QStringLiteral("release"), QStringLiteral("nonConsuming"), QStringLiteral("autoConsuming"), QStringLiteral("transparent"), QStringLiteral("ignoreMods"), QStringLiteral("dontInhibit"), QStringLiteral("longPress"), QStringLiteral("submapUniversal"), QStringLiteral("click"), QStringLiteral("drag"), QStringLiteral("allowInputCapture"), QStringLiteral("device")}, optionsPath, errors);
            record.options.repeating = readBoolean(options, QStringLiteral("repeating"), optionsPath, errors);
            record.options.locked = readBoolean(options, QStringLiteral("locked"), optionsPath, errors);
            record.options.release = readBoolean(options, QStringLiteral("release"), optionsPath, errors);
            record.options.nonConsuming = readBoolean(options, QStringLiteral("nonConsuming"), optionsPath, errors);
            record.options.autoConsuming = readBoolean(options, QStringLiteral("autoConsuming"), optionsPath, errors);
            record.options.transparent = readBoolean(options, QStringLiteral("transparent"), optionsPath, errors);
            record.options.ignoreMods = readBoolean(options, QStringLiteral("ignoreMods"), optionsPath, errors);
            record.options.dontInhibit = readBoolean(options, QStringLiteral("dontInhibit"), optionsPath, errors);
            record.options.longPress = readBoolean(options, QStringLiteral("longPress"), optionsPath, errors);
            record.options.submapUniversal = readBoolean(options, QStringLiteral("submapUniversal"), optionsPath, errors);
            record.options.click = readBoolean(options, QStringLiteral("click"), optionsPath, errors);
            record.options.drag = readBoolean(options, QStringLiteral("drag"), optionsPath, errors);
            record.options.allowInputCapture = readBoolean(options, QStringLiteral("allowInputCapture"), optionsPath, errors);
            if (options.contains(QStringLiteral("device")) && !options.value(QStringLiteral("device")).isNull()) {
                const auto device = readObject(options, QStringLiteral("device"), optionsPath, errors);
                rejectUnknownFields(device, {QStringLiteral("inclusive"), QStringLiteral("list")}, optionsPath + QStringLiteral(".device"), errors);
                BindingDeviceFilter filter;
                filter.inclusive = readBoolean(device, QStringLiteral("inclusive"), optionsPath + QStringLiteral(".device"), errors, true);
                filter.list = readStringArray(device, QStringLiteral("list"), optionsPath + QStringLiteral(".device"), maximumBindingDevices, errors, false);
                validateNonEmptySchemaStringList(
                    filter.list,
                    optionsPath + QStringLiteral(".device.list"),
                    errors
                );
                QSet<QString> devices;
                for (qsizetype index = 0; index < filter.list.size(); ++index) {
                    if (devices.contains(filter.list.at(index))) addError(errors, optionsPath + QStringLiteral(".device.list[") + QString::number(index) + QLatin1Char(']'), QStringLiteral("state.duplicate-device"), QStringLiteral("Binding device filters must be unique."));
                    devices.insert(filter.list.at(index));
                }
                record.options.device = std::move(filter);
            } else if (options.contains(QStringLiteral("device"))) {
                addError(errors, optionsPath + QStringLiteral(".device"), QStringLiteral("state.object-required"), QStringLiteral("A device filter must be omitted or be an object."));
            } else {
                // Absence is the only representation for no device filter.
            }
            if (record.options.click && record.options.drag) addError(errors, optionsPath, QStringLiteral("state.incompatible-bind-options"), QStringLiteral("click and drag are mutually exclusive."));
            if ((record.options.click || record.options.drag)
                && !record.options.release) {
                addError(errors, optionsPath + QStringLiteral(".release"), QStringLiteral("state.incompatible-bind-options"), QStringLiteral("click and drag bindings must explicitly persist release=true."));
            }
            if (record.options.repeating
                && (record.options.longPress || record.options.release
                    || record.options.click || record.options.drag)) {
                addError(errors, optionsPath, QStringLiteral("state.incompatible-bind-options"), QStringLiteral("longPress/release/click/drag cannot be combined with repeating."));
            }
            const auto chord = normalizeBindingChord(record.modifiers, record.key);
            if (chord) record.normalizedChord = *chord.value;
            else {
                for (const auto &error : chord.errors) addError(errors, path + error.path.sliced(1), error.code, error.message);
            }
            return record;
        }
    );

    state.permissions = parseRecordArray<PermissionConfiguration>(
        root,
        QStringLiteral("permissions"),
        maximumPermissions,
        result.errors,
        [](const QJsonObject &object, const QString &path, ValidationErrors &errors) {
            rejectUnknownFields(object, {QStringLiteral("id"), QStringLiteral("binary"), QStringLiteral("type"), QStringLiteral("mode")}, path, errors);
            PermissionConfiguration record;
            record.id = readString(object, QStringLiteral("id"), path, errors, 128);
            validateStableId(record.id, path + QStringLiteral(".id"), errors);
            record.binary = readString(object, QStringLiteral("binary"), path, errors, maximumStateStringLength);
            validateRe2Expression(
                object, QStringLiteral("binary"), path, errors
            );
            record.type = readString(object, QStringLiteral("type"), path, errors, 64);
            record.mode = readString(object, QStringLiteral("mode"), path, errors, 16);
            static const QSet<QString> types{QStringLiteral("screencopy"), QStringLiteral("cursorpos"), QStringLiteral("plugin"), QStringLiteral("keyboard"), QStringLiteral("input-capture")};
            static const QSet<QString> modes{QStringLiteral("ask"), QStringLiteral("allow"), QStringLiteral("deny")};
            if (!record.type.isEmpty() && !types.contains(record.type)) addError(errors, path + QStringLiteral(".type"), QStringLiteral("state.invalid-permission-type"), QStringLiteral("The permission type is not supported by 0.56."));
            if (!record.mode.isEmpty() && !modes.contains(record.mode)) addError(errors, path + QStringLiteral(".mode"), QStringLiteral("state.invalid-permission-mode"), QStringLiteral("The permission mode is not supported."));
            return record;
        }
    );

    state.environment = parseRecordArray<EnvironmentConfiguration>(
        root,
        QStringLiteral("environment"),
        maximumEnvironmentVariables,
        result.errors,
        [](const QJsonObject &object, const QString &path, ValidationErrors &errors) {
            rejectUnknownFields(object, {QStringLiteral("id"), QStringLiteral("name"), QStringLiteral("value"), QStringLiteral("scope")}, path, errors);
            EnvironmentConfiguration record;
            record.id = readString(object, QStringLiteral("id"), path, errors, 128);
            validateStableId(record.id, path + QStringLiteral(".id"), errors);
            record.name = readString(object, QStringLiteral("name"), path, errors, 128);
            static const QRegularExpression environmentName(QStringLiteral("^[A-Za-z_][A-Za-z0-9_]*$"));
            if (!record.name.isEmpty() && !environmentName.match(record.name).hasMatch()) addError(errors, path + QStringLiteral(".name"), QStringLiteral("state.invalid-environment-name"), QStringLiteral("The environment variable name is invalid."));
            record.value = readString(object, QStringLiteral("value"), path, errors, maximumStateStringLength, true, true);
            const auto scope = readString(object, QStringLiteral("scope"), path, errors, 16);
            if (scope == QStringLiteral("hyprland")) record.scope = EnvironmentScope::Hyprland;
            else if (scope == QStringLiteral("uwsm")) record.scope = EnvironmentScope::Uwsm;
            else if (!scope.isEmpty()) addError(errors, path + QStringLiteral(".scope"), QStringLiteral("state.invalid-environment-scope"), QStringLiteral("Only hyprland and uwsm ownership are supported."));
            return record;
        }
    );

    const auto rejectDuplicateIdentities = [&result](
        const auto &records,
        const QString &surface,
        const QString &field,
        const auto &identityFor
    ) {
        QSet<QString> identities;
        for (qsizetype index = 0; index < records.size(); ++index) {
            const auto identity = identityFor(records.at(index));
            if (identity.isEmpty()) continue;
            if (identities.contains(identity)) {
                addError(
                    result.errors,
                    QStringLiteral("$.") + surface + QLatin1Char('[')
                        + QString::number(index) + QStringLiteral("].") + field,
                    QStringLiteral("state.duplicate-natural-identity"),
                    QStringLiteral("The managed collection contains a duplicate natural identity.")
                );
            }
            identities.insert(identity);
        }
    };
    rejectDuplicateIdentities(
        state.monitors, QStringLiteral("monitors"), QStringLiteral("selector"),
        [](const MonitorConfiguration &record) { return record.selector; }
    );
    QMap<QString, QString> monitorMirrors;
    for (const auto &monitor : state.monitors) {
        if (!monitor.selector.isEmpty()) {
            monitorMirrors.insert(monitor.selector, monitor.mirror);
        }
    }
    QSet<QString> reportedMirrorCycles;
    for (qsizetype index = 0; index < state.monitors.size(); ++index) {
        const auto &monitor = state.monitors.at(index);
        if (monitor.mirror.isEmpty()) continue;
        QSet<QString> visited{monitor.selector};
        auto target = monitor.mirror;
        while (monitorMirrors.contains(target)
               && !monitorMirrors.value(target).isEmpty()) {
            if (visited.contains(target)) {
                const auto cycleIdentity = QStringList(visited.values())
                                               .join(QLatin1Char('|'));
                if (!reportedMirrorCycles.contains(cycleIdentity)) {
                    addError(
                        result.errors,
                        QStringLiteral("$.monitors[") + QString::number(index)
                            + QStringLiteral("].mirror"),
                        QStringLiteral("state.monitor-mirror-cycle"),
                        QStringLiteral("Managed monitor mirror targets cannot self-reference or form a cycle.")
                    );
                    reportedMirrorCycles.insert(cycleIdentity);
                }
                break;
            }
            visited.insert(target);
            target = monitorMirrors.value(target);
        }
        if (target == monitor.selector) {
            addError(
                result.errors,
                QStringLiteral("$.monitors[") + QString::number(index)
                    + QStringLiteral("].mirror"),
                QStringLiteral("state.monitor-mirror-cycle"),
                QStringLiteral("A monitor cannot mirror itself.")
            );
        }
    }
    rejectDuplicateIdentities(
        state.animations, QStringLiteral("animations"), QStringLiteral("name"),
        [](const AnimationConfiguration &record) { return record.name; }
    );
    rejectDuplicateIdentities(
        state.workspaceRules, QStringLiteral("workspaceRules"),
        QStringLiteral("selector"),
        [](const WorkspaceRule &record) { return record.selector; }
    );
    rejectDuplicateIdentities(
        state.windowRules, QStringLiteral("windowRules"), QStringLiteral("name"),
        [](const WindowRule &record) { return record.name; }
    );
    rejectDuplicateIdentities(
        state.layerRules, QStringLiteral("layerRules"), QStringLiteral("name"),
        [](const LayerRule &record) { return record.name; }
    );
    rejectDuplicateIdentities(
        state.environment, QStringLiteral("environment"), QStringLiteral("name"),
        [](const EnvironmentConfiguration &record) { return record.name; }
    );

    QSet<QString> permissionIdentities;
    for (qsizetype index = 0; index < state.permissions.size(); ++index) {
        const auto &permission = state.permissions.at(index);
        const auto identity = permission.binary + QLatin1Char('\0')
            + permission.type;
        if (permissionIdentities.contains(identity)) {
            addError(
                result.errors,
                QStringLiteral("$.permissions[") + QString::number(index)
                    + QLatin1Char(']'),
                QStringLiteral("state.duplicate-natural-identity"),
                QStringLiteral("Permission binary/type pairs must be unique because Hyprland uses first-match ordering.")
            );
        }
        permissionIdentities.insert(identity);
    }

    const auto gestureModifierIdentity = [](const QStringList &modifiers) {
        static const QStringList order{
            QStringLiteral("shift"), QStringLiteral("caps"),
            QStringLiteral("ctrl"), QStringLiteral("alt"),
            QStringLiteral("mod2"), QStringLiteral("mod3"),
            QStringLiteral("super"), QStringLiteral("mod5")
        };
        const QSet<QString> selected(modifiers.constBegin(), modifiers.constEnd());
        QStringList canonical;
        for (const auto &modifier : order) {
            if (selected.contains(modifier)) canonical.append(modifier);
        }
        return canonical.join(QLatin1Char('+'));
    };
    const auto gestureAxis = [](const QString &direction) {
        if (direction == QStringLiteral("up")
            || direction == QStringLiteral("down")
            || direction == QStringLiteral("vertical")) {
            return QStringLiteral("vertical");
        }
        if (direction == QStringLiteral("left")
            || direction == QStringLiteral("right")
            || direction == QStringLiteral("horizontal")) {
            return QStringLiteral("horizontal");
        }
        if (direction == QStringLiteral("pinch")
            || direction == QStringLiteral("pinchIn")
            || direction == QStringLiteral("pinchOut")) {
            return QStringLiteral("pinch");
        }
        return QStringLiteral("swipe");
    };
    QVector<qsizetype> activeGestures;
    for (qsizetype index = 0; index < state.gestures.size(); ++index) {
        const auto &gesture = state.gestures.at(index);
        const auto path = QStringLiteral("$.gestures[") + QString::number(index)
            + QLatin1Char(']');
        const auto modifiers = gestureModifierIdentity(gesture.modifiers);
        const auto exactTuple = [&](const GestureConfiguration &candidate) {
            return candidate.fingers == gesture.fingers
                && candidate.direction == gesture.direction
                && gestureModifierIdentity(candidate.modifiers) == modifiers
                && candidate.scale == gesture.scale
                && candidate.disableInhibit == gesture.disableInhibit;
        };
        if (gesture.action.id == QStringLiteral("unset")) {
            const auto found = std::ranges::find_if(
                activeGestures,
                [&](const qsizetype activeIndex) {
                    return exactTuple(state.gestures.at(activeIndex));
                }
            );
            if (found == activeGestures.end()) {
                addError(result.errors, path, QStringLiteral("state.unmatched-gesture-unset"), QStringLiteral("A gesture unset must remove an exact preceding gesture tuple."));
            } else {
                activeGestures.erase(found);
            }
            continue;
        }
        const auto axis = gestureAxis(gesture.direction);
        const auto shadowed = std::ranges::find_if(
            activeGestures,
            [&](const qsizetype activeIndex) {
                const auto &prior = state.gestures.at(activeIndex);
                if (prior.fingers != gesture.fingers
                    || gestureModifierIdentity(prior.modifiers) != modifiers) {
                    return false;
                }
                return prior.direction == axis
                    || prior.direction == gesture.direction
                    || ((axis == QStringLiteral("vertical")
                            || axis == QStringLiteral("horizontal"))
                        && prior.direction == QStringLiteral("swipe"));
            }
        );
        if (shadowed != activeGestures.end()) {
            addError(result.errors, path, QStringLiteral("state.shadowed-gesture"), QStringLiteral("An earlier gesture shadows this tuple in Hyprland's ordered gesture registry."));
        } else {
            activeGestures.append(index);
        }
    }

    QSet<QString> curveNames;
    for (qsizetype index = 0; index < state.curves.size(); ++index) {
        const auto &curve = state.curves.at(index);
        if (curveNames.contains(curve.name)) addError(result.errors, QStringLiteral("$.curves[") + QString::number(index) + QStringLiteral("].name"), QStringLiteral("state.duplicate-name"), QStringLiteral("Curve names must be unique."));
        curveNames.insert(curve.name);
    }
    const QSet<QString> builtinCurves{QStringLiteral("default"), QStringLiteral("linear")};
    for (qsizetype index = 0; index < state.animations.size(); ++index) {
        const auto &animation = state.animations.at(index);
        if (!builtinCurves.contains(animation.curve) && !curveNames.contains(animation.curve)) addError(result.errors, QStringLiteral("$.animations[") + QString::number(index) + QStringLiteral("].curve"), QStringLiteral("state.unknown-curve"), QStringLiteral("The animation references an unknown curve."));
    }

    QMap<QString, QString> submapReset;
    QMap<QString, bool> submapEnabled;
    for (qsizetype index = 0; index < state.submaps.size(); ++index) {
        const auto &submap = state.submaps.at(index);
        const auto path = QStringLiteral("$.submaps[") + QString::number(index) + QLatin1Char(']');
        if (submap.name == QStringLiteral("reset") || submap.name.isEmpty()) addError(result.errors, path + QStringLiteral(".name"), QStringLiteral("state.invalid-submap-name"), QStringLiteral("reset and the empty default map are reserved."));
        if (submapReset.contains(submap.name)) addError(result.errors, path + QStringLiteral(".name"), QStringLiteral("state.duplicate-name"), QStringLiteral("Submap names must be unique."));
        submapReset.insert(submap.name, submap.reset);
        submapEnabled.insert(submap.name, submap.enabled);
    }
    for (auto iterator = submapReset.constBegin(); iterator != submapReset.constEnd(); ++iterator) {
        if (!iterator.value().isEmpty() && iterator.value() != QStringLiteral("reset") && !submapReset.contains(iterator.value())) addError(result.errors, QStringLiteral("$.submaps"), QStringLiteral("state.unknown-submap-reset"), QStringLiteral("Submap %1 resets to an unknown target.").arg(iterator.key()));
        if (submapEnabled.value(iterator.key())
            && !iterator.value().isEmpty()
            && iterator.value() != QStringLiteral("reset")
            && submapReset.contains(iterator.value())
            && !submapEnabled.value(iterator.value())) {
            addError(result.errors, QStringLiteral("$.submaps"), QStringLiteral("state.disabled-submap-target"), QStringLiteral("An enabled submap cannot reset to a disabled submap."));
        }
        QSet<QString> visited{iterator.key()};
        auto targetName = iterator.value();
        while (!targetName.isEmpty() && targetName != QStringLiteral("reset") && submapReset.contains(targetName)) {
            if (visited.contains(targetName)) {
                addError(result.errors, QStringLiteral("$.submaps"), QStringLiteral("state.submap-cycle"), QStringLiteral("Submap reset targets form a cycle."));
                break;
            }
            visited.insert(targetName);
            targetName = submapReset.value(targetName);
        }
    }

    QMap<QString, qsizetype> enabledBindingsBySubmap;
    for (const auto &binding : state.bindings) {
        if (binding.enabled && !binding.submap.isEmpty()) {
            ++enabledBindingsBySubmap[binding.submap];
        }
    }
    for (qsizetype index = 0; index < state.submaps.size(); ++index) {
        const auto &submap = state.submaps.at(index);
        if (submap.enabled && !submap.name.isEmpty()
            && enabledBindingsBySubmap.value(submap.name) == 0) {
            addError(
                result.errors,
                QStringLiteral("$.submaps[") + QString::number(index)
                    + QStringLiteral("].name"),
                QStringLiteral("state.empty-submap"),
                QStringLiteral("Every declared submap must contain at least one enabled binding.")
            );
        }
    }

    const bool includesShippedDefaultBindings =
        catalog.contractVersion == currentCatalogContractVersion
        && catalog.digest == QLatin1String(reviewedCatalogDigest)
        && actionCatalog.contractVersion == currentActionCatalogContractVersion
        && actionCatalog.digest == QLatin1String(reviewedActionCatalogDigest);
    QSet<QString> replacedDefaultBindings;
    if (includesShippedDefaultBindings) {
        for (qsizetype index = 0; index < state.bindings.size(); ++index) {
            const auto &binding = state.bindings.at(index);
            const auto *matchedDefault =
                matchedShippedDefaultKeybinding(binding);
            if (matchedDefault == nullptr) continue;
            if (replacedDefaultBindings.contains(matchedDefault->id)) {
                addError(
                    result.errors,
                    QStringLiteral("$.bindings[") + QString::number(index)
                        + QLatin1Char(']'),
                    QStringLiteral(
                        "state.duplicate-default-binding-override"
                    ),
                    QStringLiteral(
                        "Only one user-layer record may replace or disable a shipped default shortcut."
                    )
                );
            }
            replacedDefaultBindings.insert(matchedDefault->id);
        }
    }

    QSet<QString> bindingChords;
    if (includesShippedDefaultBindings) {
        for (const auto &binding : shippedDefaultKeybindings()) {
            if (!binding.enabled
                || replacedDefaultBindings.contains(binding.id)) {
                continue;
            }
            bindingChords.insert(
                binding.submap + QLatin1Char('|') + binding.normalizedChord
            );
        }
    }
    for (qsizetype index = 0; index < state.bindings.size(); ++index) {
        const auto &binding = state.bindings.at(index);
        const auto path = QStringLiteral("$.bindings[") + QString::number(index) + QLatin1Char(']');
        if (!binding.submap.isEmpty() && !submapReset.contains(binding.submap)) addError(result.errors, path + QStringLiteral(".submap"), QStringLiteral("state.unknown-submap"), QStringLiteral("The binding references an unknown submap."));
        if (binding.enabled && !binding.submap.isEmpty()
            && submapReset.contains(binding.submap)
            && !submapEnabled.value(binding.submap)) {
            addError(result.errors, path + QStringLiteral(".submap"), QStringLiteral("state.disabled-submap-target"), QStringLiteral("An enabled binding cannot be scoped to a disabled submap."));
        }
        if (binding.actionType == BindingActionType::Dispatcher
            && binding.action == QStringLiteral("submap")) {
            const auto target = binding.arguments.value(QStringLiteral("name"));
            if (target.isString()) {
                const auto targetName = target.toString();
                if (targetName != QStringLiteral("reset")
                    && !submapReset.contains(targetName)) {
                    addError(result.errors, path + QStringLiteral(".arguments.name"), QStringLiteral("state.unknown-submap"), QStringLiteral("The submap action references an unknown target."));
                } else if (binding.enabled
                           && targetName != QStringLiteral("reset")
                           && (!submapEnabled.value(targetName)
                               || enabledBindingsBySubmap.value(targetName) == 0)) {
                    addError(result.errors, path + QStringLiteral(".arguments.name"), QStringLiteral("state.empty-submap"), QStringLiteral("The submap action target has no enabled binding."));
                }
            }
        }
        if (binding.key == QStringLiteral("catchall")
            && binding.submap.isEmpty()) addError(result.errors, path + QStringLiteral(".key"), QStringLiteral("state.catchall-outside-submap"), QStringLiteral("Catchall bindings are only valid in a submap."));
        const auto scopedChord = binding.submap + QLatin1Char('|') + binding.normalizedChord;
        if (!binding.normalizedChord.isEmpty() && bindingChords.contains(scopedChord)) addError(result.errors, path, QStringLiteral("state.duplicate-chord"), QStringLiteral("Binding chords must be unique within a submap."));
        bindingChords.insert(scopedChord);
    }

    if (result.errors.isEmpty()) {
        result.value = std::move(state);
    }
    return result;
}

ValidationResult<DesiredState> parseDesiredState(
    const QByteArrayView bytes,
    const Catalog &catalog,
    const ActionCatalog &actionCatalog
)
{
    ValidationResult<DesiredState> result;
    validateActiveV1Authorities(catalog, actionCatalog, result.errors);
    auto semantic = parseDesiredStateForAuthorities(
        bytes, catalog, actionCatalog
    );
    if (result.errors.isEmpty()) {
        return semantic;
    }
    result.errors.append(semantic.errors);
    return result;
}

ValidationResult<DesiredStateV2> parseDormantDesiredStateV2(
    const QByteArrayView bytes,
    const Catalog &catalog,
    const ActionCatalog &actionCatalogV2
)
{
    ValidationResult<DesiredStateV2> result;
    validateDormantV2Authorities(catalog, actionCatalogV2, result.errors);
    if (!result.errors.isEmpty()) {
        return result;
    }

    const auto parsed = JsonSupport::parseStrictObject(
        bytes, maximumDesiredStateBytes, maximumDesiredStateDepth
    );
    if (!parsed) {
        result.errors.append(parsed.errors);
        return result;
    }

    auto semanticRoot = *parsed.value;
    const auto formatVersion = readInteger(
        semanticRoot,
        QStringLiteral("formatVersion"),
        QStringLiteral("$"),
        result.errors,
        0,
        std::numeric_limits<quint32>::max()
    );
    if (formatVersion
        && *formatVersion != dormantDesiredStateV2FormatVersion) {
        addError(
            result.errors,
            QStringLiteral("$.formatVersion"),
            QStringLiteral("state.unsupported-format-version"),
            QStringLiteral("Only the dormant desired-state v2 envelope is accepted.")
        );
    }

    const auto authorityId = readString(
        semanticRoot,
        QStringLiteral("authorityId"),
        QStringLiteral("$"),
        result.errors,
        authorityIdHexLength
    );
    if (!isCanonicalAuthorityId(authorityId)) {
        addError(
            result.errors,
            QStringLiteral("$.authorityId"),
            QStringLiteral("state.invalid-authority-id"),
            QStringLiteral(
                "Authority ID must be 32 lowercase hexadecimal characters and cannot be all zero."
            )
        );
    }

    semanticRoot.remove(QStringLiteral("authorityId"));
    semanticRoot.insert(
        QStringLiteral("formatVersion"),
        static_cast<qint64>(currentDesiredStateFormatVersion)
    );
    auto semanticBytes = JsonSupport::canonicalJson(semanticRoot);
    semanticBytes.append('\n');
    const auto semantic = parseDesiredStateForAuthorities(
        QByteArrayView(semanticBytes), catalog, actionCatalogV2
    );
    result.errors.append(semantic.errors);
    if (semantic) {
        validateDormantV2ProtectedWorkspaceRuleOrder(
            semantic.value->workspaceRules,
            result.errors
        );
        if (semantic.value->targetHyprland != QStringLiteral("0.56.2")) {
            addError(
                result.errors,
                QStringLiteral("$.targetHyprland"),
                QStringLiteral("state.dormant-v2-target-required"),
                QStringLiteral(
                    "Dormant v2 state must target exact Hyprland 0.56.2."
                )
            );
        }
    }

    if (result.errors.isEmpty() && semantic) {
        result.value = DesiredStateV2{
            .authorityId = authorityId,
            .semanticState = *semantic.value,
        };
    }
    return result;
}

ValidationErrors validateManagedActivationSafety(
    const DesiredState &state,
    const Catalog &catalog
)
{
    const auto effectiveValue = [&state, &catalog](const QString &id) {
        const auto override = state.overrides.constFind(id);
        if (override != state.overrides.constEnd()) {
            return override.value();
        }
        const auto *option = findOption(catalog, id);
        return option ? option->defaultValue : QJsonValue{};
    };

    const auto enabled = effectiveValue(
        QStringLiteral("hyprland.decoration.glow.enabled")
    ).toBool();
    const auto range = effectiveValue(
        QStringLiteral("hyprland.decoration.glow.range")
    ).toInteger();
    if (!enabled || range >= 10) {
        return {};
    }

    return {{
        .path = QStringLiteral(
            "$.overrides.hyprland.decoration.glow.range"
        ),
        .code = QStringLiteral("state.unsafe-glow-range"),
        .message = QStringLiteral(
            "Inner glow can be enabled only when its range is at least 10; "
            "disable glow or raise the range."
        ),
    }};
}

DesiredState defaultDesiredState(
    const Catalog &catalog,
    const ActionCatalog &actionCatalog
)
{
    DesiredState state;
    state.targetHyprland = toString(
        catalog.hyprland.reviewedVersion
    );
    state.catalogDigest = catalogDigest(catalog);
    state.actionCatalogDigest = actionCatalogDigest(actionCatalog);
    state.compatibility = CompatibilityDecision::Exact;
    state.workspaceRules.append(WorkspaceRule{
        .id = QLatin1String(sharedSpacingWorkspaceRuleId),
        .selector = QLatin1String(sharedSpacingWorkspaceRuleSelector),
        .enabled = true,
        .monitor = QString(),
        .persistent = false,
        .isDefault = false,
        .layout = QString(),
        .overrides = QJsonObject{
            {
                QStringLiteral("gaps_out"),
                QJsonArray{0, 0, 0, 0},
            },
        },
    });
    return state;
}

ValidationResult<DesiredStateV2> defaultDormantDesiredStateV2(
    const Catalog &catalog,
    const ActionCatalog &actionCatalogV2,
    QString authorityId
)
{
    ValidationResult<DesiredStateV2> result;
    if (!isCanonicalAuthorityId(authorityId)) {
        addError(
            result.errors,
            QStringLiteral("$.authorityId"),
            QStringLiteral("state.invalid-authority-id"),
            QStringLiteral(
                "Authority ID must be 32 lowercase hexadecimal characters and cannot be all zero."
            )
        );
    }
    validateDormantV2Authorities(catalog, actionCatalogV2, result.errors);
    if (!result.errors.isEmpty()) {
        return result;
    }

    result.value = DesiredStateV2{
        .authorityId = std::move(authorityId),
        .semanticState = defaultDesiredState(catalog, actionCatalogV2),
    };
    return result;
}

ValidationResult<QByteArray> serializeDormantDesiredStateV2(
    const DesiredStateV2 &state
)
{
    ValidationResult<QByteArray> result;
    if (!isCanonicalAuthorityId(state.authorityId)) {
        addError(
            result.errors,
            QStringLiteral("$.authorityId"),
            QStringLiteral("state.invalid-authority-id"),
            QStringLiteral(
                "Authority ID must be 32 lowercase hexadecimal characters and cannot be all zero."
            )
        );
    }
    if (state.semanticState.formatVersion
        != currentDesiredStateFormatVersion) {
        addError(
            result.errors,
            QStringLiteral("$.formatVersion"),
            QStringLiteral("state.invalid-v2-semantic-envelope"),
            QStringLiteral(
                "The dormant v2 envelope must contain exact v1 semantic state."
            )
        );
    }
    if (state.semanticState.readOnly
        || state.semanticState.opaqueFutureDocument.has_value()) {
        addError(
            result.errors,
            QStringLiteral("$"),
            QStringLiteral("state.invalid-v2-semantic-envelope"),
            QStringLiteral(
                "Dormant v2 state must be editable and cannot contain an opaque future document."
            )
        );
    }
    if (state.semanticState.catalogDigest
            != QLatin1String(dormantReviewedCatalogV2Digest)
        || state.semanticState.actionCatalogDigest
            != QLatin1String(dormantReviewedActionCatalogV2Digest)
        || state.semanticState.targetHyprland != QStringLiteral("0.56.2")) {
        addError(
            result.errors,
            QStringLiteral("$.actionCatalogDigest"),
            QStringLiteral("state.invalid-v2-semantic-envelope"),
            QStringLiteral(
                "Editable dormant v2 state must target 0.56.2 and bind the exact v2 scalar and action authorities."
            )
        );
    }
    validateDormantV2ProtectedWorkspaceRuleOrder(
        state.semanticState.workspaceRules,
        result.errors
    );
    if (!result.errors.isEmpty()) {
        return result;
    }

    const auto semanticBytes = serializeDesiredState(state.semanticState);
    const auto semanticRoot = JsonSupport::parseStrictObject(
        QByteArrayView(semanticBytes),
        maximumDesiredStateBytes,
        maximumDesiredStateDepth
    );
    if (!semanticRoot) {
        result.errors.append(semanticRoot.errors);
        return result;
    }

    auto root = *semanticRoot.value;
    root.insert(
        QStringLiteral("formatVersion"),
        static_cast<qint64>(dormantDesiredStateV2FormatVersion)
    );
    root.insert(QStringLiteral("authorityId"), state.authorityId);
    auto encoded = JsonSupport::canonicalJson(root);
    encoded.append('\n');
    if (encoded.size() > maximumDesiredStateBytes) {
        addError(
            result.errors,
            QStringLiteral("$"),
            QStringLiteral("json.size-limit"),
            QStringLiteral("The JSON document exceeds its byte limit.")
        );
        return result;
    }
    result.value = std::move(encoded);
    return result;
}

QByteArray serializeDesiredState(const DesiredState &state)
{
    if (state.opaqueFutureDocument) {
        auto encoded = JsonSupport::canonicalJson(*state.opaqueFutureDocument);
        encoded.append('\n');
        return encoded;
    }
    QJsonObject root;
    root.insert(QStringLiteral("formatVersion"), static_cast<qint64>(state.formatVersion));
    root.insert(QStringLiteral("revision"), QString::number(state.revision));
    root.insert(QStringLiteral("targetHyprland"), state.targetHyprland);
    root.insert(QStringLiteral("catalogDigest"), state.catalogDigest);
    root.insert(QStringLiteral("actionCatalogDigest"), state.actionCatalogDigest);
    QJsonObject overrides;
    for (auto iterator = state.overrides.constBegin(); iterator != state.overrides.constEnd(); ++iterator) overrides.insert(iterator.key(), iterator.value());
    root.insert(QStringLiteral("overrides"), overrides);

    QJsonArray monitors;
    for (const auto &record : state.monitors) {
        QJsonArray reserved;
        for (const auto value : record.reserved) reserved.append(value);
        const auto scale = std::holds_alternative<double>(record.scale)
            ? QJsonValue(std::get<double>(record.scale))
            : QJsonValue(std::get<QString>(record.scale));
        monitors.append(QJsonObject{
            {QStringLiteral("id"), record.id},
            {QStringLiteral("selector"), record.selector},
            {QStringLiteral("enabled"), record.enabled},
            {QStringLiteral("mode"), record.mode},
            {QStringLiteral("position"), record.position},
            {QStringLiteral("scale"), scale},
            {QStringLiteral("reserved"), reserved},
            {QStringLiteral("transform"), record.transform},
            {QStringLiteral("mirror"), record.mirror},
            {QStringLiteral("bitdepth"), record.bitdepth},
            {QStringLiteral("cm"), record.colorManagement},
            {QStringLiteral("sdrEotf"), record.sdrEotf},
            {QStringLiteral("sdrBrightness"), record.sdrBrightness},
            {QStringLiteral("sdrSaturation"), record.sdrSaturation},
            {QStringLiteral("vrr"), record.vrr},
            {QStringLiteral("icc"), record.icc},
            {QStringLiteral("supportsWideColor"), record.supportsWideColor},
            {QStringLiteral("supportsHdr"), record.supportsHdr},
            {QStringLiteral("sdrMinLuminance"), record.sdrMinLuminance},
            {QStringLiteral("sdrMaxLuminance"), record.sdrMaxLuminance},
            {QStringLiteral("minLuminance"), record.minLuminance},
            {QStringLiteral("maxLuminance"), record.maxLuminance},
            {QStringLiteral("maxAvgLuminance"), record.maxAvgLuminance},
        });
    }
    root.insert(QStringLiteral("monitors"), monitors);
    QJsonArray devices;
    for (const auto &record : state.devices) devices.append(QJsonObject{{QStringLiteral("id"), record.id}, {QStringLiteral("selector"), record.selector}, {QStringLiteral("kind"), record.kind}, {QStringLiteral("enabled"), record.enabled}, {QStringLiteral("overrides"), record.overrides}});
    root.insert(QStringLiteral("devices"), devices);
    QJsonArray curves;
    for (const auto &record : state.curves) {
        QJsonObject object{{QStringLiteral("id"), record.id}, {QStringLiteral("name"), record.name}};
        if (const auto *bezier = std::get_if<BezierCurveParameters>(&record.parameters)) {
            object.insert(QStringLiteral("type"), QStringLiteral("bezier"));
            QJsonArray points;
            for (const auto &point : bezier->points) points.append(QJsonArray{point.at(0), point.at(1)});
            object.insert(QStringLiteral("points"), points);
        } else if (const auto *spring = std::get_if<SpringCurveParameters>(&record.parameters)) {
            object.insert(QStringLiteral("type"), QStringLiteral("spring"));
            object.insert(QStringLiteral("stiffness"), spring->stiffness);
            object.insert(QStringLiteral("dampening"), spring->dampening);
            object.insert(QStringLiteral("mass"), spring->mass);
        }
        curves.append(object);
    }
    root.insert(QStringLiteral("curves"), curves);
    QJsonArray animations;
    for (const auto &record : state.animations) animations.append(QJsonObject{{QStringLiteral("id"), record.id}, {QStringLiteral("name"), record.name}, {QStringLiteral("enabled"), record.enabled}, {QStringLiteral("speed"), record.speed}, {QStringLiteral("curve"), record.curve}, {QStringLiteral("style"), record.style}});
    root.insert(QStringLiteral("animations"), animations);
    QJsonArray gestures;
    for (const auto &record : state.gestures) {
        gestures.append(QJsonObject{{QStringLiteral("id"), record.id}, {QStringLiteral("fingers"), static_cast<qint64>(record.fingers)}, {QStringLiteral("direction"), record.direction}, {QStringLiteral("modifiers"), stringArray(record.modifiers)}, {QStringLiteral("scale"), record.scale}, {QStringLiteral("disableInhibit"), record.disableInhibit}, {QStringLiteral("action"), record.action.payload}});
    }
    root.insert(QStringLiteral("gestures"), gestures);
    QJsonArray workspaceRules;
    for (const auto &record : state.workspaceRules) workspaceRules.append(QJsonObject{{QStringLiteral("id"), record.id}, {QStringLiteral("selector"), record.selector}, {QStringLiteral("enabled"), record.enabled}, {QStringLiteral("monitor"), record.monitor}, {QStringLiteral("persistent"), record.persistent}, {QStringLiteral("isDefault"), record.isDefault}, {QStringLiteral("layout"), record.layout}, {QStringLiteral("overrides"), record.overrides}});
    root.insert(QStringLiteral("workspaceRules"), workspaceRules);
    QJsonArray windowRules;
    for (const auto &record : state.windowRules) windowRules.append(QJsonObject{{QStringLiteral("id"), record.id}, {QStringLiteral("name"), record.name}, {QStringLiteral("enabled"), record.enabled}, {QStringLiteral("match"), record.match}, {QStringLiteral("effects"), record.effects}});
    root.insert(QStringLiteral("windowRules"), windowRules);
    QJsonArray layerRules;
    for (const auto &record : state.layerRules) layerRules.append(QJsonObject{{QStringLiteral("id"), record.id}, {QStringLiteral("name"), record.name}, {QStringLiteral("enabled"), record.enabled}, {QStringLiteral("match"), record.match}, {QStringLiteral("effects"), record.effects}});
    root.insert(QStringLiteral("layerRules"), layerRules);
    QJsonArray submaps;
    for (const auto &record : state.submaps) submaps.append(QJsonObject{{QStringLiteral("id"), record.id}, {QStringLiteral("name"), record.name}, {QStringLiteral("reset"), record.reset}, {QStringLiteral("enabled"), record.enabled}});
    root.insert(QStringLiteral("submaps"), submaps);
    QJsonArray bindings;
    for (const auto &record : state.bindings) {
        QString actionType;
        switch (record.actionType) {
        case BindingActionType::Dispatcher: actionType = QStringLiteral("dispatcher"); break;
        case BindingActionType::DefaultApp: actionType = QStringLiteral("defaultApp"); break;
        case BindingActionType::HyprShelld: actionType = QStringLiteral("hyprshelld"); break;
        }
        QJsonObject options{{QStringLiteral("repeating"), record.options.repeating}, {QStringLiteral("locked"), record.options.locked}, {QStringLiteral("release"), record.options.release}, {QStringLiteral("nonConsuming"), record.options.nonConsuming}, {QStringLiteral("autoConsuming"), record.options.autoConsuming}, {QStringLiteral("transparent"), record.options.transparent}, {QStringLiteral("ignoreMods"), record.options.ignoreMods}, {QStringLiteral("dontInhibit"), record.options.dontInhibit}, {QStringLiteral("longPress"), record.options.longPress}, {QStringLiteral("submapUniversal"), record.options.submapUniversal}, {QStringLiteral("click"), record.options.click}, {QStringLiteral("drag"), record.options.drag}, {QStringLiteral("allowInputCapture"), record.options.allowInputCapture}};
        if (record.options.device) options.insert(QStringLiteral("device"), QJsonObject{{QStringLiteral("inclusive"), record.options.device->inclusive}, {QStringLiteral("list"), stringArray(record.options.device->list)}});
        bindings.append(QJsonObject{{QStringLiteral("id"), record.id}, {QStringLiteral("modifiers"), stringArray(record.modifiers)}, {QStringLiteral("key"), record.key}, {QStringLiteral("actionType"), actionType}, {QStringLiteral("action"), record.action}, {QStringLiteral("arguments"), record.arguments}, {QStringLiteral("description"), record.description}, {QStringLiteral("enabled"), record.enabled}, {QStringLiteral("submap"), record.submap}, {QStringLiteral("options"), options}});
    }
    root.insert(QStringLiteral("bindings"), bindings);
    QJsonArray permissions;
    for (const auto &record : state.permissions) permissions.append(QJsonObject{{QStringLiteral("id"), record.id}, {QStringLiteral("binary"), record.binary}, {QStringLiteral("type"), record.type}, {QStringLiteral("mode"), record.mode}});
    root.insert(QStringLiteral("permissions"), permissions);
    QJsonArray environment;
    for (const auto &record : state.environment) environment.append(QJsonObject{{QStringLiteral("id"), record.id}, {QStringLiteral("name"), record.name}, {QStringLiteral("value"), record.value}, {QStringLiteral("scope"), record.scope == EnvironmentScope::Hyprland ? QStringLiteral("hyprland") : QStringLiteral("uwsm")}});
    root.insert(QStringLiteral("environment"), environment);
    auto encoded = JsonSupport::canonicalJson(root);
    encoded.append('\n');
    return encoded;
}

} // namespace HyprShelld::Hyprland
