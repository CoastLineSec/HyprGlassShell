#include "config1_interface.h"

#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusInterface>
#include <QDBusReply>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QProcessEnvironment>
#include <QTemporaryDir>
#include <QVariantMap>
#include <QXmlStreamReader>
#include <QtTest>

namespace {

const QString busName = QStringLiteral("org.hyprshelld.Config1");
const QString objectPath = QStringLiteral("/org/hyprshelld/Config1");
const QString interfaceName = QStringLiteral("org.hyprshelld.Config1");
const QString propertiesInterface = QStringLiteral(
    "org.freedesktop.DBus.Properties"
);

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
                insideTarget = attribute(xml, u"name") == interfaceName;
                if (insideTarget) {
                    description.append(QStringLiteral("interface=") + interfaceName);
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
            } else if (insideTarget && (element == u"method" || element == u"signal")) {
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
                && (element == u"property" || element == u"method" || element == u"signal")) {
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

QByteArray readFile(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }
    return file.readAll();
}

bool writeFile(const QString &path, const QByteArray &data)
{
    if (!QDir().mkpath(QFileInfo(path).absolutePath())) {
        return false;
    }

    QFile file(path);
    return file.open(QIODevice::WriteOnly | QIODevice::Truncate)
        && file.write(data) == data.size();
}

bool blockDirectory(const QString &path)
{
    if (!QDir().rename(path, path + QStringLiteral(".held"))) {
        return false;
    }

    QFile blocker(path);
    return blocker.open(QIODevice::WriteOnly);
}

bool restoreDirectory(const QString &path)
{
    return QFile::remove(path)
        && QDir().rename(path + QStringLiteral(".held"), path);
}

} // namespace

class ConfigdDbusTest final : public QObject {
    Q_OBJECT

private slots:
    void initTestCase()
    {
        executable_ = qEnvironmentVariable("HYPRSHELLD_CONFIGD_EXECUTABLE");
        contractPath_ = qEnvironmentVariable("HYPRSHELLD_CONFIG1_XML");
        QVERIFY(!executable_.isEmpty());
        QVERIFY(!contractPath_.isEmpty());
        QVERIFY2(bus_.isConnected(), qPrintable(bus_.lastError().message()));
    }

    void cleanup()
    {
        stopService();
        resetSignalCapture();
    }

    void exportsContractAndMutatesConfiguration()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        QVERIFY2(startService(directory.path()), qPrintable(processError_));

        OrgHyprshelldConfig1Interface proxy(busName, objectPath, bus_);
        QVERIFY(proxy.isValid());
        QCOMPARE(proxy.barHeight(), 40U);
        QCOMPARE(proxy.revision(), 0ULL);
        QCOMPARE(proxy.recoveryState(), QStringLiteral("normal"));

