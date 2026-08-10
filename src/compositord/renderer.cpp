#include "renderer.h"

#include "hyprland/json_support.h"

#include <QCryptographicHash>
#include <QDir>
#include <QJsonArray>
#include <QRegularExpression>

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

namespace HyprShelld::Compositor {
namespace {

const QStringList modulePaths{
    QStringLiteral("modules/00-session.lua"),
    QStringLiteral("modules/10-monitors.lua"),
    QStringLiteral("modules/20-environment.lua"),
    QStringLiteral("modules/30-input.lua"),
    QStringLiteral("modules/31-gestures.lua"),
    QStringLiteral("modules/32-cursor.lua"),
    QStringLiteral("modules/40-general.lua"),
    QStringLiteral("modules/41-layouts.lua"),
    QStringLiteral("modules/42-workspaces.lua"),
    QStringLiteral("modules/43-groups.lua"),
    QStringLiteral("modules/50-decorations.lua"),
    QStringLiteral("modules/51-animations.lua"),
    QStringLiteral("modules/60-rules.lua"),
    QStringLiteral("modules/70-keybinds.lua"),
    QStringLiteral("modules/80-permissions.lua"),
    QStringLiteral("modules/90-advanced.lua"),
};

void addError(
    Hyprland::ValidationErrors &errors,
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

[[nodiscard]] QString sha256(const QByteArrayView bytes)
{
    return QString::fromLatin1(
        QCryptographicHash::hash(bytes, QCryptographicHash::Sha256).toHex()
    );
}

[[nodiscard]] bool safeAbsolutePath(const QString &path)
{
    if (path.isEmpty() || !QDir::isAbsolutePath(path)
        || QDir::cleanPath(path) != path
        || path != path.normalized(QString::NormalizationForm_C)) {
        return false;
    }
    for (const auto codePoint : path.toUcs4()) {
        const auto category = QChar::category(static_cast<char32_t>(codePoint));
        if (category == QChar::Other_Control
            || category == QChar::Other_Format
            || category == QChar::Separator_Line
            || category == QChar::Separator_Paragraph) {
            return false;
        }
    }
    return path.toUtf8().size() <= 4096;
}

[[nodiscard]] QByteArray luaString(const QString &value)
{
    const auto input = value.toUtf8();
    QByteArray result{"\""};
    static constexpr char hex[] = "0123456789abcdef";
    for (const auto byte : input) {
        const auto character = static_cast<unsigned char>(byte);
        if (character == '\\' || character == '"') {
            result.append('\\');
            result.append(static_cast<char>(character));
        } else if (character == '\n') {
            result.append("\\n");
        } else if (character == '\r') {
            result.append("\\r");
        } else if (character == '\t') {
            result.append("\\t");
        } else if (character < 0x20 || character == 0x7f) {
            // A fixed-width hex escape cannot consume following hex digits.
            result.append("\\x");
            result.append(hex[character >> 4]);
            result.append(hex[character & 0x0f]);
        } else {
            result.append(static_cast<char>(character));
        }
    }
    result.append('"');
    return result;
}

[[nodiscard]] bool luaIdentifier(const QString &value)
{
    static const QRegularExpression expression(
        QStringLiteral("^[A-Za-z_][A-Za-z0-9_]*$")
    );
    static const QSet<QString> reserved{
        QStringLiteral("and"), QStringLiteral("break"),
        QStringLiteral("do"), QStringLiteral("else"),
        QStringLiteral("elseif"), QStringLiteral("end"),
        QStringLiteral("false"), QStringLiteral("for"),
        QStringLiteral("function"), QStringLiteral("goto"),
        QStringLiteral("if"), QStringLiteral("in"),
        QStringLiteral("local"), QStringLiteral("nil"),
        QStringLiteral("not"), QStringLiteral("or"),
        QStringLiteral("repeat"), QStringLiteral("return"),
        QStringLiteral("then"), QStringLiteral("true"),
        QStringLiteral("until"), QStringLiteral("while"),
    };
    return expression.match(value).hasMatch() && !reserved.contains(value);
}

[[nodiscard]] QByteArray luaNumber(const double number)
{
    if (number == 0.0) {
        return QByteArrayLiteral("0");
    }
    if (std::floor(number) == number
        && number >= static_cast<double>(std::numeric_limits<qint64>::min())
        && number <= static_cast<double>(std::numeric_limits<qint64>::max())) {
        return QByteArray::number(static_cast<qint64>(number));
    }
    auto encoded = QByteArray::number(number, 'g', 17);
    encoded.replace("e+", "e");
    return encoded;
}

[[nodiscard]] QByteArray luaValue(const QJsonValue &value);

[[nodiscard]] QByteArray luaArray(const QJsonArray &array)
{
    QByteArray result{"{"};
    for (qsizetype index = 0; index < array.size(); ++index) {
        if (index != 0) {
            result.append(", ");
        }
        result.append(luaValue(array.at(index)));
    }
    result.append('}');
    return result;
}

[[nodiscard]] QByteArray luaObject(const QJsonObject &object)
{
    QByteArray result{"{"};
    bool first = true;
    for (auto iterator = object.constBegin(); iterator != object.constEnd();
         ++iterator) {
        if (!first) {
            result.append(", ");
        }
        first = false;
        if (luaIdentifier(iterator.key())) {
            result.append(iterator.key().toUtf8());
        } else {
            result.append('[');
            result.append(luaString(iterator.key()));
            result.append(']');
        }
        result.append(" = ");
        result.append(luaValue(iterator.value()));
    }
    result.append('}');
    return result;
}

[[nodiscard]] QByteArray luaValue(const QJsonValue &value)
{
    if (value.isBool()) {
        return value.toBool() ? QByteArrayLiteral("true")
                              : QByteArrayLiteral("false");
    }
    if (value.isDouble()) {
        return luaNumber(value.toDouble());
    }
    if (value.isString()) {
        return luaString(value.toString());
    }
    if (value.isArray()) {
        return luaArray(value.toArray());
    }
    if (value.isObject()) {
        return luaObject(value.toObject());
    }
    return QByteArrayLiteral("nil");
}

[[nodiscard]] QByteArray managedHeader(const QString &customPath)
{
    return QByteArray(managedWarningLine) + '\n'
        + "-- Put custom configuration in " + customPath.toUtf8()
        + "; it is loaded last.\n\n";
}

void appendStatement(QByteArray &body, const QByteArrayView statement)
{
    body.append(statement);
    if (!statement.endsWith('\n')) {
        body.append('\n');
    }
}

void insertNested(
    QJsonObject &object,
    const QStringList &path,
    const qsizetype index,
    const QJsonValue &value
)
{
    if (index + 1 == path.size()) {
        object.insert(path.at(index), value);
        return;
    }
    auto child = object.value(path.at(index)).toObject();
    insertNested(child, path, index + 1, value);
    object.insert(path.at(index), child);
}

[[nodiscard]] QString moduleForCatalogModule(const QString &module)
{
    if (module == QStringLiteral("input")) {
        return QStringLiteral("modules/30-input.lua");
    }
    if (module == QStringLiteral("gestures")) {
        return QStringLiteral("modules/31-gestures.lua");
    }
    if (module == QStringLiteral("cursor")) {
        return QStringLiteral("modules/32-cursor.lua");
    }
    if (module == QStringLiteral("general")) {
        return QStringLiteral("modules/40-general.lua");
    }
    if (module == QStringLiteral("dwindle")
        || module == QStringLiteral("master")
        || module == QStringLiteral("scrolling")
        || module == QStringLiteral("layout")) {
        return QStringLiteral("modules/41-layouts.lua");
    }
    if (module == QStringLiteral("group")) {
        return QStringLiteral("modules/43-groups.lua");
    }
    if (module == QStringLiteral("decoration")) {
        return QStringLiteral("modules/50-decorations.lua");
    }
    if (module == QStringLiteral("animations")) {
        return QStringLiteral("modules/51-animations.lua");
    }
    if (module == QStringLiteral("binds")) {
        return QStringLiteral("modules/70-keybinds.lua");
    }
    return QStringLiteral("modules/90-advanced.lua");
}

[[nodiscard]] int activationRank(const ActivationRequirement value)
{
    switch (value) {
    case ActivationRequirement::None: return 0;
    case ActivationRequirement::Reload: return 1;
    case ActivationRequirement::Restart: return 2;
    case ActivationRequirement::Session: return 3;
    }
    return 3;
}

void includeRequirement(
    ActivationRequirement &current,
    const Hyprland::ApplyMode applyMode
)
{
    ActivationRequirement candidate = ActivationRequirement::Reload;
    if (applyMode == Hyprland::ApplyMode::Restart) {
        candidate = ActivationRequirement::Restart;
    } else if (applyMode == Hyprland::ApplyMode::Session) {
        candidate = ActivationRequirement::Session;
    }
    if (activationRank(candidate) > activationRank(current)) {
        current = candidate;
    }
}

void includeSurfaceRequirement(
    ActivationRequirement &current,
    const Hyprland::Catalog &catalog,
    const QString &surfaceId
)
{
    const auto found = std::ranges::find_if(
        catalog.complexSurfaces,
        [&surfaceId](const Hyprland::ComplexSurfaceDefinition &surface) {
            return surface.id == surfaceId;
        }
    );
    if (found != catalog.complexSurfaces.end()) {
        includeRequirement(current, found->applyMode);
    }
}

[[nodiscard]] QJsonObject monitorObject(
    const Hyprland::MonitorConfiguration &record
)
{
    // CLuaConfigCssGap consumes named fields only. Desired state stores CSS
    // order (top, right, bottom, left); a positional Lua table would be
    // silently read as four zeroes by the tagged 0.56.x binding.
    const QJsonObject reserved{
        {QStringLiteral("top"), record.reserved.at(0)},
        {QStringLiteral("right"), record.reserved.at(1)},
        {QStringLiteral("bottom"), record.reserved.at(2)},
        {QStringLiteral("left"), record.reserved.at(3)},
    };
    QJsonObject object{
        {QStringLiteral("output"), record.selector},
        {QStringLiteral("disabled"), !record.enabled},
        {QStringLiteral("mode"), record.mode},
        {QStringLiteral("position"), record.position},
        {QStringLiteral("reserved"), reserved},
        {QStringLiteral("transform"), record.transform},
        {QStringLiteral("mirror"), record.mirror},
        {QStringLiteral("bitdepth"), record.bitdepth},
        {QStringLiteral("cm"), record.colorManagement},
        {QStringLiteral("sdr_eotf"), record.sdrEotf},
        {QStringLiteral("sdrbrightness"), record.sdrBrightness},
        {QStringLiteral("sdrsaturation"), record.sdrSaturation},
        {QStringLiteral("vrr"), record.vrr},
        {QStringLiteral("supports_wide_color"), record.supportsWideColor},
        {QStringLiteral("supports_hdr"), record.supportsHdr},
        {QStringLiteral("sdr_min_luminance"), record.sdrMinLuminance},
        {QStringLiteral("sdr_max_luminance"), record.sdrMaxLuminance},
        {QStringLiteral("min_luminance"), record.minLuminance},
        {QStringLiteral("max_luminance"), record.maxLuminance},
        {QStringLiteral("max_avg_luminance"), record.maxAvgLuminance},
    };
    // The tagged Lua monitor setter treats an empty ICC path as an attempt to
    // open a profile and rejects it. Absence is the canonical "no profile"
    // representation; a non-empty user path remains explicit.
    if (!record.icc.isEmpty()) {
        object.insert(QStringLiteral("icc"), record.icc);
    }
    if (const auto *number = std::get_if<double>(&record.scale)) {
        object.insert(QStringLiteral("scale"), *number);
    } else {
        object.insert(
            QStringLiteral("scale"), std::get<QString>(record.scale)
        );
    }
    return object;
}

void normalizeWorkspaceCssGap(QJsonObject &object, const QString &field)
{
    if (!object.contains(field)) return;
    const auto values = object.value(field).toArray();
    // Desired state validates these fields as exact CSS-order arrays. The
    // tagged CLuaConfigCssGap parser reads named fields only; numeric Lua
    // table keys otherwise silently resolve to zero.
    Q_ASSERT(values.size() == 4);
    object.insert(field, QJsonObject{
        {QStringLiteral("top"), values.at(0)},
        {QStringLiteral("right"), values.at(1)},
        {QStringLiteral("bottom"), values.at(2)},
        {QStringLiteral("left"), values.at(3)},
    });
}

[[nodiscard]] QString modifierString(const QStringList &modifiers)
{
    QStringList result;
    result.reserve(modifiers.size());
    for (const auto &modifier : modifiers) {
        result.append(modifier.toUpper());
    }
    return result.join(QStringLiteral(" + "));
}

[[nodiscard]] QString bindingChord(
    const Hyprland::BindingConfiguration &record
)
{
    auto modifiers = modifierString(record.modifiers);
    if (modifiers.isEmpty()) {
        return record.key;
    }
    return modifiers + QStringLiteral(" + ") + record.key;
}

[[nodiscard]] QByteArray dispatcherExpression(
    const Hyprland::BindingConfiguration &record,
    const Hyprland::ActionCatalog &catalog,
    Hyprland::ValidationErrors &errors,
    const QString &path
)
{
    const auto *action = Hyprland::findAction(
        catalog, Hyprland::ActionKind::Dispatcher, record.action
    );
    if (!action) {
        addError(errors, path, QStringLiteral("renderer.unknown-action"),
                 QStringLiteral("The dispatcher action is not in the pinned catalog."));
        return {};
    }
    QByteArray function{"hl"};
    for (const auto &part : action->luaPath) {
        function.append('.');
        function.append(part.toUtf8());
    }
    function.append('(');
    switch (action->invocation.kind) {
    case Hyprland::InvocationKind::None:
        break;
    case Hyprland::InvocationKind::Table:
        function.append(luaObject(record.arguments));
        break;
    case Hyprland::InvocationKind::Scalar:
        function.append(luaValue(
            record.arguments.value(action->invocation.scalarField)
        ));
        break;
    case Hyprland::InvocationKind::EmptyObjectNoneOtherwiseTable:
        if (!record.arguments.isEmpty()) {
            function.append(luaObject(record.arguments));
        }
        break;
    case Hyprland::InvocationKind::Broker:
    case Hyprland::InvocationKind::GestureTable:
        addError(errors, path, QStringLiteral("renderer.invalid-invocation"),
                 QStringLiteral("The action has an invocation incompatible with a dispatcher binding."));
        return {};
    }
    function.append(')');
    return function;
}

[[nodiscard]] QJsonObject bindingOptions(
    const Hyprland::BindingConfiguration &record
)
{
    QJsonObject options{
        {QStringLiteral("description"), record.description},
    };
    const std::array pairs{
        std::pair{QStringLiteral("repeating"), record.options.repeating},
        std::pair{QStringLiteral("locked"), record.options.locked},
        std::pair{QStringLiteral("release"), record.options.release},
        std::pair{QStringLiteral("non_consuming"), record.options.nonConsuming},
        std::pair{QStringLiteral("auto_consuming"), record.options.autoConsuming},
        std::pair{QStringLiteral("transparent"), record.options.transparent},
        std::pair{QStringLiteral("ignore_mods"), record.options.ignoreMods},
        std::pair{QStringLiteral("dont_inhibit"), record.options.dontInhibit},
        std::pair{QStringLiteral("long_press"), record.options.longPress},
        std::pair{QStringLiteral("submap_universal"), record.options.submapUniversal},
        std::pair{QStringLiteral("click"), record.options.click},
        std::pair{QStringLiteral("drag"), record.options.drag},
        std::pair{QStringLiteral("allow_input_capture"), record.options.allowInputCapture},
    };
    for (const auto &[name, enabled] : pairs) {
        if (enabled) {
            options.insert(name, true);
        }
    }
    if (record.options.device) {
        QJsonArray list;
        for (const auto &device : record.options.device->list) {
            list.append(device);
        }
        options.insert(
            QStringLiteral("device"),
            QJsonObject{
                {QStringLiteral("inclusive"), record.options.device->inclusive},
                {QStringLiteral("list"), list},
            }
        );
    }
    return options;
}

[[nodiscard]] QByteArray bindingStatement(
    const Hyprland::BindingConfiguration &record,
    const Hyprland::ActionCatalog &catalog,
    Hyprland::ValidationErrors &errors,
    const QString &path
)
{
    if (!record.enabled) {
        return {};
    }
    if (record.actionType != Hyprland::BindingActionType::Dispatcher) {
        addError(
            errors,
            path + QStringLiteral(".action"),
            QStringLiteral("renderer.broker-unavailable"),
            QStringLiteral("The fixed typed action broker is not available in this slice.")
        );
        return {};
    }
    const auto action = dispatcherExpression(
        record, catalog, errors, path + QStringLiteral(".action")
    );
    if (action.isEmpty()) {
        return {};
    }
    return QByteArrayLiteral("hl.bind(") + luaString(bindingChord(record))
        + QByteArrayLiteral(", ") + action + QByteArrayLiteral(", ")
        + luaObject(bindingOptions(record)) + QByteArrayLiteral(")");
}

[[nodiscard]] QJsonObject marshalledWindowEffects(QJsonObject effects)
{
    const auto fullscreen = effects.value(QStringLiteral("fullscreen_state"));
    if (fullscreen.isObject()) {
        const auto object = fullscreen.toObject();
        auto value = QString::number(object.value(QStringLiteral("internal")).toInt());
        if (object.contains(QStringLiteral("client"))) {
            value += QLatin1Char(' ')
                + QString::number(object.value(QStringLiteral("client")).toInt());
        }
        effects.insert(QStringLiteral("fullscreen_state"), value);
    }
    for (const auto &field : {QStringLiteral("monitor"),
                              QStringLiteral("workspace")}) {
        const auto authored = effects.value(field);
        if (!authored.isObject()) {
            continue;
        }
        const auto object = authored.toObject();
        auto value = object.value(QStringLiteral("target")).toString();
        if (object.value(QStringLiteral("silent")).toBool()) {
            value += QStringLiteral(" silent");
        }
        effects.insert(field, value);
    }
    const auto suppress = effects.value(QStringLiteral("suppress_event"));
    if (suppress.isArray()) {
        QStringList values;
        for (const auto &item : suppress.toArray()) {
            values.append(item.toString());
        }
        effects.insert(QStringLiteral("suppress_event"), values.join(QLatin1Char(' ')));
    }
    const auto opacity = effects.value(QStringLiteral("opacity"));
    if (opacity.isObject()) {
        const auto object = opacity.toObject();
        const auto active = object.value(QStringLiteral("active")).toDouble();
        const auto inactive = object.contains(QStringLiteral("inactive"))
            ? object.value(QStringLiteral("inactive")).toDouble() : active;
        const auto fullscreenValue = object.contains(QStringLiteral("fullscreen"))
            ? object.value(QStringLiteral("fullscreen")).toDouble() : active;
        QByteArray tokens = luaNumber(active);
        if (object.value(QStringLiteral("overrideActive")).toBool()) tokens += " override";
        tokens += ' ' + luaNumber(inactive);
        if (object.value(QStringLiteral("overrideInactive")).toBool()) tokens += " override";
        tokens += ' ' + luaNumber(fullscreenValue);
        if (object.value(QStringLiteral("overrideFullscreen")).toBool()) tokens += " override";
        effects.insert(QStringLiteral("opacity"), QString::fromLatin1(tokens));
    }
    return effects;
}

[[nodiscard]] QJsonObject gestureObject(
    const Hyprland::GestureConfiguration &record,
    const Hyprland::ActionCatalog &catalog,
    Hyprland::ValidationErrors &errors,
    const QString &path
)
{
    QJsonObject result{
        {QStringLiteral("fingers"), static_cast<qint64>(record.fingers)},
        {QStringLiteral("direction"), record.direction},
        {QStringLiteral("mods"), modifierString(record.modifiers).replace(QStringLiteral(" + "), QStringLiteral(" "))},
        {QStringLiteral("scale"), record.scale},
        {QStringLiteral("disable_inhibit"), record.disableInhibit},
    };
    const auto *action = Hyprland::findAction(
        catalog, Hyprland::ActionKind::Gesture, record.action.id
    );
    if (!action || action->luaPath.size() != 1
        || action->invocation.kind != Hyprland::InvocationKind::GestureTable) {
        addError(errors, path + QStringLiteral(".action"),
                 QStringLiteral("renderer.invalid-gesture-action"),
                 QStringLiteral("The gesture action has no pinned Lua marshalling."));
        return result;
    }
    result.insert(action->invocation.actionField, action->luaPath.front());
    for (const auto &parameter : action->invocation.parameters) {
        result.insert(
            parameter.field,
            record.action.payload.value(parameter.argument)
        );
    }
    return result;
}

[[nodiscard]] QByteArray createdAtString(const QDateTime &value)
{
    return value.toUTC().toString(QStringLiteral("yyyy-MM-dd'T'HH:mm:ss.zzz'Z'"))
        .toLatin1();
}

} // namespace

QStringList managedModulePaths()
{
    return modulePaths;
}

QString activationRequirementName(const ActivationRequirement value)
{
    switch (value) {
    case ActivationRequirement::None: return QStringLiteral("none");
    case ActivationRequirement::Reload: return QStringLiteral("reload");
    case ActivationRequirement::Restart: return QStringLiteral("restart");
    case ActivationRequirement::Session: return QStringLiteral("session");
    }
    return QStringLiteral("session");
}

ActivationRequirement activationRequirementForDesiredState(
    const Hyprland::DesiredState &state,
    const Hyprland::Catalog &catalog
)
{
    ActivationRequirement required = ActivationRequirement::Reload;
    for (auto iterator = state.overrides.constBegin();
         iterator != state.overrides.constEnd(); ++iterator) {
        if (const auto *option = Hyprland::findOption(catalog, iterator.key())) {
            includeRequirement(required, option->applyMode);
        }
    }
    const auto include = [&](const bool nonempty, const QString &surface) {
        if (nonempty) includeSurfaceRequirement(required, catalog, surface);
    };
    include(!state.monitors.isEmpty(), QStringLiteral("monitors"));
    include(!state.devices.isEmpty(), QStringLiteral("devices"));
    include(!state.curves.isEmpty(), QStringLiteral("curves"));
    include(!state.animations.isEmpty(), QStringLiteral("animations"));
    include(!state.gestures.isEmpty(), QStringLiteral("gestures"));
    include(!state.workspaceRules.isEmpty(), QStringLiteral("workspaceRules"));
    include(!state.windowRules.isEmpty(), QStringLiteral("windowRules"));
    include(!state.layerRules.isEmpty(), QStringLiteral("layerRules"));
    include(!state.submaps.isEmpty(), QStringLiteral("submaps"));
    include(!state.bindings.isEmpty(), QStringLiteral("bindings"));
    include(!state.permissions.isEmpty(), QStringLiteral("permissions"));
    include(!state.environment.isEmpty(), QStringLiteral("environment"));
    return required;
}

RenderResult renderGeneration(
    const Hyprland::DesiredState &state,
    const Hyprland::Catalog &catalog,
    const Hyprland::ActionCatalog &actionCatalog,
    const QString &generationRoot,
    const QString &userCustomPath,
    const QString &activationNonce,
    const QDateTime &createdAtUtc
)
{
    RenderResult result;
    static const QRegularExpression nonceExpression(
        QStringLiteral("^[0-9a-f]{32,128}$")
    );
    if (!safeAbsolutePath(generationRoot)) {
        addError(result.errors, QStringLiteral("$.generationRoot"),
                 QStringLiteral("renderer.invalid-path"),
                 QStringLiteral("The immutable generation root must be a clean absolute path."));
    }
    if (!safeAbsolutePath(userCustomPath)) {
        addError(result.errors, QStringLiteral("$.userCustomPath"),
                 QStringLiteral("renderer.invalid-path"),
                 QStringLiteral("The user custom path must be a clean absolute path."));
    }
    const auto cleanGenerationPrefix = generationRoot + QLatin1Char('/');
    if (safeAbsolutePath(generationRoot)
        && (userCustomPath == generationRoot
            || userCustomPath.startsWith(cleanGenerationPrefix))) {
        addError(result.errors, QStringLiteral("$.userCustomPath"),
                 QStringLiteral("renderer.custom-inside-generation"),
                 QStringLiteral("The user-owned custom file must remain outside the immutable generation tree."));
    }
    if (!nonceExpression.match(activationNonce).hasMatch()) {
        addError(result.errors, QStringLiteral("$.activationNonce"),
                 QStringLiteral("renderer.invalid-nonce"),
                 QStringLiteral("The activation nonce must be 32 to 128 lowercase hexadecimal characters."));
    }
    if (safeAbsolutePath(generationRoot)
        && QDir(generationRoot).dirName() != activationNonce) {
        addError(result.errors, QStringLiteral("$.generationRoot"),
                 QStringLiteral("renderer.generation-root-mismatch"),
                 QStringLiteral("The immutable generation directory must be keyed by the activation nonce."));
    }
    if (!createdAtUtc.isValid()) {
        addError(result.errors, QStringLiteral("$.createdAt"),
                 QStringLiteral("renderer.invalid-time"),
                 QStringLiteral("A valid UTC creation instant is required."));
    }

    const auto canonicalState = Hyprland::serializeDesiredState(state);
    const auto reparsed = Hyprland::parseDesiredState(
        QByteArrayView(canonicalState), catalog, actionCatalog
    );
    if (!reparsed) {
        result.errors.append(reparsed.errors);
    } else if (*reparsed.value != state) {
        addError(result.errors, QStringLiteral("$"),
                 QStringLiteral("renderer.unvalidated-state"),
                 QStringLiteral("The desired state is not the canonical strict-parser product."));
    }
    if (state.readOnly || state.opaqueFutureDocument) {
        addError(result.errors, QStringLiteral("$"),
                 QStringLiteral("renderer.read-only-state"),
                 QStringLiteral("A compatibility-preserved read-only state cannot be rendered."));
    }
    if (!result.errors.isEmpty()) {
        return result;
    }

    QMap<QString, QByteArray> moduleBodies;
    for (const auto &path : modulePaths) {
        moduleBodies.insert(path, {});
    }
    // Publishing a generation always requires at least a reload, including a
    // first adoption, an all-default state, and a state that only deletes old
    // managed values. None is reserved for an already-converged service view.
    ActivationRequirement required = ActivationRequirement::Reload;

    QMap<QString, QJsonObject> scalarConfig;
    for (auto iterator = state.overrides.constBegin();
         iterator != state.overrides.constEnd(); ++iterator) {
        const auto *option = Hyprland::findOption(catalog, iterator.key());
        if (!option || option->luaPath.isEmpty()) {
            addError(result.errors, QStringLiteral("$.overrides.") + iterator.key(),
                     QStringLiteral("renderer.missing-catalog-path"),
                     QStringLiteral("The scalar override has no pinned Lua output path."));
            continue;
        }
        const auto modulePath = moduleForCatalogModule(option->module);
        auto root = scalarConfig.value(modulePath);
        insertNested(root, option->luaPath, 0, iterator.value());
        scalarConfig.insert(modulePath, root);
        includeRequirement(required, option->applyMode);
    }
    for (auto iterator = scalarConfig.constBegin();
         iterator != scalarConfig.constEnd(); ++iterator) {
        appendStatement(
            moduleBodies[iterator.key()],
            QByteArrayLiteral("hl.config(") + luaObject(iterator.value())
                + QByteArrayLiteral(")")
        );
    }

    auto &monitors = moduleBodies[QStringLiteral("modules/10-monitors.lua")];
    for (const auto &record : state.monitors) {
        appendStatement(monitors, QByteArrayLiteral("hl.monitor(")
            + luaObject(monitorObject(record)) + QByteArrayLiteral(")"));
    }
    if (!state.monitors.isEmpty()) includeSurfaceRequirement(required, catalog, QStringLiteral("monitors"));

    auto &environment = moduleBodies[QStringLiteral("modules/20-environment.lua")];
    for (qsizetype index = 0; index < state.environment.size(); ++index) {
        const auto &record = state.environment.at(index);
        if (record.scope == Hyprland::EnvironmentScope::Uwsm) {
            addError(result.errors,
                     QStringLiteral("$.environment[%1].scope").arg(index),
                     QStringLiteral("renderer.uwsm-unavailable"),
                     QStringLiteral("UWSM environment integration is deferred and cannot be silently emitted as hl.env."));
            continue;
        }
        appendStatement(environment, QByteArrayLiteral("hl.env(")
            + luaString(record.name) + QByteArrayLiteral(", ")
            + luaString(record.value) + QByteArrayLiteral(")"));
    }
    if (!state.environment.isEmpty()) includeSurfaceRequirement(required, catalog, QStringLiteral("environment"));

    auto &input = moduleBodies[QStringLiteral("modules/30-input.lua")];
    for (const auto &record : state.devices) {
        auto object = record.overrides;
        object.insert(QStringLiteral("name"), record.selector);
        object.insert(QStringLiteral("enabled"), record.enabled);
        appendStatement(input, QByteArrayLiteral("hl.device(")
            + luaObject(object) + QByteArrayLiteral(")"));
    }
    if (!state.devices.isEmpty()) includeSurfaceRequirement(required, catalog, QStringLiteral("devices"));

    auto &gestures = moduleBodies[QStringLiteral("modules/31-gestures.lua")];
    for (qsizetype index = 0; index < state.gestures.size(); ++index) {
        const auto object = gestureObject(
            state.gestures.at(index), actionCatalog, result.errors,
            QStringLiteral("$.gestures[%1]").arg(index)
        );
        appendStatement(gestures, QByteArrayLiteral("hl.gesture(")
            + luaObject(object) + QByteArrayLiteral(")"));
    }
    if (!state.gestures.isEmpty()) includeSurfaceRequirement(required, catalog, QStringLiteral("gestures"));

    auto &workspaces = moduleBodies[QStringLiteral("modules/42-workspaces.lua")];
    for (const auto &record : state.workspaceRules) {
        auto object = record.overrides;
        normalizeWorkspaceCssGap(object, QStringLiteral("gaps_in"));
        normalizeWorkspaceCssGap(object, QStringLiteral("gaps_out"));
        normalizeWorkspaceCssGap(object, QStringLiteral("float_gaps"));
        object.insert(QStringLiteral("workspace"), record.selector);
        object.insert(QStringLiteral("enabled"), record.enabled);
        object.insert(QStringLiteral("monitor"), record.monitor);
        object.insert(QStringLiteral("persistent"), record.persistent);
        object.insert(QStringLiteral("default"), record.isDefault);
        object.insert(QStringLiteral("layout"), record.layout);
        appendStatement(workspaces, QByteArrayLiteral("hl.workspace_rule(")
            + luaObject(object) + QByteArrayLiteral(")"));
    }
    if (!state.workspaceRules.isEmpty()) includeSurfaceRequirement(required, catalog, QStringLiteral("workspaceRules"));

    auto &animations = moduleBodies[QStringLiteral("modules/51-animations.lua")];
    QMap<QString, bool> springCurves;
    for (const auto &record : state.curves) {
        QJsonObject object;
        if (const auto *bezier = std::get_if<Hyprland::BezierCurveParameters>(&record.parameters)) {
            object.insert(QStringLiteral("type"), QStringLiteral("bezier"));
            QJsonArray points;
            for (const auto &point : bezier->points) {
                points.append(QJsonArray{point.at(0), point.at(1)});
            }
            object.insert(QStringLiteral("points"), points);
            springCurves.insert(record.name, false);
        } else {
            const auto &spring = std::get<Hyprland::SpringCurveParameters>(record.parameters);
            object.insert(QStringLiteral("type"), QStringLiteral("spring"));
            object.insert(QStringLiteral("stiffness"), spring.stiffness);
            object.insert(QStringLiteral("dampening"), spring.dampening);
            object.insert(QStringLiteral("mass"), spring.mass);
            springCurves.insert(record.name, true);
        }
        appendStatement(animations, QByteArrayLiteral("hl.curve(")
            + luaString(record.name) + QByteArrayLiteral(", ")
            + luaObject(object) + QByteArrayLiteral(")"));
    }
    for (const auto &record : state.animations) {
        QJsonObject object{
            {QStringLiteral("leaf"), record.name},
            {QStringLiteral("enabled"), record.enabled},
            {QStringLiteral("speed"), record.speed},
        };
        object.insert(
            springCurves.value(record.curve, false)
                ? QStringLiteral("spring") : QStringLiteral("bezier"),
            record.curve
        );
        if (!record.style.isEmpty()) object.insert(QStringLiteral("style"), record.style);
        appendStatement(animations, QByteArrayLiteral("hl.animation(")
            + luaObject(object) + QByteArrayLiteral(")"));
    }
    if (!state.curves.isEmpty()) includeSurfaceRequirement(required, catalog, QStringLiteral("curves"));
    if (!state.animations.isEmpty()) includeSurfaceRequirement(required, catalog, QStringLiteral("animations"));

    auto &rules = moduleBodies[QStringLiteral("modules/60-rules.lua")];
    for (const auto &record : state.windowRules) {
        auto object = marshalledWindowEffects(record.effects);
        object.insert(QStringLiteral("name"), record.name);
        object.insert(QStringLiteral("enabled"), record.enabled);
        object.insert(QStringLiteral("match"), record.match);
        appendStatement(rules, QByteArrayLiteral("hl.window_rule(")
            + luaObject(object) + QByteArrayLiteral(")"));
    }
    for (const auto &record : state.layerRules) {
        auto object = record.effects;
        object.insert(QStringLiteral("name"), record.name);
        object.insert(QStringLiteral("enabled"), record.enabled);
        object.insert(QStringLiteral("match"), record.match);
        appendStatement(rules, QByteArrayLiteral("hl.layer_rule(")
            + luaObject(object) + QByteArrayLiteral(")"));
    }
    if (!state.windowRules.isEmpty()) includeSurfaceRequirement(required, catalog, QStringLiteral("windowRules"));
    if (!state.layerRules.isEmpty()) includeSurfaceRequirement(required, catalog, QStringLiteral("layerRules"));

    auto &keybinds = moduleBodies[QStringLiteral("modules/70-keybinds.lua")];
    QMap<QString, QByteArray> submapBindings;
    QByteArray topLevelBindings;
    for (qsizetype index = 0; index < state.bindings.size(); ++index) {
        const auto &record = state.bindings.at(index);
        const auto statement = bindingStatement(
            record, actionCatalog, result.errors,
            QStringLiteral("$.bindings[%1]").arg(index)
        );
        if (statement.isEmpty()) continue;
        if (record.submap.isEmpty()) appendStatement(topLevelBindings, statement);
        else appendStatement(submapBindings[record.submap], statement);
    }
    for (const auto &submap : state.submaps) {
        if (!submap.enabled) continue;
        QByteArray statement = QByteArrayLiteral("hl.define_submap(")
            + luaString(submap.name) + QByteArrayLiteral(", ");
        if (!submap.reset.isEmpty()) {
            statement += luaString(submap.reset) + QByteArrayLiteral(", ");
        }
        statement += QByteArrayLiteral("function()\n");
        statement += submapBindings.value(submap.name);
        statement += QByteArrayLiteral("end)");
        appendStatement(keybinds, statement);
    }
    // The tagged Lua API executes a submap definition callback immediately.
    // Declare every enabled submap first, then emit the default-map binds.
    keybinds.append(topLevelBindings);
    if (!state.submaps.isEmpty()) includeSurfaceRequirement(required, catalog, QStringLiteral("submaps"));
    if (std::ranges::any_of(state.bindings, [](const auto &record) { return record.enabled; })) {
        includeSurfaceRequirement(required, catalog, QStringLiteral("bindings"));
    }

    auto &permissions = moduleBodies[QStringLiteral("modules/80-permissions.lua")];
    for (const auto &record : state.permissions) {
        appendStatement(permissions, QByteArrayLiteral("hl.permission(")
            + luaString(record.binary) + QByteArrayLiteral(", ")
            + luaString(record.type) + QByteArrayLiteral(", ")
            + luaString(record.mode) + QByteArrayLiteral(")"));
    }
    if (!state.permissions.isEmpty()) includeSurfaceRequirement(required, catalog, QStringLiteral("permissions"));

    if (!result.errors.isEmpty()) {
        return result;
    }

    RenderedGeneration rendered;
    rendered.snapshotDigest = sha256(canonicalState);
    rendered.activationNonce = activationNonce;
    rendered.createdAt = QString::fromLatin1(createdAtString(createdAtUtc));
    rendered.activationRequirement = required;
    const auto header = managedHeader(userCustomPath);
    for (auto iterator = moduleBodies.constBegin();
         iterator != moduleBodies.constEnd(); ++iterator) {
        GeneratedFile file{
            .path = iterator.key(),
            .contents = header + iterator.value(),
        };
        file.size = static_cast<quint64>(file.contents.size());
        file.sha256 = sha256(file.contents);
        rendered.files.insert(file.path, std::move(file));
    }

    QByteArray loader = header;
    loader += "local function hyprshelld_is_verifier()\n";
    loader += "    local cmdline = io.open(\"/proc/self/cmdline\", \"rb\")\n";
    loader += "    if not cmdline then return true end\n";
    loader += "    local arguments = cmdline:read(\"*a\")\n";
    loader += "    local closed = cmdline:close()\n";
    loader += "    if not arguments or not closed then return true end\n";
    loader += "    if #arguments == 0 or string.byte(arguments, -1) ~= 0 then return true end\n";
    loader += "    local parsed = 0\n";
    loader += "    for argument in string.gmatch(arguments, \"([^%z]+)%z\") do\n";
    loader += "        parsed = parsed + 1\n";
    loader += "        if argument == \"--verify-config\" then return true end\n";
    loader += "    end\n";
    loader += "    return parsed == 0\n";
    loader += "end\n\n";
    loader += "local function hyprshelld_read_bounded(path)\n";
    loader += "    local file = io.open(path, \"rb\")\n";
    loader += "    if not file then return nil end\n";
    loader += "    local bytes = file:read(4097)\n";
    loader += "    local closed = file:close()\n";
    loader += "    if not bytes or not closed or #bytes > 4096 then return nil end\n";
    loader += "    return bytes\n";
    loader += "end\n\n";
    loader += "local function hyprshelld_runtime_ready()\n";
    loader += "    local runtime = os.getenv(\"XDG_RUNTIME_DIR\")\n";
    loader += "    local signature = os.getenv(\"HYPRLAND_INSTANCE_SIGNATURE\")\n";
    loader += "    if not runtime or not signature or #runtime == 0 or #runtime > 4096 then return false end\n";
    loader += "    if string.sub(runtime, 1, 1) ~= \"/\" or string.find(runtime, \"//\", 1, true) then return false end\n";
    loader += "    if #runtime > 1 and string.sub(runtime, -1) == \"/\" then return false end\n";
    loader += "    for part in string.gmatch(runtime, \"[^/]+\") do\n";
    loader += "        if part == \".\" or part == \"..\" then return false end\n";
    loader += "    end\n";
    loader += "    if #signature == 0 or #signature > 192 or signature == \".\" or signature == \"..\" then return false end\n";
    loader += "    if string.find(signature, \"[^A-Za-z0-9_.-]\") then return false end\n";
    loader += "    local stat = hyprshelld_read_bounded(\"/proc/self/stat\")\n";
    loader += "    if not stat then return false end\n";
    loader += "    local pid = string.match(stat, \"^(%d+) %(\")\n";
    loader += "    if not pid or string.sub(pid, 1, 1) == \"0\" then return false end\n";
    loader += "    local lock = hyprshelld_read_bounded(runtime .. \"/hypr/\" .. signature .. \"/hyprland.lock\")\n";
    loader += "    if not lock then return false end\n";
    loader += "    local lock_pid = string.match(lock, \"^(%d+)\\n\")\n";
    loader += "    return lock_pid == pid\n";
    loader += "end\n\n";
    loader += "hl.on(\"config.reloaded\", function()\n";
    loader += "    if hyprshelld_is_verifier() or not hyprshelld_runtime_ready() then return end\n";
    loader += "    hl.dispatch(hl.dsp.event(";
    loader += luaString(QStringLiteral("hyprshelld:") + activationNonce);
    loader += "))\nend)\n\n";
    for (const auto &path : modulePaths) {
        loader += "require(";
        loader += luaString(QDir(generationRoot).filePath(path));
        loader += ")\n";
    }
    loader += "require(";
    loader += luaString(userCustomPath);
    loader += ")\n";
    GeneratedFile entrypoint{
        .path = rendered.entrypoint,
        .contents = loader,
    };
    entrypoint.size = static_cast<quint64>(entrypoint.contents.size());
    entrypoint.sha256 = sha256(entrypoint.contents);
    rendered.files.insert(entrypoint.path, std::move(entrypoint));

    QJsonObject files;
    for (auto iterator = rendered.files.constBegin();
         iterator != rendered.files.constEnd(); ++iterator) {
        files.insert(iterator.key(), QJsonObject{
            {QStringLiteral("sha256"), iterator->sha256},
            {QStringLiteral("size"), static_cast<qint64>(iterator->size)},
        });
    }
    QJsonObject compatible{
        {QStringLiteral("major"), static_cast<qint64>(catalog.hyprland.major)},
        {QStringLiteral("minor"), static_cast<qint64>(catalog.hyprland.minor)},
        {QStringLiteral("reviewedVersion"), Hyprland::toString(catalog.hyprland.reviewedVersion)},
        {QStringLiteral("minimumPatch"), static_cast<qint64>(catalog.hyprland.minimumPatch)},
    };
    if (catalog.hyprland.maximumPatch) {
        compatible.insert(QStringLiteral("maximumPatch"),
                          static_cast<qint64>(*catalog.hyprland.maximumPatch));
    } else {
        compatible.insert(QStringLiteral("maximumPatch"), QJsonValue::Null);
    }
    QJsonObject manifestWithoutGeneration{
        {QStringLiteral("formatVersion"), 1},
        {QStringLiteral("contractVersion"), 1},
        {QStringLiteral("snapshotDigest"), rendered.snapshotDigest},
        {QStringLiteral("catalogDigest"), state.catalogDigest},
        {QStringLiteral("actionCatalogDigest"), state.actionCatalogDigest},
        {QStringLiteral("revision"), QString::number(state.revision)},
        {QStringLiteral("targetHyprland"), state.targetHyprland},
        {QStringLiteral("compatibleHyprland"), compatible},
        {QStringLiteral("rendererVersion"), static_cast<qint64>(currentRendererVersion)},
        {QStringLiteral("activationNonce"), activationNonce},
        {QStringLiteral("createdAt"), rendered.createdAt},
        {QStringLiteral("entrypoint"), rendered.entrypoint},
        {QStringLiteral("files"), files},
    };
    rendered.generation = sha256(
        Hyprland::JsonSupport::canonicalJson(manifestWithoutGeneration)
    );
    rendered.manifest = manifestWithoutGeneration;
    rendered.manifest.insert(QStringLiteral("generation"), rendered.generation);
    rendered.manifestBytes = Hyprland::JsonSupport::canonicalJson(rendered.manifest);
    rendered.manifestBytes.append('\n');
    result.value = std::move(rendered);
    return result;
}

} // namespace HyprShelld::Compositor
