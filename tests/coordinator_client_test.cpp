#include "coordinator_client.h"

#include <QDBusConnection>
#include <QDBusContext>
#include <QDBusMessage>
#include <QSignalSpy>
#include <QtTest>

#include <utility>

namespace {

const QString busName = QStringLiteral("org.hyprshelld.Coordinator1");
const QString objectPath = QStringLiteral("/org/hyprshelld/Coordinator1");
const QString interfaceName = QStringLiteral("org.hyprshelld.Coordinator1");
const QString propertiesInterface = QStringLiteral(
    "org.freedesktop.DBus.Properties"
);
const QString configUnit = QStringLiteral("hyprshelld-configd.service");
const QString surfacedUnit = QStringLiteral("hyprshelld-surfaced.service");

class FakeCoordinator final : public QObject, protected QDBusContext {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.hyprshelld.Coordinator1")
    Q_PROPERTY(bool Healthy READ healthy)
    Q_PROPERTY(QStringList FailedUnits READ failedUnits)
    Q_PROPERTY(QString FailureSummary READ failureSummary)

public:
    explicit FakeCoordinator(QDBusConnection connection)
        : connection_(std::move(connection))
    {
    }

    [[nodiscard]] bool healthy() const
    {
        return healthy_;
    }

    [[nodiscard]] QStringList failedUnits() const
    {
        return failedUnits_;
    }

    [[nodiscard]] QString failureSummary() const
    {
        return failureSummary_;
    }

    [[nodiscard]] int restartCount() const
    {
        return restartCount_;
    }

    void setState(
        bool healthy,
        QStringList failedUnits,
        QString summary,
        bool publish = true
    )
    {
        healthy_ = healthy;
        failedUnits_ = std::move(failedUnits);
        failureSummary_ = std::move(summary);

        if (publish && running_) {
            publishProperties();
        }
    }

    void setRestartFailure(bool enabled)
    {
        failNextRestart_ = enabled;
    }

    void holdNextRestart()
    {
        holdNextRestart_ = true;
    }

    [[nodiscard]] bool hasHeldRestart() const
    {
        return heldRestart_.type() != QDBusMessage::InvalidMessage;
    }

    bool start()
    {
        if (running_) {
            return true;
        }

        running_ = connection_.registerService(busName);
        return running_;
    }

    void stop()
    {
        if (!running_) {
            return;
        }

        connection_.unregisterService(busName);
        running_ = false;
    }

    bool publishInvalidated(const QStringList &properties)
    {
        auto signal = QDBusMessage::createSignal(
            objectPath,
            propertiesInterface,
            QStringLiteral("PropertiesChanged")
        );
        signal.setArguments({interfaceName, QVariantMap{}, properties});
        return connection_.send(signal);
    }

    bool publishChanged(const QVariantMap &properties)
    {
        auto signal = QDBusMessage::createSignal(
            objectPath,
            propertiesInterface,
            QStringLiteral("PropertiesChanged")
        );
        signal.setArguments({interfaceName, properties, QStringList{}});
        return connection_.send(signal);
    }

    bool finishHeldRestartWithError(
        const QString &name,
        const QString &message
    )
    {
        if (heldRestart_.type() == QDBusMessage::InvalidMessage) {
            return false;
        }

        const auto sent = connection_.send(
            heldRestart_.createErrorReply(name, message)
        );
        heldRestart_ = {};
        return sent;
    }

    void reset()
    {
        healthy_ = true;
        failedUnits_.clear();
        failureSummary_.clear();
        restartCount_ = 0;
        failNextRestart_ = false;
        holdNextRestart_ = false;
        heldRestart_ = {};
    }

public slots:
    void RestartComponent(const QString &unitName)
    {
        if (unitName != configUnit && unitName != surfacedUnit) {
            sendErrorReply(
                QStringLiteral(
                    "org.hyprshelld.Coordinator1.Error.UnknownComponent"
                ),
                QStringLiteral("The component is not managed by HyprShelld")
            );
            return;
        }

        if (!failedUnits_.contains(unitName)) {
            sendErrorReply(
                QStringLiteral(
                    "org.hyprshelld.Coordinator1.Error.ComponentNotFailed"
                ),
                QStringLiteral("The component is not persistently failed")
            );
            return;
        }

        if (failNextRestart_) {
            failNextRestart_ = false;
            sendErrorReply(
                QStringLiteral(
                    "org.hyprshelld.Coordinator1.Error.RestartFailed"
                ),
                QStringLiteral("systemd rejected the restart job")
            );
            return;
        }

        if (holdNextRestart_) {
            holdNextRestart_ = false;
            setDelayedReply(true);
            heldRestart_ = message();
            return;
        }

        ++restartCount_;
    }

private:
    void publishProperties()
    {
        QVariantMap properties;
        properties.insert(QStringLiteral("Healthy"), healthy_);
        properties.insert(QStringLiteral("FailedUnits"), failedUnits_);
        properties.insert(QStringLiteral("FailureSummary"), failureSummary_);

        publishChanged(properties);
    }

