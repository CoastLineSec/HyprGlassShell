#include "default_keybindings.h"

#include <QJsonObject>

#include <algorithm>
#include <array>
#include <tuple>
#include <utility>

namespace HyprShelld::Hyprland {
namespace {

[[nodiscard]] BindingConfiguration binding(
    QString id,
    QStringList modifiers,
    QString key,
    QString action,
    QJsonObject arguments,
    QString description,
    BindingOptions options = {}
)
{
    BindingConfiguration result{
        .id = std::move(id),
        .modifiers = std::move(modifiers),
        .key = std::move(key),
        .actionType = BindingActionType::Dispatcher,
        .action = std::move(action),
        .arguments = std::move(arguments),
        .description = std::move(description),
        .enabled = true,
        .submap = {},
        .options = std::move(options),
    };
    const auto chord = normalizeBindingChord(result.modifiers, result.key);
    Q_ASSERT(chord);
    result.normalizedChord = chord.value.value_or(QString{});
    return result;
}

[[nodiscard]] QVector<BindingConfiguration> buildDefaults()
{
    QVector<BindingConfiguration> result;
    result.reserve(shippedDefaultKeybindingCount);
    const QStringList super{QStringLiteral("super")};
    const QStringList superShift{
        QStringLiteral("super"), QStringLiteral("shift")
    };
    const QStringList superCtrl{
        QStringLiteral("super"), QStringLiteral("ctrl")
    };
    const QStringList superShiftCtrl{
        QStringLiteral("super"), QStringLiteral("ctrl"),
        QStringLiteral("shift")
    };

    const auto add = [&result](BindingConfiguration record) {
        result.append(std::move(record));
    };
    add(binding(
        QStringLiteral("hyprshelld.default.session.exit"),
        {QStringLiteral("super"), QStringLiteral("shift")},
        QStringLiteral("e"), QStringLiteral("exit"), {},
        QStringLiteral("Exit Hyprland")
    ));
    add(binding(
        QStringLiteral("hyprshelld.default.window.close"), super,
        QStringLiteral("q"), QStringLiteral("window.kill"), {},
        QStringLiteral("Close the focused window")
    ));
    add(binding(
        QStringLiteral("hyprshelld.default.window.maximize"), super,
        QStringLiteral("f"), QStringLiteral("window.fullscreen"),
        {{QStringLiteral("mode"), QStringLiteral("maximized")},
         {QStringLiteral("action"), QStringLiteral("toggle")}},
        QStringLiteral("Toggle maximized window")
    ));
    add(binding(
        QStringLiteral("hyprshelld.default.window.fullscreen"), superShift,
        QStringLiteral("f"), QStringLiteral("window.fullscreen"),
        {{QStringLiteral("mode"), QStringLiteral("fullscreen")},
         {QStringLiteral("action"), QStringLiteral("toggle")}},
        QStringLiteral("Toggle fullscreen window")
    ));
    add(binding(
        QStringLiteral("hyprshelld.default.window.float"), superShift,
        QStringLiteral("t"), QStringLiteral("window.float"),
        {{QStringLiteral("action"), QStringLiteral("toggle")}},
        QStringLiteral("Toggle floating window")
    ));
    add(binding(
        QStringLiteral("hyprshelld.default.window.group"), super,
        QStringLiteral("w"), QStringLiteral("group.toggle"), {},
        QStringLiteral("Toggle the focused window group")
    ));

    struct Direction final {
        const char *name;
        const char *arrow;
        const char *vim;
    };
    constexpr std::array directions{
        Direction{"left", "Left", "h"},
        Direction{"down", "Down", "j"},
        Direction{"up", "Up", "k"},
        Direction{"right", "Right", "l"},
    };
    for (const auto [variant, useVimKey] : {
             std::pair{QStringLiteral("arrow"), false},
             std::pair{QStringLiteral("vim"), true},
         }) {
        for (const auto &direction : directions) {
            const auto name = QString::fromLatin1(direction.name);
            const auto key = QString::fromLatin1(
                useVimKey ? direction.vim : direction.arrow
            );
            add(binding(
                QStringLiteral("hyprshelld.default.focus.window.%1.%2")
                    .arg(name, variant),
                super, key, QStringLiteral("focus"),
                {{QStringLiteral("direction"), name}},
                QStringLiteral("Focus the window to the %1").arg(name)
            ));
        }
    }
    for (const auto [variant, useVimKey] : {
             std::pair{QStringLiteral("arrow"), false},
             std::pair{QStringLiteral("vim"), true},
         }) {
        for (const auto &direction : directions) {
            const auto name = QString::fromLatin1(direction.name);
            const auto key = QString::fromLatin1(
                useVimKey ? direction.vim : direction.arrow
            );
            add(binding(
                QStringLiteral("hyprshelld.default.move.window.%1.%2")
                    .arg(name, variant),
                superShift, key, QStringLiteral("window.move"),
                {{QStringLiteral("direction"), name}},
                QStringLiteral("Move the focused window %1").arg(name)
            ));
        }
    }

    for (const auto [name, key, variant] : {
             std::tuple{QStringLiteral("left"), QStringLiteral("Left"),
                        QStringLiteral("arrow")},
             std::tuple{QStringLiteral("right"), QStringLiteral("Right"),
                        QStringLiteral("arrow")},
             std::tuple{QStringLiteral("left"), QStringLiteral("h"),
                        QStringLiteral("vim")},
             std::tuple{QStringLiteral("down"), QStringLiteral("j"),
                        QStringLiteral("vim")},
             std::tuple{QStringLiteral("up"), QStringLiteral("k"),
                        QStringLiteral("vim")},
             std::tuple{QStringLiteral("right"), QStringLiteral("l"),
                        QStringLiteral("vim")},
         }) {
        add(binding(
            QStringLiteral("hyprshelld.default.focus.monitor.%1.%2")
                .arg(name, variant),
            superCtrl, key, QStringLiteral("focus"),
            {{QStringLiteral("monitor"), name}},
            QStringLiteral("Focus the monitor to the %1").arg(name)
        ));
    }
    for (const auto [variant, useVimKey] : {
             std::pair{QStringLiteral("arrow"), false},
             std::pair{QStringLiteral("vim"), true},
         }) {
        for (const auto &direction : directions) {
            const auto name = QString::fromLatin1(direction.name);
            const auto key = QString::fromLatin1(
                useVimKey ? direction.vim : direction.arrow
            );
            add(binding(
                QStringLiteral("hyprshelld.default.move.monitor.%1.%2")
                    .arg(name, variant),
                superShiftCtrl, key, QStringLiteral("window.move"),
                {{QStringLiteral("monitor"), name}},
                QStringLiteral("Move the focused window to the %1 monitor")
                    .arg(name)
            ));
        }
    }

    for (int workspace = 1; workspace <= 9; ++workspace) {
        const auto number = QString::number(workspace);
        add(binding(
            QStringLiteral("hyprshelld.default.focus.workspace.%1").arg(number),
            super, number, QStringLiteral("focus"),
            {{QStringLiteral("workspace"), number}},
            QStringLiteral("Focus workspace %1").arg(number)
        ));
    }
    for (int workspace = 1; workspace <= 9; ++workspace) {
        const auto number = QString::number(workspace);
        add(binding(
            QStringLiteral("hyprshelld.default.move.workspace.%1").arg(number),
            superShift, number, QStringLiteral("window.move"),
            {{QStringLiteral("workspace"), number}},
            QStringLiteral("Move the focused window to workspace %1")
                .arg(number)
        ));
    }

    add(binding(
        QStringLiteral("hyprshelld.default.window.maximize-set"), superCtrl,
        QStringLiteral("f"), QStringLiteral("window.fullscreen"),
        {{QStringLiteral("mode"), QStringLiteral("maximized")},
         {QStringLiteral("action"), QStringLiteral("set")}},
        QStringLiteral("Maximize the focused window")
    ));
    add(binding(
        QStringLiteral("hyprshelld.default.window.drag"), super,
        QStringLiteral("mouse:272"), QStringLiteral("window.drag"), {},
        QStringLiteral("Drag the focused window")
    ));
    add(binding(
        QStringLiteral("hyprshelld.default.window.resize-pointer"), super,
        QStringLiteral("mouse:273"), QStringLiteral("window.resize"), {},
        QStringLiteral("Resize the focused window with the pointer")
    ));
    add(binding(
        QStringLiteral("hyprshelld.default.window.resize-code-20"), super,
        QStringLiteral("code:20"), QStringLiteral("window.resize"),
        {{QStringLiteral("x"), -100}, {QStringLiteral("y"), 0},
         {QStringLiteral("relative"), true}},
        QStringLiteral("Shrink the focused window horizontally")
    ));
    add(binding(
        QStringLiteral("hyprshelld.default.window.resize-code-21"), super,
        QStringLiteral("code:21"), QStringLiteral("window.resize"),
        {{QStringLiteral("x"), 100}, {QStringLiteral("y"), 0},
         {QStringLiteral("relative"), true}},
        QStringLiteral("Grow the focused window horizontally")
    ));
    BindingOptions repeating;
    repeating.repeating = true;
    for (const auto [id, modifiers, key, x, y, description] : {
             std::tuple{QStringLiteral("width-decrease"), super,
                        QStringLiteral("minus"), -100, 0,
                        QStringLiteral("Shrink the focused window horizontally")},
             std::tuple{QStringLiteral("width-increase"), super,
                        QStringLiteral("equal"), 100, 0,
                        QStringLiteral("Grow the focused window horizontally")},
             std::tuple{QStringLiteral("height-decrease"), superShift,
                        QStringLiteral("minus"), 0, -100,
                        QStringLiteral("Shrink the focused window vertically")},
             std::tuple{QStringLiteral("height-increase"), superShift,
                        QStringLiteral("equal"), 0, 100,
                        QStringLiteral("Grow the focused window vertically")},
         }) {
        add(binding(
            QStringLiteral("hyprshelld.default.window.resize.%1").arg(id),
            modifiers, key, QStringLiteral("window.resize"),
            {{QStringLiteral("x"), x}, {QStringLiteral("y"), y},
             {QStringLiteral("relative"), true}},
            description, repeating
        ));
    }
    add(binding(
        QStringLiteral("hyprshelld.default.display.dpms-toggle"),
        {QStringLiteral("super"), QStringLiteral("shift")},
        QStringLiteral("p"), QStringLiteral("dpms"),
        {{QStringLiteral("action"), QStringLiteral("toggle")}},
        QStringLiteral("Toggle display power")
    ));

    Q_ASSERT(result.size() == shippedDefaultKeybindingCount);
    return result;
}

} // namespace

const QVector<BindingConfiguration> &shippedDefaultKeybindings()
{
    static const auto defaults = buildDefaults();
    return defaults;
}

const BindingConfiguration *shippedDefaultKeybindingById(const QStringView id)
{
    const auto &defaults = shippedDefaultKeybindings();
    const auto found = std::ranges::find_if(
        defaults,
        [id](const auto &candidate) { return candidate.id == id; }
    );
    return found == defaults.cend() ? nullptr : &*found;
}

const BindingConfiguration *matchedShippedDefaultKeybinding(
    const BindingConfiguration &binding
)
{
    if (const auto *byId = shippedDefaultKeybindingById(binding.id)) {
        return byId;
    }
    const auto &defaults = shippedDefaultKeybindings();
    const auto found = std::ranges::find_if(
        defaults,
        [&binding](const auto &candidate) {
            return candidate.submap == binding.submap
                && candidate.normalizedChord == binding.normalizedChord;
        }
    );
    return found == defaults.cend() ? nullptr : &*found;
}

} // namespace HyprShelld::Hyprland
