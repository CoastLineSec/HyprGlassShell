#include "hyprland/monitor_profile.h"

#include "hyprland/json_support.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QtTest>

#include <algorithm>
#include <functional>
#include <limits>
#include <optional>

using namespace HyprShelld::Hyprland;

namespace {

[[nodiscard]] QByteArray readBytes(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) return {};
    return file.readAll();
}

[[nodiscard]] QJsonObject readObject(const QString &path)
{
    QJsonParseError error;
    const auto document = QJsonDocument::fromJson(readBytes(path), &error);
    if (error.error != QJsonParseError::NoError || !document.isObject()) {
        return {};
    }
    return document.object();
}

[[nodiscard]] QJsonObject readObjectFromBytes(const QByteArrayView bytes)
{
    QJsonParseError error;
    const auto document = QJsonDocument::fromJson(bytes.toByteArray(), &error);
    if (error.error != QJsonParseError::NoError || !document.isObject()) {
        return {};
    }
    return document.object();
}

[[nodiscard]] QByteArray canonical(const QJsonValue &value)
{
    auto bytes = JsonSupport::canonicalJson(value);
    bytes.append('\n');
    return bytes;
}

[[nodiscard]] bool hasCode(
    const ValidationErrors &errors,
    const QString &code
)
{
    return std::ranges::any_of(
        errors,
        [&code](const ValidationError &error) { return error.code == code; }
    );
}

[[nodiscard]] QString describeErrors(const ValidationErrors &errors)
{
    QStringList descriptions;
    for (const auto &error : errors) {
        descriptions.append(error.path + QLatin1Char(':') + error.code);
    }
    return descriptions.join(QStringLiteral(", "));
}

[[nodiscard]] QJsonObject desiredMonitor(
    const QString &selector,
    const QString &id,
    const QString &mode = QStringLiteral("preferred"),
    const bool enabled = true,
    const QString &mirror = {}
)
{
    return {
        {QStringLiteral("id"), id},
        {QStringLiteral("selector"), selector},
        {QStringLiteral("enabled"), enabled},
        {QStringLiteral("mode"), mode},
        {QStringLiteral("position"), QStringLiteral("auto-right")},
        {QStringLiteral("scale"), 1.0},
        {QStringLiteral("reserved"), QJsonArray{1, 2, 3, 4}},
        {QStringLiteral("transform"), 0},
        {QStringLiteral("mirror"), mirror},
        {QStringLiteral("bitdepth"), 8},
        {QStringLiteral("cm"), QStringLiteral("auto")},
        {QStringLiteral("sdrEotf"), QStringLiteral("default")},
        {QStringLiteral("sdrBrightness"), 1.0},
        {QStringLiteral("sdrSaturation"), 1.0},
        {QStringLiteral("vrr"), -1},
        {QStringLiteral("icc"), QString()},
        {QStringLiteral("supportsWideColor"), 0},
        {QStringLiteral("supportsHdr"), 0},
        {QStringLiteral("sdrMinLuminance"), 0.2},
        {QStringLiteral("sdrMaxLuminance"), 80},
        {QStringLiteral("minLuminance"), -1.0},
        {QStringLiteral("maxLuminance"), -1},
        {QStringLiteral("maxAvgLuminance"), -1},
    };
}

[[nodiscard]] QJsonObject upstreamMonitor(
    const qint64 id,
    const QString &name,
    const bool disabled = false,
    const QString &mirrorOf = QStringLiteral("none")
)
{
    return {
        {QStringLiteral("id"), id},
        {QStringLiteral("name"), name},
        {QStringLiteral("description"),
         QStringLiteral("Acme Panel %1").arg(name)},
        {QStringLiteral("make"), QStringLiteral("Acme")},
        {QStringLiteral("model"), QStringLiteral("Panel")},
        {QStringLiteral("serial"), QStringLiteral("serial-%1").arg(name)},
        {QStringLiteral("width"), disabled ? 0 : 2560},
        {QStringLiteral("height"), disabled ? 0 : 1440},
        {QStringLiteral("physicalWidth"), 600},
        {QStringLiteral("physicalHeight"), 340},
        {QStringLiteral("refreshRate"), disabled ? 0.0 : 143.99870},
        {QStringLiteral("x"), name == QStringLiteral("DP-1") ? 0 : 2048},
        {QStringLiteral("y"), 0},
        {QStringLiteral("activeWorkspace"), QJsonObject{
            {QStringLiteral("id"), disabled ? -1 : 1},
            {QStringLiteral("name"), disabled ? QString() : QStringLiteral("1")},
        }},
        {QStringLiteral("specialWorkspace"), QJsonObject{
            {QStringLiteral("id"), 0},
            {QStringLiteral("name"), QString()},
        }},
        // HyprCtl's wire order is left, top, right, bottom.
        {QStringLiteral("reserved"), QJsonArray{4, 1, 2, 3}},
        {QStringLiteral("scale"), 1.25},
        {QStringLiteral("transform"), 0},
        {QStringLiteral("focused"), name == QStringLiteral("DP-1")},
        {QStringLiteral("dpmsStatus"), !disabled},
        {QStringLiteral("vrr"), false},
        {QStringLiteral("solitary"), QStringLiteral("0")},
        {QStringLiteral("solitaryBlockedBy"), QJsonValue::Null},
        {QStringLiteral("activelyTearing"), false},
        {QStringLiteral("tearingBlockedBy"), QJsonValue::Null},
        {QStringLiteral("directScanoutTo"), QStringLiteral("0")},
        {QStringLiteral("directScanoutBlockedBy"), QJsonValue::Null},
        {QStringLiteral("disabled"), disabled},
        {QStringLiteral("currentFormat"), QStringLiteral("XRGB8888")},
        {QStringLiteral("mirrorOf"), mirrorOf},
        {QStringLiteral("availableModes"), QJsonArray{
            QStringLiteral("2560x1440@144.00Hz"),
            QStringLiteral("1920x1080@60.00Hz"),
        }},
        {QStringLiteral("colorManagementPreset"), QStringLiteral("srgb")},
        {QStringLiteral("sdrBrightness"), 1.0},
        {QStringLiteral("sdrSaturation"), 1.0},
        {QStringLiteral("sdrMinLuminance"), 0.2},
        {QStringLiteral("sdrMaxLuminance"), 80},
        {QStringLiteral("hardwareCursorsInUse"), true},
    };
}

