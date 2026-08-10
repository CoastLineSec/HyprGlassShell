#include "compositord/activation_backend.h"

#include "hyprland/json_support.h"

#include <QJsonObject>
#include <QtTest>

#include <memory>
#include <utility>

using namespace HyprShelld::Compositor;

namespace {

constexpr auto oldGeneration =
    "1111111111111111111111111111111111111111111111111111111111111111";
constexpr auto newGeneration =
    "2222222222222222222222222222222222222222222222222222222222222222";
constexpr auto otherGeneration =
    "3333333333333333333333333333333333333333333333333333333333333333";
constexpr auto snapshotDigest =
    "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
constexpr auto oldNonce = "0123456789abcdef0123456789abcdef";
constexpr auto newNonce = "fedcba9876543210fedcba9876543210";

[[nodiscard]] HyprlandVersionPolicy versionPolicy()
{
    return {
        .major = 0,
        .minor = 56,
        .minimumPatch = 0,
        .maximumPatch = 7,
    };
}

[[nodiscard]] ManagementStatus unmanagedStatus(
    const QString &digest = {}
)
{
    return {
        .state = ManagementState::Unmanaged,
        .entrypointKind = digest.isEmpty() ? EntrypointKind::Absent
                                           : EntrypointKind::Regular,
        .entrypointDigest = digest,
    };
}

[[nodiscard]] ManagementStatus managedStatus(
    const QString &generation,
    const QString &nonce = QString::fromLatin1(oldNonce)
)
{
    return {
        .state = ManagementState::Managed,
        .entrypointKind = EntrypointKind::Regular,
        .entrypointDigest = QStringLiteral("loader-") + generation.left(8),
        .managedGeneration = generation,
        .managedNonce = nonce,
    };
}

[[nodiscard]] ActivationGeneration preparedGeneration(
    const ActivationRequirement requirement = ActivationRequirement::Reload
)
{
    QJsonObject compatible {
        {QStringLiteral("major"), 0},
        {QStringLiteral("minor"), 56},
        {QStringLiteral("reviewedVersion"), QStringLiteral("0.56.1")},
        {QStringLiteral("minimumPatch"), 0},
        {QStringLiteral("maximumPatch"), 7},
    };
    QJsonObject manifest {
        {QStringLiteral("compatibleHyprland"), compatible},
    };
    auto bytes = HyprShelld::Hyprland::JsonSupport::canonicalJson(manifest);
    bytes.append('\n');
    return {
        .id = QString::fromLatin1(newGeneration),
        .nonce = QString::fromLatin1(newNonce),
        .snapshotDigest = QString::fromLatin1(snapshotDigest),
        .revision = 8,
        .directory = QStringLiteral("/authority/generations/")
            + QString::fromLatin1(newNonce),
        .entrypoint = QStringLiteral("/authority/generations/")
            + QString::fromLatin1(newNonce)
            + QStringLiteral("/hyprland.lua"),
        .manifest = bytes,
        .requirement = requirement,
    };
}

class FakePublisher final : public EntrypointPublisher
{
public:
    QStringList trace;
    ManagementStatus statusValue = unmanagedStatus();
    BackendResult initializeResult{
        .success = true,
        .status = unmanagedStatus(),
    };
    EntrypointPublishResult publishResult;
    EntrypointReconciliationResult pendingResult{.success = true};
    EntrypointRollbackResult rollbackResult;
    BackendResult verifyTargetResult;
    BackendResult verifyPriorResult;
    BackendResult finalizeTargetResult;
    BackendResult finalizePriorResult;
    QString watchPath = QStringLiteral("/authority/hyprland.lua");

    int initializeCalls = 0;
    int publishCalls = 0;
    int pendingCalls = 0;
    int rollbackCalls = 0;
    int verifyCalls = 0;
    int finalizeCalls = 0;
    bool lastPublishAdoption = false;
    QString lastExpectedEntrypointDigest;
    QByteArray lastBaselineErrors;
    QString lastBaselineProvider;
    bool lastVerifyTarget = false;
    bool lastFinalizeTarget = false;

