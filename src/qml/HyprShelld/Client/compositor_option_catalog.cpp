#include "compositor_option_catalog.h"

#include <QCryptographicHash>
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
    QJsonObject constraints;
    UiTier uiTier = UiTier::Common;
    Hyprland::SemanticVersion since{0, 55, 0};
    RiskLevel risk = RiskLevel::Safe;
};

[[nodiscard]] QJsonObject numericRange(
    const double minimum,
    const double maximum
)
{
    return {
        {QStringLiteral("min"), minimum},
        {QStringLiteral("max"), maximum},
    };
}

[[nodiscard]] QJsonObject vectorRange(
    QJsonArray minimum,
    QJsonArray maximum
)
{
    return {
        {QStringLiteral("min"), std::move(minimum)},
        {QStringLiteral("max"), std::move(maximum)},
    };
}

[[nodiscard]] QJsonObject choice(
    const char *label,
    QJsonValue value
)
{
    return {
        {QStringLiteral("label"), QString::fromLatin1(label)},
        {QStringLiteral("value"), std::move(value)},
    };
}

[[nodiscard]] QJsonObject numericChoices(
    QJsonArray choices,
    const double minimum,
    const double maximum
)
{
    return {
        {QStringLiteral("min"), minimum},
        {QStringLiteral("max"), maximum},
        {QStringLiteral("choices"), std::move(choices)},
    };
}

[[nodiscard]] QJsonObject stringChoices(QJsonArray choices)
{
    return {
        {QStringLiteral("choices"), std::move(choices)},
        {QStringLiteral("maxLength"), 4096},
    };
}

[[nodiscard]] QJsonObject boundedString()
{
    return {{QStringLiteral("maxLength"), 4096}};
}

const std::array appearanceExpectedOptions{
    ExpectedOption{
        "hyprland.general.border_size",
        OptionType::Integer,
        ControlKind::SpinBox,
        1,
        numericRange(0, 20),
    },
    ExpectedOption{
        "hyprland.decoration.rounding",
        OptionType::Integer,
        ControlKind::SpinBox,
        0,
        numericRange(0, 20),
    },
    ExpectedOption{
        "hyprland.general.gaps_in",
        OptionType::CssGap,
        ControlKind::Text,
        QJsonArray{5, 5, 5, 5},
        {},
    },
    ExpectedOption{
        "hyprland.general.gaps_out",
        OptionType::CssGap,
        ControlKind::Text,
        QJsonArray{20, 20, 20, 20},
        {},
    },
    ExpectedOption{
        "hyprland.decoration.blur.enabled",
        OptionType::Boolean,
        ControlKind::Toggle,
        true,
        {},
    },
    ExpectedOption{
        "hyprland.decoration.shadow.enabled",
        OptionType::Boolean,
        ControlKind::Toggle,
        true,
        {},
    },
    ExpectedOption{
        "hyprland.animations.enabled",
        OptionType::Boolean,
        ControlKind::Toggle,
        true,
        {},
    },
    ExpectedOption{
        "hyprland.decoration.dim_inactive",
        OptionType::Boolean,
        ControlKind::Toggle,
        false,
        {},
    },
    ExpectedOption{
        "hyprland.decoration.dim_strength",
        OptionType::Number,
        ControlKind::Slider,
        0.5,
        numericRange(0, 1),
    },
    ExpectedOption{
        "hyprland.decoration.active_opacity",
        OptionType::Number,
        ControlKind::Slider,
        1,
        numericRange(0, 1),
    },
    ExpectedOption{
        "hyprland.decoration.inactive_opacity",
        OptionType::Number,
        ControlKind::Slider,
        1,
        numericRange(0, 1),
    },
    ExpectedOption{
        "hyprland.decoration.fullscreen_opacity",
        OptionType::Number,
        ControlKind::Slider,
        1,
        numericRange(0, 1),
    },
    ExpectedOption{
        "hyprland.decoration.dim_modal",
        OptionType::Boolean,
        ControlKind::Toggle,
        true,
        {},
    },
    ExpectedOption{
        "hyprland.decoration.dim_special",
        OptionType::Number,
        ControlKind::Slider,
        0.2,
        numericRange(0, 1),
    },
    ExpectedOption{
        "hyprland.decoration.dim_around",
        OptionType::Number,
        ControlKind::Slider,
        0.4,
        numericRange(0, 1),
    },
    ExpectedOption{
        "hyprland.decoration.blur.size",
        OptionType::Integer,
        ControlKind::SpinBox,
        8,
        numericRange(0, 100),
    },
    ExpectedOption{
        "hyprland.decoration.blur.passes",
        OptionType::Integer,
        ControlKind::SpinBox,
        1,
        numericRange(0, 10),
    },
    ExpectedOption{
        "hyprland.decoration.blur.ignore_opacity",
        OptionType::Boolean,
        ControlKind::Toggle,
        true,
        {},
    },
    ExpectedOption{
        "hyprland.decoration.blur.new_optimizations",
        OptionType::Boolean,
        ControlKind::Toggle,
        true,
        {},
    },
    ExpectedOption{
        "hyprland.decoration.blur.xray",
        OptionType::Boolean,
        ControlKind::Toggle,
        false,
        {},
    },
    ExpectedOption{
        "hyprland.decoration.blur.special",
        OptionType::Boolean,
        ControlKind::Toggle,
        false,
        {},
    },
    ExpectedOption{
        "hyprland.decoration.blur.popups",
        OptionType::Boolean,
        ControlKind::Toggle,
        false,
        {},
    },
    ExpectedOption{
        "hyprland.decoration.blur.popups_ignorealpha",
        OptionType::Number,
        ControlKind::Slider,
        0.2,
        numericRange(0, 1),
    },
    ExpectedOption{
        "hyprland.decoration.blur.input_methods",
        OptionType::Boolean,
        ControlKind::Toggle,
        false,
        {},
    },
    ExpectedOption{
        "hyprland.decoration.blur.input_methods_ignorealpha",
        OptionType::Number,
        ControlKind::Slider,
        0.2,
        numericRange(0, 1),
    },
    ExpectedOption{
        "hyprland.decoration.blur.brightness",
        OptionType::Number,
        ControlKind::Slider,
        1,
        numericRange(0, 2),
    },
    ExpectedOption{
        "hyprland.decoration.blur.contrast",
        OptionType::Number,
        ControlKind::Slider,
        0.8916,
        numericRange(0, 2),
    },
    ExpectedOption{
        "hyprland.decoration.blur.noise",
        OptionType::Number,
        ControlKind::Slider,
        0.0117,
        numericRange(0, 1),
    },
    ExpectedOption{
        "hyprland.decoration.blur.vibrancy",
        OptionType::Number,
        ControlKind::Slider,
        0.1696,
        numericRange(0, 1),
    },
    ExpectedOption{
        "hyprland.decoration.blur.vibrancy_darkness",
        OptionType::Number,
        ControlKind::Slider,
        0,
        numericRange(0, 1),
    },
    ExpectedOption{
        "hyprland.decoration.border_part_of_window",
        OptionType::Boolean,
        ControlKind::Toggle,
        true,
        {},
    },
    ExpectedOption{
        "hyprland.decoration.rounding_power",
        OptionType::Number,
        ControlKind::Slider,
        2,
        numericRange(2, 10),
    },
    ExpectedOption{
        "hyprland.decoration.shadow.range",
        OptionType::Integer,
        ControlKind::SpinBox,
        4,
        numericRange(0, 100),
    },
    ExpectedOption{
        "hyprland.decoration.shadow.render_power",
        OptionType::Integer,
        ControlKind::SpinBox,
        3,
        numericRange(1, 4),
    },
    ExpectedOption{
        "hyprland.decoration.shadow.sharp",
        OptionType::Boolean,
        ControlKind::Toggle,
        false,
        {},
    },
    ExpectedOption{
        "hyprland.decoration.shadow.offset",
        OptionType::Vector2,
        ControlKind::Vector2,
        QJsonArray{0, 0},
        vectorRange(QJsonArray{-250, -250}, QJsonArray{250, 250}),
    },
    ExpectedOption{
        "hyprland.decoration.shadow.scale",
        OptionType::Number,
        ControlKind::Slider,
        1,
        numericRange(0, 1),
    },
    ExpectedOption{
        "hyprland.decoration.glow.enabled",
        OptionType::Boolean,
        ControlKind::Toggle,
        false,
        {},
    },
    ExpectedOption{
        "hyprland.decoration.glow.range",
        OptionType::Integer,
        ControlKind::SpinBox,
        10,
        numericRange(0, 100),
    },
    ExpectedOption{
        "hyprland.decoration.glow.render_power",
        OptionType::Integer,
        ControlKind::SpinBox,
        3,
        numericRange(1, 4),
    },
};

