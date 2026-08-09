#pragma once

#include "catalog.h"
#include "validation.h"

#include <QByteArray>
#include <QByteArrayView>
#include <QJsonObject>
#include <QJsonValue>
#include <QMap>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QVector>
#include <QtTypes>

#include <memory>
#include <optional>

namespace HyprShelld::Hyprland {

inline constexpr quint32 currentActionCatalogContractVersion = 1;
inline constexpr char reviewedActionCatalogDigest[] =
    "72e063a5476308cefdd2771367ffeed9a8e553b3a2d7141f4e08c3e105a5deb2";
inline constexpr qsizetype maximumActionCatalogBytes = 1024 * 1024;
inline constexpr qsizetype maximumActionSchemaBytes = 2 * 1024 * 1024;
inline constexpr qsizetype maximumActions = 256;
inline constexpr qsizetype maximumExcludedActions = 64;
inline constexpr qsizetype maximumSchemaNodes = 2048;
inline constexpr qsizetype maximumSchemaProperties = 256;
inline constexpr int maximumSchemaReferenceDepth = 32;

enum class ActionKind {
    Dispatcher,
    DefaultApp,
    HyprShelld,
    Gesture,
};

enum class ContractValueType {
    Any,
    Object,
    Array,
    String,
    Number,
    Integer,
    Boolean,
    Null,
};

struct ValueContract final {
    QVector<ContractValueType> acceptedTypes;
    QVector<std::shared_ptr<const ValueContract>> oneOf;
    QVector<std::shared_ptr<const ValueContract>> anyOf;
    QMap<QString, std::shared_ptr<const ValueContract>> properties;
    QSet<QString> requiredProperties;
    bool additionalProperties = true;
    std::shared_ptr<const ValueContract> items;
    std::optional<qsizetype> minimumItems;
    std::optional<qsizetype> maximumItems;
    bool uniqueItems = false;
    std::optional<qsizetype> minimumProperties;
    std::optional<qsizetype> maximumProperties;
    std::optional<qsizetype> minimumLength;
    std::optional<qsizetype> maximumLength;
    std::optional<double> minimum;
    std::optional<double> maximum;
    std::optional<double> exclusiveMinimum;
    std::optional<double> exclusiveMaximum;
    std::optional<QString> pattern;
    QVector<QJsonValue> allowedValues;
    std::optional<QJsonValue> constantValue;
};

enum class InvocationKind {
    None,
    Table,
    Scalar,
    EmptyObjectNoneOtherwiseTable,
    Broker,
    GestureTable,
};

struct InvocationParameter final {
    QString argument;
    QString field;

    friend bool operator==(
        const InvocationParameter &,
        const InvocationParameter &
    ) = default;
};

struct ActionInvocation final {
    InvocationKind kind = InvocationKind::None;
    QString scalarField;
    QString brokerNamespace;
    QString actionField;
    QVector<InvocationParameter> parameters;

    friend bool operator==(
        const ActionInvocation &,
        const ActionInvocation &
    ) = default;
};

struct ActionDefinition final {
    QString id;
    QString label;
    QString description;
    ActionKind kind = ActionKind::Dispatcher;
    QStringList luaPath;
    UiTier uiTier = UiTier::Advanced;
    RiskLevel risk = RiskLevel::Safe;
    QString schemaReference;
    QString documentation;
    ActionInvocation invocation;
    std::shared_ptr<const ValueContract> payloadContract;
};

enum class ExcludedSurface {
    Dispatcher,
    Gesture,
    WorkspaceRule,
    BindingOption,
    BindingKey,
    WindowRuleEffect,
};

struct ExcludedAction final {
    QString id;
    ExcludedSurface surface = ExcludedSurface::Dispatcher;
    QString reason;

    friend bool operator==(const ExcludedAction &, const ExcludedAction &)
        = default;
};

struct ActionCatalogSource final {
    QString repository;
    QString tag;
    QString commit;
    QString path;
    QString sha256;

    friend bool operator==(
        const ActionCatalogSource &,
        const ActionCatalogSource &
    ) = default;
};

struct ActionCatalog final {
    quint32 contractVersion = currentActionCatalogContractVersion;
    SemanticVersion reviewedVersion;
    QString reviewedTag;
    QString reviewedCommit;
    ActionCatalogSource source;
    QVector<ActionDefinition> dispatcherActions;
    QVector<ActionDefinition> semanticActions;
    QVector<ActionDefinition> gestureActions;
    QVector<ExcludedAction> excluded;
    QString digest;
    QString configSchemaDigest;
    QJsonObject canonicalDocument;
    QJsonObject canonicalConfigSchema;
};

[[nodiscard]] ValidationResult<ActionCatalog> parseActionCatalog(
    QByteArrayView actionCatalogBytes,
    QByteArrayView configSchemaBytes
);

[[nodiscard]] QByteArray canonicalActionCatalogJson(
    const ActionCatalog &catalog
);
[[nodiscard]] QString actionCatalogDigest(const ActionCatalog &catalog);

[[nodiscard]] const ActionDefinition *findAction(
    const ActionCatalog &catalog,
    ActionKind kind,
    const QString &id
);

[[nodiscard]] ValidationErrors validateActionPayload(
    const ActionDefinition &action,
    const QJsonValue &payload,
    const QString &path = QStringLiteral("$")
);

} // namespace HyprShelld::Hyprland
