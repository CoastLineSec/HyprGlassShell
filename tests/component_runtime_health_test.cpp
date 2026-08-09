#include "component/builtin_component_defaults.h"
#include "component/component_contract.h"
#include "component/declarative_document.h"
#include "coordinator/components/component_plan_builder.h"
#include "coordinator/components/component_plan_controller.h"
#include "coordinator/components/component_runtime_health_store.h"

#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QTest>
#include <QTimer>

#include <sys/stat.h>
#include <unistd.h>

#include <limits>

namespace {

using namespace HyprShelld;
using namespace HyprShelld::Components;

const QString catalogDigest(64, QLatin1Char('a'));
const QString workspaceDigest(64, QLatin1Char('b'));
const QString declarativeDigest(64, QLatin1Char('c'));
const QString workspaceId = QString::fromLatin1(workspaceSwitcherId);
const QString declarativeId = QStringLiteral("org.example.widgets.status");
const QString workspaceInstance = QString::fromLatin1(
    workspaceSwitcherDefaultInstanceId
);
const QString declarativeInstance = QStringLiteral(
    "11111111-1111-4111-8111-111111111111"
);

RuntimeCatalogSnapshot runtimeCatalog()
{
    RuntimeCatalogSnapshot catalog;
    catalog.catalogDigest = catalogDigest;
    catalog.listedComponentIds = {workspaceId, declarativeId};
    catalog.listedComponentIds.sort();
    catalog.entries.insert(workspaceId, {
        .componentId = workspaceId,
        .componentType = QStringLiteral("bar-widget"),
        .packageDigest = workspaceDigest,
        .origin = QStringLiteral("system"),
        .removable = false,
        .componentApiVersion = QStringLiteral("1.0"),
        .runtimeKind = QStringLiteral("builtin-v1"),
        .factory = QString::fromLatin1(workspaceSwitcherFactory),
        .capabilityIds = {
            QString::fromLatin1(workspacesReadCapability),
            QString::fromLatin1(workspacesActivateCapability),
        },
    });
    DeclarativeDocument document;
    document.text.value = QStringLiteral("Trusted status");
    document.tooltip = QStringLiteral("Plain text only");
    document.maximumWidth = 240;
    const auto bytes = serializeDeclarativeDocument(document);
    catalog.entries.insert(declarativeId, {
        .componentId = declarativeId,
        .componentType = QStringLiteral("bar-widget"),
        .packageDigest = declarativeDigest,
        .origin = QStringLiteral("user"),
        .removable = true,
        .componentApiVersion = QStringLiteral("1.0"),
        .runtimeKind = QStringLiteral("declarative-v1"),
        .runtimeEntryPoint = QStringLiteral("widget.json"),
        .declarativeRuntime = bytes,
        .declarativeDocument = document,
    });
    return catalog;
}

RuntimeConfigurationSnapshot runtimeConfiguration()
{
    RuntimeConfigurationSnapshot configuration;
    configuration.catalogDigest = catalogDigest;
    configuration.revision = 7;
    configuration.components.insert(workspaceId, {
        .enabled = true,
        .packageDigest = workspaceDigest,
    });
    configuration.components.insert(declarativeId, {
        .enabled = true,
        .packageDigest = declarativeDigest,
    });
    configuration.instances.insert(workspaceInstance, {
        .componentId = workspaceId,
        .enabled = true,
        .settings = workspaceSwitcherDefaultSettings(),
    });
    configuration.instances.insert(declarativeInstance, {
        .componentId = declarativeId,
        .enabled = true,
    });
    configuration.barLayouts.insert(QStringLiteral("main"), {
        .outputMode = QStringLiteral("all"),
        .start = {workspaceInstance},
        .end = {declarativeInstance},
    });
    return configuration;
}

ComponentRuntimeHealthPaths pathsFor(const QTemporaryDir &directory)
{
    return {
        .activeFile = directory.filePath(QStringLiteral("state/active.json")),
        .recoveryFile = directory.filePath(QStringLiteral("state/recovery.json")),
    };
}

ComponentRuntimeHealthState quarantinedState(
    const quint64 revision,
    const QString &digest = declarativeDigest
)
{
    ComponentRuntimeHealthState state;
    state.revision = revision;
    const auto key = ComponentRuntimeHealthStore::recordKey(
        declarativeId,
        digest
    );
    state.records.insert(key, {
        .componentId = declarativeId,
        .packageDigest = digest,
        .state = QStringLiteral("quarantined"),
        .reason = QStringLiteral("timeout"),
        .failureCount = 1,
    });
    return state;
}

QByteArray readAll(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }
    return file.readAll();
}