[[nodiscard]] QByteArray upstreamReply(const QJsonArray &outputs)
{
    return QJsonDocument(outputs).toJson(QJsonDocument::Compact);
}

[[nodiscard]] DisplayProfile profile(
    const ConnectedDisplayTopology &topology,
    const QJsonArray &outputs
)
{
    return {
        .topologyDigest = topology.topologyDigest,
        .outputs = outputs,
    };
}

} // namespace

class HyprlandMonitorProfileTest final : public QObject
{
    Q_OBJECT

private:
    Catalog catalog;
    ActionCatalog actionCatalog;
    QJsonObject defaultsObject;
    DesiredState defaults;

    [[nodiscard]] ValidationResult<DesiredState> desiredWithMonitors(
        const QJsonArray &monitors
    ) const
    {
        auto object = defaultsObject;
        object.insert(QStringLiteral("monitors"), monitors);
        return parseDesiredState(canonical(object), catalog, actionCatalog);
    }

    [[nodiscard]] ConnectedDisplayTopology twoOutputTopology() const
    {
        const auto parsed = parseConnectedDisplayTopology(upstreamReply({
            upstreamMonitor(9, QStringLiteral("DP-2")),
            upstreamMonitor(7, QStringLiteral("DP-1")),
        }));
        Q_ASSERT(parsed);
        return *parsed.value;
    }

private slots:
    void initTestCase()
    {
        const auto parsedCatalog = parseCatalog(
            readBytes(QStringLiteral(HYPRSHELLD_HYPRLAND_CATALOG_FILE))
        );
        QVERIFY2(parsedCatalog, qPrintable(describeErrors(parsedCatalog.errors)));
        catalog = *parsedCatalog.value;

        const auto parsedActions = parseActionCatalog(
            readBytes(QStringLiteral(HYPRSHELLD_HYPRLAND_ACTION_CATALOG_FILE)),
            readBytes(QStringLiteral(HYPRSHELLD_HYPRLAND_CONFIG_SCHEMA_FILE))
        );
        QVERIFY2(parsedActions, qPrintable(describeErrors(parsedActions.errors)));
        actionCatalog = *parsedActions.value;

        defaultsObject = readObject(
            QStringLiteral(HYPRSHELLD_HYPRLAND_DEFAULTS_FILE)
        );
        QVERIFY(!defaultsObject.isEmpty());
        const auto parsedDefaults = parseDesiredState(
            canonical(defaultsObject), catalog, actionCatalog
        );
        QVERIFY2(parsedDefaults, qPrintable(describeErrors(parsedDefaults.errors)));
        defaults = *parsedDefaults.value;
    }

    void profileTransportIsCanonicalAndClosed()
    {
        const DisplayProfile input{
            .topologyDigest = QString(64, QLatin1Char('a')),
            .outputs = QJsonArray{
                desiredMonitor(
                    QStringLiteral("DP-1"), QStringLiteral("caller-id")
                ),
            },
        };
        const auto bytes = serializeDisplayProfile(input);
        QVERIFY(bytes.endsWith('\n'));
        const auto roundTrip = parseDisplayProfile(bytes);
        QVERIFY2(roundTrip, qPrintable(describeErrors(roundTrip.errors)));
        QCOMPARE(*roundTrip.value, input);

        auto missingNewline = bytes;
        missingNewline.chop(1);
        auto parsed = parseDisplayProfile(missingNewline);
        QVERIFY(!parsed);
        QVERIFY(hasCode(parsed.errors, QStringLiteral("display.noncanonical-profile")));

        auto root = readObjectFromBytes(bytes);
        root.insert(QStringLiteral("unexpected"), true);
        parsed = parseDisplayProfile(canonical(root));
        QVERIFY(!parsed);
        QVERIFY(hasCode(parsed.errors, QStringLiteral("display.invalid-profile-shape")));

        root.remove(QStringLiteral("unexpected"));
        root.insert(QStringLiteral("topologyDigest"), QString(64, QLatin1Char('A')));
        parsed = parseDisplayProfile(canonical(root));
        QVERIFY(!parsed);
        QVERIFY(hasCode(parsed.errors, QStringLiteral("display.invalid-topology-digest")));

        root.insert(QStringLiteral("topologyDigest"), input.topologyDigest);
        root.insert(QStringLiteral("outputs"), QJsonArray{1});
        parsed = parseDisplayProfile(canonical(root));
        QVERIFY(!parsed);
        QVERIFY(hasCode(parsed.errors, QStringLiteral("display.output-object-required")));
    }

