#pragma once

#include "hyprland/action_catalog.h"
#include "hyprland/catalog.h"

#include <QByteArray>
#include <QByteArrayView>
#include <QJsonObject>
#include <QString>
#include <QVariantList>
#include <QVariantMap>

#include <optional>

namespace HyprShelld {

class CompositorActionCatalog final {
public:
    [[nodiscard]] static std::optional<CompositorActionCatalog> fromBytes(
        QByteArrayView actionCatalog,
        const QString &replyDigest,
        const QString &advertisedDigest,
        QByteArrayView configSchema,
        const QString &replySchemaDigest,
        QString &error
    );

    [[nodiscard]] const QString &digest() const;
    [[nodiscard]] const QString &configSchemaDigest() const;
    [[nodiscard]] const Hyprland::ActionCatalog &catalog() const;

private:
    Hyprland::ActionCatalog catalog_;
};

class CompositorOptionCatalog final {
public:
    [[nodiscard]] static std::optional<CompositorOptionCatalog> fromBytes(
        QByteArrayView bytes,
        const QString &replyDigest,
        const QString &advertisedDigest,
        QString &error
    );

    [[nodiscard]] const QString &digest() const;
    [[nodiscard]] const Hyprland::Catalog &catalog() const;
    [[nodiscard]] bool allOptionsContractAvailable() const;
    [[nodiscard]] const QString &allOptionsContractError() const;
    [[nodiscard]] const QVariantList &allOptions() const;
    [[nodiscard]] const Hyprland::OptionDefinition *allOption(
        const QString &id
    ) const;
    [[nodiscard]] std::optional<QVariantMap> allValues(
        const QJsonObject &snapshot,
        QString &error
    ) const;
    [[nodiscard]] bool appearanceContractAvailable() const;
    [[nodiscard]] const QString &appearanceContractError() const;
    [[nodiscard]] const QVariantList &appearanceOptions() const;
    [[nodiscard]] const Hyprland::OptionDefinition *appearanceOption(
        const QString &id
    ) const;
    [[nodiscard]] QStringList appearanceOptionIds() const;
    [[nodiscard]] std::optional<QVariantMap> appearanceValues(
        const QJsonObject &snapshot,
        QString &error
    ) const;
    [[nodiscard]] bool inputContractAvailable() const;
    [[nodiscard]] const QString &inputContractError() const;
    [[nodiscard]] const QVariantList &inputOptions() const;
    [[nodiscard]] const Hyprland::OptionDefinition *inputOption(
        const QString &id
    ) const;
    [[nodiscard]] QStringList inputOptionIds() const;
    [[nodiscard]] std::optional<QVariantMap> inputValues(
        const QJsonObject &snapshot,
        QString &error
    ) const;
    [[nodiscard]] bool windowsContractAvailable() const;
    [[nodiscard]] const QString &windowsContractError() const;
    [[nodiscard]] const QVariantList &windowsOptions() const;
    [[nodiscard]] const Hyprland::OptionDefinition *windowsOption(
        const QString &id
    ) const;
    [[nodiscard]] QStringList windowsOptionIds() const;
    [[nodiscard]] std::optional<QVariantMap> windowsValues(
        const QJsonObject &snapshot,
        QString &error
    ) const;
    [[nodiscard]] bool workspacesContractAvailable() const;
    [[nodiscard]] const QString &workspacesContractError() const;
    [[nodiscard]] const QVariantList &workspacesOptions() const;
    [[nodiscard]] const Hyprland::OptionDefinition *workspacesOption(
        const QString &id
    ) const;
    [[nodiscard]] QStringList workspacesOptionIds() const;
    [[nodiscard]] std::optional<QVariantMap> workspacesValues(
        const QJsonObject &snapshot,
        QString &error
    ) const;
    [[nodiscard]] bool advancedContractAvailable() const;
    [[nodiscard]] const QString &advancedContractError() const;
    [[nodiscard]] const QVariantList &advancedOptions() const;
    [[nodiscard]] const Hyprland::OptionDefinition *advancedOption(
        const QString &id
    ) const;
    [[nodiscard]] QStringList advancedOptionIds() const;
    [[nodiscard]] std::optional<QVariantMap> advancedValues(
        const QJsonObject &snapshot,
        QString &error
    ) const;

private:
    struct ContractState final {
        QVariantList options;
        QString error;
        bool available = false;
    };

    Hyprland::Catalog catalog_;
    ContractState all_;
    ContractState appearance_;
    ContractState input_;
    ContractState windows_;
    ContractState workspaces_;
    ContractState advanced_;
};

} // namespace HyprShelld