bool overwrite(const QString &path, const QByteArray &bytes)
{
    QFile file(path);
    return file.open(QIODevice::WriteOnly | QIODevice::Truncate)
        && file.write(bytes) == bytes.size();
}

class ComponentRuntimeHealthTest final : public QObject {
    Q_OBJECT

private slots:
    void declarativePlacementIsMainLayoutAndInstanceDataIsClosed()
    {
        auto configuration = runtimeConfiguration();
        const auto main = configuration.barLayouts.take(
            QStringLiteral("main")
        );
        configuration.barLayouts.insert(QStringLiteral("secondary"), main);
        auto plan = buildSurfacePlan(runtimeCatalog(), configuration);
        QVERIFY(!plan);
        QCOMPARE(
            plan.errors.constFirst().code,
            QStringLiteral("component-runtime.invalid-effective-instance")
        );

        configuration = runtimeConfiguration();
        configuration.instances[declarativeInstance].settings.insert(
            QStringLiteral("ignored"),
            true
        );
        plan = buildSurfacePlan(runtimeCatalog(), configuration);
        QVERIFY(!plan);
        QCOMPARE(
            plan.errors.constFirst().code,
            QStringLiteral("component-runtime.invalid-effective-instance")
        );
    }

    void authorizationIsExplicitAndTimeoutQuarantinesExactDigest()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        ComponentPlanController controller(pathsFor(directory));
        QString error;
        QVERIFY(controller.initializeRuntimeHealth(error));
        QVERIFY(controller.acceptSnapshots(
            runtimeCatalog(),
            runtimeConfiguration()
        ));
        QVERIFY(controller.artifact()->plan.instances.contains(
            declarativeInstance
        ));
        QVERIFY(controller.runtimeHealthRecords().isEmpty());
        QTest::qWait(30);
        QVERIFY(controller.runtimeHealthRecords().isEmpty());

        auto *timer = controller.findChild<QTimer *>();
        QVERIFY(timer != nullptr);
        QCOMPARE(timer->interval(), componentActivationProbationMs);
        timer->setInterval(40);
        const auto planRevision = controller.revision();
        QVERIFY(controller.authorizeSurfacePlan(planRevision, error));
        QCOMPARE(
            controller.runtimeHealthRecords().constFirst().state,
            QStringLiteral("probation")
        );
        QTRY_COMPARE_WITH_TIMEOUT(
            controller.runtimeHealthRecords().constFirst().state,
            QStringLiteral("quarantined"),
            1000
        );
        QVERIFY(!controller.artifact()->plan.instances.contains(
            declarativeInstance
        ));
        QVERIFY(controller.artifact()->plan.instances.contains(
            workspaceInstance
        ));

        const auto healthRevision = controller.runtimeHealthRevision();
        QVERIFY(!controller.retryComponent(
            declarativeId,
            declarativeDigest,
            healthRevision - 1,
            error
        ));
        QVERIFY(!controller.retryComponent(
            declarativeId,
            QString(64, QLatin1Char('d')),
            healthRevision,
            error
        ));
        QVERIFY(controller.retryComponent(
            declarativeId,
            declarativeDigest,
            healthRevision,
            error
        ));
        QVERIFY(controller.runtimeHealthRecords().isEmpty());
        QVERIFY(controller.artifact()->plan.instances.contains(
            declarativeInstance
        ));