    void parsesPinnedWireAndNormalizesModesAndMirrors()
    {
        const auto dp2 = upstreamMonitor(
            9, QStringLiteral("DP-2"), false, QStringLiteral("7")
        );
        const auto parsed = parseConnectedDisplayTopology(upstreamReply({
            dp2,
            upstreamMonitor(7, QStringLiteral("DP-1")),
        }));
        QVERIFY2(parsed, qPrintable(describeErrors(parsed.errors)));
        QCOMPARE(parsed.value->outputs.size(), 2);
        QCOMPARE(parsed.value->outputs.at(0).selector, QStringLiteral("DP-1"));
        QCOMPARE(parsed.value->outputs.at(1).selector, QStringLiteral("DP-2"));
        QCOMPARE(parsed.value->outputs.at(1).mirrorOf, QStringLiteral("DP-1"));
        QCOMPARE(parsed.value->outputs.at(0).refreshRate, 143.99870);
        QCOMPARE(parsed.value->outputs.at(0).scale, 1.25);
        QCOMPARE(parsed.value->outputs.at(0).modes.size(), 2);
        QCOMPARE(
            parsed.value->outputs.at(0).modes.at(0).managedMode,
            QStringLiteral("1920x1080@60")
        );
        QCOMPARE(
            parsed.value->outputs.at(0).modes.at(1).managedMode,
            QStringLiteral("2560x1440@144")
        );
        QCOMPARE(parsed.value->topologyDigest.size(), 64);
        QVERIFY(parsed.value->document.endsWith('\n'));

        const auto publicRoot = readObjectFromBytes(parsed.value->document);
        QCOMPARE(publicRoot.value(QStringLiteral("formatVersion")).toInt(), 1);
        QCOMPARE(
            publicRoot.value(QStringLiteral("topologyDigest")).toString(),
            parsed.value->topologyDigest
        );
        const auto publicOutput = publicRoot.value(
            QStringLiteral("outputs")
        ).toArray().at(0).toObject();
        const QSet<QString> expectedPublicFields{
            QStringLiteral("selector"), QStringLiteral("description"),
            QStringLiteral("make"), QStringLiteral("model"),
            QStringLiteral("serial"), QStringLiteral("enabled"),
            QStringLiteral("width"), QStringLiteral("height"),
            QStringLiteral("physicalWidthMm"),
            QStringLiteral("physicalHeightMm"),
            QStringLiteral("refreshRate"), QStringLiteral("x"),
            QStringLiteral("y"), QStringLiteral("reserved"),
            QStringLiteral("scale"),
            QStringLiteral("transform"), QStringLiteral("focused"),
            QStringLiteral("dpms"), QStringLiteral("vrrActive"),
            QStringLiteral("mirrorOf"), QStringLiteral("modes"),
            QStringLiteral("colorManagement"),
            QStringLiteral("currentFormat"),
            QStringLiteral("sdrBrightness"),
            QStringLiteral("sdrSaturation"),
            QStringLiteral("sdrMinLuminance"),
            QStringLiteral("sdrMaxLuminance"),
        };
        const auto publicKeys = publicOutput.keys();
        QCOMPARE(
            QSet<QString>(publicKeys.cbegin(), publicKeys.cend()),
            expectedPublicFields
        );
        QCOMPARE(
            publicOutput.value(QStringLiteral("reserved")).toArray(),
            QJsonArray({1, 2, 3, 4})
        );

        const auto reversed = parseConnectedDisplayTopology(upstreamReply({
            upstreamMonitor(7, QStringLiteral("DP-1")), dp2,
        }));
        QVERIFY(reversed);
        QCOMPARE(reversed.value->topologyDigest, parsed.value->topologyDigest);
        QCOMPARE(reversed.value->document, parsed.value->document);
    }

    void requiresEveryConsumedPinnedHyprCtlField_data()
    {
        QTest::addColumn<QString>("field");
        for (const auto &field : {
                 QStringLiteral("id"),
                 QStringLiteral("name"),
                 QStringLiteral("description"),
                 QStringLiteral("make"),
                 QStringLiteral("model"),
                 QStringLiteral("serial"),
                 QStringLiteral("width"),
                 QStringLiteral("height"),
                 QStringLiteral("physicalWidth"),
                 QStringLiteral("physicalHeight"),
                 QStringLiteral("refreshRate"),
                 QStringLiteral("x"),
                 QStringLiteral("y"),
                 QStringLiteral("reserved"),
                 QStringLiteral("scale"),
                 QStringLiteral("transform"),
                 QStringLiteral("focused"),
                 QStringLiteral("dpmsStatus"),
                 QStringLiteral("vrr"),
                 QStringLiteral("disabled"),
                 QStringLiteral("currentFormat"),
                 QStringLiteral("mirrorOf"),
                 QStringLiteral("availableModes"),
                 QStringLiteral("colorManagementPreset"),
                 QStringLiteral("sdrBrightness"),
                 QStringLiteral("sdrSaturation"),
                 QStringLiteral("sdrMinLuminance"),
                 QStringLiteral("sdrMaxLuminance"),
             }) {
            QTest::newRow(qPrintable(field)) << field;
        }
    }

    void requiresEveryConsumedPinnedHyprCtlField()
    {
        QFETCH(QString, field);
        auto monitor = upstreamMonitor(7, QStringLiteral("DP-1"));
        monitor.remove(field);
        const auto parsed = parseConnectedDisplayTopology(
            upstreamReply({monitor})
        );
        QVERIFY2(!parsed, qPrintable(field));
    }

    void rejectsMalformedReservedWireTuple_data()
    {
        QTest::addColumn<QJsonValue>("reserved");
        QTest::newRow("not-array") << QJsonValue(QStringLiteral("1,2,3,4"));
        QTest::newRow("too-short") << QJsonValue(QJsonArray{1, 2, 3});
        QTest::newRow("too-long") << QJsonValue(QJsonArray{1, 2, 3, 4, 5});
        QTest::newRow("non-integral") << QJsonValue(QJsonArray{1, 2, 3.5, 4});
        QTest::newRow("non-numeric")
            << QJsonValue(QJsonArray{1, 2, QStringLiteral("3"), 4});
    }

    void rejectsMalformedReservedWireTuple()
    {
        QFETCH(QJsonValue, reserved);
        auto monitor = upstreamMonitor(7, QStringLiteral("DP-1"));
        monitor.insert(QStringLiteral("reserved"), reserved);
        const auto parsed = parseConnectedDisplayTopology(
            upstreamReply({monitor})
        );
        QVERIFY(!parsed);
        QVERIFY(hasCode(
            parsed.errors, QStringLiteral("display.invalid-topology-fields")
        ));
    }

