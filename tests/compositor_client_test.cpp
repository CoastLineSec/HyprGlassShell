#include "compositor_client.h"

#include "hyprland/catalog.h"
#include "hyprland/action_catalog.h"
#include "hyprland/default_keybindings.h"
#include "hyprland/desired_state.h"
#include "hyprland/json_support.h"

#include <QDBusConnection>
#include <QDBusContext>
#include <QDBusMessage>
#include <QCryptographicHash>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSet>
#include <QSignalSpy>
#include <QTimer>
#include <QtTest>

#include <limits>
#include <utility>

namespace {

const QString busName = QStringLiteral("org.hyprshelld.Compositor1");
const QString objectPath = QStringLiteral("/org/hyprshelld/Compositor1");
const QString interfaceName = QStringLiteral("org.hyprshelld.Compositor1");
const QString propertiesInterface = QStringLiteral(
    "org.freedesktop.DBus.Properties"
);
const QString catalogDigest = QString::fromLatin1(
    HyprShelld::Hyprland::reviewedCatalogDigest
);
const QString actionCatalogDigest = QString::fromLatin1(
    HyprShelld::Hyprland::reviewedActionCatalogDigest
);
const QString generationDigest(64, QLatin1Char('c'));
const QString previewGeneration(64, QLatin1Char('d'));
const QString topologyDigest(64, QLatin1Char('e'));
const QString confirmationToken(32, QLatin1Char('f'));
constexpr qulonglong baselineRevision = 7;
constexpr qulonglong previewRevision = 8;
constexpr qulonglong previewDeadlineMs = 4'102'444'800'000ULL;
constexpr qulonglong initialSharedBorderSourceRevision = 17;
constexpr qulonglong initialSharedSpacingSourceRevision = 23;

QByteArray readBytes(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) return {};
    return file.readAll();
}

QByteArray optionCatalogBytes()
{
    const auto document = QJsonDocument::fromJson(
        readBytes(QStringLiteral(HYPRSHELLD_HYPRLAND_CATALOG_FILE))
    );
    return document.toJson(QJsonDocument::Compact);
}

QByteArray configSchemaBytes()
{
    return readBytes(QStringLiteral(HYPRSHELLD_HYPRLAND_CONFIG_SCHEMA_FILE));
}

QByteArray actionAuthorityBytes()
{
    const auto parsed = HyprShelld::Hyprland::parseActionCatalog(
        readBytes(QStringLiteral(HYPRSHELLD_HYPRLAND_ACTION_CATALOG_FILE)),
        configSchemaBytes()
    );
    return parsed
        ? HyprShelld::Hyprland::canonicalActionCatalogJson(*parsed.value)
        : QByteArray{};
}

QString configSchemaSha256()
{
    return QString::fromLatin1(
        QCryptographicHash::hash(
            configSchemaBytes(), QCryptographicHash::Sha256
        ).toHex()
    );
}

QByteArray snapshotBytes(const qulonglong revision = baselineRevision)
{
    auto object = QJsonDocument::fromJson(
        readBytes(QStringLiteral(HYPRSHELLD_HYPRLAND_DEFAULTS_FILE))
    ).object();
    object.insert(QStringLiteral("revision"), QString::number(revision));
    object.insert(QStringLiteral("catalogDigest"), catalogDigest);
    object.insert(
        QStringLiteral("actionCatalogDigest"), actionCatalogDigest
    );
    auto bytes = QJsonDocument(object).toJson(QJsonDocument::Compact);
    bytes.append('\n');
    return bytes;
}

QByteArray snapshotWithOverride(
    const QString &id,
    const QJsonValue &value,
    const qulonglong revision = baselineRevision
)
{
    auto object = QJsonDocument::fromJson(snapshotBytes(revision)).object();
    auto overrides = object.value(QStringLiteral("overrides")).toObject();
    overrides.insert(id, value);
    object.insert(QStringLiteral("overrides"), overrides);
    auto bytes = HyprShelld::Hyprland::JsonSupport::canonicalJson(object);
    bytes.append('\n');
    return bytes;
}

QByteArray snapshotWithWorkspaceRules(
    QJsonArray rules,
    const qulonglong revision = baselineRevision
)
{
    auto object = QJsonDocument::fromJson(snapshotBytes(revision)).object();
    object.insert(QStringLiteral("workspaceRules"), std::move(rules));
    auto bytes = HyprShelld::Hyprland::JsonSupport::canonicalJson(object);
    bytes.append('\n');
    return bytes;
}

QByteArray snapshotWithAppearanceCollections(
    QJsonArray curves,
    QJsonArray animations,
    const qulonglong revision = baselineRevision
)
{
    auto object = QJsonDocument::fromJson(snapshotBytes(revision)).object();
    object.insert(QStringLiteral("curves"), std::move(curves));
    object.insert(QStringLiteral("animations"), std::move(animations));
    auto bytes = HyprShelld::Hyprland::JsonSupport::canonicalJson(object);
    bytes.append('\n');
    return bytes;
}

QByteArray snapshotWithGestures(
    QJsonArray gestures,
    const qulonglong revision = baselineRevision
)
{
    auto object = QJsonDocument::fromJson(snapshotBytes(revision)).object();
    object.insert(QStringLiteral("gestures"), std::move(gestures));
    auto bytes = HyprShelld::Hyprland::JsonSupport::canonicalJson(object);
    bytes.append('\n');
    return bytes;
}

QJsonObject gestureObject(
    const QString &id,
    const int fingers,
    const QString &direction,
    const double scale,
    QJsonObject action,
    const QJsonArray &modifiers = {},
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
        {QStringLiteral("action"), std::move(action)},
    };
}

QJsonObject savedInputDevice(
    const QString &id,
    const QString &selector,
    const QString &kind,
    const bool enabled = true,
    const QJsonObject &overrides = {}
)
{
    return {
        {QStringLiteral("id"), id},
        {QStringLiteral("selector"), selector},
        {QStringLiteral("kind"), kind},
        {QStringLiteral("enabled"), enabled},
        {QStringLiteral("overrides"), overrides},
    };
}

QByteArray snapshotWithInputDevices(
    QJsonArray devices,
    const qulonglong revision = baselineRevision
)
{
    auto object = QJsonDocument::fromJson(snapshotBytes(revision)).object();
    object.insert(QStringLiteral("devices"), std::move(devices));
    auto bytes = HyprShelld::Hyprland::JsonSupport::canonicalJson(object);
    bytes.append('\n');
    return bytes;
}

QByteArray snapshotWithCollection(
    const QString &field,
    QJsonArray records,
    const qulonglong revision = baselineRevision
)
{
    auto object = QJsonDocument::fromJson(snapshotBytes(revision)).object();
    object.insert(field, std::move(records));
    auto bytes = HyprShelld::Hyprland::JsonSupport::canonicalJson(object);
    bytes.append('\n');
    return bytes;
}

QVariantMap bindingOptions()
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

QVariantMap bindingRecord(
    const QString &id = QStringLiteral("binding-main"),
    const QString &submap = QStringLiteral("resize")
)
{
    return {
        {QStringLiteral("id"), id},
        {QStringLiteral("modifiers"), QVariantList{QStringLiteral("super")}},
        {QStringLiteral("key"), QStringLiteral("F8")},
        {QStringLiteral("actionType"), QStringLiteral("dispatcher")},
        {QStringLiteral("action"), QStringLiteral("no_op")},
        {QStringLiteral("arguments"), QVariantMap{}},
        {QStringLiteral("description"), QStringLiteral("Resize mode shortcut")},
        {QStringLiteral("enabled"), true},
        {QStringLiteral("submap"), submap},
        {QStringLiteral("options"), bindingOptions()},
    };
}

QVariantMap submapRecord()
{
    return {
        {QStringLiteral("id"), QStringLiteral("submap-resize")},
        {QStringLiteral("name"), QStringLiteral("resize")},
        {QStringLiteral("reset"), QStringLiteral("reset")},
        {QStringLiteral("enabled"), true},
    };
}

QVariantMap environmentRecord()
{
    return {
        {QStringLiteral("id"), QStringLiteral("env-terminal")},
        {QStringLiteral("name"), QStringLiteral("TERMINAL")},
        {QStringLiteral("value"), QStringLiteral("foot")},
        {QStringLiteral("scope"), QStringLiteral("hyprland")},
    };
}

QVariantMap permissionRecord()
{
    return {
        {QStringLiteral("id"), QStringLiteral("permission-grim")},
        {QStringLiteral("binary"), QStringLiteral("^/usr/bin/grim$")},
        {QStringLiteral("type"), QStringLiteral("screencopy")},
        {QStringLiteral("mode"), QStringLiteral("allow")},
    };
}

QVariantMap inputDeviceRecord()
{
    return {
        {QStringLiteral("id"), QStringLiteral("device-pointer")},
        {QStringLiteral("selector"), QStringLiteral("my-mouse")},
        {QStringLiteral("kind"), QStringLiteral("pointer")},
        {QStringLiteral("enabled"), true},
        {QStringLiteral("overrides"), QVariantMap{
            {QStringLiteral("sensitivity"), 0.25},
        }},
    };
}

QJsonObject connectedInputDevice(
    const QString &selector,
    const QString &kind,
    const QJsonValue &activeKeymap = QJsonValue(QJsonValue::Null)
)
{
    return {
        {QStringLiteral("sessionSelector"), selector},
        {QStringLiteral("observedKind"), kind},
        {QStringLiteral("activeKeymap"), activeKeymap},
    };
}

QByteArray inputDeviceInventoryBytes(
    QJsonArray records = {},
    const QChar digestCharacter = QLatin1Char('a'),
    const quint32 switches = 0,
    const quint32 tabletPads = 0,
    const quint32 tabletTools = 0
)
{
    const QJsonObject object{
        {QStringLiteral("formatVersion"), 1},
        {
            QStringLiteral("inventoryDigest"),
            QString(64, digestCharacter)
        },
        {QStringLiteral("records"), std::move(records)},
        {QStringLiteral("unaddressable"), QJsonObject{
            {QStringLiteral("switches"), static_cast<qint64>(switches)},
            {QStringLiteral("tabletPads"), static_cast<qint64>(tabletPads)},
            {QStringLiteral("tabletTools"), static_cast<qint64>(tabletTools)},
        }},
    };
    auto bytes = HyprShelld::Hyprland::JsonSupport::canonicalJson(object);
    bytes.append('\n');
    return bytes;
}

QVariantMap windowRule(
    const QString &id,
    const QString &name,
    const QString &pattern = QStringLiteral("^firefox$")
)
{
    return {
        {QStringLiteral("id"), id},
        {QStringLiteral("name"), name},
        {QStringLiteral("enabled"), true},
        {QStringLiteral("match"), QVariantMap{
            {QStringLiteral("class"), pattern},
        }},
        {QStringLiteral("effects"), QVariantMap{
            {QStringLiteral("float"), true},
            {QStringLiteral("scrolling_width"), 0.373},
            {QStringLiteral("border_size"), 9007199254740991.0},
        }},
    };
}

QVariantMap layerRule(const QString &id, const QString &name)
{
    return {
        {QStringLiteral("id"), id},
        {QStringLiteral("name"), name},
        {QStringLiteral("enabled"), false},
        {QStringLiteral("match"), QVariantMap{
            {QStringLiteral("namespace"), QStringLiteral("^panel$")},
        }},
        {QStringLiteral("effects"), QVariantMap{
            {QStringLiteral("ignore_alpha"), 0.437},
            {QStringLiteral("order"), -9007199254740991.0},
        }},
    };
}

QVariantMap workspaceRule(
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
        {QStringLiteral("isDefault"), false},
        {QStringLiteral("layout"), QStringLiteral("scrolling")},
        {QStringLiteral("overrides"), overrides},
    };
}

QJsonObject protectedWorkspaceRuleObject()
{
    const auto root = QJsonDocument::fromJson(snapshotBytes()).object();
    return root.value(QStringLiteral("workspaceRules"))
        .toArray().last().toObject();
}

QVariantMap inputDefaults()
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

QVariantMap appearanceDefaults()
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

QVariantMap windowsDefaults()
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
        {QStringLiteral("hyprland.group.groupbar.disable_when_only"), false},
        {
            QStringLiteral("hyprland.group.groupbar.font_family"),
            QString{},
        },
        {QStringLiteral("hyprland.group.groupbar.font_weight_active"), 400},
        {QStringLiteral("hyprland.group.groupbar.font_weight_inactive"), 400},
        {QStringLiteral("hyprland.group.groupbar.font_size"), 8},
        {QStringLiteral("hyprland.group.groupbar.gradients"), false},
        {QStringLiteral("hyprland.group.groupbar.height"), 14},
        {QStringLiteral("hyprland.group.groupbar.indicator_gap"), 0},
        {QStringLiteral("hyprland.group.groupbar.indicator_height"), 3},
        {QStringLiteral("hyprland.group.groupbar.stacked"), false},
        {QStringLiteral("hyprland.group.groupbar.priority"), 3},
        {QStringLiteral("hyprland.group.groupbar.render_titles"), true},
        {QStringLiteral("hyprland.group.groupbar.scrolling"), true},
        {QStringLiteral("hyprland.group.groupbar.middle_click_close"), true},
        {QStringLiteral("hyprland.group.groupbar.rounding"), 1},
        {QStringLiteral("hyprland.group.groupbar.rounding_power"), 2.0},
        {QStringLiteral("hyprland.group.groupbar.gradient_rounding"), 2},
        {
            QStringLiteral("hyprland.group.groupbar.gradient_rounding_power"),
            2.0,
        },
        {QStringLiteral("hyprland.group.groupbar.round_only_edges"), true},
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

QVariantMap workspacesDefaults()
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

QVariantMap changedWorkspacesValues()
{
    auto values = workspacesDefaults();
    values.insert(
        QStringLiteral("hyprland.animations.workspace_wraparound"), true
    );
    values.insert(
        QStringLiteral("hyprland.gestures.workspace_swipe_cancel_ratio"), 0.37
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
        QStringLiteral("hyprland.misc.initial_workspace_token_timeout"), 75
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

QVariantMap advancedDefaults()
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

QVariantMap changedAdvancedValues()
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

QJsonObject unauthoredRenderAndXWaylandOverrides()
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

QByteArray topologyBytes()
{
    auto bytes = QJsonDocument(QJsonObject{
        {QStringLiteral("formatVersion"), 1},
        {QStringLiteral("topologyDigest"), topologyDigest},
        {
            QStringLiteral("outputs"),
            QJsonArray{
                QJsonObject{
                    {QStringLiteral("selector"), QStringLiteral("DP-1")},
                    {QStringLiteral("enabled"), true},
                    {QStringLiteral("modes"), QJsonArray{}},
                },
            }
        },
    }).toJson(QJsonDocument::Compact);
    bytes.append('\n');
    return bytes;
}

QVariantList validSnapshotReply()
{
    return {
        snapshotBytes(),
        QVariant::fromValue<qulonglong>(baselineRevision),
        catalogDigest,
        actionCatalogDigest,
    };
}

QVariantList validPreviewReply()
{
    return {
        QVariant::fromValue<qulonglong>(previewRevision),
        confirmationToken,
        QVariant::fromValue<qulonglong>(previewDeadlineMs),
        previewGeneration,
    };
}

class FakeCompositor final : public QObject, protected QDBusContext {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.hyprshelld.Compositor1")
    Q_PROPERTY(bool Available READ available)
    Q_PROPERTY(bool Writable READ writable)
    Q_PROPERTY(qulonglong Revision READ revision)
    Q_PROPERTY(QString LoadState READ loadState)
    Q_PROPERTY(QString ManagementState READ managementState)
    Q_PROPERTY(QString EntrypointDigest READ entrypointDigest)
    Q_PROPERTY(QString CatalogDigest READ currentCatalogDigest)
    Q_PROPERTY(QString ActionCatalogDigest READ currentActionCatalogDigest)
    Q_PROPERTY(qulonglong AppliedRevision READ appliedRevision)
    Q_PROPERTY(QString ApplyState READ applyState)
    Q_PROPERTY(QString RequiredActivation READ requiredActivation)
    Q_PROPERTY(QString GenerationDigest READ currentGenerationDigest)
    Q_PROPERTY(QString DisplayConfirmationState READ confirmationState)
    Q_PROPERTY(qulonglong DisplayConfirmationRevision READ confirmationRevision)
    Q_PROPERTY(qulonglong DisplayConfirmationDeadlineMs READ confirmationDeadlineMs)
    Q_PROPERTY(QString DisplayConfirmationGeneration READ confirmationGeneration)
    Q_PROPERTY(QString SharedBorderSyncState READ sharedBorderSyncState)
    Q_PROPERTY(qulonglong SharedBorderSourceRevision READ sharedBorderSourceRevision)
    Q_PROPERTY(QString SharedBorderSyncError READ sharedBorderSyncError)
    Q_PROPERTY(QString SharedSpacingSyncState READ sharedSpacingSyncState)
    Q_PROPERTY(qulonglong SharedSpacingSourceRevision READ sharedSpacingSourceRevision)
    Q_PROPERTY(QString SharedSpacingSyncError READ sharedSpacingSyncError)

public:
    enum class PendingBehavior {
        Success,
        NoDisplayConfirmation,
        UnexpectedError,
    };

    explicit FakeCompositor(QDBusConnection connection)
        : connection_(std::move(connection))
    {
        reset();
    }

    [[nodiscard]] bool available() const { return true; }
    [[nodiscard]] bool writable() const { return true; }
    [[nodiscard]] qulonglong revision() const { return authorityRevision_; }
    [[nodiscard]] QString loadState() const
    {
        return QStringLiteral("normal");
    }
    [[nodiscard]] QString managementState() const
    {
        return managementState_;
    }
    [[nodiscard]] QString entrypointDigest() const
    {
        return QString(64, QLatin1Char('9'));
    }
    [[nodiscard]] QString currentCatalogDigest() const
    {
        return catalogDigest;
    }
    [[nodiscard]] QString currentActionCatalogDigest() const
    {
        return actionCatalogDigest;
    }
    [[nodiscard]] qulonglong appliedRevision() const
    {
        return appliedRevision_;
    }
    [[nodiscard]] QString applyState() const
    {
        return applyState_;
    }
    [[nodiscard]] QString requiredActivation() const
    {
        return requiredActivation_;
    }
    [[nodiscard]] QString currentGenerationDigest() const
    {
        return authorityGenerationDigest_;
    }
    [[nodiscard]] QString confirmationState() const
    {
        return confirmationState_;
    }
    [[nodiscard]] qulonglong confirmationRevision() const
    {
        return confirmationRevision_;
    }
    [[nodiscard]] qulonglong confirmationDeadlineMs() const
    {
        return confirmationDeadlineMs_;
    }
    [[nodiscard]] QString confirmationGeneration() const
    {
        return confirmationGeneration_;
    }
    [[nodiscard]] QString sharedBorderSyncState() const
    {
        return sharedBorderSyncState_;
    }
    [[nodiscard]] qulonglong sharedBorderSourceRevision() const
    {
        return sharedBorderSourceRevision_;
    }
    [[nodiscard]] QString sharedBorderSyncError() const
    {
        return sharedBorderSyncError_;
    }
    [[nodiscard]] QString sharedSpacingSyncState() const
    {
        return sharedSpacingSyncState_;
    }
    [[nodiscard]] qulonglong sharedSpacingSourceRevision() const
    {
        return sharedSpacingSourceRevision_;
    }
    [[nodiscard]] QString sharedSpacingSyncError() const
    {
        return sharedSpacingSyncError_;
    }
    [[nodiscard]] qsizetype pendingCallCount() const
    {
        return pendingCallCount_;
    }
    [[nodiscard]] qsizetype heldSnapshotCount() const
    {
        return heldSnapshots_.size();
    }
    [[nodiscard]] int snapshotCallCount() const { return snapshotCallCount_; }
    [[nodiscard]] int inputDeviceCallCount() const
    {
        return inputDeviceCallCount_;
    }
    [[nodiscard]] QByteArray currentSnapshotBytes() const
    {
        return snapshotBytes_;
    }
    [[nodiscard]] qsizetype heldPreviewCount() const
    {
        return heldPreviews_.size();
    }
    [[nodiscard]] qsizetype heldInputDeviceCount() const
    {
        return heldInputDevices_.size();
    }

    bool start()
    {
        if (running_) return true;
        running_ = connection_.registerService(busName);
        return running_;
    }

    void stop()
    {
        if (!running_) return;
        connection_.unregisterService(busName);
        running_ = false;
    }

    void reset()
    {
        running_ = false;
        managementState_ = QStringLiteral("managed");
        confirmationState_ = QStringLiteral("idle");
        confirmationRevision_ = 0;
        confirmationDeadlineMs_ = 0;
        confirmationGeneration_.clear();
        sharedBorderSyncState_ = QStringLiteral("current");
        sharedBorderSourceRevision_ = initialSharedBorderSourceRevision;
        sharedBorderSyncError_.clear();
        retrySharedBorderSyncCallCount_ = 0;
        retrySharedBorderSyncErrorName_.clear();
        retrySharedBorderSyncErrorMessage_.clear();
        sharedSpacingSyncState_ = QStringLiteral("override");
        sharedSpacingSourceRevision_ = initialSharedSpacingSourceRevision;
        sharedSpacingSyncError_.clear();
        retrySharedSpacingSyncCallCount_ = 0;
        retrySharedSpacingSyncErrorName_.clear();
        retrySharedSpacingSyncErrorMessage_.clear();
        pendingBehavior_ = PendingBehavior::NoDisplayConfirmation;
        pendingCallCount_ = 0;
        holdSnapshots_ = false;
        heldSnapshots_.clear();
        snapshotCallCount_ = 0;
        holdPreviews_ = false;
        heldPreviews_.clear();
        previewReply_ = validPreviewReply();
        projectPreviewReply_ = false;
        authorityRevision_ = baselineRevision;
        appliedRevision_ = baselineRevision;
        applyState_ = QStringLiteral("current");
        requiredActivation_ = QStringLiteral("none");
        authorityGenerationDigest_ = generationDigest;
        snapshotBytes_ = snapshotBytes(baselineRevision);
        replaceCallCount_ = 0;
        applyCallCount_ = 0;
        recoverCallCount_ = 0;
        ambiguousFirstReplace_ = false;
        failNextReplace_ = false;
        failNextApply_ = false;
        ambiguousFirstApplyWithoutProperties_ = false;
        ambiguousFirstApplyWithProperties_ = false;
        ambiguousRecover_ = false;
        holdReplaces_ = false;
        heldReplaces_.clear();
        optionCatalog_ = optionCatalogBytes();
        optionCatalogDigest_ = catalogDigest;
        actionCatalog_ = actionAuthorityBytes();
        actionCatalogReplyDigest_ = actionCatalogDigest;
        configSchema_ = configSchemaBytes();
        configSchemaDigest_ = configSchemaSha256();
        actionCatalogErrorName_.clear();
        actionCatalogErrorMessage_.clear();
        connectedDisplaysFail_ = false;
        inputDeviceInventory_ = inputDeviceInventoryBytes();
        inputDevicesObservedAtMs_ = 1'800'000'000'100ULL;
        inputDeviceErrorName_.clear();
        inputDeviceErrorMessage_.clear();
        holdInputDevices_ = false;
        heldInputDevices_.clear();
        inputDeviceCallCount_ = 0;
    }

    void setPendingBehavior(const PendingBehavior behavior)
    {
        pendingBehavior_ = behavior;
    }

    void setAwaitingConfirmation(const bool publish = false)
    {
        managementState_ = QStringLiteral("preview");
        confirmationState_ = QStringLiteral("awaiting-confirmation");
        confirmationRevision_ = previewRevision;
        confirmationDeadlineMs_ = previewDeadlineMs;
        confirmationGeneration_ = previewGeneration;
        if (publish) publishConfirmation();
    }

    void replaceAwaitingConfirmationWithForeignTuple()
    {
        confirmationDeadlineMs_ = previewDeadlineMs + 1000;
        confirmationGeneration_ = QString(64, QLatin1Char('1'));
        publishConfirmation();
    }

    void setHoldSnapshots(const bool hold)
    {
        holdSnapshots_ = hold;
    }

    void setOptionCatalogReply(QByteArray bytes, QString digest)
    {
        optionCatalog_ = std::move(bytes);
        optionCatalogDigest_ = std::move(digest);
    }

    void setActionCatalogReply(
        QByteArray actionCatalog,
        QString actionDigest,
        QByteArray configSchema,
        QString schemaDigest
    )
    {
        actionCatalog_ = std::move(actionCatalog);
        actionCatalogReplyDigest_ = std::move(actionDigest);
        configSchema_ = std::move(configSchema);
        configSchemaDigest_ = std::move(schemaDigest);
        actionCatalogErrorName_.clear();
        actionCatalogErrorMessage_.clear();
    }

    void setActionCatalogError(QString name, QString message)
    {
        actionCatalogErrorName_ = std::move(name);
        actionCatalogErrorMessage_ = std::move(message);
    }

    void setConnectedDisplaysFail(const bool fail)
    {
        connectedDisplaysFail_ = fail;
    }

    void setInputDeviceReply(QByteArray inventory, qulonglong observedAtMs)
    {
        inputDeviceInventory_ = std::move(inventory);
        inputDevicesObservedAtMs_ = observedAtMs;
        inputDeviceErrorName_.clear();
        inputDeviceErrorMessage_.clear();
    }

    void setInputDeviceError(QString name, QString message)
    {
        inputDeviceErrorName_ = std::move(name);
        inputDeviceErrorMessage_ = std::move(message);
    }

    void setHoldInputDevices(const bool hold)
    {
        holdInputDevices_ = hold;
    }

    void setRevision(const qulonglong revision)
    {
        authorityRevision_ = revision;
        appliedRevision_ = revision;
        snapshotBytes_ = snapshotBytes(revision);
    }

    void setSnapshotBytes(QByteArray bytes)
    {
        snapshotBytes_ = std::move(bytes);
    }

    void setAmbiguousFirstReplace(const bool enabled)
    {
        ambiguousFirstReplace_ = enabled;
    }

    void setFailNextReplace(const bool enabled)
    {
        failNextReplace_ = enabled;
    }

    void setFailNextApply(const bool enabled)
    {
        failNextApply_ = enabled;
    }

    void setAmbiguousFirstApplyWithoutProperties(const bool enabled)
    {
        ambiguousFirstApplyWithoutProperties_ = enabled;
    }

    void setAmbiguousFirstApplyWithProperties(const bool enabled)
    {
        ambiguousFirstApplyWithProperties_ = enabled;
    }

    void setHoldReplaces(const bool hold)
    {
        holdReplaces_ = hold;
    }

    void setAmbiguousRecover(const bool enabled)
    {
        ambiguousRecover_ = enabled;
    }

    bool setSharedBorderSync(
        QString state,
        const qulonglong sourceRevision,
        QString error,
        const bool publish = false
    )
    {
        sharedBorderSyncState_ = std::move(state);
        sharedBorderSourceRevision_ = sourceRevision;
        sharedBorderSyncError_ = std::move(error);
        return !publish || publishSharedBorderProperties({
            {QStringLiteral("SharedBorderSyncState"), sharedBorderSyncState_},
            {
                QStringLiteral("SharedBorderSourceRevision"),
                QVariant::fromValue<qulonglong>(sharedBorderSourceRevision_)
            },
            {QStringLiteral("SharedBorderSyncError"), sharedBorderSyncError_},
        });
    }

    bool publishSharedBorderProperties(const QVariantMap &changed)
    {
        auto signal = QDBusMessage::createSignal(
            objectPath,
            propertiesInterface,
            QStringLiteral("PropertiesChanged")
        );
        signal.setArguments({interfaceName, changed, QStringList{}});
        return connection_.send(signal);
    }

    void setRetrySharedBorderSyncError(QString name, QString message)
    {
        retrySharedBorderSyncErrorName_ = std::move(name);
        retrySharedBorderSyncErrorMessage_ = std::move(message);
    }

    bool setSharedSpacingSync(
        QString state,
        const qulonglong sourceRevision,
        QString error,
        const bool publish = false
    )
    {
        sharedSpacingSyncState_ = std::move(state);
        sharedSpacingSourceRevision_ = sourceRevision;
        sharedSpacingSyncError_ = std::move(error);
        return !publish || publishSharedSpacingProperties({
            {QStringLiteral("SharedSpacingSyncState"), sharedSpacingSyncState_},
            {
                QStringLiteral("SharedSpacingSourceRevision"),
                QVariant::fromValue<qulonglong>(sharedSpacingSourceRevision_)
            },
            {QStringLiteral("SharedSpacingSyncError"), sharedSpacingSyncError_},
        });
    }

    bool publishSharedSpacingProperties(const QVariantMap &changed)
    {
        auto signal = QDBusMessage::createSignal(
            objectPath,
            propertiesInterface,
            QStringLiteral("PropertiesChanged")
        );
        signal.setArguments({interfaceName, changed, QStringList{}});
        return connection_.send(signal);
    }

    void setRetrySharedSpacingSyncError(QString name, QString message)
    {
        retrySharedSpacingSyncErrorName_ = std::move(name);
        retrySharedSpacingSyncErrorMessage_ = std::move(message);
    }

    [[nodiscard]] int replaceCallCount() const { return replaceCallCount_; }
    [[nodiscard]] int applyCallCount() const { return applyCallCount_; }
    [[nodiscard]] int recoverCallCount() const { return recoverCallCount_; }
    [[nodiscard]] int retrySharedBorderSyncCallCount() const
    {
        return retrySharedBorderSyncCallCount_;
    }
    [[nodiscard]] int retrySharedSpacingSyncCallCount() const
    {
        return retrySharedSpacingSyncCallCount_;
    }
    [[nodiscard]] qsizetype heldReplaceCount() const
    {
        return heldReplaces_.size();
    }

    bool releaseNextSnapshot(
        QVariantList reply = {},
        const bool duplicate = false
    )
    {
        if (heldSnapshots_.isEmpty()) return false;
        const auto call = heldSnapshots_.takeFirst();
        if (reply.isEmpty()) reply = validSnapshotReply();
        const auto response = call.createReply(reply);
        const auto sent = connection_.send(response);
        if (duplicate) connection_.send(response);
        return sent;
    }

    bool releaseInputDevice(const qsizetype index, const bool duplicate = false)
    {
        if (index < 0 || index >= heldInputDevices_.size()) return false;
        const auto held = heldInputDevices_.takeAt(index);
        const auto response = held.errorName.isEmpty()
            ? held.call.createReply({
                  held.inventory,
                  QVariant::fromValue<qulonglong>(held.observedAtMs),
              })
            : held.call.createErrorReply(
                  held.errorName, held.errorMessage
              );
        const auto sent = connection_.send(response);
        if (duplicate) connection_.send(response);
        return sent;
    }

    void setPreviewReply(QVariantList reply, const bool project)
    {
        previewReply_ = std::move(reply);
        projectPreviewReply_ = project;
    }

    void setHoldPreviews(const bool hold)
    {
        holdPreviews_ = hold;
    }

    bool releaseNextPreview(const bool duplicate = false)
    {
        if (heldPreviews_.isEmpty()) return false;
        const auto held = heldPreviews_.takeFirst();
        if (held.project) projectPreview(held.reply);
        const auto response = held.call.createReply(held.reply);
        const auto sent = connection_.send(response);
        if (duplicate) connection_.send(response);
        return sent;
    }

public slots:
    QByteArray GetOptionCatalog(QString &replyCatalogDigest)
    {
        replyCatalogDigest = optionCatalogDigest_;
        return optionCatalog_;
    }

    QByteArray GetActionCatalog(
        QString &replyActionCatalogDigest,
        QByteArray &replyConfigSchema,
        QString &replyConfigSchemaDigest
    )
    {
        replyActionCatalogDigest.clear();
        replyConfigSchema.clear();
        replyConfigSchemaDigest.clear();
        if (!actionCatalogErrorName_.isEmpty()) {
            sendErrorReply(
                actionCatalogErrorName_, actionCatalogErrorMessage_
            );
            return {};
        }
        replyActionCatalogDigest = actionCatalogReplyDigest_;
        replyConfigSchema = configSchema_;
        replyConfigSchemaDigest = configSchemaDigest_;
        return actionCatalog_;
    }

    QByteArray GetSnapshot(
        qulonglong &snapshotRevision,
        QString &snapshotCatalogDigest,
        QString &snapshotActionCatalogDigest
    )
    {
        ++snapshotCallCount_;
        if (holdSnapshots_ && calledFromDBus()) {
            setDelayedReply(true);
            heldSnapshots_.append(message());
            return {};
        }
        snapshotRevision = authorityRevision_;
        snapshotCatalogDigest = catalogDigest;
        snapshotActionCatalogDigest = actionCatalogDigest;
        return snapshotBytes_;
    }

    QByteArray GetConnectedDisplays(qulonglong &observedAtMs)
    {
        if (connectedDisplaysFail_) {
            sendErrorReply(
                QStringLiteral("org.hyprshelld.Compositor1.Error.RuntimeUnavailable"),
                QStringLiteral("Injected display discovery failure")
            );
            return {};
        }
        observedAtMs = 1'800'000'000'000ULL;
        return topologyBytes();
    }

    QByteArray GetConnectedInputDevices(qulonglong &observedAtMs)
    {
        ++inputDeviceCallCount_;
        if (holdInputDevices_ && calledFromDBus()) {
            setDelayedReply(true);
            heldInputDevices_.append({
                .call = message(),
                .inventory = inputDeviceInventory_,
                .observedAtMs = inputDevicesObservedAtMs_,
                .errorName = inputDeviceErrorName_,
                .errorMessage = inputDeviceErrorMessage_,
            });
            observedAtMs = 0;
            return {};
        }
        if (!inputDeviceErrorName_.isEmpty()) {
            sendErrorReply(inputDeviceErrorName_, inputDeviceErrorMessage_);
            observedAtMs = 0;
            return {};
        }
        observedAtMs = inputDevicesObservedAtMs_;
        return inputDeviceInventory_;
    }

    QString GetPendingDisplayConfirmation(
        qulonglong &pendingRevision,
        qulonglong &pendingDeadlineMs,
        QString &pendingGeneration
    )
    {
        ++pendingCallCount_;
        if (pendingBehavior_ == PendingBehavior::NoDisplayConfirmation) {
            sendErrorReply(
                QStringLiteral(
                    "org.hyprshelld.Compositor1.Error.NoDisplayConfirmation"
                ),
                QStringLiteral("No display confirmation belongs to this caller")
            );
            return {};
        }
        if (pendingBehavior_ == PendingBehavior::UnexpectedError) {
            sendErrorReply(
                QStringLiteral("org.freedesktop.DBus.Error.UnknownMethod"),
                QStringLiteral("Injected pending-confirmation failure")
            );
            return {};
        }
        pendingRevision = confirmationRevision_;
        pendingDeadlineMs = confirmationDeadlineMs_;
        pendingGeneration = confirmationGeneration_;
        return confirmationToken;
    }

    qulonglong ReplaceSnapshot(
        const qulonglong expectedRevision,
        const QString &expectedCatalogDigest,
        const QString &expectedActionCatalogDigest,
        const QByteArray &candidateSnapshot
    )
    {
        ++replaceCallCount_;
        if (holdReplaces_ && calledFromDBus()) {
            setDelayedReply(true);
            heldReplaces_.append(message());
            return authorityRevision_;
        }
        if (expectedCatalogDigest != catalogDigest
            || expectedActionCatalogDigest != actionCatalogDigest) {
            sendErrorReply(
                QStringLiteral("org.hyprshelld.Compositor1.Error.StaleCatalogDigest"),
                QStringLiteral("Injected stale catalog")
            );
            return authorityRevision_;
        }
        if (failNextReplace_) {
            failNextReplace_ = false;
            sendErrorReply(
                QStringLiteral(
                    "org.hyprshelld.Compositor1.Error.InvalidSnapshot"
                ),
                QStringLiteral("Injected replacement failure")
            );
            return authorityRevision_;
        }
        auto object = QJsonDocument::fromJson(candidateSnapshot).object();
        if (object.isEmpty()) {
            sendErrorReply(
                QStringLiteral("org.hyprshelld.Compositor1.Error.InvalidSnapshot"),
                QStringLiteral("Injected invalid candidate")
            );
            return authorityRevision_;
        }
        object.insert(
            QStringLiteral("revision"),
            QString::number(authorityRevision_)
        );
        auto exactCurrent = QJsonDocument(object).toJson(
            QJsonDocument::Compact
        );
        exactCurrent.append('\n');
        if (expectedRevision + 1 == authorityRevision_
            && exactCurrent == snapshotBytes_) {
            publishAuthority();
            return authorityRevision_;
        }
        if (expectedRevision != authorityRevision_) {
            sendErrorReply(
                QStringLiteral("org.hyprshelld.Compositor1.Error.StaleRevision"),
                QStringLiteral("Injected stale revision")
            );
            return authorityRevision_;
        }
        object.insert(
            QStringLiteral("revision"),
            QString::number(authorityRevision_ + 1)
        );
        snapshotBytes_ = QJsonDocument(object).toJson(QJsonDocument::Compact);
        snapshotBytes_.append('\n');
        ++authorityRevision_;
        applyState_ = QStringLiteral("retained");
        requiredActivation_ = QStringLiteral("reload");
        publishAuthority();
        if (ambiguousFirstReplace_ && replaceCallCount_ == 1) {
            sendErrorReply(
                QStringLiteral("org.freedesktop.DBus.Error.NoReply"),
                QStringLiteral("Injected lost replacement reply")
            );
        }
        return authorityRevision_;
    }

    qulonglong Apply(
        const qulonglong expectedRevision,
        const QString &expectedCatalogDigest,
        const QString &expectedActionCatalogDigest,
        QString &appliedGenerationDigest
    )
    {
        ++applyCallCount_;
        appliedGenerationDigest = authorityGenerationDigest_;
        if (expectedRevision != authorityRevision_
            || expectedCatalogDigest != catalogDigest
            || expectedActionCatalogDigest != actionCatalogDigest) {
            sendErrorReply(
                QStringLiteral("org.hyprshelld.Compositor1.Error.StaleRevision"),
                QStringLiteral("Injected stale apply")
            );
            return appliedRevision_;
        }
        if (failNextApply_) {
            failNextApply_ = false;
            sendErrorReply(
                QStringLiteral("org.hyprshelld.Compositor1.Error.ApplyFailed"),
                QStringLiteral("Injected apply failure")
            );
            return appliedRevision_;
        }
        appliedRevision_ = authorityRevision_;
        applyState_ = QStringLiteral("current");
        requiredActivation_ = QStringLiteral("none");
        authorityGenerationDigest_ = QString(64, QLatin1Char('8'));
        appliedGenerationDigest = authorityGenerationDigest_;
        if (ambiguousFirstApplyWithProperties_ && applyCallCount_ == 1) {
            publishAuthority();
            sendErrorReply(
                QStringLiteral("org.freedesktop.DBus.Error.NoReply"),
                QStringLiteral("Injected lost apply reply after properties")
            );
            return appliedRevision_;
        }
        if (ambiguousFirstApplyWithoutProperties_ && applyCallCount_ == 1) {
            sendErrorReply(
                QStringLiteral("org.freedesktop.DBus.Error.NoReply"),
                QStringLiteral("Injected lost apply reply")
            );
            return appliedRevision_;
        }
        publishAuthority();
        return appliedRevision_;
    }

    qulonglong Recover(
        const qulonglong expectedRevision,
        const QString &expectedCatalogDigest,
        const QString &expectedActionCatalogDigest,
        qulonglong &recoveredAppliedRevision,
        QString &recoveredGenerationDigest
    )
    {
        ++recoverCallCount_;
        if (expectedRevision != authorityRevision_
            || expectedCatalogDigest != catalogDigest
            || expectedActionCatalogDigest != actionCatalogDigest
            || appliedRevision_ == authorityRevision_) {
            sendErrorReply(
                QStringLiteral("org.hyprshelld.Compositor1.Error.RecoveryUnavailable"),
                QStringLiteral("Injected recovery unavailable")
            );
            recoveredAppliedRevision = appliedRevision_;
            recoveredGenerationDigest = authorityGenerationDigest_;
            return authorityRevision_;
        }
        ++authorityRevision_;
        appliedRevision_ = authorityRevision_;
        snapshotBytes_ = snapshotBytes(authorityRevision_);
        applyState_ = QStringLiteral("current");
        requiredActivation_ = QStringLiteral("none");
        authorityGenerationDigest_ = QString(64, QLatin1Char('7'));
        recoveredAppliedRevision = appliedRevision_;
        recoveredGenerationDigest = authorityGenerationDigest_;
        if (ambiguousRecover_) {
            sendErrorReply(
                QStringLiteral("org.freedesktop.DBus.Error.NoReply"),
                QStringLiteral("Injected lost recovery reply")
            );
            return authorityRevision_;
        }
        publishAuthority();
        return authorityRevision_;
    }

    qulonglong PreviewDisplayConfiguration(
        qulonglong expectedRevision,
        const QString &expectedCatalogDigest,
        const QString &expectedActionCatalogDigest,
        const QByteArray &profile,
        uint timeoutSeconds,
        QString &token,
        qulonglong &deadlineMs,
        QString &previewGenerationDigest
    )
    {
        Q_UNUSED(expectedRevision)
        Q_UNUSED(expectedCatalogDigest)
        Q_UNUSED(expectedActionCatalogDigest)
        Q_UNUSED(profile)
        Q_UNUSED(timeoutSeconds)
        Q_UNUSED(token)
        Q_UNUSED(deadlineMs)
        Q_UNUSED(previewGenerationDigest)
        if (!calledFromDBus()) return 0;
        setDelayedReply(true);
        heldPreviews_.append({
            .call = message(),
            .reply = previewReply_,
            .project = projectPreviewReply_,
        });
        if (!holdPreviews_) {
            QTimer::singleShot(0, this, [this] {
                releaseNextPreview();
            });
        }
        return 0;
    }

    qulonglong ConfirmDisplayConfiguration(
        const QString &token,
        QString &confirmedGeneration
    )
    {
        Q_UNUSED(token)
        Q_UNUSED(confirmedGeneration)
        if (!calledFromDBus()) return 0;
        setDelayedReply(true);
        const auto call = message();
        QTimer::singleShot(0, this, [this, call] {
            managementState_ = QStringLiteral("preview");
            confirmationState_ = QStringLiteral("committing");
            clearConfirmationTuple();
            publishConfirmation();
            managementState_ = QStringLiteral("managed");
            confirmationState_ = QStringLiteral("idle");
            publishConfirmation();
            connection_.send(call.createReply({
                QVariant::fromValue<qulonglong>(previewRevision),
                previewGeneration,
            }));
        });
        return 0;
    }

    qulonglong RevertDisplayConfiguration(const QString &token)
    {
        Q_UNUSED(token)
        if (!calledFromDBus()) return 0;
        setDelayedReply(true);
        const auto call = message();
        QTimer::singleShot(0, this, [this, call] {
            managementState_ = QStringLiteral("preview");
            confirmationState_ = QStringLiteral("reverting");
            clearConfirmationTuple();
            publishConfirmation();
            managementState_ = QStringLiteral("managed");
            confirmationState_ = QStringLiteral("idle");
            publishConfirmation();
            connection_.send(call.createReply({
                QVariant::fromValue<qulonglong>(baselineRevision),
            }));
        });
        return 0;
    }

    void RetrySharedBorderSync()
    {
        ++retrySharedBorderSyncCallCount_;
        if (retrySharedBorderSyncErrorName_.isEmpty()) return;
        const auto errorName = std::exchange(
            retrySharedBorderSyncErrorName_,
            {}
        );
        const auto errorMessage = std::exchange(
            retrySharedBorderSyncErrorMessage_,
            {}
        );
        sendErrorReply(errorName, errorMessage);
    }

    void RetrySharedSpacingSync()
    {
        ++retrySharedSpacingSyncCallCount_;
        if (retrySharedSpacingSyncErrorName_.isEmpty()) return;
        const auto errorName = std::exchange(
            retrySharedSpacingSyncErrorName_,
            {}
        );
        const auto errorMessage = std::exchange(
            retrySharedSpacingSyncErrorMessage_,
            {}
        );
        sendErrorReply(errorName, errorMessage);
    }

private:
    struct HeldInputDevice final {
        QDBusMessage call;
        QByteArray inventory;
        qulonglong observedAtMs = 0;
        QString errorName;
        QString errorMessage;
    };

    struct HeldPreview final {
        QDBusMessage call;
        QVariantList reply;
        bool project = false;
    };

    void projectPreview(const QVariantList &reply)
    {
        if (reply.size() != 4) return;
        managementState_ = QStringLiteral("preview");
        confirmationState_ = QStringLiteral("awaiting-confirmation");
        confirmationRevision_ = reply.at(0).toULongLong();
        confirmationDeadlineMs_ = reply.at(2).toULongLong();
        confirmationGeneration_ = reply.at(3).toString();
    }

    void clearConfirmationTuple()
    {
        confirmationRevision_ = 0;
        confirmationDeadlineMs_ = 0;
        confirmationGeneration_.clear();
    }

    bool publishAuthority()
    {
        auto signal = QDBusMessage::createSignal(
            objectPath,
            propertiesInterface,
            QStringLiteral("PropertiesChanged")
        );
        signal.setArguments({
            interfaceName,
            QVariantMap{
                {
                    QStringLiteral("Revision"),
                    QVariant::fromValue<qulonglong>(authorityRevision_)
                },
                {
                    QStringLiteral("AppliedRevision"),
                    QVariant::fromValue<qulonglong>(appliedRevision_)
                },
                {QStringLiteral("ApplyState"), applyState_},
                {QStringLiteral("RequiredActivation"), requiredActivation_},
                {
                    QStringLiteral("GenerationDigest"),
                    authorityGenerationDigest_
                },
            },
            QStringList{},
        });
        return connection_.send(signal);
    }

    bool publishConfirmation()
    {
        auto signal = QDBusMessage::createSignal(
            objectPath,
            propertiesInterface,
            QStringLiteral("PropertiesChanged")
        );
        signal.setArguments({
            interfaceName,
            QVariantMap{
                {QStringLiteral("ManagementState"), managementState_},
                {QStringLiteral("DisplayConfirmationState"), confirmationState_},
                {
                    QStringLiteral("DisplayConfirmationRevision"),
                    QVariant::fromValue<qulonglong>(confirmationRevision_)
                },
                {
                    QStringLiteral("DisplayConfirmationDeadlineMs"),
                    QVariant::fromValue<qulonglong>(confirmationDeadlineMs_)
                },
                {
                    QStringLiteral("DisplayConfirmationGeneration"),
                    confirmationGeneration_
                },
            },
            QStringList{},
        });
        return connection_.send(signal);
    }

    QDBusConnection connection_;
    QString managementState_;
    QString confirmationState_;
    qulonglong confirmationRevision_ = 0;
    qulonglong confirmationDeadlineMs_ = 0;
    QString confirmationGeneration_;
    QString sharedBorderSyncState_;
    qulonglong sharedBorderSourceRevision_ = 0;
    QString sharedBorderSyncError_;
    int retrySharedBorderSyncCallCount_ = 0;
    QString retrySharedBorderSyncErrorName_;
    QString retrySharedBorderSyncErrorMessage_;
    QString sharedSpacingSyncState_;
    qulonglong sharedSpacingSourceRevision_ = 0;
    QString sharedSpacingSyncError_;
    int retrySharedSpacingSyncCallCount_ = 0;
    QString retrySharedSpacingSyncErrorName_;
    QString retrySharedSpacingSyncErrorMessage_;
    PendingBehavior pendingBehavior_ = PendingBehavior::NoDisplayConfirmation;
    qsizetype pendingCallCount_ = 0;
    bool holdSnapshots_ = false;
    QList<QDBusMessage> heldSnapshots_;
    int snapshotCallCount_ = 0;
    bool holdPreviews_ = false;
    QList<HeldPreview> heldPreviews_;
    QVariantList previewReply_;
    bool projectPreviewReply_ = false;
    QByteArray optionCatalog_;
    QString optionCatalogDigest_;
    QByteArray actionCatalog_;
    QString actionCatalogReplyDigest_;
    QByteArray configSchema_;
    QString configSchemaDigest_;
    QString actionCatalogErrorName_;
    QString actionCatalogErrorMessage_;
    bool connectedDisplaysFail_ = false;
    QByteArray inputDeviceInventory_ = inputDeviceInventoryBytes();
    qulonglong inputDevicesObservedAtMs_ = 1'800'000'000'100ULL;
    QString inputDeviceErrorName_;
    QString inputDeviceErrorMessage_;
    bool holdInputDevices_ = false;
    QList<HeldInputDevice> heldInputDevices_;
    int inputDeviceCallCount_ = 0;
    bool running_ = false;
    qulonglong authorityRevision_ = baselineRevision;
    qulonglong appliedRevision_ = baselineRevision;
    QString applyState_ = QStringLiteral("current");
    QString requiredActivation_ = QStringLiteral("none");
    QString authorityGenerationDigest_ = generationDigest;
    QByteArray snapshotBytes_ = snapshotBytes();
    int replaceCallCount_ = 0;
    int applyCallCount_ = 0;
    int recoverCallCount_ = 0;
    bool ambiguousFirstReplace_ = false;
    bool failNextReplace_ = false;
    bool failNextApply_ = false;
    bool ambiguousFirstApplyWithoutProperties_ = false;
    bool ambiguousFirstApplyWithProperties_ = false;
    bool ambiguousRecover_ = false;
    bool holdReplaces_ = false;
    QList<QDBusMessage> heldReplaces_;
};

} // namespace

class CompositorClientTest final : public QObject {
    Q_OBJECT

private slots:
    void initTestCase()
    {
        QVERIFY2(bus_.isConnected(), qPrintable(bus_.lastError().message()));
        QVERIFY2(
            serviceBus_.isConnected(),
            qPrintable(serviceBus_.lastError().message())
        );
        QVERIFY(serviceBus_.registerObject(
            objectPath,
            &service_,
            QDBusConnection::ExportAllProperties
                | QDBusConnection::ExportAllSlots
        ));
    }

    void cleanup()
    {
        service_.stop();
        serviceBus_.unregisterService(busName);
        service_.reset();
        QCoreApplication::processEvents();
    }

    void cleanupTestCase()
    {
        serviceBus_.unregisterObject(objectPath);
        QDBusConnection::disconnectFromBus(
            QStringLiteral("compositor-client-test")
        );
        QDBusConnection::disconnectFromBus(
            QStringLiteral("compositor-service-test")
        );
    }

    void projectsEveryCatalogOptionWithFullMetadataAndResolvedValues()
    {
        service_.setSnapshotBytes(snapshotWithOverride(
            QStringLiteral("hyprland.general.locale"),
            QStringLiteral("en_US.UTF-8")
        ));
        QVERIFY(service_.start());

        HyprShelld::CompositorClient client(bus_, nullptr);
        QTRY_VERIFY_WITH_TIMEOUT(client.catalogAvailable(), 3000);
        QTRY_COMPARE_WITH_TIMEOUT(client.allOptions().size(), 353, 3000);
        QTRY_VERIFY_WITH_TIMEOUT(
            client.allValues().size() == 353
                || !client.allOptionsErrorName().isEmpty(),
            3000
        );
        QVERIFY2(
            client.allValues().size() == 353,
            qPrintable(QStringLiteral("%1: %2")
                .arg(client.allOptionsErrorName(),
                     client.allOptionsErrorMessage()))
        );
        QTRY_VERIFY_WITH_TIMEOUT(client.rulesAvailable(), 3000);
        QVERIFY2(
            client.allOptionsAvailable(),
            qPrintable(QStringLiteral("%1: %2")
                .arg(client.allOptionsErrorName(),
                     client.allOptionsErrorMessage()))
        );
        QCOMPARE(client.allOptions().size(), 353);
        QCOMPARE(client.allValues().size(), 353);
        QCOMPARE(
            client.allValues().value(
                QStringLiteral("hyprland.general.locale")
            ).toString(),
            QStringLiteral("en_US.UTF-8")
        );
        QVERIFY(client.allValues().contains(
            QStringLiteral("hyprland.input.scroll_points")
        ));

        QVariantMap borderSize;
        QVariantMap locale;
        QVariantMap scrollPoints;
        qsizetype writableCount = 0;
        qsizetype externalCount = 0;
        qsizetype inheritedDefaultCount = 0;
        QSet<QString> ids;
        for (const auto &entry : client.allOptions()) {
            const auto metadata = entry.toMap();
            const auto id = metadata.value(QStringLiteral("id")).toString();
            QVERIFY(!id.isEmpty());
            QVERIFY(!ids.contains(id));
            ids.insert(id);
            if (metadata.value(QStringLiteral("writable")).toBool()) {
                ++writableCount;
            }
            if (metadata.value(QStringLiteral("uiTier")).toString()
                == QStringLiteral("external")) {
                ++externalCount;
            }
            if (!metadata.value(
                    QStringLiteral("inheritedDefaultFrom")
                ).toString().isEmpty()) {
                ++inheritedDefaultCount;
            }
            if (id == QStringLiteral("hyprland.general.border_size")) {
                borderSize = metadata;
            } else if (id == QStringLiteral("hyprland.general.locale")) {
                locale = metadata;
            } else if (id
                       == QStringLiteral("hyprland.input.scroll_points")) {
                scrollPoints = metadata;
            }
        }
        QCOMPARE(ids.size(), 353);
        QCOMPARE(writableCount, 349);
        QCOMPARE(externalCount, 10);
        QCOMPARE(inheritedDefaultCount, 5);
        QCOMPARE(
            client.allValues().value(
                QStringLiteral("hyprland.decoration.glow.color_inactive")
            ),
            client.allValues().value(
                QStringLiteral("hyprland.decoration.glow.color")
            )
        );

        QCOMPARE(borderSize.value(QStringLiteral("module")).toString(),
                 QStringLiteral("general"));
        QCOMPARE(borderSize.value(QStringLiteral("path")).toString(),
                 QStringLiteral("general:border_size"));
        QCOMPARE(borderSize.value(QStringLiteral("luaPath")).toStringList(),
                 QStringList({QStringLiteral("general"),
                              QStringLiteral("border_size")}));
        QCOMPARE(borderSize.value(QStringLiteral("uiTier")).toString(),
                 QStringLiteral("common"));
        QCOMPARE(borderSize.value(QStringLiteral("writable")).toBool(), true);
        QCOMPARE(borderSize.value(QStringLiteral("defaultPolicy")).toString(),
                 QStringLiteral("hyprland"));
        QCOMPARE(
            borderSize.value(QStringLiteral("inheritedDefaultFrom")).toString(),
            QString{}
        );
        QCOMPARE(borderSize.value(QStringLiteral("applyMode")).toString(),
                 QStringLiteral("reload"));
        QCOMPARE(borderSize.value(QStringLiteral("since")).toString(),
                 QStringLiteral("0.55.0"));
        QCOMPARE(borderSize.value(QStringLiteral("until")).toString(),
                 QString{});
        QCOMPARE(locale.value(QStringLiteral("uiTier")).toString(),
                 QStringLiteral("external"));
        QCOMPARE(locale.value(QStringLiteral("writable")).toBool(), true);
        QCOMPARE(scrollPoints.value(QStringLiteral("writable")).toBool(), false);

        const auto closedMetadata = client.appearanceOptions().constFirst().toMap();
        QVERIFY(!closedMetadata.contains(QStringLiteral("module")));
        QVERIFY(!closedMetadata.contains(QStringLiteral("writable")));
        QVERIFY(client.allOptionsErrorName().isEmpty());
        QVERIFY(client.allOptionsErrorMessage().isEmpty());
    }

    void savesSparseWritableOptionsAndPreservesEverythingElse()
    {
        auto initial = QJsonDocument::fromJson(snapshotBytes()).object();
        auto overrides = initial.value(QStringLiteral("overrides")).toObject();
        overrides.insert(
            QStringLiteral("hyprland.cursor.zoom_factor"), 2.0
        );
        overrides.insert(
            QStringLiteral("hyprland.general.border_size"), 7
        );
        overrides.insert(
            QStringLiteral("hyprland.decoration.glow.color_inactive"),
            QJsonObject{
                {
                    QStringLiteral("colors"),
                    QJsonArray{QStringLiteral("0x99446688")},
                },
                {QStringLiteral("angle"), 0},
            }
        );
        initial.insert(QStringLiteral("overrides"), overrides);
        auto initialBytes = HyprShelld::Hyprland::JsonSupport::canonicalJson(
            initial
        );
        initialBytes.append('\n');
        service_.setSnapshotBytes(std::move(initialBytes));
        QVERIFY(service_.start());

        HyprShelld::CompositorClient client(bus_, nullptr);
        QTRY_VERIFY_WITH_TIMEOUT(client.allOptionsAvailable(), 3000);
        QStringList observedOperations;
        connect(
            &client,
            &HyprShelld::CompositorClient::busyOperationChanged,
            this,
            [&client, &observedOperations] {
                observedOperations.append(client.busyOperation());
            }
        );

        QVariantMap patch{
            {QStringLiteral("hyprland.cursor.zoom_factor"), 1.0},
            {
                QStringLiteral("hyprland.general.locale"),
                QStringLiteral("en_CA.UTF-8")
            },
        };
        patch.insert(
            QStringLiteral("hyprland.decoration.glow.color_inactive"),
            client.allValues().value(
                QStringLiteral("hyprland.decoration.glow.color")
            )
        );
        client.saveOptions(patch);
        QTRY_COMPARE_WITH_TIMEOUT(client.revision(), 8ULL, 3000);
        QTRY_VERIFY_WITH_TIMEOUT(!client.busy(), 3000);
        QTRY_VERIFY_WITH_TIMEOUT(client.allOptionsAvailable(), 3000);

        QCOMPARE(service_.replaceCallCount(), 1);
        QCOMPARE(service_.applyCallCount(), 1);
        QVERIFY(observedOperations.contains(QStringLiteral("options-save")));
        QVERIFY(observedOperations.contains(QStringLiteral("options-apply")));
        QCOMPARE(
            client.allValues().value(
                QStringLiteral("hyprland.cursor.zoom_factor")
            ).toDouble(),
            1.0
        );
        QCOMPARE(
            client.allValues().value(
                QStringLiteral("hyprland.general.locale")
            ).toString(),
            QStringLiteral("en_CA.UTF-8")
        );

        const auto saved = QJsonDocument::fromJson(
            service_.currentSnapshotBytes()
        ).object();
        const auto savedOverrides = saved.value(
            QStringLiteral("overrides")
        ).toObject();
        QVERIFY(!savedOverrides.contains(
            QStringLiteral("hyprland.cursor.zoom_factor")
        ));
        QVERIFY(!savedOverrides.contains(
            QStringLiteral("hyprland.decoration.glow.color_inactive")
        ));
        QCOMPARE(savedOverrides.value(
            QStringLiteral("hyprland.general.locale")
        ).toString(), QStringLiteral("en_CA.UTF-8"));
        QCOMPARE(savedOverrides.value(
            QStringLiteral("hyprland.general.border_size")
        ).toInt(), 7);
        for (auto iterator = initial.constBegin();
             iterator != initial.constEnd();
             ++iterator) {
            if (iterator.key() == QStringLiteral("revision")
                || iterator.key() == QStringLiteral("overrides")) {
                continue;
            }
            QCOMPARE(saved.value(iterator.key()), iterator.value());
        }
        QVERIFY(client.allOptionsErrorName().isEmpty());
        QVERIFY(client.lastErrorName().isEmpty());
    }

    void rejectsNonWritableUnknownInvalidAndUnsafeOptionPatches()
    {
        QVERIFY(service_.start());
        HyprShelld::CompositorClient client(bus_, nullptr);
        QTRY_VERIFY_WITH_TIMEOUT(client.allOptionsAvailable(), 3000);

        const QList<QVariantMap> invalidPatches{
            {{QStringLiteral("hyprland.input.scroll_points"),
              QStringLiteral("0.0 1.0")}},
            {{QStringLiteral("hyprland.unknown.option"), true}},
            {{QStringLiteral("hyprland.general.border_size"),
              QStringLiteral("2")}},
        };
        for (const auto &patch : invalidPatches) {
            client.saveOptions(patch);
            QCOMPARE(client.busy(), false);
            QCOMPARE(service_.replaceCallCount(), 0);
            QCOMPARE(service_.applyCallCount(), 0);
            QCOMPARE(
                client.allOptionsErrorName(),
                QStringLiteral(
                    "org.hyprshelld.Client.Compositor.Error.InvalidOptions"
                )
            );
            QVERIFY(client.appearanceErrorName().isEmpty());
            QVERIFY(client.lastErrorName().isEmpty());
        }

        client.saveOptions({
            {QStringLiteral("hyprland.decoration.glow.enabled"), true},
            {QStringLiteral("hyprland.decoration.glow.range"), 5},
        });
        QCOMPARE(client.busy(), false);
        QCOMPARE(service_.replaceCallCount(), 0);
        QCOMPARE(service_.applyCallCount(), 0);
        QCOMPARE(
            client.allOptionsErrorName(),
            QStringLiteral(
                "org.hyprshelld.Client.Compositor.Error.InvalidOptions"
            )
        );
        QVERIFY(client.allOptionsErrorMessage().contains(
            QStringLiteral("range is at least 10")
        ));

        client.saveOptions({
            {QStringLiteral("hyprland.cursor.zoom_factor"), 1.0},
        });
        QCOMPARE(service_.replaceCallCount(), 0);
        QCOMPARE(
            client.allOptionsErrorName(),
            QStringLiteral(
                "org.hyprshelld.Client.Compositor.Error.NoChanges"
            )
        );
    }

    void hydratesTheTrustedScalarAndRulesDomains()
    {
        QVERIFY(service_.start());

        HyprShelld::CompositorClient client(bus_, nullptr);
        QTRY_VERIFY_WITH_TIMEOUT(client.appearanceAvailable(), 3000);
        QTRY_VERIFY_WITH_TIMEOUT(client.catalogAvailable(), 3000);
        QTRY_VERIFY_WITH_TIMEOUT(client.displayDiscoveryAvailable(), 3000);
        QTRY_VERIFY_WITH_TIMEOUT(client.appearanceAvailable(), 3000);
        QTRY_VERIFY_WITH_TIMEOUT(client.windowsAvailable(), 3000);
        QTRY_VERIFY_WITH_TIMEOUT(client.workspacesAvailable(), 3000);
        QTRY_VERIFY_WITH_TIMEOUT(client.advancedAvailable(), 3000);
        QCOMPARE(client.appearanceProjectionAvailable(), true);
        QCOMPARE(client.appearanceAnimationProjectionAvailable(), true);
        QCOMPARE(client.windowsProjectionAvailable(), true);
        QCOMPARE(client.workspacesProjectionAvailable(), true);
        QCOMPARE(client.advancedProjectionAvailable(), true);
        QCOMPARE(client.appearanceOptions().size(), 40);
        QCOMPARE(client.appearanceValues(), appearanceDefaults());
        QStringList appearanceIds;
        for (const auto &option : client.appearanceOptions()) {
            appearanceIds.append(
                option.toMap().value(QStringLiteral("id")).toString()
            );
        }
        QCOMPARE(appearanceIds, QStringList({
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
            QStringLiteral("hyprland.decoration.blur.new_optimizations"),
            QStringLiteral("hyprland.decoration.blur.xray"),
            QStringLiteral("hyprland.decoration.blur.special"),
            QStringLiteral("hyprland.decoration.blur.popups"),
            QStringLiteral("hyprland.decoration.blur.popups_ignorealpha"),
            QStringLiteral("hyprland.decoration.blur.input_methods"),
            QStringLiteral(
                "hyprland.decoration.blur.input_methods_ignorealpha"
            ),
            QStringLiteral("hyprland.decoration.blur.brightness"),
            QStringLiteral("hyprland.decoration.blur.contrast"),
            QStringLiteral("hyprland.decoration.blur.noise"),
            QStringLiteral("hyprland.decoration.blur.vibrancy"),
            QStringLiteral("hyprland.decoration.blur.vibrancy_darkness"),
            QStringLiteral("hyprland.decoration.border_part_of_window"),
            QStringLiteral("hyprland.decoration.rounding_power"),
            QStringLiteral("hyprland.decoration.shadow.range"),
            QStringLiteral("hyprland.decoration.shadow.render_power"),
            QStringLiteral("hyprland.decoration.shadow.sharp"),
            QStringLiteral("hyprland.decoration.shadow.offset"),
            QStringLiteral("hyprland.decoration.shadow.scale"),
            QStringLiteral("hyprland.decoration.glow.enabled"),
            QStringLiteral("hyprland.decoration.glow.range"),
            QStringLiteral("hyprland.decoration.glow.render_power"),
        }));
        const auto dimInactive = client.appearanceOptions().at(7).toMap();
        QCOMPARE(dimInactive.value(QStringLiteral("type")).toString(),
                 QStringLiteral("boolean"));
        QCOMPARE(dimInactive.value(QStringLiteral("control")).toString(),
                 QStringLiteral("toggle"));
        QCOMPARE(dimInactive.value(QStringLiteral("defaultValue")).toBool(),
                 false);
        QVERIFY(!dimInactive.contains(QStringLiteral("min")));
        QVERIFY(!dimInactive.contains(QStringLiteral("max")));
        const auto dimStrength = client.appearanceOptions().at(8).toMap();
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

        const auto appearanceOptions = client.appearanceOptions();
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
        QCOMPARE(
            shadowOffset.value(QStringLiteral("risk")).toString(),
            QStringLiteral("safe")
        );
        QVERIFY(!shadowOffset.contains(QStringLiteral("step")));
        QVERIFY(!shadowOffset.contains(QStringLiteral("choices")));
        QVERIFY(client.appearanceCurves().isEmpty());
        QVERIFY(client.appearanceAnimations().isEmpty());
        QTRY_VERIFY_WITH_TIMEOUT(client.inputAvailable(), 3000);
        QCOMPARE(client.inputProjectionAvailable(), true);
        QCOMPARE(client.inputGesturesProjectionAvailable(), true);
        QCOMPARE(client.inputOptions().size(), 49);
        QCOMPARE(client.inputValues().size(), 49);
        QVERIFY(client.inputGestures().isEmpty());
        QVERIFY(client.inputGestureCompatibility().isEmpty());
        QStringList authoredGestureActionIds;
        for (const auto &action : client.inputGestureActions()) {
            authoredGestureActionIds.append(
                action.toMap().value(QStringLiteral("id")).toString()
            );
        }
        QCOMPARE(authoredGestureActionIds, QStringList({
            QStringLiteral("close"),
            QStringLiteral("cursorZoom"),
            QStringLiteral("float"),
            QStringLiteral("fullscreen"),
            QStringLiteral("move"),
            QStringLiteral("resize"),
            QStringLiteral("scrollMove"),
            QStringLiteral("special"),
            QStringLiteral("workspace"),
        }));
        QStringList inputIds;
        for (const auto &option : client.inputOptions()) {
            inputIds.append(
                option.toMap().value(QStringLiteral("id")).toString()
            );
        }
        QCOMPARE(inputIds, QStringList({
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
            QStringLiteral("hyprland.input.touchpad.disable_while_typing"),
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
        }));
        const auto touchdeviceEnabled = client.inputOptions().at(32).toMap();
        QCOMPARE(touchdeviceEnabled.value(QStringLiteral("type")).toString(),
                 QStringLiteral("boolean"));
        QCOMPARE(touchdeviceEnabled.value(QStringLiteral("control")).toString(),
                 QStringLiteral("toggle"));
        QCOMPARE(touchdeviceEnabled.value(
            QStringLiteral("defaultValue")
        ).toBool(), true);
        for (const auto index : {qsizetype(33), qsizetype(36)}) {
            const auto transform = client.inputOptions().at(index).toMap();
            QCOMPARE(transform.value(QStringLiteral("type")).toString(),
                     QStringLiteral("integer"));
            QCOMPARE(transform.value(QStringLiteral("control")).toString(),
                     QStringLiteral("spinBox"));
            QCOMPARE(transform.value(QStringLiteral("defaultValue")).toInt(), 0);
            QCOMPARE(transform.value(QStringLiteral("min")).toInt(), 0);
            QCOMPARE(transform.value(QStringLiteral("max")).toInt(), 6);
        }
        for (const auto index : {qsizetype(34), qsizetype(35)}) {
            const auto toggle = client.inputOptions().at(index).toMap();
            QCOMPARE(toggle.value(QStringLiteral("type")).toString(),
                     QStringLiteral("boolean"));
            QCOMPARE(toggle.value(QStringLiteral("control")).toString(),
                     QStringLiteral("toggle"));
            QCOMPARE(toggle.value(QStringLiteral("defaultValue")).toBool(), false);
        }
        const QList<std::pair<qsizetype, bool>> cursorToggleDefaults{
            {37, false}, {38, true}, {39, false},
            {42, false}, {43, false}, {44, false},
        };
        for (const auto &[index, defaultValue] : cursorToggleDefaults) {
            const auto toggle = client.inputOptions().at(index).toMap();
            QCOMPARE(toggle.value(QStringLiteral("type")).toString(),
                     QStringLiteral("boolean"));
            QCOMPARE(toggle.value(QStringLiteral("control")).toString(),
                     QStringLiteral("toggle"));
            QCOMPARE(toggle.value(QStringLiteral("defaultValue")).toBool(),
                     defaultValue);
            QCOMPARE(toggle.value(QStringLiteral("risk")).toString(),
                     QStringLiteral("safe"));
            QVERIFY(!toggle.contains(QStringLiteral("min")));
            QVERIFY(!toggle.contains(QStringLiteral("max")));
        }
        const auto cursorTimeout = client.inputOptions().at(40).toMap();
        QCOMPARE(cursorTimeout.value(QStringLiteral("type")).toString(),
                 QStringLiteral("number"));
        QCOMPARE(cursorTimeout.value(QStringLiteral("control")).toString(),
                 QStringLiteral("slider"));
        QCOMPARE(cursorTimeout.value(QStringLiteral("defaultValue")).toDouble(),
                 0.0);
        QCOMPARE(cursorTimeout.value(QStringLiteral("min")).toDouble(), 0.0);
        QCOMPARE(cursorTimeout.value(QStringLiteral("max")).toDouble(), 20.0);
        QCOMPARE(cursorTimeout.value(QStringLiteral("risk")).toString(),
                 QStringLiteral("safe"));
        const auto cursorPadding = client.inputOptions().at(41).toMap();
        QCOMPARE(cursorPadding.value(QStringLiteral("type")).toString(),
                 QStringLiteral("integer"));
        QCOMPARE(cursorPadding.value(QStringLiteral("control")).toString(),
                 QStringLiteral("spinBox"));
        QCOMPARE(cursorPadding.value(QStringLiteral("defaultValue")).toInt(), 0);
        QCOMPARE(cursorPadding.value(QStringLiteral("min")).toInt(), 0);
        QCOMPARE(cursorPadding.value(QStringLiteral("max")).toInt(), 20);
        QCOMPARE(cursorPadding.value(QStringLiteral("risk")).toString(),
                 QStringLiteral("safe"));
        for (const auto index : {qsizetype(45), qsizetype(47)}) {
            const auto vector = client.inputOptions().at(index).toMap();
            QCOMPARE(vector.value(QStringLiteral("type")).toString(),
                     QStringLiteral("vector2"));
            QCOMPARE(vector.value(QStringLiteral("control")).toString(),
                     QStringLiteral("vector2"));
            QCOMPARE(vector.value(QStringLiteral("defaultValue")).toList(),
                     QVariantList({0.0, 0.0}));
            QCOMPARE(vector.value(QStringLiteral("risk")).toString(),
                     QStringLiteral("safe"));
        }
        QCOMPARE(
            client.inputOptions().at(45).toMap()
                .value(QStringLiteral("min")).toList(),
            QVariantList({-20000.0, -20000.0})
        );
        QCOMPARE(
            client.inputOptions().at(45).toMap()
                .value(QStringLiteral("max")).toList(),
            QVariantList({20000.0, 20000.0})
        );
        const auto absoluteRegion = client.inputOptions().at(46).toMap();
        QCOMPARE(absoluteRegion.value(QStringLiteral("type")).toString(),
                 QStringLiteral("boolean"));
        QCOMPARE(absoluteRegion.value(QStringLiteral("control")).toString(),
                 QStringLiteral("toggle"));
        QCOMPARE(absoluteRegion.value(
            QStringLiteral("defaultValue")
        ).toBool(), false);
        QCOMPARE(
            client.inputOptions().at(47).toMap()
                .value(QStringLiteral("min")).toList(),
            QVariantList({-100.0, -100.0})
        );
        QCOMPARE(
            client.inputOptions().at(47).toMap()
                .value(QStringLiteral("max")).toList(),
            QVariantList({4000.0, 4000.0})
        );
        const auto resolveBindsBySymbol = client.inputOptions().at(48).toMap();
        QCOMPARE(
            resolveBindsBySymbol.value(QStringLiteral("id")).toString(),
            QStringLiteral("hyprland.input.resolve_binds_by_sym")
        );
        QCOMPARE(
            resolveBindsBySymbol.value(QStringLiteral("type")).toString(),
            QStringLiteral("boolean")
        );
        QCOMPARE(
            resolveBindsBySymbol.value(QStringLiteral("control")).toString(),
            QStringLiteral("toggle")
        );
        QCOMPARE(
            resolveBindsBySymbol.value(
                QStringLiteral("defaultValue")
            ).toBool(),
            false
        );
        QCOMPARE(
            resolveBindsBySymbol.value(QStringLiteral("risk")).toString(),
            QStringLiteral("safe")
        );
        QVERIFY(!resolveBindsBySymbol.contains(QStringLiteral("min")));
        QVERIFY(!resolveBindsBySymbol.contains(QStringLiteral("max")));
        QVERIFY(!resolveBindsBySymbol.contains(QStringLiteral("step")));
        QVERIFY(!resolveBindsBySymbol.contains(QStringLiteral("choices")));
        QTRY_VERIFY_WITH_TIMEOUT(client.windowsAvailable(), 3000);
        QCOMPARE(client.windowsProjectionAvailable(), true);
        QCOMPARE(client.windowsOptions().size(), 110);
        QCOMPARE(client.windowsValues().size(), 110);
        QStringList windowsIds;
        for (const auto &option : client.windowsOptions()) {
            windowsIds.append(
                option.toMap().value(QStringLiteral("id")).toString()
            );
        }
        QCOMPARE(windowsIds.mid(65), QStringList({
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
        }));
        const auto followMouseThreshold =
            client.windowsOptions().at(109).toMap();
        QCOMPARE(
            followMouseThreshold.value(QStringLiteral("id")).toString(),
            QStringLiteral("hyprland.input.follow_mouse_threshold")
        );
        QCOMPARE(
            followMouseThreshold.value(QStringLiteral("type")).toString(),
            QStringLiteral("number")
        );
        QCOMPARE(
            followMouseThreshold.value(QStringLiteral("control")).toString(),
            QStringLiteral("slider")
        );
        QCOMPARE(
            followMouseThreshold.value(QStringLiteral("defaultValue"))
                .toDouble(),
            0.0
        );
        QCOMPARE(
            followMouseThreshold.value(QStringLiteral("min")).toDouble(),
            0.0
        );
        QCOMPARE(
            followMouseThreshold.value(QStringLiteral("max")).toDouble(),
            1000000.0
        );
        QCOMPARE(
            followMouseThreshold.value(QStringLiteral("risk")).toString(),
            QStringLiteral("safe")
        );
        QVERIFY(!followMouseThreshold.contains(QStringLiteral("step")));
        QCOMPARE(client.workspacesOptions().size(), 21);
        QCOMPARE(client.workspacesValues().size(), 21);
        QCOMPARE(client.workspaceRulesProjectionAvailable(), true);
        QVERIFY(client.workspaceRules().isEmpty());
        QTRY_VERIFY_WITH_TIMEOUT(client.advancedAvailable(), 3000);
        QCOMPARE(client.advancedProjectionAvailable(), true);
        QCOMPARE(client.advancedOptions().size(), 16);
        QCOMPARE(client.advancedValues(), advancedDefaults());
        QStringList advancedIds;
        for (const auto &option : client.advancedOptions()) {
            advancedIds.append(
                option.toMap().value(QStringLiteral("id")).toString()
            );
        }
        QCOMPARE(advancedIds, QStringList({
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
        }));
        const auto lockDelay = client.advancedOptions().at(1).toMap();
        QCOMPARE(lockDelay.value(QStringLiteral("defaultValue")).toInt(), 1000);
        QCOMPARE(lockDelay.value(QStringLiteral("min")).toInt(), 0);
        QCOMPARE(lockDelay.value(QStringLiteral("max")).toInt(), 5000);
        const auto unfocusedFps = client.advancedOptions().at(3).toMap();
        QCOMPARE(unfocusedFps.value(QStringLiteral("defaultValue")).toInt(), 15);
        QCOMPARE(unfocusedFps.value(QStringLiteral("min")).toInt(), 1);
        QCOMPARE(unfocusedFps.value(QStringLiteral("max")).toInt(), 120);
        for (const auto index : {
                 qsizetype(5), qsizetype(6), qsizetype(7), qsizetype(8),
             }) {
            const auto option = client.advancedOptions().at(index).toMap();
            QCOMPARE(option.value(QStringLiteral("type")).toString(),
                     QStringLiteral("boolean"));
            QCOMPARE(option.value(QStringLiteral("control")).toString(),
                     QStringLiteral("toggle"));
            QCOMPARE(option.value(QStringLiteral("defaultValue")).toBool(),
                     false);
            QVERIFY(!option.contains(QStringLiteral("min")));
            QVERIFY(!option.contains(QStringLiteral("max")));
        }
        for (qsizetype index = 0; index < 9; ++index) {
            QCOMPARE(
                client.advancedOptions().at(index).toMap().value(
                    QStringLiteral("risk")
                ).toString(),
                QStringLiteral("safe")
            );
        }
        const auto nearestNeighbor = client.advancedOptions().at(9).toMap();
        QCOMPARE(nearestNeighbor.value(QStringLiteral("type")).toString(),
                 QStringLiteral("boolean"));
        QCOMPARE(nearestNeighbor.value(QStringLiteral("control")).toString(),
                 QStringLiteral("toggle"));
        QCOMPARE(nearestNeighbor.value(QStringLiteral("defaultValue")).toBool(),
                 true);
        QCOMPARE(nearestNeighbor.value(QStringLiteral("risk")).toString(),
                 QStringLiteral("caution"));
        QVERIFY(!nearestNeighbor.contains(QStringLiteral("min")));
        QVERIFY(!nearestNeighbor.contains(QStringLiteral("max")));
        const auto expandUndersizedTextures =
            client.advancedOptions().at(10).toMap();
        QCOMPARE(
            expandUndersizedTextures.value(QStringLiteral("type")).toString(),
            QStringLiteral("boolean")
        );
        QCOMPARE(
            expandUndersizedTextures.value(QStringLiteral("control")).toString(),
            QStringLiteral("toggle")
        );
        QCOMPARE(
            expandUndersizedTextures.value(
                QStringLiteral("defaultValue")
            ).toBool(),
            true
        );
        QCOMPARE(
            expandUndersizedTextures.value(QStringLiteral("risk")).toString(),
            QStringLiteral("caution")
        );
        QVERIFY(!expandUndersizedTextures.contains(QStringLiteral("min")));
        QVERIFY(!expandUndersizedTextures.contains(QStringLiteral("max")));
        const auto directScanout = client.advancedOptions().at(11).toMap();
        QCOMPARE(directScanout.value(QStringLiteral("type")).toString(),
                 QStringLiteral("enum"));
        QCOMPARE(directScanout.value(QStringLiteral("control")).toString(),
                 QStringLiteral("select"));
        QCOMPARE(directScanout.value(QStringLiteral("defaultValue")).toInt(),
                 0);
        QCOMPARE(directScanout.value(QStringLiteral("risk")).toString(),
                 QStringLiteral("caution"));
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
        const auto fp16SdrTf = client.advancedOptions().at(12).toMap();
        QCOMPARE(fp16SdrTf.value(QStringLiteral("type")).toString(),
                 QStringLiteral("enum"));
        QCOMPARE(fp16SdrTf.value(QStringLiteral("control")).toString(),
                 QStringLiteral("select"));
        QCOMPARE(fp16SdrTf.value(QStringLiteral("defaultValue")).toInt(), 0);
        QCOMPARE(fp16SdrTf.value(QStringLiteral("risk")).toString(),
                 QStringLiteral("caution"));
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
        const auto xpMode = client.advancedOptions().at(13).toMap();
        QCOMPARE(xpMode.value(QStringLiteral("type")).toString(),
                 QStringLiteral("boolean"));
        QCOMPARE(xpMode.value(QStringLiteral("control")).toString(),
                 QStringLiteral("toggle"));
        QCOMPARE(xpMode.value(QStringLiteral("defaultValue")).toBool(), false);
        QCOMPARE(xpMode.value(QStringLiteral("risk")).toString(),
                 QStringLiteral("caution"));
        QVERIFY(!xpMode.contains(QStringLiteral("min")));
        QVERIFY(!xpMode.contains(QStringLiteral("max")));
        const auto captureModifiers = client.advancedOptions().at(14).toMap();
        QCOMPARE(captureModifiers.value(QStringLiteral("type")).toString(),
                 QStringLiteral("boolean"));
        QCOMPARE(captureModifiers.value(QStringLiteral("control")).toString(),
                 QStringLiteral("toggle"));
        QCOMPARE(
            captureModifiers.value(QStringLiteral("defaultValue")).toBool(),
            false
        );
        QCOMPARE(captureModifiers.value(QStringLiteral("risk")).toString(),
                 QStringLiteral("caution"));
        QVERIFY(!captureModifiers.contains(QStringLiteral("min")));
        QVERIFY(!captureModifiers.contains(QStringLiteral("max")));
        const auto enforceBarriers = client.advancedOptions().at(15).toMap();
        QCOMPARE(enforceBarriers.value(QStringLiteral("type")).toString(),
                 QStringLiteral("boolean"));
        QCOMPARE(enforceBarriers.value(QStringLiteral("control")).toString(),
                 QStringLiteral("toggle"));
        QCOMPARE(
            enforceBarriers.value(QStringLiteral("defaultValue")).toBool(),
            true
        );
        QCOMPARE(enforceBarriers.value(QStringLiteral("risk")).toString(),
                 QStringLiteral("caution"));
        QVERIFY(!enforceBarriers.contains(QStringLiteral("min")));
        QVERIFY(!enforceBarriers.contains(QStringLiteral("max")));
        QTRY_VERIFY_WITH_TIMEOUT(client.rulesAvailable(), 3000);
        QCOMPARE(client.rulesProjectionAvailable(), true);
        QVERIFY(client.windowRules().isEmpty());
        QVERIFY(client.layerRules().isEmpty());
        QVERIFY(client.rulesErrorName().isEmpty());
        QCOMPARE(
            client.appearanceValues().value(
                QStringLiteral("hyprland.general.border_size")
            ).toInt(),
            1
        );
        QCOMPARE(
            client.inputValues().value(
                QStringLiteral("hyprland.input.repeat_rate")
            ).toInt(),
            25
        );
        QCOMPARE(
            client.inputValues().value(
                QStringLiteral("hyprland.input.repeat_delay")
            ).toInt(),
            600
        );
        QCOMPARE(
            client.inputValues().value(
                QStringLiteral("hyprland.input.sensitivity")
            ).toDouble(),
            0.0
        );
        QCOMPARE(
            client.inputValues().value(
                QStringLiteral("hyprland.input.accel_profile")
            ).toString(),
            QString{}
        );
        QCOMPARE(client.inputValues(), inputDefaults());
        QCOMPARE(client.windowsValues(), windowsDefaults());
        QCOMPARE(client.workspacesValues(), workspacesDefaults());
        QCOMPARE(client.advancedValues(), advancedDefaults());
        QCOMPARE(
            client.sharedBorderSyncState(),
            QStringLiteral("current")
        );
        QCOMPARE(
            client.sharedBorderSourceRevision(),
            initialSharedBorderSourceRevision
        );
        QCOMPARE(
            client.sharedBorderSourceRevisionToken(),
            QStringLiteral("17")
        );
        QVERIFY(client.sharedBorderSyncError().isEmpty());
        QCOMPARE(
            client.sharedSpacingSyncState(),
            QStringLiteral("override")
        );
        QCOMPARE(
            client.sharedSpacingSourceRevision(),
            initialSharedSpacingSourceRevision
        );
        QCOMPARE(
            client.sharedSpacingSourceRevisionToken(),
            QStringLiteral("23")
        );
        QVERIFY(client.sharedSpacingSyncError().isEmpty());
        QVERIFY(client.appearanceErrorName().isEmpty());
        QVERIFY(client.inputErrorName().isEmpty());
        QVERIFY(client.windowsErrorName().isEmpty());
        QVERIFY(client.workspacesErrorName().isEmpty());
        QVERIFY(client.advancedErrorName().isEmpty());
        QVERIFY(client.lastErrorName().isEmpty());
    }

    void projectsExactComplexCollectionsAndCompleteBindingActions()
    {
        QVERIFY(service_.start());

        HyprShelld::CompositorClient client(bus_, nullptr);
        QTRY_VERIFY_WITH_TIMEOUT(client.bindingsAvailable(), 3000);
        QTRY_VERIFY_WITH_TIMEOUT(client.environmentAvailable(), 3000);
        QTRY_VERIFY_WITH_TIMEOUT(client.permissionsAvailable(), 3000);
        QTRY_VERIFY_WITH_TIMEOUT(client.inputDevicesAvailable(), 3000);
        QCOMPARE(client.actionCatalogAvailable(), true);
        QCOMPARE(client.bindingsProjectionAvailable(), true);
        QCOMPARE(client.environmentProjectionAvailable(), true);
        QCOMPARE(client.permissionsProjectionAvailable(), true);
        QCOMPARE(client.inputDevicesProjectionAvailable(), true);
        QVERIFY(client.bindings().isEmpty());
        QVERIFY(client.submaps().isEmpty());
        QVERIFY(client.environmentVariables().isEmpty());
        QVERIFY(client.permissions().isEmpty());
        QVERIFY(client.inputDevices().isEmpty());
        QVERIFY(client.bindingsErrorName().isEmpty());
        QVERIFY(client.environmentErrorName().isEmpty());
        QVERIFY(client.permissionErrorName().isEmpty());
        QVERIFY(client.inputDevicesErrorName().isEmpty());

        const auto defaultBindings = client.defaultBindings();
        QCOMPARE(
            defaultBindings.size(),
            HyprShelld::Hyprland::shippedDefaultKeybindingCount
        );
        QSet<QString> defaultBindingIds;
        const auto &typedDefaults =
            HyprShelld::Hyprland::shippedDefaultKeybindings();
        for (qsizetype index = 0; index < defaultBindings.size(); ++index) {
            const auto binding = defaultBindings.at(index).toMap();
            const auto &typed = typedDefaults.at(index);
            const auto id = binding.value(QStringLiteral("id")).toString();
            QVERIFY(id.startsWith(QStringLiteral("hyprshelld.default.")));
            QVERIFY(!defaultBindingIds.contains(id));
            defaultBindingIds.insert(id);
            QCOMPARE(id, typed.id);
            QCOMPARE(
                binding.value(QStringLiteral("modifiers")).toStringList(),
                typed.modifiers
            );
            QCOMPARE(
                binding.value(QStringLiteral("key")).toString(), typed.key
            );
            QCOMPARE(
                binding.value(QStringLiteral("actionType")).toString(),
                QStringLiteral("dispatcher")
            );
            QCOMPARE(
                binding.value(QStringLiteral("action")).toString(),
                typed.action
            );
            QCOMPARE(
                binding.value(QStringLiteral("arguments")).toMap(),
                typed.arguments.toVariantMap()
            );
            QCOMPARE(
                binding.value(QStringLiteral("description")).toString(),
                typed.description
            );
            QCOMPARE(
                binding.value(QStringLiteral("enabled")).toBool(),
                typed.enabled
            );
            QCOMPARE(
                binding.value(QStringLiteral("submap")).toString(),
                typed.submap
            );
            const auto options =
                binding.value(QStringLiteral("options")).toMap();
            QCOMPARE(options.size(), 13);
            QCOMPARE(options.value(QStringLiteral("repeating")).toBool(),
                     typed.options.repeating);
            QCOMPARE(options.value(QStringLiteral("locked")).toBool(),
                     typed.options.locked);
            QCOMPARE(options.value(QStringLiteral("release")).toBool(),
                     typed.options.release);
            QCOMPARE(options.value(QStringLiteral("nonConsuming")).toBool(),
                     typed.options.nonConsuming);
            QCOMPARE(options.value(QStringLiteral("autoConsuming")).toBool(),
                     typed.options.autoConsuming);
            QCOMPARE(options.value(QStringLiteral("transparent")).toBool(),
                     typed.options.transparent);
            QCOMPARE(options.value(QStringLiteral("ignoreMods")).toBool(),
                     typed.options.ignoreMods);
            QCOMPARE(options.value(QStringLiteral("dontInhibit")).toBool(),
                     typed.options.dontInhibit);
            QCOMPARE(options.value(QStringLiteral("longPress")).toBool(),
                     typed.options.longPress);
            QCOMPARE(options.value(QStringLiteral("submapUniversal")).toBool(),
                     typed.options.submapUniversal);
            QCOMPARE(options.value(QStringLiteral("click")).toBool(),
                     typed.options.click);
            QCOMPARE(options.value(QStringLiteral("drag")).toBool(),
                     typed.options.drag);
            QCOMPARE(options.value(QStringLiteral("allowInputCapture")).toBool(),
                     typed.options.allowInputCapture);
        }

        const auto actions = client.bindingActions();
        QCOMPARE(actions.size(), 76);
        qsizetype dispatcherCount = 0;
        qsizetype defaultAppCount = 0;
        qsizetype hyprShelldCount = 0;
        QSet<QString> actionIdentities;
        QVariantMap noOp;
        for (const auto &value : actions) {
            const auto action = value.toMap();
            const auto type = action.value(
                QStringLiteral("actionType")
            ).toString();
            const auto id = action.value(QStringLiteral("id")).toString();
            QVERIFY(!id.isEmpty());
            QVERIFY(!action.value(QStringLiteral("label")).toString().isEmpty());
            QVERIFY(!action.value(
                QStringLiteral("description")
            ).toString().isEmpty());
            QVERIFY(action.contains(QStringLiteral("luaPath")));
            QVERIFY(action.contains(QStringLiteral("uiTier")));
            QVERIFY(action.contains(QStringLiteral("risk")));
            QVERIFY(action.contains(QStringLiteral("schemaReference")));
            QVERIFY(action.contains(QStringLiteral("documentation")));
            const auto identity = type + QLatin1Char(':') + id;
            QVERIFY(!actionIdentities.contains(identity));
            actionIdentities.insert(identity);
            if (type == QStringLiteral("dispatcher")) {
                ++dispatcherCount;
            } else if (type == QStringLiteral("defaultApp")) {
                ++defaultAppCount;
            } else if (type == QStringLiteral("hyprshelld")) {
                ++hyprShelldCount;
            } else {
                QFAIL("Binding action projection exposed an unknown action type");
            }
            if (type == QStringLiteral("dispatcher")
                && id == QStringLiteral("no_op")) {
                noOp = action;
            }
        }
        QCOMPARE(dispatcherCount, qsizetype(47));
        QCOMPARE(defaultAppCount, qsizetype(12));
        QCOMPARE(hyprShelldCount, qsizetype(17));
        QCOMPARE(actionIdentities.size(), 76);
        QCOMPARE(noOp.value(QStringLiteral("label")).toString(),
                 QStringLiteral("No Op"));
        QCOMPARE(noOp.value(QStringLiteral("kind")).toString(),
                 QStringLiteral("dispatcher"));
    }

    void savesComplexCollectionsAndPreservesEveryUnrelatedSurface()
    {
        auto initial = QJsonDocument::fromJson(snapshotWithOverride(
            QStringLiteral("hyprland.general.border_size"), 7
        )).object();
        service_.setSnapshotBytes(
            HyprShelld::Hyprland::JsonSupport::canonicalJson(initial)
                + QByteArray("\n")
        );
        QVERIFY(service_.start());

        HyprShelld::CompositorClient client(bus_, nullptr);
        QTRY_VERIFY_WITH_TIMEOUT(client.bindingsAvailable(), 3000);
        QStringList operations;
        connect(
            &client,
            &HyprShelld::CompositorClient::busyOperationChanged,
            &client,
            [&client, &operations]() {
                if (!client.busyOperation().isEmpty()) {
                    operations.append(client.busyOperation());
                }
            }
        );

        const QVariantList nextBindings{bindingRecord()};
        const QVariantList nextSubmaps{submapRecord()};
        const QVariantList nextEnvironment{environmentRecord()};
        const QVariantList nextPermissions{permissionRecord()};
        const QVariantList nextDevices{inputDeviceRecord()};

        client.saveBindings(nextBindings, nextSubmaps);
        QTRY_VERIFY_WITH_TIMEOUT(
            client.revision() == baselineRevision + 1
                || !client.bindingsErrorName().isEmpty(),
            3000
        );
        QVERIFY2(
            client.revision() == baselineRevision + 1,
            qPrintable(QStringLiteral("%1: %2")
                .arg(client.bindingsErrorName(),
                     client.bindingsErrorMessage()))
        );
        QTRY_VERIFY_WITH_TIMEOUT(client.bindingsAvailable(), 3000);
        QCOMPARE(client.bindings(), nextBindings);
        QCOMPARE(client.submaps(), nextSubmaps);
        QCOMPARE(service_.replaceCallCount(), 1);
        QCOMPARE(service_.applyCallCount(), 1);

        client.saveEnvironment(nextEnvironment);
        QTRY_COMPARE_WITH_TIMEOUT(
            client.revision(), baselineRevision + 2, 3000
        );
        QTRY_VERIFY_WITH_TIMEOUT(client.environmentAvailable(), 3000);
        QCOMPARE(client.environmentVariables(), nextEnvironment);
        QCOMPARE(client.bindings(), nextBindings);
        QCOMPARE(client.submaps(), nextSubmaps);

        client.savePermissions(nextPermissions);
        QTRY_COMPARE_WITH_TIMEOUT(
            client.revision(), baselineRevision + 3, 3000
        );
        QTRY_VERIFY_WITH_TIMEOUT(client.permissionsAvailable(), 3000);
        QCOMPARE(client.permissions(), nextPermissions);
        QCOMPARE(client.environmentVariables(), nextEnvironment);

        client.saveInputDevices(nextDevices);
        QTRY_COMPARE_WITH_TIMEOUT(
            client.revision(), baselineRevision + 4, 3000
        );
        QTRY_VERIFY_WITH_TIMEOUT(client.inputDevicesAvailable(), 3000);
        QCOMPARE(client.inputDevices(), nextDevices);
        QCOMPARE(client.bindings(), nextBindings);
        QCOMPARE(client.submaps(), nextSubmaps);
        QCOMPARE(client.environmentVariables(), nextEnvironment);
        QCOMPARE(client.permissions(), nextPermissions);
        QCOMPARE(service_.replaceCallCount(), 4);
        QCOMPARE(service_.applyCallCount(), 4);
        QCOMPARE(client.inputDeviceProjectionAvailable(), true);
        QCOMPARE(client.savedInputDevices().size(), qsizetype(1));
        QCOMPARE(client.otherSavedInputDevices().size(), qsizetype(1));

        QVERIFY(operations.contains(QStringLiteral("bindings-save")));
        QVERIFY(operations.contains(QStringLiteral("bindings-apply")));
        QVERIFY(operations.contains(QStringLiteral("environment-save")));
        QVERIFY(operations.contains(QStringLiteral("environment-apply")));
        QVERIFY(operations.contains(QStringLiteral("permissions-save")));
        QVERIFY(operations.contains(QStringLiteral("permissions-apply")));
        QVERIFY(operations.contains(QStringLiteral("input-devices-save")));
        QVERIFY(operations.contains(QStringLiteral("input-devices-apply")));

        const auto saved = QJsonDocument::fromJson(
            service_.currentSnapshotBytes()
        ).object();
        QCOMPARE(
            saved.value(QStringLiteral("bindings")).toArray(),
            QJsonArray::fromVariantList(nextBindings)
        );
        QCOMPARE(
            saved.value(QStringLiteral("submaps")).toArray(),
            QJsonArray::fromVariantList(nextSubmaps)
        );
        QCOMPARE(
            saved.value(QStringLiteral("environment")).toArray(),
            QJsonArray::fromVariantList(nextEnvironment)
        );
        QCOMPARE(
            saved.value(QStringLiteral("permissions")).toArray(),
            QJsonArray::fromVariantList(nextPermissions)
        );
        QCOMPARE(
            saved.value(QStringLiteral("devices")).toArray(),
            QJsonArray::fromVariantList(nextDevices)
        );
        for (const auto &key : initial.keys()) {
            if (key == QStringLiteral("revision")
                || key == QStringLiteral("bindings")
                || key == QStringLiteral("submaps")
                || key == QStringLiteral("environment")
                || key == QStringLiteral("permissions")
                || key == QStringLiteral("devices")) {
                continue;
            }
            QCOMPARE(saved.value(key), initial.value(key));
        }
        QCOMPARE(
            saved.value(QStringLiteral("overrides")).toObject().value(
                QStringLiteral("hyprland.general.border_size")
            ).toInt(),
            7
        );
    }

    void rejectsMalformedComplexRecordsBeforeCallingTheService()
    {
        QVERIFY(service_.start());
        HyprShelld::CompositorClient client(bus_, nullptr);
        QTRY_VERIFY_WITH_TIMEOUT(client.bindingsAvailable(), 3000);

        auto invalidBinding = bindingRecord();
        invalidBinding.insert(
            QStringLiteral("arguments"),
            QVariantMap{{QStringLiteral("unexpected"), true}}
        );
        client.saveBindings({invalidBinding}, {submapRecord()});
        QCOMPARE(
            client.bindingsErrorName(),
            QStringLiteral(
                "org.hyprshelld.Client.Compositor.Error.InvalidBindings"
            )
        );
        QVERIFY(client.bindingsErrorMessage().contains(
            QStringLiteral("$.bindings")
        ));

        auto invalidEnvironment = environmentRecord();
        invalidEnvironment.insert(
            QStringLiteral("name"), QStringLiteral("1INVALID")
        );
        client.saveEnvironment({invalidEnvironment});
        QCOMPARE(
            client.environmentErrorName(),
            QStringLiteral(
                "org.hyprshelld.Client.Compositor.Error.InvalidEnvironment"
            )
        );
        QVERIFY(client.environmentErrorMessage().contains(
            QStringLiteral("$.environment")
        ));

        auto invalidPermission = permissionRecord();
        invalidPermission.insert(
            QStringLiteral("mode"), QStringLiteral("sometimes")
        );
        client.savePermissions({invalidPermission});
        QCOMPARE(
            client.permissionErrorName(),
            QStringLiteral(
                "org.hyprshelld.Client.Compositor.Error.InvalidPermissions"
            )
        );
        QVERIFY(client.permissionErrorMessage().contains(
            QStringLiteral("$.permissions")
        ));

        auto invalidDevice = inputDeviceRecord();
        invalidDevice.insert(
            QStringLiteral("kind"), QStringLiteral("future-device")
        );
        client.saveInputDevices({invalidDevice});
        QCOMPARE(
            client.inputDevicesErrorName(),
            QStringLiteral(
                "org.hyprshelld.Client.Compositor.Error.InvalidInputDevices"
            )
        );
        QVERIFY(client.inputDevicesErrorMessage().contains(
            QStringLiteral("$.devices")
        ));

        QCOMPARE(service_.replaceCallCount(), 0);
        QCOMPARE(service_.applyCallCount(), 0);
        QCOMPARE(client.revision(), baselineRevision);
        QVERIFY(client.bindings().isEmpty());
        QVERIFY(client.submaps().isEmpty());
        QVERIFY(client.environmentVariables().isEmpty());
        QVERIFY(client.permissions().isEmpty());
        QVERIFY(client.inputDevices().isEmpty());
        QCOMPARE(client.bindingsAvailable(), true);
        QCOMPARE(client.environmentAvailable(), true);
        QCOMPARE(client.permissionsAvailable(), true);
        QCOMPARE(client.inputDevicesAvailable(), true);
    }

    void rejectsMalformedComplexProjections_data()
    {
        QTest::addColumn<QString>("field");
        QTest::addColumn<QJsonArray>("records");
        QTest::addColumn<QString>("errorSuffix");

        QTest::newRow("bindings")
            << QStringLiteral("bindings")
            << QJsonArray{QJsonObject{{QStringLiteral("unknown"), true}}}
            << QStringLiteral("InvalidBindingsSnapshot");

        auto environment = QJsonObject::fromVariantMap(environmentRecord());
        environment.insert(
            QStringLiteral("scope"), QStringLiteral("process")
        );
        QTest::newRow("environment")
            << QStringLiteral("environment")
            << QJsonArray{environment}
            << QStringLiteral("InvalidEnvironmentSnapshot");

        auto permission = QJsonObject::fromVariantMap(permissionRecord());
        permission.insert(QStringLiteral("mode"), QStringLiteral("audit"));
        QTest::newRow("permissions")
            << QStringLiteral("permissions")
            << QJsonArray{permission}
            << QStringLiteral("InvalidPermissionsSnapshot");

        auto device = QJsonObject::fromVariantMap(inputDeviceRecord());
        device.insert(QStringLiteral("kind"), QStringLiteral("future-device"));
        QTest::newRow("devices")
            << QStringLiteral("devices")
            << QJsonArray{device}
            << QStringLiteral("InvalidInputDevicesSnapshot");
    }

    void rejectsMalformedComplexProjections()
    {
        QFETCH(QString, field);
        QFETCH(QJsonArray, records);
        QFETCH(QString, errorSuffix);
        service_.setSnapshotBytes(snapshotWithCollection(field, records));
        QVERIFY(service_.start());

        HyprShelld::CompositorClient client(bus_, nullptr);
        QTRY_VERIFY_WITH_TIMEOUT(client.available(), 3000);
        QTRY_VERIFY_WITH_TIMEOUT(client.actionCatalogAvailable(), 3000);
        const auto expectedError = QStringLiteral(
            "org.hyprshelld.Client.Compositor.Error."
        ) + errorSuffix;
        if (field == QStringLiteral("bindings")) {
            QTRY_COMPARE_WITH_TIMEOUT(
                client.bindingsErrorName(), expectedError, 3000
            );
        } else if (field == QStringLiteral("environment")) {
            QTRY_COMPARE_WITH_TIMEOUT(
                client.environmentErrorName(), expectedError, 3000
            );
        } else if (field == QStringLiteral("permissions")) {
            QTRY_COMPARE_WITH_TIMEOUT(
                client.permissionErrorName(), expectedError, 3000
            );
        } else {
            QTRY_COMPARE_WITH_TIMEOUT(
                client.inputDevicesErrorName(), expectedError, 3000
            );
        }

        QCOMPARE(client.bindingActions().size(), 76);
        QCOMPARE(client.bindingsProjectionAvailable(), false);
        QCOMPARE(client.environmentProjectionAvailable(), false);
        QCOMPARE(client.permissionsProjectionAvailable(), false);
        QCOMPARE(client.inputDevicesProjectionAvailable(), false);
        QCOMPARE(client.bindingsAvailable(), false);
        QCOMPARE(client.environmentAvailable(), false);
        QCOMPARE(client.permissionsAvailable(), false);
        QCOMPARE(client.inputDevicesAvailable(), false);
        QVERIFY(client.bindings().isEmpty());
        QVERIFY(client.submaps().isEmpty());
        QVERIFY(client.environmentVariables().isEmpty());
        QVERIFY(client.permissions().isEmpty());
        QVERIFY(client.inputDevices().isEmpty());
        QCOMPARE(service_.replaceCallCount(), 0);
        QCOMPARE(service_.applyCallCount(), 0);
    }

    void discoversInputDevicesBeforeSavedAuthorityAndRefreshesIndependently()
    {
        const auto savedSnapshot = snapshotWithInputDevices({
            savedInputDevice(
                QStringLiteral("keyboard-main"),
                QStringLiteral("Board Alpha"),
                QStringLiteral("keyboard")
            ),
        });
        service_.setSnapshotBytes(savedSnapshot);
        service_.setHoldSnapshots(true);
        service_.setInputDeviceReply(
            inputDeviceInventoryBytes({
                connectedInputDevice(
                    QStringLiteral("Board-Alpha"),
                    QStringLiteral("keyboard"),
                    QStringLiteral("English (US)")
                ),
            }),
            1'800'000'000'101ULL
        );
        QVERIFY(service_.start());

        HyprShelld::CompositorClient client(bus_, nullptr);
        QTRY_COMPARE_WITH_TIMEOUT(service_.heldSnapshotCount(), 1, 3000);
        QTRY_VERIFY_WITH_TIMEOUT(
            client.inputDeviceDiscoveryAvailable(), 3000
        );
        QCOMPARE(client.available(), false);
        QCOMPARE(client.inputDeviceProjectionAvailable(), false);
        QCOMPARE(client.inputDeviceInventoryDigest(), QString(64, 'a'));
        QCOMPARE(client.inputDevicesObservedAtMs(), 1'800'000'000'101ULL);
        QCOMPARE(client.connectedInputDevices().size(), 1);
        QCOMPARE(
            client.connectedInputDevices().constFirst().toMap().value(
                QStringLiteral("savedSettingsState")
            ).toString(),
            QStringLiteral("unavailable")
        );

        QVERIFY(service_.releaseNextSnapshot({
            savedSnapshot,
            QVariant::fromValue<qulonglong>(baselineRevision),
            catalogDigest,
            actionCatalogDigest,
        }));
        QTRY_VERIFY_WITH_TIMEOUT(client.inputAvailable(), 3000);
        QTRY_VERIFY_WITH_TIMEOUT(
            client.inputDeviceProjectionAvailable(), 3000
        );
        QCOMPARE(client.inputValues(), inputDefaults());
        QCOMPARE(client.savedInputDevices().size(), 1);
        QCOMPARE(
            client.savedInputDevices().constFirst().toMap().value(
                QStringLiteral("matchState")
            ).toString(),
            QStringLiteral("observed")
        );
        QCOMPARE(
            client.inputDeviceProjectionInventoryDigest(),
            QString(64, 'a')
        );

        const auto snapshotCalls = service_.snapshotCallCount();
        const auto deviceCalls = service_.inputDeviceCallCount();
        service_.setInputDeviceReply(
            inputDeviceInventoryBytes({
                connectedInputDevice(
                    QStringLiteral("Board-Alpha"),
                    QStringLiteral("keyboard"),
                    QStringLiteral("English (US)")
                ),
                connectedInputDevice(
                    QStringLiteral("Mouse-Beta"),
                    QStringLiteral("pointer")
                ),
            }, QLatin1Char('b')),
            1'800'000'000'102ULL
        );
        client.refreshConnectedInputDevices();
        QCOMPARE(client.inputDeviceDiscoveryBusy(), true);
        QTRY_COMPARE_WITH_TIMEOUT(
            client.inputDeviceInventoryDigest(), QString(64, 'b'), 3000
        );
        QCOMPARE(service_.inputDeviceCallCount(), deviceCalls + 1);
        QCOMPARE(service_.snapshotCallCount(), snapshotCalls);
        QCOMPARE(client.inputDevicesObservedAtMs(), 1'800'000'000'102ULL);
        QCOMPARE(client.connectedInputDevices().size(), 2);
        QCOMPARE(client.inputValues(), inputDefaults());
        QCOMPARE(client.inputDeviceProjectionRevisionToken(),
                 QStringLiteral("7"));
    }

    void classifiesInputDevicesByExactSessionSelectorInSavedOrder()
    {
        service_.setSnapshotBytes(snapshotWithInputDevices({
            savedInputDevice(
                QStringLiteral("offline-pointer"),
                QStringLiteral("Offline Device"),
                QStringLiteral("pointer"),
                false
            ),
            savedInputDevice(
                QStringLiteral("keyboard-main"),
                QStringLiteral("Board Alpha"),
                QStringLiteral("keyboard")
            ),
            savedInputDevice(
                QStringLiteral("touchpad-main"),
                QStringLiteral("Touch Pad"),
                QStringLiteral("touchpad")
            ),
            savedInputDevice(
                QStringLiteral("case-sensitive-pointer"),
                QStringLiteral("Mouse Exact"),
                QStringLiteral("pointer"),
                true,
                QJsonObject{{QStringLiteral("sensitivity"), 0.25}}
            ),
            savedInputDevice(
                QStringLiteral("kind-mismatch"),
                QStringLiteral("Shared Selector"),
                QStringLiteral("touch")
            ),
            savedInputDevice(
                QStringLiteral("unobservable-switch"),
                QStringLiteral("Switch One"),
                QStringLiteral("switch")
            ),
        }));
        service_.setInputDeviceReply(
            inputDeviceInventoryBytes({
                connectedInputDevice(
                    QStringLiteral("Board-Alpha"),
                    QStringLiteral("keyboard"),
                    QStringLiteral("English (US)")
                ),
                connectedInputDevice(
                    QStringLiteral("Shared-Selector"),
                    QStringLiteral("pointer")
                ),
                connectedInputDevice(
                    QStringLiteral("Touch-Pad"),
                    QStringLiteral("pointer")
                ),
                connectedInputDevice(
                    QStringLiteral("mouse-Exact"),
                    QStringLiteral("pointer")
                ),
                connectedInputDevice(
                    QStringLiteral("Tablet-One"),
                    QStringLiteral("tablet")
                ),
            }, QLatin1Char('c'), 2, 3, 4),
            1'800'000'000'103ULL
        );
        QVERIFY(service_.start());

        HyprShelld::CompositorClient client(bus_, nullptr);
        QTRY_VERIFY_WITH_TIMEOUT(
            client.inputDeviceProjectionAvailable(), 3000
        );
        QTRY_VERIFY_WITH_TIMEOUT(
            client.inputDeviceDiscoveryAvailable(), 3000
        );

        const auto saved = client.savedInputDevices();
        QCOMPARE(saved.size(), 6);
        QStringList savedIds;
        QStringList matchStates;
        for (const auto &value : saved) {
            const auto row = value.toMap();
            savedIds.append(row.value(QStringLiteral("id")).toString());
            matchStates.append(
                row.value(QStringLiteral("matchState")).toString()
            );
            QVERIFY(!row.contains(QStringLiteral("overrides")));
        }
        QCOMPARE(savedIds, QStringList({
            QStringLiteral("offline-pointer"),
            QStringLiteral("keyboard-main"),
            QStringLiteral("touchpad-main"),
            QStringLiteral("case-sensitive-pointer"),
            QStringLiteral("kind-mismatch"),
            QStringLiteral("unobservable-switch"),
        }));
        QCOMPARE(matchStates, QStringList({
            QStringLiteral("not-observed"),
            QStringLiteral("observed"),
            QStringLiteral("observed"),
            QStringLiteral("not-observed"),
            QStringLiteral("kind-mismatch"),
            QStringLiteral("unobservable"),
        }));
        QCOMPARE(
            saved.at(0).toMap().value(
                QStringLiteral("configuredEnabled")
            ).toBool(),
            false
        );
        QCOMPARE(
            saved.at(3).toMap().value(QStringLiteral("overrideCount")).toInt(),
            1
        );
        QCOMPARE(
            saved.at(4).toMap().value(QStringLiteral("observedKind")).toString(),
            QStringLiteral("pointer")
        );

        const auto other = client.otherSavedInputDevices();
        QCOMPARE(other.size(), 4);
        QStringList otherIds;
        for (const auto &value : other) {
            otherIds.append(
                value.toMap().value(QStringLiteral("id")).toString()
            );
        }
        QCOMPARE(otherIds, QStringList({
            QStringLiteral("offline-pointer"),
            QStringLiteral("case-sensitive-pointer"),
            QStringLiteral("kind-mismatch"),
            QStringLiteral("unobservable-switch"),
        }));

        const auto connected = client.connectedInputDevices();
        QCOMPARE(connected.size(), 5);
        QCOMPARE(
            connected.at(0).toMap().value(
                QStringLiteral("savedSettingsState")
            ).toString(),
            QStringLiteral("matched")
        );
        QCOMPARE(
            connected.at(0).toMap().value(
                QStringLiteral("activeKeymap")
            ).toString(),
            QStringLiteral("English (US)")
        );
        QCOMPARE(
            connected.at(1).toMap().value(
                QStringLiteral("savedSettingsState")
            ).toString(),
            QStringLiteral("kind-mismatch")
        );
        QCOMPARE(
            connected.at(2).toMap().value(
                QStringLiteral("savedSettingsState")
            ).toString(),
            QStringLiteral("matched")
        );
        QCOMPARE(
            connected.at(3).toMap().value(
                QStringLiteral("savedSettingsState")
            ).toString(),
            QStringLiteral("not-saved")
        );
        QVERIFY(!connected.at(3).toMap().value(
            QStringLiteral("savedDeviceId")
        ).isValid());
        QCOMPARE(
            connected.at(4).toMap().value(
                QStringLiteral("savedSettingsState")
            ).toString(),
            QStringLiteral("not-saved")
        );
        QCOMPARE(
            client.inputDeviceUnaddressableCounts().value(
                QStringLiteral("switches")
            ).toUInt(),
            quint32(2)
        );
        QCOMPARE(
            client.inputDeviceUnaddressableCounts().value(
                QStringLiteral("tabletPads")
            ).toUInt(),
            quint32(3)
        );
        QCOMPARE(
            client.inputDeviceUnaddressableCounts().value(
                QStringLiteral("tabletTools")
            ).toUInt(),
            quint32(4)
        );
    }

    void rejectsUntrustedInputDeviceInventoryDocuments_data()
    {
        QTest::addColumn<QByteArray>("document");

        auto missingNewline = inputDeviceInventoryBytes();
        missingNewline.chop(1);
        QTest::newRow("noncanonical-missing-newline") << missingNewline;

        auto duplicateKey = inputDeviceInventoryBytes();
        duplicateKey.replace(
            QByteArrayLiteral("\"formatVersion\":1"),
            QByteArrayLiteral(
                "\"formatVersion\":1,\"formatVersion\":1"
            )
        );
        QTest::newRow("duplicate-root-key") << duplicateKey;

        auto extraFieldObject = QJsonDocument::fromJson(
            inputDeviceInventoryBytes()
        ).object();
        extraFieldObject.insert(QStringLiteral("privateAddress"),
                                QStringLiteral("0x1234"));
        auto extraField = HyprShelld::Hyprland::JsonSupport::canonicalJson(
            extraFieldObject
        );
        extraField.append('\n');
        QTest::newRow("unexpected-private-field") << extraField;

        QTest::newRow("pointer-keymap") << inputDeviceInventoryBytes({
            connectedInputDevice(
                QStringLiteral("Pointer-One"),
                QStringLiteral("pointer"),
                QStringLiteral("Not permitted")
            ),
        });
    }

    void rejectsUntrustedInputDeviceInventoryDocuments()
    {
        QFETCH(QByteArray, document);
        service_.setInputDeviceReply(
            std::move(document), 1'800'000'000'104ULL
        );
        QVERIFY(service_.start());

        HyprShelld::CompositorClient client(bus_, nullptr);
        QTRY_VERIFY_WITH_TIMEOUT(client.inputProjectionAvailable(), 3000);
        QTRY_VERIFY_WITH_TIMEOUT(
            service_.inputDeviceCallCount() > 0
                && !client.inputDeviceDiscoveryBusy(),
            3000
        );
        QCOMPARE(client.inputDeviceDiscoveryAvailable(), false);
        QCOMPARE(client.connectedInputDevices(), QVariantList{});
        QCOMPARE(client.inputDevicesObservedAtMs(), qulonglong(0));
        QVERIFY(client.inputDeviceInventoryDigest().isEmpty());
        QVERIFY(client.inputDeviceDiscoveryErrorName().endsWith(
            QStringLiteral("InvalidInputDeviceInventory")
        ));
        QVERIFY(!client.inputDeviceDiscoveryErrorMessage().isEmpty());
        QCOMPARE(client.inputValues(), inputDefaults());
        QCOMPARE(client.inputValues().size(), 49);
        QCOMPARE(client.inputDeviceProjectionAvailable(), true);
    }

    void keepsSavedProjectionAndInput49WhenLiveDiscoveryFails()
    {
        service_.setSnapshotBytes(snapshotWithInputDevices({
            savedInputDevice(
                QStringLiteral("pointer-main"),
                QStringLiteral("Pointer Main"),
                QStringLiteral("pointer")
            ),
        }));
        service_.setInputDeviceReply(
            inputDeviceInventoryBytes({
                connectedInputDevice(
                    QStringLiteral("Pointer-Main"),
                    QStringLiteral("pointer")
                ),
            }, QLatin1Char('c'), 2, 1, 3),
            1'800'000'000'104ULL
        );
        QVERIFY(service_.start());

        HyprShelld::CompositorClient client(bus_, nullptr);
        QTRY_VERIFY_WITH_TIMEOUT(client.inputAvailable(), 3000);
        QTRY_VERIFY_WITH_TIMEOUT(
            client.inputDeviceProjectionAvailable(), 3000
        );
        QTRY_VERIFY_WITH_TIMEOUT(
            client.inputDeviceDiscoveryAvailable(), 3000
        );
        QCOMPARE(client.connectedInputDevices().size(), 1);
        QCOMPARE(client.savedInputDevices().size(), 1);
        QCOMPARE(client.otherSavedInputDevices(), QVariantList{});
        QCOMPARE(client.inputDeviceInventoryDigest(), QString(64, 'c'));
        QCOMPARE(client.inputDevicesObservedAtMs(), 1'800'000'000'104ULL);
        QCOMPARE(
            client.inputDeviceUnaddressableCounts().value(
                QStringLiteral("switches")
            ).toUInt(),
            2U
        );
        QCOMPARE(
            client.savedInputDevices().constFirst().toMap().value(
                QStringLiteral("matchState")
            ).toString(),
            QStringLiteral("observed")
        );

        const auto errorName = QStringLiteral(
            "org.hyprshelld.Compositor1.Error.RuntimeUnavailable"
        );
        service_.setInputDeviceError(
            errorName, QStringLiteral("Injected input discovery failure")
        );
        const auto deviceCalls = service_.inputDeviceCallCount();
        client.refreshConnectedInputDevices();
        QTRY_VERIFY_WITH_TIMEOUT(
            service_.inputDeviceCallCount() == deviceCalls + 1
                && !client.inputDeviceDiscoveryBusy(),
            3000
        );
        QCOMPARE(client.inputProjectionAvailable(), true);
        QCOMPARE(client.inputValues(), inputDefaults());
        QCOMPARE(client.inputValues().size(), 49);
        QCOMPARE(client.inputDeviceDiscoveryAvailable(), false);
        QCOMPARE(client.connectedInputDevices(), QVariantList{});
        QCOMPARE(client.inputDevicesObservedAtMs(), qulonglong(0));
        QVERIFY(client.inputDeviceInventoryDigest().isEmpty());
        QCOMPARE(client.inputDeviceUnaddressableCounts(), QVariantMap{});
        QCOMPARE(client.inputDeviceDiscoveryErrorName(), errorName);
        QCOMPARE(client.inputDeviceDiscoveryErrorMessage(),
                 QStringLiteral("Injected input discovery failure"));
        QCOMPARE(client.inputDeviceProjectionAvailable(), true);
        QCOMPARE(client.savedInputDevices().size(), 1);
        QCOMPARE(
            client.savedInputDevices().constFirst().toMap().value(
                QStringLiteral("matchState")
            ).toString(),
            QStringLiteral("inventory-unavailable")
        );
        QCOMPARE(client.otherSavedInputDevices().size(), 1);
        QVERIFY(client.inputDeviceProjectionInventoryDigest().isEmpty());
        QVERIFY(client.inputDeviceProjectionErrorName().isEmpty());
    }

    void keepsLiveDiscoveryAndInput49WhenSavedProjectionFails()
    {
        service_.setSnapshotBytes(snapshotWithInputDevices({
            savedInputDevice(
                QStringLiteral("keyboard-main"),
                QStringLiteral("Board Alpha"),
                QStringLiteral("keyboard")
            ),
        }));
        service_.setInputDeviceReply(
            inputDeviceInventoryBytes({
                connectedInputDevice(
                    QStringLiteral("Board-Alpha"),
                    QStringLiteral("keyboard"),
                    QStringLiteral("English (US)")
                ),
            }, QLatin1Char('d'), 1, 2, 3),
            1'800'000'000'105ULL
        );
        QVERIFY(service_.start());

        HyprShelld::CompositorClient client(bus_, nullptr);
        QTRY_VERIFY_WITH_TIMEOUT(client.inputProjectionAvailable(), 3000);
        QTRY_VERIFY_WITH_TIMEOUT(
            client.inputDeviceDiscoveryAvailable(), 3000
        );
        QTRY_VERIFY_WITH_TIMEOUT(
            client.inputDeviceProjectionAvailable(), 3000
        );
        QCOMPARE(client.savedInputDevices().size(), 1);
        QCOMPARE(client.otherSavedInputDevices(), QVariantList{});
        QCOMPARE(client.inputDeviceProjectionRevisionToken(),
                 QStringLiteral("7"));
        QCOMPARE(client.inputDeviceProjectionInventoryDigest(),
                 QString(64, 'd'));
        QCOMPARE(
            client.connectedInputDevices().constFirst().toMap().value(
                QStringLiteral("savedSettingsState")
            ).toString(),
            QStringLiteral("matched")
        );

        service_.setSnapshotBytes(snapshotWithInputDevices({
            savedInputDevice(
                QStringLiteral("keyboard-main"),
                QStringLiteral("Board Alpha"),
                QStringLiteral("Keyboard")
            ),
        }));
        const auto snapshotCalls = service_.snapshotCallCount();
        client.refresh();
        QTRY_VERIFY_WITH_TIMEOUT(
            service_.snapshotCallCount() == snapshotCalls + 1
                && !client.inputDeviceProjectionErrorName().isEmpty(),
            3000
        );
        QCOMPARE(client.inputValues(), inputDefaults());
        QCOMPARE(client.inputValues().size(), 49);
        QCOMPARE(client.inputDeviceProjectionAvailable(), false);
        QCOMPARE(client.savedInputDevices(), QVariantList{});
        QCOMPARE(client.otherSavedInputDevices(), QVariantList{});
        QVERIFY(client.inputDeviceProjectionRevisionToken().isEmpty());
        QVERIFY(client.inputDeviceProjectionInventoryDigest().isEmpty());
        QVERIFY(client.inputDeviceProjectionErrorName().endsWith(
            QStringLiteral("InvalidInputDeviceProjection")
        ));
        QVERIFY(!client.inputDeviceProjectionErrorMessage().isEmpty());
        QCOMPARE(client.inputDeviceDiscoveryAvailable(), true);
        QCOMPARE(client.inputDeviceInventoryDigest(), QString(64, 'd'));
        QCOMPARE(client.inputDevicesObservedAtMs(), 1'800'000'000'105ULL);
        QCOMPARE(
            client.inputDeviceUnaddressableCounts().value(
                QStringLiteral("tabletTools")
            ).toUInt(),
            3U
        );
        QCOMPARE(client.connectedInputDevices().size(), 1);
        const auto connected = client.connectedInputDevices()
            .constFirst().toMap();
        QCOMPARE(
            connected.value(QStringLiteral("savedSettingsState")).toString(),
            QStringLiteral("unavailable")
        );
        QVERIFY(!connected.value(QStringLiteral("savedDeviceId")).isValid());
    }

    void ignoresOutOfOrderAndDuplicateInputDeviceReplies()
    {
        service_.setHoldInputDevices(true);
        service_.setInputDeviceReply(
            inputDeviceInventoryBytes({
                connectedInputDevice(
                    QStringLiteral("Pointer-Old"),
                    QStringLiteral("pointer")
                ),
            }, QLatin1Char('a')),
            1'800'000'000'106ULL
        );
        QVERIFY(service_.start());

        HyprShelld::CompositorClient client(bus_, nullptr);
        QTRY_COMPARE_WITH_TIMEOUT(service_.heldInputDeviceCount(), 1, 3000);
        service_.setInputDeviceReply(
            inputDeviceInventoryBytes({
                connectedInputDevice(
                    QStringLiteral("Pointer-New"),
                    QStringLiteral("pointer")
                ),
            }, QLatin1Char('e')),
            1'800'000'000'107ULL
        );
        client.refreshConnectedInputDevices();
        QTRY_COMPARE_WITH_TIMEOUT(service_.heldInputDeviceCount(), 2, 3000);

        QVERIFY(service_.releaseInputDevice(1, true));
        QTRY_COMPARE_WITH_TIMEOUT(
            client.inputDeviceInventoryDigest(), QString(64, 'e'), 3000
        );
        QCOMPARE(client.inputDevicesObservedAtMs(), 1'800'000'000'107ULL);
        QCOMPARE(
            client.connectedInputDevices().constFirst().toMap().value(
                QStringLiteral("sessionSelector")
            ).toString(),
            QStringLiteral("Pointer-New")
        );
        QCOMPARE(client.inputDeviceDiscoveryBusy(), false);

        QVERIFY(service_.releaseInputDevice(0, true));
        QTest::qWait(100);
        QCOMPARE(client.inputDeviceInventoryDigest(), QString(64, 'e'));
        QCOMPARE(client.inputDevicesObservedAtMs(), 1'800'000'000'107ULL);
        QCOMPARE(
            client.connectedInputDevices().constFirst().toMap().value(
                QStringLiteral("sessionSelector")
            ).toString(),
            QStringLiteral("Pointer-New")
        );
        QCOMPARE(service_.inputDeviceCallCount(), 2);
    }

    void ownerLossClearsBothInputDeviceAuthorities()
    {
        service_.setSnapshotBytes(snapshotWithInputDevices({
            savedInputDevice(
                QStringLiteral("keyboard-main"),
                QStringLiteral("Board Alpha"),
                QStringLiteral("keyboard")
            ),
        }));
        service_.setInputDeviceReply(
            inputDeviceInventoryBytes({
                connectedInputDevice(
                    QStringLiteral("Board-Alpha"),
                    QStringLiteral("keyboard"),
                    QStringLiteral("English (US)")
                ),
            }, QLatin1Char('f')),
            1'800'000'000'108ULL
        );
        QVERIFY(service_.start());

        HyprShelld::CompositorClient client(bus_, nullptr);
        QTRY_VERIFY_WITH_TIMEOUT(
            client.inputDeviceDiscoveryAvailable(), 3000
        );
        QTRY_VERIFY_WITH_TIMEOUT(
            client.inputDeviceProjectionAvailable(), 3000
        );
        QCOMPARE(client.savedInputDevices().size(), 1);
        QCOMPARE(client.connectedInputDevices().size(), 1);

        service_.stop();
        QTRY_VERIFY_WITH_TIMEOUT(
            !client.inputDeviceDiscoveryAvailable(), 3000
        );
        QTRY_VERIFY_WITH_TIMEOUT(
            !client.inputDeviceProjectionAvailable(), 3000
        );
        QCOMPARE(client.inputDeviceDiscoveryBusy(), false);
        QCOMPARE(client.connectedInputDevices(), QVariantList{});
        QCOMPARE(client.savedInputDevices(), QVariantList{});
        QCOMPARE(client.otherSavedInputDevices(), QVariantList{});
        QCOMPARE(client.inputDevicesObservedAtMs(), qulonglong(0));
        QVERIFY(client.inputDeviceInventoryDigest().isEmpty());
        QVERIFY(client.inputDeviceProjectionRevisionToken().isEmpty());
        QVERIFY(client.inputDeviceProjectionInventoryDigest().isEmpty());
        QVERIFY(client.inputDeviceDiscoveryErrorName().isEmpty());
        QVERIFY(client.inputDeviceProjectionErrorName().isEmpty());
    }

    void ownerReplacementCannotAcceptAnOldInputDeviceReply()
    {
        service_.setSnapshotBytes(snapshotWithInputDevices({
            savedInputDevice(
                QStringLiteral("keyboard-main"),
                QStringLiteral("Board Alpha"),
                QStringLiteral("keyboard")
            ),
        }));
        service_.setHoldInputDevices(true);
        service_.setInputDeviceReply(
            inputDeviceInventoryBytes({
                connectedInputDevice(
                    QStringLiteral("Pointer-Old-Owner"),
                    QStringLiteral("pointer")
                ),
            }, QLatin1Char('a')),
            1'800'000'000'109ULL
        );
        QVERIFY(service_.start());

        HyprShelld::CompositorClient client(bus_, nullptr);
        QTRY_COMPARE_WITH_TIMEOUT(service_.heldInputDeviceCount(), 1, 3000);
        QCOMPARE(client.inputDeviceDiscoveryBusy(), true);

        service_.stop();
        QTRY_VERIFY_WITH_TIMEOUT(!client.inputDeviceDiscoveryBusy(), 3000);
        QCOMPARE(client.inputDeviceDiscoveryAvailable(), false);
        QCOMPARE(client.connectedInputDevices(), QVariantList{});
        QCOMPARE(client.savedInputDevices(), QVariantList{});

        service_.setInputDeviceReply(
            inputDeviceInventoryBytes({
                connectedInputDevice(
                    QStringLiteral("Board-Alpha"),
                    QStringLiteral("keyboard"),
                    QStringLiteral("English (US)")
                ),
            }, QLatin1Char('b')),
            1'800'000'000'110ULL
        );
        QVERIFY(service_.start());
        QTRY_COMPARE_WITH_TIMEOUT(service_.heldInputDeviceCount(), 2, 3000);
        QTRY_VERIFY_WITH_TIMEOUT(
            client.inputDeviceProjectionAvailable(), 3000
        );
        QCOMPARE(client.inputDeviceDiscoveryBusy(), true);
        QCOMPARE(client.inputDeviceDiscoveryAvailable(), false);
        QVERIFY(client.inputDeviceInventoryDigest().isEmpty());
        QCOMPARE(
            client.savedInputDevices().constFirst().toMap().value(
                QStringLiteral("matchState")
            ).toString(),
            QStringLiteral("inventory-unavailable")
        );

        // The first reply belongs to the previous well-known-name ownership
        // generation and must not complete or overwrite the new request.
        QVERIFY(service_.releaseInputDevice(0, true));
        QTest::qWait(100);
        QCOMPARE(client.inputDeviceDiscoveryBusy(), true);
        QCOMPARE(client.inputDeviceDiscoveryAvailable(), false);
        QVERIFY(client.inputDeviceInventoryDigest().isEmpty());
        QCOMPARE(client.connectedInputDevices(), QVariantList{});

        QVERIFY(service_.releaseInputDevice(0));
        QTRY_COMPARE_WITH_TIMEOUT(
            client.inputDeviceInventoryDigest(), QString(64, 'b'), 3000
        );
        QCOMPARE(client.inputDeviceDiscoveryBusy(), false);
        QCOMPARE(client.inputDeviceDiscoveryAvailable(), true);
        QCOMPARE(client.inputDevicesObservedAtMs(), 1'800'000'000'110ULL);
        QCOMPARE(
            client.connectedInputDevices().constFirst().toMap().value(
                QStringLiteral("sessionSelector")
            ).toString(),
            QStringLiteral("Board-Alpha")
        );
        QCOMPARE(
            client.connectedInputDevices().constFirst().toMap().value(
                QStringLiteral("savedSettingsState")
            ).toString(),
            QStringLiteral("matched")
        );
    }

    void projectsSharedBorderPropertiesWithoutRegressingAppearance()
    {
        QVERIFY(service_.start());
        HyprShelld::CompositorClient client(bus_, nullptr);
        QTRY_VERIFY_WITH_TIMEOUT(client.appearanceAvailable(), 3000);
        const auto appearanceValues = client.appearanceValues();
        QSignalSpy sharedBorderChanges(
            &client,
            &HyprShelld::CompositorClient::sharedBorderSyncChanged
        );
        QVERIFY(sharedBorderChanges.isValid());

        QVERIFY(service_.setSharedBorderSync(
            QStringLiteral("pending"),
            initialSharedBorderSourceRevision + 1,
            {},
            true
        ));
        QTRY_COMPARE_WITH_TIMEOUT(
            client.sharedBorderSyncState(),
            QStringLiteral("pending"),
            3000
        );
        QCOMPARE(
            client.sharedBorderSourceRevision(),
            initialSharedBorderSourceRevision + 1
        );
        QCOMPARE(
            client.sharedBorderSourceRevisionToken(),
            QStringLiteral("18")
        );
        QVERIFY(client.sharedBorderSyncError().isEmpty());
        QTRY_VERIFY_WITH_TIMEOUT(client.appearanceAvailable(), 3000);
        QCOMPARE(client.appearanceValues(), appearanceValues);

        // Production publishes the complete three-property group even when
        // only its lossless source revision advances. The strict client must
        // accept that complete revision-only tuple atomically.
        QVERIFY(service_.setSharedBorderSync(
            QStringLiteral("pending"),
            initialSharedBorderSourceRevision + 2,
            {},
            true
        ));
        QTRY_COMPARE_WITH_TIMEOUT(
            client.sharedBorderSourceRevisionToken(),
            QStringLiteral("19"),
            3000
        );
        QCOMPARE(client.sharedBorderSyncState(), QStringLiteral("pending"));
        QVERIFY(client.sharedBorderSyncError().isEmpty());

        QVERIFY(service_.setSharedBorderSync(
            QStringLiteral("failed"),
            initialSharedBorderSourceRevision + 2,
            QStringLiteral("Injected synchronization failure"),
            true
        ));
        QTRY_COMPARE_WITH_TIMEOUT(
            client.sharedBorderSyncState(),
            QStringLiteral("failed"),
            3000
        );
        QCOMPARE(
            client.sharedBorderSyncError(),
            QStringLiteral("Injected synchronization failure")
        );
        QTRY_COMPARE_WITH_TIMEOUT(sharedBorderChanges.size(), 3, 3000);
        QTRY_VERIFY_WITH_TIMEOUT(client.appearanceAvailable(), 3000);
        QCOMPARE(client.appearanceValues(), appearanceValues);
    }

    void projectsSharedSpacingPropertiesWithoutRegressingOtherDomains()
    {
        QVERIFY(service_.start());
        HyprShelld::CompositorClient client(bus_, nullptr);
        QTRY_VERIFY_WITH_TIMEOUT(client.appearanceAvailable(), 3000);
        const auto appearanceValues = client.appearanceValues();
        const auto borderState = client.sharedBorderSyncState();
        const auto borderRevision = client.sharedBorderSourceRevision();
        const auto borderError = client.sharedBorderSyncError();
        QSignalSpy spacingChanges(
            &client,
            &HyprShelld::CompositorClient::sharedSpacingSyncChanged
        );
        QVERIFY(spacingChanges.isValid());

        QVERIFY(service_.setSharedSpacingSync(
            QStringLiteral("saved"),
            initialSharedSpacingSourceRevision + 1,
            {},
            true
        ));
        QTRY_COMPARE_WITH_TIMEOUT(
            client.sharedSpacingSyncState(),
            QStringLiteral("saved"),
            3000
        );
        QCOMPARE(
            client.sharedSpacingSourceRevision(),
            initialSharedSpacingSourceRevision + 1
        );
        QCOMPARE(
            client.sharedSpacingSourceRevisionToken(),
            QStringLiteral("24")
        );
        QVERIFY(client.sharedSpacingSyncError().isEmpty());
        QTRY_VERIFY_WITH_TIMEOUT(client.appearanceAvailable(), 3000);
        QCOMPARE(client.sharedBorderSyncState(), borderState);
        QCOMPARE(client.sharedBorderSourceRevision(), borderRevision);
        QCOMPARE(client.sharedBorderSyncError(), borderError);
        QCOMPARE(client.appearanceValues(), appearanceValues);

        // A complete, otherwise-identical group with a newer exact revision
        // is valid. Partial revision-only signals are covered separately and
        // must never mutate this projection.
        QVERIFY(service_.setSharedSpacingSync(
            QStringLiteral("saved"),
            initialSharedSpacingSourceRevision + 2,
            {},
            true
        ));
        QTRY_COMPARE_WITH_TIMEOUT(
            client.sharedSpacingSourceRevisionToken(),
            QStringLiteral("25"),
            3000
        );
        QCOMPARE(client.sharedSpacingSyncState(), QStringLiteral("saved"));
        QVERIFY(client.sharedSpacingSyncError().isEmpty());

        const auto boundedError = QString(1024, QLatin1Char('e'));
        QVERIFY(service_.setSharedSpacingSync(
            QStringLiteral("failed"),
            initialSharedSpacingSourceRevision + 2,
            boundedError,
            true
        ));
        QTRY_COMPARE_WITH_TIMEOUT(
            client.sharedSpacingSyncState(),
            QStringLiteral("failed"),
            3000
        );
        QCOMPARE(client.sharedSpacingSyncError(), boundedError);
        QTRY_COMPARE_WITH_TIMEOUT(spacingChanges.size(), 3, 3000);
        QTRY_VERIFY_WITH_TIMEOUT(client.appearanceAvailable(), 3000);
        QCOMPARE(client.sharedBorderSyncState(), borderState);
        QCOMPARE(client.appearanceValues(), appearanceValues);
        QVERIFY(client.lastErrorName().isEmpty());
    }

    void rejectsMalformedSharedSpacingProperties_data()
    {
        QTest::addColumn<QVariantMap>("changed");
        const auto validRevision = QVariant::fromValue<qulonglong>(
            initialSharedSpacingSourceRevision
        );
        QTest::newRow("state-wrong-type") << QVariantMap{
            {QStringLiteral("SharedSpacingSyncState"), 1U},
            {QStringLiteral("SharedSpacingSourceRevision"), validRevision},
            {QStringLiteral("SharedSpacingSyncError"), QString{}},
        };
        QTest::newRow("revision-wrong-type") << QVariantMap{
            {QStringLiteral("SharedSpacingSyncState"), QStringLiteral("saved")},
            {QStringLiteral("SharedSpacingSourceRevision"), QStringLiteral("23")},
            {QStringLiteral("SharedSpacingSyncError"), QString{}},
        };
        QTest::newRow("error-wrong-type") << QVariantMap{
            {QStringLiteral("SharedSpacingSyncState"), QStringLiteral("saved")},
            {QStringLiteral("SharedSpacingSourceRevision"), validRevision},
            {QStringLiteral("SharedSpacingSyncError"), false},
        };
        QTest::newRow("unknown-state") << QVariantMap{
            {QStringLiteral("SharedSpacingSyncState"), QStringLiteral("retrying")},
            {QStringLiteral("SharedSpacingSourceRevision"), validRevision},
            {QStringLiteral("SharedSpacingSyncError"), QString{}},
        };
        QTest::newRow("override-with-error") << QVariantMap{
            {QStringLiteral("SharedSpacingSyncState"), QStringLiteral("override")},
            {QStringLiteral("SharedSpacingSourceRevision"), validRevision},
            {QStringLiteral("SharedSpacingSyncError"), QStringLiteral("unexpected")},
        };
        QTest::newRow("failed-without-error") << QVariantMap{
            {QStringLiteral("SharedSpacingSyncState"), QStringLiteral("failed")},
            {QStringLiteral("SharedSpacingSourceRevision"), validRevision},
            {QStringLiteral("SharedSpacingSyncError"), QString{}},
        };
        QTest::newRow("oversized-error") << QVariantMap{
            {QStringLiteral("SharedSpacingSyncState"), QStringLiteral("failed")},
            {QStringLiteral("SharedSpacingSourceRevision"), validRevision},
            {QStringLiteral("SharedSpacingSyncError"), QString(1025, QLatin1Char('e'))},
        };
    }

    void rejectsMalformedSharedSpacingProperties()
    {
        QFETCH(QVariantMap, changed);
        QVERIFY(service_.start());
        HyprShelld::CompositorClient client(bus_, nullptr);
        QTRY_VERIFY_WITH_TIMEOUT(client.appearanceAvailable(), 3000);
        QCOMPARE(client.sharedSpacingSyncState(), QStringLiteral("override"));

        QVERIFY(service_.publishSharedSpacingProperties(changed));
        QTRY_VERIFY_WITH_TIMEOUT(!client.available(), 3000);
        QCOMPARE(client.sharedSpacingSyncState(), QStringLiteral("override"));
        QCOMPARE(
            client.sharedSpacingSourceRevision(),
            initialSharedSpacingSourceRevision
        );
        QVERIFY(client.sharedSpacingSyncError().isEmpty());
        QCOMPARE(client.sharedBorderSyncState(), QStringLiteral("current"));
    }

    void rejectsPartialSharedSpacingPropertyTuplesWithoutMutation()
    {
        QVERIFY(service_.start());
        HyprShelld::CompositorClient client(bus_, nullptr);
        QTRY_VERIFY_WITH_TIMEOUT(client.appearanceAvailable(), 3000);
        QSignalSpy spacingChanges(
            &client,
            &HyprShelld::CompositorClient::sharedSpacingSyncChanged
        );
        QVERIFY(spacingChanges.isValid());

        QVERIFY(service_.publishSharedSpacingProperties({
            {QStringLiteral("SharedSpacingSyncState"), QStringLiteral("saved")},
        }));
        QTRY_VERIFY_WITH_TIMEOUT(!client.available(), 3000);
        QCOMPARE(client.sharedSpacingSyncState(), QStringLiteral("override"));
        QCOMPARE(
            client.sharedSpacingSourceRevision(),
            initialSharedSpacingSourceRevision
        );
        QVERIFY(client.sharedSpacingSyncError().isEmpty());
        QCOMPARE(spacingChanges.size(), 0);
    }

    void rejectsPartialSharedBorderPropertyTuplesWithoutMutation()
    {
        QVERIFY(service_.start());
        HyprShelld::CompositorClient client(bus_, nullptr);
        QTRY_VERIFY_WITH_TIMEOUT(client.appearanceAvailable(), 3000);
        QSignalSpy borderChanges(
            &client,
            &HyprShelld::CompositorClient::sharedBorderSyncChanged
        );
        QVERIFY(borderChanges.isValid());

        QVERIFY(service_.publishSharedBorderProperties({
            {QStringLiteral("SharedBorderSyncState"), QStringLiteral("saved")},
        }));
        QTRY_VERIFY_WITH_TIMEOUT(!client.available(), 3000);
        QCOMPARE(client.sharedBorderSyncState(), QStringLiteral("current"));
        QCOMPARE(
            client.sharedBorderSourceRevision(),
            initialSharedBorderSourceRevision
        );
        QVERIFY(client.sharedBorderSyncError().isEmpty());
        QCOMPARE(borderChanges.size(), 0);
    }

    void rejectsMalformedSharedBorderProperties_data()
    {
        QTest::addColumn<QVariantMap>("changed");

        const auto validRevision = QVariant::fromValue<qulonglong>(
            initialSharedBorderSourceRevision
        );
        QTest::newRow("state-wrong-type") << QVariantMap{
            {QStringLiteral("SharedBorderSyncState"), 1U},
            {QStringLiteral("SharedBorderSourceRevision"), validRevision},
            {QStringLiteral("SharedBorderSyncError"), QString{}},
        };
        QTest::newRow("revision-wrong-type") << QVariantMap{
            {QStringLiteral("SharedBorderSyncState"), QStringLiteral("current")},
            {
                QStringLiteral("SharedBorderSourceRevision"),
                QStringLiteral("17")
            },
            {QStringLiteral("SharedBorderSyncError"), QString{}},
        };
        QTest::newRow("error-wrong-type") << QVariantMap{
            {QStringLiteral("SharedBorderSyncState"), QStringLiteral("current")},
            {QStringLiteral("SharedBorderSourceRevision"), validRevision},
            {QStringLiteral("SharedBorderSyncError"), false},
        };
        QTest::newRow("unknown-state") << QVariantMap{
            {QStringLiteral("SharedBorderSyncState"), QStringLiteral("retrying")},
            {QStringLiteral("SharedBorderSourceRevision"), validRevision},
            {QStringLiteral("SharedBorderSyncError"), QString{}},
        };
        QTest::newRow("current-with-error") << QVariantMap{
            {QStringLiteral("SharedBorderSyncState"), QStringLiteral("current")},
            {QStringLiteral("SharedBorderSourceRevision"), validRevision},
            {
                QStringLiteral("SharedBorderSyncError"),
                QStringLiteral("Unexpected error")
            },
        };
        QTest::newRow("failed-without-error") << QVariantMap{
            {QStringLiteral("SharedBorderSyncState"), QStringLiteral("failed")},
            {QStringLiteral("SharedBorderSourceRevision"), validRevision},
            {QStringLiteral("SharedBorderSyncError"), QString{}},
        };
    }

    void rejectsMalformedSharedBorderProperties()
    {
        QFETCH(QVariantMap, changed);

        QVERIFY(service_.start());
        HyprShelld::CompositorClient client(bus_, nullptr);
        QTRY_VERIFY_WITH_TIMEOUT(client.appearanceAvailable(), 3000);
        QCOMPARE(
            client.sharedBorderSyncState(),
            QStringLiteral("current")
        );

        QVERIFY(service_.publishSharedBorderProperties(changed));
        QTRY_VERIFY_WITH_TIMEOUT(!client.available(), 3000);
        QCOMPARE(
            client.sharedBorderSyncState(),
            QStringLiteral("current")
        );
        QCOMPARE(
            client.sharedBorderSourceRevision(),
            initialSharedBorderSourceRevision
        );
        QVERIFY(client.sharedBorderSyncError().isEmpty());
    }

    void ownerLossResetsBothSharedVisualProjections()
    {
        QVERIFY(service_.start());
        HyprShelld::CompositorClient client(bus_, nullptr);
        QTRY_VERIFY_WITH_TIMEOUT(client.available(), 3000);
        QCOMPARE(
            client.sharedBorderSyncState(),
            QStringLiteral("current")
        );
        QSignalSpy sharedBorderChanges(
            &client,
            &HyprShelld::CompositorClient::sharedBorderSyncChanged
        );
        QVERIFY(sharedBorderChanges.isValid());
        QSignalSpy sharedSpacingChanges(
            &client,
            &HyprShelld::CompositorClient::sharedSpacingSyncChanged
        );
        QVERIFY(sharedSpacingChanges.isValid());

        service_.stop();
        QTRY_VERIFY_WITH_TIMEOUT(!client.available(), 3000);
        QCOMPARE(
            client.sharedBorderSyncState(),
            QStringLiteral("unavailable")
        );
        QCOMPARE(client.sharedBorderSourceRevision(), 0ULL);
        QCOMPARE(
            client.sharedBorderSourceRevisionToken(),
            QStringLiteral("0")
        );
        QCOMPARE(
            client.sharedBorderSyncError(),
            QStringLiteral("Shared visual settings are unavailable")
        );
        QCOMPARE(sharedBorderChanges.size(), 1);
        QCOMPARE(
            client.sharedSpacingSyncState(),
            QStringLiteral("unavailable")
        );
        QCOMPARE(client.sharedSpacingSourceRevision(), 0ULL);
        QCOMPARE(
            client.sharedSpacingSourceRevisionToken(),
            QStringLiteral("0")
        );
        QCOMPARE(
            client.sharedSpacingSyncError(),
            QStringLiteral("Shared visual settings are unavailable")
        );
        QCOMPARE(sharedSpacingChanges.size(), 1);
    }

    void retriesSharedBorderSynchronizationWithAnEmptyReplyAndCopiesErrors()
    {
        QVERIFY(service_.setSharedBorderSync(
            QStringLiteral("failed"),
            initialSharedBorderSourceRevision,
            QStringLiteral("Injected synchronization failure")
        ));
        QVERIFY(service_.start());
        HyprShelld::CompositorClient client(bus_, nullptr);
        QTRY_VERIFY_WITH_TIMEOUT(client.appearanceAvailable(), 3000);
        const auto appearanceValues = client.appearanceValues();
        QSignalSpy failures(
            &client,
            &HyprShelld::CompositorClient::operationFailed
        );
        QVERIFY(failures.isValid());

        client.retrySharedBorderSync();
        QVERIFY(client.busy());
        QCOMPARE(
            client.busyOperation(),
            QStringLiteral("shared-border-sync")
        );
        QTRY_VERIFY_WITH_TIMEOUT(!client.busy(), 3000);
        QCOMPARE(service_.retrySharedBorderSyncCallCount(), 1);
        QCOMPARE(failures.size(), 0);
        QVERIFY(client.lastErrorName().isEmpty());
        QTRY_VERIFY_WITH_TIMEOUT(client.appearanceAvailable(), 3000);
        QCOMPARE(client.appearanceValues(), appearanceValues);

        service_.setRetrySharedBorderSyncError(
            QStringLiteral("org.hyprshelld.Compositor1.Error.Unavailable"),
            QStringLiteral("Injected retry failure")
        );
        client.retrySharedBorderSync();
        QVERIFY(client.busy());
        QTRY_VERIFY_WITH_TIMEOUT(!client.busy(), 3000);
        QCOMPARE(service_.retrySharedBorderSyncCallCount(), 2);
        QTRY_COMPARE_WITH_TIMEOUT(failures.size(), 1, 3000);
        QCOMPARE(
            client.lastErrorOperation(),
            QStringLiteral("shared-border-sync")
        );
        QCOMPARE(
            client.lastErrorName(),
            QStringLiteral("org.hyprshelld.Compositor1.Error.Unavailable")
        );
        QCOMPARE(
            client.lastErrorMessage(),
            QStringLiteral("Injected retry failure")
        );
        QTRY_VERIFY_WITH_TIMEOUT(client.appearanceAvailable(), 3000);
        QCOMPARE(client.appearanceValues(), appearanceValues);
        QCOMPARE(
            client.sharedBorderSyncState(),
            QStringLiteral("failed")
        );
    }

    void retriesSharedSpacingSynchronizationWithExactOperationIsolation()
    {
        QVERIFY(service_.setSharedSpacingSync(
            QStringLiteral("failed"),
            initialSharedSpacingSourceRevision,
            QStringLiteral("Injected spacing synchronization failure")
        ));
        QVERIFY(service_.start());
        HyprShelld::CompositorClient client(bus_, nullptr);
        QTRY_VERIFY_WITH_TIMEOUT(client.appearanceAvailable(), 3000);
        const auto appearanceValues = client.appearanceValues();
        const auto borderState = client.sharedBorderSyncState();
        QSignalSpy failures(
            &client,
            &HyprShelld::CompositorClient::operationFailed
        );
        QVERIFY(failures.isValid());

        client.retrySharedSpacingSync();
        QVERIFY(client.busy());
        QCOMPARE(client.busyOperation(), QStringLiteral("shared-spacing-sync"));
        QTRY_VERIFY_WITH_TIMEOUT(!client.busy(), 3000);
        QCOMPARE(service_.retrySharedSpacingSyncCallCount(), 1);
        QCOMPARE(failures.size(), 0);
        QVERIFY(client.lastErrorName().isEmpty());
        QTRY_VERIFY_WITH_TIMEOUT(client.appearanceAvailable(), 3000);
        QCOMPARE(client.sharedBorderSyncState(), borderState);
        QCOMPARE(client.appearanceValues(), appearanceValues);

        service_.setRetrySharedSpacingSyncError(
            QStringLiteral("org.hyprshelld.Compositor1.Error.Unavailable"),
            QStringLiteral("Injected spacing retry failure")
        );
        client.retrySharedSpacingSync();
        QVERIFY(client.busy());
        QTRY_VERIFY_WITH_TIMEOUT(!client.busy(), 3000);
        QCOMPARE(service_.retrySharedSpacingSyncCallCount(), 2);
        QTRY_COMPARE_WITH_TIMEOUT(failures.size(), 1, 3000);
        QCOMPARE(client.lastErrorOperation(),
                 QStringLiteral("shared-spacing-sync"));
        QCOMPARE(
            client.lastErrorName(),
            QStringLiteral("org.hyprshelld.Compositor1.Error.Unavailable")
        );
        QCOMPARE(client.lastErrorMessage(),
                 QStringLiteral("Injected spacing retry failure"));
        QTRY_VERIFY_WITH_TIMEOUT(client.appearanceAvailable(), 3000);
        QCOMPARE(client.sharedSpacingSyncState(), QStringLiteral("failed"));
        QCOMPARE(client.sharedBorderSyncState(), borderState);
        QCOMPARE(client.appearanceValues(), appearanceValues);
    }

    void malformedCatalogDisablesAllGroupsButNotDisplayDiscovery()
    {
        service_.setOptionCatalogReply(
            optionCatalogBytes(),
            QString(64, QLatin1Char('a'))
        );
        QVERIFY(service_.start());

        HyprShelld::CompositorClient client(bus_, nullptr);
        QTRY_VERIFY_WITH_TIMEOUT(client.available(), 3000);
        QTRY_VERIFY_WITH_TIMEOUT(client.displayDiscoveryAvailable(), 3000);
        QTRY_VERIFY_WITH_TIMEOUT(!client.appearanceErrorName().isEmpty(), 3000);
        QTRY_VERIFY_WITH_TIMEOUT(!client.inputErrorName().isEmpty(), 3000);
        QTRY_VERIFY_WITH_TIMEOUT(!client.windowsErrorName().isEmpty(), 3000);
        QTRY_VERIFY_WITH_TIMEOUT(
            !client.workspacesErrorName().isEmpty(), 3000
        );
        QTRY_VERIFY_WITH_TIMEOUT(!client.advancedErrorName().isEmpty(), 3000);
        QTRY_VERIFY_WITH_TIMEOUT(!client.rulesErrorName().isEmpty(), 3000);
        QCOMPARE(client.catalogAvailable(), false);
        QCOMPARE(client.allOptionsAvailable(), false);
        QVERIFY(client.allOptions().isEmpty());
        QVERIFY(client.allValues().isEmpty());
        QVERIFY(!client.allOptionsErrorName().isEmpty());
        QCOMPARE(client.appearanceAvailable(), false);
        QCOMPARE(client.inputAvailable(), false);
        QCOMPARE(client.windowsAvailable(), false);
        QCOMPARE(client.workspacesAvailable(), false);
        QCOMPARE(client.advancedAvailable(), false);
        QCOMPARE(client.rulesAvailable(), false);
        QCOMPARE(client.workspacesAvailable(), false);
        QCOMPARE(client.rulesAvailable(), false);
        QCOMPARE(client.appearanceProjectionAvailable(), false);
        QCOMPARE(client.inputProjectionAvailable(), false);
        QCOMPARE(client.windowsProjectionAvailable(), false);
        QCOMPARE(client.workspacesProjectionAvailable(), false);
        QCOMPARE(client.advancedProjectionAvailable(), false);
        QCOMPARE(client.rulesProjectionAvailable(), false);
        QVERIFY(client.lastErrorName().isEmpty());
    }

    void invalidActionAuthorityKeepsScalarProjectionsReadableButClosesMutations()
    {
        service_.setActionCatalogReply(
            actionAuthorityBytes(),
            actionCatalogDigest,
            configSchemaBytes(),
            QString(64, QLatin1Char('a'))
        );
        QVERIFY(service_.start());

        HyprShelld::CompositorClient client(bus_, nullptr);
        QTRY_VERIFY_WITH_TIMEOUT(client.available(), 3000);
        QTRY_VERIFY_WITH_TIMEOUT(client.catalogAvailable(), 3000);
        QTRY_VERIFY_WITH_TIMEOUT(!client.rulesErrorName().isEmpty(), 3000);
        QCOMPARE(client.actionCatalogAvailable(), false);
        QVERIFY(client.bindingActions().isEmpty());
        QVERIFY(client.defaultBindings().isEmpty());
        QCOMPARE(client.bindingsProjectionAvailable(), false);
        QCOMPARE(client.environmentProjectionAvailable(), false);
        QCOMPARE(client.permissionsProjectionAvailable(), false);
        QCOMPARE(client.inputDevicesProjectionAvailable(), false);
        QCOMPARE(client.allValues().size(), 353);
        QCOMPARE(client.allOptionsAvailable(), false);
        QCOMPARE(client.appearanceProjectionAvailable(), true);
        QCOMPARE(client.appearanceAnimationProjectionAvailable(), false);
        QCOMPARE(client.inputProjectionAvailable(), true);
        QCOMPARE(client.inputGesturesProjectionAvailable(), false);
        QVERIFY(client.inputGestures().isEmpty());
        QVERIFY(client.inputGestureCompatibility().isEmpty());
        QVERIFY(client.inputGestureActions().isEmpty());
        QCOMPARE(client.windowsProjectionAvailable(), true);
        QCOMPARE(client.workspacesProjectionAvailable(), true);
        QCOMPARE(client.advancedProjectionAvailable(), true);
        QCOMPARE(client.rulesProjectionAvailable(), false);
        QCOMPARE(client.appearanceValues().size(), 40);
        QCOMPARE(client.inputValues(), inputDefaults());
        QCOMPARE(client.windowsValues(), windowsDefaults());
        QCOMPARE(client.workspacesValues(), workspacesDefaults());
        QCOMPARE(client.advancedValues(), advancedDefaults());
        QCOMPARE(client.appearanceAvailable(), false);
        QCOMPARE(client.inputAvailable(), false);
        QCOMPARE(client.windowsAvailable(), false);
        QCOMPARE(client.workspacesAvailable(), false);
        QCOMPARE(client.advancedAvailable(), false);
        QCOMPARE(client.rulesAvailable(), false);
        QVERIFY(!client.appearanceErrorName().isEmpty());
        QCOMPARE(client.allOptionsErrorName(), client.appearanceErrorName());
        QCOMPARE(client.inputErrorName(), client.appearanceErrorName());
        QCOMPARE(client.windowsErrorName(), client.appearanceErrorName());
        QCOMPARE(client.workspacesErrorName(), client.appearanceErrorName());
        QCOMPARE(client.advancedErrorName(), client.appearanceErrorName());
        QCOMPARE(client.rulesErrorName(), client.appearanceErrorName());
        QVERIFY(!client.appearanceErrorMessage().isEmpty());
        QVERIFY(client.lastErrorName().isEmpty());

        client.saveRules({}, {});
        QCOMPARE(client.rulesErrorName(), client.appearanceErrorName());
        service_.setActionCatalogReply(
            actionAuthorityBytes(),
            actionCatalogDigest,
            configSchemaBytes(),
            configSchemaSha256()
        );
        client.refresh();
        QTRY_VERIFY_WITH_TIMEOUT(client.rulesAvailable(), 3000);
        QTRY_VERIFY_WITH_TIMEOUT(client.allOptionsAvailable(), 3000);
        QTRY_VERIFY_WITH_TIMEOUT(client.actionCatalogAvailable(), 3000);
        QTRY_COMPARE_WITH_TIMEOUT(client.bindingActions().size(), 76, 3000);
        QTRY_COMPARE_WITH_TIMEOUT(
            client.defaultBindings().size(),
            HyprShelld::Hyprland::shippedDefaultKeybindingCount,
            3000
        );
        QTRY_VERIFY_WITH_TIMEOUT(client.bindingsAvailable(), 3000);
        QTRY_VERIFY_WITH_TIMEOUT(client.environmentAvailable(), 3000);
        QTRY_VERIFY_WITH_TIMEOUT(client.permissionsAvailable(), 3000);
        QTRY_VERIFY_WITH_TIMEOUT(client.inputDevicesAvailable(), 3000);
        QVERIFY(client.allOptionsErrorName().isEmpty());
        QVERIFY(client.appearanceErrorName().isEmpty());
        QVERIFY(client.inputErrorName().isEmpty());
        QVERIFY(client.windowsErrorName().isEmpty());
        QVERIFY(client.workspacesErrorName().isEmpty());
        QCOMPARE(
            client.rulesErrorName(),
            QStringLiteral("org.hyprshelld.Client.Compositor.Error.Unavailable")
        );
        QVERIFY(!client.rulesErrorMessage().isEmpty());
    }

    void actionAuthorityBusErrorsArePlainBoundedAndDomainScoped()
    {
        service_.setActionCatalogError(
            QStringLiteral("org.hyprshelld.Compositor1.Error.Unavailable"),
            QString(2048, QLatin1Char('x'))
        );
        QVERIFY(service_.start());

        HyprShelld::CompositorClient client(bus_, nullptr);
        QTRY_VERIFY_WITH_TIMEOUT(client.available(), 3000);
        QTRY_VERIFY_WITH_TIMEOUT(!client.rulesErrorMessage().isEmpty(), 3000);
        QCOMPARE(client.rulesErrorMessage().size(), qsizetype(1024));
        QCOMPARE(client.rulesErrorMessage(), QString(1024, QLatin1Char('x')));
        QCOMPARE(client.appearanceErrorMessage(), client.rulesErrorMessage());
        QCOMPARE(client.inputErrorMessage(), client.rulesErrorMessage());
        QCOMPARE(client.windowsErrorMessage(), client.rulesErrorMessage());
        QCOMPARE(client.workspacesErrorMessage(), client.rulesErrorMessage());
        QCOMPARE(client.advancedErrorMessage(), client.rulesErrorMessage());
        QCOMPARE(client.appearanceProjectionAvailable(), true);
        QCOMPARE(client.inputProjectionAvailable(), true);
        QCOMPARE(client.inputGesturesProjectionAvailable(), false);
        QCOMPARE(client.windowsProjectionAvailable(), true);
        QCOMPARE(client.workspacesProjectionAvailable(), true);
        QCOMPARE(client.advancedProjectionAvailable(), true);
        QCOMPARE(client.rulesProjectionAvailable(), false);
        QCOMPARE(client.appearanceAvailable(), false);
        QCOMPARE(client.rulesAvailable(), false);
        QVERIFY(client.lastErrorName().isEmpty());
    }

    void invalidCompleteSnapshotKeepsScalarsReadableAndAttributesAuthority()
    {
        auto object = QJsonDocument::fromJson(snapshotBytes()).object();
        object.insert(
            QStringLiteral("animations"),
            QJsonArray{QJsonObject{{QStringLiteral("unknown"), true}}}
        );
        auto bytes = HyprShelld::Hyprland::JsonSupport::canonicalJson(object);
        bytes.append('\n');
        service_.setSnapshotBytes(bytes);
        QVERIFY(service_.start());

        HyprShelld::CompositorClient client(bus_, nullptr);
        QTRY_VERIFY_WITH_TIMEOUT(client.available(), 3000);
        QTRY_VERIFY_WITH_TIMEOUT(!client.rulesErrorName().isEmpty(), 3000);
        QCOMPARE(client.allValues().size(), 353);
        QCOMPARE(client.allOptionsAvailable(), false);
        QCOMPARE(client.appearanceProjectionAvailable(), true);
        QCOMPARE(client.appearanceAnimationProjectionAvailable(), false);
        QCOMPARE(client.inputProjectionAvailable(), true);
        QCOMPARE(client.inputGesturesProjectionAvailable(), false);
        QCOMPARE(client.windowsProjectionAvailable(), true);
        QCOMPARE(client.workspacesProjectionAvailable(), true);
        QCOMPARE(client.advancedProjectionAvailable(), true);
        QCOMPARE(client.rulesProjectionAvailable(), false);
        QCOMPARE(client.appearanceValues().size(), 40);
        QCOMPARE(client.inputValues(), inputDefaults());
        QCOMPARE(client.windowsValues(), windowsDefaults());
        QCOMPARE(client.workspacesValues(), workspacesDefaults());
        QCOMPARE(client.advancedValues(), advancedDefaults());
        QCOMPARE(client.appearanceAvailable(), false);
        QCOMPARE(client.inputAvailable(), false);
        QCOMPARE(client.windowsAvailable(), false);
        QCOMPARE(client.workspacesAvailable(), false);
        QCOMPARE(client.advancedAvailable(), false);
        QCOMPARE(client.rulesAvailable(), false);
        QVERIFY(!client.appearanceErrorName().isEmpty());
        QCOMPARE(client.allOptionsErrorName(), client.appearanceErrorName());
        QCOMPARE(client.inputErrorName(), client.appearanceErrorName());
        QCOMPARE(client.windowsErrorName(), client.appearanceErrorName());
        QCOMPARE(client.workspacesErrorName(), client.appearanceErrorName());
        QCOMPARE(client.advancedErrorName(), client.appearanceErrorName());
        QCOMPARE(client.rulesErrorName(), client.appearanceErrorName());
        QVERIFY(client.appearanceErrorMessage().contains(
            QStringLiteral("animations")
        ));
        QVERIFY(client.lastErrorName().isEmpty());
    }

    void invalidInputSnapshotValueDisablesOnlyInputProjection()
    {
        service_.setSnapshotBytes(snapshotWithOverride(
            QStringLiteral("hyprland.input.sensitivity"), 1.5
        ));
        QVERIFY(service_.start());

        HyprShelld::CompositorClient client(bus_, nullptr);
        QTRY_VERIFY_WITH_TIMEOUT(client.available(), 3000);
        QTRY_VERIFY_WITH_TIMEOUT(client.catalogAvailable(), 3000);
        QTRY_VERIFY_WITH_TIMEOUT(client.displayDiscoveryAvailable(), 3000);
        QTRY_VERIFY_WITH_TIMEOUT(!client.inputErrorName().isEmpty(), 3000);
        QTRY_VERIFY_WITH_TIMEOUT(!client.rulesErrorName().isEmpty(), 3000);
        QCOMPARE(client.inputAvailable(), false);
        QCOMPARE(client.appearanceAvailable(), false);
        QCOMPARE(client.windowsAvailable(), false);
        QCOMPARE(client.workspacesAvailable(), false);
        QCOMPARE(client.rulesAvailable(), false);
        QCOMPARE(client.inputProjectionAvailable(), false);
        QCOMPARE(client.appearanceProjectionAvailable(), true);
        QCOMPARE(client.windowsProjectionAvailable(), true);
        QCOMPARE(client.workspacesProjectionAvailable(), true);
        QCOMPARE(client.rulesProjectionAvailable(), false);
        QVERIFY(client.inputOptions().size() == 49);
        QVERIFY(client.inputValues().isEmpty());
        QVERIFY(!client.appearanceErrorName().isEmpty());
        QCOMPARE(client.windowsErrorName(), client.appearanceErrorName());
        QCOMPARE(client.workspacesErrorName(), client.appearanceErrorName());
        QCOMPARE(client.rulesErrorName(), client.appearanceErrorName());
        QVERIFY(client.inputErrorName() != client.appearanceErrorName());
        QVERIFY(client.lastErrorName().isEmpty());
    }

    void invalidAppearanceSnapshotValueDisablesOnlyAppearanceProjection()
    {
        service_.setSnapshotBytes(snapshotWithOverride(
            QStringLiteral("hyprland.decoration.shadow.range"), 101
        ));
        QVERIFY(service_.start());

        HyprShelld::CompositorClient client(bus_, nullptr);
        QTRY_VERIFY_WITH_TIMEOUT(client.available(), 3000);
        QTRY_VERIFY_WITH_TIMEOUT(client.catalogAvailable(), 3000);
        QTRY_VERIFY_WITH_TIMEOUT(client.displayDiscoveryAvailable(), 3000);
        QTRY_VERIFY_WITH_TIMEOUT(
            !client.appearanceErrorName().isEmpty(), 3000
        );
        QCOMPARE(client.appearanceAvailable(), false);
        QCOMPARE(client.inputAvailable(), false);
        QCOMPARE(client.windowsAvailable(), false);
        QCOMPARE(client.workspacesAvailable(), false);
        QCOMPARE(client.rulesAvailable(), false);
        QCOMPARE(client.appearanceProjectionAvailable(), false);
        QCOMPARE(client.inputProjectionAvailable(), true);
        QCOMPARE(client.windowsProjectionAvailable(), true);
        QCOMPARE(client.workspacesProjectionAvailable(), true);
        QCOMPARE(client.rulesProjectionAvailable(), false);
        QVERIFY(client.appearanceOptions().size() == 40);
        QVERIFY(client.appearanceValues().isEmpty());
        QCOMPARE(client.inputValues(), inputDefaults());
        QVERIFY(!client.inputErrorName().isEmpty());
        QCOMPARE(client.windowsErrorName(), client.inputErrorName());
        QCOMPARE(client.workspacesErrorName(), client.inputErrorName());
        QCOMPARE(client.rulesErrorName(), client.inputErrorName());
        QVERIFY(client.appearanceErrorName() != client.inputErrorName());
        QVERIFY(client.lastErrorName().isEmpty());
    }

    void legacyUnsafeGlowRemainsProjectedAndChangedSaveIsRejectedLocally()
    {
        auto object = QJsonDocument::fromJson(snapshotBytes()).object();
        auto overrides = object.value(QStringLiteral("overrides")).toObject();
        overrides.insert(
            QStringLiteral("hyprland.decoration.glow.enabled"), true
        );
        overrides.insert(
            QStringLiteral("hyprland.decoration.glow.range"), 9
        );
        object.insert(QStringLiteral("overrides"), overrides);
        auto bytes = HyprShelld::Hyprland::JsonSupport::canonicalJson(object);
        bytes.append('\n');
        service_.setSnapshotBytes(bytes);
        QVERIFY(service_.start());

        HyprShelld::CompositorClient client(bus_, nullptr);
        QTRY_VERIFY_WITH_TIMEOUT(client.appearanceAvailable(), 3000);
        QCOMPARE(client.appearanceProjectionAvailable(), true);
        QCOMPARE(client.appearanceValues().value(QStringLiteral(
            "hyprland.decoration.glow.enabled"
        )).toBool(), true);
        QCOMPARE(client.appearanceValues().value(QStringLiteral(
            "hyprland.decoration.glow.range"
        )).toInt(), 9);

        auto unsafeChanged = client.appearanceValues();
        unsafeChanged.insert(
            QStringLiteral("hyprland.general.border_size"), 6
        );
        client.saveAppearance(
            unsafeChanged,
            client.appearanceCurves(),
            client.appearanceAnimations()
        );
        QTRY_VERIFY_WITH_TIMEOUT(!client.busy(), 3000);
        QCOMPARE(service_.replaceCallCount(), 0);
        QCOMPARE(service_.applyCallCount(), 0);
        QCOMPARE(client.revision(), baselineRevision);
        QCOMPARE(
            client.appearanceErrorName(),
            QStringLiteral(
                "org.hyprshelld.Client.Compositor.Error.InvalidAppearance"
            )
        );
        QVERIFY(client.appearanceErrorMessage().contains(
            QStringLiteral("range is at least 10")
        ));
        QVERIFY(client.lastErrorName().isEmpty());

        auto repaired = client.appearanceValues();
        repaired.insert(
            QStringLiteral("hyprland.decoration.glow.enabled"), false
        );
        client.saveAppearance(
            repaired,
            client.appearanceCurves(),
            client.appearanceAnimations()
        );
        QTRY_COMPARE_WITH_TIMEOUT(client.revision(), 8ULL, 3000);
        QTRY_VERIFY_WITH_TIMEOUT(!client.busy(), 3000);
        QTRY_VERIFY_WITH_TIMEOUT(client.appearanceAvailable(), 3000);
        QCOMPARE(service_.replaceCallCount(), 1);
        QCOMPARE(service_.applyCallCount(), 1);
        QTRY_COMPARE_WITH_TIMEOUT(client.appearanceValues().value(QStringLiteral(
            "hyprland.decoration.glow.enabled"
        )).toBool(), false, 3000);
        QTRY_COMPARE_WITH_TIMEOUT(client.appearanceValues().value(QStringLiteral(
            "hyprland.decoration.glow.range"
        )).toInt(), 9, 3000);
    }

    void legacyUnsafeGlowRejectsChangedRulesBeforeCallingTheService()
    {
        auto object = QJsonDocument::fromJson(snapshotBytes()).object();
        auto overrides = object.value(QStringLiteral("overrides")).toObject();
        overrides.insert(
            QStringLiteral("hyprland.decoration.glow.enabled"), true
        );
        overrides.insert(
            QStringLiteral("hyprland.decoration.glow.range"), 9
        );
        object.insert(QStringLiteral("overrides"), overrides);
        auto bytes = HyprShelld::Hyprland::JsonSupport::canonicalJson(object);
        bytes.append('\n');
        service_.setSnapshotBytes(bytes);
        QVERIFY(service_.start());

        HyprShelld::CompositorClient client(bus_, nullptr);
        QTRY_VERIFY_WITH_TIMEOUT(client.rulesAvailable(), 3000);
        client.saveRules(
            QVariantList{QVariant(windowRule(
                QStringLiteral("window-unsafe-glow-change"),
                QStringLiteral("Unsafe glow change")
            ))},
            {}
        );

        QCOMPARE(client.busy(), false);
        QCOMPARE(service_.replaceCallCount(), 0);
        QCOMPARE(service_.applyCallCount(), 0);
        QCOMPARE(client.revision(), baselineRevision);
        QCOMPARE(
            client.rulesErrorName(),
            QStringLiteral(
                "org.hyprshelld.Client.Compositor.Error.InvalidRules"
            )
        );
        QCOMPARE(
            client.rulesErrorMessage(),
            QStringLiteral(
                "$.overrides.hyprland.decoration.glow.range: Inner glow can be enabled only when its range is at least 10; disable glow or raise the range."
            )
        );
        QVERIFY(client.appearanceErrorName().isEmpty());
        QVERIFY(client.inputErrorName().isEmpty());
        QVERIFY(client.windowsErrorName().isEmpty());
        QVERIFY(client.workspacesErrorName().isEmpty());
        QVERIFY(client.lastErrorName().isEmpty());
    }

    void invalidWindowsSnapshotValueDisablesOnlyWindowsProjection()
    {
        service_.setSnapshotBytes(snapshotWithOverride(
            QStringLiteral("hyprland.general.extend_border_grab_area"), 101
        ));
        QVERIFY(service_.start());

        HyprShelld::CompositorClient client(bus_, nullptr);
        QTRY_VERIFY_WITH_TIMEOUT(client.available(), 3000);
        QTRY_VERIFY_WITH_TIMEOUT(client.catalogAvailable(), 3000);
        QTRY_VERIFY_WITH_TIMEOUT(client.displayDiscoveryAvailable(), 3000);
        QTRY_VERIFY_WITH_TIMEOUT(!client.windowsErrorName().isEmpty(), 3000);
        QCOMPARE(client.windowsAvailable(), false);
        QCOMPARE(client.appearanceAvailable(), false);
        QCOMPARE(client.inputAvailable(), false);
        QCOMPARE(client.workspacesAvailable(), false);
        QCOMPARE(client.rulesAvailable(), false);
        QCOMPARE(client.windowsProjectionAvailable(), false);
        QCOMPARE(client.appearanceProjectionAvailable(), true);
        QCOMPARE(client.inputProjectionAvailable(), true);
        QCOMPARE(client.workspacesProjectionAvailable(), true);
        QCOMPARE(client.rulesProjectionAvailable(), false);
        QCOMPARE(client.windowsOptions().size(), 110);
        QVERIFY(client.windowsValues().isEmpty());
        QVERIFY(!client.appearanceErrorName().isEmpty());
        QCOMPARE(client.inputErrorName(), client.appearanceErrorName());
        QCOMPARE(client.workspacesErrorName(), client.appearanceErrorName());
        QCOMPARE(client.rulesErrorName(), client.appearanceErrorName());
        QVERIFY(client.windowsErrorName() != client.appearanceErrorName());
        QVERIFY(client.lastErrorName().isEmpty());
    }

    void invalidWorkspaceSnapshotValueDisablesOnlyWorkspaceProjection()
    {
        service_.setSnapshotBytes(snapshotWithOverride(
            QStringLiteral("hyprland.gestures.workspace_swipe_cancel_ratio"),
            1.5
        ));
        QVERIFY(service_.start());

        HyprShelld::CompositorClient client(bus_, nullptr);
        QTRY_VERIFY_WITH_TIMEOUT(client.available(), 3000);
        QTRY_VERIFY_WITH_TIMEOUT(client.catalogAvailable(), 3000);
        QTRY_VERIFY_WITH_TIMEOUT(
            !client.workspacesErrorName().isEmpty(), 3000
        );
        QCOMPARE(client.workspacesAvailable(), false);
        QCOMPARE(client.appearanceAvailable(), false);
        QCOMPARE(client.inputAvailable(), false);
        QCOMPARE(client.windowsAvailable(), false);
        QCOMPARE(client.rulesAvailable(), false);
        QCOMPARE(client.workspacesProjectionAvailable(), false);
        QCOMPARE(client.appearanceProjectionAvailable(), true);
        QCOMPARE(client.inputProjectionAvailable(), true);
        QCOMPARE(client.windowsProjectionAvailable(), true);
        QCOMPARE(client.rulesProjectionAvailable(), false);
        QCOMPARE(client.workspacesOptions().size(), 21);
        QVERIFY(client.workspacesValues().isEmpty());
        QVERIFY(!client.appearanceErrorName().isEmpty());
        QCOMPARE(client.inputErrorName(), client.appearanceErrorName());
        QCOMPARE(client.windowsErrorName(), client.appearanceErrorName());
        QCOMPARE(client.rulesErrorName(), client.appearanceErrorName());
        QVERIFY(client.workspacesErrorName() != client.appearanceErrorName());
        QVERIFY(client.lastErrorName().isEmpty());
    }

    void invalidAdvancedSnapshotValueDisablesOnlyAdvancedProjection()
    {
        service_.setSnapshotBytes(snapshotWithOverride(
            QStringLiteral("hyprland.render.fp16_sdr_tf"),
            QStringLiteral("1")
        ));
        QVERIFY(service_.start());

        HyprShelld::CompositorClient client(bus_, nullptr);
        QTRY_VERIFY_WITH_TIMEOUT(client.available(), 3000);
        QTRY_VERIFY_WITH_TIMEOUT(client.catalogAvailable(), 3000);
        QTRY_VERIFY_WITH_TIMEOUT(!client.advancedErrorName().isEmpty(), 3000);
        QCOMPARE(client.advancedAvailable(), false);
        QCOMPARE(client.appearanceAvailable(), false);
        QCOMPARE(client.inputAvailable(), false);
        QCOMPARE(client.windowsAvailable(), false);
        QCOMPARE(client.workspacesAvailable(), false);
        QCOMPARE(client.rulesAvailable(), false);
        QCOMPARE(client.advancedProjectionAvailable(), false);
        QCOMPARE(client.appearanceProjectionAvailable(), true);
        QCOMPARE(client.inputProjectionAvailable(), true);
        QCOMPARE(client.windowsProjectionAvailable(), true);
        QCOMPARE(client.workspacesProjectionAvailable(), true);
        QCOMPARE(client.rulesProjectionAvailable(), false);
        QCOMPARE(client.advancedOptions().size(), 16);
        QVERIFY(client.advancedValues().isEmpty());
        QVERIFY(!client.appearanceErrorName().isEmpty());
        QCOMPARE(client.inputErrorName(), client.appearanceErrorName());
        QCOMPARE(client.windowsErrorName(), client.appearanceErrorName());
        QCOMPARE(client.workspacesErrorName(), client.appearanceErrorName());
        QCOMPARE(client.rulesErrorName(), client.appearanceErrorName());
        QVERIFY(client.advancedErrorName() != client.appearanceErrorName());
        QVERIFY(client.lastErrorName().isEmpty());
    }

    void displayDiscoveryFailureDoesNotDisableAnySettingsGroup()
    {
        service_.setConnectedDisplaysFail(true);
        QVERIFY(service_.start());

        HyprShelld::CompositorClient client(bus_, nullptr);
        QTRY_VERIFY_WITH_TIMEOUT(client.available(), 3000);
        QTRY_VERIFY_WITH_TIMEOUT(client.catalogAvailable(), 3000);
        QTRY_VERIFY_WITH_TIMEOUT(client.appearanceAvailable(), 3000);
        QTRY_VERIFY_WITH_TIMEOUT(client.inputAvailable(), 3000);
        QTRY_VERIFY_WITH_TIMEOUT(client.windowsAvailable(), 3000);
        QTRY_VERIFY_WITH_TIMEOUT(client.workspacesAvailable(), 3000);
        QTRY_VERIFY_WITH_TIMEOUT(client.advancedAvailable(), 3000);
        QCOMPARE(client.appearanceProjectionAvailable(), true);
        QCOMPARE(client.inputProjectionAvailable(), true);
        QCOMPARE(client.windowsProjectionAvailable(), true);
        QCOMPARE(client.workspacesProjectionAvailable(), true);
        QCOMPARE(client.advancedProjectionAvailable(), true);
        QCOMPARE(client.displayDiscoveryAvailable(), false);
        QVERIFY(client.lastErrorName().isEmpty());
    }

    void displayPreviewCannotUseStaleTopologyAfterDiscoveryFails()
    {
        QVERIFY(service_.start());
        HyprShelld::CompositorClient client(bus_, nullptr);
        QTRY_VERIFY_WITH_TIMEOUT(client.displayDiscoveryAvailable(), 3000);
        QCOMPARE(client.connectedDisplays().size(), 1);
        QVERIFY(!client.topologyDigest().isEmpty());

        service_.setConnectedDisplaysFail(true);
        client.refresh();
        QTRY_VERIFY_WITH_TIMEOUT(client.available(), 3000);
        QTRY_VERIFY_WITH_TIMEOUT(!client.displayDiscoveryAvailable(), 3000);
        const QVariantList output{
            QVariantMap{
                {QStringLiteral("id"), QStringLiteral("display-DP-1")},
                {QStringLiteral("selector"), QStringLiteral("DP-1")},
                {QStringLiteral("enabled"), true},
            }
        };
        client.previewDisplayConfiguration(output, 15);
        QCOMPARE(client.busy(), false);
        QCOMPARE(service_.heldPreviewCount(), 0);
        QCOMPARE(
            client.lastErrorName(),
            QStringLiteral(
                "org.hyprshelld.Client.Compositor.Error.Unavailable"
            )
        );
    }

    void exposesAnExactRevisionTokenBeyondQmlIntegerPrecision()
    {
        constexpr qulonglong exactRevision = 9'007'199'254'740'993ULL;
        service_.setRevision(exactRevision);
        QVERIFY(service_.start());

        HyprShelld::CompositorClient client(bus_, nullptr);
        QTRY_VERIFY_WITH_TIMEOUT(client.available(), 3000);
        QCOMPARE(client.revision(), exactRevision);
        QCOMPARE(
            client.revisionToken(),
            QStringLiteral("9007199254740993")
        );
    }

    void exposesAnExactSharedBorderSourceRevisionTokenBeyondQmlIntegerPrecision()
    {
        constexpr qulonglong exactRevision = 9'007'199'254'740'993ULL;
        QVERIFY(service_.setSharedBorderSync(
            QStringLiteral("current"), exactRevision, {}
        ));
        QVERIFY(service_.start());

        HyprShelld::CompositorClient client(bus_, nullptr);
        QTRY_VERIFY_WITH_TIMEOUT(client.available(), 3000);
        QCOMPARE(client.sharedBorderSourceRevision(), exactRevision);
        QCOMPARE(
            client.sharedBorderSourceRevisionToken(),
            QStringLiteral("9007199254740993")
        );
    }

    void exposesAnExactSharedSpacingSourceRevisionTokenBeyondQmlIntegerPrecision()
    {
        constexpr qulonglong exactRevision = 9'007'199'254'740'993ULL;
        QVERIFY(service_.setSharedSpacingSync(
            QStringLiteral("saved"), exactRevision, {}
        ));
        QVERIFY(service_.start());

        HyprShelld::CompositorClient client(bus_, nullptr);
        QTRY_VERIFY_WITH_TIMEOUT(client.available(), 3000);
        QCOMPARE(client.sharedSpacingSourceRevision(), exactRevision);
        QCOMPARE(
            client.sharedSpacingSourceRevisionToken(),
            QStringLiteral("9007199254740993")
        );
        QCOMPARE(client.sharedBorderSourceRevision(),
                 initialSharedBorderSourceRevision);
    }

    void rejectsANonCanonicalSnapshotRevisionDuringHydration()
    {
        auto object = QJsonDocument::fromJson(snapshotBytes()).object();
        object.insert(QStringLiteral("revision"), QStringLiteral("07"));
        auto bytes = QJsonDocument(object).toJson(QJsonDocument::Compact);
        bytes.append('\n');
        service_.setSnapshotBytes(bytes);
        QVERIFY(service_.start());

        HyprShelld::CompositorClient client(bus_, nullptr);
        QTRY_COMPARE_WITH_TIMEOUT(service_.snapshotCallCount(), 1, 3000);
        QTest::qWait(20);
        QCOMPARE(client.available(), false);
        QCOMPARE(client.catalogAvailable(), false);
        QCOMPARE(client.appearanceAvailable(), false);
        QCOMPARE(client.inputAvailable(), false);
        QCOMPARE(client.windowsAvailable(), false);
        QCOMPARE(client.workspacesAvailable(), false);
    }

    void rejectsAPartialV1SnapshotDuringHydration()
    {
        auto object = QJsonDocument::fromJson(snapshotBytes()).object();
        object.remove(QStringLiteral("devices"));
        auto bytes = QJsonDocument(object).toJson(QJsonDocument::Compact);
        bytes.append('\n');
        service_.setSnapshotBytes(bytes);
        QVERIFY(service_.start());

        HyprShelld::CompositorClient client(bus_, nullptr);
        QTRY_COMPARE_WITH_TIMEOUT(service_.snapshotCallCount(), 1, 3000);
        QTest::qWait(20);
        QCOMPARE(client.available(), false);
        QCOMPARE(client.catalogAvailable(), false);
        QCOMPARE(client.appearanceAvailable(), false);
        QCOMPARE(client.inputAvailable(), false);
        QCOMPARE(client.windowsAvailable(), false);
        QCOMPARE(client.workspacesAvailable(), false);
    }

    void rejectsNonCanonicalSnapshotBytes_data()
    {
        QTest::addColumn<QByteArray>("bytes");
        const auto object = QJsonDocument::fromJson(snapshotBytes()).object();
        QTest::newRow("pretty-printed")
            << QJsonDocument(object).toJson(QJsonDocument::Indented);
        auto trailing = snapshotBytes();
        trailing.append(' ');
        QTest::newRow("trailing-byte") << trailing;
        auto duplicate = snapshotBytes();
        const QByteArray revision = QByteArrayLiteral("\"revision\":\"7\"");
        const auto position = duplicate.indexOf(revision);
        QVERIFY(position >= 0);
        duplicate.replace(
            position,
            revision.size(),
            QByteArrayLiteral("\"revision\":\"7\",\"revision\":\"7\"")
        );
        QTest::newRow("duplicate-key") << duplicate;
    }

    void rejectsNonCanonicalSnapshotBytes()
    {
        QFETCH(QByteArray, bytes);
        service_.setSnapshotBytes(bytes);
        QVERIFY(service_.start());

        HyprShelld::CompositorClient client(bus_, nullptr);
        QTRY_COMPARE_WITH_TIMEOUT(service_.snapshotCallCount(), 1, 3000);
        QTest::qWait(20);
        QCOMPARE(client.available(), false);
        QCOMPARE(client.catalogAvailable(), false);
        QCOMPARE(client.appearanceAvailable(), false);
        QCOMPARE(client.inputAvailable(), false);
        QCOMPARE(client.windowsAvailable(), false);
        QCOMPARE(client.workspacesAvailable(), false);
    }

    void revisionExhaustionDisablesAllSaveGroupsAndRecoveryMutations()
    {
        service_.setRevision(std::numeric_limits<qulonglong>::max());
        QVERIFY(service_.start());

        HyprShelld::CompositorClient client(bus_, nullptr);
        QTRY_VERIFY_WITH_TIMEOUT(client.available(), 3000);
        QTRY_VERIFY_WITH_TIMEOUT(client.catalogAvailable(), 3000);
        QCOMPARE(client.allOptionsAvailable(), false);
        QCOMPARE(client.allValues().size(), 353);
        QCOMPARE(client.appearanceAvailable(), false);
        QCOMPARE(client.inputAvailable(), false);
        QCOMPARE(client.windowsAvailable(), false);
        QCOMPARE(client.workspacesAvailable(), false);
        QCOMPARE(client.advancedAvailable(), false);
        QCOMPARE(client.rulesAvailable(), false);
        QCOMPARE(client.appearanceProjectionAvailable(), true);
        QCOMPARE(client.inputProjectionAvailable(), true);
        QCOMPARE(client.windowsProjectionAvailable(), true);
        QCOMPARE(client.workspacesProjectionAvailable(), true);
        QCOMPARE(client.advancedProjectionAvailable(), true);
        QCOMPARE(client.rulesProjectionAvailable(), true);
        QCOMPARE(client.recoveryAvailable(), false);
        QCOMPARE(
            client.revisionToken(),
            QStringLiteral("18446744073709551615")
        );
    }

    void savesAndAppliesAppearanceAcrossPropertiesBeforeReplies()
    {
        const QJsonObject activeGlowColor{
            {
                QStringLiteral("colors"),
                QJsonArray{
                    QStringLiteral("0xEE33CCFF"),
                    QStringLiteral("0xAA224466"),
                },
            },
            {QStringLiteral("angle"), 19},
        };
        const QJsonObject inactiveGlowColor{
            {
                QStringLiteral("colors"),
                QJsonArray{QStringLiteral("0x99446688")},
            },
            {QStringLiteral("angle"), 0},
        };
        auto initialObject = QJsonDocument::fromJson(snapshotBytes()).object();
        auto initialOverrides = initialObject.value(
            QStringLiteral("overrides")
        ).toObject();
        initialOverrides.insert(
            QStringLiteral("hyprland.decoration.glow.color"), activeGlowColor
        );
        initialOverrides.insert(
            QStringLiteral("hyprland.decoration.glow.color_inactive"),
            inactiveGlowColor
        );
        initialObject.insert(QStringLiteral("overrides"), initialOverrides);
        auto initialBytes = HyprShelld::Hyprland::JsonSupport::canonicalJson(
            initialObject
        );
        initialBytes.append('\n');
        service_.setSnapshotBytes(std::move(initialBytes));
        QVERIFY(service_.start());
        HyprShelld::CompositorClient client(bus_, nullptr);
        QTRY_VERIFY_WITH_TIMEOUT(client.appearanceAvailable(), 3000);
        const auto inputBefore = client.inputValues();
        const auto inputGesturesBefore = client.inputGestures();
        const auto windowsBefore = client.windowsValues();
        const auto workspacesBefore = client.workspacesValues();
        const auto workspaceRulesBefore = client.workspaceRules();
        const auto windowRulesBefore = client.windowRules();
        const auto layerRulesBefore = client.layerRules();
        auto values = client.appearanceValues();
        values.insert(QStringLiteral("hyprland.general.border_size"), 6);
        values.insert(
            QStringLiteral("hyprland.decoration.dim_inactive"), true
        );
        values.insert(
            QStringLiteral("hyprland.decoration.dim_strength"), 0.65
        );
        values.insert(
            QStringLiteral("hyprland.decoration.active_opacity"), 0.83
        );
        values.insert(
            QStringLiteral("hyprland.decoration.inactive_opacity"), 0.61
        );
        values.insert(
            QStringLiteral("hyprland.decoration.fullscreen_opacity"), 0.74
        );
        values.insert(
            QStringLiteral("hyprland.decoration.dim_modal"), false
        );
        values.insert(
            QStringLiteral("hyprland.decoration.dim_special"), 0.37
        );
        values.insert(
            QStringLiteral("hyprland.decoration.dim_around"), 0.43
        );
        values.insert(
            QStringLiteral("hyprland.decoration.border_part_of_window"),
            false
        );
        values.insert(
            QStringLiteral("hyprland.decoration.rounding_power"),
            7.421
        );
        values.insert(
            QStringLiteral("hyprland.decoration.shadow.enabled"), false
        );
        values.insert(
            QStringLiteral("hyprland.decoration.shadow.range"), 17
        );
        values.insert(
            QStringLiteral("hyprland.decoration.shadow.render_power"), 4
        );
        values.insert(
            QStringLiteral("hyprland.decoration.shadow.sharp"), true
        );
        values.insert(
            QStringLiteral("hyprland.decoration.shadow.offset"),
            QVariantList{125.5, -80.25}
        );
        values.insert(
            QStringLiteral("hyprland.decoration.shadow.scale"),
            0.731234567890123
        );
        values.insert(
            QStringLiteral("hyprland.decoration.glow.enabled"), false
        );
        values.insert(
            QStringLiteral("hyprland.decoration.glow.range"), 9
        );
        values.insert(
            QStringLiteral("hyprland.decoration.glow.render_power"), 4
        );
        const QVariantMap blurValues{
            {QStringLiteral("hyprland.decoration.blur.enabled"), false},
            {QStringLiteral("hyprland.decoration.blur.size"), 24},
            {QStringLiteral("hyprland.decoration.blur.passes"), 5},
            {
                QStringLiteral("hyprland.decoration.blur.ignore_opacity"),
                false,
            },
            {
                QStringLiteral(
                    "hyprland.decoration.blur.new_optimizations"
                ),
                false,
            },
            {QStringLiteral("hyprland.decoration.blur.xray"), true},
            {QStringLiteral("hyprland.decoration.blur.special"), true},
            {QStringLiteral("hyprland.decoration.blur.popups"), true},
            {
                QStringLiteral(
                    "hyprland.decoration.blur.popups_ignorealpha"
                ),
                0.35,
            },
            {
                QStringLiteral("hyprland.decoration.blur.input_methods"),
                true,
            },
            {
                QStringLiteral(
                    "hyprland.decoration.blur.input_methods_ignorealpha"
                ),
                0.45,
            },
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
        for (auto it = blurValues.cbegin(); it != blurValues.cend(); ++it) {
            values.insert(it.key(), it.value());
        }
        QStringList observedOperations;
        connect(
            &client,
            &HyprShelld::CompositorClient::busyOperationChanged,
            this,
            [&client, &observedOperations] {
                observedOperations.append(client.busyOperation());
            }
        );

        client.saveAppearance(
            values, client.appearanceCurves(), client.appearanceAnimations()
        );
        QTRY_COMPARE_WITH_TIMEOUT(client.revision(), 8ULL, 3000);
        QTRY_VERIFY_WITH_TIMEOUT(!client.busy(), 3000);
        QTRY_VERIFY_WITH_TIMEOUT(client.appearanceAvailable(), 3000);
        QCOMPARE(client.appliedRevision(), 8ULL);
        QCOMPARE(client.applyState(), QStringLiteral("current"));
        QCOMPARE(client.requiredActivation(), QStringLiteral("none"));
        QCOMPARE(client.appearanceValues(), values);
        QCOMPARE(
            client.appearanceValues().value(
                QStringLiteral("hyprland.general.border_size")
            ).toInt(),
            6
        );
        QCOMPARE(
            client.appearanceValues().value(
                QStringLiteral("hyprland.decoration.dim_inactive")
            ).toBool(),
            true
        );
        QCOMPARE(
            client.appearanceValues().value(
                QStringLiteral("hyprland.decoration.dim_strength")
            ).toDouble(),
            0.65
        );
        const auto savedOverrides = QJsonDocument::fromJson(
            service_.currentSnapshotBytes()
        ).object().value(QStringLiteral("overrides")).toObject();
        QCOMPARE(savedOverrides.value(QStringLiteral(
            "hyprland.decoration.dim_inactive"
        )).toBool(), true);
        QCOMPARE(savedOverrides.value(QStringLiteral(
            "hyprland.decoration.dim_strength"
        )).toDouble(), 0.65);
        QCOMPARE(savedOverrides.value(QStringLiteral(
            "hyprland.decoration.active_opacity"
        )).toDouble(), 0.83);
        QCOMPARE(savedOverrides.value(QStringLiteral(
            "hyprland.decoration.inactive_opacity"
        )).toDouble(), 0.61);
        QCOMPARE(savedOverrides.value(QStringLiteral(
            "hyprland.decoration.fullscreen_opacity"
        )).toDouble(), 0.74);
        QVERIFY(savedOverrides.contains(QStringLiteral(
            "hyprland.decoration.dim_modal"
        )));
        QCOMPARE(savedOverrides.value(QStringLiteral(
            "hyprland.decoration.dim_modal"
        )).toBool(), false);
        QCOMPARE(savedOverrides.value(QStringLiteral(
            "hyprland.decoration.dim_special"
        )).toDouble(), 0.37);
        QCOMPARE(savedOverrides.value(QStringLiteral(
            "hyprland.decoration.dim_around"
        )).toDouble(), 0.43);
        QVERIFY(savedOverrides.contains(QStringLiteral(
            "hyprland.decoration.border_part_of_window"
        )));
        QCOMPARE(savedOverrides.value(QStringLiteral(
            "hyprland.decoration.border_part_of_window"
        )).toBool(), false);
        QCOMPARE(savedOverrides.value(QStringLiteral(
            "hyprland.decoration.rounding_power"
        )).toDouble(), 7.421);
        QCOMPARE(savedOverrides.value(QStringLiteral(
            "hyprland.decoration.shadow.enabled"
        )).toBool(), false);
        QCOMPARE(savedOverrides.value(QStringLiteral(
            "hyprland.decoration.shadow.range"
        )).toInt(), 17);
        QCOMPARE(savedOverrides.value(QStringLiteral(
            "hyprland.decoration.shadow.render_power"
        )).toInt(), 4);
        QCOMPARE(savedOverrides.value(QStringLiteral(
            "hyprland.decoration.shadow.sharp"
        )).toBool(), true);
        QCOMPARE(savedOverrides.value(QStringLiteral(
            "hyprland.decoration.shadow.offset"
        )).toArray(), QJsonArray({125.5, -80.25}));
        QCOMPARE(savedOverrides.value(QStringLiteral(
            "hyprland.decoration.shadow.scale"
        )).toDouble(), 0.731234567890123);
        QVERIFY(!savedOverrides.contains(QStringLiteral(
            "hyprland.decoration.glow.enabled"
        )));
        QCOMPARE(savedOverrides.value(QStringLiteral(
            "hyprland.decoration.glow.range"
        )).toInt(), 9);
        QCOMPARE(savedOverrides.value(QStringLiteral(
            "hyprland.decoration.glow.render_power"
        )).toInt(), 4);
        QCOMPARE(
            savedOverrides.value(QStringLiteral(
                "hyprland.decoration.glow.color"
            )).toObject(),
            activeGlowColor
        );
        QCOMPARE(
            savedOverrides.value(QStringLiteral(
                "hyprland.decoration.glow.color_inactive"
            )).toObject(),
            inactiveGlowColor
        );
        for (auto it = blurValues.cbegin(); it != blurValues.cend(); ++it) {
            QCOMPARE(savedOverrides.value(it.key()).toVariant(), it.value());
        }
        QCOMPARE(client.inputValues(), inputBefore);
        QCOMPARE(client.inputGestures(), inputGesturesBefore);
        QCOMPARE(client.windowsValues(), windowsBefore);
        QCOMPARE(client.workspacesValues(), workspacesBefore);
        QCOMPARE(client.workspaceRules(), workspaceRulesBefore);
        QCOMPARE(client.windowRules(), windowRulesBefore);
        QCOMPARE(client.layerRules(), layerRulesBefore);
        QCOMPARE(service_.replaceCallCount(), 1);
        QCOMPARE(service_.applyCallCount(), 1);
        QVERIFY(observedOperations.contains(QStringLiteral("appearance-save")));
        QVERIFY(observedOperations.contains(QStringLiteral("appearance-apply")));
        QCOMPARE(client.busyOperation(), QString{});
        QVERIFY(client.lastErrorName().isEmpty());
    }

    void savesAppearanceCollectionsAtomicallyAndRejectsCurveStructureLocally()
    {
        const QJsonArray initialCurves{
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
                {QStringLiteral("stiffness"), 250.0},
                {QStringLiteral("dampening"), 25.0},
                {QStringLiteral("mass"), 1.0},
            },
        };
        const QJsonArray initialAnimations{QJsonObject{
            {QStringLiteral("id"), QStringLiteral("animation-windows")},
            {QStringLiteral("name"), QStringLiteral("windows")},
            {QStringLiteral("enabled"), true},
            {QStringLiteral("speed"), 6.0},
            {QStringLiteral("curve"), QStringLiteral("default")},
            {QStringLiteral("style"), QStringLiteral("slide")},
        }};
        service_.setSnapshotBytes(snapshotWithAppearanceCollections(
            initialCurves, initialAnimations
        ));
        QVERIFY(service_.start());

        HyprShelld::CompositorClient client(bus_, nullptr);
        QTRY_VERIFY_WITH_TIMEOUT(client.appearanceAvailable(), 3000);
        QCOMPARE(client.appearanceAnimationProjectionAvailable(), true);
        QCOMPARE(client.appearanceCurves(), initialCurves.toVariantList());
        QCOMPARE(
            client.appearanceAnimations(), initialAnimations.toVariantList()
        );

        QVariantList nextCurves{
            QVariantMap{
                {QStringLiteral("id"), QStringLiteral("linear-reordered")},
                {QStringLiteral("name"), QStringLiteral("linear")},
                {QStringLiteral("type"), QStringLiteral("spring")},
                {QStringLiteral("stiffness"), 275.5},
                {QStringLiteral("dampening"), 27.5},
                {QStringLiteral("mass"), 1.25},
            },
            QVariantMap{
                {QStringLiteral("id"), QStringLiteral("default-reordered")},
                {QStringLiteral("name"), QStringLiteral("default")},
                {QStringLiteral("type"), QStringLiteral("bezier")},
                {QStringLiteral("points"), QVariantList{
                    QVariantList{0.12, 0.72}, QVariantList{0.22, 0.98},
                }},
            },
        };
        const QVariantList nextAnimations{
            QVariantMap{
                {QStringLiteral("id"), QStringLiteral("animation-global")},
                {QStringLiteral("name"), QStringLiteral("global")},
                {QStringLiteral("enabled"), false},
                {QStringLiteral("speed"), 1.25},
                {QStringLiteral("curve"), QStringLiteral("default")},
                {QStringLiteral("style"), QString{}},
            },
            QVariantMap{
                {QStringLiteral("id"), QStringLiteral("animation-fade")},
                {QStringLiteral("name"), QStringLiteral("fade")},
                {QStringLiteral("enabled"), true},
                {QStringLiteral("speed"), 2.75},
                {QStringLiteral("curve"), QStringLiteral("linear")},
                {QStringLiteral("style"), QString{}},
            },
        };
        auto values = client.appearanceValues();
        values.insert(QStringLiteral("hyprland.general.border_size"), 6);
        auto observedTrustedStaleCollections = false;
        connect(
            &client,
            &HyprShelld::CompositorClient::snapshotChanged,
            this,
            [&client, &nextCurves, &nextAnimations,
             &observedTrustedStaleCollections] {
                if (client.revision() == 8
                    && client.appearanceAnimationProjectionAvailable()
                    && (client.appearanceCurves() != nextCurves
                        || client.appearanceAnimations()
                            != nextAnimations)) {
                    observedTrustedStaleCollections = true;
                }
            }
        );
        client.saveAppearance(values, nextCurves, nextAnimations);
        QTRY_COMPARE_WITH_TIMEOUT(client.revision(), 8ULL, 3000);
        QTRY_VERIFY_WITH_TIMEOUT(!client.busy(), 3000);
        QTRY_VERIFY_WITH_TIMEOUT(
            client.appearanceAnimationProjectionAvailable(), 3000
        );
        QCOMPARE(client.appearanceCurves(), nextCurves);
        QCOMPARE(client.appearanceAnimations(), nextAnimations);
        QCOMPARE(observedTrustedStaleCollections, false);
        QCOMPARE(service_.replaceCallCount(), 1);
        QCOMPARE(service_.applyCallCount(), 1);

        const auto rejected = [&](QVariantList curves) {
            const auto before = service_.replaceCallCount();
            client.saveAppearance(
                client.appearanceValues(), curves,
                client.appearanceAnimations()
            );
            QCOMPARE(client.busy(), false);
            QCOMPARE(service_.replaceCallCount(), before);
            QCOMPARE(
                client.appearanceErrorName(),
                QStringLiteral(
                    "org.hyprshelld.Client.Compositor.Error.InvalidAppearance"
                )
            );
            QVERIFY(client.appearanceErrorMessage().contains(
                QStringLiteral("verified compositor restart workflow")
            ));
            QVERIFY(client.lastErrorName().isEmpty());
        };
        auto added = nextCurves;
        added.append(QVariantMap{
            {QStringLiteral("id"), QStringLiteral("curve-added")},
            {QStringLiteral("name"), QStringLiteral("added")},
            {QStringLiteral("type"), QStringLiteral("bezier")},
            {QStringLiteral("points"), QVariantList{
                QVariantList{0.1, 0.1}, QVariantList{0.9, 0.9},
            }},
        });
        rejected(added);
        auto removed = nextCurves;
        removed.removeLast();
        rejected(removed);
        auto renamed = nextCurves;
        auto renamedCurve = renamed.first().toMap();
        renamedCurve.insert(QStringLiteral("name"), QStringLiteral("renamed"));
        renamed.replace(0, renamedCurve);
        rejected(renamed);
        auto retyped = nextCurves;
        auto retypedCurve = retyped.first().toMap();
        retypedCurve.insert(QStringLiteral("type"), QStringLiteral("bezier"));
        retypedCurve.remove(QStringLiteral("stiffness"));
        retypedCurve.remove(QStringLiteral("dampening"));
        retypedCurve.remove(QStringLiteral("mass"));
        retypedCurve.insert(
            QStringLiteral("points"),
            QVariantList{QVariantList{0.1, 0.1}, QVariantList{0.9, 0.9}}
        );
        retyped.replace(0, retypedCurve);
        rejected(retyped);
        QCOMPARE(service_.replaceCallCount(), 1);
    }

    void retriesAnExactLostReplaceWithoutIncrementingTwice()
    {
        service_.setAmbiguousFirstReplace(true);
        QVERIFY(service_.start());
        HyprShelld::CompositorClient client(bus_, nullptr);
        QTRY_VERIFY_WITH_TIMEOUT(client.appearanceAvailable(), 3000);
        auto values = client.appearanceValues();
        values.insert(
            QStringLiteral("hyprland.decoration.dim_strength"), 0.75
        );
        const QVariantMap familyValues{
            {QStringLiteral("hyprland.decoration.active_opacity"), 0.83},
            {QStringLiteral("hyprland.decoration.inactive_opacity"), 0.61},
            {QStringLiteral("hyprland.decoration.fullscreen_opacity"), 0.74},
            {QStringLiteral("hyprland.decoration.dim_modal"), false},
            {QStringLiteral("hyprland.decoration.dim_special"), 0.37},
            {QStringLiteral("hyprland.decoration.dim_around"), 0.43},
            {QStringLiteral("hyprland.decoration.blur.enabled"), false},
            {QStringLiteral("hyprland.decoration.blur.size"), 24},
            {QStringLiteral("hyprland.decoration.blur.passes"), 5},
            {
                QStringLiteral("hyprland.decoration.blur.ignore_opacity"),
                false,
            },
            {
                QStringLiteral(
                    "hyprland.decoration.blur.new_optimizations"
                ),
                false,
            },
            {QStringLiteral("hyprland.decoration.blur.xray"), true},
            {QStringLiteral("hyprland.decoration.blur.special"), true},
            {
                QStringLiteral(
                    "hyprland.decoration.blur.popups_ignorealpha"
                ),
                0.35,
            },
            {
                QStringLiteral(
                    "hyprland.decoration.blur.input_methods_ignorealpha"
                ),
                0.45,
            },
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
            {
                QStringLiteral(
                    "hyprland.decoration.border_part_of_window"
                ),
                false,
            },
            {
                QStringLiteral("hyprland.decoration.rounding_power"),
                7.421,
            },
            {
                QStringLiteral("hyprland.decoration.shadow.enabled"),
                false,
            },
            {QStringLiteral("hyprland.decoration.shadow.range"), 23},
            {
                QStringLiteral("hyprland.decoration.shadow.render_power"),
                4,
            },
            {QStringLiteral("hyprland.decoration.shadow.sharp"), true},
            {
                QStringLiteral("hyprland.decoration.shadow.offset"),
                QVariantList{123.456789, -80.25},
            },
            {
                QStringLiteral("hyprland.decoration.shadow.scale"),
                0.731234567890123,
            },
        };
        for (auto it = familyValues.cbegin(); it != familyValues.cend(); ++it) {
            values.insert(it.key(), it.value());
        }

        client.saveAppearance(
            values, client.appearanceCurves(), client.appearanceAnimations()
        );
        QTRY_VERIFY_WITH_TIMEOUT(!client.busy(), 3000);
        QTRY_COMPARE_WITH_TIMEOUT(client.revision(), 8ULL, 3000);
        QTRY_VERIFY_WITH_TIMEOUT(client.appearanceAvailable(), 3000);
        QCOMPARE(service_.replaceCallCount(), 2);
        QCOMPARE(service_.applyCallCount(), 1);
        QCOMPARE(client.appliedRevision(), 8ULL);
        QCOMPARE(client.appearanceValues().value(QStringLiteral(
            "hyprland.decoration.dim_inactive"
        )).toBool(), false);
        QCOMPARE(client.appearanceValues().value(QStringLiteral(
            "hyprland.decoration.dim_strength"
        )).toDouble(), 0.75);
        for (auto it = familyValues.cbegin(); it != familyValues.cend(); ++it) {
            QCOMPARE(client.appearanceValues().value(it.key()), it.value());
        }
        QCOMPARE(client.appearanceValues().value(QStringLiteral(
            "hyprland.decoration.blur.popups"
        )).toBool(), false);
        QCOMPARE(client.appearanceValues().value(QStringLiteral(
            "hyprland.decoration.blur.input_methods"
        )).toBool(), false);
        const auto savedOverrides = QJsonDocument::fromJson(
            service_.currentSnapshotBytes()
        ).object().value(QStringLiteral("overrides")).toObject();
        QVERIFY(!savedOverrides.contains(QStringLiteral(
            "hyprland.decoration.dim_inactive"
        )));
        QCOMPARE(savedOverrides.value(QStringLiteral(
            "hyprland.decoration.dim_strength"
        )).toDouble(), 0.75);
        for (auto it = familyValues.cbegin(); it != familyValues.cend(); ++it) {
            QCOMPARE(savedOverrides.value(it.key()).toVariant(), it.value());
        }
        QVERIFY(!savedOverrides.contains(QStringLiteral(
            "hyprland.decoration.blur.popups"
        )));
        QVERIFY(!savedOverrides.contains(QStringLiteral(
            "hyprland.decoration.blur.input_methods"
        )));
    }

    void retriesAnAmbiguousApplyOnlyForTheExactSavedRevision()
    {
        service_.setAmbiguousFirstApplyWithoutProperties(true);
        QVERIFY(service_.start());
        HyprShelld::CompositorClient client(bus_, nullptr);
        QTRY_VERIFY_WITH_TIMEOUT(client.appearanceAvailable(), 3000);
        auto values = client.appearanceValues();
        values.insert(
            QStringLiteral("hyprland.decoration.shadow.enabled"), false
        );
        values.insert(
            QStringLiteral("hyprland.decoration.blur.contrast"),
            0.87654321098765
        );
        values.insert(
            QStringLiteral("hyprland.decoration.border_part_of_window"),
            false
        );
        values.insert(
            QStringLiteral("hyprland.decoration.shadow.range"), 31
        );
        values.insert(
            QStringLiteral("hyprland.decoration.shadow.render_power"), 1
        );
        values.insert(
            QStringLiteral("hyprland.decoration.shadow.sharp"), true
        );
        values.insert(
            QStringLiteral("hyprland.decoration.shadow.offset"),
            QVariantList{-250.0, 250.0}
        );
        values.insert(
            QStringLiteral("hyprland.decoration.shadow.scale"), 0.25
        );

        client.saveAppearance(
            values, client.appearanceCurves(), client.appearanceAnimations()
        );
        QTRY_VERIFY_WITH_TIMEOUT(!client.busy(), 3000);
        QTRY_VERIFY_WITH_TIMEOUT(client.appearanceAvailable(), 3000);
        QCOMPARE(client.revision(), 8ULL);
        QCOMPARE(client.appliedRevision(), 8ULL);
        QCOMPARE(service_.applyCallCount(), 2);
        QCOMPARE(client.appearanceValues().value(QStringLiteral(
            "hyprland.decoration.blur.contrast"
        )).toDouble(), 0.87654321098765);
        QCOMPARE(client.appearanceValues().value(QStringLiteral(
            "hyprland.decoration.border_part_of_window"
        )).toBool(), false);
        QCOMPARE(client.appearanceValues().value(QStringLiteral(
            "hyprland.decoration.shadow.range"
        )).toInt(), 31);
        QCOMPARE(client.appearanceValues().value(QStringLiteral(
            "hyprland.decoration.shadow.render_power"
        )).toInt(), 1);
        QCOMPARE(client.appearanceValues().value(QStringLiteral(
            "hyprland.decoration.shadow.sharp"
        )).toBool(), true);
        QCOMPARE(client.appearanceValues().value(QStringLiteral(
            "hyprland.decoration.shadow.offset"
        )).toList(), QVariantList({-250.0, 250.0}));
        QCOMPARE(client.appearanceValues().value(QStringLiteral(
            "hyprland.decoration.shadow.scale"
        )).toDouble(), 0.25);
    }

    void acceptsExactCurrentPropertiesBeforeAnAmbiguousApplyReply()
    {
        service_.setAmbiguousFirstApplyWithProperties(true);
        QVERIFY(service_.start());
        HyprShelld::CompositorClient client(bus_, nullptr);
        QTRY_VERIFY_WITH_TIMEOUT(client.appearanceAvailable(), 3000);
        auto values = client.appearanceValues();
        values.insert(
            QStringLiteral("hyprland.decoration.blur.enabled"), false
        );
        values.insert(
            QStringLiteral("hyprland.decoration.blur.noise"),
            0.012345678901234
        );
        values.insert(
            QStringLiteral("hyprland.decoration.border_part_of_window"),
            false
        );

        client.saveAppearance(
            values, client.appearanceCurves(), client.appearanceAnimations()
        );
        QTRY_VERIFY_WITH_TIMEOUT(!client.busy(), 3000);
        QTRY_VERIFY_WITH_TIMEOUT(client.appearanceAvailable(), 3000);
        QCOMPARE(client.revision(), 8ULL);
        QCOMPARE(client.appliedRevision(), 8ULL);
        QCOMPARE(service_.applyCallCount(), 1);
        QCOMPARE(client.appearanceValues().value(QStringLiteral(
            "hyprland.decoration.blur.noise"
        )).toDouble(), 0.012345678901234);
        QCOMPARE(client.appearanceValues().value(QStringLiteral(
            "hyprland.decoration.border_part_of_window"
        )).toBool(), false);
        QVERIFY(client.lastErrorName().isEmpty());
    }

    void retainsSavedAppearanceAfterApplyFailureAndRetriesExplicitly()
    {
        service_.setFailNextApply(true);
        QVERIFY(service_.start());
        HyprShelld::CompositorClient client(bus_, nullptr);
        QTRY_VERIFY_WITH_TIMEOUT(client.appearanceAvailable(), 3000);
        auto values = client.appearanceValues();
        values.insert(QStringLiteral("hyprland.animations.enabled"), false);
        values.insert(
            QStringLiteral("hyprland.decoration.dim_inactive"), true
        );
        values.insert(
            QStringLiteral("hyprland.decoration.dim_strength"), 0.7
        );
        const QVariantMap familyValues{
            {QStringLiteral("hyprland.decoration.active_opacity"), 0.83},
            {QStringLiteral("hyprland.decoration.inactive_opacity"), 0.61},
            {QStringLiteral("hyprland.decoration.fullscreen_opacity"), 0.74},
            {QStringLiteral("hyprland.decoration.dim_modal"), false},
            {QStringLiteral("hyprland.decoration.dim_special"), 0.37},
            {QStringLiteral("hyprland.decoration.dim_around"), 0.43},
            {QStringLiteral("hyprland.decoration.blur.enabled"), false},
            {QStringLiteral("hyprland.decoration.blur.size"), 24},
            {QStringLiteral("hyprland.decoration.blur.passes"), 5},
            {
                QStringLiteral("hyprland.decoration.blur.ignore_opacity"),
                false,
            },
            {
                QStringLiteral(
                    "hyprland.decoration.blur.new_optimizations"
                ),
                false,
            },
            {QStringLiteral("hyprland.decoration.blur.xray"), true},
            {QStringLiteral("hyprland.decoration.blur.special"), true},
            {QStringLiteral("hyprland.decoration.blur.popups"), true},
            {
                QStringLiteral(
                    "hyprland.decoration.blur.popups_ignorealpha"
                ),
                0.35,
            },
            {
                QStringLiteral("hyprland.decoration.blur.input_methods"),
                true,
            },
            {
                QStringLiteral(
                    "hyprland.decoration.blur.input_methods_ignorealpha"
                ),
                0.45,
            },
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
            {
                QStringLiteral(
                    "hyprland.decoration.border_part_of_window"
                ),
                false,
            },
            {
                QStringLiteral("hyprland.decoration.rounding_power"),
                7.421,
            },
            {
                QStringLiteral("hyprland.decoration.shadow.enabled"),
                false,
            },
            {QStringLiteral("hyprland.decoration.shadow.range"), 29},
            {
                QStringLiteral("hyprland.decoration.shadow.render_power"),
                4,
            },
            {QStringLiteral("hyprland.decoration.shadow.sharp"), true},
            {
                QStringLiteral("hyprland.decoration.shadow.offset"),
                QVariantList{125.5, -80.25},
            },
            {
                QStringLiteral("hyprland.decoration.shadow.scale"),
                0.625,
            },
        };
        for (auto it = familyValues.cbegin(); it != familyValues.cend(); ++it) {
            values.insert(it.key(), it.value());
        }

        client.saveAppearance(
            values, client.appearanceCurves(), client.appearanceAnimations()
        );
        QTRY_VERIFY_WITH_TIMEOUT(!client.busy(), 3000);
        QTRY_COMPARE_WITH_TIMEOUT(client.revision(), 8ULL, 3000);
        QTRY_VERIFY_WITH_TIMEOUT(client.retryApplyAvailable(), 3000);
        QCOMPARE(client.appliedRevision(), baselineRevision);
        QCOMPARE(client.applyState(), QStringLiteral("retained"));
        QCOMPARE(client.requiredActivation(), QStringLiteral("reload"));
        QCOMPARE(client.appearanceAvailable(), false);
        QCOMPARE(client.inputAvailable(), false);
        QCOMPARE(client.windowsAvailable(), false);
        QCOMPARE(client.appearanceProjectionAvailable(), true);
        QCOMPARE(client.inputProjectionAvailable(), true);
        QCOMPARE(client.windowsProjectionAvailable(), true);
        QCOMPARE(client.appearanceValues().value(QStringLiteral(
            "hyprland.decoration.dim_inactive"
        )).toBool(), true);
        QCOMPARE(client.appearanceValues().value(QStringLiteral(
            "hyprland.decoration.dim_strength"
        )).toDouble(), 0.7);
        for (auto it = familyValues.cbegin(); it != familyValues.cend(); ++it) {
            QCOMPARE(client.appearanceValues().value(it.key()), it.value());
        }
        const auto savedOverrides = QJsonDocument::fromJson(
            service_.currentSnapshotBytes()
        ).object().value(QStringLiteral("overrides")).toObject();
        for (auto it = familyValues.cbegin(); it != familyValues.cend(); ++it) {
            QCOMPARE(savedOverrides.value(it.key()).toVariant(), it.value());
        }
        QCOMPARE(
            client.lastErrorOperation(),
            QStringLiteral("appearance-apply")
        );
        QCOMPARE(
            client.lastErrorName(),
            QStringLiteral("org.hyprshelld.Compositor1.Error.ApplyFailed")
        );
        QVERIFY(client.recoveryAvailable());

        client.retryApply();
        QTRY_VERIFY_WITH_TIMEOUT(!client.busy(), 3000);
        QTRY_VERIFY_WITH_TIMEOUT(client.appearanceAvailable(), 3000);
        QCOMPARE(client.appliedRevision(), 8ULL);
        QCOMPARE(service_.applyCallCount(), 2);
        QCOMPARE(client.appearanceValues().value(QStringLiteral(
            "hyprland.decoration.dim_inactive"
        )).toBool(), true);
        QCOMPARE(client.appearanceValues().value(QStringLiteral(
            "hyprland.decoration.dim_strength"
        )).toDouble(), 0.7);
        for (auto it = familyValues.cbegin(); it != familyValues.cend(); ++it) {
            QCOMPARE(client.appearanceValues().value(it.key()), it.value());
        }
        QVERIFY(client.lastErrorOperation().isEmpty());
        QVERIFY(client.lastErrorName().isEmpty());
    }

    void recoversTheWholeLastWorkingConfigurationAsANewRevision()
    {
        service_.setFailNextApply(true);
        QVERIFY(service_.start());
        HyprShelld::CompositorClient client(bus_, nullptr);
        QTRY_VERIFY_WITH_TIMEOUT(client.appearanceAvailable(), 3000);
        auto values = client.appearanceValues();
        values.insert(QStringLiteral("hyprland.general.border_size"), 6);
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
            {
                QStringLiteral("hyprland.decoration.rounding_power"),
                7.421,
            },
            {
                QStringLiteral("hyprland.decoration.shadow.enabled"),
                false,
            },
            {QStringLiteral("hyprland.decoration.shadow.range"), 37},
            {
                QStringLiteral("hyprland.decoration.shadow.render_power"),
                4,
            },
            {QStringLiteral("hyprland.decoration.shadow.sharp"), true},
            {
                QStringLiteral("hyprland.decoration.shadow.offset"),
                QVariantList{-125.5, 80.25},
            },
            {
                QStringLiteral("hyprland.decoration.shadow.scale"),
                0.375,
            },
        };
        for (auto it = modulationValues.cbegin();
             it != modulationValues.cend(); ++it) {
            values.insert(it.key(), it.value());
        }
        values.insert(
            QStringLiteral("hyprland.decoration.border_part_of_window"),
            false
        );
        client.saveAppearance(
            values, client.appearanceCurves(), client.appearanceAnimations()
        );
        QTRY_VERIFY_WITH_TIMEOUT(client.recoveryAvailable(), 3000);

        client.recoverConfiguration();
        QTRY_VERIFY_WITH_TIMEOUT(!client.busy(), 3000);
        QTRY_COMPARE_WITH_TIMEOUT(client.revision(), 9ULL, 3000);
        QTRY_VERIFY_WITH_TIMEOUT(client.appearanceAvailable(), 3000);
        QCOMPARE(client.appliedRevision(), 9ULL);
        QCOMPARE(service_.recoverCallCount(), 1);
        QCOMPARE(
            client.appearanceValues().value(
                QStringLiteral("hyprland.general.border_size")
            ).toInt(),
            1
        );
        for (auto it = modulationValues.cbegin();
             it != modulationValues.cend(); ++it) {
            QCOMPARE(
                client.appearanceValues().value(it.key()),
                appearanceDefaults().value(it.key())
            );
        }
        QCOMPARE(client.appearanceValues().value(QStringLiteral(
            "hyprland.decoration.border_part_of_window"
        )).toBool(), true);
        QCOMPARE(client.appearanceValues().value(QStringLiteral(
            "hyprland.decoration.rounding_power"
        )).toDouble(), 2.0);
    }

    void neverRetriesAnAmbiguousRecoveryReply()
    {
        service_.setFailNextApply(true);
        service_.setAmbiguousRecover(true);
        QVERIFY(service_.start());
        HyprShelld::CompositorClient client(bus_, nullptr);
        QTRY_VERIFY_WITH_TIMEOUT(client.appearanceAvailable(), 3000);
        auto values = client.appearanceValues();
        values.insert(QStringLiteral("hyprland.decoration.rounding"), 5);
        client.saveAppearance(
            values, client.appearanceCurves(), client.appearanceAnimations()
        );
        QTRY_VERIFY_WITH_TIMEOUT(client.recoveryAvailable(), 3000);

        client.recoverConfiguration();
        QTRY_VERIFY_WITH_TIMEOUT(!client.busy(), 3000);
        QTRY_COMPARE_WITH_TIMEOUT(client.revision(), 9ULL, 3000);
        QTRY_VERIFY_WITH_TIMEOUT(client.available(), 3000);
        QCOMPARE(client.appliedRevision(), 9ULL);
        QCOMPARE(client.applyState(), QStringLiteral("current"));
        QCOMPARE(service_.recoverCallCount(), 1);
        QCOMPARE(
            client.lastErrorOperation(),
            QStringLiteral("recover")
        );
        QCOMPARE(
            client.lastErrorName(),
            QStringLiteral("org.freedesktop.DBus.Error.NoReply")
        );
    }

    void rejectsPartialAppearanceMapsWithoutCallingTheService()
    {
        QVERIFY(service_.start());
        HyprShelld::CompositorClient client(bus_, nullptr);
        QTRY_VERIFY_WITH_TIMEOUT(client.appearanceAvailable(), 3000);
        auto values = client.appearanceValues();
        values.remove(QStringLiteral(
            "hyprland.decoration.shadow.render_power"
        ));

        client.saveAppearance(
            values, client.appearanceCurves(), client.appearanceAnimations()
        );
        QCOMPARE(client.busy(), false);
        QCOMPARE(service_.replaceCallCount(), 0);
        QCOMPARE(
            client.appearanceErrorName(),
            QStringLiteral(
                "org.hyprshelld.Client.Compositor.Error.InvalidAppearance"
            )
        );
        QVERIFY(client.lastErrorName().isEmpty());
        QVERIFY(client.inputErrorName().isEmpty());
        QVERIFY(client.windowsErrorName().isEmpty());
    }

    void ownerLossClearsAppearanceBusyStateAndOperation()
    {
        service_.setHoldReplaces(true);
        QVERIFY(service_.start());
        HyprShelld::CompositorClient client(bus_, nullptr);
        QTRY_VERIFY_WITH_TIMEOUT(client.appearanceAvailable(), 3000);
        auto values = client.appearanceValues();
        values.insert(
            QStringLiteral("hyprland.decoration.shadow.enabled"), false
        );
        values.insert(
            QStringLiteral("hyprland.decoration.blur.vibrancy_darkness"),
            0.34567890123456
        );
        values.insert(
            QStringLiteral("hyprland.decoration.border_part_of_window"),
            false
        );
        values.insert(
            QStringLiteral("hyprland.decoration.shadow.range"), 41
        );
        values.insert(
            QStringLiteral("hyprland.decoration.shadow.render_power"), 4
        );
        values.insert(
            QStringLiteral("hyprland.decoration.shadow.sharp"), true
        );
        client.saveAppearance(
            values, client.appearanceCurves(), client.appearanceAnimations()
        );
        QTRY_COMPARE_WITH_TIMEOUT(service_.heldReplaceCount(), 1, 3000);
        QCOMPARE(client.busy(), true);
        QCOMPARE(client.busyOperation(), QStringLiteral("appearance-save"));

        service_.stop();
        QTRY_VERIFY_WITH_TIMEOUT(!client.busy(), 3000);
        QCOMPARE(client.busyOperation(), QString{});
        QCOMPARE(client.available(), false);
        QCOMPARE(client.appearanceProjectionAvailable(), false);
        QCOMPARE(client.appearanceAnimationProjectionAvailable(), false);
        QVERIFY(client.appearanceValues().isEmpty());
        QVERIFY(client.appearanceCurves().isEmpty());
        QVERIFY(client.appearanceAnimations().isEmpty());
    }

    void savesAndAppliesInputAcrossTheExactSharedPipeline()
    {
        const QJsonArray devices{
            savedInputDevice(
                QStringLiteral("pointer-main"),
                QStringLiteral("Pointer Main"),
                QStringLiteral("pointer"),
                false,
                QJsonObject{
                    {QStringLiteral("sensitivity"), 0.25},
                    {QStringLiteral("resolve_binds_by_sym"), false},
                }
            ),
        };
        auto initialSnapshot = QJsonDocument::fromJson(
            snapshotWithInputDevices(devices)
        ).object();
        const QJsonObject preservedKeyboardAndBindingOverrides{
            {
                QStringLiteral("hyprland.input.kb_file"),
                QStringLiteral("/tmp/custom-keymap.xkb")
            },
            {
                QStringLiteral("hyprland.input.kb_layout"),
                QStringLiteral("us,de")
            },
            {
                QStringLiteral("hyprland.input.kb_model"),
                QStringLiteral("pc105")
            },
            {
                QStringLiteral("hyprland.input.kb_options"),
                QStringLiteral("grp:alt_shift_toggle,caps:escape")
            },
            {
                QStringLiteral("hyprland.input.kb_rules"),
                QStringLiteral("evdev")
            },
            {
                QStringLiteral("hyprland.input.kb_variant"),
                QStringLiteral(",nodeadkeys")
            },
            {
                QStringLiteral("hyprland.binds.disable_keybind_grabbing"),
                true
            },
            {QStringLiteral("hyprland.binds.drag_threshold"), 17},
            {
                QStringLiteral("hyprland.binds.pass_mouse_when_bound"),
                true
            },
            {QStringLiteral("hyprland.binds.scroll_event_delay"), 411},
        };
        auto initialOverrides = initialSnapshot.value(
            QStringLiteral("overrides")
        ).toObject();
        for (auto iterator = preservedKeyboardAndBindingOverrides.constBegin();
             iterator != preservedKeyboardAndBindingOverrides.constEnd();
             ++iterator) {
            initialOverrides.insert(iterator.key(), iterator.value());
        }
        initialSnapshot.insert(QStringLiteral("overrides"), initialOverrides);
        auto initialBytes = HyprShelld::Hyprland::JsonSupport::canonicalJson(
            initialSnapshot
        );
        initialBytes.append('\n');
        service_.setSnapshotBytes(std::move(initialBytes));
        QVERIFY(service_.start());
        HyprShelld::CompositorClient client(bus_, nullptr);
        QTRY_VERIFY_WITH_TIMEOUT(client.inputAvailable(), 3000);
        const auto appearanceBefore = client.appearanceValues();
        const auto windowsBefore = client.windowsValues();
        const auto workspacesBefore = client.workspacesValues();
        const auto workspaceRulesBefore = client.workspaceRules();
        const auto windowRulesBefore = client.windowRules();
        const auto layerRulesBefore = client.layerRules();
        auto values = client.inputValues();
        values.insert(QStringLiteral("hyprland.input.repeat_rate"), 86);
        values.insert(QStringLiteral("hyprland.input.sensitivity"), 0.07);
        values.insert(QStringLiteral("hyprland.input.scroll_factor"), 1.03);
        values.insert(
            QStringLiteral("hyprland.input.accel_profile"),
            QStringLiteral("flat")
        );
        values.insert(QStringLiteral("hyprland.input.natural_scroll"), true);
        values.insert(
            QStringLiteral("hyprland.input.touchpad.scroll_factor"), 0.97
        );
        values.insert(
            QStringLiteral("hyprland.input.scroll_method"),
            QStringLiteral("on_button_down")
        );
        values.insert(QStringLiteral("hyprland.input.scroll_button"), 274);
        values.insert(QStringLiteral("hyprland.input.scroll_button_lock"), true);
        values.insert(QStringLiteral("hyprland.input.off_window_axis_events"), 3.0);
        values.insert(QStringLiteral("hyprland.input.emulate_discrete_scroll"), 2.0);
        values.insert(
            QStringLiteral("hyprland.input.touchpad.clickfinger_behavior"),
            true
        );
        values.insert(QStringLiteral("hyprland.input.touchpad.drag_3fg"), 2.0);
        values.insert(QStringLiteral("hyprland.input.touchpad.drag_lock"), 1.0);
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
            QStringLiteral("hyprland.gestures.close_max_timeout"), 1375
        );
        values.insert(
            QStringLiteral("hyprland.input.touchdevice.enabled"), false
        );
        values.insert(
            QStringLiteral("hyprland.input.touchdevice.transform"), 5.0
        );
        values.insert(
            QStringLiteral("hyprland.input.tablet.relative_input"), true
        );
        values.insert(
            QStringLiteral("hyprland.input.tablet.left_handed"), true
        );
        values.insert(
            QStringLiteral("hyprland.input.tablet.transform"), 6.0
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
            QStringLiteral("hyprland.cursor.hotspot_padding"), 13.0
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
            QVariantList{123.456789, -987.654321}
        );
        values.insert(
            QStringLiteral(
                "hyprland.input.tablet.absolute_region_position"
            ),
            true
        );
        values.insert(
            QStringLiteral("hyprland.input.tablet.region_size"),
            QVariantList{-99.999999, 0.0}
        );
        values.insert(
            QStringLiteral("hyprland.input.resolve_binds_by_sym"), true
        );
        QStringList observedOperations;
        connect(
            &client,
            &HyprShelld::CompositorClient::busyOperationChanged,
            this,
            [&client, &observedOperations] {
                observedOperations.append(client.busyOperation());
            }
        );

        client.saveInput(values, client.inputGestures());
        QTRY_COMPARE_WITH_TIMEOUT(client.revision(), 8ULL, 3000);
        QTRY_VERIFY_WITH_TIMEOUT(!client.busy(), 3000);
        QTRY_VERIFY_WITH_TIMEOUT(client.inputAvailable(), 3000);
        QCOMPARE(client.appliedRevision(), 8ULL);
        QCOMPARE(client.applyState(), QStringLiteral("current"));
        QCOMPARE(client.requiredActivation(), QStringLiteral("none"));
        QCOMPARE(client.inputValues(), values);
        QCOMPARE(client.inputValues().value(
            QStringLiteral("hyprland.input.sensitivity")
        ).toDouble(), 0.07);
        QCOMPARE(client.inputValues().value(
            QStringLiteral("hyprland.input.scroll_factor")
        ).toDouble(), 1.03);
        QCOMPARE(client.inputValues().value(
            QStringLiteral("hyprland.input.touchpad.scroll_factor")
        ).toDouble(), 0.97);
        QCOMPARE(client.inputValues().value(
            QStringLiteral("hyprland.input.scroll_method")
        ).toString(), QStringLiteral("on_button_down"));
        QCOMPARE(client.inputValues().value(
            QStringLiteral("hyprland.input.scroll_button")
        ).toInt(), 274);
        QCOMPARE(client.inputValues().value(
            QStringLiteral("hyprland.input.scroll_button_lock")
        ).toBool(), true);
        QCOMPARE(client.inputValues().value(
            QStringLiteral("hyprland.input.off_window_axis_events")
        ).toDouble(), 3.0);
        QCOMPARE(client.inputValues().value(
            QStringLiteral("hyprland.input.emulate_discrete_scroll")
        ).toDouble(), 2.0);
        QCOMPARE(client.inputValues().value(QStringLiteral(
            "hyprland.input.touchpad.clickfinger_behavior"
        )).toBool(), true);
        QCOMPARE(client.inputValues().value(QStringLiteral(
            "hyprland.input.touchpad.drag_3fg"
        )).toDouble(), 2.0);
        QCOMPARE(client.inputValues().value(QStringLiteral(
            "hyprland.input.touchpad.drag_lock"
        )).toDouble(), 1.0);
        QCOMPARE(client.inputValues().value(QStringLiteral(
            "hyprland.input.touchpad.flip_x"
        )).toBool(), true);
        QCOMPARE(client.inputValues().value(QStringLiteral(
            "hyprland.input.touchpad.flip_y"
        )).toBool(), true);
        QCOMPARE(client.inputValues().value(QStringLiteral(
            "hyprland.input.touchpad.middle_button_emulation"
        )).toBool(), true);
        QCOMPARE(client.inputValues().value(QStringLiteral(
            "hyprland.input.touchpad.tap_button_map"
        )).toString(), QStringLiteral("lmr"));
        QCOMPARE(client.inputValues().value(
            QStringLiteral("hyprland.input.numlock_by_default")
        ).toBool(), true);
        QCOMPARE(client.inputValues().value(
            QStringLiteral("hyprland.input.virtualkeyboard.share_states")
        ).toDouble(), 1.0);
        QCOMPARE(client.inputValues().value(QStringLiteral(
            "hyprland.input.virtualkeyboard.release_pressed_on_close"
        )).toBool(), true);
        QCOMPARE(client.inputValues().value(
            QStringLiteral("hyprland.misc.name_vk_after_proc")
        ).toBool(), false);
        QCOMPARE(client.inputValues().value(
            QStringLiteral("hyprland.input.force_no_accel")
        ).toBool(), true);
        QCOMPARE(client.inputValues().value(
            QStringLiteral("hyprland.input.rotation")
        ).toDouble(), 137.0);
        QCOMPARE(client.inputValues().value(
            QStringLiteral("hyprland.misc.middle_click_paste")
        ).toBool(), false);
        QCOMPARE(client.inputValues().value(
            QStringLiteral("hyprland.gestures.close_max_timeout")
        ).toInt(), 1375);
        QCOMPARE(client.inputValues().value(
            QStringLiteral("hyprland.input.touchdevice.enabled")
        ).toBool(), false);
        QCOMPARE(client.inputValues().value(
            QStringLiteral("hyprland.input.touchdevice.transform")
        ).toDouble(), 5.0);
        QCOMPARE(client.inputValues().value(
            QStringLiteral("hyprland.input.tablet.relative_input")
        ).toBool(), true);
        QCOMPARE(client.inputValues().value(
            QStringLiteral("hyprland.input.tablet.left_handed")
        ).toBool(), true);
        QCOMPARE(client.inputValues().value(
            QStringLiteral("hyprland.input.tablet.transform")
        ).toDouble(), 6.0);
        QCOMPARE(client.inputValues().value(
            QStringLiteral("hyprland.cursor.hide_on_key_press")
        ).toBool(), true);
        QCOMPARE(client.inputValues().value(
            QStringLiteral("hyprland.cursor.hide_on_touch")
        ).toBool(), false);
        QCOMPARE(client.inputValues().value(
            QStringLiteral("hyprland.cursor.hide_on_tablet")
        ).toBool(), true);
        QCOMPARE(client.inputValues().value(
            QStringLiteral("hyprland.cursor.inactive_timeout")
        ).toDouble(), 2.37);
        QCOMPARE(client.inputValues().value(
            QStringLiteral("hyprland.cursor.hotspot_padding")
        ).toDouble(), 13.0);
        QCOMPARE(client.inputValues().value(
            QStringLiteral("hyprland.cursor.no_warps")
        ).toBool(), true);
        QCOMPARE(client.inputValues().value(
            QStringLiteral("hyprland.cursor.persistent_warps")
        ).toBool(), true);
        QCOMPARE(client.inputValues().value(QStringLiteral(
            "hyprland.cursor.warp_back_after_non_mouse_input"
        )).toBool(), true);
        QCOMPARE(client.inputValues().value(
            QStringLiteral("hyprland.input.tablet.region_position")
        ).toList(), QVariantList({123.456789, -987.654321}));
        QCOMPARE(client.inputValues().value(QStringLiteral(
            "hyprland.input.tablet.absolute_region_position"
        )).toBool(), true);
        QCOMPARE(client.inputValues().value(
            QStringLiteral("hyprland.input.tablet.region_size")
        ).toList(), QVariantList({-99.999999, 0.0}));
        QCOMPARE(client.inputValues().value(
            QStringLiteral("hyprland.input.resolve_binds_by_sym")
        ).toBool(), true);
        const auto savedSnapshot = QJsonDocument::fromJson(
            service_.currentSnapshotBytes()
        ).object();
        const auto savedOverrides = savedSnapshot.value(
            QStringLiteral("overrides")
        ).toObject();
        for (auto iterator = preservedKeyboardAndBindingOverrides.constBegin();
             iterator != preservedKeyboardAndBindingOverrides.constEnd();
             ++iterator) {
            QCOMPARE(savedOverrides.value(iterator.key()), iterator.value());
        }
        QCOMPARE(
            savedSnapshot.value(QStringLiteral("devices")).toArray(), devices
        );
        QCOMPARE(
            savedSnapshot.value(QStringLiteral("bindings")),
            initialSnapshot.value(QStringLiteral("bindings"))
        );
        QCOMPARE(
            savedSnapshot.value(QStringLiteral("submaps")),
            initialSnapshot.value(QStringLiteral("submaps"))
        );
        QCOMPARE(
            savedSnapshot.value(QStringLiteral("devices")).toArray()
                .at(0).toObject().value(QStringLiteral("overrides"))
                .toObject().value(QStringLiteral("resolve_binds_by_sym"))
                .toBool(),
            false
        );
        QCOMPARE(client.appearanceValues(), appearanceBefore);
        QCOMPARE(client.windowsValues(), windowsBefore);
        QCOMPARE(client.workspacesValues(), workspacesBefore);
        QCOMPARE(client.workspaceRules(), workspaceRulesBefore);
        QCOMPARE(client.windowRules(), windowRulesBefore);
        QCOMPARE(client.layerRules(), layerRulesBefore);
        QCOMPARE(service_.replaceCallCount(), 1);
        QCOMPARE(service_.applyCallCount(), 1);
        QVERIFY(observedOperations.contains(QStringLiteral("input-save")));
        QVERIFY(observedOperations.contains(QStringLiteral("input-apply")));
        QCOMPARE(client.busyOperation(), QString{});
        QVERIFY(client.inputErrorName().isEmpty());
        QVERIFY(client.appearanceErrorName().isEmpty());
        QVERIFY(client.lastErrorName().isEmpty());
    }

    void projectsAndSavesGesturesAsOneAtomicInputDomain()
    {
        const QJsonArray initialGestures{
            gestureObject(
                QStringLiteral("gesture-workspace"),
                3,
                QStringLiteral("left"),
                1.5,
                QJsonObject{{QStringLiteral("type"), QStringLiteral("workspace")}},
                QJsonArray{QStringLiteral("super")}
            ),
            gestureObject(
                QStringLiteral("gesture-pinch-compatibility"),
                4,
                QStringLiteral("pinchIn"),
                1.5,
                QJsonObject{
                    {QStringLiteral("type"), QStringLiteral("cursorZoom")},
                    {QStringLiteral("zoomLevel"), 1.25},
                    {QStringLiteral("mode"), QStringLiteral("live")},
                }
            ),
        };
        service_.setSnapshotBytes(snapshotWithGestures(initialGestures));
        QVERIFY(service_.start());

        HyprShelld::CompositorClient client(bus_, nullptr);
        QTRY_VERIFY_WITH_TIMEOUT(client.inputAvailable(), 3000);
        QCOMPARE(client.inputGesturesProjectionAvailable(), true);
        QCOMPARE(client.inputGestures(), initialGestures.toVariantList());
        QCOMPARE(client.inputGestureCompatibility().size(), 2);
        QCOMPARE(
            client.inputGestureCompatibility().at(0).toMap().value(
                QStringLiteral("editable")
            ).toBool(),
            true
        );
        const auto compatibility =
            client.inputGestureCompatibility().at(1).toMap();
        QCOMPARE(
            compatibility.value(QStringLiteral("id")).toString(),
            QStringLiteral("gesture-pinch-compatibility")
        );
        QCOMPARE(
            compatibility.value(QStringLiteral("editable")).toBool(), false
        );
        QVERIFY(
            compatibility.value(QStringLiteral("reason")).toString()
                .contains(QStringLiteral("ignores scale"))
        );

        auto values = client.inputValues();
        values.insert(
            QStringLiteral("hyprland.gestures.close_max_timeout"), 1375
        );
        QVariantList nextGestures{
            initialGestures.at(1).toObject().toVariantMap(),
            gestureObject(
                QStringLiteral("gesture-workspace"),
                3,
                QStringLiteral("right"),
                1.75,
                QJsonObject{{QStringLiteral("type"), QStringLiteral("workspace")}},
                QJsonArray{QStringLiteral("super")},
                true
            ).toVariantMap(),
            gestureObject(
                QStringLiteral("gesture-special"),
                4,
                QStringLiteral("up"),
                2.0,
                QJsonObject{
                    {QStringLiteral("type"), QStringLiteral("special")},
                    {QStringLiteral("workspace"), QStringLiteral("magic")},
                }
            ).toVariantMap(),
        };
        client.saveInput(values, nextGestures);
        QTRY_COMPARE_WITH_TIMEOUT(client.revision(), 8ULL, 3000);
        QTRY_VERIFY_WITH_TIMEOUT(!client.busy(), 3000);
        QTRY_VERIFY_WITH_TIMEOUT(client.inputAvailable(), 3000);
        QCOMPARE(client.inputValues(), values);
        QCOMPARE(client.inputGestures(), nextGestures);
        QCOMPARE(client.inputGestureCompatibility().size(), 3);
        QCOMPARE(
            client.inputGestureCompatibility().at(0).toMap().value(
                QStringLiteral("editable")
            ).toBool(),
            false
        );
        QCOMPARE(service_.replaceCallCount(), 1);
        QCOMPARE(service_.applyCallCount(), 1);

        auto changedCompatibility = nextGestures;
        auto changedCompatibilityRecord = changedCompatibility.at(0).toMap();
        changedCompatibilityRecord.insert(QStringLiteral("scale"), 1.0);
        changedCompatibility[0] = changedCompatibilityRecord;
        client.saveInput(values, changedCompatibility);
        QCOMPARE(client.busy(), false);
        QCOMPARE(service_.replaceCallCount(), 1);
        QCOMPARE(service_.applyCallCount(), 1);
        QCOMPARE(
            client.inputErrorName(),
            QStringLiteral(
                "org.hyprshelld.Client.Compositor.Error.InvalidInput"
            )
        );

        auto newUnset = nextGestures;
        newUnset.append(gestureObject(
            QStringLiteral("gesture-unset-new"),
            5,
            QStringLiteral("down"),
            1.0,
            QJsonObject{{QStringLiteral("type"), QStringLiteral("unset")}}
        ).toVariantMap());
        client.saveInput(values, newUnset);
        QCOMPARE(client.busy(), false);
        QCOMPARE(service_.replaceCallCount(), 1);
        QVERIFY(client.inputErrorMessage().contains(QStringLiteral("Unset")));
    }

    void retriesAnExactLostInputReplaceWithoutIncrementingTwice()
    {
        service_.setAmbiguousFirstReplace(true);
        QVERIFY(service_.start());
        HyprShelld::CompositorClient client(bus_, nullptr);
        QTRY_VERIFY_WITH_TIMEOUT(client.inputAvailable(), 3000);
        auto values = client.inputValues();
        values.insert(
            QStringLiteral("hyprland.input.resolve_binds_by_sym"), true
        );

        client.saveInput(values, client.inputGestures());
        QTRY_VERIFY_WITH_TIMEOUT(!client.busy(), 3000);
        QTRY_COMPARE_WITH_TIMEOUT(client.revision(), 8ULL, 3000);
        QTRY_VERIFY_WITH_TIMEOUT(client.inputAvailable(), 3000);
        QCOMPARE(service_.replaceCallCount(), 2);
        QCOMPARE(service_.applyCallCount(), 1);
        QCOMPARE(client.appliedRevision(), 8ULL);
        QCOMPARE(client.inputValues(), values);
        QVERIFY(client.inputErrorName().isEmpty());
    }

    void retriesAnAmbiguousInputApplyForOnlyTheExactSavedRevision()
    {
        service_.setAmbiguousFirstApplyWithoutProperties(true);
        QVERIFY(service_.start());
        HyprShelld::CompositorClient client(bus_, nullptr);
        QTRY_VERIFY_WITH_TIMEOUT(client.inputAvailable(), 3000);
        auto values = client.inputValues();
        values.insert(
            QStringLiteral("hyprland.input.resolve_binds_by_sym"), true
        );

        client.saveInput(values, client.inputGestures());
        QTRY_VERIFY_WITH_TIMEOUT(!client.busy(), 3000);
        QTRY_VERIFY_WITH_TIMEOUT(client.inputAvailable(), 3000);
        QCOMPARE(client.revision(), 8ULL);
        QCOMPARE(client.appliedRevision(), 8ULL);
        QCOMPARE(service_.replaceCallCount(), 1);
        QCOMPARE(service_.applyCallCount(), 2);
        QCOMPARE(client.inputValues(), values);
        QVERIFY(client.lastErrorName().isEmpty());
    }

    void retainsSavedInputAfterApplyFailureAndRetriesWithNeutralScope()
    {
        service_.setFailNextApply(true);
        QVERIFY(service_.start());
        HyprShelld::CompositorClient client(bus_, nullptr);
        QTRY_VERIFY_WITH_TIMEOUT(client.inputAvailable(), 3000);
        auto values = client.inputValues();
        values.insert(
            QStringLiteral("hyprland.input.resolve_binds_by_sym"), true
        );

        client.saveInput(values, client.inputGestures());
        QTRY_VERIFY_WITH_TIMEOUT(!client.busy(), 3000);
        QTRY_COMPARE_WITH_TIMEOUT(client.revision(), 8ULL, 3000);
        QTRY_VERIFY_WITH_TIMEOUT(client.retryApplyAvailable(), 3000);
        QCOMPARE(client.appliedRevision(), baselineRevision);
        QCOMPARE(client.applyState(), QStringLiteral("retained"));
        QCOMPARE(client.requiredActivation(), QStringLiteral("reload"));
        QCOMPARE(client.appearanceAvailable(), false);
        QCOMPARE(client.inputAvailable(), false);
        QCOMPARE(client.appearanceProjectionAvailable(), true);
        QCOMPARE(client.inputProjectionAvailable(), true);
        QCOMPARE(client.inputValues(), values);
        QVERIFY(client.inputErrorName().isEmpty());
        QVERIFY(client.appearanceErrorName().isEmpty());
        QCOMPARE(
            client.lastErrorOperation(),
            QStringLiteral("input-apply")
        );
        QCOMPARE(
            client.lastErrorName(),
            QStringLiteral("org.hyprshelld.Compositor1.Error.ApplyFailed")
        );
        QVERIFY(client.recoveryAvailable());

        QStringList observedOperations;
        connect(
            &client,
            &HyprShelld::CompositorClient::busyOperationChanged,
            this,
            [&client, &observedOperations] {
                observedOperations.append(client.busyOperation());
            }
        );
        client.retryApply();
        QTRY_VERIFY_WITH_TIMEOUT(!client.busy(), 3000);
        QTRY_VERIFY_WITH_TIMEOUT(client.inputAvailable(), 3000);
        QCOMPARE(client.appliedRevision(), 8ULL);
        QCOMPARE(service_.applyCallCount(), 2);
        QVERIFY(observedOperations.contains(
            QStringLiteral("compositor-apply")
        ));
        QVERIFY(client.lastErrorOperation().isEmpty());
        QVERIFY(client.lastErrorName().isEmpty());
    }

    void recoversTheWholeSnapshotAfterAnInput49ApplyFailure()
    {
        service_.setFailNextApply(true);
        QVERIFY(service_.start());
        HyprShelld::CompositorClient client(bus_, nullptr);
        QTRY_VERIFY_WITH_TIMEOUT(client.inputAvailable(), 3000);

        auto values = client.inputValues();
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
            QStringLiteral("hyprland.cursor.hotspot_padding"), 13.0
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
            QStringLiteral("hyprland.input.resolve_binds_by_sym"), true
        );
        client.saveInput(values, client.inputGestures());
        QTRY_VERIFY_WITH_TIMEOUT(client.recoveryAvailable(), 3000);
        QCOMPARE(client.inputValues(), values);

        client.recoverConfiguration();
        QTRY_VERIFY_WITH_TIMEOUT(!client.busy(), 3000);
        QTRY_COMPARE_WITH_TIMEOUT(client.revision(), 9ULL, 3000);
        QTRY_VERIFY_WITH_TIMEOUT(client.inputAvailable(), 3000);
        QCOMPARE(client.appliedRevision(), 9ULL);
        QCOMPARE(service_.recoverCallCount(), 1);
        QCOMPARE(client.inputValues(), inputDefaults());
        QVERIFY(client.inputErrorName().isEmpty());
    }

    void inputReplaceFailureIsScopedAndDoesNotPoisonAppearance()
    {
        service_.setFailNextReplace(true);
        QVERIFY(service_.start());
        HyprShelld::CompositorClient client(bus_, nullptr);
        QTRY_VERIFY_WITH_TIMEOUT(client.inputAvailable(), 3000);
        auto values = client.inputValues();
        values.insert(
            QStringLiteral("hyprland.input.resolve_binds_by_sym"), true
        );

        client.saveInput(values, client.inputGestures());
        QTRY_VERIFY_WITH_TIMEOUT(!client.busy(), 3000);
        QTRY_COMPARE_WITH_TIMEOUT(service_.replaceCallCount(), 1, 3000);
        QCOMPARE(service_.applyCallCount(), 0);
        QCOMPARE(client.revision(), baselineRevision);
        QTRY_VERIFY_WITH_TIMEOUT(client.inputAvailable(), 3000);
        QTRY_VERIFY_WITH_TIMEOUT(client.appearanceAvailable(), 3000);
        QCOMPARE(
            client.inputErrorName(),
            QStringLiteral(
                "org.hyprshelld.Compositor1.Error.InvalidSnapshot"
            )
        );
        QCOMPARE(
            client.inputErrorMessage(),
            QStringLiteral("Injected replacement failure")
        );
        QVERIFY(client.appearanceErrorName().isEmpty());
        QVERIFY(client.lastErrorName().isEmpty());
    }

    void rejectsInvalidInputMapsWithoutCallingTheService()
    {
        QVERIFY(service_.start());
        HyprShelld::CompositorClient client(bus_, nullptr);
        QTRY_VERIFY_WITH_TIMEOUT(client.inputAvailable(), 3000);

        QList<QVariantMap> invalidMaps;
        auto invalid = client.inputValues();
        invalid.remove(QStringLiteral("hyprland.input.repeat_rate"));
        invalidMaps.append(invalid);
        invalid = client.inputValues();
        invalid.remove(
            QStringLiteral("hyprland.input.touchdevice.enabled")
        );
        invalidMaps.append(invalid);
        invalid = client.inputValues();
        invalid.remove(QStringLiteral("hyprland.cursor.hide_on_key_press"));
        invalidMaps.append(invalid);
        invalid = client.inputValues();
        invalid.insert(QStringLiteral("hyprland.input.unknown"), true);
        invalidMaps.append(invalid);
        invalid = client.inputValues();
        invalid.insert(QStringLiteral("hyprland.input.repeat_rate"), 25.5);
        invalidMaps.append(invalid);
        invalid = client.inputValues();
        invalid.insert(
            QStringLiteral("hyprland.input.sensitivity"),
            std::numeric_limits<double>::quiet_NaN()
        );
        invalidMaps.append(invalid);
        invalid = client.inputValues();
        invalid.insert(QStringLiteral("hyprland.input.scroll_factor"), 2.01);
        invalidMaps.append(invalid);
        invalid = client.inputValues();
        invalid.insert(
            QStringLiteral("hyprland.input.accel_profile"),
            QStringLiteral("automatic")
        );
        invalidMaps.append(invalid);
        invalid = client.inputValues();
        invalid.insert(
            QStringLiteral("hyprland.input.scroll_method"),
            QStringLiteral("button")
        );
        invalidMaps.append(invalid);
        invalid = client.inputValues();
        invalid.insert(QStringLiteral("hyprland.input.scroll_button"), 301);
        invalidMaps.append(invalid);
        invalid = client.inputValues();
        invalid.insert(QStringLiteral("hyprland.input.scroll_button"), 1.5);
        invalidMaps.append(invalid);
        invalid = client.inputValues();
        invalid.insert(QStringLiteral("hyprland.input.scroll_button_lock"), 1);
        invalidMaps.append(invalid);
        invalid = client.inputValues();
        invalid.insert(QStringLiteral("hyprland.input.off_window_axis_events"), 4);
        invalidMaps.append(invalid);
        invalid = client.inputValues();
        invalid.insert(
            QStringLiteral("hyprland.input.emulate_discrete_scroll"),
            std::numeric_limits<double>::quiet_NaN()
        );
        invalidMaps.append(invalid);
        invalid = client.inputValues();
        invalid.insert(QStringLiteral("hyprland.input.touchpad.drag_3fg"), 3);
        invalidMaps.append(invalid);
        invalid = client.inputValues();
        invalid.insert(
            QStringLiteral("hyprland.input.touchpad.drag_lock"), 1.5
        );
        invalidMaps.append(invalid);
        invalid = client.inputValues();
        invalid.insert(
            QStringLiteral("hyprland.input.touchpad.tap_button_map"),
            QStringLiteral("automatic")
        );
        invalidMaps.append(invalid);
        invalid = client.inputValues();
        invalid.insert(
            QStringLiteral("hyprland.input.touchpad.clickfinger_behavior"),
            1
        );
        invalidMaps.append(invalid);
        invalid = client.inputValues();
        invalid.insert(
            QStringLiteral("hyprland.input.virtualkeyboard.share_states"), 3
        );
        invalidMaps.append(invalid);
        invalid = client.inputValues();
        invalid.insert(
            QStringLiteral("hyprland.input.virtualkeyboard.share_states"),
            1.5
        );
        invalidMaps.append(invalid);
        invalid = client.inputValues();
        invalid.insert(
            QStringLiteral("hyprland.input.virtualkeyboard.share_states"),
            QStringLiteral("1")
        );
        invalidMaps.append(invalid);
        invalid = client.inputValues();
        invalid.insert(
            QStringLiteral("hyprland.input.virtualkeyboard.share_states"),
            true
        );
        invalidMaps.append(invalid);
        invalid = client.inputValues();
        invalid.insert(
            QStringLiteral("hyprland.input.virtualkeyboard.share_states"),
            std::numeric_limits<double>::quiet_NaN()
        );
        invalidMaps.append(invalid);
        invalid = client.inputValues();
        invalid.insert(
            QStringLiteral("hyprland.input.virtualkeyboard.share_states"),
            std::numeric_limits<double>::infinity()
        );
        invalidMaps.append(invalid);
        invalid = client.inputValues();
        invalid.insert(
            QStringLiteral("hyprland.input.virtualkeyboard.share_states"),
            -std::numeric_limits<double>::infinity()
        );
        invalidMaps.append(invalid);

        for (const auto &invalidRotation : QVariantList{
                 -1,
                 360,
                 137.5,
                 QStringLiteral("137"),
                 true,
                 std::numeric_limits<double>::quiet_NaN(),
                 std::numeric_limits<double>::infinity(),
                 -std::numeric_limits<double>::infinity(),
             }) {
            invalid = client.inputValues();
            invalid.insert(
                QStringLiteral("hyprland.input.rotation"), invalidRotation
            );
            invalidMaps.append(invalid);
        }

        for (const auto &id : {
                 QStringLiteral("hyprland.input.touchdevice.transform"),
                 QStringLiteral("hyprland.input.tablet.transform"),
             }) {
            for (const auto &invalidTransform : QVariantList{
                     -1,
                     7,
                     3.5,
                     QStringLiteral("3"),
                     true,
                     std::numeric_limits<double>::quiet_NaN(),
                     std::numeric_limits<double>::infinity(),
                     -std::numeric_limits<double>::infinity(),
                 }) {
                invalid = client.inputValues();
                invalid.insert(id, invalidTransform);
                invalidMaps.append(invalid);
            }
        }

        for (const auto &id : {
                 QStringLiteral("hyprland.input.tablet.region_position"),
                 QStringLiteral("hyprland.input.tablet.region_size"),
             }) {
            for (const auto &invalidVector : QVariantList{
                     true,
                     QStringLiteral("0 0"),
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
                invalid = client.inputValues();
                invalid.insert(id, invalidVector);
                invalidMaps.append(invalid);
            }
        }
        invalid = client.inputValues();
        invalid.insert(
            QStringLiteral("hyprland.input.tablet.region_position"),
            QVariantList{-20000.000001, 0.0}
        );
        invalidMaps.append(invalid);
        invalid = client.inputValues();
        invalid.insert(
            QStringLiteral("hyprland.input.tablet.region_size"),
            QVariantList{0.0, 4000.000001}
        );
        invalidMaps.append(invalid);

        const QStringList strictBooleanIds{
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
            invalid = client.inputValues();
            invalid.insert(id, 1);
            invalidMaps.append(invalid);
            invalid = client.inputValues();
            invalid.insert(id, QStringLiteral("true"));
            invalidMaps.append(invalid);
        }

        for (const auto &invalidCursorTimeout : QVariantList{
                 -0.01,
                 20.01,
                 QStringLiteral("2.37"),
                 true,
                 std::numeric_limits<double>::quiet_NaN(),
                 std::numeric_limits<double>::infinity(),
                 -std::numeric_limits<double>::infinity(),
             }) {
            invalid = client.inputValues();
            invalid.insert(
                QStringLiteral("hyprland.cursor.inactive_timeout"),
                invalidCursorTimeout
            );
            invalidMaps.append(invalid);
        }

        for (const auto &invalidHotspotPadding : QVariantList{
                 -1,
                 21,
                 13.5,
                 QStringLiteral("13"),
                 true,
                 std::numeric_limits<double>::quiet_NaN(),
                 std::numeric_limits<double>::infinity(),
                 -std::numeric_limits<double>::infinity(),
             }) {
            invalid = client.inputValues();
            invalid.insert(
                QStringLiteral("hyprland.cursor.hotspot_padding"),
                invalidHotspotPadding
            );
            invalidMaps.append(invalid);
        }

        for (const auto &invalidTimeout : QVariantList{
                 9,
                 2001,
                 137.5,
                 QStringLiteral("1000"),
                 true,
                 std::numeric_limits<double>::quiet_NaN(),
                 std::numeric_limits<double>::infinity(),
             }) {
            invalid = client.inputValues();
            invalid.insert(
                QStringLiteral("hyprland.gestures.close_max_timeout"),
                invalidTimeout
            );
            invalidMaps.append(invalid);
        }

        for (const auto &map : invalidMaps) {
            client.saveInput(map, client.inputGestures());
            QCOMPARE(client.busy(), false);
            QCOMPARE(service_.replaceCallCount(), 0);
            QCOMPARE(service_.applyCallCount(), 0);
            QCOMPARE(
                client.inputErrorName(),
                QStringLiteral(
                    "org.hyprshelld.Client.Compositor.Error.InvalidInput"
                )
            );
            QVERIFY(client.appearanceErrorName().isEmpty());
            QVERIFY(client.lastErrorName().isEmpty());
        }

        client.saveInput(client.inputValues(), client.inputGestures());
        QCOMPARE(service_.replaceCallCount(), 0);
        QCOMPARE(
            client.inputErrorName(),
            QStringLiteral(
                "org.hyprshelld.Client.Compositor.Error.NoChanges"
            )
        );
        QVERIFY(client.appearanceErrorName().isEmpty());
        QVERIFY(client.lastErrorName().isEmpty());
    }

    void ownerLossClearsInputBusyStateOperationAndScopedError()
    {
        service_.setHoldReplaces(true);
        QVERIFY(service_.start());
        HyprShelld::CompositorClient client(bus_, nullptr);
        QTRY_VERIFY_WITH_TIMEOUT(client.inputAvailable(), 3000);

        auto invalid = client.inputValues();
        invalid.remove(QStringLiteral("hyprland.input.repeat_rate"));
        client.saveInput(invalid, client.inputGestures());
        QVERIFY(!client.inputErrorName().isEmpty());

        auto values = client.inputValues();
        values.insert(
            QStringLiteral("hyprland.input.resolve_binds_by_sym"), true
        );
        client.saveInput(values, client.inputGestures());
        QTRY_COMPARE_WITH_TIMEOUT(service_.heldReplaceCount(), 1, 3000);
        QCOMPARE(client.busy(), true);
        QCOMPARE(client.busyOperation(), QStringLiteral("input-save"));
        QVERIFY(client.inputErrorName().isEmpty());

        service_.stop();
        QTRY_VERIFY_WITH_TIMEOUT(!client.busy(), 3000);
        QCOMPARE(client.busyOperation(), QString{});
        QCOMPARE(client.available(), false);
        QCOMPARE(client.inputAvailable(), false);
        QCOMPARE(client.inputProjectionAvailable(), false);
        QCOMPARE(client.inputGesturesProjectionAvailable(), false);
        QVERIFY(client.inputErrorName().isEmpty());
        QVERIFY(client.inputValues().isEmpty());
        QVERIFY(client.inputGestures().isEmpty());
        QVERIFY(client.inputGestureCompatibility().isEmpty());
        QVERIFY(client.inputGestureActions().isEmpty());
    }

    void savesAndAppliesWindowsAcrossTheExactSharedPipeline()
    {
        auto initialSnapshot = QJsonDocument::fromJson(snapshotBytes()).object();
        auto initialOverrides = initialSnapshot.value(
            QStringLiteral("overrides")
        ).toObject();
        const auto unauthoredOverrides =
            unauthoredRenderAndXWaylandOverrides();
        for (auto iterator = unauthoredOverrides.constBegin();
             iterator != unauthoredOverrides.constEnd(); ++iterator) {
            initialOverrides.insert(iterator.key(), iterator.value());
        }
        initialSnapshot.insert(
            QStringLiteral("overrides"), initialOverrides
        );
        auto initialBytes = HyprShelld::Hyprland::JsonSupport::canonicalJson(
            initialSnapshot
        );
        initialBytes.append('\n');
        service_.setSnapshotBytes(std::move(initialBytes));
        QVERIFY(service_.start());
        HyprShelld::CompositorClient client(bus_, nullptr);
        QTRY_VERIFY_WITH_TIMEOUT(client.windowsAvailable(), 3000);
        const auto appearanceBefore = client.appearanceValues();
        const auto inputBefore = client.inputValues();
        const auto workspacesBefore = client.workspacesValues();
        const auto advancedBefore = client.advancedValues();
        auto values = client.windowsValues();
        values.insert(
            QStringLiteral("hyprland.general.layout"),
            QStringLiteral("master")
        );
        values.insert(
            QStringLiteral("hyprland.general.resize_on_border"), true
        );
        values.insert(
            QStringLiteral("hyprland.general.extend_border_grab_area"), 31
        );
        values.insert(QStringLiteral("hyprland.general.resize_corner"), 4.0);
        values.insert(QStringLiteral("hyprland.general.snap.enabled"), true);
        values.insert(
            QStringLiteral("hyprland.general.snap.monitor_gap"), 23
        );
        values.insert(QStringLiteral("hyprland.input.follow_mouse"), 3.0);
        values.insert(
            QStringLiteral("hyprland.input.follow_mouse_shrink"), 37
        );
        values.insert(
            QStringLiteral("hyprland.input.float_switch_override_focus"),
            2.0
        );
        values.insert(QStringLiteral("hyprland.input.focus_on_close"), 2.0);
        values.insert(
            QStringLiteral("hyprland.general.float_gaps"),
            QVariantList{0, 5, -6, 7}
        );
        values.insert(QStringLiteral("hyprland.general.gaps_workspaces"), 37);
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
            QStringLiteral("hyprland.dwindle.default_split_ratio"), 1.03
        );
        values.insert(QStringLiteral("hyprland.master.mfact"), 0.57);
        values.insert(QStringLiteral("hyprland.scrolling.column_width"), 0.53);
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
        values.insert(QStringLiteral("hyprland.group.groupbar.enabled"), false);
        values.insert(
            QStringLiteral("hyprland.group.groupbar.disable_when_only"), true
        );
        values.insert(
            QStringLiteral("hyprland.group.groupbar.font_family"),
            QStringLiteral("Iosevka Term")
        );
        values.insert(
            QStringLiteral("hyprland.group.groupbar.font_weight_active"), 735
        );
        values.insert(
            QStringLiteral("hyprland.group.groupbar.font_weight_inactive"), 325
        );
        values.insert(QStringLiteral("hyprland.group.groupbar.font_size"), 17);
        values.insert(QStringLiteral("hyprland.group.groupbar.gradients"), true);
        values.insert(QStringLiteral("hyprland.group.groupbar.height"), 29);
        values.insert(
            QStringLiteral("hyprland.group.groupbar.indicator_gap"), 7
        );
        values.insert(
            QStringLiteral("hyprland.group.groupbar.indicator_height"), 6
        );
        values.insert(QStringLiteral("hyprland.group.groupbar.stacked"), true);
        values.insert(QStringLiteral("hyprland.group.groupbar.priority"), 5);
        values.insert(
            QStringLiteral("hyprland.group.groupbar.render_titles"), false
        );
        values.insert(QStringLiteral("hyprland.group.groupbar.scrolling"), false);
        values.insert(
            QStringLiteral("hyprland.group.groupbar.middle_click_close"), false
        );
        values.insert(QStringLiteral("hyprland.group.groupbar.rounding"), 9);
        values.insert(
            QStringLiteral("hyprland.group.groupbar.rounding_power"), 2.573
        );
        values.insert(
            QStringLiteral("hyprland.group.groupbar.gradient_rounding"), 8
        );
        values.insert(
            QStringLiteral("hyprland.group.groupbar.gradient_rounding_power"),
            3.257
        );
        values.insert(
            QStringLiteral("hyprland.group.groupbar.round_only_edges"), false
        );
        values.insert(
            QStringLiteral(
                "hyprland.group.groupbar.gradient_round_only_edges"
            ),
            false
        );
        values.insert(QStringLiteral("hyprland.group.groupbar.gaps_out"), 11);
        values.insert(QStringLiteral("hyprland.group.groupbar.gaps_in"), 9);
        values.insert(
            QStringLiteral("hyprland.group.groupbar.keep_upper_gap"), false
        );
        values.insert(
            QStringLiteral("hyprland.group.groupbar.text_offset"), -7
        );
        values.insert(
            QStringLiteral("hyprland.group.groupbar.text_padding"), 13
        );
        values.insert(QStringLiteral("hyprland.group.groupbar.blur"), true);
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
        QStringList observedOperations;
        connect(
            &client,
            &HyprShelld::CompositorClient::busyOperationChanged,
            this,
            [&client, &observedOperations] {
                observedOperations.append(client.busyOperation());
            }
        );

        client.saveWindows(values);
        QTRY_COMPARE_WITH_TIMEOUT(client.revision(), 8ULL, 3000);
        QTRY_VERIFY_WITH_TIMEOUT(!client.busy(), 3000);
        QTRY_VERIFY_WITH_TIMEOUT(client.windowsAvailable(), 3000);
        QCOMPARE(client.appliedRevision(), 8ULL);
        QCOMPARE(client.applyState(), QStringLiteral("current"));
        QCOMPARE(client.requiredActivation(), QStringLiteral("none"));
        QCOMPARE(client.windowsValues(), values);
        QCOMPARE(client.appearanceValues(), appearanceBefore);
        QCOMPARE(client.inputValues(), inputBefore);
        QCOMPARE(client.workspacesValues(), workspacesBefore);
        QCOMPARE(client.advancedValues(), advancedBefore);
        const auto savedSnapshot = QJsonDocument::fromJson(
            service_.currentSnapshotBytes()
        ).object();
        const auto savedOverrides = savedSnapshot.value(
            QStringLiteral("overrides")
        ).toObject();
        QCOMPARE(savedOverrides.value(
            QStringLiteral("hyprland.input.follow_mouse_threshold")
        ).toDouble(), followMouseThreshold);
        for (auto iterator = unauthoredOverrides.constBegin();
             iterator != unauthoredOverrides.constEnd(); ++iterator) {
            QCOMPARE(savedOverrides.value(iterator.key()), iterator.value());
        }
        for (const auto &field : {
                 QStringLiteral("monitors"),
                 QStringLiteral("devices"),
                 QStringLiteral("curves"),
                 QStringLiteral("animations"),
                 QStringLiteral("gestures"),
                 QStringLiteral("workspaceRules"),
                 QStringLiteral("windowRules"),
                 QStringLiteral("layerRules"),
                 QStringLiteral("bindings"),
                 QStringLiteral("submaps"),
                 QStringLiteral("permissions"),
                 QStringLiteral("environment"),
             }) {
            QCOMPARE(savedSnapshot.value(field), initialSnapshot.value(field));
        }
        QCOMPARE(service_.replaceCallCount(), 1);
        QCOMPARE(service_.applyCallCount(), 1);
        QVERIFY(service_.snapshotCallCount() >= 2);
        QVERIFY(observedOperations.contains(QStringLiteral("windows-save")));
        QVERIFY(observedOperations.contains(QStringLiteral("windows-apply")));
        QCOMPARE(client.busyOperation(), QString{});
        QVERIFY(client.windowsErrorName().isEmpty());
        QVERIFY(client.appearanceErrorName().isEmpty());
        QVERIFY(client.inputErrorName().isEmpty());
        QVERIFY(client.workspacesErrorName().isEmpty());
        QVERIFY(client.lastErrorName().isEmpty());
    }

    void retriesAnExactLostWindowsReplaceWithoutIncrementingTwice()
    {
        service_.setAmbiguousFirstReplace(true);
        QVERIFY(service_.start());
        HyprShelld::CompositorClient client(bus_, nullptr);
        QTRY_VERIFY_WITH_TIMEOUT(client.windowsAvailable(), 3000);
        auto values = client.windowsValues();
        values.insert(
            QStringLiteral("hyprland.input.follow_mouse_threshold"),
            234567.890625
        );
        values.insert(QStringLiteral("hyprland.misc.enable_swallow"), true);
        values.insert(
            QStringLiteral("hyprland.misc.swallow_regex"),
            QStringLiteral("^Terminal$")
        );
        values.insert(
            QStringLiteral("hyprland.misc.swallow_exception_regex"),
            QStringLiteral("^scratch$")
        );

        client.saveWindows(values);
        QTRY_VERIFY_WITH_TIMEOUT(!client.busy(), 3000);
        QTRY_COMPARE_WITH_TIMEOUT(client.revision(), 8ULL, 3000);
        QTRY_VERIFY_WITH_TIMEOUT(client.windowsAvailable(), 3000);
        QCOMPARE(service_.replaceCallCount(), 2);
        QCOMPARE(service_.applyCallCount(), 1);
        QCOMPARE(client.appliedRevision(), 8ULL);
        QCOMPARE(client.windowsValues(), values);
        QVERIFY(client.windowsErrorName().isEmpty());
    }

    void retriesAnAmbiguousWindowsApplyForOnlyTheExactSavedRevision()
    {
        service_.setAmbiguousFirstApplyWithoutProperties(true);
        QVERIFY(service_.start());
        HyprShelld::CompositorClient client(bus_, nullptr);
        QTRY_VERIFY_WITH_TIMEOUT(client.windowsAvailable(), 3000);
        auto values = client.windowsValues();
        values.insert(
            QStringLiteral("hyprland.input.follow_mouse_threshold"),
            345678.9015625
        );

        client.saveWindows(values);
        QTRY_VERIFY_WITH_TIMEOUT(!client.busy(), 3000);
        QTRY_VERIFY_WITH_TIMEOUT(client.windowsAvailable(), 3000);
        QCOMPARE(client.revision(), 8ULL);
        QCOMPARE(client.appliedRevision(), 8ULL);
        QCOMPARE(service_.replaceCallCount(), 1);
        QCOMPARE(service_.applyCallCount(), 2);
        QCOMPARE(client.windowsValues(), values);
        QVERIFY(client.lastErrorName().isEmpty());
    }

    void retainsSavedWindowsAfterApplyFailureAndRetriesWithNeutralScope()
    {
        service_.setFailNextApply(true);
        QVERIFY(service_.start());
        HyprShelld::CompositorClient client(bus_, nullptr);
        QTRY_VERIFY_WITH_TIMEOUT(client.windowsAvailable(), 3000);
        auto values = client.windowsValues();
        values.insert(
            QStringLiteral("hyprland.input.follow_mouse_threshold"),
            456789.0125
        );

        client.saveWindows(values);
        QTRY_VERIFY_WITH_TIMEOUT(!client.busy(), 3000);
        QTRY_COMPARE_WITH_TIMEOUT(client.revision(), 8ULL, 3000);
        QTRY_VERIFY_WITH_TIMEOUT(client.retryApplyAvailable(), 3000);
        QCOMPARE(client.appliedRevision(), baselineRevision);
        QCOMPARE(client.applyState(), QStringLiteral("retained"));
        QCOMPARE(client.requiredActivation(), QStringLiteral("reload"));
        QCOMPARE(client.appearanceAvailable(), false);
        QCOMPARE(client.inputAvailable(), false);
        QCOMPARE(client.windowsAvailable(), false);
        QCOMPARE(client.workspacesAvailable(), false);
        QCOMPARE(client.appearanceProjectionAvailable(), true);
        QCOMPARE(client.inputProjectionAvailable(), true);
        QCOMPARE(client.windowsProjectionAvailable(), true);
        QCOMPARE(client.workspacesProjectionAvailable(), true);
        QCOMPARE(client.windowsValues(), values);
        QVERIFY(client.windowsErrorName().isEmpty());
        QVERIFY(client.appearanceErrorName().isEmpty());
        QVERIFY(client.inputErrorName().isEmpty());
        QVERIFY(client.workspacesErrorName().isEmpty());
        QCOMPARE(client.lastErrorOperation(), QStringLiteral("windows-apply"));
        QCOMPARE(
            client.lastErrorName(),
            QStringLiteral("org.hyprshelld.Compositor1.Error.ApplyFailed")
        );
        QVERIFY(client.recoveryAvailable());

        QStringList observedOperations;
        connect(
            &client,
            &HyprShelld::CompositorClient::busyOperationChanged,
            this,
            [&client, &observedOperations] {
                observedOperations.append(client.busyOperation());
            }
        );
        client.retryApply();
        QTRY_VERIFY_WITH_TIMEOUT(!client.busy(), 3000);
        QTRY_VERIFY_WITH_TIMEOUT(client.windowsAvailable(), 3000);
        QCOMPARE(client.appliedRevision(), 8ULL);
        QCOMPARE(service_.applyCallCount(), 2);
        QVERIFY(observedOperations.contains(
            QStringLiteral("compositor-apply")
        ));
        QVERIFY(client.lastErrorOperation().isEmpty());
        QVERIFY(client.lastErrorName().isEmpty());
    }

    void recoversTheWholeSnapshotAfterAWindowsApplyFailure()
    {
        service_.setFailNextApply(true);
        QVERIFY(service_.start());
        HyprShelld::CompositorClient client(bus_, nullptr);
        QTRY_VERIFY_WITH_TIMEOUT(client.windowsAvailable(), 3000);

        auto values = client.windowsValues();
        values.insert(
            QStringLiteral("hyprland.input.follow_mouse_threshold"),
            567890.1234375
        );
        client.saveWindows(values);
        QTRY_VERIFY_WITH_TIMEOUT(client.recoveryAvailable(), 3000);
        QCOMPARE(client.windowsValues(), values);

        client.recoverConfiguration();
        QTRY_VERIFY_WITH_TIMEOUT(!client.busy(), 3000);
        QTRY_COMPARE_WITH_TIMEOUT(client.revision(), 9ULL, 3000);
        QTRY_VERIFY_WITH_TIMEOUT(client.windowsAvailable(), 3000);
        QCOMPARE(client.appliedRevision(), 9ULL);
        QCOMPARE(service_.recoverCallCount(), 1);
        QCOMPARE(client.windowsValues(), windowsDefaults());
        QVERIFY(client.windowsErrorName().isEmpty());
    }

    void windowsReplaceFailureIsScopedAndDoesNotPoisonOtherGroups()
    {
        service_.setFailNextReplace(true);
        QVERIFY(service_.start());
        HyprShelld::CompositorClient client(bus_, nullptr);
        QTRY_VERIFY_WITH_TIMEOUT(client.windowsAvailable(), 3000);
        auto values = client.windowsValues();
        values.insert(
            QStringLiteral("hyprland.input.follow_mouse_threshold"),
            678901.234375
        );

        client.saveWindows(values);
        QTRY_VERIFY_WITH_TIMEOUT(!client.busy(), 3000);
        QTRY_COMPARE_WITH_TIMEOUT(service_.replaceCallCount(), 1, 3000);
        QCOMPARE(service_.applyCallCount(), 0);
        QCOMPARE(client.revision(), baselineRevision);
        QTRY_VERIFY_WITH_TIMEOUT(client.windowsAvailable(), 3000);
        QTRY_VERIFY_WITH_TIMEOUT(client.appearanceAvailable(), 3000);
        QTRY_VERIFY_WITH_TIMEOUT(client.inputAvailable(), 3000);
        QTRY_VERIFY_WITH_TIMEOUT(client.workspacesAvailable(), 3000);
        QCOMPARE(
            client.windowsErrorName(),
            QStringLiteral(
                "org.hyprshelld.Compositor1.Error.InvalidSnapshot"
            )
        );
        QCOMPARE(
            client.windowsErrorMessage(),
            QStringLiteral("Injected replacement failure")
        );
        QVERIFY(client.appearanceErrorName().isEmpty());
        QVERIFY(client.inputErrorName().isEmpty());
        QVERIFY(client.workspacesErrorName().isEmpty());
        QVERIFY(client.lastErrorName().isEmpty());
    }

    void rejectsInvalidWindowsMapsWithoutCallingTheService()
    {
        QVERIFY(service_.start());
        HyprShelld::CompositorClient client(bus_, nullptr);
        QTRY_VERIFY_WITH_TIMEOUT(client.windowsAvailable(), 3000);

        QList<QVariantMap> invalidMaps;
        auto invalid = client.windowsValues();
        invalid.remove(QStringLiteral("hyprland.general.layout"));
        invalidMaps.append(invalid);
        invalid = client.windowsValues();
        invalid.insert(QStringLiteral("hyprland.windows.unknown"), true);
        invalidMaps.append(invalid);
        invalid = client.windowsValues();
        invalid.remove(
            QStringLiteral("hyprland.input.follow_mouse_threshold")
        );
        invalidMaps.append(invalid);
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
            invalid = client.windowsValues();
            invalid.insert(
                QStringLiteral("hyprland.input.follow_mouse_threshold"),
                value
            );
            invalidMaps.append(invalid);
        }
        invalid = client.windowsValues();
        invalid.insert(
            QStringLiteral("hyprland.general.extend_border_grab_area"), 15.5
        );
        invalidMaps.append(invalid);
        invalid = client.windowsValues();
        invalid.insert(
            QStringLiteral("hyprland.general.resize_corner"),
            std::numeric_limits<double>::quiet_NaN()
        );
        invalidMaps.append(invalid);
        invalid = client.windowsValues();
        invalid.insert(QStringLiteral("hyprland.input.follow_mouse"), 1.5);
        invalidMaps.append(invalid);
        invalid = client.windowsValues();
        invalid.insert(
            QStringLiteral("hyprland.input.focus_on_close"),
            QStringLiteral("1")
        );
        invalidMaps.append(invalid);
        invalid = client.windowsValues();
        invalid.insert(QStringLiteral("hyprland.group.drag_into_group"), 3);
        invalidMaps.append(invalid);
        invalid = client.windowsValues();
        invalid.insert(
            QStringLiteral("hyprland.group.drag_into_group"), 1.5
        );
        invalidMaps.append(invalid);
        invalid = client.windowsValues();
        invalid.insert(
            QStringLiteral("hyprland.binds.focus_preferred_method"), 2
        );
        invalidMaps.append(invalid);
        invalid = client.windowsValues();
        invalid.insert(
            QStringLiteral("hyprland.binds.focus_preferred_method"), 0.5
        );
        invalidMaps.append(invalid);
        for (const auto &id : {
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
                 QStringLiteral(
                     "hyprland.binds.movefocus_cycles_fullscreen"
                 ),
                 QStringLiteral(
                     "hyprland.binds.movefocus_cycles_groupfirst"
                 ),
                 QStringLiteral(
                     "hyprland.binds.window_direction_monitor_fallback"
                 ),
                 QStringLiteral("hyprland.misc.enable_swallow"),
             }) {
            invalid = client.windowsValues();
            invalid.insert(id, 1);
            invalidMaps.append(invalid);
        }
        invalid = client.windowsValues();
        invalid.insert(
            QStringLiteral("hyprland.general.layout"),
            QStringLiteral("lua:unreviewed")
        );
        invalidMaps.append(invalid);
        invalid = client.windowsValues();
        invalid.insert(
            QStringLiteral("hyprland.general.float_gaps"),
            QVariantList{0, 1, 2}
        );
        invalidMaps.append(invalid);
        invalid = client.windowsValues();
        invalid.insert(
            QStringLiteral("hyprland.general.float_gaps"),
            QVariantList{
                0,
                1,
                2,
                QVariant::fromValue<qulonglong>(
                    std::numeric_limits<qulonglong>::max()
                ),
            }
        );
        invalidMaps.append(invalid);
        invalid = client.windowsValues();
        invalid.insert(
            QStringLiteral("hyprland.layout.single_window_aspect_ratio"),
            QVariantList{16.0, std::numeric_limits<double>::quiet_NaN()}
        );
        invalidMaps.append(invalid);
        invalid = client.windowsValues();
        invalid.insert(QStringLiteral("hyprland.group.groupbar.enabled"), 1);
        invalidMaps.append(invalid);
        invalid = client.windowsValues();
        invalid.insert(QStringLiteral("hyprland.group.groupbar.height"), 14.5);
        invalidMaps.append(invalid);
        invalid = client.windowsValues();
        invalid.insert(QStringLiteral("hyprland.group.groupbar.height"), 65);
        invalidMaps.append(invalid);
        for (const QVariant &badWeight : {
                 QVariant{-1},
                 QVariant{2147483648ULL},
                 QVariant{400.5},
                 QVariant{QStringLiteral("400")},
                 QVariant{true},
             }) {
            invalid = client.windowsValues();
            invalid.insert(
                QStringLiteral("hyprland.group.groupbar.font_weight_active"),
                badWeight
            );
            invalidMaps.append(invalid);
        }
        invalid = client.windowsValues();
        invalid.insert(
            QStringLiteral("hyprland.group.groupbar.rounding_power"),
            std::numeric_limits<double>::quiet_NaN()
        );
        invalidMaps.append(invalid);
        invalid = client.windowsValues();
        invalid.insert(
            QStringLiteral("hyprland.group.groupbar.gradient_rounding_power"),
            10.01
        );
        invalidMaps.append(invalid);
        invalid = client.windowsValues();
        invalid.insert(
            QStringLiteral("hyprland.group.groupbar.font_family"),
            QStringLiteral("bad") + QChar::Null + QStringLiteral("font")
        );
        invalidMaps.append(invalid);
        invalid = client.windowsValues();
        invalid.insert(
            QStringLiteral("hyprland.group.groupbar.font_family"),
            QString(4097, QLatin1Char('x'))
        );
        invalidMaps.append(invalid);
        for (const auto &id : {
                 QStringLiteral("hyprland.misc.swallow_regex"),
                 QStringLiteral("hyprland.misc.swallow_exception_regex"),
             }) {
            invalid = client.windowsValues();
            invalid.insert(id, 1);
            invalidMaps.append(invalid);
            invalid = client.windowsValues();
            invalid.insert(id, QString(4097, QLatin1Char('x')));
            invalidMaps.append(invalid);
            invalid = client.windowsValues();
            invalid.insert(
                id,
                QStringLiteral("bad") + QChar::Null
                    + QStringLiteral("pattern")
            );
            invalidMaps.append(invalid);
            invalid = client.windowsValues();
            invalid.insert(id, QStringLiteral("("));
            invalidMaps.append(invalid);
        }

        for (const auto &map : invalidMaps) {
            client.saveWindows(map);
            QCOMPARE(client.busy(), false);
            QCOMPARE(service_.replaceCallCount(), 0);
            QCOMPARE(service_.applyCallCount(), 0);
            QCOMPARE(
                client.windowsErrorName(),
                QStringLiteral(
                    "org.hyprshelld.Client.Compositor.Error.InvalidWindows"
                )
            );
            QVERIFY(client.appearanceErrorName().isEmpty());
            QVERIFY(client.inputErrorName().isEmpty());
            QVERIFY(client.workspacesErrorName().isEmpty());
            QVERIFY(client.lastErrorName().isEmpty());
        }

        invalid = client.windowsValues();
        invalid.insert(
            QStringLiteral("hyprland.misc.swallow_regex"),
            QStringLiteral("(")
        );
        client.saveWindows(invalid);
        QCOMPARE(service_.replaceCallCount(), 0);
        QCOMPARE(service_.applyCallCount(), 0);
        QVERIFY(client.windowsErrorMessage().contains(
            QStringLiteral("hyprland.misc.swallow_regex")
        ));

        client.saveWindows(client.windowsValues());
        QCOMPARE(service_.replaceCallCount(), 0);
        QCOMPARE(
            client.windowsErrorName(),
            QStringLiteral(
                "org.hyprshelld.Client.Compositor.Error.NoChanges"
            )
        );
        QVERIFY(client.appearanceErrorName().isEmpty());
        QVERIFY(client.inputErrorName().isEmpty());
        QVERIFY(client.workspacesErrorName().isEmpty());
        QVERIFY(client.lastErrorName().isEmpty());
    }

    void ownerLossClearsWindowsBusyStateOperationAndScopedError()
    {
        service_.setHoldReplaces(true);
        QVERIFY(service_.start());
        HyprShelld::CompositorClient client(bus_, nullptr);
        QTRY_VERIFY_WITH_TIMEOUT(client.windowsAvailable(), 3000);

        auto invalid = client.windowsValues();
        invalid.remove(QStringLiteral("hyprland.general.layout"));
        client.saveWindows(invalid);
        QVERIFY(!client.windowsErrorName().isEmpty());

        auto values = client.windowsValues();
        values.insert(
            QStringLiteral("hyprland.input.follow_mouse_threshold"),
            789012.34375
        );
        client.saveWindows(values);
        QTRY_COMPARE_WITH_TIMEOUT(service_.heldReplaceCount(), 1, 3000);
        QCOMPARE(client.busy(), true);
        QCOMPARE(client.busyOperation(), QStringLiteral("windows-save"));
        QVERIFY(client.windowsErrorName().isEmpty());

        service_.stop();
        QTRY_VERIFY_WITH_TIMEOUT(!client.busy(), 3000);
        QCOMPARE(client.busyOperation(), QString{});
        QCOMPARE(client.available(), false);
        QCOMPARE(client.windowsProjectionAvailable(), false);
        QCOMPARE(client.workspacesProjectionAvailable(), false);
        QVERIFY(client.windowsErrorName().isEmpty());
        QVERIFY(client.windowsValues().isEmpty());
        QVERIFY(client.windowsOptions().isEmpty());
        QVERIFY(client.workspacesErrorName().isEmpty());
    }

    void savesAndAppliesAdvancedAcrossTheExactSharedPipeline()
    {
        auto initialSnapshot = QJsonDocument::fromJson(snapshotBytes()).object();
        auto initialOverrides = initialSnapshot.value(
            QStringLiteral("overrides")
        ).toObject();
        const auto unauthoredOverrides =
            unauthoredRenderAndXWaylandOverrides();
        for (auto it = unauthoredOverrides.constBegin();
             it != unauthoredOverrides.constEnd(); ++it) {
            initialOverrides.insert(it.key(), it.value());
        }
        initialSnapshot.insert(
            QStringLiteral("overrides"), initialOverrides
        );
        auto initialBytes = HyprShelld::Hyprland::JsonSupport::canonicalJson(
            initialSnapshot
        );
        initialBytes.append('\n');
        service_.setSnapshotBytes(std::move(initialBytes));
        QVERIFY(service_.start());
        HyprShelld::CompositorClient client(bus_, nullptr);
        QTRY_VERIFY_WITH_TIMEOUT(client.advancedAvailable(), 3000);
        const auto appearanceBefore = client.appearanceValues();
        const auto inputBefore = client.inputValues();
        QCOMPARE(inputBefore.size(), 49);
        const auto windowsBefore = client.windowsValues();
        const auto workspacesBefore = client.workspacesValues();
        const auto workspaceRulesBefore = client.workspaceRules();
        const auto windowRulesBefore = client.windowRules();
        const auto layerRulesBefore = client.layerRules();
        QStringList observedOperations;
        connect(
            &client,
            &HyprShelld::CompositorClient::busyOperationChanged,
            this,
            [&client, &observedOperations] {
                observedOperations.append(client.busyOperation());
            }
        );

        const auto values = changedAdvancedValues();
        client.saveAdvanced(values);
        QTRY_VERIFY_WITH_TIMEOUT(!client.busy(), 3000);
        QTRY_COMPARE_WITH_TIMEOUT(client.revision(), 8ULL, 3000);
        QTRY_VERIFY_WITH_TIMEOUT(client.advancedAvailable(), 3000);
        QCOMPARE(client.appliedRevision(), 8ULL);
        QCOMPARE(client.applyState(), QStringLiteral("current"));
        QCOMPARE(client.requiredActivation(), QStringLiteral("none"));
        QCOMPARE(client.advancedValues(), values);
        QCOMPARE(client.appearanceValues(), appearanceBefore);
        QCOMPARE(client.inputValues(), inputBefore);
        QCOMPARE(client.windowsValues(), windowsBefore);
        QCOMPARE(client.workspacesValues(), workspacesBefore);
        QCOMPARE(client.workspaceRules(), workspaceRulesBefore);
        QCOMPARE(client.windowRules(), windowRulesBefore);
        QCOMPARE(client.layerRules(), layerRulesBefore);
        QCOMPARE(service_.replaceCallCount(), 1);
        QCOMPARE(service_.applyCallCount(), 1);
        QVERIFY(service_.snapshotCallCount() >= 2);
        QVERIFY(observedOperations.contains(QStringLiteral("advanced-save")));
        QVERIFY(observedOperations.contains(QStringLiteral("advanced-apply")));
        const auto overrides = QJsonDocument::fromJson(
            service_.currentSnapshotBytes()
        ).object().value(QStringLiteral("overrides")).toObject();
        for (auto it = values.cbegin(); it != values.cend(); ++it) {
            QCOMPARE(overrides.value(it.key()).toVariant(), it.value());
        }
        for (auto it = unauthoredOverrides.constBegin();
             it != unauthoredOverrides.constEnd(); ++it) {
            QCOMPARE(overrides.value(it.key()), it.value());
        }
        QCOMPARE(overrides.value(
            QStringLiteral("hyprland.xwayland.use_nearest_neighbor")
        ), QJsonValue(false));
        QCOMPARE(overrides.value(
            QStringLiteral("hyprland.render.expand_undersized_textures")
        ), QJsonValue(false));
        QCOMPARE(overrides.value(
            QStringLiteral("hyprland.render.direct_scanout")
        ), QJsonValue(2));
        QCOMPARE(overrides.value(
            QStringLiteral("hyprland.render.fp16_sdr_tf")
        ), QJsonValue(1));
        QCOMPARE(overrides.value(
            QStringLiteral("hyprland.render.xp_mode")
        ), QJsonValue(true));
        QCOMPARE(overrides.value(
            QStringLiteral("hyprland.input-capture.capture_modifiers")
        ), QJsonValue(true));
        QCOMPARE(overrides.value(
            QStringLiteral("hyprland.input-capture.enforce_barriers")
        ), QJsonValue(false));
        QVERIFY(client.advancedErrorName().isEmpty());
        QVERIFY(client.appearanceErrorName().isEmpty());
        QVERIFY(client.inputErrorName().isEmpty());
        QVERIFY(client.windowsErrorName().isEmpty());
        QVERIFY(client.workspacesErrorName().isEmpty());
        QVERIFY(client.rulesErrorName().isEmpty());
        QVERIFY(client.lastErrorName().isEmpty());
    }

    void retriesLostAdvancedReplaceAndApplyForTheExactSavedRevision()
    {
        service_.setAmbiguousFirstReplace(true);
        service_.setAmbiguousFirstApplyWithoutProperties(true);
        QVERIFY(service_.start());
        HyprShelld::CompositorClient client(bus_, nullptr);
        QTRY_VERIFY_WITH_TIMEOUT(client.advancedAvailable(), 3000);

        const auto values = changedAdvancedValues();
        client.saveAdvanced(values);
        QTRY_VERIFY_WITH_TIMEOUT(!client.busy(), 3000);
        QTRY_COMPARE_WITH_TIMEOUT(client.revision(), 8ULL, 3000);
        QTRY_VERIFY_WITH_TIMEOUT(client.advancedAvailable(), 3000);
        QCOMPARE(client.appliedRevision(), 8ULL);
        QCOMPARE(service_.replaceCallCount(), 2);
        QCOMPARE(service_.applyCallCount(), 2);
        QCOMPARE(client.advancedValues(), values);
        QCOMPARE(client.advancedValues().value(
            QStringLiteral("hyprland.xwayland.use_nearest_neighbor")
        ).toBool(), false);
        QCOMPARE(client.advancedValues().value(
            QStringLiteral("hyprland.render.expand_undersized_textures")
        ).toBool(), false);
        QCOMPARE(client.advancedValues().value(
            QStringLiteral("hyprland.render.direct_scanout")
        ).toInt(), 2);
        QCOMPARE(client.advancedValues().value(
            QStringLiteral("hyprland.render.fp16_sdr_tf")
        ).toInt(), 1);
        QCOMPARE(client.advancedValues().value(
            QStringLiteral("hyprland.render.xp_mode")
        ).toBool(), true);
        QCOMPARE(client.advancedValues().value(
            QStringLiteral("hyprland.input-capture.capture_modifiers")
        ).toBool(), true);
        QCOMPARE(client.advancedValues().value(
            QStringLiteral("hyprland.input-capture.enforce_barriers")
        ).toBool(), false);
        QVERIFY(client.advancedErrorName().isEmpty());
        QVERIFY(client.lastErrorName().isEmpty());
    }

    void retainsSavedAdvancedAfterApplyFailureAndRetriesExplicitly()
    {
        service_.setFailNextApply(true);
        QVERIFY(service_.start());
        HyprShelld::CompositorClient client(bus_, nullptr);
        QTRY_VERIFY_WITH_TIMEOUT(client.advancedAvailable(), 3000);

        auto values = changedAdvancedValues();
        values.insert(
            QStringLiteral("hyprland.misc.session_lock_xray"), false
        );
        client.saveAdvanced(values);
        QTRY_VERIFY_WITH_TIMEOUT(!client.busy(), 3000);
        QTRY_COMPARE_WITH_TIMEOUT(client.revision(), 8ULL, 3000);
        QTRY_VERIFY_WITH_TIMEOUT(client.retryApplyAvailable(), 3000);
        QCOMPARE(client.appliedRevision(), baselineRevision);
        QCOMPARE(client.applyState(), QStringLiteral("retained"));
        QCOMPARE(client.requiredActivation(), QStringLiteral("reload"));
        QCOMPARE(client.advancedAvailable(), false);
        QCOMPARE(client.advancedProjectionAvailable(), true);
        QCOMPARE(client.advancedValues(), values);
        const auto savedOverrides = QJsonDocument::fromJson(
            service_.currentSnapshotBytes()
        ).object().value(QStringLiteral("overrides")).toObject();
        QVERIFY(!savedOverrides.contains(
            QStringLiteral("hyprland.misc.session_lock_xray")
        ));
        QCOMPARE(savedOverrides.value(
            QStringLiteral("hyprland.misc.session_lock_blur")
        ).toBool(), true);
        QCOMPARE(savedOverrides.value(
            QStringLiteral("hyprland.xwayland.use_nearest_neighbor")
        ).toBool(), false);
        QCOMPARE(savedOverrides.value(
            QStringLiteral("hyprland.render.expand_undersized_textures")
        ).toBool(), false);
        QCOMPARE(savedOverrides.value(
            QStringLiteral("hyprland.render.direct_scanout")
        ).toInt(), 2);
        QCOMPARE(savedOverrides.value(
            QStringLiteral("hyprland.render.fp16_sdr_tf")
        ).toInt(), 1);
        QCOMPARE(savedOverrides.value(
            QStringLiteral("hyprland.render.xp_mode")
        ).toBool(), true);
        QCOMPARE(savedOverrides.value(
            QStringLiteral("hyprland.input-capture.capture_modifiers")
        ).toBool(), true);
        QCOMPARE(savedOverrides.value(
            QStringLiteral("hyprland.input-capture.enforce_barriers")
        ).toBool(), false);
        QVERIFY(client.advancedErrorName().isEmpty());
        QCOMPARE(client.lastErrorOperation(), QStringLiteral("advanced-apply"));
        QCOMPARE(
            client.lastErrorName(),
            QStringLiteral("org.hyprshelld.Compositor1.Error.ApplyFailed")
        );
        QVERIFY(client.recoveryAvailable());

        client.retryApply();
        QTRY_VERIFY_WITH_TIMEOUT(!client.busy(), 3000);
        QTRY_VERIFY_WITH_TIMEOUT(client.advancedAvailable(), 3000);
        QCOMPARE(client.appliedRevision(), 8ULL);
        QCOMPARE(service_.applyCallCount(), 2);
        QCOMPARE(client.advancedValues(), values);
        QVERIFY(client.lastErrorOperation().isEmpty());
        QVERIFY(client.lastErrorName().isEmpty());
    }

    void recoversTheWholeSnapshotAfterAnAdvancedApplyFailure()
    {
        service_.setFailNextApply(true);
        QVERIFY(service_.start());
        HyprShelld::CompositorClient client(bus_, nullptr);
        QTRY_VERIFY_WITH_TIMEOUT(client.advancedAvailable(), 3000);

        client.saveAdvanced(changedAdvancedValues());
        QTRY_VERIFY_WITH_TIMEOUT(client.recoveryAvailable(), 3000);
        QCOMPARE(client.advancedValues(), changedAdvancedValues());
        QCOMPARE(client.advancedValues().value(
            QStringLiteral("hyprland.xwayland.use_nearest_neighbor")
        ).toBool(), false);
        QCOMPARE(client.advancedValues().value(
            QStringLiteral("hyprland.render.expand_undersized_textures")
        ).toBool(), false);
        QCOMPARE(client.advancedValues().value(
            QStringLiteral("hyprland.render.direct_scanout")
        ).toInt(), 2);
        QCOMPARE(client.advancedValues().value(
            QStringLiteral("hyprland.render.fp16_sdr_tf")
        ).toInt(), 1);
        QCOMPARE(client.advancedValues().value(
            QStringLiteral("hyprland.render.xp_mode")
        ).toBool(), true);
        QCOMPARE(client.advancedValues().value(
            QStringLiteral("hyprland.input-capture.capture_modifiers")
        ).toBool(), true);
        QCOMPARE(client.advancedValues().value(
            QStringLiteral("hyprland.input-capture.enforce_barriers")
        ).toBool(), false);

        client.recoverConfiguration();
        QTRY_VERIFY_WITH_TIMEOUT(!client.busy(), 3000);
        QTRY_COMPARE_WITH_TIMEOUT(client.revision(), 9ULL, 3000);
        QTRY_VERIFY_WITH_TIMEOUT(client.advancedAvailable(), 3000);
        QCOMPARE(client.appliedRevision(), 9ULL);
        QCOMPARE(service_.recoverCallCount(), 1);
        QCOMPARE(client.advancedValues(), advancedDefaults());
        QCOMPARE(client.advancedValues().value(
            QStringLiteral("hyprland.xwayland.use_nearest_neighbor")
        ).toBool(), true);
        QCOMPARE(client.advancedValues().value(
            QStringLiteral("hyprland.render.expand_undersized_textures")
        ).toBool(), true);
        QCOMPARE(client.advancedValues().value(
            QStringLiteral("hyprland.render.direct_scanout")
        ).toInt(), 0);
        QCOMPARE(client.advancedValues().value(
            QStringLiteral("hyprland.render.fp16_sdr_tf")
        ).toInt(), 0);
        QCOMPARE(client.advancedValues().value(
            QStringLiteral("hyprland.render.xp_mode")
        ).toBool(), false);
        QCOMPARE(client.advancedValues().value(
            QStringLiteral("hyprland.input-capture.capture_modifiers")
        ).toBool(), false);
        QCOMPARE(client.advancedValues().value(
            QStringLiteral("hyprland.input-capture.enforce_barriers")
        ).toBool(), true);
        const auto recoveredOverrides = QJsonDocument::fromJson(
            service_.currentSnapshotBytes()
        ).object().value(QStringLiteral("overrides")).toObject();
        QVERIFY(!recoveredOverrides.contains(
            QStringLiteral("hyprland.render.fp16_sdr_tf")
        ));
        QVERIFY(!recoveredOverrides.contains(
            QStringLiteral("hyprland.render.xp_mode")
        ));
        QVERIFY(!recoveredOverrides.contains(
            QStringLiteral("hyprland.input-capture.capture_modifiers")
        ));
        QVERIFY(!recoveredOverrides.contains(
            QStringLiteral("hyprland.input-capture.enforce_barriers")
        ));
        QVERIFY(client.advancedErrorName().isEmpty());
    }

    void rejectsInvalidAdvancedMapsAndScopesReplacementErrors()
    {
        QVERIFY(service_.start());
        HyprShelld::CompositorClient client(bus_, nullptr);
        QTRY_VERIFY_WITH_TIMEOUT(client.advancedAvailable(), 3000);

        QList<QVariantMap> invalidMaps;
        auto invalid = client.advancedValues();
        invalid.remove(
            QStringLiteral("hyprland.misc.disable_scale_notification")
        );
        invalidMaps.append(invalid);
        invalid = client.advancedValues();
        invalid.remove(QStringLiteral("hyprland.misc.session_lock_blur"));
        invalidMaps.append(invalid);
        invalid = client.advancedValues();
        invalid.remove(
            QStringLiteral("hyprland.xwayland.use_nearest_neighbor")
        );
        invalidMaps.append(invalid);
        invalid = client.advancedValues();
        invalid.remove(
            QStringLiteral("hyprland.render.expand_undersized_textures")
        );
        invalidMaps.append(invalid);
        invalid = client.advancedValues();
        invalid.remove(QStringLiteral("hyprland.render.direct_scanout"));
        invalidMaps.append(invalid);
        invalid = client.advancedValues();
        invalid.remove(QStringLiteral("hyprland.render.fp16_sdr_tf"));
        invalidMaps.append(invalid);
        invalid = client.advancedValues();
        invalid.remove(QStringLiteral("hyprland.render.xp_mode"));
        invalidMaps.append(invalid);
        invalid = client.advancedValues();
        invalid.remove(
            QStringLiteral("hyprland.input-capture.capture_modifiers")
        );
        invalidMaps.append(invalid);
        invalid = client.advancedValues();
        invalid.remove(
            QStringLiteral("hyprland.input-capture.enforce_barriers")
        );
        invalidMaps.append(invalid);
        invalid = client.advancedValues();
        invalid.insert(QStringLiteral("hyprland.misc.unknown"), false);
        invalidMaps.append(invalid);
        invalid = client.advancedValues();
        invalid.insert(
            QStringLiteral("hyprland.misc.allow_session_lock_restore"), 1
        );
        invalidMaps.append(invalid);
        invalid = client.advancedValues();
        invalid.insert(
            QStringLiteral("hyprland.misc.lockdead_screen_delay"), 5001
        );
        invalidMaps.append(invalid);
        invalid = client.advancedValues();
        invalid.insert(
            QStringLiteral("hyprland.misc.render_unfocused_fps"), 1.5
        );
        invalidMaps.append(invalid);
        invalid = client.advancedValues();
        invalid.insert(
            QStringLiteral("hyprland.misc.screencopy_force_8b"),
            QStringLiteral("false")
        );
        invalidMaps.append(invalid);
        for (const auto &value : QVariantList{
                 -1, 3, 1.5, true, QStringLiteral("2"),
             }) {
            invalid = client.advancedValues();
            invalid.insert(
                QStringLiteral("hyprland.render.direct_scanout"), value
            );
            invalidMaps.append(invalid);
        }
        for (const auto &value : QVariantList{
                 -1, 2, 0.5, true, QStringLiteral("1"),
             }) {
            invalid = client.advancedValues();
            invalid.insert(
                QStringLiteral("hyprland.render.fp16_sdr_tf"), value
            );
            invalidMaps.append(invalid);
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
                invalid = client.advancedValues();
                invalid.insert(id, value);
                invalidMaps.append(invalid);
            }
        }

        for (const auto &values : invalidMaps) {
            client.saveAdvanced(values);
            QCOMPARE(client.busy(), false);
            QCOMPARE(service_.replaceCallCount(), 0);
            QCOMPARE(service_.applyCallCount(), 0);
            QCOMPARE(
                client.advancedErrorName(),
                QStringLiteral(
                    "org.hyprshelld.Client.Compositor.Error.InvalidAdvanced"
                )
            );
            QVERIFY(client.appearanceErrorName().isEmpty());
            QVERIFY(client.inputErrorName().isEmpty());
            QVERIFY(client.windowsErrorName().isEmpty());
            QVERIFY(client.workspacesErrorName().isEmpty());
            QVERIFY(client.rulesErrorName().isEmpty());
            QVERIFY(client.lastErrorName().isEmpty());
        }

        client.saveAdvanced(client.advancedValues());
        QCOMPARE(service_.replaceCallCount(), 0);
        QCOMPARE(
            client.advancedErrorName(),
            QStringLiteral(
                "org.hyprshelld.Client.Compositor.Error.NoChanges"
            )
        );

        service_.setFailNextReplace(true);
        client.saveAdvanced(changedAdvancedValues());
        QTRY_VERIFY_WITH_TIMEOUT(!client.busy(), 3000);
        QTRY_COMPARE_WITH_TIMEOUT(service_.replaceCallCount(), 1, 3000);
        QCOMPARE(service_.applyCallCount(), 0);
        QCOMPARE(client.revision(), baselineRevision);
        QTRY_VERIFY_WITH_TIMEOUT(client.advancedAvailable(), 3000);
        QCOMPARE(
            client.advancedErrorName(),
            QStringLiteral(
                "org.hyprshelld.Compositor1.Error.InvalidSnapshot"
            )
        );
        QCOMPARE(
            client.advancedErrorMessage(),
            QStringLiteral("Injected replacement failure")
        );
        QVERIFY(client.appearanceErrorName().isEmpty());
        QVERIFY(client.inputErrorName().isEmpty());
        QVERIFY(client.windowsErrorName().isEmpty());
        QVERIFY(client.workspacesErrorName().isEmpty());
        QVERIFY(client.rulesErrorName().isEmpty());
        QVERIFY(client.lastErrorName().isEmpty());
    }

    void ownerLossClearsAdvancedBusyStateOperationAndScopedError()
    {
        service_.setHoldReplaces(true);
        QVERIFY(service_.start());
        HyprShelld::CompositorClient client(bus_, nullptr);
        QTRY_VERIFY_WITH_TIMEOUT(client.advancedAvailable(), 3000);

        auto invalid = client.advancedValues();
        invalid.remove(
            QStringLiteral("hyprland.render.xp_mode")
        );
        client.saveAdvanced(invalid);
        QVERIFY(!client.advancedErrorName().isEmpty());

        client.saveAdvanced(changedAdvancedValues());
        QTRY_COMPARE_WITH_TIMEOUT(service_.heldReplaceCount(), 1, 3000);
        QCOMPARE(client.busy(), true);
        QCOMPARE(client.busyOperation(), QStringLiteral("advanced-save"));
        QVERIFY(client.advancedErrorName().isEmpty());

        service_.stop();
        QTRY_VERIFY_WITH_TIMEOUT(!client.busy(), 3000);
        QCOMPARE(client.busyOperation(), QString{});
        QCOMPARE(client.available(), false);
        QCOMPARE(client.advancedAvailable(), false);
        QCOMPARE(client.advancedProjectionAvailable(), false);
        QVERIFY(client.advancedErrorName().isEmpty());
        QVERIFY(client.advancedValues().isEmpty());
        QVERIFY(client.advancedOptions().isEmpty());
    }

    void savesAndAppliesWorkspacesAcrossTheExactSharedPipeline()
    {
        QVERIFY(service_.start());
        HyprShelld::CompositorClient client(bus_, nullptr);
        QTRY_VERIFY_WITH_TIMEOUT(client.workspacesAvailable(), 3000);
        const auto appearanceBefore = client.appearanceValues();
        const auto inputBefore = client.inputValues();
        const auto windowsBefore = client.windowsValues();
        const auto values = changedWorkspacesValues();
        QStringList observedOperations;
        connect(
            &client,
            &HyprShelld::CompositorClient::busyOperationChanged,
            this,
            [&client, &observedOperations] {
                observedOperations.append(client.busyOperation());
            }
        );

        client.saveWorkspaces(values, client.workspaceRules());
        QTRY_COMPARE_WITH_TIMEOUT(client.revision(), 8ULL, 3000);
        QTRY_VERIFY_WITH_TIMEOUT(!client.busy(), 3000);
        QTRY_VERIFY_WITH_TIMEOUT(client.workspacesAvailable(), 3000);
        QCOMPARE(client.appliedRevision(), 8ULL);
        QCOMPARE(client.applyState(), QStringLiteral("current"));
        QCOMPARE(client.requiredActivation(), QStringLiteral("none"));
        QCOMPARE(client.workspacesValues(), values);
        QCOMPARE(client.appearanceValues(), appearanceBefore);
        QCOMPARE(client.inputValues(), inputBefore);
        QCOMPARE(client.windowsValues(), windowsBefore);
        QCOMPARE(service_.replaceCallCount(), 1);
        QCOMPARE(service_.applyCallCount(), 1);
        QVERIFY(service_.snapshotCallCount() >= 2);
        QVERIFY(observedOperations.contains(QStringLiteral("workspaces-save")));
        QVERIFY(observedOperations.contains(QStringLiteral("workspaces-apply")));
        QCOMPARE(client.busyOperation(), QString{});
        QVERIFY(client.workspacesErrorName().isEmpty());
        QVERIFY(client.appearanceErrorName().isEmpty());
        QVERIFY(client.inputErrorName().isEmpty());
        QVERIFY(client.windowsErrorName().isEmpty());
        QVERIFY(client.lastErrorName().isEmpty());
    }

    void savesWorkspaceScalarsAndOrderedUserRulesAtomically()
    {
        QVERIFY(service_.start());
        HyprShelld::CompositorClient client(bus_, nullptr);
        QTRY_VERIFY_WITH_TIMEOUT(client.workspacesAvailable(), 3000);
        QTRY_VERIFY_WITH_TIMEOUT(
            client.workspaceRulesProjectionAvailable(), 3000
        );
        QVERIFY(client.workspaceRules().isEmpty());
        auto values = client.workspacesValues();
        values.insert(
            QStringLiteral("hyprland.gestures.workspace_swipe_distance"),
            733
        );
        const QVariantList rules{
            workspaceRule(
                QStringLiteral("workspace-client-a"),
                QStringLiteral("special:music"),
                QVariantMap{
                    {QStringLiteral("gaps_in"), QVariantList{1, 2, 3, 4}},
                    {QStringLiteral("border_size"), 9007199254740991.0},
                    {QStringLiteral("animation"),
                     QStringLiteral("slidefadevert left 37%")},
                }
            ),
            workspaceRule(
                QStringLiteral("workspace-client-b"),
                QStringLiteral("2147483647")
            ),
        };
        bool observedNewRevisionWithTrustedStaleRules = false;
        connect(
            &client, &HyprShelld::CompositorClient::snapshotChanged,
            this, [&client, &rules, &observedNewRevisionWithTrustedStaleRules] {
                if (client.revision() == baselineRevision + 1
                    && client.workspaceRulesProjectionAvailable()
                    && client.workspaceRules() != rules) {
                    observedNewRevisionWithTrustedStaleRules = true;
                }
            }
        );

        client.saveWorkspaces(values, rules);
        QTRY_VERIFY_WITH_TIMEOUT(!client.busy(), 3000);
        QTRY_COMPARE_WITH_TIMEOUT(
            client.revision(), baselineRevision + 1, 3000
        );
        QTRY_VERIFY_WITH_TIMEOUT(client.workspacesAvailable(), 3000);
        QCOMPARE(client.workspacesValues(), values);
        QCOMPARE(client.workspaceRules(), rules);
        QVERIFY(client.workspaceRulesProjectionAvailable());
        QVERIFY(!observedNewRevisionWithTrustedStaleRules);
        QCOMPARE(service_.replaceCallCount(), 1);
        QCOMPARE(service_.applyCallCount(), 1);
        const auto persisted = QJsonDocument::fromJson(
            service_.currentSnapshotBytes()
        ).object().value(QStringLiteral("workspaceRules")).toArray();
        QCOMPARE(persisted.size(), 3);
        QCOMPARE(persisted.at(0).toVariant(), rules.at(0));
        QCOMPARE(persisted.at(1).toVariant(), rules.at(1));
        QCOMPARE(persisted.last().toObject(), protectedWorkspaceRuleObject());
    }

    void protectedWorkspaceAuthorityFailsClosedForEveryMutationGroup()
    {
        const auto protectedRule = protectedWorkspaceRuleObject();
        const auto authored = QJsonObject::fromVariantMap(workspaceRule(
            QStringLiteral("workspace-authority-user"), QStringLiteral("2")
        ));
        auto spoof = protectedRule;
        spoof.insert(QStringLiteral("enabled"), false);
        const QList<QJsonArray> invalidRuleSets{
            QJsonArray{},
            QJsonArray{protectedRule, authored},
            QJsonArray{spoof},
            QJsonArray{protectedRule, protectedRule},
        };
        const auto expectedError = QStringLiteral(
            "org.hyprshelld.Client.Compositor.Error.InvalidSnapshotAuthority"
        );
        for (qsizetype invalidIndex = 0;
             invalidIndex < invalidRuleSets.size(); ++invalidIndex) {
            const auto &rules = invalidRuleSets.at(invalidIndex);
            const auto workspaceError = invalidIndex < 2
                ? QStringLiteral(
                    "org.hyprshelld.Client.Compositor.Error."
                    "InvalidWorkspaceRulesSnapshot"
                  )
                : expectedError;
            service_.setSnapshotBytes(snapshotWithWorkspaceRules(rules));
            QVERIFY(service_.start());
            {
                HyprShelld::CompositorClient client(bus_, nullptr);
                QTRY_VERIFY_WITH_TIMEOUT(client.available(), 3000);
                QTRY_VERIFY_WITH_TIMEOUT(client.catalogAvailable(), 3000);
                QTRY_VERIFY_WITH_TIMEOUT(
                    client.appearanceProjectionAvailable(), 3000
                );
                QTRY_VERIFY_WITH_TIMEOUT(client.inputProjectionAvailable(), 3000);
                QTRY_VERIFY_WITH_TIMEOUT(
                    client.windowsProjectionAvailable(), 3000
                );
                QTRY_VERIFY_WITH_TIMEOUT(
                    client.workspacesProjectionAvailable(), 3000
                );
                QVERIFY(!client.appearanceValues().isEmpty());
                QVERIFY(!client.inputValues().isEmpty());
                QVERIFY(!client.windowsValues().isEmpty());
                QVERIFY(!client.workspacesValues().isEmpty());
                QTRY_COMPARE_WITH_TIMEOUT(
                    client.workspacesErrorName(), workspaceError,
                    3000
                );
                QCOMPARE(client.appearanceAvailable(), false);
                QCOMPARE(client.inputAvailable(), false);
                QCOMPARE(client.windowsAvailable(), false);
                QCOMPARE(client.workspacesAvailable(), false);
                QCOMPARE(client.rulesAvailable(), false);
                QCOMPARE(client.workspaceRulesProjectionAvailable(), false);
                QVERIFY(client.workspaceRules().isEmpty());
                QCOMPARE(client.appearanceErrorName(), expectedError);
                QCOMPARE(client.inputErrorName(), expectedError);
                QCOMPARE(client.windowsErrorName(), expectedError);
                QCOMPARE(client.rulesErrorName(), expectedError);
                QVERIFY(client.lastErrorName().isEmpty());

                client.saveWorkspaces(
                    client.workspacesValues(), QVariantList{}
                );
                QCOMPARE(service_.replaceCallCount(), 0);
                QCOMPARE(
                    client.workspacesErrorName(), workspaceError
                );
                QVERIFY(!client.workspacesErrorMessage().isEmpty());
                QCOMPARE(client.appearanceErrorName(), expectedError);
                QCOMPARE(client.inputErrorName(), expectedError);
                QCOMPARE(client.windowsErrorName(), expectedError);
                QCOMPARE(client.rulesErrorName(), expectedError);
            }
            service_.stop();
            service_.reset();
            QCoreApplication::processEvents();
        }
    }

    void retriesAmbiguousWorkspacesReplaceAndApplyOnlyForTheSavedRevision()
    {
        service_.setAmbiguousFirstReplace(true);
        service_.setAmbiguousFirstApplyWithoutProperties(true);
        QVERIFY(service_.start());
        HyprShelld::CompositorClient client(bus_, nullptr);
        QTRY_VERIFY_WITH_TIMEOUT(client.workspacesAvailable(), 3000);
        auto values = client.workspacesValues();
        values.insert(
            QStringLiteral("hyprland.gestures.workspace_swipe_distance"), 451
        );

        client.saveWorkspaces(values, client.workspaceRules());
        QTRY_VERIFY_WITH_TIMEOUT(!client.busy(), 3000);
        QTRY_COMPARE_WITH_TIMEOUT(client.revision(), 8ULL, 3000);
        QTRY_VERIFY_WITH_TIMEOUT(client.workspacesAvailable(), 3000);
        QCOMPARE(service_.replaceCallCount(), 2);
        QCOMPARE(service_.applyCallCount(), 2);
        QCOMPARE(client.appliedRevision(), 8ULL);
        QCOMPARE(client.workspacesValues(), values);
        QVERIFY(client.workspacesErrorName().isEmpty());
        QVERIFY(client.lastErrorName().isEmpty());
    }

    void retainsSavedWorkspacesAfterApplyFailureAndRetriesWithNeutralScope()
    {
        service_.setFailNextApply(true);
        QVERIFY(service_.start());
        HyprShelld::CompositorClient client(bus_, nullptr);
        QTRY_VERIFY_WITH_TIMEOUT(client.workspacesAvailable(), 3000);
        auto values = client.workspacesValues();
        values.insert(
            QStringLiteral("hyprland.gestures.workspace_swipe_forever"), true
        );

        client.saveWorkspaces(values, client.workspaceRules());
        QTRY_VERIFY_WITH_TIMEOUT(!client.busy(), 3000);
        QTRY_COMPARE_WITH_TIMEOUT(client.revision(), 8ULL, 3000);
        QTRY_VERIFY_WITH_TIMEOUT(client.retryApplyAvailable(), 3000);
        QCOMPARE(client.appliedRevision(), baselineRevision);
        QCOMPARE(client.applyState(), QStringLiteral("retained"));
        QCOMPARE(client.requiredActivation(), QStringLiteral("reload"));
        QCOMPARE(client.appearanceAvailable(), false);
        QCOMPARE(client.inputAvailable(), false);
        QCOMPARE(client.windowsAvailable(), false);
        QCOMPARE(client.workspacesAvailable(), false);
        QCOMPARE(client.appearanceProjectionAvailable(), true);
        QCOMPARE(client.inputProjectionAvailable(), true);
        QCOMPARE(client.windowsProjectionAvailable(), true);
        QCOMPARE(client.workspacesProjectionAvailable(), true);
        QCOMPARE(client.workspacesValues(), values);
        QVERIFY(client.workspacesErrorName().isEmpty());
        QVERIFY(client.appearanceErrorName().isEmpty());
        QVERIFY(client.inputErrorName().isEmpty());
        QVERIFY(client.windowsErrorName().isEmpty());
        QCOMPARE(
            client.lastErrorOperation(), QStringLiteral("workspaces-apply")
        );
        QCOMPARE(
            client.lastErrorName(),
            QStringLiteral("org.hyprshelld.Compositor1.Error.ApplyFailed")
        );

        QStringList observedOperations;
        connect(
            &client,
            &HyprShelld::CompositorClient::busyOperationChanged,
            this,
            [&client, &observedOperations] {
                observedOperations.append(client.busyOperation());
            }
        );
        client.retryApply();
        QTRY_VERIFY_WITH_TIMEOUT(!client.busy(), 3000);
        QTRY_VERIFY_WITH_TIMEOUT(client.workspacesAvailable(), 3000);
        QCOMPARE(client.appliedRevision(), 8ULL);
        QCOMPARE(service_.applyCallCount(), 2);
        QVERIFY(observedOperations.contains(
            QStringLiteral("compositor-apply")
        ));
        QVERIFY(client.lastErrorOperation().isEmpty());
        QVERIFY(client.lastErrorName().isEmpty());
    }

    void workspacesReplaceFailureIsScopedAndDoesNotPoisonOtherGroups()
    {
        service_.setFailNextReplace(true);
        QVERIFY(service_.start());
        HyprShelld::CompositorClient client(bus_, nullptr);
        QTRY_VERIFY_WITH_TIMEOUT(client.workspacesAvailable(), 3000);
        auto values = client.workspacesValues();
        values.insert(
            QStringLiteral("hyprland.animations.workspace_wraparound"), true
        );

        client.saveWorkspaces(values, client.workspaceRules());
        QTRY_VERIFY_WITH_TIMEOUT(!client.busy(), 3000);
        QTRY_COMPARE_WITH_TIMEOUT(service_.replaceCallCount(), 1, 3000);
        QCOMPARE(service_.applyCallCount(), 0);
        QCOMPARE(client.revision(), baselineRevision);
        QTRY_VERIFY_WITH_TIMEOUT(client.workspacesAvailable(), 3000);
        QTRY_VERIFY_WITH_TIMEOUT(client.appearanceAvailable(), 3000);
        QTRY_VERIFY_WITH_TIMEOUT(client.inputAvailable(), 3000);
        QTRY_VERIFY_WITH_TIMEOUT(client.windowsAvailable(), 3000);
        QCOMPARE(
            client.workspacesErrorName(),
            QStringLiteral(
                "org.hyprshelld.Compositor1.Error.InvalidSnapshot"
            )
        );
        QCOMPARE(
            client.workspacesErrorMessage(),
            QStringLiteral("Injected replacement failure")
        );
        QVERIFY(client.appearanceErrorName().isEmpty());
        QVERIFY(client.inputErrorName().isEmpty());
        QVERIFY(client.windowsErrorName().isEmpty());
        QVERIFY(client.lastErrorName().isEmpty());
    }

    void rejectsInvalidWorkspacesMapsWithoutCallingTheService()
    {
        QVERIFY(service_.start());
        HyprShelld::CompositorClient client(bus_, nullptr);
        QTRY_VERIFY_WITH_TIMEOUT(client.workspacesAvailable(), 3000);

        QList<QVariantMap> invalidMaps;
        auto invalid = client.workspacesValues();
        invalid.remove(
            QStringLiteral("hyprland.animations.workspace_wraparound")
        );
        invalidMaps.append(invalid);
        invalid = client.workspacesValues();
        invalid.insert(QStringLiteral("hyprland.workspaces.unknown"), true);
        invalidMaps.append(invalid);
        invalid = client.workspacesValues();
        invalid.insert(
            QStringLiteral("hyprland.gestures.workspace_swipe_distance"),
            300.5
        );
        invalidMaps.append(invalid);
        invalid = client.workspacesValues();
        invalid.insert(
            QStringLiteral("hyprland.gestures.workspace_swipe_cancel_ratio"),
            std::numeric_limits<double>::quiet_NaN()
        );
        invalidMaps.append(invalid);
        invalid = client.workspacesValues();
        invalid.insert(
            QStringLiteral("hyprland.misc.initial_workspace_tracking"),
            QStringLiteral("1")
        );
        invalidMaps.append(invalid);
        invalid = client.workspacesValues();
        invalid.insert(
            QStringLiteral("hyprland.misc.initial_workspace_tracking"), 3.0
        );
        invalidMaps.append(invalid);
        invalid = client.workspacesValues();
        invalid.insert(
            QStringLiteral("hyprland.binds.allow_workspace_cycles"), 1
        );
        invalidMaps.append(invalid);
        invalid = client.workspacesValues();
        invalid.insert(
            QStringLiteral("hyprland.binds.workspace_center_on"), 2
        );
        invalidMaps.append(invalid);
        invalid = client.workspacesValues();
        invalid.insert(
            QStringLiteral("hyprland.cursor.warp_on_change_workspace"), -1
        );
        invalidMaps.append(invalid);
        invalid = client.workspacesValues();
        invalid.insert(
            QStringLiteral("hyprland.cursor.warp_on_toggle_special"), 3
        );
        invalidMaps.append(invalid);

        for (const auto &map : invalidMaps) {
            client.saveWorkspaces(map, client.workspaceRules());
            QCOMPARE(client.busy(), false);
            QCOMPARE(service_.replaceCallCount(), 0);
            QCOMPARE(service_.applyCallCount(), 0);
            QCOMPARE(
                client.workspacesErrorName(),
                QStringLiteral(
                    "org.hyprshelld.Client.Compositor.Error.InvalidWorkspaces"
                )
            );
            QVERIFY(client.appearanceErrorName().isEmpty());
            QVERIFY(client.inputErrorName().isEmpty());
            QVERIFY(client.windowsErrorName().isEmpty());
            QVERIFY(client.lastErrorName().isEmpty());
        }

        QList<QVariantList> invalidRuleLists;
        auto reservedId = workspaceRule(
            QString::fromLatin1(
                HyprShelld::Hyprland::sharedSpacingWorkspaceRuleId
            ),
            QStringLiteral("2")
        );
        invalidRuleLists.append(QVariantList{reservedId});
        auto reservedSelector = workspaceRule(
            QStringLiteral("workspace-user-spoof"),
            QString::fromLatin1(
                HyprShelld::Hyprland::sharedSpacingWorkspaceRuleSelector
            )
        );
        invalidRuleLists.append(QVariantList{reservedSelector});
        auto invalidRule = workspaceRule(
            QStringLiteral("workspace-invalid"), QStringLiteral("2")
        );
        invalidRule.remove(QStringLiteral("selector"));
        invalidRuleLists.append(QVariantList{invalidRule});
        invalidRule = workspaceRule(
            QStringLiteral("workspace-invalid"), QStringLiteral("2")
        );
        invalidRule.insert(
            QStringLiteral("overrides"),
            QVariantMap{{QStringLiteral("gaps_in"),
                         QVariantList{1, 2.5, 3, 4}}}
        );
        invalidRuleLists.append(QVariantList{invalidRule});
        QVariantList excessiveRules;
        excessiveRules.reserve(
            HyprShelld::Hyprland::maximumUserWorkspaceRules + 1
        );
        for (int index = 0;
             index < HyprShelld::Hyprland::maximumUserWorkspaceRules;
             ++index) {
            excessiveRules.append(workspaceRule(
                QStringLiteral("workspace-limit-%1").arg(index),
                QString::number(index + 1)
            ));
        }
        excessiveRules.append(
            QVariant::fromValue(static_cast<QObject *>(nullptr))
        );
        invalidRuleLists.append(excessiveRules);
        for (const auto &rules : invalidRuleLists) {
            client.saveWorkspaces(client.workspacesValues(), rules);
            QCOMPARE(client.busy(), false);
            QCOMPARE(service_.replaceCallCount(), 0);
            QCOMPARE(service_.applyCallCount(), 0);
            QCOMPARE(
                client.workspacesErrorName(),
                QStringLiteral(
                    "org.hyprshelld.Client.Compositor.Error.InvalidWorkspaces"
                )
            );
            QVERIFY(client.lastErrorName().isEmpty());
        }

        client.saveWorkspaces(
            client.workspacesValues(), client.workspaceRules()
        );
        QCOMPARE(service_.replaceCallCount(), 0);
        QCOMPARE(
            client.workspacesErrorName(),
            QStringLiteral("org.hyprshelld.Client.Compositor.Error.NoChanges")
        );
        QVERIFY(client.appearanceErrorName().isEmpty());
        QVERIFY(client.inputErrorName().isEmpty());
        QVERIFY(client.windowsErrorName().isEmpty());
        QVERIFY(client.lastErrorName().isEmpty());
    }

    void ownerLossClearsWorkspacesBusyStateOperationAndScopedError()
    {
        service_.setHoldReplaces(true);
        QVERIFY(service_.start());
        HyprShelld::CompositorClient client(bus_, nullptr);
        QTRY_VERIFY_WITH_TIMEOUT(client.workspacesAvailable(), 3000);

        auto invalid = client.workspacesValues();
        invalid.remove(
            QStringLiteral("hyprland.animations.workspace_wraparound")
        );
        client.saveWorkspaces(invalid, client.workspaceRules());
        QVERIFY(!client.workspacesErrorName().isEmpty());

        auto values = client.workspacesValues();
        values.insert(
            QStringLiteral("hyprland.gestures.workspace_swipe_create_new"),
            false
        );
        client.saveWorkspaces(values, client.workspaceRules());
        QTRY_COMPARE_WITH_TIMEOUT(service_.heldReplaceCount(), 1, 3000);
        QCOMPARE(client.busy(), true);
        QCOMPARE(client.busyOperation(), QStringLiteral("workspaces-save"));
        QVERIFY(client.workspacesErrorName().isEmpty());

        service_.stop();
        QTRY_VERIFY_WITH_TIMEOUT(!client.busy(), 3000);
        QCOMPARE(client.busyOperation(), QString{});
        QCOMPARE(client.available(), false);
        QCOMPARE(client.workspacesProjectionAvailable(), false);
        QCOMPARE(client.workspaceRulesProjectionAvailable(), false);
        QVERIFY(client.workspaceRules().isEmpty());
        QVERIFY(client.workspacesErrorName().isEmpty());
        QVERIFY(client.workspacesValues().isEmpty());
        QVERIFY(client.workspacesOptions().isEmpty());
    }

    void savesBothOrderedRulesListsThroughOneCasAndApply()
    {
        QVERIFY(service_.start());
        HyprShelld::CompositorClient client(bus_, nullptr);
        QTRY_VERIFY_WITH_TIMEOUT(client.rulesAvailable(), 3000);
        const auto appearanceBefore = client.appearanceValues();
        const auto inputBefore = client.inputValues();
        const auto windowsBefore = client.windowsValues();
        const auto workspacesBefore = client.workspacesValues();
        QVariantList windows;
        windows.append(windowRule(
            QStringLiteral("window-one"), QStringLiteral("Window one")
        ));
        windows.append(windowRule(
            QStringLiteral("window-two"), QStringLiteral("Window two"),
            QStringLiteral("^kitty$")
        ));
        QVariantList layers;
        layers.append(layerRule(
            QStringLiteral("layer-one"), QStringLiteral("Layer one")
        ));
        layers.append(layerRule(
            QStringLiteral("layer-two"), QStringLiteral("Layer two")
        ));
        QStringList observedOperations;
        connect(
            &client,
            &HyprShelld::CompositorClient::busyOperationChanged,
            this,
            [&client, &observedOperations] {
                observedOperations.append(client.busyOperation());
            }
        );

        client.saveRules(windows, layers);
        QTRY_COMPARE_WITH_TIMEOUT(client.revision(), 8ULL, 3000);
        QTRY_VERIFY_WITH_TIMEOUT(!client.busy(), 3000);
        QTRY_VERIFY_WITH_TIMEOUT(client.rulesAvailable(), 3000);
        QCOMPARE(client.windowRules(), windows);
        QCOMPARE(client.layerRules(), layers);
        QCOMPARE(client.appliedRevision(), 8ULL);
        QCOMPARE(client.applyState(), QStringLiteral("current"));
        QCOMPARE(client.requiredActivation(), QStringLiteral("none"));
        QCOMPARE(client.appearanceValues(), appearanceBefore);
        QCOMPARE(client.inputValues(), inputBefore);
        QCOMPARE(client.windowsValues(), windowsBefore);
        QCOMPARE(client.workspacesValues(), workspacesBefore);
        QCOMPARE(service_.replaceCallCount(), 1);
        QCOMPARE(service_.applyCallCount(), 1);
        QVERIFY(service_.snapshotCallCount() >= 2);
        QVERIFY(observedOperations.contains(QStringLiteral("rules-save")));
        QVERIFY(observedOperations.contains(QStringLiteral("rules-apply")));
        QVERIFY(client.rulesErrorName().isEmpty());
        QVERIFY(client.lastErrorName().isEmpty());
    }

    void retriesAmbiguousRulesReplaceAndApplyOnlyForTheSavedPair()
    {
        service_.setAmbiguousFirstReplace(true);
        service_.setAmbiguousFirstApplyWithoutProperties(true);
        QVERIFY(service_.start());
        HyprShelld::CompositorClient client(bus_, nullptr);
        QTRY_VERIFY_WITH_TIMEOUT(client.rulesAvailable(), 3000);
        const QVariantList windows{QVariant(windowRule(
            QStringLiteral("window-one"), QStringLiteral("Window one")
        ))};
        const QVariantList layers{QVariant(layerRule(
            QStringLiteral("layer-one"), QStringLiteral("Layer one")
        ))};

        client.saveRules(windows, layers);
        QTRY_VERIFY_WITH_TIMEOUT(!client.busy(), 3000);
        QTRY_COMPARE_WITH_TIMEOUT(client.revision(), 8ULL, 3000);
        QTRY_VERIFY_WITH_TIMEOUT(client.rulesAvailable(), 3000);
        QCOMPARE(service_.replaceCallCount(), 2);
        QCOMPARE(service_.applyCallCount(), 2);
        QCOMPARE(client.appliedRevision(), 8ULL);
        QCOMPARE(client.windowRules(), windows);
        QCOMPARE(client.layerRules(), layers);
        QVERIFY(client.rulesErrorName().isEmpty());
        QVERIFY(client.lastErrorName().isEmpty());
    }

    void retainsSavedRulesAfterApplyFailureAndRetriesWithNeutralScope()
    {
        service_.setFailNextApply(true);
        QVERIFY(service_.start());
        HyprShelld::CompositorClient client(bus_, nullptr);
        QTRY_VERIFY_WITH_TIMEOUT(client.rulesAvailable(), 3000);
        const QVariantList windows{QVariant(windowRule(
            QStringLiteral("window-one"), QStringLiteral("Window one")
        ))};
        const QVariantList layers{QVariant(layerRule(
            QStringLiteral("layer-one"), QStringLiteral("Layer one")
        ))};

        client.saveRules(windows, layers);
        QTRY_VERIFY_WITH_TIMEOUT(!client.busy(), 3000);
        QTRY_COMPARE_WITH_TIMEOUT(client.revision(), 8ULL, 3000);
        QTRY_VERIFY_WITH_TIMEOUT(client.retryApplyAvailable(), 3000);
        QCOMPARE(client.rulesAvailable(), false);
        QCOMPARE(client.rulesProjectionAvailable(), true);
        QCOMPARE(client.windowRules(), windows);
        QCOMPARE(client.layerRules(), layers);
        QCOMPARE(client.appliedRevision(), baselineRevision);
        QCOMPARE(client.applyState(), QStringLiteral("retained"));
        QCOMPARE(client.requiredActivation(), QStringLiteral("reload"));
        QCOMPARE(client.lastErrorOperation(), QStringLiteral("rules-apply"));
        QCOMPARE(
            client.lastErrorName(),
            QStringLiteral("org.hyprshelld.Compositor1.Error.ApplyFailed")
        );
        QVERIFY(client.rulesErrorName().isEmpty());

        QStringList observedOperations;
        connect(
            &client,
            &HyprShelld::CompositorClient::busyOperationChanged,
            this,
            [&client, &observedOperations] {
                observedOperations.append(client.busyOperation());
            }
        );
        client.retryApply();
        QTRY_VERIFY_WITH_TIMEOUT(!client.busy(), 3000);
        QTRY_VERIFY_WITH_TIMEOUT(client.rulesAvailable(), 3000);
        QCOMPARE(client.appliedRevision(), 8ULL);
        QCOMPARE(service_.applyCallCount(), 2);
        QVERIFY(observedOperations.contains(QStringLiteral("compositor-apply")));
        QVERIFY(client.lastErrorOperation().isEmpty());
        QVERIFY(client.lastErrorName().isEmpty());
    }

    void rejectsInvalidAndOversizedRulesBeforeCallingTheService()
    {
        QVERIFY(service_.start());
        HyprShelld::CompositorClient client(bus_, nullptr);
        QTRY_VERIFY_WITH_TIMEOUT(client.rulesAvailable(), 3000);

        const auto validLayer = layerRule(
            QStringLiteral("layer-one"), QStringLiteral("Layer one")
        );
        auto invalidRe2 = windowRule(
            QStringLiteral("window-one"), QStringLiteral("Window one"),
            QStringLiteral("[")
        );
        client.saveRules(
            QVariantList{QVariant(invalidRe2)},
            QVariantList{QVariant(validLayer)}
        );
        QCOMPARE(client.busy(), false);
        QCOMPARE(service_.replaceCallCount(), 0);
        QCOMPARE(service_.applyCallCount(), 0);
        QCOMPARE(
            client.rulesErrorName(),
            QStringLiteral("org.hyprshelld.Client.Compositor.Error.InvalidRules")
        );
        QVERIFY(!client.rulesErrorMessage().isEmpty());
        QVERIFY(client.appearanceErrorName().isEmpty());
        QVERIFY(client.inputErrorName().isEmpty());
        QVERIFY(client.windowsErrorName().isEmpty());
        QVERIFY(client.workspacesErrorName().isEmpty());
        QVERIFY(client.lastErrorName().isEmpty());

        auto unsafe = windowRule(
            QStringLiteral("window-two"), QStringLiteral("Window two")
        );
        auto effects = unsafe.value(QStringLiteral("effects")).toMap();
        effects.insert(
            QStringLiteral("border_size"), 9007199254740992.0
        );
        unsafe.insert(QStringLiteral("effects"), effects);
        client.saveRules(QVariantList{QVariant(unsafe)}, {});
        QCOMPARE(service_.replaceCallCount(), 0);
        QCOMPARE(
            client.rulesErrorName(),
            QStringLiteral("org.hyprshelld.Client.Compositor.Error.InvalidRules")
        );

        QVariantList oversized;
        oversized.reserve(HyprShelld::Hyprland::maximumWindowRules + 1);
        for (qsizetype index = 0;
             index <= HyprShelld::Hyprland::maximumWindowRules; ++index) {
            oversized.append(QVariantMap{
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
        client.saveRules(oversized, {});
        QCOMPARE(service_.replaceCallCount(), 0);
        QCOMPARE(service_.applyCallCount(), 0);
        QCOMPARE(
            client.rulesErrorName(),
            QStringLiteral("org.hyprshelld.Client.Compositor.Error.InvalidRules")
        );
    }

    void rulesReplaceFailureIsScopedAndOwnerLossClearsBusyProjectionAndError()
    {
        service_.setFailNextReplace(true);
        QVERIFY(service_.start());
        HyprShelld::CompositorClient client(bus_, nullptr);
        QTRY_VERIFY_WITH_TIMEOUT(client.rulesAvailable(), 3000);
        const QVariantList first{QVariant(windowRule(
            QStringLiteral("window-one"), QStringLiteral("Window one")
        ))};
        client.saveRules(first, {});
        QTRY_VERIFY_WITH_TIMEOUT(!client.busy(), 3000);
        QTRY_COMPARE_WITH_TIMEOUT(service_.replaceCallCount(), 1, 3000);
        QCOMPARE(service_.applyCallCount(), 0);
        QCOMPARE(
            client.rulesErrorName(),
            QStringLiteral("org.hyprshelld.Compositor1.Error.InvalidSnapshot")
        );
        QVERIFY(client.appearanceErrorName().isEmpty());
        QVERIFY(client.inputErrorName().isEmpty());
        QVERIFY(client.windowsErrorName().isEmpty());
        QVERIFY(client.workspacesErrorName().isEmpty());
        QVERIFY(client.lastErrorName().isEmpty());

        service_.setHoldReplaces(true);
        QTRY_VERIFY_WITH_TIMEOUT(client.rulesAvailable(), 3000);
        const QVariantList second{QVariant(windowRule(
            QStringLiteral("window-two"), QStringLiteral("Window two")
        ))};
        client.saveRules(second, {});
        QTRY_COMPARE_WITH_TIMEOUT(service_.heldReplaceCount(), 1, 3000);
        QCOMPARE(client.busy(), true);
        QCOMPARE(client.busyOperation(), QStringLiteral("rules-save"));
        QVERIFY(client.rulesErrorName().isEmpty());

        service_.stop();
        QTRY_VERIFY_WITH_TIMEOUT(!client.busy(), 3000);
        QCOMPARE(client.busyOperation(), QString{});
        QCOMPARE(client.available(), false);
        QCOMPARE(client.rulesProjectionAvailable(), false);
        QVERIFY(client.windowRules().isEmpty());
        QVERIFY(client.layerRules().isEmpty());
        QVERIFY(client.rulesErrorName().isEmpty());
    }

    void globalErrorTripleIsAtomicAndSurvivesOwnerLoss()
    {
        QVERIFY(service_.start());
        HyprShelld::CompositorClient client(bus_, nullptr);
        QTRY_VERIFY_WITH_TIMEOUT(client.available(), 3000);
        QTRY_VERIFY_WITH_TIMEOUT(client.displayDiscoveryAvailable(), 3000);
        QList<QStringList> observedErrors;
        connect(
            &client,
            &HyprShelld::CompositorClient::lastErrorChanged,
            this,
            [&client, &observedErrors] {
                observedErrors.append({
                    client.lastErrorOperation(),
                    client.lastErrorName(),
                    client.lastErrorMessage(),
                });
            }
        );

        client.previewDisplayConfiguration({}, 15);
        QCOMPARE(observedErrors.size(), 1);
        QCOMPARE(
            observedErrors.constFirst().at(0),
            QStringLiteral("display-preview")
        );
        QCOMPARE(
            observedErrors.constFirst().at(1),
            QStringLiteral(
                "org.hyprshelld.Client.Compositor.Error.Unavailable"
            )
        );
        QVERIFY(!observedErrors.constFirst().at(2).isEmpty());

        const auto retainedError = observedErrors.constFirst();
        service_.stop();
        QTRY_VERIFY_WITH_TIMEOUT(!client.available(), 3000);
        QCOMPARE(client.appearanceProjectionAvailable(), false);
        QCOMPARE(client.inputProjectionAvailable(), false);
        QCOMPARE(observedErrors.size(), 1);
        QCOMPARE(client.lastErrorOperation(), retainedError.at(0));
        QCOMPARE(client.lastErrorName(), retainedError.at(1));
        QCOMPARE(client.lastErrorMessage(), retainedError.at(2));

        client.clearError();
        QCOMPARE(observedErrors.size(), 2);
        QCOMPARE(
            observedErrors.constLast(),
            QStringList({QString{}, QString{}, QString{}})
        );
        QVERIFY(client.lastErrorOperation().isEmpty());
        QVERIFY(client.lastErrorName().isEmpty());
        QVERIFY(client.lastErrorMessage().isEmpty());

        client.applyConfiguration();
        QCOMPARE(observedErrors.size(), 3);
        QCOMPARE(
            observedErrors.constLast().at(0),
            QStringLiteral("compositor-apply")
        );
        QVERIFY(!observedErrors.constLast().at(1).isEmpty());
        QVERIFY(!observedErrors.constLast().at(2).isEmpty());
        client.clearError();
        QCOMPARE(observedErrors.size(), 4);
        QCOMPARE(
            observedErrors.constLast(),
            QStringList({QString{}, QString{}, QString{}})
        );
    }

    void labelsEveryPageLocalGuardErrorForFiltering()
    {
        QVERIFY(service_.start());
        HyprShelld::CompositorClient client(bus_, nullptr);
        QTRY_VERIFY_WITH_TIMEOUT(client.available(), 3000);

        client.adoptManagedConfiguration();
        QCOMPARE(client.lastErrorOperation(), QStringLiteral("adopt"));
        QVERIFY(!client.lastErrorName().isEmpty());
        QVERIFY(!client.lastErrorMessage().isEmpty());

        client.confirmDisplayConfiguration();
        QCOMPARE(
            client.lastErrorOperation(),
            QStringLiteral("display-confirm")
        );
        QVERIFY(!client.lastErrorName().isEmpty());
        QVERIFY(!client.lastErrorMessage().isEmpty());

        client.revertDisplayConfiguration();
        QCOMPARE(
            client.lastErrorOperation(),
            QStringLiteral("display-revert")
        );
        QVERIFY(!client.lastErrorName().isEmpty());
        QVERIFY(!client.lastErrorMessage().isEmpty());

        client.previewDisplayConfiguration({}, 15);
        QCOMPARE(
            client.lastErrorOperation(),
            QStringLiteral("display-preview")
        );
        QVERIFY(!client.lastErrorName().isEmpty());
        QVERIFY(!client.lastErrorMessage().isEmpty());
    }

    void recoversOnlyTheCallingClientsPendingConfirmation()
    {
        service_.setPendingBehavior(FakeCompositor::PendingBehavior::Success);
        service_.setAwaitingConfirmation();
        QVERIFY(service_.start());

        HyprShelld::CompositorClient client(bus_, nullptr);
        QTRY_VERIFY_WITH_TIMEOUT(client.available(), 3000);
        QTRY_COMPARE_WITH_TIMEOUT(service_.pendingCallCount(), 1, 3000);
        QCOMPARE(
            client.displayConfirmationState(),
            QStringLiteral("awaiting-confirmation")
        );
        QCOMPARE(client.displayConfirmationRevision(), previewRevision);
        QCOMPARE(client.displayConfirmationDeadlineMs(), previewDeadlineMs);
        QCOMPARE(client.displayConfirmationGeneration(), previewGeneration);
        QCOMPARE(client.displayConfirmationOwned(), true);

        service_.setPendingBehavior(
            FakeCompositor::PendingBehavior::NoDisplayConfirmation
        );
        service_.replaceAwaitingConfirmationWithForeignTuple();
        QTRY_COMPARE_WITH_TIMEOUT(service_.pendingCallCount(), 2, 3000);
        QTRY_VERIFY_WITH_TIMEOUT(client.available(), 3000);
        QCOMPARE(client.displayConfirmationOwned(), false);
        QCOMPARE(
            client.displayConfirmationGeneration(),
            QString(64, QLatin1Char('1'))
        );

        service_.stop();
        QTRY_VERIFY_WITH_TIMEOUT(!client.available(), 3000);
        QCOMPARE(client.displayConfirmationState(), QStringLiteral("idle"));
        QCOMPARE(client.displayConfirmationRevision(), 0ULL);
        QCOMPARE(client.displayConfirmationDeadlineMs(), 0ULL);
        QVERIFY(client.displayConfirmationGeneration().isEmpty());
        QCOMPARE(client.displayConfirmationOwned(), false);
        QCOMPARE(client.managementState(), QStringLiteral("unmanaged"));
    }

    void keepsForeignPendingConfirmationLockedAndAvailable()
    {
        service_.setPendingBehavior(
            FakeCompositor::PendingBehavior::NoDisplayConfirmation
        );
        service_.setAwaitingConfirmation();
        QVERIFY(service_.start());

        HyprShelld::CompositorClient client(bus_, nullptr);
        QTRY_VERIFY_WITH_TIMEOUT(client.available(), 3000);
        QTRY_COMPARE_WITH_TIMEOUT(service_.pendingCallCount(), 1, 3000);
        QCOMPARE(
            client.displayConfirmationState(),
            QStringLiteral("awaiting-confirmation")
        );
        QCOMPARE(client.displayConfirmationOwned(), false);
        QCOMPARE(client.managementState(), QStringLiteral("preview"));
        QVERIFY(client.lastErrorName().isEmpty());
    }

    void failsHydrationOnUnexpectedPendingLookupErrors()
    {
        service_.setPendingBehavior(
            FakeCompositor::PendingBehavior::UnexpectedError
        );
        service_.setAwaitingConfirmation();
        QVERIFY(service_.start());

        HyprShelld::CompositorClient client(bus_, nullptr);
        QTRY_COMPARE_WITH_TIMEOUT(service_.pendingCallCount(), 1, 3000);
        QTRY_COMPARE_WITH_TIMEOUT(
            client.lastErrorName(),
            QStringLiteral("org.freedesktop.DBus.Error.UnknownMethod"),
            3000
        );
        QCOMPARE(
            client.lastErrorOperation(),
            QStringLiteral("display-refresh")
        );
        QCOMPARE(client.available(), true);
        QCOMPARE(client.displayDiscoveryAvailable(), false);
        QCOMPARE(client.displayConfirmationOwned(), false);
    }

    void rejectsMalformedPreviewRepliesAndAcceptsTheExactTuple()
    {
        QVERIFY(service_.start());
        HyprShelld::CompositorClient client(bus_, nullptr);
        QTRY_VERIFY_WITH_TIMEOUT(client.available(), 3000);
        QTRY_VERIFY_WITH_TIMEOUT(client.displayDiscoveryAvailable(), 3000);
        QSignalSpy failures(
            &client,
            &HyprShelld::CompositorClient::operationFailed
        );
        QVERIFY(failures.isValid());

        const QVariantList output{
            QVariantMap{
                {QStringLiteral("id"), QStringLiteral("display-DP-1")},
                {QStringLiteral("selector"), QStringLiteral("DP-1")},
                {QStringLiteral("enabled"), true},
            }
        };
        const QList<QVariantList> malformedReplies{
            {
                QVariant::fromValue<qulonglong>(previewRevision),
                QString{},
                QVariant::fromValue<qulonglong>(previewDeadlineMs),
                previewGeneration,
            },
            {
                QVariant::fromValue<qulonglong>(previewRevision),
                QString(32, QLatin1Char('F')),
                QVariant::fromValue<qulonglong>(previewDeadlineMs),
                previewGeneration,
            },
            {
                QVariant::fromValue<qulonglong>(baselineRevision),
                confirmationToken,
                QVariant::fromValue<qulonglong>(previewDeadlineMs),
                previewGeneration,
            },
            {
                QVariant::fromValue<qulonglong>(previewRevision + 1),
                confirmationToken,
                QVariant::fromValue<qulonglong>(previewDeadlineMs),
                previewGeneration,
            },
            {
                QVariant::fromValue<qulonglong>(previewRevision),
                confirmationToken,
                QVariant::fromValue<qulonglong>(0),
                previewGeneration,
            },
            {
                QVariant::fromValue<qulonglong>(previewRevision),
                confirmationToken,
                QVariant::fromValue<qulonglong>(previewDeadlineMs),
                QStringLiteral("not-a-generation-digest"),
            },
            {
                QVariant::fromValue<qulonglong>(previewRevision),
                confirmationToken,
            },
        };

        for (const auto &reply : malformedReplies) {
            const auto expectedFailures = failures.size() + 1;
            service_.setPreviewReply(reply, false);
            client.previewDisplayConfiguration(output, 15);
            QVERIFY(client.busy());
            QTRY_COMPARE_WITH_TIMEOUT(failures.size(), expectedFailures, 3000);
            QCOMPARE(
                failures.last().at(0).toString(),
                QStringLiteral(
                    "org.hyprshelld.Client.Compositor.Error.InvalidReply"
                )
            );
            QCOMPARE(
                client.lastErrorOperation(),
                QStringLiteral("display-preview")
            );
            QTRY_VERIFY_WITH_TIMEOUT(!client.busy(), 3000);
            QTRY_VERIFY_WITH_TIMEOUT(client.available(), 3000);
            QCOMPARE(client.displayConfirmationOwned(), false);
        }

        service_.setPreviewReply(validPreviewReply(), true);
        client.previewDisplayConfiguration(output, 15);
        QTRY_VERIFY_WITH_TIMEOUT(!client.busy(), 3000);
        QTRY_COMPARE_WITH_TIMEOUT(
            client.displayConfirmationState(),
            QStringLiteral("awaiting-confirmation"),
            3000
        );
        QCOMPARE(client.displayConfirmationRevision(), previewRevision);
        QCOMPARE(client.displayConfirmationDeadlineMs(), previewDeadlineMs);
        QCOMPARE(client.displayConfirmationGeneration(), previewGeneration);
        QCOMPARE(client.displayConfirmationOwned(), true);
        QVERIFY(client.lastErrorOperation().isEmpty());
        QVERIFY(client.lastErrorName().isEmpty());
        QVERIFY(client.lastErrorMessage().isEmpty());
    }

    void ignoresLateAndDuplicateAsyncReplies()
    {
        service_.setHoldSnapshots(true);
        QVERIFY(service_.start());
        HyprShelld::CompositorClient client(bus_, nullptr);
        QTRY_COMPARE_WITH_TIMEOUT(service_.heldSnapshotCount(), 1, 3000);

        client.refresh();
        QTRY_COMPARE_WITH_TIMEOUT(service_.heldSnapshotCount(), 2, 3000);
        QVERIFY(service_.releaseNextSnapshot(
            {QByteArrayLiteral("malformed")},
            true
        ));
        QTest::qWait(50);
        QCOMPARE(client.available(), false);

        QVERIFY(service_.releaseNextSnapshot({}, true));
        QTRY_VERIFY_WITH_TIMEOUT(client.available(), 3000);
        QCOMPARE(client.revision(), baselineRevision);

        service_.setHoldSnapshots(false);
        service_.setHoldPreviews(true);
        service_.setPreviewReply(validPreviewReply(), false);
        const QVariantList output{
            QVariantMap{
                {QStringLiteral("id"), QStringLiteral("display-DP-1")},
                {QStringLiteral("selector"), QStringLiteral("DP-1")},
                {QStringLiteral("enabled"), true},
            }
        };
        QSignalSpy failures(
            &client,
            &HyprShelld::CompositorClient::operationFailed
        );
        client.previewDisplayConfiguration(output, 15);
        QTRY_COMPARE_WITH_TIMEOUT(service_.heldPreviewCount(), 1, 3000);
        QCOMPARE(client.busy(), true);

        service_.stop();
        QTRY_VERIFY_WITH_TIMEOUT(!client.available(), 3000);
        QCOMPARE(client.busy(), false);
        QCOMPARE(client.displayConfirmationState(), QStringLiteral("idle"));
        QCOMPARE(client.displayConfirmationOwned(), false);

        QVERIFY(service_.start());
        QTRY_VERIFY_WITH_TIMEOUT(client.available(), 3000);
        QVERIFY(service_.releaseNextPreview(true));
        QTest::qWait(100);
        QCOMPARE(client.displayConfirmationState(), QStringLiteral("idle"));
        QCOMPARE(client.displayConfirmationOwned(), false);
        QCOMPARE(failures.size(), 0);
    }

    void acceptsConfirmReplyAfterCommittingAndIdleProperties()
    {
        service_.setPendingBehavior(FakeCompositor::PendingBehavior::Success);
        service_.setAwaitingConfirmation();
        QVERIFY(service_.start());

        HyprShelld::CompositorClient client(bus_, nullptr);
        QTRY_VERIFY_WITH_TIMEOUT(client.available(), 3000);
        QTRY_VERIFY_WITH_TIMEOUT(client.displayConfirmationOwned(), 3000);
        QSignalSpy failures(
            &client,
            &HyprShelld::CompositorClient::operationFailed
        );
        QVERIFY(failures.isValid());

        client.confirmDisplayConfiguration();
        QVERIFY(client.busy());
        QTRY_VERIFY_WITH_TIMEOUT(!client.busy(), 3000);
        QTRY_COMPARE_WITH_TIMEOUT(
            client.displayConfirmationState(),
            QStringLiteral("idle"),
            3000
        );
        QTRY_VERIFY_WITH_TIMEOUT(client.available(), 3000);
        QCOMPARE(failures.size(), 0);
        QVERIFY(client.lastErrorName().isEmpty());
        QCOMPARE(client.displayConfirmationOwned(), false);
    }

    void acceptsRevertReplyAfterRevertingAndIdleProperties()
    {
        service_.setPendingBehavior(FakeCompositor::PendingBehavior::Success);
        service_.setAwaitingConfirmation();
        QVERIFY(service_.start());

        HyprShelld::CompositorClient client(bus_, nullptr);
        QTRY_VERIFY_WITH_TIMEOUT(client.available(), 3000);
        QTRY_VERIFY_WITH_TIMEOUT(client.displayConfirmationOwned(), 3000);
        QSignalSpy failures(
            &client,
            &HyprShelld::CompositorClient::operationFailed
        );
        QVERIFY(failures.isValid());

        client.revertDisplayConfiguration();
        QVERIFY(client.busy());
        QTRY_VERIFY_WITH_TIMEOUT(!client.busy(), 3000);
        QTRY_COMPARE_WITH_TIMEOUT(
            client.displayConfirmationState(),
            QStringLiteral("idle"),
            3000
        );
        QTRY_VERIFY_WITH_TIMEOUT(client.available(), 3000);
        QCOMPARE(failures.size(), 0);
        QVERIFY(client.lastErrorName().isEmpty());
        QCOMPARE(client.displayConfirmationOwned(), false);
        QCOMPARE(client.revision(), baselineRevision);
    }

private:
    QDBusConnection bus_ = QDBusConnection::connectToBus(
        QDBusConnection::SessionBus,
        QStringLiteral("compositor-client-test")
    );
    QDBusConnection serviceBus_ = QDBusConnection::connectToBus(
        QDBusConnection::SessionBus,
        QStringLiteral("compositor-service-test")
    );
    FakeCompositor service_{serviceBus_};
};

QTEST_GUILESS_MAIN(CompositorClientTest)

#include "compositor_client_test.moc"
