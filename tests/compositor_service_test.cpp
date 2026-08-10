#include "compositord/activation_backend.h"
#include "compositord/compositor_service.h"

#include <QCryptographicHash>
#include <QDBusConnection>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QtTest>

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include <memory>
#include <functional>
#include <tuple>

using namespace HyprShelld::Compositor;
using namespace HyprShelld::Hyprland;

namespace {

constexpr auto generationId =
    "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
constexpr auto snapshotId =
    "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb";
constexpr auto activationNonce = "0123456789abcdef0123456789abcdef";

[[nodiscard]] QString sha256(const QByteArrayView bytes)
{
    return QString::fromLatin1(
        QCryptographicHash::hash(bytes, QCryptographicHash::Sha256).toHex()
    );
}

[[nodiscard]] bool writeFile(
    const QString &path,
    const QByteArrayView bytes,
    const mode_t mode = 0600
)
{
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::NewOnly)
        || file.write(bytes.data(), bytes.size()) != bytes.size()
        || !file.flush()) {
        return false;
    }
    file.close();
    return ::chmod(QFile::encodeName(path).constData(), mode) == 0;
}

[[nodiscard]] bool makeDirectory(const QString &path, const mode_t mode = 0700)
{
    return QDir().mkpath(path)
        && ::chmod(QFile::encodeName(path).constData(), mode) == 0;
}

[[nodiscard]] bool makeSymlink(
    const QString &target,
    const QString &linkPath
)
{
    return ::symlink(
        QFile::encodeName(target).constData(),
        QFile::encodeName(linkPath).constData()
    ) == 0;
}

struct EntrypointTree final {
    QTemporaryDir temporary;
    QString configRoot;
    QString entrypoint;

    EntrypointTree()
        : configRoot(QDir(temporary.path()).filePath(QStringLiteral("hypr")))
        , entrypoint(QDir(configRoot).filePath(QStringLiteral("hyprland.lua")))
    {
        if (temporary.isValid()) {
            ::chmod(QFile::encodeName(temporary.path()).constData(), 0700);
            if (!makeDirectory(configRoot)) {
                configRoot.clear();
                entrypoint.clear();
            }
        }
    }
};

[[nodiscard]] AuthoritySnapshot dirtySnapshot()
{
    return {
        .available = true,
        .writable = true,
        .desiredState = QByteArrayLiteral("{\"revision\":\"5\"}\n"),
        .revision = 5,
        .catalogDigest = QStringLiteral("catalog-v1"),
        .actionCatalogDigest = QStringLiteral("actions-v1"),
        .loadState = QStringLiteral("ready"),
        .appliedRevision = 4,
        .applyState = QStringLiteral("dirty"),
        .requiredActivation = ActivationRequirement::Reload,
        .generationDigest = QStringLiteral("old-generation"),
    };
}

[[nodiscard]] AuthoritySnapshot committedSnapshot(
    const quint64 revision = 5,
    const QByteArray &desired = QByteArrayLiteral("{\"revision\":\"5\"}\n")
)
{
    auto snapshot = dirtySnapshot();
    snapshot.desiredState = desired;
    snapshot.revision = revision;
    snapshot.appliedRevision = revision;
    snapshot.applyState = QStringLiteral("current");
    snapshot.requiredActivation.reset();
    snapshot.generationDigest = QString::fromLatin1(generationId);
    return snapshot;
}

[[nodiscard]] AuthoritySnapshot unavailableSnapshot(
    const AuthoritySnapshot &baseline
)
{
    auto snapshot = baseline;
    snapshot.available = false;
    snapshot.writable = false;
    snapshot.loadState = QStringLiteral("unavailable");
    snapshot.applyState = QStringLiteral("failed");
    return snapshot;
}

[[nodiscard]] ManagementStatus managedStatus()
{
    return {
        .state = ManagementState::Managed,
        .entrypointKind = EntrypointKind::Regular,
        .entrypointDigest = QStringLiteral("managed-entrypoint"),
        .managedGeneration = QStringLiteral("old-generation"),
    };
}

[[nodiscard]] ActivationGeneration preparedGeneration(
    const ActivationRequirement requirement = ActivationRequirement::Reload,
    const quint64 revision = 5
)
{
    ActivationGeneration result{
        .id = QString::fromLatin1(generationId),
        .nonce = QString::fromLatin1(activationNonce),
        .snapshotDigest = QString::fromLatin1(snapshotId),
        .revision = revision,
        .directory = QStringLiteral("/tmp/generations/")
            + QString::fromLatin1(activationNonce),
        .requirement = requirement,
    };
    result.entrypoint = QDir(result.directory).filePath(
        QStringLiteral("hyprland.lua")
    );
    result.manifest = QJsonDocument(QJsonObject{
        {QStringLiteral("generation"), result.id},
        {QStringLiteral("activationNonce"), result.nonce},
        {QStringLiteral("snapshotDigest"), result.snapshotDigest},
        {QStringLiteral("revision"), QString::number(result.revision)},
        {QStringLiteral("entrypoint"), QStringLiteral("hyprland.lua")},
    }).toJson(QJsonDocument::Compact);
    result.manifest.append('\n');
    return result;
}

[[nodiscard]] ConnectedDisplayTopology connectedDisplayTopology()
{
    return {
        .outputs = QVector<ConnectedDisplay>{ConnectedDisplay{
            .upstreamId = 7,
            .selector = QStringLiteral("DP-1"),
            .description = QStringLiteral("Acme Panel DP-1"),
            .make = QStringLiteral("Acme"),
            .model = QStringLiteral("Panel"),
            .serial = QStringLiteral("serial-DP-1"),
            .enabled = true,
            .width = 2560,
            .height = 1440,
            .physicalWidthMm = 600,
            .physicalHeightMm = 340,
            .refreshRate = 144.0,
            .x = 0,
            .y = 0,
            .reserved = {0, 0, 0, 0},
            .scale = 1.25,
            .transform = 0,
            .focused = true,
            .dpms = true,
            .vrrActive = false,
            .modes = QVector<ConnectedDisplayMode>{ConnectedDisplayMode{
                .width = 2560,
                .height = 1440,
                .refreshRate = 144.0,
                .managedMode = QStringLiteral("2560x1440@144"),
            }},
            .colorManagement = QStringLiteral("srgb"),
            .currentFormat = QStringLiteral("XRGB8888"),
            .sdrBrightness = 1.0,
            .sdrSaturation = 1.0,
            .sdrMinLuminance = 0.2,
            .sdrMaxLuminance = 80,
        }},
        .topologyDigest = QString(64, QLatin1Char('d')),
    };
}

[[nodiscard]] QByteArray displayProfileBytes(
    const ConnectedDisplayTopology &topology
)
{
    return serializeDisplayProfile(DisplayProfile{
        .topologyDigest = topology.topologyDigest,
        .outputs = QJsonArray{QJsonObject{
            {QStringLiteral("selector"), QStringLiteral("DP-1")},
            {QStringLiteral("enabled"), true},
            {QStringLiteral("mode"), QStringLiteral("2560x1440@144")},
            {QStringLiteral("position"), QStringLiteral("0x0")},
            {QStringLiteral("scale"), 1.25},
            {QStringLiteral("transform"), 0},
            {QStringLiteral("mirror"), QString()},
            {QStringLiteral("bitdepth"), 8},
            {QStringLiteral("cm"), QStringLiteral("srgb")},
            {QStringLiteral("sdrBrightness"), 1.0},
            {QStringLiteral("sdrSaturation"), 1.0},
            {QStringLiteral("sdrMinLuminance"), 0.2},
            {QStringLiteral("sdrMaxLuminance"), 80},
        }},
    });
}

class FakeAuthority final : public ConfigurationAuthority
{
public:
    AuthorityResult initializeResult;
    AuthorityResult replaceResult;
    AuthorityResult prepareApplyResult;
    AuthorityResult prepareRecoveryResult;
    AuthorityResult prepareDisplayResult;
    AuthorityResult commitResult;
    AuthorityResult abortResult;
    AuthoritySnapshot current;
    int initializeCalls = 0;
    int replaceCalls = 0;
    int prepareApplyCalls = 0;
    int prepareRecoveryCalls = 0;
    int prepareDisplayCalls = 0;
    int commitCalls = 0;
    int abortCalls = 0;
    mutable int filesystemContextCalls = 0;
    bool filesystemContextSuccess = true;
    QString filesystemContextError;
    QStringList *startupTrace = nullptr;
    std::function<void()> abortHook;
    quint64 lastReplaceExpected = 0;
    QByteArray lastReplaceCandidate;
    QByteArray optionCatalogBytes;
    mutable int optionCatalogCalls = 0;
    QStringList calls;

    AuthorityResult initialize() override
    {
        ++initializeCalls;
        calls.append(QStringLiteral("initialize"));
        if (startupTrace) {
            startupTrace->append(QStringLiteral("authority-initialize"));
        }
        if (initializeResult.success) {
            current = initializeResult.snapshot;
        }
        return initializeResult;
    }

    FilesystemContextResult duplicateActivationFilesystemContext() const override
    {
        ++filesystemContextCalls;
        if (startupTrace) {
            startupTrace->append(QStringLiteral("authority-context"));
        }
        return {
            .success = filesystemContextSuccess,
            .errorCode = filesystemContextSuccess
                ? QString{} : QStringLiteral("PersistenceFailed"),
            .errorMessage = filesystemContextError,
        };
    }

    AuthoritySnapshot snapshot() const override
    {
        return current;
    }

    QByteArray optionCatalog() const override
    {
        ++optionCatalogCalls;
        return optionCatalogBytes;
    }

    AuthorityResult replaceSnapshot(
        const quint64 expectedRevision,
        const QByteArray &candidate
    ) override
    {
        ++replaceCalls;
        lastReplaceExpected = expectedRevision;
        lastReplaceCandidate = candidate;
        calls.append(QStringLiteral("replace"));
        if (replaceResult.success) {
            current = replaceResult.snapshot;
        }
        return replaceResult;
    }

    AuthorityResult prepareApply(
        quint64,
        const QString &,
        const QDateTime &
    ) override
    {
        ++prepareApplyCalls;
        calls.append(QStringLiteral("prepare-apply"));
        return prepareApplyResult;
    }

    AuthorityResult prepareRecovery(
        quint64,
        const QString &,
        const QDateTime &
    ) override
    {
        ++prepareRecoveryCalls;
        calls.append(QStringLiteral("prepare-recovery"));
        return prepareRecoveryResult;
    }

    AuthorityResult prepareDisplayApply(
        quint64,
        const DisplayProfile &,
        const ConnectedDisplayTopology &,
        const QString &,
        const QDateTime &
    ) override
    {
        ++prepareDisplayCalls;
        calls.append(QStringLiteral("prepare-display"));
        return prepareDisplayResult;
    }

    AuthorityResult commitApply(const QString &) override
    {
        ++commitCalls;
        calls.append(QStringLiteral("commit"));
        if (commitResult.success) {
            current = commitResult.snapshot;
        }
        return commitResult;
    }

    AuthorityResult abortApply(const QString &) override
    {
        ++abortCalls;
        calls.append(QStringLiteral("abort"));
        if (abortHook) {
            abortHook();
        }
        if (abortResult.success) {
            current = abortResult.snapshot;
        }
        return abortResult;
    }
};

class FakeActivationBackend final : public ActivationBackend
{
public:
    ManagementStatus statusValue = managedStatus();
    QVector<ActivationRequirement> supported{ActivationRequirement::Reload};
    ActivationResult adoptionResult;
    ActivationResult activationResult;
    ActivationResult rollbackResult;
    BackendResult reconcileResult;
    BackendResult bindResult;
    BackendResult finalizeResult;
    ConnectedDisplaysResult connectedResult;
    QVector<ConnectedDisplaysResult> connectedSequence;
    int connectedSequenceIndex = 0;
    BackendResult verifyPendingResult;
    bool reconcileResultConfigured = false;
    bool bindResultConfigured = false;
    bool finalizeResultConfigured = false;
    QString managementWatchPathValue;
    mutable int statusCalls = 0;
    mutable QVector<ActivationRequirement> capabilityChecks;
    int adoptCalls = 0;
    int activateCalls = 0;
    int rollbackCalls = 0;
    int reconcileCalls = 0;
    int bindCalls = 0;
    int finalizeCalls = 0;
    int connectedCalls = 0;
    QVector<int> connectedMaximumWaits;
    mutable int verifyPendingCalls = 0;
    QStringList calls;
    QByteArray lastRollbackToken;
    QByteArray lastFinalizeToken;
    QString lastReconcileGeneration;
    QString lastFinalizeGeneration;
    mutable QByteArray lastVerifyPendingToken;
    mutable QString lastVerifyPendingGeneration;
    bool lastFilesystemContextComplete = false;
    QStringList *startupTrace = nullptr;

    ManagementStatus status() const override
    {
        ++statusCalls;
        return statusValue;
    }

    bool canSatisfy(const ActivationRequirement requirement) const override
    {
        capabilityChecks.append(requirement);
        return supported.contains(requirement);
    }

    QString managementWatchPath() const override
    {
        return managementWatchPathValue;
    }

    BackendResult bindFilesystemContext(
        ActivationFilesystemContext context
    ) override
    {
        ++bindCalls;
        lastFilesystemContextComplete = context.complete();
        if (startupTrace) {
            startupTrace->append(QStringLiteral("backend-bind"));
        }
        return bindResultConfigured
            ? bindResult
            : BackendResult{.success = true, .status = statusValue};
    }

    BackendResult reconcileStartup(QStringView committedGeneration) override
    {
        ++reconcileCalls;
        if (startupTrace) {
            startupTrace->append(QStringLiteral("backend-reconcile"));
        }
        lastReconcileGeneration = committedGeneration.toString();
        return reconcileResultConfigured
            ? reconcileResult
            : BackendResult{.success = true, .status = statusValue};
    }

    ActivationResult adopt(
        const ActivationGeneration &prepared,
        QStringView
    ) override
    {
        ++adoptCalls;
        calls.append(QStringLiteral("adopt"));
        statusValue = adoptionResult.status;
        if (statusValue.state == ManagementState::Managed) {
            statusValue.managedGeneration = prepared.id;
        }
        adoptionResult.status = statusValue;
        return adoptionResult;
    }

    ActivationResult activate(const ActivationGeneration &prepared) override
    {
        ++activateCalls;
        calls.append(QStringLiteral("activate"));
        statusValue = activationResult.status;
        if (statusValue.state == ManagementState::Managed) {
            statusValue.managedGeneration = prepared.id;
        }
        activationResult.status = statusValue;
        return activationResult;
    }

    ActivationResult rollback(const ActivationReceipt &receipt) override
    {
        ++rollbackCalls;
        calls.append(QStringLiteral("rollback"));
        lastRollbackToken = receipt.rollbackToken;
        statusValue = rollbackResult.status;
        return rollbackResult;
    }

    BackendResult finalizeCommitted(
        const ActivationReceipt &receipt,
        QStringView committedGeneration
    ) override
    {
        ++finalizeCalls;
        lastFinalizeToken = receipt.rollbackToken;
        lastFinalizeGeneration = committedGeneration.toString();
        if (finalizeResultConfigured) {
            statusValue = finalizeResult.status;
            return finalizeResult;
        }
        return {.success = true, .status = statusValue};
    }

