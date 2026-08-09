#include "shell_runtime_status.h"

#include <QDBusArgument>
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusContext>
#include <QDBusMessage>
#include <QDBusMetaType>
#include <QDBusObjectPath>
#include <QTest>
#include <QVariant>

#include <utility>

namespace {

const QString systemdService = QStringLiteral("org.freedesktop.systemd1");
const QString systemdManagerPath = QStringLiteral("/org/freedesktop/systemd1");

const QString targetUnit = QStringLiteral("hyprshelld.target");
const QString coordinatorUnit = QStringLiteral("hyprshelld.service");
const QString configurationUnit = QStringLiteral("hyprshelld-configd.service");
const QString componentUnit = QStringLiteral("hyprshelld-componentd.service");
const QString surfaceUnit = QStringLiteral("hyprshelld-surfaced.service");

const QStringList expectedUnits {
    targetUnit,
    coordinatorUnit,
    configurationUnit,
    componentUnit,
    surfaceUnit,
};

struct TestSystemdUnitRecord {
    QString name;
    QString description;
    QString loadState;
    QString activeState;
    QString subState;
    QString following;
    QDBusObjectPath unitPath;
    quint32 jobId = 0;
    QString jobType;
    QDBusObjectPath jobPath;
};

using TestSystemdUnitRecords = QList<TestSystemdUnitRecord>;

QDBusArgument &operator<<(
    QDBusArgument &argument,
    const TestSystemdUnitRecord &record
)
{
    argument.beginStructure();
    argument << record.name
             << record.description
             << record.loadState
             << record.activeState
             << record.subState
             << record.following
             << record.unitPath
             << record.jobId
             << record.jobType
             << record.jobPath;
    argument.endStructure();
    return argument;
}

const QDBusArgument &operator>>(
    const QDBusArgument &argument,
    TestSystemdUnitRecord &record
)
{
    argument.beginStructure();
    argument >> record.name
             >> record.description
             >> record.loadState
             >> record.activeState
             >> record.subState
             >> record.following
             >> record.unitPath
             >> record.jobId
             >> record.jobType
             >> record.jobPath;
    argument.endStructure();
    return argument;
}

TestSystemdUnitRecord unitRecord(const QString &name, const QString &state)
{
    auto objectName = name;
    objectName.replace(QLatin1Char('-'), QStringLiteral("_2d"));
    objectName.replace(QLatin1Char('.'), QStringLiteral("_2e"));

    return {
        .name = name,
        .description = name,
        .loadState = QStringLiteral("loaded"),
        .activeState = state,
        .subState = state == QStringLiteral("active")
            ? QStringLiteral("running")
            : QStringLiteral("dead"),
        .following = {},
        .unitPath = QDBusObjectPath(
            QStringLiteral("/org/freedesktop/systemd1/unit/%1").arg(objectName)
        ),
        .jobId = 0,
        .jobType = {},
        .jobPath = QDBusObjectPath(QStringLiteral("/")),
    };
}

TestSystemdUnitRecords completeSnapshot()
{
    return {
        unitRecord(targetUnit, QStringLiteral("active")),
        unitRecord(coordinatorUnit, QStringLiteral("active")),
        unitRecord(configurationUnit, QStringLiteral("inactive")),
        unitRecord(componentUnit, QStringLiteral("active")),
        unitRecord(surfaceUnit, QStringLiteral("failed")),
    };
}

} // namespace

Q_DECLARE_METATYPE(TestSystemdUnitRecord)
Q_DECLARE_METATYPE(TestSystemdUnitRecords)

namespace {

class FakeSystemdManager final : public QObject, protected QDBusContext {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.freedesktop.systemd1.Manager")

public:
    explicit FakeSystemdManager(QDBusConnection connection)
        : connection_(std::move(connection))
    {
        reset();
    }

    void reset()
    {
        records_ = completeSnapshot();
        requests_.clear();
        pendingRequests_.clear();
        delayed_ = false;
    }

    void setRecords(TestSystemdUnitRecords records)
    {
        records_ = std::move(records);
    }