    BackendResult initialize(ActivationFilesystemContext) override
    {
        ++initializeCalls;
        trace.append(QStringLiteral("publisher-initialize"));
        return initializeResult;
    }

    ManagementStatus status() const override { return statusValue; }

    QString managementWatchPath() const override { return watchPath; }

    EntrypointPublishResult publish(
        const ActivationGeneration &,
        const bool adoption,
        const QStringView expectedEntrypointDigest,
        const QByteArrayView baselineConfigErrors,
        const QStringView baselineProvider
    ) override
    {
        ++publishCalls;
        lastPublishAdoption = adoption;
        lastExpectedEntrypointDigest = expectedEntrypointDigest.toString();
        lastBaselineErrors = baselineConfigErrors.toByteArray();
        lastBaselineProvider = baselineProvider.toString();
        trace.append(adoption ? QStringLiteral("publisher-publish-adoption")
                              : QStringLiteral("publisher-publish-managed"));
        return publishResult;
    }

    EntrypointReconciliationResult pendingReconciliation() const override
    {
        auto *self = const_cast<FakePublisher *>(this);
        ++self->pendingCalls;
        self->trace.append(QStringLiteral("publisher-pending"));
        return pendingResult;
    }

    EntrypointRollbackResult rollback(
        const ActivationReceipt &
    ) override
    {
        ++rollbackCalls;
        trace.append(QStringLiteral("publisher-rollback"));
        return rollbackResult;
    }

    BackendResult finalize(
        const ActivationReceipt &,
        const bool target
    ) override
    {
        ++finalizeCalls;
        lastFinalizeTarget = target;
        trace.append(target ? QStringLiteral("publisher-finalize-target")
                            : QStringLiteral("publisher-finalize-prior"));
        const auto result = target ? finalizeTargetResult
                                   : finalizePriorResult;
        if (result.success) statusValue = result.status;
        return result;
    }

    BackendResult verifyTransition(
        const ActivationReceipt &,
        const bool target
    ) const override
    {
        auto *self = const_cast<FakePublisher *>(this);
        ++self->verifyCalls;
        self->lastVerifyTarget = target;
        self->trace.append(
            target ? QStringLiteral("publisher-verify-target")
                   : QStringLiteral("publisher-verify-prior")
        );
        return target ? verifyTargetResult : verifyPriorResult;
    }
};

class FakeRuntime final : public HyprlandActivationRuntime
{
public:
    QStringList trace;
    bool reloadSupported = true;
    RuntimeSessionResult prepareResult {
        .success = true,
        .session = RuntimeSession {
            .token = QByteArrayLiteral("runtime-session"),
            .baselineConfigErrors = QByteArrayLiteral("[]"),
            .baselineProvider = QStringLiteral("lua"),
        },
    };
    RuntimeProofResult proofResult{.success = true};
    ConnectedDisplaysResult connectedResult{
        .success = true,
        .runtimeIdentity = QStringLiteral("runtime-identity"),
        .topology = HyprShelld::Hyprland::ConnectedDisplayTopology{
            .topologyDigest = QString(64, QLatin1Char('d')),
            .document = QByteArrayLiteral("{\"formatVersion\":1}\n"),
        },
    };
    int setPolicyCalls = 0;
    int prepareCalls = 0;
    int proofCalls = 0;
    int cancelCalls = 0;
    int unboundedConnectedCalls = 0;
    QVector<int> connectedMaximumWaits;
    HyprlandVersionPolicy lastPolicy;
    ActivationRequirement lastRequirement = ActivationRequirement::None;
    RuntimeActivationMode lastMode = RuntimeActivationMode::ManagedReload;
    QString lastNonce;
    QByteArray lastExpectedErrors;
    QString lastExpectedProvider;

