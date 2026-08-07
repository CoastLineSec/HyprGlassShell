#include "coordinator1_interface.h"

#include <QCoreApplication>
#include <QDBusArgument>
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusContext>
#include <QDBusInterface>
#include <QDBusMessage>
#include <QDBusObjectPath>
#include <QDBusReply>
#include <QElapsedTimer>
#include <QFile>
#include <QMap>
#include <QProcess>
#include <QProcessEnvironment>
#include <QSet>
#include <QVariantMap>
#include <QXmlStreamReader>
#include <QtTest>

#include <algorithm>

namespace {

const QString coordinatorBusName = QStringLiteral("org.hyprshelld.Coordinator1");
const QString coordinatorObjectPath = QStringLiteral("/org/hyprshelld/Coordinator1");
const QString coordinatorInterface = QStringLiteral("org.hyprshelld.Coordinator1");
const QString propertiesInterface = QStringLiteral("org.freedesktop.DBus.Properties");

const QString systemdBusName = QStringLiteral("org.freedesktop.systemd1");
const QString systemdManagerPath = QStringLiteral("/org/freedesktop/systemd1");
const QString systemdManagerInterface = QStringLiteral("org.freedesktop.systemd1.Manager");
const QString systemdUnitInterface = QStringLiteral("org.freedesktop.systemd1.Unit");

const QString configdUnit = QStringLiteral("hyprshelld-configd.service");
const QString surfacedUnit = QStringLiteral("hyprshelld-surfaced.service");

const QString unknownComponentError = QStringLiteral(
    "org.hyprshelld.Coordinator1.Error.UnknownComponent"
);
const QString componentNotFailedError = QStringLiteral(
    "org.hyprshelld.Coordinator1.Error.ComponentNotFailed"
);
const QString restartFailedError = QStringLiteral(
    "org.hyprshelld.Coordinator1.Error.RestartFailed"
);

struct SystemdUnitRecord {
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

using SystemdUnitRecordList = QList<SystemdUnitRecord>;

QDBusArgument &operator<<(QDBusArgument &argument, const SystemdUnitRecord &record)
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
    SystemdUnitRecord &record
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

QString unitPathForName(const QString &unitName)
{
    if (unitName == configdUnit) {
        return QStringLiteral(
            "/org/freedesktop/systemd1/unit/hyprshelld_2dconfigd_2eservice"
        );
    }

    if (unitName == surfacedUnit) {
        return QStringLiteral(
            "/org/freedesktop/systemd1/unit/hyprshelld_2dsurfaced_2eservice"
        );
    }

    return QStringLiteral("/org/freedesktop/systemd1/unit/not_2dfound");
}

QString subStateForActiveState(const QString &activeState)
{
    if (activeState == QStringLiteral("active")) {
        return QStringLiteral("running");
    }
    if (activeState == QStringLiteral("failed")) {
        return QStringLiteral("failed");
    }
    if (activeState == QStringLiteral("activating")) {
        return QStringLiteral("start");
    }
    return QStringLiteral("dead");
}

QString attribute(const QXmlStreamReader &xml, const char16_t *name)
{
    return xml.attributes().value(QStringView(name)).toString();
}

QStringList describeInterface(QXmlStreamReader &xml, QString &error)
{
    QStringList description;
    QString memberKind;
    QString memberName;
    bool insideTarget = false;

    while (!xml.atEnd()) {
        xml.readNext();

        if (xml.isStartElement()) {
            const auto element = xml.name();
            if (element == u"interface") {
                insideTarget = attribute(xml, u"name") == coordinatorInterface;
                if (insideTarget) {
                    description.append(
                        QStringLiteral("interface=") + coordinatorInterface
                    );
                }
            } else if (insideTarget && element == u"property") {
                memberKind = QStringLiteral("property");
                memberName = attribute(xml, u"name");
                description.append(
                    QStringLiteral("property=%1:%2:%3")
                        .arg(
                            memberName,
                            attribute(xml, u"type"),
                            attribute(xml, u"access")
                        )
                );
            } else if (insideTarget
                       && (element == u"method" || element == u"signal")) {
                memberKind = element.toString();
                memberName = attribute(xml, u"name");
                description.append(
                    QStringLiteral("%1=%2").arg(memberKind, memberName)
                );
            } else if (insideTarget && element == u"arg") {
                description.append(
                    QStringLiteral("arg=%1:%2:%3:%4:%5")
                        .arg(
                            memberKind,
                            memberName,
                            attribute(xml, u"name"),
                            attribute(xml, u"type"),
                            attribute(xml, u"direction")
                        )
                );
            } else if (insideTarget && element == u"annotation") {
                description.append(
                    QStringLiteral("annotation=%1:%2:%3:%4")
                        .arg(
                            memberKind,
                            memberName,
                            attribute(xml, u"name"),
                            attribute(xml, u"value")
                        )
                );
            }
        } else if (xml.isEndElement()) {
            const auto element = xml.name();
            if (insideTarget
                && (element == u"property"
                    || element == u"method"
                    || element == u"signal")) {
                memberKind.clear();
                memberName.clear();
            } else if (element == u"interface" && insideTarget) {
                insideTarget = false;
            }
        }
    }

    if (xml.hasError()) {
        error = xml.errorString();
        return {};
    }

    description.sort();
    return description;
}

QStringList describeInterface(const QString &xmlText, QString &error)
{
    QXmlStreamReader xml(xmlText);
    return describeInterface(xml, error);
}

QStringList describeInterfaceFile(const QString &path, QString &error)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        error = file.errorString();
        return {};
    }