        controller.sourceUnavailable(QStringLiteral("authority lost"));
        QCOMPARE(controller.stateName(), QStringLiteral("retained"));
        QVERIFY(!controller.artifact()->plan.instances.contains(
            declarativeInstance
        ));
        QVERIFY(controller.artifact()->plan.instances.contains(
            workspaceInstance
        ));
    }

    void quarantineFollowsOnlyTheExactPackageDigest()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const auto paths = pathsFor(directory);
        ComponentRuntimeHealthStore store(paths);
        QVERIFY(store.persist(quarantinedState(1)).durable());

        ComponentPlanController controller(paths);
        QString error;
        QVERIFY(controller.initializeRuntimeHealth(error));
        QVERIFY(controller.acceptSnapshots(
            runtimeCatalog(),
            runtimeConfiguration()
        ));
        QVERIFY(!controller.artifact()->plan.instances.contains(
            declarativeInstance
        ));

        const auto updatedDigest = QString(64, QLatin1Char('d'));
        auto updatedCatalog = runtimeCatalog();
        updatedCatalog.entries[declarativeId].packageDigest = updatedDigest;
        auto updatedConfiguration = runtimeConfiguration();
        updatedConfiguration.components[declarativeId].packageDigest =
            updatedDigest;
        QVERIFY(controller.acceptSnapshots(
            updatedCatalog,
            updatedConfiguration
        ));
        QVERIFY(controller.artifact()->plan.instances.contains(
            declarativeInstance
        ));
        QCOMPARE(
            controller.artifact()->plan.instances.value(
                declarativeInstance
            ).packageDigest,
            updatedDigest
        );

        QVERIFY(controller.acceptSnapshots(
            runtimeCatalog(),
            runtimeConfiguration()
        ));
        QVERIFY(!controller.artifact()->plan.instances.contains(
            declarativeInstance
        ));
        QVERIFY(controller.acceptSnapshots(
            updatedCatalog,
            updatedConfiguration
        ));
        const auto healthRevision = controller.runtimeHealthRevision();
        QVERIFY(!controller.retryComponent(
            declarativeId,
            updatedDigest,
            healthRevision,
            error
        ));
        QVERIFY(controller.retryComponent(
            declarativeId,
            declarativeDigest,
            healthRevision,
            error
        ));
        QVERIFY(controller.runtimeHealthRecords().isEmpty());
        QVERIFY(controller.artifact()->plan.instances.contains(
            declarativeInstance
        ));
    }

    void incompleteAuthorizedPlanQuarantinesOnRestart()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const auto paths = pathsFor(directory);
        {
            ComponentPlanController controller(paths);
            QString error;
            QVERIFY(controller.initializeRuntimeHealth(error));
            QVERIFY(controller.acceptSnapshots(
                runtimeCatalog(),
                runtimeConfiguration()
            ));
            QVERIFY(controller.authorizeSurfacePlan(
                controller.revision(),
                error
            ));
            QCOMPARE(
                controller.runtimeHealthRecords().constFirst().state,
                QStringLiteral("probation")
            );
        }

        ComponentPlanController restarted(paths);
        QString error;
        QVERIFY(restarted.initializeRuntimeHealth(error));
        const auto records = restarted.runtimeHealthRecords();
        QCOMPARE(records.size(), 1);
        QCOMPARE(records.constFirst().state, QStringLiteral("quarantined"));
        QCOMPARE(
            records.constFirst().reason,
            QStringLiteral("incomplete-startup")
        );
    }

    void stableActivationClearsPersistentPending()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const auto paths = pathsFor(directory);
        {
            ComponentPlanController controller(paths);
            QString error;
            QVERIFY(controller.initializeRuntimeHealth(error));
            QVERIFY(controller.acceptSnapshots(
                runtimeCatalog(),
                runtimeConfiguration()
            ));
            const auto revision = controller.revision();
            QVERIFY(controller.authorizeSurfacePlan(revision, error));
            QVERIFY(controller.activationStable(
                declarativeInstance,
                declarativeId,
                declarativeDigest,
                revision,
                error
            ));
            QVERIFY(controller.runtimeHealthRecords().isEmpty());
        }
        ComponentPlanController restarted(paths);
        QString error;
        QVERIFY(restarted.initializeRuntimeHealth(error));
        QVERIFY(restarted.runtimeHealthRecords().isEmpty());
    }

    void stableReportBeforeDeadlineWins()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        ComponentPlanController controller(pathsFor(directory));
        QString error;
        QVERIFY(controller.initializeRuntimeHealth(error));
        QVERIFY(controller.acceptSnapshots(
            runtimeCatalog(),
            runtimeConfiguration()
        ));
        auto *timer = controller.findChild<QTimer *>();
        QVERIFY(timer != nullptr);
        timer->setInterval(200);
        const auto revision = controller.revision();
        QVERIFY(controller.authorizeSurfacePlan(revision, error));
        QTest::qWait(100);
        QVERIFY(controller.activationStable(
            declarativeInstance,
            declarativeId,
            declarativeDigest,
            revision,
            error
        ));
        QTest::qWait(150);
        QVERIFY(controller.runtimeHealthRecords().isEmpty());
    }

    void sourceLossClearsProbationWithoutChargingThePackage()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        ComponentPlanController controller(pathsFor(directory));
        QString error;
        QVERIFY(controller.initializeRuntimeHealth(error));
        QVERIFY(controller.acceptSnapshots(
            runtimeCatalog(),
            runtimeConfiguration()
        ));
        auto *timer = controller.findChild<QTimer *>();
        QVERIFY(timer != nullptr);
        timer->setInterval(40);
        QVERIFY(controller.authorizeSurfacePlan(controller.revision(), error));
        QCOMPARE(
            controller.runtimeHealthRecords().constFirst().state,
            QStringLiteral("probation")
        );

        controller.sourceUnavailable(QStringLiteral("authority lost"));
        QVERIFY(controller.runtimeHealthRecords().isEmpty());
        QTest::qWait(80);
        QVERIFY(controller.runtimeHealthRecords().isEmpty());
    }

    void planAuthorizationCancellationClearsProbation()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const auto paths = pathsFor(directory);
        {
            ComponentPlanController controller(paths);
            QString error;
            QVERIFY(controller.initializeRuntimeHealth(error));
            QVERIFY(controller.acceptSnapshots(
                runtimeCatalog(),
                runtimeConfiguration()
            ));
            auto *timer = controller.findChild<QTimer *>();
            QVERIFY(timer != nullptr);
            timer->setInterval(40);
            const auto revision = controller.revision();
            QVERIFY(controller.authorizeSurfacePlan(revision, error));
            QCOMPARE(
                controller.runtimeHealthRecords().constFirst().state,
                QStringLiteral("probation")
            );
            QVERIFY(controller.cancelSurfacePlanAuthorization(
                revision,
                error
            ));
            QVERIFY(controller.runtimeHealthRecords().isEmpty());
            QTest::qWait(80);
            QVERIFY(controller.runtimeHealthRecords().isEmpty());
        }
        ComponentPlanController restarted(paths);
        QString error;
        QVERIFY(restarted.initializeRuntimeHealth(error));
        QVERIFY(restarted.runtimeHealthRecords().isEmpty());
    }

    void explicitRendererLossCancelsProbation()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        ComponentPlanController controller(pathsFor(directory));
        QString error;
        QVERIFY(controller.initializeRuntimeHealth(error));
        QVERIFY(controller.acceptSnapshots(
            runtimeCatalog(),
            runtimeConfiguration()
        ));
        auto *timer = controller.findChild<QTimer *>();
        QVERIFY(timer != nullptr);
        timer->setInterval(40);
        const auto revision = controller.revision();
        QVERIFY(controller.authorizeSurfacePlan(revision, error));
        QVERIFY(!controller.runtimeHealthRecords().isEmpty());

        QVERIFY(controller.cancelSurfacePlanAuthorization(revision, error));
        QVERIFY(controller.runtimeHealthRecords().isEmpty());
        QTest::qWait(80);
        QVERIFY(controller.runtimeHealthRecords().isEmpty());
        QVERIFY(controller.artifact()->plan.instances.contains(
            declarativeInstance
        ));
        QVERIFY(!controller.cancelSurfacePlanAuthorization(
            revision + 1,
            error
        ));
    }

    void interruptedWriteSelectsHigherRevisionAndRepairs()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const auto paths = pathsFor(directory);
        ComponentRuntimeHealthStore store(paths);
        const auto first = quarantinedState(1);
        QVERIFY(store.persist(first).durable());

        const auto second = quarantinedState(
            2,
            QString(64, QLatin1Char('d'))
        );
        ComponentRuntimeHealthStore interrupted({
            .activeFile = paths.recoveryFile,
            .recoveryFile = directory.filePath(QStringLiteral("state/stage.json")),
        });
        QVERIFY(interrupted.persist(second).durable());

        const auto loaded = store.load();
        QVERIFY2(loaded.success, qPrintable(loaded.error));
        QCOMPARE(loaded.state, second);
        QCOMPARE(readAll(paths.activeFile), readAll(paths.recoveryFile));
    }

    void persistDurabilityTracksTheRecoveryCommitPoint()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const auto paths = pathsFor(directory);
        ComponentRuntimeHealthStore store(paths);
        const auto first = quarantinedState(1);
        const auto second = quarantinedState(
            2,
            QString(64, QLatin1Char('d'))
        );
        QVERIFY(store.persist(first).durable());

        ComponentRuntimeHealthStore beforeRecovery(
            paths,
            [](const ComponentRuntimeHealthPersistPhase phase) {
                return phase
                    == ComponentRuntimeHealthPersistPhase::BeforeRecoveryCommit;
            }
        );
        const auto notDurable = beforeRecovery.persist(second);
        QVERIFY(!notDurable.durable());
        auto loaded = store.load();
        QVERIFY2(loaded.success, qPrintable(loaded.error));
        QCOMPARE(loaded.state, first);

        ComponentRuntimeHealthStore beforeActive(
            paths,
            [](const ComponentRuntimeHealthPersistPhase phase) {
                return phase
                    == ComponentRuntimeHealthPersistPhase::BeforeActiveMirror;
            }
        );
        const auto recoveryDurable = beforeActive.persist(second);
        QVERIFY(recoveryDurable.durable());
        QVERIFY(
            recoveryDurable.durability
            == ComponentRuntimeHealthPersistDurability::RecoveryDurable
        );
        loaded = store.load();
        QVERIFY2(loaded.success, qPrintable(loaded.error));
        QCOMPARE(loaded.state, second);
        QCOMPARE(readAll(paths.activeFile), readAll(paths.recoveryFile));
    }

    void controllerAdoptsRecoveryDurableAuthorization()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const auto paths = pathsFor(directory);
        {
            ComponentPlanController controller(
                paths,
                [](const ComponentRuntimeHealthPersistPhase phase) {
                    return phase
                        == ComponentRuntimeHealthPersistPhase::BeforeActiveMirror;
                }
            );
            QString error;
            QVERIFY(controller.initializeRuntimeHealth(error));
            QVERIFY(controller.acceptSnapshots(
                runtimeCatalog(),
                runtimeConfiguration()
            ));
            QVERIFY(controller.authorizeSurfacePlan(
                controller.revision(),
                error
            ));
            QCOMPARE(
                controller.runtimeHealthRecords().constFirst().state,
                QStringLiteral("probation")
            );
        }

        ComponentPlanController restarted(paths);
        QString error;
        QVERIFY(restarted.initializeRuntimeHealth(error));
        QCOMPARE(
            restarted.runtimeHealthRecords().constFirst().state,
            QStringLiteral("quarantined")
        );
        QCOMPARE(
            restarted.runtimeHealthRecords().constFirst().reason,
            QStringLiteral("incomplete-startup")
        );
    }

    void healthFilesAndParentMustRemainPrivate()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const auto paths = pathsFor(directory);
        ComponentRuntimeHealthStore store(paths);
        const auto state = quarantinedState(5);
        QVERIFY(store.persist(state).durable());

        const auto active = QFile::encodeName(paths.activeFile);
        const auto parent = QFile::encodeName(
            QFileInfo(paths.activeFile).absolutePath()
        );
        struct stat status {};
        QVERIFY(::lstat(active.constData(), &status) == 0);
        QCOMPARE(status.st_mode & 0077, static_cast<mode_t>(0));
        QVERIFY(::lstat(parent.constData(), &status) == 0);
        QCOMPARE(status.st_mode & 0077, static_cast<mode_t>(0));

        QVERIFY(::chmod(active.constData(), 0666) == 0);
        const auto exposedFile = store.load();
        QVERIFY(!exposedFile.success);
        QVERIFY(exposedFile.state.safeMode);
        QVERIFY(::chmod(active.constData(), 0600) == 0);

        QVERIFY(::chmod(parent.constData(), 0755) == 0);
        const auto exposedParent = store.load();
        QVERIFY(!exposedParent.success);
        QVERIFY(exposedParent.state.safeMode);
        QVERIFY(::chmod(parent.constData(), 0700) == 0);
        const auto recovered = store.load();
        QVERIFY2(recovered.success, qPrintable(recovered.error));
        QCOMPARE(recovered.state, state);
    }

    void persistentHealthRecordVocabularyIsClosed()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        ComponentRuntimeHealthStore store(pathsFor(directory));

        const QStringList reasons{
            QStringLiteral("incomplete-startup"),
            QStringLiteral("timeout"),
            QStringLiteral("render-failed"),
            QStringLiteral("protocol-invalid"),
        };
        quint64 revision = 1;
        for (const auto &reason : reasons) {
            auto state = quarantinedState(revision++);
            state.records.begin()->reason = reason;
            QVERIFY2(store.persist(state).durable(), qPrintable(reason));
        }

        auto invalid = quarantinedState(revision++);
        invalid.records.begin()->reason = QStringLiteral("unknown");
        QVERIFY(!store.persist(invalid).durable());

        invalid = quarantinedState(revision++);
        invalid.records.begin()->reason.clear();
        QVERIFY(!store.persist(invalid).durable());

        invalid = quarantinedState(revision++);
        invalid.records.begin()->failureCount = 0;
        QVERIFY(!store.persist(invalid).durable());

        invalid = quarantinedState(revision);
        invalid.records.begin()->failureCount = 1000001;
        QVERIFY(!store.persist(invalid).durable());
    }

    void packageStateBoundIsSharedAcrossRecordsAndPending()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        ComponentRuntimeHealthStore store(pathsFor(directory));
        ComponentRuntimeHealthState state;
        state.revision = 1;
        for (int index = 0; index < 256; ++index) {
            const auto digest = QStringLiteral("%1").arg(
                index + 1,
                64,
                16,
                QLatin1Char('0')
            );
            const auto key = ComponentRuntimeHealthStore::recordKey(
                declarativeId,
                digest
            );
            state.records.insert(key, {
                .componentId = declarativeId,
                .packageDigest = digest,
                .state = QStringLiteral("quarantined"),
                .reason = QStringLiteral("timeout"),
                .failureCount = 1,
            });
        }
        for (int index = 256; index < 512; ++index) {
            const auto instanceId = QStringLiteral(
                "00000000-0000-4000-8000-%1"
            ).arg(index, 12, 16, QLatin1Char('0'));
            const auto digest = QStringLiteral("%1").arg(
                index + 1,
                64,
                16,
                QLatin1Char('0')
            );
            state.pending.insert(instanceId, {
                .instanceId = instanceId,
                .componentId = declarativeId,
                .packageDigest = digest,
            });
        }
        QVERIFY(store.persist(state).durable());

        const auto extraInstance = QStringLiteral(
            "00000000-0000-4000-8000-000000000200"
        );
        state.pending.insert(extraInstance, {
            .instanceId = extraInstance,
            .componentId = declarativeId,
            .packageDigest = QStringLiteral("%1").arg(
                513,
                64,
                16,
                QLatin1Char('0')
            ),
        });
        QVERIFY(!store.persist(state).durable());
    }

    void divergentSameRevisionAndRevisionExhaustionFailClosed()
    {
        QTemporaryDir divergentDirectory;
        QVERIFY(divergentDirectory.isValid());
        const auto divergentPaths = pathsFor(divergentDirectory);
        ComponentRuntimeHealthStore divergentStore(divergentPaths);
        const auto state = quarantinedState(6);
        QVERIFY(divergentStore.persist(state).durable());
        auto active = QJsonDocument::fromJson(
            readAll(divergentPaths.activeFile)
        ).object();
        active.insert(QStringLiteral("safeMode"), true);
        auto activeBytes = QJsonDocument(active).toJson(
            QJsonDocument::Compact
        );
        activeBytes.append('\n');
        QVERIFY(overwrite(divergentPaths.activeFile, activeBytes));
        const auto divergent = divergentStore.load();
        QVERIFY(!divergent.success);
        QVERIFY(divergent.state.safeMode);
        QCOMPARE(readAll(divergentPaths.activeFile), activeBytes);

        QTemporaryDir exhaustedDirectory;
        QVERIFY(exhaustedDirectory.isValid());
        const auto exhaustedPaths = pathsFor(exhaustedDirectory);
        ComponentRuntimeHealthStore exhaustedStore(exhaustedPaths);
        const auto exhaustedState = quarantinedState(
            std::numeric_limits<quint64>::max()
        );
        QVERIFY(exhaustedStore.persist(exhaustedState).durable());
        const auto loaded = exhaustedStore.load();
        QVERIFY2(loaded.success, qPrintable(loaded.error));
        QCOMPARE(loaded.state, exhaustedState);

        ComponentPlanController controller(exhaustedPaths);
        QString error;
        QVERIFY(controller.initializeRuntimeHealth(error));
        QVERIFY(!controller.retryComponent(
            declarativeId,
            declarativeDigest,
            std::numeric_limits<quint64>::max(),
            error
        ));
        QVERIFY(controller.thirdPartySafeMode());

        QTemporaryDir pendingDirectory;
        QVERIFY(pendingDirectory.isValid());
        const auto pendingPaths = pathsFor(pendingDirectory);
        ComponentRuntimeHealthStore pendingStore(pendingPaths);
        ComponentRuntimeHealthState pendingState;
        pendingState.revision = std::numeric_limits<quint64>::max();
        pendingState.pending.insert(declarativeInstance, {
            .instanceId = declarativeInstance,
            .componentId = declarativeId,
            .packageDigest = declarativeDigest,
        });
        QVERIFY(pendingStore.persist(pendingState).durable());

        ComponentPlanController pendingController(pendingPaths);
        QVERIFY(!pendingController.initializeRuntimeHealth(error));
        QCOMPARE(
            pendingController.runtimeHealthRevision(),
            std::numeric_limits<quint64>::max()
        );
        QVERIFY(pendingController.thirdPartySafeMode());
        const auto pendingRecords = pendingController.runtimeHealthRecords();
        QCOMPARE(pendingRecords.size(), 1);
        QCOMPARE(
            pendingRecords.constFirst().state,
            QStringLiteral("probation")
        );
        QVERIFY(pendingRecords.constFirst().reason.isEmpty());
        QCOMPARE(pendingRecords.constFirst().failureCount, 0U);
        const auto stillPending = pendingStore.load();
        QVERIFY2(stillPending.success, qPrintable(stillPending.error));
        QCOMPARE(stillPending.state, pendingState);
    }

    void damagedRegularSnapshotRecoversButUnsafeSnapshotSafeModes()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const auto paths = pathsFor(directory);
        ComponentRuntimeHealthStore store(paths);
        const auto state = quarantinedState(3);
        QVERIFY(store.persist(state).durable());

        QVERIFY(overwrite(paths.activeFile, QByteArrayLiteral("{broken")));
        auto loaded = store.load();
        QVERIFY2(loaded.success, qPrintable(loaded.error));
        QCOMPARE(loaded.state, state);
        QCOMPARE(readAll(paths.activeFile), readAll(paths.recoveryFile));

        QVERIFY(overwrite(paths.recoveryFile, QByteArrayLiteral("{broken")));
        loaded = store.load();
        QVERIFY2(loaded.success, qPrintable(loaded.error));
        QCOMPARE(loaded.state, state);

        auto future = QJsonDocument::fromJson(
            readAll(paths.recoveryFile)
        ).object();
        future.insert(QStringLiteral("formatVersion"), 2);
        future.insert(QStringLiteral("futureField"), true);
        auto futureBytes = QJsonDocument(future).toJson(QJsonDocument::Compact);
        futureBytes.append('\n');
        QVERIFY(overwrite(paths.recoveryFile, futureBytes));
        const auto futureLoaded = store.load();
        QVERIFY(!futureLoaded.success);
        QVERIFY(futureLoaded.state.safeMode);
        QCOMPARE(readAll(paths.recoveryFile), futureBytes);
    }

    void fifoAndSymlinkNeverBlockOrRecoverUnsafely()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const auto paths = pathsFor(directory);
        ComponentRuntimeHealthStore store(paths);
        QVERIFY(store.persist(quarantinedState(4)).durable());

        const auto recovery = QFile::encodeName(paths.recoveryFile);
        QVERIFY(::unlink(recovery.constData()) == 0);
        QVERIFY(::mkfifo(recovery.constData(), 0600) == 0);
        QElapsedTimer elapsed;
        elapsed.start();
        auto loaded = store.load();
        QVERIFY(!loaded.success);
        QVERIFY(loaded.state.safeMode);
        QVERIFY(elapsed.elapsed() < 1000);

        QVERIFY(::unlink(recovery.constData()) == 0);
        const auto target = directory.filePath(QStringLiteral("outside.json"));
        QVERIFY(overwrite(target, QByteArrayLiteral("{}")));
        const auto encodedTarget = QFile::encodeName(target);
        QVERIFY(::symlink(encodedTarget.constData(), recovery.constData()) == 0);
        loaded = store.load();
        QVERIFY(!loaded.success);
        QVERIFY(loaded.state.safeMode);
        QCOMPARE(readAll(target), QByteArrayLiteral("{}"));
    }
};

} // namespace

QTEST_GUILESS_MAIN(ComponentRuntimeHealthTest)

#include "component_runtime_health_test.moc"