    ConnectedDisplaysResult connectedDisplays() override
    {
        ++connectedCalls;
        if (connectedSequenceIndex < connectedSequence.size()) {
            return connectedSequence.at(connectedSequenceIndex++);
        }
        return connectedResult;
    }

    ConnectedDisplaysResult connectedDisplays(
        const int maximumWaitMilliseconds
    ) override
    {
        ++connectedCalls;
        connectedMaximumWaits.append(maximumWaitMilliseconds);
        if (connectedSequenceIndex < connectedSequence.size()) {
            return connectedSequence.at(connectedSequenceIndex++);
        }
        return connectedResult;
    }

    BackendResult verifyPendingTarget(
        const ActivationReceipt &receipt,
        QStringView generation
    ) const override
    {
        ++verifyPendingCalls;
        lastVerifyPendingToken = receipt.rollbackToken;
        lastVerifyPendingGeneration = generation.toString();
        return verifyPendingResult;
    }
};

struct ServiceHarness final {
    QStringList startupTrace;
    FakeAuthority *authority = nullptr;
    FakeActivationBackend *backend = nullptr;
    std::unique_ptr<CompositorService> service;

    explicit ServiceHarness(
        const AuthoritySnapshot &initial,
        const ManagementStatus &management = managedStatus(),
        const QString &managementWatchPath = {},
        CompositorService::DisplayDeadlineRemaining deadlineRemaining = {},
        CompositorService::DisplayOwnerPresent ownerPresent = {}
    )
    {
        auto ownedBackend = std::make_unique<FakeActivationBackend>();
        backend = ownedBackend.get();
        backend->startupTrace = &startupTrace;
        backend->statusValue = management;
        backend->managementWatchPathValue = managementWatchPath;
        service = std::make_unique<CompositorService>(
            std::move(ownedBackend),
            QDBusConnection(QStringLiteral("compositor-service-test")),
            nullptr,
            std::move(deadlineRemaining),
            std::move(ownerPresent)
        );

        auto ownedAuthority = std::make_unique<FakeAuthority>();
        authority = ownedAuthority.get();
        authority->startupTrace = &startupTrace;
        authority->initializeResult = {
            .success = true,
            .snapshot = initial,
        };
        QString error;
        if (!service->initializeAuthority(std::move(ownedAuthority), error)) {
            qFatal("fake authority initialization failed: %s", qPrintable(error));
        }
    }
};

void configureDisplayPreview(
    ServiceHarness &harness,
    const AuthoritySnapshot &initial,
    const ConnectedDisplayTopology &topology
)
{
    auto staged = initial;
    staged.writable = false;
    harness.authority->prepareDisplayResult = {
        .success = true,
        .snapshot = staged,
        .prepared = preparedGeneration(ActivationRequirement::Reload, 6),
    };
    harness.authority->abortResult = {
        .success = true,
        .snapshot = initial,
    };
    harness.backend->connectedResult = {
        .success = true,
        .runtimeIdentity = QStringLiteral("hyprland-runtime-identity"),
        .topology = topology,
    };
    auto target = managedStatus();
    target.managedGeneration = QString::fromLatin1(generationId);
    harness.backend->activationResult = {
        .success = true,
        .activationMayHaveOccurred = true,
        .generation = QString::fromLatin1(generationId),
        .runtimeIdentity = QStringLiteral("hyprland-runtime-identity"),
        .confirmedRequirement = ActivationRequirement::Reload,
        .receipt = {QByteArrayLiteral("display-rollback-token")},
        .status = target,
    };
    harness.backend->rollbackResult = {
        .success = true,
        .status = managedStatus(),
    };
    harness.backend->verifyPendingResult = {
        .success = true,
        .status = target,
    };
}

} // namespace

class CompositorServiceTest final : public QObject
{
    Q_OBJECT

private slots:
    void optionCatalogReturnsExactAuthorityBytesWithoutMutation()
    {
        const QByteArray catalog = QByteArrayLiteral(
            "{\"contractVersion\":1,\"options\":[]}"
        );
        auto initial = dirtySnapshot();
        initial.catalogDigest = sha256(catalog);
        ServiceHarness harness(initial);
        harness.authority->optionCatalogBytes = catalog;

        QString digest = QStringLiteral("must-be-replaced");
        QCOMPARE(harness.service->GetOptionCatalog(digest), catalog);
        QCOMPARE(digest, initial.catalogDigest);
        QCOMPARE(harness.authority->optionCatalogCalls, 1);
        QCOMPARE(harness.authority->replaceCalls, 0);
        QCOMPARE(harness.authority->prepareApplyCalls, 0);
        QCOMPARE(harness.authority->prepareRecoveryCalls, 0);
        QCOMPARE(harness.authority->current, initial);
    }

    void optionCatalogFailsClosedForMismatchedOrOversizedAuthorityBytes()
    {
        {
            auto initial = dirtySnapshot();
            initial.catalogDigest = QString(64, QLatin1Char('c'));
            ServiceHarness harness(initial);
            harness.authority->optionCatalogBytes =
                QByteArrayLiteral("{\"contractVersion\":1}");

            QString digest = QStringLiteral("must-be-cleared");
            QVERIFY(harness.service->GetOptionCatalog(digest).isEmpty());
            QVERIFY(digest.isEmpty());
            QCOMPARE(harness.authority->optionCatalogCalls, 1);
        }

        {
            const QByteArray oversized(maximumCatalogBytes + 1, 'x');
            auto initial = dirtySnapshot();
            initial.catalogDigest = sha256(oversized);
            ServiceHarness harness(initial);
            harness.authority->optionCatalogBytes = oversized;

            QString digest = QStringLiteral("must-be-cleared");
            QVERIFY(harness.service->GetOptionCatalog(digest).isEmpty());
            QVERIFY(digest.isEmpty());
            QCOMPARE(harness.authority->optionCatalogCalls, 1);
        }
    }

    void inspectorDistinguishesAbsentFromRegular()
    {
        EntrypointTree tree;
        QVERIFY(tree.temporary.isValid());
        DeferredActivationBackend backend(tree.configRoot, tree.entrypoint);

        QCOMPARE(
            backend.status(),
            (ManagementStatus{
                .state = ManagementState::Unmanaged,
                .entrypointKind = EntrypointKind::Absent,
            })
        );
        const QByteArray contents{"-- existing user configuration\n"};
        QVERIFY(writeFile(tree.entrypoint, contents));
        QCOMPARE(
            backend.status(),
            (ManagementStatus{
                .state = ManagementState::Unmanaged,
                .entrypointKind = EntrypointKind::Regular,
                .entrypointDigest = sha256(contents),
            })
        );
        QVERIFY(!backend.canSatisfy(ActivationRequirement::Reload));
        const auto unavailable = backend.activate(preparedGeneration());
        QVERIFY(!unavailable.success);
        QCOMPARE(unavailable.errorCode, QStringLiteral("ActivationRequired"));
    }

    void inspectorRejectsUnsafeFinalObjects()
    {
        {
            EntrypointTree tree;
            QVERIFY(tree.temporary.isValid());
            const auto target = QDir(tree.temporary.path()).filePath(
                QStringLiteral("target.lua")
            );
            QVERIFY(writeFile(target, QByteArrayLiteral("target\n")));
            QVERIFY(makeSymlink(target, tree.entrypoint));
            QCOMPARE(DeferredActivationBackend(tree.configRoot, tree.entrypoint)
                         .status().entrypointKind,
                     EntrypointKind::Unsafe);
        }
        {
            EntrypointTree tree;
            QVERIFY(tree.temporary.isValid());
            QVERIFY(::mkfifo(QFile::encodeName(tree.entrypoint).constData(), 0600)
                    == 0);
            QCOMPARE(DeferredActivationBackend(tree.configRoot, tree.entrypoint)
                         .status().entrypointKind,
                     EntrypointKind::Unsafe);
        }
        {
            EntrypointTree tree;
            QVERIFY(tree.temporary.isValid());
            const auto source = QDir(tree.temporary.path()).filePath(
                QStringLiteral("source.lua")
            );
            QVERIFY(writeFile(source, QByteArrayLiteral("hard linked\n")));
            QVERIFY(::link(
                        QFile::encodeName(source).constData(),
                        QFile::encodeName(tree.entrypoint).constData()
                    ) == 0);
            QCOMPARE(DeferredActivationBackend(tree.configRoot, tree.entrypoint)
                         .status().entrypointKind,
                     EntrypointKind::Unsafe);
        }
        {
            EntrypointTree tree;
            QVERIFY(tree.temporary.isValid());
            QVERIFY(writeFile(tree.entrypoint, QByteArrayLiteral("writable\n"), 0620));
            QCOMPARE(DeferredActivationBackend(tree.configRoot, tree.entrypoint)
                         .status().entrypointKind,
                     EntrypointKind::Unsafe);
        }
        {
            EntrypointTree tree;
            QVERIFY(tree.temporary.isValid());
            QFile oversized(tree.entrypoint);
            QVERIFY(oversized.open(QIODevice::WriteOnly | QIODevice::NewOnly));
            QVERIFY(oversized.resize(16 * 1024 * 1024 + 1));
            oversized.close();
            QCOMPARE(DeferredActivationBackend(tree.configRoot, tree.entrypoint)
                         .status().entrypointKind,
                     EntrypointKind::Unsafe);
        }
    }

    void inspectorRejectsUntrustedDirectoriesAndAncestors()
    {
        {
            EntrypointTree tree;
            QVERIFY(tree.temporary.isValid());
            QVERIFY(writeFile(tree.entrypoint, QByteArrayLiteral("entrypoint\n")));
            QVERIFY(::chmod(QFile::encodeName(tree.configRoot).constData(), 0770)
                    == 0);
            QCOMPARE(DeferredActivationBackend(tree.configRoot, tree.entrypoint)
                         .status().entrypointKind,
                     EntrypointKind::Unsafe);
        }
        {
            EntrypointTree tree;
            QVERIFY(tree.temporary.isValid());
            QVERIFY(writeFile(tree.entrypoint, QByteArrayLiteral("entrypoint\n")));
            QVERIFY(::chmod(
                        QFile::encodeName(tree.temporary.path()).constData(),
                        0770
                    ) == 0);
            QCOMPARE(DeferredActivationBackend(tree.configRoot, tree.entrypoint)
                         .status().entrypointKind,
                     EntrypointKind::Unsafe);
        }
        {
            QTemporaryDir temporary;
            QVERIFY(temporary.isValid());
            QVERIFY(::chmod(QFile::encodeName(temporary.path()).constData(), 0700)
                    == 0);
            const auto actual = QDir(temporary.path()).filePath(
                QStringLiteral("actual-hypr")
            );
            QVERIFY(makeDirectory(actual));
            const auto linked = QDir(temporary.path()).filePath(
                QStringLiteral("hypr")
            );
            QVERIFY(makeSymlink(actual, linked));
            const auto entrypoint = QDir(linked).filePath(
                QStringLiteral("hyprland.lua")
            );
            QCOMPARE(DeferredActivationBackend(linked, entrypoint)
                         .status().entrypointKind,
                     EntrypointKind::Unsafe);
        }
        {
            QTemporaryDir temporary;
            QVERIFY(temporary.isValid());
            QVERIFY(::chmod(QFile::encodeName(temporary.path()).constData(), 0700)
                    == 0);
            const auto actual = QDir(temporary.path()).filePath(
                QStringLiteral("actual")
            );
            const auto actualParent = QDir(actual).filePath(
                QStringLiteral("parent")
            );
            const auto actualRoot = QDir(actualParent).filePath(
                QStringLiteral("hypr")
            );
            QVERIFY(makeDirectory(actualRoot));
            const auto link = QDir(temporary.path()).filePath(
                QStringLiteral("linked-ancestor")
            );
            QVERIFY(makeSymlink(actual, link));
            const auto linkedRoot = QDir(link).filePath(
                QStringLiteral("parent/hypr")
            );
            const auto entrypoint = QDir(linkedRoot).filePath(
                QStringLiteral("hyprland.lua")
            );
            QVERIFY(writeFile(
                QDir(actualRoot).filePath(QStringLiteral("hyprland.lua")),
                QByteArrayLiteral("must not follow ancestor\n")
            ));
            QCOMPARE(DeferredActivationBackend(linkedRoot, entrypoint)
                         .status().entrypointKind,
                     EntrypointKind::Unsafe);
        }
    }

    void inspectorRejectsForeignOwnerWhenPrivileged()
    {
        if (::geteuid() != 0) {
            QSKIP("Changing ownership requires a privileged test runner");
        }
        EntrypointTree tree;
        QVERIFY(tree.temporary.isValid());
        QVERIFY(writeFile(tree.entrypoint, QByteArrayLiteral("foreign\n")));
        QVERIFY(::chown(QFile::encodeName(tree.entrypoint).constData(), 1, -1)
                == 0);
        QCOMPARE(DeferredActivationBackend(tree.configRoot, tree.entrypoint)
                     .status().entrypointKind,
                 EntrypointKind::Unsafe);
    }

    void mutationCasChecksRunBeforeAuthorityCalls()
    {
        const auto initial = dirtySnapshot();
        ServiceHarness harness(initial);
        const QByteArray candidate{"candidate"};

        QCOMPARE(harness.service->ReplaceSnapshot(
                     initial.revision + 1,
                     initial.catalogDigest,
                     initial.actionCatalogDigest,
                     candidate
                 ),
                 qulonglong(initial.revision));
        QCOMPARE(harness.service->ReplaceSnapshot(
                     initial.revision,
                     QStringLiteral("wrong-catalog"),
                     initial.actionCatalogDigest,
                     candidate
                 ),
                 qulonglong(initial.revision));
        QCOMPARE(harness.service->ReplaceSnapshot(
                     initial.revision,
                     initial.catalogDigest,
                     QStringLiteral("wrong-actions"),
                     candidate
                 ),
                 qulonglong(initial.revision));
        QCOMPARE(harness.authority->replaceCalls, 0);

        auto next = initial;
        next.revision++;
        next.desiredState = candidate;
        harness.authority->replaceResult = {
            .success = true,
            .snapshot = next,
        };
        QCOMPARE(harness.service->ReplaceSnapshot(
                     initial.revision,
                     initial.catalogDigest,
                     initial.actionCatalogDigest,
                     candidate
                 ),
                 qulonglong(next.revision));
        QCOMPARE(harness.authority->replaceCalls, 1);
        QCOMPARE(harness.service->revision(), qulonglong(next.revision));
    }

    void recoveryUsesPreparedRequirementRatherThanDesiredRequirement()
    {
        auto initial = dirtySnapshot();
        initial.requiredActivation = ActivationRequirement::Session;
        ServiceHarness harness(initial);
        harness.backend->supported = {ActivationRequirement::Reload};
        harness.authority->prepareRecoveryResult = {
            .success = true,
            .snapshot = initial,
            .prepared = preparedGeneration(ActivationRequirement::Reload),
        };
        const auto recovered = committedSnapshot(
            6,
            QByteArrayLiteral("{\"revision\":\"6\",\"recovered\":true}\n")
        );
        harness.authority->commitResult = {
            .success = true,
            .snapshot = recovered,
        };
        harness.backend->activationResult = {
            .success = true,
            .activationMayHaveOccurred = true,
            .generation = QString::fromLatin1(generationId),
            .confirmedRequirement = ActivationRequirement::Reload,
            .receipt = {QByteArrayLiteral("rollback-token")},
            .status = managedStatus(),
        };

        qulonglong applied = 0;
        QString generation;
        QCOMPARE(harness.service->Recover(
                     initial.revision,
                     initial.catalogDigest,
                     initial.actionCatalogDigest,
                     applied,
                     generation
                 ),
                 qulonglong(6));
        QCOMPARE(applied, qulonglong(6));
        QCOMPARE(generation, QString::fromLatin1(generationId));
        QCOMPARE(harness.authority->prepareRecoveryCalls, 1);
        QCOMPARE(harness.backend->activateCalls, 1);
        QCOMPARE(harness.authority->commitCalls, 1);
        QCOMPARE(harness.authority->abortCalls, 0);
        QCOMPARE(harness.backend->capabilityChecks,
                 QVector{ActivationRequirement::Reload});
        QCOMPARE(harness.service->requiredActivation(), QStringLiteral("none"));
    }

