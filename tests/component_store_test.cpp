#include "component_config_test_fixture.h"

#include <QFile>
#include <QFileInfo>
#include <QElapsedTimer>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QtTest>

#include <limits>

#include <sys/stat.h>
#include <unistd.h>

using namespace HyprShelld;

namespace {

const QString workspaceId = QString::fromLatin1(
    Components::workspaceSwitcherId
);
const QString previousWorkspaceDigest = QStringLiteral(
    "f4febcab5a093a803d35b93ae5300df3149f9bff5a571c759c771fe61699f0f7"
);

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

QJsonObject previousWorkspaceSettings(
    const QString &labelMode,
    const bool showApplications = false,
    const int maximumApplications = 3,
    const bool occupiedOnly = false,
    const QString &scrollMode = QStringLiteral("disabled")
)
{
    return {
        {QStringLiteral("labelMode"), labelMode},
        {QStringLiteral("showApplications"), showApplications},
        {QStringLiteral("maximumApplications"), maximumApplications},
        {QStringLiteral("occupiedOnly"), occupiedOnly},
        {QStringLiteral("scrollMode"), scrollMode},
    };
}

Components::ComponentConfiguration previousWorkspaceConfiguration(
    const QString &labelMode = QStringLiteral("numbers")
)
{
    const auto parsed = Components::parseComponentConfiguration(
        QByteArrayView(Tests::readBytes(defaultsFile())), catalog()
    );
    Q_ASSERT(parsed);
    auto state = *parsed.value;
    state.components[workspaceId].packageDigest = previousWorkspaceDigest;
    state.instances.first().settings = previousWorkspaceSettings(labelMode);
    return state;
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

    void migratesEveryKnownLabelModeAndPreservesComposition()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const auto paths = Tests::componentPathsFor(
            directory.path(), defaultsFile()
        );
        auto previous = previousWorkspaceConfiguration();
        previous.revision = 41;
        previous.components[workspaceId].enabled = false;

        const auto numbersId = QString::fromLatin1(
            Components::workspaceSwitcherDefaultInstanceId
        );
        previous.instances[numbersId].enabled = false;
        previous.instances[numbersId].settings = previousWorkspaceSettings(
            QStringLiteral("numbers"), true, 5, true,
            QStringLiteral("reversed")
        );
        const auto compactId = QStringLiteral(
            "11111111-1111-4111-8111-111111111111"
        );
        previous.instances.insert(compactId, {
            .componentId = workspaceId,
            .enabled = true,
            .settings = previousWorkspaceSettings(
                QStringLiteral("compact"), false, 2, false,
                QStringLiteral("normal")
            ),
        });
        const auto namesId = QStringLiteral(
            "22222222-2222-4222-8222-222222222222"
        );
        previous.instances.insert(namesId, {
            .componentId = workspaceId,
            .enabled = true,
            .settings = previousWorkspaceSettings(
                QStringLiteral("names"), true, 4, true
            ),
        });

        const auto dormantId = QStringLiteral("org.example.retained");
        const auto dormantInstanceId = QStringLiteral(
            "33333333-3333-4333-8333-333333333333"
        );
        previous.components.insert(dormantId, {
            .packageDigest = QString(64, QLatin1Char('b')),
            .enabled = true,
            .grantedCapabilities = {QStringLiteral("org.example.read")},
            .settings = {{QStringLiteral("retained"), true}},
        });
        previous.instances.insert(dormantInstanceId, {
            .componentId = dormantId,
            .enabled = false,
            .settings = {{QStringLiteral("value"), QStringLiteral("kept")}},
        });
        previous.bars[QStringLiteral("main")].start = {numbersId};
        previous.bars[QStringLiteral("main")].center = {compactId};
        previous.bars[QStringLiteral("main")].end = {
            namesId, dormantInstanceId,
        };

        const auto bytes = Components::serializeComponentConfiguration(
            previous
        );
        QVERIFY(Tests::writeBytes(paths.activeFile, bytes));
        QVERIFY(Tests::writeBytes(paths.recoveryFile, bytes));

        const auto loaded = ComponentStore(paths).load(catalog());
        QVERIFY2(loaded.available, qPrintable(loaded.error));
        QVERIFY(loaded.writable);
        QCOMPARE(loaded.loadState, ComponentLoadState::Normal);
        QCOMPARE(loaded.state.revision, quint64(42));
        QCOMPARE(
            loaded.state.components.value(workspaceId).packageDigest,
            catalog().entries.value(workspaceId).packageDigest
        );
        QCOMPARE(loaded.state.components.value(workspaceId).enabled, false);
        QCOMPARE(loaded.state.instances.value(numbersId).enabled, false);
        QCOMPARE(loaded.state.instances.value(numbersId).settings, QJsonObject({
            {QStringLiteral("showIdentifiers"), true},
            {QStringLiteral("showNames"), false},
            {QStringLiteral("showApplications"), true},
            {QStringLiteral("maximumApplications"), 5},
            {QStringLiteral("occupiedOnly"), true},
            {QStringLiteral("scrollMode"), QStringLiteral("reversed")},
        }));
        QCOMPARE(loaded.state.instances.value(compactId).settings, QJsonObject({
            {QStringLiteral("showIdentifiers"), false},
            {QStringLiteral("showNames"), false},
            {QStringLiteral("showApplications"), false},
            {QStringLiteral("maximumApplications"), 2},
            {QStringLiteral("occupiedOnly"), false},
            {QStringLiteral("scrollMode"), QStringLiteral("normal")},
        }));
        QCOMPARE(loaded.state.instances.value(namesId).settings, QJsonObject({
            {QStringLiteral("showIdentifiers"), true},
            {QStringLiteral("showNames"), true},
            {QStringLiteral("showApplications"), true},
            {QStringLiteral("maximumApplications"), 4},
            {QStringLiteral("occupiedOnly"), true},
            {QStringLiteral("scrollMode"), QStringLiteral("disabled")},
        }));
        QCOMPARE(
            loaded.state.components.value(dormantId),
            previous.components.value(dormantId)
        );
        QCOMPARE(
            loaded.state.instances.value(dormantInstanceId),
            previous.instances.value(dormantInstanceId)
        );
        QCOMPARE(loaded.state.bars, previous.bars);
        QCOMPARE(
            Tests::readBytes(paths.activeFile),
            Tests::readBytes(paths.recoveryFile)
        );

        const auto restarted = ComponentStore(paths).load(catalog());
        QVERIFY2(restarted.available, qPrintable(restarted.error));
        QCOMPARE(restarted.state, loaded.state);
        QCOMPARE(restarted.state.revision, quint64(42));
    }