    void setVersionPolicy(HyprlandVersionPolicy policy) override
    {
        ++setPolicyCalls;
        lastPolicy = std::move(policy);
    }

    bool canSatisfy(const ActivationRequirement requirement) const override
    {
        return reloadSupported && requirement == ActivationRequirement::Reload;
    }

    RuntimeSessionResult prepare(
        const ActivationRequirement requirement,
        const RuntimeActivationMode mode
    ) override
    {
        ++prepareCalls;
        lastRequirement = requirement;
        lastMode = mode;
        switch (mode) {
        case RuntimeActivationMode::ManagedReload:
            trace.append(QStringLiteral("runtime-prepare-managed"));
            break;
        case RuntimeActivationMode::AdoptionFullReset:
            trace.append(QStringLiteral("runtime-prepare-adoption"));
            break;
        case RuntimeActivationMode::ManagedRollback:
            trace.append(QStringLiteral("runtime-prepare-managed-rollback"));
            break;
        case RuntimeActivationMode::LegacyRollback:
            trace.append(QStringLiteral("runtime-prepare-legacy-rollback"));
            break;
        }
        return prepareResult;
    }

    RuntimeProofResult reloadAndConfirm(
        const RuntimeSession &,
        const QStringView exactNonce,
        const QByteArrayView expectedConfigErrors,
        const QStringView expectedProvider
    ) override
    {
        ++proofCalls;
        lastNonce = exactNonce.toString();
        lastExpectedErrors = expectedConfigErrors.toByteArray();
        lastExpectedProvider = expectedProvider.toString();
        trace.append(QStringLiteral("runtime-proof"));
        return proofResult;
    }

    void cancel(const RuntimeSession &) noexcept override
    {
        ++cancelCalls;
        trace.append(QStringLiteral("runtime-cancel"));
    }

    ConnectedDisplaysResult connectedDisplays() override
    {
        ++unboundedConnectedCalls;
        trace.append(QStringLiteral("runtime-connected-unbounded"));
        return connectedResult;
    }

    ConnectedDisplaysResult connectedDisplays(
        const int maximumWaitMilliseconds
    ) override
    {
        connectedMaximumWaits.append(maximumWaitMilliseconds);
        trace.append(QStringLiteral("runtime-connected-bounded"));
        return connectedResult;
    }
};

struct LiveHarness final {
    FakePublisher *publisher = nullptr;
    FakeRuntime *runtime = nullptr;
    std::unique_ptr<LiveActivationBackend> backend;

    explicit LiveHarness(const ManagementStatus &initial)
    {
        auto ownedPublisher = std::make_unique<FakePublisher>();
        publisher = ownedPublisher.get();
        publisher->statusValue = initial;
        publisher->initializeResult.status = initial;
        auto ownedRuntime = std::make_unique<FakeRuntime>();
        runtime = ownedRuntime.get();
        backend = std::make_unique<LiveActivationBackend>(
            std::move(ownedPublisher), std::move(ownedRuntime)
        );
        backend->setVersionPolicy(versionPolicy());
    }

    [[nodiscard]] BackendResult bind()
    {
        return backend->bindFilesystemContext({});
    }

    [[nodiscard]] BackendResult start(const QString &generation)
    {
        const auto bound = bind();
        if (!bound.success) return bound;
        return backend->reconcileStartup(generation);
    }

    void clearTrace()
    {
        publisher->trace.clear();
        runtime->trace.clear();
    }

