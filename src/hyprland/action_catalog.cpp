#include "action_catalog.h"

#include "json_support.h"

#include <QCryptographicHash>
#include <QJsonArray>
#include <QRegularExpression>
#include <QSet>
#include <QUrl>

#include <algorithm>
#include <cmath>
#include <string>

namespace HyprShelld::Hyprland {
namespace {

constexpr int maximumActionDocumentDepth = 64;

struct ActionCatalogContract final {
    quint32 contractVersion;
    const char *reviewedVersion;
    const char *reviewedTag;
    const char *reviewedCommit;
    bool requiresPatchRange;
    quint32 minimumPatch;
    quint32 maximumPatch;
    bool requiresSourceManifestDigest;
    const char *sourceManifestDigest;
    const char *dispatcherSourceSha256;
    const char *integrityDigest;
};

[[nodiscard]] const ActionCatalogContract &activeActionCatalogContract()
{
    static const ActionCatalogContract contract{
        .contractVersion = currentActionCatalogContractVersion,
        .reviewedVersion = "0.56.1",
        .reviewedTag = "v0.56.1",
        .reviewedCommit = "5c9377c15f85c50648f35ca5a213754f95b93ca0",
        .requiresPatchRange = false,
        .minimumPatch = 0,
        .maximumPatch = 0,
        .requiresSourceManifestDigest = false,
        .sourceManifestDigest = nullptr,
        .dispatcherSourceSha256 = "76488e1f4893fcf835c13ed98e51ab4d1c72d76a12c753eb0ad3a2237bf95223",
        .integrityDigest = reviewedActionCatalogDigest,
    };
    return contract;
}

[[nodiscard]] const ActionCatalogContract &dormantActionCatalogContractV2()
{
    static const ActionCatalogContract contract{
        .contractVersion = dormantActionCatalogV2ContractVersion,
        .reviewedVersion = "0.56.2",
        .reviewedTag = "v0.56.2",
        .reviewedCommit = "efb50993780079460b0cbed1363e2166a2de1d9f",
        .requiresPatchRange = true,
        .minimumPatch = 2,
        .maximumPatch = 2,
        .requiresSourceManifestDigest = true,
        .sourceManifestDigest = dormantReviewedSourceManifestDigest,
        .dispatcherSourceSha256 = "a109eeb982856e0fe2ac9d88c29115a09984511787e19a20e7b4804e14a9d4de",
        .integrityDigest = dormantReviewedActionCatalogV2Digest,
    };
    return contract;
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

void rejectUnknownFields(
    const QJsonObject &object,
    const QSet<QString> &allowed,
    const QString &path,
    ValidationErrors &errors,
    const QString &code = QStringLiteral("action-catalog.unknown-field")
)
{
    for (auto iterator = object.constBegin(); iterator != object.constEnd();
         ++iterator) {
        if (!allowed.contains(iterator.key())) {
            addError(
                errors,
                path + QLatin1Char('.') + iterator.key(),
                code,
                QStringLiteral("Unsupported field: %1").arg(iterator.key())
            );
        }
    }
}

[[nodiscard]] QString readString(
    const QJsonObject &object,
    const QString &key,
    const QString &path,
    const qsizetype maximumLength,
    ValidationErrors &errors,
    const bool allowEmpty = false
)
{
    const auto valuePath = path + QLatin1Char('.') + key;
    const auto value = object.value(key);
    if (!value.isString()) {
        addError(errors, valuePath, QStringLiteral("action-catalog.string-required"), QStringLiteral("A string is required."));
        return {};
    }
    const auto text = value.toString();
    if ((!allowEmpty && text.isEmpty()) || text.size() > maximumLength
        || text != text.normalized(QString::NormalizationForm_C)
        || text.contains(QChar::Null)) {
        addError(errors, valuePath, QStringLiteral("action-catalog.invalid-string"), QStringLiteral("The string is empty, non-canonical, too long, or contains NUL."));
        return {};
    }
    return text;
}

[[nodiscard]] bool isSafePresentationText(const QString &text)
{
    return std::ranges::none_of(text, [](const QChar character) {
        const auto category = character.category();
        return category == QChar::Other_Control
            || category == QChar::Other_Format;
    });
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
        addError(errors, path + QLatin1Char('.') + key, QStringLiteral("action-catalog.object-required"), QStringLiteral("An object is required."));
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
        addError(errors, path + QLatin1Char('.') + key, QStringLiteral("action-catalog.array-required"), QStringLiteral("An array is required."));
        return {};
    }
    return value.toArray();
}

[[nodiscard]] bool isInteger(const QJsonValue &value)
{
    return value.isDouble() && std::isfinite(value.toDouble())
        && std::floor(value.toDouble()) == value.toDouble();
}

[[nodiscard]] std::optional<quint32> readPatch(
    const QJsonObject &object,
    const QString &key,
    const QString &path,
    ValidationErrors &errors
)
{
    const auto value = object.value(key);
    if (!isInteger(value) || value.toDouble() < 0
        || value.toDouble() > 65535) {
        addError(
            errors,
            path + QLatin1Char('.') + key,
            QStringLiteral("action-catalog.invalid-patch-range"),
            QStringLiteral("A supported unsigned patch number is required.")
        );
        return std::nullopt;
    }
    return static_cast<quint32>(value.toDouble());
}

[[nodiscard]] bool isSha256(const QString &value)
{
    static const QRegularExpression expression(
        QStringLiteral("^[0-9a-f]{64}$")
    );
    return expression.match(value).hasMatch();
}

[[nodiscard]] std::optional<qsizetype> schemaSize(
    const QJsonObject &schema,
    const QString &key,
    const QString &path,
    ValidationErrors &errors
)
{
    if (!schema.contains(key)) return std::nullopt;
    const auto value = schema.value(key);
    if (!isInteger(value) || value.toDouble() < 0
        || value.toDouble() > maximumSchemaProperties * 16) {
        addError(errors, path + QLatin1Char('.') + key, QStringLiteral("action-schema.invalid-bound"), QStringLiteral("A supported non-negative schema bound is required."));
        return std::nullopt;
    }
    return static_cast<qsizetype>(value.toDouble());
}

[[nodiscard]] std::optional<double> schemaNumber(
    const QJsonObject &schema,
    const QString &key,
    const QString &path,
    ValidationErrors &errors
)
{
    if (!schema.contains(key)) return std::nullopt;
    const auto value = schema.value(key);
    if (!value.isDouble() || !std::isfinite(value.toDouble())) {
        addError(errors, path + QLatin1Char('.') + key, QStringLiteral("action-schema.invalid-bound"), QStringLiteral("A finite schema bound is required."));
        return std::nullopt;
    }
    return value.toDouble();
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
        const auto codePoint = character.unicode();
        if (codePoint < 0x20 || codePoint == 0x7f) {
            converted.append(QStringLiteral("\\x{%1}").arg(
                static_cast<quint32>(codePoint), 4, 16, QLatin1Char('0')
            ));
        } else {
            converted.append(character);
        }
    }
    return converted;
}

[[nodiscard]] std::optional<ContractValueType> contractType(
    const QString &type
)
{
    if (type == QStringLiteral("object")) return ContractValueType::Object;
    if (type == QStringLiteral("array")) return ContractValueType::Array;
    if (type == QStringLiteral("string")) return ContractValueType::String;
    if (type == QStringLiteral("number")) return ContractValueType::Number;
    if (type == QStringLiteral("integer")) return ContractValueType::Integer;
    if (type == QStringLiteral("boolean")) return ContractValueType::Boolean;
    if (type == QStringLiteral("null")) return ContractValueType::Null;
    return std::nullopt;
}

[[nodiscard]] std::optional<ContractValueType> contractTypeForValue(
    const QJsonValue &value
)
{
    if (value.isObject()) return ContractValueType::Object;
    if (value.isArray()) return ContractValueType::Array;
    if (value.isString()) return ContractValueType::String;
    if (value.isBool()) return ContractValueType::Boolean;
    if (value.isNull()) return ContractValueType::Null;
    if (value.isDouble()) {
        return isInteger(value) ? ContractValueType::Integer
                                : ContractValueType::Number;
    }
    return std::nullopt;
}

class ContractCompiler final {
public:
    ContractCompiler(QJsonObject definitions, ValidationErrors &errors)
        : definitions_(std::move(definitions))
        , errors_(errors)
    {
    }

    [[nodiscard]] std::shared_ptr<const ValueContract> compileReference(
        const QString &externalReference,
        const QString &path
    )
    {
        constexpr auto prefix = "config.schema.json#/$defs/";
        if (!externalReference.startsWith(QLatin1String(prefix))) {
            addError(errors_, path, QStringLiteral("action-schema.remote-reference"), QStringLiteral("Only local config.schema.json definitions are accepted."));
            return {};
        }
        const auto name = externalReference.sliced(
            static_cast<qsizetype>(std::char_traits<char>::length(prefix))
        );
        return compileNamedDefinition(name, path, 0);
    }

private:
    [[nodiscard]] std::shared_ptr<const ValueContract> compileNamedDefinition(
        const QString &name,
        const QString &path,
        const int depth
    )
    {
        if (const auto cached = cache_.constFind(name); cached != cache_.constEnd()) {
            return *cached;
        }
        if (depth > maximumSchemaReferenceDepth || active_.contains(name)) {
            addError(errors_, path, QStringLiteral("action-schema.reference-cycle"), QStringLiteral("The schema reference graph is cyclic or too deep."));
            return {};
        }
        const auto value = definitions_.value(name);
        if (!value.isObject()) {
            addError(errors_, path, QStringLiteral("action-schema.missing-reference"), QStringLiteral("The referenced config schema definition is absent."));
            return {};
        }
        active_.insert(name);
        const auto compiled = compileNode(
            value.toObject(), QStringLiteral("$.$defs.") + name, depth + 1
        );
        active_.remove(name);
        if (compiled) cache_.insert(name, compiled);
        return compiled;
    }