    void fingerprintIgnoresPreviewEffectsButDetectsConnectionDrift()
    {
        const auto baseline = parseConnectedDisplayTopology(upstreamReply({
            upstreamMonitor(7, QStringLiteral("DP-1")),
            upstreamMonitor(9, QStringLiteral("DP-2")),
        }));
        QVERIFY(baseline);

        auto changed = upstreamMonitor(
            7, QStringLiteral("DP-1"), true, QStringLiteral("9")
        );
        changed.insert(QStringLiteral("width"), 3440);
        changed.insert(QStringLiteral("height"), 1440);
        changed.insert(QStringLiteral("refreshRate"), 74.97321);
        changed.insert(QStringLiteral("x"), -100);
        changed.insert(QStringLiteral("y"), 50);
        changed.insert(QStringLiteral("scale"), 1.5);
        changed.insert(QStringLiteral("transform"), 3);
        changed.insert(QStringLiteral("focused"), false);
        changed.insert(QStringLiteral("dpmsStatus"), false);
        changed.insert(QStringLiteral("vrr"), true);
        changed.insert(
            QStringLiteral("colorManagementPreset"), QStringLiteral("hdr")
        );
        const auto dynamic = parseConnectedDisplayTopology(upstreamReply({
            changed,
            upstreamMonitor(9, QStringLiteral("DP-2")),
        }));
        QVERIFY(dynamic);
        QCOMPARE(dynamic.value->topologyDigest, baseline.value->topologyDigest);
        QVERIFY(dynamic.value->document != baseline.value->document);

        auto identityChanged = upstreamMonitor(7, QStringLiteral("DP-1"));
        identityChanged.insert(
            QStringLiteral("serial"), QStringLiteral("replacement-panel")
        );
        const auto replacement = parseConnectedDisplayTopology(upstreamReply({
            identityChanged,
            upstreamMonitor(9, QStringLiteral("DP-2")),
        }));
        QVERIFY(replacement);
        QVERIFY(replacement.value->topologyDigest
                != baseline.value->topologyDigest);

        auto modesChanged = upstreamMonitor(7, QStringLiteral("DP-1"));
        modesChanged.insert(
            QStringLiteral("availableModes"),
            QJsonArray{QStringLiteral("1920x1080@60.00Hz")}
        );
        const auto modeDrift = parseConnectedDisplayTopology(upstreamReply({
            modesChanged,
            upstreamMonitor(9, QStringLiteral("DP-2")),
        }));
        QVERIFY(modeDrift);
        QVERIFY(modeDrift.value->topologyDigest
                != baseline.value->topologyDigest);

        const auto unplugged = parseConnectedDisplayTopology(upstreamReply({
            upstreamMonitor(7, QStringLiteral("DP-1")),
        }));
        QVERIFY(unplugged);
        QVERIFY(unplugged.value->topologyDigest
                != baseline.value->topologyDigest);
    }

    void rejectsMalformedTopology_data()
    {
        QTest::addColumn<QByteArray>("reply");
        QTest::addColumn<QString>("code");

        QTest::newRow("not-array")
            << QByteArrayLiteral("{}")
            << QStringLiteral("display.invalid-topology");

        auto duplicateName = upstreamMonitor(9, QStringLiteral("DP-1"));
        QTest::newRow("duplicate-name")
            << upstreamReply({
                upstreamMonitor(7, QStringLiteral("DP-1")), duplicateName,
            })
            << QStringLiteral("display.duplicate-topology-output");

        auto duplicateId = upstreamMonitor(7, QStringLiteral("DP-2"));
        QTest::newRow("duplicate-id")
            << upstreamReply({
                upstreamMonitor(7, QStringLiteral("DP-1")), duplicateId,
            })
            << QStringLiteral("display.duplicate-topology-output");

        auto invalidConnector = upstreamMonitor(7, QStringLiteral("DP-1"));
        invalidConnector.insert(
            QStringLiteral("name"), QStringLiteral("desc:unsafe")
        );
        QTest::newRow("selector-not-connector")
            << upstreamReply({invalidConnector})
            << QStringLiteral("display.invalid-topology-identity");

        auto missingScale = upstreamMonitor(7, QStringLiteral("DP-1"));
        missingScale.remove(QStringLiteral("scale"));
        QTest::newRow("missing-required-field")
            << upstreamReply({missingScale})
            << QStringLiteral("display.invalid-topology-fields");

        auto unresolvedMirror = upstreamMonitor(
            7, QStringLiteral("DP-1"), false, QStringLiteral("99")
        );
        QTest::newRow("unresolved-mirror")
            << upstreamReply({unresolvedMirror})
            << QStringLiteral("display.invalid-topology-mirror");

        auto duplicateMode = upstreamMonitor(7, QStringLiteral("DP-1"));
        duplicateMode.insert(
            QStringLiteral("availableModes"),
            QJsonArray{
                QStringLiteral("1920x1080@60.00Hz"),
                QStringLiteral("1920x1080@60.00Hz"),
            }
        );
        QTest::newRow("duplicate-mode")
            << upstreamReply({duplicateMode})
            << QStringLiteral("display.duplicate-topology-mode");

        for (const auto &mode : {
                 QStringLiteral("1920x1080@60Hz"),
                 QStringLiteral("1920x1080@60.0Hz"),
                 QStringLiteral("1920x1080@60.000Hz"),
                 QStringLiteral("1920x1080@60.00hz"),
                 QStringLiteral("1920x1080@60.00"),
             }) {
            auto malformedMode = upstreamMonitor(7, QStringLiteral("DP-1"));
            malformedMode.insert(
                QStringLiteral("availableModes"), QJsonArray{mode}
            );
            QTest::newRow(qPrintable(QStringLiteral("mode-%1").arg(mode)))
                << upstreamReply({malformedMode})
                << QStringLiteral("display.invalid-topology-mode");
        }

        for (const auto &mirror : {
                 QStringLiteral("+7"), QStringLiteral("07"),
                 QStringLiteral(" 7"), QStringLiteral("7 "),
             }) {
            auto noncanonicalMirror = upstreamMonitor(
                9, QStringLiteral("DP-2"), false, mirror
            );
            QTest::newRow(
                qPrintable(QStringLiteral("mirror-%1").arg(mirror))
            ) << upstreamReply({
                upstreamMonitor(7, QStringLiteral("DP-1")),
                noncanonicalMirror,
            }) << QStringLiteral("display.invalid-topology-mirror");
        }
    }

