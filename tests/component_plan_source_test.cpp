#include "component/builtin_component_defaults.h"
#include "component/component_configuration.h"
#include "component/component_contract.h"
#include "coordinator/components/component_plan_source.h"

#include <QCryptographicHash>
#include <QFile>
#include <QTest>
#include <QtEndian>

#include <algorithm>
#include <array>

namespace {

using namespace HyprShelld::Components;

const QString workspaceId = QString::fromLatin1(workspaceSwitcherId);
const QString instanceId = QString::fromLatin1(
    workspaceSwitcherDefaultInstanceId
);

void addDigestField(
    QCryptographicHash &hash,
    const QByteArray &name,
    const QByteArray &value
)
{
    std::array<uchar, sizeof(quint64)> length{};
    qToBigEndian<quint64>(static_cast<quint64>(name.size()), length.data());
    hash.addData(QByteArrayView(
        reinterpret_cast<const char *>(length.data()),
        length.size()
    ));
    hash.addData(name);
    qToBigEndian<quint64>(static_cast<quint64>(value.size()), length.data());
    hash.addData(QByteArrayView(
        reinterpret_cast<const char *>(length.data()),
        length.size()
    ));
    hash.addData(value);
}

QString catalogDigest(
    const QStringList &ids,
    const QVector<RuntimeCatalogComponentRecord> &records
)
{
    QHash<QString, QString> packages;
    for (const auto &record : records) {
        packages.insert(record.componentId, record.packageDigest);
    }
    QCryptographicHash hash(QCryptographicHash::Sha256);
    for (const auto &id : ids) {
        addDigestField(hash, id.toUtf8(), packages.value(id).toLatin1());
    }
    return QString::fromLatin1(hash.result().toHex());
}

QByteArray workspaceSettingsSchema()
{
    QFile file(QFINDTESTDATA(
        "../data/components/"
        "io.github.coastlinesec.hyprshelld.workspace-switcher/"
        "settings.schema.json"
    ));
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }
    return file.readAll();
}

RuntimeCatalogComponentRecord workspaceRecord()
{
    return {
        .componentId = workspaceId,
        .manifestVersion = 1,
        .componentType = QStringLiteral("bar-widget"),
        .version = QStringLiteral("0.1.0"),
        .name = QStringLiteral("Workspace Switcher"),
        .description = QStringLiteral("Workspace test component."),
        .authorNames = {QStringLiteral("CoastLineSec")},
        .authorEmails = {QString()},
        .authorHomepages = {QStringLiteral("https://example.com")},
        .license = QStringLiteral("LicenseRef-HyprShelld"),
        .homepage = QStringLiteral("https://example.com"),
        .source = QStringLiteral("https://example.com/source"),
        .issues = QStringLiteral("https://example.com/issues"),
        .componentApiVersion = QStringLiteral("1.0"),
        .runtimeKind = QStringLiteral("builtin-v1"),
        .runtimeFactory = QString::fromLatin1(workspaceSwitcherFactory),
        .settingsSchema = workspaceSettingsSchema(),
        .capabilityIds = {
            QString::fromLatin1(workspacesReadCapability),
            QString::fromLatin1(workspacesActivateCapability),
        },
        .capabilityReasons = {
            QStringLiteral("Read workspace state."),
            QStringLiteral("Activate selected workspaces."),
        },
        .packageDigest = QString(64, QLatin1Char('a')),
        .origin = QStringLiteral("system"),
        .removable = false,
    };
}

RuntimeCatalogComponentRecord extraDeclarativeRecord()
{
    return {
        .componentId = QStringLiteral("org.example.community-widget"),
        .manifestVersion = 1,
        .componentType = QStringLiteral("bar-widget"),
        .version = QStringLiteral("1.0.0"),
        .name = QStringLiteral("Community Widget"),
        .description = QStringLiteral("A test community component."),
        .authorNames = {QStringLiteral("Example Author")},
        .authorEmails = {QStringLiteral("author@example.com")},
        .authorHomepages = {QString()},
        .license = QStringLiteral("MIT"),
        .componentApiVersion = QStringLiteral("1.0"),
        .runtimeKind = QStringLiteral("declarative-v1"),
        .runtimeEntryPoint = QStringLiteral("payload/Main.qml"),
        .packageDigest = QString(64, QLatin1Char('b')),
        .origin = QStringLiteral("user"),
        .removable = true,
    };
}