    [[nodiscard]] std::shared_ptr<const ValueContract> compileNode(
        const QJsonObject &schema,
        const QString &path,
        const int depth
    )
    {
        if (depth > maximumSchemaReferenceDepth) {
            addError(errors_, path, QStringLiteral("action-schema.reference-cycle"), QStringLiteral("The schema reference graph is cyclic or too deep."));
            return {};
        }
        if (++nodeCount_ > maximumSchemaNodes) {
            addError(errors_, path, QStringLiteral("action-schema.complexity-limit"), QStringLiteral("The supported schema complexity limit was exceeded."));
            return {};
        }
        rejectUnknownFields(
            schema,
            {
                QStringLiteral("$ref"), QStringLiteral("type"),
                QStringLiteral("oneOf"), QStringLiteral("anyOf"),
                QStringLiteral("properties"), QStringLiteral("required"),
                QStringLiteral("additionalProperties"), QStringLiteral("items"),
                QStringLiteral("minItems"), QStringLiteral("maxItems"),
                QStringLiteral("uniqueItems"), QStringLiteral("minProperties"),
                QStringLiteral("maxProperties"), QStringLiteral("minLength"),
                QStringLiteral("maxLength"), QStringLiteral("pattern"),
                QStringLiteral("minimum"), QStringLiteral("maximum"),
                QStringLiteral("exclusiveMinimum"), QStringLiteral("exclusiveMaximum"),
                QStringLiteral("enum"), QStringLiteral("const"),
                QStringLiteral("$comment"),
            },
            path,
            errors_,
            QStringLiteral("action-schema.unsupported-keyword")
        );
        if (schema.contains(QStringLiteral("$comment"))) {
            const auto comment = schema.value(QStringLiteral("$comment"));
            if (!comment.isString() || comment.toString().size() > 1024
                || comment.toString().contains(QChar::Null)) {
                addError(
                    errors_,
                    path + QStringLiteral(".$comment"),
                    QStringLiteral("action-schema.invalid-comment"),
                    QStringLiteral("Schema comments must be bounded NUL-free strings.")
                );
            }
        }
        if (schema.contains(QStringLiteral("$ref"))) {
            const auto referenceWithOnlyAnnotation = schema.size() == 1
                || (schema.size() == 2
                    && schema.contains(QStringLiteral("$comment")));
            if (!referenceWithOnlyAnnotation
                || !schema.value(QStringLiteral("$ref")).isString()) {
                addError(errors_, path, QStringLiteral("action-schema.invalid-reference"), QStringLiteral("A $ref may only be accompanied by the bounded $comment annotation."));
                return {};
            }
            constexpr auto localPrefix = "#/$defs/";
            const auto reference = schema.value(QStringLiteral("$ref")).toString();
            if (!reference.startsWith(QLatin1String(localPrefix))) {
                addError(errors_, path, QStringLiteral("action-schema.remote-reference"), QStringLiteral("Only local #/$defs references are supported."));
                return {};
            }
            return compileNamedDefinition(
                reference.sliced(static_cast<qsizetype>(std::char_traits<char>::length(localPrefix))),
                path + QStringLiteral(".$ref"),
                depth + 1
            );
        }


        const auto hasOneOf = schema.contains(QStringLiteral("oneOf"));
        const auto hasAnyOf = schema.contains(QStringLiteral("anyOf"));
        if (hasOneOf && hasAnyOf) {
            addError(
                errors_,
                path,
                QStringLiteral("action-schema.invalid-alternatives"),
                QStringLiteral("A schema node cannot combine oneOf and anyOf.")
            );
        }
        auto alternativeSiblings = schema.keys();
        alternativeSiblings.removeAll(QStringLiteral("oneOf"));
        alternativeSiblings.removeAll(QStringLiteral("anyOf"));
        alternativeSiblings.removeAll(QStringLiteral("$comment"));
        if ((hasOneOf || hasAnyOf) && !alternativeSiblings.isEmpty()) {
            addError(
                errors_,
                path,
                QStringLiteral("action-schema.alternative-siblings"),
                QStringLiteral("Alternative nodes cannot contain sibling constraints in the supported schema subset.")
            );
        }

        auto contract = std::make_shared<ValueContract>();
        if (schema.contains(QStringLiteral("type"))) {
            const auto value = schema.value(QStringLiteral("type"));
            const auto appendType = [&](const QJsonValue &item, const QString &itemPath) {
                if (!item.isString()) {
                    addError(errors_, itemPath, QStringLiteral("action-schema.invalid-type"), QStringLiteral("Schema types must be strings."));
                    return;
                }
                const auto type = contractType(item.toString());
                if (!type) addError(errors_, itemPath, QStringLiteral("action-schema.unsupported-type"), QStringLiteral("The schema value type is unsupported."));
                else if (contract->acceptedTypes.contains(*type)) addError(errors_, itemPath, QStringLiteral("action-schema.duplicate-type"), QStringLiteral("Schema types must be unique."));
                else contract->acceptedTypes.append(*type);
            };
            if (value.isString()) appendType(value, path + QStringLiteral(".type"));
            else if (value.isArray() && !value.toArray().isEmpty() && value.toArray().size() <= 8) {
                for (qsizetype index = 0; index < value.toArray().size(); ++index) appendType(value.toArray().at(index), path + QStringLiteral(".type[") + QString::number(index) + QLatin1Char(']'));
            } else addError(errors_, path + QStringLiteral(".type"), QStringLiteral("action-schema.invalid-type"), QStringLiteral("A type string or bounded type array is required."));
        }

        const auto compileAlternatives = [&](const QString &key, QVector<std::shared_ptr<const ValueContract>> &target) {
            if (!schema.contains(key)) return;
            const auto value = schema.value(key);
            if (!value.isArray() || value.toArray().isEmpty() || value.toArray().size() > 16) {
                addError(errors_, path + QLatin1Char('.') + key, QStringLiteral("action-schema.invalid-alternatives"), QStringLiteral("A bounded non-empty alternative array is required."));
                return;
            }
            for (qsizetype index = 0; index < value.toArray().size(); ++index) {
                if (!value.toArray().at(index).isObject()) {
                    addError(errors_, path + QLatin1Char('.') + key + QLatin1Char('[') + QString::number(index) + QLatin1Char(']'), QStringLiteral("action-schema.object-required"), QStringLiteral("An alternative schema object is required."));
                    continue;
                }
                const auto item = compileNode(value.toArray().at(index).toObject(), path + QLatin1Char('.') + key + QLatin1Char('[') + QString::number(index) + QLatin1Char(']'), depth + 1);
                if (item) target.append(item);
            }
        };
        compileAlternatives(QStringLiteral("oneOf"), contract->oneOf);
        compileAlternatives(QStringLiteral("anyOf"), contract->anyOf);

        if (schema.contains(QStringLiteral("properties"))) {
            const auto properties = schema.value(QStringLiteral("properties"));
            if (!properties.isObject() || properties.toObject().size() > maximumSchemaProperties) {
                addError(errors_, path + QStringLiteral(".properties"), QStringLiteral("action-schema.property-limit"), QStringLiteral("A bounded properties object is required."));
            } else {
                const auto propertyObject = properties.toObject();
                for (auto iterator = propertyObject.constBegin(); iterator != propertyObject.constEnd(); ++iterator) {
                    if (iterator.key().isEmpty() || iterator.key().size() > 128 || !iterator.value().isObject()) {
                        addError(errors_, path + QStringLiteral(".properties.") + iterator.key(), QStringLiteral("action-schema.invalid-property"), QStringLiteral("A bounded property schema is required."));
                        continue;
                    }
                    const auto property = compileNode(iterator.value().toObject(), path + QStringLiteral(".properties.") + iterator.key(), depth + 1);
                    if (property) contract->properties.insert(iterator.key(), property);
                }
            }
        }
        if (schema.contains(QStringLiteral("required"))) {
            const auto required = schema.value(QStringLiteral("required"));
            if (!required.isArray() || required.toArray().size() > maximumSchemaProperties) {
                addError(errors_, path + QStringLiteral(".required"), QStringLiteral("action-schema.invalid-required"), QStringLiteral("A bounded required-property array is required."));
            } else {
                for (qsizetype index = 0; index < required.toArray().size(); ++index) {
                    const auto value = required.toArray().at(index);
                    if (!value.isString() || !contract->properties.contains(value.toString())
                        || contract->requiredProperties.contains(value.toString())) {
                        addError(errors_, path + QStringLiteral(".required[") + QString::number(index) + QLatin1Char(']'), QStringLiteral("action-schema.invalid-required"), QStringLiteral("Required properties must be declared and unique."));
                    } else contract->requiredProperties.insert(value.toString());
                }
            }
        }
        if (schema.contains(QStringLiteral("additionalProperties"))) {
            const auto additional = schema.value(QStringLiteral("additionalProperties"));
            if (!additional.isBool() || additional.toBool()) {
                addError(errors_, path + QStringLiteral(".additionalProperties"), QStringLiteral("action-schema.open-object"), QStringLiteral("Action payload objects must reject additional properties."));
            } else contract->additionalProperties = false;
        } else if (contract->acceptedTypes.contains(ContractValueType::Object)) {
            addError(errors_, path, QStringLiteral("action-schema.open-object"), QStringLiteral("Action payload object schemas must explicitly close additional properties."));
        }
        if (schema.contains(QStringLiteral("items"))) {
            if (!schema.value(QStringLiteral("items")).isObject()) addError(errors_, path + QStringLiteral(".items"), QStringLiteral("action-schema.object-required"), QStringLiteral("An item schema object is required."));
            else contract->items = compileNode(schema.value(QStringLiteral("items")).toObject(), path + QStringLiteral(".items"), depth + 1);
        }
        contract->minimumItems = schemaSize(schema, QStringLiteral("minItems"), path, errors_);
        contract->maximumItems = schemaSize(schema, QStringLiteral("maxItems"), path, errors_);
        contract->minimumProperties = schemaSize(schema, QStringLiteral("minProperties"), path, errors_);
        contract->maximumProperties = schemaSize(schema, QStringLiteral("maxProperties"), path, errors_);
        contract->minimumLength = schemaSize(schema, QStringLiteral("minLength"), path, errors_);
        contract->maximumLength = schemaSize(schema, QStringLiteral("maxLength"), path, errors_);
        if (schema.contains(QStringLiteral("uniqueItems"))) {
            if (!schema.value(QStringLiteral("uniqueItems")).isBool()) addError(errors_, path + QStringLiteral(".uniqueItems"), QStringLiteral("action-schema.boolean-required"), QStringLiteral("uniqueItems must be boolean."));
            else contract->uniqueItems = schema.value(QStringLiteral("uniqueItems")).toBool();
        }
        contract->minimum = schemaNumber(schema, QStringLiteral("minimum"), path, errors_);
        contract->maximum = schemaNumber(schema, QStringLiteral("maximum"), path, errors_);
        contract->exclusiveMinimum = schemaNumber(schema, QStringLiteral("exclusiveMinimum"), path, errors_);
        contract->exclusiveMaximum = schemaNumber(schema, QStringLiteral("exclusiveMaximum"), path, errors_);
        if (schema.contains(QStringLiteral("pattern"))) {
            if (!schema.value(QStringLiteral("pattern")).isString() || schema.value(QStringLiteral("pattern")).toString().size() > 2048) {
                addError(errors_, path + QStringLiteral(".pattern"), QStringLiteral("action-schema.invalid-pattern"), QStringLiteral("A bounded valid regular expression is required."));
            } else {
                const auto pattern = qtCompatiblePattern(
                    schema.value(QStringLiteral("pattern")).toString()
                );
                if (!QRegularExpression(pattern).isValid()) addError(errors_, path + QStringLiteral(".pattern"), QStringLiteral("action-schema.invalid-pattern"), QStringLiteral("A bounded valid regular expression is required."));
                else contract->pattern = pattern;
            }
        }
        if (schema.contains(QStringLiteral("enum"))) {
            const auto values = schema.value(QStringLiteral("enum"));
            if (!values.isArray() || values.toArray().isEmpty() || values.toArray().size() > 256) addError(errors_, path + QStringLiteral(".enum"), QStringLiteral("action-schema.invalid-enum"), QStringLiteral("A bounded non-empty enum is required."));
            else {
                QSet<QByteArray> seen;
                for (const auto &value : values.toArray()) {
                    const auto encoded = JsonSupport::canonicalJson(value);
                    if (seen.contains(encoded)) addError(errors_, path + QStringLiteral(".enum"), QStringLiteral("action-schema.duplicate-enum"), QStringLiteral("Enum values must be unique."));
                    seen.insert(encoded);
                    contract->allowedValues.append(value);
                }
            }
        }
        if (schema.contains(QStringLiteral("const"))) contract->constantValue = schema.value(QStringLiteral("const"));

        // Enum- and const-only nodes are deliberately supported because the
        // checked-in action grammar uses them for closed discriminators.  No
        // other untyped node is allowed to degrade into an "accept anything"
        // contract.
        if (contract->acceptedTypes.isEmpty() && !hasOneOf && !hasAnyOf) {
            const auto appendInferredType = [&contract](const QJsonValue &value) {
                const auto inferred = contractTypeForValue(value);
                if (inferred && !contract->acceptedTypes.contains(*inferred)) {
                    contract->acceptedTypes.append(*inferred);
                }
            };
            for (const auto &value : contract->allowedValues) {
                appendInferredType(value);
            }
            if (contract->constantValue) {
                appendInferredType(*contract->constantValue);
            }
            if (contract->acceptedTypes.isEmpty()) {
                addError(
                    errors_,
                    path,
                    QStringLiteral("action-schema.unconstrained-node"),
                    QStringLiteral("Every action schema node must declare a type, reference, alternative, enum, or const constraint.")
                );
            }
        }

        const auto closedByValues = contract->constantValue.has_value()
            || !contract->allowedValues.isEmpty();
        if (!closedByValues
            && contract->acceptedTypes.contains(ContractValueType::Array)) {
            if (!contract->items) {
                addError(
                    errors_,
                    path + QStringLiteral(".items"),
                    QStringLiteral("action-schema.unbounded-array"),
                    QStringLiteral("Action arrays must declare an item contract.")
                );
            }
            if (!contract->maximumItems) {
                addError(
                    errors_,
                    path + QStringLiteral(".maxItems"),
                    QStringLiteral("action-schema.unbounded-array"),
                    QStringLiteral("Action arrays must declare a finite maximum item count.")
                );
            }
        }
        if (!closedByValues
            && contract->acceptedTypes.contains(ContractValueType::String)
            && !contract->maximumLength) {
            addError(
                errors_,
                path + QStringLiteral(".maxLength"),
                QStringLiteral("action-schema.unbounded-string"),
                QStringLiteral("Action strings must declare a finite maximum length.")
            );
        }
        const auto numeric = contract->acceptedTypes.contains(
                                 ContractValueType::Number
                             )
            || contract->acceptedTypes.contains(ContractValueType::Integer);
        if (!closedByValues && numeric) {
            const auto hasLower = contract->minimum.has_value()
                || contract->exclusiveMinimum.has_value();
            const auto hasUpper = contract->maximum.has_value()
                || contract->exclusiveMaximum.has_value();
            if (!hasLower || !hasUpper) {
                addError(
                    errors_,
                    path,
                    QStringLiteral("action-schema.unbounded-number"),
                    QStringLiteral("Action numbers must declare finite lower and upper bounds.")
                );
            }
        }
        if (contract->minimumItems && contract->maximumItems
            && *contract->minimumItems > *contract->maximumItems) {
            addError(errors_, path, QStringLiteral("action-schema.invalid-bound"), QStringLiteral("minItems cannot exceed maxItems."));
        }
        if (contract->minimumProperties && contract->maximumProperties
            && *contract->minimumProperties > *contract->maximumProperties) {
            addError(errors_, path, QStringLiteral("action-schema.invalid-bound"), QStringLiteral("minProperties cannot exceed maxProperties."));
        }
        if (contract->minimumLength && contract->maximumLength
            && *contract->minimumLength > *contract->maximumLength) {
            addError(errors_, path, QStringLiteral("action-schema.invalid-bound"), QStringLiteral("minLength cannot exceed maxLength."));
        }
        const auto lower = contract->minimum
            ? contract->minimum : contract->exclusiveMinimum;
        const auto upper = contract->maximum
            ? contract->maximum : contract->exclusiveMaximum;
        if (lower && upper && *lower > *upper) {
            addError(errors_, path, QStringLiteral("action-schema.invalid-bound"), QStringLiteral("The numeric lower bound cannot exceed the upper bound."));
        }
        return contract;
    }