    void replaceDelegatesPreviousTokenForLostResponseRetry()
    {
        auto current = dirtySnapshot();
        current.revision = 6;
        current.desiredState = QByteArrayLiteral("current revision six");
        ServiceHarness harness(current);
        harness.authority->replaceResult = {
            .success = true,
            .snapshot = current,
        };
        const QByteArray priorCandidate{"candidate embedding revision five"};

        QCOMPARE(harness.service->ReplaceSnapshot(
                     5,
                     current.catalogDigest,
                     current.actionCatalogDigest,
                     priorCandidate
                 ),
                 qulonglong(6));
        QCOMPARE(harness.authority->replaceCalls, 1);
        QCOMPARE(harness.authority->lastReplaceExpected, quint64(5));
        QCOMPARE(harness.authority->lastReplaceCandidate, priorCandidate);

        QCOMPARE(harness.service->ReplaceSnapshot(
                     4,
                     current.catalogDigest,
                     current.actionCatalogDigest,
                     QByteArrayLiteral("too old")
                 ),
                 qulonglong(6));
        QCOMPARE(harness.authority->replaceCalls, 1);
    }

    void uncertainReplacePublicationMakesServiceUnavailable()
    {
        const auto initial = dirtySnapshot();
        ServiceHarness harness(initial);
        harness.authority->replaceResult = {
            .success = false,
            .errorCode = QStringLiteral("PersistenceFailed"),
            .errorMessage = QStringLiteral("desired directory sync uncertain"),
            .snapshot = {
                .available = false,
                .writable = false,
                .loadState = QStringLiteral("unavailable"),
                .appliedRevision = initial.appliedRevision,
                .applyState = QStringLiteral("failed"),
                .generationDigest = initial.generationDigest,
            },
        };

        QCOMPARE(harness.service->ReplaceSnapshot(
                     initial.revision,
                     initial.catalogDigest,
                     initial.actionCatalogDigest,
                     QByteArrayLiteral("candidate")
                 ),
                 qulonglong(0));
        QCOMPARE(harness.authority->replaceCalls, 1);
        QVERIFY(!harness.service->available());
        QVERIFY(!harness.service->writable());
        QCOMPARE(harness.service->revision(), qulonglong(0));
        QCOMPARE(harness.service->appliedRevision(),
                 qulonglong(initial.appliedRevision));
        QCOMPARE(harness.service->generationDigest(), initial.generationDigest);
        QCOMPARE(harness.service->loadState(), QStringLiteral("unavailable"));
        QCOMPARE(harness.service->applyState(), QStringLiteral("failed"));
    }

    void applyRequiresAnUnchangedManagedEntrypoint()
    {
        const auto initial = dirtySnapshot();
        for (const auto &management : {
                 ManagementStatus{
                     .state = ManagementState::Unmanaged,
                     .entrypointKind = EntrypointKind::Absent,
                 },
                 ManagementStatus{
                     .state = ManagementState::Conflict,
                     .entrypointKind = EntrypointKind::Unsafe,
                 },
             }) {
            ServiceHarness harness(initial, management);
            QString generation;
            QCOMPARE(harness.service->Apply(
                         initial.revision,
                         initial.catalogDigest,
                         initial.actionCatalogDigest,
                         generation
                     ),
                     qulonglong(initial.appliedRevision));
            QCOMPARE(harness.authority->prepareApplyCalls, 0);
            QCOMPARE(harness.backend->activateCalls, 0);
            QCOMPARE(harness.authority->commitCalls, 0);
            QCOMPARE(generation, initial.generationDigest);
        }
    }

    void managedStateMustMatchAuthorityCommittedGeneration()
    {
        const auto initial = dirtySnapshot();
        ServiceHarness harness(
            initial,
            {
                .state = ManagementState::Managed,
                .entrypointKind = EntrypointKind::Regular,
                .entrypointDigest = QStringLiteral("canonical-loader-digest"),
                .managedGeneration = QStringLiteral("different-generation"),
                .managedNonce = QString::fromLatin1(activationNonce),
            }
        );

        QCOMPARE(harness.service->entrypointDigest(),
                 QStringLiteral("canonical-loader-digest"));
        QCOMPARE(harness.service->managementState(), QStringLiteral("conflict"));
        QString generation;
        QCOMPARE(harness.service->Apply(
                     initial.revision,
                     initial.catalogDigest,
                     initial.actionCatalogDigest,
                     generation
                 ),
                 qulonglong(initial.appliedRevision));
        QCOMPARE(harness.authority->prepareApplyCalls, 0);
        QCOMPARE(harness.backend->activateCalls, 0);
    }

    void appliedAuthorityNeverSurfacesRawUnmanagedState()
    {
        const auto initial = dirtySnapshot();
        ServiceHarness harness(
            initial,
            {
                .state = ManagementState::Unmanaged,
                .entrypointKind = EntrypointKind::Absent,
            }
        );
        QCOMPARE(harness.service->managementState(), QStringLiteral("conflict"));
        QVERIFY(harness.service->entrypointDigest().isEmpty());
    }

    void inactiveAuthorityNeverAcceptsRawManagedOwnership()
    {
        auto initial = dirtySnapshot();
        initial.appliedRevision = 0;
        initial.generationDigest.clear();
        ServiceHarness harness(
            initial,
            {
                .state = ManagementState::Managed,
                .entrypointKind = EntrypointKind::Regular,
                .entrypointDigest = QStringLiteral("managed-looking-loader"),
                .managedGeneration = QString::fromLatin1(generationId),
                .managedNonce = QString::fromLatin1(activationNonce),
            }
        );

        QCOMPARE(harness.service->entrypointDigest(),
                 QStringLiteral("managed-looking-loader"));
        QCOMPARE(harness.service->managementState(), QStringLiteral("conflict"));
    }

    void adoptionProofDistinguishesAbsenceFromExactRegularFile()
    {
        auto initial = dirtySnapshot();
        initial.appliedRevision = 0;
        initial.generationDigest.clear();
        const auto runSuccessfulAdoption = [initial](
            const ManagementStatus &management,
            const QString &proof
        ) {
            ServiceHarness harness(initial, management);
            harness.authority->prepareApplyResult = {
                .success = true,
                .snapshot = initial,
                .prepared = preparedGeneration(),
            };
            harness.authority->commitResult = {
                .success = true,
                .snapshot = committedSnapshot(),
            };
            harness.backend->adoptionResult = {
                .success = true,
                .activationMayHaveOccurred = true,
                .generation = QString::fromLatin1(generationId),
                .confirmedRequirement = ActivationRequirement::Reload,
                .receipt = {QByteArrayLiteral("adoption-rollback")},
                .status = {
                    .state = ManagementState::Managed,
                    .entrypointKind = EntrypointKind::Regular,
                    .entrypointDigest = QStringLiteral("managed-loader-digest"),
                },
            };

            QString generation;
            QString entrypoint;
            const auto applied = harness.service->AdoptManagedConfiguration(
                initial.revision,
                initial.catalogDigest,
                initial.actionCatalogDigest,
                proof,
                generation,
                entrypoint
            );
            return std::tuple{
                applied,
                generation,
                entrypoint,
                harness.authority->prepareApplyCalls,
                harness.backend->adoptCalls,
                harness.backend->activateCalls,
                harness.authority->commitCalls
            };
        };

        auto [absentApplied, absentGeneration, absentEntrypoint,
              absentPrepare, absentAdopt, absentActivate, absentCommit]
            = runSuccessfulAdoption(
                {
                    .state = ManagementState::Unmanaged,
                    .entrypointKind = EntrypointKind::Absent,
                },
                QString{}
            );
        QCOMPARE(absentApplied, qulonglong(initial.revision));
        QCOMPARE(absentGeneration, QString::fromLatin1(generationId));
        QCOMPARE(absentEntrypoint, QStringLiteral("managed-loader-digest"));
        QCOMPARE(absentPrepare, 1);
        QCOMPARE(absentAdopt, 1);
        QCOMPARE(absentActivate, 0);
        QCOMPARE(absentCommit, 1);

        const QString existingDigest = QString(64, QLatin1Char('c'));
        auto [regularApplied, regularGeneration, regularEntrypoint,
              regularPrepare, regularAdopt, regularActivate, regularCommit]
            = runSuccessfulAdoption(
                {
                    .state = ManagementState::Unmanaged,
                    .entrypointKind = EntrypointKind::Regular,
                    .entrypointDigest = existingDigest,
                },
                existingDigest
            );
        QCOMPARE(regularApplied, qulonglong(initial.revision));
        QCOMPARE(regularGeneration, QString::fromLatin1(generationId));
        QCOMPARE(regularEntrypoint, QStringLiteral("managed-loader-digest"));
        QCOMPARE(regularPrepare, 1);
        QCOMPARE(regularAdopt, 1);
        QCOMPARE(regularActivate, 0);
        QCOMPARE(regularCommit, 1);

        for (const auto &[management, proof] : QVector<QPair<ManagementStatus, QString>>{
                 {{
                      .state = ManagementState::Unmanaged,
                      .entrypointKind = EntrypointKind::Absent,
                  }, existingDigest},
                 {{
                      .state = ManagementState::Unmanaged,
                      .entrypointKind = EntrypointKind::Regular,
                      .entrypointDigest = existingDigest,
                  }, QString{}},
                 {{
                      .state = ManagementState::Unmanaged,
                      .entrypointKind = EntrypointKind::Regular,
                      .entrypointDigest = existingDigest,
                  }, QString(64, QLatin1Char('d'))},
                 {{
                      .state = ManagementState::Conflict,
                      .entrypointKind = EntrypointKind::Unsafe,
                  }, QString{}},
             }) {
            ServiceHarness harness(initial, management);
            QString generation;
            QString entrypoint;
            QCOMPARE(harness.service->AdoptManagedConfiguration(
                         initial.revision,
                         initial.catalogDigest,
                         initial.actionCatalogDigest,
                         proof,
                         generation,
                         entrypoint
                     ),
                     qulonglong(initial.appliedRevision));
            QCOMPARE(harness.authority->prepareApplyCalls, 0);
            QCOMPARE(harness.backend->adoptCalls, 0);
            QCOMPARE(harness.authority->commitCalls, 0);
        }
    }

    void unsupportedPreparedRecoveryAbortsWithoutPublishing()
    {
        const auto initial = dirtySnapshot();
        ServiceHarness harness(initial);
        harness.backend->supported = {ActivationRequirement::Reload};
        harness.authority->prepareRecoveryResult = {
            .success = true,
            .snapshot = initial,
            .prepared = preparedGeneration(ActivationRequirement::Session),
        };
        harness.authority->abortResult = {
            .success = true,
            .snapshot = initial,
        };

        qulonglong applied = 99;
        QString generation = QStringLiteral("sentinel");
        QCOMPARE(harness.service->Recover(
                     initial.revision,
                     initial.catalogDigest,
                     initial.actionCatalogDigest,
                     applied,
                     generation
                 ),
                 qulonglong(initial.revision));
        QCOMPARE(applied, qulonglong(initial.appliedRevision));
        QCOMPARE(generation, initial.generationDigest);
        QCOMPARE(harness.authority->prepareRecoveryCalls, 1);
        QCOMPARE(harness.authority->abortCalls, 1);
        QCOMPARE(harness.backend->activateCalls, 0);
        QCOMPARE(harness.authority->commitCalls, 0);
        QCOMPARE(harness.service->revision(), qulonglong(initial.revision));
        QCOMPARE(harness.service->appliedRevision(),
                 qulonglong(initial.appliedRevision));
        QCOMPARE(harness.service->generationDigest(), initial.generationDigest);
        QCOMPARE(harness.service->requiredActivation(), QStringLiteral("reload"));
    }

    void successfulActivationWithCommitFailurePreservesPendingConflict()
    {
        const auto initial = dirtySnapshot();
        ServiceHarness harness(initial);
        harness.authority->prepareApplyResult = {
            .success = true,
            .snapshot = initial,
            .prepared = preparedGeneration(),
        };
        harness.authority->commitResult = {
            .success = false,
            .commitDecisionDurable = true,
            .commitDecisionMayExist = true,
            .errorCode = QStringLiteral("PersistenceFailed"),
            .errorMessage = QStringLiteral("commit failed"),
            .snapshot = initial,
        };
        harness.backend->activationResult = {
            .success = true,
            .activationMayHaveOccurred = true,
            .generation = QString::fromLatin1(generationId),
            .confirmedRequirement = ActivationRequirement::Reload,
            .receipt = {QByteArrayLiteral("rollback-token")},
            .status = managedStatus(),
        };
        QString generation;
        QCOMPARE(harness.service->Apply(
                     initial.revision,
                     initial.catalogDigest,
                     initial.actionCatalogDigest,
                     generation
                 ),
                 qulonglong(initial.appliedRevision));
        QCOMPARE(generation, initial.generationDigest);
        QCOMPARE(harness.backend->activateCalls, 1);
        QCOMPARE(harness.authority->commitCalls, 1);
        QCOMPARE(harness.backend->rollbackCalls, 0);
        QCOMPARE(harness.authority->abortCalls, 0);
        QVERIFY(!harness.service->available());
        QVERIFY(!harness.service->writable());
        QCOMPARE(harness.service->revision(), qulonglong(0));
        QCOMPARE(harness.service->appliedRevision(),
                 qulonglong(initial.appliedRevision));
        QCOMPARE(harness.service->generationDigest(), initial.generationDigest);
        QCOMPARE(harness.service->loadState(), QStringLiteral("unavailable"));
        QCOMPARE(harness.service->applyState(), QStringLiteral("failed"));
        QCOMPARE(harness.service->managementState(), QStringLiteral("conflict"));
        QVERIFY(harness.service->entrypointDigest().isEmpty());
    }

