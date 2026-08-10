#include "monitor_profile.h"

#include "json_support.h"

#include <QCryptographicHash>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSet>

#include <algorithm>
#include <cmath>
#include <limits>

namespace HyprShelld::Hyprland {
namespace {

constexpr int maximumProfileDepth = 32;

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

[[nodiscard]] bool validDigest(const QStringView value)
{
    if (value.size() != 64) return false;
    return std::ranges::all_of(value, [](const QChar character) {
        return (character >= u'0' && character <= u'9')
            || (character >= u'a' && character <= u'f');
    });
}

[[nodiscard]] bool validConnector(const QStringView value)
{
    static const QRegularExpression expression(
        QStringLiteral("^[A-Za-z][A-Za-z0-9_.-]{0,127}$")
    );
    return expression.matchView(value).hasMatch();
}

[[nodiscard]] bool boundedString(
    const QJsonValue &value,
    QString &result,
    const qsizetype maximum = 512
)
{
    if (!value.isString()) return false;
    result = value.toString();
    if (result.size() > maximum
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

[[nodiscard]] std::optional<qint64> exactInteger(
    const QJsonValue &value,
    const qint64 minimum,
    const qint64 maximum
)
{
    if (!value.isDouble() || !std::isfinite(value.toDouble())
        || std::floor(value.toDouble()) != value.toDouble()
        || value.toDouble() < static_cast<double>(minimum)
        || value.toDouble() > static_cast<double>(maximum)) {
        return std::nullopt;
    }
    return static_cast<qint64>(value.toDouble());
}

[[nodiscard]] std::optional<double> finiteNumber(
    const QJsonValue &value,
    const double minimum,
    const double maximum
)
{
    if (!value.isDouble() || !std::isfinite(value.toDouble())
        || value.toDouble() < minimum || value.toDouble() > maximum) {
        return std::nullopt;
    }
    return value.toDouble();
}

[[nodiscard]] QString normalizedRefresh(const double refresh)
{
    auto text = QString::number(refresh, 'f', 3);
    while (text.contains(QLatin1Char('.'))
           && text.endsWith(QLatin1Char('0'))) {
        text.chop(1);
    }
    if (text.endsWith(QLatin1Char('.'))) text.chop(1);
    return text;
}

[[nodiscard]] std::optional<ConnectedDisplayMode> parseAvailableMode(
    const QString &value
)
{
    static const QRegularExpression expression(
        QStringLiteral(
            "^([1-9][0-9]{0,4})x([1-9][0-9]{0,4})@"
            "((?:0|[1-9][0-9]{0,3})\\.[0-9]{2})Hz$"
        )
    );
    const auto match = expression.match(value);
    if (!match.hasMatch()) return std::nullopt;
    bool widthOk = false;
    bool heightOk = false;
    bool refreshOk = false;
    const auto width = match.captured(1).toInt(&widthOk);
    const auto height = match.captured(2).toInt(&heightOk);
    const auto refresh = match.captured(3).toDouble(&refreshOk);
    if (!widthOk || !heightOk || !refreshOk || refresh <= 0.0
        || refresh >= 10000.0) {
        return std::nullopt;
    }
    return ConnectedDisplayMode{
        .width = width,
        .height = height,
        .refreshRate = refresh,
        .managedMode = QStringLiteral("%1x%2@%3")
                           .arg(width)
                           .arg(height)
                           .arg(normalizedRefresh(refresh)),
    };
}

[[nodiscard]] QJsonObject modeObject(const ConnectedDisplayMode &mode)
{
    return {
        {QStringLiteral("width"), mode.width},
        {QStringLiteral("height"), mode.height},
        {QStringLiteral("refreshRate"), mode.refreshRate},
        {QStringLiteral("managedMode"), mode.managedMode},
    };
}

[[nodiscard]] QJsonObject outputObject(const ConnectedDisplay &output)
{
    QJsonArray modes;
    for (const auto &mode : output.modes) modes.append(modeObject(mode));
    QJsonArray reserved;
    for (const auto extent : output.reserved) reserved.append(extent);
    return {
        {QStringLiteral("selector"), output.selector},
        {QStringLiteral("description"), output.description},
        {QStringLiteral("make"), output.make},
        {QStringLiteral("model"), output.model},
        {QStringLiteral("serial"), output.serial},
        {QStringLiteral("enabled"), output.enabled},
        {QStringLiteral("width"), output.width},
        {QStringLiteral("height"), output.height},
        {QStringLiteral("physicalWidthMm"), output.physicalWidthMm},
        {QStringLiteral("physicalHeightMm"), output.physicalHeightMm},
        {QStringLiteral("refreshRate"), output.refreshRate},
        {QStringLiteral("x"), output.x},
        {QStringLiteral("y"), output.y},
        {QStringLiteral("reserved"), reserved},
        {QStringLiteral("scale"), output.scale},
        {QStringLiteral("transform"), output.transform},
        {QStringLiteral("focused"), output.focused},
        {QStringLiteral("dpms"), output.dpms},
        {QStringLiteral("vrrActive"), output.vrrActive},
        {QStringLiteral("mirrorOf"), output.mirrorOf},
        {QStringLiteral("modes"), modes},
        {QStringLiteral("colorManagement"), output.colorManagement},
        {QStringLiteral("currentFormat"), output.currentFormat},
        {QStringLiteral("sdrBrightness"), output.sdrBrightness},
        {QStringLiteral("sdrSaturation"), output.sdrSaturation},
        {QStringLiteral("sdrMinLuminance"), output.sdrMinLuminance},
        {QStringLiteral("sdrMaxLuminance"), output.sdrMaxLuminance},
    };
}

[[nodiscard]] QJsonObject fingerprintObject(const ConnectedDisplay &output)
{
    QJsonArray modes;
    for (const auto &mode : output.modes) modes.append(modeObject(mode));
    return {
        {QStringLiteral("selector"), output.selector},
        {QStringLiteral("description"), output.description},
        {QStringLiteral("make"), output.make},
        {QStringLiteral("model"), output.model},
        {QStringLiteral("serial"), output.serial},
        {QStringLiteral("physicalWidthMm"), output.physicalWidthMm},
        {QStringLiteral("physicalHeightMm"), output.physicalHeightMm},
        {QStringLiteral("modes"), modes},
    };
}

[[nodiscard]] QString stableMonitorId(
    const QString &selector,
    QSet<QString> &used
)
{
    // Connector names may occupy the complete 128-character selector bound.
    // Use a fixed-size deterministic identity so adding a legal connector can
    // never overflow the desired-state stable-ID grammar.
    const auto selectorDigest = QCryptographicHash::hash(
        selector.toUtf8(), QCryptographicHash::Sha256
    ).toHex();
    auto base = QStringLiteral("monitor:")
        + QString::fromLatin1(selectorDigest.first(32));
    if (!used.contains(base)) {
        used.insert(base);
        return base;
    }
    for (quint32 suffix = 2; suffix < 10000; ++suffix) {
        const auto candidate = base + QLatin1Char(':')
            + QString::number(suffix);
        if (!used.contains(candidate)) {
            used.insert(candidate);
            return candidate;
        }
    }
    return base;
}

} // namespace

ValidationResult<DisplayProfile> parseDisplayProfile(
    const QByteArrayView bytes
)
{
    ValidationResult<DisplayProfile> result;
    const auto parsed = JsonSupport::parseStrictObject(
        bytes, maximumDisplayProfileBytes, maximumProfileDepth
    );
    if (!parsed) {
        result.errors = parsed.errors;
        return result;
    }
    const auto &root = *parsed.value;
    static const QSet<QString> fields{
        QStringLiteral("formatVersion"),
        QStringLiteral("topologyDigest"),
        QStringLiteral("outputs"),
    };
    QSet<QString> observed;
    for (auto iterator = root.constBegin(); iterator != root.constEnd(); ++iterator) {
        observed.insert(iterator.key());
    }
    if (observed != fields) {
        addError(result.errors, QStringLiteral("$"),
                 QStringLiteral("display.invalid-profile-shape"),
                 QStringLiteral("A display profile must contain exactly formatVersion, topologyDigest, and outputs."));
    }
    const auto version = exactInteger(
        root.value(QStringLiteral("formatVersion")), 1, 1
    );
    if (!version) {
        addError(result.errors, QStringLiteral("$.formatVersion"),
                 QStringLiteral("display.unsupported-profile-version"),
                 QStringLiteral("Only display profile format version 1 is supported."));
    }
    const auto digest = root.value(QStringLiteral("topologyDigest")).toString();
    if (!root.value(QStringLiteral("topologyDigest")).isString()
        || !validDigest(digest)) {
        addError(result.errors, QStringLiteral("$.topologyDigest"),
                 QStringLiteral("display.invalid-topology-digest"),
                 QStringLiteral("A lowercase SHA-256 topology digest is required."));
    }
    const auto outputsValue = root.value(QStringLiteral("outputs"));
    if (!outputsValue.isArray()
        || outputsValue.toArray().size() > maximumMonitors) {
        addError(result.errors, QStringLiteral("$.outputs"),
                 QStringLiteral("display.invalid-output-list"),
                 QStringLiteral("A bounded display-output array is required."));
    } else {
        const auto outputs = outputsValue.toArray();
        for (qsizetype index = 0; index < outputs.size(); ++index) {
            if (!outputs.at(index).isObject()) {
                addError(result.errors,
                         QStringLiteral("$.outputs[%1]").arg(index),
                         QStringLiteral("display.output-object-required"),
                         QStringLiteral("Each display output must be an object."));
            }
        }
    }
    auto canonical = JsonSupport::canonicalJson(root);
    canonical.append('\n');
    if (QByteArrayView(canonical) != bytes) {
        addError(result.errors, QStringLiteral("$"),
                 QStringLiteral("display.noncanonical-profile"),
                 QStringLiteral("The display profile must use canonical JSON with one trailing newline."));
    }
    if (!result.errors.isEmpty()) return result;
    result.value = DisplayProfile{
        .topologyDigest = digest,
        .outputs = outputsValue.toArray(),
    };
    return result;
}

QByteArray serializeDisplayProfile(const DisplayProfile &profile)
{
    auto bytes = JsonSupport::canonicalJson(QJsonObject{
        {QStringLiteral("formatVersion"),
         static_cast<qint64>(currentDisplayProfileFormatVersion)},
        {QStringLiteral("topologyDigest"), profile.topologyDigest},
        {QStringLiteral("outputs"), profile.outputs},
    });
    bytes.append('\n');
    return bytes;
}

ValidationResult<ConnectedDisplayTopology> parseConnectedDisplayTopology(
    const QByteArrayView reply
)
{
    ValidationResult<ConnectedDisplayTopology> result;
    if (reply.size() > maximumDisplayProfileBytes) {
        addError(result.errors, QStringLiteral("$"),
                 QStringLiteral("display.topology-too-large"),
                 QStringLiteral("The connected-display reply exceeded its size limit."));
        return result;
    }
    QByteArray wrapped = QByteArrayLiteral("{\"outputs\":");
    wrapped.append(reply.data(), reply.size());
    wrapped.append('}');
    const auto parsed = JsonSupport::parseStrictObject(
        QByteArrayView(wrapped), maximumDisplayProfileBytes + 32,
        maximumProfileDepth
    );
    if (!parsed || !parsed.value->value(QStringLiteral("outputs")).isArray()) {
        result.errors = parsed.errors;
        if (result.errors.isEmpty()) {
            addError(result.errors, QStringLiteral("$"),
                     QStringLiteral("display.invalid-topology"),
                     QStringLiteral("Hyprland returned a malformed monitor array."));
        }
        return result;
    }
    const auto array = parsed.value->value(QStringLiteral("outputs")).toArray();
    if (array.size() > maximumMonitors) {
        addError(result.errors, QStringLiteral("$"),
                 QStringLiteral("display.too-many-outputs"),
                 QStringLiteral("Hyprland returned too many connected outputs."));
        return result;
    }

    QVector<ConnectedDisplay> outputs;
    QMap<qint64, QString> selectorsById;
    outputs.reserve(array.size());
    for (qsizetype index = 0; index < array.size(); ++index) {
        const auto path = QStringLiteral("$[%1]").arg(index);
        if (!array.at(index).isObject()) {
            addError(result.errors, path,
                     QStringLiteral("display.invalid-topology-output"),
                     QStringLiteral("A topology output is not an object."));
            continue;
        }
        const auto object = array.at(index).toObject();
        ConnectedDisplay output;
        const auto id = exactInteger(object.value(QStringLiteral("id")),
                                     0, std::numeric_limits<qint32>::max());
        QString selector;
        if (!id || !boundedString(object.value(QStringLiteral("name")), selector, 128)
            || !validConnector(selector)) {
            addError(result.errors, path,
                     QStringLiteral("display.invalid-topology-identity"),
                     QStringLiteral("A topology output has an invalid ID or connector name."));
            continue;
        }
        output.upstreamId = *id;
        output.selector = selector;
        if (selectorsById.contains(*id) || selectorsById.values().contains(selector)) {
            addError(result.errors, path,
                     QStringLiteral("display.duplicate-topology-output"),
                     QStringLiteral("Hyprland returned a duplicate output identity."));
            continue;
        }
        selectorsById.insert(*id, selector);
        const auto stringsValid =
            boundedString(object.value(QStringLiteral("description")), output.description)
            && boundedString(object.value(QStringLiteral("make")), output.make)
            && boundedString(object.value(QStringLiteral("model")), output.model)
            && boundedString(object.value(QStringLiteral("serial")), output.serial)
            && boundedString(object.value(QStringLiteral("colorManagementPreset")), output.colorManagement, 64)
            && boundedString(object.value(QStringLiteral("currentFormat")), output.currentFormat, 32);
        const auto width = exactInteger(object.value(QStringLiteral("width")), 0, 99999);
        const auto height = exactInteger(object.value(QStringLiteral("height")), 0, 99999);
        const auto physicalWidth = exactInteger(object.value(QStringLiteral("physicalWidth")), 0, 1000000);
        const auto physicalHeight = exactInteger(object.value(QStringLiteral("physicalHeight")), 0, 1000000);
        const auto refresh = finiteNumber(object.value(QStringLiteral("refreshRate")), 0.0, 10000.0);
        const auto x = exactInteger(object.value(QStringLiteral("x")), -1000000, 1000000);
        const auto y = exactInteger(object.value(QStringLiteral("y")), -1000000, 1000000);
        const auto reserved = object.value(QStringLiteral("reserved")).toArray();
        const auto reservedShapeValid =
            object.value(QStringLiteral("reserved")).isArray()
            && reserved.size() == 4;
        QVector<qint32> reservedWire;
        if (reservedShapeValid) {
            reservedWire.reserve(4);
            for (const auto value : reserved) {
                const auto extent = exactInteger(value, -1000000, 1000000);
                if (!extent) {
                    reservedWire.clear();
                    break;
                }
                reservedWire.append(static_cast<qint32>(*extent));
            }
        }
        const auto scale = finiteNumber(object.value(QStringLiteral("scale")), 0.01, std::numeric_limits<float>::max());
        const auto transform = exactInteger(object.value(QStringLiteral("transform")), 0, 7);
        const auto sdrBrightness = finiteNumber(
            object.value(QStringLiteral("sdrBrightness")), 0.0, 10.0
        );
        const auto sdrSaturation = finiteNumber(
            object.value(QStringLiteral("sdrSaturation")), 0.0, 10.0
        );
        const auto sdrMinLuminance = finiteNumber(
            object.value(QStringLiteral("sdrMinLuminance")), 0.0, 10000.0
        );
        const auto sdrMaxLuminance = exactInteger(
            object.value(QStringLiteral("sdrMaxLuminance")), -1,
            std::numeric_limits<qint32>::max()
        );
        if (!stringsValid || !width || !height || !physicalWidth || !physicalHeight
            || !refresh || !x || !y || !scale || !transform
            || !reservedShapeValid || reservedWire.size() != 4
            || !sdrBrightness || !sdrSaturation || !sdrMinLuminance
            || !sdrMaxLuminance
            || !object.value(QStringLiteral("focused")).isBool()
            || !object.value(QStringLiteral("dpmsStatus")).isBool()
            || !object.value(QStringLiteral("vrr")).isBool()
            || !object.value(QStringLiteral("disabled")).isBool()) {
            addError(result.errors, path,
                     QStringLiteral("display.invalid-topology-fields"),
                     QStringLiteral("Hyprland returned invalid bounded monitor fields."));
            continue;
        }
        output.enabled = !object.value(QStringLiteral("disabled")).toBool();
        output.width = static_cast<qint32>(*width);
        output.height = static_cast<qint32>(*height);
        output.physicalWidthMm = static_cast<qint32>(*physicalWidth);
        output.physicalHeightMm = static_cast<qint32>(*physicalHeight);
        output.refreshRate = *refresh;
        output.x = static_cast<qint32>(*x);
        output.y = static_cast<qint32>(*y);
        // Pinned HyprCtl JSON is left, top, right, bottom. The public
        // compositor contract and desired `reserved` field use CSS order.
        output.reserved = {
            reservedWire.at(1), reservedWire.at(2),
            reservedWire.at(3), reservedWire.at(0),
        };
        output.scale = *scale;
        output.transform = static_cast<qint32>(*transform);
        output.focused = object.value(QStringLiteral("focused")).toBool();
        output.dpms = object.value(QStringLiteral("dpmsStatus")).toBool();
        output.vrrActive = object.value(QStringLiteral("vrr")).toBool();
        output.sdrBrightness = *sdrBrightness;
        output.sdrSaturation = *sdrSaturation;
        output.sdrMinLuminance = *sdrMinLuminance;
        output.sdrMaxLuminance = *sdrMaxLuminance;
        if (!boundedString(object.value(QStringLiteral("mirrorOf")), output.mirrorOf, 32)
            || !object.value(QStringLiteral("availableModes")).isArray()
            || object.value(QStringLiteral("availableModes")).toArray().size() > 256) {
            addError(result.errors, path,
                     QStringLiteral("display.invalid-topology-modes"),
                     QStringLiteral("Hyprland returned an invalid mirror or mode list."));
            continue;
        }
        const auto modes = object.value(QStringLiteral("availableModes")).toArray();
        for (qsizetype modeIndex = 0; modeIndex < modes.size(); ++modeIndex) {
            if (!modes.at(modeIndex).isString()) {
                addError(result.errors, path,
                         QStringLiteral("display.invalid-topology-mode"),
                         QStringLiteral("An advertised monitor mode is malformed."));
                break;
            }
            const auto mode = parseAvailableMode(modes.at(modeIndex).toString());
            if (!mode) {
                addError(result.errors, path,
                         QStringLiteral("display.invalid-topology-mode"),
                         QStringLiteral("An advertised monitor mode is outside the pinned grammar."));
                break;
            }
            if (std::ranges::any_of(output.modes, [&mode](const auto &existing) {
                    return existing.managedMode == mode->managedMode;
                })) {
                addError(result.errors, path,
                         QStringLiteral("display.duplicate-topology-mode"),
                         QStringLiteral("Hyprland returned a duplicate advertised monitor mode."));
                break;
            }
            output.modes.append(*mode);
        }
        std::ranges::sort(output.modes, [](const auto &left, const auto &right) {
            if (left.width != right.width) return left.width < right.width;
            if (left.height != right.height) return left.height < right.height;
            return left.refreshRate < right.refreshRate;
        });
        outputs.append(std::move(output));
    }
    if (!result.errors.isEmpty()) return result;

    for (auto &output : outputs) {
        if (output.mirrorOf == QStringLiteral("none")) {
            output.mirrorOf.clear();
            continue;
        }
        static const QRegularExpression canonicalId(
            QStringLiteral("^(?:0|[1-9][0-9]{0,9})$")
        );
        if (!canonicalId.match(output.mirrorOf).hasMatch()) {
            addError(result.errors, QStringLiteral("$"),
                     QStringLiteral("display.invalid-topology-mirror"),
                     QStringLiteral("Hyprland returned a non-canonical mirror target."));
            continue;
        }
        bool converted = false;
        const auto mirrorId = output.mirrorOf.toLongLong(&converted, 10);
        if (!converted || !selectorsById.contains(mirrorId)) {
            addError(result.errors, QStringLiteral("$"),
                     QStringLiteral("display.invalid-topology-mirror"),
                     QStringLiteral("Hyprland returned an unresolved mirror target."));
            continue;
        }
        output.mirrorOf = selectorsById.value(mirrorId);
    }
    if (!result.errors.isEmpty()) return result;
    std::ranges::sort(outputs, [](const auto &left, const auto &right) {
        return left.selector < right.selector;
    });

    QJsonArray fingerprintOutputs;
    QJsonArray publicOutputs;
    for (const auto &output : outputs) {
        fingerprintOutputs.append(fingerprintObject(output));
        publicOutputs.append(outputObject(output));
    }
    const auto fingerprint = JsonSupport::canonicalJson(QJsonObject{
        {QStringLiteral("formatVersion"), 1},
        {QStringLiteral("outputs"), fingerprintOutputs},
    });
    const auto digest = QString::fromLatin1(
        QCryptographicHash::hash(fingerprint, QCryptographicHash::Sha256)
            .toHex()
    );
    auto document = JsonSupport::canonicalJson(QJsonObject{
        {QStringLiteral("formatVersion"), 1},
        {QStringLiteral("topologyDigest"), digest},
        {QStringLiteral("outputs"), publicOutputs},
    });
    document.append('\n');
    result.value = ConnectedDisplayTopology{
        .outputs = std::move(outputs),
        .topologyDigest = digest,
        .document = std::move(document),
    };
    return result;
}

ValidationErrors validateDisplayProfileTopology(
    const DisplayProfile &profile,
    const ConnectedDisplayTopology &topology
)
{
    ValidationErrors errors;
    if (profile.topologyDigest != topology.topologyDigest) {
        addError(errors, QStringLiteral("$.topologyDigest"),
                 QStringLiteral("display.stale-topology"),
                 QStringLiteral("The connected-display topology changed."));
    }
    QSet<QString> expected;
    for (const auto &output : topology.outputs) expected.insert(output.selector);
    QSet<QString> observed;
    QMap<QString, QJsonObject> records;
    for (qsizetype index = 0; index < profile.outputs.size(); ++index) {
        const auto object = profile.outputs.at(index).toObject();
        const auto selector = object.value(QStringLiteral("selector")).toString();
        if (!validConnector(selector)) {
            addError(errors, QStringLiteral("$.outputs[%1].selector").arg(index),
                     QStringLiteral("display.connector-selector-required"),
                     QStringLiteral("Live display previews require exact connector-name selectors."));
        }
        if (observed.contains(selector)) {
            addError(errors, QStringLiteral("$.outputs[%1].selector").arg(index),
                     QStringLiteral("display.duplicate-connector"),
                     QStringLiteral("A connected output appears more than once."));
        }
        observed.insert(selector);
        records.insert(selector, object);
    }
    if (observed != expected) {
        addError(errors, QStringLiteral("$.outputs"),
                 QStringLiteral("display.connector-set-changed"),
                 QStringLiteral("The profile must contain exactly the currently connected connector set."));
    }
    bool usable = false;
    for (auto iterator = records.constBegin(); iterator != records.constEnd();
         ++iterator) {
        const auto sourceEnabled = iterator->value(
            QStringLiteral("enabled")
        ).toBool(false);
        const auto mirror = iterator->value(
            QStringLiteral("mirror")
        ).toString();
        if (!sourceEnabled) continue;
        if (mirror.isEmpty()) {
            usable = true;
            continue;
        }
        const auto target = records.value(mirror);
        if (mirror == iterator.key() || target.isEmpty()
            || !target.value(QStringLiteral("enabled")).toBool(false)) {
            addError(errors, QStringLiteral("$.outputs"),
                     QStringLiteral("display.invalid-live-mirror"),
                     QStringLiteral("A live mirror requires a distinct enabled connected target."));
            continue;
        }
        if (!target.value(QStringLiteral("mirror")).toString().isEmpty()) {
            addError(errors, QStringLiteral("$.outputs"),
                     QStringLiteral("display.mirror-chain"),
                     QStringLiteral("Live display previews do not accept mirror chains."));
        }
        if (iterator->value(QStringLiteral("position"))
                != target.value(QStringLiteral("position"))) {
            addError(errors, QStringLiteral("$.outputs"),
                     QStringLiteral("display.mirror-position-mismatch"),
                     QStringLiteral("A mirrored output must use its target's desired position."));
        }
    }
    if (!usable) {
        addError(errors, QStringLiteral("$.outputs"),
                 QStringLiteral("display.no-enabled-output"),
                 QStringLiteral("At least one connected non-mirrored output must remain enabled."));
    }
    return errors;
}

ValidationErrors validateDisplayRealization(
    const DesiredState &candidate,
    const ConnectedDisplayTopology &topology
)
{
    constexpr double realizedRefreshToleranceHz = 0.00505;
    // HyprCtl publishes scale and the SDR floats at two decimal places.
    // 0.00505 keeps the exact half-step (allowing binary representation noise)
    // while rejecting the next observable value beyond that boundary.
    constexpr double realizedTwoDecimalTolerance = 0.00505;
    ValidationErrors errors;
    QMap<QString, MonitorConfiguration> records;
    for (const auto &record : candidate.monitors) {
        if (validConnector(record.selector)) records.insert(record.selector, record);
    }
    static const QRegularExpression explicitMode(
        QStringLiteral(
            "^([1-9][0-9]{0,4})x([1-9][0-9]{0,4})"
            "(?:@([1-9][0-9]{0,3}(?:\\.[0-9]{1,3})?|0\\.[0-9]{0,2}[1-9]))?$"
        )
    );
    static const QRegularExpression explicitPosition(
        QStringLiteral(
            "^(0|[+-]?(?:[1-9][0-9]{0,5}|1000000))x"
            "(0|[+-]?(?:[1-9][0-9]{0,5}|1000000))$"
        )
    );
    static const QSet<QString> automaticModes{
        QStringLiteral("preferred"), QStringLiteral("highrr"),
        QStringLiteral("highres"), QStringLiteral("maxwidth"),
    };
    for (const auto &output : topology.outputs) {
        const auto path = QStringLiteral("$.outputs[%1]").arg(output.selector);
        if (!records.contains(output.selector)) {
            addError(errors, path,
                     QStringLiteral("display.realization-selector-changed"),
                     QStringLiteral("A connected output is absent from the preview candidate."));
            continue;
        }
        const auto &record = records[output.selector];
        if (record.enabled != output.enabled) {
            addError(errors, path,
                     QStringLiteral("display.realization-enabled-mismatch"),
                     QStringLiteral("Hyprland did not realize the requested enabled state."));
            continue;
        }
        if (!record.enabled) continue;
        if (record.mirror != output.mirrorOf) {
            addError(errors, path,
                     QStringLiteral("display.realization-mirror-mismatch"),
                     QStringLiteral("Hyprland did not realize the requested mirror target."));
        }
        if (record.transform != output.transform) {
            addError(errors, path,
                     QStringLiteral("display.realization-transform-mismatch"),
                     QStringLiteral("Hyprland did not realize the requested transform."));
        }
        if (const auto *scale = std::get_if<double>(&record.scale);
            scale && std::abs(*scale - output.scale)
                > realizedTwoDecimalTolerance) {
            addError(errors, path,
                     QStringLiteral("display.realization-scale-mismatch"),
                     QStringLiteral("Hyprland did not realize the requested scale."));
        }
        const auto position = explicitPosition.match(record.position);
        if (position.hasMatch()) {
            bool xOk = false;
            bool yOk = false;
            const auto x = position.captured(1).toInt(&xOk);
            const auto y = position.captured(2).toInt(&yOk);
            if (!xOk || !yOk || x != output.x || y != output.y) {
                addError(errors, path,
                         QStringLiteral("display.realization-position-mismatch"),
                         QStringLiteral("Hyprland did not realize the requested position."));
            }
        }
        const auto mode = explicitMode.match(record.mode);
        if (mode.hasMatch()) {
            bool widthOk = false;
            bool heightOk = false;
            const auto width = mode.captured(1).toInt(&widthOk);
            const auto height = mode.captured(2).toInt(&heightOk);
            if (!widthOk || !heightOk || width != output.width
                || height != output.height) {
                addError(errors, path,
                         QStringLiteral("display.realization-mode-mismatch"),
                         QStringLiteral("Hyprland did not realize the requested resolution."));
            }
            if (!mode.captured(3).isEmpty()) {
                bool refreshOk = false;
                const auto refresh = mode.captured(3).toDouble(&refreshOk);
                if (!refreshOk
                    || std::abs(refresh - output.refreshRate)
                        > realizedRefreshToleranceHz) {
                    addError(errors, path,
                             QStringLiteral("display.realization-refresh-mismatch"),
                             QStringLiteral("Hyprland did not realize the requested refresh rate."));
                }
            }
        } else if (automaticModes.contains(record.mode)
                   && record.mirror.isEmpty()) {
            const auto realizedPositive = output.width > 0
                && output.height > 0 && output.refreshRate > 0.0;
            const auto advertised = std::ranges::any_of(
                output.modes, [&output](const ConnectedDisplayMode &candidate) {
                    return candidate.width == output.width
                        && candidate.height == output.height
                        && std::abs(candidate.refreshRate - output.refreshRate)
                            <= realizedRefreshToleranceHz;
                }
            );
            if (!realizedPositive || !advertised) {
                addError(errors, path,
                         QStringLiteral("display.realization-mode-mismatch"),
                         QStringLiteral("Hyprland did not realize an advertised positive output mode."));
            }
        } else if (!automaticModes.contains(record.mode)) {
            // Desired-state validation should make this unreachable. Keep
            // realization fail-closed if an unrecognized mode ever reaches
            // the proof boundary.
            addError(errors, path,
                     QStringLiteral("display.realization-mode-mismatch"),
                     QStringLiteral("The requested output mode cannot be proven."));
        }
        const auto tenBit = output.currentFormat == QStringLiteral("XRGB2101010")
            || output.currentFormat == QStringLiteral("XBGR2101010");
        const auto eightBit = output.currentFormat == QStringLiteral("XRGB8888")
            || output.currentFormat == QStringLiteral("XBGR8888");
        if ((record.bitdepth == 10 && !tenBit)
            || (record.bitdepth == 8 && !eightBit)) {
            addError(errors, path,
                     QStringLiteral("display.realization-bitdepth-mismatch"),
                     QStringLiteral("Hyprland did not realize the requested output bit depth."));
        }
        if (record.colorManagement != QStringLiteral("auto")
            && record.colorManagement != output.colorManagement) {
            addError(errors, path,
                     QStringLiteral("display.realization-color-mismatch"),
                     QStringLiteral("Hyprland did not realize the requested color preset."));
        }
        if (std::abs(record.sdrBrightness - output.sdrBrightness)
                > realizedTwoDecimalTolerance
            || std::abs(record.sdrSaturation - output.sdrSaturation)
                > realizedTwoDecimalTolerance
            || std::abs(record.sdrMinLuminance - output.sdrMinLuminance)
                > realizedTwoDecimalTolerance
            || record.sdrMaxLuminance != output.sdrMaxLuminance) {
            addError(errors, path,
                     QStringLiteral("display.realization-sdr-mismatch"),
                     QStringLiteral("Hyprland did not realize the requested SDR output values."));
        }
    }
    return errors;
}

ValidationErrors validateDisplayRealization(
    const DisplayProfile &profile,
    const ConnectedDisplayTopology &topology
)
{
    DesiredState connectedCandidate;
    for (const auto value : profile.outputs) {
        const auto object = value.toObject();
        MonitorConfiguration record;
        record.selector = object.value(QStringLiteral("selector")).toString();
        record.enabled = object.value(QStringLiteral("enabled")).toBool();
        record.mode = object.value(QStringLiteral("mode")).toString();
        record.position = object.value(QStringLiteral("position")).toString();
        const auto scale = object.value(QStringLiteral("scale"));
        record.scale = scale.isDouble()
            ? std::variant<double, QString>(scale.toDouble())
            : std::variant<double, QString>(scale.toString());
        record.transform = object.value(QStringLiteral("transform")).toInt();
        record.mirror = object.value(QStringLiteral("mirror")).toString();
        record.bitdepth = object.value(QStringLiteral("bitdepth")).toInt();
        record.colorManagement = object.value(QStringLiteral("cm")).toString();
        record.sdrBrightness = object.value(
            QStringLiteral("sdrBrightness")
        ).toDouble();
        record.sdrSaturation = object.value(
            QStringLiteral("sdrSaturation")
        ).toDouble();
        record.sdrMinLuminance = object.value(
            QStringLiteral("sdrMinLuminance")
        ).toDouble();
        record.sdrMaxLuminance = static_cast<qint64>(object.value(
            QStringLiteral("sdrMaxLuminance")
        ).toDouble());
        connectedCandidate.monitors.append(std::move(record));
    }
    return validateDisplayRealization(connectedCandidate, topology);
}

ValidationResult<DisplayCandidate> buildDisplayCandidate(
    const DesiredState &baseline,
    const DisplayProfile &profile,
    const ConnectedDisplayTopology &topology,
    const Catalog &catalog,
    const ActionCatalog &actionCatalog
)
{
    ValidationResult<DisplayCandidate> result;
    result.errors = validateDisplayProfileTopology(profile, topology);
    if (baseline.readOnly || baseline.opaqueFutureDocument) {
        addError(result.errors, QStringLiteral("$"),
                 QStringLiteral("display.read-only-baseline"),
                 QStringLiteral("A display preview requires an exact writable desired-state baseline."));
    }
    if (baseline.revision == std::numeric_limits<quint64>::max()) {
        addError(result.errors, QStringLiteral("$.revision"),
                 QStringLiteral("display.revision-exhausted"),
                 QStringLiteral("The display preview revision cannot advance."));
    }
    if (!result.errors.isEmpty()) return result;

    QSet<QString> connected;
    QStringList connectedOrder;
    for (const auto &output : topology.outputs) {
        connected.insert(output.selector);
        connectedOrder.append(output.selector);
    }
    QMap<QString, QJsonObject> proposed;
    for (const auto value : profile.outputs) {
        const auto object = value.toObject();
        proposed.insert(object.value(QStringLiteral("selector")).toString(), object);
    }

    QJsonArray merged;
    QSet<QString> usedIds;
    QMap<QString, QString> baselineIds;
    for (const auto &record : baseline.monitors) {
        if (connected.contains(record.selector)) {
            baselineIds.insert(record.selector, record.id);
            usedIds.insert(record.id);
            continue;
        }
        // Preserve every offline or description-selected record exactly by
        // reusing its canonical desired-state object below.
        usedIds.insert(record.id);
    }
    const auto baselineRoot = JsonSupport::parseStrictObject(
        QByteArrayView(serializeDesiredState(baseline)),
        maximumDesiredStateBytes, 64
    );
    if (!baselineRoot) {
        result.errors = baselineRoot.errors;
        return result;
    }
    const auto baselineObjects = baselineRoot.value->value(
        QStringLiteral("monitors")
    ).toArray();
    for (const auto value : baselineObjects) {
        const auto object = value.toObject();
        if (!connected.contains(object.value(QStringLiteral("selector")).toString())) {
            merged.append(object);
        }
    }
    for (const auto &selector : connectedOrder) {
        auto object = proposed.value(selector);
        if (baselineIds.contains(selector)) {
            object.insert(QStringLiteral("id"), baselineIds.value(selector));
        } else {
            object.insert(QStringLiteral("id"), stableMonitorId(selector, usedIds));
        }
        merged.append(object);
    }

    auto candidateRoot = *baselineRoot.value;
    candidateRoot.insert(QStringLiteral("revision"),
                         QString::number(baseline.revision + 1));
    candidateRoot.insert(QStringLiteral("monitors"), merged);
    auto candidateBytes = JsonSupport::canonicalJson(candidateRoot);
    candidateBytes.append('\n');
    const auto candidate = parseDesiredState(
        QByteArrayView(candidateBytes), catalog, actionCatalog
    );
    if (!candidate) {
        result.errors = candidate.errors;
        for (auto &error : result.errors) {
            error.path.replace(QStringLiteral("$.monitors"),
                               QStringLiteral("$.outputs"));
        }
        return result;
    }
    if (candidate.value->monitors == baseline.monitors) {
        addError(result.errors, QStringLiteral("$.outputs"),
                 QStringLiteral("display.no-change"),
                 QStringLiteral("The display profile does not change the active monitor configuration."));
        return result;
    }
    result.value = DisplayCandidate{
        .state = *candidate.value,
        .bytes = std::move(candidateBytes),
    };
    return result;
}

} // namespace HyprShelld::Hyprland
