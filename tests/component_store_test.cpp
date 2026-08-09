#include "component_config_test_fixture.h"

#include <QFile>
#include <QFileInfo>
#include <QElapsedTimer>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QtTest>

#include <sys/stat.h>
#include <unistd.h>

using namespace HyprShelld;

namespace {

QString defaultsFile()
{
    return QStringLiteral(HYPRSHELLD_COMPONENT_DEFAULTS_FILE);
}

Components::ConfigurationCatalog catalog()
{
    return Tests::configurationCatalog(
        defaultsFile(),
        QStringLiteral(HYPRSHELLD_WORKSPACE_SCHEMA_FILE)
    );
}

bool createFifo(const QString &path)
{
    if (!QDir().mkpath(QFileInfo(path).absolutePath())) {
        return false;
    }
    const auto encoded = QFile::encodeName(path);
    return ::mkfifo(encoded.constData(), 0600) == 0;
}

bool createSymlink(const QString &target, const QString &path)
{
    if (!QDir().mkpath(QFileInfo(path).absolutePath())) {
        return false;
    }
    const auto encodedTarget = QFile::encodeName(target);
    const auto encodedPath = QFile::encodeName(path);
    return ::symlink(encodedTarget.constData(), encodedPath.constData()) == 0;
}

} // namespace