    void committedActivationFinalizeFailureIsOneWayAndObservable()
    {
        const auto initial = dirtySnapshot();
        ServiceHarness harness(initial);
        harness.authority->prepareApplyResult = {
            .success = true,
            .snapshot = initial,
            .prepared = preparedGeneration(),
        };
        harness.authority->commitResult = {
            .success = true,
            .commitDecisionDurable = true,
            .snapshot = committedSnapshot(),
        };
        harness.backend->activationResult = {
            .success = true,
            .activationMayHaveOccurred = true,
            .generation = QString::fromLatin1(generationId),
            .confirmedRequirement = ActivationRequirement::Reload,
            .receipt = {QByteArrayLiteral("durable-bridge-token")},
            .status = managedStatus(),
        };
        harness.backend->finalizeResultConfigured = true;
        harness.backend->finalizeResult = {
            .success = false,
            .errorCode = QStringLiteral("PersistenceFailed"),
            .errorMessage = QStringLiteral("bridge directory sync failed"),
            .status = managedStatus(),
        };

        QString generation;
        QCOMPARE(harness.service->Apply(
                     initial.revision,
                     initial.catalogDigest,
                     initial.actionCatalogDigest,
                     generation
                 ),
                 qulonglong(initial.revision));
        QCOMPARE(generation, initial.generationDigest);
        QCOMPARE(harness.authority->commitCalls, 1);
        QCOMPARE(harness.backend->finalizeCalls, 1);
        QCOMPARE(harness.backend->lastFinalizeToken,
                 QByteArrayLiteral("durable-bridge-token"));
        QCOMPARE(harness.backend->lastFinalizeGeneration,
                 QString::fromLatin1(generationId));
        QCOMPARE(harness.backend->rollbackCalls, 0);
        QCOMPARE(harness.authority->abortCalls, 0);

        QVERIFY(!harness.service->available());
        QVERIFY(!harness.service->writable());
        QCOMPARE(harness.service->revision(), qulonglong(0));
        QCOMPARE(harness.service->appliedRevision(), qulonglong(initial.revision));
        QCOMPARE(harness.service->generationDigest(),
                 QString::fromLatin1(generationId));
        QCOMPARE(harness.service->loadState(), QStringLiteral("unavailable"));
        QCOMPARE(harness.service->applyState(), QStringLiteral("failed"));
        QCOMPARE(harness.service->managementState(), QStringLiteral("conflict"));
    }

    void startupReconciliationFailureKeepsFailClosedServiceAlive()
    {
        const auto initial = committedSnapshot();
        auto ownedBackend = std::make_unique<FakeActivationBackend>();
        auto *backend = ownedBackend.get();
        backend->reconcileResultConfigured = true;
        backend->reconcileResult = {
            .success = false,
            .errorCode = QStringLiteral("PersistenceFailed"),
            .errorMessage = QStringLiteral("pending bridge is inconsistent"),
            .status = {
                .state = ManagementState::Conflict,
                .entrypointKind = EntrypointKind::Unsafe,
            },
        };
        CompositorService service(
            std::move(ownedBackend),
            QDBusConnection(QStringLiteral("startup-reconcile-failure-test"))
        );

        auto ownedAuthority = std::make_unique<FakeAuthority>();
        auto *authority = ownedAuthority.get();
        authority->initializeResult = {
            .success = true,
            .snapshot = initial,
        };
        QString error;
        QVERIFY(service.initializeAuthority(std::move(ownedAuthority), error));
        QCOMPARE(backend->reconcileCalls, 1);
        QCOMPARE(backend->lastReconcileGeneration, initial.generationDigest);
        QVERIFY(!service.available());
        QVERIFY(!service.writable());
        QCOMPARE(service.revision(), qulonglong(0));
        QCOMPARE(service.appliedRevision(), qulonglong(initial.appliedRevision));
        QCOMPARE(service.generationDigest(), initial.generationDigest);
        QCOMPARE(service.loadState(), QStringLiteral("unavailable"));
        QCOMPARE(service.applyState(), QStringLiteral("failed"));
        QCOMPARE(service.managementState(), QStringLiteral("conflict"));

        const auto statusCalls = backend->statusCalls;
        QCOMPARE(service.ReplaceSnapshot(
                     initial.revision,
                     initial.catalogDigest,
                     initial.actionCatalogDigest,
                     initial.desiredState
                 ),
                 qulonglong(0));
        QString generation;
        QCOMPARE(service.Apply(
                     initial.revision,
                     initial.catalogDigest,
                     initial.actionCatalogDigest,
                     generation
                 ),
                 qulonglong(initial.appliedRevision));
        qulonglong recoveredApplied = 0;
        QCOMPARE(service.Recover(
                     initial.revision,
                     initial.catalogDigest,
                     initial.actionCatalogDigest,
                     recoveredApplied,
                     generation
                 ),
                 qulonglong(0));
        QString entrypoint;
        QCOMPARE(service.AdoptManagedConfiguration(
                     initial.revision,
                     initial.catalogDigest,
                     initial.actionCatalogDigest,
                     QString{},
                     generation,
                     entrypoint
                 ),
                 qulonglong(initial.appliedRevision));
        QCOMPARE(authority->replaceCalls, 0);
        QCOMPARE(authority->prepareApplyCalls, 0);
        QCOMPARE(authority->prepareRecoveryCalls, 0);
        QCOMPARE(backend->activateCalls, 0);
        QCOMPARE(backend->adoptCalls, 0);
        QCOMPARE(backend->statusCalls, statusCalls);
    }

    void startupBindsAuthorityFilesystemBeforeReconciliation()
    {
        const auto initial = dirtySnapshot();
        ServiceHarness harness(initial);
        QCOMPARE(harness.startupTrace,
                 QStringList({
                     QStringLiteral("authority-initialize"),
                     QStringLiteral("authority-context"),
                     QStringLiteral("backend-bind"),
                     QStringLiteral("backend-reconcile"),
                 }));
        QCOMPARE(harness.authority->filesystemContextCalls, 1);
        QCOMPARE(harness.backend->bindCalls, 1);
        QVERIFY(!harness.backend->lastFilesystemContextComplete);
        QCOMPARE(harness.backend->reconcileCalls, 1);
    }

    void filesystemContextErrorKeepsServiceAliveWithoutBackendMutation()
    {
        const auto initial = dirtySnapshot();
        auto ownedBackend = std::make_unique<FakeActivationBackend>();
        auto *backend = ownedBackend.get();
        CompositorService service(
            std::move(ownedBackend),
            QDBusConnection(QStringLiteral("filesystem-context-error-test"))
        );
        auto ownedAuthority = std::make_unique<FakeAuthority>();
        auto *authority = ownedAuthority.get();
        authority->initializeResult = {
            .success = true,
            .snapshot = initial,
        };
        authority->filesystemContextSuccess = false;
        authority->filesystemContextError =
            QStringLiteral("root descriptor duplication failed");

        QString error;
        QVERIFY(service.initializeAuthority(std::move(ownedAuthority), error));
        QCOMPARE(authority->filesystemContextCalls, 1);
        QCOMPARE(backend->bindCalls, 0);
        QCOMPARE(backend->reconcileCalls, 0);
        QVERIFY(!service.available());
        QVERIFY(!service.writable());
        QCOMPARE(service.appliedRevision(), qulonglong(initial.appliedRevision));
        QCOMPARE(service.generationDigest(), initial.generationDigest);
        QCOMPARE(service.applyState(), QStringLiteral("failed"));
        QCOMPARE(service.managementState(), QStringLiteral("conflict"));
    }

    void liveBackendMayRejectAbsentFilesystemContextWithoutRestartLoop()
    {
        const auto initial = dirtySnapshot();
        auto ownedBackend = std::make_unique<FakeActivationBackend>();
        auto *backend = ownedBackend.get();
        backend->bindResultConfigured = true;
        backend->bindResult = {
            .success = false,
            .errorCode = QStringLiteral("PersistenceFailed"),
            .errorMessage = QStringLiteral("authority context is absent"),
            .status = {
                .state = ManagementState::Conflict,
                .entrypointKind = EntrypointKind::Unsafe,
            },
        };
        CompositorService service(
            std::move(ownedBackend),
            QDBusConnection(QStringLiteral("filesystem-context-absent-test"))
        );
        auto ownedAuthority = std::make_unique<FakeAuthority>();
        auto *authority = ownedAuthority.get();
        authority->initializeResult = {
            .success = true,
            .snapshot = initial,
        };

        QString error;
        QVERIFY(service.initializeAuthority(std::move(ownedAuthority), error));
        QCOMPARE(authority->filesystemContextCalls, 1);
        QCOMPARE(backend->bindCalls, 1);
        QVERIFY(!backend->lastFilesystemContextComplete);
        QCOMPARE(backend->reconcileCalls, 0);
        QVERIFY(!service.available());
        QCOMPARE(service.managementState(), QStringLiteral("conflict"));
    }

    void authorityInitializationFailureStillFailsStartupBeforeReconciliation()
    {
        auto ownedBackend = std::make_unique<FakeActivationBackend>();
        auto *backend = ownedBackend.get();
        CompositorService service(
            std::move(ownedBackend),
            QDBusConnection(QStringLiteral("authority-init-failure-test"))
        );
        auto ownedAuthority = std::make_unique<FakeAuthority>();
        ownedAuthority->initializeResult = {
            .success = false,
            .errorCode = QStringLiteral("PersistenceFailed"),
            .errorMessage = QStringLiteral("authority lease failed"),
        };
        QString error;
        QVERIFY(!service.initializeAuthority(std::move(ownedAuthority), error));
        QCOMPARE(error, QStringLiteral("authority lease failed"));
        QCOMPARE(backend->bindCalls, 0);
        QCOMPARE(backend->reconcileCalls, 0);
        QVERIFY(!service.available());
    }

    void externalEntrypointChangesPublishIndependentManagementProperties()
    {
        EntrypointTree tree;
        QVERIFY(tree.temporary.isValid());
        auto initial = dirtySnapshot();
        initial.appliedRevision = 0;
        initial.generationDigest.clear();
        ServiceHarness harness(
            initial,
            {
                .state = ManagementState::Unmanaged,
                .entrypointKind = EntrypointKind::Absent,
            },
            tree.entrypoint
        );
        QSignalSpy published(
            harness.service.get(),
            &CompositorService::propertiesPublished
        );
        QVERIFY(published.isValid());

        const QByteArray firstBytes{"-- first external entrypoint\n"};
        const auto firstDigest = sha256(firstBytes);
        harness.backend->statusValue = {
            .state = ManagementState::Unmanaged,
            .entrypointKind = EntrypointKind::Regular,
            .entrypointDigest = firstDigest,
        };
        QVERIFY(writeFile(tree.entrypoint, firstBytes));
        QTRY_COMPARE(harness.service->entrypointDigest(), firstDigest);
        QTRY_VERIFY(!published.isEmpty());
        auto changed = published.last().at(0).toMap();
        QCOMPARE(changed.value(QStringLiteral("EntrypointDigest")).toString(),
                 firstDigest);
        QVERIFY(!changed.contains(QStringLiteral("ManagementState")));

        published.clear();
        harness.backend->statusValue = {
            .state = ManagementState::Conflict,
            .entrypointKind = EntrypointKind::Regular,
            .entrypointDigest = firstDigest,
        };
        QVERIFY(::chmod(QFile::encodeName(tree.entrypoint).constData(), 0660) == 0);
        QTRY_COMPARE(harness.service->managementState(), QStringLiteral("conflict"));
        QTRY_VERIFY(!published.isEmpty());
        changed = published.last().at(0).toMap();
        QCOMPARE(changed.value(QStringLiteral("ManagementState")).toString(),
                 QStringLiteral("conflict"));
        QVERIFY(!changed.contains(QStringLiteral("EntrypointDigest")));

        const QByteArray replacementBytes{"-- replacement entrypoint\n"};
        const auto replacementDigest = sha256(replacementBytes);
        const auto replacement = QDir(tree.configRoot).filePath(
            QStringLiteral("replacement.lua")
        );
        QVERIFY(writeFile(replacement, replacementBytes));
        published.clear();
        harness.backend->statusValue = {
            .state = ManagementState::Unmanaged,
            .entrypointKind = EntrypointKind::Regular,
            .entrypointDigest = replacementDigest,
        };
        QVERIFY(::rename(
            QFile::encodeName(replacement).constData(),
            QFile::encodeName(tree.entrypoint).constData()
        ) == 0);
        QTRY_COMPARE(harness.service->managementState(), QStringLiteral("unmanaged"));
        QTRY_COMPARE(harness.service->entrypointDigest(), replacementDigest);
        QTRY_VERIFY(!published.isEmpty());
        changed = published.last().at(0).toMap();
        QCOMPARE(changed.value(QStringLiteral("ManagementState")).toString(),
                 QStringLiteral("unmanaged"));
        QCOMPARE(changed.value(QStringLiteral("EntrypointDigest")).toString(),
                 replacementDigest);

        published.clear();
        harness.backend->statusValue = {
            .state = ManagementState::Unmanaged,
            .entrypointKind = EntrypointKind::Absent,
        };
        QVERIFY(QFile::remove(tree.entrypoint));
        QTRY_VERIFY(harness.service->entrypointDigest().isEmpty());
        QTRY_VERIFY(!published.isEmpty());
        changed = published.last().at(0).toMap();
        QCOMPARE(changed.value(QStringLiteral("EntrypointDigest")).toString(),
                 QString{});
        QVERIFY(!changed.contains(QStringLiteral("ManagementState")));
    }

    void uncertainCommitMarkerNeverRollsBackOrAborts()
    {
        const auto initial = dirtySnapshot();
        ServiceHarness harness(initial);
        harness.authority->prepareApplyResult = {
            .success = true,
            .snapshot = initial,
            .prepared = preparedGeneration(),
        };
        harness.authority->commitResult = {
            .success = false,
            .commitDecisionDurable = false,
            .commitDecisionMayExist = true,
            .errorCode = QStringLiteral("PersistenceFailed"),
            .errorMessage = QStringLiteral("committing marker sync uncertain"),
            .snapshot = {
                .available = false,
                .writable = false,
                .loadState = QStringLiteral("unavailable"),
                .appliedRevision = initial.appliedRevision,
                .applyState = QStringLiteral("failed"),
                .generationDigest = initial.generationDigest,
            },
        };
        harness.backend->activationResult = {
            .success = true,
            .activationMayHaveOccurred = true,
            .generation = QString::fromLatin1(generationId),
            .confirmedRequirement = ActivationRequirement::Reload,
            .receipt = {QByteArrayLiteral("must-not-rollback")},
            .status = managedStatus(),
        };

        QString generation;
        QCOMPARE(harness.service->Apply(
                     initial.revision,
                     initial.catalogDigest,
                     initial.actionCatalogDigest,
                     generation
                 ),
                 qulonglong(initial.appliedRevision));
        QCOMPARE(generation, initial.generationDigest);
        QCOMPARE(harness.backend->activateCalls, 1);
        QCOMPARE(harness.authority->commitCalls, 1);
        QCOMPARE(harness.backend->rollbackCalls, 0);
        QCOMPARE(harness.authority->abortCalls, 0);
        QVERIFY(!harness.service->available());
        QVERIFY(!harness.service->writable());
        QCOMPARE(harness.service->revision(), qulonglong(0));
        QCOMPARE(harness.service->appliedRevision(),
                 qulonglong(initial.appliedRevision));
        QCOMPARE(harness.service->generationDigest(), initial.generationDigest);
        QCOMPARE(harness.service->loadState(), QStringLiteral("unavailable"));
        QCOMPARE(harness.service->applyState(), QStringLiteral("failed"));
        QCOMPARE(harness.service->managementState(), QStringLiteral("conflict"));
    }