    void setDelayed(bool delayed)
    {
        delayed_ = delayed;
    }

    [[nodiscard]] const QList<QStringList> &requests() const
    {
        return requests_;
    }

    [[nodiscard]] int pendingCount() const
    {
        return pendingRequests_.size();
    }

    void replyToPending()
    {
        QVERIFY(!pendingRequests_.isEmpty());
        const auto request = pendingRequests_.takeFirst();
        const QVariantList arguments {QVariant::fromValue(records_)};
        QVERIFY(connection_.send(request.createReply(arguments)));
    }

public slots:
    TestSystemdUnitRecords ListUnitsByNames(const QStringList &units)
    {
        requests_.append(units);
        if (delayed_) {
            setDelayedReply(true);
            pendingRequests_.append(message());
            return {};
        }
        return records_;
    }

private:
    QDBusConnection connection_;
    TestSystemdUnitRecords records_;
    QList<QStringList> requests_;
    QList<QDBusMessage> pendingRequests_;
    bool delayed_ = false;
};

class ShellRuntimeStatusTest final : public QObject {
    Q_OBJECT

private slots:
    void initTestCase()
    {
        qDBusRegisterMetaType<TestSystemdUnitRecord>();
        qDBusRegisterMetaType<TestSystemdUnitRecords>();
        QVERIFY2(bus_.isConnected(), qPrintable(bus_.lastError().message()));
        QVERIFY2(
            serviceBus_.isConnected(),
            qPrintable(serviceBus_.lastError().message())
        );
        QVERIFY(serviceBus_.registerService(systemdService));
        QVERIFY(serviceBus_.registerObject(
            systemdManagerPath,
            &manager_,
            QDBusConnection::ExportAllSlots
        ));
    }

    void cleanupTestCase()
    {
        QDBusConnection::disconnectFromBus(
            QStringLiteral("shell-runtime-status-client-test")
        );
        QDBusConnection::disconnectFromBus(
            QStringLiteral("shell-runtime-status-service-test")
        );
    }

    void cleanup()
    {
        while (manager_.pendingCount() > 0) {
            manager_.replyToPending();
        }
        manager_.reset();
        if (!bus_.interface()->isServiceRegistered(systemdService).value()) {
            QVERIFY(serviceBus_.registerService(systemdService));
        }
    }

    void defaultsAndExplicitRefresh()
    {
        HyprShelld::ShellRuntimeStatus status(bus_, nullptr);

        QVERIFY(!status.active());
        QVERIFY(!status.available());
        QVERIFY(!status.busy());
        QCOMPARE(status.targetState(), QStringLiteral("unknown"));
        QCOMPARE(status.coordinatorState(), QStringLiteral("unknown"));
        QCOMPARE(status.configurationState(), QStringLiteral("unknown"));
        QCOMPARE(status.componentManagerState(), QStringLiteral("unknown"));
        QCOMPARE(status.surfaceState(), QStringLiteral("unknown"));
        QVERIFY(status.lastErrorName().isEmpty());
        QCOMPARE(manager_.requests().size(), 0);

        status.refresh();
        QVERIFY(!status.busy());
        QCOMPARE(manager_.requests().size(), 0);

        status.setActive(true);
        QVERIFY(status.busy());
        QTRY_VERIFY_WITH_TIMEOUT(status.available(), 1000);
        QVERIFY(!status.busy());
        QCOMPARE(manager_.requests(), QList<QStringList>({expectedUnits}));
        QCOMPARE(status.targetState(), QStringLiteral("active"));
        QCOMPARE(status.coordinatorState(), QStringLiteral("active"));
        QCOMPARE(status.configurationState(), QStringLiteral("inactive"));
        QCOMPARE(status.componentManagerState(), QStringLiteral("active"));
        QCOMPARE(status.surfaceState(), QStringLiteral("failed"));
        QVERIFY(status.lastErrorName().isEmpty());
        QVERIFY(status.lastErrorMessage().isEmpty());
    }