    QDBusConnection connection_;
    bool healthy_ = true;
    QStringList failedUnits_;
    QString failureSummary_;
    int restartCount_ = 0;
    bool failNextRestart_ = false;
    bool holdNextRestart_ = false;
    bool running_ = false;
    QDBusMessage heldRestart_;
};

class MalformedCoordinator final : public QObject {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.hyprshelld.Coordinator1")
    Q_PROPERTY(QString Healthy READ healthy)
    Q_PROPERTY(QStringList FailedUnits READ failedUnits)

public:
    [[nodiscard]] QString healthy() const
    {
        return QStringLiteral("true");
    }

    [[nodiscard]] QStringList failedUnits() const
    {
        return {};
    }
};

} // namespace

class CoordinatorClientTest final : public QObject {
    Q_OBJECT

private slots:
    void initTestCase()
    {
        QVERIFY2(bus_.isConnected(), qPrintable(bus_.lastError().message()));
        QVERIFY2(
            serviceBus_.isConnected(),
            qPrintable(serviceBus_.lastError().message())
        );
        QVERIFY(registerPrimaryObject());
    }

    void cleanup()
    {
        service_.stop();
        serviceBus_.unregisterService(busName);
        serviceBus_.unregisterObject(objectPath);
        service_.reset();
        QVERIFY(registerPrimaryObject());
        QCoreApplication::processEvents();
    }

    void cleanupTestCase()
    {
        serviceBus_.unregisterObject(objectPath);
        QDBusConnection::disconnectFromBus(
            QStringLiteral("coordinator-client-test")
        );
        QDBusConnection::disconnectFromBus(
            QStringLiteral("coordinator-service-test")
        );
    }

    void tracksPersistentFailuresAcrossServiceLifetime()
    {
        HyprShelld::CoordinatorClient client(bus_, nullptr);
        QCOMPARE(client.available(), false);
        QCOMPARE(client.busy(), false);
        QCOMPARE(client.healthy(), true);
        QCOMPARE(client.failedUnits(), QStringList{});
        QVERIFY(client.failureSummary().isEmpty());
        QVERIFY(client.lastErrorUnit().isEmpty());

        QSignalSpy addedSpy(
            &client,
            &HyprShelld::CoordinatorClient::persistentFailureAdded
        );
        QVERIFY(addedSpy.isValid());

        const auto initialSummary = QStringLiteral(
            "Configuration and desktop shell need attention"
        );
        service_.setState(
            false,
            {surfacedUnit, configUnit},
            initialSummary,
            false
        );
        QVERIFY(service_.start());

        QTRY_VERIFY_WITH_TIMEOUT(client.available(), 3000);
        QCOMPARE(client.healthy(), false);
        QCOMPARE(client.failedUnits(), QStringList({configUnit, surfacedUnit}));
        QCOMPARE(client.failureSummary(), initialSummary);
        QTRY_COMPARE_WITH_TIMEOUT(addedSpy.count(), 1, 3000);
        QCOMPARE(addedSpy.at(0).at(0).toString(), initialSummary);
        QCOMPARE(
            addedSpy.at(0).at(1).toStringList(),
            QStringList({configUnit, surfacedUnit})
        );

        service_.setState(
            false,
            {surfacedUnit},
            QStringLiteral("Desktop shell needs attention")
        );
        QTRY_COMPARE_WITH_TIMEOUT(
            client.failedUnits(),
            QStringList({surfacedUnit}),
            3000
        );
        QCOMPARE(addedSpy.count(), 1);

        service_.setState(
            false,
            {surfacedUnit, configUnit},
            initialSummary
        );
        QTRY_COMPARE_WITH_TIMEOUT(addedSpy.count(), 2, 3000);
        QCOMPARE(
            addedSpy.at(1).at(1).toStringList(),
            QStringList({configUnit})
        );

        service_.setState(true, {}, {}, false);
        QVERIFY(service_.publishInvalidated({
            QStringLiteral("Healthy"),
            QStringLiteral("FailedUnits"),
            QStringLiteral("FailureSummary")
        }));
        QTRY_VERIFY_WITH_TIMEOUT(client.healthy(), 3000);
        QTRY_COMPARE_WITH_TIMEOUT(client.failedUnits(), QStringList{}, 3000);
        QVERIFY(client.failureSummary().isEmpty());

        service_.setState(
            false,
            {configUnit},
            QStringLiteral("Configuration needs attention")
        );
        QTRY_COMPARE_WITH_TIMEOUT(
            client.failedUnits(),
            QStringList({configUnit}),
            3000
        );

        service_.stop();
        QTRY_VERIFY_WITH_TIMEOUT(!client.available(), 3000);
        QCOMPARE(client.healthy(), false);
        QCOMPARE(client.failedUnits(), QStringList({configUnit}));

        service_.setState(true, {}, {}, false);
        QVERIFY(service_.start());
        QTRY_VERIFY_WITH_TIMEOUT(client.available(), 3000);
        QTRY_VERIFY_WITH_TIMEOUT(client.healthy(), 3000);
        QCOMPARE(client.failedUnits(), QStringList{});
        QVERIFY(client.failureSummary().isEmpty());
    }

