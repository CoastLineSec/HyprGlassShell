#include "component/builtin_component_defaults.h"
#include "component/component_configuration.h"
#include "component/component_contract.h"
#include "component/surface_plan.h"
#include "coordinator/components/component_plan_controller.h"
#include "coordinator/components/component_plan_hydrator.h"
#include "coordinator/components/component_runtime_service.h"

#include <QCryptographicHash>
#include <QDBusConnection>
#include <QDBusContext>
#include <QDBusMessage>
#include <QFile>
#include <QtEndian>
#include <QtTest>

#include <array>
#include <utility>

namespace {

using namespace HyprShelld::Components;

const QString managerName = QStringLiteral(
    "org.hyprshelld.ComponentManager1"
);
const QString managerPath = QStringLiteral(
    "/org/hyprshelld/ComponentManager1"
);
const QString managerInterface = QStringLiteral(
    "org.hyprshelld.ComponentManager1"
);
const QString configName = QStringLiteral("org.hyprshelld.Config1");
const QString configPath = QStringLiteral(
    "/org/hyprshelld/Config1/Components"
);
const QString configInterface = QStringLiteral(
    "org.hyprshelld.ComponentConfig1"
);
const QString runtimeName = QStringLiteral("org.hyprshelld.Coordinator1");
const QString runtimePath = QStringLiteral(
    "/org/hyprshelld/Coordinator1/Components"
);
const QString propertiesInterface = QStringLiteral(
    "org.freedesktop.DBus.Properties"
);
const QString workspaceId = QString::fromLatin1(workspaceSwitcherId);
const QString instanceId = QString::fromLatin1(
    workspaceSwitcherDefaultInstanceId
);
const QString packageDigest(64, QLatin1Char('a'));

enum class RecordFault : int {
    None,
    OversizedSchema,
    OversizedString,
    OversizedList,
    MalformedRuntime,
};

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

QString catalogDigest()
{
    QCryptographicHash hash(QCryptographicHash::Sha256);
    addDigestField(hash, workspaceId.toUtf8(), packageDigest.toLatin1());
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

ComponentConfiguration configuration(
    const quint64 revision,
    const bool componentEnabled
)
{
    ComponentConfiguration result;
    result.revision = revision;
    result.components.insert(workspaceId, {
        .packageDigest = packageDigest,
        .enabled = componentEnabled,
    });
    result.instances.insert(instanceId, {
        .componentId = workspaceId,
        .enabled = true,
        .settings = workspaceSwitcherDefaultSettings(),
    });
    result.bars.insert(QStringLiteral("main"), {
        .outputs = {.mode = QStringLiteral("all")},
        .start = {instanceId},
    });
    return result;
}

class FakeManager final : public QObject, protected QDBusContext {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.hyprshelld.ComponentManager1")
    Q_PROPERTY(QString CatalogDigest READ catalogDigest)

public:
    explicit FakeManager(QDBusConnection connection)
        : connection_(std::move(connection))
        , settingsSchema_(workspaceSettingsSchema())
    {
    }

    [[nodiscard]] QString catalogDigest() const
    {
        return listedCatalogDigest_;
    }

    bool start()
    {
        return connection_.registerService(managerName);
    }

    void stop()
    {
        connection_.unregisterService(managerName);
    }

    [[nodiscard]] bool valid() const
    {
        return !settingsSchema_.isEmpty();
    }

    void configureListing(
        QStringList componentIds,
        QString catalogDigest
    )
    {
        listedComponentIds_ = std::move(componentIds);
        listedCatalogDigest_ = std::move(catalogDigest);
    }

    void setRecordFault(const RecordFault fault)
    {
        recordFault_ = fault;
    }

    void reset()
    {
        listedComponentIds_ = {workspaceId};
        listedCatalogDigest_ = ::catalogDigest();
        requestedComponentIds_.clear();
        listComponentsCallCount_ = 0;
        recordFault_ = RecordFault::None;
    }

    [[nodiscard]] int listComponentsCallCount() const
    {
        return listComponentsCallCount_;
    }

    [[nodiscard]] QStringList requestedComponentIds() const
    {
        return requestedComponentIds_;
    }

public slots:
    QStringList ListComponents(QString &digest) const
    {
        ++listComponentsCallCount_;
        digest = catalogDigest();
        return listedComponentIds_;
    }

    uint GetComponent(
        const QString &componentId,
        const QString &expectedCatalogDigest,
        QString &componentType,
        QString &version,
        QString &name,
        QString &description,
        QStringList &authorNames,
        QStringList &authorEmails,
        QStringList &authorHomepages,
        QString &license,
        QString &homepage,
        QString &source,
        QString &issues,
        QString &componentApiVersion,
        QString &runtimeKind,
        QString &runtimeFactory,
        QString &runtimeEntryPoint,
        QStringList &runtimeArguments,
        QByteArray &settingsSchema,
        QStringList &capabilityIds,
        QStringList &capabilityReasons,
        QStringList &dependencyIds,
        QStringList &dependencyVersionRequirements,
        QString &returnedPackageDigest,
        QString &origin,
        bool &removable
    ) const
    {
        requestedComponentIds_.append(componentId);
        if (!listedComponentIds_.contains(componentId)
            || expectedCatalogDigest != catalogDigest()) {
            sendErrorReply(
                QStringLiteral("org.hyprshelld.ComponentManager1.Error.StaleCatalogDigest"),
                QStringLiteral("Stale test catalog")
            );
            return 0;
        }
        componentType = QStringLiteral("bar-widget");
        version = QStringLiteral("0.1.0");
        name = QStringLiteral("Workspace Switcher");
        description = QStringLiteral("Workspace test component.");
        authorNames = {QStringLiteral("CoastLineSec")};
        authorEmails = {QString()};
        authorHomepages = {QStringLiteral("https://example.com")};
        license = QStringLiteral("LicenseRef-HyprShelld");
        homepage = QStringLiteral("https://example.com");
        source = QStringLiteral("https://example.com/source");
        issues = QStringLiteral("https://example.com/issues");
        componentApiVersion = QStringLiteral("1.0");
        runtimeKind = QStringLiteral("builtin-v1");
        runtimeFactory = QString::fromLatin1(workspaceSwitcherFactory);
        runtimeEntryPoint.clear();
        runtimeArguments.clear();
        settingsSchema = settingsSchema_;
        capabilityIds = {
            QString::fromLatin1(workspacesReadCapability),
            QString::fromLatin1(workspacesActivateCapability),
        };
        capabilityReasons = {
            QStringLiteral("Read workspace state."),
            QStringLiteral("Activate selected workspaces."),
        };
        dependencyIds.clear();
        dependencyVersionRequirements.clear();
        returnedPackageDigest = packageDigest;
        origin = QStringLiteral("system");
        removable = false;

        if (!listedComponentIds_.isEmpty()
            && componentId == listedComponentIds_.constFirst()) {
            switch (recordFault_) {
            case RecordFault::None:
                break;
            case RecordFault::OversizedSchema:
                settingsSchema = QByteArray(256 * 1024 + 1, 'x');
                break;
            case RecordFault::OversizedString:
                name = QString(129, QLatin1Char('x'));
                break;
            case RecordFault::OversizedList:
                authorNames.clear();
                authorEmails.clear();
                authorHomepages.clear();
                for (int index = 0; index < 17; ++index) {
                    authorNames.append(
                        QStringLiteral("Test Author %1").arg(index)
                    );
                    authorEmails.append(QString());
                    authorHomepages.append(QString());
                }
                break;
            case RecordFault::MalformedRuntime:
                runtimeKind = QStringLiteral("unknown-v1");
                break;
            }
        }
        return 1;
    }

private:
    QDBusConnection connection_;
    QByteArray settingsSchema_;
    QStringList listedComponentIds_{workspaceId};
    QString listedCatalogDigest_{::catalogDigest()};
    mutable QStringList requestedComponentIds_;
    mutable int listComponentsCallCount_ = 0;
    RecordFault recordFault_ = RecordFault::None;
};

class FakeConfig final : public QObject, protected QDBusContext {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.hyprshelld.ComponentConfig1")
    Q_PROPERTY(bool Available READ available)
    Q_PROPERTY(bool CatalogAvailable READ catalogAvailable)
    Q_PROPERTY(qulonglong Revision READ revision)
    Q_PROPERTY(QString CatalogDigest READ catalogDigest)
    Q_PROPERTY(QString LoadState READ loadState)

public:
    explicit FakeConfig(QDBusConnection connection)
        : connection_(std::move(connection))
        , state_(configuration(1, true))
    {
    }

    [[nodiscard]] bool available() const
    {
        return true;
    }

    [[nodiscard]] bool catalogAvailable() const
    {
        return true;
    }

    [[nodiscard]] qulonglong revision() const
    {
        return state_.revision;
    }

    [[nodiscard]] QString catalogDigest() const
    {
        return ::catalogDigest();
    }

    [[nodiscard]] QString loadState() const
    {
        return QStringLiteral("normal");
    }

    bool start()
    {
        return connection_.registerService(configName);
    }

    void stop()
    {
        connection_.unregisterService(configName);
    }

    void setConfiguration(
        const quint64 revision,
        const bool componentEnabled
    )
    {
        state_ = configuration(revision, componentEnabled);
    }

    void holdNextSnapshot()
    {
        holdNextSnapshot_ = true;
    }

    [[nodiscard]] bool hasHeldSnapshot() const
    {
        return heldRequest_.type() != QDBusMessage::InvalidMessage;
    }

    bool replyHeldSnapshot()
    {
        if (!hasHeldSnapshot()) {
            return false;
        }
        const auto sent = connection_.send(heldRequest_.createReply({
            heldBytes_,
            QVariant::fromValue<qulonglong>(heldRevision_),
            heldDigest_,
        }));
        heldRequest_ = {};
        heldBytes_.clear();
        heldDigest_.clear();
        heldRevision_ = 0;
        return sent;
    }

    void setMalformedSnapshots(const bool malformed)
    {
        malformedSnapshots_ = malformed;
    }

    bool publishRevision() const
    {
        auto signal = QDBusMessage::createSignal(
            configPath,
            propertiesInterface,
            QStringLiteral("PropertiesChanged")
        );
        signal.setArguments({
            configInterface,
            QVariantMap{{
                QStringLiteral("Revision"),
                QVariant::fromValue<qulonglong>(revision())
            }},
            QStringList{},
        });
        return connection_.send(signal);
    }

public slots:
    QByteArray GetSnapshot(
        qulonglong &snapshotRevision,
        QString &snapshotCatalogDigest
    )
    {
        const auto bytes = malformedSnapshots_
            ? QByteArrayLiteral("{}")
            : serializeComponentConfiguration(state_);
        if (holdNextSnapshot_) {
            holdNextSnapshot_ = false;
            heldRequest_ = message();
            heldBytes_ = bytes;
            heldRevision_ = revision();
            heldDigest_ = catalogDigest();
            setDelayedReply(true);
            snapshotRevision = 0;
            snapshotCatalogDigest.clear();
            return {};
        }
        snapshotRevision = revision();
        snapshotCatalogDigest = catalogDigest();
        return bytes;
    }

private:
    QDBusConnection connection_;
    ComponentConfiguration state_;
    QDBusMessage heldRequest_;
    QByteArray heldBytes_;
    QString heldDigest_;
    quint64 heldRevision_ = 0;
    bool holdNextSnapshot_ = false;
    bool malformedSnapshots_ = false;
};

} // namespace

class ComponentPlanHydratorTest final : public QObject {
    Q_OBJECT

private slots:
    void initTestCase()
    {
        QVERIFY2(bus_.isConnected(), qPrintable(bus_.lastError().message()));
        QVERIFY2(
            managerBus_.isConnected(),
            qPrintable(managerBus_.lastError().message())
        );
        QVERIFY2(
            configBus_.isConnected(),
            qPrintable(configBus_.lastError().message())
        );
        QVERIFY2(
            runtimeBus_.isConnected(),
            qPrintable(runtimeBus_.lastError().message())
        );
        QVERIFY(manager_.valid());
        QVERIFY(managerBus_.registerObject(
            managerPath,
            &manager_,
            QDBusConnection::ExportAllProperties
                | QDBusConnection::ExportAllSlots
        ));
        QVERIFY(configBus_.registerObject(
            configPath,
            &config_,
            QDBusConnection::ExportAllProperties
                | QDBusConnection::ExportAllSlots
        ));
    }