class ComponentStoreTest final : public QObject {
    Q_OBJECT

private slots:
    void createsExactFirstRunDefaults()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const auto paths = Tests::componentPathsFor(
            directory.path(), defaultsFile()
        );
        const auto loaded = ComponentStore(paths).load(catalog());
        QVERIFY2(loaded.available, qPrintable(loaded.error));
        QVERIFY(loaded.writable);
        QCOMPARE(loaded.loadState, ComponentLoadState::Normal);
        QCOMPARE(
            Tests::readBytes(paths.activeFile),
            Tests::readBytes(paths.recoveryFile)
        );
        QCOMPARE(loaded.state.revision, quint64(0));
        QCOMPARE(loaded.state.components.size(), 1);
        QCOMPARE(loaded.state.instances.size(), 1);
        QCOMPARE(loaded.state.bars.first().outputs.mode, QStringLiteral("all"));
        QCOMPARE(
            loaded.state.bars.first().start,
            QStringList{QString::fromLatin1(
                Components::workspaceSwitcherDefaultInstanceId
            )}
        );
    }

    void recoversDamagedActiveFromLastGood()
    {
        QTemporaryDir directory;
        const auto paths = Tests::componentPathsFor(
            directory.path(), defaultsFile()
        );
        const ComponentStore store(paths);
        const auto initial = store.load(catalog());
        QVERIFY(initial.available);
        auto next = initial.state;
        next.revision = 4;
        next.instances.first().settings.insert(
            QStringLiteral("occupiedOnly"), true
        );
        QString error;
        QVERIFY2(store.persist(initial.state, next, error), qPrintable(error));
        QVERIFY(Tests::writeBytes(paths.activeFile, QByteArrayLiteral("broken\n")));

        const auto loaded = ComponentStore(paths).load(catalog());
        QVERIFY2(loaded.available, qPrintable(loaded.error));
        QVERIFY(loaded.writable);
        QCOMPARE(loaded.loadState, ComponentLoadState::Recovered);
        QCOMPARE(loaded.state, next);
        QCOMPARE(
            Tests::readBytes(paths.activeFile),
            Tests::readBytes(paths.recoveryFile)
        );
    }

    void preservesUnsupportedActiveBytes()
    {
        QTemporaryDir directory;
        const auto paths = Tests::componentPathsFor(
            directory.path(), defaultsFile()
        );
        QVERIFY(ComponentStore(paths).load(catalog()).available);
        const QByteArray future(
            "{\"formatVersion\":2,\"revision\":\"8\","
            "\"components\":{},\"instances\":{},"
            "\"layouts\":{\"bars\":{},\"desktops\":{}}}\n"
        );
        QVERIFY(Tests::writeBytes(paths.activeFile, future));
        const auto loaded = ComponentStore(paths).load(catalog());
        QVERIFY(!loaded.available);
        QVERIFY(!loaded.writable);
        QCOMPARE(loaded.loadState, ComponentLoadState::Unsupported);
        QCOMPARE(Tests::readBytes(paths.activeFile), future);
    }

    void validActiveAndFutureRecoveryIsReadableOnly()
    {
        QTemporaryDir directory;
        const auto paths = Tests::componentPathsFor(
            directory.path(), defaultsFile()
        );
        const auto initial = ComponentStore(paths).load(catalog());
        QVERIFY(initial.available);
        const auto active = Tests::readBytes(paths.activeFile);
        const QByteArray future(
            "{\"formatVersion\":2,\"revision\":\"8\","
            "\"components\":{},\"instances\":{},"
            "\"layouts\":{\"bars\":{},\"desktops\":{}}}\n"
        );
        QVERIFY(Tests::writeBytes(paths.recoveryFile, future));
        const auto loaded = ComponentStore(paths).load(catalog());
        QVERIFY(loaded.available);
        QVERIFY(!loaded.writable);
        QCOMPARE(loaded.loadState, ComponentLoadState::Unsupported);
        QCOMPARE(Tests::readBytes(paths.activeFile), active);
        QCOMPARE(Tests::readBytes(paths.recoveryFile), future);
    }

    void validActiveRepairsOrdinaryRecoveryDamage_data()
    {
        QTest::addColumn<QString>("damage");
        QTest::newRow("missing") << QStringLiteral("missing");
        QTest::newRow("damaged") << QStringLiteral("damaged");
        QTest::newRow("mismatched") << QStringLiteral("mismatched");
    }

    void validActiveRepairsOrdinaryRecoveryDamage()
    {
        QFETCH(QString, damage);
        QTemporaryDir directory;
        const auto paths = Tests::componentPathsFor(
            directory.path(), defaultsFile()
        );
        const auto initial = ComponentStore(paths).load(catalog());
        QVERIFY(initial.available);
        const auto active = Tests::readBytes(paths.activeFile);
        if (damage == QStringLiteral("missing")) {
            QVERIFY(QFile::remove(paths.recoveryFile));
        } else if (damage == QStringLiteral("damaged")) {
            QVERIFY(Tests::writeBytes(paths.recoveryFile, QByteArrayLiteral("bad\n")));
        } else {
            auto other = initial.state;
            other.revision = 9;
            QVERIFY(Tests::writeBytes(
                paths.recoveryFile,
                Components::serializeComponentConfiguration(other)
            ));
        }
        const auto loaded = ComponentStore(paths).load(catalog());
        QVERIFY2(loaded.available, qPrintable(loaded.error));
        QVERIFY(loaded.writable);
        QCOMPARE(loaded.loadState, ComponentLoadState::Normal);
        QCOMPARE(Tests::readBytes(paths.activeFile), active);
        QCOMPARE(Tests::readBytes(paths.recoveryFile), active);
    }

    void boundsPersistedFileRead()
    {
        QTemporaryDir directory;
        const auto paths = Tests::componentPathsFor(
            directory.path(), defaultsFile()
        );
        QVERIFY(Tests::writeBytes(
            paths.activeFile,
            QByteArray(Components::maximumComponentConfigurationBytes + 1, 'x')
        ));
        const auto loaded = ComponentStore(paths).load(catalog());
        QVERIFY2(loaded.available, qPrintable(loaded.error));
        QCOMPARE(loaded.loadState, ComponentLoadState::Defaulted);
    }

    void exposesValidActiveWhenRecoveryRepairFails()
    {
        QTemporaryDir directory;
        const auto initialPaths = Tests::componentPathsFor(
            directory.path(), defaultsFile()
        );
        const auto initial = ComponentStore(initialPaths).load(catalog());
        QVERIFY(initial.available);
        QVERIFY(QFile::remove(initialPaths.recoveryFile));

        const auto stateDirectory = QFileInfo(
            initialPaths.recoveryFile
        ).absolutePath();
        const auto held = stateDirectory + QStringLiteral(".held");
        QVERIFY(QDir().rename(stateDirectory, held));
        QFile blocker(stateDirectory);
        QVERIFY(blocker.open(QIODevice::WriteOnly));
        blocker.close();

        const auto loaded = ComponentStore(initialPaths).load(catalog());
        QVERIFY(loaded.available);
        QVERIFY(!loaded.writable);
        QCOMPARE(loaded.loadState, ComponentLoadState::Unavailable);
        QCOMPARE(loaded.state, initial.state);
    }

    void rejectsFifoSnapshotWithoutBlocking()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const auto paths = Tests::componentPathsFor(
            directory.path(), defaultsFile()
        );
        QVERIFY(createFifo(paths.activeFile));

        QElapsedTimer timer;
        timer.start();
        const auto loaded = ComponentStore(paths).load(catalog());
        QVERIFY(timer.elapsed() < 1000);
        QVERIFY(!loaded.available);
        QVERIFY(!loaded.writable);
        QCOMPARE(loaded.loadState, ComponentLoadState::Unavailable);
        QVERIFY(loaded.error.contains(QStringLiteral("regular file")));
    }

    void fifoRecoveryLeavesValidActiveReadableOnlyWithoutBlocking()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const auto paths = Tests::componentPathsFor(
            directory.path(), defaultsFile()
        );
        const auto initial = ComponentStore(paths).load(catalog());
        QVERIFY2(initial.available, qPrintable(initial.error));
        QVERIFY(QFile::remove(paths.recoveryFile));
        QVERIFY(createFifo(paths.recoveryFile));

        QElapsedTimer timer;
        timer.start();
        const auto loaded = ComponentStore(paths).load(catalog());
        QVERIFY(timer.elapsed() < 1000);
        QVERIFY(loaded.available);
        QVERIFY(!loaded.writable);
        QCOMPARE(loaded.loadState, ComponentLoadState::Unavailable);
        QCOMPARE(loaded.state, initial.state);
    }

    void rejectsSymlinkedProtectedDefaultsWithoutFollowingThem()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const auto linkedDefaults = directory.path()
            + QStringLiteral("/linked-defaults.json");
        QVERIFY(createSymlink(defaultsFile(), linkedDefaults));
        const auto paths = Tests::componentPathsFor(
            directory.path() + QStringLiteral("/state"),
            linkedDefaults
        );

        QElapsedTimer timer;
        timer.start();
        const auto loaded = ComponentStore(paths).load(catalog());
        QVERIFY(timer.elapsed() < 1000);
        QVERIFY(!loaded.available);
        QVERIFY(!loaded.writable);
        QCOMPARE(loaded.loadState, ComponentLoadState::Unavailable);
        QVERIFY(!loaded.error.isEmpty());
    }

    void rejectsTamperedProtectedDefault()
    {
        QTemporaryDir directory;
        const auto tampered = directory.path() + QStringLiteral("/defaults.json");
        auto root = QJsonDocument::fromJson(
            Tests::readBytes(defaultsFile())
        ).object();
        auto components = root.value(QStringLiteral("components")).toObject();
        const auto id = components.begin().key();
        auto desired = components.value(id).toObject();
        desired.insert(QStringLiteral("enabled"), false);
        components.insert(id, desired);
        root.insert(QStringLiteral("components"), components);
        QVERIFY(Tests::writeBytes(
            tampered,
            QJsonDocument(root).toJson(QJsonDocument::Compact)
        ));
        const auto paths = Tests::componentPathsFor(directory.path(), tampered);
        const auto loaded = ComponentStore(paths).load(catalog());
        QVERIFY(!loaded.available);
        QCOMPARE(loaded.loadState, ComponentLoadState::Unavailable);
    }

    void importsLegacyWorkspaceSettingsOnlyOnTrueFirstRun()
    {
        QTemporaryDir baselineDirectory;
        QTemporaryDir migratedDirectory;
        QVERIFY(baselineDirectory.isValid());
        QVERIFY(migratedDirectory.isValid());

        const auto baselinePaths = Tests::componentPathsFor(
            baselineDirectory.path(), defaultsFile()
        );
        const auto baseline = ComponentStore(baselinePaths).load(catalog());
        QVERIFY2(baseline.available, qPrintable(baseline.error));

        const auto migratedPaths = Tests::componentPathsFor(
            migratedDirectory.path(), defaultsFile()
        );
        const LegacyWorkspaceSettings legacy{
            .labelMode = QStringLiteral("names"),
            .showApplications = true,
            .maximumApplications = 5,
            .occupiedOnly = true,
            .scrollMode = QStringLiteral("reversed"),
        };
        const auto migrated = ComponentStore(migratedPaths).load(
            catalog(),
            legacy
        );
        QVERIFY2(migrated.available, qPrintable(migrated.error));
        QVERIFY(migrated.writable);
        QCOMPARE(migrated.loadState, ComponentLoadState::Normal);

        auto expected = baseline.state;
        const auto instanceId = QString::fromLatin1(
            Components::workspaceSwitcherDefaultInstanceId
        );
        expected.instances[instanceId].settings = {
            {QStringLiteral("labelMode"), QStringLiteral("names")},
            {QStringLiteral("showApplications"), true},
            {QStringLiteral("maximumApplications"), 5},
            {QStringLiteral("occupiedOnly"), true},
            {QStringLiteral("scrollMode"), QStringLiteral("reversed")},
        };
        QCOMPARE(migrated.state, expected);
        QCOMPARE(migrated.state.revision, quint64(0));
        QCOMPARE(
            Tests::readBytes(migratedPaths.activeFile),
            Tests::readBytes(migratedPaths.recoveryFile)
        );

        const LegacyWorkspaceSettings different{
            .labelMode = QStringLiteral("compact"),
            .showApplications = false,
            .maximumApplications = 1,
            .occupiedOnly = false,
            .scrollMode = QStringLiteral("normal"),
        };
        const auto reloaded = ComponentStore(migratedPaths).load(
            catalog(),
            different
        );
        QVERIFY2(reloaded.available, qPrintable(reloaded.error));
        QCOMPARE(reloaded.state, migrated.state);
        QCOMPARE(reloaded.loadState, ComponentLoadState::Normal);
    }

    void existingOrDamagedComponentFilesNeverImportLegacy()
    {
        const LegacyWorkspaceSettings legacy{
            .labelMode = QStringLiteral("names"),
            .showApplications = true,
            .maximumApplications = 5,
            .occupiedOnly = true,
            .scrollMode = QStringLiteral("reversed"),
        };

        for (const auto &scenario : {
                 QStringLiteral("active"),
                 QStringLiteral("recovery"),
                 QStringLiteral("damaged"),
             }) {
            QTemporaryDir directory;
            QVERIFY(directory.isValid());
            const auto paths = Tests::componentPathsFor(
                directory.path(), defaultsFile()
            );
            const auto initial = ComponentStore(paths).load(catalog());
            QVERIFY2(initial.available, qPrintable(initial.error));

            if (scenario == QStringLiteral("active")) {
                QVERIFY(QFile::remove(paths.recoveryFile));
            } else if (scenario == QStringLiteral("recovery")) {
                QVERIFY(QFile::remove(paths.activeFile));
            } else {
                QVERIFY(Tests::writeBytes(
                    paths.activeFile, QByteArrayLiteral("broken\n")
                ));
                QVERIFY(Tests::writeBytes(
                    paths.recoveryFile, QByteArrayLiteral("broken too\n")
                ));
            }

            const auto loaded = ComponentStore(paths).load(catalog(), legacy);
            QVERIFY2(loaded.available, qPrintable(loaded.error));
            QCOMPARE(
                loaded.state.instances.first().settings,
                Components::workspaceSwitcherDefaultSettings()
            );
            QCOMPARE(
                loaded.loadState,
                scenario == QStringLiteral("damaged")
                    ? ComponentLoadState::Defaulted
                    : scenario == QStringLiteral("recovery")
                        ? ComponentLoadState::Recovered
                        : ComponentLoadState::Normal
            );
        }
    }

    void ignoresInvalidTypedLegacyWorkspaceSettings()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const auto paths = Tests::componentPathsFor(
            directory.path(), defaultsFile()
        );
        const LegacyWorkspaceSettings invalid{
            .labelMode = QStringLiteral("icons"),
            .showApplications = true,
            .maximumApplications = 99,
            .occupiedOnly = true,
            .scrollMode = QStringLiteral("natural"),
        };

        const auto loaded = ComponentStore(paths).load(catalog(), invalid);
        QVERIFY2(loaded.available, qPrintable(loaded.error));
        QCOMPARE(
            loaded.state.instances.first().settings,
            Components::workspaceSwitcherDefaultSettings()
        );
    }

    void persistFailurePhases_data()
    {
        QTest::addColumn<int>("failedWrite");
        QTest::addColumn<bool>("expectedSuccess");
        QTest::addColumn<int>("expectedCalls");
        QTest::newRow("recovery-current") << 1 << false << 1;
        QTest::newRow("active-next") << 2 << false << 2;
        QTest::newRow("recovery-refresh-best-effort") << 3 << true << 3;
    }

    void persistFailurePhases()
    {
        QFETCH(int, failedWrite);
        QFETCH(bool, expectedSuccess);
        QFETCH(int, expectedCalls);
        int calls = 0;
        QVector<quint64> revisions;
        ComponentStore store(
            {
                .activeFile = QStringLiteral("active"),
                .recoveryFile = QStringLiteral("recovery"),
                .defaultsFile = QStringLiteral("defaults"),
            },
            [&calls, &revisions, failedWrite](
                const QString &,
                const Components::ComponentConfiguration &state,
                QString &error
            ) {
                ++calls;
                revisions.append(state.revision);
                if (calls == failedWrite) {
                    error = QStringLiteral("injected write failure");
                    return false;
                }
                return true;
            }
        );
        Components::ComponentConfiguration current;
        current.revision = 4;
        auto next = current;
        next.revision = 5;
        QString error;
        QCOMPARE(store.persist(current, next, error), expectedSuccess);
        QCOMPARE(calls, expectedCalls);
        if (failedWrite == 1) {
            QCOMPARE(revisions, QVector<quint64>{4});
        } else if (failedWrite == 2) {
            QCOMPARE(revisions, QVector<quint64>({4, 5}));
        } else {
            QCOMPARE(revisions, QVector<quint64>({4, 5, 5}));
        }
    }
};

QTEST_GUILESS_MAIN(ComponentStoreTest)
#include "component_store_test.moc"
