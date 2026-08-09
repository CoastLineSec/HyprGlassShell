#pragma once

#include "action_catalog.h"
#include "catalog.h"
#include "validation.h"

#include <QByteArray>
#include <QByteArrayView>
#include <QJsonObject>
#include <QJsonValue>
#include <QMap>
#include <QString>
#include <QStringList>
#include <QVector>
#include <QtTypes>

#include <array>
#include <optional>
#include <variant>

namespace HyprShelld::Hyprland {

inline constexpr quint32 currentDesiredStateFormatVersion = 1;
inline constexpr qsizetype maximumDesiredStateBytes = 4 * 1024 * 1024;
inline constexpr qsizetype maximumOverrides = 353;
inline constexpr qsizetype maximumMonitors = 64;
inline constexpr qsizetype maximumDevices = 256;
inline constexpr qsizetype maximumAnimations = 256;
inline constexpr qsizetype maximumCurves = 256;
inline constexpr qsizetype maximumGestures = 64;
inline constexpr qsizetype maximumWorkspaceRules = 1024;
inline constexpr qsizetype maximumWindowRules = 4096;
inline constexpr qsizetype maximumLayerRules = 4096;
inline constexpr qsizetype maximumBindings = 2048;
inline constexpr qsizetype maximumSubmaps = 256;
inline constexpr qsizetype maximumPermissions = 256;
inline constexpr qsizetype maximumEnvironmentVariables = 512;
inline constexpr qsizetype maximumStateStringLength = 4096;
inline constexpr qsizetype maximumBindingModifiers = 8;
inline constexpr qsizetype maximumBindingDevices = 64;
inline constexpr qsizetype maximumGenericArrayItems = 64;
inline constexpr qsizetype maximumGenericMapEntries = 256;

struct MonitorConfiguration final {
    QString id;
    QString selector;
    bool enabled = true;
    QString mode;
    QString position;
    std::variant<double, QString> scale = 1.0;
    std::array<qint64, 4> reserved{0, 0, 0, 0};
    qint32 transform = 0;
    QString mirror;
    qint32 bitdepth = 8;
    QString colorManagement;
    QString sdrEotf;
    double sdrBrightness = 1.0;
    double sdrSaturation = 1.0;
    qint32 vrr = 0;
    QString icc;
    qint32 supportsWideColor = 0;
    qint32 supportsHdr = 0;
    double sdrMinLuminance = 0.2;
    qint64 sdrMaxLuminance = 80;
    double minLuminance = -1.0;
    qint64 maxLuminance = -1;
    qint64 maxAvgLuminance = -1;

    friend bool operator==(
        const MonitorConfiguration &,
        const MonitorConfiguration &
    ) = default;
};

struct DeviceConfiguration final {
    QString id;
    QString selector;
    QString kind;
    bool enabled = true;
    QJsonObject overrides;

    friend bool operator==(
        const DeviceConfiguration &,
        const DeviceConfiguration &
    ) = default;
};

struct AnimationConfiguration final {
    QString id;
    QString name;
    bool enabled = true;
    double speed = 1.0;
    QString curve;
    QString style;

    friend bool operator==(
        const AnimationConfiguration &,
        const AnimationConfiguration &
    ) = default;
};

struct BezierCurveParameters final {
    std::array<std::array<double, 2>, 2> points{{{0.0, 0.0}, {1.0, 1.0}}};

    friend bool operator==(
        const BezierCurveParameters &,
        const BezierCurveParameters &
    ) = default;
};

struct SpringCurveParameters final {
    double stiffness = 1.0;
    double dampening = 1.0;
    double mass = 1.0;

    friend bool operator==(
        const SpringCurveParameters &,
        const SpringCurveParameters &
    ) = default;
};

using AnimationCurveParameters =
    std::variant<BezierCurveParameters, SpringCurveParameters>;

struct AnimationCurve final {
    QString id;
    QString name;
    AnimationCurveParameters parameters = BezierCurveParameters{};

    friend bool operator==(const AnimationCurve &, const AnimationCurve &)
        = default;
};

struct BindingDeviceFilter final {
    bool inclusive = true;
    QStringList list;

    friend bool operator==(
        const BindingDeviceFilter &,
        const BindingDeviceFilter &
    ) = default;
};

struct BindingOptions final {
    bool repeating = false;
    bool locked = false;
    bool release = false;
    bool nonConsuming = false;
    bool autoConsuming = false;
    bool transparent = false;
    bool ignoreMods = false;
    bool dontInhibit = false;
    bool longPress = false;
    bool submapUniversal = false;
    bool click = false;
    bool drag = false;
    bool allowInputCapture = false;
    std::optional<BindingDeviceFilter> device;