    void rejectsMalformedTopology()
    {
        QFETCH(QByteArray, reply);
        QFETCH(QString, code);
        const auto parsed = parseConnectedDisplayTopology(reply);
        QVERIFY(parsed.value == std::nullopt);
        QVERIFY2(hasCode(parsed.errors, code),
                 qPrintable(describeErrors(parsed.errors)));
    }

    void validatesExactConnectedSetAndSafeMirrorGraph()
    {
        const auto topology = twoOutputTopology();
        const auto dp1 = desiredMonitor(
            QStringLiteral("DP-1"), QStringLiteral("one")
        );
        const auto dp2 = desiredMonitor(
            QStringLiteral("DP-2"), QStringLiteral("two")
        );
        QVERIFY(validateDisplayProfileTopology(
            profile(topology, {dp2, dp1}), topology
        ).isEmpty());

        auto errors = validateDisplayProfileTopology(
            profile(topology, {dp1}), topology
        );
        QVERIFY(hasCode(errors, QStringLiteral("display.connector-set-changed")));

        errors = validateDisplayProfileTopology(
            profile(topology, {
                dp1,
                desiredMonitor(
                    QStringLiteral("HDMI-A-1"), QStringLiteral("extra")
                ),
            }), topology
        );
        QVERIFY(hasCode(errors, QStringLiteral("display.connector-set-changed")));

        errors = validateDisplayProfileTopology(
            profile(topology, {dp1, dp1}), topology
        );
        QVERIFY(hasCode(errors, QStringLiteral("display.duplicate-connector")));

        errors = validateDisplayProfileTopology(
            profile(topology, {
                desiredMonitor(
                    QStringLiteral("DP-1"), QStringLiteral("one"),
                    QStringLiteral("preferred"), false
                ),
                desiredMonitor(
                    QStringLiteral("DP-2"), QStringLiteral("two"),
                    QStringLiteral("preferred"), false
                ),
            }), topology
        );
        QVERIFY(hasCode(errors, QStringLiteral("display.no-enabled-output")));

        auto stale = profile(topology, {dp1, dp2});
        stale.topologyDigest = QString(64, QLatin1Char('f'));
        errors = validateDisplayProfileTopology(stale, topology);
        QVERIFY(hasCode(errors, QStringLiteral("display.stale-topology")));

        // At least one enabled boolean is insufficient: a usable scanout
        // cannot depend on a disabled, self-mirroring, or cyclic target.
        errors = validateDisplayProfileTopology(
            profile(topology, {
                desiredMonitor(
                    QStringLiteral("DP-1"), QStringLiteral("one"),
                    QStringLiteral("preferred"), true,
                    QStringLiteral("DP-2")
                ),
                desiredMonitor(
                    QStringLiteral("DP-2"), QStringLiteral("two"),
                    QStringLiteral("preferred"), false
                ),
            }), topology
        );
        QVERIFY(!errors.isEmpty());

        errors = validateDisplayProfileTopology(
            profile(topology, {
                desiredMonitor(
                    QStringLiteral("DP-1"), QStringLiteral("one"),
                    QStringLiteral("preferred"), true,
                    QStringLiteral("DP-1")
                ),
                dp2,
            }), topology
        );
        QVERIFY(!errors.isEmpty());

        errors = validateDisplayProfileTopology(
            profile(topology, {
                desiredMonitor(
                    QStringLiteral("DP-1"), QStringLiteral("one"),
                    QStringLiteral("preferred"), true,
                    QStringLiteral("DP-2")
                ),
                desiredMonitor(
                    QStringLiteral("DP-2"), QStringLiteral("two"),
                    QStringLiteral("preferred"), true,
                    QStringLiteral("DP-1")
                ),
            }), topology
        );
        QVERIFY(!errors.isEmpty());
    }

    void mirrorTransitionRequiresTargetPositionAndRealizesDeterministically()
    {
        auto sourceWire = upstreamMonitor(
            9, QStringLiteral("DP-2"), false, QStringLiteral("7")
        );
        sourceWire.insert(QStringLiteral("x"), 0);
        const auto parsedTopology = parseConnectedDisplayTopology(
            upstreamReply({
                upstreamMonitor(7, QStringLiteral("DP-1")), sourceWire,
            })
        );
        QVERIFY2(parsedTopology,
                 qPrintable(describeErrors(parsedTopology.errors)));
        const auto &topology = *parsedTopology.value;

        auto target = desiredMonitor(
            QStringLiteral("DP-1"), QStringLiteral("target")
        );
        target.insert(QStringLiteral("position"), QStringLiteral("0x0"));
        target.insert(QStringLiteral("scale"), 1.25);
        auto source = desiredMonitor(
            QStringLiteral("DP-2"), QStringLiteral("source"),
            QStringLiteral("preferred"), true, QStringLiteral("DP-1")
        );
        source.insert(QStringLiteral("position"), QStringLiteral("2048x0"));
        source.insert(QStringLiteral("scale"), 1.25);

        auto errors = validateDisplayProfileTopology(
            profile(topology, {target, source}), topology
        );
        QVERIFY2(
            hasCode(errors, QStringLiteral("display.mirror-position-mismatch")),
            qPrintable(describeErrors(errors))
        );

        source.insert(QStringLiteral("position"), QStringLiteral("0x0"));
        const auto exactProfile = profile(topology, {target, source});
        errors = validateDisplayProfileTopology(exactProfile, topology);
        QVERIFY2(errors.isEmpty(), qPrintable(describeErrors(errors)));
        const auto candidate = buildDisplayCandidate(
            defaults, exactProfile, topology, catalog, actionCatalog
        );
        QVERIFY2(candidate, qPrintable(describeErrors(candidate.errors)));
        QCOMPARE(candidate.value->state.monitors.size(), 2);
        QCOMPARE(candidate.value->state.monitors.at(1).mirror,
                 QStringLiteral("DP-1"));
        QCOMPARE(candidate.value->state.monitors.at(1).position,
                 QStringLiteral("0x0"));
        errors = validateDisplayRealization(exactProfile, topology);
        QVERIFY2(errors.isEmpty(), qPrintable(describeErrors(errors)));
    }