    void cleanupTestCase()
    {
        manager_.stop();
        config_.stop();
        runtimeBus_.unregisterService(runtimeName);
        runtimeBus_.unregisterObject(runtimePath);
        managerBus_.unregisterObject(managerPath);
        configBus_.unregisterObject(configPath);
        for (const auto &name : {
                 QStringLiteral("component-plan-hydrator-test"),
                 QStringLiteral("component-plan-manager-test"),
                 QStringLiteral("component-plan-config-test"),
                 QStringLiteral("component-plan-runtime-test"),
             }) {
            QDBusConnection::disconnectFromBus(name);
        }
    }

    void rejectsInvalidCatalogListingsBeforeRecordFetch_data()
    {
        QTest::addColumn<QStringList>("componentIds");
        QTest::addColumn<QString>("catalogDigest");

        QStringList oversized;
        oversized.reserve(513);
        for (int index = 0; index < 513; ++index) {
            oversized.append(
                QStringLiteral("org.example.widget%1")
                    .arg(index, 4, 10, QLatin1Char('0'))
            );
        }
        const QString validDigest(64, QLatin1Char('a'));
        QTest::newRow("oversized") << oversized << validDigest;
        QTest::newRow("duplicate")
            << QStringList{workspaceId, workspaceId}
            << validDigest;
        QTest::newRow("unsorted")
            << QStringList{
                   QStringLiteral("org.example.widget"),
                   QStringLiteral("com.example.widget"),
               }
            << validDigest;
        QTest::newRow("invalid-id")
            << QStringList{QStringLiteral("invalid")}
            << validDigest;
        QTest::newRow("invalid-digest")
            << QStringList{workspaceId}
            << QStringLiteral("not-a-digest");
    }