const std::array inputExpectedOptions{
    ExpectedOption{
        "hyprland.input.repeat_rate",
        OptionType::Integer,
        ControlKind::SpinBox,
        25,
        numericRange(0, 200),
    },
    ExpectedOption{
        "hyprland.input.repeat_delay",
        OptionType::Integer,
        ControlKind::SpinBox,
        600,
        numericRange(0, 2000),
    },
    ExpectedOption{
        "hyprland.input.sensitivity",
        OptionType::Number,
        ControlKind::Slider,
        0,
        numericRange(-1, 1),
    },
    ExpectedOption{
        "hyprland.input.accel_profile",
        OptionType::Enumeration,
        ControlKind::Select,
        QStringLiteral(""),
        stringChoices(QJsonArray{
            choice("automatic", QJsonValue(QStringLiteral(""))),
            choice("adaptive", QJsonValue(QStringLiteral("adaptive"))),
            choice("flat", QJsonValue(QStringLiteral("flat"))),
        }),
    },
    ExpectedOption{
        "hyprland.input.natural_scroll",
        OptionType::Boolean,
        ControlKind::Toggle,
        false,
        {},
    },
    ExpectedOption{
        "hyprland.input.left_handed",
        OptionType::Boolean,
        ControlKind::Toggle,
        false,
        {},
    },
    ExpectedOption{
        "hyprland.input.scroll_factor",
        OptionType::Number,
        ControlKind::Slider,
        1,
        numericRange(0, 2),
    },
    ExpectedOption{
        "hyprland.input.touchpad.tap-to-click",
        OptionType::Boolean,
        ControlKind::Toggle,
        true,
        {},
    },
    ExpectedOption{
        "hyprland.input.touchpad.tap-and-drag",
        OptionType::Boolean,
        ControlKind::Toggle,
        true,
        {},
    },
    ExpectedOption{
        "hyprland.input.touchpad.natural_scroll",
        OptionType::Boolean,
        ControlKind::Toggle,
        false,
        {},
    },
    ExpectedOption{
        "hyprland.input.touchpad.disable_while_typing",
        OptionType::Boolean,
        ControlKind::Toggle,
        true,
        {},
    },
    ExpectedOption{
        "hyprland.input.touchpad.scroll_factor",
        OptionType::Number,
        ControlKind::Slider,
        1,
        numericRange(0, 2),
    },
    ExpectedOption{
        "hyprland.input.scroll_method",
        OptionType::Enumeration,
        ControlKind::Select,
        QStringLiteral(""),
        stringChoices(QJsonArray{
            choice("automatic", QJsonValue(QStringLiteral(""))),
            choice("two-finger", QJsonValue(QStringLiteral("2fg"))),
            choice("edge", QJsonValue(QStringLiteral("edge"))),
            choice("button", QJsonValue(QStringLiteral("on_button_down"))),
            choice("disabled", QJsonValue(QStringLiteral("no_scroll"))),
        }),
    },
    ExpectedOption{
        "hyprland.input.scroll_button",
        OptionType::Integer,
        ControlKind::SpinBox,
        0,
        numericRange(0, 300),
    },
    ExpectedOption{
        "hyprland.input.scroll_button_lock",
        OptionType::Boolean,
        ControlKind::Toggle,
        false,
        {},
    },
    ExpectedOption{
        "hyprland.input.off_window_axis_events",
        OptionType::Enumeration,
        ControlKind::Select,
        1,
        numericChoices(
            QJsonArray{
                choice("ignore", QJsonValue(0)),
                choice("send", QJsonValue(1)),
                choice("clamp", QJsonValue(2)),
                choice("warp", QJsonValue(3)),
            },
            0,
            3
        ),
    },
    ExpectedOption{
        "hyprland.input.emulate_discrete_scroll",
        OptionType::Enumeration,
        ControlKind::Select,
        1,
        numericChoices(
            QJsonArray{
                choice("disable", QJsonValue(0)),
                choice("non_standard", QJsonValue(1)),
                choice("force_all", QJsonValue(2)),
            },
            0,
            2
        ),
    },
    ExpectedOption{
        "hyprland.input.touchpad.clickfinger_behavior",
        OptionType::Boolean,
        ControlKind::Toggle,
        false,
        {},
    },
    ExpectedOption{
        "hyprland.input.touchpad.drag_3fg",
        OptionType::Enumeration,
        ControlKind::Select,
        0,
        numericChoices(
            QJsonArray{
                choice("disable", QJsonValue(0)),
                choice("3_finger", QJsonValue(1)),
                choice("4_finger", QJsonValue(2)),
            },
            0,
            2
        ),
    },
    ExpectedOption{
        "hyprland.input.touchpad.drag_lock",
        OptionType::Enumeration,
        ControlKind::Select,
        0,
        numericChoices(
            QJsonArray{
                choice("disabled", QJsonValue(0)),
                choice("enabled with timeout", QJsonValue(1)),
                choice("sticky", QJsonValue(2)),
            },
            0,
            2
        ),
    },
    ExpectedOption{
        "hyprland.input.touchpad.flip_x",
        OptionType::Boolean,
        ControlKind::Toggle,
        false,
        {},
    },
    ExpectedOption{
        "hyprland.input.touchpad.flip_y",
        OptionType::Boolean,
        ControlKind::Toggle,
        false,
        {},
    },
    ExpectedOption{
        "hyprland.input.touchpad.middle_button_emulation",
        OptionType::Boolean,
        ControlKind::Toggle,
        false,
        {},
    },
    ExpectedOption{
        "hyprland.input.touchpad.tap_button_map",
        OptionType::Enumeration,
        ControlKind::Select,
        QStringLiteral(""),
        stringChoices(QJsonArray{
            choice("automatic", QJsonValue(QStringLiteral(""))),
            choice("left-right-middle", QJsonValue(QStringLiteral("lrm"))),
            choice("left-middle-right", QJsonValue(QStringLiteral("lmr"))),
        }),
    },
    ExpectedOption{
        "hyprland.input.numlock_by_default",
        OptionType::Boolean,
        ControlKind::Toggle,
        false,
        {},
        UiTier::Advanced,
    },
    ExpectedOption{
        "hyprland.input.virtualkeyboard.share_states",
        OptionType::Enumeration,
        ControlKind::Select,
        2,
        numericChoices(
            QJsonArray{
                choice("disable", QJsonValue(0)),
                choice("enable", QJsonValue(1)),
                choice("only_non_ime", QJsonValue(2)),
            },
            0,
            2
        ),
    },
    ExpectedOption{
        "hyprland.input.virtualkeyboard.release_pressed_on_close",
        OptionType::Boolean,
        ControlKind::Toggle,
        false,
        {},
    },
    ExpectedOption{
        "hyprland.misc.name_vk_after_proc",
        OptionType::Boolean,
        ControlKind::Toggle,
        true,
        {},
        UiTier::Advanced,
    },
    ExpectedOption{
        "hyprland.input.force_no_accel",
        OptionType::Boolean,
        ControlKind::Toggle,
        false,
        {},
    },
    ExpectedOption{
        "hyprland.input.rotation",
        OptionType::Integer,
        ControlKind::SpinBox,
        0,
        numericRange(0, 359),
    },
    ExpectedOption{
        "hyprland.misc.middle_click_paste",
        OptionType::Boolean,
        ControlKind::Toggle,
        true,
        {},
        UiTier::Advanced,
    },
    ExpectedOption{
        "hyprland.gestures.close_max_timeout",
        OptionType::Integer,
        ControlKind::SpinBox,
        1000,
        numericRange(10, 2000),
    },
    ExpectedOption{
        "hyprland.input.touchdevice.enabled",
        OptionType::Boolean,
        ControlKind::Toggle,
        true,
        {},
        UiTier::Advanced,
    },
    ExpectedOption{
        "hyprland.input.touchdevice.transform",
        OptionType::Integer,
        ControlKind::SpinBox,
        0,
        numericRange(0, 6),
        UiTier::Advanced,
    },
    ExpectedOption{
        "hyprland.input.tablet.relative_input",
        OptionType::Boolean,
        ControlKind::Toggle,
        false,
        {},
        UiTier::Advanced,
    },
    ExpectedOption{
        "hyprland.input.tablet.left_handed",
        OptionType::Boolean,
        ControlKind::Toggle,
        false,
        {},
        UiTier::Advanced,
    },
    ExpectedOption{
        "hyprland.input.tablet.transform",
        OptionType::Integer,
        ControlKind::SpinBox,
        0,
        numericRange(0, 6),
        UiTier::Advanced,
    },
    ExpectedOption{
        "hyprland.cursor.hide_on_key_press",
        OptionType::Boolean,
        ControlKind::Toggle,
        false,
        {},
        UiTier::Advanced,
    },
    ExpectedOption{
        "hyprland.cursor.hide_on_touch",
        OptionType::Boolean,
        ControlKind::Toggle,
        true,
        {},
        UiTier::Advanced,
    },
    ExpectedOption{
        "hyprland.cursor.hide_on_tablet",
        OptionType::Boolean,
        ControlKind::Toggle,
        false,
        {},
        UiTier::Advanced,
    },
    ExpectedOption{
        "hyprland.cursor.inactive_timeout",
        OptionType::Number,
        ControlKind::Slider,
        0,
        numericRange(0, 20),
        UiTier::Advanced,
    },
    ExpectedOption{
        "hyprland.cursor.hotspot_padding",
        OptionType::Integer,
        ControlKind::SpinBox,
        0,
        numericRange(0, 20),
        UiTier::Advanced,
    },
    ExpectedOption{
        "hyprland.cursor.no_warps",
        OptionType::Boolean,
        ControlKind::Toggle,
        false,
        {},
        UiTier::Advanced,
    },
    ExpectedOption{
        "hyprland.cursor.persistent_warps",
        OptionType::Boolean,
        ControlKind::Toggle,
        false,
        {},
        UiTier::Advanced,
    },
    ExpectedOption{
        "hyprland.cursor.warp_back_after_non_mouse_input",
        OptionType::Boolean,
        ControlKind::Toggle,
        false,
        {},
        UiTier::Advanced,
    },
    ExpectedOption{
        "hyprland.input.tablet.region_position",
        OptionType::Vector2,
        ControlKind::Vector2,
        QJsonArray{0, 0},
        vectorRange(
            QJsonArray{-20000, -20000},
            QJsonArray{20000, 20000}
        ),
        UiTier::Advanced,
    },
    ExpectedOption{
        "hyprland.input.tablet.absolute_region_position",
        OptionType::Boolean,
        ControlKind::Toggle,
        false,
        {},
        UiTier::Advanced,
    },
    ExpectedOption{
        "hyprland.input.tablet.region_size",
        OptionType::Vector2,
        ControlKind::Vector2,
        QJsonArray{0, 0},
        vectorRange(QJsonArray{-100, -100}, QJsonArray{4000, 4000}),
        UiTier::Advanced,
    },
    ExpectedOption{
        "hyprland.input.resolve_binds_by_sym",
        OptionType::Boolean,
        ControlKind::Toggle,
        false,
        {},
    },
};