    void mergeKeepsOfflineFirstAndAppendsConnectedDeterministically()
    {
        auto desc = desiredMonitor(
            QStringLiteral("desc:Acme Panel"), QStringLiteral("description")
        );
        desc.insert(QStringLiteral("sdrBrightness"), 1.75);
        auto offline = desiredMonitor(
            QStringLiteral("HDMI-A-9"), QStringLiteral("offline")
        );
        offline.insert(QStringLiteral("transform"), 6);
        auto oldDp2 = desiredMonitor(
            QStringLiteral("DP-2"), QStringLiteral("keep-dp2")
        );
        auto winningDp1 = desiredMonitor(
            QStringLiteral("DP-1"), QStringLiteral("winning-dp1")
        );
        const auto baseline = desiredWithMonitors({
            oldDp2, desc, offline, winningDp1,
        });
        QVERIFY2(baseline, qPrintable(describeErrors(baseline.errors)));

        const auto topology = twoOutputTopology();
        auto proposedDp2 = desiredMonitor(
            QStringLiteral("DP-2"), QStringLiteral("caller-two"),
            QStringLiteral("1920x1080@60")
        );
        proposedDp2.insert(QStringLiteral("bitdepth"), 10);
        auto proposedDp1 = desiredMonitor(
            QStringLiteral("DP-1"), QStringLiteral("caller-one"),
            QStringLiteral("3440x1440@74.973")
        );
        proposedDp1.insert(QStringLiteral("cm"), QStringLiteral("dp3"));
        const auto candidate = buildDisplayCandidate(
            *baseline.value,
            profile(topology, {proposedDp2, proposedDp1}),
            topology,
            catalog,
            actionCatalog
        );
        QVERIFY2(candidate, qPrintable(describeErrors(candidate.errors)));
        QCOMPARE(candidate.value->state.revision, baseline.value->revision + 1);
        QCOMPARE(candidate.value->state.monitors.size(), 4);
        QCOMPARE(
            candidate.value->state.monitors.at(0).selector,
            QStringLiteral("desc:Acme Panel")
        );
        QCOMPARE(
            candidate.value->state.monitors.at(1).selector,
            QStringLiteral("HDMI-A-9")
        );
        QCOMPARE(
            candidate.value->state.monitors.at(2).selector,
            QStringLiteral("DP-1")
        );
        QCOMPARE(
            candidate.value->state.monitors.at(3).selector,
            QStringLiteral("DP-2")
        );
        QCOMPARE(
            candidate.value->state.monitors.at(2).id,
            QStringLiteral("winning-dp1")
        );
        QCOMPARE(
            candidate.value->state.monitors.at(3).id,
            QStringLiteral("keep-dp2")
        );
        QCOMPARE(
            candidate.value->state.monitors.at(2).mode,
            QStringLiteral("3440x1440@74.973")
        );
        QCOMPARE(
            candidate.value->state.monitors.at(2).colorManagement,
            QStringLiteral("dp3")
        );
        QCOMPARE(candidate.value->state.monitors.at(3).bitdepth, 10);

        const auto baselineRoot = readObjectFromBytes(
            serializeDesiredState(*baseline.value)
        );
        const auto candidateRoot = readObjectFromBytes(candidate.value->bytes);
        const auto beforeMonitors = baselineRoot.value(
            QStringLiteral("monitors")
        ).toArray();
        const auto afterMonitors = candidateRoot.value(
            QStringLiteral("monitors")
        ).toArray();
        QCOMPARE(afterMonitors.at(0), beforeMonitors.at(1));
        QCOMPARE(afterMonitors.at(1), beforeMonitors.at(2));

        auto expectedRoot = baselineRoot;
        expectedRoot.insert(
            QStringLiteral("revision"),
            QString::number(baseline.value->revision + 1)
        );
        expectedRoot.insert(QStringLiteral("monitors"), afterMonitors);
        QCOMPARE(candidateRoot, expectedRoot);
    }

    void generatesBoundedStableIdForMaximumConnector()
    {
        const auto selector = QStringLiteral("D")
            + QString(127, QLatin1Char('p'));
        QCOMPARE(selector.size(), 128);
        ConnectedDisplayTopology topology{
            .outputs = QVector<ConnectedDisplay>{ConnectedDisplay{
                .upstreamId = 1,
                .selector = selector,
                .enabled = true,
            }},
            .topologyDigest = QString(64, QLatin1Char('a')),
        };
        const auto candidate = buildDisplayCandidate(
            defaults,
            profile(topology, {
                desiredMonitor(selector, QStringLiteral("caller")),
            }),
            topology,
            catalog,
            actionCatalog
        );
        QVERIFY2(candidate, qPrintable(describeErrors(candidate.errors)));
        QCOMPARE(candidate.value->state.monitors.size(), 1);
        const auto id = candidate.value->state.monitors.constFirst().id;
        QVERIFY(!id.isEmpty());
        QVERIFY(id.size() <= 128);

        const auto repeated = buildDisplayCandidate(
            defaults,
            profile(topology, {
                desiredMonitor(selector, QStringLiteral("different-caller")),
            }),
            topology,
            catalog,
            actionCatalog
        );
        QVERIFY(repeated);
        QCOMPARE(repeated.value->state.monitors.constFirst().id, id);
    }