    void rejectsMalformedFullSnapshots_data()
    {
        QTest::addColumn<bool>("healthy");
        QTest::addColumn<QStringList>("failedUnits");
        QTest::addColumn<QString>("summary");

        QTest::newRow("duplicate-unit")
            << false
            << QStringList({configUnit, configUnit})
            << QStringLiteral("A component needs attention");
        QTest::newRow("unknown-unit")
            << false
            << QStringList({QStringLiteral("unrelated.service")})
            << QStringLiteral("A component needs attention");
        QTest::newRow("healthy-with-failure")
            << true
            << QStringList({configUnit})
            << QStringLiteral("A component needs attention");
        QTest::newRow("failure-without-summary")
            << false
            << QStringList({configUnit})
            << QString{};
        QTest::newRow("healthy-with-summary")
            << true
            << QStringList{}
            << QStringLiteral("Unexpected summary");
    }

    void rejectsMalformedFullSnapshots()
    {
        QFETCH(bool, healthy);
        QFETCH(QStringList, failedUnits);
        QFETCH(QString, summary);

        service_.setState(
            healthy,
            std::move(failedUnits),
            std::move(summary),
            false
        );
        QVERIFY(service_.start());

        HyprShelld::CoordinatorClient client(bus_, nullptr);
        QTest::qWait(150);
        QVERIFY(!client.available());
        QVERIFY(client.healthy());
        QCOMPARE(client.failedUnits(), QStringList{});
        QVERIFY(client.failureSummary().isEmpty());

        service_.setState(true, {}, {});
        QTRY_VERIFY_WITH_TIMEOUT(client.available(), 3000);
        QVERIFY(client.healthy());
        QCOMPARE(client.failedUnits(), QStringList{});
    }

    void rejectsIncompleteAndIncorrectlyTypedFullSnapshot()
    {
        serviceBus_.unregisterObject(objectPath);
        QVERIFY(serviceBus_.registerObject(
            objectPath,
            &malformedService_,
            QDBusConnection::ExportAllProperties
        ));
        QVERIFY(serviceBus_.registerService(busName));

        HyprShelld::CoordinatorClient client(bus_, nullptr);
        QTest::qWait(150);
        QVERIFY(!client.available());
        QVERIFY(client.healthy());
        QCOMPARE(client.failedUnits(), QStringList{});

        QVERIFY(serviceBus_.unregisterService(busName));
        serviceBus_.unregisterObject(objectPath);
        QVERIFY(registerPrimaryObject());
        service_.setState(true, {}, {}, false);
        QVERIFY(service_.start());
        QTRY_VERIFY_WITH_TIMEOUT(client.available(), 3000);
    }