const std::array windowsExpectedOptions{
    ExpectedOption{
        "hyprland.general.layout",
        OptionType::Enumeration,
        ControlKind::Select,
        QStringLiteral("dwindle"),
        stringChoices(QJsonArray{
            choice("dwindle", QJsonValue(QStringLiteral("dwindle"))),
            choice("master", QJsonValue(QStringLiteral("master"))),
            choice("scrolling", QJsonValue(QStringLiteral("scrolling"))),
            choice("monocle", QJsonValue(QStringLiteral("monocle"))),
        }),
    },
    ExpectedOption{
        "hyprland.general.resize_on_border",
        OptionType::Boolean,
        ControlKind::Toggle,
        false,
        {},
    },
    ExpectedOption{
        "hyprland.general.extend_border_grab_area",
        OptionType::Integer,
        ControlKind::SpinBox,
        15,
        numericRange(0, 100),
    },
    ExpectedOption{
        "hyprland.general.hover_icon_on_border",
        OptionType::Boolean,
        ControlKind::Toggle,
        true,
        {},
    },
    ExpectedOption{
        "hyprland.general.resize_corner",
        OptionType::Enumeration,
        ControlKind::Select,
        0,
        numericChoices(
            QJsonArray{
                choice("disable", QJsonValue(0)),
                choice("top_left", QJsonValue(1)),
                choice("top_right", QJsonValue(2)),
                choice("bottom_right", QJsonValue(3)),
                choice("bottom_left", QJsonValue(4)),
            },
            0,
            4
        ),
    },
    ExpectedOption{
        "hyprland.general.snap.enabled",
        OptionType::Boolean,
        ControlKind::Toggle,
        false,
        {},
    },
    ExpectedOption{
        "hyprland.general.snap.border_overlap",
        OptionType::Boolean,
        ControlKind::Toggle,
        false,
        {},
    },
    ExpectedOption{
        "hyprland.general.snap.monitor_gap",
        OptionType::Integer,
        ControlKind::SpinBox,
        10,
        numericRange(0, 100),
    },
    ExpectedOption{
        "hyprland.general.snap.respect_gaps",
        OptionType::Boolean,
        ControlKind::Toggle,
        false,
        {},
    },
    ExpectedOption{
        "hyprland.general.snap.window_gap",
        OptionType::Integer,
        ControlKind::SpinBox,
        10,
        numericRange(0, 100),
    },
    ExpectedOption{
        "hyprland.input.follow_mouse",
        OptionType::Enumeration,
        ControlKind::Select,
        1,
        numericChoices(
            QJsonArray{
                choice("disabled", QJsonValue(0)),
                choice("follow", QJsonValue(1)),
                choice("detached", QJsonValue(2)),
                choice("separate", QJsonValue(3)),
            },
            0,
            3
        ),
    },
    ExpectedOption{
        "hyprland.input.mouse_refocus",
        OptionType::Boolean,
        ControlKind::Toggle,
        true,
        {},
    },
    ExpectedOption{
        "hyprland.input.follow_mouse_shrink",
        OptionType::Integer,
        ControlKind::SpinBox,
        0,
        numericRange(0, 300),
    },
    ExpectedOption{
        "hyprland.input.float_switch_override_focus",
        OptionType::Enumeration,
        ControlKind::Select,
        1,
        numericChoices(
            QJsonArray{
                choice("disabled", QJsonValue(0)),
                choice("tiled/floating transitions", QJsonValue(1)),
                choice("all floating transitions", QJsonValue(2)),
            },
            0,
            2
        ),
    },
    ExpectedOption{
        "hyprland.input.focus_on_close",
        OptionType::Enumeration,
        ControlKind::Select,
        0,
        numericChoices(
            QJsonArray{
                choice("next", QJsonValue(0)),
                choice("cursor", QJsonValue(1)),
                choice("mru", QJsonValue(2)),
            },
            0,
            2
        ),
    },
    ExpectedOption{
        "hyprland.input.special_fallthrough",
        OptionType::Boolean,
        ControlKind::Toggle,
        false,
        {},
    },
    ExpectedOption{
        "hyprland.general.no_focus_fallback",
        OptionType::Boolean,
        ControlKind::Toggle,
        false,
        {},
    },
    ExpectedOption{
        "hyprland.general.modal_parent_blocking",
        OptionType::Boolean,
        ControlKind::Toggle,
        true,
        {},
    },
    ExpectedOption{
        "hyprland.general.float_gaps",
        OptionType::CssGap,
        ControlKind::Text,
        QJsonArray{0, 0, 0, 0},
        {},
    },
    ExpectedOption{
        "hyprland.general.gaps_workspaces",
        OptionType::Integer,
        ControlKind::SpinBox,
        0,
        numericRange(0, 100),
    },
    ExpectedOption{
        "hyprland.layout.single_window_aspect_ratio",
        OptionType::Vector2,
        ControlKind::Vector2,
        QJsonArray{0, 0},
        vectorRange(QJsonArray{0, 0}, QJsonArray{1000, 1000}),
        UiTier::Advanced,
    },
    ExpectedOption{
        "hyprland.layout.single_window_aspect_ratio_tolerance",
        OptionType::Number,
        ControlKind::Slider,
        0.1,
        numericRange(0, 1),
        UiTier::Advanced,
    },
    ExpectedOption{
        "hyprland.dwindle.default_split_ratio",
        OptionType::Number,
        ControlKind::Slider,
        1,
        numericRange(0.1, 1.9),
    },
    ExpectedOption{
        "hyprland.dwindle.force_split",
        OptionType::Enumeration,
        ControlKind::Select,
        0,
        numericChoices(
            QJsonArray{
                choice("follow_mouse", QJsonValue(0)),
                choice("left", QJsonValue(1)),
                choice("right", QJsonValue(2)),
            },
            0,
            2
        ),
    },
    ExpectedOption{
        "hyprland.dwindle.permanent_direction_override",
        OptionType::Boolean,
        ControlKind::Toggle,
        false,
        {},
    },
    ExpectedOption{
        "hyprland.dwindle.precise_mouse_move",
        OptionType::Boolean,
        ControlKind::Toggle,
        false,
        {},
    },
    ExpectedOption{
        "hyprland.dwindle.preserve_split",
        OptionType::Boolean,
        ControlKind::Toggle,
        false,
        {},
    },
    ExpectedOption{
        "hyprland.dwindle.smart_resizing",
        OptionType::Boolean,
        ControlKind::Toggle,
        true,
        {},
    },
    ExpectedOption{
        "hyprland.dwindle.smart_split",
        OptionType::Boolean,
        ControlKind::Toggle,
        false,
        {},
    },
    ExpectedOption{
        "hyprland.dwindle.special_scale_factor",
        OptionType::Number,
        ControlKind::Slider,
        1,
        numericRange(0, 1),
    },
    ExpectedOption{
        "hyprland.dwindle.split_bias",
        OptionType::Enumeration,
        ControlKind::Select,
        0,
        numericChoices(
            QJsonArray{
                choice("directional", QJsonValue(0)),
                choice("current", QJsonValue(1)),
            },
            0,
            1
        ),
    },
    ExpectedOption{
        "hyprland.dwindle.split_width_multiplier",
        OptionType::Number,
        ControlKind::Slider,
        1,
        numericRange(0.1, 3),
    },
    ExpectedOption{
        "hyprland.dwindle.use_active_for_splits",
        OptionType::Boolean,
        ControlKind::Toggle,
        true,
        {},
    },
    ExpectedOption{
        "hyprland.master.allow_small_split",
        OptionType::Boolean,
        ControlKind::Toggle,
        false,
        {},
    },
    ExpectedOption{
        "hyprland.master.always_keep_position",
        OptionType::Boolean,
        ControlKind::Toggle,
        false,
        {},
    },
    ExpectedOption{
        "hyprland.master.center_ignores_reserved",
        OptionType::Boolean,
        ControlKind::Toggle,
        false,
        {},
    },
    ExpectedOption{
        "hyprland.master.center_master_fallback",
        OptionType::Enumeration,
        ControlKind::Select,
        QStringLiteral("left"),
        stringChoices(QJsonArray{
            choice("left", QJsonValue(QStringLiteral("left"))),
            choice("right", QJsonValue(QStringLiteral("right"))),
            choice("top", QJsonValue(QStringLiteral("top"))),
            choice("bottom", QJsonValue(QStringLiteral("bottom"))),
        }),
    },
    ExpectedOption{
        "hyprland.master.drop_at_cursor",
        OptionType::Boolean,
        ControlKind::Toggle,
        true,
        {},
    },
    ExpectedOption{
        "hyprland.master.focus_master_on_close",
        OptionType::Boolean,
        ControlKind::Toggle,
        false,
        {},
        UiTier::Common,
        {0, 56, 0},
    },
    ExpectedOption{
        "hyprland.master.mfact",
        OptionType::Number,
        ControlKind::Slider,
        0.55,
        numericRange(0, 1),
    },
    ExpectedOption{
        "hyprland.master.new_on_active",
        OptionType::Enumeration,
        ControlKind::Select,
        QStringLiteral("none"),
        stringChoices(QJsonArray{
            choice("none", QJsonValue(QStringLiteral("none"))),
            choice("before", QJsonValue(QStringLiteral("before"))),
            choice("after", QJsonValue(QStringLiteral("after"))),
        }),
    },
    ExpectedOption{
        "hyprland.master.new_on_top",
        OptionType::Boolean,
        ControlKind::Toggle,
        false,
        {},
    },
    ExpectedOption{
        "hyprland.master.new_status",
        OptionType::Enumeration,
        ControlKind::Select,
        QStringLiteral("slave"),
        stringChoices(QJsonArray{
            choice("master", QJsonValue(QStringLiteral("master"))),
            choice("slave", QJsonValue(QStringLiteral("slave"))),
            choice("inherit", QJsonValue(QStringLiteral("inherit"))),
        }),
    },
    ExpectedOption{
        "hyprland.master.orientation",
        OptionType::Enumeration,
        ControlKind::Select,
        QStringLiteral("left"),
        stringChoices(QJsonArray{
            choice("left", QJsonValue(QStringLiteral("left"))),
            choice("right", QJsonValue(QStringLiteral("right"))),
            choice("top", QJsonValue(QStringLiteral("top"))),
            choice("bottom", QJsonValue(QStringLiteral("bottom"))),
            choice("center", QJsonValue(QStringLiteral("center"))),
        }),
    },
    ExpectedOption{
        "hyprland.master.slave_count_for_center_master",
        OptionType::Integer,
        ControlKind::SpinBox,
        2,
        numericRange(0, 10),
    },
    ExpectedOption{
        "hyprland.master.smart_resizing",
        OptionType::Boolean,
        ControlKind::Toggle,
        true,
        {},
    },
    ExpectedOption{
        "hyprland.master.special_scale_factor",
        OptionType::Number,
        ControlKind::Slider,
        1,
        numericRange(0, 1),
    },
    ExpectedOption{
        "hyprland.scrolling.column_width",
        OptionType::Number,
        ControlKind::Slider,
        0.5,
        numericRange(0.1, 1),
    },
    ExpectedOption{
        "hyprland.scrolling.direction",
        OptionType::Enumeration,
        ControlKind::Select,
        QStringLiteral("right"),
        stringChoices(QJsonArray{
            choice("left", QJsonValue(QStringLiteral("left"))),
            choice("right", QJsonValue(QStringLiteral("right"))),
            choice("up", QJsonValue(QStringLiteral("up"))),
            choice("down", QJsonValue(QStringLiteral("down"))),
        }),
    },
    ExpectedOption{
        "hyprland.scrolling.focus_fit_method",
        OptionType::Enumeration,
        ControlKind::Select,
        1,
        numericChoices(
            QJsonArray{
                choice("center", QJsonValue(0)),
                choice("fit", QJsonValue(1)),
            },
            0,
            1
        ),
    },
    ExpectedOption{
        "hyprland.scrolling.follow_focus",
        OptionType::Boolean,
        ControlKind::Toggle,
        true,
        {},
    },
    ExpectedOption{
        "hyprland.scrolling.follow_min_visible",
        OptionType::Number,
        ControlKind::Slider,
        0.4,
        numericRange(0, 1),
    },
    ExpectedOption{
        "hyprland.scrolling.fullscreen_on_one_column",
        OptionType::Boolean,
        ControlKind::Toggle,
        true,
        {},
    },
    ExpectedOption{
        "hyprland.scrolling.wrap_focus",
        OptionType::Boolean,
        ControlKind::Toggle,
        true,
        {},
    },
    ExpectedOption{
        "hyprland.scrolling.wrap_swapcol",
        OptionType::Boolean,
        ControlKind::Toggle,
        true,
        {},
    },
    ExpectedOption{
        "hyprland.gestures.scrolling.move_snap_cursor",
        OptionType::Boolean,
        ControlKind::Toggle,
        true,
        {},
    },
    ExpectedOption{
        "hyprland.gestures.scrolling.move_snap_to_grid",
        OptionType::Boolean,
        ControlKind::Toggle,
        true,
        {},
    },
    ExpectedOption{
        "hyprland.group.auto_group",
        OptionType::Boolean,
        ControlKind::Toggle,
        true,
        {},
        UiTier::Advanced,
    },
    ExpectedOption{
        "hyprland.group.insert_after_current",
        OptionType::Boolean,
        ControlKind::Toggle,
        true,
        {},
        UiTier::Advanced,
    },
    ExpectedOption{
        "hyprland.group.focus_removed_window",
        OptionType::Boolean,
        ControlKind::Toggle,
        true,
        {},
        UiTier::Advanced,
    },
    ExpectedOption{
        "hyprland.group.drag_into_group",
        OptionType::Enumeration,
        ControlKind::Select,
        1,
        numericChoices(
            QJsonArray{
                choice("disabled", QJsonValue(0)),
                choice("enabled", QJsonValue(1)),
                choice(
                    "only when dragging into the groupbar",
                    QJsonValue(2)
                ),
            },
            0,
            2
        ),
        UiTier::Advanced,
    },
    ExpectedOption{
        "hyprland.group.merge_groups_on_drag",
        OptionType::Boolean,
        ControlKind::Toggle,
        true,
        {},
        UiTier::Advanced,
    },
    ExpectedOption{
        "hyprland.group.merge_groups_on_groupbar",
        OptionType::Boolean,
        ControlKind::Toggle,
        true,
        {},
        UiTier::Advanced,
    },
    ExpectedOption{
        "hyprland.group.merge_floated_into_tiled_on_groupbar",
        OptionType::Boolean,
        ControlKind::Toggle,
        false,
        {},
        UiTier::Advanced,
    },
    ExpectedOption{
        "hyprland.group.group_on_movetoworkspace",
        OptionType::Boolean,
        ControlKind::Toggle,
        false,
        {},
        UiTier::Advanced,
    },
    ExpectedOption{
        "hyprland.group.groupbar.enabled",
        OptionType::Boolean,
        ControlKind::Toggle,
        true,
        {},
        UiTier::Advanced,
    },
    ExpectedOption{
        "hyprland.group.groupbar.disable_when_only",
        OptionType::Boolean,
        ControlKind::Toggle,
        false,
        {},
        UiTier::Advanced,
        {0, 56, 0},
    },
    ExpectedOption{
        "hyprland.group.groupbar.font_family",
        OptionType::String,
        ControlKind::Text,
        QStringLiteral(""),
        boundedString(),
        UiTier::Advanced,
    },
    ExpectedOption{
        "hyprland.group.groupbar.font_weight_active",
        OptionType::FontWeight,
        ControlKind::SpinBox,
        400,
        numericRange(0, 2147483647),
        UiTier::Advanced,
    },
    ExpectedOption{
        "hyprland.group.groupbar.font_weight_inactive",
        OptionType::FontWeight,
        ControlKind::SpinBox,
        400,
        numericRange(0, 2147483647),
        UiTier::Advanced,
    },
    ExpectedOption{
        "hyprland.group.groupbar.font_size",
        OptionType::Integer,
        ControlKind::SpinBox,
        8,
        numericRange(2, 64),
        UiTier::Advanced,
    },
    ExpectedOption{
        "hyprland.group.groupbar.gradients",
        OptionType::Boolean,
        ControlKind::Toggle,
        false,
        {},
        UiTier::Advanced,
    },
    ExpectedOption{
        "hyprland.group.groupbar.height",
        OptionType::Integer,
        ControlKind::SpinBox,
        14,
        numericRange(1, 64),
        UiTier::Advanced,
    },
    ExpectedOption{
        "hyprland.group.groupbar.indicator_gap",
        OptionType::Integer,
        ControlKind::SpinBox,
        0,
        numericRange(0, 64),
        UiTier::Advanced,
    },
    ExpectedOption{
        "hyprland.group.groupbar.indicator_height",
        OptionType::Integer,
        ControlKind::SpinBox,
        3,
        numericRange(1, 64),
        UiTier::Advanced,
    },
    ExpectedOption{
        "hyprland.group.groupbar.stacked",
        OptionType::Boolean,
        ControlKind::Toggle,
        false,
        {},
        UiTier::Advanced,
    },
    ExpectedOption{
        "hyprland.group.groupbar.priority",
        OptionType::Integer,
        ControlKind::SpinBox,
        3,
        numericRange(0, 6),
        UiTier::Advanced,
    },
    ExpectedOption{
        "hyprland.group.groupbar.render_titles",
        OptionType::Boolean,
        ControlKind::Toggle,
        true,
        {},
        UiTier::Advanced,
    },
    ExpectedOption{
        "hyprland.group.groupbar.scrolling",
        OptionType::Boolean,
        ControlKind::Toggle,
        true,
        {},
        UiTier::Advanced,
    },
    ExpectedOption{
        "hyprland.group.groupbar.middle_click_close",
        OptionType::Boolean,
        ControlKind::Toggle,
        true,
        {},
        UiTier::Advanced,
    },
    ExpectedOption{
        "hyprland.group.groupbar.rounding",
        OptionType::Integer,
        ControlKind::SpinBox,
        1,
        numericRange(0, 20),
        UiTier::Advanced,
    },
    ExpectedOption{
        "hyprland.group.groupbar.rounding_power",
        OptionType::Number,
        ControlKind::Slider,
        2,
        numericRange(2, 10),
        UiTier::Advanced,
    },
    ExpectedOption{
        "hyprland.group.groupbar.gradient_rounding",
        OptionType::Integer,
        ControlKind::SpinBox,
        2,
        numericRange(0, 20),
        UiTier::Advanced,
    },
    ExpectedOption{
        "hyprland.group.groupbar.gradient_rounding_power",
        OptionType::Number,
        ControlKind::Slider,
        2,
        numericRange(2, 10),
        UiTier::Advanced,
    },
    ExpectedOption{
        "hyprland.group.groupbar.round_only_edges",
        OptionType::Boolean,
        ControlKind::Toggle,
        true,
        {},
        UiTier::Advanced,
    },
    ExpectedOption{
        "hyprland.group.groupbar.gradient_round_only_edges",
        OptionType::Boolean,
        ControlKind::Toggle,
        true,
        {},
        UiTier::Advanced,
    },
    ExpectedOption{
        "hyprland.group.groupbar.gaps_out",
        OptionType::Integer,
        ControlKind::SpinBox,
        2,
        numericRange(0, 20),
        UiTier::Advanced,
    },
    ExpectedOption{
        "hyprland.group.groupbar.gaps_in",
        OptionType::Integer,
        ControlKind::SpinBox,
        2,
        numericRange(0, 20),
        UiTier::Advanced,
    },
    ExpectedOption{
        "hyprland.group.groupbar.keep_upper_gap",
        OptionType::Boolean,
        ControlKind::Toggle,
        true,
        {},
        UiTier::Advanced,
    },
    ExpectedOption{
        "hyprland.group.groupbar.text_offset",
        OptionType::Integer,
        ControlKind::SpinBox,
        0,
        numericRange(-20, 20),
        UiTier::Advanced,
    },
    ExpectedOption{
        "hyprland.group.groupbar.text_padding",
        OptionType::Integer,
        ControlKind::SpinBox,
        0,
        numericRange(0, 22),
        UiTier::Advanced,
    },
    ExpectedOption{
        "hyprland.group.groupbar.blur",
        OptionType::Boolean,
        ControlKind::Toggle,
        false,
        {},
        UiTier::Advanced,
    },
    ExpectedOption{
        "hyprland.binds.allow_pin_fullscreen",
        OptionType::Boolean,
        ControlKind::Toggle,
        false,
        {},
        UiTier::Advanced,
    },
    ExpectedOption{
        "hyprland.binds.focus_preferred_method",
        OptionType::Enumeration,
        ControlKind::Select,
        0,
        numericChoices(
            QJsonArray{
                choice("history", QJsonValue(0)),
                choice("shared edge length", QJsonValue(1)),
            },
            0,
            1
        ),
        UiTier::Advanced,
    },
    ExpectedOption{
        "hyprland.binds.ignore_group_lock",
        OptionType::Boolean,
        ControlKind::Toggle,
        false,
        {},
        UiTier::Advanced,
    },
    ExpectedOption{
        "hyprland.binds.movefocus_cycles_fullscreen",
        OptionType::Boolean,
        ControlKind::Toggle,
        false,
        {},
        UiTier::Advanced,
    },
    ExpectedOption{
        "hyprland.binds.movefocus_cycles_groupfirst",
        OptionType::Boolean,
        ControlKind::Toggle,
        false,
        {},
        UiTier::Advanced,
    },
    ExpectedOption{
        "hyprland.binds.window_direction_monitor_fallback",
        OptionType::Boolean,
        ControlKind::Toggle,
        true,
        {},
        UiTier::Advanced,
    },
    ExpectedOption{
        "hyprland.misc.enable_anr_dialog",
        OptionType::Boolean,
        ControlKind::Toggle,
        true,
        {},
        UiTier::Advanced,
    },
    ExpectedOption{
        "hyprland.misc.anr_missed_pings",
        OptionType::Integer,
        ControlKind::SpinBox,
        5,
        numericRange(1, 20),
        UiTier::Advanced,
    },
    ExpectedOption{
        "hyprland.misc.size_limits_tiled",
        OptionType::Boolean,
        ControlKind::Toggle,
        false,
        {},
        UiTier::Advanced,
    },
    ExpectedOption{
        "hyprland.misc.always_follow_on_dnd",
        OptionType::Boolean,
        ControlKind::Toggle,
        true,
        {},
        UiTier::Advanced,
    },
    ExpectedOption{
        "hyprland.misc.focus_on_activate",
        OptionType::Boolean,
        ControlKind::Toggle,
        false,
        {},
        UiTier::Advanced,
    },
    ExpectedOption{
        "hyprland.misc.mouse_move_focuses_monitor",
        OptionType::Boolean,
        ControlKind::Toggle,
        true,
        {},
        UiTier::Advanced,
    },
    ExpectedOption{
        "hyprland.misc.on_focus_under_fullscreen",
        OptionType::Enumeration,
        ControlKind::Select,
        2,
        numericChoices(
            QJsonArray{
                choice("ignore", QJsonValue(0)),
                choice("take_over", QJsonValue(1)),
                choice("exit_fullscreen", QJsonValue(2)),
            },
            0,
            2
        ),
        UiTier::Advanced,
    },
    ExpectedOption{
        "hyprland.misc.exit_window_retains_fullscreen",
        OptionType::Boolean,
        ControlKind::Toggle,
        false,
        {},
        UiTier::Advanced,
    },
    ExpectedOption{
        "hyprland.misc.enable_swallow",
        OptionType::Boolean,
        ControlKind::Toggle,
        false,
        {},
        UiTier::Advanced,
    },
    ExpectedOption{
        "hyprland.misc.swallow_regex",
        OptionType::String,
        ControlKind::Text,
        QStringLiteral(""),
        boundedString(),
        UiTier::Advanced,
    },
    ExpectedOption{
        "hyprland.misc.swallow_exception_regex",
        OptionType::String,
        ControlKind::Text,
        QStringLiteral(""),
        boundedString(),
        UiTier::Advanced,
    },
    ExpectedOption{
        "hyprland.input.follow_mouse_threshold",
        OptionType::Number,
        ControlKind::Slider,
        0,
        numericRange(0, 1000000),
        UiTier::Advanced,
    },
};