        QDBusInterface introspection(
            busName,
            objectPath,
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
        const auto liveDescription = describeInterface(liveXml.value(), liveError);
        QVERIFY2(sourceError.isEmpty(), qPrintable(sourceError));
        QVERIFY2(liveError.isEmpty(), qPrintable(liveError));
        QCOMPARE(liveDescription, sourceDescription);

        QVERIFY(connectPropertiesSignal());
        const auto originalBytes = readFile(activeFile_);
        QVERIFY(!originalBytes.isEmpty());

        for (const auto invalidHeight : {23U, 97U}) {
            auto invalid = proxy.SetBarHeight(invalidHeight);
            invalid.waitForFinished();
            QVERIFY(invalid.isError());
            QCOMPARE(
                invalid.error().name(),
                QStringLiteral("org.hyprshelld.Config1.Error.InvalidBarHeight")
            );
        }
        QTest::qWait(50);
        QCOMPARE(signalCount_, 0);
        QCOMPARE(proxy.barHeight(), 40U);
        QCOMPARE(proxy.revision(), 0ULL);
        QCOMPARE(readFile(activeFile_), originalBytes);

        auto changed = proxy.SetBarHeight(60);
        changed.waitForFinished();
        QVERIFY2(!changed.isError(), qPrintable(changed.error().message()));
        QCOMPARE(changed.value(), 1ULL);
        QTRY_COMPARE_WITH_TIMEOUT(signalCount_, 1, 1000);
        QCOMPARE(lastChanged_.value(QStringLiteral("BarHeight")).toUInt(), 60U);
        QCOMPARE(lastChanged_.value(QStringLiteral("Revision")).toULongLong(), 1ULL);
        QVERIFY(lastInvalidated_.isEmpty());
        QVERIFY(!bytesSeenAtSignal_.isEmpty());

        const auto persisted = QJsonDocument::fromJson(bytesSeenAtSignal_).object();
        QCOMPARE(persisted.value(QStringLiteral("barHeight")).toInteger(), 60);
        QCOMPARE(persisted.value(QStringLiteral("revision")).toString(), QStringLiteral("1"));
        QCOMPARE(proxy.barHeight(), 60U);
        QCOMPARE(proxy.revision(), 1ULL);

        const auto changedBytes = readFile(activeFile_);
        const auto changedRecoveryBytes = readFile(recoveryFile_);
        const auto configDirectory = QFileInfo(activeFile_).absolutePath();
        const auto stateDirectory = QFileInfo(recoveryFile_).absolutePath();
        QVERIFY(blockDirectory(configDirectory));
        QVERIFY(blockDirectory(stateDirectory));
        auto idempotent = proxy.SetBarHeight(60);
        idempotent.waitForFinished();
        QVERIFY(restoreDirectory(stateDirectory));
        QVERIFY(restoreDirectory(configDirectory));
        QVERIFY2(!idempotent.isError(), qPrintable(idempotent.error().message()));
        QCOMPARE(idempotent.value(), 1ULL);
        QTest::qWait(50);
        QCOMPARE(signalCount_, 1);
        QCOMPARE(readFile(activeFile_), changedBytes);
        QCOMPARE(readFile(recoveryFile_), changedRecoveryBytes);

        auto reset = proxy.ResetBarHeight();
        reset.waitForFinished();
        QVERIFY2(!reset.isError(), qPrintable(reset.error().message()));
        QCOMPARE(reset.value(), 2ULL);
        QTRY_COMPARE_WITH_TIMEOUT(signalCount_, 2, 1000);
        QCOMPARE(proxy.barHeight(), 40U);
        QCOMPARE(proxy.revision(), 2ULL);

        const auto resetBytes = readFile(activeFile_);
        const auto resetRecoveryBytes = readFile(recoveryFile_);
        QVERIFY(blockDirectory(configDirectory));
        QVERIFY(blockDirectory(stateDirectory));
        auto repeatedReset = proxy.ResetBarHeight();
        repeatedReset.waitForFinished();
        QVERIFY(restoreDirectory(stateDirectory));
        QVERIFY(restoreDirectory(configDirectory));
        QVERIFY2(
            !repeatedReset.isError(),
            qPrintable(repeatedReset.error().message())
        );
        QCOMPARE(repeatedReset.value(), 2ULL);
        QTest::qWait(50);
        QCOMPARE(signalCount_, 2);
        QCOMPARE(readFile(activeFile_), resetBytes);
        QCOMPARE(readFile(recoveryFile_), resetRecoveryBytes);
    }

    void reportsPersistenceFailureWithoutChangingState()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        QVERIFY2(startService(directory.path()), qPrintable(processError_));
        QVERIFY(connectPropertiesSignal());

        OrgHyprshelldConfig1Interface proxy(busName, objectPath, bus_);
        const auto originalBytes = readFile(activeFile_);
        const auto configDirectory = QFileInfo(activeFile_).absolutePath();
        QVERIFY(blockDirectory(configDirectory));

        auto failed = proxy.SetBarHeight(64);
        failed.waitForFinished();