    [[nodiscard]] QStringList mergedTrace() const
    {
        // The tests below compare explicit per-collaborator counters and use
        // this only when calls do not interleave between the collaborators.
        auto result = publisher->trace;
        result.append(runtime->trace);
        return result;
    }
};

[[nodiscard]] EntrypointReconciliation pendingBridge(
    const QString &priorGeneration = QString::fromLatin1(oldGeneration),
    const QString &priorNonce = QString::fromLatin1(oldNonce)
)
{
    return {
        .pending = true,
        .targetGeneration = QString::fromLatin1(newGeneration),
        .priorGeneration = priorGeneration,
        .priorNonce = priorNonce,
        .baselineConfigErrors = QByteArrayLiteral("[]"),
        .baselineProvider = priorNonce.isEmpty() ? QStringLiteral("hyprlang")
                                                 : QStringLiteral("lua"),
        .receipt = {.rollbackToken = QByteArrayLiteral("bridge-token")},
    };
}

void configureTargetActivation(LiveHarness &harness)
{
    const auto target = managedStatus(
        QString::fromLatin1(newGeneration), QString::fromLatin1(newNonce)
    );
    harness.publisher->publishResult = {
        .success = true,
        .namespaceMayHaveChanged = true,
        .receipt = {.rollbackToken = QByteArrayLiteral("bridge-token")},
        .status = target,
    };
    harness.publisher->verifyTargetResult = {
        .success = true,
        .status = target,
    };
    harness.publisher->finalizeTargetResult = {
        .success = true,
        .status = target,
    };
}

void configurePriorRollback(
    LiveHarness &harness,
    const QString &priorGeneration = QString::fromLatin1(oldGeneration),
    const QString &priorNonce = QString::fromLatin1(oldNonce),
    const QString &provider = QStringLiteral("lua")
)
{
    const auto prior = priorGeneration.isEmpty()
        ? unmanagedStatus(QStringLiteral("original-loader"))
        : managedStatus(priorGeneration, priorNonce);
    harness.publisher->pendingResult = {
        .success = true,
        .value = pendingBridge(priorGeneration, priorNonce),
    };
    harness.publisher->rollbackResult = {
        .success = true,
        .namespaceMayHaveChanged = true,
        .proofNonce = priorNonce,
        .baselineConfigErrors = QByteArrayLiteral("[]"),
        .baselineProvider = provider,
        .status = prior,
    };
    harness.publisher->verifyPriorResult = {
        .success = true,
        .status = prior,
    };
    harness.publisher->finalizePriorResult = {
        .success = true,
        .status = prior,
    };
}

} // namespace

class CompositorActivationBackendTest final : public QObject
{
    Q_OBJECT

private slots:
    void statusIsBoundToTheAuthorityGeneration()
    {
        LiveHarness managed(managedStatus(QString::fromLatin1(oldGeneration)));
        QVERIFY(managed.start(QString::fromLatin1(oldGeneration)).success);
        QCOMPARE(managed.backend->status().state, ManagementState::Managed);

        managed.publisher->statusValue = managedStatus(
            QString::fromLatin1(otherGeneration)
        );
        QCOMPARE(managed.backend->status().state, ManagementState::Conflict);
        managed.publisher->statusValue = unmanagedStatus();
        QCOMPARE(managed.backend->status().state, ManagementState::Conflict);

        LiveHarness inactive(unmanagedStatus());
        QVERIFY(inactive.start({}).success);
        QCOMPARE(inactive.backend->status().state, ManagementState::Unmanaged);
        inactive.publisher->statusValue = managedStatus(
            QString::fromLatin1(newGeneration), QString::fromLatin1(newNonce)
        );
        QCOMPARE(inactive.backend->status().state, ManagementState::Conflict);
    }