const std::array workspacesExpectedOptions{
    ExpectedOption{
        "hyprland.animations.workspace_wraparound",
        OptionType::Boolean,
        ControlKind::Toggle,
        false,
        {},
    },
    ExpectedOption{
        "hyprland.gestures.workspace_swipe_cancel_ratio",
        OptionType::Number,
        ControlKind::Slider,
        0.5,
        numericRange(0, 1),
    },
    ExpectedOption{
        "hyprland.gestures.workspace_swipe_create_new",
        OptionType::Boolean,
        ControlKind::Toggle,
        true,
        {},
    },
    ExpectedOption{
        "hyprland.gestures.workspace_swipe_direction_lock",
        OptionType::Boolean,
        ControlKind::Toggle,
        true,
        {},
    },
    ExpectedOption{
        "hyprland.gestures.workspace_swipe_direction_lock_threshold",
        OptionType::Integer,
        ControlKind::SpinBox,
        10,
        numericRange(0, 200),
    },
    ExpectedOption{
        "hyprland.gestures.workspace_swipe_distance",
        OptionType::Integer,
        ControlKind::SpinBox,
        300,
        numericRange(0, 2000),
    },
    ExpectedOption{
        "hyprland.gestures.workspace_swipe_forever",
        OptionType::Boolean,
        ControlKind::Toggle,
        false,
        {},
    },
    ExpectedOption{
        "hyprland.gestures.workspace_swipe_invert",
        OptionType::Boolean,
        ControlKind::Toggle,
        true,
        {},
    },
    ExpectedOption{
        "hyprland.gestures.workspace_swipe_min_speed_to_force",
        OptionType::Integer,
        ControlKind::SpinBox,
        30,
        numericRange(0, 200),
    },
    ExpectedOption{
        "hyprland.gestures.workspace_swipe_touch",
        OptionType::Boolean,
        ControlKind::Toggle,
        false,
        {},
    },
    ExpectedOption{
        "hyprland.gestures.workspace_swipe_touch_invert",
        OptionType::Boolean,
        ControlKind::Toggle,
        false,
        {},
    },
    ExpectedOption{
        "hyprland.gestures.workspace_swipe_use_r",
        OptionType::Boolean,
        ControlKind::Toggle,
        false,
        {},
    },
    ExpectedOption{
        "hyprland.misc.close_special_on_empty",
        OptionType::Boolean,
        ControlKind::Toggle,
        true,
        {},
        UiTier::Advanced,
    },
    ExpectedOption{
        "hyprland.misc.initial_workspace_tracking",
        OptionType::Enumeration,
        ControlKind::Select,
        1,
        numericChoices(
            QJsonArray{
                choice("disabled", QJsonValue(0)),
                choice("single-shot", QJsonValue(1)),
                choice("persistent for children", QJsonValue(2)),
            },
            0,
            2
        ),
        UiTier::Advanced,
    },
    ExpectedOption{
        "hyprland.misc.initial_workspace_token_timeout",
        OptionType::Integer,
        ControlKind::SpinBox,
        10,
        numericRange(1, 3600),
        UiTier::Advanced,
        {0, 56, 0},
    },
    ExpectedOption{
        "hyprland.binds.allow_workspace_cycles",
        OptionType::Boolean,
        ControlKind::Toggle,
        false,
        {},
        UiTier::Advanced,
    },
    ExpectedOption{
        "hyprland.binds.hide_special_on_workspace_change",
        OptionType::Boolean,
        ControlKind::Toggle,
        false,
        {},
        UiTier::Advanced,
    },
    ExpectedOption{
        "hyprland.binds.workspace_back_and_forth",
        OptionType::Boolean,
        ControlKind::Toggle,
        false,
        {},
        UiTier::Advanced,
    },
    ExpectedOption{
        "hyprland.binds.workspace_center_on",
        OptionType::Enumeration,
        ControlKind::Select,
        1,
        numericChoices(
            QJsonArray{
                choice("workspace center", QJsonValue(0)),
                choice("last active window", QJsonValue(1)),
            },
            0,
            1
        ),
        UiTier::Advanced,
    },
    ExpectedOption{
        "hyprland.cursor.warp_on_change_workspace",
        OptionType::Enumeration,
        ControlKind::Select,
        0,
        numericChoices(
            QJsonArray{
                choice("disable", QJsonValue(0)),
                choice("enable", QJsonValue(1)),
                choice("force", QJsonValue(2)),
            },
            0,
            2
        ),
        UiTier::Advanced,
    },
    ExpectedOption{
        "hyprland.cursor.warp_on_toggle_special",
        OptionType::Enumeration,
        ControlKind::Select,
        0,
        numericChoices(
            QJsonArray{
                choice("disable", QJsonValue(0)),
                choice("enable", QJsonValue(1)),
                choice("force", QJsonValue(2)),
            },
            0,
            2
        ),
        UiTier::Advanced,
    },
};