    friend bool operator==(const BindingOptions &, const BindingOptions &)
        = default;
};

enum class BindingActionType {
    Dispatcher,
    DefaultApp,
    HyprShelld,
};

struct GestureAction final {
    QString id;
    QJsonObject payload;

    friend bool operator==(const GestureAction &, const GestureAction &)
        = default;
};

struct GestureConfiguration final {
    QString id;
    quint32 fingers = 3;
    QString direction;
    QStringList modifiers;
    double scale = 1.0;
    bool disableInhibit = false;
    GestureAction action;

    friend bool operator==(
        const GestureConfiguration &,
        const GestureConfiguration &
    ) = default;
};

struct WorkspaceRule final {
    QString id;
    QString selector;
    bool enabled = true;
    QString monitor;
    bool persistent = false;
    bool isDefault = false;
    QString layout;
    QJsonObject overrides;

    friend bool operator==(const WorkspaceRule &, const WorkspaceRule &)
        = default;
};

struct WindowRule final {
    QString id;
    QString name;
    bool enabled = true;
    QJsonObject match;
    QJsonObject effects;

    friend bool operator==(const WindowRule &, const WindowRule &) = default;
};

struct LayerRule final {
    QString id;
    QString name;
    bool enabled = true;
    QJsonObject match;
    QJsonObject effects;

    friend bool operator==(const LayerRule &, const LayerRule &) = default;
};

struct BindingConfiguration final {
    QString id;
    QStringList modifiers;
    QString key;
    BindingActionType actionType = BindingActionType::Dispatcher;
    QString action;
    QJsonObject arguments;
    QString description;
    bool enabled = true;
    QString submap;
    BindingOptions options;
    QString normalizedChord;

    friend bool operator==(
        const BindingConfiguration &,
        const BindingConfiguration &
    ) = default;
};

struct SubmapConfiguration final {
    QString id;
    QString name;
    QString reset;
    bool enabled = true;

    friend bool operator==(
        const SubmapConfiguration &,
        const SubmapConfiguration &
    ) = default;
};

struct PermissionConfiguration final {
    QString id;
    QString binary;
    QString type;
    QString mode;

    friend bool operator==(
        const PermissionConfiguration &,
        const PermissionConfiguration &
    ) = default;
};

enum class EnvironmentScope {
    Hyprland,
    Uwsm,
};

struct EnvironmentConfiguration final {
    QString id;
    QString name;
    QString value;
    EnvironmentScope scope = EnvironmentScope::Hyprland;

    friend bool operator==(
        const EnvironmentConfiguration &,
        const EnvironmentConfiguration &
    ) = default;
};

struct DesiredState final {
    quint32 formatVersion = currentDesiredStateFormatVersion;
    quint64 revision = 0;
    QString targetHyprland;
    QString catalogDigest;
    QString actionCatalogDigest;
    QMap<QString, QJsonValue> overrides;
    QVector<MonitorConfiguration> monitors;
    QVector<DeviceConfiguration> devices;
    QVector<AnimationCurve> curves;
    QVector<AnimationConfiguration> animations;
    QVector<GestureConfiguration> gestures;
    QVector<WorkspaceRule> workspaceRules;
    QVector<WindowRule> windowRules;
    QVector<LayerRule> layerRules;
    QVector<SubmapConfiguration> submaps;
    QVector<BindingConfiguration> bindings;
    QVector<PermissionConfiguration> permissions;
    QVector<EnvironmentConfiguration> environment;
    CompatibilityDecision compatibility = CompatibilityDecision::Exact;
    bool readOnly = false;
    std::optional<QJsonObject> opaqueFutureDocument;

    friend bool operator==(const DesiredState &, const DesiredState &) = default;
};

[[nodiscard]] ValidationResult<QString> normalizeBindingChord(
    const QStringList &modifiers,
    const QString &key
);

[[nodiscard]] ValidationResult<DesiredState> parseDesiredState(
    QByteArrayView bytes,
    const Catalog &catalog,
    const ActionCatalog &actionCatalog
);

[[nodiscard]] DesiredState defaultDesiredState(
    const Catalog &catalog,
    const ActionCatalog &actionCatalog
);

// Compact canonical JSON, terminated by one newline for safe durable-file use.
[[nodiscard]] QByteArray serializeDesiredState(const DesiredState &state);

} // namespace HyprShelld::Hyprland