    void displayCandidateRejectsRevisionExhaustionBeforeMutation()
    {
        auto exhausted = defaults;
        exhausted.revision = std::numeric_limits<quint64>::max();
        const auto parsedTopology = parseConnectedDisplayTopology(
            upstreamReply({upstreamMonitor(7, QStringLiteral("DP-1"))})
        );
        QVERIFY(parsedTopology);
        const auto candidate = buildDisplayCandidate(
            exhausted,
            profile(*parsedTopology.value, {
                desiredMonitor(
                    QStringLiteral("DP-1"), QStringLiteral("caller")
                ),
            }),
            *parsedTopology.value,
            catalog,
            actionCatalog
        );
        QVERIFY(!candidate);
        QVERIFY2(
            hasCode(candidate.errors,
                    QStringLiteral("display.revision-exhausted")),
            qPrintable(describeErrors(candidate.errors))
        );
    }

    void explicitModeNeedNotAppearInAdvertisedModes()
    {
        const auto topology = parseConnectedDisplayTopology(upstreamReply({
            upstreamMonitor(7, QStringLiteral("DP-1")),
        }));
        QVERIFY(topology);
        const auto candidate = buildDisplayCandidate(
            defaults,
            profile(*topology.value, {
                desiredMonitor(
                    QStringLiteral("DP-1"), QStringLiteral("caller"),
                    QStringLiteral("3440x1440@74.973")
                ),
            }),
            *topology.value,
            catalog,
            actionCatalog
        );
        QVERIFY2(candidate, qPrintable(describeErrors(candidate.errors)));
        QCOMPARE(
            candidate.value->state.monitors.constFirst().mode,
            QStringLiteral("3440x1440@74.973")
        );
    }

    void realizationRequiresExactSafetyRelevantState()
    {
        auto desired = desiredMonitor(
            QStringLiteral("DP-1"), QStringLiteral("display-one"),
            QStringLiteral("2560x1440@144")
        );
        desired.insert(QStringLiteral("position"), QStringLiteral("0x0"));
        desired.insert(QStringLiteral("scale"), 1.25);
        desired.insert(QStringLiteral("cm"), QStringLiteral("srgb"));
        const auto candidate = desiredWithMonitors({desired});
        QVERIFY2(candidate, qPrintable(describeErrors(candidate.errors)));

        const auto parsedTopology = parseConnectedDisplayTopology(
            upstreamReply({upstreamMonitor(7, QStringLiteral("DP-1"))})
        );
        QVERIFY(parsedTopology);
        const auto topology = *parsedTopology.value;
        QVERIFY(validateDisplayRealization(*candidate.value, topology).isEmpty());

        const auto expectMismatch = [this, &candidate, &topology](
            const QString &code,
            const std::function<void(ConnectedDisplay &)> &mutate
        ) {
            auto changed = topology;
            mutate(changed.outputs.first());
            const auto errors = validateDisplayRealization(
                *candidate.value, changed
            );
            const auto detail = code + QStringLiteral(": ")
                + describeErrors(errors);
            QVERIFY2(hasCode(errors, code), qPrintable(detail));
        };
        expectMismatch(
            QStringLiteral("display.realization-enabled-mismatch"),
            [](ConnectedDisplay &output) { output.enabled = false; }
        );
        expectMismatch(
            QStringLiteral("display.realization-mirror-mismatch"),
            [](ConnectedDisplay &output) {
                output.mirrorOf = QStringLiteral("DP-2");
            }
        );
        expectMismatch(
            QStringLiteral("display.realization-transform-mismatch"),
            [](ConnectedDisplay &output) { output.transform = 1; }
        );
        expectMismatch(
            QStringLiteral("display.realization-scale-mismatch"),
            [](ConnectedDisplay &output) { output.scale += 0.02; }
        );
        expectMismatch(
            QStringLiteral("display.realization-position-mismatch"),
            [](ConnectedDisplay &output) { output.x = 1; }
        );
        expectMismatch(
            QStringLiteral("display.realization-mode-mismatch"),
            [](ConnectedDisplay &output) { output.width = 1920; }
        );
        expectMismatch(
            QStringLiteral("display.realization-refresh-mismatch"),
            [](ConnectedDisplay &output) { output.refreshRate = 143.8; }
        );
        expectMismatch(
            QStringLiteral("display.realization-bitdepth-mismatch"),
            [](ConnectedDisplay &output) {
                output.currentFormat = QStringLiteral("XRGB2101010");
            }
        );
        expectMismatch(
            QStringLiteral("display.realization-color-mismatch"),
            [](ConnectedDisplay &output) {
                output.colorManagement = QStringLiteral("hdr");
            }
        );
        expectMismatch(
            QStringLiteral("display.realization-sdr-mismatch"),
            [](ConnectedDisplay &output) { output.sdrBrightness += 0.02; }
        );

        // `Invalid` is an upstream diagnostic, not proof that an 8-bit
        // scanout format was realized.
        expectMismatch(
            QStringLiteral("display.realization-bitdepth-mismatch"),
            [](ConnectedDisplay &output) {
                output.currentFormat = QStringLiteral("Invalid");
            }
        );

        auto dynamicOnly = topology;
        dynamicOnly.outputs.first().focused = false;
        dynamicOnly.outputs.first().dpms = false;
        dynamicOnly.outputs.first().vrrActive = true;
        QVERIFY(validateDisplayRealization(
            *candidate.value, dynamicOnly
        ).isEmpty());
    }