    QJsonObject definitions_;
    ValidationErrors &errors_;
    QMap<QString, std::shared_ptr<const ValueContract>> cache_;
    QSet<QString> active_;
    qsizetype nodeCount_ = 0;
};

[[nodiscard]] bool actionIdValid(const QString &id)
{
    static const QRegularExpression expression(QStringLiteral(
        "^[a-z][A-Za-z0-9_]*(?:\\.[a-z][A-Za-z0-9_]*)*$"
    ));
    return id.size() <= 128 && expression.match(id).hasMatch();
}

[[nodiscard]] std::optional<UiTier> uiTier(const QString &text)
{
    if (text == QStringLiteral("common")) return UiTier::Common;
    if (text == QStringLiteral("advanced")) return UiTier::Advanced;
    if (text == QStringLiteral("expert")) return UiTier::Expert;
    return std::nullopt;
}

[[nodiscard]] std::optional<RiskLevel> riskLevel(const QString &text)
{
    if (text == QStringLiteral("safe")) return RiskLevel::Safe;
    if (text == QStringLiteral("caution")) return RiskLevel::Caution;
    if (text == QStringLiteral("dangerous")) return RiskLevel::Dangerous;
    return std::nullopt;
}

[[nodiscard]] bool documentationValid(const QString &text)
{
    const QUrl url(text, QUrl::StrictMode);
    return url.isValid() && url.scheme() == QStringLiteral("https")
        && url.host() == QStringLiteral("wiki.hypr.land")
        && url.path().startsWith(QStringLiteral("/0.56.0/"))
        && url.userInfo().isEmpty();
}

struct PinnedAction final {
    const char *id;
    const char *luaPath;
    const char *schemaName;
    ActionKind kind;
    UiTier uiTier;
    RiskLevel risk;
};

constexpr PinnedAction pinnedDispatcherActions[]{
    {"cursor.move", "dsp.cursor.move", "cursorMoveArguments", ActionKind::Dispatcher, UiTier::Advanced, RiskLevel::Safe},
    {"cursor.move_to_corner", "dsp.cursor.move_to_corner", "cursorCornerArguments", ActionKind::Dispatcher, UiTier::Advanced, RiskLevel::Safe},
    {"dpms", "dsp.dpms", "dpmsArguments", ActionKind::Dispatcher, UiTier::Advanced, RiskLevel::Caution},
    {"event", "dsp.event", "eventArguments", ActionKind::Dispatcher, UiTier::Expert, RiskLevel::Caution},
    {"exit", "dsp.exit", "emptyArguments", ActionKind::Dispatcher, UiTier::Advanced, RiskLevel::Dangerous},
    {"focus", "dsp.focus", "focusArguments", ActionKind::Dispatcher, UiTier::Advanced, RiskLevel::Safe},
    {"force_idle", "dsp.force_idle", "forceIdleArguments", ActionKind::Dispatcher, UiTier::Advanced, RiskLevel::Safe},
    {"force_renderer_reload", "dsp.force_renderer_reload", "emptyArguments", ActionKind::Dispatcher, UiTier::Expert, RiskLevel::Caution},
    {"global", "dsp.global", "globalArguments", ActionKind::Dispatcher, UiTier::Expert, RiskLevel::Caution},
    {"group.active", "dsp.group.active", "groupActiveArguments", ActionKind::Dispatcher, UiTier::Advanced, RiskLevel::Safe},
    {"group.lock", "dsp.group.lock", "toggleArguments", ActionKind::Dispatcher, UiTier::Advanced, RiskLevel::Safe},
    {"group.lock_active", "dsp.group.lock_active", "toggleArguments", ActionKind::Dispatcher, UiTier::Advanced, RiskLevel::Safe},
    {"group.move_window", "dsp.group.move_window", "groupMoveWindowArguments", ActionKind::Dispatcher, UiTier::Advanced, RiskLevel::Safe},
    {"group.next", "dsp.group.next", "emptyArguments", ActionKind::Dispatcher, UiTier::Advanced, RiskLevel::Safe},
    {"group.prev", "dsp.group.prev", "emptyArguments", ActionKind::Dispatcher, UiTier::Advanced, RiskLevel::Safe},
    {"group.toggle", "dsp.group.toggle", "emptyArguments", ActionKind::Dispatcher, UiTier::Advanced, RiskLevel::Safe},
    {"no_op", "dsp.no_op", "emptyArguments", ActionKind::Dispatcher, UiTier::Advanced, RiskLevel::Safe},
    {"pass", "dsp.pass", "passArguments", ActionKind::Dispatcher, UiTier::Advanced, RiskLevel::Caution},
    {"release_input_capture", "dsp.release_input_capture", "emptyArguments", ActionKind::Dispatcher, UiTier::Expert, RiskLevel::Caution},
    {"send_key_state", "dsp.send_key_state", "sendKeyStateArguments", ActionKind::Dispatcher, UiTier::Advanced, RiskLevel::Caution},
    {"send_shortcut", "dsp.send_shortcut", "sendShortcutArguments", ActionKind::Dispatcher, UiTier::Advanced, RiskLevel::Caution},
    {"submap", "dsp.submap", "submapArguments", ActionKind::Dispatcher, UiTier::Advanced, RiskLevel::Safe},
    {"window.alter_zorder", "dsp.window.alter_zorder", "windowAlterZOrderArguments", ActionKind::Dispatcher, UiTier::Advanced, RiskLevel::Safe},
    {"window.bring_to_top", "dsp.window.bring_to_top", "emptyArguments", ActionKind::Dispatcher, UiTier::Advanced, RiskLevel::Safe},
    {"window.center", "dsp.window.center", "emptyArguments", ActionKind::Dispatcher, UiTier::Advanced, RiskLevel::Safe},
    {"window.clear_tags", "dsp.window.clear_tags", "emptyArguments", ActionKind::Dispatcher, UiTier::Advanced, RiskLevel::Safe},
    {"window.close", "dsp.window.close", "emptyArguments", ActionKind::Dispatcher, UiTier::Advanced, RiskLevel::Safe},
    {"window.cycle_next", "dsp.window.cycle_next", "windowCycleArguments", ActionKind::Dispatcher, UiTier::Advanced, RiskLevel::Safe},
    {"window.deny_from_group", "dsp.window.deny_from_group", "toggleArguments", ActionKind::Dispatcher, UiTier::Advanced, RiskLevel::Safe},
    {"window.drag", "dsp.window.drag", "emptyArguments", ActionKind::Dispatcher, UiTier::Advanced, RiskLevel::Safe},
    {"window.float", "dsp.window.float", "toggleArguments", ActionKind::Dispatcher, UiTier::Advanced, RiskLevel::Safe},
    {"window.fullscreen", "dsp.window.fullscreen", "windowFullscreenArguments", ActionKind::Dispatcher, UiTier::Advanced, RiskLevel::Safe},
    {"window.fullscreen_state", "dsp.window.fullscreen_state", "windowFullscreenStateArguments", ActionKind::Dispatcher, UiTier::Advanced, RiskLevel::Safe},
    {"window.kill", "dsp.window.kill", "emptyArguments", ActionKind::Dispatcher, UiTier::Advanced, RiskLevel::Dangerous},
    {"window.move", "dsp.window.move", "windowMoveArguments", ActionKind::Dispatcher, UiTier::Advanced, RiskLevel::Safe},
    {"window.pin", "dsp.window.pin", "toggleArguments", ActionKind::Dispatcher, UiTier::Advanced, RiskLevel::Safe},
    {"window.pseudo", "dsp.window.pseudo", "toggleArguments", ActionKind::Dispatcher, UiTier::Advanced, RiskLevel::Safe},
    {"window.resize", "dsp.window.resize", "windowResizeArguments", ActionKind::Dispatcher, UiTier::Advanced, RiskLevel::Safe},
    {"window.signal", "dsp.window.signal", "windowSignalArguments", ActionKind::Dispatcher, UiTier::Expert, RiskLevel::Dangerous},
    {"window.swap", "dsp.window.swap", "windowSwapArguments", ActionKind::Dispatcher, UiTier::Advanced, RiskLevel::Safe},
    {"window.tag", "dsp.window.tag", "windowTagArguments", ActionKind::Dispatcher, UiTier::Advanced, RiskLevel::Safe},
    {"window.toggle_swallow", "dsp.window.toggle_swallow", "emptyArguments", ActionKind::Dispatcher, UiTier::Advanced, RiskLevel::Safe},
    {"workspace.change_id", "dsp.workspace.change_id", "workspaceChangeIdArguments", ActionKind::Dispatcher, UiTier::Advanced, RiskLevel::Safe},
    {"workspace.move", "dsp.workspace.move", "workspaceMoveArguments", ActionKind::Dispatcher, UiTier::Advanced, RiskLevel::Safe},
    {"workspace.rename", "dsp.workspace.rename", "workspaceRenameArguments", ActionKind::Dispatcher, UiTier::Advanced, RiskLevel::Safe},
    {"workspace.swap_monitors", "dsp.workspace.swap_monitors", "workspaceSwapMonitorsArguments", ActionKind::Dispatcher, UiTier::Advanced, RiskLevel::Safe},
    {"workspace.toggle_special", "dsp.workspace.toggle_special", "workspaceToggleSpecialArguments", ActionKind::Dispatcher, UiTier::Advanced, RiskLevel::Safe},
};

constexpr PinnedAction pinnedSemanticActions[]{
    {"defaultApp.browser", nullptr, "emptyArguments", ActionKind::DefaultApp, UiTier::Common, RiskLevel::Safe},
    {"defaultApp.calendar", nullptr, "emptyArguments", ActionKind::DefaultApp, UiTier::Common, RiskLevel::Safe},
    {"defaultApp.codeEditor", nullptr, "emptyArguments", ActionKind::DefaultApp, UiTier::Common, RiskLevel::Safe},
    {"defaultApp.fileManager", nullptr, "emptyArguments", ActionKind::DefaultApp, UiTier::Common, RiskLevel::Safe},
    {"defaultApp.imageViewer", nullptr, "emptyArguments", ActionKind::DefaultApp, UiTier::Common, RiskLevel::Safe},
    {"defaultApp.mail", nullptr, "emptyArguments", ActionKind::DefaultApp, UiTier::Common, RiskLevel::Safe},
    {"defaultApp.music", nullptr, "emptyArguments", ActionKind::DefaultApp, UiTier::Common, RiskLevel::Safe},
    {"defaultApp.pdfViewer", nullptr, "emptyArguments", ActionKind::DefaultApp, UiTier::Common, RiskLevel::Safe},
    {"defaultApp.systemMonitor", nullptr, "emptyArguments", ActionKind::DefaultApp, UiTier::Common, RiskLevel::Safe},
    {"defaultApp.terminal", nullptr, "emptyArguments", ActionKind::DefaultApp, UiTier::Common, RiskLevel::Safe},
    {"defaultApp.textEditor", nullptr, "emptyArguments", ActionKind::DefaultApp, UiTier::Common, RiskLevel::Safe},
    {"defaultApp.video", nullptr, "emptyArguments", ActionKind::DefaultApp, UiTier::Common, RiskLevel::Safe},
    {"hyprshelld.display.brightnessDown", nullptr, "emptyArguments", ActionKind::HyprShelld, UiTier::Common, RiskLevel::Safe},
    {"hyprshelld.display.brightnessUp", nullptr, "emptyArguments", ActionKind::HyprShelld, UiTier::Common, RiskLevel::Safe},
    {"hyprshelld.launcher", nullptr, "emptyArguments", ActionKind::HyprShelld, UiTier::Common, RiskLevel::Safe},
    {"hyprshelld.lock", nullptr, "emptyArguments", ActionKind::HyprShelld, UiTier::Common, RiskLevel::Safe},
    {"hyprshelld.media.next", nullptr, "emptyArguments", ActionKind::HyprShelld, UiTier::Common, RiskLevel::Safe},
    {"hyprshelld.media.playPause", nullptr, "emptyArguments", ActionKind::HyprShelld, UiTier::Common, RiskLevel::Safe},
    {"hyprshelld.media.previous", nullptr, "emptyArguments", ActionKind::HyprShelld, UiTier::Common, RiskLevel::Safe},
    {"hyprshelld.media.toggleMicMute", nullptr, "emptyArguments", ActionKind::HyprShelld, UiTier::Common, RiskLevel::Safe},
    {"hyprshelld.media.toggleMute", nullptr, "emptyArguments", ActionKind::HyprShelld, UiTier::Common, RiskLevel::Safe},
    {"hyprshelld.media.volumeDown", nullptr, "emptyArguments", ActionKind::HyprShelld, UiTier::Common, RiskLevel::Safe},
    {"hyprshelld.media.volumeUp", nullptr, "emptyArguments", ActionKind::HyprShelld, UiTier::Common, RiskLevel::Safe},
    {"hyprshelld.screenshot.full", nullptr, "emptyArguments", ActionKind::HyprShelld, UiTier::Common, RiskLevel::Safe},
    {"hyprshelld.screenshot.region", nullptr, "emptyArguments", ActionKind::HyprShelld, UiTier::Common, RiskLevel::Safe},
    {"hyprshelld.screenshot.window", nullptr, "emptyArguments", ActionKind::HyprShelld, UiTier::Common, RiskLevel::Safe},
    {"hyprshelld.sessionMenu", nullptr, "emptyArguments", ActionKind::HyprShelld, UiTier::Common, RiskLevel::Safe},
    {"hyprshelld.settings", nullptr, "emptyArguments", ActionKind::HyprShelld, UiTier::Common, RiskLevel::Safe},
    {"hyprshelld.shortcutGuide", nullptr, "emptyArguments", ActionKind::HyprShelld, UiTier::Common, RiskLevel::Safe},
};

constexpr PinnedAction pinnedGestureActions[]{
    {"close", "close", "gestureAction", ActionKind::Gesture, UiTier::Common, RiskLevel::Safe},
    {"cursorZoom", "cursorZoom", "gestureAction", ActionKind::Gesture, UiTier::Common, RiskLevel::Safe},
    {"float", "float", "gestureAction", ActionKind::Gesture, UiTier::Common, RiskLevel::Safe},
    {"fullscreen", "fullscreen", "gestureAction", ActionKind::Gesture, UiTier::Common, RiskLevel::Safe},
    {"move", "move", "gestureAction", ActionKind::Gesture, UiTier::Common, RiskLevel::Safe},
    {"resize", "resize", "gestureAction", ActionKind::Gesture, UiTier::Common, RiskLevel::Safe},
    {"scrollMove", "scroll_move", "gestureAction", ActionKind::Gesture, UiTier::Common, RiskLevel::Safe},
    {"special", "special", "gestureAction", ActionKind::Gesture, UiTier::Common, RiskLevel::Safe},
    {"unset", "unset", "gestureAction", ActionKind::Gesture, UiTier::Common, RiskLevel::Safe},
    {"workspace", "workspace", "gestureAction", ActionKind::Gesture, UiTier::Common, RiskLevel::Safe},
};

struct PinnedExclusion final {
    const char *surface;
    const char *id;
    const char *reason;
};

constexpr PinnedExclusion pinnedExclusions[]{
    {"bindingKey", "switch:*", "Tagged switch selectors are not representable as one stable, portable managed key token in v1."},
    {"bindingOption", "mouse", "The tagged Lua parser documents this option but never assigns it; mouse buttons are represented by key symbols."},
    {"dispatcher", "exec_cmd", "Arbitrary process commands are outside managed desired state."},
    {"dispatcher", "exec_raw", "Raw process commands are outside managed desired state."},
    {"dispatcher", "layout", "layout-dependent-message: tagged layout messages use active-layout and plugin-specific mini-languages outside managed v1."},
    {"dispatcher", "window.set_prop", "untyped-prop-value: tagged property values use a property-specific mini-language that is not part of managed v1."},
    {"gesture", "callback", "Live Lua callbacks cannot be represented as declarative state."},
    {"windowRuleEffect", "group", "contextual-group-command: tagged group effects use an order- and window-context-dependent mini-language outside managed v1."},
    {"workspaceRule", "on_created_empty", "Arbitrary workspace creation commands belong in user-custom.lua."},
};

static_assert(std::size(pinnedDispatcherActions) == 47);
static_assert(std::size(pinnedSemanticActions) == 29);
static_assert(std::size(pinnedGestureActions) == 10);
static_assert(std::size(pinnedExclusions) == 9);

[[nodiscard]] const PinnedAction *findPinnedAction(
    const ActionKind kind,
    const QString &id
)
{
    const auto find = [&id](const auto &values) -> const PinnedAction * {
        const auto match = std::ranges::find_if(values, [&id](const PinnedAction &value) {
            return id == QLatin1String(value.id);
        });
        return match == std::end(values) ? nullptr : &*match;
    };
    if (kind == ActionKind::Dispatcher) return find(pinnedDispatcherActions);
    if (kind == ActionKind::Gesture) return find(pinnedGestureActions);
    return find(pinnedSemanticActions);
}

[[nodiscard]] ActionInvocation expectedInvocation(
    const PinnedAction &action
)
{
    ActionInvocation result;
    if (action.kind == ActionKind::DefaultApp
        || action.kind == ActionKind::HyprShelld) {
        result.kind = InvocationKind::Broker;
        result.brokerNamespace = action.kind == ActionKind::DefaultApp
            ? QStringLiteral("defaultApp") : QStringLiteral("hyprshelld");
        return result;
    }
    if (action.kind == ActionKind::Gesture) {
        result.kind = InvocationKind::GestureTable;
        result.actionField = QStringLiteral("action");
        const auto id = QLatin1String(action.id);
        if (id == QStringLiteral("special")) {
            result.parameters.append({QStringLiteral("workspace"), QStringLiteral("workspace_name")});
        } else if (id == QStringLiteral("float")
                   || id == QStringLiteral("fullscreen")) {
            result.parameters.append({QStringLiteral("mode"), QStringLiteral("mode")});
        } else if (id == QStringLiteral("cursorZoom")) {
            result.parameters.append({QStringLiteral("zoomLevel"), QStringLiteral("zoom_level")});
            result.parameters.append({QStringLiteral("mode"), QStringLiteral("mode")});
        }
        return result;
    }

    static const QMap<QString, QString> scalarFields{
        {QStringLiteral("event"), QStringLiteral("event")},
        {QStringLiteral("force_idle"), QStringLiteral("seconds")},
        {QStringLiteral("global"), QStringLiteral("name")},
        {QStringLiteral("submap"), QStringLiteral("name")},
        {QStringLiteral("workspace.toggle_special"), QStringLiteral("name")},
    };
    const auto id = QString::fromLatin1(action.id);
    if (const auto field = scalarFields.constFind(id);
        field != scalarFields.constEnd()) {
        result.kind = InvocationKind::Scalar;
        result.scalarField = *field;
    } else if (id == QStringLiteral("window.resize")) {
        result.kind = InvocationKind::EmptyObjectNoneOtherwiseTable;
    } else if (QLatin1String(action.schemaName)
               == QStringLiteral("emptyArguments")) {
        result.kind = InvocationKind::None;
    } else {
        result.kind = InvocationKind::Table;
    }
    return result;
}

void validatePinnedAction(
    const ActionDefinition &action,
    const QString &path,
    ValidationErrors &errors
)
{
    const auto *expected = findPinnedAction(action.kind, action.id);
    if (!expected) {
        addError(
            errors,
            path + QStringLiteral(".id"),
            QStringLiteral("action-catalog.unreviewed-action"),
            QStringLiteral("The action is not part of the pinned 0.56.1 authority.")
        );
        return;
    }
    const auto expectedSchema = QStringLiteral("config.schema.json#/$defs/")
        + QLatin1String(expected->schemaName);
    if (action.schemaReference != expectedSchema) {
        addError(errors, path + QStringLiteral(".argumentsSchemaRef"), QStringLiteral("action-catalog.action-contract-mismatch"), QStringLiteral("The action references the wrong reviewed payload contract."));
    }
    const auto expectedLuaPath = expected->luaPath
        ? QString::fromLatin1(expected->luaPath).split(QLatin1Char('.'))
        : QStringList{};
    if (action.luaPath != expectedLuaPath) {
        addError(errors, path + QStringLiteral(".luaPath"), QStringLiteral("action-catalog.action-contract-mismatch"), QStringLiteral("The action Lua invocation path does not match the reviewed source."));
    }
    if (action.kind != expected->kind || action.uiTier != expected->uiTier
        || action.risk != expected->risk) {
        addError(errors, path, QStringLiteral("action-catalog.action-contract-mismatch"), QStringLiteral("The action kind, UI tier, or risk differs from the reviewed authority."));
    }
    if (action.invocation != expectedInvocation(*expected)) {
        addError(errors, path + QStringLiteral(".invocation"), QStringLiteral("action-catalog.action-contract-mismatch"), QStringLiteral("The action marshalling descriptor differs from the reviewed invocation contract."));
    }
}

[[nodiscard]] QStringList parseLuaPath(
    const QJsonObject &object,
    const QString &path,
    ValidationErrors &errors
)
{
    QStringList result;
    const auto array = readArray(object, QStringLiteral("luaPath"), path, errors);
    static const QRegularExpression identifier(
        QStringLiteral("^[A-Za-z_][A-Za-z0-9_]*$")
    );
    if (array.size() < 2 || array.size() > 4) addError(errors, path + QStringLiteral(".luaPath"), QStringLiteral("action-catalog.invalid-lua-path"), QStringLiteral("A two-to-four segment Lua path is required."));
    for (qsizetype index = 0; index < array.size() && index < 4; ++index) {
        if (!array.at(index).isString()
            || array.at(index).toString().size() > 64
            || !identifier.match(array.at(index).toString()).hasMatch()) addError(errors, path + QStringLiteral(".luaPath[") + QString::number(index) + QLatin1Char(']'), QStringLiteral("action-catalog.invalid-lua-path"), QStringLiteral("A Lua identifier of at most 64 characters is required."));
        else result.append(array.at(index).toString());
    }
    return result;
}

[[nodiscard]] ActionInvocation parseInvocation(
    const QJsonObject &action,
    const QString &path,
    const ActionKind actionKind,
    ValidationErrors &errors
)
{
    const auto object = readObject(
        action, QStringLiteral("invocation"), path, errors
    );
    const auto invocationPath = path + QStringLiteral(".invocation");
    ActionInvocation result;
    const auto kind = readString(
        object, QStringLiteral("kind"), invocationPath, 64, errors
    );
    static const QRegularExpression identifier(
        QStringLiteral("^[A-Za-z_][A-Za-z0-9_]*$")
    );

    if (actionKind == ActionKind::Dispatcher) {
        const auto scalar = kind == QStringLiteral("scalar");
        rejectUnknownFields(
            object,
            scalar
                ? QSet<QString>{QStringLiteral("kind"), QStringLiteral("field")}
                : QSet<QString>{QStringLiteral("kind")},
            invocationPath,
            errors
        );
        if (kind == QStringLiteral("none")) {
            result.kind = InvocationKind::None;
        } else if (kind == QStringLiteral("table")) {
            result.kind = InvocationKind::Table;
        } else if (scalar) {
            result.kind = InvocationKind::Scalar;
            result.scalarField = readString(
                object, QStringLiteral("field"), invocationPath, 64, errors
            );
            if (!result.scalarField.isEmpty()
                && !identifier.match(result.scalarField).hasMatch()) {
                addError(errors, invocationPath + QStringLiteral(".field"), QStringLiteral("action-catalog.invalid-invocation"), QStringLiteral("A scalar invocation field must be a Lua identifier."));
            }
        } else if (kind == QStringLiteral("empty-object-none-otherwise-table")) {
            result.kind = InvocationKind::EmptyObjectNoneOtherwiseTable;
        } else if (!kind.isEmpty()) {
            addError(errors, invocationPath + QStringLiteral(".kind"), QStringLiteral("action-catalog.invalid-invocation"), QStringLiteral("The dispatcher invocation kind is unsupported."));
        }
        return result;
    }

    if (actionKind == ActionKind::DefaultApp
        || actionKind == ActionKind::HyprShelld) {
        rejectUnknownFields(
            object,
            {QStringLiteral("kind"), QStringLiteral("namespace")},
            invocationPath,
            errors
        );
        result.kind = InvocationKind::Broker;
        if (kind != QStringLiteral("broker")) {
            addError(errors, invocationPath + QStringLiteral(".kind"), QStringLiteral("action-catalog.invalid-invocation"), QStringLiteral("Semantic actions require broker invocation."));
        }
        result.brokerNamespace = readString(
            object,
            QStringLiteral("namespace"),
            invocationPath,
            32,
            errors
        );
        const auto expectedNamespace = actionKind == ActionKind::DefaultApp
            ? QStringLiteral("defaultApp") : QStringLiteral("hyprshelld");
        if (!result.brokerNamespace.isEmpty()
            && result.brokerNamespace != expectedNamespace) {
            addError(errors, invocationPath + QStringLiteral(".namespace"), QStringLiteral("action-catalog.invalid-invocation"), QStringLiteral("The broker namespace must match actionType."));
        }
        return result;
    }

    rejectUnknownFields(
        object,
        {QStringLiteral("kind"), QStringLiteral("actionField"),
         QStringLiteral("parameters")},
        invocationPath,
        errors
    );
    result.kind = InvocationKind::GestureTable;
    if (kind != QStringLiteral("gesture-table")) {
        addError(errors, invocationPath + QStringLiteral(".kind"), QStringLiteral("action-catalog.invalid-invocation"), QStringLiteral("Gesture actions require a gesture-table invocation."));
    }
    result.actionField = readString(
        object, QStringLiteral("actionField"), invocationPath, 64, errors
    );
    if (result.actionField != QStringLiteral("action")) {
        addError(errors, invocationPath + QStringLiteral(".actionField"), QStringLiteral("action-catalog.invalid-invocation"), QStringLiteral("The reviewed gesture action field is 'action'."));
    }
    const auto parameters = readArray(
        object, QStringLiteral("parameters"), invocationPath, errors
    );
    if (parameters.size() > 2) {
        addError(errors, invocationPath + QStringLiteral(".parameters"), QStringLiteral("action-catalog.collection-limit"), QStringLiteral("Gesture invocation accepts at most two mapped parameters."));
    }
    QSet<QByteArray> seen;
    for (qsizetype index = 0; index < parameters.size() && index < 2; ++index) {
        const auto parameterPath = invocationPath + QStringLiteral(".parameters[")
            + QString::number(index) + QLatin1Char(']');
        if (!parameters.at(index).isObject()) {
            addError(errors, parameterPath, QStringLiteral("action-catalog.object-required"), QStringLiteral("A gesture parameter mapping object is required."));
            continue;
        }
        const auto parameter = parameters.at(index).toObject();
        rejectUnknownFields(
            parameter,
            {QStringLiteral("argument"), QStringLiteral("field")},
            parameterPath,
            errors
        );
        InvocationParameter parsed{
            .argument = readString(
                parameter, QStringLiteral("argument"), parameterPath, 64, errors
            ),
            .field = readString(
                parameter, QStringLiteral("field"), parameterPath, 64, errors
            ),
        };
        if ((!parsed.argument.isEmpty()
             && !identifier.match(parsed.argument).hasMatch())
            || (!parsed.field.isEmpty()
                && !identifier.match(parsed.field).hasMatch())) {
            addError(errors, parameterPath, QStringLiteral("action-catalog.invalid-invocation"), QStringLiteral("Gesture argument and Lua field names must be identifiers."));
        }
        const auto encoded = JsonSupport::canonicalJson(parameter);
        if (seen.contains(encoded)) {
            addError(errors, parameterPath, QStringLiteral("action-catalog.duplicate-invocation-parameter"), QStringLiteral("Gesture parameter mappings must be unique."));
        }
        seen.insert(encoded);
        result.parameters.append(std::move(parsed));
    }
    return result;
}

[[nodiscard]] std::optional<ActionDefinition> parseActionDefinition(
    const QJsonObject &object,
    const QString &path,
    const ActionKind kind,
    ContractCompiler &compiler,
    ValidationErrors &errors
)
{
    const auto dispatcher = kind == ActionKind::Dispatcher;
    const auto gesture = kind == ActionKind::Gesture;
    QSet<QString> fields{
        QStringLiteral("id"), QStringLiteral("label"),
        QStringLiteral("description"), QStringLiteral("uiTier"),
        QStringLiteral("risk"), QStringLiteral("documentation"),
        QStringLiteral("invocation"),
    };
    fields.insert(dispatcher ? QStringLiteral("argumentsSchemaRef")
                             : gesture ? QStringLiteral("actionSchemaRef")
                                       : QStringLiteral("argumentsSchemaRef"));
    if (dispatcher) fields.insert(QStringLiteral("luaPath"));
    if (gesture) fields.insert(QStringLiteral("luaAction"));
    if (!dispatcher && !gesture) fields.insert(QStringLiteral("actionType"));
    rejectUnknownFields(object, fields, path, errors);

    ActionDefinition definition;
    definition.kind = kind;
    definition.id = readString(object, QStringLiteral("id"), path, 128, errors);
    if (!definition.id.isEmpty() && !actionIdValid(definition.id)) addError(errors, path + QStringLiteral(".id"), QStringLiteral("action-catalog.invalid-id"), QStringLiteral("The action ID is invalid."));
    definition.label = readString(object, QStringLiteral("label"), path, 128, errors);
    if (!definition.label.isEmpty() && !isSafePresentationText(definition.label)) addError(errors, path + QStringLiteral(".label"), QStringLiteral("action-catalog.invalid-presentation"), QStringLiteral("Action labels cannot contain control or format characters."));
    definition.description = readString(object, QStringLiteral("description"), path, 512, errors);
    if (!definition.description.isEmpty() && !isSafePresentationText(definition.description)) addError(errors, path + QStringLiteral(".description"), QStringLiteral("action-catalog.invalid-presentation"), QStringLiteral("Action descriptions cannot contain control or format characters."));
    if (dispatcher) definition.luaPath = parseLuaPath(object, path, errors);
    if (gesture) {
        const auto luaAction = readString(object, QStringLiteral("luaAction"), path, 64, errors);
        static const QRegularExpression identifier(QStringLiteral("^[A-Za-z_][A-Za-z0-9_]*$"));
        if (!luaAction.isEmpty() && !identifier.match(luaAction).hasMatch()) addError(errors, path + QStringLiteral(".luaAction"), QStringLiteral("action-catalog.invalid-lua-path"), QStringLiteral("The Lua action token is invalid."));
        definition.luaPath.append(luaAction);
    }
    const auto tierText = readString(object, QStringLiteral("uiTier"), path, 32, errors);
    if (const auto tier = uiTier(tierText)) definition.uiTier = *tier;
    else if (!tierText.isEmpty()) addError(errors, path + QStringLiteral(".uiTier"), QStringLiteral("action-catalog.invalid-ui-tier"), QStringLiteral("The UI tier is unsupported."));
    const auto riskText = readString(object, QStringLiteral("risk"), path, 32, errors);
    if (const auto risk = riskLevel(riskText)) definition.risk = *risk;
    else if (!riskText.isEmpty()) addError(errors, path + QStringLiteral(".risk"), QStringLiteral("action-catalog.invalid-risk"), QStringLiteral("The risk level is unsupported."));
    const auto schemaKey = gesture ? QStringLiteral("actionSchemaRef")
                                   : QStringLiteral("argumentsSchemaRef");
    definition.schemaReference = readString(object, schemaKey, path, 256, errors);
    static const QRegularExpression schemaReference(
        QStringLiteral("^config\\.schema\\.json#/\\$defs/[A-Za-z][A-Za-z0-9]*$")
    );
    if (!definition.schemaReference.isEmpty()
        && !schemaReference.match(definition.schemaReference).hasMatch()) {
        addError(
            errors,
            path + QLatin1Char('.') + schemaKey,
            QStringLiteral("action-catalog.invalid-schema-reference"),
            QStringLiteral("An exact local config schema definition reference is required.")
        );
    }
    definition.payloadContract = compiler.compileReference(
        definition.schemaReference, path + QLatin1Char('.') + schemaKey
    );
    definition.documentation = readString(object, QStringLiteral("documentation"), path, 2048, errors);
    if (!definition.documentation.isEmpty() && !documentationValid(definition.documentation)) addError(errors, path + QStringLiteral(".documentation"), QStringLiteral("action-catalog.invalid-documentation"), QStringLiteral("A pinned official Hyprland wiki URL is required."));
    definition.invocation = parseInvocation(
        object, path, definition.kind, errors
    );
    return definition;
}

[[nodiscard]] bool valueTypeMatches(
    const QJsonValue &value,
    const ContractValueType type
)
{
    switch (type) {
    case ContractValueType::Any: return true;
    case ContractValueType::Object: return value.isObject();
    case ContractValueType::Array: return value.isArray();
    case ContractValueType::String: return value.isString();
    case ContractValueType::Number: return value.isDouble() && std::isfinite(value.toDouble());
    case ContractValueType::Integer: return isInteger(value);
    case ContractValueType::Boolean: return value.isBool();
    case ContractValueType::Null: return value.isNull();
    }
    return false;
}

void validateContract(
    const ValueContract &contract,
    const QJsonValue &value,
    const QString &path,
    ValidationErrors &errors,
    const int depth
)
{
    if (depth > maximumSchemaReferenceDepth) {
        addError(errors, path, QStringLiteral("state.schema-depth"), QStringLiteral("The action payload is too deeply nested."));
        return;
    }
    if (!contract.oneOf.isEmpty()) {
        int matches = 0;
        ValidationErrors closestErrors;
        for (const auto &alternative : contract.oneOf) {
            ValidationErrors candidate;
            validateContract(*alternative, value, path, candidate, depth + 1);
            if (candidate.isEmpty()) ++matches;
            else if (closestErrors.isEmpty()
                || candidate.size() < closestErrors.size()) {
                closestErrors = std::move(candidate);
            }
        }
        if (matches != 1) {
            if (matches == 0) errors.append(closestErrors);
            addError(errors, path, QStringLiteral("state.schema-one-of"), QStringLiteral("The payload must match exactly one supported action shape."));
        }
        return;
    }
    if (!contract.anyOf.isEmpty()) {
        bool matches = false;
        ValidationErrors closestErrors;
        for (const auto &alternative : contract.anyOf) {
            ValidationErrors candidate;
            validateContract(*alternative, value, path, candidate, depth + 1);
            matches = matches || candidate.isEmpty();
            if (!candidate.isEmpty() && (closestErrors.isEmpty()
                || candidate.size() < closestErrors.size())) {
                closestErrors = std::move(candidate);
            }
        }
        if (!matches) {
            errors.append(closestErrors);
            addError(errors, path, QStringLiteral("state.schema-any-of"), QStringLiteral("The payload does not match a supported action value."));
        }
        return;
    }
    const auto typeMatches = std::ranges::any_of(
        contract.acceptedTypes,
        [&value](const ContractValueType type) {
            return valueTypeMatches(value, type);
        }
    );
    if (!typeMatches) {
        QString code = QStringLiteral("state.value-type");
        if (contract.acceptedTypes.contains(ContractValueType::Number)) code = QStringLiteral("state.number-required");
        else if (contract.acceptedTypes.contains(ContractValueType::Integer)) code = QStringLiteral("state.integer-required");
        else if (contract.acceptedTypes.contains(ContractValueType::String)) code = QStringLiteral("state.string-required");
        else if (contract.acceptedTypes.contains(ContractValueType::Object)) code = QStringLiteral("state.object-required");
        else if (contract.acceptedTypes.contains(ContractValueType::Array)) code = QStringLiteral("state.array-required");
        else if (contract.acceptedTypes.contains(ContractValueType::Boolean)) code = QStringLiteral("state.boolean-required");
        addError(errors, path, code, QStringLiteral("The value does not have the required action-contract type."));
        return;
    }
    const auto encoded = JsonSupport::canonicalJson(value);
    if (contract.constantValue
        && encoded != JsonSupport::canonicalJson(*contract.constantValue)) addError(errors, path, QStringLiteral("state.constant-mismatch"), QStringLiteral("The action discriminator has the wrong constant value."));
    if (!contract.allowedValues.isEmpty()
        && !std::ranges::any_of(contract.allowedValues, [&encoded](const QJsonValue &allowed) { return JsonSupport::canonicalJson(allowed) == encoded; })) addError(errors, path, QStringLiteral("state.enum-mismatch"), QStringLiteral("The value is not in the action-contract enum."));

    if (value.isDouble()) {
        const auto number = value.toDouble();
        if ((contract.minimum && number < *contract.minimum)
            || (contract.maximum && number > *contract.maximum)
            || (contract.exclusiveMinimum && number <= *contract.exclusiveMinimum)
            || (contract.exclusiveMaximum && number >= *contract.exclusiveMaximum)) addError(errors, path, QStringLiteral("state.value-out-of-range"), QStringLiteral("The numeric action value is outside its contract range."));
    }
    if (value.isString()) {
        const auto text = value.toString();
        if ((contract.minimumLength && text.size() < *contract.minimumLength)
            || (contract.maximumLength && text.size() > *contract.maximumLength)
            || (contract.pattern && !QRegularExpression(*contract.pattern).match(text).hasMatch())) addError(errors, path, QStringLiteral("state.invalid-string"), QStringLiteral("The string violates its action contract."));
    }
    if (value.isObject()) {
        const auto object = value.toObject();
        if ((contract.minimumProperties && object.size() < *contract.minimumProperties)
            || (contract.maximumProperties && object.size() > *contract.maximumProperties)) addError(errors, path, QStringLiteral("state.collection-limit"), QStringLiteral("The action object has the wrong field count."));
        for (const auto &required : contract.requiredProperties) {
            if (!object.contains(required)) addError(errors, path + QLatin1Char('.') + required, QStringLiteral("state.required-field"), QStringLiteral("The action argument is required."));
        }
        for (auto iterator = object.constBegin(); iterator != object.constEnd(); ++iterator) {
            const auto property = contract.properties.constFind(iterator.key());
            if (property == contract.properties.constEnd()) {
                if (!contract.additionalProperties) addError(errors, path + QLatin1Char('.') + iterator.key(), QStringLiteral("state.unknown-field"), QStringLiteral("The action contract does not accept this field."));
            } else validateContract(**property, iterator.value(), path + QLatin1Char('.') + iterator.key(), errors, depth + 1);
        }
    }
    if (value.isArray()) {
        const auto array = value.toArray();
        if ((contract.minimumItems && array.size() < *contract.minimumItems)
            || (contract.maximumItems && array.size() > *contract.maximumItems)) addError(errors, path, QStringLiteral("state.collection-limit"), QStringLiteral("The action array violates its item bound."));
        QSet<QByteArray> seen;
        for (qsizetype index = 0; index < array.size(); ++index) {
            if (contract.items) validateContract(*contract.items, array.at(index), path + QLatin1Char('[') + QString::number(index) + QLatin1Char(']'), errors, depth + 1);
            if (contract.uniqueItems) {
                const auto item = JsonSupport::canonicalJson(array.at(index));
                if (seen.contains(item)) addError(errors, path + QLatin1Char('[') + QString::number(index) + QLatin1Char(']'), QStringLiteral("state.duplicate-item"), QStringLiteral("Action array items must be unique."));
                seen.insert(item);
            }
        }
    }
}

} // namespace