    QXmlStreamReader xml(&file);
    return describeInterface(xml, error);
}

QStringList sorted(QStringList values)
{
    values.sort();
    return values;
}

} // namespace

Q_DECLARE_METATYPE(SystemdUnitRecord)
Q_DECLARE_METATYPE(SystemdUnitRecordList)

class FakeSystemdManager final : public QObject, protected QDBusContext {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.freedesktop.systemd1.Manager")

public:
    explicit FakeSystemdManager(const QDBusConnection &bus, QObject *parent = nullptr)
        : QObject(parent)
        , bus_(bus)
    {
        reset();
    }

    void reset()
    {
        activeStates_.clear();
        activeStates_.insert(configdUnit, QStringLiteral("active"));
        activeStates_.insert(surfacedUnit, QStringLiteral("active"));
        listRequests_.clear();
        callLog_.clear();
        listCallCount_ = 0;
        subscribeCallCount_ = 0;
        raceUnit_.clear();
        raceState_.clear();
        lifecycleChurn_ = false;
        failReset_ = false;
        failRestart_ = false;
        malformedRestartReply_ = false;
    }

    void armHydrationRace(const QString &unitName, const QString &activeState)
    {
        raceUnit_ = unitName;
        raceState_ = activeState;
    }

    void setLifecycleChurn(bool enabled)
    {
        lifecycleChurn_ = enabled;
    }

    void setActiveState(const QString &unitName, const QString &activeState)
    {
        Q_ASSERT(activeStates_.contains(unitName));
        activeStates_.insert(unitName, activeState);
        announceUnitChange(unitName);
    }

    void setResetFailure(bool enabled)
    {
        failReset_ = enabled;
    }

    void setRestartFailure(bool enabled)
    {
        failRestart_ = enabled;
    }

    void setMalformedRestartReply(bool enabled)
    {
        malformedRestartReply_ = enabled;
    }

    int listCallCount() const
    {
        return listCallCount_;
    }

    int subscribeCallCount() const
    {
        return subscribeCallCount_;
    }

    QList<QStringList> listRequests() const
    {
        return listRequests_;
    }

    QStringList callLog() const
    {
        return callLog_;
    }

    void clearCallLog()
    {
        callLog_.clear();
    }

public slots:
    void Subscribe()
    {
        ++subscribeCallCount_;
    }

    SystemdUnitRecordList ListUnitsByNames(const QStringList &unitNames)
    {
        ++listCallCount_;
        listRequests_.append(unitNames);

        auto records = recordsForNames(unitNames);
        if (!raceUnit_.isEmpty()) {
            const auto unitName = raceUnit_;
            const auto activeState = raceState_;
            raceUnit_.clear();
            raceState_.clear();

            activeStates_.insert(unitName, activeState);
            announceUnitChange(unitName);
        }

        if (lifecycleChurn_) {
            for (const auto &unitName : unitNames) {
                const QDBusObjectPath path(unitPathForName(unitName));
                emit UnitNew(unitName, path);
                emit UnitRemoved(unitName, path);
            }
        }

        return records;
    }