    void managedActivationOrdersPublicationAndExactProof()
    {
        LiveHarness harness(managedStatus(QString::fromLatin1(oldGeneration)));
        QVERIFY(harness.start(QString::fromLatin1(oldGeneration)).success);
        configureTargetActivation(harness);
        harness.clearTrace();

        const auto result = harness.backend->activate(preparedGeneration());
        QVERIFY(result.success);
        QVERIFY(result.activationMayHaveOccurred);
        QCOMPARE(result.generation, QString::fromLatin1(newGeneration));
        QCOMPARE(harness.runtime->prepareCalls, 1);
        QCOMPARE(harness.runtime->lastMode,
                 RuntimeActivationMode::ManagedReload);
        QCOMPARE(harness.publisher->publishCalls, 1);
        QVERIFY(!harness.publisher->lastPublishAdoption);
        QCOMPARE(harness.publisher->lastBaselineErrors,
                 QByteArrayLiteral("[]"));
        QCOMPARE(harness.publisher->lastBaselineProvider,
                 QStringLiteral("lua"));
        QCOMPARE(harness.runtime->lastNonce, QString::fromLatin1(newNonce));
        QCOMPARE(harness.runtime->lastExpectedErrors, QByteArrayLiteral("[]"));
        QCOMPARE(harness.runtime->lastExpectedProvider, QStringLiteral("lua"));
        QCOMPARE(harness.publisher->verifyCalls, 1);
        QVERIFY(harness.publisher->lastVerifyTarget);
        QCOMPARE(harness.publisher->finalizeCalls, 0);
        QCOMPARE(harness.runtime->cancelCalls, 1);
    }

    void adoptionUsesFullResetModeAndExactEntrypointCas()
    {
        const auto originalDigest = QStringLiteral("original-digest");
        LiveHarness harness(unmanagedStatus(originalDigest));
        QVERIFY(harness.start({}).success);
        configureTargetActivation(harness);

        const auto result = harness.backend->adopt(
            preparedGeneration(), originalDigest
        );
        QVERIFY(result.success);
        QCOMPARE(harness.runtime->lastMode,
                 RuntimeActivationMode::AdoptionFullReset);
        QVERIFY(harness.publisher->lastPublishAdoption);
        QCOMPARE(harness.publisher->lastExpectedEntrypointDigest,
                 originalDigest);
        QCOMPARE(harness.runtime->lastNonce, QString::fromLatin1(newNonce));
    }

    void publisherFailureWithBridgeAlwaysRequiresRollback()
    {
        LiveHarness harness(managedStatus(QString::fromLatin1(oldGeneration)));
        QVERIFY(harness.start(QString::fromLatin1(oldGeneration)).success);
        harness.publisher->publishResult = {
            .success = false,
            .namespaceMayHaveChanged = false,
            .errorCode = QStringLiteral("PersistenceFailed"),
            .receipt = {.rollbackToken = QByteArrayLiteral("bridge-token")},
            .status = {
                .state = ManagementState::Conflict,
                .entrypointKind = EntrypointKind::Regular,
            },
        };

        const auto result = harness.backend->activate(preparedGeneration());
        QVERIFY(!result.success);
        QVERIFY(result.activationMayHaveOccurred);
        QCOMPARE(result.receipt.rollbackToken,
                 QByteArrayLiteral("bridge-token"));
        QCOMPARE(harness.runtime->proofCalls, 0);
        QCOMPARE(harness.publisher->verifyCalls, 0);
        QCOMPARE(harness.runtime->cancelCalls, 1);
    }

    void failedTargetProofRetainsReceiptWithoutFinalization()
    {
        LiveHarness harness(managedStatus(QString::fromLatin1(oldGeneration)));
        QVERIFY(harness.start(QString::fromLatin1(oldGeneration)).success);
        configureTargetActivation(harness);
        harness.runtime->proofResult = {
            .success = false,
            .errorCode = QStringLiteral("ReloadFailed"),
            .errorMessage = QStringLiteral("nonce was not observed"),
        };

        const auto result = harness.backend->activate(preparedGeneration());
        QVERIFY(!result.success);
        QVERIFY(result.activationMayHaveOccurred);
        QCOMPARE(result.receipt.rollbackToken,
                 QByteArrayLiteral("bridge-token"));
        QCOMPARE(harness.publisher->verifyCalls, 0);
        QCOMPARE(harness.publisher->finalizeCalls, 0);
        QCOMPARE(harness.runtime->cancelCalls, 1);
    }