    void rejectsInvalidCatalogListingsBeforeRecordFetch()
    {
        QFETCH(QStringList, componentIds);
        QFETCH(QString, catalogDigest);

        manager_.reset();
        manager_.configureListing(
            std::move(componentIds),
            std::move(catalogDigest)
        );
        QVERIFY(manager_.start());
        {
            HyprShelld::ComponentPlanController controller;
            HyprShelld::ComponentPlanHydrator hydrator(
                &controller,
                bus_
            );
            hydrator.start();

            QTRY_VERIFY_WITH_TIMEOUT(
                manager_.listComponentsCallCount() > 0,
                3000
            );
            QTRY_COMPARE_WITH_TIMEOUT(
                controller.stateName(),
                QStringLiteral("unavailable"),
                3000
            );
            QVERIFY(manager_.requestedComponentIds().isEmpty());
        }
        manager_.stop();
    }

    void rejectsInvalidFirstRecordBeforeSecondFetch_data()
    {
        QTest::addColumn<int>("recordFault");

        QTest::newRow("oversized-schema")
            << static_cast<int>(RecordFault::OversizedSchema);
        QTest::newRow("oversized-string")
            << static_cast<int>(RecordFault::OversizedString);
        QTest::newRow("oversized-list")
            << static_cast<int>(RecordFault::OversizedList);
        QTest::newRow("malformed-runtime")
            << static_cast<int>(RecordFault::MalformedRuntime);
    }