    void ResetFailedUnit(const QString &unitName)
    {
        callLog_.append(QStringLiteral("reset:%1").arg(unitName));
        if (failReset_) {
            sendErrorReply(
                QStringLiteral("org.freedesktop.DBus.Error.Failed"),
                QStringLiteral("Injected ResetFailedUnit failure")
            );
            return;
        }

        activeStates_.insert(unitName, QStringLiteral("inactive"));
        announceUnitChange(unitName);
    }

    QDBusObjectPath RestartUnit(const QString &unitName, const QString &mode)
    {
        callLog_.append(
            QStringLiteral("restart:%1:%2").arg(unitName, mode)
        );
        if (failRestart_) {
            sendErrorReply(
                QStringLiteral("org.freedesktop.DBus.Error.Failed"),
                QStringLiteral("Injected RestartUnit failure")
            );
            return QDBusObjectPath(QStringLiteral("/"));
        }

        if (malformedRestartReply_) {
            return QDBusObjectPath(QStringLiteral("/"));
        }

        activeStates_.insert(unitName, QStringLiteral("activating"));
        announceUnitChange(unitName);
        return QDBusObjectPath(
            QStringLiteral("/org/freedesktop/systemd1/job/1")
        );
    }

signals:
    void UnitNew(const QString &unitName, const QDBusObjectPath &unitPath);
    void UnitRemoved(const QString &unitName, const QDBusObjectPath &unitPath);
    void JobRemoved(
        quint32 jobId,
        const QDBusObjectPath &jobPath,
        const QString &unitName,
        const QString &result
    );

private:
    SystemdUnitRecordList recordsForNames(const QStringList &unitNames) const
    {
        SystemdUnitRecordList records;
        for (auto iterator = unitNames.crbegin(); iterator != unitNames.crend(); ++iterator) {
            const auto &unitName = *iterator;
            const auto known = activeStates_.contains(unitName);
            const auto activeState = known
                ? activeStates_.value(unitName)
                : QStringLiteral("inactive");

            records.append({
                .name = unitName,
                .description = known
                    ? QStringLiteral("Test unit %1").arg(unitName)
                    : QString(),
                .loadState = known
                    ? QStringLiteral("loaded")
                    : QStringLiteral("not-found"),
                .activeState = activeState,
                .subState = subStateForActiveState(activeState),
                .following = QString(),
                .unitPath = QDBusObjectPath(unitPathForName(unitName)),
                .jobId = 0,
                .jobType = QString(),
                .jobPath = QDBusObjectPath(QStringLiteral("/")),
            });
        }
        return records;
    }

    void announceUnitChange(const QString &unitName)
    {
        const QDBusObjectPath unitPath(unitPathForName(unitName));

        QVariantMap changed;
        changed.insert(
            QStringLiteral("ActiveState"),
            activeStates_.value(unitName)
        );

        QDBusMessage propertiesChanged = QDBusMessage::createSignal(
            unitPath.path(),
            propertiesInterface,
            QStringLiteral("PropertiesChanged")
        );
        propertiesChanged.setArguments({
            QVariant::fromValue(systemdUnitInterface),
            QVariant::fromValue(changed),
            QVariant::fromValue(QStringList()),
        });
        bus_.send(propertiesChanged);
    }

    QDBusConnection bus_;
    QMap<QString, QString> activeStates_;
    QList<QStringList> listRequests_;
    QStringList callLog_;
    int listCallCount_ = 0;
    int subscribeCallCount_ = 0;
    QString raceUnit_;
    QString raceState_;
    bool lifecycleChurn_ = false;
    bool failReset_ = false;
    bool failRestart_ = false;
    bool malformedRestartReply_ = false;
};