const std::array advancedExpectedOptions{
    ExpectedOption{
        "hyprland.misc.allow_session_lock_restore",
        OptionType::Boolean,
        ControlKind::Toggle,
        false,
        {},
        UiTier::Advanced,
    },
    ExpectedOption{
        "hyprland.misc.lockdead_screen_delay",
        OptionType::Integer,
        ControlKind::SpinBox,
        1000,
        numericRange(0, 5000),
        UiTier::Advanced,
    },
    ExpectedOption{
        "hyprland.misc.disable_scale_notification",
        OptionType::Boolean,
        ControlKind::Toggle,
        false,
        {},
        UiTier::Advanced,
    },
    ExpectedOption{
        "hyprland.misc.render_unfocused_fps",
        OptionType::Integer,
        ControlKind::SpinBox,
        15,
        numericRange(1, 120),
        UiTier::Advanced,
    },
    ExpectedOption{
        "hyprland.misc.screencopy_force_8b",
        OptionType::Boolean,
        ControlKind::Toggle,
        true,
        {},
        UiTier::Advanced,
    },
    ExpectedOption{
        "hyprland.misc.disable_hyprland_logo",
        OptionType::Boolean,
        ControlKind::Toggle,
        false,
        {},
        UiTier::Advanced,
    },
    ExpectedOption{
        "hyprland.misc.disable_splash_rendering",
        OptionType::Boolean,
        ControlKind::Toggle,
        false,
        {},
        UiTier::Advanced,
    },
    ExpectedOption{
        "hyprland.misc.session_lock_xray",
        OptionType::Boolean,
        ControlKind::Toggle,
        false,
        {},
        UiTier::Advanced,
    },
    ExpectedOption{
        "hyprland.misc.session_lock_blur",
        OptionType::Boolean,
        ControlKind::Toggle,
        false,
        {},
        UiTier::Advanced,
        Hyprland::SemanticVersion{0, 56, 0},
    },
    ExpectedOption{
        "hyprland.xwayland.use_nearest_neighbor",
        OptionType::Boolean,
        ControlKind::Toggle,
        true,
        {},
        UiTier::Advanced,
        Hyprland::SemanticVersion{0, 55, 0},
        RiskLevel::Caution,
    },
    ExpectedOption{
        "hyprland.render.expand_undersized_textures",
        OptionType::Boolean,
        ControlKind::Toggle,
        true,
        {},
        UiTier::Advanced,
        Hyprland::SemanticVersion{0, 55, 0},
        RiskLevel::Caution,
    },
    ExpectedOption{
        "hyprland.render.direct_scanout",
        OptionType::Enumeration,
        ControlKind::Select,
        0,
        numericChoices(
            QJsonArray{
                choice("disable", QJsonValue(0)),
                choice("enable", QJsonValue(1)),
                choice("auto", QJsonValue(2)),
            },
            0,
            2
        ),
        UiTier::Advanced,
        Hyprland::SemanticVersion{0, 55, 0},
        RiskLevel::Caution,
    },
    ExpectedOption{
        "hyprland.render.fp16_sdr_tf",
        OptionType::Enumeration,
        ControlKind::Select,
        0,
        numericChoices(
            QJsonArray{
                choice("monitor", QJsonValue(0)),
                choice("linear", QJsonValue(1)),
            },
            0,
            1
        ),
        UiTier::Advanced,
        Hyprland::SemanticVersion{0, 55, 0},
        RiskLevel::Caution,
    },
    ExpectedOption{
        "hyprland.render.xp_mode",
        OptionType::Boolean,
        ControlKind::Toggle,
        false,
        {},
        UiTier::Advanced,
        Hyprland::SemanticVersion{0, 55, 0},
        RiskLevel::Caution,
    },
    ExpectedOption{
        "hyprland.input-capture.capture_modifiers",
        OptionType::Boolean,
        ControlKind::Toggle,
        false,
        {},
        UiTier::Advanced,
        Hyprland::SemanticVersion{0, 56, 0},
        RiskLevel::Caution,
    },
    ExpectedOption{
        "hyprland.input-capture.enforce_barriers",
        OptionType::Boolean,
        ControlKind::Toggle,
        true,
        {},
        UiTier::Advanced,
        Hyprland::SemanticVersion{0, 56, 0},
        RiskLevel::Caution,
    },
};