    void rejectsInvalidFirstRecordBeforeSecondFetch()
    {
        QFETCH(int, recordFault);

        const QString secondComponentId = QStringLiteral(
            "org.example.community-widget"
        );
        manager_.reset();
        manager_.configureListing(
            {workspaceId, secondComponentId},
            QString(64, QLatin1Char('a'))
        );
        manager_.setRecordFault(
            static_cast<RecordFault>(recordFault)
        );
        QVERIFY(manager_.start());
        {
            HyprShelld::ComponentPlanController controller;
            HyprShelld::ComponentPlanHydrator hydrator(
                &controller,
                bus_
            );
            hydrator.start();

            QTRY_VERIFY_WITH_TIMEOUT(
                !manager_.requestedComponentIds().isEmpty(),
                3000
            );
            QTRY_COMPARE_WITH_TIMEOUT(
                controller.stateName(),
                QStringLiteral("unavailable"),
                3000
            );
            QCOMPARE(
                manager_.requestedComponentIds(),
                QStringList{workspaceId}
            );
        }
        manager_.stop();
    }

    void joinsAuthoritiesAtomicallyAndRetainsLastGood()
    {
        manager_.reset();
        HyprShelld::ComponentPlanController controller;
        HyprShelld::ComponentRuntimeService runtime(
            &controller,
            runtimeBus_
        );
        HyprShelld::ComponentPlanHydrator hydrator(
            &controller,
            bus_
        );

        // Runtime1 is exported before the coordinator name becomes visible;
        // hydration begins only after both coordinator objects would be ready.
        QVERIFY(runtimeBus_.registerObject(
            runtimePath,
            &runtime,
            QDBusConnection::ExportAllProperties
                | QDBusConnection::ExportAllSlots
        ));
        QVERIFY(runtimeBus_.registerService(runtimeName));
        hydrator.start();

        QTRY_COMPARE_WITH_TIMEOUT(
            controller.stateName(),
            QStringLiteral("unavailable"),
            3000
        );
        QVERIFY(controller.artifact() == nullptr);

        QVERIFY(manager_.start());
        QTest::qWait(100);
        QVERIFY(controller.artifact() == nullptr);
        QVERIFY(config_.start());
        QTRY_COMPARE_WITH_TIMEOUT(
            controller.stateName(),
            QStringLiteral("authoritative"),
            3000
        );
        QCOMPARE(controller.artifact()->plan.configurationRevision, 1ULL);
        QCOMPARE(controller.artifact()->plan.instances.size(), 1);

        // Hold generation N, then publish N+1. The held old reply must never
        // replace the newer authoritative empty plan.
        config_.holdNextSnapshot();
        config_.setConfiguration(2, true);
        QVERIFY(config_.publishRevision());
        QTRY_VERIFY_WITH_TIMEOUT(config_.hasHeldSnapshot(), 3000);
        config_.setConfiguration(3, false);
        QVERIFY(config_.publishRevision());
        QTRY_COMPARE_WITH_TIMEOUT(
            controller.stateName(),
            QStringLiteral("authoritative"),
            3000
        );
        QTRY_COMPARE_WITH_TIMEOUT(
            controller.artifact()->plan.configurationRevision,
            3ULL,
            3000
        );
        QVERIFY(controller.artifact()->plan.instances.isEmpty());
        QVERIFY(runtime.surfacePlanRevision() != 0);
        QString returnedDigest;
        const auto emptyBytes = runtime.GetSurfacePlan(
            runtime.surfacePlanRevision(),
            returnedDigest
        );
        const auto emptyPublished = parseSurfacePlan(emptyBytes);
        QVERIFY(emptyPublished);
        QVERIFY(emptyPublished.value->instances.isEmpty());

        QVERIFY(config_.replyHeldSnapshot());
        QTest::qWait(100);
        QCOMPARE(controller.artifact()->plan.configurationRevision, 3ULL);
        QVERIFY(controller.artifact()->plan.instances.isEmpty());

        // A malformed current reply degrades to retained without replacing
        // the accepted empty plan.
        config_.setMalformedSnapshots(true);
        config_.setConfiguration(4, true);
        QVERIFY(config_.publishRevision());
        QTRY_COMPARE_WITH_TIMEOUT(
            controller.stateName(),
            QStringLiteral("retained"),
            3000
        );
        QCOMPARE(controller.artifact()->plan.configurationRevision, 3ULL);
        QVERIFY(controller.artifact()->plan.instances.isEmpty());

        config_.stop();
        QTRY_COMPARE_WITH_TIMEOUT(
            controller.stateName(),
            QStringLiteral("retained"),
            3000
        );
        QCOMPARE(controller.artifact()->plan.configurationRevision, 3ULL);

        config_.setMalformedSnapshots(false);
        config_.setConfiguration(5, true);
        QVERIFY(config_.start());
        QTRY_COMPARE_WITH_TIMEOUT(
            controller.stateName(),
            QStringLiteral("authoritative"),
            3000
        );
        QTRY_COMPARE_WITH_TIMEOUT(
            controller.artifact()->plan.configurationRevision,
            5ULL,
            3000
        );
        QCOMPARE(controller.artifact()->plan.instances.size(), 1);

        config_.stop();
        manager_.stop();
        runtimeBus_.unregisterService(runtimeName);
        runtimeBus_.unregisterObject(runtimePath);
    }

private:
    QDBusConnection bus_ = QDBusConnection::connectToBus(
        QDBusConnection::SessionBus,
        QStringLiteral("component-plan-hydrator-test")
    );
    QDBusConnection managerBus_ = QDBusConnection::connectToBus(
        QDBusConnection::SessionBus,
        QStringLiteral("component-plan-manager-test")
    );
    QDBusConnection configBus_ = QDBusConnection::connectToBus(
        QDBusConnection::SessionBus,
        QStringLiteral("component-plan-config-test")
    );
    QDBusConnection runtimeBus_ = QDBusConnection::connectToBus(
        QDBusConnection::SessionBus,
        QStringLiteral("component-plan-runtime-test")
    );
    FakeManager manager_{managerBus_};
    FakeConfig config_{configBus_};
};

QTEST_GUILESS_MAIN(ComponentPlanHydratorTest)

#include "component_plan_hydrator_test.moc"