    void activePollingStopsWhenInactive()
    {
        HyprShelld::ShellRuntimeStatus status(bus_, nullptr);

        status.setActive(true);
        QVERIFY(status.active());
        QTRY_VERIFY_WITH_TIMEOUT(status.available(), 1000);
        const auto initialRequests = manager_.requests().size();

        auto updated = completeSnapshot();
        updated[2].activeState = QStringLiteral("failed");
        updated[3].activeState = QStringLiteral("failed");
        updated[4].activeState = QStringLiteral("active");
        manager_.setRecords(std::move(updated));
        QTRY_VERIFY_WITH_TIMEOUT(
            manager_.requests().size() > initialRequests,
            2500
        );
        QTRY_COMPARE_WITH_TIMEOUT(
            status.configurationState(),
            QStringLiteral("failed"),
            1000
        );
        QCOMPARE(status.componentManagerState(), QStringLiteral("failed"));
        QCOMPARE(status.surfaceState(), QStringLiteral("active"));
        QTRY_VERIFY_WITH_TIMEOUT(!status.busy(), 1000);

        status.setActive(false);
        QVERIFY(!status.active());
        QVERIFY(!status.available());
        QCOMPARE(status.configurationState(), QStringLiteral("failed"));
        QCOMPARE(status.componentManagerState(), QStringLiteral("failed"));
        QCOMPARE(status.surfaceState(), QStringLiteral("active"));

        manager_.setRecords(completeSnapshot());
        status.setActive(true);
        QTRY_VERIFY_WITH_TIMEOUT(status.available(), 1000);
        QCOMPARE(status.configurationState(), QStringLiteral("inactive"));
        QCOMPARE(status.componentManagerState(), QStringLiteral("active"));
        QCOMPARE(status.surfaceState(), QStringLiteral("failed"));

        status.setActive(false);
        const auto stoppedRequestCount = manager_.requests().size();
        QTest::qWait(2100);
        QCOMPARE(manager_.requests().size(), stoppedRequestCount);
    }

    void reportsLoadFailureInsteadOfInactiveState()
    {
        auto records = completeSnapshot();
        records[2].loadState = QStringLiteral("not-found");
        records[2].activeState = QStringLiteral("inactive");
        records[3].loadState = QStringLiteral("masked");
        records[3].activeState = QStringLiteral("inactive");
        records[4].loadState = QStringLiteral("masked");
        records[4].activeState = QStringLiteral("inactive");
        manager_.setRecords(std::move(records));

        HyprShelld::ShellRuntimeStatus status(bus_, nullptr);
        status.setActive(true);
        QTRY_VERIFY_WITH_TIMEOUT(status.available(), 1000);

        QCOMPARE(status.configurationState(), QStringLiteral("not-found"));
        QCOMPARE(status.componentManagerState(), QStringLiteral("masked"));
        QCOMPARE(status.surfaceState(), QStringLiteral("masked"));
    }

    void preservesSnapshotOnInvalidReply_data()
    {
        QTest::addColumn<TestSystemdUnitRecords>("records");
        QTest::addColumn<QString>("messageFragment");

        auto incomplete = completeSnapshot();
        incomplete.removeLast();
        QTest::newRow("incomplete")
            << incomplete
            << QStringLiteral("incomplete");

        auto duplicate = completeSnapshot();
        duplicate[3] = duplicate[2];
        QTest::newRow("duplicate")
            << duplicate
            << QStringLiteral("duplicate");

        auto unexpected = completeSnapshot();
        unexpected[3] = unitRecord(
            QStringLiteral("unrelated.service"),
            QStringLiteral("active")
        );
        QTest::newRow("unexpected")
            << unexpected
            << QStringLiteral("unexpected");

        auto emptyActiveState = completeSnapshot();
        emptyActiveState[2].activeState.clear();
        QTest::newRow("empty-active-state")
            << emptyActiveState
            << QStringLiteral("empty unit state");

        auto emptyLoadState = completeSnapshot();
        emptyLoadState[2].loadState.clear();
        QTest::newRow("empty-load-state")
            << emptyLoadState
            << QStringLiteral("empty unit state");
    }

