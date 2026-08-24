#include "input_device_projection.h"

#include <QHash>
#include <QVariantMap>

namespace HyprShelld {
namespace {

[[nodiscard]] QString sessionSelector(QString selector)
{
    selector.replace(QLatin1Char(' '), QLatin1Char('-'));
    return selector;
}

[[nodiscard]] bool isObservableKind(const QString &configuredKind)
{
    return configuredKind == QStringLiteral("keyboard")
        || configuredKind == QStringLiteral("pointer")
        || configuredKind == QStringLiteral("touchpad")
        || configuredKind == QStringLiteral("touch")
        || configuredKind == QStringLiteral("tablet");
}

[[nodiscard]] bool kindsMatch(
    const QString &configuredKind,
    const Hyprland::ConnectedInputDeviceKind observedKind
)
{
    using Kind = Hyprland::ConnectedInputDeviceKind;
    if (configuredKind == QStringLiteral("keyboard")) {
        return observedKind == Kind::Keyboard;
    }
    if (configuredKind == QStringLiteral("pointer")
        || configuredKind == QStringLiteral("touchpad")) {
        return observedKind == Kind::Pointer;
    }
    if (configuredKind == QStringLiteral("touch")) {
        return observedKind == Kind::Touch;
    }
    if (configuredKind == QStringLiteral("tablet")) {
        return observedKind == Kind::Tablet;
    }
    return false;
}

[[nodiscard]] QVariantMap savedMetadata(
    const Hyprland::DeviceConfiguration &device
)
{
    return {
        {QStringLiteral("savedDeviceId"), device.id},
        {QStringLiteral("configuredKind"), device.kind},
        {QStringLiteral("configuredEnabled"), device.enabled},
        {QStringLiteral("overrideCount"), device.overrides.size()},
    };
}

void addUnavailableSavedMetadata(QVariantMap &row)
{
    row.insert(QStringLiteral("savedDeviceId"), QVariant{});
    row.insert(QStringLiteral("configuredKind"), QVariant{});
    row.insert(QStringLiteral("configuredEnabled"), QVariant{});
    row.insert(QStringLiteral("overrideCount"), QVariant{});
}

} // namespace

InputDeviceProjection projectInputDevices(
    const std::optional<QVector<Hyprland::DeviceConfiguration>> &savedDevices,
    const std::optional<Hyprland::ConnectedInputDeviceInventory> &inventory
)
{
    InputDeviceProjection projection;

    QHash<QString, const Hyprland::ConnectedInputDevice *> observedBySelector;
    if (inventory) {
        for (const auto &device : inventory->records) {
            observedBySelector.insert(device.sessionSelector, &device);
        }
    }

    QHash<QString, const Hyprland::DeviceConfiguration *> savedBySelector;
    if (savedDevices) {
        for (const auto &device : *savedDevices) {
            savedBySelector.insert(sessionSelector(device.selector), &device);

            QString matchState;
            QVariant observedKind;
            if (!inventory) {
                matchState = QStringLiteral("inventory-unavailable");
            } else if (!isObservableKind(device.kind)) {
                matchState = QStringLiteral("unobservable");
            } else if (const auto iterator = observedBySelector.constFind(
                           sessionSelector(device.selector)
                       );
                       iterator == observedBySelector.constEnd()) {
                matchState = QStringLiteral("not-observed");
            } else {
                observedKind = Hyprland::connectedInputDeviceKindName(
                    (*iterator)->observedKind
                );
                matchState = kindsMatch(device.kind, (*iterator)->observedKind)
                    ? QStringLiteral("observed")
                    : QStringLiteral("kind-mismatch");
            }

            QVariantMap row{
                {QStringLiteral("id"), device.id},
                {QStringLiteral("selector"), device.selector},
                {QStringLiteral("configuredKind"), device.kind},
                {QStringLiteral("configuredEnabled"), device.enabled},
                {QStringLiteral("overrideCount"), device.overrides.size()},
                {QStringLiteral("matchState"), matchState},
                {QStringLiteral("observedKind"), observedKind},
            };
            projection.savedDevices.append(row);
            if (matchState != QStringLiteral("observed")) {
                projection.otherSavedDevices.append(std::move(row));
            }
        }
    }

    if (inventory) {
        for (const auto &device : inventory->records) {
            QVariantMap row{
                {QStringLiteral("sessionSelector"), device.sessionSelector},
                {
                    QStringLiteral("observedKind"),
                    Hyprland::connectedInputDeviceKindName(device.observedKind),
                },
                {
                    QStringLiteral("activeKeymap"),
                    device.activeKeymap
                        ? QVariant::fromValue(*device.activeKeymap)
                        : QVariant{},
                },
            };

            QString savedSettingsState;
            if (!savedDevices) {
                savedSettingsState = QStringLiteral("unavailable");
                addUnavailableSavedMetadata(row);
            } else if (const auto iterator = savedBySelector.constFind(
                           device.sessionSelector
                       );
                       iterator == savedBySelector.constEnd()) {
                savedSettingsState = QStringLiteral("not-saved");
                addUnavailableSavedMetadata(row);
            } else {
                const auto *saved = *iterator;
                savedSettingsState = kindsMatch(
                    saved->kind, device.observedKind
                ) ? QStringLiteral("matched")
                  : QStringLiteral("kind-mismatch");
                const auto metadata = savedMetadata(*saved);
                for (auto metadataIterator = metadata.constBegin();
                     metadataIterator != metadata.constEnd();
                     ++metadataIterator) {
                    row.insert(metadataIterator.key(), metadataIterator.value());
                }
            }
            row.insert(QStringLiteral("savedSettingsState"), savedSettingsState);
            projection.connectedDevices.append(std::move(row));
        }
    }

    return projection;
}

} // namespace HyprShelld
