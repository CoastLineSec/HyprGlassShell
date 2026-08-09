#include "declarative_document.h"

#include "strict_json.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSet>

#include <algorithm>
#include <cmath>
#include <utility>

namespace HyprShelld::Components {
namespace {

constexpr int maximumDocumentDepth = 8;

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
                QStringLiteral("declarative.unknown-field"),
                QStringLiteral("The declarative document contains an unknown field.")
            );
        }
    }
}

[[nodiscard]] bool hasDisallowedCharacter(const QString &value)
{
    for (const auto codePoint : value.toUcs4()) {
        const auto category = QChar::category(
            static_cast<char32_t>(codePoint)
        );
        if (category == QChar::Other_Control
            || category == QChar::Other_Format
            || category == QChar::Other_Surrogate
            || category == QChar::Other_NotAssigned
            || category == QChar::Separator_Line
            || category == QChar::Separator_Paragraph) {
            return true;
        }
    }
    return false;
}

[[nodiscard]] std::optional<QString> boundedText(
    const QJsonValue &value,
    const QString &path,
    const qsizetype maximumLength,
    ValidationErrors &errors
)
{
    if (!value.isString()) {
        addError(
            errors,
            path,
            QStringLiteral("declarative.string-required"),
            QStringLiteral("A string value is required.")
        );
        return std::nullopt;
    }
    const auto text = value.toString().normalized(
        QString::NormalizationForm_C
    );
    if (text.isEmpty() || text.size() > maximumLength
        || text != text.trimmed() || hasDisallowedCharacter(text)) {
        addError(
            errors,
            path,
            QStringLiteral("declarative.invalid-string"),
            QStringLiteral("The string is empty, padded, too long, or contains disallowed characters.")
        );
        return std::nullopt;
    }
    return text;
}

[[nodiscard]] bool isSettingKey(const QString &key)
{
    static const QRegularExpression expression(QStringLiteral(
        "^[a-z][A-Za-z0-9]{0,63}$"
    ));
    return expression.match(key).hasMatch();
}

} // namespace

bool isValidDeclarativeResolvedText(const QString &value)
{
    return !value.isEmpty()
        && value.size() <= maximumDeclarativeResolvedTextLength
        && value == value.normalized(QString::NormalizationForm_C)
        && value == value.trimmed() && !hasDisallowedCharacter(value);
}