[[nodiscard]] bool hasExactContract(
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
        && option.uiTier == expected.uiTier
        && option.applyMode == ApplyMode::Reload
        && option.risk == expected.risk
        && option.since == expected.since
        && !option.until.has_value();
}

[[nodiscard]] QJsonObject constraintsObject(const OptionDefinition &option)
{
    QJsonObject result;
    if (option.constraints.minimum) {
        result.insert(QStringLiteral("min"), *option.constraints.minimum);
    }
    if (option.constraints.maximum) {
        result.insert(QStringLiteral("max"), *option.constraints.maximum);
    }
    if (option.constraints.step) {
        result.insert(QStringLiteral("step"), *option.constraints.step);
    }
    if (!option.constraints.choices.isEmpty()) {
        QJsonArray choices;
        for (const auto &choice : option.constraints.choices) {
            choices.append(choice);
        }
        result.insert(QStringLiteral("choices"), choices);
    }
    if (option.constraints.maximumLength) {
        result.insert(
            QStringLiteral("maxLength"),
            static_cast<qint64>(*option.constraints.maximumLength)
        );
    }
    if (option.constraints.pattern) {
        result.insert(QStringLiteral("pattern"), *option.constraints.pattern);
    }
    return result;
}

[[nodiscard]] QVariantMap optionMetadata(const OptionDefinition &option)
{
    QVariantMap result{
        {QStringLiteral("id"), option.id},
        {QStringLiteral("type"), Hyprland::toString(option.type)},
        {QStringLiteral("control"), Hyprland::toString(option.control)},
        {QStringLiteral("defaultValue"), option.defaultValue.toVariant()},
        {QStringLiteral("risk"), Hyprland::toString(option.risk)},
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
    if (option.constraints.maximumLength) {
        result.insert(
            QStringLiteral("maxLength"),
            static_cast<qint64>(*option.constraints.maximumLength)
        );
    }
    if (option.constraints.pattern) {
        result.insert(QStringLiteral("pattern"), *option.constraints.pattern);
    }
    return result;
}

[[nodiscard]] QVariantMap fullOptionMetadata(const OptionDefinition &option)
{
    auto result = optionMetadata(option);
    result.insert(QStringLiteral("module"), option.module);
    result.insert(QStringLiteral("path"), option.path);
    result.insert(QStringLiteral("luaPath"), option.luaPath);
    result.insert(QStringLiteral("uiTier"), Hyprland::toString(option.uiTier));
    result.insert(QStringLiteral("writable"), option.writable);
    result.insert(
        QStringLiteral("defaultPolicy"),
        Hyprland::toString(option.defaultPolicy)
    );
    result.insert(
        QStringLiteral("inheritedDefaultFrom"),
        option.inheritedDefaultFrom.value_or(QString{})
    );
    result.insert(
        QStringLiteral("applyMode"), Hyprland::toString(option.applyMode)
    );
    result.insert(QStringLiteral("since"), Hyprland::toString(option.since));
    result.insert(
        QStringLiteral("until"),
        option.until ? Hyprland::toString(*option.until) : QString{}
    );
    return result;
}

[[nodiscard]] std::optional<QJsonValue> resolvedOptionValue(
    const Hyprland::Catalog &catalog,
    const QJsonObject &overrides,
    const OptionDefinition &option,
    QSet<QString> &visiting,
    QString &error
)
{
    if (visiting.contains(option.id)) {
        error = QStringLiteral(
            "The compositor option default inheritance is cyclic"
        );
        return std::nullopt;
    }
    visiting.insert(option.id);

    QJsonValue value;
    if (overrides.contains(option.id)) {
        value = overrides.value(option.id);
    } else if (option.inheritedDefaultFrom) {
        const auto source = std::ranges::find_if(
            catalog.options,
            [&option](const OptionDefinition &candidate) {
                return candidate.path == *option.inheritedDefaultFrom;
            }
        );
        if (source == catalog.options.cend()) {
            error = QStringLiteral(
                "The compositor option inherited default source is missing"
            );
            visiting.remove(option.id);
            return std::nullopt;
        }
        const auto inherited = resolvedOptionValue(
            catalog, overrides, *source, visiting, error
        );
        if (!inherited) {
            visiting.remove(option.id);
            return std::nullopt;
        }
        value = *inherited;
    } else {
        value = option.defaultValue;
    }
    visiting.remove(option.id);

    if (!Hyprland::validateOptionValue(option, value).isEmpty()) {
        error = QStringLiteral("The compositor option value %1 is invalid")
                    .arg(option.id);
        return std::nullopt;
    }
    return value;
}

[[nodiscard]] std::optional<QVariantMap> allOptionValues(
    const Hyprland::Catalog &catalog,
    const QJsonObject &snapshot,
    QString &error
)
{
    error.clear();
    const auto overridesValue = snapshot.value(QStringLiteral("overrides"));
    if (!overridesValue.isObject()) {
        error = QStringLiteral("The compositor snapshot has invalid overrides");
        return std::nullopt;
    }

    const auto overrides = overridesValue.toObject();
    QVariantMap result;
    for (const auto &option : catalog.options) {
        QSet<QString> visiting;
        const auto value = resolvedOptionValue(
            catalog, overrides, option, visiting, error
        );
        if (!value) return std::nullopt;
        result.insert(option.id, value->toVariant());
    }
    return result;
}

template<std::size_t Size>
[[nodiscard]] bool qualifyGroup(
    const Hyprland::Catalog &catalog,
    const std::array<ExpectedOption, Size> &expectedOptions,
    const QString &unsupportedError,
    QVariantList &metadata,
    QString &error
)
{
    metadata.clear();
    metadata.reserve(static_cast<qsizetype>(expectedOptions.size()));
    for (const auto &expected : expectedOptions) {
        const auto *option = Hyprland::findOption(
            catalog, QString::fromLatin1(expected.id)
        );
        if (option == nullptr || !hasExactContract(*option, expected)
            || constraintsObject(*option) != expected.constraints) {
            metadata.clear();
            error = unsupportedError;
            return false;
        }
        metadata.append(optionMetadata(*option));
    }
    error.clear();
    return true;
}

template<std::size_t Size>
[[nodiscard]] bool containsOption(
    const std::array<ExpectedOption, Size> &expectedOptions,
    const QString &id
)
{
    return std::ranges::any_of(
        expectedOptions,
        [&id](const auto &expected) {
            return id == QLatin1String(expected.id);
        }
    );
}

template<std::size_t Size>
[[nodiscard]] QStringList optionIds(
    const std::array<ExpectedOption, Size> &expectedOptions
)
{
    QStringList result;
    result.reserve(static_cast<qsizetype>(expectedOptions.size()));
    for (const auto &expected : expectedOptions) {
        result.append(QString::fromLatin1(expected.id));
    }
    return result;
}

template<std::size_t Size>
[[nodiscard]] std::optional<QVariantMap> optionValues(
    const Hyprland::Catalog &catalog,
    const std::array<ExpectedOption, Size> &expectedOptions,
    const bool contractAvailable,
    const QString &incompleteError,
    const QString &invalidValueError,
    const QJsonObject &snapshot,
    QString &error
)
{
    error.clear();
    if (!contractAvailable) {
        error = incompleteError;
        return std::nullopt;
    }
    const auto overridesValue = snapshot.value(QStringLiteral("overrides"));
    if (!overridesValue.isObject()) {
        error = QStringLiteral("The compositor snapshot has invalid overrides");
        return std::nullopt;
    }
    const auto overrides = overridesValue.toObject();
    QVariantMap result;
    for (const auto &expected : expectedOptions) {
        const auto id = QString::fromLatin1(expected.id);
        const auto *option = Hyprland::findOption(catalog, id);
        if (option == nullptr) {
            error = incompleteError;
            return std::nullopt;
        }
        const auto value = overrides.contains(id)
            ? overrides.value(id)
            : option->defaultValue;
        if (!Hyprland::validateOptionValue(*option, value).isEmpty()) {
            error = invalidValueError;
            return std::nullopt;
        }
        result.insert(id, value.toVariant());
    }
    return result;
}

} // namespace