ComponentConfiguration desiredConfiguration(const QString &packageDigest)
{
    ComponentConfiguration configuration;
    configuration.revision = 7;
    configuration.components.insert(workspaceId, {
        .packageDigest = packageDigest,
        .enabled = true,
    });
    configuration.instances.insert(instanceId, {
        .componentId = workspaceId,
        .enabled = true,
        .settings = workspaceSwitcherDefaultSettings(),
    });
    configuration.bars.insert(QStringLiteral("main"), {
        .outputs = {.mode = QStringLiteral("all")},
        .start = {instanceId},
    });
    return configuration;
}

class ComponentPlanSourceTest final : public QObject {
    Q_OBJECT

private slots:
    void hydratesCompleteCatalogAndStrictConfiguration()
    {
        const QVector records{workspaceRecord()};
        QVERIFY(!records.constFirst().settingsSchema.isEmpty());
        const QStringList ids{workspaceId};
        const auto digest = catalogDigest(ids, records);
        auto catalog = hydrateRuntimeCatalog(ids, digest, records);
        QVERIFY2(catalog, qPrintable(catalog.errors.value(0).message));

        const auto configuration = desiredConfiguration(
            records.constFirst().packageDigest
        );
        const auto bytes = serializeComponentConfiguration(configuration);
        auto runtime = hydrateRuntimeConfiguration(
            QByteArrayView(bytes),
            configuration.revision,
            digest,
            catalog.value->configurationCatalog
        );
        QVERIFY2(runtime, qPrintable(runtime.errors.value(0).message));
        QCOMPARE(runtime.value->revision, configuration.revision);
        QCOMPARE(runtime.value->instances.size(), 1);
        QCOMPARE(
            runtime.value->instances.value(instanceId).settings,
            workspaceSwitcherDefaultSettings()
        );

        runtime = hydrateRuntimeConfiguration(
            QByteArrayView(bytes),
            configuration.revision + 1,
            digest,
            catalog.value->configurationCatalog
        );
        QVERIFY(!runtime);
        QCOMPARE(
            runtime.errors.constFirst().code,
            QStringLiteral("component-runtime.configuration-revision-mismatch")
        );
    }

    void rejectsDuplicateCatalogIds()
    {
        const auto record = workspaceRecord();
        const QStringList ids{workspaceId, workspaceId};
        const QVector records{record, record};
        const auto hydrated = hydrateRuntimeCatalog(
            ids,
            QString(64, QLatin1Char('c')),
            records
        );
        QVERIFY(!hydrated);
        QCOMPARE(
            hydrated.errors.constFirst().code,
            QStringLiteral("component-runtime.invalid-catalog-id-set")
        );
    }

    void rejectsMalformedExtraCatalogRecord()
    {
        auto extra = extraDeclarativeRecord();
        // Arguments are forbidden for declarative-v1 even though this record
        // is not currently eligible for a surfaced factory.
        extra.runtimeArguments = {QStringLiteral("--unexpected")};
        const QStringList ids{workspaceId, extra.componentId};
        const QVector records{workspaceRecord(), extra};
        const auto hydrated = hydrateRuntimeCatalog(
            ids,
            catalogDigest(ids, records),
            records
        );
        QVERIFY(!hydrated);
        QVERIFY(std::ranges::any_of(
            hydrated.errors,
            [](const ValidationError &error) {
                return error.code
                    == QStringLiteral("component-runtime.invalid-runtime-fields");
            }
        ));
    }

    void authoritativeParserRejectsUnknownConfigurationFields()
    {
        const QVector records{workspaceRecord()};
        const QStringList ids{workspaceId};
        const auto digest = catalogDigest(ids, records);
        const auto catalog = hydrateRuntimeCatalog(ids, digest, records);
        QVERIFY(catalog);

        auto bytes = serializeComponentConfiguration(
            desiredConfiguration(records.constFirst().packageDigest)
        );
        bytes.replace("\"components\":{", "\"unexpected\":true,\"components\":{");
        const auto runtime = hydrateRuntimeConfiguration(
            QByteArrayView(bytes),
            7,
            digest,
            catalog.value->configurationCatalog
        );
        QVERIFY(!runtime);
        QVERIFY(std::ranges::any_of(
            runtime.errors,
            [](const ValidationError &error) {
                return error.code
                    == QStringLiteral("component-config.unknown-field");
            }
        ));
    }
};

} // namespace

QTEST_GUILESS_MAIN(ComponentPlanSourceTest)

#include "component_plan_source_test.moc"