    void migrationIsRecoveryFirstAndCompletesPartialCommit()
    {
        auto previous = previousWorkspaceConfiguration(
            QStringLiteral("compact")
        );
        previous.revision = 8;
        QStringList writes;
        const ComponentPaths injectedPaths{
            .activeFile = QStringLiteral("active"),
            .recoveryFile = QStringLiteral("recovery"),
            .defaultsFile = QStringLiteral("defaults"),
        };
        ComponentStore injected(
            injectedPaths,
            [&writes](
                const QString &path,
                const Components::ComponentConfiguration &,
                QString &error
            ) {
                writes.append(path);
                if (path == QStringLiteral("active")) {
                    error = QStringLiteral("injected active failure");
                    return false;
                }
                return true;
            }
        );
        const auto interrupted = injected.migrate(previous, catalog());
        QVERIFY(interrupted.changed);
        QVERIFY(!interrupted.writable);
        QCOMPARE(
            writes,
            QStringList({QStringLiteral("recovery"), QStringLiteral("active")})
        );
        QCOMPARE(interrupted.state.revision, quint64(9));

        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const auto paths = Tests::componentPathsFor(
            directory.path(), defaultsFile()
        );
        QVERIFY(Tests::writeBytes(
            paths.activeFile,
            Components::serializeComponentConfiguration(previous)
        ));
        QVERIFY(Tests::writeBytes(
            paths.recoveryFile,
            Components::serializeComponentConfiguration(interrupted.state)
        ));

        const auto completed = ComponentStore(paths).load(catalog());
        QVERIFY2(completed.available, qPrintable(completed.error));
        QVERIFY(completed.writable);
        QCOMPARE(completed.state, interrupted.state);
        QCOMPARE(completed.state.revision, quint64(9));
        QCOMPARE(
            Tests::readBytes(paths.activeFile),
            Tests::readBytes(paths.recoveryFile)
        );
        const auto restarted = ComponentStore(paths).load(catalog());
        QCOMPARE(restarted.state, completed.state);
        QCOMPARE(restarted.state.revision, quint64(9));

        QVERIFY(Tests::writeBytes(
            paths.activeFile,
            Components::serializeComponentConfiguration(interrupted.state)
        ));
        QVERIFY(Tests::writeBytes(
            paths.recoveryFile,
            Components::serializeComponentConfiguration(previous)
        ));
        const auto inversePartial = ComponentStore(paths).load(catalog());
        QVERIFY2(inversePartial.available, qPrintable(inversePartial.error));
        QVERIFY(inversePartial.writable);
        QCOMPARE(inversePartial.state, interrupted.state);
        QCOMPARE(inversePartial.state.revision, quint64(9));
        QCOMPARE(
            Tests::readBytes(paths.activeFile),
            Tests::readBytes(paths.recoveryFile)
        );
    }

