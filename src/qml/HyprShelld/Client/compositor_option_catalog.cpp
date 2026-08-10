#include "compositor_option_catalog.h"

#include <QJsonArray>
#include <QJsonValue>
#include <QMetaType>
#include <QSet>

#include <array>
#include <ranges>
#include <utility>

namespace HyprShelld {
namespace {

using Hyprland::ApplyMode;
using Hyprland::ControlKind;
using Hyprland::DefaultPolicy;
using Hyprland::OptionDefinition;
using Hyprland::OptionType;
using Hyprland::RiskLevel;
using Hyprland::UiTier;

struct ExpectedOption final {
    const char *id;
    OptionType type;
    ControlKind control;
    QJsonValue defaultValue;
};

const std::array expectedOptions{
    ExpectedOption{
        "hyprland.general.border_size",
        OptionType::Integer,
        ControlKind::SpinBox,
        1,
    },
    ExpectedOption{
        "hyprland.decoration.rounding",
        OptionType::Integer,
        ControlKind::SpinBox,
        0,
    },
    ExpectedOption{
        "hyprland.decoration.blur.enabled",
        OptionType::Boolean,
        ControlKind::Toggle,
        true,
    },
    ExpectedOption{
        "hyprland.decoration.shadow.enabled",
        OptionType::Boolean,
        ControlKind::Toggle,
        true,
    },
    ExpectedOption{
        "hyprland.animations.enabled",
        OptionType::Boolean,
        ControlKind::Toggle,
        true,
    },
    ExpectedOption{
        "hyprland.general.layout",
        OptionType::Enumeration,
        ControlKind::Select,
        QStringLiteral("dwindle"),
    },
    ExpectedOption{
        "hyprland.general.resize_on_border",
        OptionType::Boolean,
        ControlKind::Toggle,
        false,
    },
    ExpectedOption{
        "hyprland.general.snap.enabled",
        OptionType::Boolean,
        ControlKind::Toggle,
        false,
    },
};

[[nodiscard]] bool hasExactCommonContract(
    const OptionDefinition &option,
    const ExpectedOption &expected
)
{
    return option.id == QLatin1String(expected.id)
        && option.type == expected.type
        && option.control == expected.control
        && option.defaultValue == expected.defaultValue
        && option.defaultPolicy == DefaultPolicy::Hyprland
        && option.writable
        && !option.inheritedDefaultFrom.has_value()
        && option.uiTier == UiTier::Common
        && option.applyMode == ApplyMode::Reload
        && option.risk == RiskLevel::Safe;
}

[[nodiscard]] bool hasExactIntegerRange(const OptionDefinition &option)
{
    return option.constraints.minimum.has_value()
        && option.constraints.maximum.has_value()
        && option.constraints.minimum->toInt(-1) == 0
        && option.constraints.maximum->toInt(-1) == 20
        && !option.constraints.step.has_value()
        && option.constraints.choices.isEmpty()
        && !option.constraints.maximumLength.has_value()
        && !option.constraints.pattern.has_value();
}

[[nodiscard]] bool hasEmptyBooleanConstraints(const OptionDefinition &option)
{
    return !option.constraints.minimum.has_value()
        && !option.constraints.maximum.has_value()
        && !option.constraints.step.has_value()
        && option.constraints.choices.isEmpty()
        && !option.constraints.maximumLength.has_value()
        && !option.constraints.pattern.has_value();
}

[[nodiscard]] bool hasExactLayoutChoices(const OptionDefinition &option)
{
    const QStringList expected{
        QStringLiteral("dwindle"),
        QStringLiteral("master"),
        QStringLiteral("scrolling"),
        QStringLiteral("monocle"),
    };
    QStringList actual;
    for (const auto &choice : option.constraints.choices) {
        if (!choice.isObject()) return false;
        const auto object = choice.toObject();
        if (object.size() != 2
            || object.value(QStringLiteral("label")).toString().isEmpty()) {
            return false;
        }
        actual.append(object.value(QStringLiteral("value")).toString());
    }
    return actual == expected
        && option.constraints.maximumLength == std::optional<quint32>(4096)
        && !option.constraints.minimum.has_value()
        && !option.constraints.maximum.has_value()
        && !option.constraints.pattern.has_value();
}

[[nodiscard]] QVariantMap optionMetadata(const OptionDefinition &option)
{
    QVariantMap result{
        {QStringLiteral("id"), option.id},
        {QStringLiteral("type"), Hyprland::toString(option.type)},
        {QStringLiteral("control"), Hyprland::toString(option.control)},
        {QStringLiteral("defaultValue"), option.defaultValue.toVariant()},
        {QStringLiteral("description"), option.description},
        {QStringLiteral("documentation"), option.documentation},
    };
    if (option.constraints.minimum) {
        result.insert(
            QStringLiteral("min"),
            option.constraints.minimum->toVariant()
        );
    }
    if (option.constraints.maximum) {
        result.insert(
            QStringLiteral("max"),
            option.constraints.maximum->toVariant()
        );
    }
    if (option.constraints.step) {
        result.insert(QStringLiteral("step"), *option.constraints.step);
    }
    if (!option.constraints.choices.isEmpty()) {
        result.insert(
            QStringLiteral("choices"),
            QJsonArray::fromVariantList([&option] {
                QVariantList choices;
                choices.reserve(option.constraints.choices.size());
                for (const auto &choice : option.constraints.choices) {
                    choices.append(choice.toVariant());
                }
                return choices;
            }()).toVariantList()
        );
    }
    return result;
}

} // namespace

std::optional<CompositorOptionCatalog> CompositorOptionCatalog::fromBytes(
    const QByteArrayView bytes,
    const QString &replyDigest,
    const QString &advertisedDigest,
    QString &error
)
{
    error.clear();
    if (bytes.isEmpty() || bytes.size() > Hyprland::maximumCatalogBytes) {
        error = QStringLiteral("The compositor option catalog has an invalid size");
        return std::nullopt;
    }
    if (replyDigest != advertisedDigest) {
        error = QStringLiteral("The compositor option catalog authority changed");
        return std::nullopt;
    }
    auto parsed = Hyprland::parseCatalog(bytes);
    if (!parsed) {
        error = parsed.errors.isEmpty()
            ? QStringLiteral("The compositor option catalog is invalid")
            : parsed.errors.constFirst().message;
        return std::nullopt;
    }
    if (Hyprland::canonicalCatalogJson(*parsed.value) != bytes
        || Hyprland::catalogDigest(*parsed.value) != replyDigest) {
        error = QStringLiteral("The compositor option catalog is not canonical");
        return std::nullopt;
    }

    CompositorOptionCatalog result;
    result.catalog_ = std::move(*parsed.value);
    result.appearanceOptions_.reserve(expectedOptions.size());
    for (const auto &expected : expectedOptions) {
        const auto *option = Hyprland::findOption(
            result.catalog_, QString::fromLatin1(expected.id)
        );
        if (option == nullptr || !hasExactCommonContract(*option, expected)
            || (option->type == OptionType::Integer
                && !hasExactIntegerRange(*option))
            || (option->type == OptionType::Boolean
                && !hasEmptyBooleanConstraints(*option))
            || (option->type == OptionType::Enumeration
                && !hasExactLayoutChoices(*option))) {
            error = QStringLiteral(
                "The compositor appearance option contract is unsupported"
            );
            return std::nullopt;
        }
        result.appearanceOptions_.append(optionMetadata(*option));
    }
    return result;
}

const QString &CompositorOptionCatalog::digest() const
{
    return catalog_.digest;
}

const QVariantList &CompositorOptionCatalog::appearanceOptions() const
{
    return appearanceOptions_;
}

const OptionDefinition *CompositorOptionCatalog::appearanceOption(
    const QString &id
) const
{
    if (!std::ranges::any_of(expectedOptions, [&id](const auto &expected) {
            return id == QLatin1String(expected.id);
        })) {
        return nullptr;
    }
    return Hyprland::findOption(catalog_, id);
}

QStringList CompositorOptionCatalog::appearanceOptionIds() const
{
    QStringList result;
    result.reserve(expectedOptions.size());
    for (const auto &expected : expectedOptions) {
        result.append(QString::fromLatin1(expected.id));
    }
    return result;
}

std::optional<QVariantMap> CompositorOptionCatalog::appearanceValues(
    const QJsonObject &snapshot,
    QString &error
) const
{
    error.clear();
    const auto overridesValue = snapshot.value(QStringLiteral("overrides"));
    if (!overridesValue.isObject()) {
        error = QStringLiteral("The compositor snapshot has invalid overrides");
        return std::nullopt;
    }
    const auto overrides = overridesValue.toObject();
    QVariantMap result;
    for (const auto &expected : expectedOptions) {
        const auto id = QString::fromLatin1(expected.id);
        const auto *option = appearanceOption(id);
        if (option == nullptr) {
            error = QStringLiteral("The compositor appearance catalog is incomplete");
            return std::nullopt;
        }
        const auto value = overrides.contains(id)
            ? overrides.value(id)
            : option->defaultValue;
        if (!Hyprland::validateOptionValue(*option, value).isEmpty()) {
            error = QStringLiteral("The compositor appearance value is invalid");
            return std::nullopt;
        }
        result.insert(id, value.toVariant());
    }
    return result;
}

} // namespace HyprShelld