    void preservesSnapshotOnInvalidReply()
    {
        QFETCH(TestSystemdUnitRecords, records);
        QFETCH(QString, messageFragment);

        HyprShelld::ShellRuntimeStatus status(bus_, nullptr);
        status.setActive(true);
        QTRY_VERIFY_WITH_TIMEOUT(status.available(), 1000);

        manager_.setRecords(std::move(records));
        status.refresh();
        QTRY_VERIFY_WITH_TIMEOUT(!status.busy(), 1000);

        QVERIFY(!status.available());
        QCOMPARE(
            status.lastErrorName(),
            QStringLiteral("org.hyprshelld.Client.Error.InvalidSystemdSnapshot")
        );
        QVERIFY(status.lastErrorMessage().contains(messageFragment));
        QCOMPARE(status.targetState(), QStringLiteral("active"));
        QCOMPARE(status.coordinatorState(), QStringLiteral("active"));
        QCOMPARE(status.configurationState(), QStringLiteral("inactive"));
        QCOMPARE(status.componentManagerState(), QStringLiteral("active"));
        QCOMPARE(status.surfaceState(), QStringLiteral("failed"));

        manager_.setRecords(completeSnapshot());
        status.refresh();
        QTRY_VERIFY_WITH_TIMEOUT(status.available(), 1000);
        QVERIFY(status.lastErrorName().isEmpty());
        QVERIFY(status.lastErrorMessage().isEmpty());
    }

    void preservesSnapshotOnTransportFailure()
    {
        HyprShelld::ShellRuntimeStatus status(bus_, nullptr);
        status.setActive(true);
        QTRY_VERIFY_WITH_TIMEOUT(status.available(), 1000);

        QVERIFY(serviceBus_.unregisterService(systemdService));
        status.refresh();
        QTRY_VERIFY_WITH_TIMEOUT(!status.busy(), 1000);

        QVERIFY(!status.available());
        QVERIFY(!status.lastErrorName().isEmpty());
        QVERIFY(!status.lastErrorMessage().isEmpty());
        QCOMPARE(status.targetState(), QStringLiteral("active"));
        QCOMPARE(status.componentManagerState(), QStringLiteral("active"));
        QCOMPARE(status.surfaceState(), QStringLiteral("failed"));
    }

    void deactivationInvalidatesPendingReply()
    {
        manager_.setDelayed(true);
        HyprShelld::ShellRuntimeStatus status(bus_, nullptr);

        status.setActive(true);
        QTRY_COMPARE_WITH_TIMEOUT(manager_.pendingCount(), 1, 1000);
        QVERIFY(status.busy());

        status.setActive(false);
        QVERIFY(!status.busy());
        manager_.replyToPending();
        QTest::qWait(100);

        QVERIFY(!status.available());
        QCOMPARE(status.targetState(), QStringLiteral("unknown"));
        QCOMPARE(status.componentManagerState(), QStringLiteral("unknown"));
        QCOMPARE(status.surfaceState(), QStringLiteral("unknown"));
    }

    void refreshDoesNotOverlapPendingCall()
    {
        manager_.setDelayed(true);
        HyprShelld::ShellRuntimeStatus status(bus_, nullptr);

        status.setActive(true);
        QTRY_COMPARE_WITH_TIMEOUT(manager_.pendingCount(), 1, 1000);
        status.refresh();
        status.refresh();
        QCOMPARE(manager_.requests().size(), 1);

        manager_.replyToPending();
        QTRY_VERIFY_WITH_TIMEOUT(status.available(), 1000);
        QVERIFY(!status.busy());
    }

private:
    QDBusConnection bus_ = QDBusConnection::connectToBus(
        QDBusConnection::SessionBus,
        QStringLiteral("shell-runtime-status-client-test")
    );
    QDBusConnection serviceBus_ = QDBusConnection::connectToBus(
        QDBusConnection::SessionBus,
        QStringLiteral("shell-runtime-status-service-test")
    );
    FakeSystemdManager manager_ {serviceBus_};
};

} // namespace

QTEST_GUILESS_MAIN(ShellRuntimeStatusTest)

#include "shell_runtime_status_test.moc"