class CoordinatorDbusTest final : public QObject {
    Q_OBJECT

private slots:
    void initTestCase()
    {
        qDBusRegisterMetaType<SystemdUnitRecord>();
        qDBusRegisterMetaType<SystemdUnitRecordList>();

        executable_ = qEnvironmentVariable("HYPRSHELLD_COORDINATOR_EXECUTABLE");
        contractPath_ = qEnvironmentVariable("HYPRSHELLD_COORDINATOR1_XML");
        QVERIFY(!executable_.isEmpty());
        QVERIFY(!contractPath_.isEmpty());
        QVERIFY2(bus_.isConnected(), qPrintable(bus_.lastError().message()));

        QVERIFY2(
            bus_.registerObject(
                systemdManagerPath,
                &fakeSystemd_,
                QDBusConnection::ExportAllSlots
                    | QDBusConnection::ExportAllSignals
            ),
            qPrintable(bus_.lastError().message())
        );
        QVERIFY2(
            bus_.registerService(systemdBusName),
            qPrintable(bus_.lastError().message())
        );
    }

    void cleanupTestCase()
    {
        stopCoordinator();
        bus_.unregisterService(systemdBusName);
        bus_.unregisterObject(systemdManagerPath);
    }

    void cleanup()
    {
        stopCoordinator();
        fakeSystemd_.reset();
        resetSignalCapture();
    }

    void fakeSystemdManagerSurfaceIsUsable()
    {
        QDBusInterface manager(
            systemdBusName,
            systemdManagerPath,
            systemdManagerInterface,
            bus_
        );
        QVERIFY(manager.isValid());

        QDBusPendingReply<> subscribed = manager.asyncCall(
            QStringLiteral("Subscribe")
        );
        QTRY_VERIFY_WITH_TIMEOUT(subscribed.isFinished(), 1000);
        QVERIFY2(!subscribed.isError(), qPrintable(subscribed.error().message()));

        QDBusPendingReply<SystemdUnitRecordList> listed = manager.asyncCall(
            QStringLiteral("ListUnitsByNames"),
            QStringList({configdUnit, surfacedUnit})
        );
        QTRY_VERIFY_WITH_TIMEOUT(listed.isFinished(), 1000);
        QVERIFY2(!listed.isError(), qPrintable(listed.error().message()));
        QCOMPARE(listed.value().size(), 2);
        QCOMPARE(listed.value().at(0).name, surfacedUnit);
        QCOMPARE(listed.value().at(1).name, configdUnit);
        QCOMPARE(listed.value().at(0).activeState, QStringLiteral("active"));
    }

    void hydrationRacePublishesLatestSnapshotAndExactContract()
    {
        fakeSystemd_.armHydrationRace(
            surfacedUnit,
            QStringLiteral("failed")
        );
        QVERIFY2(startCoordinator(), qPrintable(processError_));

        OrgHyprshelldCoordinator1Interface proxy(
            coordinatorBusName,
            coordinatorObjectPath,
            bus_
        );
        QVERIFY(proxy.isValid());

        QTRY_COMPARE_WITH_TIMEOUT(
            proxy.failedUnits(),
            QStringList({surfacedUnit}),
            2000
        );
        QVERIFY(!proxy.healthy());
        QCOMPARE(
            proxy.failureSummary(),
            QStringLiteral("A HyprShelld component needs attention.")
        );
        QVERIFY(fakeSystemd_.subscribeCallCount() >= 1);
        QVERIFY(fakeSystemd_.listCallCount() >= 2);

        const auto expectedUnits = sorted({configdUnit, surfacedUnit});
        for (auto request : fakeSystemd_.listRequests()) {
            QCOMPARE(sorted(request), expectedUnits);
        }

        QDBusInterface introspection(
            coordinatorBusName,
            coordinatorObjectPath,
            QStringLiteral("org.freedesktop.DBus.Introspectable"),
            bus_
        );
        const QDBusReply<QString> liveXml = introspection.call(
            QStringLiteral("Introspect")
        );
        QVERIFY2(liveXml.isValid(), qPrintable(liveXml.error().message()));

        QString sourceError;
        QString liveError;
        const auto sourceDescription = describeInterfaceFile(
            contractPath_,
            sourceError
        );
        const auto liveDescription = describeInterface(
            liveXml.value(),
            liveError
        );
        QVERIFY2(sourceError.isEmpty(), qPrintable(sourceError));
        QVERIFY2(liveError.isEmpty(), qPrintable(liveError));
        QCOMPARE(liveDescription, sourceDescription);
    }

