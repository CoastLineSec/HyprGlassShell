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

SurfacePlan declarativePlan()
{
    SurfacePlan plan;
    plan.catalogDigest = QString(64, QLatin1Char('d'));
    plan.configurationRevision = 5;
    const auto instanceId = QStringLiteral(
        "11111111-1111-4111-8111-111111111111"
    );
    plan.instances.insert(instanceId, {
        .componentId = QStringLiteral("org.example.widgets.status"),
        .componentType = QStringLiteral("bar-widget"),
        .packageDigest = QString(64, QLatin1Char('e')),
        .runtimeKind = QStringLiteral("declarative-v1"),
        .declarativeText = QStringLiteral("Safe status"),
        .declarativeTooltip = QStringLiteral("Trusted plain text"),
        .declarativeMaximumWidth = 240,
    });
    plan.barLayouts.insert(QStringLiteral("main"), {
        .outputMode = QStringLiteral("all"),
        .end = {instanceId},
    });
    return plan;
}

class FakeComponentRuntime final : public QObject, protected QDBusContext {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.hyprshelld.ComponentRuntime1")
    Q_PROPERTY(qulonglong SurfacePlanRevision READ surfacePlanRevision)
    Q_PROPERTY(QString SurfacePlanDigest READ surfacePlanDigest)
    Q_PROPERTY(QString SurfacePlanState READ surfacePlanState)
    Q_PROPERTY(qulonglong RuntimeHealthRevision READ runtimeHealthRevision)
    Q_PROPERTY(bool ThirdPartySafeMode READ thirdPartySafeMode)

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

    [[nodiscard]] qulonglong runtimeHealthRevision() const
    {
        return runtimeHealthRevision_;
    }

    [[nodiscard]] bool thirdPartySafeMode() const
    {
        return false;
    }

    void setPlan(const SurfacePlan &plan)
    {
        const auto artifact = makeSurfacePlanArtifact(plan);
        Q_ASSERT(artifact);
        artifact_ = *artifact.value;
    }

    void setQuarantined(bool quarantined)
    {
        if (!quarantined) {
            setRuntimeHealthRows({}, {}, {}, {}, {});
            return;
        }
        setRuntimeHealthRows(
            {QStringLiteral("org.example.widgets.status")},
            {QString(64, QLatin1Char('e'))},
            {QStringLiteral("quarantined")},
            {QStringLiteral("timeout")},
            {1}
        );
    }

    void setRuntimeHealthRows(
        QStringList componentIds,
        QStringList packageDigests,
        QStringList states,
        QStringList reasons,
        QList<uint> failureCounts
    )
    {
        healthComponentIds_ = std::move(componentIds);
        healthPackageDigests_ = std::move(packageDigests);
        healthStates_ = std::move(states);
        healthReasons_ = std::move(reasons);
        healthFailureCounts_ = std::move(failureCounts);
        runtimeHealthRevision_ = healthComponentIds_.isEmpty() ? 0 : 1;
    }

    void failHealthList(bool fail)
    {
        failHealthList_ = fail;
    }

    [[nodiscard]] qsizetype healthListCount() const
    {
        return healthListCount_;
    }

    [[nodiscard]] qsizetype heldRequestCount() const
    {
        return heldRequests_.size();
    }

    [[nodiscard]] qsizetype stableReportCount() const
    {
        return stableReports_.size();
    }

    [[nodiscard]] qsizetype failedReportCount() const
    {
        return failedReports_.size();
    }

    [[nodiscard]] qsizetype cancellationCount() const
    {
        return cancellationCount_;
    }

    void holdAuthorization(bool hold)
    {
        holdAuthorization_ = hold;
    }

    bool replyAuthorization()
    {
        if (heldAuthorizations_.isEmpty()) {
            return false;
        }
        return connection_.send(
            heldAuthorizations_.takeFirst().createReply(QVariant(true))
        );
    }

    void holdRetries(bool hold)
    {
        holdRetries_ = hold;
    }

    [[nodiscard]] qsizetype heldRetryCount() const
    {
        return heldRetries_.size();
    }

