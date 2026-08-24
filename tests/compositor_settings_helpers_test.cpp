#include "compositor_option_catalog.h"
#include "compositor_snapshot_editor.h"
#include "hyprland/catalog.h"
#include "hyprland/desired_state.h"
#include "hyprland/input_device_inventory.h"
#include "hyprland/json_support.h"
#include "input_device_projection.h"

#include <QFile>
#include <QCryptographicHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSet>
#include <QtTest>

#include <array>
#include <limits>
#include <tuple>

namespace HyprShelld::Internal {

[[nodiscard]] bool qualifyAppearanceCatalogContract(
    const Hyprland::Catalog &catalog,
    QVariantList &metadata,
    QString &error
);

[[nodiscard]] bool qualifyInputCatalogContract(
    const Hyprland::Catalog &catalog,
    QVariantList &metadata,
    QString &error
);

} // namespace HyprShelld::Internal

namespace {

[[nodiscard]] QVariantMap appearanceDefaults()
{
    return {
        {QStringLiteral("hyprland.general.border_size"), 1},
        {QStringLiteral("hyprland.decoration.rounding"), 0},
        {
            QStringLiteral("hyprland.general.gaps_in"),
            QVariantList{5, 5, 5, 5},
        },
        {
            QStringLiteral("hyprland.general.gaps_out"),
            QVariantList{20, 20, 20, 20},
        },
        {QStringLiteral("hyprland.decoration.blur.enabled"), true},
        {QStringLiteral("hyprland.decoration.shadow.enabled"), true},
        {QStringLiteral("hyprland.animations.enabled"), true},
        {QStringLiteral("hyprland.decoration.dim_inactive"), false},
        {QStringLiteral("hyprland.decoration.dim_strength"), 0.5},
        {QStringLiteral("hyprland.decoration.active_opacity"), 1.0},
        {QStringLiteral("hyprland.decoration.inactive_opacity"), 1.0},
        {QStringLiteral("hyprland.decoration.fullscreen_opacity"), 1.0},
        {QStringLiteral("hyprland.decoration.dim_modal"), true},
        {QStringLiteral("hyprland.decoration.dim_special"), 0.2},
        {QStringLiteral("hyprland.decoration.dim_around"), 0.4},
        {QStringLiteral("hyprland.decoration.blur.size"), 8},
        {QStringLiteral("hyprland.decoration.blur.passes"), 1},
        {QStringLiteral("hyprland.decoration.blur.ignore_opacity"), true},
        {QStringLiteral("hyprland.decoration.blur.new_optimizations"), true},
        {QStringLiteral("hyprland.decoration.blur.xray"), false},
        {QStringLiteral("hyprland.decoration.blur.special"), false},
        {QStringLiteral("hyprland.decoration.blur.popups"), false},
        {
            QStringLiteral("hyprland.decoration.blur.popups_ignorealpha"),
            0.2,
        },
        {QStringLiteral("hyprland.decoration.blur.input_methods"), false},
        {
            QStringLiteral(
                "hyprland.decoration.blur.input_methods_ignorealpha"
            ),
            0.2,
        },
        {QStringLiteral("hyprland.decoration.blur.brightness"), 1.0},
        {QStringLiteral("hyprland.decoration.blur.contrast"), 0.8916},
        {QStringLiteral("hyprland.decoration.blur.noise"), 0.0117},
        {QStringLiteral("hyprland.decoration.blur.vibrancy"), 0.1696},
        {
            QStringLiteral("hyprland.decoration.blur.vibrancy_darkness"),
            0.0,
        },
        {
            QStringLiteral("hyprland.decoration.border_part_of_window"),
            true,
        },
        {QStringLiteral("hyprland.decoration.rounding_power"), 2.0},
        {QStringLiteral("hyprland.decoration.shadow.range"), 4},
        {QStringLiteral("hyprland.decoration.shadow.render_power"), 3},
        {QStringLiteral("hyprland.decoration.shadow.sharp"), false},
        {
            QStringLiteral("hyprland.decoration.shadow.offset"),
            QVariantList{0.0, 0.0},
        },
        {QStringLiteral("hyprland.decoration.shadow.scale"), 1.0},
        {QStringLiteral("hyprland.decoration.glow.enabled"), false},
        {QStringLiteral("hyprland.decoration.glow.range"), 10},
        {QStringLiteral("hyprland.decoration.glow.render_power"), 3},
    };
}

[[nodiscard]] QByteArray readBytes(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) return {};
    return file.readAll();
}

[[nodiscard]] QJsonObject readObject(const QString &path)
{
    return QJsonDocument::fromJson(readBytes(path)).object();
}

[[nodiscard]] QVariantMap inputDefaults()
{
    return {
        {QStringLiteral("hyprland.input.repeat_rate"), 25},
        {QStringLiteral("hyprland.input.repeat_delay"), 600},
        {QStringLiteral("hyprland.input.sensitivity"), 0.0},
        {QStringLiteral("hyprland.input.accel_profile"), QString{}},
        {QStringLiteral("hyprland.input.natural_scroll"), false},
        {QStringLiteral("hyprland.input.left_handed"), false},
        {QStringLiteral("hyprland.input.scroll_factor"), 1.0},
        {QStringLiteral("hyprland.input.touchpad.tap-to-click"), true},
        {QStringLiteral("hyprland.input.touchpad.tap-and-drag"), true},
        {QStringLiteral("hyprland.input.touchpad.natural_scroll"), false},
        {
            QStringLiteral("hyprland.input.touchpad.disable_while_typing"),
            true,
        },
        {QStringLiteral("hyprland.input.touchpad.scroll_factor"), 1.0},
        {QStringLiteral("hyprland.input.scroll_method"), QString{}},
        {QStringLiteral("hyprland.input.scroll_button"), 0},
        {QStringLiteral("hyprland.input.scroll_button_lock"), false},
        {QStringLiteral("hyprland.input.off_window_axis_events"), 1},
        {QStringLiteral("hyprland.input.emulate_discrete_scroll"), 1},
        {
            QStringLiteral("hyprland.input.touchpad.clickfinger_behavior"),
            false,
        },
        {QStringLiteral("hyprland.input.touchpad.drag_3fg"), 0},
        {QStringLiteral("hyprland.input.touchpad.drag_lock"), 0},
        {QStringLiteral("hyprland.input.touchpad.flip_x"), false},
        {QStringLiteral("hyprland.input.touchpad.flip_y"), false},
        {
            QStringLiteral(
                "hyprland.input.touchpad.middle_button_emulation"
            ),
            false,
        },
        {QStringLiteral("hyprland.input.touchpad.tap_button_map"), QString{}},
        {QStringLiteral("hyprland.input.numlock_by_default"), false},
        {
            QStringLiteral("hyprland.input.virtualkeyboard.share_states"),
            2,
        },
        {
            QStringLiteral(
                "hyprland.input.virtualkeyboard.release_pressed_on_close"
            ),
            false,
        },
        {QStringLiteral("hyprland.misc.name_vk_after_proc"), true},
        {QStringLiteral("hyprland.input.force_no_accel"), false},
        {QStringLiteral("hyprland.input.rotation"), 0},
        {QStringLiteral("hyprland.misc.middle_click_paste"), true},
        {QStringLiteral("hyprland.gestures.close_max_timeout"), 1000},
        {QStringLiteral("hyprland.input.touchdevice.enabled"), true},
        {QStringLiteral("hyprland.input.touchdevice.transform"), 0},
        {QStringLiteral("hyprland.input.tablet.relative_input"), false},
        {QStringLiteral("hyprland.input.tablet.left_handed"), false},
        {QStringLiteral("hyprland.input.tablet.transform"), 0},
        {QStringLiteral("hyprland.cursor.hide_on_key_press"), false},
        {QStringLiteral("hyprland.cursor.hide_on_touch"), true},
        {QStringLiteral("hyprland.cursor.hide_on_tablet"), false},
        {QStringLiteral("hyprland.cursor.inactive_timeout"), 0.0},
        {QStringLiteral("hyprland.cursor.hotspot_padding"), 0},
        {QStringLiteral("hyprland.cursor.no_warps"), false},
        {QStringLiteral("hyprland.cursor.persistent_warps"), false},
        {
            QStringLiteral(
                "hyprland.cursor.warp_back_after_non_mouse_input"
            ),
            false,
        },
        {
            QStringLiteral("hyprland.input.tablet.region_position"),
            QVariantList{0.0, 0.0},
        },
        {
            QStringLiteral(
                "hyprland.input.tablet.absolute_region_position"
            ),
            false,
        },
        {
            QStringLiteral("hyprland.input.tablet.region_size"),
            QVariantList{0.0, 0.0},
        },
        {QStringLiteral("hyprland.input.resolve_binds_by_sym"), false},
    };
}

[[nodiscard]] QVariantMap windowsDefaults()
{
    return {
        {QStringLiteral("hyprland.general.layout"), QStringLiteral("dwindle")},
        {QStringLiteral("hyprland.general.resize_on_border"), false},
        {QStringLiteral("hyprland.general.extend_border_grab_area"), 15},
        {QStringLiteral("hyprland.general.hover_icon_on_border"), true},
        {QStringLiteral("hyprland.general.resize_corner"), 0},
        {QStringLiteral("hyprland.general.snap.enabled"), false},
        {QStringLiteral("hyprland.general.snap.border_overlap"), false},
        {QStringLiteral("hyprland.general.snap.monitor_gap"), 10},
        {QStringLiteral("hyprland.general.snap.respect_gaps"), false},
        {QStringLiteral("hyprland.general.snap.window_gap"), 10},
        {QStringLiteral("hyprland.input.follow_mouse"), 1},
        {QStringLiteral("hyprland.input.mouse_refocus"), true},
        {QStringLiteral("hyprland.input.follow_mouse_shrink"), 0},
        {QStringLiteral("hyprland.input.float_switch_override_focus"), 1},
        {QStringLiteral("hyprland.input.focus_on_close"), 0},
        {QStringLiteral("hyprland.input.special_fallthrough"), false},
        {QStringLiteral("hyprland.general.no_focus_fallback"), false},
        {QStringLiteral("hyprland.general.modal_parent_blocking"), true},
        {
            QStringLiteral("hyprland.general.float_gaps"),
            QVariantList{0, 0, 0, 0},
        },
        {QStringLiteral("hyprland.general.gaps_workspaces"), 0},
        {
            QStringLiteral("hyprland.layout.single_window_aspect_ratio"),
            QVariantList{0, 0},
        },
        {
            QStringLiteral(
                "hyprland.layout.single_window_aspect_ratio_tolerance"
            ),
            0.1,
        },
        {QStringLiteral("hyprland.dwindle.default_split_ratio"), 1.0},
        {QStringLiteral("hyprland.dwindle.force_split"), 0},
        {
            QStringLiteral("hyprland.dwindle.permanent_direction_override"),
            false,
        },
        {QStringLiteral("hyprland.dwindle.precise_mouse_move"), false},
        {QStringLiteral("hyprland.dwindle.preserve_split"), false},
        {QStringLiteral("hyprland.dwindle.smart_resizing"), true},
        {QStringLiteral("hyprland.dwindle.smart_split"), false},
        {QStringLiteral("hyprland.dwindle.special_scale_factor"), 1.0},
        {QStringLiteral("hyprland.dwindle.split_bias"), 0},
        {QStringLiteral("hyprland.dwindle.split_width_multiplier"), 1.0},
        {QStringLiteral("hyprland.dwindle.use_active_for_splits"), true},
        {QStringLiteral("hyprland.master.allow_small_split"), false},
        {QStringLiteral("hyprland.master.always_keep_position"), false},
        {QStringLiteral("hyprland.master.center_ignores_reserved"), false},
        {
            QStringLiteral("hyprland.master.center_master_fallback"),
            QStringLiteral("left"),
        },
        {QStringLiteral("hyprland.master.drop_at_cursor"), true},
        {QStringLiteral("hyprland.master.focus_master_on_close"), false},
        {QStringLiteral("hyprland.master.mfact"), 0.55},
        {
            QStringLiteral("hyprland.master.new_on_active"),
            QStringLiteral("none"),
        },
        {QStringLiteral("hyprland.master.new_on_top"), false},
        {
            QStringLiteral("hyprland.master.new_status"),
            QStringLiteral("slave"),
        },
        {
            QStringLiteral("hyprland.master.orientation"),
            QStringLiteral("left"),
        },
        {
            QStringLiteral("hyprland.master.slave_count_for_center_master"),
            2,
        },
        {QStringLiteral("hyprland.master.smart_resizing"), true},
        {QStringLiteral("hyprland.master.special_scale_factor"), 1.0},
        {QStringLiteral("hyprland.scrolling.column_width"), 0.5},
        {
            QStringLiteral("hyprland.scrolling.direction"),
            QStringLiteral("right"),
        },
        {QStringLiteral("hyprland.scrolling.focus_fit_method"), 1},
        {QStringLiteral("hyprland.scrolling.follow_focus"), true},
        {QStringLiteral("hyprland.scrolling.follow_min_visible"), 0.4},
        {
            QStringLiteral("hyprland.scrolling.fullscreen_on_one_column"),
            true,
        },
        {QStringLiteral("hyprland.scrolling.wrap_focus"), true},
        {QStringLiteral("hyprland.scrolling.wrap_swapcol"), true},
        {
            QStringLiteral("hyprland.gestures.scrolling.move_snap_cursor"),
            true,
        },
        {
            QStringLiteral("hyprland.gestures.scrolling.move_snap_to_grid"),
            true,
        },
        {QStringLiteral("hyprland.group.auto_group"), true},
        {QStringLiteral("hyprland.group.insert_after_current"), true},
        {QStringLiteral("hyprland.group.focus_removed_window"), true},
        {QStringLiteral("hyprland.group.drag_into_group"), 1},
        {QStringLiteral("hyprland.group.merge_groups_on_drag"), true},
        {QStringLiteral("hyprland.group.merge_groups_on_groupbar"), true},
        {
            QStringLiteral(
                "hyprland.group.merge_floated_into_tiled_on_groupbar"
            ),
            false,
        },
        {
            QStringLiteral("hyprland.group.group_on_movetoworkspace"),
            false,
        },
        {QStringLiteral("hyprland.group.groupbar.enabled"), true},
        {
            QStringLiteral("hyprland.group.groupbar.disable_when_only"),
            false,
        },
        {QStringLiteral("hyprland.group.groupbar.font_family"), QString{}},
        {
            QStringLiteral("hyprland.group.groupbar.font_weight_active"),
            400,
        },
        {
            QStringLiteral("hyprland.group.groupbar.font_weight_inactive"),
            400,
        },
        {QStringLiteral("hyprland.group.groupbar.font_size"), 8},
        {QStringLiteral("hyprland.group.groupbar.gradients"), false},
        {QStringLiteral("hyprland.group.groupbar.height"), 14},
        {QStringLiteral("hyprland.group.groupbar.indicator_gap"), 0},
        {QStringLiteral("hyprland.group.groupbar.indicator_height"), 3},
        {QStringLiteral("hyprland.group.groupbar.stacked"), false},
        {QStringLiteral("hyprland.group.groupbar.priority"), 3},
        {QStringLiteral("hyprland.group.groupbar.render_titles"), true},
        {QStringLiteral("hyprland.group.groupbar.scrolling"), true},
        {
            QStringLiteral("hyprland.group.groupbar.middle_click_close"),
            true,
        },
        {QStringLiteral("hyprland.group.groupbar.rounding"), 1},
        {QStringLiteral("hyprland.group.groupbar.rounding_power"), 2.0},
        {
            QStringLiteral("hyprland.group.groupbar.gradient_rounding"),
            2,
        },
        {
            QStringLiteral(
                "hyprland.group.groupbar.gradient_rounding_power"
            ),
            2.0,
        },
        {
            QStringLiteral("hyprland.group.groupbar.round_only_edges"),
            true,
        },
        {
            QStringLiteral(
                "hyprland.group.groupbar.gradient_round_only_edges"
            ),
            true,
        },
        {QStringLiteral("hyprland.group.groupbar.gaps_out"), 2},
        {QStringLiteral("hyprland.group.groupbar.gaps_in"), 2},
        {QStringLiteral("hyprland.group.groupbar.keep_upper_gap"), true},
        {QStringLiteral("hyprland.group.groupbar.text_offset"), 0},
        {QStringLiteral("hyprland.group.groupbar.text_padding"), 0},
        {QStringLiteral("hyprland.group.groupbar.blur"), false},
        {QStringLiteral("hyprland.binds.allow_pin_fullscreen"), false},
        {QStringLiteral("hyprland.binds.focus_preferred_method"), 0},
        {QStringLiteral("hyprland.binds.ignore_group_lock"), false},
        {
            QStringLiteral("hyprland.binds.movefocus_cycles_fullscreen"),
            false,
        },
        {
            QStringLiteral("hyprland.binds.movefocus_cycles_groupfirst"),
            false,
        },
        {
            QStringLiteral(
                "hyprland.binds.window_direction_monitor_fallback"
            ),
            true,
        },
        {QStringLiteral("hyprland.misc.enable_anr_dialog"), true},
        {QStringLiteral("hyprland.misc.anr_missed_pings"), 5},
        {QStringLiteral("hyprland.misc.size_limits_tiled"), false},
        {QStringLiteral("hyprland.misc.always_follow_on_dnd"), true},
        {QStringLiteral("hyprland.misc.focus_on_activate"), false},
        {QStringLiteral("hyprland.misc.mouse_move_focuses_monitor"), true},
        {QStringLiteral("hyprland.misc.on_focus_under_fullscreen"), 2},
        {
            QStringLiteral("hyprland.misc.exit_window_retains_fullscreen"),
            false,
        },
        {QStringLiteral("hyprland.misc.enable_swallow"), false},
        {QStringLiteral("hyprland.misc.swallow_regex"), QString{}},
        {
            QStringLiteral("hyprland.misc.swallow_exception_regex"),
            QString{},
        },
        {QStringLiteral("hyprland.input.follow_mouse_threshold"), 0.0},
    };
}

[[nodiscard]] QVariantMap changedGroupbarValues()
{
    return {
        {QStringLiteral("hyprland.group.groupbar.enabled"), false},
        {
            QStringLiteral("hyprland.group.groupbar.disable_when_only"),
            true,
        },
        {
            QStringLiteral("hyprland.group.groupbar.font_family"),
            QStringLiteral("Fira Sans"),
        },
        {
            QStringLiteral("hyprland.group.groupbar.font_weight_active"),
            650,
        },
        {
            QStringLiteral("hyprland.group.groupbar.font_weight_inactive"),
            325,
        },
        {QStringLiteral("hyprland.group.groupbar.font_size"), 17},
        {QStringLiteral("hyprland.group.groupbar.gradients"), true},
        {QStringLiteral("hyprland.group.groupbar.height"), 23},
        {QStringLiteral("hyprland.group.groupbar.indicator_gap"), 4},
        {QStringLiteral("hyprland.group.groupbar.indicator_height"), 5},
        {QStringLiteral("hyprland.group.groupbar.stacked"), true},
        {QStringLiteral("hyprland.group.groupbar.priority"), 6},
        {QStringLiteral("hyprland.group.groupbar.render_titles"), false},
        {QStringLiteral("hyprland.group.groupbar.scrolling"), false},
        {
            QStringLiteral("hyprland.group.groupbar.middle_click_close"),
            false,
        },
        {QStringLiteral("hyprland.group.groupbar.rounding"), 7},
        {
            QStringLiteral("hyprland.group.groupbar.rounding_power"),
            2.573,
        },
        {
            QStringLiteral("hyprland.group.groupbar.gradient_rounding"),
            9,
        },
        {
            QStringLiteral(
                "hyprland.group.groupbar.gradient_rounding_power"
            ),
            3.14159,
        },
        {
            QStringLiteral("hyprland.group.groupbar.round_only_edges"),
            false,
        },
        {
            QStringLiteral(
                "hyprland.group.groupbar.gradient_round_only_edges"
            ),
            false,
        },
        {QStringLiteral("hyprland.group.groupbar.gaps_out"), 8},
        {QStringLiteral("hyprland.group.groupbar.gaps_in"), 9},
        {QStringLiteral("hyprland.group.groupbar.keep_upper_gap"), false},
        {QStringLiteral("hyprland.group.groupbar.text_offset"), -3},
        {QStringLiteral("hyprland.group.groupbar.text_padding"), 6},
        {QStringLiteral("hyprland.group.groupbar.blur"), true},
    };
}

[[nodiscard]] QJsonObject excludedGroupbarVisualOverrides()
{
    const auto gradient = [](const QString &first,
                             const QString &second,
                             const int angle) {
        return QJsonObject{
            {QStringLiteral("colors"), QJsonArray{first, second}},
            {QStringLiteral("angle"), angle},
        };
    };
    return {
        {
            QStringLiteral("hyprland.group.groupbar.col.active"),
            gradient(
                QStringLiteral("0xFF102030"),
                QStringLiteral("0xFF405060"),
                11
            ),
        },
        {
            QStringLiteral("hyprland.group.groupbar.col.inactive"),
            gradient(
                QStringLiteral("0xFF112233"),
                QStringLiteral("0xFF445566"),
                22
            ),
        },
        {
            QStringLiteral("hyprland.group.groupbar.col.locked_active"),
            gradient(
                QStringLiteral("0xFF213243"),
                QStringLiteral("0xFF546576"),
                33
            ),
        },
        {
            QStringLiteral("hyprland.group.groupbar.col.locked_inactive"),
            gradient(
                QStringLiteral("0xFF314253"),
                QStringLiteral("0xFF647586"),
                44
            ),
        },
        {
            QStringLiteral("hyprland.group.groupbar.text_color"),
            QStringLiteral("0xFF718293"),
        },
        {
            QStringLiteral("hyprland.group.groupbar.text_color_inactive"),
            QStringLiteral("0xFF8293A4"),
        },
        {
            QStringLiteral("hyprland.group.groupbar.text_color_locked_active"),
            QStringLiteral("0xFF93A4B5"),
        },
        {
            QStringLiteral(
                "hyprland.group.groupbar.text_color_locked_inactive"
            ),
            QStringLiteral("0xFFA4B5C6"),
        },
    };
}

[[nodiscard]] QVariantMap workspacesDefaults()
{
    return {
        {QStringLiteral("hyprland.animations.workspace_wraparound"), false},
        {
            QStringLiteral("hyprland.gestures.workspace_swipe_cancel_ratio"),
            0.5,
        },
        {
            QStringLiteral("hyprland.gestures.workspace_swipe_create_new"),
            true,
        },
        {
            QStringLiteral("hyprland.gestures.workspace_swipe_direction_lock"),
            true,
        },
        {
            QStringLiteral(
                "hyprland.gestures.workspace_swipe_direction_lock_threshold"
            ),
            10,
        },
        {QStringLiteral("hyprland.gestures.workspace_swipe_distance"), 300},
        {QStringLiteral("hyprland.gestures.workspace_swipe_forever"), false},
        {QStringLiteral("hyprland.gestures.workspace_swipe_invert"), true},
        {
            QStringLiteral(
                "hyprland.gestures.workspace_swipe_min_speed_to_force"
            ),
            30,
        },
        {QStringLiteral("hyprland.gestures.workspace_swipe_touch"), false},
        {
            QStringLiteral("hyprland.gestures.workspace_swipe_touch_invert"),
            false,
        },
        {QStringLiteral("hyprland.gestures.workspace_swipe_use_r"), false},
        {QStringLiteral("hyprland.misc.close_special_on_empty"), true},
        {QStringLiteral("hyprland.misc.initial_workspace_tracking"), 1},
        {
            QStringLiteral("hyprland.misc.initial_workspace_token_timeout"),
            10,
        },
        {QStringLiteral("hyprland.binds.allow_workspace_cycles"), false},
        {
            QStringLiteral("hyprland.binds.hide_special_on_workspace_change"),
            false,
        },
        {QStringLiteral("hyprland.binds.workspace_back_and_forth"), false},
        {QStringLiteral("hyprland.binds.workspace_center_on"), 1},
        {QStringLiteral("hyprland.cursor.warp_on_change_workspace"), 0},
        {QStringLiteral("hyprland.cursor.warp_on_toggle_special"), 0},
    };
}

[[nodiscard]] QVariantMap changedWorkspacesValues()
{
    auto values = workspacesDefaults();
    values.insert(
        QStringLiteral("hyprland.animations.workspace_wraparound"), true
    );
    values.insert(
        QStringLiteral("hyprland.gestures.workspace_swipe_cancel_ratio"),
        0.37
    );
    values.insert(
        QStringLiteral("hyprland.gestures.workspace_swipe_create_new"), false
    );
    values.insert(
        QStringLiteral("hyprland.gestures.workspace_swipe_direction_lock"),
        false
    );
    values.insert(
        QStringLiteral(
            "hyprland.gestures.workspace_swipe_direction_lock_threshold"
        ),
        25
    );
    values.insert(
        QStringLiteral("hyprland.gestures.workspace_swipe_distance"), 500
    );
    values.insert(
        QStringLiteral("hyprland.gestures.workspace_swipe_forever"), true
    );
    values.insert(
        QStringLiteral("hyprland.gestures.workspace_swipe_invert"), false
    );
    values.insert(
        QStringLiteral(
            "hyprland.gestures.workspace_swipe_min_speed_to_force"
        ),
        44
    );
    values.insert(
        QStringLiteral("hyprland.gestures.workspace_swipe_touch"), true
    );
    values.insert(
        QStringLiteral("hyprland.gestures.workspace_swipe_touch_invert"), true
    );
    values.insert(
        QStringLiteral("hyprland.gestures.workspace_swipe_use_r"), true
    );
    values.insert(QStringLiteral("hyprland.misc.close_special_on_empty"), false);
    values.insert(
        QStringLiteral("hyprland.misc.initial_workspace_tracking"), 2.0
    );
    values.insert(
        QStringLiteral("hyprland.misc.initial_workspace_token_timeout"), 19
    );
    values.insert(
        QStringLiteral("hyprland.binds.allow_workspace_cycles"), true
    );
    values.insert(
        QStringLiteral("hyprland.binds.hide_special_on_workspace_change"), true
    );
    values.insert(
        QStringLiteral("hyprland.binds.workspace_back_and_forth"), true
    );
    values.insert(QStringLiteral("hyprland.binds.workspace_center_on"), 0);
    values.insert(
        QStringLiteral("hyprland.cursor.warp_on_change_workspace"), 1
    );
    values.insert(
        QStringLiteral("hyprland.cursor.warp_on_toggle_special"), 2
    );
    return values;
}

[[nodiscard]] QVariantMap advancedDefaults()
{
    return {
        {
            QStringLiteral("hyprland.misc.allow_session_lock_restore"),
            false,
        },
        {QStringLiteral("hyprland.misc.lockdead_screen_delay"), 1000},
        {
            QStringLiteral("hyprland.misc.disable_scale_notification"),
            false,
        },
        {QStringLiteral("hyprland.misc.render_unfocused_fps"), 15},
        {QStringLiteral("hyprland.misc.screencopy_force_8b"), true},
        {QStringLiteral("hyprland.misc.disable_hyprland_logo"), false},
        {QStringLiteral("hyprland.misc.disable_splash_rendering"), false},
        {QStringLiteral("hyprland.misc.session_lock_xray"), false},
        {QStringLiteral("hyprland.misc.session_lock_blur"), false},
        {
            QStringLiteral("hyprland.xwayland.use_nearest_neighbor"),
            true,
        },
        {
            QStringLiteral("hyprland.render.expand_undersized_textures"),
            true,
        },
        {QStringLiteral("hyprland.render.direct_scanout"), 0},
        {QStringLiteral("hyprland.render.fp16_sdr_tf"), 0},
        {QStringLiteral("hyprland.render.xp_mode"), false},
        {
            QStringLiteral("hyprland.input-capture.capture_modifiers"),
            false,
        },
        {
            QStringLiteral("hyprland.input-capture.enforce_barriers"),
            true,
        },
    };
}

[[nodiscard]] QVariantMap changedAdvancedValues()
{
    return {
        {
            QStringLiteral("hyprland.misc.allow_session_lock_restore"),
            true,
        },
        {QStringLiteral("hyprland.misc.lockdead_screen_delay"), 2750},
        {
            QStringLiteral("hyprland.misc.disable_scale_notification"),
            true,
        },
        {QStringLiteral("hyprland.misc.render_unfocused_fps"), 37},
        {QStringLiteral("hyprland.misc.screencopy_force_8b"), false},
        {QStringLiteral("hyprland.misc.disable_hyprland_logo"), true},
        {QStringLiteral("hyprland.misc.disable_splash_rendering"), true},
        {QStringLiteral("hyprland.misc.session_lock_xray"), true},
        {QStringLiteral("hyprland.misc.session_lock_blur"), true},
        {
            QStringLiteral("hyprland.xwayland.use_nearest_neighbor"),
            false,
        },
        {
            QStringLiteral("hyprland.render.expand_undersized_textures"),
            false,
        },
        {QStringLiteral("hyprland.render.direct_scanout"), 2},
        {QStringLiteral("hyprland.render.fp16_sdr_tf"), 1},
        {QStringLiteral("hyprland.render.xp_mode"), true},
        {
            QStringLiteral("hyprland.input-capture.capture_modifiers"),
            true,
        },
        {
            QStringLiteral("hyprland.input-capture.enforce_barriers"),
            false,
        },
    };
}

[[nodiscard]] QJsonObject unauthoredRenderAndXWaylandOverrides()
{
    return {
        {QStringLiteral("hyprland.render.cm_auto_hdr"), 2},
        {QStringLiteral("hyprland.render.cm_enabled"), false},
        {
            QStringLiteral("hyprland.render.cm_sdr_eotf"),
            QStringLiteral("gamma22"),
        },
        {QStringLiteral("hyprland.render.commit_timing_enabled"), false},
        {QStringLiteral("hyprland.render.ctm_animation"), 1},
        {QStringLiteral("hyprland.render.icc_vcgt_enabled"), false},
        {QStringLiteral("hyprland.render.keep_unmodified_copy"), 1},
        {QStringLiteral("hyprland.render.new_render_scheduling"), true},
        {QStringLiteral("hyprland.render.non_shader_cm"), 1},
        {QStringLiteral("hyprland.render.non_shader_cm_interop"), 1},
        {QStringLiteral("hyprland.render.send_content_type"), false},
        {QStringLiteral("hyprland.render.use_fp16"), 1},
        {QStringLiteral("hyprland.render.use_shader_blur_blend"), true},
        {QStringLiteral("hyprland.xwayland.create_abstract_socket"), true},
        {QStringLiteral("hyprland.xwayland.enabled"), false},
        {QStringLiteral("hyprland.xwayland.force_zero_scaling"), true},
    };
}

[[nodiscard]] HyprShelld::CompositorOptionCatalog trustedCatalog()
{
    const auto parsed = HyprShelld::Hyprland::parseCatalog(
        readBytes(QStringLiteral(HYPRSHELLD_HYPRLAND_CATALOG_FILE))
    );
    if (!parsed) return {};
    const auto bytes = HyprShelld::Hyprland::canonicalCatalogJson(
        *parsed.value
    );
    QString error;
    const auto catalog = HyprShelld::CompositorOptionCatalog::fromBytes(
        bytes,
        parsed.value->digest,
        parsed.value->digest,
        error
    );
    return catalog ? *catalog : HyprShelld::CompositorOptionCatalog{};
}

[[nodiscard]] HyprShelld::CompositorActionCatalog trustedActionCatalog()
{
    const auto actionBytes = readBytes(
        QStringLiteral(HYPRSHELLD_HYPRLAND_ACTION_CATALOG_FILE)
    );
    const auto schemaBytes = readBytes(
        QStringLiteral(HYPRSHELLD_HYPRLAND_CONFIG_SCHEMA_FILE)
    );
    const auto parsed = HyprShelld::Hyprland::parseActionCatalog(
        actionBytes, schemaBytes
    );
    if (!parsed) return {};
    const auto canonical = HyprShelld::Hyprland::canonicalActionCatalogJson(
        *parsed.value
    );
    const auto schemaDigest = QString::fromLatin1(
        QCryptographicHash::hash(
            schemaBytes, QCryptographicHash::Sha256
        ).toHex()
    );
    QString error;
    const auto catalog = HyprShelld::CompositorActionCatalog::fromBytes(
        canonical,
        parsed.value->digest,
        parsed.value->digest,
        schemaBytes,
        schemaDigest,
        error
    );
    return catalog ? *catalog : HyprShelld::CompositorActionCatalog{};
}

[[nodiscard]] QJsonObject baselineSnapshot()
{
    auto object = readObject(
        QStringLiteral(HYPRSHELLD_HYPRLAND_DEFAULTS_FILE)
    );
    object.insert(QStringLiteral("revision"), QStringLiteral("7"));
    object.insert(
        QStringLiteral("environment"),
        QJsonArray{QJsonObject{
            {QStringLiteral("id"), QStringLiteral("environment-one")},
            {QStringLiteral("name"), QStringLiteral("EXAMPLE")},
            {QStringLiteral("value"), QStringLiteral("preserved")},
            {QStringLiteral("scope"), QStringLiteral("hyprland")},
        }}
    );
    object.insert(
        QStringLiteral("overrides"),
        QJsonObject{
            {QStringLiteral("hyprland.misc.disable_hyprland_logo"), true},
            {QStringLiteral("hyprland.animations.enabled"), false},
        }
    );
    return object;
}

[[nodiscard]] QVariantList userWorkspaceRules(const QJsonObject &snapshot)
{
    auto rules = snapshot.value(QStringLiteral("workspaceRules")).toArray();
    if (!rules.isEmpty()) rules.removeLast();
    return rules.toVariantList();
}

[[nodiscard]] QVariantList snapshotGestures(const QJsonObject &snapshot)
{
    return snapshot.value(QStringLiteral("gestures")).toArray().toVariantList();
}

[[nodiscard]] QVariantMap gestureRecord(
    const QString &id,
    const int fingers,
    const QString &direction,
    const QVariantMap &action,
    const QVariantList &modifiers = {},
    const double scale = 1.0,
    const bool disableInhibit = false
)
{
    return {
        {QStringLiteral("id"), id},
        {QStringLiteral("fingers"), fingers},
        {QStringLiteral("direction"), direction},
        {QStringLiteral("modifiers"), modifiers},
        {QStringLiteral("scale"), scale},
        {QStringLiteral("disableInhibit"), disableInhibit},
        {QStringLiteral("action"), action},
    };
}

[[nodiscard]] QVariantMap workspaceRule(
    const QString &id,
    const QString &selector,
    const QVariantMap &overrides = {}
)
{
    return {
        {QStringLiteral("id"), id},
        {QStringLiteral("selector"), selector},
        {QStringLiteral("enabled"), false},
        {QStringLiteral("monitor"), QStringLiteral("desc:Workspace display")},
        {QStringLiteral("persistent"), true},
        {QStringLiteral("isDefault"), true},
        {QStringLiteral("layout"), QStringLiteral("scrolling")},
        {QStringLiteral("overrides"), overrides},
    };
}

[[nodiscard]] QVariantMap completeWorkspaceRuleOverrides()
{
    constexpr auto safeInteger = 9007199254740991.0;
    return {
        {QStringLiteral("gaps_in"),
         QVariantList{-safeInteger, 0.0, 1.0, safeInteger}},
        {QStringLiteral("gaps_out"), QVariantList{1, 2, 3, 4}},
        {QStringLiteral("float_gaps"), QVariantList{4, 3, 2, 1}},
        {QStringLiteral("border_size"), safeInteger},
        {QStringLiteral("no_border"), true},
        {QStringLiteral("no_rounding"), false},
        {QStringLiteral("decorate"), true},
        {QStringLiteral("no_shadow"), false},
        {QStringLiteral("default_name"), QStringLiteral("Authored workspace")},
        {QStringLiteral("animation"), QStringLiteral("slidefadevert left 37%")},
        {QStringLiteral("layout_opts"), QVariantMap{
             {QStringLiteral("orientation"), QStringLiteral("center")},
             {QStringLiteral("direction"), QStringLiteral("up")},
         }},
    };
}

[[nodiscard]] QJsonObject bindingOptions()
{
    return {
        {QStringLiteral("repeating"), false},
        {QStringLiteral("locked"), false},
        {QStringLiteral("release"), false},
        {QStringLiteral("nonConsuming"), false},
        {QStringLiteral("autoConsuming"), false},
        {QStringLiteral("transparent"), false},
        {QStringLiteral("ignoreMods"), false},
        {QStringLiteral("dontInhibit"), false},
        {QStringLiteral("longPress"), false},
        {QStringLiteral("submapUniversal"), false},
        {QStringLiteral("click"), false},
        {QStringLiteral("drag"), false},
        {QStringLiteral("allowInputCapture"), false},
    };
}

[[nodiscard]] QJsonObject dispatcherBinding(
    const QString &id,
    const QString &key,
    const QString &submap = {}
)
{
    return {
        {QStringLiteral("id"), id},
        {QStringLiteral("modifiers"), QJsonArray{QStringLiteral("super")}},
        {QStringLiteral("key"), key},
        {QStringLiteral("actionType"), QStringLiteral("dispatcher")},
        {QStringLiteral("action"), QStringLiteral("cursor.move")},
        {QStringLiteral("arguments"), QJsonObject{
            {QStringLiteral("x"), 10},
            {QStringLiteral("y"), 20},
        }},
        {QStringLiteral("description"), QStringLiteral("Move the cursor")},
        {QStringLiteral("enabled"), true},
        {QStringLiteral("submap"), submap},
        {QStringLiteral("options"), bindingOptions()},
    };
}

[[nodiscard]] QJsonObject snapshotWithValidComplexSurfaces()
{
    auto object = baselineSnapshot();
    object.insert(QStringLiteral("monitors"), QJsonArray{QJsonObject{
        {QStringLiteral("id"), QStringLiteral("monitor-one")},
        {QStringLiteral("selector"), QStringLiteral("DP-1")},
        {QStringLiteral("enabled"), true},
        {QStringLiteral("mode"), QStringLiteral("preferred")},
        {QStringLiteral("position"), QStringLiteral("auto")},
        {QStringLiteral("scale"), 1.0},
        {QStringLiteral("reserved"), QJsonArray{1, 2, 3, 4}},
        {QStringLiteral("transform"), 0},
        {QStringLiteral("mirror"), QString()},
        {QStringLiteral("bitdepth"), 8},
        {QStringLiteral("cm"), QStringLiteral("auto")},
        {QStringLiteral("sdrEotf"), QStringLiteral("default")},
        {QStringLiteral("sdrBrightness"), 1.0},
        {QStringLiteral("sdrSaturation"), 1.0},
        {QStringLiteral("vrr"), 0},
        {QStringLiteral("icc"), QString()},
        {QStringLiteral("supportsWideColor"), 0},
        {QStringLiteral("supportsHdr"), 0},
        {QStringLiteral("sdrMinLuminance"), 0.2},
        {QStringLiteral("sdrMaxLuminance"), 80},
        {QStringLiteral("minLuminance"), -1.0},
        {QStringLiteral("maxLuminance"), -1},
        {QStringLiteral("maxAvgLuminance"), -1},
    }});
    object.insert(QStringLiteral("devices"), QJsonArray{QJsonObject{
        {QStringLiteral("id"), QStringLiteral("pointer-main")},
        {QStringLiteral("selector"), QStringLiteral("Main Pointer")},
        {QStringLiteral("kind"), QStringLiteral("pointer")},
        {QStringLiteral("enabled"), true},
        {QStringLiteral("overrides"), QJsonObject{
            {QStringLiteral("sensitivity"), 0.25},
        }},
    }});
    object.insert(QStringLiteral("curves"), QJsonArray{QJsonObject{
        {QStringLiteral("id"), QStringLiteral("curve-bezier")},
        {QStringLiteral("name"), QStringLiteral("ease-custom")},
        {QStringLiteral("type"), QStringLiteral("bezier")},
        {QStringLiteral("points"), QJsonArray{
            QJsonArray{0.2, 0.0}, QJsonArray{0.8, 1.0},
        }},
    }});
    object.insert(QStringLiteral("animations"), QJsonArray{QJsonObject{
        {QStringLiteral("id"), QStringLiteral("animation-windows")},
        {QStringLiteral("name"), QStringLiteral("windows")},
        {QStringLiteral("enabled"), true},
        {QStringLiteral("speed"), 6.0},
        {QStringLiteral("curve"), QStringLiteral("ease-custom")},
        {QStringLiteral("style"), QStringLiteral("slide")},
    }});
    object.insert(QStringLiteral("gestures"), QJsonArray{QJsonObject{
        {QStringLiteral("id"), QStringLiteral("gesture-special")},
        {QStringLiteral("fingers"), 4},
        {QStringLiteral("direction"), QStringLiteral("left")},
        {QStringLiteral("modifiers"), QJsonArray{QStringLiteral("super")}},
        {QStringLiteral("scale"), 1.25},
        {QStringLiteral("disableInhibit"), false},
        {QStringLiteral("action"), QJsonObject{
            {QStringLiteral("type"), QStringLiteral("special")},
            {QStringLiteral("workspace"), QStringLiteral("magic")},
        }},
    }});
    auto workspaceRules = object.value(
        QStringLiteral("workspaceRules")
    ).toArray();
    workspaceRules.insert(0, QJsonObject{
        {QStringLiteral("id"), QStringLiteral("workspace-one")},
        {QStringLiteral("selector"), QStringLiteral("1")},
        {QStringLiteral("enabled"), true},
        {QStringLiteral("monitor"), QString()},
        {QStringLiteral("persistent"), true},
        {QStringLiteral("isDefault"), false},
        {QStringLiteral("layout"), QStringLiteral("dwindle")},
        {QStringLiteral("overrides"), QJsonObject{
            {QStringLiteral("gaps_in"), QJsonArray{5, 6, 7, 8}},
            {QStringLiteral("gaps_out"), QJsonArray{9, 10, 11, 12}},
            {QStringLiteral("float_gaps"), QJsonArray{13, 14, 15, 16}},
        }},
    });
    object.insert(QStringLiteral("workspaceRules"), workspaceRules);
    object.insert(QStringLiteral("windowRules"), QJsonArray{QJsonObject{
        {QStringLiteral("id"), QStringLiteral("window-browser")},
        {QStringLiteral("name"), QStringLiteral("Browser")},
        {QStringLiteral("enabled"), true},
        {QStringLiteral("match"), QJsonObject{
            {QStringLiteral("class"), QStringLiteral("^(firefox)$")},
        }},
        {QStringLiteral("effects"), QJsonObject{
            {QStringLiteral("float"), true},
            {QStringLiteral("rounding"), 4},
        }},
    }});
    object.insert(QStringLiteral("layerRules"), QJsonArray{QJsonObject{
        {QStringLiteral("id"), QStringLiteral("layer-panel")},
        {QStringLiteral("name"), QStringLiteral("Panel")},
        {QStringLiteral("enabled"), true},
        {QStringLiteral("match"), QJsonObject{
            {QStringLiteral("namespace"), QStringLiteral("^panel$")},
        }},
        {QStringLiteral("effects"), QJsonObject{
            {QStringLiteral("ignore_alpha"), 0.5},
            {QStringLiteral("above_lock"), 1},
        }},
    }});
    object.insert(QStringLiteral("submaps"), QJsonArray{QJsonObject{
        {QStringLiteral("id"), QStringLiteral("resize-submap")},
        {QStringLiteral("name"), QStringLiteral("resize")},
        {QStringLiteral("reset"), QString()},
        {QStringLiteral("enabled"), true},
    }});
    object.insert(QStringLiteral("bindings"), QJsonArray{
        dispatcherBinding(QStringLiteral("move-cursor"), QStringLiteral("F7")),
        dispatcherBinding(
            QStringLiteral("resize-move"),
            QStringLiteral("F8"),
            QStringLiteral("resize")
        ),
    });
    object.insert(QStringLiteral("permissions"), QJsonArray{QJsonObject{
        {QStringLiteral("id"), QStringLiteral("permission-one")},
        {QStringLiteral("binary"), QStringLiteral("^/usr/bin/foo$")},
        {QStringLiteral("type"), QStringLiteral("screencopy")},
        {QStringLiteral("mode"), QStringLiteral("deny")},
    }});
    object.insert(QStringLiteral("environment"), QJsonArray{QJsonObject{
        {QStringLiteral("id"), QStringLiteral("cursor-size")},
        {QStringLiteral("name"), QStringLiteral("XCURSOR_SIZE")},
        {QStringLiteral("value"), QStringLiteral("24")},
        {QStringLiteral("scope"), QStringLiteral("hyprland")},
    }});
    return object;
}

[[nodiscard]] QJsonObject fullWindowRule(
    const QString &id = QStringLiteral("window-full"),
    const QString &name = QStringLiteral("Full window rule"),
    const double borderSize = 9007199254740991.0
)
{
    return {
        {QStringLiteral("id"), id},
        {QStringLiteral("name"), name},
        {QStringLiteral("enabled"), true},
        {QStringLiteral("match"), QJsonObject{
            {QStringLiteral("class"), QStringLiteral("^(firefox)$")},
            {QStringLiteral("title"), QStringLiteral("negative:^Private")},
            {QStringLiteral("initial_class"), QStringLiteral("^firefox$")},
            {QStringLiteral("initial_title"), QStringLiteral("^Home$")},
            {QStringLiteral("float"), false},
            {QStringLiteral("tag"), QStringLiteral("browser")},
            {QStringLiteral("xwayland"), false},
            {QStringLiteral("fullscreen"), false},
            {QStringLiteral("pin"), false},
            {QStringLiteral("focus"), true},
            {QStringLiteral("group"), false},
            {QStringLiteral("modal"), false},
            {QStringLiteral("fullscreen_state_internal"), 1},
            {QStringLiteral("fullscreen_state_client"), 2},
            {QStringLiteral("workspace"), QStringLiteral("name:web")},
            {QStringLiteral("content"), QStringLiteral("^browser$")},
            {QStringLiteral("xdg_tag"), QStringLiteral("^browser$")},
            {QStringLiteral("namespace"), QStringLiteral("^firefox$")},
        }},
        {QStringLiteral("effects"), QJsonObject{
            {QStringLiteral("float"), true},
            {QStringLiteral("tile"), false},
            {QStringLiteral("fullscreen"), false},
            {QStringLiteral("maximize"), true},
            {QStringLiteral("center"), true},
            {QStringLiteral("pseudo"), false},
            {QStringLiteral("no_initial_focus"), false},
            {QStringLiteral("pin"), false},
            {QStringLiteral("fullscreen_state"), QJsonObject{
                {QStringLiteral("internal"), 1},
                {QStringLiteral("client"), 2},
            }},
            {QStringLiteral("move"), QJsonArray{-1000000, 1000000}},
            {QStringLiteral("size"), QJsonArray{800, 600}},
            {QStringLiteral("monitor"), QJsonObject{
                {QStringLiteral("target"), QStringLiteral("DP-1")},
                {QStringLiteral("silent"), true},
            }},
            {QStringLiteral("workspace"), QJsonObject{
                {QStringLiteral("target"), QStringLiteral("1")},
                {QStringLiteral("silent"), false},
            }},
            {QStringLiteral("suppress_event"), QJsonArray{
                QStringLiteral("fullscreen"),
                QStringLiteral("maximize"),
                QStringLiteral("activate"),
                QStringLiteral("activatefocus"),
                QStringLiteral("fullscreenoutput"),
                QStringLiteral("x11configurerequest"),
            }},
            {QStringLiteral("content"), QStringLiteral("game")},
            {QStringLiteral("no_close_for"), 2147483647},
            {QStringLiteral("scrolling_width"), 0.375},
            {QStringLiteral("rounding"), 20},
            {QStringLiteral("border_size"), borderSize},
            {QStringLiteral("rounding_power"), 10.0},
            {QStringLiteral("scroll_mouse"), 0.01},
            {QStringLiteral("scroll_touchpad"), 10.0},
            {QStringLiteral("animation"), QStringLiteral("slide left")},
            {QStringLiteral("idle_inhibit"), QStringLiteral("fullscreen")},
            {QStringLiteral("opacity"), QJsonObject{
                {QStringLiteral("active"), 0.9},
                {QStringLiteral("inactive"), 0.7},
                {QStringLiteral("fullscreen"), 1.0},
                {QStringLiteral("overrideActive"), true},
                {QStringLiteral("overrideInactive"), false},
                {QStringLiteral("overrideFullscreen"), true},
            }},
            {QStringLiteral("tag"), QStringLiteral("+managed")},
            {QStringLiteral("max_size"), QJsonArray{1920, 1080}},
            {QStringLiteral("min_size"), QJsonArray{320, 200}},
            {QStringLiteral("border_color"), QJsonObject{
                {QStringLiteral("colors"), QJsonArray{
                    QStringLiteral("0xFF112233"),
                    QStringLiteral("0xFF445566"),
                }},
                {QStringLiteral("angle"), 37.0},
            }},
            {QStringLiteral("persistent_size"), true},
            {QStringLiteral("allows_input"), true},
            {QStringLiteral("dim_around"), false},
            {QStringLiteral("decorate"), true},
            {QStringLiteral("focus_on_activate"), true},
            {QStringLiteral("keep_aspect_ratio"), true},
            {QStringLiteral("nearest_neighbor"), false},
            {QStringLiteral("no_anim"), false},
            {QStringLiteral("no_blur"), false},
            {QStringLiteral("no_dim"), false},
            {QStringLiteral("no_focus"), false},
            {QStringLiteral("no_follow_mouse"), false},
            {QStringLiteral("no_max_size"), false},
            {QStringLiteral("no_shadow"), false},
            {QStringLiteral("no_shortcuts_inhibit"), false},
            {QStringLiteral("opaque"), false},
            {QStringLiteral("force_rgbx"), false},
            {QStringLiteral("sync_fullscreen"), true},
            {QStringLiteral("immediate"), false},
            {QStringLiteral("xray"), false},
            {QStringLiteral("render_unfocused"), false},
            {QStringLiteral("no_screen_share"), false},
            {QStringLiteral("no_vrr"), false},
            {QStringLiteral("no_auto_hdr"), false},
            {QStringLiteral("stay_focused"), false},
            {QStringLiteral("confine_pointer"), false},
            {QStringLiteral("tonemap"), QStringLiteral("limited")},
        }},
    };
}

[[nodiscard]] QJsonObject fullLayerRule(
    const QString &id = QStringLiteral("layer-full"),
    const QString &name = QStringLiteral("Full layer rule"),
    const double order = -9007199254740991.0
)
{
    return {
        {QStringLiteral("id"), id},
        {QStringLiteral("name"), name},
        {QStringLiteral("enabled"), false},
        {QStringLiteral("match"), QJsonObject{
            {QStringLiteral("namespace"), QStringLiteral("negative:^launcher$")},
        }},
        {QStringLiteral("effects"), QJsonObject{
            {QStringLiteral("no_anim"), false},
            {QStringLiteral("blur"), true},
            {QStringLiteral("blur_popups"), true},
            {QStringLiteral("ignore_alpha"), 0.375},
            {QStringLiteral("dim_around"), false},
            {QStringLiteral("xray"), false},
            {QStringLiteral("animation"), QStringLiteral("slide top")},
            {QStringLiteral("order"), order},
            {QStringLiteral("above_lock"), 2},
            {QStringLiteral("no_screen_share"), true},
        }},
    };
}

} // namespace

class CompositorSettingsHelpersTest final : public QObject {
    Q_OBJECT

private slots:
    void projectsSavedAndObservedInputDevicesWithoutFuzzyMatching()
    {
        using HyprShelld::Hyprland::ConnectedInputDevice;
        using HyprShelld::Hyprland::ConnectedInputDeviceInventory;
        using HyprShelld::Hyprland::ConnectedInputDeviceKind;
        using HyprShelld::Hyprland::DeviceConfiguration;

        QVector<DeviceConfiguration> saved{
            {
                .id = QStringLiteral("device:keyboard"),
                .selector = QStringLiteral("exact keyboard"),
                .kind = QStringLiteral("keyboard"),
                .enabled = false,
                .overrides = QJsonObject{
                    {QStringLiteral("kb_layout"), QStringLiteral("us")},
                },
            },
            {
                .id = QStringLiteral("device:touchpad"),
                .selector = QStringLiteral("touchpad one"),
                .kind = QStringLiteral("touchpad"),
                .enabled = true,
                .overrides = {},
            },
            {
                .id = QStringLiteral("device:mismatch"),
                .selector = QStringLiteral("mismatch"),
                .kind = QStringLiteral("keyboard"),
                .enabled = true,
                .overrides = {},
            },
            {
                .id = QStringLiteral("device:uppercase"),
                .selector = QStringLiteral("Upper Case"),
                .kind = QStringLiteral("pointer"),
                .enabled = true,
                .overrides = {},
            },
            {
                .id = QStringLiteral("device:tool"),
                .selector = QStringLiteral("tablet tool"),
                .kind = QStringLiteral("tabletTool"),
                .enabled = true,
                .overrides = {},
            },
        };
        ConnectedInputDeviceInventory inventory{
            .records = {
                ConnectedInputDevice{
                    .sessionSelector = QStringLiteral("exact-keyboard"),
                    .observedKind = ConnectedInputDeviceKind::Keyboard,
                    .activeKeymap = QStringLiteral("English (US)"),
                },
                ConnectedInputDevice{
                    .sessionSelector = QStringLiteral("touchpad-one"),
                    .observedKind = ConnectedInputDeviceKind::Pointer,
                },
                ConnectedInputDevice{
                    .sessionSelector = QStringLiteral("mismatch"),
                    .observedKind = ConnectedInputDeviceKind::Touch,
                },
                ConnectedInputDevice{
                    .sessionSelector = QStringLiteral("upper-case"),
                    .observedKind = ConnectedInputDeviceKind::Pointer,
                },
                ConnectedInputDevice{
                    .sessionSelector = QStringLiteral("live-only"),
                    .observedKind = ConnectedInputDeviceKind::Tablet,
                },
            },
            .inventoryDigest = QString(64, QLatin1Char('a')),
        };

        const auto projection = HyprShelld::projectInputDevices(saved, inventory);
        QCOMPARE(projection.savedDevices.size(), 5);
        QCOMPARE(projection.otherSavedDevices.size(), 3);
        QCOMPARE(projection.connectedDevices.size(), 5);

        const auto keyboard = projection.savedDevices.at(0).toMap();
        QCOMPARE(keyboard.value(QStringLiteral("matchState")).toString(),
                 QStringLiteral("observed"));
        QCOMPARE(keyboard.value(QStringLiteral("overrideCount")).toInt(), 1);
        QVERIFY(!keyboard.contains(QStringLiteral("overrides")));
        const auto touchpad = projection.savedDevices.at(1).toMap();
        QCOMPARE(touchpad.value(QStringLiteral("matchState")).toString(),
                 QStringLiteral("observed"));
        QCOMPARE(touchpad.value(QStringLiteral("observedKind")).toString(),
                 QStringLiteral("pointer"));
        QCOMPARE(projection.savedDevices.at(2).toMap()
                     .value(QStringLiteral("matchState")).toString(),
                 QStringLiteral("kind-mismatch"));
        QCOMPARE(projection.savedDevices.at(3).toMap()
                     .value(QStringLiteral("matchState")).toString(),
                 QStringLiteral("not-observed"));
        QCOMPARE(projection.savedDevices.at(4).toMap()
                     .value(QStringLiteral("matchState")).toString(),
                 QStringLiteral("unobservable"));

        const auto observedKeyboard = projection.connectedDevices.at(0).toMap();
        QCOMPARE(observedKeyboard.value(QStringLiteral("savedSettingsState")).toString(),
                 QStringLiteral("matched"));
        QCOMPARE(observedKeyboard.value(QStringLiteral("activeKeymap")).toString(),
                 QStringLiteral("English (US)"));
        QCOMPARE(projection.connectedDevices.at(2).toMap()
                     .value(QStringLiteral("savedSettingsState")).toString(),
                 QStringLiteral("kind-mismatch"));
        QCOMPARE(projection.connectedDevices.at(3).toMap()
                     .value(QStringLiteral("savedSettingsState")).toString(),
                 QStringLiteral("not-saved"));
        QCOMPARE(projection.connectedDevices.at(4).toMap()
                     .value(QStringLiteral("savedSettingsState")).toString(),
                 QStringLiteral("not-saved"));

        const auto savedOnly = HyprShelld::projectInputDevices(saved, std::nullopt);
        QCOMPARE(savedOnly.connectedDevices.size(), 0);
        QCOMPARE(savedOnly.otherSavedDevices.size(), 5);
        for (const auto &value : savedOnly.savedDevices) {
            QCOMPARE(value.toMap().value(QStringLiteral("matchState")).toString(),
                     QStringLiteral("inventory-unavailable"));
        }

        const auto liveOnly = HyprShelld::projectInputDevices(std::nullopt, inventory);
        QCOMPARE(liveOnly.savedDevices.size(), 0);
        QCOMPARE(liveOnly.otherSavedDevices.size(), 0);
        QCOMPARE(liveOnly.connectedDevices.size(), 5);
        for (const auto &value : liveOnly.connectedDevices) {
            const auto row = value.toMap();
            QCOMPARE(row.value(QStringLiteral("savedSettingsState")).toString(),
                     QStringLiteral("unavailable"));
            QVERIFY(row.value(QStringLiteral("savedDeviceId")).isNull());
        }
    }

    void authenticatesCatalogAndQualifiesAllReviewedGroupsIndependently()
    {
        const auto parsed = HyprShelld::Hyprland::parseCatalog(
            readBytes(QStringLiteral(HYPRSHELLD_HYPRLAND_CATALOG_FILE))
        );
        QVERIFY(parsed);
        const auto canonical = HyprShelld::Hyprland::canonicalCatalogJson(
            *parsed.value
        );
        QString error;
        const auto accepted =
            HyprShelld::CompositorOptionCatalog::fromBytes(
                canonical,
                parsed.value->digest,
                parsed.value->digest,
                error
            );
        QVERIFY2(accepted, qPrintable(error));
        QVERIFY(accepted->appearanceContractAvailable());
        QVERIFY(accepted->appearanceContractError().isEmpty());
        QVERIFY(accepted->inputContractAvailable());
        QVERIFY(accepted->inputContractError().isEmpty());
        QVERIFY(accepted->windowsContractAvailable());
        QVERIFY(accepted->windowsContractError().isEmpty());
        QVERIFY(accepted->advancedContractAvailable());
        QVERIFY(accepted->advancedContractError().isEmpty());
        QCOMPARE(accepted->appearanceOptions().size(), 40);
        QCOMPARE(
            accepted->appearanceOptionIds(),
            QStringList({
                QStringLiteral("hyprland.general.border_size"),
                QStringLiteral("hyprland.decoration.rounding"),
                QStringLiteral("hyprland.general.gaps_in"),
                QStringLiteral("hyprland.general.gaps_out"),
                QStringLiteral("hyprland.decoration.blur.enabled"),
                QStringLiteral("hyprland.decoration.shadow.enabled"),
                QStringLiteral("hyprland.animations.enabled"),
                QStringLiteral("hyprland.decoration.dim_inactive"),
                QStringLiteral("hyprland.decoration.dim_strength"),
                QStringLiteral("hyprland.decoration.active_opacity"),
                QStringLiteral("hyprland.decoration.inactive_opacity"),
                QStringLiteral("hyprland.decoration.fullscreen_opacity"),
                QStringLiteral("hyprland.decoration.dim_modal"),
                QStringLiteral("hyprland.decoration.dim_special"),
                QStringLiteral("hyprland.decoration.dim_around"),
                QStringLiteral("hyprland.decoration.blur.size"),
                QStringLiteral("hyprland.decoration.blur.passes"),
                QStringLiteral("hyprland.decoration.blur.ignore_opacity"),
                QStringLiteral(
                    "hyprland.decoration.blur.new_optimizations"
                ),
                QStringLiteral("hyprland.decoration.blur.xray"),
                QStringLiteral("hyprland.decoration.blur.special"),
                QStringLiteral("hyprland.decoration.blur.popups"),
                QStringLiteral(
                    "hyprland.decoration.blur.popups_ignorealpha"
                ),
                QStringLiteral("hyprland.decoration.blur.input_methods"),
                QStringLiteral(
                    "hyprland.decoration.blur.input_methods_ignorealpha"
                ),
                QStringLiteral("hyprland.decoration.blur.brightness"),
                QStringLiteral("hyprland.decoration.blur.contrast"),
                QStringLiteral("hyprland.decoration.blur.noise"),
                QStringLiteral("hyprland.decoration.blur.vibrancy"),
                QStringLiteral(
                    "hyprland.decoration.blur.vibrancy_darkness"
                ),
                QStringLiteral(
                    "hyprland.decoration.border_part_of_window"
                ),
                QStringLiteral("hyprland.decoration.rounding_power"),
                QStringLiteral("hyprland.decoration.shadow.range"),
                QStringLiteral("hyprland.decoration.shadow.render_power"),
                QStringLiteral("hyprland.decoration.shadow.sharp"),
                QStringLiteral("hyprland.decoration.shadow.offset"),
                QStringLiteral("hyprland.decoration.shadow.scale"),
                QStringLiteral("hyprland.decoration.glow.enabled"),
                QStringLiteral("hyprland.decoration.glow.range"),
                QStringLiteral("hyprland.decoration.glow.render_power"),
            })
        );
        const auto border = accepted->appearanceOptions().first().toMap();
        QCOMPARE(border.value(QStringLiteral("type")).toString(),
                 QStringLiteral("integer"));
        QCOMPARE(border.value(QStringLiteral("control")).toString(),
                 QStringLiteral("spinBox"));
        QCOMPARE(border.value(QStringLiteral("min")).toInt(), 0);
        QCOMPARE(border.value(QStringLiteral("max")).toInt(), 20);

        const auto appearanceOptions = accepted->appearanceOptions();
        const auto dimInactive = appearanceOptions.at(7).toMap();
        QCOMPARE(dimInactive.value(QStringLiteral("id")).toString(),
                 QStringLiteral("hyprland.decoration.dim_inactive"));
        QCOMPARE(dimInactive.value(QStringLiteral("type")).toString(),
                 QStringLiteral("boolean"));
        QCOMPARE(dimInactive.value(QStringLiteral("control")).toString(),
                 QStringLiteral("toggle"));
        QCOMPARE(dimInactive.value(QStringLiteral("defaultValue")).toBool(),
                 false);
        QVERIFY(!dimInactive.contains(QStringLiteral("min")));
        QVERIFY(!dimInactive.contains(QStringLiteral("max")));
        QVERIFY(!dimInactive.contains(QStringLiteral("step")));
        QVERIFY(!dimInactive.contains(QStringLiteral("choices")));

        const auto dimStrength = appearanceOptions.at(8).toMap();
        QCOMPARE(dimStrength.value(QStringLiteral("id")).toString(),
                 QStringLiteral("hyprland.decoration.dim_strength"));
        QCOMPARE(dimStrength.value(QStringLiteral("type")).toString(),
                 QStringLiteral("number"));
        QCOMPARE(dimStrength.value(QStringLiteral("control")).toString(),
                 QStringLiteral("slider"));
        QCOMPARE(dimStrength.value(QStringLiteral("defaultValue")).toDouble(),
                 0.5);
        QCOMPARE(dimStrength.value(QStringLiteral("min")).toDouble(), 0.0);
        QCOMPARE(dimStrength.value(QStringLiteral("max")).toDouble(), 1.0);
        QVERIFY(!dimStrength.contains(QStringLiteral("step")));
        QVERIFY(!dimStrength.contains(QStringLiteral("choices")));

        const auto verifyNumber = [&appearanceOptions](
            const qsizetype index,
            const QString &id,
            const double defaultValue,
            const double minimum = 0.0,
            const double maximum = 1.0
        ) {
            const auto option = appearanceOptions.at(index).toMap();
            QCOMPARE(option.value(QStringLiteral("id")).toString(), id);
            QCOMPARE(option.value(QStringLiteral("type")).toString(),
                     QStringLiteral("number"));
            QCOMPARE(option.value(QStringLiteral("control")).toString(),
                     QStringLiteral("slider"));
            QCOMPARE(option.value(QStringLiteral("defaultValue")).toDouble(),
                     defaultValue);
            QCOMPARE(option.value(QStringLiteral("min")).toDouble(), minimum);
            QCOMPARE(option.value(QStringLiteral("max")).toDouble(), maximum);
            QVERIFY(!option.contains(QStringLiteral("step")));
            QVERIFY(!option.contains(QStringLiteral("choices")));
            QCOMPARE(option.value(QStringLiteral("risk")).toString(),
                     QStringLiteral("safe"));
        };
        verifyNumber(
            9, QStringLiteral("hyprland.decoration.active_opacity"), 1.0
        );
        verifyNumber(
            10, QStringLiteral("hyprland.decoration.inactive_opacity"), 1.0
        );
        verifyNumber(
            11, QStringLiteral("hyprland.decoration.fullscreen_opacity"), 1.0
        );

        const auto dimModal = appearanceOptions.at(12).toMap();
        QCOMPARE(dimModal.value(QStringLiteral("id")).toString(),
                 QStringLiteral("hyprland.decoration.dim_modal"));
        QCOMPARE(dimModal.value(QStringLiteral("type")).toString(),
                 QStringLiteral("boolean"));
        QCOMPARE(dimModal.value(QStringLiteral("control")).toString(),
                 QStringLiteral("toggle"));
        QCOMPARE(dimModal.value(QStringLiteral("defaultValue")).toBool(),
                 true);
        QVERIFY(!dimModal.contains(QStringLiteral("min")));
        QVERIFY(!dimModal.contains(QStringLiteral("max")));
        QVERIFY(!dimModal.contains(QStringLiteral("step")));
        QVERIFY(!dimModal.contains(QStringLiteral("choices")));

        verifyNumber(
            13, QStringLiteral("hyprland.decoration.dim_special"), 0.2
        );
        verifyNumber(
            14, QStringLiteral("hyprland.decoration.dim_around"), 0.4
        );

        const auto verifyInteger = [&appearanceOptions](
            const qsizetype index,
            const QString &id,
            const int defaultValue,
            const int minimum,
            const int maximum
        ) {
            const auto option = appearanceOptions.at(index).toMap();
            QCOMPARE(option.value(QStringLiteral("id")).toString(), id);
            QCOMPARE(option.value(QStringLiteral("type")).toString(),
                     QStringLiteral("integer"));
            QCOMPARE(option.value(QStringLiteral("control")).toString(),
                     QStringLiteral("spinBox"));
            QCOMPARE(option.value(QStringLiteral("defaultValue")).toInt(),
                     defaultValue);
            QCOMPARE(option.value(QStringLiteral("min")).toInt(), minimum);
            QCOMPARE(option.value(QStringLiteral("max")).toInt(), maximum);
            QVERIFY(!option.contains(QStringLiteral("step")));
            QVERIFY(!option.contains(QStringLiteral("choices")));
        };
        const auto verifyBoolean = [&appearanceOptions](
            const qsizetype index,
            const QString &id,
            const bool defaultValue
        ) {
            const auto option = appearanceOptions.at(index).toMap();
            QCOMPARE(option.value(QStringLiteral("id")).toString(), id);
            QCOMPARE(option.value(QStringLiteral("type")).toString(),
                     QStringLiteral("boolean"));
            QCOMPARE(option.value(QStringLiteral("control")).toString(),
                     QStringLiteral("toggle"));
            QCOMPARE(option.value(QStringLiteral("defaultValue")).toBool(),
                     defaultValue);
            QVERIFY(!option.contains(QStringLiteral("min")));
            QVERIFY(!option.contains(QStringLiteral("max")));
            QVERIFY(!option.contains(QStringLiteral("step")));
            QVERIFY(!option.contains(QStringLiteral("choices")));
        };
        verifyInteger(
            15, QStringLiteral("hyprland.decoration.blur.size"), 8, 0, 100
        );
        verifyInteger(
            16, QStringLiteral("hyprland.decoration.blur.passes"), 1, 0, 10
        );
        verifyBoolean(
            17,
            QStringLiteral("hyprland.decoration.blur.ignore_opacity"),
            true
        );
        verifyBoolean(
            18,
            QStringLiteral("hyprland.decoration.blur.new_optimizations"),
            true
        );
        verifyBoolean(
            19, QStringLiteral("hyprland.decoration.blur.xray"), false
        );
        verifyBoolean(
            20, QStringLiteral("hyprland.decoration.blur.special"), false
        );
        verifyBoolean(
            21, QStringLiteral("hyprland.decoration.blur.popups"), false
        );
        verifyNumber(
            22,
            QStringLiteral("hyprland.decoration.blur.popups_ignorealpha"),
            0.2
        );
        verifyBoolean(
            23,
            QStringLiteral("hyprland.decoration.blur.input_methods"),
            false
        );
        verifyNumber(
            24,
            QStringLiteral(
                "hyprland.decoration.blur.input_methods_ignorealpha"
            ),
            0.2
        );
        verifyNumber(
            25,
            QStringLiteral("hyprland.decoration.blur.brightness"),
            1.0,
            0.0,
            2.0
        );
        verifyNumber(
            26,
            QStringLiteral("hyprland.decoration.blur.contrast"),
            0.8916,
            0.0,
            2.0
        );
        verifyNumber(
            27,
            QStringLiteral("hyprland.decoration.blur.noise"),
            0.0117
        );
        verifyNumber(
            28,
            QStringLiteral("hyprland.decoration.blur.vibrancy"),
            0.1696
        );
        verifyNumber(
            29,
            QStringLiteral("hyprland.decoration.blur.vibrancy_darkness"),
            0.0
        );
        verifyBoolean(
            30,
            QStringLiteral("hyprland.decoration.border_part_of_window"),
            true
        );
        QCOMPARE(
            appearanceOptions.at(30).toMap()
                .value(QStringLiteral("risk")).toString(),
            QStringLiteral("safe")
        );
        verifyNumber(
            31,
            QStringLiteral("hyprland.decoration.rounding_power"),
            2.0,
            2.0,
            10.0
        );
        verifyInteger(
            32,
            QStringLiteral("hyprland.decoration.shadow.range"),
            4,
            0,
            100
        );
        verifyInteger(
            33,
            QStringLiteral("hyprland.decoration.shadow.render_power"),
            3,
            1,
            4
        );
        verifyBoolean(
            34,
            QStringLiteral("hyprland.decoration.shadow.sharp"),
            false
        );
        const auto shadowOffset = appearanceOptions.at(35).toMap();
        QCOMPARE(
            shadowOffset.value(QStringLiteral("id")).toString(),
            QStringLiteral("hyprland.decoration.shadow.offset")
        );
        QCOMPARE(
            shadowOffset.value(QStringLiteral("type")).toString(),
            QStringLiteral("vector2")
        );
        QCOMPARE(
            shadowOffset.value(QStringLiteral("control")).toString(),
            QStringLiteral("vector2")
        );
        QCOMPARE(
            shadowOffset.value(QStringLiteral("defaultValue")).toList(),
            QVariantList({0.0, 0.0})
        );
        QCOMPARE(
            shadowOffset.value(QStringLiteral("min")).toList(),
            QVariantList({-250.0, -250.0})
        );
        QCOMPARE(
            shadowOffset.value(QStringLiteral("max")).toList(),
            QVariantList({250.0, 250.0})
        );
        verifyNumber(
            36,
            QStringLiteral("hyprland.decoration.shadow.scale"),
            1.0
        );
        verifyBoolean(
            37,
            QStringLiteral("hyprland.decoration.glow.enabled"),
            false
        );
        verifyInteger(
            38,
            QStringLiteral("hyprland.decoration.glow.range"),
            10,
            0,
            100
        );
        verifyInteger(
            39,
            QStringLiteral("hyprland.decoration.glow.render_power"),
            3,
            1,
            4
        );
        QVERIFY(!shadowOffset.contains(QStringLiteral("step")));
        QVERIFY(!shadowOffset.contains(QStringLiteral("choices")));
        QCOMPARE(
            shadowOffset.value(QStringLiteral("risk")).toString(),
            QStringLiteral("safe")
        );
        for (qsizetype index = 32; index < 37; ++index) {
            const auto id = accepted->appearanceOptionIds().at(index);
            const auto *option = accepted->appearanceOption(id);
            QVERIFY2(option != nullptr, qPrintable(id));
            QCOMPARE(option->module, QStringLiteral("decoration"));
            QCOMPARE(
                option->path,
                QStringLiteral("decoration:shadow:")
                    + id.mid(QStringLiteral(
                        "hyprland.decoration.shadow."
                    ).size())
            );
            QCOMPARE(
                option->luaPath,
                QStringList({
                    QStringLiteral("decoration"),
                    QStringLiteral("shadow"),
                    id.mid(QStringLiteral(
                        "hyprland.decoration.shadow."
                    ).size()),
                })
            );
            QCOMPARE(
                static_cast<int>(option->uiTier),
                static_cast<int>(HyprShelld::Hyprland::UiTier::Common)
            );
            QCOMPARE(
                static_cast<int>(option->applyMode),
                static_cast<int>(HyprShelld::Hyprland::ApplyMode::Reload)
            );
            QCOMPARE(
                static_cast<int>(option->risk),
                static_cast<int>(HyprShelld::Hyprland::RiskLevel::Safe)
            );
            QVERIFY((option->since
                     == HyprShelld::Hyprland::SemanticVersion{0, 55, 0}));
            QVERIFY(!option->until.has_value());
        }
        for (qsizetype index = 37; index < 40; ++index) {
            const auto id = accepted->appearanceOptionIds().at(index);
            const auto *option = accepted->appearanceOption(id);
            QVERIFY2(option != nullptr, qPrintable(id));
            QCOMPARE(option->module, QStringLiteral("decoration"));
            QCOMPARE(
                option->path,
                QStringLiteral("decoration:glow:")
                    + id.mid(QStringLiteral(
                        "hyprland.decoration.glow."
                    ).size())
            );
            QCOMPARE(
                option->luaPath,
                QStringList({
                    QStringLiteral("decoration"),
                    QStringLiteral("glow"),
                    id.mid(QStringLiteral(
                        "hyprland.decoration.glow."
                    ).size()),
                })
            );
            QCOMPARE(
                static_cast<int>(option->uiTier),
                static_cast<int>(HyprShelld::Hyprland::UiTier::Common)
            );
            QCOMPARE(
                static_cast<int>(option->applyMode),
                static_cast<int>(HyprShelld::Hyprland::ApplyMode::Reload)
            );
            QCOMPARE(
                static_cast<int>(option->risk),
                static_cast<int>(HyprShelld::Hyprland::RiskLevel::Safe)
            );
            QVERIFY((option->since
                     == HyprShelld::Hyprland::SemanticVersion{0, 55, 0}));
            QVERIFY(!option->until.has_value());
        }

        const QStringList expectedInputIds{
            QStringLiteral("hyprland.input.repeat_rate"),
            QStringLiteral("hyprland.input.repeat_delay"),
            QStringLiteral("hyprland.input.sensitivity"),
            QStringLiteral("hyprland.input.accel_profile"),
            QStringLiteral("hyprland.input.natural_scroll"),
            QStringLiteral("hyprland.input.left_handed"),
            QStringLiteral("hyprland.input.scroll_factor"),
            QStringLiteral("hyprland.input.touchpad.tap-to-click"),
            QStringLiteral("hyprland.input.touchpad.tap-and-drag"),
            QStringLiteral("hyprland.input.touchpad.natural_scroll"),
            QStringLiteral(
                "hyprland.input.touchpad.disable_while_typing"
            ),
            QStringLiteral("hyprland.input.touchpad.scroll_factor"),
            QStringLiteral("hyprland.input.scroll_method"),
            QStringLiteral("hyprland.input.scroll_button"),
            QStringLiteral("hyprland.input.scroll_button_lock"),
            QStringLiteral("hyprland.input.off_window_axis_events"),
            QStringLiteral("hyprland.input.emulate_discrete_scroll"),
            QStringLiteral(
                "hyprland.input.touchpad.clickfinger_behavior"
            ),
            QStringLiteral("hyprland.input.touchpad.drag_3fg"),
            QStringLiteral("hyprland.input.touchpad.drag_lock"),
            QStringLiteral("hyprland.input.touchpad.flip_x"),
            QStringLiteral("hyprland.input.touchpad.flip_y"),
            QStringLiteral(
                "hyprland.input.touchpad.middle_button_emulation"
            ),
            QStringLiteral("hyprland.input.touchpad.tap_button_map"),
            QStringLiteral("hyprland.input.numlock_by_default"),
            QStringLiteral("hyprland.input.virtualkeyboard.share_states"),
            QStringLiteral(
                "hyprland.input.virtualkeyboard.release_pressed_on_close"
            ),
            QStringLiteral("hyprland.misc.name_vk_after_proc"),
            QStringLiteral("hyprland.input.force_no_accel"),
            QStringLiteral("hyprland.input.rotation"),
            QStringLiteral("hyprland.misc.middle_click_paste"),
            QStringLiteral("hyprland.gestures.close_max_timeout"),
            QStringLiteral("hyprland.input.touchdevice.enabled"),
            QStringLiteral("hyprland.input.touchdevice.transform"),
            QStringLiteral("hyprland.input.tablet.relative_input"),
            QStringLiteral("hyprland.input.tablet.left_handed"),
            QStringLiteral("hyprland.input.tablet.transform"),
            QStringLiteral("hyprland.cursor.hide_on_key_press"),
            QStringLiteral("hyprland.cursor.hide_on_touch"),
            QStringLiteral("hyprland.cursor.hide_on_tablet"),
            QStringLiteral("hyprland.cursor.inactive_timeout"),
            QStringLiteral("hyprland.cursor.hotspot_padding"),
            QStringLiteral("hyprland.cursor.no_warps"),
            QStringLiteral("hyprland.cursor.persistent_warps"),
            QStringLiteral(
                "hyprland.cursor.warp_back_after_non_mouse_input"
            ),
            QStringLiteral("hyprland.input.tablet.region_position"),
            QStringLiteral(
                "hyprland.input.tablet.absolute_region_position"
            ),
            QStringLiteral("hyprland.input.tablet.region_size"),
            QStringLiteral("hyprland.input.resolve_binds_by_sym"),
        };
        QCOMPARE(accepted->inputOptionIds(), expectedInputIds);
        QCOMPARE(accepted->inputOptions().size(), 49);

        const auto inputOptions = accepted->inputOptions();
        const auto requireNumeric = [&inputOptions](
            const qsizetype index,
            const QString &id,
            const QString &type,
            const QString &control,
            const double defaultValue,
            const double minimum,
            const double maximum
        ) {
            const auto option = inputOptions.at(index).toMap();
            QCOMPARE(option.value(QStringLiteral("id")).toString(), id);
            QCOMPARE(option.value(QStringLiteral("type")).toString(), type);
            QCOMPARE(
                option.value(QStringLiteral("control")).toString(), control
            );
            QCOMPARE(
                option.value(QStringLiteral("defaultValue")).toDouble(),
                defaultValue
            );
            QCOMPARE(option.value(QStringLiteral("min")).toDouble(), minimum);
            QCOMPARE(option.value(QStringLiteral("max")).toDouble(), maximum);
            QVERIFY(!option.contains(QStringLiteral("step")));
            QVERIFY(!option.contains(QStringLiteral("choices")));
        };
        requireNumeric(
            0, expectedInputIds.at(0), QStringLiteral("integer"),
            QStringLiteral("spinBox"), 25, 0, 200
        );
        requireNumeric(
            1, expectedInputIds.at(1), QStringLiteral("integer"),
            QStringLiteral("spinBox"), 600, 0, 2000
        );
        requireNumeric(
            2, expectedInputIds.at(2), QStringLiteral("number"),
            QStringLiteral("slider"), 0, -1, 1
        );
        requireNumeric(
            6, expectedInputIds.at(6), QStringLiteral("number"),
            QStringLiteral("slider"), 1, 0, 2
        );
        requireNumeric(
            11, expectedInputIds.at(11), QStringLiteral("number"),
            QStringLiteral("slider"), 1, 0, 2
        );
        requireNumeric(
            13, expectedInputIds.at(13), QStringLiteral("integer"),
            QStringLiteral("spinBox"), 0, 0, 300
        );
        requireNumeric(
            29, expectedInputIds.at(29), QStringLiteral("integer"),
            QStringLiteral("spinBox"), 0, 0, 359
        );
        requireNumeric(
            31, expectedInputIds.at(31), QStringLiteral("integer"),
            QStringLiteral("spinBox"), 1000, 10, 2000
        );
        requireNumeric(
            33, expectedInputIds.at(33), QStringLiteral("integer"),
            QStringLiteral("spinBox"), 0, 0, 6
        );
        requireNumeric(
            36, expectedInputIds.at(36), QStringLiteral("integer"),
            QStringLiteral("spinBox"), 0, 0, 6
        );
        requireNumeric(
            40, expectedInputIds.at(40), QStringLiteral("number"),
            QStringLiteral("slider"), 0, 0, 20
        );
        requireNumeric(
            41, expectedInputIds.at(41), QStringLiteral("integer"),
            QStringLiteral("spinBox"), 0, 0, 20
        );

        const auto requireVector = [&inputOptions, &expectedInputIds](
            const qsizetype index,
            const QVariantList &minimum,
            const QVariantList &maximum
        ) {
            const auto option = inputOptions.at(index).toMap();
            QCOMPARE(option.value(QStringLiteral("id")).toString(),
                     expectedInputIds.at(index));
            QCOMPARE(option.value(QStringLiteral("type")).toString(),
                     QStringLiteral("vector2"));
            QCOMPARE(option.value(QStringLiteral("control")).toString(),
                     QStringLiteral("vector2"));
            QCOMPARE(option.value(QStringLiteral("defaultValue")).toList(),
                     QVariantList({0.0, 0.0}));
            QCOMPARE(option.value(QStringLiteral("min")).toList(), minimum);
            QCOMPARE(option.value(QStringLiteral("max")).toList(), maximum);
            QVERIFY(!option.contains(QStringLiteral("step")));
            QVERIFY(!option.contains(QStringLiteral("choices")));
        };
        requireVector(45, {-20000.0, -20000.0}, {20000.0, 20000.0});
        requireVector(47, {-100.0, -100.0}, {4000.0, 4000.0});

        const auto acceleration = inputOptions.at(3).toMap();
        QCOMPARE(acceleration.value(QStringLiteral("id")).toString(),
                 expectedInputIds.at(3));
        QCOMPARE(acceleration.value(QStringLiteral("type")).toString(),
                 QStringLiteral("enum"));
        QCOMPARE(acceleration.value(QStringLiteral("control")).toString(),
                 QStringLiteral("select"));
        QCOMPARE(acceleration.value(QStringLiteral("defaultValue")).toString(),
                 QString{});
        const auto accelerationChoices = acceleration.value(
            QStringLiteral("choices")
        ).toList();
        QCOMPARE(accelerationChoices.size(), 3);
        QCOMPARE(accelerationChoices.at(0).toMap(), QVariantMap({
            {QStringLiteral("label"), QStringLiteral("automatic")},
            {QStringLiteral("value"), QString{}},
        }));
        QCOMPARE(accelerationChoices.at(1).toMap(), QVariantMap({
            {QStringLiteral("label"), QStringLiteral("adaptive")},
            {QStringLiteral("value"), QStringLiteral("adaptive")},
        }));
        QCOMPARE(accelerationChoices.at(2).toMap(), QVariantMap({
            {QStringLiteral("label"), QStringLiteral("flat")},
            {QStringLiteral("value"), QStringLiteral("flat")},
        }));

        const auto scrollMethod = inputOptions.at(12).toMap();
        QCOMPARE(scrollMethod.value(QStringLiteral("id")).toString(),
                 expectedInputIds.at(12));
        QCOMPARE(scrollMethod.value(QStringLiteral("type")).toString(),
                 QStringLiteral("enum"));
        QCOMPARE(scrollMethod.value(QStringLiteral("control")).toString(),
                 QStringLiteral("select"));
        QCOMPARE(scrollMethod.value(QStringLiteral("defaultValue")).toString(),
                 QString{});
        QCOMPARE(scrollMethod.value(QStringLiteral("choices")).toList(),
                 QVariantList({
                     QVariantMap({
                         {QStringLiteral("label"), QStringLiteral("automatic")},
                         {QStringLiteral("value"), QString{}},
                     }),
                     QVariantMap({
                         {QStringLiteral("label"), QStringLiteral("two-finger")},
                         {QStringLiteral("value"), QStringLiteral("2fg")},
                     }),
                     QVariantMap({
                         {QStringLiteral("label"), QStringLiteral("edge")},
                         {QStringLiteral("value"), QStringLiteral("edge")},
                     }),
                     QVariantMap({
                         {QStringLiteral("label"), QStringLiteral("button")},
                         {QStringLiteral("value"), QStringLiteral("on_button_down")},
                     }),
                     QVariantMap({
                         {QStringLiteral("label"), QStringLiteral("disabled")},
                         {QStringLiteral("value"), QStringLiteral("no_scroll")},
                     }),
                 }));

        const auto requireNumericEnum = [&inputOptions, &expectedInputIds](
            const qsizetype index,
            const int defaultValue,
            const int minimum,
            const int maximum,
            const QVariantList &choices
        ) {
            const auto option = inputOptions.at(index).toMap();
            QCOMPARE(option.value(QStringLiteral("id")).toString(),
                     expectedInputIds.at(index));
            QCOMPARE(option.value(QStringLiteral("type")).toString(),
                     QStringLiteral("enum"));
            QCOMPARE(option.value(QStringLiteral("control")).toString(),
                     QStringLiteral("select"));
            QCOMPARE(option.value(QStringLiteral("defaultValue")).toInt(),
                     defaultValue);
            QCOMPARE(option.value(QStringLiteral("min")).toInt(), minimum);
            QCOMPARE(option.value(QStringLiteral("max")).toInt(), maximum);
            QCOMPARE(option.value(QStringLiteral("choices")).toList(), choices);
        };
        requireNumericEnum(
            15, 1, 0, 3,
            QVariantList({
                QVariantMap({{QStringLiteral("label"), QStringLiteral("ignore")},
                             {QStringLiteral("value"), 0}}),
                QVariantMap({{QStringLiteral("label"), QStringLiteral("send")},
                             {QStringLiteral("value"), 1}}),
                QVariantMap({{QStringLiteral("label"), QStringLiteral("clamp")},
                             {QStringLiteral("value"), 2}}),
                QVariantMap({{QStringLiteral("label"), QStringLiteral("warp")},
                             {QStringLiteral("value"), 3}}),
            })
        );
        requireNumericEnum(
            16, 1, 0, 2,
            QVariantList({
                QVariantMap({{QStringLiteral("label"), QStringLiteral("disable")},
                             {QStringLiteral("value"), 0}}),
                QVariantMap({{QStringLiteral("label"), QStringLiteral("non_standard")},
                             {QStringLiteral("value"), 1}}),
                QVariantMap({{QStringLiteral("label"), QStringLiteral("force_all")},
                             {QStringLiteral("value"), 2}}),
            })
        );
        requireNumericEnum(
            18, 0, 0, 2,
            QVariantList({
                QVariantMap({{QStringLiteral("label"), QStringLiteral("disable")},
                             {QStringLiteral("value"), 0}}),
                QVariantMap({{QStringLiteral("label"), QStringLiteral("3_finger")},
                             {QStringLiteral("value"), 1}}),
                QVariantMap({{QStringLiteral("label"), QStringLiteral("4_finger")},
                             {QStringLiteral("value"), 2}}),
            })
        );
        requireNumericEnum(
            19, 0, 0, 2,
            QVariantList({
                QVariantMap({{QStringLiteral("label"), QStringLiteral("disabled")},
                             {QStringLiteral("value"), 0}}),
                QVariantMap({{QStringLiteral("label"), QStringLiteral("enabled with timeout")},
                             {QStringLiteral("value"), 1}}),
                QVariantMap({{QStringLiteral("label"), QStringLiteral("sticky")},
                             {QStringLiteral("value"), 2}}),
            })
        );
        requireNumericEnum(
            25, 2, 0, 2,
            QVariantList({
                QVariantMap({{QStringLiteral("label"), QStringLiteral("disable")},
                             {QStringLiteral("value"), 0}}),
                QVariantMap({{QStringLiteral("label"), QStringLiteral("enable")},
                             {QStringLiteral("value"), 1}}),
                QVariantMap({{QStringLiteral("label"), QStringLiteral("only_non_ime")},
                             {QStringLiteral("value"), 2}}),
            })
        );

        const auto tapButtonMap = inputOptions.at(23).toMap();
        QCOMPARE(tapButtonMap.value(QStringLiteral("id")).toString(),
                 expectedInputIds.at(23));
        QCOMPARE(tapButtonMap.value(QStringLiteral("type")).toString(),
                 QStringLiteral("enum"));
        QCOMPARE(tapButtonMap.value(QStringLiteral("control")).toString(),
                 QStringLiteral("select"));
        QCOMPARE(tapButtonMap.value(QStringLiteral("defaultValue")).toString(),
                 QString{});
        QCOMPARE(tapButtonMap.value(QStringLiteral("choices")).toList(),
                 QVariantList({
                     QVariantMap({
                         {QStringLiteral("label"), QStringLiteral("automatic")},
                         {QStringLiteral("value"), QString{}},
                     }),
                     QVariantMap({
                         {QStringLiteral("label"), QStringLiteral("left-right-middle")},
                         {QStringLiteral("value"), QStringLiteral("lrm")},
                     }),
                     QVariantMap({
                         {QStringLiteral("label"), QStringLiteral("left-middle-right")},
                         {QStringLiteral("value"), QStringLiteral("lmr")},
                     }),
                 }));

        const QList<std::pair<qsizetype, bool>> booleanDefaults{
            {4, false}, {5, false}, {7, true}, {8, true},
            {9, false}, {10, true}, {14, false}, {17, false},
            {20, false}, {21, false}, {22, false}, {24, false},
            {26, false}, {27, true}, {28, false}, {30, true},
            {32, true}, {34, false}, {35, false}, {37, false},
            {38, true}, {39, false}, {42, false}, {43, false},
            {44, false}, {46, false}, {48, false},
        };
        for (const auto &[index, defaultValue] : booleanDefaults) {
            const auto option = inputOptions.at(index).toMap();
            QCOMPARE(option.value(QStringLiteral("id")).toString(),
                     expectedInputIds.at(index));
            QCOMPARE(option.value(QStringLiteral("type")).toString(),
                     QStringLiteral("boolean"));
            QCOMPARE(option.value(QStringLiteral("control")).toString(),
                     QStringLiteral("toggle"));
            QCOMPARE(option.value(QStringLiteral("defaultValue")).toBool(),
                     defaultValue);
            QVERIFY(!option.contains(QStringLiteral("min")));
            QVERIFY(!option.contains(QStringLiteral("max")));
            QVERIFY(!option.contains(QStringLiteral("step")));
            QVERIFY(!option.contains(QStringLiteral("choices")));
        }

        const QStringList expectedWindowsIds{
            QStringLiteral("hyprland.general.layout"),
            QStringLiteral("hyprland.general.resize_on_border"),
            QStringLiteral("hyprland.general.extend_border_grab_area"),
            QStringLiteral("hyprland.general.hover_icon_on_border"),
            QStringLiteral("hyprland.general.resize_corner"),
            QStringLiteral("hyprland.general.snap.enabled"),
            QStringLiteral("hyprland.general.snap.border_overlap"),
            QStringLiteral("hyprland.general.snap.monitor_gap"),
            QStringLiteral("hyprland.general.snap.respect_gaps"),
            QStringLiteral("hyprland.general.snap.window_gap"),
            QStringLiteral("hyprland.input.follow_mouse"),
            QStringLiteral("hyprland.input.mouse_refocus"),
            QStringLiteral("hyprland.input.follow_mouse_shrink"),
            QStringLiteral("hyprland.input.float_switch_override_focus"),
            QStringLiteral("hyprland.input.focus_on_close"),
            QStringLiteral("hyprland.input.special_fallthrough"),
            QStringLiteral("hyprland.general.no_focus_fallback"),
            QStringLiteral("hyprland.general.modal_parent_blocking"),
            QStringLiteral("hyprland.general.float_gaps"),
            QStringLiteral("hyprland.general.gaps_workspaces"),
            QStringLiteral("hyprland.layout.single_window_aspect_ratio"),
            QStringLiteral(
                "hyprland.layout.single_window_aspect_ratio_tolerance"
            ),
            QStringLiteral("hyprland.dwindle.default_split_ratio"),
            QStringLiteral("hyprland.dwindle.force_split"),
            QStringLiteral("hyprland.dwindle.permanent_direction_override"),
            QStringLiteral("hyprland.dwindle.precise_mouse_move"),
            QStringLiteral("hyprland.dwindle.preserve_split"),
            QStringLiteral("hyprland.dwindle.smart_resizing"),
            QStringLiteral("hyprland.dwindle.smart_split"),
            QStringLiteral("hyprland.dwindle.special_scale_factor"),
            QStringLiteral("hyprland.dwindle.split_bias"),
            QStringLiteral("hyprland.dwindle.split_width_multiplier"),
            QStringLiteral("hyprland.dwindle.use_active_for_splits"),
            QStringLiteral("hyprland.master.allow_small_split"),
            QStringLiteral("hyprland.master.always_keep_position"),
            QStringLiteral("hyprland.master.center_ignores_reserved"),
            QStringLiteral("hyprland.master.center_master_fallback"),
            QStringLiteral("hyprland.master.drop_at_cursor"),
            QStringLiteral("hyprland.master.focus_master_on_close"),
            QStringLiteral("hyprland.master.mfact"),
            QStringLiteral("hyprland.master.new_on_active"),
            QStringLiteral("hyprland.master.new_on_top"),
            QStringLiteral("hyprland.master.new_status"),
            QStringLiteral("hyprland.master.orientation"),
            QStringLiteral("hyprland.master.slave_count_for_center_master"),
            QStringLiteral("hyprland.master.smart_resizing"),
            QStringLiteral("hyprland.master.special_scale_factor"),
            QStringLiteral("hyprland.scrolling.column_width"),
            QStringLiteral("hyprland.scrolling.direction"),
            QStringLiteral("hyprland.scrolling.focus_fit_method"),
            QStringLiteral("hyprland.scrolling.follow_focus"),
            QStringLiteral("hyprland.scrolling.follow_min_visible"),
            QStringLiteral("hyprland.scrolling.fullscreen_on_one_column"),
            QStringLiteral("hyprland.scrolling.wrap_focus"),
            QStringLiteral("hyprland.scrolling.wrap_swapcol"),
            QStringLiteral("hyprland.gestures.scrolling.move_snap_cursor"),
            QStringLiteral("hyprland.gestures.scrolling.move_snap_to_grid"),
            QStringLiteral("hyprland.group.auto_group"),
            QStringLiteral("hyprland.group.insert_after_current"),
            QStringLiteral("hyprland.group.focus_removed_window"),
            QStringLiteral("hyprland.group.drag_into_group"),
            QStringLiteral("hyprland.group.merge_groups_on_drag"),
            QStringLiteral("hyprland.group.merge_groups_on_groupbar"),
            QStringLiteral(
                "hyprland.group.merge_floated_into_tiled_on_groupbar"
            ),
            QStringLiteral("hyprland.group.group_on_movetoworkspace"),
            QStringLiteral("hyprland.group.groupbar.enabled"),
            QStringLiteral("hyprland.group.groupbar.disable_when_only"),
            QStringLiteral("hyprland.group.groupbar.font_family"),
            QStringLiteral("hyprland.group.groupbar.font_weight_active"),
            QStringLiteral("hyprland.group.groupbar.font_weight_inactive"),
            QStringLiteral("hyprland.group.groupbar.font_size"),
            QStringLiteral("hyprland.group.groupbar.gradients"),
            QStringLiteral("hyprland.group.groupbar.height"),
            QStringLiteral("hyprland.group.groupbar.indicator_gap"),
            QStringLiteral("hyprland.group.groupbar.indicator_height"),
            QStringLiteral("hyprland.group.groupbar.stacked"),
            QStringLiteral("hyprland.group.groupbar.priority"),
            QStringLiteral("hyprland.group.groupbar.render_titles"),
            QStringLiteral("hyprland.group.groupbar.scrolling"),
            QStringLiteral("hyprland.group.groupbar.middle_click_close"),
            QStringLiteral("hyprland.group.groupbar.rounding"),
            QStringLiteral("hyprland.group.groupbar.rounding_power"),
            QStringLiteral("hyprland.group.groupbar.gradient_rounding"),
            QStringLiteral(
                "hyprland.group.groupbar.gradient_rounding_power"
            ),
            QStringLiteral("hyprland.group.groupbar.round_only_edges"),
            QStringLiteral(
                "hyprland.group.groupbar.gradient_round_only_edges"
            ),
            QStringLiteral("hyprland.group.groupbar.gaps_out"),
            QStringLiteral("hyprland.group.groupbar.gaps_in"),
            QStringLiteral("hyprland.group.groupbar.keep_upper_gap"),
            QStringLiteral("hyprland.group.groupbar.text_offset"),
            QStringLiteral("hyprland.group.groupbar.text_padding"),
            QStringLiteral("hyprland.group.groupbar.blur"),
            QStringLiteral("hyprland.binds.allow_pin_fullscreen"),
            QStringLiteral("hyprland.binds.focus_preferred_method"),
            QStringLiteral("hyprland.binds.ignore_group_lock"),
            QStringLiteral("hyprland.binds.movefocus_cycles_fullscreen"),
            QStringLiteral("hyprland.binds.movefocus_cycles_groupfirst"),
            QStringLiteral(
                "hyprland.binds.window_direction_monitor_fallback"
            ),
            QStringLiteral("hyprland.misc.enable_anr_dialog"),
            QStringLiteral("hyprland.misc.anr_missed_pings"),
            QStringLiteral("hyprland.misc.size_limits_tiled"),
            QStringLiteral("hyprland.misc.always_follow_on_dnd"),
            QStringLiteral("hyprland.misc.focus_on_activate"),
            QStringLiteral("hyprland.misc.mouse_move_focuses_monitor"),
            QStringLiteral("hyprland.misc.on_focus_under_fullscreen"),
            QStringLiteral(
                "hyprland.misc.exit_window_retains_fullscreen"
            ),
            QStringLiteral("hyprland.misc.enable_swallow"),
            QStringLiteral("hyprland.misc.swallow_regex"),
            QStringLiteral("hyprland.misc.swallow_exception_regex"),
            QStringLiteral("hyprland.input.follow_mouse_threshold"),
        };
        QCOMPARE(accepted->windowsOptionIds(), expectedWindowsIds);
        QCOMPARE(accepted->windowsOptions().size(), 110);

        const QStringList expectedWorkspacesIds{
            QStringLiteral("hyprland.animations.workspace_wraparound"),
            QStringLiteral("hyprland.gestures.workspace_swipe_cancel_ratio"),
            QStringLiteral("hyprland.gestures.workspace_swipe_create_new"),
            QStringLiteral("hyprland.gestures.workspace_swipe_direction_lock"),
            QStringLiteral(
                "hyprland.gestures.workspace_swipe_direction_lock_threshold"
            ),
            QStringLiteral("hyprland.gestures.workspace_swipe_distance"),
            QStringLiteral("hyprland.gestures.workspace_swipe_forever"),
            QStringLiteral("hyprland.gestures.workspace_swipe_invert"),
            QStringLiteral(
                "hyprland.gestures.workspace_swipe_min_speed_to_force"
            ),
            QStringLiteral("hyprland.gestures.workspace_swipe_touch"),
            QStringLiteral("hyprland.gestures.workspace_swipe_touch_invert"),
            QStringLiteral("hyprland.gestures.workspace_swipe_use_r"),
            QStringLiteral("hyprland.misc.close_special_on_empty"),
            QStringLiteral("hyprland.misc.initial_workspace_tracking"),
            QStringLiteral("hyprland.misc.initial_workspace_token_timeout"),
            QStringLiteral("hyprland.binds.allow_workspace_cycles"),
            QStringLiteral(
                "hyprland.binds.hide_special_on_workspace_change"
            ),
            QStringLiteral("hyprland.binds.workspace_back_and_forth"),
            QStringLiteral("hyprland.binds.workspace_center_on"),
            QStringLiteral("hyprland.cursor.warp_on_change_workspace"),
            QStringLiteral("hyprland.cursor.warp_on_toggle_special"),
        };
        QVERIFY(accepted->workspacesContractAvailable());
        QVERIFY(accepted->workspacesContractError().isEmpty());
        QCOMPARE(accepted->workspacesOptionIds(), expectedWorkspacesIds);
        QCOMPARE(accepted->workspacesOptions().size(), 21);

        const QStringList expectedAdvancedIds{
            QStringLiteral("hyprland.misc.allow_session_lock_restore"),
            QStringLiteral("hyprland.misc.lockdead_screen_delay"),
            QStringLiteral("hyprland.misc.disable_scale_notification"),
            QStringLiteral("hyprland.misc.render_unfocused_fps"),
            QStringLiteral("hyprland.misc.screencopy_force_8b"),
            QStringLiteral("hyprland.misc.disable_hyprland_logo"),
            QStringLiteral("hyprland.misc.disable_splash_rendering"),
            QStringLiteral("hyprland.misc.session_lock_xray"),
            QStringLiteral("hyprland.misc.session_lock_blur"),
            QStringLiteral("hyprland.xwayland.use_nearest_neighbor"),
            QStringLiteral("hyprland.render.expand_undersized_textures"),
            QStringLiteral("hyprland.render.direct_scanout"),
            QStringLiteral("hyprland.render.fp16_sdr_tf"),
            QStringLiteral("hyprland.render.xp_mode"),
            QStringLiteral("hyprland.input-capture.capture_modifiers"),
            QStringLiteral("hyprland.input-capture.enforce_barriers"),
        };
        QCOMPARE(accepted->advancedOptionIds(), expectedAdvancedIds);
        QCOMPARE(accepted->advancedOptions().size(), 16);

        QSet<QString> uniqueIds;
        for (const auto &ids : {
                 accepted->appearanceOptionIds(),
                 accepted->inputOptionIds(),
                 accepted->windowsOptionIds(),
                 accepted->workspacesOptionIds(),
                 accepted->advancedOptionIds(),
             }) {
            for (const auto &id : ids) {
                QVERIFY2(!uniqueIds.contains(id), qPrintable(id));
                uniqueIds.insert(id);
            }
        }
        QCOMPARE(uniqueIds.size(), 236);
        qsizetype safeCount = 0;
        for (const auto &id : uniqueIds) {
            const auto *option = HyprShelld::Hyprland::findOption(
                accepted->catalog(), id
            );
            QVERIFY2(option != nullptr, qPrintable(id));
            if (option->risk == HyprShelld::Hyprland::RiskLevel::Safe) {
                ++safeCount;
            }
        }
        QCOMPARE(safeCount, 229);
        QCOMPARE(uniqueIds.size() - safeCount, 7);

        const auto verifyMetadata = [&accepted](
            const QStringList &ids,
            const QVariantList &options,
            const QVariantMap &defaults,
            const auto optionLookup
        ) {
            QCOMPARE(options.size(), ids.size());
            for (qsizetype index = 0; index < options.size(); ++index) {
                const auto id = ids.at(index);
                const auto metadata = options.at(index).toMap();
                const auto *option = (accepted.operator->()->*optionLookup)(id);
                QVERIFY2(option != nullptr, qPrintable(id));
                QCOMPARE(metadata.value(QStringLiteral("id")).toString(), id);
                QCOMPARE(metadata.value(QStringLiteral("type")).toString(),
                         HyprShelld::Hyprland::toString(option->type));
                QCOMPARE(metadata.value(QStringLiteral("control")).toString(),
                         HyprShelld::Hyprland::toString(option->control));
                QCOMPARE(metadata.value(QStringLiteral("risk")).toString(),
                         HyprShelld::Hyprland::toString(option->risk));
                QCOMPARE(
                    QJsonValue::fromVariant(metadata.value(
                        QStringLiteral("defaultValue")
                    )),
                    QJsonValue::fromVariant(defaults.value(id))
                );
                QVERIFY(!metadata.value(QStringLiteral("description"))
                             .toString().isEmpty());
                QVERIFY(!metadata.value(QStringLiteral("documentation"))
                             .toString().isEmpty());
                QVERIFY(!metadata.contains(QStringLiteral("step")));
            }
        };
        verifyMetadata(
            expectedInputIds,
            accepted->inputOptions(),
            inputDefaults(),
            &HyprShelld::CompositorOptionCatalog::inputOption
        );
        verifyMetadata(
            expectedWindowsIds,
            accepted->windowsOptions(),
            windowsDefaults(),
            &HyprShelld::CompositorOptionCatalog::windowsOption
        );
        verifyMetadata(
            expectedWorkspacesIds,
            accepted->workspacesOptions(),
            workspacesDefaults(),
            &HyprShelld::CompositorOptionCatalog::workspacesOption
        );
        verifyMetadata(
            expectedAdvancedIds,
            accepted->advancedOptions(),
            advancedDefaults(),
            &HyprShelld::CompositorOptionCatalog::advancedOption
        );

        const auto advancedOptions = accepted->advancedOptions();
        for (const auto index : {
                 qsizetype(0), qsizetype(2), qsizetype(4), qsizetype(5),
                 qsizetype(6), qsizetype(7), qsizetype(8), qsizetype(9),
                 qsizetype(10), qsizetype(13), qsizetype(14), qsizetype(15),
             }) {
            const auto option = advancedOptions.at(index).toMap();
            QCOMPARE(option.value(QStringLiteral("type")).toString(),
                     QStringLiteral("boolean"));
            QCOMPARE(option.value(QStringLiteral("control")).toString(),
                     QStringLiteral("toggle"));
            QVERIFY(!option.contains(QStringLiteral("min")));
            QVERIFY(!option.contains(QStringLiteral("max")));
        }
        const auto lockDelay = advancedOptions.at(1).toMap();
        QCOMPARE(lockDelay.value(QStringLiteral("type")).toString(),
                 QStringLiteral("integer"));
        QCOMPARE(lockDelay.value(QStringLiteral("control")).toString(),
                 QStringLiteral("spinBox"));
        QCOMPARE(lockDelay.value(QStringLiteral("defaultValue")).toInt(), 1000);
        QCOMPARE(lockDelay.value(QStringLiteral("min")).toInt(), 0);
        QCOMPARE(lockDelay.value(QStringLiteral("max")).toInt(), 5000);
        const auto unfocusedFps = advancedOptions.at(3).toMap();
        QCOMPARE(unfocusedFps.value(QStringLiteral("type")).toString(),
                 QStringLiteral("integer"));
        QCOMPARE(unfocusedFps.value(QStringLiteral("control")).toString(),
                 QStringLiteral("spinBox"));
        QCOMPARE(unfocusedFps.value(QStringLiteral("defaultValue")).toInt(), 15);
        QCOMPARE(unfocusedFps.value(QStringLiteral("min")).toInt(), 1);
        QCOMPARE(unfocusedFps.value(QStringLiteral("max")).toInt(), 120);
        const auto directScanout = advancedOptions.at(11).toMap();
        QCOMPARE(directScanout.value(QStringLiteral("type")).toString(),
                 QStringLiteral("enum"));
        QCOMPARE(directScanout.value(QStringLiteral("control")).toString(),
                 QStringLiteral("select"));
        QCOMPARE(directScanout.value(QStringLiteral("defaultValue")).toInt(),
                 0);
        QCOMPARE(directScanout.value(QStringLiteral("min")).toInt(), 0);
        QCOMPARE(directScanout.value(QStringLiteral("max")).toInt(), 2);
        QCOMPARE(
            directScanout.value(QStringLiteral("choices")).toList(),
            QVariantList({
                QVariantMap({
                    {QStringLiteral("label"), QStringLiteral("disable")},
                    {QStringLiteral("value"), 0},
                }),
                QVariantMap({
                    {QStringLiteral("label"), QStringLiteral("enable")},
                    {QStringLiteral("value"), 1},
                }),
                QVariantMap({
                    {QStringLiteral("label"), QStringLiteral("auto")},
                    {QStringLiteral("value"), 2},
                }),
            })
        );
        const auto fp16SdrTf = advancedOptions.at(12).toMap();
        QCOMPARE(fp16SdrTf.value(QStringLiteral("type")).toString(),
                 QStringLiteral("enum"));
        QCOMPARE(fp16SdrTf.value(QStringLiteral("control")).toString(),
                 QStringLiteral("select"));
        QCOMPARE(fp16SdrTf.value(QStringLiteral("defaultValue")).toInt(), 0);
        QCOMPARE(fp16SdrTf.value(QStringLiteral("min")).toInt(), 0);
        QCOMPARE(fp16SdrTf.value(QStringLiteral("max")).toInt(), 1);
        QCOMPARE(
            fp16SdrTf.value(QStringLiteral("choices")).toList(),
            QVariantList({
                QVariantMap({
                    {QStringLiteral("label"), QStringLiteral("monitor")},
                    {QStringLiteral("value"), 0},
                }),
                QVariantMap({
                    {QStringLiteral("label"), QStringLiteral("linear")},
                    {QStringLiteral("value"), 1},
                }),
            })
        );
        const auto xpMode = advancedOptions.at(13).toMap();
        QCOMPARE(xpMode.value(QStringLiteral("type")).toString(),
                 QStringLiteral("boolean"));
        QCOMPARE(xpMode.value(QStringLiteral("control")).toString(),
                 QStringLiteral("toggle"));
        QCOMPARE(xpMode.value(QStringLiteral("defaultValue")).toBool(), false);
        QCOMPARE(xpMode.value(QStringLiteral("risk")).toString(),
                 QStringLiteral("caution"));
        QVERIFY(!xpMode.contains(QStringLiteral("min")));
        QVERIFY(!xpMode.contains(QStringLiteral("max")));
        const auto captureModifiers = advancedOptions.at(14).toMap();
        QCOMPARE(captureModifiers.value(QStringLiteral("defaultValue")).toBool(),
                 false);
        QCOMPARE(captureModifiers.value(QStringLiteral("risk")).toString(),
                 QStringLiteral("caution"));
        const auto enforceBarriers = advancedOptions.at(15).toMap();
        QCOMPARE(enforceBarriers.value(QStringLiteral("defaultValue")).toBool(),
                 true);
        QCOMPARE(enforceBarriers.value(QStringLiteral("risk")).toString(),
                 QStringLiteral("caution"));
        for (const auto &id : expectedAdvancedIds) {
            const auto *option = accepted->advancedOption(id);
            QVERIFY2(option != nullptr, qPrintable(id));
            const auto isNearestNeighbor = id == QStringLiteral(
                "hyprland.xwayland.use_nearest_neighbor"
            );
            const auto expandsUndersizedTextures = id == QStringLiteral(
                "hyprland.render.expand_undersized_textures"
            );
            const auto isDirectScanout = id == QStringLiteral(
                "hyprland.render.direct_scanout"
            );
            const auto isFp16SdrTf = id == QStringLiteral(
                "hyprland.render.fp16_sdr_tf"
            );
            const auto isXpMode = id == QStringLiteral(
                "hyprland.render.xp_mode"
            );
            const auto capturesModifiers = id == QStringLiteral(
                "hyprland.input-capture.capture_modifiers"
            );
            const auto enforcesBarriers = id == QStringLiteral(
                "hyprland.input-capture.enforce_barriers"
            );
            const auto isInputCapture = capturesModifiers || enforcesBarriers;
            const auto isRenderOption = expandsUndersizedTextures
                || isDirectScanout || isFp16SdrTf || isXpMode;
            const auto expectedModule = isNearestNeighbor
                ? QStringLiteral("xwayland")
                : isInputCapture ? QStringLiteral("input")
                : isRenderOption ? QStringLiteral("render")
                                 : QStringLiteral("misc");
            QCOMPARE(option->module, expectedModule);
            QCOMPARE(
                static_cast<int>(option->uiTier),
                static_cast<int>(HyprShelld::Hyprland::UiTier::Advanced)
            );
            QCOMPARE(
                static_cast<int>(option->applyMode),
                static_cast<int>(HyprShelld::Hyprland::ApplyMode::Reload)
            );
            QCOMPARE(
                static_cast<int>(option->risk),
                static_cast<int>(
                    isNearestNeighbor || expandsUndersizedTextures
                            || isDirectScanout || isFp16SdrTf || isXpMode
                            || isInputCapture
                        ? HyprShelld::Hyprland::RiskLevel::Caution
                        : HyprShelld::Hyprland::RiskLevel::Safe
                )
            );
            const auto expectedSince = id
                        == QStringLiteral("hyprland.misc.session_lock_blur")
                    || isInputCapture
                ? HyprShelld::Hyprland::SemanticVersion{0, 56, 0}
                : HyprShelld::Hyprland::SemanticVersion{0, 55, 0};
            QVERIFY(option->since == expectedSince);
            QVERIFY(!option->until.has_value());
            if (isNearestNeighbor) {
                QCOMPARE(
                    option->path,
                    QStringLiteral("xwayland:use_nearest_neighbor")
                );
                QCOMPARE(
                    option->luaPath,
                    QStringList({
                        QStringLiteral("xwayland"),
                        QStringLiteral("use_nearest_neighbor"),
                    })
                );
                QCOMPARE(option->defaultValue, QJsonValue(true));
            }
            if (expandsUndersizedTextures) {
                QCOMPARE(
                    option->path,
                    QStringLiteral("render:expand_undersized_textures")
                );
                QCOMPARE(
                    option->luaPath,
                    QStringList({
                        QStringLiteral("render"),
                        QStringLiteral("expand_undersized_textures"),
                    })
                );
                QCOMPARE(option->defaultValue, QJsonValue(true));
            }
            if (isDirectScanout) {
                QCOMPARE(option->path, QStringLiteral("render:direct_scanout"));
                QCOMPARE(
                    option->luaPath,
                    QStringList({
                        QStringLiteral("render"),
                        QStringLiteral("direct_scanout"),
                    })
                );
                QCOMPARE(option->defaultValue, QJsonValue(0));
            }
            if (isFp16SdrTf) {
                QCOMPARE(option->path, QStringLiteral("render:fp16_sdr_tf"));
                QCOMPARE(
                    option->luaPath,
                    QStringList({
                        QStringLiteral("render"),
                        QStringLiteral("fp16_sdr_tf"),
                    })
                );
                QCOMPARE(option->defaultValue, QJsonValue(0));
            }
            if (isXpMode) {
                QCOMPARE(option->path, QStringLiteral("render:xp_mode"));
                QCOMPARE(
                    option->luaPath,
                    QStringList({
                        QStringLiteral("render"),
                        QStringLiteral("xp_mode"),
                    })
                );
                QCOMPARE(option->defaultValue, QJsonValue(false));
            }
            if (capturesModifiers) {
                QCOMPARE(
                    option->path,
                    QStringLiteral("input-capture:capture_modifiers")
                );
                QCOMPARE(
                    option->luaPath,
                    QStringList({
                        QStringLiteral("input_capture"),
                        QStringLiteral("capture_modifiers"),
                    })
                );
                QCOMPARE(option->defaultValue, QJsonValue(false));
            }
            if (enforcesBarriers) {
                QCOMPARE(
                    option->path,
                    QStringLiteral("input-capture:enforce_barriers")
                );
                QCOMPARE(
                    option->luaPath,
                    QStringList({
                        QStringLiteral("input_capture"),
                        QStringLiteral("enforce_barriers"),
                    })
                );
                QCOMPARE(option->defaultValue, QJsonValue(true));
            }
        }

        const auto windowsOptions = accepted->windowsOptions();

        const auto requireRange = [&windowsOptions](
            const qsizetype index,
            const double defaultValue,
            const double minimum,
            const double maximum
        ) {
            const auto option = windowsOptions.at(index).toMap();
            QCOMPARE(option.value(QStringLiteral("defaultValue")).toDouble(),
                     defaultValue);
            QCOMPARE(option.value(QStringLiteral("min")).toDouble(), minimum);
            QCOMPARE(option.value(QStringLiteral("max")).toDouble(), maximum);
        };
        requireRange(2, 15, 0, 100);
        requireRange(4, 0, 0, 4);
        requireRange(7, 10, 0, 100);
        requireRange(9, 10, 0, 100);
        requireRange(10, 1, 0, 3);
        requireRange(12, 0, 0, 300);
        requireRange(13, 1, 0, 2);
        requireRange(14, 0, 0, 2);
        requireRange(19, 0, 0, 100);
        requireRange(21, 0.1, 0, 1);
        requireRange(22, 1, 0.1, 1.9);
        requireRange(29, 1, 0, 1);
        requireRange(31, 1, 0.1, 3);
        requireRange(39, 0.55, 0, 1);
        requireRange(44, 2, 0, 10);
        requireRange(47, 0.5, 0.1, 1);
        requireRange(51, 0.4, 0, 1);
        requireRange(68, 400, 0, 2147483647);
        requireRange(69, 400, 0, 2147483647);
        requireRange(70, 8, 2, 64);
        requireRange(72, 14, 1, 64);
        requireRange(73, 0, 0, 64);
        requireRange(74, 3, 1, 64);
        requireRange(76, 3, 0, 6);
        requireRange(80, 1, 0, 20);
        requireRange(81, 2, 2, 10);
        requireRange(82, 2, 0, 20);
        requireRange(83, 2, 2, 10);
        requireRange(86, 2, 0, 20);
        requireRange(87, 2, 0, 20);
        requireRange(89, 0, -20, 20);
        requireRange(90, 0, 0, 22);
        requireRange(93, 0, 0, 1);
        requireRange(104, 2, 0, 2);
        requireRange(109, 0, 0, 1000000);

        const auto requireChoices = [&windowsOptions](
            const qsizetype index,
            const QStringList &labels,
            const QVariantList &values
        ) {
            const auto choices = windowsOptions.at(index).toMap()
                                     .value(QStringLiteral("choices"))
                                     .toList();
            QCOMPARE(choices.size(), labels.size());
            for (qsizetype choiceIndex = 0;
                 choiceIndex < choices.size(); ++choiceIndex) {
                const auto choice = choices.at(choiceIndex).toMap();
                QCOMPARE(choice.value(QStringLiteral("label")).toString(),
                         labels.at(choiceIndex));
                if (values.at(choiceIndex).metaType().id()
                    == QMetaType::QString) {
                    QCOMPARE(choice.value(QStringLiteral("value")).toString(),
                             values.at(choiceIndex).toString());
                } else {
                    QCOMPARE(choice.value(QStringLiteral("value")).toInt(),
                             values.at(choiceIndex).toInt());
                }
            }
        };
        requireChoices(
            0,
            {QStringLiteral("dwindle"), QStringLiteral("master"),
             QStringLiteral("scrolling"), QStringLiteral("monocle")},
            {QStringLiteral("dwindle"), QStringLiteral("master"),
             QStringLiteral("scrolling"), QStringLiteral("monocle")}
        );
        requireChoices(
            4,
            {QStringLiteral("disable"), QStringLiteral("top_left"),
             QStringLiteral("top_right"), QStringLiteral("bottom_right"),
             QStringLiteral("bottom_left")},
            {0, 1, 2, 3, 4}
        );
        requireChoices(
            10,
            {QStringLiteral("disabled"), QStringLiteral("follow"),
             QStringLiteral("detached"), QStringLiteral("separate")},
            {0, 1, 2, 3}
        );
        requireChoices(
            13,
            {QStringLiteral("disabled"),
             QStringLiteral("tiled/floating transitions"),
             QStringLiteral("all floating transitions")},
            {0, 1, 2}
        );
        requireChoices(
            14,
            {QStringLiteral("next"), QStringLiteral("cursor"),
             QStringLiteral("mru")},
            {0, 1, 2}
        );
        requireChoices(
            23,
            {QStringLiteral("follow_mouse"), QStringLiteral("left"),
             QStringLiteral("right")},
            {0, 1, 2}
        );
        requireChoices(
            36,
            {QStringLiteral("left"), QStringLiteral("right"),
             QStringLiteral("top"), QStringLiteral("bottom")},
            {QStringLiteral("left"), QStringLiteral("right"),
             QStringLiteral("top"), QStringLiteral("bottom")}
        );
        requireChoices(
            43,
            {QStringLiteral("left"), QStringLiteral("right"),
             QStringLiteral("top"), QStringLiteral("bottom"),
             QStringLiteral("center")},
            {QStringLiteral("left"), QStringLiteral("right"),
             QStringLiteral("top"), QStringLiteral("bottom"),
            QStringLiteral("center")}
        );
        requireChoices(
            60,
            {QStringLiteral("disabled"), QStringLiteral("enabled"),
             QStringLiteral("only when dragging into the groupbar")},
            {0, 1, 2}
        );
        requireChoices(
            93,
            {QStringLiteral("history"),
             QStringLiteral("shared edge length")},
            {0, 1}
        );
        requireChoices(
            104,
            {QStringLiteral("ignore"), QStringLiteral("take_over"),
             QStringLiteral("exit_fullscreen")},
            {0, 1, 2}
        );

        const auto floatGaps = windowsOptions.at(18).toMap();
        QCOMPARE(floatGaps.value(QStringLiteral("type")).toString(),
                 QStringLiteral("cssGap"));
        QCOMPARE(floatGaps.value(QStringLiteral("control")).toString(),
                 QStringLiteral("text"));
        QCOMPARE(floatGaps.value(QStringLiteral("defaultValue")).toList(),
                 QVariantList({0, 0, 0, 0}));
        const auto aspectRatio = windowsOptions.at(20).toMap();
        QCOMPARE(aspectRatio.value(QStringLiteral("type")).toString(),
                 QStringLiteral("vector2"));
        QCOMPARE(aspectRatio.value(QStringLiteral("control")).toString(),
                 QStringLiteral("vector2"));
        QCOMPARE(aspectRatio.value(QStringLiteral("min")).toList(),
                 QVariantList({0, 0}));
        QCOMPARE(aspectRatio.value(QStringLiteral("max")).toList(),
                 QVariantList({1000, 1000}));

        const auto groupbarFontFamily = windowsOptions.at(67).toMap();
        QCOMPARE(
            groupbarFontFamily.value(QStringLiteral("type")).toString(),
            QStringLiteral("string")
        );
        QCOMPARE(
            groupbarFontFamily.value(QStringLiteral("control")).toString(),
            QStringLiteral("text")
        );
        QCOMPARE(
            groupbarFontFamily.value(QStringLiteral("defaultValue")).toString(),
            QString{}
        );
        QCOMPARE(
            groupbarFontFamily.value(QStringLiteral("maxLength")).toInt(),
            4096
        );
        QVERIFY(!groupbarFontFamily.contains(QStringLiteral("min")));
        QVERIFY(!groupbarFontFamily.contains(QStringLiteral("max")));
        QVERIFY(!groupbarFontFamily.contains(QStringLiteral("choices")));

        for (const auto index : {68, 69}) {
            const auto fontWeight = windowsOptions.at(index).toMap();
            QCOMPARE(
                fontWeight.value(QStringLiteral("type")).toString(),
                QStringLiteral("fontWeight")
            );
            QCOMPARE(
                fontWeight.value(QStringLiteral("control")).toString(),
                QStringLiteral("spinBox")
            );
        }
        for (const auto index : {81, 83}) {
            const auto power = windowsOptions.at(index).toMap();
            QCOMPARE(
                power.value(QStringLiteral("type")).toString(),
                QStringLiteral("number")
            );
            QCOMPARE(
                power.value(QStringLiteral("control")).toString(),
                QStringLiteral("slider")
            );
        }

        const QList<std::pair<qsizetype, bool>> groupbarBooleanDefaults{
            {65, true}, {66, false}, {71, false}, {75, false},
            {77, true}, {78, true}, {79, true}, {84, true},
            {85, true}, {88, true}, {91, false},
        };
        for (const auto &[index, defaultValue] : groupbarBooleanDefaults) {
            const auto option = windowsOptions.at(index).toMap();
            QCOMPARE(
                option.value(QStringLiteral("type")).toString(),
                QStringLiteral("boolean")
            );
            QCOMPARE(
                option.value(QStringLiteral("control")).toString(),
                QStringLiteral("toggle")
            );
            QCOMPARE(
                option.value(QStringLiteral("defaultValue")).toBool(),
                defaultValue
            );
            QVERIFY(!option.contains(QStringLiteral("min")));
            QVERIFY(!option.contains(QStringLiteral("max")));
            QVERIFY(!option.contains(QStringLiteral("choices")));
        }

        const QList<std::pair<qsizetype, bool>> bindsBooleanDefaults{
            {92, false}, {94, false}, {95, false},
            {96, false}, {97, true},
        };
        for (const auto &[index, defaultValue] : bindsBooleanDefaults) {
            const auto option = windowsOptions.at(index).toMap();
            QCOMPARE(
                option.value(QStringLiteral("type")).toString(),
                QStringLiteral("boolean")
            );
            QCOMPARE(
                option.value(QStringLiteral("control")).toString(),
                QStringLiteral("toggle")
            );
            QCOMPARE(
                option.value(QStringLiteral("defaultValue")).toBool(),
                defaultValue
            );
            QVERIFY(!option.contains(QStringLiteral("min")));
            QVERIFY(!option.contains(QStringLiteral("max")));
            QVERIFY(!option.contains(QStringLiteral("choices")));
        }

        const auto *advancedVector = accepted->windowsOption(
            QStringLiteral("hyprland.layout.single_window_aspect_ratio")
        );
        const auto *advancedNumlock = accepted->inputOption(
            QStringLiteral("hyprland.input.numlock_by_default")
        );
        const auto *advancedVirtualKeyboardName = accepted->inputOption(
            QStringLiteral("hyprland.misc.name_vk_after_proc")
        );
        const auto *virtualKeyboardShareStates = accepted->inputOption(
            QStringLiteral("hyprland.input.virtualkeyboard.share_states")
        );
        const auto *virtualKeyboardRelease = accepted->inputOption(
            QStringLiteral(
                "hyprland.input.virtualkeyboard.release_pressed_on_close"
            )
        );
        const auto *forceNoAccel = accepted->inputOption(
            QStringLiteral("hyprland.input.force_no_accel")
        );
        const auto *rotation = accepted->inputOption(
            QStringLiteral("hyprland.input.rotation")
        );
        const auto *advancedMiddleClickPaste = accepted->inputOption(
            QStringLiteral("hyprland.misc.middle_click_paste")
        );
        const auto *touchdeviceEnabled = accepted->inputOption(
            QStringLiteral("hyprland.input.touchdevice.enabled")
        );
        const auto *touchdeviceTransform = accepted->inputOption(
            QStringLiteral("hyprland.input.touchdevice.transform")
        );
        const auto *tabletRelativeInput = accepted->inputOption(
            QStringLiteral("hyprland.input.tablet.relative_input")
        );
        const auto *tabletLeftHanded = accepted->inputOption(
            QStringLiteral("hyprland.input.tablet.left_handed")
        );
        const auto *tabletTransform = accepted->inputOption(
            QStringLiteral("hyprland.input.tablet.transform")
        );
        const auto *tabletRegionPosition = accepted->inputOption(
            QStringLiteral("hyprland.input.tablet.region_position")
        );
        const auto *tabletAbsoluteRegionPosition = accepted->inputOption(
            QStringLiteral(
                "hyprland.input.tablet.absolute_region_position"
            )
        );
        const auto *tabletRegionSize = accepted->inputOption(
            QStringLiteral("hyprland.input.tablet.region_size")
        );
        const auto *resolveBindsBySymbol = accepted->inputOption(
            QStringLiteral("hyprland.input.resolve_binds_by_sym")
        );
        const auto *addedMaster = accepted->windowsOption(
            QStringLiteral("hyprland.master.focus_master_on_close")
        );
        const auto *advancedWorkspace = accepted->workspacesOption(
            QStringLiteral("hyprland.misc.initial_workspace_tracking")
        );
        const auto *addedWorkspace = accepted->workspacesOption(
            QStringLiteral("hyprland.misc.initial_workspace_token_timeout")
        );
        const auto *followMouseThreshold = accepted->windowsOption(
            QStringLiteral("hyprland.input.follow_mouse_threshold")
        );
        QVERIFY(advancedVector != nullptr);
        QVERIFY(advancedNumlock != nullptr);
        QVERIFY(advancedVirtualKeyboardName != nullptr);
        QVERIFY(virtualKeyboardShareStates != nullptr);
        QVERIFY(virtualKeyboardRelease != nullptr);
        QVERIFY(forceNoAccel != nullptr);
        QVERIFY(rotation != nullptr);
        QVERIFY(advancedMiddleClickPaste != nullptr);
        QVERIFY(touchdeviceEnabled != nullptr);
        QVERIFY(touchdeviceTransform != nullptr);
        QVERIFY(tabletRelativeInput != nullptr);
        QVERIFY(tabletLeftHanded != nullptr);
        QVERIFY(tabletTransform != nullptr);
        QVERIFY(tabletRegionPosition != nullptr);
        QVERIFY(tabletAbsoluteRegionPosition != nullptr);
        QVERIFY(tabletRegionSize != nullptr);
        QVERIFY(resolveBindsBySymbol != nullptr);
        QVERIFY(addedMaster != nullptr);
        QVERIFY(advancedWorkspace != nullptr);
        QVERIFY(addedWorkspace != nullptr);
        QVERIFY(followMouseThreshold != nullptr);
        for (qsizetype index = 57; index < 65; ++index) {
            const auto *groupOption = accepted->windowsOption(
                expectedWindowsIds.at(index)
            );
            QVERIFY(groupOption != nullptr);
            QCOMPARE(
                static_cast<int>(groupOption->uiTier),
                static_cast<int>(HyprShelld::Hyprland::UiTier::Advanced)
            );
            QCOMPARE(
                static_cast<int>(groupOption->applyMode),
                static_cast<int>(HyprShelld::Hyprland::ApplyMode::Reload)
            );
            QCOMPARE(
                static_cast<int>(groupOption->risk),
                static_cast<int>(HyprShelld::Hyprland::RiskLevel::Safe)
            );
            QVERIFY((groupOption->since
                     == HyprShelld::Hyprland::SemanticVersion{0, 55, 0}));
        }
        for (qsizetype index = 65; index < 92; ++index) {
            const auto *groupbarOption = accepted->windowsOption(
                expectedWindowsIds.at(index)
            );
            QVERIFY(groupbarOption != nullptr);
            const auto suffix = expectedWindowsIds.at(index).mid(
                QStringLiteral("hyprland.group.groupbar.").size()
            );
            QCOMPARE(
                groupbarOption->path,
                QStringLiteral("group:groupbar:") + suffix
            );
            QCOMPARE(groupbarOption->module, QStringLiteral("group"));
            QCOMPARE(
                groupbarOption->luaPath,
                QStringList({
                    QStringLiteral("group"),
                    QStringLiteral("groupbar"),
                    suffix,
                })
            );
            QVERIFY(groupbarOption->writable);
            QCOMPARE(
                static_cast<int>(groupbarOption->defaultPolicy),
                static_cast<int>(
                    HyprShelld::Hyprland::DefaultPolicy::Hyprland
                )
            );
            QVERIFY(!groupbarOption->inheritedDefaultFrom.has_value());
            QCOMPARE(
                static_cast<int>(groupbarOption->uiTier),
                static_cast<int>(HyprShelld::Hyprland::UiTier::Advanced)
            );
            QCOMPARE(
                static_cast<int>(groupbarOption->applyMode),
                static_cast<int>(HyprShelld::Hyprland::ApplyMode::Reload)
            );
            QCOMPARE(
                static_cast<int>(groupbarOption->risk),
                static_cast<int>(HyprShelld::Hyprland::RiskLevel::Safe)
            );
            const auto expectedSince = index == 66
                ? HyprShelld::Hyprland::SemanticVersion{0, 56, 0}
                : HyprShelld::Hyprland::SemanticVersion{0, 55, 0};
            QVERIFY(groupbarOption->since == expectedSince);
            QVERIFY(!groupbarOption->until.has_value());
        }
        for (qsizetype index = 92; index < 98; ++index) {
            const auto *bindsOption = accepted->windowsOption(
                expectedWindowsIds.at(index)
            );
            QVERIFY(bindsOption != nullptr);
            const auto suffix = expectedWindowsIds.at(index).mid(
                QStringLiteral("hyprland.binds.").size()
            );
            QCOMPARE(
                bindsOption->path,
                QStringLiteral("binds:") + suffix
            );
            QCOMPARE(bindsOption->module, QStringLiteral("binds"));
            QCOMPARE(
                bindsOption->luaPath,
                QStringList({QStringLiteral("binds"), suffix})
            );
            QVERIFY(bindsOption->writable);
            QCOMPARE(
                static_cast<int>(bindsOption->defaultPolicy),
                static_cast<int>(
                    HyprShelld::Hyprland::DefaultPolicy::Hyprland
                )
            );
            QVERIFY(!bindsOption->inheritedDefaultFrom.has_value());
            QCOMPARE(
                static_cast<int>(bindsOption->uiTier),
                static_cast<int>(HyprShelld::Hyprland::UiTier::Advanced)
            );
            QCOMPARE(
                static_cast<int>(bindsOption->applyMode),
                static_cast<int>(HyprShelld::Hyprland::ApplyMode::Reload)
            );
            QCOMPARE(
                static_cast<int>(bindsOption->risk),
                static_cast<int>(HyprShelld::Hyprland::RiskLevel::Safe)
            );
            QVERIFY((bindsOption->since
                     == HyprShelld::Hyprland::SemanticVersion{0, 55, 0}));
            QVERIFY(!bindsOption->until.has_value());
        }
        const auto *anrEnabled = accepted->windowsOption(
            QStringLiteral("hyprland.misc.enable_anr_dialog")
        );
        const auto *anrThreshold = accepted->windowsOption(
            QStringLiteral("hyprland.misc.anr_missed_pings")
        );
        QVERIFY(anrEnabled != nullptr);
        QVERIFY(anrThreshold != nullptr);
        QCOMPARE(anrEnabled->path, QStringLiteral("misc:enable_anr_dialog"));
        QCOMPARE(anrThreshold->path, QStringLiteral("misc:anr_missed_pings"));
        QCOMPARE(anrEnabled->module, QStringLiteral("misc"));
        QCOMPARE(anrThreshold->module, QStringLiteral("misc"));
        QCOMPARE(
            anrEnabled->luaPath,
            QStringList({QStringLiteral("misc"),
                         QStringLiteral("enable_anr_dialog")})
        );
        QCOMPARE(
            anrThreshold->luaPath,
            QStringList({QStringLiteral("misc"),
                         QStringLiteral("anr_missed_pings")})
        );
        QCOMPARE(
            static_cast<int>(anrEnabled->uiTier),
            static_cast<int>(HyprShelld::Hyprland::UiTier::Advanced)
        );
        QCOMPARE(
            static_cast<int>(anrThreshold->uiTier),
            static_cast<int>(HyprShelld::Hyprland::UiTier::Advanced)
        );
        QCOMPARE(anrEnabled->defaultValue.toBool(), true);
        QCOMPARE(anrThreshold->defaultValue.toDouble(), 5.0);
        QVERIFY(anrThreshold->constraints.minimum.has_value());
        QVERIFY(anrThreshold->constraints.maximum.has_value());
        QCOMPARE(anrThreshold->constraints.minimum->toDouble(), 1.0);
        QCOMPARE(anrThreshold->constraints.maximum->toDouble(), 20.0);
        const QStringList scalarSuffixes{
            QStringLiteral("size_limits_tiled"),
            QStringLiteral("always_follow_on_dnd"),
            QStringLiteral("focus_on_activate"),
            QStringLiteral("mouse_move_focuses_monitor"),
            QStringLiteral("on_focus_under_fullscreen"),
            QStringLiteral("exit_window_retains_fullscreen"),
            QStringLiteral("enable_swallow"),
            QStringLiteral("swallow_regex"),
            QStringLiteral("swallow_exception_regex"),
        };
        const QVariantList scalarDefaults{
            false, true, false, true, 2, false, false, QString{}, QString{},
        };
        const QStringList scalarTypes{
            QStringLiteral("boolean"), QStringLiteral("boolean"),
            QStringLiteral("boolean"), QStringLiteral("boolean"),
            QStringLiteral("enum"), QStringLiteral("boolean"),
            QStringLiteral("boolean"), QStringLiteral("string"),
            QStringLiteral("string"),
        };
        const QStringList scalarControls{
            QStringLiteral("toggle"), QStringLiteral("toggle"),
            QStringLiteral("toggle"), QStringLiteral("toggle"),
            QStringLiteral("select"), QStringLiteral("toggle"),
            QStringLiteral("toggle"), QStringLiteral("text"),
            QStringLiteral("text"),
        };
        for (qsizetype index = 0; index < scalarSuffixes.size(); ++index) {
            const auto suffix = scalarSuffixes.at(index);
            const auto id = QStringLiteral("hyprland.misc.") + suffix;
            const auto *option = accepted->windowsOption(id);
            QVERIFY2(option != nullptr, qPrintable(id));
            QCOMPARE(option->path, QStringLiteral("misc:") + suffix);
            QCOMPARE(option->module, QStringLiteral("misc"));
            QCOMPARE(
                option->luaPath,
                QStringList({QStringLiteral("misc"), suffix})
            );
            QCOMPARE(
                QJsonValue::fromVariant(option->defaultValue),
                QJsonValue::fromVariant(scalarDefaults.at(index))
            );
            QCOMPARE(
                static_cast<int>(option->uiTier),
                static_cast<int>(HyprShelld::Hyprland::UiTier::Advanced)
            );
            QVERIFY(option->writable);
            QCOMPARE(
                static_cast<int>(option->defaultPolicy),
                static_cast<int>(
                    HyprShelld::Hyprland::DefaultPolicy::Hyprland
                )
            );
            QCOMPARE(
                static_cast<int>(option->applyMode),
                static_cast<int>(HyprShelld::Hyprland::ApplyMode::Reload)
            );
            QCOMPARE(
                static_cast<int>(option->risk),
                static_cast<int>(HyprShelld::Hyprland::RiskLevel::Safe)
            );
            QVERIFY((option->since
                     == HyprShelld::Hyprland::SemanticVersion{0, 55, 0}));
            QVERIFY(!option->until.has_value());
            const auto metadata = windowsOptions.at(100 + index).toMap();
            QCOMPARE(metadata.value(QStringLiteral("type")).toString(),
                     scalarTypes.at(index));
            QCOMPARE(metadata.value(QStringLiteral("control")).toString(),
                     scalarControls.at(index));
            if (index >= 7) {
                QCOMPARE(
                    metadata.value(QStringLiteral("maxLength")).toInt(),
                    4096
                );
                QVERIFY(option->constraints.maximumLength.has_value());
                QCOMPARE(*option->constraints.maximumLength, 4096U);
            } else {
                QVERIFY(!metadata.contains(QStringLiteral("maxLength")));
            }
        }
        QCOMPARE(static_cast<int>(advancedVector->uiTier),
                 static_cast<int>(HyprShelld::Hyprland::UiTier::Advanced));
        QCOMPARE(
            static_cast<int>(advancedNumlock->uiTier),
            static_cast<int>(HyprShelld::Hyprland::UiTier::Advanced)
        );
        QCOMPARE(
            static_cast<int>(advancedVirtualKeyboardName->uiTier),
            static_cast<int>(HyprShelld::Hyprland::UiTier::Advanced)
        );
        const auto requireInputContract = [](
            const HyprShelld::Hyprland::OptionDefinition *option,
            const QString &path,
            const QString &module,
            const QStringList &luaPath,
            const HyprShelld::Hyprland::UiTier tier
        ) {
            QCOMPARE(option->path, path);
            QCOMPARE(option->module, module);
            QCOMPARE(option->luaPath, luaPath);
            QVERIFY(option->writable);
            QCOMPARE(
                static_cast<int>(option->defaultPolicy),
                static_cast<int>(
                    HyprShelld::Hyprland::DefaultPolicy::Hyprland
                )
            );
            QCOMPARE(static_cast<int>(option->uiTier), static_cast<int>(tier));
            QCOMPARE(
                static_cast<int>(option->applyMode),
                static_cast<int>(HyprShelld::Hyprland::ApplyMode::Reload)
            );
            QCOMPARE(
                static_cast<int>(option->risk),
                static_cast<int>(HyprShelld::Hyprland::RiskLevel::Safe)
            );
            QVERIFY((option->since
                     == HyprShelld::Hyprland::SemanticVersion{0, 55, 0}));
            QVERIFY(!option->until.has_value());
        };
        requireInputContract(
            advancedNumlock,
            QStringLiteral("input:numlock_by_default"),
            QStringLiteral("input"),
            {QStringLiteral("input"), QStringLiteral("numlock_by_default")},
            HyprShelld::Hyprland::UiTier::Advanced
        );
        requireInputContract(
            virtualKeyboardShareStates,
            QStringLiteral("input:virtualkeyboard:share_states"),
            QStringLiteral("input"),
            {QStringLiteral("input"), QStringLiteral("virtualkeyboard"),
             QStringLiteral("share_states")},
            HyprShelld::Hyprland::UiTier::Common
        );
        requireInputContract(
            virtualKeyboardRelease,
            QStringLiteral("input:virtualkeyboard:release_pressed_on_close"),
            QStringLiteral("input"),
            {QStringLiteral("input"), QStringLiteral("virtualkeyboard"),
             QStringLiteral("release_pressed_on_close")},
            HyprShelld::Hyprland::UiTier::Common
        );
        requireInputContract(
            advancedVirtualKeyboardName,
            QStringLiteral("misc:name_vk_after_proc"),
            QStringLiteral("misc"),
            {QStringLiteral("misc"), QStringLiteral("name_vk_after_proc")},
            HyprShelld::Hyprland::UiTier::Advanced
        );
        requireInputContract(
            forceNoAccel,
            QStringLiteral("input:force_no_accel"),
            QStringLiteral("input"),
            {QStringLiteral("input"), QStringLiteral("force_no_accel")},
            HyprShelld::Hyprland::UiTier::Common
        );
        requireInputContract(
            rotation,
            QStringLiteral("input:rotation"),
            QStringLiteral("input"),
            {QStringLiteral("input"), QStringLiteral("rotation")},
            HyprShelld::Hyprland::UiTier::Common
        );
        requireInputContract(
            followMouseThreshold,
            QStringLiteral("input:follow_mouse_threshold"),
            QStringLiteral("input"),
            {QStringLiteral("input"),
             QStringLiteral("follow_mouse_threshold")},
            HyprShelld::Hyprland::UiTier::Advanced
        );
        requireInputContract(
            advancedMiddleClickPaste,
            QStringLiteral("misc:middle_click_paste"),
            QStringLiteral("misc"),
            {QStringLiteral("misc"), QStringLiteral("middle_click_paste")},
            HyprShelld::Hyprland::UiTier::Advanced
        );
        requireInputContract(
            touchdeviceEnabled,
            QStringLiteral("input:touchdevice:enabled"),
            QStringLiteral("input"),
            {QStringLiteral("input"), QStringLiteral("touchdevice"),
             QStringLiteral("enabled")},
            HyprShelld::Hyprland::UiTier::Advanced
        );
        requireInputContract(
            touchdeviceTransform,
            QStringLiteral("input:touchdevice:transform"),
            QStringLiteral("input"),
            {QStringLiteral("input"), QStringLiteral("touchdevice"),
             QStringLiteral("transform")},
            HyprShelld::Hyprland::UiTier::Advanced
        );
        requireInputContract(
            tabletRelativeInput,
            QStringLiteral("input:tablet:relative_input"),
            QStringLiteral("input"),
            {QStringLiteral("input"), QStringLiteral("tablet"),
             QStringLiteral("relative_input")},
            HyprShelld::Hyprland::UiTier::Advanced
        );
        requireInputContract(
            tabletLeftHanded,
            QStringLiteral("input:tablet:left_handed"),
            QStringLiteral("input"),
            {QStringLiteral("input"), QStringLiteral("tablet"),
             QStringLiteral("left_handed")},
            HyprShelld::Hyprland::UiTier::Advanced
        );
        requireInputContract(
            tabletTransform,
            QStringLiteral("input:tablet:transform"),
            QStringLiteral("input"),
            {QStringLiteral("input"), QStringLiteral("tablet"),
             QStringLiteral("transform")},
            HyprShelld::Hyprland::UiTier::Advanced
        );
        requireInputContract(
            tabletRegionPosition,
            QStringLiteral("input:tablet:region_position"),
            QStringLiteral("input"),
            {QStringLiteral("input"), QStringLiteral("tablet"),
             QStringLiteral("region_position")},
            HyprShelld::Hyprland::UiTier::Advanced
        );
        requireInputContract(
            tabletAbsoluteRegionPosition,
            QStringLiteral("input:tablet:absolute_region_position"),
            QStringLiteral("input"),
            {QStringLiteral("input"), QStringLiteral("tablet"),
             QStringLiteral("absolute_region_position")},
            HyprShelld::Hyprland::UiTier::Advanced
        );
        requireInputContract(
            tabletRegionSize,
            QStringLiteral("input:tablet:region_size"),
            QStringLiteral("input"),
            {QStringLiteral("input"), QStringLiteral("tablet"),
             QStringLiteral("region_size")},
            HyprShelld::Hyprland::UiTier::Advanced
        );
        requireInputContract(
            resolveBindsBySymbol,
            QStringLiteral("input:resolve_binds_by_sym"),
            QStringLiteral("input"),
            {QStringLiteral("input"),
             QStringLiteral("resolve_binds_by_sym")},
            HyprShelld::Hyprland::UiTier::Common
        );
        for (qsizetype index = 37; index < 45; ++index) {
            const auto id = expectedInputIds.at(index);
            const auto suffix = id.mid(QStringLiteral("hyprland.cursor.").size());
            const auto *option = accepted->inputOption(id);
            QVERIFY2(option != nullptr, qPrintable(id));
            requireInputContract(
                option,
                QStringLiteral("cursor:") + suffix,
                QStringLiteral("cursor"),
                {QStringLiteral("cursor"), suffix},
                HyprShelld::Hyprland::UiTier::Advanced
            );
            QVERIFY(!option->inheritedDefaultFrom.has_value());
        }
        QCOMPARE(static_cast<int>(advancedWorkspace->uiTier),
                 static_cast<int>(HyprShelld::Hyprland::UiTier::Advanced));
        QVERIFY((addedMaster->since
                 == HyprShelld::Hyprland::SemanticVersion{0, 56, 0}));
        QVERIFY((addedWorkspace->since
                 == HyprShelld::Hyprland::SemanticVersion{0, 56, 0}));

        QVERIFY(!HyprShelld::CompositorOptionCatalog::fromBytes(
            readBytes(QStringLiteral(HYPRSHELLD_HYPRLAND_CATALOG_FILE)),
            parsed.value->digest,
            parsed.value->digest,
            error
        ));
        QVERIFY(!HyprShelld::CompositorOptionCatalog::fromBytes(
            canonical,
            QString(64, QLatin1Char('a')),
            parsed.value->digest,
            error
        ));
        QVERIFY(!HyprShelld::CompositorOptionCatalog::fromBytes(
            canonical,
            parsed.value->digest,
            QString(64, QLatin1Char('a')),
            error
        ));
        QVERIFY(!HyprShelld::CompositorOptionCatalog::fromBytes(
            QByteArray(HyprShelld::Hyprland::maximumCatalogBytes + 1, 'x'),
            parsed.value->digest,
            parsed.value->digest,
            error
        ));
    }

    void rejectsEveryShadowAndGlowRenderingCatalogContractMutation()
    {
        const auto trusted = trustedCatalog();
        QVariantList baselineMetadata;
        QString baselineError;
        QVERIFY2(
            HyprShelld::Internal::qualifyAppearanceCatalogContract(
                trusted.catalog(), baselineMetadata, baselineError
            ),
            qPrintable(baselineError)
        );
        QCOMPARE(baselineMetadata.size(), 40);
        QVERIFY(baselineError.isEmpty());

        const auto rejectsMutation = [&trusted](
            const QByteArray &label,
            const QString &id,
            const auto &mutate
        ) {
            auto catalog = trusted.catalog();
            HyprShelld::Hyprland::OptionDefinition *target = nullptr;
            for (auto &option : catalog.options) {
                if (option.id == id) {
                    target = &option;
                    break;
                }
            }
            QVERIFY2(target != nullptr, label.constData());
            mutate(*target);

            QVariantList metadata;
            QString error;
            QVERIFY2(
                !HyprShelld::Internal::qualifyAppearanceCatalogContract(
                    catalog, metadata, error
                ),
                label.constData()
            );
            QVERIFY2(metadata.isEmpty(), label.constData());
            QCOMPARE(
                error,
                QStringLiteral(
                    "The compositor appearance option contract is unsupported"
                )
            );
        };

        rejectsMutation(
            "range type",
            QStringLiteral("hyprland.decoration.shadow.range"),
            [](HyprShelld::Hyprland::OptionDefinition &option) {
                option.type = HyprShelld::Hyprland::OptionType::Number;
            }
        );
        rejectsMutation(
            "range control",
            QStringLiteral("hyprland.decoration.shadow.range"),
            [](HyprShelld::Hyprland::OptionDefinition &option) {
                option.control = HyprShelld::Hyprland::ControlKind::Slider;
            }
        );
        rejectsMutation(
            "range default",
            QStringLiteral("hyprland.decoration.shadow.range"),
            [](HyprShelld::Hyprland::OptionDefinition &option) {
                option.defaultValue = 5;
            }
        );
        rejectsMutation(
            "range minimum",
            QStringLiteral("hyprland.decoration.shadow.range"),
            [](HyprShelld::Hyprland::OptionDefinition &option) {
                option.constraints.minimum = QJsonValue(1);
            }
        );
        rejectsMutation(
            "range maximum",
            QStringLiteral("hyprland.decoration.shadow.range"),
            [](HyprShelld::Hyprland::OptionDefinition &option) {
                option.constraints.maximum = QJsonValue(99);
            }
        );
        rejectsMutation(
            "render power default",
            QStringLiteral("hyprland.decoration.shadow.render_power"),
            [](HyprShelld::Hyprland::OptionDefinition &option) {
                option.defaultValue = 2;
            }
        );
        rejectsMutation(
            "sharp default",
            QStringLiteral("hyprland.decoration.shadow.sharp"),
            [](HyprShelld::Hyprland::OptionDefinition &option) {
                option.defaultValue = true;
            }
        );
        rejectsMutation(
            "sharp tier",
            QStringLiteral("hyprland.decoration.shadow.sharp"),
            [](HyprShelld::Hyprland::OptionDefinition &option) {
                option.uiTier = HyprShelld::Hyprland::UiTier::Advanced;
            }
        );
        rejectsMutation(
            "render power risk",
            QStringLiteral("hyprland.decoration.shadow.render_power"),
            [](HyprShelld::Hyprland::OptionDefinition &option) {
                option.risk = HyprShelld::Hyprland::RiskLevel::Caution;
            }
        );
        rejectsMutation(
            "render power apply mode",
            QStringLiteral("hyprland.decoration.shadow.render_power"),
            [](HyprShelld::Hyprland::OptionDefinition &option) {
                option.applyMode = HyprShelld::Hyprland::ApplyMode::Restart;
            }
        );
        rejectsMutation(
            "offset type",
            QStringLiteral("hyprland.decoration.shadow.offset"),
            [](HyprShelld::Hyprland::OptionDefinition &option) {
                option.type = HyprShelld::Hyprland::OptionType::Number;
            }
        );
        rejectsMutation(
            "offset control",
            QStringLiteral("hyprland.decoration.shadow.offset"),
            [](HyprShelld::Hyprland::OptionDefinition &option) {
                option.control = HyprShelld::Hyprland::ControlKind::Slider;
            }
        );
        rejectsMutation(
            "offset default",
            QStringLiteral("hyprland.decoration.shadow.offset"),
            [](HyprShelld::Hyprland::OptionDefinition &option) {
                option.defaultValue = QJsonArray{1, 0};
            }
        );
        rejectsMutation(
            "offset minimum",
            QStringLiteral("hyprland.decoration.shadow.offset"),
            [](HyprShelld::Hyprland::OptionDefinition &option) {
                option.constraints.minimum = QJsonArray{-249, -250};
            }
        );
        rejectsMutation(
            "offset maximum",
            QStringLiteral("hyprland.decoration.shadow.offset"),
            [](HyprShelld::Hyprland::OptionDefinition &option) {
                option.constraints.maximum = QJsonArray{250, 249};
            }
        );
        rejectsMutation(
            "offset tier",
            QStringLiteral("hyprland.decoration.shadow.offset"),
            [](HyprShelld::Hyprland::OptionDefinition &option) {
                option.uiTier = HyprShelld::Hyprland::UiTier::Advanced;
            }
        );
        rejectsMutation(
            "offset risk",
            QStringLiteral("hyprland.decoration.shadow.offset"),
            [](HyprShelld::Hyprland::OptionDefinition &option) {
                option.risk = HyprShelld::Hyprland::RiskLevel::Caution;
            }
        );
        rejectsMutation(
            "offset apply mode",
            QStringLiteral("hyprland.decoration.shadow.offset"),
            [](HyprShelld::Hyprland::OptionDefinition &option) {
                option.applyMode = HyprShelld::Hyprland::ApplyMode::Restart;
            }
        );
        rejectsMutation(
            "scale type",
            QStringLiteral("hyprland.decoration.shadow.scale"),
            [](HyprShelld::Hyprland::OptionDefinition &option) {
                option.type = HyprShelld::Hyprland::OptionType::Integer;
            }
        );
        rejectsMutation(
            "scale control",
            QStringLiteral("hyprland.decoration.shadow.scale"),
            [](HyprShelld::Hyprland::OptionDefinition &option) {
                option.control = HyprShelld::Hyprland::ControlKind::SpinBox;
            }
        );
        rejectsMutation(
            "scale default",
            QStringLiteral("hyprland.decoration.shadow.scale"),
            [](HyprShelld::Hyprland::OptionDefinition &option) {
                option.defaultValue = 0.9;
            }
        );
        rejectsMutation(
            "scale minimum",
            QStringLiteral("hyprland.decoration.shadow.scale"),
            [](HyprShelld::Hyprland::OptionDefinition &option) {
                option.constraints.minimum = QJsonValue(0.1);
            }
        );
        rejectsMutation(
            "scale maximum",
            QStringLiteral("hyprland.decoration.shadow.scale"),
            [](HyprShelld::Hyprland::OptionDefinition &option) {
                option.constraints.maximum = QJsonValue(0.9);
            }
        );
        rejectsMutation(
            "scale tier",
            QStringLiteral("hyprland.decoration.shadow.scale"),
            [](HyprShelld::Hyprland::OptionDefinition &option) {
                option.uiTier = HyprShelld::Hyprland::UiTier::Advanced;
            }
        );
        rejectsMutation(
            "scale risk",
            QStringLiteral("hyprland.decoration.shadow.scale"),
            [](HyprShelld::Hyprland::OptionDefinition &option) {
                option.risk = HyprShelld::Hyprland::RiskLevel::Caution;
            }
        );
        rejectsMutation(
            "scale apply mode",
            QStringLiteral("hyprland.decoration.shadow.scale"),
            [](HyprShelld::Hyprland::OptionDefinition &option) {
                option.applyMode = HyprShelld::Hyprland::ApplyMode::Restart;
            }
        );
        rejectsMutation(
            "glow enabled type",
            QStringLiteral("hyprland.decoration.glow.enabled"),
            [](HyprShelld::Hyprland::OptionDefinition &option) {
                option.type = HyprShelld::Hyprland::OptionType::Integer;
            }
        );
        rejectsMutation(
            "glow enabled control",
            QStringLiteral("hyprland.decoration.glow.enabled"),
            [](HyprShelld::Hyprland::OptionDefinition &option) {
                option.control = HyprShelld::Hyprland::ControlKind::SpinBox;
            }
        );
        rejectsMutation(
            "glow enabled default",
            QStringLiteral("hyprland.decoration.glow.enabled"),
            [](HyprShelld::Hyprland::OptionDefinition &option) {
                option.defaultValue = true;
            }
        );
        rejectsMutation(
            "glow range type",
            QStringLiteral("hyprland.decoration.glow.range"),
            [](HyprShelld::Hyprland::OptionDefinition &option) {
                option.type = HyprShelld::Hyprland::OptionType::Number;
            }
        );
        rejectsMutation(
            "glow range control",
            QStringLiteral("hyprland.decoration.glow.range"),
            [](HyprShelld::Hyprland::OptionDefinition &option) {
                option.control = HyprShelld::Hyprland::ControlKind::Slider;
            }
        );
        rejectsMutation(
            "glow range default",
            QStringLiteral("hyprland.decoration.glow.range"),
            [](HyprShelld::Hyprland::OptionDefinition &option) {
                option.defaultValue = 9;
            }
        );
        rejectsMutation(
            "glow range minimum",
            QStringLiteral("hyprland.decoration.glow.range"),
            [](HyprShelld::Hyprland::OptionDefinition &option) {
                option.constraints.minimum = QJsonValue(1);
            }
        );
        rejectsMutation(
            "glow range maximum",
            QStringLiteral("hyprland.decoration.glow.range"),
            [](HyprShelld::Hyprland::OptionDefinition &option) {
                option.constraints.maximum = QJsonValue(99);
            }
        );
        rejectsMutation(
            "glow falloff default",
            QStringLiteral("hyprland.decoration.glow.render_power"),
            [](HyprShelld::Hyprland::OptionDefinition &option) {
                option.defaultValue = 2;
            }
        );
        rejectsMutation(
            "glow falloff minimum",
            QStringLiteral("hyprland.decoration.glow.render_power"),
            [](HyprShelld::Hyprland::OptionDefinition &option) {
                option.constraints.minimum = QJsonValue(0);
            }
        );
        rejectsMutation(
            "glow falloff maximum",
            QStringLiteral("hyprland.decoration.glow.render_power"),
            [](HyprShelld::Hyprland::OptionDefinition &option) {
                option.constraints.maximum = QJsonValue(5);
            }
        );
    }

    void rejectsEveryActiveLayoutShortcutCatalogContractMutation()
    {
        const auto trusted = trustedCatalog();
        QVariantList baselineMetadata;
        QString baselineError;
        QVERIFY2(
            HyprShelld::Internal::qualifyInputCatalogContract(
                trusted.catalog(), baselineMetadata, baselineError
            ),
            qPrintable(baselineError)
        );
        QCOMPARE(baselineMetadata.size(), 49);
        QCOMPARE(
            baselineMetadata.at(47).toMap().value(
                QStringLiteral("id")
            ).toString(),
            QStringLiteral("hyprland.input.tablet.region_size")
        );
        QCOMPARE(
            baselineMetadata.at(48).toMap().value(
                QStringLiteral("id")
            ).toString(),
            QStringLiteral("hyprland.input.resolve_binds_by_sym")
        );
        QVERIFY(baselineError.isEmpty());

        auto reordered = trusted.catalog();
        qsizetype targetIndex = -1;
        for (qsizetype index = 0; index < reordered.options.size(); ++index) {
            if (reordered.options.at(index).id
                == QStringLiteral("hyprland.input.resolve_binds_by_sym")) {
                targetIndex = index;
                break;
            }
        }
        QVERIFY(targetIndex > 0);
        reordered.options.swapItemsAt(0, targetIndex);
        QVariantList reorderedMetadata;
        QString reorderedError;
        QVERIFY2(
            HyprShelld::Internal::qualifyInputCatalogContract(
                reordered, reorderedMetadata, reorderedError
            ),
            qPrintable(reorderedError)
        );
        QCOMPARE(reorderedMetadata, baselineMetadata);

        const auto rejectsMutation = [&trusted](
            const QByteArray &label,
            const auto &mutate
        ) {
            auto catalog = trusted.catalog();
            HyprShelld::Hyprland::OptionDefinition *target = nullptr;
            for (auto &option : catalog.options) {
                if (option.id == QStringLiteral(
                        "hyprland.input.resolve_binds_by_sym"
                    )) {
                    target = &option;
                    break;
                }
            }
            QVERIFY2(target != nullptr, label.constData());
            mutate(*target);

            QVariantList metadata;
            QString error;
            QVERIFY2(
                !HyprShelld::Internal::qualifyInputCatalogContract(
                    catalog, metadata, error
                ),
                label.constData()
            );
            QVERIFY2(metadata.isEmpty(), label.constData());
            QCOMPARE(
                error,
                QStringLiteral(
                    "The compositor input option contract is unsupported"
                )
            );
        };

        rejectsMutation(
            "id",
            [](HyprShelld::Hyprland::OptionDefinition &option) {
                option.id = QStringLiteral(
                    "hyprland.input.resolve_binds_by_symbol"
                );
            }
        );
        rejectsMutation(
            "type",
            [](HyprShelld::Hyprland::OptionDefinition &option) {
                option.type = HyprShelld::Hyprland::OptionType::Integer;
            }
        );
        rejectsMutation(
            "control",
            [](HyprShelld::Hyprland::OptionDefinition &option) {
                option.control = HyprShelld::Hyprland::ControlKind::Select;
            }
        );
        rejectsMutation(
            "default",
            [](HyprShelld::Hyprland::OptionDefinition &option) {
                option.defaultValue = true;
            }
        );
        rejectsMutation(
            "tier",
            [](HyprShelld::Hyprland::OptionDefinition &option) {
                option.uiTier = HyprShelld::Hyprland::UiTier::Advanced;
            }
        );
        rejectsMutation(
            "risk",
            [](HyprShelld::Hyprland::OptionDefinition &option) {
                option.risk = HyprShelld::Hyprland::RiskLevel::Caution;
            }
        );
        rejectsMutation(
            "apply mode",
            [](HyprShelld::Hyprland::OptionDefinition &option) {
                option.applyMode = HyprShelld::Hyprland::ApplyMode::Restart;
            }
        );
    }

    void authenticatesTheExactCombinedActionAndSchemaAuthority()
    {
        const auto actionSource = readBytes(
            QStringLiteral(HYPRSHELLD_HYPRLAND_ACTION_CATALOG_FILE)
        );
        const auto schema = readBytes(
            QStringLiteral(HYPRSHELLD_HYPRLAND_CONFIG_SCHEMA_FILE)
        );
        const auto parsed = HyprShelld::Hyprland::parseActionCatalog(
            actionSource, schema
        );
        QVERIFY(parsed);
        const auto canonical =
            HyprShelld::Hyprland::canonicalActionCatalogJson(*parsed.value);
        const auto schemaDigest = QString::fromLatin1(
            QCryptographicHash::hash(
                schema, QCryptographicHash::Sha256
            ).toHex()
        );
        QString error;
        const auto accepted = HyprShelld::CompositorActionCatalog::fromBytes(
            canonical,
            parsed.value->digest,
            parsed.value->digest,
            schema,
            schemaDigest,
            error
        );
        QVERIFY2(accepted, qPrintable(error));
        QCOMPARE(accepted->digest(), parsed.value->digest);
        QCOMPARE(accepted->configSchemaDigest(), schemaDigest);
        QCOMPARE(accepted->catalog().configSchemaDocument, schema);
        QCOMPARE(
            HyprShelld::Hyprland::canonicalActionCatalogJson(
                accepted->catalog()
            ),
            canonical
        );

        QVERIFY(!HyprShelld::CompositorActionCatalog::fromBytes(
            actionSource,
            parsed.value->digest,
            parsed.value->digest,
            schema,
            schemaDigest,
            error
        ));
        QVERIFY(!HyprShelld::CompositorActionCatalog::fromBytes(
            canonical,
            QString(64, QLatin1Char('a')),
            parsed.value->digest,
            schema,
            schemaDigest,
            error
        ));
        QVERIFY(!HyprShelld::CompositorActionCatalog::fromBytes(
            canonical,
            parsed.value->digest,
            parsed.value->digest,
            schema,
            QString(64, QLatin1Char('b')),
            error
        ));
        auto tamperedSchema = schema;
        tamperedSchema.append('\n');
        const auto tamperedSchemaDigest = QString::fromLatin1(
            QCryptographicHash::hash(
                tamperedSchema, QCryptographicHash::Sha256
            ).toHex()
        );
        QVERIFY(!HyprShelld::CompositorActionCatalog::fromBytes(
            canonical,
            parsed.value->digest,
            parsed.value->digest,
            tamperedSchema,
            tamperedSchemaDigest,
            error
        ));
        QVERIFY(!HyprShelld::CompositorActionCatalog::fromBytes(
            {}, parsed.value->digest, parsed.value->digest,
            schema, schemaDigest, error
        ));
        QVERIFY(!HyprShelld::CompositorActionCatalog::fromBytes(
            canonical, parsed.value->digest, parsed.value->digest,
            QByteArray(HyprShelld::Hyprland::maximumActionSchemaBytes + 1, 'x'),
            schemaDigest, error
        ));
        QVERIFY(!HyprShelld::CompositorActionCatalog::fromBytes(
            QByteArray(
                HyprShelld::Hyprland::maximumActionCatalogBytes + 1, 'x'
            ),
            parsed.value->digest,
            parsed.value->digest,
            schema,
            schemaDigest,
            error
        ));
    }

    void atomicallyReplacesOrderedWindowAndLayerRulesAndPreservesEverythingElse()
    {
        const auto catalog = trustedCatalog();
        const auto actions = trustedActionCatalog();
        auto snapshot = snapshotWithValidComplexSurfaces();
        const auto originalWindow = snapshot.value(
            QStringLiteral("windowRules")
        ).toArray().first().toObject();
        const auto originalLayer = snapshot.value(
            QStringLiteral("layerRules")
        ).toArray().first().toObject();
        const auto allWindow = fullWindowRule();
        const auto allLayer = fullLayerRule();
        QCOMPARE(allWindow.value(QStringLiteral("match")).toObject().size(), 18);
        QCOMPARE(allWindow.value(QStringLiteral("effects")).toObject().size(), 56);
        QCOMPARE(allLayer.value(QStringLiteral("match")).toObject().size(), 1);
        QCOMPARE(allLayer.value(QStringLiteral("effects")).toObject().size(), 10);
        QVariantList windowRules;
        windowRules.append(allWindow.toVariantMap());
        windowRules.append(originalWindow.toVariantMap());
        QVariantList layerRules;
        layerRules.append(allLayer.toVariantMap());
        layerRules.append(originalLayer.toVariantMap());

        QString error;
        const auto edit = HyprShelld::CompositorSnapshotEditor::replaceRules(
            snapshot,
            7,
            catalog.digest(),
            actions.digest(),
            catalog,
            actions,
            windowRules,
            layerRules,
            error
        );
        QVERIFY2(edit, qPrintable(error));
        QVERIFY(edit->changed);
        QVERIFY(edit->candidate.endsWith('\n'));
        const auto candidate = QJsonDocument::fromJson(
            edit->candidate
        ).object();
        QCOMPARE(
            candidate.value(QStringLiteral("windowRules")).toArray(),
            QJsonArray::fromVariantList(windowRules)
        );
        QCOMPARE(
            candidate.value(QStringLiteral("layerRules")).toArray(),
            QJsonArray::fromVariantList(layerRules)
        );
        for (const auto &field : snapshot.keys()) {
            if (field != QStringLiteral("windowRules")
                && field != QStringLiteral("layerRules")) {
                QCOMPARE(candidate.value(field), snapshot.value(field));
            }
        }
        QCOMPARE(
            candidate.value(QStringLiteral("workspaceRules")),
            snapshot.value(QStringLiteral("workspaceRules"))
        );
        QCOMPARE(
            candidate.value(QStringLiteral("windowRules")).toArray()
                .first().toObject().value(QStringLiteral("effects"))
                .toObject().value(QStringLiteral("border_size")).toDouble(),
            9007199254740991.0
        );
        QCOMPARE(
            candidate.value(QStringLiteral("layerRules")).toArray()
                .first().toObject().value(QStringLiteral("effects"))
                .toObject().value(QStringLiteral("order")).toDouble(),
            -9007199254740991.0
        );

        const auto unchanged =
            HyprShelld::CompositorSnapshotEditor::replaceRules(
                candidate,
                7,
                catalog.digest(),
                actions.digest(),
                catalog,
                actions,
                windowRules,
                layerRules,
                error
            );
        QVERIFY2(unchanged, qPrintable(error));
        QVERIFY(!unchanged->changed);

        auto reorderedWindows = windowRules;
        reorderedWindows.swapItemsAt(0, 1);
        const auto reordered =
            HyprShelld::CompositorSnapshotEditor::replaceRules(
                candidate,
                7,
                catalog.digest(),
                actions.digest(),
                catalog,
                actions,
                reorderedWindows,
                layerRules,
                error
            );
        QVERIFY2(reordered, qPrintable(error));
        QVERIFY(reordered->changed);
        QCOMPARE(
            QJsonDocument::fromJson(reordered->candidate).object()
                .value(QStringLiteral("windowRules")).toArray()
                .first().toObject().value(QStringLiteral("id")).toString(),
            QStringLiteral("window-browser")
        );
    }

    void rejectsInvalidRulesAndEnforcesAuthorityLimitsAndFullStateValidity()
    {
        const auto catalog = trustedCatalog();
        const auto actions = trustedActionCatalog();
        const auto snapshot = baselineSnapshot();
        const auto validWindow = fullWindowRule().toVariantMap();
        const auto validLayer = fullLayerRule().toVariantMap();
        QString error;
        const auto rejected = [&](
            const QVariantList &windows,
            const QVariantList &layers,
            const QJsonObject &source = QJsonObject{},
            const qulonglong revision = 7,
            const QString &actionDigest = QString{}
        ) {
            return !HyprShelld::CompositorSnapshotEditor::replaceRules(
                source.isEmpty() ? snapshot : source,
                revision,
                catalog.digest(),
                actionDigest.isEmpty() ? actions.digest() : actionDigest,
                catalog,
                actions,
                windows,
                layers,
                error
            );
        };

        auto invalidWindow = validWindow;
        invalidWindow.insert(QStringLiteral("enabled"), QStringLiteral("true"));
        QVERIFY(rejected({invalidWindow}, {validLayer}));
        invalidWindow = validWindow;
        invalidWindow.insert(QStringLiteral("match"), QVariantMap{});
        QVERIFY(rejected({invalidWindow}, {validLayer}));
        invalidWindow = validWindow;
        invalidWindow.insert(QStringLiteral("effects"), QVariantMap{});
        QVERIFY(rejected({invalidWindow}, {validLayer}));
        invalidWindow = validWindow;
        auto invalidMatch = invalidWindow.value(
            QStringLiteral("match")
        ).toMap();
        invalidMatch.insert(QStringLiteral("class"), QStringLiteral("["));
        invalidWindow.insert(QStringLiteral("match"), invalidMatch);
        QVERIFY(rejected({invalidWindow}, {validLayer}));
        QVERIFY2(!error.isEmpty(), "Invalid RE2 must expose a save error");
        invalidWindow = validWindow;
        auto invalidEffects = invalidWindow.value(
            QStringLiteral("effects")
        ).toMap();
        invalidEffects.insert(QStringLiteral("group"), true);
        invalidWindow.insert(QStringLiteral("effects"), invalidEffects);
        QVERIFY(rejected({invalidWindow}, {validLayer}));
        invalidEffects.remove(QStringLiteral("group"));
        invalidEffects.insert(QStringLiteral("exec"), QStringLiteral("rm"));
        invalidWindow.insert(QStringLiteral("effects"), invalidEffects);
        QVERIFY(rejected({invalidWindow}, {validLayer}));
        invalidWindow = validWindow;
        invalidEffects = invalidWindow.value(QStringLiteral("effects")).toMap();
        invalidEffects.insert(QStringLiteral("border_size"), 0.5);
        invalidWindow.insert(QStringLiteral("effects"), invalidEffects);
        QVERIFY(rejected({invalidWindow}, {validLayer}));
        invalidEffects.insert(
            QStringLiteral("border_size"),
            std::numeric_limits<double>::infinity()
        );
        invalidWindow.insert(QStringLiteral("effects"), invalidEffects);
        QVERIFY(rejected({invalidWindow}, {validLayer}));
        invalidEffects.insert(
            QStringLiteral("border_size"), 9007199254740992.0
        );
        invalidWindow.insert(QStringLiteral("effects"), invalidEffects);
        QVERIFY(rejected({invalidWindow}, {validLayer}));

        auto invalidLayer = validLayer;
        auto layerMatch = invalidLayer.value(QStringLiteral("match")).toMap();
        layerMatch.insert(QStringLiteral("namespace"), QStringLiteral("["));
        invalidLayer.insert(QStringLiteral("match"), layerMatch);
        QVERIFY(rejected({validWindow}, {invalidLayer}));
        auto layerEffects = validLayer.value(QStringLiteral("effects")).toMap();
        layerEffects.insert(QStringLiteral("order"), -9007199254740992.0);
        invalidLayer = validLayer;
        invalidLayer.insert(QStringLiteral("effects"), layerEffects);
        QVERIFY(rejected({validWindow}, {invalidLayer}));

        auto duplicateWindow = validWindow;
        duplicateWindow.insert(
            QStringLiteral("id"), validWindow.value(QStringLiteral("id"))
        );
        duplicateWindow.insert(QStringLiteral("name"), QStringLiteral("Other"));
        QVERIFY(rejected({validWindow, duplicateWindow}, {validLayer}));
        duplicateWindow.insert(QStringLiteral("id"), QStringLiteral("other"));
        duplicateWindow.insert(
            QStringLiteral("name"), validWindow.value(QStringLiteral("name"))
        );
        QVERIFY(rejected({validWindow, duplicateWindow}, {validLayer}));

        QVariantList maximumWindows;
        maximumWindows.reserve(HyprShelld::Hyprland::maximumWindowRules);
        for (qsizetype index = 0;
             index < HyprShelld::Hyprland::maximumWindowRules; ++index) {
            maximumWindows.append(QVariantMap{
                {QStringLiteral("id"), QStringLiteral("w-%1").arg(index)},
                {QStringLiteral("name"), QStringLiteral("Window %1").arg(index)},
                {QStringLiteral("enabled"), true},
                {QStringLiteral("match"), QVariantMap{
                    {QStringLiteral("class"), QStringLiteral("^x$")},
                }},
                {QStringLiteral("effects"), QVariantMap{
                    {QStringLiteral("float"), true},
                }},
            });
        }
        const auto maximum =
            HyprShelld::CompositorSnapshotEditor::replaceRules(
                snapshot,
                7,
                catalog.digest(),
                actions.digest(),
                catalog,
                actions,
                maximumWindows,
                {},
                error
            );
        QVERIFY2(maximum, qPrintable(error));
        maximumWindows.append(validWindow);
        QVERIFY(rejected(maximumWindows, {}));

        QVERIFY(rejected({validWindow}, {validLayer}, {}, 6));
        QVERIFY(rejected(
            {validWindow}, {validLayer}, {},
            std::numeric_limits<qulonglong>::max()
        ));
        QVERIFY(rejected(
            {validWindow}, {validLayer}, {}, 7,
            QString(64, QLatin1Char('f'))
        ));

        auto invalidWholeState = snapshotWithValidComplexSurfaces();
        auto animations = invalidWholeState.value(
            QStringLiteral("animations")
        ).toArray();
        auto animation = animations.first().toObject();
        animation.insert(QStringLiteral("curve"), QStringLiteral("missing"));
        animations.replace(0, animation);
        invalidWholeState.insert(QStringLiteral("animations"), animations);
        QVERIFY(rejected(
            {validWindow}, {validLayer}, invalidWholeState
        ));
        auto appearance = appearanceDefaults();
        appearance.insert(QStringLiteral("hyprland.general.border_size"), 2);
        QVERIFY(!HyprShelld::CompositorSnapshotEditor::replaceAppearance(
            invalidWholeState,
            7,
            catalog.digest(),
            actions.digest(),
            catalog,
            actions,
            appearance,
            invalidWholeState.value(QStringLiteral("curves"))
                .toArray().toVariantList(),
            invalidWholeState.value(QStringLiteral("animations"))
                .toArray().toVariantList(),
            error
        ));

        const QVariantMap workspaceRule{
            {QStringLiteral("id"), QStringLiteral("workspace-one")},
            {QStringLiteral("selector"), QStringLiteral("1")},
            {QStringLiteral("enabled"), true},
        };
        QVERIFY(rejected({workspaceRule}, {}));
    }

    void editsOnlyTheFortyAppearanceOverridesAndPreservesForeignRiskyAndComplexState()
    {
        const auto catalog = trustedCatalog();
        QCOMPARE(catalog.appearanceOptions().size(), 40);
        auto snapshot = snapshotWithValidComplexSurfaces();
        auto originalOverrides = snapshot.value(
            QStringLiteral("overrides")
        ).toObject();
        originalOverrides.insert(
            QStringLiteral("hyprland.decoration.glow.enabled"), true
        );
        originalOverrides.insert(
            QStringLiteral("hyprland.decoration.glow.range"), 23
        );
        originalOverrides.insert(
            QStringLiteral("hyprland.decoration.glow.render_power"), 2
        );
        const QJsonObject preservedForeignAppearanceOverrides{
            {
                QStringLiteral("hyprland.decoration.shadow.color"),
                QJsonObject{
                    {QStringLiteral("colors"), QJsonArray{
                         QStringLiteral("0xCC112233"),
                         QStringLiteral("0xAA445566"),
                     }},
                    {QStringLiteral("angle"), 37},
                },
            },
            {
                QStringLiteral("hyprland.decoration.shadow.color_inactive"),
                QJsonObject{
                    {QStringLiteral("colors"), QJsonArray{
                         QStringLiteral("0x88778899"),
                     }},
                    {QStringLiteral("angle"), 0},
                },
            },
            {
                QStringLiteral("hyprland.decoration.glow.color"),
                QJsonObject{
                    {QStringLiteral("colors"), QJsonArray{
                         QStringLiteral("0xEE33CCFF"),
                         QStringLiteral("0xAA224466"),
                     }},
                    {QStringLiteral("angle"), 19},
                },
            },
            {
                QStringLiteral("hyprland.decoration.glow.color_inactive"),
                QJsonObject{
                    {QStringLiteral("colors"), QJsonArray{
                         QStringLiteral("0x99446688"),
                     }},
                    {QStringLiteral("angle"), 0},
                },
            },
        };
        for (auto it = preservedForeignAppearanceOverrides.constBegin();
             it != preservedForeignAppearanceOverrides.constEnd(); ++it) {
            originalOverrides.insert(it.key(), it.value());
        }
        originalOverrides.insert(
            QStringLiteral("hyprland.general.layout"),
            QStringLiteral("master")
        );
        originalOverrides.insert(
            QStringLiteral("hyprland.general.resize_on_border"), true
        );
        originalOverrides.insert(
            QStringLiteral("hyprland.general.snap.enabled"), true
        );
        originalOverrides.insert(
            QStringLiteral("hyprland.input.repeat_rate"), 70
        );
        originalOverrides.insert(
            QStringLiteral("hyprland.render.direct_scanout"), 2
        );
        snapshot.insert(QStringLiteral("overrides"), originalOverrides);
        QString error;
        auto values = catalog.appearanceValues(snapshot, error);
        QVERIFY2(values, qPrintable(error));
        values->insert(QStringLiteral("hyprland.general.border_size"), 6);
        values->insert(QStringLiteral("hyprland.decoration.rounding"), 12);
        values->insert(
            QStringLiteral("hyprland.general.gaps_in"),
            QVariantList{1, 2, 3, 4}
        );
        values->insert(
            QStringLiteral("hyprland.general.gaps_out"),
            QVariantList{0, 6, 7, 8}
        );
        values->insert(
            QStringLiteral("hyprland.decoration.dim_inactive"), true
        );
        values->insert(
            QStringLiteral("hyprland.decoration.dim_strength"), 0.65
        );
        values->insert(
            QStringLiteral("hyprland.decoration.active_opacity"), 0.83
        );
        values->insert(
            QStringLiteral("hyprland.decoration.inactive_opacity"), 0.61
        );
        values->insert(
            QStringLiteral("hyprland.decoration.fullscreen_opacity"), 0.74
        );
        values->insert(
            QStringLiteral("hyprland.decoration.dim_modal"), false
        );
        values->insert(
            QStringLiteral("hyprland.decoration.dim_special"), 0.37
        );
        values->insert(
            QStringLiteral("hyprland.decoration.dim_around"), 0.43
        );
        values->insert(
            QStringLiteral("hyprland.decoration.blur.enabled"), false
        );
        values->insert(
            QStringLiteral("hyprland.decoration.blur.size"), 24
        );
        values->insert(
            QStringLiteral("hyprland.decoration.blur.passes"), 5
        );
        values->insert(
            QStringLiteral("hyprland.decoration.blur.ignore_opacity"), false
        );
        values->insert(
            QStringLiteral("hyprland.decoration.blur.new_optimizations"),
            false
        );
        values->insert(
            QStringLiteral("hyprland.decoration.blur.xray"), true
        );
        values->insert(
            QStringLiteral("hyprland.decoration.blur.special"), true
        );
        values->insert(
            QStringLiteral("hyprland.decoration.blur.popups"), true
        );
        values->insert(
            QStringLiteral("hyprland.decoration.blur.popups_ignorealpha"),
            0.35
        );
        values->insert(
            QStringLiteral("hyprland.decoration.blur.input_methods"), true
        );
        values->insert(
            QStringLiteral(
                "hyprland.decoration.blur.input_methods_ignorealpha"
            ),
            0.45
        );
        const QVariantMap modulationValues{
            {
                QStringLiteral("hyprland.decoration.blur.brightness"),
                1.23456789012345,
            },
            {
                QStringLiteral("hyprland.decoration.blur.contrast"),
                0.87654321098765,
            },
            {
                QStringLiteral("hyprland.decoration.blur.noise"),
                0.012345678901234,
            },
            {
                QStringLiteral("hyprland.decoration.blur.vibrancy"),
                0.23456789012345,
            },
            {
                QStringLiteral(
                    "hyprland.decoration.blur.vibrancy_darkness"
                ),
                0.34567890123456,
            },
        };
        for (auto it = modulationValues.cbegin();
             it != modulationValues.cend(); ++it) {
            values->insert(it.key(), it.value());
        }
        values->insert(
            QStringLiteral("hyprland.decoration.border_part_of_window"),
            false
        );
        values->insert(
            QStringLiteral("hyprland.decoration.rounding_power"),
            7.421
        );
        values->insert(
            QStringLiteral("hyprland.decoration.shadow.range"), 17
        );
        values->insert(
            QStringLiteral("hyprland.decoration.shadow.render_power"), 4
        );
        values->insert(
            QStringLiteral("hyprland.decoration.shadow.sharp"), true
        );
        values->insert(
            QStringLiteral("hyprland.decoration.shadow.offset"),
            QVariantList{125.5, -80.25}
        );
        values->insert(
            QStringLiteral("hyprland.decoration.shadow.scale"),
            0.731234567890123
        );
        values->insert(QStringLiteral("hyprland.animations.enabled"), true);

        const auto edit = HyprShelld::CompositorSnapshotEditor::replaceAppearance(
            snapshot,
            7,
            snapshot.value(QStringLiteral("catalogDigest")).toString(),
            snapshot.value(QStringLiteral("actionCatalogDigest")).toString(),
            catalog, trustedActionCatalog(),
            *values,
            snapshot.value(QStringLiteral("curves")).toArray().toVariantList(),
            snapshot.value(QStringLiteral("animations"))
                .toArray().toVariantList(),
            error
        );
        QVERIFY2(edit, qPrintable(error));
        QVERIFY(edit->changed);
        QVERIFY(edit->candidate.endsWith('\n'));
        QVERIFY(edit->candidate.size()
                <= HyprShelld::Hyprland::maximumDesiredStateBytes);
        const auto candidate = QJsonDocument::fromJson(
            edit->candidate
        ).object();
        QCOMPARE(candidate.value(QStringLiteral("revision")).toString(),
                 QStringLiteral("7"));
        QCOMPARE(candidate.value(QStringLiteral("environment")),
                 snapshot.value(QStringLiteral("environment")));
        for (const auto &field : snapshot.keys()) {
            if (field != QStringLiteral("overrides")) {
                QCOMPARE(candidate.value(field), snapshot.value(field));
            }
        }
        for (const auto &orderedSurface : {
                 QStringLiteral("devices"),
                 QStringLiteral("gestures"),
                 QStringLiteral("workspaceRules"),
                 QStringLiteral("windowRules"),
                 QStringLiteral("layerRules"),
                 QStringLiteral("submaps"),
                 QStringLiteral("bindings"),
             }) {
            QVERIFY(snapshot.value(orderedSurface).isArray());
            QVERIFY(!snapshot.value(orderedSurface).toArray().isEmpty());
            QCOMPARE(
                candidate.value(orderedSurface),
                snapshot.value(orderedSurface)
            );
        }
        const auto overrides = candidate.value(
            QStringLiteral("overrides")
        ).toObject();
        QCOMPARE(overrides.value(
            QStringLiteral("hyprland.misc.disable_hyprland_logo")
        ).toBool(), true);
        QCOMPARE(overrides.value(
            QStringLiteral("hyprland.general.border_size")
        ).toInt(), 6);
        QCOMPARE(overrides.value(
            QStringLiteral("hyprland.decoration.rounding")
        ).toInt(), 12);
        QCOMPARE(overrides.value(
            QStringLiteral("hyprland.general.gaps_in")
        ).toArray(), QJsonArray({1, 2, 3, 4}));
        QCOMPARE(overrides.value(
            QStringLiteral("hyprland.general.gaps_out")
        ).toArray(), QJsonArray({0, 6, 7, 8}));
        QCOMPARE(overrides.value(
            QStringLiteral("hyprland.decoration.dim_inactive")
        ).toBool(), true);
        QCOMPARE(overrides.value(
            QStringLiteral("hyprland.decoration.dim_strength")
        ).toDouble(), 0.65);
        QCOMPARE(overrides.value(
            QStringLiteral("hyprland.decoration.active_opacity")
        ).toDouble(), 0.83);
        QCOMPARE(overrides.value(
            QStringLiteral("hyprland.decoration.inactive_opacity")
        ).toDouble(), 0.61);
        QCOMPARE(overrides.value(
            QStringLiteral("hyprland.decoration.fullscreen_opacity")
        ).toDouble(), 0.74);
        QVERIFY(overrides.contains(
            QStringLiteral("hyprland.decoration.dim_modal")
        ));
        QCOMPARE(overrides.value(
            QStringLiteral("hyprland.decoration.dim_modal")
        ).toBool(), false);
        QCOMPARE(overrides.value(
            QStringLiteral("hyprland.decoration.dim_special")
        ).toDouble(), 0.37);
        QCOMPARE(overrides.value(
            QStringLiteral("hyprland.decoration.dim_around")
        ).toDouble(), 0.43);
        QCOMPARE(overrides.value(
            QStringLiteral("hyprland.decoration.blur.enabled")
        ).toBool(), false);
        QCOMPARE(overrides.value(
            QStringLiteral("hyprland.decoration.blur.size")
        ).toInt(), 24);
        QCOMPARE(overrides.value(
            QStringLiteral("hyprland.decoration.blur.passes")
        ).toInt(), 5);
        QCOMPARE(overrides.value(
            QStringLiteral("hyprland.decoration.blur.ignore_opacity")
        ).toBool(), false);
        QCOMPARE(overrides.value(
            QStringLiteral("hyprland.decoration.blur.new_optimizations")
        ).toBool(), false);
        QCOMPARE(overrides.value(
            QStringLiteral("hyprland.decoration.blur.xray")
        ).toBool(), true);
        QCOMPARE(overrides.value(
            QStringLiteral("hyprland.decoration.blur.special")
        ).toBool(), true);
        QCOMPARE(overrides.value(
            QStringLiteral("hyprland.decoration.blur.popups")
        ).toBool(), true);
        QCOMPARE(overrides.value(
            QStringLiteral("hyprland.decoration.blur.popups_ignorealpha")
        ).toDouble(), 0.35);
        QCOMPARE(overrides.value(
            QStringLiteral("hyprland.decoration.blur.input_methods")
        ).toBool(), true);
        QCOMPARE(overrides.value(QStringLiteral(
            "hyprland.decoration.blur.input_methods_ignorealpha"
        )).toDouble(), 0.45);
        for (auto it = modulationValues.cbegin();
             it != modulationValues.cend(); ++it) {
            QCOMPARE(overrides.value(it.key()).toVariant(), it.value());
        }
        QVERIFY(overrides.contains(QStringLiteral(
            "hyprland.decoration.border_part_of_window"
        )));
        QCOMPARE(overrides.value(QStringLiteral(
            "hyprland.decoration.border_part_of_window"
        )).toBool(), false);
        QCOMPARE(overrides.value(QStringLiteral(
            "hyprland.decoration.rounding_power"
        )).toDouble(), 7.421);
        QCOMPARE(overrides.value(QStringLiteral(
            "hyprland.decoration.shadow.range"
        )).toInt(), 17);
        QCOMPARE(overrides.value(QStringLiteral(
            "hyprland.decoration.shadow.render_power"
        )).toInt(), 4);
        QCOMPARE(overrides.value(QStringLiteral(
            "hyprland.decoration.shadow.sharp"
        )).toBool(), true);
        QCOMPARE(overrides.value(QStringLiteral(
            "hyprland.decoration.shadow.offset"
        )).toArray(), QJsonArray({125.5, -80.25}));
        QCOMPARE(overrides.value(QStringLiteral(
            "hyprland.decoration.shadow.scale"
        )).toDouble(), 0.731234567890123);
        QCOMPARE(overrides.value(QStringLiteral(
            "hyprland.decoration.glow.enabled"
        )).toBool(), true);
        QCOMPARE(overrides.value(QStringLiteral(
            "hyprland.decoration.glow.range"
        )).toInt(), 23);
        QCOMPARE(overrides.value(QStringLiteral(
            "hyprland.decoration.glow.render_power"
        )).toInt(), 2);
        for (auto it = preservedForeignAppearanceOverrides.constBegin();
             it != preservedForeignAppearanceOverrides.constEnd(); ++it) {
            QCOMPARE(overrides.value(it.key()), it.value());
        }
        QCOMPARE(overrides.value(QStringLiteral("hyprland.general.layout")),
                 originalOverrides.value(
                     QStringLiteral("hyprland.general.layout")));
        QCOMPARE(overrides.value(
            QStringLiteral("hyprland.general.resize_on_border")),
            originalOverrides.value(
                QStringLiteral("hyprland.general.resize_on_border")));
        QCOMPARE(overrides.value(
            QStringLiteral("hyprland.general.snap.enabled")),
            originalOverrides.value(
                QStringLiteral("hyprland.general.snap.enabled")));
        QCOMPARE(overrides.value(QStringLiteral("hyprland.input.repeat_rate")),
                 originalOverrides.value(
                     QStringLiteral("hyprland.input.repeat_rate")));
        QCOMPARE(overrides.value(QStringLiteral(
            "hyprland.render.direct_scanout"
        )), originalOverrides.value(QStringLiteral(
            "hyprland.render.direct_scanout"
        )));
        QVERIFY(!overrides.contains(
            QStringLiteral("hyprland.animations.enabled")
        ));
    }

    void preservesDisabledLowGlowAndRejectsChangedUnsafeCandidates()
    {
        const auto catalog = trustedCatalog();
        const auto actions = trustedActionCatalog();
        const auto baseline = baselineSnapshot();
        const auto curves = baseline.value(
            QStringLiteral("curves")
        ).toArray().toVariantList();
        const auto animations = baseline.value(
            QStringLiteral("animations")
        ).toArray().toVariantList();
        const auto replaceAppearance = [&catalog, &actions, &curves,
                                        &animations](
            const QJsonObject &snapshot,
            const QVariantMap &values,
            QString &error
        ) {
            return HyprShelld::CompositorSnapshotEditor::replaceAppearance(
                snapshot,
                7,
                snapshot.value(QStringLiteral("catalogDigest")).toString(),
                snapshot.value(
                    QStringLiteral("actionCatalogDigest")
                ).toString(),
                catalog,
                actions,
                values,
                curves,
                animations,
                error
            );
        };

        for (const int lowRange : {0, 9}) {
            auto values = appearanceDefaults();
            values.insert(
                QStringLiteral("hyprland.decoration.glow.enabled"), false
            );
            values.insert(
                QStringLiteral("hyprland.decoration.glow.range"), lowRange
            );
            values.insert(
                QStringLiteral("hyprland.decoration.glow.render_power"), 4
            );
            QString error;
            const auto edit = replaceAppearance(baseline, values, error);
            QVERIFY2(edit, qPrintable(error));
            QVERIFY(edit->changed);
            const auto overrides = QJsonDocument::fromJson(
                edit->candidate
            ).object().value(QStringLiteral("overrides")).toObject();
            QVERIFY(!overrides.contains(QStringLiteral(
                "hyprland.decoration.glow.enabled"
            )));
            QCOMPARE(overrides.value(QStringLiteral(
                "hyprland.decoration.glow.range"
            )).toInt(), lowRange);
            QCOMPARE(overrides.value(QStringLiteral(
                "hyprland.decoration.glow.render_power"
            )).toInt(), 4);
        }

        auto unsafeValues = appearanceDefaults();
        unsafeValues.insert(
            QStringLiteral("hyprland.decoration.glow.enabled"), true
        );
        unsafeValues.insert(
            QStringLiteral("hyprland.decoration.glow.range"), 9
        );
        QString error;
        QVERIFY(!replaceAppearance(baseline, unsafeValues, error));
        QCOMPARE(
            error,
            QStringLiteral(
                "$.overrides.hyprland.decoration.glow.range: Inner glow can be enabled only when its range is at least 10; disable glow or raise the range."
            )
        );

        auto safeEnabledValues = unsafeValues;
        safeEnabledValues.insert(
            QStringLiteral("hyprland.decoration.glow.range"), 10
        );
        const auto safeEnabled = replaceAppearance(
            baseline, safeEnabledValues, error
        );
        QVERIFY2(safeEnabled, qPrintable(error));
        QVERIFY(safeEnabled->changed);

        auto legacyUnsafe = baseline;
        auto legacyOverrides = legacyUnsafe.value(
            QStringLiteral("overrides")
        ).toObject();
        legacyOverrides.insert(
            QStringLiteral("hyprland.decoration.glow.enabled"), true
        );
        legacyOverrides.insert(
            QStringLiteral("hyprland.decoration.glow.range"), 9
        );
        legacyUnsafe.insert(QStringLiteral("overrides"), legacyOverrides);
        auto projected = catalog.appearanceValues(legacyUnsafe, error);
        QVERIFY2(projected, qPrintable(error));
        QCOMPARE(projected->value(QStringLiteral(
            "hyprland.decoration.glow.enabled"
        )).toBool(), true);
        QCOMPARE(projected->value(QStringLiteral(
            "hyprland.decoration.glow.range"
        )).toInt(), 9);

        const auto exactNoOp = replaceAppearance(
            legacyUnsafe, *projected, error
        );
        QVERIFY2(exactNoOp, qPrintable(error));
        QVERIFY(!exactNoOp->changed);

        auto disableRepair = *projected;
        disableRepair.insert(
            QStringLiteral("hyprland.decoration.glow.enabled"), false
        );
        const auto disabled = replaceAppearance(
            legacyUnsafe, disableRepair, error
        );
        QVERIFY2(disabled, qPrintable(error));
        QVERIFY(disabled->changed);

        auto rangeRepair = *projected;
        rangeRepair.insert(
            QStringLiteral("hyprland.decoration.glow.range"), 10
        );
        const auto raised = replaceAppearance(
            legacyUnsafe, rangeRepair, error
        );
        QVERIFY2(raised, qPrintable(error));
        QVERIFY(raised->changed);

        auto inputValues = catalog.inputValues(legacyUnsafe, error);
        QVERIFY2(inputValues, qPrintable(error));
        inputValues->insert(
            QStringLiteral("hyprland.input.repeat_rate"), 26
        );
        QVERIFY(!HyprShelld::CompositorSnapshotEditor::replaceInput(
            legacyUnsafe,
            7,
            legacyUnsafe.value(
                QStringLiteral("catalogDigest")
            ).toString(),
            legacyUnsafe.value(
                QStringLiteral("actionCatalogDigest")
            ).toString(),
            catalog,
            actions,
            *inputValues,
            snapshotGestures(legacyUnsafe),
            error
        ));
        QVERIFY(error.contains(QStringLiteral("range is at least 10")));

        auto legacyBytes = HyprShelld::Hyprland::JsonSupport::canonicalJson(
            legacyUnsafe
        );
        legacyBytes.append('\n');
        const auto parsedLegacy = HyprShelld::Hyprland::parseDesiredState(
            legacyBytes, catalog.catalog(), actions.catalog()
        );
        QVERIFY(parsedLegacy);
        const auto safetyErrors =
            HyprShelld::Hyprland::validateManagedActivationSafety(
                *parsedLegacy.value, catalog.catalog()
            );
        QCOMPARE(safetyErrors.size(), 1);
        QCOMPARE(
            safetyErrors.constFirst().path,
            QStringLiteral("$.overrides.hyprland.decoration.glow.range")
        );
        QCOMPARE(
            safetyErrors.constFirst().code,
            QStringLiteral("state.unsafe-glow-range")
        );
        QCOMPARE(
            safetyErrors.constFirst().message,
            QStringLiteral(
                "Inner glow can be enabled only when its range is at least 10; disable glow or raise the range."
            )
        );

        const auto originalWindowRules = legacyUnsafe.value(
            QStringLiteral("windowRules")
        ).toArray().toVariantList();
        const auto originalLayerRules = legacyUnsafe.value(
            QStringLiteral("layerRules")
        ).toArray().toVariantList();
        const auto rulesNoOp =
            HyprShelld::CompositorSnapshotEditor::replaceRules(
                legacyUnsafe,
                7,
                legacyUnsafe.value(
                    QStringLiteral("catalogDigest")
                ).toString(),
                legacyUnsafe.value(
                    QStringLiteral("actionCatalogDigest")
                ).toString(),
                catalog,
                actions,
                originalWindowRules,
                originalLayerRules,
                error
            );
        QVERIFY2(rulesNoOp, qPrintable(error));
        QVERIFY(!rulesNoOp->changed);

        auto changedWindowRules = originalWindowRules;
        changedWindowRules.append(fullWindowRule(
            QStringLiteral("window-unsafe-glow-change")
        ).toVariantMap());
        QVERIFY(!HyprShelld::CompositorSnapshotEditor::replaceRules(
            legacyUnsafe,
            7,
            legacyUnsafe.value(
                QStringLiteral("catalogDigest")
            ).toString(),
            legacyUnsafe.value(
                QStringLiteral("actionCatalogDigest")
            ).toString(),
            catalog,
            actions,
            changedWindowRules,
            originalLayerRules,
            error
        ));
        QCOMPARE(
            error,
            QStringLiteral(
                "$.overrides.hyprland.decoration.glow.range: Inner glow can be enabled only when its range is at least 10; disable glow or raise the range."
            )
        );
    }

    void elidesAppearanceDefaultsWithoutTouchingMigratedWindowsOverrides()
    {
        const auto catalog = trustedCatalog();
        auto snapshot = baselineSnapshot();
        auto overrides = snapshot.value(QStringLiteral("overrides")).toObject();
        overrides.insert(QStringLiteral("hyprland.general.border_size"), 6);
        overrides.insert(QStringLiteral("hyprland.decoration.rounding"), 8);
        overrides.insert(
            QStringLiteral("hyprland.decoration.blur.enabled"), false
        );
        overrides.insert(
            QStringLiteral("hyprland.decoration.shadow.enabled"), false
        );
        overrides.insert(
            QStringLiteral("hyprland.decoration.dim_inactive"), true
        );
        overrides.insert(
            QStringLiteral("hyprland.decoration.dim_strength"), 0.8
        );
        overrides.insert(
            QStringLiteral("hyprland.decoration.active_opacity"), 0.83
        );
        overrides.insert(
            QStringLiteral("hyprland.decoration.inactive_opacity"), 0.61
        );
        overrides.insert(
            QStringLiteral("hyprland.decoration.fullscreen_opacity"), 0.74
        );
        overrides.insert(
            QStringLiteral("hyprland.decoration.dim_modal"), false
        );
        overrides.insert(
            QStringLiteral("hyprland.decoration.dim_special"), 0.37
        );
        overrides.insert(
            QStringLiteral("hyprland.decoration.dim_around"), 0.43
        );
        overrides.insert(
            QStringLiteral("hyprland.decoration.blur.size"), 24
        );
        overrides.insert(
            QStringLiteral("hyprland.decoration.blur.passes"), 5
        );
        overrides.insert(
            QStringLiteral("hyprland.decoration.blur.ignore_opacity"), false
        );
        overrides.insert(
            QStringLiteral("hyprland.decoration.blur.new_optimizations"),
            false
        );
        overrides.insert(
            QStringLiteral("hyprland.decoration.blur.xray"), true
        );
        overrides.insert(
            QStringLiteral("hyprland.decoration.blur.special"), true
        );
        overrides.insert(
            QStringLiteral("hyprland.decoration.blur.popups"), true
        );
        overrides.insert(
            QStringLiteral("hyprland.decoration.blur.popups_ignorealpha"),
            0.35
        );
        overrides.insert(
            QStringLiteral("hyprland.decoration.blur.input_methods"), true
        );
        overrides.insert(
            QStringLiteral(
                "hyprland.decoration.blur.input_methods_ignorealpha"
            ),
            0.45
        );
        overrides.insert(
            QStringLiteral("hyprland.decoration.blur.brightness"),
            1.23456789012345
        );
        overrides.insert(
            QStringLiteral("hyprland.decoration.blur.contrast"),
            0.87654321098765
        );
        overrides.insert(
            QStringLiteral("hyprland.decoration.blur.noise"),
            0.012345678901234
        );
        overrides.insert(
            QStringLiteral("hyprland.decoration.blur.vibrancy"),
            0.23456789012345
        );
        overrides.insert(
            QStringLiteral("hyprland.decoration.blur.vibrancy_darkness"),
            0.34567890123456
        );
        overrides.insert(
            QStringLiteral("hyprland.decoration.border_part_of_window"),
            false
        );
        overrides.insert(
            QStringLiteral("hyprland.decoration.rounding_power"),
            7.421
        );
        overrides.insert(
            QStringLiteral("hyprland.decoration.shadow.range"), 17
        );
        overrides.insert(
            QStringLiteral("hyprland.decoration.shadow.render_power"), 4
        );
        overrides.insert(
            QStringLiteral("hyprland.decoration.shadow.sharp"), true
        );
        overrides.insert(
            QStringLiteral("hyprland.general.layout"),
            QStringLiteral("scrolling")
        );
        overrides.insert(
            QStringLiteral("hyprland.general.resize_on_border"), true
        );
        overrides.insert(QStringLiteral("hyprland.general.snap.enabled"), true);
        snapshot.insert(QStringLiteral("overrides"), overrides);

        QString error;
        const auto edit = HyprShelld::CompositorSnapshotEditor::replaceAppearance(
            snapshot,
            7,
            catalog.digest(),
            snapshot.value(QStringLiteral("actionCatalogDigest")).toString(),
            catalog, trustedActionCatalog(),
            appearanceDefaults(),
            snapshot.value(QStringLiteral("curves")).toArray().toVariantList(),
            snapshot.value(QStringLiteral("animations"))
                .toArray().toVariantList(),
            error
        );
        QVERIFY2(edit, qPrintable(error));
        QVERIFY(edit->changed);
        const auto candidateOverrides = QJsonDocument::fromJson(
            edit->candidate
        ).object().value(QStringLiteral("overrides")).toObject();
        for (const auto &id : catalog.appearanceOptionIds()) {
            QVERIFY2(!candidateOverrides.contains(id), qPrintable(id));
        }
        QCOMPARE(candidateOverrides.value(QStringLiteral(
            "hyprland.general.layout"
        )).toString(), QStringLiteral("scrolling"));
        QCOMPARE(candidateOverrides.value(QStringLiteral(
            "hyprland.general.resize_on_border"
        )).toBool(), true);
        QCOMPARE(candidateOverrides.value(QStringLiteral(
            "hyprland.general.snap.enabled"
        )).toBool(), true);

        auto dormantValues = appearanceDefaults();
        dormantValues.insert(
            QStringLiteral("hyprland.decoration.dim_strength"), 0.65
        );
        dormantValues.insert(
            QStringLiteral("hyprland.decoration.active_opacity"), 0.83
        );
        dormantValues.insert(
            QStringLiteral("hyprland.decoration.inactive_opacity"), 0.61
        );
        dormantValues.insert(
            QStringLiteral("hyprland.decoration.fullscreen_opacity"), 0.74
        );
        dormantValues.insert(
            QStringLiteral("hyprland.decoration.dim_modal"), false
        );
        dormantValues.insert(
            QStringLiteral("hyprland.decoration.dim_special"), 0.37
        );
        dormantValues.insert(
            QStringLiteral("hyprland.decoration.dim_around"), 0.43
        );
        dormantValues.insert(
            QStringLiteral("hyprland.decoration.blur.enabled"), false
        );
        dormantValues.insert(
            QStringLiteral("hyprland.decoration.blur.size"), 24
        );
        dormantValues.insert(
            QStringLiteral("hyprland.decoration.blur.passes"), 5
        );
        dormantValues.insert(
            QStringLiteral("hyprland.decoration.blur.ignore_opacity"), false
        );
        dormantValues.insert(
            QStringLiteral("hyprland.decoration.blur.new_optimizations"),
            false
        );
        dormantValues.insert(
            QStringLiteral("hyprland.decoration.blur.xray"), true
        );
        dormantValues.insert(
            QStringLiteral("hyprland.decoration.blur.special"), true
        );
        dormantValues.insert(
            QStringLiteral("hyprland.decoration.blur.popups_ignorealpha"),
            0.35
        );
        dormantValues.insert(
            QStringLiteral(
                "hyprland.decoration.blur.input_methods_ignorealpha"
            ),
            0.45
        );
        const QVariantMap dormantModulationValues{
            {
                QStringLiteral("hyprland.decoration.blur.brightness"),
                1.23456789012345,
            },
            {
                QStringLiteral("hyprland.decoration.blur.contrast"),
                0.87654321098765,
            },
            {
                QStringLiteral("hyprland.decoration.blur.noise"),
                0.012345678901234,
            },
            {
                QStringLiteral("hyprland.decoration.blur.vibrancy"),
                0.23456789012345,
            },
            {
                QStringLiteral(
                    "hyprland.decoration.blur.vibrancy_darkness"
                ),
                0.34567890123456,
            },
        };
        for (auto it = dormantModulationValues.cbegin();
             it != dormantModulationValues.cend(); ++it) {
            dormantValues.insert(it.key(), it.value());
        }
        dormantValues.insert(
            QStringLiteral("hyprland.decoration.border_part_of_window"),
            false
        );
        dormantValues.insert(
            QStringLiteral("hyprland.decoration.rounding_power"),
            7.421
        );
        dormantValues.insert(
            QStringLiteral("hyprland.decoration.shadow.enabled"), false
        );
        dormantValues.insert(
            QStringLiteral("hyprland.decoration.shadow.range"), 19
        );
        dormantValues.insert(
            QStringLiteral("hyprland.decoration.shadow.render_power"), 4
        );
        dormantValues.insert(
            QStringLiteral("hyprland.decoration.shadow.sharp"), true
        );
        dormantValues.insert(
            QStringLiteral("hyprland.decoration.shadow.offset"),
            QVariantList{-125.5, 80.25}
        );
        const auto dormantEdit =
            HyprShelld::CompositorSnapshotEditor::replaceAppearance(
                snapshot,
                7,
                catalog.digest(),
                snapshot.value(QStringLiteral("actionCatalogDigest"))
                    .toString(),
                catalog, trustedActionCatalog(), dormantValues,
                snapshot.value(QStringLiteral("curves"))
                    .toArray().toVariantList(),
                snapshot.value(QStringLiteral("animations"))
                    .toArray().toVariantList(),
                error
            );
        QVERIFY2(dormantEdit, qPrintable(error));
        QVERIFY(dormantEdit->changed);
        const auto dormantOverrides = QJsonDocument::fromJson(
            dormantEdit->candidate
        ).object().value(QStringLiteral("overrides")).toObject();
        QVERIFY(!dormantOverrides.contains(QStringLiteral(
            "hyprland.decoration.dim_inactive"
        )));
        QCOMPARE(dormantOverrides.value(QStringLiteral(
            "hyprland.decoration.dim_strength"
        )).toDouble(), 0.65);
        QCOMPARE(dormantOverrides.value(QStringLiteral(
            "hyprland.decoration.active_opacity"
        )).toDouble(), 0.83);
        QCOMPARE(dormantOverrides.value(QStringLiteral(
            "hyprland.decoration.inactive_opacity"
        )).toDouble(), 0.61);
        QCOMPARE(dormantOverrides.value(QStringLiteral(
            "hyprland.decoration.fullscreen_opacity"
        )).toDouble(), 0.74);
        QVERIFY(dormantOverrides.contains(QStringLiteral(
            "hyprland.decoration.dim_modal"
        )));
        QCOMPARE(dormantOverrides.value(QStringLiteral(
            "hyprland.decoration.dim_modal"
        )).toBool(), false);
        QCOMPARE(dormantOverrides.value(QStringLiteral(
            "hyprland.decoration.dim_special"
        )).toDouble(), 0.37);
        QCOMPARE(dormantOverrides.value(QStringLiteral(
            "hyprland.decoration.dim_around"
        )).toDouble(), 0.43);
        QCOMPARE(dormantOverrides.value(QStringLiteral(
            "hyprland.decoration.blur.enabled"
        )).toBool(), false);
        QCOMPARE(dormantOverrides.value(QStringLiteral(
            "hyprland.decoration.blur.size"
        )).toInt(), 24);
        QCOMPARE(dormantOverrides.value(QStringLiteral(
            "hyprland.decoration.blur.passes"
        )).toInt(), 5);
        QCOMPARE(dormantOverrides.value(QStringLiteral(
            "hyprland.decoration.blur.ignore_opacity"
        )).toBool(), false);
        QCOMPARE(dormantOverrides.value(QStringLiteral(
            "hyprland.decoration.blur.new_optimizations"
        )).toBool(), false);
        QCOMPARE(dormantOverrides.value(QStringLiteral(
            "hyprland.decoration.blur.xray"
        )).toBool(), true);
        QCOMPARE(dormantOverrides.value(QStringLiteral(
            "hyprland.decoration.blur.special"
        )).toBool(), true);
        QVERIFY(!dormantOverrides.contains(QStringLiteral(
            "hyprland.decoration.blur.popups"
        )));
        QCOMPARE(dormantOverrides.value(QStringLiteral(
            "hyprland.decoration.blur.popups_ignorealpha"
        )).toDouble(), 0.35);
        QVERIFY(!dormantOverrides.contains(QStringLiteral(
            "hyprland.decoration.blur.input_methods"
        )));
        QCOMPARE(dormantOverrides.value(QStringLiteral(
            "hyprland.decoration.blur.input_methods_ignorealpha"
        )).toDouble(), 0.45);
        for (auto it = dormantModulationValues.cbegin();
             it != dormantModulationValues.cend(); ++it) {
            QCOMPARE(dormantOverrides.value(it.key()).toVariant(), it.value());
        }
        QVERIFY(dormantOverrides.contains(QStringLiteral(
            "hyprland.decoration.border_part_of_window"
        )));
        QCOMPARE(dormantOverrides.value(QStringLiteral(
            "hyprland.decoration.border_part_of_window"
        )).toBool(), false);
        QCOMPARE(dormantOverrides.value(QStringLiteral(
            "hyprland.decoration.rounding_power"
        )).toDouble(), 7.421);
        QCOMPARE(dormantOverrides.value(QStringLiteral(
            "hyprland.decoration.shadow.enabled"
        )).toBool(), false);
        QCOMPARE(dormantOverrides.value(QStringLiteral(
            "hyprland.decoration.shadow.range"
        )).toInt(), 19);
        QCOMPARE(dormantOverrides.value(QStringLiteral(
            "hyprland.decoration.shadow.render_power"
        )).toInt(), 4);
        QCOMPARE(dormantOverrides.value(QStringLiteral(
            "hyprland.decoration.shadow.sharp"
        )).toBool(), true);
        QCOMPARE(dormantOverrides.value(QStringLiteral(
            "hyprland.decoration.shadow.offset"
        )).toArray(), QJsonArray({-125.5, 80.25}));
        QCOMPARE(dormantOverrides.value(QStringLiteral(
            "hyprland.general.layout"
        )).toString(), QStringLiteral("scrolling"));
        QCOMPARE(dormantOverrides.value(QStringLiteral(
            "hyprland.general.resize_on_border"
        )).toBool(), true);
        QCOMPARE(dormantOverrides.value(QStringLiteral(
            "hyprland.general.snap.enabled"
        )).toBool(), true);
    }

    void editsAppearanceCurvesAndAnimationsAtomicallyButBlocksCurveStructure()
    {
        const auto catalog = trustedCatalog();
        const auto actions = trustedActionCatalog();
        auto snapshot = snapshotWithValidComplexSurfaces();
        const QJsonArray originalCurves{
            QJsonObject{
                {QStringLiteral("id"), QStringLiteral("curve-custom")},
                {QStringLiteral("name"), QStringLiteral("ease-custom")},
                {QStringLiteral("type"), QStringLiteral("bezier")},
                {QStringLiteral("points"), QJsonArray{
                    QJsonArray{0.2, 0.0}, QJsonArray{0.8, 1.0},
                }},
            },
            QJsonObject{
                {QStringLiteral("id"), QStringLiteral("curve-default")},
                {QStringLiteral("name"), QStringLiteral("default")},
                {QStringLiteral("type"), QStringLiteral("bezier")},
                {QStringLiteral("points"), QJsonArray{
                    QJsonArray{0.1, 0.7}, QJsonArray{0.2, 1.0},
                }},
            },
            QJsonObject{
                {QStringLiteral("id"), QStringLiteral("curve-linear")},
                {QStringLiteral("name"), QStringLiteral("linear")},
                {QStringLiteral("type"), QStringLiteral("spring")},
                {QStringLiteral("stiffness"), 240.0},
                {QStringLiteral("dampening"), 24.0},
                {QStringLiteral("mass"), 1.0},
            },
        };
        snapshot.insert(QStringLiteral("curves"), originalCurves);
        const QJsonArray originalAnimations{QJsonObject{
            {QStringLiteral("id"), QStringLiteral("animation-windows")},
            {QStringLiteral("name"), QStringLiteral("windows")},
            {QStringLiteral("enabled"), true},
            {QStringLiteral("speed"), 6.0},
            {QStringLiteral("curve"), QStringLiteral("ease-custom")},
            {QStringLiteral("style"), QStringLiteral("slide")},
        }};
        snapshot.insert(QStringLiteral("animations"), originalAnimations);

        QJsonArray nextCurves{
            QJsonObject{
                {QStringLiteral("id"), QStringLiteral("linear-reordered")},
                {QStringLiteral("name"), QStringLiteral("linear")},
                {QStringLiteral("type"), QStringLiteral("spring")},
                {QStringLiteral("stiffness"), 260.5},
                {QStringLiteral("dampening"), 26.25},
                {QStringLiteral("mass"), 1.25},
            },
            QJsonObject{
                {QStringLiteral("id"), QStringLiteral("default-reordered")},
                {QStringLiteral("name"), QStringLiteral("default")},
                {QStringLiteral("type"), QStringLiteral("bezier")},
                {QStringLiteral("points"), QJsonArray{
                    QJsonArray{0.12, 0.72}, QJsonArray{0.22, 0.98},
                }},
            },
            QJsonObject{
                {QStringLiteral("id"), QStringLiteral("custom-reordered")},
                {QStringLiteral("name"), QStringLiteral("ease-custom")},
                {QStringLiteral("type"), QStringLiteral("bezier")},
                {QStringLiteral("points"), QJsonArray{
                    QJsonArray{0.25, 0.05}, QJsonArray{0.75, 0.95},
                }},
            },
        };
        const QJsonArray nextAnimations{
            QJsonObject{
                {QStringLiteral("id"), QStringLiteral("animation-global")},
                {QStringLiteral("name"), QStringLiteral("global")},
                {QStringLiteral("enabled"), false},
                {QStringLiteral("speed"), 1.25},
                {QStringLiteral("curve"), QStringLiteral("default")},
                {QStringLiteral("style"), QString{}},
            },
            QJsonObject{
                {QStringLiteral("id"), QStringLiteral("animation-fade")},
                {QStringLiteral("name"), QStringLiteral("fade")},
                {QStringLiteral("enabled"), true},
                {QStringLiteral("speed"), 2.75},
                {QStringLiteral("curve"), QStringLiteral("linear")},
                {QStringLiteral("style"), QString{}},
            },
        };
        QString error;
        const auto edit = HyprShelld::CompositorSnapshotEditor::replaceAppearance(
            snapshot, 7, catalog.digest(), actions.digest(), catalog, actions,
            appearanceDefaults(), nextCurves.toVariantList(),
            nextAnimations.toVariantList(), error
        );
        QVERIFY2(edit, qPrintable(error));
        QVERIFY(edit->changed);
        const auto candidate = QJsonDocument::fromJson(edit->candidate).object();
        QCOMPARE(candidate.value(QStringLiteral("curves")).toArray(), nextCurves);
        QCOMPARE(
            candidate.value(QStringLiteral("animations")).toArray(),
            nextAnimations
        );
        for (const auto &surface : {
                 QStringLiteral("monitors"), QStringLiteral("devices"),
                 QStringLiteral("gestures"), QStringLiteral("workspaceRules"),
                 QStringLiteral("windowRules"), QStringLiteral("layerRules"),
                 QStringLiteral("submaps"), QStringLiteral("bindings"),
                 QStringLiteral("permissions"), QStringLiteral("environment"),
             }) {
            QCOMPARE(candidate.value(surface), snapshot.value(surface));
        }

        const auto rejected = [&](const QJsonArray &curves) {
            error.clear();
            const auto result =
                HyprShelld::CompositorSnapshotEditor::replaceAppearance(
                    snapshot, 7, catalog.digest(), actions.digest(), catalog,
                    actions, appearanceDefaults(), curves.toVariantList(),
                    originalAnimations.toVariantList(), error
                );
            QVERIFY(!result);
            QCOMPARE(
                error,
                QStringLiteral(
                    "Curve additions, removals, renames, and type changes require a verified compositor restart workflow and cannot be saved from Settings"
                )
            );
        };
        auto added = originalCurves;
        added.append(QJsonObject{
            {QStringLiteral("id"), QStringLiteral("curve-added")},
            {QStringLiteral("name"), QStringLiteral("added")},
            {QStringLiteral("type"), QStringLiteral("bezier")},
            {QStringLiteral("points"), QJsonArray{
                QJsonArray{0.1, 0.1}, QJsonArray{0.9, 0.9},
            }},
        });
        rejected(added);
        auto removed = originalCurves;
        removed.removeLast();
        rejected(removed);
        auto renamed = originalCurves;
        auto renamedCurve = renamed.first().toObject();
        renamedCurve.insert(QStringLiteral("name"), QStringLiteral("renamed"));
        renamed.replace(0, renamedCurve);
        rejected(renamed);
        auto retyped = originalCurves;
        auto retypedCurve = retyped.first().toObject();
        retypedCurve.insert(QStringLiteral("type"), QStringLiteral("spring"));
        retypedCurve.remove(QStringLiteral("points"));
        retypedCurve.insert(QStringLiteral("stiffness"), 250.0);
        retypedCurve.insert(QStringLiteral("dampening"), 25.0);
        retypedCurve.insert(QStringLiteral("mass"), 1.0);
        retyped.replace(0, retypedCurve);
        rejected(retyped);

        QVariantList oversizedCurves;
        oversizedCurves.resize(HyprShelld::Hyprland::maximumCurves + 1);
        error.clear();
        QVERIFY(!HyprShelld::CompositorSnapshotEditor::replaceAppearance(
            snapshot, 7, catalog.digest(), actions.digest(), catalog, actions,
            appearanceDefaults(), oversizedCurves,
            originalAnimations.toVariantList(), error
        ));
        QVERIFY(error.contains(QStringLiteral("item limit")));

        QVariantList oversizedAnimations;
        oversizedAnimations.resize(
            HyprShelld::Hyprland::maximumAnimations + 1
        );
        error.clear();
        QVERIFY(!HyprShelld::CompositorSnapshotEditor::replaceAppearance(
            snapshot, 7, catalog.digest(), actions.digest(), catalog, actions,
            appearanceDefaults(), originalCurves.toVariantList(),
            oversizedAnimations, error
        ));
        QVERIFY(error.contains(QStringLiteral("item limit")));

        auto malformed = originalCurves.toVariantList();
        auto malformedCurve = malformed.first().toMap();
        malformedCurve.insert(
            QStringLiteral("points"),
            QVariantList{
                QVariantList{
                    std::numeric_limits<double>::quiet_NaN(), 0.0,
                },
                QVariantList{0.8, 1.0},
            }
        );
        malformed.replace(0, malformedCurve);
        error.clear();
        QVERIFY(!HyprShelld::CompositorSnapshotEditor::replaceAppearance(
            snapshot, 7, catalog.digest(), actions.digest(), catalog, actions,
            appearanceDefaults(), malformed,
            originalAnimations.toVariantList(), error
        ));
        QVERIFY(error.contains(QStringLiteral("finite numbers")));

        auto duplicateAnimations = originalAnimations;
        auto duplicateAnimation = duplicateAnimations.first().toObject();
        duplicateAnimation.insert(
            QStringLiteral("id"), QStringLiteral("animation-windows-second")
        );
        duplicateAnimations.append(duplicateAnimation);
        error.clear();
        QVERIFY(!HyprShelld::CompositorSnapshotEditor::replaceAppearance(
            snapshot, 7, catalog.digest(), actions.digest(), catalog, actions,
            appearanceDefaults(), originalCurves.toVariantList(),
            duplicateAnimations.toVariantList(), error
        ));
        QVERIFY(error.contains(QStringLiteral("animations")));
    }

    void editsTheExactWindowsMapAndPreservesAppearanceInputAndComplexState()
    {
        const auto catalog = trustedCatalog();
        QVERIFY(catalog.windowsContractAvailable());
        QCOMPARE(catalog.windowsOptions().size(), 110);
        auto snapshot = snapshotWithValidComplexSurfaces();
        auto originalOverrides = snapshot.value(
            QStringLiteral("overrides")
        ).toObject();
        originalOverrides.insert(
            QStringLiteral("hyprland.general.border_size"), 7
        );
        originalOverrides.insert(
            QStringLiteral("hyprland.input.repeat_rate"), 65
        );
        originalOverrides.insert(
            QStringLiteral("hyprland.animations.workspace_wraparound"), true
        );
        const auto excludedVisuals = excludedGroupbarVisualOverrides();
        QCOMPARE(excludedVisuals.size(), 8);
        for (auto iterator = excludedVisuals.constBegin();
             iterator != excludedVisuals.constEnd(); ++iterator) {
            originalOverrides.insert(iterator.key(), iterator.value());
        }
        const auto unauthoredOverrides =
            unauthoredRenderAndXWaylandOverrides();
        for (auto iterator = unauthoredOverrides.constBegin();
             iterator != unauthoredOverrides.constEnd(); ++iterator) {
            originalOverrides.insert(iterator.key(), iterator.value());
        }
        snapshot.insert(QStringLiteral("overrides"), originalOverrides);

        auto values = windowsDefaults();
        values.insert(
            QStringLiteral("hyprland.general.layout"),
            QStringLiteral("master")
        );
        values.insert(
            QStringLiteral("hyprland.general.resize_on_border"), true
        );
        values.insert(
            QStringLiteral("hyprland.general.extend_border_grab_area"), 37
        );
        values.insert(
            QStringLiteral("hyprland.general.hover_icon_on_border"), false
        );
        values.insert(QStringLiteral("hyprland.general.resize_corner"), 4.0);
        values.insert(QStringLiteral("hyprland.general.snap.enabled"), true);
        values.insert(
            QStringLiteral("hyprland.general.snap.border_overlap"), true
        );
        values.insert(
            QStringLiteral("hyprland.general.snap.monitor_gap"), 21
        );
        values.insert(
            QStringLiteral("hyprland.general.snap.respect_gaps"), true
        );
        values.insert(
            QStringLiteral("hyprland.general.snap.window_gap"), 19
        );
        values.insert(QStringLiteral("hyprland.input.follow_mouse"), 3.0);
        values.insert(QStringLiteral("hyprland.input.mouse_refocus"), false);
        values.insert(
            QStringLiteral("hyprland.input.follow_mouse_shrink"), 41
        );
        values.insert(
            QStringLiteral("hyprland.input.float_switch_override_focus"),
            2.0
        );
        values.insert(QStringLiteral("hyprland.input.focus_on_close"), 2.0);
        values.insert(
            QStringLiteral("hyprland.input.special_fallthrough"), true
        );
        values.insert(
            QStringLiteral("hyprland.general.no_focus_fallback"), true
        );
        values.insert(
            QStringLiteral("hyprland.general.modal_parent_blocking"), false
        );
        values.insert(
            QStringLiteral("hyprland.general.float_gaps"),
            QVariantList{0, 5, -6, 7}
        );
        values.insert(
            QStringLiteral("hyprland.general.gaps_workspaces"), 17
        );
        values.insert(
            QStringLiteral("hyprland.layout.single_window_aspect_ratio"),
            QVariantList{16.0, 9.0}
        );
        values.insert(
            QStringLiteral(
                "hyprland.layout.single_window_aspect_ratio_tolerance"
            ),
            0.37
        );
        values.insert(
            QStringLiteral("hyprland.dwindle.default_split_ratio"), 0.73
        );
        values.insert(QStringLiteral("hyprland.dwindle.force_split"), 2.0);
        values.insert(
            QStringLiteral("hyprland.master.focus_master_on_close"), true
        );
        values.insert(
            QStringLiteral("hyprland.master.orientation"),
            QStringLiteral("center")
        );
        values.insert(
            QStringLiteral("hyprland.scrolling.direction"),
            QStringLiteral("up")
        );
        values.insert(
            QStringLiteral("hyprland.scrolling.follow_min_visible"), 0.37
        );
        values.insert(
            QStringLiteral("hyprland.gestures.scrolling.move_snap_cursor"),
            false
        );
        values.insert(QStringLiteral("hyprland.group.auto_group"), false);
        values.insert(
            QStringLiteral("hyprland.group.insert_after_current"), false
        );
        values.insert(
            QStringLiteral("hyprland.group.focus_removed_window"), false
        );
        values.insert(
            QStringLiteral("hyprland.group.drag_into_group"), 2.0
        );
        values.insert(
            QStringLiteral("hyprland.group.merge_groups_on_drag"), false
        );
        values.insert(
            QStringLiteral("hyprland.group.merge_groups_on_groupbar"), false
        );
        values.insert(
            QStringLiteral(
                "hyprland.group.merge_floated_into_tiled_on_groupbar"
            ),
            true
        );
        values.insert(
            QStringLiteral("hyprland.group.group_on_movetoworkspace"), true
        );
        const auto groupbarValues = changedGroupbarValues();
        QCOMPARE(groupbarValues.size(), 27);
        for (auto iterator = groupbarValues.constBegin();
             iterator != groupbarValues.constEnd(); ++iterator) {
            values.insert(iterator.key(), iterator.value());
        }
        values.insert(
            QStringLiteral("hyprland.binds.allow_pin_fullscreen"), true
        );
        values.insert(
            QStringLiteral("hyprland.binds.focus_preferred_method"), 1.0
        );
        values.insert(
            QStringLiteral("hyprland.binds.ignore_group_lock"), true
        );
        values.insert(
            QStringLiteral("hyprland.binds.movefocus_cycles_fullscreen"),
            true
        );
        values.insert(
            QStringLiteral("hyprland.binds.movefocus_cycles_groupfirst"),
            true
        );
        values.insert(
            QStringLiteral(
                "hyprland.binds.window_direction_monitor_fallback"
            ),
            false
        );
        values.insert(
            QStringLiteral("hyprland.misc.size_limits_tiled"), true
        );
        values.insert(
            QStringLiteral("hyprland.misc.always_follow_on_dnd"), false
        );
        values.insert(
            QStringLiteral("hyprland.misc.focus_on_activate"), true
        );
        values.insert(
            QStringLiteral("hyprland.misc.mouse_move_focuses_monitor"), false
        );
        values.insert(
            QStringLiteral("hyprland.misc.on_focus_under_fullscreen"), 1.0
        );
        values.insert(
            QStringLiteral("hyprland.misc.exit_window_retains_fullscreen"),
            true
        );
        values.insert(QStringLiteral("hyprland.misc.enable_swallow"), true);
        values.insert(
            QStringLiteral("hyprland.misc.swallow_regex"),
            QStringLiteral("^(kitty|Alacritty)$")
        );
        values.insert(
            QStringLiteral("hyprland.misc.swallow_exception_regex"),
            QStringLiteral("^scratch$")
        );
        constexpr double followMouseThreshold = 123456.7890625;
        values.insert(
            QStringLiteral("hyprland.input.follow_mouse_threshold"),
            followMouseThreshold
        );

        QString error;
        const auto edit = HyprShelld::CompositorSnapshotEditor::replaceWindows(
            snapshot,
            7,
            snapshot.value(QStringLiteral("catalogDigest")).toString(),
            snapshot.value(QStringLiteral("actionCatalogDigest")).toString(),
            catalog, trustedActionCatalog(),
            values,
            error
        );
        QVERIFY2(edit, qPrintable(error));
        QVERIFY(edit->changed);
        QVERIFY(edit->candidate.endsWith('\n'));
        const auto candidate = QJsonDocument::fromJson(
            edit->candidate
        ).object();
        for (const auto &field : snapshot.keys()) {
            if (field != QStringLiteral("overrides")) {
                QCOMPARE(candidate.value(field), snapshot.value(field));
            }
        }

        const auto overrides = candidate.value(
            QStringLiteral("overrides")
        ).toObject();
        auto unrelatedBefore = originalOverrides;
        auto unrelatedAfter = overrides;
        for (const auto &id : catalog.windowsOptionIds()) {
            unrelatedBefore.remove(id);
            unrelatedAfter.remove(id);
        }
        QCOMPARE(unrelatedAfter, unrelatedBefore);
        QCOMPARE(overrides.value(QStringLiteral("hyprland.general.border_size")),
                 originalOverrides.value(
                     QStringLiteral("hyprland.general.border_size")));
        QCOMPARE(overrides.value(QStringLiteral("hyprland.input.repeat_rate")),
                 originalOverrides.value(
                     QStringLiteral("hyprland.input.repeat_rate")));
        QCOMPARE(overrides.value(
            QStringLiteral("hyprland.animations.workspace_wraparound")
        ), originalOverrides.value(
            QStringLiteral("hyprland.animations.workspace_wraparound")
        ));
        QCOMPARE(overrides.value(QStringLiteral("hyprland.general.layout"))
                     .toString(),
                 QStringLiteral("master"));
        QCOMPARE(overrides.value(
            QStringLiteral("hyprland.general.extend_border_grab_area")
        ).toInt(), 37);
        QCOMPARE(overrides.value(
            QStringLiteral("hyprland.general.resize_corner")
        ).toInt(), 4);
        QCOMPARE(overrides.value(
            QStringLiteral("hyprland.input.follow_mouse")
        ).toInt(), 3);
        QCOMPARE(overrides.value(
            QStringLiteral("hyprland.input.focus_on_close")
        ).toInt(), 2);
        QCOMPARE(overrides.value(
            QStringLiteral("hyprland.general.float_gaps")
        ).toArray(), QJsonArray({0, 5, -6, 7}));
        QCOMPARE(overrides.value(
            QStringLiteral("hyprland.layout.single_window_aspect_ratio")
        ).toArray(), QJsonArray({16.0, 9.0}));
        QCOMPARE(overrides.value(
            QStringLiteral(
                "hyprland.layout.single_window_aspect_ratio_tolerance"
            )
        ).toDouble(), 0.37);
        QCOMPARE(overrides.value(
            QStringLiteral("hyprland.group.drag_into_group")
        ).toInt(), 2);
        QCOMPARE(overrides.value(
            QStringLiteral(
                "hyprland.group.merge_floated_into_tiled_on_groupbar"
            )
        ).toBool(), true);
        QCOMPARE(overrides.value(
            QStringLiteral("hyprland.group.group_on_movetoworkspace")
        ).toBool(), true);
        QCOMPARE(overrides.value(
            QStringLiteral("hyprland.binds.allow_pin_fullscreen")
        ).toBool(), true);
        QCOMPARE(overrides.value(
            QStringLiteral("hyprland.binds.focus_preferred_method")
        ).toInt(), 1);
        QCOMPARE(overrides.value(
            QStringLiteral("hyprland.binds.ignore_group_lock")
        ).toBool(), true);
        QCOMPARE(overrides.value(
            QStringLiteral("hyprland.binds.movefocus_cycles_fullscreen")
        ).toBool(), true);
        QCOMPARE(overrides.value(
            QStringLiteral("hyprland.binds.movefocus_cycles_groupfirst")
        ).toBool(), true);
        QCOMPARE(overrides.value(
            QStringLiteral(
                "hyprland.binds.window_direction_monitor_fallback"
            )
        ).toBool(), false);
        QCOMPARE(overrides.value(
            QStringLiteral("hyprland.misc.size_limits_tiled")
        ).toBool(), true);
        QCOMPARE(overrides.value(
            QStringLiteral("hyprland.misc.always_follow_on_dnd")
        ).toBool(), false);
        QCOMPARE(overrides.value(
            QStringLiteral("hyprland.misc.focus_on_activate")
        ).toBool(), true);
        QCOMPARE(overrides.value(
            QStringLiteral("hyprland.misc.mouse_move_focuses_monitor")
        ).toBool(), false);
        QCOMPARE(overrides.value(
            QStringLiteral("hyprland.misc.on_focus_under_fullscreen")
        ).toInt(), 1);
        QCOMPARE(overrides.value(
            QStringLiteral("hyprland.misc.exit_window_retains_fullscreen")
        ).toBool(), true);
        QCOMPARE(overrides.value(
            QStringLiteral("hyprland.misc.enable_swallow")
        ).toBool(), true);
        QCOMPARE(overrides.value(
            QStringLiteral("hyprland.misc.swallow_regex")
        ).toString(), QStringLiteral("^(kitty|Alacritty)$"));
        QCOMPARE(overrides.value(
            QStringLiteral("hyprland.misc.swallow_exception_regex")
        ).toString(), QStringLiteral("^scratch$"));
        QCOMPARE(overrides.value(
            QStringLiteral("hyprland.input.follow_mouse_threshold")
        ).toDouble(), followMouseThreshold);
        for (auto iterator = groupbarValues.constBegin();
             iterator != groupbarValues.constEnd(); ++iterator) {
            QCOMPARE(
                overrides.value(iterator.key()),
                QJsonValue::fromVariant(iterator.value())
            );
        }
        for (auto iterator = excludedVisuals.constBegin();
             iterator != excludedVisuals.constEnd(); ++iterator) {
            QCOMPARE(overrides.value(iterator.key()), iterator.value());
        }
        for (auto iterator = unauthoredOverrides.constBegin();
             iterator != unauthoredOverrides.constEnd(); ++iterator) {
            QCOMPARE(overrides.value(iterator.key()), iterator.value());
        }

        const auto projected = catalog.windowsValues(candidate, error);
        QVERIFY2(projected, qPrintable(error));
        QCOMPARE(*projected, values);

        const auto dormantSnapshot = snapshotWithValidComplexSurfaces();
        auto dormantValues = windowsDefaults();
        dormantValues.insert(
            QStringLiteral("hyprland.misc.enable_swallow"), true
        );
        const auto dormantEdit =
            HyprShelld::CompositorSnapshotEditor::replaceWindows(
                dormantSnapshot,
                7,
                catalog.digest(),
                dormantSnapshot.value(
                    QStringLiteral("actionCatalogDigest")
                ).toString(),
                catalog,
                trustedActionCatalog(),
                dormantValues,
                error
            );
        QVERIFY2(dormantEdit, qPrintable(error));
        const auto dormantCandidate = QJsonDocument::fromJson(
            dormantEdit->candidate
        ).object();
        const auto dormantOverrides = dormantCandidate.value(
            QStringLiteral("overrides")
        ).toObject();
        QCOMPARE(dormantOverrides.value(
            QStringLiteral("hyprland.misc.enable_swallow")
        ).toBool(), true);
        QVERIFY(!dormantOverrides.contains(
            QStringLiteral("hyprland.misc.swallow_regex")
        ));
        QVERIFY(!dormantOverrides.contains(
            QStringLiteral("hyprland.misc.swallow_exception_regex")
        ));
        QCOMPARE(
            dormantCandidate.value(QStringLiteral("windowRules")),
            dormantSnapshot.value(QStringLiteral("windowRules"))
        );
        const auto dormantProjected = catalog.windowsValues(
            dormantCandidate, error
        );
        QVERIFY2(dormantProjected, qPrintable(error));
        QCOMPARE(*dormantProjected, dormantValues);
    }

    void elidesWindowsDefaultsWithoutTouchingOtherDomainOverrides()
    {
        const auto catalog = trustedCatalog();
        auto snapshot = snapshotWithValidComplexSurfaces();
        auto overrides = snapshot.value(QStringLiteral("overrides")).toObject();
        auto changed = windowsDefaults();
        changed.insert(
            QStringLiteral("hyprland.general.layout"),
            QStringLiteral("monocle")
        );
        changed.insert(
            QStringLiteral("hyprland.general.resize_on_border"), true
        );
        changed.insert(
            QStringLiteral("hyprland.general.extend_border_grab_area"), 44
        );
        changed.insert(
            QStringLiteral("hyprland.general.hover_icon_on_border"), false
        );
        changed.insert(QStringLiteral("hyprland.general.resize_corner"), 2);
        changed.insert(QStringLiteral("hyprland.general.snap.enabled"), true);
        changed.insert(
            QStringLiteral("hyprland.general.snap.border_overlap"), true
        );
        changed.insert(
            QStringLiteral("hyprland.general.snap.monitor_gap"), 22
        );
        changed.insert(
            QStringLiteral("hyprland.general.snap.respect_gaps"), true
        );
        changed.insert(
            QStringLiteral("hyprland.general.snap.window_gap"), 23
        );
        changed.insert(QStringLiteral("hyprland.input.follow_mouse"), 0);
        changed.insert(QStringLiteral("hyprland.input.mouse_refocus"), false);
        changed.insert(
            QStringLiteral("hyprland.input.follow_mouse_shrink"), 24
        );
        changed.insert(
            QStringLiteral("hyprland.input.float_switch_override_focus"), 2
        );
        changed.insert(QStringLiteral("hyprland.input.focus_on_close"), 2);
        changed.insert(
            QStringLiteral("hyprland.input.special_fallthrough"), true
        );
        changed.insert(
            QStringLiteral("hyprland.general.no_focus_fallback"), true
        );
        changed.insert(
            QStringLiteral("hyprland.general.modal_parent_blocking"), false
        );
        changed.insert(QStringLiteral("hyprland.group.auto_group"), false);
        changed.insert(
            QStringLiteral("hyprland.group.insert_after_current"), false
        );
        changed.insert(
            QStringLiteral("hyprland.group.focus_removed_window"), false
        );
        changed.insert(
            QStringLiteral("hyprland.group.drag_into_group"), 2
        );
        changed.insert(
            QStringLiteral("hyprland.group.merge_groups_on_drag"), false
        );
        changed.insert(
            QStringLiteral("hyprland.group.merge_groups_on_groupbar"), false
        );
        changed.insert(
            QStringLiteral(
                "hyprland.group.merge_floated_into_tiled_on_groupbar"
            ),
            true
        );
        changed.insert(
            QStringLiteral("hyprland.group.group_on_movetoworkspace"), true
        );
        const auto groupbarValues = changedGroupbarValues();
        QCOMPARE(groupbarValues.size(), 27);
        for (auto iterator = groupbarValues.constBegin();
             iterator != groupbarValues.constEnd(); ++iterator) {
            changed.insert(iterator.key(), iterator.value());
        }
        changed.insert(
            QStringLiteral("hyprland.binds.allow_pin_fullscreen"), true
        );
        changed.insert(
            QStringLiteral("hyprland.binds.focus_preferred_method"), 1
        );
        changed.insert(
            QStringLiteral("hyprland.binds.ignore_group_lock"), true
        );
        changed.insert(
            QStringLiteral("hyprland.binds.movefocus_cycles_fullscreen"),
            true
        );
        changed.insert(
            QStringLiteral("hyprland.binds.movefocus_cycles_groupfirst"),
            true
        );
        changed.insert(
            QStringLiteral(
                "hyprland.binds.window_direction_monitor_fallback"
            ),
            false
        );
        changed.insert(
            QStringLiteral("hyprland.misc.size_limits_tiled"), true
        );
        changed.insert(
            QStringLiteral("hyprland.misc.always_follow_on_dnd"), false
        );
        changed.insert(
            QStringLiteral("hyprland.misc.focus_on_activate"), true
        );
        changed.insert(
            QStringLiteral("hyprland.misc.mouse_move_focuses_monitor"), false
        );
        changed.insert(
            QStringLiteral("hyprland.misc.on_focus_under_fullscreen"), 1
        );
        changed.insert(
            QStringLiteral("hyprland.misc.exit_window_retains_fullscreen"),
            true
        );
        changed.insert(QStringLiteral("hyprland.misc.enable_swallow"), true);
        changed.insert(
            QStringLiteral("hyprland.misc.swallow_regex"),
            QStringLiteral("^(kitty|Alacritty)$")
        );
        changed.insert(
            QStringLiteral("hyprland.misc.swallow_exception_regex"),
            QStringLiteral("^scratch$")
        );
        changed.insert(
            QStringLiteral("hyprland.input.follow_mouse_threshold"),
            987654.32109375
        );
        for (auto it = changed.cbegin(); it != changed.cend(); ++it) {
            overrides.insert(it.key(), QJsonValue::fromVariant(it.value()));
        }
        overrides.insert(QStringLiteral("hyprland.general.border_size"), 8);
        overrides.insert(QStringLiteral("hyprland.input.repeat_rate"), 81);
        overrides.insert(
            QStringLiteral("hyprland.animations.workspace_wraparound"), true
        );
        const auto excludedVisuals = excludedGroupbarVisualOverrides();
        QCOMPARE(excludedVisuals.size(), 8);
        for (auto iterator = excludedVisuals.constBegin();
             iterator != excludedVisuals.constEnd(); ++iterator) {
            overrides.insert(iterator.key(), iterator.value());
        }
        auto unrelatedBefore = overrides;
        for (const auto &id : catalog.windowsOptionIds()) {
            unrelatedBefore.remove(id);
        }
        snapshot.insert(QStringLiteral("overrides"), overrides);

        QString error;
        const auto edit = HyprShelld::CompositorSnapshotEditor::replaceWindows(
            snapshot,
            7,
            catalog.digest(),
            snapshot.value(QStringLiteral("actionCatalogDigest")).toString(),
            catalog, trustedActionCatalog(),
            windowsDefaults(),
            error
        );
        QVERIFY2(edit, qPrintable(error));
        QVERIFY(edit->changed);
        const auto candidate = QJsonDocument::fromJson(edit->candidate).object();
        const auto candidateOverrides = candidate.value(
            QStringLiteral("overrides")
        ).toObject();
        for (const auto &field : snapshot.keys()) {
            if (field != QStringLiteral("overrides")) {
                QCOMPARE(candidate.value(field), snapshot.value(field));
            }
        }
        for (const auto &id : catalog.windowsOptionIds()) {
            QVERIFY2(!candidateOverrides.contains(id), qPrintable(id));
        }
        auto unrelatedAfter = candidateOverrides;
        for (const auto &id : catalog.windowsOptionIds()) {
            unrelatedAfter.remove(id);
        }
        QCOMPARE(unrelatedAfter, unrelatedBefore);
        QCOMPARE(candidateOverrides.value(
            QStringLiteral("hyprland.general.border_size")
        ).toInt(), 8);
        QCOMPARE(candidateOverrides.value(
            QStringLiteral("hyprland.input.repeat_rate")
        ).toInt(), 81);
        QCOMPARE(candidateOverrides.value(
            QStringLiteral("hyprland.animations.workspace_wraparound")
        ).toBool(), true);
        QCOMPARE(candidateOverrides.value(
            QStringLiteral("hyprland.misc.disable_hyprland_logo")
        ).toBool(), true);
        for (auto iterator = excludedVisuals.constBegin();
             iterator != excludedVisuals.constEnd(); ++iterator) {
            QCOMPARE(candidateOverrides.value(iterator.key()), iterator.value());
        }

        const auto unchanged =
            HyprShelld::CompositorSnapshotEditor::replaceWindows(
                baselineSnapshot(),
                7,
                catalog.digest(),
                baselineSnapshot().value(
                    QStringLiteral("actionCatalogDigest")
                ).toString(),
                catalog, trustedActionCatalog(),
                windowsDefaults(),
                error
            );
        QVERIFY2(unchanged, qPrintable(error));
        QVERIFY(!unchanged->changed);
    }

    void clearsOneSwallowPatternBeforeParsingAndPreservesOtherState()
    {
        const auto catalog = trustedCatalog();
        auto snapshot = snapshotWithValidComplexSurfaces();
        auto overrides = snapshot.value(QStringLiteral("overrides")).toObject();
        overrides.insert(QStringLiteral("hyprland.misc.enable_swallow"), true);
        overrides.insert(
            QStringLiteral("hyprland.misc.swallow_regex"),
            QStringLiteral("^Terminal$")
        );
        overrides.insert(
            QStringLiteral("hyprland.misc.swallow_exception_regex"),
            QStringLiteral("^scratch$")
        );
        overrides.insert(QStringLiteral("hyprland.general.border_size"), 8);
        snapshot.insert(QStringLiteral("overrides"), overrides);

        auto values = windowsDefaults();
        values.insert(QStringLiteral("hyprland.misc.enable_swallow"), true);
        values.insert(
            QStringLiteral("hyprland.misc.swallow_regex"),
            QStringLiteral("^Terminal$")
        );

        QString error;
        const auto edit = HyprShelld::CompositorSnapshotEditor::replaceWindows(
            snapshot,
            7,
            catalog.digest(),
            snapshot.value(QStringLiteral("actionCatalogDigest")).toString(),
            catalog,
            trustedActionCatalog(),
            values,
            error
        );
        QVERIFY2(edit, qPrintable(error));
        QVERIFY(edit->changed);
        const auto candidate = QJsonDocument::fromJson(
            edit->candidate
        ).object();
        const auto candidateOverrides = candidate.value(
            QStringLiteral("overrides")
        ).toObject();
        QCOMPARE(candidateOverrides.value(
            QStringLiteral("hyprland.misc.enable_swallow")
        ).toBool(), true);
        QCOMPARE(candidateOverrides.value(
            QStringLiteral("hyprland.misc.swallow_regex")
        ).toString(), QStringLiteral("^Terminal$"));
        QVERIFY(!candidateOverrides.contains(
            QStringLiteral("hyprland.misc.swallow_exception_regex")
        ));
        QCOMPARE(candidateOverrides.value(
            QStringLiteral("hyprland.general.border_size")
        ).toInt(), 8);
        QCOMPARE(
            candidate.value(QStringLiteral("windowRules")),
            snapshot.value(QStringLiteral("windowRules"))
        );
        const auto projected = catalog.windowsValues(candidate, error);
        QVERIFY2(projected, qPrintable(error));
        QCOMPARE(*projected, values);
    }

    void editsTheExactWorkspacesMapAndPreservesEveryOtherDomainAndSurface()
    {
        const auto catalog = trustedCatalog();
        QVERIFY(catalog.workspacesContractAvailable());
        QCOMPARE(catalog.workspacesOptions().size(), 21);
        auto snapshot = snapshotWithValidComplexSurfaces();
        auto originalOverrides = snapshot.value(
            QStringLiteral("overrides")
        ).toObject();
        originalOverrides.insert(
            QStringLiteral("hyprland.general.border_size"), 7
        );
        originalOverrides.insert(
            QStringLiteral("hyprland.input.repeat_rate"), 65
        );
        originalOverrides.insert(
            QStringLiteral("hyprland.gestures.close_max_timeout"), 1333
        );
        originalOverrides.insert(
            QStringLiteral("hyprland.general.float_gaps"),
            QJsonArray{3, 2, 1, 0}
        );
        originalOverrides.insert(
            QStringLiteral("hyprland.layout.single_window_aspect_ratio"),
            QJsonArray{4.0, 3.0}
        );
        snapshot.insert(QStringLiteral("overrides"), originalOverrides);

        const auto values = changedWorkspacesValues();
        QString error;
        const auto edit =
            HyprShelld::CompositorSnapshotEditor::replaceWorkspaces(
                snapshot,
                7,
                snapshot.value(QStringLiteral("catalogDigest")).toString(),
                snapshot.value(
                    QStringLiteral("actionCatalogDigest")
                ).toString(),
                catalog, trustedActionCatalog(),
                values,
                userWorkspaceRules(snapshot),
                error
            );
        QVERIFY2(edit, qPrintable(error));
        QVERIFY(edit->changed);
        QVERIFY(edit->candidate.endsWith('\n'));
        const auto candidate = QJsonDocument::fromJson(
            edit->candidate
        ).object();
        for (const auto &field : snapshot.keys()) {
            if (field != QStringLiteral("overrides")) {
                QCOMPARE(candidate.value(field), snapshot.value(field));
            }
        }
        const auto overrides = candidate.value(
            QStringLiteral("overrides")
        ).toObject();
        for (const auto &id : {
                 QStringLiteral("hyprland.general.border_size"),
                 QStringLiteral("hyprland.input.repeat_rate"),
                 QStringLiteral("hyprland.gestures.close_max_timeout"),
                 QStringLiteral("hyprland.general.float_gaps"),
                 QStringLiteral(
                     "hyprland.layout.single_window_aspect_ratio"
                 ),
             }) {
            QCOMPARE(overrides.value(id), originalOverrides.value(id));
        }
        QCOMPARE(overrides.value(QStringLiteral(
            "hyprland.gestures.workspace_swipe_cancel_ratio"
        )).toDouble(), 0.37);
        QCOMPARE(overrides.value(QStringLiteral(
            "hyprland.misc.initial_workspace_tracking"
        )).toInt(), 2);
        QCOMPARE(overrides.value(QStringLiteral(
            "hyprland.binds.allow_workspace_cycles"
        )).toBool(), true);
        QCOMPARE(overrides.value(QStringLiteral(
            "hyprland.binds.hide_special_on_workspace_change"
        )).toBool(), true);
        QCOMPARE(overrides.value(QStringLiteral(
            "hyprland.binds.workspace_back_and_forth"
        )).toBool(), true);
        QCOMPARE(overrides.value(QStringLiteral(
            "hyprland.binds.workspace_center_on"
        )).toInt(), 0);
        QCOMPARE(overrides.value(QStringLiteral(
            "hyprland.cursor.warp_on_change_workspace"
        )).toInt(), 1);
        QCOMPARE(overrides.value(QStringLiteral(
            "hyprland.cursor.warp_on_toggle_special"
        )).toInt(), 2);

        const auto projected = catalog.workspacesValues(candidate, error);
        QVERIFY2(projected, qPrintable(error));
        QCOMPARE(*projected, values);
    }

    void elidesWorkspacesDefaultsWithoutTouchingOtherDomainOverrides()
    {
        const auto catalog = trustedCatalog();
        auto snapshot = baselineSnapshot();
        auto overrides = snapshot.value(QStringLiteral("overrides")).toObject();
        const auto changed = changedWorkspacesValues();
        for (auto iterator = changed.cbegin(); iterator != changed.cend();
             ++iterator) {
            overrides.insert(
                iterator.key(), QJsonValue::fromVariant(iterator.value())
            );
        }
        overrides.insert(QStringLiteral("hyprland.general.border_size"), 8);
        overrides.insert(QStringLiteral("hyprland.input.repeat_rate"), 81);
        overrides.insert(
            QStringLiteral("hyprland.gestures.close_max_timeout"), 1444
        );
        overrides.insert(
            QStringLiteral("hyprland.general.float_gaps"),
            QJsonArray{0, 1, 2, 3}
        );
        snapshot.insert(QStringLiteral("overrides"), overrides);

        QString error;
        const auto edit =
            HyprShelld::CompositorSnapshotEditor::replaceWorkspaces(
                snapshot,
                7,
                catalog.digest(),
                snapshot.value(
                    QStringLiteral("actionCatalogDigest")
                ).toString(),
                catalog, trustedActionCatalog(),
                workspacesDefaults(),
                userWorkspaceRules(snapshot),
                error
            );
        QVERIFY2(edit, qPrintable(error));
        QVERIFY(edit->changed);
        const auto candidateOverrides = QJsonDocument::fromJson(
            edit->candidate
        ).object().value(QStringLiteral("overrides")).toObject();
        for (const auto &id : catalog.workspacesOptionIds()) {
            QVERIFY2(!candidateOverrides.contains(id), qPrintable(id));
        }
        QCOMPARE(candidateOverrides.value(QStringLiteral(
            "hyprland.general.border_size"
        )).toInt(), 8);
        QCOMPARE(candidateOverrides.value(QStringLiteral(
            "hyprland.input.repeat_rate"
        )).toInt(), 81);
        QCOMPARE(candidateOverrides.value(QStringLiteral(
            "hyprland.gestures.close_max_timeout"
        )).toInt(), 1444);
        QCOMPARE(candidateOverrides.value(QStringLiteral(
            "hyprland.general.float_gaps"
        )).toArray(), QJsonArray({0, 1, 2, 3}));

        const auto unchanged =
            HyprShelld::CompositorSnapshotEditor::replaceWorkspaces(
                baselineSnapshot(),
                7,
                catalog.digest(),
                baselineSnapshot().value(
                    QStringLiteral("actionCatalogDigest")
                ).toString(),
                catalog, trustedActionCatalog(),
                workspacesDefaults(),
                userWorkspaceRules(baselineSnapshot()),
                error
            );
        QVERIFY2(unchanged, qPrintable(error));
        QVERIFY(!unchanged->changed);
    }

    void editsTheExactAdvancedMapAndPreservesEveryOtherSnapshotSurface()
    {
        const auto catalog = trustedCatalog();
        QVERIFY(catalog.advancedContractAvailable());
        auto snapshot = snapshotWithValidComplexSurfaces();
        auto originalOverrides = snapshot.value(
            QStringLiteral("overrides")
        ).toObject();
        originalOverrides.insert(
            QStringLiteral("hyprland.general.border_size"), 7
        );
        originalOverrides.insert(
            QStringLiteral("hyprland.input.repeat_rate"), 65
        );
        originalOverrides.insert(
            QStringLiteral("hyprland.input.touchdevice.enabled"), false
        );
        originalOverrides.insert(
            QStringLiteral("hyprland.input.touchdevice.transform"), 5
        );
        originalOverrides.insert(
            QStringLiteral("hyprland.input.tablet.relative_input"), true
        );
        originalOverrides.insert(
            QStringLiteral("hyprland.input.tablet.left_handed"), true
        );
        originalOverrides.insert(
            QStringLiteral("hyprland.input.tablet.transform"), 6
        );
        originalOverrides.insert(
            QStringLiteral("hyprland.general.extend_border_grab_area"), 25
        );
        originalOverrides.insert(
            QStringLiteral("hyprland.gestures.workspace_swipe_distance"),
            450
        );
        const auto unauthoredOverrides =
            unauthoredRenderAndXWaylandOverrides();
        for (auto it = unauthoredOverrides.constBegin();
             it != unauthoredOverrides.constEnd(); ++it) {
            originalOverrides.insert(it.key(), it.value());
        }
        snapshot.insert(QStringLiteral("overrides"), originalOverrides);

        QString error;
        const auto edit = HyprShelld::CompositorSnapshotEditor::replaceAdvanced(
            snapshot,
            7,
            catalog.digest(),
            snapshot.value(QStringLiteral("actionCatalogDigest")).toString(),
            catalog,
            trustedActionCatalog(),
            changedAdvancedValues(),
            error
        );
        QVERIFY2(edit, qPrintable(error));
        QVERIFY(edit->changed);
        QVERIFY(edit->candidate.endsWith('\n'));
        const auto candidate = QJsonDocument::fromJson(edit->candidate).object();
        for (const auto &field : snapshot.keys()) {
            if (field != QStringLiteral("overrides")) {
                QCOMPARE(candidate.value(field), snapshot.value(field));
            }
        }
        const auto overrides = candidate.value(
            QStringLiteral("overrides")
        ).toObject();
        for (const auto &id : {
                 QStringLiteral("hyprland.general.border_size"),
                 QStringLiteral("hyprland.input.repeat_rate"),
                 QStringLiteral("hyprland.input.touchdevice.enabled"),
                 QStringLiteral("hyprland.input.touchdevice.transform"),
                 QStringLiteral("hyprland.input.tablet.relative_input"),
                 QStringLiteral("hyprland.input.tablet.left_handed"),
                 QStringLiteral("hyprland.input.tablet.transform"),
                 QStringLiteral("hyprland.general.extend_border_grab_area"),
                 QStringLiteral(
                     "hyprland.gestures.workspace_swipe_distance"
                 ),
             }) {
            QCOMPARE(overrides.value(id), originalOverrides.value(id));
        }
        for (auto it = unauthoredOverrides.constBegin();
             it != unauthoredOverrides.constEnd(); ++it) {
            QCOMPARE(overrides.value(it.key()), it.value());
        }
        QCOMPARE(
            overrides.value(
                QStringLiteral("hyprland.xwayland.use_nearest_neighbor")
            ),
            QJsonValue(false)
        );
        QCOMPARE(
            overrides.value(
                QStringLiteral(
                    "hyprland.render.expand_undersized_textures"
                )
            ),
            QJsonValue(false)
        );
        QCOMPARE(
            overrides.value(QStringLiteral("hyprland.render.direct_scanout")),
            QJsonValue(2)
        );
        QCOMPARE(
            overrides.value(QStringLiteral("hyprland.render.fp16_sdr_tf")),
            QJsonValue(1)
        );
        QCOMPARE(
            overrides.value(QStringLiteral("hyprland.render.xp_mode")),
            QJsonValue(true)
        );
        QCOMPARE(
            overrides.value(
                QStringLiteral("hyprland.input-capture.capture_modifiers")
            ),
            QJsonValue(true)
        );
        QCOMPARE(
            overrides.value(
                QStringLiteral("hyprland.input-capture.enforce_barriers")
            ),
            QJsonValue(false)
        );
        const auto projected = catalog.advancedValues(candidate, error);
        QVERIFY2(projected, qPrintable(error));
        QCOMPARE(*projected, changedAdvancedValues());

        auto retainedBlurValues = changedAdvancedValues();
        retainedBlurValues.insert(
            QStringLiteral("hyprland.misc.session_lock_xray"), false
        );
        const auto retainedBlurEdit =
            HyprShelld::CompositorSnapshotEditor::replaceAdvanced(
                candidate,
                7,
                catalog.digest(),
                candidate.value(
                    QStringLiteral("actionCatalogDigest")
                ).toString(),
                catalog,
                trustedActionCatalog(),
                retainedBlurValues,
                error
            );
        QVERIFY2(retainedBlurEdit, qPrintable(error));
        QVERIFY(retainedBlurEdit->changed);
        const auto retainedBlurCandidate = QJsonDocument::fromJson(
            retainedBlurEdit->candidate
        ).object();
        const auto retainedBlurOverrides = retainedBlurCandidate.value(
            QStringLiteral("overrides")
        ).toObject();
        QVERIFY(!retainedBlurOverrides.contains(
            QStringLiteral("hyprland.misc.session_lock_xray")
        ));
        QCOMPARE(retainedBlurOverrides.value(
            QStringLiteral("hyprland.misc.session_lock_blur")
        ).toBool(), true);
        const auto retainedBlurProjection = catalog.advancedValues(
            retainedBlurCandidate, error
        );
        QVERIFY2(retainedBlurProjection, qPrintable(error));
        QCOMPARE(*retainedBlurProjection, retainedBlurValues);

        const auto defaultsEdit =
            HyprShelld::CompositorSnapshotEditor::replaceAdvanced(
                retainedBlurCandidate,
                7,
                catalog.digest(),
                retainedBlurCandidate.value(
                    QStringLiteral("actionCatalogDigest")
                ).toString(),
                catalog,
                trustedActionCatalog(),
                advancedDefaults(),
                error
            );
        QVERIFY2(defaultsEdit, qPrintable(error));
        QVERIFY(defaultsEdit->changed);
        const auto defaultsCandidate = QJsonDocument::fromJson(
            defaultsEdit->candidate
        ).object();
        const auto defaultsOverrides = defaultsCandidate.value(
            QStringLiteral("overrides")
        ).toObject();
        for (const auto &id : catalog.advancedOptionIds()) {
            QVERIFY2(!defaultsOverrides.contains(id), qPrintable(id));
        }
        for (const auto &id : {
                 QStringLiteral("hyprland.general.border_size"),
                 QStringLiteral("hyprland.input.repeat_rate"),
                 QStringLiteral("hyprland.input.touchdevice.enabled"),
                 QStringLiteral("hyprland.input.touchdevice.transform"),
                 QStringLiteral("hyprland.input.tablet.relative_input"),
                 QStringLiteral("hyprland.input.tablet.left_handed"),
                 QStringLiteral("hyprland.input.tablet.transform"),
                 QStringLiteral("hyprland.general.extend_border_grab_area"),
                 QStringLiteral(
                     "hyprland.gestures.workspace_swipe_distance"
                 ),
             }) {
            QCOMPARE(defaultsOverrides.value(id), originalOverrides.value(id));
        }
        for (auto it = unauthoredOverrides.constBegin();
             it != unauthoredOverrides.constEnd(); ++it) {
            QCOMPARE(defaultsOverrides.value(it.key()), it.value());
        }
        QVERIFY(!defaultsOverrides.contains(
            QStringLiteral("hyprland.xwayland.use_nearest_neighbor")
        ));
        QVERIFY(!defaultsOverrides.contains(
            QStringLiteral("hyprland.render.expand_undersized_textures")
        ));
        QVERIFY(!defaultsOverrides.contains(
            QStringLiteral("hyprland.render.direct_scanout")
        ));
        QVERIFY(!defaultsOverrides.contains(
            QStringLiteral("hyprland.render.fp16_sdr_tf")
        ));
        QVERIFY(!defaultsOverrides.contains(
            QStringLiteral("hyprland.render.xp_mode")
        ));
        QVERIFY(!defaultsOverrides.contains(
            QStringLiteral("hyprland.input-capture.capture_modifiers")
        ));
        QVERIFY(!defaultsOverrides.contains(
            QStringLiteral("hyprland.input-capture.enforce_barriers")
        ));
        for (const auto &field : retainedBlurCandidate.keys()) {
            if (field != QStringLiteral("overrides")) {
                QCOMPARE(
                    defaultsCandidate.value(field),
                    retainedBlurCandidate.value(field)
                );
            }
        }

        const auto unchanged =
            HyprShelld::CompositorSnapshotEditor::replaceAdvanced(
                defaultsCandidate,
                7,
                catalog.digest(),
                defaultsCandidate.value(
                    QStringLiteral("actionCatalogDigest")
                ).toString(),
                catalog,
                trustedActionCatalog(),
                advancedDefaults(),
                error
            );
        QVERIFY2(unchanged, qPrintable(error));
        QVERIFY(!unchanged->changed);
    }

    void preservesUnauthoredMiscCompatibilityOverridesAcrossEveryAuthoredReplacement()
    {
        const auto catalog = trustedCatalog();
        const auto actions = trustedActionCatalog();
        const std::array<std::tuple<QString, bool>, 3> exclusions{{
            {
                QStringLiteral("hyprland.misc.animate_manual_resizes"),
                false,
            },
            {
                QStringLiteral("hyprland.misc.animate_mouse_windowdragging"),
                false,
            },
            {
                QStringLiteral("hyprland.misc.layers_hog_keyboard_focus"),
                true,
            },
        }};

        const auto defaultSnapshot = readObject(
            QStringLiteral(HYPRSHELLD_HYPRLAND_DEFAULTS_FILE)
        );
        const auto defaultOverrides = defaultSnapshot.value(
            QStringLiteral("overrides")
        ).toObject();
        for (const auto &[id, defaultValue] : exclusions) {
            const auto *option = HyprShelld::Hyprland::findOption(
                catalog.catalog(), id
            );
            QVERIFY2(option != nullptr, qPrintable(id));
            const auto suffix = id.sliced(
                QStringLiteral("hyprland.misc.").size()
            );
            QCOMPARE(option->path, QStringLiteral("misc:") + suffix);
            QCOMPARE(option->module, QStringLiteral("misc"));
            QCOMPARE(
                option->luaPath,
                QStringList({
                    QStringLiteral("misc"),
                    suffix,
                })
            );
            QCOMPARE(
                static_cast<int>(option->type),
                static_cast<int>(HyprShelld::Hyprland::OptionType::Boolean)
            );
            QVERIFY(option->writable);
            QCOMPARE(option->defaultValue.toBool(), defaultValue);
            QCOMPARE(
                static_cast<int>(option->defaultPolicy),
                static_cast<int>(
                    HyprShelld::Hyprland::DefaultPolicy::Hyprland
                )
            );
            QCOMPARE(
                static_cast<int>(option->uiTier),
                static_cast<int>(HyprShelld::Hyprland::UiTier::Advanced)
            );
            QCOMPARE(
                static_cast<int>(option->control),
                static_cast<int>(HyprShelld::Hyprland::ControlKind::Toggle)
            );
            QCOMPARE(
                static_cast<int>(option->applyMode),
                static_cast<int>(HyprShelld::Hyprland::ApplyMode::Reload)
            );
            QCOMPARE(
                static_cast<int>(option->risk),
                static_cast<int>(HyprShelld::Hyprland::RiskLevel::Safe)
            );
            QVERIFY(!catalog.appearanceOption(id));
            QVERIFY(!catalog.inputOption(id));
            QVERIFY(!catalog.windowsOption(id));
            QVERIFY(!catalog.workspacesOption(id));
            QVERIFY(!catalog.advancedOption(id));
            QVERIFY2(!defaultOverrides.contains(id), qPrintable(id));
        }

        auto snapshot = snapshotWithValidComplexSurfaces();
        auto overrides = snapshot.value(QStringLiteral("overrides")).toObject();
        overrides.insert(
            QStringLiteral("hyprland.misc.animate_manual_resizes"), true
        );
        overrides.insert(
            QStringLiteral("hyprland.misc.animate_mouse_windowdragging"), true
        );
        overrides.insert(
            QStringLiteral("hyprland.misc.layers_hog_keyboard_focus"), false
        );
        snapshot.insert(QStringLiteral("overrides"), overrides);

        const auto parsedWholeSnapshot = HyprShelld::Hyprland::parseDesiredState(
            QJsonDocument(snapshot).toJson(QJsonDocument::Compact),
            catalog.catalog(),
            actions.catalog()
        );
        QVERIFY(parsedWholeSnapshot);
        const auto wholeSnapshot = QJsonDocument::fromJson(
            HyprShelld::Hyprland::serializeDesiredState(
                *parsedWholeSnapshot.value
            )
        ).object();
        const auto wholeOverrides = wholeSnapshot.value(
            QStringLiteral("overrides")
        ).toObject();
        QCOMPARE(wholeOverrides.value(QStringLiteral(
            "hyprland.misc.animate_manual_resizes"
        )), QJsonValue(true));
        QCOMPARE(wholeOverrides.value(QStringLiteral(
            "hyprland.misc.animate_mouse_windowdragging"
        )), QJsonValue(true));
        QCOMPARE(wholeOverrides.value(QStringLiteral(
            "hyprland.misc.layers_hog_keyboard_focus"
        )), QJsonValue(false));

        QString error;
        QList<QByteArray> candidates;

        auto appearance = catalog.appearanceValues(snapshot, error);
        QVERIFY2(appearance, qPrintable(error));
        appearance->insert(QStringLiteral("hyprland.general.border_size"), 6);
        const auto appearanceEdit =
            HyprShelld::CompositorSnapshotEditor::replaceAppearance(
                snapshot, 7, catalog.digest(), actions.digest(),
                catalog, actions, *appearance,
                snapshot.value(QStringLiteral("curves"))
                    .toArray().toVariantList(),
                snapshot.value(QStringLiteral("animations"))
                    .toArray().toVariantList(),
                error
            );
        QVERIFY2(appearanceEdit, qPrintable(error));
        candidates.append(appearanceEdit->candidate);

        auto input = catalog.inputValues(snapshot, error);
        QVERIFY2(input, qPrintable(error));
        input->insert(QStringLiteral("hyprland.input.repeat_rate"), 26);
        const auto inputEdit =
            HyprShelld::CompositorSnapshotEditor::replaceInput(
                snapshot, 7, catalog.digest(), actions.digest(),
                catalog, actions, *input, snapshotGestures(snapshot), error
            );
        QVERIFY2(inputEdit, qPrintable(error));
        candidates.append(inputEdit->candidate);

        auto windows = catalog.windowsValues(snapshot, error);
        QVERIFY2(windows, qPrintable(error));
        windows->insert(
            QStringLiteral("hyprland.general.resize_on_border"), true
        );
        const auto windowsEdit =
            HyprShelld::CompositorSnapshotEditor::replaceWindows(
                snapshot, 7, catalog.digest(), actions.digest(),
                catalog, actions, *windows, error
            );
        QVERIFY2(windowsEdit, qPrintable(error));
        candidates.append(windowsEdit->candidate);

        auto workspaces = catalog.workspacesValues(snapshot, error);
        QVERIFY2(workspaces, qPrintable(error));
        workspaces->insert(
            QStringLiteral("hyprland.animations.workspace_wraparound"), true
        );
        const auto workspacesEdit =
            HyprShelld::CompositorSnapshotEditor::replaceWorkspaces(
                snapshot, 7, catalog.digest(), actions.digest(),
                catalog, actions, *workspaces, userWorkspaceRules(snapshot),
                error
            );
        QVERIFY2(workspacesEdit, qPrintable(error));
        candidates.append(workspacesEdit->candidate);

        auto advanced = catalog.advancedValues(snapshot, error);
        QVERIFY2(advanced, qPrintable(error));
        advanced->insert(
            QStringLiteral("hyprland.misc.allow_session_lock_restore"), true
        );
        const auto advancedEdit =
            HyprShelld::CompositorSnapshotEditor::replaceAdvanced(
                snapshot, 7, catalog.digest(), actions.digest(),
                catalog, actions, *advanced, error
            );
        QVERIFY2(advancedEdit, qPrintable(error));
        candidates.append(advancedEdit->candidate);

        auto windowRules = snapshot.value(
            QStringLiteral("windowRules")
        ).toArray().toVariantList();
        windowRules.append(fullWindowRule().toVariantMap());
        const auto rulesEdit =
            HyprShelld::CompositorSnapshotEditor::replaceRules(
                snapshot, 7, catalog.digest(), actions.digest(),
                catalog, actions, windowRules,
                snapshot.value(QStringLiteral("layerRules"))
                    .toArray().toVariantList(),
                error
            );
        QVERIFY2(rulesEdit, qPrintable(error));
        candidates.append(rulesEdit->candidate);

        QCOMPARE(candidates.size(), 6);
        for (const auto &candidateBytes : candidates) {
            const auto candidate = QJsonDocument::fromJson(
                candidateBytes
            ).object();
            const auto candidateOverrides = candidate.value(
                QStringLiteral("overrides")
            ).toObject();
            QCOMPARE(candidateOverrides.value(QStringLiteral(
                "hyprland.misc.animate_manual_resizes"
            )), QJsonValue(true));
            QCOMPARE(candidateOverrides.value(QStringLiteral(
                "hyprland.misc.animate_mouse_windowdragging"
            )), QJsonValue(true));
            QCOMPARE(candidateOverrides.value(QStringLiteral(
                "hyprland.misc.layers_hog_keyboard_focus"
            )), QJsonValue(false));
        }
    }

    void validatesAdvancedBoundsTypesAndExactMap()
    {
        const auto catalog = trustedCatalog();
        const auto actionCatalog = trustedActionCatalog();
        const auto snapshot = baselineSnapshot();
        QString error;
        const auto accepts = [&](const QVariantMap &values) {
            error.clear();
            return HyprShelld::CompositorSnapshotEditor::replaceAdvanced(
                snapshot,
                7,
                catalog.digest(),
                actionCatalog.digest(),
                catalog,
                actionCatalog,
                values,
                error
            ).has_value();
        };

        auto values = advancedDefaults();
        values.insert(
            QStringLiteral("hyprland.misc.lockdead_screen_delay"), 0
        );
        values.insert(
            QStringLiteral("hyprland.misc.render_unfocused_fps"), 1
        );
        QVERIFY2(accepts(values), qPrintable(error));
        values.insert(
            QStringLiteral("hyprland.misc.lockdead_screen_delay"), 5000
        );
        values.insert(
            QStringLiteral("hyprland.misc.render_unfocused_fps"), 120
        );
        values.insert(QStringLiteral("hyprland.render.direct_scanout"), 1);
        QVERIFY2(accepts(values), qPrintable(error));
        values.insert(QStringLiteral("hyprland.render.direct_scanout"), 2);
        QVERIFY2(accepts(values), qPrintable(error));
        values.insert(QStringLiteral("hyprland.render.fp16_sdr_tf"), 1);
        QVERIFY2(accepts(values), qPrintable(error));
        values.insert(QStringLiteral("hyprland.render.xp_mode"), true);
        QVERIFY2(accepts(values), qPrintable(error));
        values.insert(
            QStringLiteral("hyprland.input-capture.capture_modifiers"), true
        );
        values.insert(
            QStringLiteral("hyprland.input-capture.enforce_barriers"), false
        );
        QVERIFY2(accepts(values), qPrintable(error));

        for (const auto &[id, value] : QList<std::pair<QString, QVariant>>{
                 {QStringLiteral("hyprland.misc.lockdead_screen_delay"), -1},
                 {QStringLiteral("hyprland.misc.lockdead_screen_delay"), 5001},
                 {QStringLiteral("hyprland.misc.render_unfocused_fps"), 0},
                 {QStringLiteral("hyprland.misc.render_unfocused_fps"), 121},
                 {QStringLiteral("hyprland.misc.lockdead_screen_delay"), 2.5},
                 {QStringLiteral("hyprland.misc.render_unfocused_fps"), true},
                 {QStringLiteral("hyprland.misc.screencopy_force_8b"), 1},
                 {QStringLiteral("hyprland.render.direct_scanout"), -1},
                 {QStringLiteral("hyprland.render.direct_scanout"), 3},
                 {QStringLiteral("hyprland.render.direct_scanout"), 1.5},
                 {QStringLiteral("hyprland.render.direct_scanout"), true},
                 {
                     QStringLiteral("hyprland.render.direct_scanout"),
                     QStringLiteral("2"),
                 },
                 {QStringLiteral("hyprland.render.fp16_sdr_tf"), -1},
                 {QStringLiteral("hyprland.render.fp16_sdr_tf"), 2},
                 {QStringLiteral("hyprland.render.fp16_sdr_tf"), 0.5},
                 {QStringLiteral("hyprland.render.fp16_sdr_tf"), true},
                 {
                     QStringLiteral("hyprland.render.fp16_sdr_tf"),
                     QStringLiteral("1"),
                 },
                 {
                     QStringLiteral(
                         "hyprland.misc.allow_session_lock_restore"
                     ),
                     QStringLiteral("true"),
                 },
             }) {
            values = advancedDefaults();
            values.insert(id, value);
            QVERIFY2(!accepts(values), qPrintable(id));
            QCOMPARE(error, QStringLiteral("An advanced value is invalid"));
        }

        for (const auto &id : {
                 QStringLiteral("hyprland.misc.disable_hyprland_logo"),
                 QStringLiteral("hyprland.misc.disable_splash_rendering"),
                 QStringLiteral("hyprland.misc.session_lock_xray"),
                 QStringLiteral("hyprland.misc.session_lock_blur"),
                 QStringLiteral("hyprland.xwayland.use_nearest_neighbor"),
                 QStringLiteral(
                     "hyprland.render.expand_undersized_textures"
                 ),
                 QStringLiteral("hyprland.render.xp_mode"),
                 QStringLiteral("hyprland.input-capture.capture_modifiers"),
                 QStringLiteral("hyprland.input-capture.enforce_barriers"),
             }) {
            for (const auto &value : QVariantList{
                     0, 1, QStringLiteral("false"), QStringLiteral("true"),
                 }) {
                values = advancedDefaults();
                values.insert(id, value);
                QVERIFY2(!accepts(values), qPrintable(id));
                QCOMPARE(
                    error, QStringLiteral("An advanced value is invalid")
                );
            }
        }

        values = advancedDefaults();
        values.remove(
            QStringLiteral("hyprland.misc.disable_scale_notification")
        );
        QVERIFY(!accepts(values));
        QCOMPARE(
            error,
            QStringLiteral("Exactly the supported advanced values are required")
        );
        values = advancedDefaults();
        values.remove(
            QStringLiteral("hyprland.xwayland.use_nearest_neighbor")
        );
        QVERIFY(!accepts(values));
        QCOMPARE(
            error,
            QStringLiteral("Exactly the supported advanced values are required")
        );
        values = advancedDefaults();
        values.remove(
            QStringLiteral("hyprland.render.expand_undersized_textures")
        );
        QVERIFY(!accepts(values));
        QCOMPARE(
            error,
            QStringLiteral("Exactly the supported advanced values are required")
        );
        values = advancedDefaults();
        values.remove(QStringLiteral("hyprland.render.direct_scanout"));
        QVERIFY(!accepts(values));
        QCOMPARE(
            error,
            QStringLiteral("Exactly the supported advanced values are required")
        );
        values = advancedDefaults();
        values.remove(QStringLiteral("hyprland.render.fp16_sdr_tf"));
        QVERIFY(!accepts(values));
        QCOMPARE(
            error,
            QStringLiteral("Exactly the supported advanced values are required")
        );
        values = advancedDefaults();
        values.remove(QStringLiteral("hyprland.render.xp_mode"));
        QVERIFY(!accepts(values));
        QCOMPARE(
            error,
            QStringLiteral("Exactly the supported advanced values are required")
        );
        values = advancedDefaults();
        values.remove(
            QStringLiteral("hyprland.input-capture.capture_modifiers")
        );
        QVERIFY(!accepts(values));
        QCOMPARE(
            error,
            QStringLiteral("Exactly the supported advanced values are required")
        );
        values = advancedDefaults();
        values.remove(
            QStringLiteral("hyprland.input-capture.enforce_barriers")
        );
        QVERIFY(!accepts(values));
        QCOMPARE(
            error,
            QStringLiteral("Exactly the supported advanced values are required")
        );
        values = advancedDefaults();
        values.insert(QStringLiteral("hyprland.misc.unknown"), false);
        QVERIFY(!accepts(values));
        QCOMPARE(
            error,
            QStringLiteral("Exactly the supported advanced values are required")
        );
    }

    void atomicallyReplacesWorkspaceScalarsAndOrderedUserRules()
    {
        const auto catalog = trustedCatalog();
        const auto actions = trustedActionCatalog();
        const auto snapshot = snapshotWithValidComplexSurfaces();
        auto values = changedWorkspacesValues();
        values.insert(
            QStringLiteral("hyprland.gestures.workspace_swipe_distance"),
            777
        );
        const QVariantList rules{
            workspaceRule(
                QStringLiteral("workspace-authored-a"),
                QStringLiteral("special:music"),
                completeWorkspaceRuleOverrides()
            ),
            workspaceRule(
                QStringLiteral("workspace-authored-b"),
                QStringLiteral("2147483647")
            ),
        };

        QString error;
        const auto edit = HyprShelld::CompositorSnapshotEditor::replaceWorkspaces(
            snapshot, 7,
            snapshot.value(QStringLiteral("catalogDigest")).toString(),
            snapshot.value(QStringLiteral("actionCatalogDigest")).toString(),
            catalog, actions, values, rules, error
        );
        QVERIFY2(edit, qPrintable(error));
        QVERIFY(edit->changed);
        const auto candidate = QJsonDocument::fromJson(edit->candidate).object();
        QCOMPARE(
            candidate.value(QStringLiteral("workspaceRules")).toArray().size(),
            3
        );
        auto expectedRules = QJsonArray::fromVariantList(rules);
        expectedRules.append(
            snapshot.value(QStringLiteral("workspaceRules")).toArray().last()
        );
        QCOMPARE(
            candidate.value(QStringLiteral("workspaceRules")).toArray(),
            expectedRules
        );
        for (const auto &surface : {
                 QStringLiteral("monitors"), QStringLiteral("devices"),
                 QStringLiteral("curves"), QStringLiteral("animations"),
                 QStringLiteral("gestures"), QStringLiteral("windowRules"),
                 QStringLiteral("layerRules"), QStringLiteral("submaps"),
                 QStringLiteral("bindings"), QStringLiteral("permissions"),
                 QStringLiteral("environment"),
             }) {
            QCOMPARE(candidate.value(surface), snapshot.value(surface));
        }
        const auto projected = catalog.workspacesValues(candidate, error);
        QVERIFY2(projected, qPrintable(error));
        QCOMPARE(*projected, values);

        const auto unchanged =
            HyprShelld::CompositorSnapshotEditor::replaceWorkspaces(
                candidate, 7, catalog.digest(), actions.digest(),
                catalog, actions, values, rules, error
            );
        QVERIFY2(unchanged, qPrintable(error));
        QVERIFY(!unchanged->changed);
    }

    void rejectsWorkspaceRuleLimitsSpoofsAndInvalidProtectedBaselines()
    {
        const auto catalog = trustedCatalog();
        const auto actions = trustedActionCatalog();
        auto snapshot = baselineSnapshot();
        const auto replace = [&](const QJsonObject &source,
                                 const QVariantList &rules,
                                 QString &error) {
            return HyprShelld::CompositorSnapshotEditor::replaceWorkspaces(
                source, 7, catalog.digest(), actions.digest(), catalog,
                actions, workspacesDefaults(), rules, error
            );
        };
        QString error;

        QVariantList maximum;
        maximum.reserve(HyprShelld::Hyprland::maximumUserWorkspaceRules);
        for (int index = 0;
             index < HyprShelld::Hyprland::maximumUserWorkspaceRules;
             ++index) {
            maximum.append(workspaceRule(
                QStringLiteral("workspace-limit-%1").arg(index),
                QString::number(index + 1)
            ));
        }
        const auto accepted = replace(snapshot, maximum, error);
        QVERIFY2(accepted, qPrintable(error));
        QCOMPARE(
            QJsonDocument::fromJson(accepted->candidate)
                .object().value(QStringLiteral("workspaceRules"))
                .toArray().size(),
            HyprShelld::Hyprland::maximumWorkspaceRules
        );

        auto excessive = maximum;
        excessive.append(QVariant::fromValue(static_cast<QObject *>(nullptr)));
        QVERIFY(!replace(snapshot, excessive, error));
        QVERIFY(error.contains(QStringLiteral("limit")));

        auto reservedId = workspaceRule(
            QString::fromLatin1(
                HyprShelld::Hyprland::sharedSpacingWorkspaceRuleId
            ),
            QStringLiteral("2")
        );
        QVERIFY(!replace(snapshot, {reservedId}, error));
        auto reservedSelector = workspaceRule(
            QStringLiteral("workspace-spoof"),
            QString::fromLatin1(
                HyprShelld::Hyprland::sharedSpacingWorkspaceRuleSelector
            )
        );
        QVERIFY(!replace(snapshot, {reservedSelector}, error));

        const auto protectedRule = snapshot.value(
            QStringLiteral("workspaceRules")
        ).toArray().last();
        snapshot.insert(QStringLiteral("workspaceRules"), QJsonArray{});
        QVERIFY(!replace(snapshot, {}, error));
        snapshot.insert(
            QStringLiteral("workspaceRules"),
            QJsonArray{protectedRule, QJsonObject::fromVariantMap(
                workspaceRule(
                    QStringLiteral("workspace-after-protected"),
                    QStringLiteral("2")
                )
            )}
        );
        QVERIFY(!replace(snapshot, {}, error));
        auto spoof = protectedRule.toObject();
        spoof.insert(QStringLiteral("enabled"), false);
        snapshot.insert(QStringLiteral("workspaceRules"), QJsonArray{spoof});
        QVERIFY(!replace(snapshot, {}, error));
        snapshot.insert(
            QStringLiteral("workspaceRules"),
            QJsonArray{protectedRule, protectedRule}
        );
        QVERIFY(!replace(snapshot, {}, error));
    }

    void editsTheExactInputMapAndPreservesEveryOtherSnapshotSurface()
    {
        const auto catalog = trustedCatalog();
        QVERIFY(catalog.inputContractAvailable());
        QCOMPARE(catalog.inputOptions().size(), 49);
        auto snapshot = snapshotWithValidComplexSurfaces();
        auto devices = snapshot.value(QStringLiteral("devices")).toArray();
        auto savedDevice = devices.at(0).toObject();
        auto savedDeviceOverrides = savedDevice.value(
            QStringLiteral("overrides")
        ).toObject();
        savedDeviceOverrides.insert(
            QStringLiteral("resolve_binds_by_sym"), false
        );
        savedDevice.insert(
            QStringLiteral("overrides"), savedDeviceOverrides
        );
        devices.replace(0, savedDevice);
        snapshot.insert(QStringLiteral("devices"), devices);
        auto originalOverrides = snapshot.value(
            QStringLiteral("overrides")
        ).toObject();
        originalOverrides.insert(
            QStringLiteral("hyprland.cursor.invisible"), true
        );
        snapshot.insert(QStringLiteral("overrides"), originalOverrides);
        auto values = inputDefaults();
        values.insert(QStringLiteral("hyprland.input.repeat_rate"), 91);
        values.insert(QStringLiteral("hyprland.input.repeat_delay"), 740);
        values.insert(QStringLiteral("hyprland.input.sensitivity"), 0.07);
        values.insert(
            QStringLiteral("hyprland.input.accel_profile"),
            QStringLiteral("flat")
        );
        values.insert(QStringLiteral("hyprland.input.natural_scroll"), true);
        values.insert(QStringLiteral("hyprland.input.left_handed"), true);
        values.insert(QStringLiteral("hyprland.input.scroll_factor"), 1.03);
        values.insert(
            QStringLiteral("hyprland.input.touchpad.tap-to-click"), false
        );
        values.insert(
            QStringLiteral("hyprland.input.touchpad.tap-and-drag"), false
        );
        values.insert(
            QStringLiteral("hyprland.input.touchpad.natural_scroll"), true
        );
        values.insert(
            QStringLiteral(
                "hyprland.input.touchpad.disable_while_typing"
            ),
            false
        );
        values.insert(
            QStringLiteral("hyprland.input.touchpad.scroll_factor"), 0.97
        );
        values.insert(
            QStringLiteral("hyprland.input.scroll_method"),
            QStringLiteral("on_button_down")
        );
        values.insert(QStringLiteral("hyprland.input.scroll_button"), 274);
        values.insert(QStringLiteral("hyprland.input.scroll_button_lock"), true);
        values.insert(QStringLiteral("hyprland.input.off_window_axis_events"), 3);
        values.insert(QStringLiteral("hyprland.input.emulate_discrete_scroll"), 2);
        values.insert(
            QStringLiteral("hyprland.input.touchpad.clickfinger_behavior"),
            true
        );
        values.insert(QStringLiteral("hyprland.input.touchpad.drag_3fg"), 2);
        values.insert(QStringLiteral("hyprland.input.touchpad.drag_lock"), 2);
        values.insert(QStringLiteral("hyprland.input.touchpad.flip_x"), true);
        values.insert(QStringLiteral("hyprland.input.touchpad.flip_y"), true);
        values.insert(
            QStringLiteral(
                "hyprland.input.touchpad.middle_button_emulation"
            ),
            true
        );
        values.insert(
            QStringLiteral("hyprland.input.touchpad.tap_button_map"),
            QStringLiteral("lmr")
        );
        values.insert(
            QStringLiteral("hyprland.input.numlock_by_default"), true
        );
        values.insert(
            QStringLiteral("hyprland.input.virtualkeyboard.share_states"),
            1.0
        );
        values.insert(
            QStringLiteral(
                "hyprland.input.virtualkeyboard.release_pressed_on_close"
            ),
            true
        );
        values.insert(
            QStringLiteral("hyprland.misc.name_vk_after_proc"), false
        );
        values.insert(QStringLiteral("hyprland.input.force_no_accel"), true);
        values.insert(QStringLiteral("hyprland.input.rotation"), 137.0);
        values.insert(
            QStringLiteral("hyprland.misc.middle_click_paste"), false
        );
        values.insert(
            QStringLiteral("hyprland.gestures.close_max_timeout"), 1337
        );
        values.insert(
            QStringLiteral("hyprland.input.touchdevice.enabled"), false
        );
        values.insert(
            QStringLiteral("hyprland.input.touchdevice.transform"), 5
        );
        values.insert(
            QStringLiteral("hyprland.input.tablet.relative_input"), true
        );
        values.insert(
            QStringLiteral("hyprland.input.tablet.left_handed"), true
        );
        values.insert(
            QStringLiteral("hyprland.input.tablet.transform"), 6
        );
        values.insert(
            QStringLiteral("hyprland.cursor.hide_on_key_press"), true
        );
        values.insert(
            QStringLiteral("hyprland.cursor.hide_on_touch"), false
        );
        values.insert(
            QStringLiteral("hyprland.cursor.hide_on_tablet"), true
        );
        values.insert(
            QStringLiteral("hyprland.cursor.inactive_timeout"), 2.37
        );
        values.insert(
            QStringLiteral("hyprland.cursor.hotspot_padding"), 13
        );
        values.insert(QStringLiteral("hyprland.cursor.no_warps"), true);
        values.insert(
            QStringLiteral("hyprland.cursor.persistent_warps"), true
        );
        values.insert(
            QStringLiteral(
                "hyprland.cursor.warp_back_after_non_mouse_input"
            ),
            true
        );
        values.insert(
            QStringLiteral("hyprland.input.tablet.region_position"),
            QVariantList{123.125, -456.875}
        );
        values.insert(
            QStringLiteral(
                "hyprland.input.tablet.absolute_region_position"
            ),
            true
        );
        values.insert(
            QStringLiteral("hyprland.input.tablet.region_size"),
            QVariantList{-99.5, 2048.25}
        );
        values.insert(
            QStringLiteral("hyprland.input.resolve_binds_by_sym"), true
        );

        QString error;
        const auto edit = HyprShelld::CompositorSnapshotEditor::replaceInput(
            snapshot,
            7,
            snapshot.value(QStringLiteral("catalogDigest")).toString(),
            snapshot.value(QStringLiteral("actionCatalogDigest")).toString(),
            catalog, trustedActionCatalog(),
            values,
            snapshotGestures(snapshot), error
        );
        QVERIFY2(edit, qPrintable(error));
        QVERIFY(edit->changed);
        QVERIFY(edit->candidate.endsWith('\n'));
        QVERIFY(edit->candidate.size()
                <= HyprShelld::Hyprland::maximumDesiredStateBytes);
        const auto candidate = QJsonDocument::fromJson(
            edit->candidate
        ).object();
        QCOMPARE(candidate.value(QStringLiteral("revision")).toString(),
                 QStringLiteral("7"));
        for (const auto &field : snapshot.keys()) {
            if (field != QStringLiteral("overrides")) {
                QCOMPARE(candidate.value(field), snapshot.value(field));
            }
        }

        const auto overrides = candidate.value(
            QStringLiteral("overrides")
        ).toObject();
        QCOMPARE(overrides.value(
            QStringLiteral("hyprland.misc.disable_hyprland_logo")
        ).toBool(), true);
        QCOMPARE(overrides.value(
            QStringLiteral("hyprland.animations.enabled")
        ).toBool(), false);
        QCOMPARE(overrides.value(
            QStringLiteral("hyprland.cursor.invisible")
        ).toBool(), true);
        QCOMPARE(overrides.value(
            QStringLiteral("hyprland.input.repeat_rate")
        ).toInt(), 91);
        QCOMPARE(overrides.value(
            QStringLiteral("hyprland.input.repeat_delay")
        ).toInt(), 740);
        QCOMPARE(overrides.value(
            QStringLiteral("hyprland.input.sensitivity")
        ).toDouble(), 0.07);
        QCOMPARE(overrides.value(
            QStringLiteral("hyprland.input.accel_profile")
        ).toString(), QStringLiteral("flat"));
        QCOMPARE(overrides.value(
            QStringLiteral("hyprland.input.natural_scroll")
        ).toBool(), true);
        QCOMPARE(overrides.value(
            QStringLiteral("hyprland.input.left_handed")
        ).toBool(), true);
        QCOMPARE(overrides.value(
            QStringLiteral("hyprland.input.scroll_factor")
        ).toDouble(), 1.03);
        QCOMPARE(overrides.value(
            QStringLiteral("hyprland.input.touchpad.tap-to-click")
        ).toBool(), false);
        QCOMPARE(overrides.value(
            QStringLiteral("hyprland.input.touchpad.tap-and-drag")
        ).toBool(), false);
        QCOMPARE(overrides.value(
            QStringLiteral("hyprland.input.touchpad.natural_scroll")
        ).toBool(), true);
        QCOMPARE(overrides.value(QStringLiteral(
            "hyprland.input.touchpad.disable_while_typing"
        )).toBool(), false);
        QCOMPARE(overrides.value(
            QStringLiteral("hyprland.input.touchpad.scroll_factor")
        ).toDouble(), 0.97);
        QCOMPARE(overrides.value(
            QStringLiteral("hyprland.input.scroll_method")
        ).toString(), QStringLiteral("on_button_down"));
        QCOMPARE(overrides.value(
            QStringLiteral("hyprland.input.scroll_button")
        ).toInt(), 274);
        QCOMPARE(overrides.value(
            QStringLiteral("hyprland.input.scroll_button_lock")
        ).toBool(), true);
        QCOMPARE(overrides.value(
            QStringLiteral("hyprland.input.off_window_axis_events")
        ).toInt(), 3);
        QCOMPARE(overrides.value(
            QStringLiteral("hyprland.input.emulate_discrete_scroll")
        ).toInt(), 2);
        QCOMPARE(overrides.value(QStringLiteral(
            "hyprland.input.touchpad.clickfinger_behavior"
        )).toBool(), true);
        QCOMPARE(overrides.value(QStringLiteral(
            "hyprland.input.touchpad.drag_3fg"
        )).toInt(), 2);
        QCOMPARE(overrides.value(QStringLiteral(
            "hyprland.input.touchpad.drag_lock"
        )).toInt(), 2);
        QCOMPARE(overrides.value(QStringLiteral(
            "hyprland.input.touchpad.flip_x"
        )).toBool(), true);
        QCOMPARE(overrides.value(QStringLiteral(
            "hyprland.input.touchpad.flip_y"
        )).toBool(), true);
        QCOMPARE(overrides.value(QStringLiteral(
            "hyprland.input.touchpad.middle_button_emulation"
        )).toBool(), true);
        QCOMPARE(overrides.value(QStringLiteral(
            "hyprland.input.touchpad.tap_button_map"
        )).toString(), QStringLiteral("lmr"));
        QCOMPARE(overrides.value(
            QStringLiteral("hyprland.input.numlock_by_default")
        ).toBool(), true);
        QCOMPARE(overrides.value(
            QStringLiteral("hyprland.input.virtualkeyboard.share_states")
        ).toDouble(), 1.0);
        QCOMPARE(overrides.value(QStringLiteral(
            "hyprland.input.virtualkeyboard.release_pressed_on_close"
        )).toBool(), true);
        QCOMPARE(overrides.value(
            QStringLiteral("hyprland.misc.name_vk_after_proc")
        ).toBool(), false);
        QCOMPARE(overrides.value(
            QStringLiteral("hyprland.input.force_no_accel")
        ).toBool(), true);
        QCOMPARE(overrides.value(
            QStringLiteral("hyprland.input.rotation")
        ).toDouble(), 137.0);
        QCOMPARE(overrides.value(
            QStringLiteral("hyprland.misc.middle_click_paste")
        ).toBool(), false);
        QCOMPARE(overrides.value(
            QStringLiteral("hyprland.gestures.close_max_timeout")
        ).toInt(), 1337);
        QCOMPARE(overrides.value(
            QStringLiteral("hyprland.input.touchdevice.enabled")
        ).toBool(), false);
        QCOMPARE(overrides.value(
            QStringLiteral("hyprland.input.touchdevice.transform")
        ).toInt(), 5);
        QCOMPARE(overrides.value(
            QStringLiteral("hyprland.input.tablet.relative_input")
        ).toBool(), true);
        QCOMPARE(overrides.value(
            QStringLiteral("hyprland.input.tablet.left_handed")
        ).toBool(), true);
        QCOMPARE(overrides.value(
            QStringLiteral("hyprland.input.tablet.transform")
        ).toInt(), 6);
        QCOMPARE(overrides.value(
            QStringLiteral("hyprland.cursor.hide_on_key_press")
        ).toBool(), true);
        QCOMPARE(overrides.value(
            QStringLiteral("hyprland.cursor.hide_on_touch")
        ).toBool(), false);
        QCOMPARE(overrides.value(
            QStringLiteral("hyprland.cursor.hide_on_tablet")
        ).toBool(), true);
        QCOMPARE(overrides.value(
            QStringLiteral("hyprland.cursor.inactive_timeout")
        ).toDouble(), 2.37);
        QCOMPARE(overrides.value(
            QStringLiteral("hyprland.cursor.hotspot_padding")
        ).toInt(), 13);
        QCOMPARE(overrides.value(
            QStringLiteral("hyprland.cursor.no_warps")
        ).toBool(), true);
        QCOMPARE(overrides.value(
            QStringLiteral("hyprland.cursor.persistent_warps")
        ).toBool(), true);
        QCOMPARE(overrides.value(QStringLiteral(
            "hyprland.cursor.warp_back_after_non_mouse_input"
        )).toBool(), true);
        QCOMPARE(overrides.value(
            QStringLiteral("hyprland.input.tablet.region_position")
        ).toArray(), QJsonArray({123.125, -456.875}));
        QCOMPARE(overrides.value(QStringLiteral(
            "hyprland.input.tablet.absolute_region_position"
        )).toBool(), true);
        QCOMPARE(overrides.value(
            QStringLiteral("hyprland.input.tablet.region_size")
        ).toArray(), QJsonArray({-99.5, 2048.25}));
        QCOMPARE(overrides.value(
            QStringLiteral("hyprland.input.resolve_binds_by_sym")
        ).toBool(), true);
        QCOMPARE(
            candidate.value(QStringLiteral("devices")).toArray(),
            devices
        );
        QCOMPARE(
            candidate.value(QStringLiteral("devices")).toArray()
                .at(0).toObject().value(QStringLiteral("overrides"))
                .toObject().value(QStringLiteral("resolve_binds_by_sym"))
                .toBool(),
            false
        );

        const auto projected = catalog.inputValues(candidate, error);
        QVERIFY2(projected, qPrintable(error));
        QCOMPARE(*projected, values);
    }

    void elidesInputDefaultsWithoutTouchingOtherOverrides()
    {
        const auto catalog = trustedCatalog();
        auto snapshot = baselineSnapshot();
        auto overrides = snapshot.value(QStringLiteral("overrides")).toObject();
        overrides.insert(QStringLiteral("hyprland.input.repeat_rate"), 80);
        overrides.insert(QStringLiteral("hyprland.input.repeat_delay"), 700);
        overrides.insert(QStringLiteral("hyprland.input.sensitivity"), -0.2);
        overrides.insert(
            QStringLiteral("hyprland.input.accel_profile"),
            QStringLiteral("adaptive")
        );
        overrides.insert(QStringLiteral("hyprland.input.natural_scroll"), true);
        overrides.insert(QStringLiteral("hyprland.input.left_handed"), true);
        overrides.insert(QStringLiteral("hyprland.input.scroll_factor"), 1.2);
        overrides.insert(
            QStringLiteral("hyprland.input.touchpad.tap-to-click"), false
        );
        overrides.insert(
            QStringLiteral("hyprland.input.touchpad.tap-and-drag"), false
        );
        overrides.insert(
            QStringLiteral("hyprland.input.touchpad.natural_scroll"), true
        );
        overrides.insert(
            QStringLiteral(
                "hyprland.input.touchpad.disable_while_typing"
            ),
            false
        );
        overrides.insert(
            QStringLiteral("hyprland.input.touchpad.scroll_factor"), 1.2
        );
        overrides.insert(
            QStringLiteral("hyprland.input.scroll_method"),
            QStringLiteral("edge")
        );
        overrides.insert(QStringLiteral("hyprland.input.scroll_button"), 274);
        overrides.insert(QStringLiteral("hyprland.input.scroll_button_lock"), true);
        overrides.insert(QStringLiteral("hyprland.input.off_window_axis_events"), 3);
        overrides.insert(QStringLiteral("hyprland.input.emulate_discrete_scroll"), 2);
        overrides.insert(
            QStringLiteral("hyprland.input.touchpad.clickfinger_behavior"),
            true
        );
        overrides.insert(QStringLiteral("hyprland.input.touchpad.drag_3fg"), 2);
        overrides.insert(QStringLiteral("hyprland.input.touchpad.drag_lock"), 2);
        overrides.insert(QStringLiteral("hyprland.input.touchpad.flip_x"), true);
        overrides.insert(QStringLiteral("hyprland.input.touchpad.flip_y"), true);
        overrides.insert(
            QStringLiteral(
                "hyprland.input.touchpad.middle_button_emulation"
            ),
            true
        );
        overrides.insert(
            QStringLiteral("hyprland.input.touchpad.tap_button_map"),
            QStringLiteral("lmr")
        );
        overrides.insert(
            QStringLiteral("hyprland.input.numlock_by_default"), true
        );
        overrides.insert(
            QStringLiteral("hyprland.input.virtualkeyboard.share_states"), 1
        );
        overrides.insert(
            QStringLiteral(
                "hyprland.input.virtualkeyboard.release_pressed_on_close"
            ),
            true
        );
        overrides.insert(
            QStringLiteral("hyprland.misc.name_vk_after_proc"), false
        );
        overrides.insert(
            QStringLiteral("hyprland.input.force_no_accel"), true
        );
        overrides.insert(QStringLiteral("hyprland.input.rotation"), 137);
        overrides.insert(
            QStringLiteral("hyprland.misc.middle_click_paste"), false
        );
        overrides.insert(
            QStringLiteral("hyprland.gestures.close_max_timeout"), 1337
        );
        overrides.insert(
            QStringLiteral("hyprland.input.touchdevice.enabled"), false
        );
        overrides.insert(
            QStringLiteral("hyprland.input.touchdevice.transform"), 5
        );
        overrides.insert(
            QStringLiteral("hyprland.input.tablet.relative_input"), true
        );
        overrides.insert(
            QStringLiteral("hyprland.input.tablet.left_handed"), true
        );
        overrides.insert(
            QStringLiteral("hyprland.input.tablet.transform"), 6
        );
        overrides.insert(
            QStringLiteral("hyprland.cursor.hide_on_key_press"), true
        );
        overrides.insert(
            QStringLiteral("hyprland.cursor.hide_on_touch"), false
        );
        overrides.insert(
            QStringLiteral("hyprland.cursor.hide_on_tablet"), true
        );
        overrides.insert(
            QStringLiteral("hyprland.cursor.inactive_timeout"), 2.37
        );
        overrides.insert(
            QStringLiteral("hyprland.cursor.hotspot_padding"), 13
        );
        overrides.insert(QStringLiteral("hyprland.cursor.no_warps"), true);
        overrides.insert(
            QStringLiteral("hyprland.cursor.persistent_warps"), true
        );
        overrides.insert(
            QStringLiteral(
                "hyprland.cursor.warp_back_after_non_mouse_input"
            ),
            true
        );
        overrides.insert(
            QStringLiteral("hyprland.input.tablet.region_position"),
            QJsonArray{400.5, -600.25}
        );
        overrides.insert(
            QStringLiteral(
                "hyprland.input.tablet.absolute_region_position"
            ),
            true
        );
        overrides.insert(
            QStringLiteral("hyprland.input.tablet.region_size"),
            QJsonArray{-50.5, 900.75}
        );
        overrides.insert(
            QStringLiteral("hyprland.input.resolve_binds_by_sym"), true
        );
        overrides.insert(QStringLiteral("hyprland.cursor.invisible"), true);
        snapshot.insert(QStringLiteral("overrides"), overrides);

        QString error;
        const auto edit = HyprShelld::CompositorSnapshotEditor::replaceInput(
            snapshot,
            7,
            snapshot.value(QStringLiteral("catalogDigest")).toString(),
            snapshot.value(QStringLiteral("actionCatalogDigest")).toString(),
            catalog, trustedActionCatalog(),
            inputDefaults(),
            snapshotGestures(snapshot),
            error
        );
        QVERIFY2(edit, qPrintable(error));
        QVERIFY(edit->changed);
        const auto candidate = QJsonDocument::fromJson(
            edit->candidate
        ).object();
        const auto candidateOverrides = candidate.value(
            QStringLiteral("overrides")
        ).toObject();
        for (const auto &id : catalog.inputOptionIds()) {
            QVERIFY2(!candidateOverrides.contains(id), qPrintable(id));
        }
        QCOMPARE(candidateOverrides.value(
            QStringLiteral("hyprland.misc.disable_hyprland_logo")
        ).toBool(), true);
        QCOMPARE(candidateOverrides.value(
            QStringLiteral("hyprland.animations.enabled")
        ).toBool(), false);
        QCOMPARE(candidateOverrides.value(
            QStringLiteral("hyprland.cursor.invisible")
        ).toBool(), true);

        const auto unchanged =
            HyprShelld::CompositorSnapshotEditor::replaceInput(
                baselineSnapshot(),
                7,
                catalog.digest(),
                baselineSnapshot().value(
                    QStringLiteral("actionCatalogDigest")
                ).toString(),
                catalog, trustedActionCatalog(),
                inputDefaults(),
                snapshotGestures(baselineSnapshot()),
                error
            );
        QVERIFY2(unchanged, qPrintable(error));
        QVERIFY(!unchanged->changed);
    }

    void acceptsInputBoundariesAndNonStepAlignedFiniteValues()
    {
        const auto catalog = trustedCatalog();
        const auto snapshot = baselineSnapshot();
        QString error;

        auto minimums = inputDefaults();
        minimums.insert(QStringLiteral("hyprland.input.repeat_rate"), 0);
        minimums.insert(QStringLiteral("hyprland.input.repeat_delay"), 0);
        minimums.insert(QStringLiteral("hyprland.input.sensitivity"), -1.0);
        minimums.insert(QStringLiteral("hyprland.input.scroll_factor"), 0.0);
        minimums.insert(
            QStringLiteral("hyprland.input.touchpad.scroll_factor"), 0.0
        );
        minimums.insert(QStringLiteral("hyprland.input.scroll_button"), 0);
        minimums.insert(QStringLiteral("hyprland.input.off_window_axis_events"), 0);
        minimums.insert(QStringLiteral("hyprland.input.emulate_discrete_scroll"), 0);
        minimums.insert(QStringLiteral("hyprland.input.touchpad.drag_3fg"), 0);
        minimums.insert(QStringLiteral("hyprland.input.touchpad.drag_lock"), 0);
        minimums.insert(
            QStringLiteral("hyprland.input.virtualkeyboard.share_states"), 0
        );
        minimums.insert(QStringLiteral("hyprland.input.rotation"), 0.0);
        minimums.insert(
            QStringLiteral("hyprland.gestures.close_max_timeout"), 10
        );
        minimums.insert(
            QStringLiteral("hyprland.input.touchdevice.transform"), 0
        );
        minimums.insert(
            QStringLiteral("hyprland.input.tablet.transform"), 0
        );
        minimums.insert(
            QStringLiteral("hyprland.cursor.inactive_timeout"), 0.0
        );
        minimums.insert(
            QStringLiteral("hyprland.cursor.hotspot_padding"), 0
        );
        minimums.insert(
            QStringLiteral("hyprland.input.tablet.region_position"),
            QVariantList{-20000.0, -20000.0}
        );
        minimums.insert(
            QStringLiteral("hyprland.input.tablet.region_size"),
            QVariantList{-100.0, -100.0}
        );
        QVERIFY(HyprShelld::CompositorSnapshotEditor::replaceInput(
            snapshot, 7, catalog.digest(),
            snapshot.value(QStringLiteral("actionCatalogDigest")).toString(),
            catalog, trustedActionCatalog(), minimums,
            snapshotGestures(snapshot), error
        ));

        auto maximums = inputDefaults();
        maximums.insert(QStringLiteral("hyprland.input.repeat_rate"), 200);
        maximums.insert(QStringLiteral("hyprland.input.repeat_delay"), 2000);
        maximums.insert(QStringLiteral("hyprland.input.sensitivity"), 1.0);
        maximums.insert(QStringLiteral("hyprland.input.scroll_factor"), 2.0);
        maximums.insert(
            QStringLiteral("hyprland.input.touchpad.scroll_factor"), 2.0
        );
        maximums.insert(QStringLiteral("hyprland.input.scroll_button"), 300);
        maximums.insert(QStringLiteral("hyprland.input.off_window_axis_events"), 3);
        maximums.insert(QStringLiteral("hyprland.input.emulate_discrete_scroll"), 2);
        maximums.insert(QStringLiteral("hyprland.input.touchpad.drag_3fg"), 2);
        maximums.insert(QStringLiteral("hyprland.input.touchpad.drag_lock"), 2);
        maximums.insert(
            QStringLiteral("hyprland.input.virtualkeyboard.share_states"),
            2.0
        );
        maximums.insert(QStringLiteral("hyprland.input.rotation"), 359.0);
        maximums.insert(
            QStringLiteral("hyprland.input.touchpad.tap_button_map"),
            QStringLiteral("lmr")
        );
        maximums.insert(
            QStringLiteral("hyprland.gestures.close_max_timeout"), 2000
        );
        maximums.insert(
            QStringLiteral("hyprland.input.touchdevice.transform"), 6
        );
        maximums.insert(
            QStringLiteral("hyprland.input.tablet.transform"), 6
        );
        maximums.insert(
            QStringLiteral("hyprland.cursor.inactive_timeout"), 20.0
        );
        maximums.insert(
            QStringLiteral("hyprland.cursor.hotspot_padding"), 20
        );
        maximums.insert(
            QStringLiteral("hyprland.input.tablet.region_position"),
            QVariantList{20000.0, 20000.0}
        );
        maximums.insert(
            QStringLiteral("hyprland.input.tablet.region_size"),
            QVariantList{4000.0, 4000.0}
        );
        QVERIFY(HyprShelld::CompositorSnapshotEditor::replaceInput(
            snapshot, 7, catalog.digest(),
            snapshot.value(QStringLiteral("actionCatalogDigest")).toString(),
            catalog, trustedActionCatalog(), maximums,
            snapshotGestures(snapshot), error
        ));

        auto arbitrary = inputDefaults();
        arbitrary.insert(QStringLiteral("hyprland.input.sensitivity"), 0.07);
        arbitrary.insert(QStringLiteral("hyprland.input.scroll_factor"), 1.03);
        arbitrary.insert(
            QStringLiteral("hyprland.input.touchpad.scroll_factor"), 0.97
        );
        arbitrary.insert(
            QStringLiteral("hyprland.input.virtualkeyboard.share_states"),
            1.0
        );
        arbitrary.insert(QStringLiteral("hyprland.input.rotation"), 137.0);
        arbitrary.insert(
            QStringLiteral("hyprland.input.touchdevice.transform"), 5.0
        );
        arbitrary.insert(
            QStringLiteral("hyprland.input.tablet.transform"), 6.0
        );
        arbitrary.insert(
            QStringLiteral("hyprland.cursor.inactive_timeout"), 2.37
        );
        arbitrary.insert(
            QStringLiteral("hyprland.input.tablet.region_position"),
            QVariantList{123.456789, -987.654321}
        );
        arbitrary.insert(
            QStringLiteral("hyprland.input.tablet.region_size"),
            QVariantList{-99.999999, 0.0}
        );
        const auto edit = HyprShelld::CompositorSnapshotEditor::replaceInput(
            snapshot, 7, catalog.digest(),
            snapshot.value(QStringLiteral("actionCatalogDigest")).toString(),
            catalog, trustedActionCatalog(), arbitrary,
            snapshotGestures(snapshot), error
        );
        QVERIFY2(edit, qPrintable(error));
        const auto candidate = QJsonDocument::fromJson(
            edit->candidate
        ).object();
        const auto projected = catalog.inputValues(candidate, error);
        QVERIFY2(projected, qPrintable(error));
        QCOMPARE(projected->value(
            QStringLiteral("hyprland.input.sensitivity")
        ).toDouble(), 0.07);
        QCOMPARE(projected->value(
            QStringLiteral("hyprland.input.scroll_factor")
        ).toDouble(), 1.03);
        QCOMPARE(projected->value(
            QStringLiteral("hyprland.input.touchpad.scroll_factor")
        ).toDouble(), 0.97);
        QCOMPARE(projected->value(
            QStringLiteral("hyprland.input.virtualkeyboard.share_states")
        ).toDouble(), 1.0);
        QCOMPARE(projected->value(
            QStringLiteral("hyprland.input.rotation")
        ).toDouble(), 137.0);
        QCOMPARE(projected->value(
            QStringLiteral("hyprland.input.touchdevice.transform")
        ).toDouble(), 5.0);
        QCOMPARE(projected->value(
            QStringLiteral("hyprland.input.tablet.transform")
        ).toDouble(), 6.0);
        QCOMPARE(projected->value(
            QStringLiteral("hyprland.cursor.inactive_timeout")
        ).toDouble(), 2.37);

        for (const auto &shareStates : QVariantList{
                 0, 1, 2, 0.0, 1.0, 2.0,
             }) {
            auto exactEnum = inputDefaults();
            exactEnum.insert(
                QStringLiteral(
                    "hyprland.input.virtualkeyboard.share_states"
                ),
                shareStates
            );
            QVERIFY(HyprShelld::CompositorSnapshotEditor::replaceInput(
                snapshot, 7, catalog.digest(),
                snapshot.value(
                    QStringLiteral("actionCatalogDigest")
                ).toString(),
                catalog, trustedActionCatalog(), exactEnum,
                snapshotGestures(snapshot), error
            ));
        }
    }

    void atomicallyReplacesInputScalarsAndOrderedGestures()
    {
        const auto catalog = trustedCatalog();
        const auto actions = trustedActionCatalog();
        const auto snapshot = snapshotWithValidComplexSurfaces();
        auto values = inputDefaults();
        values.insert(
            QStringLiteral("hyprland.gestures.close_max_timeout"), 1350
        );
        const auto edited = gestureRecord(
            QStringLiteral("gesture-special"),
            4,
            QStringLiteral("right"),
            QVariantMap{
                {QStringLiteral("type"), QStringLiteral("special")},
                {QStringLiteral("workspace"), QStringLiteral("music")},
            },
            QVariantList{QStringLiteral("super")},
            1.25
        );
        const auto added = gestureRecord(
            QStringLiteral("gesture-close"),
            3,
            QStringLiteral("down"),
            QVariantMap{{QStringLiteral("type"), QStringLiteral("close")}},
            QVariantList{QStringLiteral("alt")}
        );
        const auto zoom = gestureRecord(
            QStringLiteral("gesture-zoom"),
            5,
            QStringLiteral("pinchOut"),
            QVariantMap{
                {QStringLiteral("type"), QStringLiteral("cursorZoom")},
                {QStringLiteral("zoomLevel"), 2.0},
                {QStringLiteral("mode"), QStringLiteral("live")},
            },
            QVariantList{QStringLiteral("ctrl")}
        );
        const QVariantList gestures{edited, added, zoom};
        QString error;
        const auto replace = [&](const QJsonObject &source,
                                 const QVariantList &records) {
            return HyprShelld::CompositorSnapshotEditor::replaceInput(
                source, 7, catalog.digest(), actions.digest(), catalog,
                actions, values, records, error
            );
        };

        const auto edit = replace(snapshot, gestures);
        QVERIFY2(edit, qPrintable(error));
        QVERIFY(edit->changed);
        const auto candidate = QJsonDocument::fromJson(
            edit->candidate
        ).object();
        QCOMPARE(
            candidate.value(QStringLiteral("gestures")).toArray(),
            QJsonArray::fromVariantList(gestures)
        );
        for (const auto &field : snapshot.keys()) {
            if (field != QStringLiteral("overrides")
                && field != QStringLiteral("gestures")) {
                QCOMPARE(candidate.value(field), snapshot.value(field));
            }
        }
        const auto projected = catalog.inputValues(candidate, error);
        QVERIFY2(projected, qPrintable(error));
        QCOMPARE(*projected, values);

        const QVariantList reordered{zoom, edited, added};
        const auto reorder = replace(candidate, reordered);
        QVERIFY2(reorder, qPrintable(error));
        QVERIFY(reorder->changed);
        const auto reorderedCandidate = QJsonDocument::fromJson(
            reorder->candidate
        ).object();
        QCOMPARE(
            reorderedCandidate.value(QStringLiteral("gestures")).toArray(),
            QJsonArray::fromVariantList(reordered)
        );

        const QVariantList removed{zoom, edited};
        const auto removal = replace(reorderedCandidate, removed);
        QVERIFY2(removal, qPrintable(error));
        QVERIFY(removal->changed);
        const auto removalCandidate = QJsonDocument::fromJson(
            removal->candidate
        ).object();
        QCOMPARE(
            removalCandidate.value(QStringLiteral("gestures")).toArray(),
            QJsonArray::fromVariantList(removed)
        );

        const auto unchanged = replace(removalCandidate, removed);
        QVERIFY2(unchanged, qPrintable(error));
        QVERIFY(!unchanged->changed);
    }

    void preservesCompatibilityGesturesWithoutPermittingMaterialEdits()
    {
        const auto catalog = trustedCatalog();
        const auto actions = trustedActionCatalog();
        auto snapshot = baselineSnapshot();
        const auto active = gestureRecord(
            QStringLiteral("gesture-before-unset"),
            2,
            QStringLiteral("left"),
            QVariantMap{
                {QStringLiteral("type"), QStringLiteral("workspace")},
            },
            QVariantList{QStringLiteral("super")}
        );
        const auto unset = gestureRecord(
            QStringLiteral("gesture-unset"),
            2,
            QStringLiteral("left"),
            QVariantMap{{QStringLiteral("type"), QStringLiteral("unset")}},
            QVariantList{QStringLiteral("super")}
        );
        const auto pinchScale = gestureRecord(
            QStringLiteral("gesture-pinch-scale"),
            3,
            QStringLiteral("pinchIn"),
            QVariantMap{{QStringLiteral("type"), QStringLiteral("close")}},
            QVariantList{QStringLiteral("ctrl")},
            1.5
        );
        const auto pinchScroll = gestureRecord(
            QStringLiteral("gesture-pinch-scroll"),
            4,
            QStringLiteral("pinchOut"),
            QVariantMap{
                {QStringLiteral("type"), QStringLiteral("scrollMove")},
            },
            QVariantList{QStringLiteral("alt")}
        );
        const auto liveNonPinch = gestureRecord(
            QStringLiteral("gesture-live-non-pinch"),
            5,
            QStringLiteral("right"),
            QVariantMap{
                {QStringLiteral("type"), QStringLiteral("cursorZoom")},
                {QStringLiteral("zoomLevel"), 2.0},
                {QStringLiteral("mode"), QStringLiteral("live")},
            },
            QVariantList{QStringLiteral("mod2")}
        );
        const QVariantList compatibility{
            active, unset, pinchScale, pinchScroll, liveNonPinch,
        };
        snapshot.insert(
            QStringLiteral("gestures"),
            QJsonArray::fromVariantList(compatibility)
        );
        QString error;
        const auto replace = [&](const QJsonObject &source,
                                 const QVariantList &records) {
            return HyprShelld::CompositorSnapshotEditor::replaceInput(
                source, 7, catalog.digest(), actions.digest(), catalog,
                actions, inputDefaults(), records, error
            );
        };

        const auto unchanged = replace(snapshot, compatibility);
        QVERIFY2(unchanged, qPrintable(error));
        QVERIFY(!unchanged->changed);

        const QVariantList reordered{
            pinchScale, pinchScroll, active, unset, liveNonPinch,
        };
        const auto reorder = replace(snapshot, reordered);
        QVERIFY2(reorder, qPrintable(error));
        QVERIFY(reorder->changed);
        const auto reorderedCandidate = QJsonDocument::fromJson(
            reorder->candidate
        ).object();
        QCOMPARE(
            reorderedCandidate.value(QStringLiteral("gestures")).toArray(),
            QJsonArray::fromVariantList(reordered)
        );

        const QVariantList retained{active, unset, liveNonPinch};
        const auto removal = replace(reorderedCandidate, retained);
        QVERIFY2(removal, qPrintable(error));
        QVERIFY(removal->changed);
        QCOMPARE(
            QJsonDocument::fromJson(removal->candidate)
                .object().value(QStringLiteral("gestures")).toArray(),
            QJsonArray::fromVariantList(retained)
        );

        const QList<QPair<qsizetype, QVariantMap>> materialEdits{
            {
                1,
                QVariantMap{
                    {QStringLiteral("direction"), QStringLiteral("right")},
                    {
                        QStringLiteral("action"),
                        QVariantMap{
                            {QStringLiteral("type"), QStringLiteral("close")},
                        },
                    },
                },
            },
            {
                2,
                QVariantMap{{QStringLiteral("scale"), 1.0}},
            },
            {
                3,
                QVariantMap{{
                    QStringLiteral("action"),
                    QVariantMap{
                        {QStringLiteral("type"), QStringLiteral("close")},
                    },
                }},
            },
            {
                4,
                QVariantMap{{
                    QStringLiteral("action"),
                    QVariantMap{
                        {QStringLiteral("type"), QStringLiteral("cursorZoom")},
                        {QStringLiteral("zoomLevel"), 2.0},
                        {QStringLiteral("mode"), QStringLiteral("toggle")},
                    },
                }},
            },
        };
        for (const auto &[index, changes] : materialEdits) {
            auto edited = compatibility;
            auto row = edited.at(index).toMap();
            for (auto iterator = changes.cbegin();
                 iterator != changes.cend(); ++iterator) {
                row.insert(iterator.key(), iterator.value());
            }
            edited[index] = row;
            QVERIFY2(!replace(snapshot, edited), qPrintable(error));
            QVERIFY2(
                error.contains(QStringLiteral("Compatibility gestures")),
                qPrintable(error)
            );
        }
    }

    void rejectsInertMalformedAndOversizedGestureCandidates()
    {
        const auto catalog = trustedCatalog();
        const auto actions = trustedActionCatalog();
        const auto snapshot = baselineSnapshot();
        QString error;
        const auto replace = [&](const QJsonObject &source,
                                 const QVariantList &records) {
            return HyprShelld::CompositorSnapshotEditor::replaceInput(
                source, 7, catalog.digest(), actions.digest(), catalog,
                actions, inputDefaults(), records, error
            );
        };

        const QStringList modifiers{
            QStringLiteral("shift"), QStringLiteral("caps"),
            QStringLiteral("ctrl"), QStringLiteral("alt"),
            QStringLiteral("mod2"), QStringLiteral("mod3"),
            QStringLiteral("super"), QStringLiteral("mod5"),
        };
        QVariantList maximum;
        maximum.reserve(HyprShelld::Hyprland::maximumGestures);
        for (qsizetype index = 0;
             index < HyprShelld::Hyprland::maximumGestures; ++index) {
            QVariantList gestureModifiers;
            const auto modifierGroup = index / 8;
            if (modifierGroup > 0) {
                gestureModifiers.append(modifiers.at(modifierGroup - 1));
            }
            maximum.append(gestureRecord(
                QStringLiteral("gesture-limit-%1").arg(index),
                2 + static_cast<int>(index % 8),
                QStringLiteral("left"),
                QVariantMap{
                    {QStringLiteral("type"), QStringLiteral("close")},
                },
                gestureModifiers
            ));
        }
        const auto accepted = replace(snapshot, maximum);
        QVERIFY2(accepted, qPrintable(error));
        QCOMPARE(
            QJsonDocument::fromJson(accepted->candidate)
                .object().value(QStringLiteral("gestures")).toArray().size(),
            HyprShelld::Hyprland::maximumGestures
        );

        auto excessive = maximum;
        excessive.append(gestureRecord(
            QStringLiteral("gesture-limit-64"),
            2,
            QStringLiteral("left"),
            QVariantMap{{QStringLiteral("type"), QStringLiteral("close")}},
            QVariantList{QStringLiteral("mod5")}
        ));
        QVERIFY(!replace(snapshot, excessive));
        QVERIFY(error.contains(QStringLiteral("item limit")));

        const auto unset = gestureRecord(
            QStringLiteral("gesture-new-unset"),
            3,
            QStringLiteral("left"),
            QVariantMap{{QStringLiteral("type"), QStringLiteral("unset")}}
        );
        QVERIFY(!replace(snapshot, {unset}));
        QVERIFY(error.contains(QStringLiteral("Unset gestures")));

        const auto pinchScale = gestureRecord(
            QStringLiteral("gesture-new-pinch-scale"),
            3,
            QStringLiteral("pinch"),
            QVariantMap{{QStringLiteral("type"), QStringLiteral("close")}},
            {},
            1.5
        );
        QVERIFY(!replace(snapshot, {pinchScale}));
        QVERIFY(error.contains(QStringLiteral("scale of 1")));

        const auto pinchScroll = gestureRecord(
            QStringLiteral("gesture-new-pinch-scroll"),
            3,
            QStringLiteral("pinchIn"),
            QVariantMap{
                {QStringLiteral("type"), QStringLiteral("scrollMove")},
            }
        );
        QVERIFY(!replace(snapshot, {pinchScroll}));
        QVERIFY(error.contains(QStringLiteral("Scroll Move")));

        const auto liveNonPinch = gestureRecord(
            QStringLiteral("gesture-new-live-non-pinch"),
            3,
            QStringLiteral("right"),
            QVariantMap{
                {QStringLiteral("type"), QStringLiteral("cursorZoom")},
                {QStringLiteral("zoomLevel"), 2.0},
                {QStringLiteral("mode"), QStringLiteral("live")},
            }
        );
        QVERIFY(!replace(snapshot, {liveNonPinch}));
        QVERIFY(error.contains(QStringLiteral("pinch gesture")));

        auto nonStrict = gestureRecord(
            QStringLiteral("gesture-nonstrict"),
            3,
            QStringLiteral("left"),
            QVariantMap{{QStringLiteral("type"), QStringLiteral("close")}}
        );
        nonStrict.insert(
            QStringLiteral("unsupported"),
            QVariant::fromValue(static_cast<QObject *>(nullptr))
        );
        QVERIFY(!replace(snapshot, {nonStrict}));
        QVERIFY(error.contains(QStringLiteral("finite numbers")));

        auto nonFinite = gestureRecord(
            QStringLiteral("gesture-nonfinite"),
            3,
            QStringLiteral("left"),
            QVariantMap{{QStringLiteral("type"), QStringLiteral("close")}}
        );
        nonFinite.insert(
            QStringLiteral("scale"),
            std::numeric_limits<double>::quiet_NaN()
        );
        QVERIFY(!replace(snapshot, {nonFinite}));
        QVERIFY(error.contains(QStringLiteral("finite numbers")));

        auto unknownField = gestureRecord(
            QStringLiteral("gesture-unknown-field"),
            3,
            QStringLiteral("left"),
            QVariantMap{{QStringLiteral("type"), QStringLiteral("close")}}
        );
        unknownField.insert(QStringLiteral("unknown"), true);
        QVERIFY(!replace(snapshot, {unknownField}));
        QVERIFY(error.contains(QStringLiteral("gestures")));

        const QVariantList shadowed{
            gestureRecord(
                QStringLiteral("gesture-horizontal"),
                3,
                QStringLiteral("horizontal"),
                QVariantMap{
                    {QStringLiteral("type"), QStringLiteral("close")},
                },
                QVariantList{QStringLiteral("super")}
            ),
            gestureRecord(
                QStringLiteral("gesture-shadowed-left"),
                3,
                QStringLiteral("left"),
                QVariantMap{
                    {QStringLiteral("type"), QStringLiteral("workspace")},
                },
                QVariantList{QStringLiteral("super")}
            ),
        };
        QVERIFY(!replace(snapshot, shadowed));
        QVERIFY(error.contains(QStringLiteral("shadows")));

        const auto active = gestureRecord(
            QStringLiteral("gesture-unset-source"),
            4,
            QStringLiteral("up"),
            QVariantMap{{QStringLiteral("type"), QStringLiteral("close")}},
            QVariantList{QStringLiteral("ctrl")}
        );
        const auto existingUnset = gestureRecord(
            QStringLiteral("gesture-existing-unset"),
            4,
            QStringLiteral("up"),
            QVariantMap{{QStringLiteral("type"), QStringLiteral("unset")}},
            QVariantList{QStringLiteral("ctrl")}
        );
        auto unsetSnapshot = snapshot;
        unsetSnapshot.insert(
            QStringLiteral("gestures"),
            QJsonArray::fromVariantList(QVariantList{active, existingUnset})
        );
        QVERIFY(!replace(unsetSnapshot, {existingUnset}));
        QVERIFY(error.contains(QStringLiteral("exact preceding")));
    }

    void invalidSnapshotValuesFailOnlyTheirOwningProjection()
    {
        const auto catalog = trustedCatalog();
        auto snapshot = baselineSnapshot();
        auto overrides = snapshot.value(QStringLiteral("overrides")).toObject();
        QString error;

        overrides.insert(QStringLiteral("hyprland.input.sensitivity"), 1.5);
        snapshot.insert(QStringLiteral("overrides"), overrides);
        const auto appearanceWithBadInput = catalog.appearanceValues(
            snapshot, error
        );
        QVERIFY2(appearanceWithBadInput, qPrintable(error));
        QVERIFY(!catalog.inputValues(snapshot, error));
        const auto windowsWithBadInput = catalog.windowsValues(
            snapshot, error
        );
        QVERIFY2(windowsWithBadInput, qPrintable(error));
        QCOMPARE(*windowsWithBadInput, windowsDefaults());
        const auto workspacesWithBadInput = catalog.workspacesValues(
            snapshot, error
        );
        QVERIFY2(workspacesWithBadInput, qPrintable(error));
        QCOMPARE(*workspacesWithBadInput, workspacesDefaults());

        snapshot = baselineSnapshot();
        overrides = snapshot.value(QStringLiteral("overrides")).toObject();
        overrides.insert(
            QStringLiteral("hyprland.decoration.shadow.range"), 101
        );
        snapshot.insert(QStringLiteral("overrides"), overrides);
        QVERIFY(!catalog.appearanceValues(snapshot, error));
        const auto inputWithBadAppearance = catalog.inputValues(
            snapshot, error
        );
        QVERIFY2(inputWithBadAppearance, qPrintable(error));
        QCOMPARE(*inputWithBadAppearance, inputDefaults());
        const auto windowsWithBadAppearance = catalog.windowsValues(
            snapshot, error
        );
        QVERIFY2(windowsWithBadAppearance, qPrintable(error));
        QCOMPARE(*windowsWithBadAppearance, windowsDefaults());
        const auto workspacesWithBadAppearance = catalog.workspacesValues(
            snapshot, error
        );
        QVERIFY2(workspacesWithBadAppearance, qPrintable(error));
        QCOMPARE(*workspacesWithBadAppearance, workspacesDefaults());

        snapshot = baselineSnapshot();
        overrides = snapshot.value(QStringLiteral("overrides")).toObject();
        overrides.insert(
            QStringLiteral("hyprland.general.extend_border_grab_area"), 101
        );
        snapshot.insert(QStringLiteral("overrides"), overrides);
        QVERIFY(!catalog.windowsValues(snapshot, error));
        const auto appearanceWithBadWindows = catalog.appearanceValues(
            snapshot, error
        );
        QVERIFY2(appearanceWithBadWindows, qPrintable(error));
        QCOMPARE(appearanceWithBadWindows->size(), 40);
        QCOMPARE(appearanceWithBadWindows->value(
            QStringLiteral("hyprland.animations.enabled")
        ).toBool(), false);
        const auto inputWithBadWindows = catalog.inputValues(snapshot, error);
        QVERIFY2(inputWithBadWindows, qPrintable(error));
        QCOMPARE(*inputWithBadWindows, inputDefaults());
        const auto workspacesWithBadWindows = catalog.workspacesValues(
            snapshot, error
        );
        QVERIFY2(workspacesWithBadWindows, qPrintable(error));
        QCOMPARE(*workspacesWithBadWindows, workspacesDefaults());

        snapshot = baselineSnapshot();
        overrides = snapshot.value(QStringLiteral("overrides")).toObject();
        overrides.insert(
            QStringLiteral("hyprland.gestures.workspace_swipe_distance"),
            2001
        );
        snapshot.insert(QStringLiteral("overrides"), overrides);
        QVERIFY(!catalog.workspacesValues(snapshot, error));
        const auto appearanceWithBadWorkspaces = catalog.appearanceValues(
            snapshot, error
        );
        const auto inputWithBadWorkspaces = catalog.inputValues(
            snapshot, error
        );
        const auto windowsWithBadWorkspaces = catalog.windowsValues(
            snapshot, error
        );
        QVERIFY2(appearanceWithBadWorkspaces, qPrintable(error));
        QVERIFY2(inputWithBadWorkspaces, qPrintable(error));
        QVERIFY2(windowsWithBadWorkspaces, qPrintable(error));
        QCOMPARE(*inputWithBadWorkspaces, inputDefaults());
        QCOMPARE(*windowsWithBadWorkspaces, windowsDefaults());

        snapshot = baselineSnapshot();
        overrides = snapshot.value(QStringLiteral("overrides")).toObject();
        overrides.insert(
            QStringLiteral("hyprland.misc.render_unfocused_fps"), 121
        );
        snapshot.insert(QStringLiteral("overrides"), overrides);
        QVERIFY(!catalog.advancedValues(snapshot, error));
        const auto appearanceWithBadAdvanced = catalog.appearanceValues(
            snapshot, error
        );
        const auto inputWithBadAdvanced = catalog.inputValues(snapshot, error);
        const auto windowsWithBadAdvanced = catalog.windowsValues(
            snapshot, error
        );
        const auto workspacesWithBadAdvanced = catalog.workspacesValues(
            snapshot, error
        );
        QVERIFY2(appearanceWithBadAdvanced, qPrintable(error));
        QVERIFY2(inputWithBadAdvanced, qPrintable(error));
        QVERIFY2(windowsWithBadAdvanced, qPrintable(error));
        QVERIFY2(workspacesWithBadAdvanced, qPrintable(error));
        QCOMPARE(*inputWithBadAdvanced, inputDefaults());
        QCOMPARE(*windowsWithBadAdvanced, windowsDefaults());
        QCOMPARE(*workspacesWithBadAdvanced, workspacesDefaults());
    }

    void rejectsPartialUnknownInvalidStaleAndMalformedEdits()
    {
        const auto catalog = trustedCatalog();
        auto snapshot = baselineSnapshot();
        QString error;
        auto values = catalog.appearanceValues(snapshot, error);
        QVERIFY(values);
        const auto rejectsValues = [&](const QVariantMap &candidate) {
            QString candidateError;
            return !HyprShelld::CompositorSnapshotEditor::replaceAppearance(
                snapshot, 7, catalog.digest(),
                snapshot.value(QStringLiteral("actionCatalogDigest"))
                    .toString(),
                catalog, trustedActionCatalog(), candidate,
                snapshot.value(QStringLiteral("curves"))
                    .toArray().toVariantList(),
                snapshot.value(QStringLiteral("animations"))
                    .toArray().toVariantList(),
                candidateError
            );
        };

        auto invalid = *values;
        invalid.remove(QStringLiteral(
            "hyprland.decoration.border_part_of_window"
        ));
        QVERIFY(rejectsValues(invalid));

        invalid = *values;
        invalid.remove(QStringLiteral(
            "hyprland.decoration.rounding_power"
        ));
        QVERIFY(rejectsValues(invalid));

        for (const auto &id : {
                 QStringLiteral("hyprland.decoration.shadow.range"),
                 QStringLiteral("hyprland.decoration.shadow.render_power"),
                 QStringLiteral("hyprland.decoration.shadow.sharp"),
                 QStringLiteral("hyprland.decoration.shadow.offset"),
                 QStringLiteral("hyprland.decoration.shadow.scale"),
                 QStringLiteral("hyprland.decoration.glow.enabled"),
                 QStringLiteral("hyprland.decoration.glow.range"),
                 QStringLiteral("hyprland.decoration.glow.render_power"),
             }) {
            invalid = *values;
            invalid.remove(id);
            QVERIFY2(rejectsValues(invalid), qPrintable(id));
        }

        invalid = *values;
        invalid.remove(QStringLiteral("hyprland.animations.enabled"));
        QVERIFY(!HyprShelld::CompositorSnapshotEditor::replaceAppearance(
            snapshot, 7, catalog.digest(),
            snapshot.value(QStringLiteral("actionCatalogDigest")).toString(),
            catalog, trustedActionCatalog(), invalid,
            snapshot.value(QStringLiteral("curves")).toArray().toVariantList(),
            snapshot.value(QStringLiteral("animations"))
                .toArray().toVariantList(),
            error
        ));

        invalid = *values;
        invalid.insert(QStringLiteral("hyprland.unknown"), true);
        QVERIFY(!HyprShelld::CompositorSnapshotEditor::replaceAppearance(
            snapshot, 7, catalog.digest(),
            snapshot.value(QStringLiteral("actionCatalogDigest")).toString(),
            catalog, trustedActionCatalog(), invalid,
            snapshot.value(QStringLiteral("curves")).toArray().toVariantList(),
            snapshot.value(QStringLiteral("animations"))
                .toArray().toVariantList(),
            error
        ));

        invalid = *values;
        invalid.insert(QStringLiteral("hyprland.general.border_size"), 21);
        QVERIFY(!HyprShelld::CompositorSnapshotEditor::replaceAppearance(
            snapshot, 7, catalog.digest(),
            snapshot.value(QStringLiteral("actionCatalogDigest")).toString(),
            catalog, trustedActionCatalog(), invalid,
            snapshot.value(QStringLiteral("curves")).toArray().toVariantList(),
            snapshot.value(QStringLiteral("animations"))
                .toArray().toVariantList(),
            error
        ));

        invalid = *values;
        invalid.insert(
            QStringLiteral("hyprland.decoration.dim_inactive"), 1
        );
        QVERIFY(!HyprShelld::CompositorSnapshotEditor::replaceAppearance(
            snapshot, 7, catalog.digest(),
            snapshot.value(QStringLiteral("actionCatalogDigest")).toString(),
            catalog, trustedActionCatalog(), invalid,
            snapshot.value(QStringLiteral("curves")).toArray().toVariantList(),
            snapshot.value(QStringLiteral("animations"))
                .toArray().toVariantList(),
            error
        ));

        invalid = *values;
        invalid.insert(QStringLiteral("hyprland.decoration.dim_modal"), 1);
        QVERIFY(!HyprShelld::CompositorSnapshotEditor::replaceAppearance(
            snapshot, 7, catalog.digest(),
            snapshot.value(QStringLiteral("actionCatalogDigest")).toString(),
            catalog, trustedActionCatalog(), invalid,
            snapshot.value(QStringLiteral("curves")).toArray().toVariantList(),
            snapshot.value(QStringLiteral("animations"))
                .toArray().toVariantList(),
            error
        ));

        const QStringList appearanceBooleanIds{
            QStringLiteral("hyprland.decoration.blur.ignore_opacity"),
            QStringLiteral("hyprland.decoration.blur.new_optimizations"),
            QStringLiteral("hyprland.decoration.blur.xray"),
            QStringLiteral("hyprland.decoration.blur.special"),
            QStringLiteral("hyprland.decoration.blur.popups"),
            QStringLiteral("hyprland.decoration.blur.input_methods"),
            QStringLiteral("hyprland.decoration.border_part_of_window"),
            QStringLiteral("hyprland.decoration.shadow.sharp"),
            QStringLiteral("hyprland.decoration.glow.enabled"),
        };
        for (const auto &id : appearanceBooleanIds) {
            invalid = *values;
            invalid.insert(id, 1);
            QVERIFY2(rejectsValues(invalid), qPrintable(id));
        }

        const std::array appearanceIntegerContracts{
            std::tuple{
                QStringLiteral("hyprland.decoration.blur.size"), 0, 100,
            },
            std::tuple{
                QStringLiteral("hyprland.decoration.blur.passes"), 0, 10,
            },
            std::tuple{
                QStringLiteral("hyprland.decoration.shadow.range"), 0, 100,
            },
            std::tuple{
                QStringLiteral("hyprland.decoration.shadow.render_power"),
                1,
                4,
            },
            std::tuple{
                QStringLiteral("hyprland.decoration.glow.range"), 0, 100,
            },
            std::tuple{
                QStringLiteral("hyprland.decoration.glow.render_power"),
                1,
                4,
            },
        };
        for (const auto &[id, minimum, maximum]
             : appearanceIntegerContracts) {
            for (const auto validValue : {minimum, maximum}) {
                auto valid = *values;
                valid.insert(id, validValue);
                QVERIFY2(!rejectsValues(valid), qPrintable(id));
            }
            for (const auto invalidValue : {minimum - 1, maximum + 1}) {
                invalid = *values;
                invalid.insert(id, invalidValue);
                QVERIFY2(rejectsValues(invalid), qPrintable(id));
            }
            invalid = *values;
            invalid.insert(id, 1.5);
            QVERIFY2(rejectsValues(invalid), qPrintable(id));
        }

        const QStringList unitNumberIds{
            QStringLiteral("hyprland.decoration.dim_strength"),
            QStringLiteral("hyprland.decoration.active_opacity"),
            QStringLiteral("hyprland.decoration.inactive_opacity"),
            QStringLiteral("hyprland.decoration.fullscreen_opacity"),
            QStringLiteral("hyprland.decoration.dim_special"),
            QStringLiteral("hyprland.decoration.dim_around"),
            QStringLiteral(
                "hyprland.decoration.blur.popups_ignorealpha"
            ),
            QStringLiteral(
                "hyprland.decoration.blur.input_methods_ignorealpha"
            ),
            QStringLiteral("hyprland.decoration.blur.noise"),
            QStringLiteral("hyprland.decoration.blur.vibrancy"),
            QStringLiteral("hyprland.decoration.blur.vibrancy_darkness"),
        };
        for (const auto &id : unitNumberIds) {
            for (const auto invalidValue : {-0.01, 1.01}) {
                invalid = *values;
                invalid.insert(id, invalidValue);
                QVERIFY(!HyprShelld::CompositorSnapshotEditor::replaceAppearance(
                    snapshot, 7, catalog.digest(),
                    snapshot.value(
                        QStringLiteral("actionCatalogDigest")
                    ).toString(),
                    catalog, trustedActionCatalog(), invalid,
                    snapshot.value(QStringLiteral("curves"))
                        .toArray().toVariantList(),
                    snapshot.value(QStringLiteral("animations"))
                        .toArray().toVariantList(),
                    error
                ));
            }

            invalid = *values;
            invalid.insert(id, QStringLiteral("0.5"));
            QVERIFY(!HyprShelld::CompositorSnapshotEditor::replaceAppearance(
                snapshot, 7, catalog.digest(),
                snapshot.value(
                    QStringLiteral("actionCatalogDigest")
                ).toString(),
                catalog, trustedActionCatalog(), invalid,
                snapshot.value(QStringLiteral("curves"))
                    .toArray().toVariantList(),
                snapshot.value(QStringLiteral("animations"))
                    .toArray().toVariantList(),
                error
            ));
        }

        const std::array modulationContracts{
            std::tuple{
                QStringLiteral("hyprland.decoration.blur.brightness"),
                2.0,
            },
            std::tuple{
                QStringLiteral("hyprland.decoration.blur.contrast"),
                2.0,
            },
            std::tuple{
                QStringLiteral("hyprland.decoration.blur.noise"),
                1.0,
            },
            std::tuple{
                QStringLiteral("hyprland.decoration.blur.vibrancy"),
                1.0,
            },
            std::tuple{
                QStringLiteral(
                    "hyprland.decoration.blur.vibrancy_darkness"
                ),
                1.0,
            },
            std::tuple{
                QStringLiteral("hyprland.decoration.shadow.scale"),
                1.0,
            },
        };
        for (const auto &[id, maximum] : modulationContracts) {
            for (const auto &validBoundary : {0.0, maximum}) {
                auto valid = *values;
                valid.insert(id, validBoundary);
                QVERIFY2(!rejectsValues(valid), qPrintable(id));
            }
            for (const auto &invalidValue : QVariantList{
                     -0.01,
                     maximum + 0.01,
                     QStringLiteral("0.5"),
                     true,
                     std::numeric_limits<double>::quiet_NaN(),
                     std::numeric_limits<double>::infinity(),
                     -std::numeric_limits<double>::infinity(),
                 }) {
                invalid = *values;
                invalid.insert(id, invalidValue);
                QVERIFY2(rejectsValues(invalid), qPrintable(id));
            }
        }

        for (const auto &validBoundary : {2.0, 10.0, 2.573, 7.421}) {
            auto valid = *values;
            valid.insert(
                QStringLiteral("hyprland.decoration.rounding_power"),
                validBoundary
            );
            QVERIFY(!rejectsValues(valid));
        }
        for (const auto &invalidValue : QVariantList{
                 1.999,
                 10.001,
                 QStringLiteral("2.5"),
                 true,
                 std::numeric_limits<double>::quiet_NaN(),
                 std::numeric_limits<double>::infinity(),
                 -std::numeric_limits<double>::infinity(),
             }) {
            invalid = *values;
            invalid.insert(
                QStringLiteral("hyprland.decoration.rounding_power"),
                invalidValue
            );
            QVERIFY(rejectsValues(invalid));
        }

        const auto offsetId = QStringLiteral(
            "hyprland.decoration.shadow.offset"
        );
        for (const auto &validOffset : {
                 QVariantList{-250.0, 250.0},
                 QVariantList{250.0, -250.0},
                 QVariantList{125.5, -80.25},
                 QVariantList{-0.0, 0.0},
             }) {
            auto valid = *values;
            valid.insert(offsetId, validOffset);
            QVERIFY2(!rejectsValues(valid), qPrintable(offsetId));
        }
        const QList<QVariant> invalidOffsets{
            QVariant::fromValue(QVariantList{}),
            QVariant::fromValue(QVariantList{0.0}),
            QVariant::fromValue(QVariantList{0.0, 0.0, 0.0}),
            QVariant::fromValue(QVariantList{QStringLiteral("0"), 0.0}),
            QVariant::fromValue(QVariantList{true, 0.0}),
            QVariant::fromValue(QVariantList{
                std::numeric_limits<double>::quiet_NaN(), 0.0,
            }),
            QVariant::fromValue(QVariantList{
                std::numeric_limits<double>::infinity(), 0.0,
            }),
            QVariant::fromValue(QVariantList{
                -std::numeric_limits<double>::infinity(), 0.0,
            }),
            QVariant::fromValue(QVariantList{-250.001, 0.0}),
            QVariant::fromValue(QVariantList{250.001, 0.0}),
            QVariant::fromValue(QVariantList{0.0, -250.001}),
            QVariant::fromValue(QVariantList{0.0, 250.001}),
            QVariant::fromValue(0.0),
            QVariant::fromValue(QVariantMap{}),
        };
        for (const auto &invalidOffset : invalidOffsets) {
            invalid = *values;
            invalid.insert(offsetId, invalidOffset);
            QVERIFY2(rejectsValues(invalid), qPrintable(offsetId));
        }

        invalid = *values;
        invalid.insert(
            QStringLiteral("hyprland.general.layout"),
            QStringLiteral("plugin-layout")
        );
        QVERIFY(!HyprShelld::CompositorSnapshotEditor::replaceAppearance(
            snapshot, 7, catalog.digest(),
            snapshot.value(QStringLiteral("actionCatalogDigest")).toString(),
            catalog, trustedActionCatalog(), invalid,
            snapshot.value(QStringLiteral("curves")).toArray().toVariantList(),
            snapshot.value(QStringLiteral("animations"))
                .toArray().toVariantList(),
            error
        ));

        QVERIFY(!HyprShelld::CompositorSnapshotEditor::replaceAppearance(
            snapshot, 6, catalog.digest(),
            snapshot.value(QStringLiteral("actionCatalogDigest")).toString(),
            catalog, trustedActionCatalog(), *values,
            snapshot.value(QStringLiteral("curves")).toArray().toVariantList(),
            snapshot.value(QStringLiteral("animations"))
                .toArray().toVariantList(),
            error
        ));
        snapshot.insert(QStringLiteral("revision"), QStringLiteral("07"));
        QVERIFY(!HyprShelld::CompositorSnapshotEditor::replaceAppearance(
            snapshot, 7, catalog.digest(),
            snapshot.value(QStringLiteral("actionCatalogDigest")).toString(),
            catalog, trustedActionCatalog(), *values,
            snapshot.value(QStringLiteral("curves")).toArray().toVariantList(),
            snapshot.value(QStringLiteral("animations"))
                .toArray().toVariantList(),
            error
        ));
        snapshot.insert(QStringLiteral("revision"), QString::number(
            std::numeric_limits<qulonglong>::max()
        ));
        QVERIFY(!HyprShelld::CompositorSnapshotEditor::replaceAppearance(
            snapshot, std::numeric_limits<qulonglong>::max(), catalog.digest(),
            snapshot.value(QStringLiteral("actionCatalogDigest")).toString(),
            catalog, trustedActionCatalog(), *values,
            snapshot.value(QStringLiteral("curves")).toArray().toVariantList(),
            snapshot.value(QStringLiteral("animations"))
                .toArray().toVariantList(),
            error
        ));
        snapshot.insert(QStringLiteral("revision"), QStringLiteral("7"));
        snapshot.remove(QStringLiteral("devices"));
        QVERIFY(!HyprShelld::CompositorSnapshotEditor::replaceAppearance(
            snapshot, 7, catalog.digest(),
            snapshot.value(QStringLiteral("actionCatalogDigest")).toString(),
            catalog, trustedActionCatalog(), *values,
            snapshot.value(QStringLiteral("curves")).toArray().toVariantList(),
            snapshot.value(QStringLiteral("animations"))
                .toArray().toVariantList(),
            error
        ));
    }

    void acceptsWindowsBoundariesAndIntegralNumericEnumRoundTrips()
    {
        const auto catalog = trustedCatalog();
        const auto snapshot = baselineSnapshot();
        const auto actionDigest = snapshot.value(
            QStringLiteral("actionCatalogDigest")
        ).toString();
        QString error;

        auto minimums = windowsDefaults();
        minimums.insert(
            QStringLiteral("hyprland.general.extend_border_grab_area"), 0
        );
        minimums.insert(QStringLiteral("hyprland.general.resize_corner"), 0.0);
        minimums.insert(
            QStringLiteral("hyprland.general.snap.monitor_gap"), 0
        );
        minimums.insert(
            QStringLiteral("hyprland.general.snap.window_gap"), 0
        );
        minimums.insert(QStringLiteral("hyprland.input.follow_mouse"), 0.0);
        minimums.insert(
            QStringLiteral("hyprland.input.follow_mouse_shrink"), 0
        );
        minimums.insert(
            QStringLiteral("hyprland.input.float_switch_override_focus"),
            0.0
        );
        minimums.insert(
            QStringLiteral("hyprland.input.focus_on_close"), 0.0
        );
        minimums.insert(
            QStringLiteral("hyprland.general.float_gaps"),
            QVariantList{
                -9007199254740991LL,
                0,
                1,
                9007199254740991LL,
            }
        );
        minimums.insert(
            QStringLiteral("hyprland.layout.single_window_aspect_ratio"),
            QVariantList{0.0, 0.0}
        );
        minimums.insert(
            QStringLiteral(
                "hyprland.layout.single_window_aspect_ratio_tolerance"
            ),
            0.0
        );
        minimums.insert(
            QStringLiteral("hyprland.dwindle.default_split_ratio"), 0.1
        );
        minimums.insert(
            QStringLiteral("hyprland.dwindle.split_width_multiplier"), 0.1
        );
        minimums.insert(QStringLiteral("hyprland.master.mfact"), 0.0);
        minimums.insert(
            QStringLiteral("hyprland.scrolling.column_width"), 0.1
        );
        minimums.insert(
            QStringLiteral("hyprland.scrolling.follow_min_visible"), 0.0
        );
        minimums.insert(
            QStringLiteral("hyprland.group.drag_into_group"), 0.0
        );
        minimums.insert(
            QStringLiteral("hyprland.misc.anr_missed_pings"), 1
        );
        minimums.insert(
            QStringLiteral("hyprland.misc.on_focus_under_fullscreen"), 0.0
        );
        minimums.insert(
            QStringLiteral("hyprland.input.follow_mouse_threshold"), 0.0
        );
        QVERIFY(HyprShelld::CompositorSnapshotEditor::replaceWindows(
            snapshot, 7, catalog.digest(), actionDigest,
            catalog, trustedActionCatalog(), minimums, error
        ));

        auto maximums = windowsDefaults();
        maximums.insert(
            QStringLiteral("hyprland.general.extend_border_grab_area"), 100
        );
        maximums.insert(QStringLiteral("hyprland.general.resize_corner"), 4.0);
        maximums.insert(
            QStringLiteral("hyprland.general.snap.monitor_gap"), 100
        );
        maximums.insert(
            QStringLiteral("hyprland.general.snap.window_gap"), 100
        );
        maximums.insert(QStringLiteral("hyprland.input.follow_mouse"), 3.0);
        maximums.insert(
            QStringLiteral("hyprland.input.follow_mouse_shrink"), 300
        );
        maximums.insert(
            QStringLiteral("hyprland.input.float_switch_override_focus"),
            2.0
        );
        maximums.insert(
            QStringLiteral("hyprland.input.focus_on_close"), 2.0
        );
        maximums.insert(
            QStringLiteral("hyprland.general.float_gaps"),
            QVariantList{0, 4, -5, 6}
        );
        maximums.insert(
            QStringLiteral("hyprland.general.gaps_workspaces"), 100
        );
        maximums.insert(
            QStringLiteral("hyprland.layout.single_window_aspect_ratio"),
            QVariantList{1000.0, 1000.0}
        );
        maximums.insert(
            QStringLiteral(
                "hyprland.layout.single_window_aspect_ratio_tolerance"
            ),
            1.0
        );
        maximums.insert(
            QStringLiteral("hyprland.dwindle.default_split_ratio"), 1.9
        );
        maximums.insert(QStringLiteral("hyprland.dwindle.force_split"), 2.0);
        maximums.insert(
            QStringLiteral("hyprland.dwindle.split_width_multiplier"), 3.0
        );
        maximums.insert(QStringLiteral("hyprland.master.mfact"), 1.0);
        maximums.insert(
            QStringLiteral("hyprland.master.slave_count_for_center_master"),
            10
        );
        maximums.insert(
            QStringLiteral("hyprland.scrolling.column_width"), 1.0
        );
        maximums.insert(
            QStringLiteral("hyprland.scrolling.focus_fit_method"), 0.0
        );
        maximums.insert(
            QStringLiteral("hyprland.scrolling.follow_min_visible"), 1.0
        );
        maximums.insert(
            QStringLiteral("hyprland.group.drag_into_group"), 2.0
        );
        maximums.insert(
            QStringLiteral("hyprland.misc.anr_missed_pings"), 20
        );
        maximums.insert(
            QStringLiteral("hyprland.misc.on_focus_under_fullscreen"), 2.0
        );
        maximums.insert(
            QStringLiteral("hyprland.misc.swallow_regex"),
            QString(4096, QLatin1Char('a'))
        );
        maximums.insert(
            QStringLiteral("hyprland.misc.swallow_exception_regex"),
            QString(4096, QLatin1Char('b'))
        );
        maximums.insert(
            QStringLiteral("hyprland.input.follow_mouse_threshold"),
            1000000.0
        );
        const auto edit =
            HyprShelld::CompositorSnapshotEditor::replaceWindows(
                snapshot, 7, catalog.digest(), actionDigest,
                catalog, trustedActionCatalog(), maximums, error
            );
        QVERIFY2(edit, qPrintable(error));
        const auto candidate = QJsonDocument::fromJson(
            edit->candidate
        ).object();
        const auto projected = catalog.windowsValues(candidate, error);
        QVERIFY2(projected, qPrintable(error));
        QCOMPARE(*projected, maximums);
    }

    void validatesStrictGroupbarScalarValues()
    {
        const auto catalog = trustedCatalog();
        const auto snapshot = baselineSnapshot();
        const auto actionDigest = snapshot.value(
            QStringLiteral("actionCatalogDigest")
        ).toString();
        QString error;
        const auto replace = [&](const QVariantMap &values) {
            return HyprShelld::CompositorSnapshotEditor::replaceWindows(
                snapshot,
                7,
                catalog.digest(),
                actionDigest,
                catalog,
                trustedActionCatalog(),
                values,
                error
            );
        };
        const auto rejected = [&replace](const QVariantMap &values) {
            return !replace(values);
        };

        auto minimums = windowsDefaults();
        minimums.insert(
            QStringLiteral("hyprland.group.groupbar.font_family"),
            QString(4096, QLatin1Char('F'))
        );
        minimums.insert(
            QStringLiteral("hyprland.group.groupbar.font_weight_active"),
            0
        );
        minimums.insert(
            QStringLiteral("hyprland.group.groupbar.font_weight_inactive"),
            0
        );
        minimums.insert(QStringLiteral("hyprland.group.groupbar.font_size"), 2);
        minimums.insert(QStringLiteral("hyprland.group.groupbar.height"), 1);
        minimums.insert(
            QStringLiteral("hyprland.group.groupbar.indicator_gap"), 0
        );
        minimums.insert(
            QStringLiteral("hyprland.group.groupbar.indicator_height"), 1
        );
        minimums.insert(QStringLiteral("hyprland.group.groupbar.priority"), 0);
        minimums.insert(
            QStringLiteral("hyprland.group.groupbar.rounding"), 0
        );
        minimums.insert(
            QStringLiteral("hyprland.group.groupbar.rounding_power"), 2.573
        );
        minimums.insert(
            QStringLiteral("hyprland.group.groupbar.gradient_rounding"), 0
        );
        minimums.insert(
            QStringLiteral(
                "hyprland.group.groupbar.gradient_rounding_power"
            ),
            7.421
        );
        minimums.insert(
            QStringLiteral("hyprland.group.groupbar.gaps_out"), 0
        );
        minimums.insert(
            QStringLiteral("hyprland.group.groupbar.gaps_in"), 0
        );
        minimums.insert(
            QStringLiteral("hyprland.group.groupbar.text_offset"), -20
        );
        minimums.insert(
            QStringLiteral("hyprland.group.groupbar.text_padding"), 0
        );
        const auto nonStepEdit = replace(minimums);
        QVERIFY2(nonStepEdit, qPrintable(error));
        const auto nonStepCandidate = QJsonDocument::fromJson(
            nonStepEdit->candidate
        ).object();
        QCOMPARE(
            nonStepCandidate.value(QStringLiteral("overrides"))
                .toObject()
                .value(QStringLiteral(
                    "hyprland.group.groupbar.rounding_power"
                ))
                .toDouble(),
            2.573
        );
        const auto nonStepProjection = catalog.windowsValues(
            nonStepCandidate, error
        );
        QVERIFY2(nonStepProjection, qPrintable(error));
        QCOMPARE(*nonStepProjection, minimums);

        auto maximums = windowsDefaults();
        maximums.insert(
            QStringLiteral("hyprland.group.groupbar.font_weight_active"),
            2147483647
        );
        maximums.insert(
            QStringLiteral("hyprland.group.groupbar.font_weight_inactive"),
            2147483647
        );
        maximums.insert(
            QStringLiteral("hyprland.group.groupbar.font_size"), 64
        );
        maximums.insert(QStringLiteral("hyprland.group.groupbar.height"), 64);
        maximums.insert(
            QStringLiteral("hyprland.group.groupbar.indicator_gap"), 64
        );
        maximums.insert(
            QStringLiteral("hyprland.group.groupbar.indicator_height"), 64
        );
        maximums.insert(QStringLiteral("hyprland.group.groupbar.priority"), 6);
        maximums.insert(
            QStringLiteral("hyprland.group.groupbar.rounding"), 20
        );
        maximums.insert(
            QStringLiteral("hyprland.group.groupbar.rounding_power"), 10.0
        );
        maximums.insert(
            QStringLiteral("hyprland.group.groupbar.gradient_rounding"), 20
        );
        maximums.insert(
            QStringLiteral(
                "hyprland.group.groupbar.gradient_rounding_power"
            ),
            10.0
        );
        maximums.insert(
            QStringLiteral("hyprland.group.groupbar.gaps_out"), 20
        );
        maximums.insert(
            QStringLiteral("hyprland.group.groupbar.gaps_in"), 20
        );
        maximums.insert(
            QStringLiteral("hyprland.group.groupbar.text_offset"), 20
        );
        maximums.insert(
            QStringLiteral("hyprland.group.groupbar.text_padding"), 22
        );
        QVERIFY2(replace(maximums), qPrintable(error));

        auto invalid = windowsDefaults();
        invalid.remove(QStringLiteral("hyprland.group.groupbar.blur"));
        QVERIFY(rejected(invalid));
        invalid = windowsDefaults();
        invalid.insert(
            QStringLiteral("hyprland.group.groupbar.unreviewed"), true
        );
        QVERIFY(rejected(invalid));

        const QStringList booleanIds{
            QStringLiteral("hyprland.group.groupbar.enabled"),
            QStringLiteral("hyprland.group.groupbar.disable_when_only"),
            QStringLiteral("hyprland.group.groupbar.gradients"),
            QStringLiteral("hyprland.group.groupbar.stacked"),
            QStringLiteral("hyprland.group.groupbar.render_titles"),
            QStringLiteral("hyprland.group.groupbar.scrolling"),
            QStringLiteral("hyprland.group.groupbar.middle_click_close"),
            QStringLiteral("hyprland.group.groupbar.round_only_edges"),
            QStringLiteral(
                "hyprland.group.groupbar.gradient_round_only_edges"
            ),
            QStringLiteral("hyprland.group.groupbar.keep_upper_gap"),
            QStringLiteral("hyprland.group.groupbar.blur"),
        };
        for (const auto &id : booleanIds) {
            invalid = windowsDefaults();
            invalid.insert(id, 1);
            QVERIFY2(rejected(invalid), qPrintable(id));
            invalid = windowsDefaults();
            invalid.insert(id, QStringLiteral("true"));
            QVERIFY2(rejected(invalid), qPrintable(id));
        }

        const QStringList integerIds{
            QStringLiteral("hyprland.group.groupbar.font_size"),
            QStringLiteral("hyprland.group.groupbar.height"),
            QStringLiteral("hyprland.group.groupbar.indicator_gap"),
            QStringLiteral("hyprland.group.groupbar.indicator_height"),
            QStringLiteral("hyprland.group.groupbar.priority"),
            QStringLiteral("hyprland.group.groupbar.rounding"),
            QStringLiteral("hyprland.group.groupbar.gradient_rounding"),
            QStringLiteral("hyprland.group.groupbar.gaps_out"),
            QStringLiteral("hyprland.group.groupbar.gaps_in"),
            QStringLiteral("hyprland.group.groupbar.text_offset"),
            QStringLiteral("hyprland.group.groupbar.text_padding"),
        };
        for (const auto &id : integerIds) {
            invalid = windowsDefaults();
            invalid.insert(id, windowsDefaults().value(id).toDouble() + 0.5);
            QVERIFY2(rejected(invalid), qPrintable(id));
            invalid = windowsDefaults();
            invalid.insert(id, QStringLiteral("1"));
            QVERIFY2(rejected(invalid), qPrintable(id));
        }
        invalid = windowsDefaults();
        invalid.insert(
            QStringLiteral("hyprland.group.groupbar.height"),
            std::numeric_limits<double>::quiet_NaN()
        );
        QVERIFY(rejected(invalid));
        invalid = windowsDefaults();
        invalid.insert(QStringLiteral("hyprland.group.groupbar.height"), 0);
        QVERIFY(rejected(invalid));
        invalid = windowsDefaults();
        invalid.insert(QStringLiteral("hyprland.group.groupbar.height"), 65);
        QVERIFY(rejected(invalid));

        const QStringList fontWeightIds{
            QStringLiteral("hyprland.group.groupbar.font_weight_active"),
            QStringLiteral("hyprland.group.groupbar.font_weight_inactive"),
        };
        for (const auto &id : fontWeightIds) {
            for (const auto &value : QList<QVariant>{
                     QVariant::fromValue(400.5),
                     QVariant::fromValue(QStringLiteral("400")),
                     QVariant::fromValue(true),
                     QVariant::fromValue(
                         std::numeric_limits<double>::quiet_NaN()
                     ),
                     QVariant::fromValue(
                         std::numeric_limits<double>::infinity()
                     ),
                     QVariant::fromValue(-1),
                     QVariant::fromValue(2147483648LL),
                 }) {
                invalid = windowsDefaults();
                invalid.insert(id, value);
                QVERIFY2(rejected(invalid), qPrintable(id));
            }
        }

        const QStringList numberIds{
            QStringLiteral("hyprland.group.groupbar.rounding_power"),
            QStringLiteral(
                "hyprland.group.groupbar.gradient_rounding_power"
            ),
        };
        for (const auto &id : numberIds) {
            for (const auto &value : QList<QVariant>{
                     QVariant::fromValue(QStringLiteral("2.573")),
                     QVariant::fromValue(
                         std::numeric_limits<double>::quiet_NaN()
                     ),
                     QVariant::fromValue(
                         std::numeric_limits<double>::infinity()
                     ),
                     QVariant::fromValue(1.999),
                     QVariant::fromValue(10.001),
                 }) {
                invalid = windowsDefaults();
                invalid.insert(id, value);
                QVERIFY2(rejected(invalid), qPrintable(id));
            }
        }

        invalid = windowsDefaults();
        invalid.insert(
            QStringLiteral("hyprland.group.groupbar.font_family"), 1
        );
        QVERIFY(rejected(invalid));
        invalid = windowsDefaults();
        invalid.insert(
            QStringLiteral("hyprland.group.groupbar.font_family"),
            QString(4097, QLatin1Char('F'))
        );
        QVERIFY(rejected(invalid));
        auto nulFamily = QStringLiteral("Fira");
        nulFamily.append(QChar::Null);
        nulFamily.append(QStringLiteral("Sans"));
        invalid = windowsDefaults();
        invalid.insert(
            QStringLiteral("hyprland.group.groupbar.font_family"),
            nulFamily
        );
        QVERIFY(rejected(invalid));
    }

    void rejectsInvalidWindowsMapsAndStaleOrMalformedAuthority()
    {
        const auto catalog = trustedCatalog();
        auto snapshot = baselineSnapshot();
        const auto actionDigest = snapshot.value(
            QStringLiteral("actionCatalogDigest")
        ).toString();
        QString error;
        const auto rejected = [&](const QVariantMap &values) {
            return !HyprShelld::CompositorSnapshotEditor::replaceWindows(
                snapshot,
                7,
                catalog.digest(),
                actionDigest,
                catalog, trustedActionCatalog(),
                values,
                error
            );
        };

        auto invalid = windowsDefaults();
        invalid.remove(QStringLiteral("hyprland.general.layout"));
        QVERIFY(rejected(invalid));

        invalid = windowsDefaults();
        invalid.insert(QStringLiteral("hyprland.windows.unknown"), true);
        QVERIFY(rejected(invalid));

        invalid = windowsDefaults();
        invalid.remove(
            QStringLiteral("hyprland.input.follow_mouse_threshold")
        );
        QVERIFY(rejected(invalid));

        invalid = windowsDefaults();
        invalid.insert(QStringLiteral("hyprland.general.layout"), 1);
        QVERIFY(rejected(invalid));
        invalid = windowsDefaults();
        invalid.insert(
            QStringLiteral("hyprland.general.layout"),
            QStringLiteral("lua:unreviewed")
        );
        QVERIFY(rejected(invalid));

        invalid = windowsDefaults();
        invalid.insert(
            QStringLiteral("hyprland.general.resize_on_border"), 1
        );
        QVERIFY(rejected(invalid));
        invalid = windowsDefaults();
        invalid.insert(
            QStringLiteral("hyprland.general.extend_border_grab_area"), 2.5
        );
        QVERIFY(rejected(invalid));
        invalid = windowsDefaults();
        invalid.insert(
            QStringLiteral("hyprland.general.extend_border_grab_area"),
            QStringLiteral("15")
        );
        QVERIFY(rejected(invalid));
        invalid = windowsDefaults();
        invalid.insert(
            QStringLiteral("hyprland.general.extend_border_grab_area"), -1
        );
        QVERIFY(rejected(invalid));
        invalid = windowsDefaults();
        invalid.insert(
            QStringLiteral("hyprland.general.extend_border_grab_area"), 101
        );
        QVERIFY(rejected(invalid));

        const QList<QVariant> invalidCssGaps{
            QVariant::fromValue(QVariantList{0, 0, 0}),
            QVariant::fromValue(QVariantList{0, 0, 0, 0.5}),
            QVariant::fromValue(QVariantList{0, 0, 0, QStringLiteral("1")}),
            QVariant::fromValue(QVariantList{0, 0, 0, true}),
            QVariant::fromValue(QVariantList{
                0,
                0,
                0,
                std::numeric_limits<double>::quiet_NaN(),
            }),
            QVariant::fromValue(QVariantList{
                0,
                0,
                0,
                9007199254740992.0,
            }),
        };
        for (const auto &value : invalidCssGaps) {
            invalid = windowsDefaults();
            invalid.insert(QStringLiteral("hyprland.general.float_gaps"), value);
            QVERIFY(rejected(invalid));
        }

        const QList<QVariant> invalidVectors{
            QVariant::fromValue(QVariantList{1.0}),
            QVariant::fromValue(QVariantList{1.0, QStringLiteral("2")}),
            QVariant::fromValue(QVariantList{1.0, true}),
            QVariant::fromValue(QVariantList{
                1.0, std::numeric_limits<double>::quiet_NaN(),
            }),
            QVariant::fromValue(QVariantList{
                1.0, std::numeric_limits<double>::infinity(),
            }),
            QVariant::fromValue(QVariantList{-0.01, 1.0}),
            QVariant::fromValue(QVariantList{1.0, 1000.01}),
        };
        for (const auto &value : invalidVectors) {
            invalid = windowsDefaults();
            invalid.insert(
                QStringLiteral(
                    "hyprland.layout.single_window_aspect_ratio"
                ),
                value
            );
            QVERIFY(rejected(invalid));
        }

        const QStringList newNumberIds{
            QStringLiteral(
                "hyprland.layout.single_window_aspect_ratio_tolerance"
            ),
            QStringLiteral("hyprland.dwindle.default_split_ratio"),
            QStringLiteral("hyprland.master.mfact"),
            QStringLiteral("hyprland.scrolling.follow_min_visible"),
        };
        for (const auto &id : newNumberIds) {
            invalid = windowsDefaults();
            invalid.insert(id, std::numeric_limits<double>::quiet_NaN());
            QVERIFY2(rejected(invalid), qPrintable(id));
            invalid = windowsDefaults();
            invalid.insert(id, std::numeric_limits<double>::infinity());
            QVERIFY2(rejected(invalid), qPrintable(id));
            invalid = windowsDefaults();
            invalid.insert(id, QStringLiteral("0.5"));
            QVERIFY2(rejected(invalid), qPrintable(id));
        }

        for (const auto &value : QList<QVariant>{
                 QVariant::fromValue(true),
                 QVariant::fromValue(QStringLiteral("123456.7890625")),
                 QVariant::fromValue(
                     std::numeric_limits<double>::quiet_NaN()
                 ),
                 QVariant::fromValue(
                     std::numeric_limits<double>::infinity()
                 ),
                 QVariant::fromValue(-0.0000001),
                 QVariant::fromValue(1000000.0000001),
             }) {
            invalid = windowsDefaults();
            invalid.insert(
                QStringLiteral("hyprland.input.follow_mouse_threshold"),
                value
            );
            QVERIFY(rejected(invalid));
        }

        const QStringList numericEnumIds{
            QStringLiteral("hyprland.general.resize_corner"),
            QStringLiteral("hyprland.input.follow_mouse"),
            QStringLiteral("hyprland.input.float_switch_override_focus"),
            QStringLiteral("hyprland.input.focus_on_close"),
            QStringLiteral("hyprland.dwindle.force_split"),
            QStringLiteral("hyprland.dwindle.split_bias"),
            QStringLiteral("hyprland.scrolling.focus_fit_method"),
            QStringLiteral("hyprland.group.drag_into_group"),
            QStringLiteral("hyprland.binds.focus_preferred_method"),
            QStringLiteral("hyprland.misc.on_focus_under_fullscreen"),
        };
        for (const auto &id : numericEnumIds) {
            invalid = windowsDefaults();
            invalid.insert(id, QStringLiteral("1"));
            QVERIFY2(rejected(invalid), qPrintable(id));
            invalid = windowsDefaults();
            invalid.insert(id, true);
            QVERIFY2(rejected(invalid), qPrintable(id));
            invalid = windowsDefaults();
            invalid.insert(id, 1.5);
            QVERIFY2(rejected(invalid), qPrintable(id));
            invalid = windowsDefaults();
            invalid.insert(id, std::numeric_limits<double>::quiet_NaN());
            QVERIFY2(rejected(invalid), qPrintable(id));
            invalid = windowsDefaults();
            invalid.insert(id, std::numeric_limits<double>::infinity());
            QVERIFY2(rejected(invalid), qPrintable(id));
        }

        const QList<std::pair<QString, int>> outOfRangeEnums{
            {QStringLiteral("hyprland.general.resize_corner"), 5},
            {QStringLiteral("hyprland.input.follow_mouse"), 4},
            {
                QStringLiteral(
                    "hyprland.input.float_switch_override_focus"
                ),
                3,
            },
            {QStringLiteral("hyprland.input.focus_on_close"), 3},
            {QStringLiteral("hyprland.dwindle.force_split"), 3},
            {QStringLiteral("hyprland.dwindle.split_bias"), 2},
            {QStringLiteral("hyprland.scrolling.focus_fit_method"), 2},
            {QStringLiteral("hyprland.group.drag_into_group"), 3},
            {QStringLiteral("hyprland.binds.focus_preferred_method"), 2},
            {QStringLiteral("hyprland.misc.on_focus_under_fullscreen"), 3},
        };
        for (const auto &[id, value] : outOfRangeEnums) {
            invalid = windowsDefaults();
            invalid.insert(id, value);
            QVERIFY2(rejected(invalid), qPrintable(id));
        }

        invalid = windowsDefaults();
        invalid.insert(
            QStringLiteral("hyprland.input.follow_mouse_shrink"), 301
        );
        QVERIFY(rejected(invalid));
        invalid = windowsDefaults();
        invalid.insert(
            QStringLiteral("hyprland.general.snap.monitor_gap"), -1
        );
        QVERIFY(rejected(invalid));
        invalid = windowsDefaults();
        invalid.insert(
            QStringLiteral("hyprland.general.snap.window_gap"), 101
        );
        QVERIFY(rejected(invalid));
        invalid = windowsDefaults();
        invalid.insert(
            QStringLiteral("hyprland.dwindle.default_split_ratio"), 0.09
        );
        QVERIFY(rejected(invalid));
        invalid = windowsDefaults();
        invalid.insert(QStringLiteral("hyprland.master.mfact"), 1.01);
        QVERIFY(rejected(invalid));
        invalid = windowsDefaults();
        invalid.insert(
            QStringLiteral("hyprland.master.orientation"),
            QStringLiteral("diagonal")
        );
        QVERIFY(rejected(invalid));

        const QStringList windowsBooleanIds{
            QStringLiteral("hyprland.group.auto_group"),
            QStringLiteral("hyprland.group.insert_after_current"),
            QStringLiteral("hyprland.group.focus_removed_window"),
            QStringLiteral("hyprland.group.merge_groups_on_drag"),
            QStringLiteral("hyprland.group.merge_groups_on_groupbar"),
            QStringLiteral(
                "hyprland.group.merge_floated_into_tiled_on_groupbar"
            ),
            QStringLiteral("hyprland.group.group_on_movetoworkspace"),
            QStringLiteral("hyprland.binds.allow_pin_fullscreen"),
            QStringLiteral("hyprland.binds.ignore_group_lock"),
            QStringLiteral("hyprland.binds.movefocus_cycles_fullscreen"),
            QStringLiteral("hyprland.binds.movefocus_cycles_groupfirst"),
            QStringLiteral(
                "hyprland.binds.window_direction_monitor_fallback"
            ),
            QStringLiteral("hyprland.misc.size_limits_tiled"),
            QStringLiteral("hyprland.misc.always_follow_on_dnd"),
            QStringLiteral("hyprland.misc.focus_on_activate"),
            QStringLiteral("hyprland.misc.mouse_move_focuses_monitor"),
            QStringLiteral(
                "hyprland.misc.exit_window_retains_fullscreen"
            ),
            QStringLiteral("hyprland.misc.enable_swallow"),
        };
        for (const auto &id : windowsBooleanIds) {
            invalid = windowsDefaults();
            invalid.insert(id, 1);
            QVERIFY2(rejected(invalid), qPrintable(id));
            invalid = windowsDefaults();
            invalid.insert(id, QStringLiteral("true"));
            QVERIFY2(rejected(invalid), qPrintable(id));
        }

        const QStringList swallowPatternIds{
            QStringLiteral("hyprland.misc.swallow_regex"),
            QStringLiteral("hyprland.misc.swallow_exception_regex"),
        };
        for (const auto &id : swallowPatternIds) {
            invalid = windowsDefaults();
            invalid.insert(id, 1);
            QVERIFY2(rejected(invalid), qPrintable(id));
            invalid = windowsDefaults();
            invalid.insert(id, QString(4097, QLatin1Char('a')));
            QVERIFY2(rejected(invalid), qPrintable(id));
            auto nulPattern = QStringLiteral("prefix");
            nulPattern.append(QChar::Null);
            nulPattern.append(QStringLiteral("suffix"));
            invalid = windowsDefaults();
            invalid.insert(id, nulPattern);
            QVERIFY2(rejected(invalid), qPrintable(id));
            invalid = windowsDefaults();
            invalid.insert(id, QStringLiteral("("));
            QVERIFY2(rejected(invalid), qPrintable(id));
        }

        QVERIFY(!HyprShelld::CompositorSnapshotEditor::replaceWindows(
            snapshot, 6, catalog.digest(), actionDigest,
            catalog, trustedActionCatalog(), windowsDefaults(), error
        ));
        QVERIFY(!HyprShelld::CompositorSnapshotEditor::replaceWindows(
            snapshot, 7, QString(64, QLatin1Char('a')), actionDigest,
            catalog, trustedActionCatalog(), windowsDefaults(), error
        ));
        QVERIFY(!HyprShelld::CompositorSnapshotEditor::replaceWindows(
            snapshot, 7, catalog.digest(), QString(64, QLatin1Char('a')),
            catalog, trustedActionCatalog(), windowsDefaults(), error
        ));

        snapshot.insert(QStringLiteral("revision"), QStringLiteral("07"));
        QVERIFY(!HyprShelld::CompositorSnapshotEditor::replaceWindows(
            snapshot, 7, catalog.digest(), actionDigest,
            catalog, trustedActionCatalog(), windowsDefaults(), error
        ));
        snapshot.insert(QStringLiteral("revision"), QString::number(
            std::numeric_limits<qulonglong>::max()
        ));
        QVERIFY(!HyprShelld::CompositorSnapshotEditor::replaceWindows(
            snapshot, std::numeric_limits<qulonglong>::max(),
            catalog.digest(), actionDigest,
            catalog, trustedActionCatalog(), windowsDefaults(), error
        ));
        snapshot = baselineSnapshot();
        snapshot.remove(QStringLiteral("devices"));
        QVERIFY(!HyprShelld::CompositorSnapshotEditor::replaceWindows(
            snapshot, 7, catalog.digest(), actionDigest,
            catalog, trustedActionCatalog(), windowsDefaults(), error
        ));
    }

    void validatesWorkspacesBoundariesTypesAndAuthority()
    {
        const auto catalog = trustedCatalog();
        auto snapshot = baselineSnapshot();
        const auto actionDigest = snapshot.value(
            QStringLiteral("actionCatalogDigest")
        ).toString();
        QString error;
        const auto replace = [&](const QVariantMap &values) {
            return HyprShelld::CompositorSnapshotEditor::replaceWorkspaces(
                snapshot,
                7,
                catalog.digest(),
                actionDigest,
                catalog, trustedActionCatalog(),
                values,
                userWorkspaceRules(snapshot),
                error
            );
        };

        auto minimums = workspacesDefaults();
        minimums.insert(
            QStringLiteral("hyprland.gestures.workspace_swipe_cancel_ratio"),
            0.0
        );
        minimums.insert(
            QStringLiteral(
                "hyprland.gestures.workspace_swipe_direction_lock_threshold"
            ),
            0
        );
        minimums.insert(
            QStringLiteral("hyprland.gestures.workspace_swipe_distance"), 0
        );
        minimums.insert(
            QStringLiteral(
                "hyprland.gestures.workspace_swipe_min_speed_to_force"
            ),
            0
        );
        minimums.insert(
            QStringLiteral("hyprland.misc.initial_workspace_tracking"), 0.0
        );
        minimums.insert(
            QStringLiteral("hyprland.misc.initial_workspace_token_timeout"),
            1
        );
        minimums.insert(
            QStringLiteral("hyprland.binds.workspace_center_on"), 0.0
        );
        minimums.insert(
            QStringLiteral("hyprland.cursor.warp_on_change_workspace"), 0.0
        );
        minimums.insert(
            QStringLiteral("hyprland.cursor.warp_on_toggle_special"), 0.0
        );
        QVERIFY2(replace(minimums), qPrintable(error));

        auto maximums = workspacesDefaults();
        maximums.insert(
            QStringLiteral("hyprland.gestures.workspace_swipe_cancel_ratio"),
            1.0
        );
        maximums.insert(
            QStringLiteral(
                "hyprland.gestures.workspace_swipe_direction_lock_threshold"
            ),
            200
        );
        maximums.insert(
            QStringLiteral("hyprland.gestures.workspace_swipe_distance"),
            2000
        );
        maximums.insert(
            QStringLiteral(
                "hyprland.gestures.workspace_swipe_min_speed_to_force"
            ),
            200
        );
        maximums.insert(
            QStringLiteral("hyprland.misc.initial_workspace_tracking"), 2.0
        );
        maximums.insert(
            QStringLiteral("hyprland.misc.initial_workspace_token_timeout"),
            3600
        );
        maximums.insert(
            QStringLiteral("hyprland.binds.workspace_center_on"), 1.0
        );
        maximums.insert(
            QStringLiteral("hyprland.cursor.warp_on_change_workspace"), 2.0
        );
        maximums.insert(
            QStringLiteral("hyprland.cursor.warp_on_toggle_special"), 2.0
        );
        const auto maximumEdit = replace(maximums);
        QVERIFY2(maximumEdit, qPrintable(error));
        const auto maximumCandidate = QJsonDocument::fromJson(
            maximumEdit->candidate
        ).object();
        const auto projected = catalog.workspacesValues(
            maximumCandidate, error
        );
        QVERIFY2(projected, qPrintable(error));
        QCOMPARE(*projected, maximums);

        const auto rejected = [&](const QVariantMap &values) {
            return !replace(values);
        };
        auto invalid = workspacesDefaults();
        invalid.remove(
            QStringLiteral("hyprland.animations.workspace_wraparound")
        );
        QVERIFY(rejected(invalid));
        invalid = workspacesDefaults();
        invalid.insert(QStringLiteral("hyprland.workspaces.unknown"), true);
        QVERIFY(rejected(invalid));
        invalid = workspacesDefaults();
        invalid.insert(
            QStringLiteral("hyprland.animations.workspace_wraparound"), 1
        );
        QVERIFY(rejected(invalid));
        invalid = workspacesDefaults();
        invalid.insert(
            QStringLiteral("hyprland.binds.allow_workspace_cycles"), 1
        );
        QVERIFY(rejected(invalid));

        const QStringList integerIds{
            QStringLiteral(
                "hyprland.gestures.workspace_swipe_direction_lock_threshold"
            ),
            QStringLiteral("hyprland.gestures.workspace_swipe_distance"),
            QStringLiteral(
                "hyprland.gestures.workspace_swipe_min_speed_to_force"
            ),
            QStringLiteral("hyprland.misc.initial_workspace_token_timeout"),
        };
        for (const auto &id : integerIds) {
            invalid = workspacesDefaults();
            invalid.insert(id, 10.5);
            QVERIFY2(rejected(invalid), qPrintable(id));
            invalid = workspacesDefaults();
            invalid.insert(id, QStringLiteral("10"));
            QVERIFY2(rejected(invalid), qPrintable(id));
            invalid = workspacesDefaults();
            invalid.insert(id, std::numeric_limits<double>::infinity());
            QVERIFY2(rejected(invalid), qPrintable(id));
        }
        invalid = workspacesDefaults();
        invalid.insert(
            QStringLiteral("hyprland.gestures.workspace_swipe_distance"),
            2001
        );
        QVERIFY(rejected(invalid));

        invalid = workspacesDefaults();
        invalid.insert(
            QStringLiteral("hyprland.gestures.workspace_swipe_cancel_ratio"),
            std::numeric_limits<double>::quiet_NaN()
        );
        QVERIFY(rejected(invalid));
        invalid = workspacesDefaults();
        invalid.insert(
            QStringLiteral("hyprland.gestures.workspace_swipe_cancel_ratio"),
            -0.01
        );
        QVERIFY(rejected(invalid));
        invalid = workspacesDefaults();
        invalid.insert(
            QStringLiteral("hyprland.gestures.workspace_swipe_cancel_ratio"),
            1.01
        );
        QVERIFY(rejected(invalid));

        for (const auto &value : QVariantList{
                 QStringLiteral("1"), true, 1.5, 3,
                 std::numeric_limits<double>::quiet_NaN(),
             }) {
            invalid = workspacesDefaults();
            invalid.insert(
                QStringLiteral("hyprland.misc.initial_workspace_tracking"),
                value
            );
            QVERIFY(rejected(invalid));
        }

        for (const auto &value : QVariantList{
                 QStringLiteral("1"), true, 0.5, -1, 2,
                 std::numeric_limits<double>::quiet_NaN(),
             }) {
            invalid = workspacesDefaults();
            invalid.insert(
                QStringLiteral("hyprland.binds.workspace_center_on"), value
            );
            QVERIFY(rejected(invalid));
        }
        for (const auto &id : {
                 QStringLiteral("hyprland.cursor.warp_on_change_workspace"),
                 QStringLiteral("hyprland.cursor.warp_on_toggle_special"),
             }) {
            for (const auto &value : QVariantList{
                     QStringLiteral("1"), true, 0.5, -1, 3,
                     std::numeric_limits<double>::quiet_NaN(),
                 }) {
                invalid = workspacesDefaults();
                invalid.insert(id, value);
                QVERIFY2(rejected(invalid), qPrintable(id));
            }
        }

        QVERIFY(!HyprShelld::CompositorSnapshotEditor::replaceWorkspaces(
            snapshot, 6, catalog.digest(), actionDigest,
            catalog, trustedActionCatalog(), workspacesDefaults(),
            userWorkspaceRules(snapshot), error
        ));
        QVERIFY(!HyprShelld::CompositorSnapshotEditor::replaceWorkspaces(
            snapshot, 7, QString(64, QLatin1Char('a')), actionDigest,
            catalog, trustedActionCatalog(), workspacesDefaults(),
            userWorkspaceRules(snapshot), error
        ));
        QVERIFY(!HyprShelld::CompositorSnapshotEditor::replaceWorkspaces(
            snapshot, 7, catalog.digest(), QString(64, QLatin1Char('a')),
            catalog, trustedActionCatalog(), workspacesDefaults(),
            userWorkspaceRules(snapshot), error
        ));
        snapshot.insert(QStringLiteral("revision"), QStringLiteral("07"));
        QVERIFY(!HyprShelld::CompositorSnapshotEditor::replaceWorkspaces(
            snapshot, 7, catalog.digest(), actionDigest,
            catalog, trustedActionCatalog(), workspacesDefaults(),
            userWorkspaceRules(snapshot), error
        ));
        snapshot.insert(QStringLiteral("revision"), QString::number(
            std::numeric_limits<qulonglong>::max()
        ));
        QVERIFY(!HyprShelld::CompositorSnapshotEditor::replaceWorkspaces(
            snapshot, std::numeric_limits<qulonglong>::max(),
            catalog.digest(), actionDigest,
            catalog, trustedActionCatalog(), workspacesDefaults(),
            userWorkspaceRules(snapshot), error
        ));
        snapshot = baselineSnapshot();
        snapshot.remove(QStringLiteral("workspaceRules"));
        QVERIFY(!HyprShelld::CompositorSnapshotEditor::replaceWorkspaces(
            snapshot, 7, catalog.digest(), actionDigest,
            catalog, trustedActionCatalog(), workspacesDefaults(), {}, error
        ));
    }

    void rejectsInvalidInputMapsAndStaleOrMalformedAuthority()
    {
        const auto catalog = trustedCatalog();
        auto snapshot = baselineSnapshot();
        const auto actionDigest = snapshot.value(
            QStringLiteral("actionCatalogDigest")
        ).toString();
        QString error;
        const auto rejected = [&](const QVariantMap &values) {
            return !HyprShelld::CompositorSnapshotEditor::replaceInput(
                snapshot,
                7,
                catalog.digest(),
                actionDigest,
                catalog, trustedActionCatalog(),
                values,
                snapshotGestures(snapshot), error
            );
        };

        auto invalid = inputDefaults();
        invalid.remove(QStringLiteral("hyprland.input.repeat_rate"));
        QVERIFY(rejected(invalid));

        invalid = inputDefaults();
        invalid.remove(
            QStringLiteral("hyprland.input.touchdevice.enabled")
        );
        QVERIFY(rejected(invalid));

        invalid = inputDefaults();
        invalid.remove(QStringLiteral("hyprland.cursor.hide_on_key_press"));
        QVERIFY(rejected(invalid));

        invalid = inputDefaults();
        invalid.insert(QStringLiteral("hyprland.input.unknown"), true);
        QVERIFY(rejected(invalid));

        invalid = inputDefaults();
        invalid.insert(QStringLiteral("hyprland.input.repeat_rate"), 25.5);
        QVERIFY(rejected(invalid));
        invalid.insert(
            QStringLiteral("hyprland.input.repeat_rate"),
            QStringLiteral("25")
        );
        QVERIFY(rejected(invalid));
        invalid.insert(QStringLiteral("hyprland.input.repeat_rate"), false);
        QVERIFY(rejected(invalid));

        invalid = inputDefaults();
        invalid.insert(QStringLiteral("hyprland.input.repeat_rate"), -1);
        QVERIFY(rejected(invalid));
        invalid.insert(QStringLiteral("hyprland.input.repeat_rate"), 201);
        QVERIFY(rejected(invalid));
        invalid = inputDefaults();
        invalid.insert(QStringLiteral("hyprland.input.repeat_delay"), 2001);
        QVERIFY(rejected(invalid));

        invalid = inputDefaults();
        invalid.insert(QStringLiteral("hyprland.input.rotation"), -1);
        QVERIFY(rejected(invalid));
        invalid.insert(QStringLiteral("hyprland.input.rotation"), 360);
        QVERIFY(rejected(invalid));
        invalid.insert(QStringLiteral("hyprland.input.rotation"), 137.5);
        QVERIFY(rejected(invalid));
        invalid.insert(
            QStringLiteral("hyprland.input.rotation"),
            QStringLiteral("137")
        );
        QVERIFY(rejected(invalid));
        invalid.insert(QStringLiteral("hyprland.input.rotation"), true);
        QVERIFY(rejected(invalid));
        invalid.insert(
            QStringLiteral("hyprland.input.rotation"),
            std::numeric_limits<double>::quiet_NaN()
        );
        QVERIFY(rejected(invalid));
        invalid.insert(
            QStringLiteral("hyprland.input.rotation"),
            std::numeric_limits<double>::infinity()
        );
        QVERIFY(rejected(invalid));
        invalid.insert(
            QStringLiteral("hyprland.input.rotation"),
            -std::numeric_limits<double>::infinity()
        );
        QVERIFY(rejected(invalid));

        for (const auto &id : {
                 QStringLiteral("hyprland.input.touchdevice.transform"),
                 QStringLiteral("hyprland.input.tablet.transform"),
             }) {
            for (const auto &value : QVariantList{
                     -1,
                     7,
                     3.5,
                     QStringLiteral("3"),
                     true,
                     std::numeric_limits<double>::quiet_NaN(),
                     std::numeric_limits<double>::infinity(),
                     -std::numeric_limits<double>::infinity(),
                 }) {
                invalid = inputDefaults();
                invalid.insert(id, value);
                QVERIFY2(rejected(invalid), qPrintable(id));
            }
        }

        const auto rejectVector = [&](const QString &id,
                                      const QVariant &value) {
            invalid = inputDefaults();
            invalid.insert(id, value);
            QVERIFY2(rejected(invalid), qPrintable(id));
        };
        const auto positionId = QStringLiteral(
            "hyprland.input.tablet.region_position"
        );
        const auto sizeId = QStringLiteral(
            "hyprland.input.tablet.region_size"
        );
        for (const auto &value : QVariantList{
                 true,
                 QStringLiteral("0 0"),
                 QVariantList{},
                 QVariantList{0.0},
                 QVariantList{0.0, 0.0, 0.0},
                 QVariantList{QStringLiteral("0"), 0.0},
                 QVariantList{
                     std::numeric_limits<double>::quiet_NaN(), 0.0
                 },
                 QVariantList{
                     0.0, std::numeric_limits<double>::infinity()
                 },
             }) {
            rejectVector(positionId, value);
            rejectVector(sizeId, value);
        }
        rejectVector(positionId, QVariantList{-20000.000001, 0.0});
        rejectVector(positionId, QVariantList{0.0, 20000.000001});
        rejectVector(sizeId, QVariantList{-100.000001, 0.0});
        rejectVector(sizeId, QVariantList{0.0, 4000.000001});

        invalid = inputDefaults();
        invalid.insert(
            QStringLiteral("hyprland.input.sensitivity"),
            QStringLiteral("0.5")
        );
        QVERIFY(rejected(invalid));
        invalid.insert(
            QStringLiteral("hyprland.input.sensitivity"),
            std::numeric_limits<double>::quiet_NaN()
        );
        QVERIFY(rejected(invalid));
        invalid.insert(
            QStringLiteral("hyprland.input.sensitivity"),
            std::numeric_limits<double>::infinity()
        );
        QVERIFY(rejected(invalid));
        invalid.insert(
            QStringLiteral("hyprland.input.sensitivity"),
            -std::numeric_limits<double>::infinity()
        );
        QVERIFY(rejected(invalid));
        invalid.insert(QStringLiteral("hyprland.input.sensitivity"), 1.01);
        QVERIFY(rejected(invalid));
        invalid.insert(QStringLiteral("hyprland.input.sensitivity"), -1.01);
        QVERIFY(rejected(invalid));

        invalid = inputDefaults();
        invalid.insert(
            QStringLiteral("hyprland.input.accel_profile"),
            QStringLiteral("automatic")
        );
        QVERIFY(rejected(invalid));
        invalid.insert(QStringLiteral("hyprland.input.accel_profile"), 1);
        QVERIFY(rejected(invalid));

        invalid = inputDefaults();
        invalid.insert(QStringLiteral("hyprland.input.natural_scroll"), 1);
        QVERIFY(rejected(invalid));

        invalid = inputDefaults();
        invalid.insert(QStringLiteral("hyprland.input.scroll_factor"), -0.01);
        QVERIFY(rejected(invalid));
        invalid.insert(QStringLiteral("hyprland.input.scroll_factor"), 2.01);
        QVERIFY(rejected(invalid));
        invalid = inputDefaults();
        invalid.insert(
            QStringLiteral("hyprland.input.touchpad.scroll_factor"),
            2.01
        );
        QVERIFY(rejected(invalid));

        invalid = inputDefaults();
        invalid.insert(
            QStringLiteral("hyprland.input.scroll_method"),
            QStringLiteral("button")
        );
        QVERIFY(rejected(invalid));
        invalid.insert(QStringLiteral("hyprland.input.scroll_method"), 0);
        QVERIFY(rejected(invalid));

        invalid = inputDefaults();
        invalid.insert(QStringLiteral("hyprland.input.scroll_button"), -1);
        QVERIFY(rejected(invalid));
        invalid.insert(QStringLiteral("hyprland.input.scroll_button"), 301);
        QVERIFY(rejected(invalid));
        invalid.insert(QStringLiteral("hyprland.input.scroll_button"), 1.5);
        QVERIFY(rejected(invalid));
        invalid.insert(
            QStringLiteral("hyprland.input.scroll_button"),
            QStringLiteral("274")
        );
        QVERIFY(rejected(invalid));
        invalid.insert(QStringLiteral("hyprland.input.scroll_button"), true);
        QVERIFY(rejected(invalid));

        invalid = inputDefaults();
        invalid.insert(QStringLiteral("hyprland.input.scroll_button_lock"), 1);
        QVERIFY(rejected(invalid));

        const QStringList numericEnumIds{
            QStringLiteral("hyprland.input.off_window_axis_events"),
            QStringLiteral("hyprland.input.emulate_discrete_scroll"),
            QStringLiteral("hyprland.input.touchpad.drag_3fg"),
            QStringLiteral("hyprland.input.touchpad.drag_lock"),
            QStringLiteral("hyprland.input.virtualkeyboard.share_states"),
        };
        for (const auto &id : numericEnumIds) {
            invalid = inputDefaults();
            invalid.insert(id, -1);
            QVERIFY2(rejected(invalid), qPrintable(id));
            invalid.insert(id, 1.5);
            QVERIFY2(rejected(invalid), qPrintable(id));
            invalid.insert(id, QStringLiteral("1"));
            QVERIFY2(rejected(invalid), qPrintable(id));
            invalid.insert(id, true);
            QVERIFY2(rejected(invalid), qPrintable(id));
            invalid.insert(id, std::numeric_limits<double>::quiet_NaN());
            QVERIFY2(rejected(invalid), qPrintable(id));
            invalid.insert(id, std::numeric_limits<double>::infinity());
            QVERIFY2(rejected(invalid), qPrintable(id));
            invalid.insert(id, -std::numeric_limits<double>::infinity());
            QVERIFY2(rejected(invalid), qPrintable(id));
        }
        invalid = inputDefaults();
        invalid.insert(QStringLiteral("hyprland.input.off_window_axis_events"), 4);
        QVERIFY(rejected(invalid));
        invalid = inputDefaults();
        invalid.insert(QStringLiteral("hyprland.input.emulate_discrete_scroll"), 3);
        QVERIFY(rejected(invalid));
        invalid = inputDefaults();
        invalid.insert(QStringLiteral("hyprland.input.touchpad.drag_3fg"), 3);
        QVERIFY(rejected(invalid));
        invalid = inputDefaults();
        invalid.insert(QStringLiteral("hyprland.input.touchpad.drag_lock"), 3);
        QVERIFY(rejected(invalid));
        invalid = inputDefaults();
        invalid.insert(
            QStringLiteral("hyprland.input.virtualkeyboard.share_states"), 3
        );
        QVERIFY(rejected(invalid));

        const QStringList strictBooleanIds{
            QStringLiteral("hyprland.input.touchpad.clickfinger_behavior"),
            QStringLiteral("hyprland.input.touchpad.flip_x"),
            QStringLiteral("hyprland.input.touchpad.flip_y"),
            QStringLiteral(
                "hyprland.input.touchpad.middle_button_emulation"
            ),
            QStringLiteral("hyprland.input.numlock_by_default"),
            QStringLiteral(
                "hyprland.input.virtualkeyboard.release_pressed_on_close"
            ),
            QStringLiteral("hyprland.misc.name_vk_after_proc"),
            QStringLiteral("hyprland.input.force_no_accel"),
            QStringLiteral("hyprland.misc.middle_click_paste"),
            QStringLiteral("hyprland.input.touchdevice.enabled"),
            QStringLiteral("hyprland.input.tablet.relative_input"),
            QStringLiteral("hyprland.input.tablet.left_handed"),
            QStringLiteral("hyprland.cursor.hide_on_key_press"),
            QStringLiteral("hyprland.cursor.hide_on_touch"),
            QStringLiteral("hyprland.cursor.hide_on_tablet"),
            QStringLiteral("hyprland.cursor.no_warps"),
            QStringLiteral("hyprland.cursor.persistent_warps"),
            QStringLiteral(
                "hyprland.cursor.warp_back_after_non_mouse_input"
            ),
            QStringLiteral(
                "hyprland.input.tablet.absolute_region_position"
            ),
            QStringLiteral("hyprland.input.resolve_binds_by_sym"),
        };
        for (const auto &id : strictBooleanIds) {
            for (const auto &value : QVariantList{
                     0, 1, QStringLiteral("false"), QStringLiteral("true"),
                 }) {
                invalid = inputDefaults();
                invalid.insert(id, value);
                QVERIFY2(rejected(invalid), qPrintable(id));
            }
        }

        for (const auto &value : QVariantList{
                 -0.01,
                 20.01,
                 QStringLiteral("2.37"),
                 true,
                 std::numeric_limits<double>::quiet_NaN(),
                 std::numeric_limits<double>::infinity(),
                 -std::numeric_limits<double>::infinity(),
             }) {
            invalid = inputDefaults();
            invalid.insert(
                QStringLiteral("hyprland.cursor.inactive_timeout"), value
            );
            QVERIFY(rejected(invalid));
        }

        for (const auto &value : QVariantList{
                 -1,
                 21,
                 13.5,
                 QStringLiteral("13"),
                 true,
                 std::numeric_limits<double>::quiet_NaN(),
                 std::numeric_limits<double>::infinity(),
                 -std::numeric_limits<double>::infinity(),
             }) {
            invalid = inputDefaults();
            invalid.insert(
                QStringLiteral("hyprland.cursor.hotspot_padding"), value
            );
            QVERIFY(rejected(invalid));
        }

        invalid = inputDefaults();
        invalid.insert(
            QStringLiteral("hyprland.input.touchpad.tap_button_map"),
            QStringLiteral("automatic")
        );
        QVERIFY(rejected(invalid));
        invalid.insert(
            QStringLiteral("hyprland.input.touchpad.tap_button_map"), 0
        );
        QVERIFY(rejected(invalid));

        for (const auto &value : QVariantList{
                 9,
                 2001,
                 10.5,
                 QStringLiteral("10"),
                 true,
                 std::numeric_limits<double>::quiet_NaN(),
                 std::numeric_limits<double>::infinity(),
                 -std::numeric_limits<double>::infinity(),
             }) {
            invalid = inputDefaults();
            invalid.insert(
                QStringLiteral("hyprland.gestures.close_max_timeout"), value
            );
            QVERIFY(rejected(invalid));
        }

        auto integralDoubleEnums = inputDefaults();
        integralDoubleEnums.insert(
            QStringLiteral("hyprland.input.off_window_axis_events"), 2.0
        );
        integralDoubleEnums.insert(
            QStringLiteral("hyprland.input.emulate_discrete_scroll"), 0.0
        );
        integralDoubleEnums.insert(
            QStringLiteral("hyprland.input.touchpad.drag_3fg"), 1.0
        );
        integralDoubleEnums.insert(
            QStringLiteral("hyprland.input.touchpad.drag_lock"), 2.0
        );
        integralDoubleEnums.insert(
            QStringLiteral("hyprland.input.virtualkeyboard.share_states"),
            1.0
        );
        integralDoubleEnums.insert(
            QStringLiteral("hyprland.input.rotation"), 137.0
        );
        integralDoubleEnums.insert(
            QStringLiteral("hyprland.input.touchdevice.transform"), 5.0
        );
        integralDoubleEnums.insert(
            QStringLiteral("hyprland.input.tablet.transform"), 6.0
        );
        QVERIFY(!rejected(integralDoubleEnums));

        QVERIFY(!HyprShelld::CompositorSnapshotEditor::replaceInput(
            snapshot, 6, catalog.digest(), actionDigest,
            catalog, trustedActionCatalog(), inputDefaults(),
            snapshotGestures(snapshot), error
        ));
        QVERIFY(!HyprShelld::CompositorSnapshotEditor::replaceInput(
            snapshot, 7, QString(64, QLatin1Char('a')), actionDigest,
            catalog, trustedActionCatalog(), inputDefaults(),
            snapshotGestures(snapshot), error
        ));
        QVERIFY(!HyprShelld::CompositorSnapshotEditor::replaceInput(
            snapshot, 7, catalog.digest(), QString(64, QLatin1Char('a')),
            catalog, trustedActionCatalog(), inputDefaults(),
            snapshotGestures(snapshot), error
        ));

        snapshot.insert(QStringLiteral("revision"), QStringLiteral("07"));
        QVERIFY(!HyprShelld::CompositorSnapshotEditor::replaceInput(
            snapshot, 7, catalog.digest(), actionDigest,
            catalog, trustedActionCatalog(), inputDefaults(),
            snapshotGestures(snapshot), error
        ));
        snapshot.insert(QStringLiteral("revision"), QString::number(
            std::numeric_limits<qulonglong>::max()
        ));
        QVERIFY(!HyprShelld::CompositorSnapshotEditor::replaceInput(
            snapshot, std::numeric_limits<qulonglong>::max(),
            catalog.digest(), actionDigest,
            catalog, trustedActionCatalog(), inputDefaults(),
            snapshotGestures(snapshot), error
        ));
        snapshot = baselineSnapshot();
        snapshot.remove(QStringLiteral("devices"));
        QVERIFY(!HyprShelld::CompositorSnapshotEditor::replaceInput(
            snapshot, 7, catalog.digest(), actionDigest,
            catalog, trustedActionCatalog(), inputDefaults(),
            snapshotGestures(snapshot), error
        ));

    }
};

QTEST_GUILESS_MAIN(CompositorSettingsHelpersTest)

#include "compositor_settings_helpers_test.moc"