        QVERIFY(restoreDirectory(configDirectory));
        QVERIFY(failed.isError());
        QCOMPARE(
            failed.error().name(),
            QStringLiteral("org.hyprshelld.Config1.Error.PersistenceFailed")
        );
        QTest::qWait(50);
        QCOMPARE(signalCount_, 0);
        QCOMPARE(proxy.barHeight(), 40U);
        QCOMPARE(proxy.revision(), 0ULL);
        QCOMPARE(readFile(activeFile_), originalBytes);
    }

    void acceptsInclusiveRangeBoundaries()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        QVERIFY2(startService(directory.path()), qPrintable(processError_));

        OrgHyprshelldConfig1Interface proxy(busName, objectPath, bus_);
        auto minimum = proxy.SetBarHeight(24);
        minimum.waitForFinished();
        QVERIFY2(!minimum.isError(), qPrintable(minimum.error().message()));
        QCOMPARE(minimum.value(), 1ULL);
        QCOMPARE(proxy.barHeight(), 24U);

        auto maximum = proxy.SetBarHeight(96);
        maximum.waitForFinished();
        QVERIFY2(!maximum.isError(), qPrintable(maximum.error().message()));
        QCOMPARE(maximum.value(), 2ULL);
        QCOMPARE(proxy.barHeight(), 96U);
    }

    void reportsRestartRecoveryStates()
    {
        QTemporaryDir recoveredDirectory;
        QVERIFY(recoveredDirectory.isValid());
        QVERIFY2(startService(recoveredDirectory.path()), qPrintable(processError_));

        {
            OrgHyprshelldConfig1Interface proxy(busName, objectPath, bus_);
            auto changed = proxy.SetBarHeight(72);
            changed.waitForFinished();
            QVERIFY2(!changed.isError(), qPrintable(changed.error().message()));
            QCOMPARE(changed.value(), 1ULL);
        }

        stopService();
        QVERIFY2(startService(recoveredDirectory.path()), qPrintable(processError_));
        {
            OrgHyprshelldConfig1Interface proxy(busName, objectPath, bus_);
            QCOMPARE(proxy.recoveryState(), QStringLiteral("normal"));
            QCOMPARE(proxy.barHeight(), 72U);
            QCOMPARE(proxy.revision(), 1ULL);
        }

        stopService();
        QVERIFY(writeFile(activeFile_, "not json\n"));
        QVERIFY2(startService(recoveredDirectory.path()), qPrintable(processError_));
        {
            OrgHyprshelldConfig1Interface proxy(busName, objectPath, bus_);
            QCOMPARE(proxy.recoveryState(), QStringLiteral("recovered"));
            QCOMPARE(proxy.barHeight(), 72U);
            QCOMPARE(proxy.revision(), 1ULL);
        }

        stopService();

        QTemporaryDir defaultedDirectory;
        QVERIFY(defaultedDirectory.isValid());
        const auto defaultedActive = defaultedDirectory.path()
            + QStringLiteral("/config/hyprshelld/settings.json");
        QVERIFY(writeFile(defaultedActive, "not json\n"));
        QVERIFY2(startService(defaultedDirectory.path()), qPrintable(processError_));

        OrgHyprshelldConfig1Interface defaulted(busName, objectPath, bus_);
        QCOMPARE(defaulted.recoveryState(), QStringLiteral("defaulted"));
        QCOMPARE(defaulted.barHeight(), 40U);
        QCOMPARE(defaulted.revision(), 0ULL);
    }

    void propertiesChanged(
        const QString &changedInterface,
        const QVariantMap &changed,
        const QStringList &invalidated
    )
    {
        if (changedInterface != interfaceName) {
            return;
        }

        ++signalCount_;
        lastChanged_ = changed;
        lastInvalidated_ = invalidated;
        bytesSeenAtSignal_ = readFile(activeFile_);
    }

private:
    bool startService(const QString &root)
    {
        stopService();
        resetSignalCapture();
        processError_.clear();

        const auto configHome = root + QStringLiteral("/config");
        const auto stateHome = root + QStringLiteral("/state");
        activeFile_ = configHome + QStringLiteral("/hyprshelld/settings.json");
        recoveryFile_ = stateHome
            + QStringLiteral("/hyprshelld/settings.last-good.json");

        auto environment = QProcessEnvironment::systemEnvironment();
        environment.insert(QStringLiteral("XDG_CONFIG_HOME"), configHome);
        environment.insert(QStringLiteral("XDG_STATE_HOME"), stateHome);

        process_.setProcessEnvironment(environment);
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
            if (process_.state() == QProcess::NotRunning) {
                processError_ = QString::fromUtf8(process_.readAll());
                return false;
            }

            const auto registered = bus_.interface()->isServiceRegistered(busName);
            if (registered.isValid() && registered.value()) {
                return true;
            }
            QTest::qWait(20);
        }

        processError_ = QStringLiteral("Timed out waiting for Config1 service");
        return false;
    }

    void stopService()
    {
        bus_.disconnect(
            busName,
            objectPath,
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
            const auto registered = bus_.interface()->isServiceRegistered(busName);
            if (!registered.isValid() || !registered.value()) {
                break;
            }
            QTest::qWait(10);
        }
    }

    bool connectPropertiesSignal()
    {
        return bus_.connect(
            busName,
            objectPath,
            propertiesInterface,
            QStringLiteral("PropertiesChanged"),
            this,
            SLOT(propertiesChanged(QString,QVariantMap,QStringList))
        );
    }

    void resetSignalCapture()
    {
        signalCount_ = 0;
        lastChanged_.clear();
        lastInvalidated_.clear();
        bytesSeenAtSignal_.clear();
    }

    QString executable_;
    QString contractPath_;
    QString activeFile_;
    QString recoveryFile_;
    QString processError_;
    QProcess process_;
    QDBusConnection bus_ = QDBusConnection::sessionBus();
    int signalCount_ = 0;
    QVariantMap lastChanged_;
    QStringList lastInvalidated_;
    QByteArray bytesSeenAtSignal_;
};

QTEST_GUILESS_MAIN(ConfigdDbusTest)

#include "configd_dbus_test.moc"
