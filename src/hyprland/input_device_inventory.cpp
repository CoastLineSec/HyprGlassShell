#include "input_device_inventory.h"

#include "json_support.h"

#include <QCryptographicHash>
#include <QJsonArray>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSet>

#include <algorithm>
#include <array>
#include <cmath>
#include <utility>

namespace HyprShelld::Hyprland {
namespace {

constexpr int maximumInventoryJsonDepth = 16;
constexpr qsizetype maximumSelectorCodeUnits = 256;
constexpr qsizetype maximumDiagnosticCodeUnits = 512;

struct PrivateAddressableRecord final {
    QString address;
    QString kind;
    QString selector;
};

struct PrivateUnaddressableRecord final {
    QString address;
    QString kind;
    QString parentAddress;
    QString parentSelector;
};

void addError(
    ValidationErrors &errors,
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

[[nodiscard]] QSet<QString> objectKeys(const QJsonObject &object)
{
    QSet<QString> keys;
    for (auto iterator = object.constBegin(); iterator != object.constEnd();
         ++iterator) {
        keys.insert(iterator.key());
    }
    return keys;
}

[[nodiscard]] bool exactShape(
    const QJsonObject &object,
    const QSet<QString> &expected
)
{
    return objectKeys(object) == expected;
}

[[nodiscard]] bool boundedCleanString(
    const QJsonValue &value,
    QString &result,
    const qsizetype minimum,
    const qsizetype maximum
)
{
    if (!value.isString()) return false;
    result = value.toString();
    if (result.size() < minimum || result.size() > maximum
        || result != result.normalized(QString::NormalizationForm_C)) {
        return false;
    }
    for (const auto codePoint : result.toUcs4()) {
        const auto category = QChar::category(static_cast<char32_t>(codePoint));
        if (category == QChar::Other_Control
            || category == QChar::Other_Format) {
            return false;
        }
    }
    return true;
}

[[nodiscard]] bool finiteNumber(const QJsonValue &value)
{
    return value.isDouble() && std::isfinite(value.toDouble());
}

[[nodiscard]] std::optional<quint32> exactUnsignedInteger(
    const QJsonValue &value,
    const quint32 maximum
)
{
    if (!value.isDouble() || !std::isfinite(value.toDouble())
        || std::floor(value.toDouble()) != value.toDouble()
        || value.toDouble() < 0.0
        || value.toDouble() > static_cast<double>(maximum)) {
        return std::nullopt;
    }
    return static_cast<quint32>(value.toDouble());
}

[[nodiscard]] bool validAddress(
    const QString &address,
    const bool allowZero
)
{
    static const QRegularExpression nonzero(
        QStringLiteral("^0x[1-9a-f][0-9a-f]{0,15}$")
    );
    static const QRegularExpression includingZero(
        QStringLiteral("^0x(?:0|[1-9a-f][0-9a-f]{0,15})$")
    );
    return (allowZero ? includingZero : nonzero).match(address).hasMatch();
}

[[nodiscard]] bool jsonWhitespace(const char character)
{
    return character == ' ' || character == '\t'
        || character == '\r' || character == '\n';
}

// Hyprland 0.56 emits one non-JSON token for an unavailable keyboard layout
// index. This scanner deliberately recognizes only the exact raw object key
// and exact bare value. Everything else remains untouched for the strict JSON
// reader to reject.
[[nodiscard]] QByteArray normalizeActiveLayoutNone(QByteArrayView reply)
{
    QByteArray normalized(reply.data(), reply.size());
    constexpr QByteArrayView key("\"active_layout_index\"");

    qsizetype position = 0;
    while (position < normalized.size()) {
        if (normalized.at(position) != '"') {
            ++position;
            continue;
        }

        const auto stringStart = position;
        ++position;
        while (position < normalized.size()) {
            const auto character = normalized.at(position++);
            if (character == '\\') {
                if (position < normalized.size()) ++position;
                continue;
            }
            if (character == '"') break;
        }
        const auto stringEnd = position;
        if (stringEnd - stringStart != key.size()
            || QByteArrayView(normalized).sliced(stringStart, key.size())
                   != key) {
            continue;
        }

        auto cursor = stringEnd;
        while (cursor < normalized.size()
               && jsonWhitespace(normalized.at(cursor))) ++cursor;
        if (cursor >= normalized.size() || normalized.at(cursor++) != ':') {
            continue;
        }
        while (cursor < normalized.size()
               && jsonWhitespace(normalized.at(cursor))) ++cursor;
        if (cursor + 4 > normalized.size()
            || QByteArrayView(normalized).sliced(cursor, 4)
                   != QByteArrayView("none")) {
            continue;
        }
        auto delimiter = cursor + 4;
        while (delimiter < normalized.size()
               && jsonWhitespace(normalized.at(delimiter))) ++delimiter;
        if (delimiter >= normalized.size()
            || (normalized.at(delimiter) != ','
                && normalized.at(delimiter) != '}')) {
            continue;
        }
        normalized.replace(cursor, 4, "null");
    }
    return normalized;
}

[[nodiscard]] bool registerTopLevelAddress(
    const QJsonValue &value,
    QString &address,
    QSet<QString> &addresses,
    ValidationErrors &errors,
    const QString &path
)
{
    if (!boundedCleanString(value, address, 3, 18)
        || !validAddress(address, false)) {
        addError(
            errors, path + QStringLiteral(".address"),
            QStringLiteral("input-devices.invalid-address"),
            QStringLiteral("A device has an invalid private runtime address.")
        );
        return false;
    }
    if (addresses.contains(address)) {
        addError(
            errors, path + QStringLiteral(".address"),
            QStringLiteral("input-devices.duplicate-address"),
            QStringLiteral("Hyprland returned a duplicate private device address.")
        );
        return false;
    }
    addresses.insert(address);
    return true;
}

void appendAddressable(
    QVector<ConnectedInputDevice> &records,
    QVector<PrivateAddressableRecord> &privateRecords,
    QString address,
    QString selector,
    const ConnectedInputDeviceKind kind,
    std::optional<QString> activeKeymap = std::nullopt
)
{
    const auto kindName = connectedInputDeviceKindName(kind);
    records.append({
        .sessionSelector = selector,
        .observedKind = kind,
        .activeKeymap = std::move(activeKeymap),
    });
    privateRecords.append({
        .address = std::move(address),
        .kind = kindName,
        .selector = std::move(selector),
    });
}

[[nodiscard]] QJsonObject publicRecordObject(
    const ConnectedInputDevice &record
)
{
    return {
        {QStringLiteral("sessionSelector"), record.sessionSelector},
        {QStringLiteral("observedKind"),
         connectedInputDeviceKindName(record.observedKind)},
        {QStringLiteral("activeKeymap"), record.activeKeymap
             ? QJsonValue(*record.activeKeymap)
             : QJsonValue(QJsonValue::Null)},
    };
}

[[nodiscard]] std::optional<ConnectedInputDeviceKind> inputDeviceKind(
    const QStringView name
)
{
    if (name == QStringView(u"keyboard")) {
        return ConnectedInputDeviceKind::Keyboard;
    }
    if (name == QStringView(u"pointer")) {
        return ConnectedInputDeviceKind::Pointer;
    }
    if (name == QStringView(u"touch")) {
        return ConnectedInputDeviceKind::Touch;
    }
    if (name == QStringView(u"tablet")) {
        return ConnectedInputDeviceKind::Tablet;
    }
    return std::nullopt;
}

} // namespace

QString connectedInputDeviceKindName(const ConnectedInputDeviceKind kind)
{
    switch (kind) {
    case ConnectedInputDeviceKind::Keyboard:
        return QStringLiteral("keyboard");
    case ConnectedInputDeviceKind::Pointer:
        return QStringLiteral("pointer");
    case ConnectedInputDeviceKind::Touch:
        return QStringLiteral("touch");
    case ConnectedInputDeviceKind::Tablet:
        return QStringLiteral("tablet");
    }
    Q_UNREACHABLE_RETURN(QString());
}

ValidationResult<ConnectedInputDeviceInventory>
parseConnectedInputDeviceInventoryDocument(const QByteArrayView document)
{
    ValidationResult<ConnectedInputDeviceInventory> result;
    if (document.size() > maximumInputDeviceInventoryBytes) {
        addError(
            result.errors, QStringLiteral("$"),
            QStringLiteral("input-devices.document-too-large"),
            QStringLiteral("The connected-input-device inventory exceeded its size limit.")
        );
        return result;
    }

    const auto parsed = JsonSupport::parseStrictObject(
        document, maximumInputDeviceInventoryBytes, maximumInventoryJsonDepth
    );
    if (!parsed) {
        result.errors = parsed.errors;
        return result;
    }
    const auto &root = *parsed.value;
    static const QSet<QString> rootFields{
        QStringLiteral("formatVersion"),
        QStringLiteral("inventoryDigest"),
        QStringLiteral("records"),
        QStringLiteral("unaddressable"),
    };
    if (!exactShape(root, rootFields)) {
        addError(
            result.errors, QStringLiteral("$"),
            QStringLiteral("input-devices.invalid-document-shape"),
            QStringLiteral("The input-device inventory has an unexpected shape.")
        );
    }
    const auto version = exactUnsignedInteger(
        root.value(QStringLiteral("formatVersion")),
        currentInputDeviceInventoryFormatVersion
    );
    if (!version || *version != currentInputDeviceInventoryFormatVersion) {
        addError(
            result.errors, QStringLiteral("$.formatVersion"),
            QStringLiteral("input-devices.unsupported-document-version"),
            QStringLiteral("Only input-device inventory format version 1 is supported.")
        );
    }

    static const QRegularExpression digestExpression(
        QStringLiteral("^[0-9a-f]{64}$")
    );
    const auto digestValue = root.value(QStringLiteral("inventoryDigest"));
    const auto digest = digestValue.toString();
    if (!digestValue.isString()
        || !digestExpression.match(digest).hasMatch()) {
        addError(
            result.errors, QStringLiteral("$.inventoryDigest"),
            QStringLiteral("input-devices.invalid-inventory-digest"),
            QStringLiteral("A lowercase SHA-256 inventory digest is required.")
        );
    }

    QVector<ConnectedInputDevice> records;
    const auto recordsValue = root.value(QStringLiteral("records"));
    if (!recordsValue.isArray()
        || recordsValue.toArray().size() > maximumConnectedInputDevices) {
        addError(
            result.errors, QStringLiteral("$.records"),
            QStringLiteral("input-devices.invalid-public-records"),
            QStringLiteral("A bounded input-device record array is required.")
        );
    } else {
        static const QSet<QString> recordFields{
            QStringLiteral("sessionSelector"),
            QStringLiteral("observedKind"),
            QStringLiteral("activeKeymap"),
        };
        const auto array = recordsValue.toArray();
        records.reserve(array.size());
        QSet<QString> selectors;
        std::optional<ConnectedInputDevice> previous;
        for (qsizetype index = 0; index < array.size(); ++index) {
            const auto path = QStringLiteral("$.records[%1]").arg(index);
            if (!array.at(index).isObject()) {
                addError(result.errors, path,
                         QStringLiteral("input-devices.public-record-object-required"),
                         QStringLiteral("An inventory record is not an object."));
                continue;
            }
            const auto object = array.at(index).toObject();
            QString selector;
            QString kindName;
            if (!exactShape(object, recordFields)
                || !boundedCleanString(
                    object.value(QStringLiteral("sessionSelector")),
                    selector, 1, maximumSelectorCodeUnits
                )
                || !boundedCleanString(
                    object.value(QStringLiteral("observedKind")),
                    kindName, 1, 16
                )) {
                addError(result.errors, path,
                         QStringLiteral("input-devices.invalid-public-record"),
                         QStringLiteral("An inventory record has invalid fields."));
                continue;
            }
            const auto kind = inputDeviceKind(kindName);
            if (!kind) {
                addError(result.errors, path + QStringLiteral(".observedKind"),
                         QStringLiteral("input-devices.invalid-public-kind"),
                         QStringLiteral("An inventory record has an unknown observed kind."));
                continue;
            }

            std::optional<QString> activeKeymap;
            const auto activeKeymapValue = object.value(
                QStringLiteral("activeKeymap")
            );
            if (*kind == ConnectedInputDeviceKind::Keyboard) {
                if (!activeKeymapValue.isNull()) {
                    QString value;
                    if (!boundedCleanString(
                            activeKeymapValue, value, 0,
                            maximumDiagnosticCodeUnits
                        )) {
                        addError(result.errors,
                                 path + QStringLiteral(".activeKeymap"),
                                 QStringLiteral("input-devices.invalid-active-keymap"),
                                 QStringLiteral("A keyboard inventory record has invalid diagnostic keymap text."));
                        continue;
                    }
                    activeKeymap = std::move(value);
                }
            } else if (!activeKeymapValue.isNull()) {
                addError(result.errors,
                         path + QStringLiteral(".activeKeymap"),
                         QStringLiteral("input-devices.unexpected-active-keymap"),
                         QStringLiteral("Only keyboard inventory records may carry active-keymap text."));
                continue;
            }

            ConnectedInputDevice record{
                .sessionSelector = std::move(selector),
                .observedKind = *kind,
                .activeKeymap = std::move(activeKeymap),
            };
            if (selectors.contains(record.sessionSelector)) {
                addError(result.errors, path,
                         QStringLiteral("input-devices.duplicate-selector"),
                         QStringLiteral("The inventory contains a duplicate public selector."));
                continue;
            }
            selectors.insert(record.sessionSelector);
            if (previous
                && (previous->observedKind > record.observedKind
                    || (previous->observedKind == record.observedKind
                        && QString::compare(
                            previous->sessionSelector,
                            record.sessionSelector, Qt::CaseSensitive
                        ) >= 0))) {
                addError(result.errors, path,
                         QStringLiteral("input-devices.invalid-record-order"),
                         QStringLiteral("The inventory records are not in canonical order."));
                continue;
            }
            previous = record;
            records.append(std::move(record));
        }
    }

    UnaddressableInputDeviceCounts unaddressable;
    const auto unaddressableValue = root.value(
        QStringLiteral("unaddressable")
    );
    static const QSet<QString> unaddressableFields{
        QStringLiteral("switches"),
        QStringLiteral("tabletPads"),
        QStringLiteral("tabletTools"),
    };
    if (!unaddressableValue.isObject()
        || !exactShape(unaddressableValue.toObject(), unaddressableFields)) {
        addError(
            result.errors, QStringLiteral("$.unaddressable"),
            QStringLiteral("input-devices.invalid-unaddressable-shape"),
            QStringLiteral("The unaddressable input-device counts are malformed.")
        );
    } else {
        const auto object = unaddressableValue.toObject();
        const auto switches = exactUnsignedInteger(
            object.value(QStringLiteral("switches")),
            maximumConnectedInputDevices
        );
        const auto pads = exactUnsignedInteger(
            object.value(QStringLiteral("tabletPads")),
            maximumConnectedInputDevices
        );
        const auto tools = exactUnsignedInteger(
            object.value(QStringLiteral("tabletTools")),
            maximumConnectedInputDevices
        );
        if (!switches || !pads || !tools
            || static_cast<quint64>(records.size()) + *switches
                 + *pads + *tools > maximumConnectedInputDevices) {
            addError(
                result.errors, QStringLiteral("$.unaddressable"),
                QStringLiteral("input-devices.invalid-unaddressable-counts"),
                QStringLiteral("The unaddressable input-device counts exceed their bounds.")
            );
        } else {
            unaddressable = {
                .switches = *switches,
                .tabletPads = *pads,
                .tabletTools = *tools,
            };
        }
    }

    auto canonical = JsonSupport::canonicalJson(root);
    canonical.append('\n');
    if (canonical != document) {
        addError(
            result.errors, QStringLiteral("$"),
            QStringLiteral("input-devices.noncanonical-document"),
            QStringLiteral("The inventory must use canonical JSON with one trailing newline.")
        );
    }
    if (!result.errors.isEmpty()) return result;

    result.value = ConnectedInputDeviceInventory{
        .records = std::move(records),
        .unaddressable = unaddressable,
        .inventoryDigest = digest,
        .document = document.toByteArray(),
    };
    return result;
}

ValidationResult<ConnectedInputDeviceInventory>
parseConnectedInputDeviceInventory(
    const QByteArrayView hyprlandReply,
    const QStringView authenticatedRuntimeIdentity,
    const QByteArrayView serviceEpoch
)
{
    ValidationResult<ConnectedInputDeviceInventory> result;
    if (hyprlandReply.size() > maximumInputDeviceInventoryBytes) {
        addError(
            result.errors, QStringLiteral("$"),
            QStringLiteral("input-devices.reply-too-large"),
            QStringLiteral("The connected-input-device reply exceeded its size limit.")
        );
        return result;
    }

    QString runtimeIdentity;
    if (!boundedCleanString(
            QJsonValue(authenticatedRuntimeIdentity.toString()),
            runtimeIdentity, 1, 512
        ) || serviceEpoch.isEmpty() || serviceEpoch.size() > 512) {
        addError(
            result.errors, QStringLiteral("$"),
            QStringLiteral("input-devices.invalid-fingerprint-authority"),
            QStringLiteral("The input-device fingerprint authority is incomplete.")
        );
        return result;
    }

    const auto normalized = normalizeActiveLayoutNone(hyprlandReply);
    const auto parsed = JsonSupport::parseStrictObject(
        QByteArrayView(normalized), maximumInputDeviceInventoryBytes,
        maximumInventoryJsonDepth
    );
    if (!parsed) {
        result.errors = parsed.errors;
        return result;
    }
    const auto &root = *parsed.value;
    static const QSet<QString> rootFields{
        QStringLiteral("mice"),
        QStringLiteral("keyboards"),
        QStringLiteral("tablets"),
        QStringLiteral("touch"),
        QStringLiteral("switches"),
    };
    if (!exactShape(root, rootFields)) {
        addError(
            result.errors, QStringLiteral("$"),
            QStringLiteral("input-devices.invalid-root-shape"),
            QStringLiteral("Hyprland returned an unexpected device-inventory shape.")
        );
        return result;
    }

    const std::array<QString, 5> arrayNames{
        QStringLiteral("mice"), QStringLiteral("keyboards"),
        QStringLiteral("tablets"), QStringLiteral("touch"),
        QStringLiteral("switches"),
    };
    qsizetype aggregateCount = 0;
    for (const auto &name : arrayNames) {
        const auto value = root.value(name);
        if (!value.isArray()) {
            addError(
                result.errors, QStringLiteral("$.") + name,
                QStringLiteral("input-devices.array-required"),
                QStringLiteral("Every device-inventory collection must be an array.")
            );
            continue;
        }
        aggregateCount += value.toArray().size();
    }
    if (!result.errors.isEmpty()) return result;
    if (aggregateCount > maximumConnectedInputDevices) {
        addError(
            result.errors, QStringLiteral("$"),
            QStringLiteral("input-devices.too-many-devices"),
            QStringLiteral("Hyprland returned too many input-device records.")
        );
        return result;
    }

    QVector<ConnectedInputDevice> records;
    QVector<PrivateAddressableRecord> privateAddressable;
    QVector<PrivateUnaddressableRecord> privateUnaddressable;
    UnaddressableInputDeviceCounts unaddressable;
    QSet<QString> addresses;
    records.reserve(aggregateCount);
    privateAddressable.reserve(aggregateCount);
    privateUnaddressable.reserve(aggregateCount);

    static const QSet<QString> mouseFields{
        QStringLiteral("address"), QStringLiteral("name"),
        QStringLiteral("defaultSpeed"), QStringLiteral("scrollFactor"),
    };
    const auto mice = root.value(QStringLiteral("mice")).toArray();
    for (qsizetype index = 0; index < mice.size(); ++index) {
        const auto path = QStringLiteral("$.mice[%1]").arg(index);
        if (!mice.at(index).isObject()) {
            addError(result.errors, path,
                     QStringLiteral("input-devices.object-required"),
                     QStringLiteral("A pointer record is not an object."));
            continue;
        }
        const auto object = mice.at(index).toObject();
        QString address;
        QString selector;
        const auto valid = exactShape(object, mouseFields)
            && registerTopLevelAddress(
                object.value(QStringLiteral("address")), address, addresses,
                result.errors, path
            )
            && boundedCleanString(
                object.value(QStringLiteral("name")), selector, 1,
                maximumSelectorCodeUnits
            )
            && finiteNumber(object.value(QStringLiteral("defaultSpeed")))
            && finiteNumber(object.value(QStringLiteral("scrollFactor")));
        if (!valid) {
            if (result.errors.isEmpty()
                || result.errors.back().path != path + QStringLiteral(".address")) {
                addError(result.errors, path,
                         QStringLiteral("input-devices.invalid-pointer"),
                         QStringLiteral("Hyprland returned an invalid pointer record."));
            }
            continue;
        }
        appendAddressable(records, privateAddressable, std::move(address),
                          std::move(selector), ConnectedInputDeviceKind::Pointer);
    }

    static const QSet<QString> keyboardFields{
        QStringLiteral("address"), QStringLiteral("name"),
        QStringLiteral("rules"), QStringLiteral("model"),
        QStringLiteral("layout"), QStringLiteral("variant"),
        QStringLiteral("options"), QStringLiteral("active_layout_index"),
        QStringLiteral("active_keymap"), QStringLiteral("capsLock"),
        QStringLiteral("numLock"), QStringLiteral("main"),
    };
    const auto keyboards = root.value(QStringLiteral("keyboards")).toArray();
    for (qsizetype index = 0; index < keyboards.size(); ++index) {
        const auto path = QStringLiteral("$.keyboards[%1]").arg(index);
        if (!keyboards.at(index).isObject()) {
            addError(result.errors, path,
                     QStringLiteral("input-devices.object-required"),
                     QStringLiteral("A keyboard record is not an object."));
            continue;
        }
        const auto object = keyboards.at(index).toObject();
        QString address;
        QString selector;
        QString activeKeymap;
        QString ignored;
        bool valid = exactShape(object, keyboardFields)
            && registerTopLevelAddress(
                object.value(QStringLiteral("address")), address, addresses,
                result.errors, path
            )
            && boundedCleanString(
                object.value(QStringLiteral("name")), selector, 1,
                maximumSelectorCodeUnits
            );
        for (const auto &field : {
                 QStringLiteral("rules"), QStringLiteral("model"),
                 QStringLiteral("layout"), QStringLiteral("variant"),
                 QStringLiteral("options")}) {
            valid = boundedCleanString(
                object.value(field), ignored, 0, maximumDiagnosticCodeUnits
            ) && valid;
        }
        valid = boundedCleanString(
            object.value(QStringLiteral("active_keymap")), activeKeymap, 0,
            maximumDiagnosticCodeUnits
        ) && valid;
        const auto layoutIndex = object.value(
            QStringLiteral("active_layout_index")
        );
        valid = (layoutIndex.isNull()
                 || exactUnsignedInteger(layoutIndex, 255).has_value())
            && object.value(QStringLiteral("capsLock")).isBool()
            && object.value(QStringLiteral("numLock")).isBool()
            && object.value(QStringLiteral("main")).isBool()
            && valid;
        if (!valid) {
            if (result.errors.isEmpty()
                || result.errors.back().path != path + QStringLiteral(".address")) {
                addError(result.errors, path,
                         QStringLiteral("input-devices.invalid-keyboard"),
                         QStringLiteral("Hyprland returned an invalid keyboard record."));
            }
            continue;
        }
        appendAddressable(
            records, privateAddressable, std::move(address),
            std::move(selector), ConnectedInputDeviceKind::Keyboard,
            std::move(activeKeymap)
        );
    }

    static const QSet<QString> tabletFields{
        QStringLiteral("address"), QStringLiteral("name"),
    };
    static const QSet<QString> tabletPadFields{
        QStringLiteral("address"), QStringLiteral("type"),
        QStringLiteral("belongsTo"),
    };
    static const QSet<QString> tabletToolFields{
        QStringLiteral("address"), QStringLiteral("type"),
    };
    static const QSet<QString> parentFields{
        QStringLiteral("address"), QStringLiteral("name"),
    };
    const auto tablets = root.value(QStringLiteral("tablets")).toArray();
    for (qsizetype index = 0; index < tablets.size(); ++index) {
        const auto path = QStringLiteral("$.tablets[%1]").arg(index);
        if (!tablets.at(index).isObject()) {
            addError(result.errors, path,
                     QStringLiteral("input-devices.object-required"),
                     QStringLiteral("A tablet record is not an object."));
            continue;
        }
        const auto object = tablets.at(index).toObject();
        QString address;
        if (!registerTopLevelAddress(
                object.value(QStringLiteral("address")), address, addresses,
                result.errors, path
            )) {
            continue;
        }
        if (exactShape(object, tabletFields)) {
            QString selector;
            if (!boundedCleanString(
                    object.value(QStringLiteral("name")), selector, 1,
                    maximumSelectorCodeUnits
                )) {
                addError(result.errors, path,
                         QStringLiteral("input-devices.invalid-tablet"),
                         QStringLiteral("Hyprland returned an invalid tablet record."));
                continue;
            }
            appendAddressable(
                records, privateAddressable, std::move(address),
                std::move(selector), ConnectedInputDeviceKind::Tablet
            );
            continue;
        }

        const auto typeValue = object.value(QStringLiteral("type"));
        if (!typeValue.isString()) {
            addError(result.errors, path,
                     QStringLiteral("input-devices.invalid-tablet-variant"),
                     QStringLiteral("Hyprland returned an unknown tablet record."));
            continue;
        }
        const auto type = typeValue.toString();
        if (type == QStringLiteral("tabletTool")
            && exactShape(object, tabletToolFields)) {
            ++unaddressable.tabletTools;
            privateUnaddressable.append({
                .address = std::move(address),
                .kind = QStringLiteral("tabletTool"),
            });
            continue;
        }
        if (type == QStringLiteral("tabletPad")
            && exactShape(object, tabletPadFields)
            && object.value(QStringLiteral("belongsTo")).isObject()) {
            const auto parent = object.value(QStringLiteral("belongsTo")).toObject();
            QString parentAddress;
            QString parentSelector;
            if (!exactShape(parent, parentFields)
                || !boundedCleanString(
                    parent.value(QStringLiteral("address")), parentAddress,
                    3, 18
                ) || !validAddress(parentAddress, true)
                || !boundedCleanString(
                    parent.value(QStringLiteral("name")), parentSelector,
                    0, maximumSelectorCodeUnits
                )) {
                addError(result.errors, path,
                         QStringLiteral("input-devices.invalid-tablet-pad"),
                         QStringLiteral("Hyprland returned an invalid tablet-pad record."));
                continue;
            }
            ++unaddressable.tabletPads;
            privateUnaddressable.append({
                .address = std::move(address),
                .kind = QStringLiteral("tabletPad"),
                .parentAddress = std::move(parentAddress),
                .parentSelector = std::move(parentSelector),
            });
            continue;
        }
        addError(result.errors, path,
                 QStringLiteral("input-devices.invalid-tablet-variant"),
                 QStringLiteral("Hyprland returned an unknown tablet record."));
    }

    static const QSet<QString> touchFields{
        QStringLiteral("address"), QStringLiteral("name"),
    };
    const auto touch = root.value(QStringLiteral("touch")).toArray();
    for (qsizetype index = 0; index < touch.size(); ++index) {
        const auto path = QStringLiteral("$.touch[%1]").arg(index);
        if (!touch.at(index).isObject()) {
            addError(result.errors, path,
                     QStringLiteral("input-devices.object-required"),
                     QStringLiteral("A touch record is not an object."));
            continue;
        }
        const auto object = touch.at(index).toObject();
        QString address;
        QString selector;
        const auto valid = exactShape(object, touchFields)
            && registerTopLevelAddress(
                object.value(QStringLiteral("address")), address, addresses,
                result.errors, path
            )
            && boundedCleanString(
                object.value(QStringLiteral("name")), selector, 1,
                maximumSelectorCodeUnits
            );
        if (!valid) {
            if (result.errors.isEmpty()
                || result.errors.back().path != path + QStringLiteral(".address")) {
                addError(result.errors, path,
                         QStringLiteral("input-devices.invalid-touch"),
                         QStringLiteral("Hyprland returned an invalid touch record."));
            }
            continue;
        }
        appendAddressable(records, privateAddressable, std::move(address),
                          std::move(selector), ConnectedInputDeviceKind::Touch);
    }

    static const QSet<QString> switchFields{
        QStringLiteral("address"), QStringLiteral("name"),
    };
    const auto switches = root.value(QStringLiteral("switches")).toArray();
    for (qsizetype index = 0; index < switches.size(); ++index) {
        const auto path = QStringLiteral("$.switches[%1]").arg(index);
        if (!switches.at(index).isObject()) {
            addError(result.errors, path,
                     QStringLiteral("input-devices.object-required"),
                     QStringLiteral("A switch record is not an object."));
            continue;
        }
        const auto object = switches.at(index).toObject();
        QString address;
        QString ignoredName;
        const auto valid = exactShape(object, switchFields)
            && registerTopLevelAddress(
                object.value(QStringLiteral("address")), address, addresses,
                result.errors, path
            )
            && boundedCleanString(
                object.value(QStringLiteral("name")), ignoredName, 0,
                maximumSelectorCodeUnits
            );
        if (!valid) {
            if (result.errors.isEmpty()
                || result.errors.back().path != path + QStringLiteral(".address")) {
                addError(result.errors, path,
                         QStringLiteral("input-devices.invalid-switch"),
                         QStringLiteral("Hyprland returned an invalid switch record."));
            }
            continue;
        }
        ++unaddressable.switches;
        privateUnaddressable.append({
            .address = std::move(address),
            .kind = QStringLiteral("switch"),
        });
    }

    if (!result.errors.isEmpty()) return result;

    std::ranges::sort(records, [](const auto &left, const auto &right) {
        if (left.observedKind != right.observedKind) {
            return left.observedKind < right.observedKind;
        }
        return QString::compare(
            left.sessionSelector, right.sessionSelector, Qt::CaseSensitive
        ) < 0;
    });
    QSet<QString> publicSelectors;
    for (const auto &record : records) {
        if (publicSelectors.contains(record.sessionSelector)) {
            addError(
                result.errors, QStringLiteral("$.records"),
                QStringLiteral("input-devices.duplicate-selector"),
                QStringLiteral("Hyprland returned a duplicate public device selector.")
            );
            return result;
        }
        publicSelectors.insert(record.sessionSelector);
    }

    std::ranges::sort(privateAddressable, [](const auto &left, const auto &right) {
        if (left.kind != right.kind) return left.kind < right.kind;
        if (left.selector != right.selector) return left.selector < right.selector;
        return left.address < right.address;
    });
    std::ranges::sort(privateUnaddressable, [](const auto &left, const auto &right) {
        if (left.kind != right.kind) return left.kind < right.kind;
        if (left.address != right.address) return left.address < right.address;
        if (left.parentAddress != right.parentAddress) {
            return left.parentAddress < right.parentAddress;
        }
        return left.parentSelector < right.parentSelector;
    });

    QJsonArray privateAddressableArray;
    for (const auto &record : privateAddressable) {
        privateAddressableArray.append(QJsonObject{
            {QStringLiteral("address"), record.address},
            {QStringLiteral("kind"), record.kind},
            {QStringLiteral("sessionSelector"), record.selector},
        });
    }
    QJsonArray privateUnaddressableArray;
    for (const auto &record : privateUnaddressable) {
        QJsonObject object{
            {QStringLiteral("address"), record.address},
            {QStringLiteral("kind"), record.kind},
        };
        if (record.kind == QStringLiteral("tabletPad")) {
            object.insert(QStringLiteral("parentAddress"), record.parentAddress);
            object.insert(QStringLiteral("parentSelector"), record.parentSelector);
        }
        privateUnaddressableArray.append(object);
    }
    const auto fingerprint = JsonSupport::canonicalJson(QJsonObject{
        {QStringLiteral("formatVersion"), 1},
        {QStringLiteral("runtimeIdentity"), runtimeIdentity},
        {QStringLiteral("serviceEpoch"),
         QString::fromLatin1(serviceEpoch.toByteArray().toHex())},
        {QStringLiteral("addressable"), privateAddressableArray},
        {QStringLiteral("unaddressable"), privateUnaddressableArray},
    });
    const auto inventoryDigest = QString::fromLatin1(
        QCryptographicHash::hash(fingerprint, QCryptographicHash::Sha256).toHex()
    );

    QJsonArray publicRecords;
    for (const auto &record : records) {
        publicRecords.append(publicRecordObject(record));
    }
    auto document = JsonSupport::canonicalJson(QJsonObject{
        {QStringLiteral("formatVersion"),
         static_cast<qint64>(currentInputDeviceInventoryFormatVersion)},
        {QStringLiteral("inventoryDigest"), inventoryDigest},
        {QStringLiteral("records"), publicRecords},
        {QStringLiteral("unaddressable"), QJsonObject{
             {QStringLiteral("switches"),
              static_cast<qint64>(unaddressable.switches)},
             {QStringLiteral("tabletPads"),
              static_cast<qint64>(unaddressable.tabletPads)},
             {QStringLiteral("tabletTools"),
              static_cast<qint64>(unaddressable.tabletTools)},
         }},
    });
    document.append('\n');
    if (document.size() > maximumInputDeviceInventoryBytes) {
        addError(
            result.errors, QStringLiteral("$"),
            QStringLiteral("input-devices.document-too-large"),
            QStringLiteral("The canonical input-device inventory exceeded its size limit.")
        );
        return result;
    }

    result.value = ConnectedInputDeviceInventory{
        .records = std::move(records),
        .unaddressable = unaddressable,
        .inventoryDigest = inventoryDigest,
        .document = std::move(document),
    };
    return result;
}

} // namespace HyprShelld::Hyprland
