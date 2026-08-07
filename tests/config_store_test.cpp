#include "config_store.h"

#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QtTest>

using HyprShelld::ConfigPaths;
using HyprShelld::ConfigRecoveryState;
using HyprShelld::ConfigState;
using HyprShelld::ConfigStore;

namespace {

ConfigPaths pathsFor(const QString &root)
{
    return {
        .activeFile = root + QStringLiteral("/config/hyprshelld/settings.json"),
        .recoveryFile = root
            + QStringLiteral("/state/hyprshelld/settings.last-good.json"),
    };
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
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile file(path);
    return file.open(QIODevice::WriteOnly | QIODevice::Truncate)
        && file.write(data) == data.size();
}

} // namespace

class ConfigStoreTest final : public QObject {
    Q_OBJECT

private slots:
    void createsNormalDefaultsOnFirstRun()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const auto paths = pathsFor(directory.path());

        const ConfigStore store(paths);
        const auto loaded = store.load();

        QVERIFY2(loaded.success, qPrintable(loaded.error));
        QCOMPARE(loaded.state, ConfigState());
        QCOMPARE(loaded.recoveryState, ConfigRecoveryState::Normal);
        QVERIFY(QFileInfo::exists(paths.activeFile));
        QCOMPARE(readFile(paths.activeFile), readFile(paths.recoveryFile));
    }

    void persistsAndReloadsState()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const auto paths = pathsFor(directory.path());
        const ConfigStore store(paths);
        const auto loaded = store.load();
        QVERIFY2(loaded.success, qPrintable(loaded.error));

        const ConfigState next {.barHeight = 64, .revision = 1};
        QString error;
        QVERIFY2(store.persist(loaded.state, next, error), qPrintable(error));

        const auto reloaded = ConfigStore(paths).load();
        QVERIFY2(reloaded.success, qPrintable(reloaded.error));
        QCOMPARE(reloaded.state, next);
        QCOMPARE(reloaded.recoveryState, ConfigRecoveryState::Normal);
        QCOMPARE(readFile(paths.activeFile), readFile(paths.recoveryFile));
    }

    void recoversFromDamagedActiveState()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const auto paths = pathsFor(directory.path());
        const ConfigStore store(paths);
        const auto loaded = store.load();
        QVERIFY2(loaded.success, qPrintable(loaded.error));

        const ConfigState next {.barHeight = 72, .revision = 4};
        QString error;
        QVERIFY2(store.persist(loaded.state, next, error), qPrintable(error));
        QVERIFY(writeFile(paths.activeFile, "not json\n"));

        const auto recovered = ConfigStore(paths).load();
        QVERIFY2(recovered.success, qPrintable(recovered.error));
        QCOMPARE(recovered.state, next);
        QCOMPARE(recovered.recoveryState, ConfigRecoveryState::Recovered);
        QCOMPARE(readFile(paths.activeFile), readFile(paths.recoveryFile));
    }

    void replacesDamagedStateWithDefaults()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const auto paths = pathsFor(directory.path());
        QVERIFY(writeFile(paths.activeFile, "not json\n"));
        QVERIFY(writeFile(paths.recoveryFile, "also not json\n"));

        const auto defaulted = ConfigStore(paths).load();
        QVERIFY2(defaulted.success, qPrintable(defaulted.error));
        QCOMPARE(defaulted.state, ConfigState());
        QCOMPARE(defaulted.recoveryState, ConfigRecoveryState::Defaulted);
        QCOMPARE(readFile(paths.activeFile), readFile(paths.recoveryFile));

        const auto reloaded = ConfigStore(paths).load();
        QVERIFY2(reloaded.success, qPrintable(reloaded.error));
        QCOMPARE(reloaded.recoveryState, ConfigRecoveryState::Normal);
    }

    void preservesUnsupportedFutureState()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const auto paths = pathsFor(directory.path());
        const ConfigStore store(paths);
        const auto initial = store.load();
        QVERIFY2(initial.success, qPrintable(initial.error));

        const QByteArray future(
            "{\"formatVersion\":2,\"revision\":\"9\",\"barHeight\":80}\n"
        );
        QVERIFY(writeFile(paths.activeFile, future));

        const auto loaded = ConfigStore(paths).load();
        QVERIFY(!loaded.success);
        QVERIFY(loaded.error.contains(QStringLiteral("Unsupported format version")));
        QCOMPARE(readFile(paths.activeFile), future);
    }

    void preservesUnsupportedFutureRecoveryState()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const auto paths = pathsFor(directory.path());
        const ConfigStore store(paths);
        const auto initial = store.load();
        QVERIFY2(initial.success, qPrintable(initial.error));

        const auto active = readFile(paths.activeFile);
        const QByteArray future(
            "{\"formatVersion\":2,\"revision\":\"9\",\"barHeight\":80}\n"
        );
        QVERIFY(writeFile(paths.recoveryFile, future));

        const auto loaded = ConfigStore(paths).load();
        QVERIFY(!loaded.success);
        QVERIFY(loaded.error.contains(QStringLiteral("Unsupported format version")));
        QCOMPARE(readFile(paths.activeFile), active);
        QCOMPARE(readFile(paths.recoveryFile), future);
    }

    void defaultsSemanticallyDamagedState_data()
    {
        QTest::addColumn<QByteArray>("data");

        QTest::newRow("missing-height")
            << QByteArray("{\"formatVersion\":1,\"revision\":\"0\"}\n");
        QTest::newRow("fractional-version")
            << QByteArray(
                   "{\"formatVersion\":1.5,\"revision\":\"0\",\"barHeight\":48}\n"
               );
        QTest::newRow("fractional-height")
            << QByteArray(
                   "{\"formatVersion\":1,\"revision\":\"0\",\"barHeight\":48.5}\n"
               );
        QTest::newRow("leading-zero-revision")
            << QByteArray(
                   "{\"formatVersion\":1,\"revision\":\"01\",\"barHeight\":48}\n"
               );
        QTest::newRow("overflowing-revision")
            << QByteArray(
                   "{\"formatVersion\":1,\"revision\":\"18446744073709551616\",\"barHeight\":48}\n"
               );
        QTest::newRow("height-out-of-range")
            << QByteArray(
                   "{\"formatVersion\":1,\"revision\":\"0\",\"barHeight\":31}\n"
               );
    }

    void defaultsSemanticallyDamagedState()
    {
        QFETCH(QByteArray, data);

        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const auto paths = pathsFor(directory.path());
        QVERIFY(writeFile(paths.activeFile, data));

        const auto loaded = ConfigStore(paths).load();
        QVERIFY2(loaded.success, qPrintable(loaded.error));
        QCOMPARE(loaded.state, ConfigState());
        QCOMPARE(loaded.recoveryState, ConfigRecoveryState::Defaulted);
    }

    void failedWritePreservesActiveState()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const auto paths = pathsFor(directory.path());
        const ConfigStore store(paths);
        const auto loaded = store.load();
        QVERIFY2(loaded.success, qPrintable(loaded.error));

        const auto original = readFile(paths.activeFile);
        const auto configDirectory = QFileInfo(paths.activeFile).absolutePath();
        const auto heldDirectory = configDirectory + QStringLiteral(".held");
        QVERIFY(QDir().rename(configDirectory, heldDirectory));
        QFile blocker(configDirectory);
        QVERIFY(blocker.open(QIODevice::WriteOnly));
        blocker.close();

        QString error;
        const ConfigState next {.barHeight = 56, .revision = 1};
        const auto persisted = store.persist(loaded.state, next, error);

        QVERIFY(blocker.remove());
        QVERIFY(QDir().rename(heldDirectory, configDirectory));
        QVERIFY(!persisted);
        QVERIFY(!error.isEmpty());
        QCOMPARE(readFile(paths.activeFile), original);
    }
};

QTEST_APPLESS_MAIN(ConfigStoreTest)

#include "config_store_test.moc"