    void hydratesThroughSamePathUnitLifecycleChurn()
    {
        fakeSystemd_.setLifecycleChurn(true);
        QVERIFY2(startCoordinator(), qPrintable(processError_));

        OrgHyprshelldCoordinator1Interface proxy(
            coordinatorBusName,
            coordinatorObjectPath,
            bus_
        );
        QVERIFY(proxy.isValid());
        QVERIFY(proxy.healthy());
        QCOMPARE(proxy.failedUnits(), QStringList());
        QVERIFY(fakeSystemd_.listCallCount() <= 3);
    }

    void latchesFailuresAndEmitsAtomicPropertyChanges()
    {
        QVERIFY2(startCoordinator(), qPrintable(processError_));
        QVERIFY(connectPropertiesSignal());

        OrgHyprshelldCoordinator1Interface proxy(
            coordinatorBusName,
            coordinatorObjectPath,
            bus_
        );
        QVERIFY(proxy.isValid());
        QVERIFY(proxy.healthy());
        QCOMPARE(proxy.failedUnits(), QStringList());
        QCOMPARE(proxy.failureSummary(), QString());

        auto beforeLists = fakeSystemd_.listCallCount();
        fakeSystemd_.setActiveState(configdUnit, QStringLiteral("activating"));
        QTRY_VERIFY_WITH_TIMEOUT(
            fakeSystemd_.listCallCount() > beforeLists,
            1000
        );
        QTest::qWait(30);
        QCOMPARE(changedHistory_.size(), 0);
        QVERIFY(proxy.healthy());

        fakeSystemd_.setActiveState(configdUnit, QStringLiteral("failed"));
        QTRY_COMPARE_WITH_TIMEOUT(changedHistory_.size(), 1, 1000);
        assertAtomicChange(
            changedHistory_.at(0),
            false,
            {configdUnit},
            QStringLiteral("A HyprShelld component needs attention.")
        );
        QCOMPARE(invalidatedHistory_.at(0), QStringList());
        QVERIFY(!proxy.healthy());
        QCOMPARE(proxy.failedUnits(), QStringList({configdUnit}));

        beforeLists = fakeSystemd_.listCallCount();
        fakeSystemd_.setActiveState(configdUnit, QStringLiteral("inactive"));
        QTRY_VERIFY_WITH_TIMEOUT(
            fakeSystemd_.listCallCount() > beforeLists,
            1000
        );
        QTest::qWait(30);
        QCOMPARE(changedHistory_.size(), 1);
        QCOMPARE(proxy.failedUnits(), QStringList({configdUnit}));

        fakeSystemd_.setActiveState(surfacedUnit, QStringLiteral("failed"));
        QTRY_COMPARE_WITH_TIMEOUT(changedHistory_.size(), 2, 1000);
        assertAtomicChange(
            changedHistory_.at(1),
            false,
            {configdUnit, surfacedUnit},
            QStringLiteral("2 HyprShelld components need attention.")
        );

        beforeLists = fakeSystemd_.listCallCount();
        fakeSystemd_.setActiveState(surfacedUnit, QStringLiteral("failed"));
        QTRY_VERIFY_WITH_TIMEOUT(
            fakeSystemd_.listCallCount() > beforeLists,
            1000
        );
        QTest::qWait(30);
        QCOMPARE(changedHistory_.size(), 2);

        fakeSystemd_.setActiveState(configdUnit, QStringLiteral("active"));
        QTRY_COMPARE_WITH_TIMEOUT(changedHistory_.size(), 3, 1000);
        assertAtomicChange(
            changedHistory_.at(2),
            false,
            {surfacedUnit},
            QStringLiteral("A HyprShelld component needs attention.")
        );

        fakeSystemd_.setActiveState(surfacedUnit, QStringLiteral("active"));
        QTRY_COMPARE_WITH_TIMEOUT(changedHistory_.size(), 4, 1000);
        assertAtomicChange(changedHistory_.at(3), true, {}, QString());
        QVERIFY(proxy.healthy());
        QCOMPARE(proxy.failedUnits(), QStringList());
        QCOMPARE(proxy.failureSummary(), QString());
    }