    void failedCommitMarkerRollsBackThenAbortsPreparedTransaction()
    {
        const auto initial = dirtySnapshot();
        ServiceHarness harness(initial);
        harness.authority->prepareApplyResult = {
            .success = true,
            .snapshot = initial,
            .prepared = preparedGeneration(),
        };
        harness.authority->commitResult = {
            .success = false,
            .commitDecisionDurable = false,
            .errorCode = QStringLiteral("PersistenceFailed"),
            .errorMessage = QStringLiteral("committing marker failed"),
            .snapshot = initial,
        };
        harness.authority->abortResult = {
            .success = true,
            .snapshot = initial,
        };
        harness.backend->activationResult = {
            .success = true,
            .activationMayHaveOccurred = true,
            .generation = QString::fromLatin1(generationId),
            .confirmedRequirement = ActivationRequirement::Reload,
            .receipt = {QByteArrayLiteral("exact-rollback-token")},
            .status = managedStatus(),
        };
        harness.backend->rollbackResult = {
            .success = true,
            .status = managedStatus(),
        };

        QString generation;
        QCOMPARE(harness.service->Apply(
                     initial.revision,
                     initial.catalogDigest,
                     initial.actionCatalogDigest,
                     generation
                 ),
                 qulonglong(initial.appliedRevision));
        QCOMPARE(generation, initial.generationDigest);
        QCOMPARE(harness.backend->activateCalls, 1);
        QCOMPARE(harness.authority->commitCalls, 1);
        QCOMPARE(harness.backend->rollbackCalls, 1);
        QCOMPARE(harness.backend->lastRollbackToken,
                 QByteArrayLiteral("exact-rollback-token"));
        QCOMPARE(harness.authority->abortCalls, 1);
        QCOMPARE(harness.backend->calls,
                 QStringList({QStringLiteral("activate"),
                              QStringLiteral("rollback")}));
        QCOMPARE(harness.authority->calls,
                 QStringList({QStringLiteral("initialize"),
                              QStringLiteral("prepare-apply"),
                              QStringLiteral("commit"),
                              QStringLiteral("abort")}));
        QVERIFY(harness.service->available());
        QVERIFY(harness.service->writable());
        QCOMPARE(harness.service->revision(), qulonglong(initial.revision));
        QCOMPARE(harness.service->appliedRevision(),
                 qulonglong(initial.appliedRevision));
        QCOMPARE(harness.service->generationDigest(), initial.generationDigest);
        QCOMPARE(harness.service->loadState(), initial.loadState);
        QCOMPARE(harness.service->applyState(), initial.applyState);
        QCOMPARE(harness.service->requiredActivation(), QStringLiteral("reload"));
        QCOMPARE(harness.service->managementState(), QStringLiteral("managed"));
    }

    void failedCommitMarkerRollbackOrAbortFailurePublishesConflict()
    {
        for (const bool rollbackSucceeds : {false, true}) {
            const auto initial = dirtySnapshot();
            ServiceHarness harness(initial);
            harness.authority->prepareApplyResult = {
                .success = true,
                .snapshot = initial,
                .prepared = preparedGeneration(),
            };
            harness.authority->commitResult = {
                .success = false,
                .commitDecisionDurable = false,
                .errorCode = QStringLiteral("PersistenceFailed"),
                .errorMessage = QStringLiteral("committing marker failed"),
                .snapshot = initial,
            };
            harness.authority->abortResult = {
                .success = false,
                .errorCode = QStringLiteral("PersistenceFailed"),
                .errorMessage = QStringLiteral("abort failed"),
                .snapshot = initial,
            };
            harness.backend->activationResult = {
                .success = true,
                .activationMayHaveOccurred = true,
                .generation = QString::fromLatin1(generationId),
                .confirmedRequirement = ActivationRequirement::Reload,
                .receipt = {QByteArrayLiteral("exact-rollback-token")},
                .status = managedStatus(),
            };
            harness.backend->rollbackResult = {
                .success = rollbackSucceeds,
                .errorCode = rollbackSucceeds
                    ? QString{}
                    : QStringLiteral("ApplyFailed"),
                .errorMessage = rollbackSucceeds
                    ? QString{}
                    : QStringLiteral("rollback failed"),
                .status = rollbackSucceeds
                    ? managedStatus()
                    : ManagementStatus{
                          .state = ManagementState::Conflict,
                          .entrypointKind = EntrypointKind::Unsafe,
                      },
            };

            QString generation;
            QCOMPARE(harness.service->Apply(
                         initial.revision,
                         initial.catalogDigest,
                         initial.actionCatalogDigest,
                         generation
                     ),
                     qulonglong(initial.appliedRevision));
            QCOMPARE(harness.backend->rollbackCalls, 1);
            QCOMPARE(harness.backend->lastRollbackToken,
                     QByteArrayLiteral("exact-rollback-token"));
            QCOMPARE(harness.authority->abortCalls,
                     rollbackSucceeds ? 1 : 0);
            QVERIFY(!harness.service->available());
            QVERIFY(!harness.service->writable());
            QCOMPARE(harness.service->revision(), qulonglong(0));
            QCOMPARE(harness.service->appliedRevision(),
                     qulonglong(initial.appliedRevision));
            QCOMPARE(harness.service->generationDigest(),
                     initial.generationDigest);
            QCOMPARE(harness.service->loadState(), QStringLiteral("unavailable"));
            QCOMPARE(harness.service->applyState(), QStringLiteral("failed"));
            QCOMPARE(harness.service->managementState(),
                     QStringLiteral("conflict"));
            QVERIFY(harness.service->entrypointDigest().isEmpty());
        }
    }

    void rollbackFailurePublishesExplicitUnreconciledState()
    {
        const auto initial = dirtySnapshot();
        ServiceHarness harness(initial);
        harness.authority->prepareApplyResult = {
            .success = true,
            .snapshot = initial,
            .prepared = preparedGeneration(),
        };
        harness.backend->activationResult = {
            .success = false,
            .activationMayHaveOccurred = true,
            .errorCode = QStringLiteral("ApplyFailed"),
            .errorMessage = QStringLiteral("activation failed after publication"),
            .receipt = {QByteArrayLiteral("rollback-token")},
            .status = managedStatus(),
        };
        harness.backend->rollbackResult = {
            .success = false,
            .errorCode = QStringLiteral("ApplyFailed"),
            .errorMessage = QStringLiteral("rollback failed"),
            .status = {
                .state = ManagementState::Conflict,
                .entrypointKind = EntrypointKind::Unsafe,
            },
        };

        QString generation;
        QCOMPARE(harness.service->Apply(
                     initial.revision,
                     initial.catalogDigest,
                     initial.actionCatalogDigest,
                     generation
                 ),
                 qulonglong(initial.appliedRevision));
        QCOMPARE(harness.backend->rollbackCalls, 1);
        QCOMPARE(harness.authority->commitCalls, 0);
        QCOMPARE(harness.authority->abortCalls, 0);
        QVERIFY(!harness.service->available());
        QVERIFY(!harness.service->writable());
        QCOMPARE(harness.service->revision(), qulonglong(0));
        QCOMPARE(harness.service->appliedRevision(),
                 qulonglong(initial.appliedRevision));
        QCOMPARE(harness.service->generationDigest(), initial.generationDigest);
        QCOMPARE(harness.service->loadState(), QStringLiteral("unavailable"));
        QCOMPARE(harness.service->applyState(), QStringLiteral("failed"));
        QCOMPARE(harness.service->managementState(), QStringLiteral("conflict"));
        QVERIFY(harness.service->entrypointDigest().isEmpty());
    }

    void incompletePreparedIdentityFailsBeforeActivation()
    {
        const auto initial = dirtySnapshot();
        ServiceHarness harness(initial);
        auto incomplete = preparedGeneration();
        incomplete.directory.clear();
        incomplete.manifest.clear();
        harness.authority->prepareApplyResult = {
            .success = true,
            .snapshot = initial,
            .prepared = incomplete,
        };

        QString generation;
        QCOMPARE(harness.service->Apply(
                     initial.revision,
                     initial.catalogDigest,
                     initial.actionCatalogDigest,
                     generation
                 ),
                 qulonglong(initial.appliedRevision));
        QCOMPARE(harness.backend->activateCalls, 0);
        QCOMPARE(harness.authority->commitCalls, 0);
        QCOMPARE(harness.service->applyState(), QStringLiteral("failed"));
        QVERIFY(!harness.service->available());
    }