    void rollbackAllowsCandidateErrorsThenProvesJournaledCleanPrior()
    {
        LiveHarness harness(managedStatus(QString::fromLatin1(oldGeneration)));
        QVERIFY(harness.start(QString::fromLatin1(oldGeneration)).success);
        configurePriorRollback(harness);
        harness.runtime->prepareResult.session->baselineConfigErrors =
            QByteArrayLiteral("[\"candidate error\"]");
        harness.clearTrace();

        const auto result = harness.backend->rollback(
            {.rollbackToken = QByteArrayLiteral("bridge-token")}
        );
        QVERIFY(result.success);
        QCOMPARE(harness.runtime->lastMode,
                 RuntimeActivationMode::ManagedRollback);
        QCOMPARE(harness.runtime->lastNonce, QString::fromLatin1(oldNonce));
        QCOMPARE(harness.runtime->lastExpectedErrors, QByteArrayLiteral("[]"));
        QCOMPARE(harness.runtime->lastExpectedProvider, QStringLiteral("lua"));
        QCOMPARE(harness.publisher->rollbackCalls, 1);
        QCOMPARE(harness.publisher->verifyCalls, 1);
        QVERIFY(!harness.publisher->lastVerifyTarget);
        QCOMPARE(harness.publisher->finalizeCalls, 1);
        QVERIFY(!harness.publisher->lastFinalizeTarget);
        QCOMPARE(harness.runtime->cancelCalls, 1);
    }

    void rollbackRefusesBridgeWhosePriorIsNotAuthorityBound()
    {
        LiveHarness harness(managedStatus(QString::fromLatin1(oldGeneration)));
        QVERIFY(harness.start(QString::fromLatin1(oldGeneration)).success);
        configurePriorRollback(
            harness, QString::fromLatin1(otherGeneration),
            QString::fromLatin1(oldNonce)
        );

        const auto result = harness.backend->rollback(
            {.rollbackToken = QByteArrayLiteral("bridge-token")}
        );
        QVERIFY(!result.success);
        QCOMPARE(harness.runtime->prepareCalls, 0);
        QCOMPARE(harness.publisher->rollbackCalls, 0);
        QCOMPARE(harness.publisher->finalizeCalls, 0);
    }

    void startupCommittedTargetFinalizesTargetWithoutReloadOrRollback()
    {
        const auto target = managedStatus(
            QString::fromLatin1(newGeneration), QString::fromLatin1(newNonce)
        );
        LiveHarness harness(target);
        QVERIFY(harness.bind().success);
        configureTargetActivation(harness);
        harness.publisher->statusValue = target;
        harness.publisher->pendingResult = {
            .success = true,
            .value = pendingBridge(),
        };
        harness.clearTrace();

        const auto result = harness.backend->reconcileStartup(
            QString::fromLatin1(newGeneration)
        );
        QVERIFY(result.success);
        QCOMPARE(result.status.state, ManagementState::Managed);
        QCOMPARE(result.status.managedGeneration,
                 QString::fromLatin1(newGeneration));
        QCOMPARE(harness.publisher->rollbackCalls, 0);
        QCOMPARE(harness.runtime->prepareCalls, 0);
        QCOMPARE(harness.publisher->verifyCalls, 0);
        QCOMPARE(harness.publisher->finalizeCalls, 1);
        QVERIFY(harness.publisher->lastFinalizeTarget);
    }