namespace Internal {

bool qualifyAppearanceCatalogContract(
    const Hyprland::Catalog &catalog,
    QVariantList &metadata,
    QString &error
)
{
    return qualifyGroup(
        catalog,
        appearanceExpectedOptions,
        QStringLiteral(
            "The compositor appearance option contract is unsupported"
        ),
        metadata,
        error
    );
}

bool qualifyInputCatalogContract(
    const Hyprland::Catalog &catalog,
    QVariantList &metadata,
    QString &error
)
{
    return qualifyGroup(
        catalog,
        inputExpectedOptions,
        QStringLiteral("The compositor input option contract is unsupported"),
        metadata,
        error
    );
}

} // namespace Internal

std::optional<CompositorActionCatalog> CompositorActionCatalog::fromBytes(
    const QByteArrayView actionCatalog,
    const QString &replyDigest,
    const QString &advertisedDigest,
    const QByteArrayView configSchema,
    const QString &replySchemaDigest,
    QString &error
)
{
    error.clear();
    if (actionCatalog.isEmpty()
        || actionCatalog.size() > Hyprland::maximumActionCatalogBytes
        || configSchema.isEmpty()
        || configSchema.size() > Hyprland::maximumActionSchemaBytes) {
        error = QStringLiteral(
            "The compositor action authority has an invalid size"
        );
        return std::nullopt;
    }
    if (replyDigest != advertisedDigest) {
        error = QStringLiteral(
            "The compositor action authority changed"
        );
        return std::nullopt;
    }
    const auto schemaDigest = QString::fromLatin1(
        QCryptographicHash::hash(
            configSchema, QCryptographicHash::Sha256
        ).toHex()
    );
    if (schemaDigest != replySchemaDigest) {
        error = QStringLiteral(
            "The compositor config schema digest is invalid"
        );
        return std::nullopt;
    }

    auto parsed = Hyprland::parseActionCatalog(actionCatalog, configSchema);
    if (!parsed) {
        error = parsed.errors.isEmpty()
            ? QStringLiteral("The compositor action authority is invalid")
            : parsed.errors.constFirst().message;
        return std::nullopt;
    }
    if (Hyprland::canonicalActionCatalogJson(*parsed.value) != actionCatalog
        || parsed.value->digest != replyDigest
        || parsed.value->configSchemaDigest != replySchemaDigest
        || QByteArrayView(parsed.value->configSchemaDocument) != configSchema) {
        error = QStringLiteral(
            "The compositor action authority is not canonical"
        );
        return std::nullopt;
    }

    CompositorActionCatalog result;
    result.catalog_ = std::move(*parsed.value);
    return result;
}