    void boundsRestartRequestsAndMapsSystemdErrors()
    {
        QVERIFY2(startCoordinator(), qPrintable(processError_));
        QVERIFY(connectPropertiesSignal());

        OrgHyprshelldCoordinator1Interface proxy(
            coordinatorBusName,
            coordinatorObjectPath,
            bus_
        );
        QVERIFY(proxy.isValid());

        auto unknown = proxy.RestartComponent(
            QStringLiteral("unrelated.service")
        );
        QTRY_VERIFY_WITH_TIMEOUT(unknown.isFinished(), 1000);
        QVERIFY(unknown.isError());
        QCOMPARE(unknown.error().name(), unknownComponentError);
        QCOMPARE(fakeSystemd_.callLog(), QStringList());

        auto healthy = proxy.RestartComponent(configdUnit);
        QTRY_VERIFY_WITH_TIMEOUT(healthy.isFinished(), 1000);
        QVERIFY(healthy.isError());
        QCOMPARE(healthy.error().name(), componentNotFailedError);
        QCOMPARE(fakeSystemd_.callLog(), QStringList());

        fakeSystemd_.setActiveState(configdUnit, QStringLiteral("failed"));
        QTRY_COMPARE_WITH_TIMEOUT(changedHistory_.size(), 1, 1000);
        fakeSystemd_.clearCallLog();

        auto restarted = proxy.RestartComponent(configdUnit);
        QTRY_VERIFY_WITH_TIMEOUT(restarted.isFinished(), 2000);
        QVERIFY2(!restarted.isError(), qPrintable(restarted.error().message()));
        QCOMPARE(
            fakeSystemd_.callLog(),
            QStringList({
                QStringLiteral("reset:%1").arg(configdUnit),
                QStringLiteral("restart:%1:replace").arg(configdUnit),
            })
        );
        QTest::qWait(50);
        QCOMPARE(changedHistory_.size(), 1);
        QCOMPARE(proxy.failedUnits(), QStringList({configdUnit}));

        fakeSystemd_.setActiveState(configdUnit, QStringLiteral("active"));
        QTRY_COMPARE_WITH_TIMEOUT(changedHistory_.size(), 2, 1000);
        QVERIFY(proxy.healthy());

        fakeSystemd_.setActiveState(configdUnit, QStringLiteral("failed"));
        QTRY_COMPARE_WITH_TIMEOUT(changedHistory_.size(), 3, 1000);
        fakeSystemd_.clearCallLog();
        fakeSystemd_.setResetFailure(true);

        auto resetFailed = proxy.RestartComponent(configdUnit);
        QTRY_VERIFY_WITH_TIMEOUT(resetFailed.isFinished(), 2000);
        QVERIFY(resetFailed.isError());
        QCOMPARE(resetFailed.error().name(), restartFailedError);
        QCOMPARE(
            fakeSystemd_.callLog(),
            QStringList({QStringLiteral("reset:%1").arg(configdUnit)})
        );
        QCOMPARE(proxy.failedUnits(), QStringList({configdUnit}));

        fakeSystemd_.clearCallLog();
        fakeSystemd_.setResetFailure(false);
        fakeSystemd_.setRestartFailure(true);

        auto restartFailed = proxy.RestartComponent(configdUnit);
        QTRY_VERIFY_WITH_TIMEOUT(restartFailed.isFinished(), 2000);
        QVERIFY(restartFailed.isError());
        QCOMPARE(restartFailed.error().name(), restartFailedError);
        QCOMPARE(
            fakeSystemd_.callLog(),
            QStringList({
                QStringLiteral("reset:%1").arg(configdUnit),
                QStringLiteral("restart:%1:replace").arg(configdUnit),
            })
        );
        QTest::qWait(50);
        QCOMPARE(changedHistory_.size(), 3);
        QCOMPARE(proxy.failedUnits(), QStringList({configdUnit}));

        fakeSystemd_.clearCallLog();
        fakeSystemd_.setRestartFailure(false);
        fakeSystemd_.setMalformedRestartReply(true);

        auto malformedRestart = proxy.RestartComponent(configdUnit);
        QTRY_VERIFY_WITH_TIMEOUT(malformedRestart.isFinished(), 2000);
        QVERIFY(malformedRestart.isError());
        QCOMPARE(malformedRestart.error().name(), restartFailedError);
        QCOMPARE(
            fakeSystemd_.callLog(),
            QStringList({
                QStringLiteral("reset:%1").arg(configdUnit),
                QStringLiteral("restart:%1:replace").arg(configdUnit),
            })
        );
        QCOMPARE(proxy.failedUnits(), QStringList({configdUnit}));
    }