[[nodiscard]] static ValidationResult<ActionCatalog>
parseActionCatalogContract(
    const QByteArrayView actionCatalogBytes,
    const QByteArrayView configSchemaBytes,
    const ActionCatalogContract &contract
)
{
    ValidationResult<ActionCatalog> result;
    const auto actionParsed = JsonSupport::parseStrictObject(
        actionCatalogBytes, maximumActionCatalogBytes, maximumActionDocumentDepth
    );
    if (!actionParsed) {
        result.errors = actionParsed.errors;
        return result;
    }
    const auto schemaParsed = JsonSupport::parseStrictObject(
        configSchemaBytes, maximumActionSchemaBytes, maximumActionDocumentDepth
    );
    if (!schemaParsed) {
        result.errors = schemaParsed.errors;
        return result;
    }
    const auto &root = *actionParsed.value;
    const auto &schemaRoot = *schemaParsed.value;
    QSet<QString> rootFields{
        QStringLiteral("contractVersion"), QStringLiteral("hyprland"),
        QStringLiteral("configSchemaDigest"), QStringLiteral("source"),
        QStringLiteral("dispatcherActions"), QStringLiteral("semanticActions"),
        QStringLiteral("gestureActions"), QStringLiteral("excluded"),
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
    ActionCatalog catalog;
    const auto contractVersion = root.value(QStringLiteral("contractVersion"));
    if (!isInteger(contractVersion)
        || contractVersion.toDouble() != contract.contractVersion) {
        addError(
            result.errors,
            QStringLiteral("$.contractVersion"),
            QStringLiteral("action-catalog.unsupported-contract-version"),
            QStringLiteral("Only action catalog contract v%1 is supported.")
                .arg(contract.contractVersion)
        );
    } else {
        catalog.contractVersion = contract.contractVersion;
    }

    const auto hyprland = readObject(root, QStringLiteral("hyprland"), QStringLiteral("$"), result.errors);
    QSet<QString> hyprlandFields{
        QStringLiteral("reviewedVersion"),
        QStringLiteral("reviewedTag"),
        QStringLiteral("reviewedCommit"),
    };
    if (contract.requiresPatchRange) {
        hyprlandFields.insert(QStringLiteral("minimumPatch"));
        hyprlandFields.insert(QStringLiteral("maximumPatch"));
    }
    rejectUnknownFields(
        hyprland,
        hyprlandFields,
        QStringLiteral("$.hyprland"),
        result.errors
    );
    const auto reviewed = readString(hyprland, QStringLiteral("reviewedVersion"), QStringLiteral("$.hyprland"), 32, result.errors);
    if (const auto version = semanticVersionFromString(reviewed)) catalog.reviewedVersion = *version;
    else if (!reviewed.isEmpty()) addError(result.errors, QStringLiteral("$.hyprland.reviewedVersion"), QStringLiteral("action-catalog.invalid-version"), QStringLiteral("A strict reviewed version is required."));
    catalog.reviewedTag = readString(hyprland, QStringLiteral("reviewedTag"), QStringLiteral("$.hyprland"), 64, result.errors);
    catalog.reviewedCommit = readString(hyprland, QStringLiteral("reviewedCommit"), QStringLiteral("$.hyprland"), 40, result.errors);
    if (reviewed != QLatin1String(contract.reviewedVersion)) addError(result.errors, QStringLiteral("$.hyprland.reviewedVersion"), QStringLiteral("action-catalog.invalid-version"), QStringLiteral("The authority must use the exact reviewed Hyprland version."));
    if (catalog.reviewedTag != QLatin1String(contract.reviewedTag)) addError(result.errors, QStringLiteral("$.hyprland.reviewedTag"), QStringLiteral("action-catalog.invalid-tag"), QStringLiteral("The authority must use the exact reviewed tag."));
    if (catalog.reviewedCommit != QLatin1String(contract.reviewedCommit)) addError(result.errors, QStringLiteral("$.hyprland.reviewedCommit"), QStringLiteral("action-catalog.invalid-commit"), QStringLiteral("The authority must use the reviewed Hyprland commit."));
    if (contract.requiresPatchRange) {
        if (const auto minimum = readPatch(
                hyprland,
                QStringLiteral("minimumPatch"),
                QStringLiteral("$.hyprland"),
                result.errors
            )) {
            catalog.minimumPatch = *minimum;
            if (*minimum != contract.minimumPatch) {
                addError(result.errors, QStringLiteral("$.hyprland.minimumPatch"), QStringLiteral("action-catalog.invalid-patch-range"), QStringLiteral("The authority has the wrong exact minimum patch."));
            }
        }
        if (const auto maximum = readPatch(
                hyprland,
                QStringLiteral("maximumPatch"),
                QStringLiteral("$.hyprland"),
                result.errors
            )) {
            catalog.maximumPatch = *maximum;
            if (*maximum != contract.maximumPatch) {
                addError(result.errors, QStringLiteral("$.hyprland.maximumPatch"), QStringLiteral("action-catalog.invalid-patch-range"), QStringLiteral("The authority has the wrong exact maximum patch."));
            }
        }
    }

    if (contract.requiresSourceManifestDigest) {
        catalog.sourceManifestDigest = readString(
            root,
            QStringLiteral("sourceManifestDigest"),
            QStringLiteral("$"),
            64,
            result.errors
        );
        if (!catalog.sourceManifestDigest.isEmpty()
            && !isSha256(catalog.sourceManifestDigest)) {
            addError(result.errors, QStringLiteral("$.sourceManifestDigest"), QStringLiteral("action-catalog.invalid-source-manifest-digest"), QStringLiteral("A lowercase SHA-256 source-manifest digest is required."));
        } else if (catalog.sourceManifestDigest
            != QLatin1String(contract.sourceManifestDigest)) {
            addError(result.errors, QStringLiteral("$.sourceManifestDigest"), QStringLiteral("action-catalog.source-manifest-digest-mismatch"), QStringLiteral("The action catalog is not bound to the exact reviewed source manifest."));
        }
    }

    catalog.configSchemaDigest = readString(root, QStringLiteral("configSchemaDigest"), QStringLiteral("$"), 64, result.errors);
    const auto actualSchemaDigest = QString::fromLatin1(QCryptographicHash::hash(configSchemaBytes, QCryptographicHash::Sha256).toHex());
    if (catalog.configSchemaDigest != actualSchemaDigest) addError(result.errors, QStringLiteral("$.configSchemaDigest"), QStringLiteral("action-catalog.schema-digest-mismatch"), QStringLiteral("The action catalog is not bound to these exact config schema bytes."));

    const auto source = readObject(root, QStringLiteral("source"), QStringLiteral("$"), result.errors);
    rejectUnknownFields(source, {QStringLiteral("repository"), QStringLiteral("tag"), QStringLiteral("commit"), QStringLiteral("path"), QStringLiteral("sha256")}, QStringLiteral("$.source"), result.errors);
    catalog.source.repository = readString(source, QStringLiteral("repository"), QStringLiteral("$.source"), 256, result.errors);
    catalog.source.tag = readString(source, QStringLiteral("tag"), QStringLiteral("$.source"), 64, result.errors);
    catalog.source.commit = readString(source, QStringLiteral("commit"), QStringLiteral("$.source"), 40, result.errors);
    catalog.source.path = readString(source, QStringLiteral("path"), QStringLiteral("$.source"), 256, result.errors);
    catalog.source.sha256 = readString(source, QStringLiteral("sha256"), QStringLiteral("$.source"), 64, result.errors);
    if (catalog.source.repository != QStringLiteral("https://github.com/hyprwm/Hyprland") || catalog.source.tag != catalog.reviewedTag || catalog.source.commit != catalog.reviewedCommit || catalog.source.path != QStringLiteral("src/config/lua/bindings/LuaBindingsDispatchers.cpp") || catalog.source.sha256 != QLatin1String(contract.dispatcherSourceSha256)) addError(result.errors, QStringLiteral("$.source"), QStringLiteral("action-catalog.invalid-provenance"), QStringLiteral("The source provenance is incomplete or inconsistent."));

    const auto definitions = schemaRoot.value(QStringLiteral("$defs"));
    if (!definitions.isObject() || definitions.toObject().size() > 1024) {
        addError(result.errors, QStringLiteral("$schema.$defs"), QStringLiteral("action-schema.invalid-definitions"), QStringLiteral("A bounded config schema definitions object is required."));
    }
    ContractCompiler compiler(definitions.toObject(), result.errors);
    QSet<QString> bindingIds;
    const auto parseList = [&](const QString &key, const ActionKind fixedKind, QVector<ActionDefinition> &target, const qsizetype exactCount = -1) {
        const auto array = readArray(root, key, QStringLiteral("$"), result.errors);
        if (array.isEmpty() || array.size() > maximumActions || (exactCount >= 0 && array.size() != exactCount)) addError(result.errors, QStringLiteral("$.") + key, QStringLiteral("action-catalog.collection-limit"), QStringLiteral("The action collection has the wrong bounded size."));
        QString previous;
        for (qsizetype index = 0; index < array.size() && index < maximumActions; ++index) {
            const auto path = QStringLiteral("$.") + key + QLatin1Char('[') + QString::number(index) + QLatin1Char(']');
            if (!array.at(index).isObject()) { addError(result.errors, path, QStringLiteral("action-catalog.object-required"), QStringLiteral("An action object is required.")); continue; }
            auto kind = fixedKind;
            if (fixedKind == ActionKind::DefaultApp) {
                const auto authored = array.at(index).toObject().value(QStringLiteral("actionType"));
                if (authored == QStringLiteral("defaultApp")) kind = ActionKind::DefaultApp;
                else if (authored == QStringLiteral("hyprshelld")) kind = ActionKind::HyprShelld;
                else addError(result.errors, path + QStringLiteral(".actionType"), QStringLiteral("action-catalog.invalid-action-type"), QStringLiteral("The semantic action type is unsupported."));
            }
            auto action = parseActionDefinition(array.at(index).toObject(), path, kind, compiler, result.errors);
            if (!action) continue;
            validatePinnedAction(*action, path, result.errors);
            if (!previous.isEmpty() && action->id <= previous) addError(result.errors, path + QStringLiteral(".id"), QStringLiteral("action-catalog.non-canonical-order"), QStringLiteral("Actions must be strictly sorted by ID."));
            if (fixedKind != ActionKind::Gesture && bindingIds.contains(action->id)) addError(result.errors, path + QStringLiteral(".id"), QStringLiteral("action-catalog.duplicate-id"), QStringLiteral("Binding action IDs must be globally unique."));
            if (fixedKind != ActionKind::Gesture) bindingIds.insert(action->id);
            previous = action->id;
            target.append(std::move(*action));
        }
    };
    parseList(QStringLiteral("dispatcherActions"), ActionKind::Dispatcher, catalog.dispatcherActions, 47);
    parseList(QStringLiteral("semanticActions"), ActionKind::DefaultApp, catalog.semanticActions, 29);
    parseList(QStringLiteral("gestureActions"), ActionKind::Gesture, catalog.gestureActions, 10);

    const auto excluded = readArray(root, QStringLiteral("excluded"), QStringLiteral("$"), result.errors);
    if (excluded.size() != 9) addError(result.errors, QStringLiteral("$.excluded"), QStringLiteral("action-catalog.collection-limit"), QStringLiteral("The closed authority must document exactly nine exclusions."));
    QSet<QString> exclusionKeys;
    for (qsizetype index = 0; index < excluded.size() && index < maximumExcludedActions; ++index) {
        const auto path = QStringLiteral("$.excluded[") + QString::number(index) + QLatin1Char(']');
        if (!excluded.at(index).isObject()) { addError(result.errors, path, QStringLiteral("action-catalog.object-required"), QStringLiteral("An exclusion object is required.")); continue; }
        const auto object = excluded.at(index).toObject();
        rejectUnknownFields(object, {QStringLiteral("id"), QStringLiteral("surface"), QStringLiteral("reason")}, path, result.errors);
        ExcludedAction item;
        item.id = readString(object, QStringLiteral("id"), path, 128, result.errors);
        const auto surface = readString(object, QStringLiteral("surface"), path, 32, result.errors);
        if (surface == QStringLiteral("dispatcher")) item.surface = ExcludedSurface::Dispatcher;
        else if (surface == QStringLiteral("gesture")) item.surface = ExcludedSurface::Gesture;
        else if (surface == QStringLiteral("workspaceRule")) item.surface = ExcludedSurface::WorkspaceRule;
        else if (surface == QStringLiteral("bindingOption")) item.surface = ExcludedSurface::BindingOption;
        else if (surface == QStringLiteral("bindingKey")) item.surface = ExcludedSurface::BindingKey;
        else if (surface == QStringLiteral("windowRuleEffect")) item.surface = ExcludedSurface::WindowRuleEffect;
        else if (!surface.isEmpty()) addError(result.errors, path + QStringLiteral(".surface"), QStringLiteral("action-catalog.invalid-exclusion"), QStringLiteral("The excluded surface is unsupported."));
        item.reason = readString(object, QStringLiteral("reason"), path, 512, result.errors);
        const auto key = surface + QLatin1Char(':') + item.id;
        const auto pinned = std::ranges::find_if(
            pinnedExclusions,
            [&surface, &item](const PinnedExclusion &expected) {
                return surface == QLatin1String(expected.surface)
                    && item.id == QLatin1String(expected.id);
            }
        );
        if (pinned == std::end(pinnedExclusions)
            || item.reason != QLatin1String(pinned->reason)) {
            addError(
                result.errors,
                path,
                QStringLiteral("action-catalog.exclusion-contract-mismatch"),
                QStringLiteral("The exclusion is not part of the exact reviewed safety policy.")
            );
        }
        if (exclusionKeys.contains(key)) addError(result.errors, path, QStringLiteral("action-catalog.duplicate-exclusion"), QStringLiteral("Exclusions must be unique."));
        exclusionKeys.insert(key);
        catalog.excluded.append(std::move(item));
    }
    QSet<QString> pinnedExclusionKeys;
    for (const auto &expected : pinnedExclusions) {
        pinnedExclusionKeys.insert(
            QLatin1String(expected.surface) + QLatin1Char(':')
            + QLatin1String(expected.id)
        );
    }
    if (exclusionKeys != pinnedExclusionKeys) {
        addError(
            result.errors,
            QStringLiteral("$.excluded"),
            QStringLiteral("action-catalog.exclusion-inventory-mismatch"),
            QStringLiteral("The exact reviewed exclusion inventory is required.")
        );
    }

    catalog.canonicalDocument = root;
    catalog.canonicalConfigSchema = schemaRoot;
    catalog.configSchemaDocument = QByteArray(
        configSchemaBytes.data(), configSchemaBytes.size()
    );
    QByteArray digestInput = JsonSupport::canonicalJson(root);
    digestInput.append('\n');
    digestInput.append(configSchemaBytes.data(), configSchemaBytes.size());
    catalog.digest = QString::fromLatin1(QCryptographicHash::hash(digestInput, QCryptographicHash::Sha256).toHex());
    if (catalog.digest != QLatin1String(contract.integrityDigest)) {
        addError(
            result.errors,
            QStringLiteral("$"),
            QStringLiteral("action-catalog.integrity-mismatch"),
            QStringLiteral("The action catalog and exact config schema bytes do not match the compiled reviewed v%1 authority.")
                .arg(contract.contractVersion)
        );
    }
    if (result.errors.isEmpty()) result.value = std::move(catalog);
    return result;
}

ValidationResult<ActionCatalog> parseActionCatalog(
    const QByteArrayView actionCatalogBytes,
    const QByteArrayView configSchemaBytes
)
{
    return parseActionCatalogContract(
        actionCatalogBytes,
        configSchemaBytes,
        activeActionCatalogContract()
    );
}

ValidationResult<ActionCatalog> parseDormantActionCatalogV2(
    const QByteArrayView actionCatalogBytes,
    const QByteArrayView configSchemaBytes
)
{
    return parseActionCatalogContract(
        actionCatalogBytes,
        configSchemaBytes,
        dormantActionCatalogContractV2()
    );
}

QByteArray canonicalActionCatalogJson(const ActionCatalog &catalog)
{
    return JsonSupport::canonicalJson(catalog.canonicalDocument);
}

QString actionCatalogDigest(const ActionCatalog &catalog)
{
    return catalog.digest;
}

const ActionDefinition *findAction(
    const ActionCatalog &catalog,
    const ActionKind kind,
    const QString &id
)
{
    const QVector<ActionDefinition> *actions = nullptr;
    if (kind == ActionKind::Dispatcher) actions = &catalog.dispatcherActions;
    else if (kind == ActionKind::Gesture) actions = &catalog.gestureActions;
    else actions = &catalog.semanticActions;
    const auto found = std::ranges::find_if(*actions, [&id, kind](const ActionDefinition &action) { return action.id == id && action.kind == kind; });
    return found == actions->end() ? nullptr : &*found;
}

ValidationErrors validateActionPayload(
    const ActionDefinition &action,
    const QJsonValue &payload,
    const QString &path
)
{
    ValidationErrors errors;
    if (!action.payloadContract) {
        addError(errors, path, QStringLiteral("state.missing-action-contract"), QStringLiteral("The action has no compiled payload contract."));
        return errors;
    }
    validateContract(*action.payloadContract, payload, path, errors, 0);
    return errors;
}

} // namespace HyprShelld::Hyprland