    bool replyNextRetry()
    {
        if (heldRetries_.isEmpty()) {
            return false;
        }
        return connection_.send(
            heldRetries_.takeFirst().createReply({
                QVariant::fromValue<qulonglong>(runtimeHealthRevision_)
            })
        );
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
        stableReports_.clear();
        failedReports_.clear();
        heldAuthorizations_.clear();
        cancellationCount_ = 0;
        holdAuthorization_ = false;
        heldRetries_.clear();
        holdRetries_ = false;
        setQuarantined(false);
        failHealthList_ = false;
        healthListCount_ = 0;
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

    QStringList ListComponentRuntimeStates(
        qulonglong expectedRuntimeHealthRevision,
        QStringList &packageDigests,
        QStringList &states,
        QStringList &reasons,
        QList<uint> &failureCounts
    )
    {
        Q_UNUSED(expectedRuntimeHealthRevision)
        ++healthListCount_;
        packageDigests.clear();
        states.clear();
        reasons.clear();
        failureCounts.clear();
        if (failHealthList_) {
            sendErrorReply(
                QStringLiteral(
                    "org.hyprshelld.ComponentRuntime1.Error.TestFailure"
                ),
                QStringLiteral("Injected runtime health list failure")
            );
            return {};
        }
        packageDigests = healthPackageDigests_;
        states = healthStates_;
        reasons = healthReasons_;
        failureCounts = healthFailureCounts_;
        return healthComponentIds_;
    }

    qulonglong RetryComponent(
        const QString &componentId,
        const QString &expectedPackageDigest,
        qulonglong expectedRuntimeHealthRevision
    )
    {
        Q_UNUSED(componentId)
        Q_UNUSED(expectedPackageDigest)
        Q_UNUSED(expectedRuntimeHealthRevision)
        if (holdRetries_) {
            setDelayedReply(true);
            heldRetries_.append(message());
        }
        return runtimeHealthRevision_;
    }

    bool AuthorizeSurfacePlan(qulonglong surfacePlanRevision)
    {
        if (holdAuthorization_) {
            setDelayedReply(true);
            heldAuthorizations_.append(message());
            return false;
        }
        return surfacePlanRevision == artifact_.revision;
    }

    bool CancelSurfacePlanAuthorization(qulonglong surfacePlanRevision)
    {
        ++cancellationCount_;
        return surfacePlanRevision == artifact_.revision;
    }

    void ActivationStable(
        const QString &instanceId,
        const QString &componentId,
        const QString &packageDigest,
        qulonglong surfacePlanRevision
    )
    {
        stableReports_.append({
            instanceId,
            componentId,
            packageDigest,
            QString::number(surfacePlanRevision),
        });
    }

    void ActivationFailed(
        const QString &instanceId,
        const QString &componentId,
        const QString &packageDigest,
        qulonglong surfacePlanRevision,
        const QString &reason
    )
    {
        failedReports_.append({
            instanceId,
            componentId,
            packageDigest,
            QString::number(surfacePlanRevision),
            reason,
        });
    }

private:
    QDBusConnection connection_;
    SurfacePlanArtifact artifact_;
    QList<QDBusMessage> heldRequests_;
    QList<QStringList> stableReports_;
    QList<QStringList> failedReports_;
    QList<QDBusMessage> heldAuthorizations_;
    QList<QDBusMessage> heldRetries_;
    qsizetype cancellationCount_ = 0;
    bool holdAuthorization_ = false;
    bool holdRetries_ = false;
    QStringList healthComponentIds_;
    QStringList healthPackageDigests_;
    QStringList healthStates_;
    QStringList healthReasons_;
    QList<uint> healthFailureCounts_;
    qulonglong runtimeHealthRevision_ = 0;
    bool failHealthList_ = false;
    qsizetype healthListCount_ = 0;
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
        QTRY_VERIFY_WITH_TIMEOUT(client.runtimeHealthAvailable(), 3000);
        const auto acceptedRevision = client.planRevision();

        service_.setPlan(emptyPlan());
        QVERIFY(service_.publishCurrentPlan());
        QTRY_COMPARE_WITH_TIMEOUT(service_.heldRequestCount(), 1, 3000);
        QTRY_VERIFY_WITH_TIMEOUT(!client.planCurrent(), 3000);
        QTRY_VERIFY_WITH_TIMEOUT(!client.lastError().isEmpty(), 4500);
        QTRY_VERIFY_WITH_TIMEOUT(service_.heldRequestCount() >= 2, 4500);

        QCOMPARE(client.planRevision(), acceptedRevision);
        QVERIFY(client.runtimeHealthAvailable());
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

    void ownerLossDropsRetainedThirdPartyInstances()
    {
        service_.setPlan(declarativePlan());
        HyprShelld::ComponentRuntimeClient client(bus_, nullptr);
        QVERIFY(service_.start());
        QTRY_COMPARE_WITH_TIMEOUT(service_.heldRequestCount(), 1, 3000);
        QVERIFY(service_.replyNextPlan());
        QTRY_VERIFY_WITH_TIMEOUT(client.planCurrent(), 3000);
        QVERIFY(client.planRevision() > 9007199254740992ULL);
        QVERIFY(client.authorizeCurrentPlan());
        QTRY_COMPARE_WITH_TIMEOUT(
            client.barInstances(
                QStringLiteral("main"),
                QStringLiteral("DP-4"),
                QStringLiteral("end")
            ).size(),
            1,
            3000
        );
        QCOMPARE(
            client.barInstances(
                QStringLiteral("main"),
                QStringLiteral("DP-4"),
                QStringLiteral("end")
            ).size(),
            1
        );
        QVERIFY(!client.barInstances(
            QStringLiteral("main"),
            QStringLiteral("DP-4"),
            QStringLiteral("end")
        ).constFirst().toMap().contains(
            QStringLiteral("surfacePlanRevision")
        ));

        service_.stop();
        QTRY_VERIFY_WITH_TIMEOUT(!client.available(), 3000);
        QCOMPARE(client.planState(), QStringLiteral("retained"));
        QVERIFY(client.barInstances(
            QStringLiteral("main"),
            QStringLiteral("DP-4"),
            QStringLiteral("end")
        ).isEmpty());
    }

    void declarativeEntriesInInactiveLayoutsRemainInert()
    {
        auto plan = declarativePlan();
        const auto layout = plan.barLayouts.take(QStringLiteral("main"));
        plan.barLayouts.insert(QStringLiteral("secondary"), layout);
        service_.setPlan(plan);
        HyprShelld::ComponentRuntimeClient client(bus_, nullptr);
        QVERIFY(service_.start());
        QTRY_COMPARE_WITH_TIMEOUT(service_.heldRequestCount(), 1, 3000);
        QVERIFY(service_.replyNextPlan());
        QTRY_VERIFY_WITH_TIMEOUT(client.planCurrent(), 3000);
        QVERIFY(client.authorizeCurrentPlan());
        QTest::qWait(50);
        QVERIFY(client.barInstances(
            QStringLiteral("secondary"),
            QStringLiteral("DP-4"),
            QStringLiteral("end")
        ).isEmpty());
    }

    void delayedActivationCallbackCannotAffectNewPlanGeneration()
    {
        const auto instanceId = QStringLiteral(
            "11111111-1111-4111-8111-111111111111"
        );
        const auto componentId = QStringLiteral(
            "org.example.widgets.status"
        );
        const auto packageDigest = QString(64, QLatin1Char('e'));
        service_.setPlan(declarativePlan());
        HyprShelld::ComponentRuntimeClient client(bus_, nullptr);
        QVERIFY(service_.start());
        QTRY_COMPARE_WITH_TIMEOUT(service_.heldRequestCount(), 1, 3000);
        QVERIFY(service_.replyNextPlan());
        QTRY_VERIFY_WITH_TIMEOUT(client.planCurrent(), 3000);
        QVERIFY(client.authorizeCurrentPlan());
        QTRY_COMPARE_WITH_TIMEOUT(
            client.barInstances(
                QStringLiteral("main"),
                QStringLiteral("DP-4"),
                QStringLiteral("end")
            ).size(),
            1,
            3000
        );
        const auto planADigest = client.barInstances(
            QStringLiteral("main"),
            QStringLiteral("DP-4"),
            QStringLiteral("end")
        ).constFirst().toMap().value(
            QStringLiteral("surfacePlanDigest")
        ).toString();

        auto planB = declarativePlan();
        planB.configurationRevision = 6;
        planB.instances[instanceId].declarativeText =
            QStringLiteral("New trusted status");
        service_.setPlan(planB);
        QVERIFY(service_.publishCurrentPlan());
        QTRY_VERIFY_WITH_TIMEOUT(!client.planCurrent(), 3000);
        QVERIFY(client.barInstances(
            QStringLiteral("main"),
            QStringLiteral("DP-4"),
            QStringLiteral("end")
        ).isEmpty());
        QTRY_COMPARE_WITH_TIMEOUT(service_.heldRequestCount(), 1, 3000);
        QVERIFY(service_.replyNextPlan());
        QTRY_VERIFY_WITH_TIMEOUT(client.planCurrent(), 3000);
        QVERIFY(client.authorizeCurrentPlan());
        QTRY_COMPARE_WITH_TIMEOUT(
            client.barInstances(
                QStringLiteral("main"),
                QStringLiteral("DP-4"),
                QStringLiteral("end")
            ).size(),
            1,
            3000
        );
        const auto planBDigest = client.barInstances(
            QStringLiteral("main"),
            QStringLiteral("DP-4"),
            QStringLiteral("end")
        ).constFirst().toMap().value(
            QStringLiteral("surfacePlanDigest")
        ).toString();
        QVERIFY(planADigest != planBDigest);

        client.reportActivationStable(
            instanceId,
            componentId,
            packageDigest,
            planADigest
        );
        client.reportActivationFailed(
            instanceId,
            componentId,
            packageDigest,
            planADigest,
            QStringLiteral("render-failed")
        );
        QTest::qWait(50);
        QCOMPARE(service_.stableReportCount(), 0);
        QCOMPARE(service_.failedReportCount(), 0);

        client.reportActivationStable(
            instanceId,
            componentId,
            packageDigest,
            planBDigest
        );
        QTRY_COMPARE_WITH_TIMEOUT(service_.stableReportCount(), 1, 3000);
    }

    void inFlightAuthorizationCanBeCancelledWithoutReexposure()
    {
        service_.setPlan(declarativePlan());
        service_.holdAuthorization(true);
        HyprShelld::ComponentRuntimeClient client(bus_, nullptr);
        QVERIFY(service_.start());
        QTRY_COMPARE_WITH_TIMEOUT(service_.heldRequestCount(), 1, 3000);
        QVERIFY(service_.replyNextPlan());
        QTRY_VERIFY_WITH_TIMEOUT(client.planCurrent(), 3000);
        QVERIFY(client.authorizeCurrentPlan());
        QVERIFY(client.cancelCurrentPlanAuthorization());
        QTRY_COMPARE_WITH_TIMEOUT(service_.cancellationCount(), 1, 3000);
        QVERIFY(client.barInstances(
            QStringLiteral("main"),
            QStringLiteral("DP-4"),
            QStringLiteral("end")
        ).isEmpty());

        QVERIFY(service_.replyAuthorization());
        QTest::qWait(50);
        QVERIFY(client.barInstances(
            QStringLiteral("main"),
            QStringLiteral("DP-4"),
            QStringLiteral("end")
        ).isEmpty());
    }

    void oldOwnerRetryReplyCannotClearNewRequestBusyState()
    {
        const auto componentId = QStringLiteral(
            "org.example.widgets.status"
        );
        const auto packageDigest = QString(64, QLatin1Char('e'));
        service_.setPlan(workspacePlan());
        service_.setQuarantined(true);
        service_.holdRetries(true);
        HyprShelld::ComponentRuntimeClient client(bus_, nullptr);
        QVERIFY(service_.start());
        QTRY_COMPARE_WITH_TIMEOUT(service_.heldRequestCount(), 1, 3000);
        QVERIFY(service_.replyNextPlan());
        QTRY_VERIFY_WITH_TIMEOUT(client.runtimeHealthAvailable(), 3000);
        QVERIFY(client.retryComponent(componentId, packageDigest));
        QTRY_COMPARE_WITH_TIMEOUT(service_.heldRetryCount(), 1, 3000);
        QCOMPARE(client.runtimeRetryBusyComponentId(), componentId);

        service_.stop();
        QTRY_VERIFY_WITH_TIMEOUT(!client.available(), 3000);
        QVERIFY(client.runtimeRetryBusyComponentId().isEmpty());
        QVERIFY(service_.start());
        QTRY_VERIFY_WITH_TIMEOUT(client.available(), 3000);
        QTRY_VERIFY_WITH_TIMEOUT(client.runtimeHealthAvailable(), 3000);
        QVERIFY(client.retryComponent(componentId, packageDigest));
        QTRY_COMPARE_WITH_TIMEOUT(service_.heldRetryCount(), 2, 3000);

        QVERIFY(service_.replyNextRetry());
        QTest::qWait(50);
        QCOMPARE(client.runtimeRetryBusyComponentId(), componentId);
        QVERIFY(service_.replyNextRetry());
        QTRY_VERIFY_WITH_TIMEOUT(
            client.runtimeRetryBusyComponentId().isEmpty(),
            3000
        );
    }

    void malformedHealthRecordIsRejected_data()
    {
        QTest::addColumn<QString>("state");
        QTest::addColumn<QString>("reason");
        QTest::addColumn<uint>("failureCount");

        QTest::newRow("unknown-state")
            << QStringLiteral("healthy") << QString() << 0U;
        QTest::newRow("probation-reason")
            << QStringLiteral("probation") << QStringLiteral("timeout")
            << 0U;
        QTest::newRow("probation-count")
            << QStringLiteral("probation") << QString() << 1U;
        QTest::newRow("quarantine-empty-reason")
            << QStringLiteral("quarantined") << QString() << 1U;
        QTest::newRow("quarantine-unknown-reason")
            << QStringLiteral("quarantined") << QStringLiteral("unknown")
            << 1U;
        QTest::newRow("quarantine-zero-count")
            << QStringLiteral("quarantined") << QStringLiteral("timeout")
            << 0U;
        QTest::newRow("quarantine-excess-count")
            << QStringLiteral("quarantined") << QStringLiteral("timeout")
            << 1000001U;
    }

    void malformedHealthRecordIsRejected()
    {
        QFETCH(QString, state);
        QFETCH(QString, reason);
        QFETCH(uint, failureCount);

        service_.setRuntimeHealthRows(
            {QStringLiteral("org.example.widgets.status")},
            {QString(64, QLatin1Char('e'))},
            {state},
            {reason},
            {failureCount}
        );
        HyprShelld::ComponentRuntimeClient client(bus_, nullptr);
        QVERIFY(service_.start());
        QTRY_COMPARE_WITH_TIMEOUT(service_.heldRequestCount(), 1, 3000);
        QVERIFY(service_.replyNextPlan());
        QTRY_COMPARE_WITH_TIMEOUT(service_.healthListCount(), 1, 3000);
        QVERIFY(!client.runtimeHealthAvailable());
        QVERIFY(client.runtimeStates().isEmpty());
    }

    void probationHealthRecordRequiresEmptyReasonAndZeroCount()
    {
        service_.setRuntimeHealthRows(
            {QStringLiteral("org.example.widgets.status")},
            {QString(64, QLatin1Char('e'))},
            {QStringLiteral("probation")},
            {QString()},
            {0}
        );
        HyprShelld::ComponentRuntimeClient client(bus_, nullptr);
        QVERIFY(service_.start());
        QTRY_COMPARE_WITH_TIMEOUT(service_.heldRequestCount(), 1, 3000);
        QVERIFY(service_.replyNextPlan());
        QTRY_VERIFY_WITH_TIMEOUT(client.runtimeHealthAvailable(), 3000);
        QCOMPARE(client.runtimeStates().size(), 1);
        const auto record = client.runtimeStates().constFirst().toMap();
        QCOMPARE(record.value(QStringLiteral("state")).toString(),
            QStringLiteral("probation"));
        QVERIFY(record.value(QStringLiteral("reason")).toString().isEmpty());
        QCOMPARE(record.value(QStringLiteral("failureCount")).toUInt(), 0U);
    }

    void outOfOrderHealthRowsAreRejected()
    {
        service_.setRuntimeHealthRows(
            {
                QStringLiteral("org.example.widgets.zed"),
                QStringLiteral("org.example.widgets.alpha"),
            },
            {
                QString(64, QLatin1Char('e')),
                QString(64, QLatin1Char('f')),
            },
            {
                QStringLiteral("probation"),
                QStringLiteral("probation"),
            },
            {QString(), QString()},
            {0, 0}
        );
        HyprShelld::ComponentRuntimeClient client(bus_, nullptr);
        QVERIFY(service_.start());
        QTRY_COMPARE_WITH_TIMEOUT(service_.heldRequestCount(), 1, 3000);
        QVERIFY(service_.replyNextPlan());
        QTRY_COMPARE_WITH_TIMEOUT(service_.healthListCount(), 1, 3000);
        QVERIFY(!client.runtimeHealthAvailable());
        QVERIFY(client.runtimeStates().isEmpty());
    }

    void healthListFailureDoesNotSpinAndPropertyChangeRecovers()
    {
        service_.setPlan(workspacePlan());
        service_.failHealthList(true);
        HyprShelld::ComponentRuntimeClient client(bus_, nullptr);
        QVERIFY(service_.start());
        QTRY_COMPARE_WITH_TIMEOUT(service_.heldRequestCount(), 1, 3000);
        QVERIFY(service_.replyNextPlan());
        QTRY_COMPARE_WITH_TIMEOUT(service_.healthListCount(), 1, 3000);
        QVERIFY(!client.runtimeHealthAvailable());
        QTest::qWait(100);
        QCOMPARE(service_.healthListCount(), 1);

        service_.failHealthList(false);
        QVERIFY(service_.publishChanged({
            {
                QStringLiteral("RuntimeHealthRevision"),
                QVariant::fromValue<qulonglong>(0)
            },
        }));
        QTRY_COMPARE_WITH_TIMEOUT(service_.healthListCount(), 2, 3000);
        QTRY_VERIFY_WITH_TIMEOUT(client.runtimeHealthAvailable(), 3000);
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
