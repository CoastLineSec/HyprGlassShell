#include "component/builtin_component_defaults.h"
#include "component/component_contract.h"
#include "component/surface_plan.h"
#include "component_runtime_client.h"

#include <QDBusConnection>
#include <QDBusContext>
#include <QDBusMessage>
#include <QtTest>

#include <utility>

namespace {

using namespace HyprShelld::Components;

const QString busName = QStringLiteral("org.hyprshelld.Coordinator1");
const QString objectPath = QStringLiteral(
    "/org/hyprshelld/Coordinator1/Components"
);
const QString interfaceName = QStringLiteral(
    "org.hyprshelld.ComponentRuntime1"
);
const QString propertiesInterface = QStringLiteral(
    "org.freedesktop.DBus.Properties"
);

SurfacePlan workspacePlan()
{
    SurfacePlan plan;
    plan.catalogDigest = QString(64, QLatin1Char('a'));
    plan.configurationRevision = 3;
    const auto instanceId = QString::fromLatin1(
        workspaceSwitcherDefaultInstanceId
    );
    plan.instances.insert(instanceId, {
        .componentId = QString::fromLatin1(workspaceSwitcherId),
        .componentType = QStringLiteral("bar-widget"),
        .packageDigest = QString(64, QLatin1Char('b')),
        .runtimeKind = QStringLiteral("builtin-v1"),
        .factory = QString::fromLatin1(workspaceSwitcherFactory),
        .settings = workspaceSwitcherDefaultSettings(),
    });
    plan.barLayouts.insert(QStringLiteral("main"), {
        .outputMode = QStringLiteral("all"),
        .start = {instanceId},
    });
    return plan;
}

SurfacePlan emptyPlan()
{
    SurfacePlan plan;
    plan.catalogDigest = QString(64, QLatin1Char('c'));
    plan.configurationRevision = 4;
    plan.barLayouts.insert(QStringLiteral("main"), {
        .outputMode = QStringLiteral("all"),
    });
    return plan;
}

class FakeComponentRuntime final : public QObject, protected QDBusContext {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.hyprshelld.ComponentRuntime1")
    Q_PROPERTY(qulonglong SurfacePlanRevision READ surfacePlanRevision)
    Q_PROPERTY(QString SurfacePlanDigest READ surfacePlanDigest)
    Q_PROPERTY(QString SurfacePlanState READ surfacePlanState)

public:
    explicit FakeComponentRuntime(QDBusConnection connection)
        : connection_(std::move(connection))
    {
    }

    [[nodiscard]] qulonglong surfacePlanRevision() const
    {
        return artifact_.revision;
    }

    [[nodiscard]] QString surfacePlanDigest() const
    {
        return artifact_.digest;
    }

    [[nodiscard]] QString surfacePlanState() const
    {
        return QStringLiteral("authoritative");
    }

    void setPlan(const SurfacePlan &plan)
    {
        const auto artifact = makeSurfacePlanArtifact(plan);
        Q_ASSERT(artifact);
        artifact_ = *artifact.value;
    }

    [[nodiscard]] qsizetype heldRequestCount() const
    {
        return heldRequests_.size();
    }

    bool start()
    {
        return connection_.registerService(busName);
    }

    void stop()
    {
        connection_.unregisterService(busName);
    }

    bool publishChanged(const QVariantMap &changed)
    {
        auto signal = QDBusMessage::createSignal(
            objectPath,
            propertiesInterface,
            QStringLiteral("PropertiesChanged")
        );
        signal.setArguments({interfaceName, changed, QStringList{}});
        return connection_.send(signal);
    }

    bool publishCurrentPlan() const
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
                    QStringLiteral("SurfacePlanRevision"),
                    QVariant::fromValue<qulonglong>(artifact_.revision)
                },
                {QStringLiteral("SurfacePlanDigest"), artifact_.digest},
                {
                    QStringLiteral("SurfacePlanState"),
                    QStringLiteral("authoritative")
                },
            },
            QStringList{},
        });
        return connection_.send(signal);
    }

    bool replyNextPlan()
    {
        if (heldRequests_.isEmpty()) {
            return false;
        }
        const auto request = heldRequests_.takeFirst();
        return connection_.send(request.createReply({
            artifact_.bytes,
            artifact_.digest,
        }));
    }

    void reset()
    {
        stop();
        heldRequests_.clear();
        setPlan(workspacePlan());
    }

public slots:
    QByteArray GetSurfacePlan(
        qulonglong expectedSurfacePlanRevision,
        QString &surfacePlanDigest
    )
    {
        Q_UNUSED(expectedSurfacePlanRevision)
        surfacePlanDigest.clear();
        setDelayedReply(true);
        heldRequests_.append(message());
        return {};
    }

private:
    QDBusConnection connection_;
    SurfacePlanArtifact artifact_;
    QList<QDBusMessage> heldRequests_;
};

} // namespace

class ComponentRuntimeClientTest final : public QObject {
    Q_OBJECT

private slots:
    void initTestCase()
    {
        QVERIFY2(bus_.isConnected(), qPrintable(bus_.lastError().message()));
        QVERIFY2(
            serviceBus_.isConnected(),
            qPrintable(serviceBus_.lastError().message())
        );
        service_.setPlan(workspacePlan());
        QVERIFY(serviceBus_.registerObject(
            objectPath,
            &service_,
            QDBusConnection::ExportAllProperties
                | QDBusConnection::ExportAllSlots
        ));
    }

    void cleanup()
    {
        service_.reset();
        QCoreApplication::processEvents();
    }