    void propertiesChanged(
        const QString &changedInterface,
        const QVariantMap &changed,
        const QStringList &invalidated
    )
    {
        if (changedInterface != coordinatorInterface) {
            return;
        }

        changedHistory_.append(changed);
        invalidatedHistory_.append(invalidated);
    }

private:
    void assertAtomicChange(
        const QVariantMap &changed,
        bool healthy,
        const QStringList &failedUnits,
        const QString &summary
    )
    {
        QCOMPARE(
            sorted(changed.keys()),
            sorted({
                QStringLiteral("FailedUnits"),
                QStringLiteral("FailureSummary"),
                QStringLiteral("Healthy"),
            })
        );
        QCOMPARE(changed.value(QStringLiteral("Healthy")).toBool(), healthy);
        QCOMPARE(
            changed.value(QStringLiteral("FailedUnits")).toStringList(),
            failedUnits
        );
        QCOMPARE(
            changed.value(QStringLiteral("FailureSummary")).toString(),
            summary
        );
    }

    bool startCoordinator()
    {
        stopCoordinator();
        resetSignalCapture();
        processError_.clear();

        process_.setProcessEnvironment(QProcessEnvironment::systemEnvironment());
        process_.setProgram(executable_);
        process_.setProcessChannelMode(QProcess::MergedChannels);
        process_.start();
        if (!process_.waitForStarted(3000)) {
            processError_ = process_.errorString();
            return false;
        }

        QElapsedTimer timer;
        timer.start();
        while (timer.elapsed() < 5000) {
            QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
            if (process_.state() == QProcess::NotRunning) {
                processError_ = QString::fromUtf8(process_.readAll());
                return false;
            }

            const auto registered = bus_.interface()->isServiceRegistered(
                coordinatorBusName
            );
            if (registered.isValid() && registered.value()) {
                return true;
            }
            QTest::qWait(10);
        }

        processError_ = QStringLiteral(
            "Timed out waiting for Coordinator1 service. Output: %1"
        ).arg(QString::fromUtf8(process_.readAll()));
        return false;
    }

    void stopCoordinator()
    {
        bus_.disconnect(
            coordinatorBusName,
            coordinatorObjectPath,
            propertiesInterface,
            QStringLiteral("PropertiesChanged"),
            this,
            SLOT(propertiesChanged(QString,QVariantMap,QStringList))
        );

        if (process_.state() != QProcess::NotRunning) {
            process_.terminate();
            if (!process_.waitForFinished(3000)) {
                process_.kill();
                process_.waitForFinished(3000);
            }
        }

        QElapsedTimer timer;
        timer.start();
        while (timer.elapsed() < 1000) {
            const auto registered = bus_.interface()->isServiceRegistered(
                coordinatorBusName
            );
            if (!registered.isValid() || !registered.value()) {
                break;
            }
            QTest::qWait(10);
        }
    }

    bool connectPropertiesSignal()
    {
        return bus_.connect(
            coordinatorBusName,
            coordinatorObjectPath,
            propertiesInterface,
            QStringLiteral("PropertiesChanged"),
            this,
            SLOT(propertiesChanged(QString,QVariantMap,QStringList))
        );
    }

    void resetSignalCapture()
    {
        changedHistory_.clear();
        invalidatedHistory_.clear();
    }

    QString executable_;
    QString contractPath_;
    QString processError_;
    QProcess process_;
    QDBusConnection bus_ = QDBusConnection::sessionBus();
    FakeSystemdManager fakeSystemd_{bus_};
    QList<QVariantMap> changedHistory_;
    QList<QStringList> invalidatedHistory_;
};

QTEST_GUILESS_MAIN(CoordinatorDbusTest)

#include "coordinator_dbus_test.moc"