    void migrationRequiresDurableRecoveryBeforePublication()
    {
        auto previous = previousWorkspaceConfiguration(
            QStringLiteral("names")
        );
        previous.revision = 3;
        int writes = 0;
        ComponentStore store(
            {
                .activeFile = QStringLiteral("active"),
                .recoveryFile = QStringLiteral("recovery"),
                .defaultsFile = QStringLiteral("defaults"),
            },
            [&writes](
                const QString &,
                const Components::ComponentConfiguration &,
                QString &error
            ) {
                ++writes;
                error = QStringLiteral("injected recovery failure");
                return false;
            }
        );
        const auto failed = store.migrate(previous, catalog());
        QVERIFY(!failed.changed);
        QVERIFY(!failed.writable);
        QCOMPARE(failed.state, previous);
        QCOMPARE(writes, 1);
    }

    void refusesUnknownMalformedAndExhaustedMigrationInputs()
    {
        QVector<Components::ComponentConfiguration> candidates;
        auto unknown = previousWorkspaceConfiguration();
        unknown.components[workspaceId].packageDigest = QString(
            64, QLatin1Char('c')
        );
        candidates.append(unknown);

        auto malformed = previousWorkspaceConfiguration();
        malformed.instances.first().settings.insert(
            QStringLiteral("labelMode"), QStringLiteral("icons")
        );
        candidates.append(malformed);

        auto exhausted = previousWorkspaceConfiguration();
        exhausted.revision = std::numeric_limits<quint64>::max();
        candidates.append(exhausted);

        for (const auto &candidate : candidates) {
            int writes = 0;
            ComponentStore store(
                {
                    .activeFile = QStringLiteral("active"),
                    .recoveryFile = QStringLiteral("recovery"),
                    .defaultsFile = QStringLiteral("defaults"),
                },
                [&writes](
                    const QString &,
                    const Components::ComponentConfiguration &,
                    QString &
                ) {
                    ++writes;
                    return true;
                }
            );
            const auto ignored = store.migrate(candidate, catalog());
            QVERIFY(!ignored.changed);
            QVERIFY(ignored.writable);
            QCOMPARE(ignored.state, candidate);
            QCOMPARE(writes, 0);
        }

        int wrongTargetWrites = 0;
        auto wrongTargetCatalog = catalog();
        wrongTargetCatalog.entries[workspaceId].packageDigest = QString(
            64, QLatin1Char('e')
        );
        ComponentStore wrongTargetStore(
            {
                .activeFile = QStringLiteral("active"),
                .recoveryFile = QStringLiteral("recovery"),
                .defaultsFile = QStringLiteral("defaults"),
            },
            [&wrongTargetWrites](
                const QString &,
                const Components::ComponentConfiguration &,
                QString &
            ) {
                ++wrongTargetWrites;
                return true;
            }
        );
        const auto wrongTarget = wrongTargetStore.migrate(
            previousWorkspaceConfiguration(), wrongTargetCatalog
        );
        QVERIFY(!wrongTarget.changed);
        QVERIFY(wrongTarget.writable);
        QCOMPARE(wrongTargetWrites, 0);
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
            {QStringLiteral("showIdentifiers"), true},
            {QStringLiteral("showNames"), true},
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

    void mapsEveryLegacyLabelModeOnFirstRun_data()
    {
        QTest::addColumn<QString>("labelMode");
        QTest::addColumn<bool>("showIdentifiers");
        QTest::addColumn<bool>("showNames");
        QTest::newRow("numbers")
            << QStringLiteral("numbers") << true << false;
        QTest::newRow("compact")
            << QStringLiteral("compact") << false << false;
        QTest::newRow("names")
            << QStringLiteral("names") << true << true;
    }

    void mapsEveryLegacyLabelModeOnFirstRun()
    {
        QFETCH(QString, labelMode);
        QFETCH(bool, showIdentifiers);
        QFETCH(bool, showNames);
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const auto paths = Tests::componentPathsFor(
            directory.path(), defaultsFile()
        );
        const LegacyWorkspaceSettings legacy{
            .labelMode = labelMode,
            .showApplications = true,
            .maximumApplications = 4,
            .occupiedOnly = true,
            .scrollMode = QStringLiteral("normal"),
        };
        const auto loaded = ComponentStore(paths).load(catalog(), legacy);
        QVERIFY2(loaded.available, qPrintable(loaded.error));
        const auto settings = loaded.state.instances.first().settings;
        QCOMPARE(
            settings.value(QStringLiteral("showIdentifiers")).toBool(),
            showIdentifiers
        );
        QCOMPARE(
            settings.value(QStringLiteral("showNames")).toBool(),
            showNames
        );
        QVERIFY(!settings.contains(QStringLiteral("labelMode")));
        QCOMPARE(
            settings.value(QStringLiteral("showApplications")).toBool(),
            true
        );
        QCOMPARE(
            settings.value(QStringLiteral("maximumApplications")).toInt(),
            4
        );
        QCOMPARE(
            settings.value(QStringLiteral("occupiedOnly")).toBool(),
            true
        );
        QCOMPARE(
            settings.value(QStringLiteral("scrollMode")).toString(),
            QStringLiteral("normal")
        );
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
