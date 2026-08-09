#pragma once

#include "validation.h"

#include <QByteArray>
#include <QByteArrayView>
#include <QJsonObject>
#include <QJsonValue>
#include <QString>
#include <QStringList>
#include <QVector>
#include <QtTypes>

#include <optional>

namespace HyprShelld::Hyprland {

inline constexpr quint32 currentCatalogContractVersion = 1;
inline constexpr char reviewedCatalogDigest[] =
    "0232f9b036849e2b9423d5960dd32f22001c79b5b6b6696330f481d1d0c657e0";
inline constexpr qsizetype maximumCatalogBytes = 4 * 1024 * 1024;
inline constexpr qsizetype maximumCatalogOptions = 1024;
inline constexpr qsizetype maximumComplexSurfaces = 12;

struct SemanticVersion final {
    quint32 major = 0;
    quint32 minor = 0;
    quint32 patch = 0;

    friend auto operator<=>(const SemanticVersion &, const SemanticVersion &)
        = default;
};

struct HyprlandReleaseRange final {
    quint32 major = 0;
    quint32 minor = 0;
    SemanticVersion reviewedVersion;
    QString reviewedTag;
    QString reviewedCommit;
    QString repository;
    quint32 minimumPatch = 0;
    std::optional<quint32> maximumPatch;

    friend bool operator==(
        const HyprlandReleaseRange &,
        const HyprlandReleaseRange &
    ) = default;
};

enum class OptionType {
    Boolean,
    Integer,
    Number,
    String,
    Color,
    Gradient,
    Vector2,
    Enumeration,
    CssGap,
    FontWeight,
};

enum class DefaultPolicy {
    Hyprland,
    HyprShelld,
};

enum class UiTier {
    Common,
    Advanced,
    Expert,
    External,
};

enum class ControlKind {
    Toggle,
    SpinBox,
    Slider,
    Text,
    Color,
    Gradient,
    Vector2,
    Select,
};

enum class ApplyMode {
    Reload,
    Restart,
    Session,
};

enum class RiskLevel {
    Safe,
    Caution,
    Dangerous,
};

struct OptionConstraints final {
    std::optional<QJsonValue> minimum;
    std::optional<QJsonValue> maximum;
    std::optional<double> step;
    QVector<QJsonValue> choices;
    std::optional<quint32> maximumLength;
    std::optional<QString> pattern;

    friend bool operator==(
        const OptionConstraints &,
        const OptionConstraints &
    ) = default;
};

struct OptionDefinition final {
    QString id;
    QString path;
    QStringList luaPath;
    QString module;
    OptionType type = OptionType::String;
    DefaultPolicy defaultPolicy = DefaultPolicy::Hyprland;
    bool writable = true;
    QJsonValue defaultValue;
    std::optional<QString> inheritedDefaultFrom;
    UiTier uiTier = UiTier::Advanced;
    ControlKind control = ControlKind::Text;
    OptionConstraints constraints;
    ApplyMode applyMode = ApplyMode::Reload;
    RiskLevel risk = RiskLevel::Safe;
    SemanticVersion since;
    std::optional<SemanticVersion> until;
    QString description;
    QString documentation;

    friend bool operator==(
        const OptionDefinition &,
        const OptionDefinition &
    ) = default;
};

struct ComplexSurfaceDefinition final {
    QString id;
    QString kind;
    QString module;
    QStringList luaPath;
    bool ordered = true;
    QString identityField;
    ApplyMode applyMode = ApplyMode::Reload;
    RiskLevel risk = RiskLevel::Safe;
    QString description;
    QString schemaReference;
    QString documentation;

    friend bool operator==(
        const ComplexSurfaceDefinition &,
        const ComplexSurfaceDefinition &
    ) = default;
};

enum class OlderMinorPolicy {
    Migration,
    Unsupported,
};

enum class NewerMinorPolicy {
    ReadOnly,
    Unsupported,
};

enum class UnknownMajorPolicy {
    Unsupported,
};

struct CompatibilityPolicy final {
    SemanticVersion minimumSupported;
    QStringList fullyQualified;
    OlderMinorPolicy olderMinor = OlderMinorPolicy::Migration;
    NewerMinorPolicy newerMinor = NewerMinorPolicy::ReadOnly;
    UnknownMajorPolicy unknownMajor = UnknownMajorPolicy::Unsupported;

    friend bool operator==(
        const CompatibilityPolicy &,
        const CompatibilityPolicy &
    ) = default;
};

struct Catalog final {
    quint32 contractVersion = currentCatalogContractVersion;
    HyprlandReleaseRange hyprland;
    QVector<OptionDefinition> options;
    QVector<ComplexSurfaceDefinition> complexSurfaces;
    CompatibilityPolicy compatibility;
    QString digest;

    // Retained only to make canonical serialization and digesting independent
    // of input whitespace and object-key order.
    QJsonObject canonicalDocument;
};

enum class CompatibilityDecision {
    Exact,
    SupportedMinor,
    UnsupportedOlder,
    UnsupportedFuture,
    UnsupportedMajor,
};

[[nodiscard]] QString toString(OptionType value);
[[nodiscard]] QString toString(DefaultPolicy value);
[[nodiscard]] QString toString(UiTier value);
[[nodiscard]] QString toString(ControlKind value);
[[nodiscard]] QString toString(ApplyMode value);
[[nodiscard]] QString toString(RiskLevel value);
[[nodiscard]] QString toString(SemanticVersion value);

[[nodiscard]] std::optional<SemanticVersion> semanticVersionFromString(
    const QString &value
);

[[nodiscard]] ValidationResult<Catalog> parseCatalog(QByteArrayView bytes);

[[nodiscard]] QByteArray canonicalCatalogJson(const Catalog &catalog);
[[nodiscard]] QString catalogDigest(const Catalog &catalog);

[[nodiscard]] CompatibilityDecision compatibilityForVersion(
    const Catalog &catalog,
    SemanticVersion version
);

[[nodiscard]] const OptionDefinition *findOption(
    const Catalog &catalog,
    const QString &id
);

[[nodiscard]] ValidationErrors validateOptionValue(
    const OptionDefinition &option,
    const QJsonValue &value,
    const QString &path = QStringLiteral("$")
);

} // namespace HyprShelld::Hyprland