    void rejectsMalformedPartialChangesAndRefreshes()
    {
        service_.setState(true, {}, {}, false);
        QVERIFY(service_.start());

        HyprShelld::CoordinatorClient client(bus_, nullptr);
        QTRY_VERIFY_WITH_TIMEOUT(client.available(), 3000);

        QList<bool> availabilityStates;
        connect(
            &client,
            &HyprShelld::CoordinatorClient::availableChanged,
            this,
            [&client, &availabilityStates] {
                availabilityStates.append(client.available());
            }
        );
        QSignalSpy addedSpy(
            &client,
            &HyprShelld::CoordinatorClient::persistentFailureAdded
        );

        const QList<QVariantMap> malformedChanges {
            {{QStringLiteral("Healthy"), QStringLiteral("true")}},
            {
                {QStringLiteral("Healthy"), false},
                {
                    QStringLiteral("FailedUnits"),
                    QStringList({configUnit, configUnit})
                },
                {
                    QStringLiteral("FailureSummary"),
                    QStringLiteral("A component needs attention")
                },
            },
            {
                {QStringLiteral("Healthy"), false},
                {
                    QStringLiteral("FailedUnits"),
                    QStringList({QStringLiteral("unrelated.service")})
                },
                {
                    QStringLiteral("FailureSummary"),
                    QStringLiteral("A component needs attention")
                },
            },
            {{QStringLiteral("FailedUnits"), QStringList({configUnit})}},
        };

        for (const auto &change : malformedChanges) {
            availabilityStates.clear();
            QVERIFY(service_.publishChanged(change));
            QTRY_VERIFY_WITH_TIMEOUT(availabilityStates.contains(false), 3000);
            QTRY_VERIFY_WITH_TIMEOUT(client.available(), 3000);
            QVERIFY(client.healthy());
            QCOMPARE(client.failedUnits(), QStringList{});
            QVERIFY(client.failureSummary().isEmpty());
            QCOMPARE(addedSpy.count(), 0);
        }

        service_.setState(
            false,
            {configUnit},
            QStringLiteral("A HyprShelld component needs attention.")
        );
        QTRY_VERIFY_WITH_TIMEOUT(!client.healthy(), 3000);
        QCOMPARE(client.failedUnits(), QStringList({configUnit}));
        QCOMPARE(addedSpy.count(), 1);
    }

    void restartsThroughCoordinatorAndPreservesTypedErrors()
    {
        service_.setState(
            false,
            {configUnit},
            QStringLiteral("Configuration needs attention"),
            false
        );
        QVERIFY(service_.start());

        HyprShelld::CoordinatorClient client(bus_, nullptr);
        QTRY_VERIFY_WITH_TIMEOUT(client.available(), 3000);
        QCOMPARE(client.failedUnits(), QStringList({configUnit}));

        client.restartComponent(configUnit);
        QVERIFY(client.busy());
        QCOMPARE(client.restartingUnit(), configUnit);
        QTRY_VERIFY_WITH_TIMEOUT(!client.busy(), 3000);
        QVERIFY(client.restartingUnit().isEmpty());
        QCOMPARE(service_.restartCount(), 1);
        QCOMPARE(client.failedUnits(), QStringList({configUnit}));
        QVERIFY(client.lastErrorName().isEmpty());
        QVERIFY(client.lastErrorUnit().isEmpty());

        client.restartComponent(surfacedUnit);
        QTRY_VERIFY_WITH_TIMEOUT(!client.busy(), 3000);
        QCOMPARE(
            client.lastErrorName(),
            QStringLiteral(
                "org.hyprshelld.Coordinator1.Error.ComponentNotFailed"
            )
        );
        QCOMPARE(client.lastErrorUnit(), surfacedUnit);
        QVERIFY(!client.lastErrorMessage().isEmpty());

        client.clearError();
        QVERIFY(client.lastErrorName().isEmpty());
        QVERIFY(client.lastErrorMessage().isEmpty());
        QVERIFY(client.lastErrorUnit().isEmpty());

        client.restartComponent(QStringLiteral("example.service"));
        QTRY_VERIFY_WITH_TIMEOUT(!client.busy(), 3000);
        QCOMPARE(
            client.lastErrorName(),
            QStringLiteral(
                "org.hyprshelld.Coordinator1.Error.UnknownComponent"
            )
        );
        QCOMPARE(client.lastErrorUnit(), QStringLiteral("example.service"));

        service_.setRestartFailure(true);
        client.restartComponent(configUnit);
        QTRY_VERIFY_WITH_TIMEOUT(!client.busy(), 3000);
        QCOMPARE(
            client.lastErrorName(),
            QStringLiteral(
                "org.hyprshelld.Coordinator1.Error.RestartFailed"
            )
        );
        QCOMPARE(client.lastErrorUnit(), configUnit);
        QCOMPARE(client.failedUnits(), QStringList({configUnit}));

        service_.setState(true, {}, {});
        QTRY_VERIFY_WITH_TIMEOUT(client.lastErrorUnit().isEmpty(), 3000);
        QVERIFY(client.lastErrorName().isEmpty());
        QVERIFY(client.lastErrorMessage().isEmpty());

        service_.setState(
            false,
            {configUnit},
            QStringLiteral("A HyprShelld component needs attention.")
        );
        QTRY_COMPARE_WITH_TIMEOUT(
            client.failedUnits(),
            QStringList({configUnit}),
            3000
        );
        service_.setRestartFailure(true);
        client.restartComponent(configUnit);
        QTRY_COMPARE_WITH_TIMEOUT(client.lastErrorUnit(), configUnit, 3000);
        service_.stop();
        QTRY_VERIFY_WITH_TIMEOUT(!client.available(), 3000);
        QVERIFY(client.lastErrorUnit().isEmpty());
        QVERIFY(client.lastErrorName().isEmpty());
    }