const QString &CompositorActionCatalog::digest() const
{
    return catalog_.digest;
}

const QString &CompositorActionCatalog::configSchemaDigest() const
{
    return catalog_.configSchemaDigest;
}

const Hyprland::ActionCatalog &CompositorActionCatalog::catalog() const
{
    return catalog_;
}

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
    result.all_.options.reserve(result.catalog_.options.size());
    for (const auto &option : result.catalog_.options) {
        result.all_.options.append(fullOptionMetadata(option));
    }
    result.all_.available = true;
    result.all_.error.clear();
    result.appearance_.available = Internal::qualifyAppearanceCatalogContract(
        result.catalog_,
        result.appearance_.options,
        result.appearance_.error
    );
    result.input_.available = Internal::qualifyInputCatalogContract(
        result.catalog_,
        result.input_.options,
        result.input_.error
    );
    result.windows_.available = qualifyGroup(
        result.catalog_,
        windowsExpectedOptions,
        QStringLiteral("The compositor windows option contract is unsupported"),
        result.windows_.options,
        result.windows_.error
    );
    result.workspaces_.available = qualifyGroup(
        result.catalog_,
        workspacesExpectedOptions,
        QStringLiteral(
            "The compositor workspaces option contract is unsupported"
        ),
        result.workspaces_.options,
        result.workspaces_.error
    );
    result.advanced_.available = qualifyGroup(
        result.catalog_,
        advancedExpectedOptions,
        QStringLiteral(
            "The compositor advanced option contract is unsupported"
        ),
        result.advanced_.options,
        result.advanced_.error
    );
    return result;
}

const QString &CompositorOptionCatalog::digest() const
{
    return catalog_.digest;
}

const Hyprland::Catalog &CompositorOptionCatalog::catalog() const
{
    return catalog_;
}

bool CompositorOptionCatalog::allOptionsContractAvailable() const
{
    return all_.available;
}

const QString &CompositorOptionCatalog::allOptionsContractError() const
{
    return all_.error;
}

const QVariantList &CompositorOptionCatalog::allOptions() const
{
    return all_.options;
}

const OptionDefinition *CompositorOptionCatalog::allOption(
    const QString &id
) const
{
    return all_.available ? Hyprland::findOption(catalog_, id) : nullptr;
}

std::optional<QVariantMap> CompositorOptionCatalog::allValues(
    const QJsonObject &snapshot,
    QString &error
) const
{
    if (!all_.available) {
        error = QStringLiteral("The compositor option catalog is incomplete");
        return std::nullopt;
    }
    return allOptionValues(catalog_, snapshot, error);
}

bool CompositorOptionCatalog::appearanceContractAvailable() const
{
    return appearance_.available;
}

const QString &CompositorOptionCatalog::appearanceContractError() const
{
    return appearance_.error;
}

const QVariantList &CompositorOptionCatalog::appearanceOptions() const
{
    return appearance_.options;
}

const OptionDefinition *CompositorOptionCatalog::appearanceOption(
    const QString &id
) const
{
    if (!appearance_.available
        || !containsOption(appearanceExpectedOptions, id)) {
        return nullptr;
    }
    return Hyprland::findOption(catalog_, id);
}

QStringList CompositorOptionCatalog::appearanceOptionIds() const
{
    return optionIds(appearanceExpectedOptions);
}

std::optional<QVariantMap> CompositorOptionCatalog::appearanceValues(
    const QJsonObject &snapshot,
    QString &error
) const
{
    return optionValues(
        catalog_,
        appearanceExpectedOptions,
        appearance_.available,
        QStringLiteral("The compositor appearance catalog is incomplete"),
        QStringLiteral("The compositor appearance value is invalid"),
        snapshot,
        error
    );
}

bool CompositorOptionCatalog::inputContractAvailable() const
{
    return input_.available;
}

const QString &CompositorOptionCatalog::inputContractError() const
{
    return input_.error;
}

const QVariantList &CompositorOptionCatalog::inputOptions() const
{
    return input_.options;
}

const OptionDefinition *CompositorOptionCatalog::inputOption(
    const QString &id
) const
{
    if (!input_.available || !containsOption(inputExpectedOptions, id)) {
        return nullptr;
    }
    return Hyprland::findOption(catalog_, id);
}

QStringList CompositorOptionCatalog::inputOptionIds() const
{
    return optionIds(inputExpectedOptions);
}

std::optional<QVariantMap> CompositorOptionCatalog::inputValues(
    const QJsonObject &snapshot,
    QString &error
) const
{
    return optionValues(
        catalog_,
        inputExpectedOptions,
        input_.available,
        QStringLiteral("The compositor input catalog is incomplete"),
        QStringLiteral("The compositor input value is invalid"),
        snapshot,
        error
    );
}

bool CompositorOptionCatalog::windowsContractAvailable() const
{
    return windows_.available;
}

const QString &CompositorOptionCatalog::windowsContractError() const
{
    return windows_.error;
}

const QVariantList &CompositorOptionCatalog::windowsOptions() const
{
    return windows_.options;
}

const OptionDefinition *CompositorOptionCatalog::windowsOption(
    const QString &id
) const
{
    if (!windows_.available || !containsOption(windowsExpectedOptions, id)) {
        return nullptr;
    }
    return Hyprland::findOption(catalog_, id);
}

QStringList CompositorOptionCatalog::windowsOptionIds() const
{
    return optionIds(windowsExpectedOptions);
}

std::optional<QVariantMap> CompositorOptionCatalog::windowsValues(
    const QJsonObject &snapshot,
    QString &error
) const
{
    return optionValues(
        catalog_,
        windowsExpectedOptions,
        windows_.available,
        QStringLiteral("The compositor windows catalog is incomplete"),
        QStringLiteral("The compositor windows value is invalid"),
        snapshot,
        error
    );
}

bool CompositorOptionCatalog::workspacesContractAvailable() const
{
    return workspaces_.available;
}

const QString &CompositorOptionCatalog::workspacesContractError() const
{
    return workspaces_.error;
}

const QVariantList &CompositorOptionCatalog::workspacesOptions() const
{
    return workspaces_.options;
}

const OptionDefinition *CompositorOptionCatalog::workspacesOption(
    const QString &id
) const
{
    if (!workspaces_.available
        || !containsOption(workspacesExpectedOptions, id)) {
        return nullptr;
    }
    return Hyprland::findOption(catalog_, id);
}

QStringList CompositorOptionCatalog::workspacesOptionIds() const
{
    return optionIds(workspacesExpectedOptions);
}

std::optional<QVariantMap> CompositorOptionCatalog::workspacesValues(
    const QJsonObject &snapshot,
    QString &error
) const
{
    return optionValues(
        catalog_,
        workspacesExpectedOptions,
        workspaces_.available,
        QStringLiteral("The compositor workspaces catalog is incomplete"),
        QStringLiteral("The compositor workspaces value is invalid"),
        snapshot,
        error
    );
}

bool CompositorOptionCatalog::advancedContractAvailable() const
{
    return advanced_.available;
}

const QString &CompositorOptionCatalog::advancedContractError() const
{
    return advanced_.error;
}

const QVariantList &CompositorOptionCatalog::advancedOptions() const
{
    return advanced_.options;
}

const OptionDefinition *CompositorOptionCatalog::advancedOption(
    const QString &id
) const
{
    if (!advanced_.available || !containsOption(advancedExpectedOptions, id)) {
        return nullptr;
    }
    return Hyprland::findOption(catalog_, id);
}

QStringList CompositorOptionCatalog::advancedOptionIds() const
{
    return optionIds(advancedExpectedOptions);
}

std::optional<QVariantMap> CompositorOptionCatalog::advancedValues(
    const QJsonObject &snapshot,
    QString &error
) const
{
    return optionValues(
        catalog_,
        advancedExpectedOptions,
        advanced_.available,
        QStringLiteral("The compositor advanced catalog is incomplete"),
        QStringLiteral("The compositor advanced value is invalid"),
        snapshot,
        error
    );
}

} // namespace HyprShelld