    void startupUncommittedTargetRollsBackAndProvesPrior()
    {
        const auto target = managedStatus(
            QString::fromLatin1(newGeneration), QString::fromLatin1(newNonce)
        );
        LiveHarness harness(target);
        QVERIFY(harness.bind().success);
        configurePriorRollback(harness);
        harness.publisher->statusValue = target;

        const auto result = harness.backend->reconcileStartup(
            QString::fromLatin1(oldGeneration)
        );
        QVERIFY(result.success);
        QCOMPARE(result.status.state, ManagementState::Managed);
        QCOMPARE(result.status.managedGeneration,
                 QString::fromLatin1(oldGeneration));
        QCOMPARE(harness.publisher->rollbackCalls, 1);
        QCOMPARE(harness.runtime->lastMode,
                 RuntimeActivationMode::ManagedRollback);
        QCOMPARE(harness.runtime->proofCalls, 1);
        QCOMPARE(harness.publisher->finalizeCalls, 1);
        QVERIFY(!harness.publisher->lastFinalizeTarget);
    }

    void committedFinalizeFailureIsStickyAndNeverRollsBack()
    {
        LiveHarness harness(managedStatus(QString::fromLatin1(oldGeneration)));
        QVERIFY(harness.start(QString::fromLatin1(oldGeneration)).success);
        configureTargetActivation(harness);
        const auto target = harness.publisher->verifyTargetResult.status;
        harness.publisher->statusValue = target;
        harness.publisher->pendingResult = {
            .success = true,
            .value = pendingBridge(),
        };
        harness.publisher->finalizeTargetResult = {
            .success = false,
            .errorCode = QStringLiteral("PersistenceFailed"),
            .errorMessage = QStringLiteral("bridge sync failed"),
            .status = target,
        };

        const auto finalized = harness.backend->finalizeCommitted(
            {.rollbackToken = QByteArrayLiteral("bridge-token")},
            QString::fromLatin1(newGeneration)
        );
        QVERIFY(!finalized.success);
        QCOMPARE(harness.publisher->finalizeCalls, 1);
        QVERIFY(harness.publisher->lastFinalizeTarget);
        QCOMPARE(harness.publisher->rollbackCalls, 0);
        QCOMPARE(harness.backend->status().state, ManagementState::Conflict);
        QVERIFY(!harness.backend->canSatisfy(ActivationRequirement::Reload));
    }

    void connectedDisplaysRequiresBoundAuthorityAndPreservesDeadlineBound()
    {
        LiveHarness harness(managedStatus(QString::fromLatin1(oldGeneration)));
        auto unavailable = harness.backend->connectedDisplays(41);
        QVERIFY(!unavailable.success);
        QCOMPARE(unavailable.errorCode, QStringLiteral("RuntimeUnavailable"));
        QVERIFY(harness.runtime->connectedMaximumWaits.isEmpty());
        QCOMPARE(harness.runtime->unboundedConnectedCalls, 0);

        QVERIFY(harness.start(QString::fromLatin1(oldGeneration)).success);
        auto connected = harness.backend->connectedDisplays(41);
        QVERIFY2(connected.success, qPrintable(connected.errorMessage));
        QCOMPARE(connected.runtimeIdentity, QStringLiteral("runtime-identity"));
        QCOMPARE(harness.runtime->connectedMaximumWaits, QVector<int>{41});
        QCOMPARE(harness.runtime->unboundedConnectedCalls, 0);

        connected = harness.backend->connectedDisplays(-1);
        QVERIFY(connected.success);
        QCOMPARE(harness.runtime->connectedMaximumWaits, QVector<int>{41});
        QCOMPARE(harness.runtime->unboundedConnectedCalls, 1);

        connected = harness.backend->connectedDisplays(0);
        QVERIFY(connected.success);
        QCOMPARE(harness.runtime->connectedMaximumWaits,
                 (QVector<int>{41, 0}));
    }