    void ignoresRestartReplyFromPreviousOwnerGeneration()
    {
        service_.setState(
            false,
            {configUnit},
            QStringLiteral("Configuration needs attention"),
            false
        );
        QVERIFY(service_.start());

        HyprShelld::CoordinatorClient client(bus_, nullptr);
        QTRY_VERIFY_WITH_TIMEOUT(client.available(), 3000);

        service_.holdNextRestart();
        client.restartComponent(configUnit);
        QVERIFY(client.busy());
        QTRY_VERIFY_WITH_TIMEOUT(service_.hasHeldRestart(), 3000);

        service_.stop();
        QTRY_VERIFY_WITH_TIMEOUT(!client.available(), 3000);
        service_.setState(true, {}, {}, false);
        QVERIFY(service_.start());
        QTRY_VERIFY_WITH_TIMEOUT(client.available(), 3000);
        QTRY_VERIFY_WITH_TIMEOUT(client.healthy(), 3000);

        QVERIFY(service_.finishHeldRestartWithError(
            QStringLiteral(
                "org.hyprshelld.Coordinator1.Error.RestartFailed"
            ),
            QStringLiteral("stale restart failure")
        ));
        QTRY_VERIFY_WITH_TIMEOUT(!client.busy(), 3000);
        QVERIFY(client.lastErrorName().isEmpty());
        QVERIFY(client.lastErrorMessage().isEmpty());
        QCOMPARE(client.failedUnits(), QStringList{});
    }

    void doesNotRetainRestartErrorAfterAuthoritativeRecovery()
    {
        service_.setState(
            false,
            {configUnit},
            QStringLiteral("A HyprShelld component needs attention."),
            false
        );
        QVERIFY(service_.start());

        HyprShelld::CoordinatorClient client(bus_, nullptr);
        QTRY_VERIFY_WITH_TIMEOUT(client.available(), 3000);
        QCOMPARE(client.failedUnits(), QStringList({configUnit}));

        QSignalSpy operationFailedSpy(
            &client,
            &HyprShelld::CoordinatorClient::operationFailed
        );
        service_.holdNextRestart();
        client.restartComponent(configUnit);
        QVERIFY(client.busy());
        QTRY_VERIFY_WITH_TIMEOUT(service_.hasHeldRestart(), 3000);

        service_.setState(true, {}, {});
        QTRY_VERIFY_WITH_TIMEOUT(client.healthy(), 3000);
        QCOMPARE(client.failedUnits(), QStringList{});

        QVERIFY(service_.finishHeldRestartWithError(
            QStringLiteral(
                "org.hyprshelld.Coordinator1.Error.ComponentNotFailed"
            ),
            QStringLiteral("the component recovered before restart")
        ));
        QTRY_VERIFY_WITH_TIMEOUT(!client.busy(), 3000);
        QVERIFY(client.lastErrorUnit().isEmpty());
        QVERIFY(client.lastErrorName().isEmpty());
        QVERIFY(client.lastErrorMessage().isEmpty());
        QCOMPARE(operationFailedSpy.count(), 0);

        service_.setState(
            false,
            {configUnit},
            QStringLiteral("A HyprShelld component needs attention.")
        );
        QTRY_COMPARE_WITH_TIMEOUT(
            client.failedUnits(),
            QStringList({configUnit}),
            3000
        );
        QVERIFY(client.lastErrorUnit().isEmpty());
        QVERIFY(client.lastErrorName().isEmpty());
        QVERIFY(client.lastErrorMessage().isEmpty());
    }

private:
    bool registerPrimaryObject()
    {
        return serviceBus_.registerObject(
            objectPath,
            &service_,
            QDBusConnection::ExportAllProperties
                | QDBusConnection::ExportAllSlots
        );
    }

    QDBusConnection bus_ = QDBusConnection::connectToBus(
        QDBusConnection::SessionBus,
        QStringLiteral("coordinator-client-test")
    );
    QDBusConnection serviceBus_ = QDBusConnection::connectToBus(
        QDBusConnection::SessionBus,
        QStringLiteral("coordinator-service-test")
    );
    FakeCoordinator service_{serviceBus_};
    MalformedCoordinator malformedService_;
};

QTEST_GUILESS_MAIN(CoordinatorClientTest)

#include "coordinator_client_test.moc"