ValidationResult<DeclarativeDocument> parseDeclarativeDocument(
    const QByteArrayView bytes,
    const SettingsSchema *settingsSchema
)
{
    ValidationResult<DeclarativeDocument> result;
    const auto parsed = parseStrictJsonObject(
        bytes,
        {
            .maximumBytes = maximumDeclarativeDocumentBytes,
            .maximumDepth = maximumDocumentDepth,
        }
    );
    if (!parsed) {
        result.errors = parsed.errors;
        return result;
    }

    const auto &root = *parsed.value;
    rejectUnknownFields(
        root,
        {
            QStringLiteral("documentVersion"),
            QStringLiteral("type"),
            QStringLiteral("text"),
            QStringLiteral("tooltip"),
            QStringLiteral("maximumWidth"),
        },
        QStringLiteral("$"),
        result.errors
    );

    DeclarativeDocument document;
    const auto version = root.value(QStringLiteral("documentVersion"));
    if (!version.isDouble() || !std::isfinite(version.toDouble())
        || version.toDouble() != 1.0) {
        addError(
            result.errors,
            QStringLiteral("$.documentVersion"),
            QStringLiteral("declarative.unsupported-version"),
            QStringLiteral("documentVersion must be exactly 1.")
        );
    }
    const auto type = root.value(QStringLiteral("type"));
    if (!type.isString()
        || type.toString() != QStringLiteral("text-pill")) {
        addError(
            result.errors,
            QStringLiteral("$.type"),
            QStringLiteral("declarative.unsupported-type"),
            QStringLiteral("Version one supports only the text-pill primitive.")
        );
    }

    const auto textValue = root.value(QStringLiteral("text"));
    if (!textValue.isObject()) {
        addError(
            result.errors,
            QStringLiteral("$.text"),
            QStringLiteral("declarative.text-object-required"),
            QStringLiteral("text must be a literal or component-setting source object.")
        );
    } else {
        const auto text = textValue.toObject();
        rejectUnknownFields(
            text,
            {QStringLiteral("literal"), QStringLiteral("setting")},
            QStringLiteral("$.text"),
            result.errors
        );
        const auto hasLiteral = text.contains(QStringLiteral("literal"));
        const auto hasSetting = text.contains(QStringLiteral("setting"));
        if (hasLiteral == hasSetting) {
            addError(
                result.errors,
                QStringLiteral("$.text"),
                QStringLiteral("declarative.exactly-one-text-source"),
                QStringLiteral("text must contain exactly one of literal or setting.")
            );
        } else if (hasLiteral) {
            const auto literal = boundedText(
                text.value(QStringLiteral("literal")),
                QStringLiteral("$.text.literal"),
                maximumDeclarativeResolvedTextLength,
                result.errors
            );
            if (literal.has_value()) {
                document.text = {
                    .kind = DeclarativeTextSourceKind::Literal,
                    .value = *literal,
                };
            }
        } else {
            const auto setting = boundedText(
                text.value(QStringLiteral("setting")),
                QStringLiteral("$.text.setting"),
                64,
                result.errors
            );
            if (setting.has_value()) {
                document.text = {
                    .kind = DeclarativeTextSourceKind::ComponentSetting,
                    .value = *setting,
                };
                if (!isSettingKey(*setting)) {
                    addError(
                        result.errors,
                        QStringLiteral("$.text.setting"),
                        QStringLiteral("declarative.invalid-setting-key"),
                        QStringLiteral("The component setting key is malformed.")
                    );
                } else {
                    const auto *definition = settingsSchema == nullptr
                        ? nullptr
                        : settingsSchema->find(*setting);
                    if (definition == nullptr) {
                        addError(
                            result.errors,
                            QStringLiteral("$.text.setting"),
                            QStringLiteral("declarative.unknown-setting"),
                            QStringLiteral("The text source does not resolve to a declared setting.")
                        );
                    } else if (
                        definition->scope != SettingScope::Component
                        || (definition->type != SettingType::String
                            && definition->type
                                != SettingType::Enumeration)
                    ) {
                        addError(
                            result.errors,
                            QStringLiteral("$.text.setting"),
                            QStringLiteral("declarative.unsupported-setting"),
                            QStringLiteral("Text may bind only to a component-scoped string or enumeration setting.")
                        );
                    } else if (
                        definition->type == SettingType::String
                        && (!definition->minimumLength.has_value()
                            || *definition->minimumLength < 1
                            || !definition->maximumLength.has_value()
                            || *definition->maximumLength
                                > maximumDeclarativeResolvedTextLength)
                    ) {
                        addError(
                            result.errors,
                            QStringLiteral("$.text.setting"),
                            QStringLiteral("declarative.invalid-setting-text-bounds"),
                            QStringLiteral("A bound string setting must require one through %1 characters.")
                                .arg(maximumDeclarativeResolvedTextLength)
                        );
                    } else if (!definition->defaultValue.isString()
                        || !isValidDeclarativeResolvedText(
                            definition->defaultValue.toString()
                        )) {
                        addError(
                            result.errors,
                            QStringLiteral("$.text.setting"),
                            QStringLiteral(
                                "declarative.invalid-setting-default"
                            ),
                            QStringLiteral(
                                "The bound setting default must be valid normalized renderer text."
                            )
                        );
                    } else if (definition->type
                            == SettingType::Enumeration
                        && std::ranges::any_of(
                            definition->options,
                            [](const EnumOption &option) {
                                return !isValidDeclarativeResolvedText(
                                    option.value
                                );
                            }
                        )) {
                        addError(
                            result.errors,
                            QStringLiteral("$.text.setting"),
                            QStringLiteral(
                                "declarative.invalid-setting-options"
                            ),
                            QStringLiteral(
                                "Every bound enumeration value must be valid normalized renderer text."
                            )
                        );
                    }
                }
            }
        }
    }

    if (root.contains(QStringLiteral("tooltip"))) {
        document.tooltip = boundedText(
            root.value(QStringLiteral("tooltip")),
            QStringLiteral("$.tooltip"),
            maximumDeclarativeTooltipLength,
            result.errors
        );
    }

    if (root.contains(QStringLiteral("maximumWidth"))) {
        const auto width = root.value(QStringLiteral("maximumWidth"));
        if (!width.isDouble() || !std::isfinite(width.toDouble())
            || std::floor(width.toDouble()) != width.toDouble()
            || width.toDouble() < minimumDeclarativeMaximumWidth
            || width.toDouble() > maximumDeclarativeMaximumWidth) {
            addError(
                result.errors,
                QStringLiteral("$.maximumWidth"),
                QStringLiteral("declarative.invalid-maximum-width"),
                QStringLiteral("maximumWidth must be an integer from %1 through %2.")
                    .arg(minimumDeclarativeMaximumWidth)
                    .arg(maximumDeclarativeMaximumWidth)
            );
        } else {
            document.maximumWidth = static_cast<quint32>(width.toDouble());
        }
    }

    if (result.errors.isEmpty()) {
        result.value = std::move(document);
    }
    return result;
}

QByteArray serializeDeclarativeDocument(
    const DeclarativeDocument &document
)
{
    QJsonObject text;
    if (document.text.kind == DeclarativeTextSourceKind::Literal) {
        text.insert(QStringLiteral("literal"), document.text.value);
    } else {
        text.insert(QStringLiteral("setting"), document.text.value);
    }

    QJsonObject root{
        {QStringLiteral("documentVersion"), 1},
        {QStringLiteral("type"), QStringLiteral("text-pill")},
        {QStringLiteral("text"), text},
    };
    if (document.tooltip.has_value()) {
        root.insert(QStringLiteral("tooltip"), *document.tooltip);
    }
    if (document.maximumWidth.has_value()) {
        root.insert(
            QStringLiteral("maximumWidth"),
            static_cast<qint64>(*document.maximumWidth)
        );
    }
    return QJsonDocument(root).toJson(QJsonDocument::Compact);
}

} // namespace HyprShelld::Components