    void pendingDisplayTargetIsBoundToExactBridgeReceiptAndGeneration()
    {
        LiveHarness harness(managedStatus(QString::fromLatin1(oldGeneration)));
        QVERIFY(harness.start(QString::fromLatin1(oldGeneration)).success);
        const auto target = managedStatus(
            QString::fromLatin1(newGeneration), QString::fromLatin1(newNonce)
        );
        harness.publisher->statusValue = target;
        harness.publisher->pendingResult = {
            .success = true,
            .value = pendingBridge(),
        };
        harness.publisher->verifyTargetResult = {
            .success = true,
            .status = target,
        };

        // The ordinary public status remains conflict while the durable
        // authority still names the prior generation; only the private
        // receipt-bound verifier may prove the transient target.
        QCOMPARE(harness.backend->status().state, ManagementState::Conflict);
        const ActivationReceipt receipt{
            .rollbackToken = QByteArrayLiteral("bridge-token")
        };
        auto verified = harness.backend->verifyPendingTarget(
            receipt, QString::fromLatin1(newGeneration)
        );
        QVERIFY2(verified.success, qPrintable(verified.errorMessage));
        QCOMPARE(verified.status.managedGeneration,
                 QString::fromLatin1(newGeneration));
        QCOMPARE(harness.publisher->verifyCalls, 1);
        QVERIFY(harness.publisher->lastVerifyTarget);
        QCOMPARE(harness.backend->status().state, ManagementState::Conflict);

        verified = harness.backend->verifyPendingTarget(
            {.rollbackToken = QByteArrayLiteral("wrong-token")},
            QString::fromLatin1(newGeneration)
        );
        QVERIFY(!verified.success);
        QCOMPARE(verified.errorCode, QStringLiteral("EntrypointChanged"));
        QCOMPARE(harness.publisher->verifyCalls, 1);

        verified = harness.backend->verifyPendingTarget(
            receipt, QString::fromLatin1(oldGeneration)
        );
        QVERIFY(!verified.success);
        QCOMPARE(verified.errorCode, QStringLiteral("EntrypointChanged"));
        QCOMPARE(harness.publisher->verifyCalls, 1);

        auto corrupt = pendingBridge();
        corrupt.targetGeneration = QString::fromLatin1(otherGeneration);
        harness.publisher->pendingResult = {
            .success = true,
            .value = corrupt,
        };
        verified = harness.backend->verifyPendingTarget(
            receipt, QString::fromLatin1(newGeneration)
        );
        QVERIFY(!verified.success);
        QCOMPARE(verified.errorCode, QStringLiteral("EntrypointChanged"));
        QCOMPARE(harness.publisher->verifyCalls, 1);

        harness.publisher->pendingResult = {
            .success = true,
            .value = pendingBridge(),
        };
        harness.publisher->verifyTargetResult = {
            .success = false,
            .errorCode = QStringLiteral("VerificationFailed"),
            .errorMessage = QStringLiteral("authority root changed"),
            .status = {
                .state = ManagementState::Conflict,
                .entrypointKind = EntrypointKind::Unsafe,
            },
        };
        verified = harness.backend->verifyPendingTarget(
            receipt, QString::fromLatin1(newGeneration)
        );
        QVERIFY(!verified.success);
        QCOMPARE(verified.errorCode, QStringLiteral("VerificationFailed"));
        QCOMPARE(harness.publisher->verifyCalls, 2);
        QCOMPARE(harness.backend->status().state, ManagementState::Conflict);
    }

    void unsupportedRequirementDoesNotPrepareOrPublish()
    {
        LiveHarness harness(managedStatus(QString::fromLatin1(oldGeneration)));
        QVERIFY(harness.start(QString::fromLatin1(oldGeneration)).success);
        const auto result = harness.backend->activate(
            preparedGeneration(ActivationRequirement::Restart)
        );
        QVERIFY(!result.success);
        QCOMPARE(result.errorCode, QStringLiteral("ActivationRequired"));
        QCOMPARE(harness.runtime->prepareCalls, 0);
        QCOMPARE(harness.publisher->publishCalls, 0);
    }
};

QTEST_GUILESS_MAIN(CompositorActivationBackendTest)

#include "compositor_activation_backend_test.moc"