    void explicitRefreshRealizationUsesPinnedPrecision_data()
    {
        QTest::addColumn<double>("realizedRefresh");
        QTest::addColumn<bool>("accepted");
        QTest::newRow("positive-at-boundary") << 144.00500 << true;
        QTest::newRow("negative-at-boundary") << 143.99500 << true;
        QTest::newRow("positive-outside") << 144.00510 << false;
        QTest::newRow("negative-outside") << 143.99490 << false;
    }

    void explicitRefreshRealizationUsesPinnedPrecision()
    {
        QFETCH(double, realizedRefresh);
        QFETCH(bool, accepted);
        const auto candidate = desiredWithMonitors({desiredMonitor(
            QStringLiteral("DP-1"), QStringLiteral("display-one"),
            QStringLiteral("2560x1440@144")
        )});
        QVERIFY(candidate);
        const auto parsed = parseConnectedDisplayTopology(
            upstreamReply({upstreamMonitor(7, QStringLiteral("DP-1"))})
        );
        QVERIFY(parsed);
        auto topology = *parsed.value;
        topology.outputs.first().refreshRate = realizedRefresh;
        const auto errors = validateDisplayRealization(
            *candidate.value, topology
        );
        QCOMPARE(
            !hasCode(
                errors,
                QStringLiteral("display.realization-refresh-mismatch")
            ),
            accepted
        );
    }

    void automaticModeRequiresPositiveAdvertisedRealization()
    {
        auto desired = desiredMonitor(
            QStringLiteral("DP-1"), QStringLiteral("display-one"),
            QStringLiteral("preferred")
        );
        desired.insert(QStringLiteral("scale"), 1.25);
        const auto candidate = desiredWithMonitors({desired});
        QVERIFY(candidate);
        const auto parsed = parseConnectedDisplayTopology(
            upstreamReply({upstreamMonitor(7, QStringLiteral("DP-1"))})
        );
        QVERIFY(parsed);
        const auto topology = *parsed.value;
        QVERIFY(validateDisplayRealization(
            *candidate.value, topology
        ).isEmpty());

        const auto expectModeMismatch = [this, &candidate, &topology](
            const std::function<void(ConnectedDisplay &)> &mutate
        ) {
            auto changed = topology;
            mutate(changed.outputs.first());
            const auto errors = validateDisplayRealization(
                *candidate.value, changed
            );
            QVERIFY2(
                hasCode(
                    errors, QStringLiteral("display.realization-mode-mismatch")
                ),
                qPrintable(describeErrors(errors))
            );
        };
        expectModeMismatch([](ConnectedDisplay &output) { output.width = 0; });
        expectModeMismatch([](ConnectedDisplay &output) { output.height = 0; });
        expectModeMismatch(
            [](ConnectedDisplay &output) { output.refreshRate = 0.0; }
        );
        expectModeMismatch([](ConnectedDisplay &output) { output.modes.clear(); });
        expectModeMismatch([](ConnectedDisplay &output) {
            output.modes.last().refreshRate = output.refreshRate + 0.0051;
        });

        auto atBoundary = topology;
        atBoundary.outputs.first().modes.last().refreshRate =
            atBoundary.outputs.first().refreshRate + 0.0050;
        QVERIFY(validateDisplayRealization(
            *candidate.value, atBoundary
        ).isEmpty());
    }

    void twoDecimalRealizationUsesHalfStepBoundary_data()
    {
        QTest::addColumn<QString>("field");
        QTest::addColumn<double>("delta");
        QTest::addColumn<bool>("accepted");
        for (const auto &field : {
                 QStringLiteral("scale"),
                 QStringLiteral("sdrBrightness"),
                 QStringLiteral("sdrSaturation"),
                 QStringLiteral("sdrMinLuminance"),
             }) {
            QTest::newRow(qPrintable(field + QStringLiteral("-half-step")))
                << field << 0.00500 << true;
            QTest::newRow(qPrintable(field + QStringLiteral("-outside")))
                << field << 0.00510 << false;
        }
    }

    void twoDecimalRealizationUsesHalfStepBoundary()
    {
        QFETCH(QString, field);
        QFETCH(double, delta);
        QFETCH(bool, accepted);
        auto desired = desiredMonitor(
            QStringLiteral("DP-1"), QStringLiteral("display-one"),
            QStringLiteral("2560x1440@144")
        );
        desired.insert(QStringLiteral("scale"), 1.25);
        const auto candidate = desiredWithMonitors({desired});
        QVERIFY(candidate);
        const auto parsed = parseConnectedDisplayTopology(
            upstreamReply({upstreamMonitor(7, QStringLiteral("DP-1"))})
        );
        QVERIFY(parsed);
        auto topology = *parsed.value;
        auto &output = topology.outputs.first();
        if (field == QStringLiteral("scale")) {
            output.scale += delta;
        } else if (field == QStringLiteral("sdrBrightness")) {
            output.sdrBrightness += delta;
        } else if (field == QStringLiteral("sdrSaturation")) {
            output.sdrSaturation += delta;
        } else {
            output.sdrMinLuminance += delta;
        }
        const auto errors = validateDisplayRealization(
            *candidate.value, topology
        );
        const auto code = field == QStringLiteral("scale")
            ? QStringLiteral("display.realization-scale-mismatch")
            : QStringLiteral("display.realization-sdr-mismatch");
        QCOMPARE(!hasCode(errors, code), accepted);
    }
};

QTEST_GUILESS_MAIN(HyprlandMonitorProfileTest)

#include "hyprland_monitor_profile_test.moc"