    void cleanupTestCase()
    {
        service_.stop();
        serviceBus_.unregisterObject(objectPath);
        QDBusConnection::disconnectFromBus(
            QStringLiteral("component-runtime-client-test")
        );
        QDBusConnection::disconnectFromBus(
            QStringLiteral("component-runtime-service-test")
        );
    }

    void malformedMetadataCancelsOlderPlanFetch()
    {
        HyprShelld::ComponentRuntimeClient client(bus_, nullptr);
        QVERIFY(client.usingFallback());
        QVERIFY(!client.planCurrent());
        QCOMPARE(
            client.barInstances(
                QStringLiteral("main"),
                QStringLiteral("DP-4"),
                QStringLiteral("start")
            ).constFirst().toMap().value(
                QStringLiteral("compiledFallback")
            ).toBool(),
            true
        );

        QList<bool> availabilityStates;
        connect(
            &client,
            &HyprShelld::ComponentRuntimeClient::availableChanged,
            this,
            [&client, &availabilityStates] {
                availabilityStates.append(client.available());
            }
        );
        QVERIFY(service_.start());
        QTRY_COMPARE_WITH_TIMEOUT(service_.heldRequestCount(), 1, 3000);
        QTRY_VERIFY_WITH_TIMEOUT(client.available(), 3000);
        availabilityStates.clear();

        QVERIFY(service_.publishChanged({
            {QStringLiteral("SurfacePlanRevision"), QStringLiteral("bad")},
        }));
        QTRY_VERIFY_WITH_TIMEOUT(
            availabilityStates.contains(false),
            3000
        );
        QTRY_COMPARE_WITH_TIMEOUT(service_.heldRequestCount(), 2, 3000);

        QVERIFY(service_.replyNextPlan());
        QTest::qWait(50);
        QVERIFY(client.usingFallback());
        QVERIFY(!client.planCurrent());

        QVERIFY(service_.replyNextPlan());
        QTRY_VERIFY_WITH_TIMEOUT(!client.usingFallback(), 3000);
        QTRY_VERIFY_WITH_TIMEOUT(client.planCurrent(), 3000);
        QCOMPARE(client.planState(), QStringLiteral("authoritative"));
    }

    void acceptedEmptyPlanIsRetainedAcrossOwnerLoss()
    {
        service_.setPlan(emptyPlan());
        HyprShelld::ComponentRuntimeClient client(bus_, nullptr);
        QVERIFY(service_.start());
        QTRY_COMPARE_WITH_TIMEOUT(service_.heldRequestCount(), 1, 3000);
        QVERIFY(service_.replyNextPlan());
        QTRY_VERIFY_WITH_TIMEOUT(!client.usingFallback(), 3000);
        QTRY_VERIFY_WITH_TIMEOUT(client.planCurrent(), 3000);
        QVERIFY(client.barInstances(
            QStringLiteral("main"),
            QStringLiteral("DP-4"),
            QStringLiteral("start")
        ).isEmpty());

        service_.stop();
        QTRY_VERIFY_WITH_TIMEOUT(!client.available(), 3000);
        QCOMPARE(client.planState(), QStringLiteral("retained"));
        QVERIFY(!client.planCurrent());
        QVERIFY(!client.usingFallback());
        QVERIFY(client.barInstances(
            QStringLiteral("main"),
            QStringLiteral("DP-4"),
            QStringLiteral("start")
        ).isEmpty());
    }

    void timedOutRefreshKeepsAcceptedPlanAsLastGood()
    {
        service_.setPlan(workspacePlan());
        HyprShelld::ComponentRuntimeClient client(bus_, nullptr);
        QVERIFY(service_.start());
        QTRY_COMPARE_WITH_TIMEOUT(service_.heldRequestCount(), 1, 3000);
        QVERIFY(service_.replyNextPlan());
        QTRY_VERIFY_WITH_TIMEOUT(client.planCurrent(), 3000);
        const auto acceptedRevision = client.planRevision();

        service_.setPlan(emptyPlan());
        QVERIFY(service_.publishCurrentPlan());
        QTRY_COMPARE_WITH_TIMEOUT(service_.heldRequestCount(), 1, 3000);
        QTRY_VERIFY_WITH_TIMEOUT(!client.planCurrent(), 3000);
        QTRY_VERIFY_WITH_TIMEOUT(!client.lastError().isEmpty(), 4500);
        QTRY_VERIFY_WITH_TIMEOUT(service_.heldRequestCount() >= 2, 4500);

        QCOMPARE(client.planRevision(), acceptedRevision);
        QVERIFY(!client.usingFallback());
        const auto retained = client.barInstances(
            QStringLiteral("main"),
            QStringLiteral("DP-4"),
            QStringLiteral("start")
        );
        QCOMPARE(retained.size(), 1);
        QCOMPARE(
            retained.constFirst().toMap().value(
                QStringLiteral("compiledFallback")
            ).toBool(),
            false
        );
    }

private:
    QDBusConnection bus_ = QDBusConnection::connectToBus(
        QDBusConnection::SessionBus,
        QStringLiteral("component-runtime-client-test")
    );
    QDBusConnection serviceBus_ = QDBusConnection::connectToBus(
        QDBusConnection::SessionBus,
        QStringLiteral("component-runtime-service-test")
    );
    FakeComponentRuntime service_{serviceBus_};
};

QTEST_GUILESS_MAIN(ComponentRuntimeClientTest)

#include "component_runtime_client_test.moc"