    void pendingDisplayQueriesUseProvedCacheAndConfirmProofIsDeadlineBound()
    {
        auto initial = committedSnapshot();
        initial.generationDigest = QStringLiteral("old-generation");
        int deadlineChecks = 0;
        QVector<int> ownerMaximumWaits;
        ServiceHarness harness(
            initial, managedStatus(), {},
            [&deadlineChecks](const QDeadlineTimer &) {
                ++deadlineChecks;
                switch (deadlineChecks) {
                case 1: return qint64(10'000);
                case 2: return qint64(9'999);
                case 3: return qint64(9'000);
                case 4: return qint64(8'000);
                case 5: return qint64(137);
                default: return qint64(136);
                }
            },
            [&ownerMaximumWaits](
                const QString &owner,
                const int maximumWaitMilliseconds
            ) {
                if (owner != QStringLiteral("in-process")) return false;
                ownerMaximumWaits.append(maximumWaitMilliseconds);
                return true;
            }
        );
        auto topology = connectedDisplayTopology();
        topology.document = QByteArrayLiteral("[{\"name\":\"DP-1\"}]\n");
        configureDisplayPreview(harness, initial, topology);
        const auto committed = committedSnapshot(
            6, QByteArrayLiteral("{\"revision\":\"6\"}\n")
        );
        harness.authority->commitResult = {
            .success = true,
            .commitDecisionDurable = true,
            .commitDecisionMayExist = true,
            .snapshot = committed,
        };
        auto target = managedStatus();
        target.managedGeneration = QString::fromLatin1(generationId);
        harness.backend->finalizeResultConfigured = true;
        harness.backend->finalizeResult = {
            .success = true,
            .status = target,
        };
        QSignalSpy published(
            harness.service.get(), &CompositorService::propertiesPublished
        );
        QVERIFY(published.isValid());

        QString token;
        qulonglong deadline = 0;
        QString previewGeneration;
        QCOMPARE(harness.service->PreviewDisplayConfiguration(
                     initial.revision,
                     initial.catalogDigest,
                     initial.actionCatalogDigest,
                     displayProfileBytes(topology),
                     10,
                     token,
                     deadline,
                     previewGeneration),
                 qulonglong(6));
        QCOMPARE(harness.backend->connectedCalls, 2);
        QVERIFY(harness.backend->connectedMaximumWaits.isEmpty());
        QVERIFY(!published.isEmpty());
        auto changed = published.last().at(0).toMap();
        QCOMPARE(changed.value(QStringLiteral("Writable")).toBool(), false);
        QCOMPARE(
            changed.value(QStringLiteral("DisplayConfirmationState")).toString(),
            QStringLiteral("awaiting-confirmation")
        );
        QCOMPARE(
            changed.value(QStringLiteral("DisplayConfirmationRevision"))
                .toULongLong(),
            qulonglong(6)
        );
        QCOMPARE(
            changed.value(QStringLiteral("DisplayConfirmationDeadlineMs"))
                .toULongLong(),
            deadline
        );
        QCOMPARE(
            changed.value(QStringLiteral("DisplayConfirmationGeneration"))
                .toString(),
            previewGeneration
        );
        QVERIFY(!changed.contains(QStringLiteral("Revision")));

        // UI hydration during the unsafe interval must be a non-blocking read
        // of the topology that was proved after activation. It must not start
        // another synchronous compositor IPC operation on the timer thread.
        qulonglong observedAtMs = 0;
        QCOMPARE(
            harness.service->GetConnectedDisplays(observedAtMs),
            topology.document
        );
        QVERIFY(observedAtMs > 0);
        QCOMPARE(harness.backend->connectedCalls, 2);

        published.clear();
        QString confirmedGeneration;
        QCOMPARE(harness.service->ConfirmDisplayConfiguration(
                     token, confirmedGeneration),
                 qulonglong(6));
        QCOMPARE(confirmedGeneration, QString::fromLatin1(generationId));
        QCOMPARE(harness.backend->connectedCalls, 3);
        QCOMPARE(harness.backend->connectedMaximumWaits.size(), 1);
        QCOMPARE(harness.backend->connectedMaximumWaits.front(), 8'000);
        QCOMPARE(deadlineChecks, 6);
        QCOMPARE(ownerMaximumWaits, QVector<int>({250, 137}));

        // The authority revision and complete terminal display tuple are
        // published together, so clients cannot observe revision N+1 while
        // retaining an actionable N+1 preview capability.
        QVERIFY(!published.isEmpty());
        changed = published.last().at(0).toMap();
        QCOMPARE(changed.value(QStringLiteral("Revision")).toULongLong(),
                 qulonglong(6));
        QCOMPARE(changed.value(QStringLiteral("AppliedRevision")).toULongLong(),
                 qulonglong(6));
        QCOMPARE(
            changed.value(QStringLiteral("DisplayConfirmationState")).toString(),
            QStringLiteral("idle")
        );
        QCOMPARE(
            changed.value(QStringLiteral("DisplayConfirmationRevision"))
                .toULongLong(),
            qulonglong(0)
        );
        QCOMPARE(
            changed.value(QStringLiteral("DisplayConfirmationDeadlineMs"))
                .toULongLong(),
            qulonglong(0)
        );
        QVERIFY(
            changed.value(QStringLiteral("DisplayConfirmationGeneration"))
                .toString().isEmpty()
        );

        observedAtMs = 0;
        QCOMPARE(
            harness.service->GetConnectedDisplays(observedAtMs),
            topology.document
        );
        QVERIFY(observedAtMs > 0);
        QCOMPARE(harness.backend->connectedCalls, 4);
    }

    void displayPreviewCleanupFailuresFailClosed_data()
    {
        QTest::addColumn<int>("scenario");
        QTest::addColumn<int>("expectedActivateCalls");
        QTest::addColumn<int>("expectedRollbackCalls");
        QTest::addColumn<int>("expectedAbortCalls");

        QTest::newRow("prepared-journal-failure") << 0 << 0 << 0 << 0;
        QTest::newRow("invalid-prepared-abort-failure") << 1 << 0 << 0 << 1;
        QTest::newRow("unsupported-requirement-abort-failure")
            << 2 << 0 << 0 << 1;
        QTest::newRow("activation-rollback-failure") << 3 << 1 << 1 << 0;
        QTest::newRow("post-realization-rollback-failure")
            << 4 << 1 << 1 << 0;
    }

    void displayPreviewCleanupFailuresFailClosed()
    {
        QFETCH(int, scenario);
        QFETCH(int, expectedActivateCalls);
        QFETCH(int, expectedRollbackCalls);
        QFETCH(int, expectedAbortCalls);

        auto initial = committedSnapshot();
        initial.generationDigest = QStringLiteral("old-generation");
        ServiceHarness harness(initial);
        const auto topology = connectedDisplayTopology();
        configureDisplayPreview(harness, initial, topology);
        const auto unavailable = unavailableSnapshot(initial);
        harness.authority->abortResult = {
            .success = false,
            .errorCode = QStringLiteral("PersistenceFailed"),
            .errorMessage = QStringLiteral("prepared journal retained"),
            .snapshot = unavailable,
        };

        if (scenario == 0) {
            harness.authority->prepareDisplayResult = {
                .success = false,
                .errorCode = QStringLiteral("PersistenceFailed"),
                .errorMessage = QStringLiteral("journal publication uncertain"),
                .snapshot = unavailable,
            };
        } else if (scenario == 1) {
            harness.authority->prepareDisplayResult.prepared->manifest.clear();
        } else if (scenario == 2) {
            harness.authority->prepareDisplayResult.prepared->requirement =
                ActivationRequirement::Restart;
        } else if (scenario == 3) {
            harness.backend->activationResult.success = false;
            harness.backend->activationResult.errorCode =
                QStringLiteral("ReloadFailed");
            harness.backend->activationResult.errorMessage =
                QStringLiteral("activation proof failed");
            harness.backend->rollbackResult = {
                .success = false,
                .errorCode = QStringLiteral("ApplyFailed"),
                .errorMessage = QStringLiteral("live rollback failed"),
                .status = {
                    .state = ManagementState::Conflict,
                    .entrypointKind = EntrypointKind::Unsafe,
                },
            };
        } else {
            auto badTopology = topology;
            badTopology.outputs.front().scale = 1.5;
            const auto goodResult = harness.backend->connectedResult;
            auto badResult = goodResult;
            badResult.topology = badTopology;
            harness.backend->connectedSequence = {goodResult, badResult};
            harness.backend->rollbackResult = {
                .success = false,
                .errorCode = QStringLiteral("ApplyFailed"),
                .errorMessage = QStringLiteral("live rollback failed"),
                .status = {
                    .state = ManagementState::Conflict,
                    .entrypointKind = EntrypointKind::Unsafe,
                },
            };
        }

        QSignalSpy published(
            harness.service.get(), &CompositorService::propertiesPublished
        );
        QVERIFY(published.isValid());
        QString token;
        qulonglong deadline = 0;
        QString generation;
        QCOMPARE(harness.service->PreviewDisplayConfiguration(
                     initial.revision,
                     initial.catalogDigest,
                     initial.actionCatalogDigest,
                     displayProfileBytes(topology),
                     10,
                     token,
                     deadline,
                     generation),
                 qulonglong(0));
        QVERIFY(token.isEmpty());
        QCOMPARE(deadline, qulonglong(0));
        QVERIFY(generation.isEmpty());
        QCOMPARE(harness.backend->activateCalls, expectedActivateCalls);
        QCOMPARE(harness.backend->rollbackCalls, expectedRollbackCalls);
        QCOMPARE(harness.authority->abortCalls, expectedAbortCalls);
        QVERIFY(!harness.service->available());
        QVERIFY(!harness.service->writable());
        QCOMPARE(harness.service->loadState(), QStringLiteral("unavailable"));
        QCOMPARE(harness.service->applyState(), QStringLiteral("failed"));
        QCOMPARE(harness.service->managementState(), QStringLiteral("conflict"));
        QCOMPARE(harness.service->displayConfirmationState(),
                 QStringLiteral("failed"));
        QCOMPARE(harness.service->displayConfirmationRevision(), qulonglong(0));
        QCOMPARE(harness.service->displayConfirmationDeadlineMs(), qulonglong(0));
        QVERIFY(harness.service->displayConfirmationGeneration().isEmpty());
        const auto connectedCallsBeforeFailedRead =
            harness.backend->connectedCalls;
        qulonglong observedAtMs = 99;
        QVERIFY(harness.service->GetConnectedDisplays(observedAtMs).isEmpty());
        QCOMPARE(observedAtMs, qulonglong(0));
        QCOMPARE(harness.backend->connectedCalls,
                 connectedCallsBeforeFailedRead);
        QVERIFY(!published.isEmpty());
        const auto changed = published.last().at(0).toMap();
        QCOMPARE(changed.value(QStringLiteral("Available")).toBool(), false);
        QCOMPARE(changed.value(QStringLiteral("ManagementState")).toString(),
                 QStringLiteral("conflict"));
        QCOMPARE(
            changed.value(QStringLiteral("DisplayConfirmationState")).toString(),
            QStringLiteral("failed")
        );
    }

    void displayAbortFailureAfterLiveRollbackFailsClosed()
    {
        auto initial = committedSnapshot();
        initial.generationDigest = QStringLiteral("old-generation");
        ServiceHarness harness(initial);
        const auto topology = connectedDisplayTopology();
        configureDisplayPreview(harness, initial, topology);
        QString token;
        qulonglong deadline = 0;
        QString generation;
        QCOMPARE(harness.service->PreviewDisplayConfiguration(
                     initial.revision,
                     initial.catalogDigest,
                     initial.actionCatalogDigest,
                     displayProfileBytes(topology),
                     10,
                     token,
                     deadline,
                     generation),
                 qulonglong(6));
        QSignalSpy published(
            harness.service.get(), &CompositorService::propertiesPublished
        );
        QVERIFY(published.isValid());
        harness.authority->abortResult = {
            .success = false,
            .errorCode = QStringLiteral("PersistenceFailed"),
            .errorMessage = QStringLiteral("prepared journal retained"),
            .snapshot = unavailableSnapshot(initial),
        };

        QCOMPARE(harness.service->RevertDisplayConfiguration(token),
                 qulonglong(0));
        QCOMPARE(harness.backend->rollbackCalls, 1);
        QCOMPARE(harness.authority->abortCalls, 1);
        QVERIFY(!harness.service->available());
        QVERIFY(!harness.service->writable());
        QCOMPARE(harness.service->loadState(), QStringLiteral("unavailable"));
        QCOMPARE(harness.service->applyState(), QStringLiteral("failed"));
        QCOMPARE(harness.service->managementState(), QStringLiteral("conflict"));
        QCOMPARE(harness.service->displayConfirmationState(),
                 QStringLiteral("failed"));
        QCOMPARE(harness.service->displayConfirmationRevision(), qulonglong(0));
        QCOMPARE(harness.service->displayConfirmationDeadlineMs(), qulonglong(0));
        QVERIFY(harness.service->displayConfirmationGeneration().isEmpty());
        QVERIFY(!published.isEmpty());
        const auto changed = published.last().at(0).toMap();
        QCOMPARE(changed.value(QStringLiteral("Available")).toBool(), false);
        QCOMPARE(changed.value(QStringLiteral("ManagementState")).toString(),
                 QStringLiteral("conflict"));
        QCOMPARE(
            changed.value(QStringLiteral("DisplayConfirmationState")).toString(),
            QStringLiteral("failed")
        );
        QCOMPARE(
            changed.value(QStringLiteral("DisplayConfirmationRevision"))
                .toULongLong(),
            qulonglong(0)
        );
    }

    void displayRollbackFailureDuringRevertPublishesOneFailedTuple()
    {
        auto initial = committedSnapshot();
        initial.generationDigest = QStringLiteral("old-generation");
        ServiceHarness harness(initial);
        const auto topology = connectedDisplayTopology();
        configureDisplayPreview(harness, initial, topology);
        QString token;
        qulonglong deadline = 0;
        QString generation;
        QCOMPARE(harness.service->PreviewDisplayConfiguration(
                     initial.revision,
                     initial.catalogDigest,
                     initial.actionCatalogDigest,
                     displayProfileBytes(topology),
                     10,
                     token,
                     deadline,
                     generation),
                 qulonglong(6));
        QSignalSpy published(
            harness.service.get(), &CompositorService::propertiesPublished
        );
        QVERIFY(published.isValid());
        harness.backend->rollbackResult = {
            .success = false,
            .errorCode = QStringLiteral("ApplyFailed"),
            .errorMessage = QStringLiteral("live rollback failed"),
            .status = {
                .state = ManagementState::Conflict,
                .entrypointKind = EntrypointKind::Unsafe,
            },
        };

        QCOMPARE(harness.service->RevertDisplayConfiguration(token),
                 qulonglong(0));
        QCOMPARE(harness.backend->rollbackCalls, 1);
        QCOMPARE(harness.authority->abortCalls, 0);
        QVERIFY(!harness.service->available());
        QCOMPARE(harness.service->managementState(), QStringLiteral("conflict"));
        QCOMPARE(harness.service->displayConfirmationState(),
                 QStringLiteral("failed"));
        QCOMPARE(harness.service->displayConfirmationRevision(), qulonglong(0));
        QCOMPARE(harness.service->displayConfirmationDeadlineMs(), qulonglong(0));
        QVERIFY(harness.service->displayConfirmationGeneration().isEmpty());
        QVERIFY(!published.isEmpty());
        const auto changed = published.last().at(0).toMap();
        QCOMPARE(changed.value(QStringLiteral("Available")).toBool(), false);
        QCOMPARE(changed.value(QStringLiteral("ManagementState")).toString(),
                 QStringLiteral("conflict"));
        QCOMPARE(
            changed.value(QStringLiteral("DisplayConfirmationState")).toString(),
            QStringLiteral("failed")
        );
        QCOMPARE(
            changed.value(QStringLiteral("DisplayConfirmationDeadlineMs"))
                .toULongLong(),
            qulonglong(0)
        );
    }

    void topologyOrReceiptDriftRevertsBeforeAuthorityAbort_data()
    {
        QTest::addColumn<bool>("topologyDrift");
        QTest::newRow("topology-drift") << true;
        QTest::newRow("receipt-target-drift") << false;
    }

    void topologyOrReceiptDriftRevertsBeforeAuthorityAbort()
    {
        QFETCH(bool, topologyDrift);
        auto initial = committedSnapshot();
        initial.generationDigest = QStringLiteral("old-generation");
        ServiceHarness harness(initial);
        const auto topology = connectedDisplayTopology();
        configureDisplayPreview(harness, initial, topology);
        bool abortObservedRollback = false;
        harness.authority->abortHook = [&] {
            abortObservedRollback = harness.backend->rollbackCalls == 1;
        };
        QString token;
        qulonglong deadline = 0;
        QString generation;
        QCOMPARE(harness.service->PreviewDisplayConfiguration(
                     initial.revision,
                     initial.catalogDigest,
                     initial.actionCatalogDigest,
                     displayProfileBytes(topology),
                     10,
                     token,
                     deadline,
                     generation),
                 qulonglong(6));
        if (topologyDrift) {
            auto drift = topology;
            drift.topologyDigest = QString(64, QLatin1Char('e'));
            harness.backend->connectedResult.topology = drift;
        } else {
            harness.backend->verifyPendingResult = {
                .success = false,
                .errorCode = QStringLiteral("VerificationFailed"),
                .errorMessage = QStringLiteral("receipt target changed"),
                .status = managedStatus(),
            };
        }

        QString confirmedGeneration;
        QCOMPARE(harness.service->ConfirmDisplayConfiguration(
                     token, confirmedGeneration),
                 qulonglong(initial.revision));
        QVERIFY(confirmedGeneration.isEmpty());
        QVERIFY(abortObservedRollback);
        QCOMPARE(harness.backend->rollbackCalls, 1);
        QCOMPARE(harness.authority->abortCalls, 1);
        QCOMPARE(harness.authority->commitCalls, 0);
        QCOMPARE(harness.service->displayConfirmationState(),
                 QStringLiteral("idle"));
        QCOMPARE(harness.service->revision(), qulonglong(initial.revision));
        QCOMPARE(harness.service->applyState(), QStringLiteral("current"));
        QCOMPARE(harness.service->managementState(), QStringLiteral("managed"));
    }

    void displayRollbackFailureDuringConfirmPublishesOneFailedTuple()
    {
        auto initial = committedSnapshot();
        initial.generationDigest = QStringLiteral("old-generation");
        ServiceHarness harness(initial);
        const auto topology = connectedDisplayTopology();
        configureDisplayPreview(harness, initial, topology);
        QString token;
        qulonglong deadline = 0;
        QString generation;
        QCOMPARE(harness.service->PreviewDisplayConfiguration(
                     initial.revision,
                     initial.catalogDigest,
                     initial.actionCatalogDigest,
                     displayProfileBytes(topology),
                     10,
                     token,
                     deadline,
                     generation),
                 qulonglong(6));
        auto drift = topology;
        drift.topologyDigest = QString(64, QLatin1Char('e'));
        harness.backend->connectedResult.topology = drift;
        harness.backend->rollbackResult = {
            .success = false,
            .errorCode = QStringLiteral("ApplyFailed"),
            .errorMessage = QStringLiteral("live rollback failed"),
            .status = {
                .state = ManagementState::Conflict,
                .entrypointKind = EntrypointKind::Unsafe,
            },
        };
        QSignalSpy published(
            harness.service.get(), &CompositorService::propertiesPublished
        );
        QVERIFY(published.isValid());

        QString confirmedGeneration;
        QCOMPARE(harness.service->ConfirmDisplayConfiguration(
                     token, confirmedGeneration),
                 qulonglong(0));
        QCOMPARE(harness.backend->rollbackCalls, 1);
        QCOMPARE(harness.authority->abortCalls, 0);
        QCOMPARE(harness.authority->commitCalls, 0);
        QVERIFY(!harness.service->available());
        QCOMPARE(harness.service->managementState(), QStringLiteral("conflict"));
        QCOMPARE(harness.service->displayConfirmationState(),
                 QStringLiteral("failed"));
        QCOMPARE(harness.service->displayConfirmationRevision(), qulonglong(0));
        QCOMPARE(harness.service->displayConfirmationDeadlineMs(), qulonglong(0));
        QVERIFY(harness.service->displayConfirmationGeneration().isEmpty());
        QVERIFY(!published.isEmpty());
        const auto changed = published.last().at(0).toMap();
        QCOMPARE(changed.value(QStringLiteral("Available")).toBool(), false);
        QCOMPARE(changed.value(QStringLiteral("ManagementState")).toString(),
                 QStringLiteral("conflict"));
        QCOMPARE(
            changed.value(QStringLiteral("DisplayConfirmationState")).toString(),
            QStringLiteral("failed")
        );
        QCOMPARE(
            changed.value(QStringLiteral("DisplayConfirmationGeneration"))
                .toString(),
            QString()
        );
    }

    void confirmationDeadlineWinsBeforeTopologyProof()
    {
        auto initial = committedSnapshot();
        initial.generationDigest = QStringLiteral("old-generation");
        int deadlineChecks = 0;
        ServiceHarness harness(
            initial, managedStatus(), {},
            [&deadlineChecks](const QDeadlineTimer &) {
                return ++deadlineChecks <= 2 ? qint64(10'000) : qint64(0);
            }
        );
        const auto topology = connectedDisplayTopology();
        configureDisplayPreview(harness, initial, topology);
        QString token;
        qulonglong deadline = 0;
        QString generation;
        QCOMPARE(harness.service->PreviewDisplayConfiguration(
                     initial.revision,
                     initial.catalogDigest,
                     initial.actionCatalogDigest,
                     displayProfileBytes(topology),
                     10,
                     token,
                     deadline,
                     generation),
                 qulonglong(6));

        QString confirmedGeneration;
        QCOMPARE(harness.service->ConfirmDisplayConfiguration(
                     token, confirmedGeneration),
                 qulonglong(initial.revision));
        QCOMPARE(deadlineChecks, 3);
        QVERIFY(harness.backend->connectedMaximumWaits.isEmpty());
        QCOMPARE(harness.backend->connectedCalls, 2);
        QCOMPARE(harness.authority->commitCalls, 0);
        QCOMPARE(harness.backend->rollbackCalls, 1);
        QCOMPARE(harness.authority->abortCalls, 1);
        QCOMPARE(harness.service->displayConfirmationState(),
                 QStringLiteral("idle"));
    }

    void confirmationDeadlineWinsAfterBoundedTopologyProof()
    {
        auto initial = committedSnapshot();
        initial.generationDigest = QStringLiteral("old-generation");
        int deadlineChecks = 0;
        ServiceHarness harness(
            initial, managedStatus(), {},
            [&deadlineChecks](const QDeadlineTimer &) {
                return ++deadlineChecks <= 4 ? qint64(7) : qint64(0);
            }
        );
        const auto topology = connectedDisplayTopology();
        configureDisplayPreview(harness, initial, topology);
        QString token;
        qulonglong deadline = 0;
        QString generation;
        QCOMPARE(harness.service->PreviewDisplayConfiguration(
                     initial.revision,
                     initial.catalogDigest,
                     initial.actionCatalogDigest,
                     displayProfileBytes(topology),
                     10,
                     token,
                     deadline,
                     generation),
                 qulonglong(6));

        QString confirmedGeneration;
        QCOMPARE(harness.service->ConfirmDisplayConfiguration(
                     token, confirmedGeneration),
                 qulonglong(initial.revision));
        QCOMPARE(deadlineChecks, 5);
        QCOMPARE(harness.backend->connectedMaximumWaits, QVector<int>{7});
        QCOMPARE(harness.backend->connectedCalls, 3);
        QCOMPARE(harness.authority->commitCalls, 0);
        QCOMPARE(harness.backend->rollbackCalls, 1);
        QCOMPARE(harness.authority->abortCalls, 1);
        QCOMPARE(harness.service->displayConfirmationState(),
                 QStringLiteral("idle"));
    }

    void previewDeadlineWinsBeforeOwnerLookupAndPublication()
    {
        auto initial = committedSnapshot();
        initial.generationDigest = QStringLiteral("old-generation");
        QStringList trace;
        ServiceHarness harness(
            initial, managedStatus(), {},
            [&trace](const QDeadlineTimer &) {
                trace.append(QStringLiteral("deadline"));
                return qint64(0);
            },
            [&trace](const QString &owner, const int maximumWaitMilliseconds) {
                trace.append(
                    QStringLiteral("owner:") + owner + QLatin1Char(':')
                        + QString::number(maximumWaitMilliseconds)
                );
                return true;
            }
        );
        const auto topology = connectedDisplayTopology();
        configureDisplayPreview(harness, initial, topology);
        QSignalSpy published(
            harness.service.get(), &CompositorService::propertiesPublished
        );
        QVERIFY(published.isValid());

        QString token;
        qulonglong deadline = 0;
        QString generation;
        QCOMPARE(harness.service->PreviewDisplayConfiguration(
                     initial.revision,
                     initial.catalogDigest,
                     initial.actionCatalogDigest,
                     displayProfileBytes(topology),
                     10,
                     token,
                     deadline,
                     generation),
                 qulonglong(initial.revision));
        QCOMPARE(trace, QStringList({QStringLiteral("deadline")}));
        QVERIFY(token.isEmpty());
        QCOMPARE(deadline, qulonglong(0));
        QVERIFY(generation.isEmpty());
        QCOMPARE(harness.backend->rollbackCalls, 1);
        QCOMPARE(harness.authority->abortCalls, 1);
        QCOMPARE(harness.authority->commitCalls, 0);
        QCOMPARE(harness.service->displayConfirmationState(),
                 QStringLiteral("idle"));
        QCOMPARE(harness.service->displayConfirmationRevision(), qulonglong(0));
        QCOMPARE(harness.service->displayConfirmationDeadlineMs(), qulonglong(0));
        QVERIFY(harness.service->displayConfirmationGeneration().isEmpty());
        for (const auto &arguments : published) {
            const auto changed = arguments.at(0).toMap();
            QVERIFY(changed.value(
                QStringLiteral("DisplayConfirmationState")
            ).toString() != QStringLiteral("awaiting-confirmation"));
        }
    }

    void previewOwnerLookupUsesRemainingBudget_data()
    {
        QTest::addColumn<qint64>("remainingBeforeOwner");
        QTest::addColumn<qint64>("remainingAfterOwner");
        QTest::addColumn<int>("expectedMaximumWait");
        QTest::newRow("ceiling") << qint64(10'000) << qint64(9'999) << 250;
        QTest::newRow("smaller-remaining") << qint64(137) << qint64(136)
                                             << 137;
    }

    void previewOwnerLookupUsesRemainingBudget()
    {
        QFETCH(qint64, remainingBeforeOwner);
        QFETCH(qint64, remainingAfterOwner);
        QFETCH(int, expectedMaximumWait);
        auto initial = committedSnapshot();
        initial.generationDigest = QStringLiteral("old-generation");
        int deadlineChecks = 0;
        QVector<int> ownerMaximumWaits;
        ServiceHarness harness(
            initial, managedStatus(), {},
            [&deadlineChecks, remainingBeforeOwner, remainingAfterOwner](
                const QDeadlineTimer &
            ) {
                return ++deadlineChecks == 1
                    ? remainingBeforeOwner : remainingAfterOwner;
            },
            [&ownerMaximumWaits](
                const QString &owner,
                const int maximumWaitMilliseconds
            ) {
                if (owner != QStringLiteral("in-process")) return false;
                ownerMaximumWaits.append(maximumWaitMilliseconds);
                return true;
            }
        );
        const auto topology = connectedDisplayTopology();
        configureDisplayPreview(harness, initial, topology);

        QString token;
        qulonglong deadline = 0;
        QString generation;
        QCOMPARE(harness.service->PreviewDisplayConfiguration(
                     initial.revision,
                     initial.catalogDigest,
                     initial.actionCatalogDigest,
                     displayProfileBytes(topology),
                     10,
                     token,
                     deadline,
                     generation),
                 qulonglong(6));
        QCOMPARE(deadlineChecks, 2);
        QCOMPARE(ownerMaximumWaits, QVector<int>({expectedMaximumWait}));
        QVERIFY(!token.isEmpty());
        QVERIFY(deadline > 0);
        QCOMPARE(generation, QString::fromLatin1(generationId));

        QCOMPARE(harness.service->RevertDisplayConfiguration(token),
                 qulonglong(initial.revision));
        QCOMPARE(harness.backend->rollbackCalls, 1);
        QCOMPARE(harness.authority->abortCalls, 1);
    }

    void ownerLossDuringPreviewLivenessProofEndsInstalledCapability()
    {
        auto initial = committedSnapshot();
        initial.generationDigest = QStringLiteral("old-generation");
        CompositorService *service = nullptr;
        int deadlineChecks = 0;
        ServiceHarness harness(
            initial, managedStatus(), {},
            [&deadlineChecks](const QDeadlineTimer &) {
                ++deadlineChecks;
                return qint64(10'000);
            },
            [&service](const QString &owner, const int maximumWaitMilliseconds) {
                if (maximumWaitMilliseconds != 250) return false;
                if (!service) return false;
                const auto invoked = QMetaObject::invokeMethod(
                    service, "handleDisplayOwnerLoss", Qt::DirectConnection,
                    Q_ARG(QString, owner)
                );
                return invoked;
            }
        );
        service = harness.service.get();
        const auto topology = connectedDisplayTopology();
        configureDisplayPreview(harness, initial, topology);

        QString token;
        qulonglong deadline = 0;
        QString generation;
        QCOMPARE(harness.service->PreviewDisplayConfiguration(
                     initial.revision,
                     initial.catalogDigest,
                     initial.actionCatalogDigest,
                     displayProfileBytes(topology),
                     10,
                     token,
                     deadline,
                     generation),
                 qulonglong(initial.revision));
        QVERIFY(token.isEmpty());
        QCOMPARE(deadline, qulonglong(0));
        QVERIFY(generation.isEmpty());
        QCOMPARE(deadlineChecks, 1);
        QCOMPARE(harness.backend->rollbackCalls, 1);
        QCOMPARE(harness.authority->abortCalls, 1);
        QCOMPARE(harness.authority->commitCalls, 0);
        QCOMPARE(harness.service->displayConfirmationState(),
                 QStringLiteral("idle"));
        QCOMPARE(harness.service->displayConfirmationRevision(), qulonglong(0));
        QCOMPARE(harness.service->displayConfirmationDeadlineMs(), qulonglong(0));
        QVERIFY(harness.service->displayConfirmationGeneration().isEmpty());
    }

    void serverTimeoutRevertsExactlyOnce()
    {
        auto initial = committedSnapshot();
        initial.generationDigest = QStringLiteral("old-generation");
        ServiceHarness harness(initial);
        const auto topology = connectedDisplayTopology();
        configureDisplayPreview(harness, initial, topology);
        QString token;
        qulonglong deadline = 0;
        QString generation;
        QCOMPARE(harness.service->PreviewDisplayConfiguration(
                     initial.revision,
                     initial.catalogDigest,
                     initial.actionCatalogDigest,
                     displayProfileBytes(topology),
                     10,
                     token,
                     deadline,
                     generation),
                 qulonglong(6));

        QVERIFY(QMetaObject::invokeMethod(
            harness.service.get(), "handleDisplayConfirmationTimeout",
            Qt::DirectConnection
        ));
        QCOMPARE(harness.backend->rollbackCalls, 1);
        QCOMPARE(harness.authority->abortCalls, 1);
        QCOMPARE(harness.service->displayConfirmationState(),
                 QStringLiteral("idle"));
        QCOMPARE(harness.service->displayConfirmationRevision(), qulonglong(0));
        QCOMPARE(harness.service->displayConfirmationDeadlineMs(), qulonglong(0));
        QVERIFY(harness.service->displayConfirmationGeneration().isEmpty());
        QCOMPARE(harness.service->revision(), qulonglong(initial.revision));

        QVERIFY(QMetaObject::invokeMethod(
            harness.service.get(), "handleDisplayConfirmationTimeout",
            Qt::DirectConnection
        ));
        QCOMPARE(harness.backend->rollbackCalls, 1);
        QCOMPARE(harness.authority->abortCalls, 1);
    }

    void ownerLossMatchesOnlyTheActiveCapability()
    {
        auto initial = committedSnapshot();
        initial.generationDigest = QStringLiteral("old-generation");
        ServiceHarness harness(initial);
        const auto topology = connectedDisplayTopology();
        configureDisplayPreview(harness, initial, topology);
        QString token;
        qulonglong deadline = 0;
        QString generation;
        QCOMPARE(harness.service->PreviewDisplayConfiguration(
                     initial.revision,
                     initial.catalogDigest,
                     initial.actionCatalogDigest,
                     displayProfileBytes(topology),
                     10,
                     token,
                     deadline,
                     generation),
                 qulonglong(6));

        QVERIFY(QMetaObject::invokeMethod(
            harness.service.get(), "handleDisplayOwnerLoss",
            Qt::DirectConnection,
            Q_ARG(QString, QStringLiteral(":1.999"))
        ));
        QCOMPARE(harness.backend->rollbackCalls, 0);
        QCOMPARE(harness.authority->abortCalls, 0);
        QCOMPARE(harness.service->displayConfirmationState(),
                 QStringLiteral("awaiting-confirmation"));

        QVERIFY(QMetaObject::invokeMethod(
            harness.service.get(), "handleDisplayOwnerLoss",
            Qt::DirectConnection,
            Q_ARG(QString, QStringLiteral("in-process"))
        ));
        QCOMPARE(harness.backend->rollbackCalls, 1);
        QCOMPARE(harness.authority->abortCalls, 1);
        QCOMPARE(harness.service->displayConfirmationState(),
                 QStringLiteral("idle"));

        QVERIFY(QMetaObject::invokeMethod(
            harness.service.get(), "handleDisplayOwnerLoss",
            Qt::DirectConnection,
            Q_ARG(QString, QStringLiteral("in-process"))
        ));
        QCOMPARE(harness.backend->rollbackCalls, 1);
        QCOMPARE(harness.authority->abortCalls, 1);
    }

    void callerDeathDuringConfirmProofWinsBeforeCommit()
    {
        auto initial = committedSnapshot();
        initial.generationDigest = QStringLiteral("old-generation");
        int ownerChecks = 0;
        QString lastOwner;
        QVector<int> ownerMaximumWaits;
        ServiceHarness harness(
            initial, managedStatus(), {}, {},
            [&ownerChecks, &lastOwner, &ownerMaximumWaits](
                const QString &owner,
                const int maximumWaitMilliseconds
            ) {
                lastOwner = owner;
                ownerMaximumWaits.append(maximumWaitMilliseconds);
                return ++ownerChecks == 1;
            }
        );
        const auto topology = connectedDisplayTopology();
        configureDisplayPreview(harness, initial, topology);
        QString token;
        qulonglong deadline = 0;
        QString generation;
        QCOMPARE(harness.service->PreviewDisplayConfiguration(
                     initial.revision,
                     initial.catalogDigest,
                     initial.actionCatalogDigest,
                     displayProfileBytes(topology),
                     10,
                     token,
                     deadline,
                     generation),
                 qulonglong(6));
        QCOMPARE(ownerChecks, 1);
        QCOMPARE(lastOwner, QStringLiteral("in-process"));

        QString confirmedGeneration;
        QCOMPARE(harness.service->ConfirmDisplayConfiguration(
                     token, confirmedGeneration),
                 qulonglong(initial.revision));
        QCOMPARE(ownerChecks, 2);
        QCOMPARE(ownerMaximumWaits, QVector<int>({250, 250}));
        QCOMPARE(harness.backend->connectedMaximumWaits.size(), 1);
        QCOMPARE(harness.authority->commitCalls, 0);
        QCOMPARE(harness.backend->rollbackCalls, 1);
        QCOMPARE(harness.authority->abortCalls, 1);
        QCOMPARE(harness.service->displayConfirmationState(),
                 QStringLiteral("idle"));
        QCOMPARE(harness.service->revision(), qulonglong(initial.revision));
    }

    void ownerLossReentrancyDuringConfirmProofEndsCapability()
    {
        auto initial = committedSnapshot();
        initial.generationDigest = QStringLiteral("old-generation");
        CompositorService *service = nullptr;
        int ownerChecks = 0;
        ServiceHarness harness(
            initial, managedStatus(), {}, {},
            [&service, &ownerChecks](
                const QString &owner,
                const int maximumWaitMilliseconds
            ) {
                if (maximumWaitMilliseconds != 250) return false;
                ++ownerChecks;
                if (ownerChecks == 1) return true;
                if (!service) return false;
                return QMetaObject::invokeMethod(
                    service, "handleDisplayOwnerLoss", Qt::DirectConnection,
                    Q_ARG(QString, owner)
                );
            }
        );
        service = harness.service.get();
        const auto topology = connectedDisplayTopology();
        configureDisplayPreview(harness, initial, topology);
        QString token;
        qulonglong deadline = 0;
        QString generation;
        QCOMPARE(harness.service->PreviewDisplayConfiguration(
                     initial.revision,
                     initial.catalogDigest,
                     initial.actionCatalogDigest,
                     displayProfileBytes(topology),
                     10,
                     token,
                     deadline,
                     generation),
                 qulonglong(6));
        QCOMPARE(ownerChecks, 1);

        QString confirmedGeneration;
        QCOMPARE(harness.service->ConfirmDisplayConfiguration(
                     token, confirmedGeneration),
                 qulonglong(initial.revision));
        QCOMPARE(ownerChecks, 2);
        QVERIFY(confirmedGeneration.isEmpty());
        QCOMPARE(harness.authority->commitCalls, 0);
        QCOMPARE(harness.backend->rollbackCalls, 1);
        QCOMPARE(harness.authority->abortCalls, 1);
        QCOMPARE(harness.service->displayConfirmationState(),
                 QStringLiteral("idle"));
        QCOMPARE(harness.service->displayConfirmationRevision(), qulonglong(0));
        QCOMPARE(harness.service->displayConfirmationDeadlineMs(), qulonglong(0));
        QVERIFY(harness.service->displayConfirmationGeneration().isEmpty());
        QCOMPARE(harness.service->revision(), qulonglong(initial.revision));
    }

    void activeDisplayPreviewExcludesMutationsAndWrongTokens()
    {
        auto initial = committedSnapshot();
        initial.generationDigest = QStringLiteral("old-generation");
        ServiceHarness harness(initial);
        const auto topology = connectedDisplayTopology();
        configureDisplayPreview(harness, initial, topology);
        QString token;
        qulonglong deadline = 0;
        QString generation;
        QCOMPARE(harness.service->PreviewDisplayConfiguration(
                     initial.revision,
                     initial.catalogDigest,
                     initial.actionCatalogDigest,
                     displayProfileBytes(topology),
                     10,
                     token,
                     deadline,
                     generation),
                 qulonglong(6));

        QCOMPARE(harness.service->ReplaceSnapshot(
                     initial.revision,
                     initial.catalogDigest,
                     initial.actionCatalogDigest,
                     initial.desiredState),
                 qulonglong(initial.revision));
        QString ordinaryGeneration;
        QCOMPARE(harness.service->Apply(
                     initial.revision,
                     initial.catalogDigest,
                     initial.actionCatalogDigest,
                     ordinaryGeneration),
                 qulonglong(initial.appliedRevision));
        qulonglong recoveredAppliedRevision = 0;
        QCOMPARE(harness.service->Recover(
                     initial.revision,
                     initial.catalogDigest,
                     initial.actionCatalogDigest,
                     recoveredAppliedRevision,
                     ordinaryGeneration),
                 qulonglong(initial.revision));
        QCOMPARE(harness.authority->replaceCalls, 0);
        QCOMPARE(harness.authority->prepareApplyCalls, 0);
        QCOMPARE(harness.authority->prepareRecoveryCalls, 0);
        QCOMPARE(harness.authority->prepareDisplayCalls, 1);

        const auto wrongToken = QString(32, QLatin1Char('f'));
        QString confirmedGeneration;
        QCOMPARE(harness.service->ConfirmDisplayConfiguration(
                     wrongToken, confirmedGeneration),
                 qulonglong(initial.revision));
        QCOMPARE(harness.service->RevertDisplayConfiguration(wrongToken),
                 qulonglong(initial.revision));
        QCOMPARE(harness.backend->rollbackCalls, 0);
        QCOMPARE(harness.authority->abortCalls, 0);

        qulonglong pendingRevision = 0;
        qulonglong pendingDeadline = 0;
        QString pendingGeneration;
        QCOMPARE(harness.service->GetPendingDisplayConfirmation(
                     pendingRevision, pendingDeadline, pendingGeneration),
                 token);
        QCOMPARE(pendingRevision, qulonglong(6));
        QCOMPARE(pendingDeadline, deadline);
        QCOMPARE(pendingGeneration, generation);
        QCOMPARE(harness.service->displayConfirmationState(),
                 QStringLiteral("awaiting-confirmation"));
    }

    void uncertainDisplayCommitRevokesEveryRollbackPath()
    {
        auto initial = committedSnapshot();
        initial.generationDigest = QStringLiteral("old-generation");
        ServiceHarness harness(initial);
        const auto topology = connectedDisplayTopology();
        configureDisplayPreview(harness, initial, topology);
        auto unavailable = initial;
        unavailable.available = false;
        unavailable.writable = false;
        unavailable.loadState = QStringLiteral("unavailable");
        unavailable.applyState = QStringLiteral("failed");
        harness.authority->commitResult = {
            .success = false,
            .commitDecisionDurable = false,
            .commitDecisionMayExist = true,
            .errorCode = QStringLiteral("PersistenceFailed"),
            .errorMessage = QStringLiteral("commit marker durability uncertain"),
            .snapshot = unavailable,
        };

        QString token;
        qulonglong deadline = 0;
        QString previewGeneration;
        QCOMPARE(
            harness.service->PreviewDisplayConfiguration(
                initial.revision,
                initial.catalogDigest,
                initial.actionCatalogDigest,
                displayProfileBytes(topology),
                10,
                token,
                deadline,
                previewGeneration
            ),
            qulonglong(6)
        );
        QVERIFY(QRegularExpression(QStringLiteral("^[0-9a-f]{32}$"))
                    .match(token).hasMatch());
        QVERIFY(deadline > 0);
        QCOMPARE(previewGeneration, QString::fromLatin1(generationId));
        QCOMPARE(harness.service->displayConfirmationState(),
                 QStringLiteral("awaiting-confirmation"));

        QString confirmedGeneration;
        QCOMPARE(harness.service->ConfirmDisplayConfiguration(
                     token, confirmedGeneration),
                 qulonglong(0));
        QVERIFY(confirmedGeneration.isEmpty());
        QCOMPARE(harness.authority->commitCalls, 1);
        QCOMPARE(harness.backend->finalizeCalls, 0);
        QCOMPARE(harness.backend->rollbackCalls, 0);
        QCOMPARE(harness.authority->abortCalls, 0);
        QCOMPARE(harness.service->displayConfirmationState(),
                 QStringLiteral("failed"));
        QCOMPARE(harness.service->displayConfirmationRevision(), qulonglong(0));
        QCOMPARE(harness.service->displayConfirmationDeadlineMs(), qulonglong(0));
        QVERIFY(harness.service->displayConfirmationGeneration().isEmpty());
        QVERIFY(!harness.service->available());
        QCOMPARE(harness.service->managementState(), QStringLiteral("conflict"));

        QCOMPARE(harness.service->RevertDisplayConfiguration(token),
                 qulonglong(0));
        QCOMPARE(harness.service->ConfirmDisplayConfiguration(
                     token, confirmedGeneration),
                 qulonglong(0));
        QCOMPARE(harness.authority->commitCalls, 1);
        QCOMPARE(harness.backend->rollbackCalls, 0);
        QCOMPARE(harness.authority->abortCalls, 0);
    }

    void definitelyFailedDisplayCommitStillUsesRevertProtocol_data()
    {
        QTest::addColumn<bool>("rollbackSucceeds");
        QTest::newRow("rollback-and-abort") << true;
        QTest::newRow("rollback-uncertain") << false;
    }

    void definitelyFailedDisplayCommitStillUsesRevertProtocol()
    {
        QFETCH(bool, rollbackSucceeds);
        auto initial = committedSnapshot();
        initial.generationDigest = QStringLiteral("old-generation");
        ServiceHarness harness(initial);
        const auto topology = connectedDisplayTopology();
        configureDisplayPreview(harness, initial, topology);
        harness.authority->commitResult = {
            .success = false,
            .commitDecisionDurable = false,
            .commitDecisionMayExist = false,
            .errorCode = QStringLiteral("PersistenceFailed"),
            .errorMessage = QStringLiteral("commit marker was not published"),
            .snapshot = initial,
        };
        if (!rollbackSucceeds) {
            harness.backend->rollbackResult = {
                .success = false,
                .errorCode = QStringLiteral("ApplyFailed"),
                .errorMessage = QStringLiteral("live rollback failed"),
                .status = {
                    .state = ManagementState::Conflict,
                    .entrypointKind = EntrypointKind::Unsafe,
                },
            };
        }
        QString token;
        qulonglong deadline = 0;
        QString generation;
        QCOMPARE(harness.service->PreviewDisplayConfiguration(
                     initial.revision,
                     initial.catalogDigest,
                     initial.actionCatalogDigest,
                     displayProfileBytes(topology),
                     10,
                     token,
                     deadline,
                     generation),
                 qulonglong(6));

        QString confirmedGeneration;
        QCOMPARE(harness.service->ConfirmDisplayConfiguration(
                     token, confirmedGeneration),
                 rollbackSucceeds ? qulonglong(initial.revision)
                                  : qulonglong(0));
        QCOMPARE(harness.authority->commitCalls, 1);
        QCOMPARE(harness.backend->rollbackCalls, 1);
        QCOMPARE(harness.authority->abortCalls, rollbackSucceeds ? 1 : 0);
        if (rollbackSucceeds) {
            QCOMPARE(harness.service->displayConfirmationState(),
                     QStringLiteral("idle"));
            QCOMPARE(harness.service->revision(), qulonglong(initial.revision));
            QCOMPARE(harness.service->managementState(),
                     QStringLiteral("managed"));
        } else {
            QCOMPARE(harness.service->displayConfirmationState(),
                     QStringLiteral("failed"));
            QVERIFY(!harness.service->available());
            QCOMPARE(harness.service->managementState(),
                     QStringLiteral("conflict"));
            QCOMPARE(harness.service->displayConfirmationRevision(),
                     qulonglong(0));
        }
    }

    void displayFinalizeFailureCannotRollBackCommittedAuthority()
    {
        auto initial = committedSnapshot();
        initial.generationDigest = QStringLiteral("old-generation");
        ServiceHarness harness(initial);
        const auto topology = connectedDisplayTopology();
        configureDisplayPreview(harness, initial, topology);
        const auto committed = committedSnapshot(
            6, QByteArrayLiteral("{\"revision\":\"6\"}\n")
        );
        harness.authority->commitResult = {
            .success = true,
            .commitDecisionDurable = true,
            .commitDecisionMayExist = true,
            .snapshot = committed,
        };
        auto target = managedStatus();
        target.managedGeneration = QString::fromLatin1(generationId);
        harness.backend->finalizeResultConfigured = true;
        harness.backend->finalizeResult = {
            .success = false,
            .errorCode = QStringLiteral("PersistenceFailed"),
            .errorMessage = QStringLiteral("bridge cleanup failed"),
            .status = target,
        };

        QString token;
        qulonglong deadline = 0;
        QString previewGeneration;
        QCOMPARE(harness.service->PreviewDisplayConfiguration(
                     initial.revision,
                     initial.catalogDigest,
                     initial.actionCatalogDigest,
                     displayProfileBytes(topology),
                     10,
                     token,
                     deadline,
                     previewGeneration),
                 qulonglong(6));
        QString confirmedGeneration;
        QCOMPARE(harness.service->ConfirmDisplayConfiguration(
                     token, confirmedGeneration),
                 qulonglong(0));
        QCOMPARE(harness.authority->commitCalls, 1);
        QCOMPARE(harness.backend->finalizeCalls, 1);
        QCOMPARE(harness.backend->rollbackCalls, 0);
        QCOMPARE(harness.authority->abortCalls, 0);
        QCOMPARE(harness.service->displayConfirmationState(),
                 QStringLiteral("failed"));
        QCOMPARE(harness.service->displayConfirmationRevision(), qulonglong(0));
        QVERIFY(!harness.service->available());
        QCOMPARE(harness.service->managementState(), QStringLiteral("conflict"));

        QCOMPARE(harness.service->RevertDisplayConfiguration(token),
                 qulonglong(0));
        QCOMPARE(harness.service->ConfirmDisplayConfiguration(
                     token, confirmedGeneration),
                 qulonglong(0));
        QCOMPARE(harness.authority->commitCalls, 1);
        QCOMPARE(harness.backend->finalizeCalls, 1);
        QCOMPARE(harness.backend->rollbackCalls, 0);
        QCOMPARE(harness.authority->abortCalls, 0);
    }

    void confirmedDisplayCapabilityIsIdempotentAndNonRevertible()
    {
        auto initial = committedSnapshot();
        initial.generationDigest = QStringLiteral("old-generation");
        ServiceHarness harness(initial);
        const auto topology = connectedDisplayTopology();
        configureDisplayPreview(harness, initial, topology);
        const auto committed = committedSnapshot(
            6, QByteArrayLiteral("{\"revision\":\"6\"}\n")
        );
        harness.authority->commitResult = {
            .success = true,
            .commitDecisionDurable = true,
            .commitDecisionMayExist = true,
            .snapshot = committed,
        };
        auto target = managedStatus();
        target.managedGeneration = QString::fromLatin1(generationId);
        harness.backend->finalizeResultConfigured = true;
        harness.backend->finalizeResult = {
            .success = true,
            .status = target,
        };

        QString token;
        qulonglong deadline = 0;
        QString previewGeneration;
        QCOMPARE(harness.service->PreviewDisplayConfiguration(
                     initial.revision,
                     initial.catalogDigest,
                     initial.actionCatalogDigest,
                     displayProfileBytes(topology),
                     10,
                     token,
                     deadline,
                     previewGeneration),
                 qulonglong(6));
        QString confirmedGeneration;
        QCOMPARE(harness.service->ConfirmDisplayConfiguration(
                     token, confirmedGeneration),
                 qulonglong(6));
        QCOMPARE(confirmedGeneration, QString::fromLatin1(generationId));
        QCOMPARE(harness.service->displayConfirmationState(),
                 QStringLiteral("idle"));
        QCOMPARE(harness.service->managementState(), QStringLiteral("managed"));

        // A timeout event already queued at the one-way commit boundary must
        // become a no-op after the capability is revoked.
        QVERIFY(QMetaObject::invokeMethod(
            harness.service.get(), "handleDisplayConfirmationTimeout",
            Qt::DirectConnection
        ));
        QVERIFY(QMetaObject::invokeMethod(
            harness.service.get(), "handleDisplayOwnerLoss",
            Qt::DirectConnection,
            Q_ARG(QString, QStringLiteral("in-process"))
        ));

        confirmedGeneration.clear();
        QCOMPARE(harness.service->ConfirmDisplayConfiguration(
                     token, confirmedGeneration),
                 qulonglong(6));
        QCOMPARE(confirmedGeneration, QString::fromLatin1(generationId));
        QCOMPARE(harness.service->RevertDisplayConfiguration(token),
                 qulonglong(6));
        QCOMPARE(harness.authority->commitCalls, 1);
        QCOMPARE(harness.backend->finalizeCalls, 1);
        QCOMPARE(harness.backend->rollbackCalls, 0);
        QCOMPARE(harness.authority->abortCalls, 0);
    }

    void displayRevertRollsBackBeforeAuthorityAbortAndIsIdempotent()
    {
        auto initial = committedSnapshot();
        initial.generationDigest = QStringLiteral("old-generation");
        ServiceHarness harness(initial);
        const auto topology = connectedDisplayTopology();
        configureDisplayPreview(harness, initial, topology);
        bool abortObservedRollback = false;
        harness.authority->abortHook = [&] {
            abortObservedRollback = harness.backend->rollbackCalls == 1;
        };

        QString token;
        qulonglong deadline = 0;
        QString previewGeneration;
        QCOMPARE(harness.service->PreviewDisplayConfiguration(
                     initial.revision,
                     initial.catalogDigest,
                     initial.actionCatalogDigest,
                     displayProfileBytes(topology),
                     10,
                     token,
                     deadline,
                     previewGeneration),
                 qulonglong(6));
        QCOMPARE(harness.service->RevertDisplayConfiguration(token),
                 qulonglong(initial.revision));
        QVERIFY(abortObservedRollback);
        QCOMPARE(harness.backend->rollbackCalls, 1);
        QCOMPARE(harness.authority->abortCalls, 1);
        QCOMPARE(harness.service->displayConfirmationState(),
                 QStringLiteral("idle"));
        QCOMPARE(harness.service->revision(), qulonglong(initial.revision));

        QCOMPARE(harness.service->RevertDisplayConfiguration(token),
                 qulonglong(initial.revision));
        QCOMPARE(harness.backend->rollbackCalls, 1);
        QCOMPARE(harness.authority->abortCalls, 1);
    }
};

QTEST_MAIN(CompositorServiceTest)

#include "compositor_service_test.moc"
