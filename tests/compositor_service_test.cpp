#include "compositord/activation_backend.h"
#include "compositord/compositor_service.h"
#include "compositord/shared_border_reconciler.h"
#include "compositord/shared_spacing_reconciler.h"

#include "hyprland/catalog.h"
#include "hyprland/action_catalog.h"
#include "hyprland/json_support.h"

#include <QCryptographicHash>
#include <QDBusConnection>
#include <QDir>
#include <QFile>
#include <QJsonArray>
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
constexpr auto inputDeviceInventoryEpoch =
    "fedcba9876543210fedcba9876543210";

[[nodiscard]] QString sha256(const QByteArrayView bytes)
{
    return QString::fromLatin1(
        QCryptographicHash::hash(bytes, QCryptographicHash::Sha256).toHex()
    );
}

[[nodiscard]] QByteArray protectedCatalog()
{
    QFile file(QFINDTESTDATA("../data/hyprland/config-catalog-v1.json"));
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }
    const auto parsed = parseCatalog(file.readAll());
    return parsed ? canonicalCatalogJson(*parsed.value) : QByteArray{};
}

[[nodiscard]] QByteArray protectedConfigSchema()
{
    QFile file(QFINDTESTDATA("../interfaces/hyprland/v1/config.schema.json"));
    return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray{};
}

[[nodiscard]] QByteArray protectedActionCatalog()
{
    QFile file(QFINDTESTDATA("../data/hyprland/action-catalog-v1.json"));
    if (!file.open(QIODevice::ReadOnly)) return {};
    const auto parsed = parseActionCatalog(file.readAll(), protectedConfigSchema());
    return parsed ? canonicalActionCatalogJson(*parsed.value) : QByteArray{};
}

[[nodiscard]] QJsonObject exactProtectedSpacingRule();

[[nodiscard]] QByteArray sharedBorderSnapshot(
    const quint64 revision,
    const QString &catalogDigest,
    QJsonObject overrides = {},
    const QString &marker = {}
)
{
    QJsonObject object{
        {QStringLiteral("formatVersion"), 1},
        {QStringLiteral("revision"), QString::number(revision)},
        {QStringLiteral("targetHyprland"), QStringLiteral("0.56.1")},
        {QStringLiteral("catalogDigest"), catalogDigest},
        {
            QStringLiteral("actionCatalogDigest"),
            QString::fromLatin1(reviewedActionCatalogDigest)
        },
        {QStringLiteral("overrides"), overrides},
        {QStringLiteral("monitors"), QJsonArray{}},
        {QStringLiteral("devices"), QJsonArray{}},
        {QStringLiteral("curves"), QJsonArray{}},
        {QStringLiteral("animations"), QJsonArray{}},
        {QStringLiteral("gestures"), QJsonArray{}},
        {
            QStringLiteral("workspaceRules"),
            QJsonArray{exactProtectedSpacingRule()}
        },
        {QStringLiteral("windowRules"), QJsonArray{}},
        {QStringLiteral("layerRules"), QJsonArray{}},
        {QStringLiteral("submaps"), QJsonArray{}},
        {QStringLiteral("bindings"), QJsonArray{}},
        {QStringLiteral("permissions"), QJsonArray{}},
        {QStringLiteral("environment"), QJsonArray{}},
    };
    if (!marker.isEmpty()) {
        object.insert(QStringLiteral("testMarker"), marker);
    }
    auto bytes = JsonSupport::canonicalJson(object);
    bytes.append('\n');
    return bytes;
}

[[nodiscard]] QJsonObject exactProtectedSpacingRule()
{
    return {
        {
            QStringLiteral("id"),
            QStringLiteral("hyprshelld.internal.shared-spacing.maximized")
        },
        {QStringLiteral("selector"), QStringLiteral("f[1]")},
        {QStringLiteral("enabled"), true},
        {QStringLiteral("monitor"), QString()},
        {QStringLiteral("persistent"), false},
        {QStringLiteral("isDefault"), false},
        {QStringLiteral("layout"), QString()},
        {
            QStringLiteral("overrides"),
            QJsonObject{{
                QStringLiteral("gaps_out"), QJsonArray{0, 0, 0, 0}
            }}
        },
    };
}

[[nodiscard]] QByteArray snapshotAtRevision(
    const QByteArray &candidate,
    const quint64 revision
)
{
    auto object = QJsonDocument::fromJson(candidate).object();
    object.insert(QStringLiteral("revision"), QString::number(revision));
    auto bytes = JsonSupport::canonicalJson(object);
    bytes.append('\n');
    return bytes;
}

[[nodiscard]] std::optional<QByteArray> reconciledSharedVisualSnapshot(
    const QByteArray &snapshot,
    const quint64 expectedRevision,
    const QString &catalogDigest,
    const SharedVisualProjection &projection,
    const bool reconcileBorder,
    const bool reconcileSpacing,
    QString &error
)
{
    auto candidate = snapshot;
    if (reconcileBorder) {
        SharedBorderReconciler border;
        if (!border.configure(protectedCatalog(), catalogDigest, error)) {
            return std::nullopt;
        }
        const auto edit = border.edit(
            candidate,
            expectedRevision,
            catalogDigest,
            projection,
            error
        );
        if (!edit) return std::nullopt;
        candidate = edit->candidate;
    }
    if (reconcileSpacing) {
        SharedSpacingReconciler spacing;
        if (!spacing.configure(protectedCatalog(), catalogDigest, error)) {
            return std::nullopt;
        }
        const auto edit = spacing.edit(
            candidate,
            expectedRevision,
            catalogDigest,
            projection,
            error
        );
        if (!edit) return std::nullopt;
        candidate = edit->candidate;
    }
    return candidate;
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
    const auto catalogDigest = sha256(protectedCatalog());
    const auto desiredState = sharedBorderSnapshot(5, catalogDigest);
    return {
        .available = true,
        .writable = true,
        .desiredState = desiredState,
        .appliedDesiredState = snapshotAtRevision(desiredState, 4),
        .revision = 5,
        .catalogDigest = catalogDigest,
        .actionCatalogDigest = QString::fromLatin1(reviewedActionCatalogDigest),
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
    snapshot.appliedDesiredState = desired;
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

[[nodiscard]] ConnectedInputDeviceInventory connectedInputDeviceInventory()
{
    const auto digest = QString(64, QLatin1Char('e'));
    QByteArray document = JsonSupport::canonicalJson(QJsonObject{
        {QStringLiteral("formatVersion"), 1},
        {QStringLiteral("inventoryDigest"), digest},
        {QStringLiteral("records"), QJsonArray{QJsonObject{
             {QStringLiteral("sessionSelector"), QStringLiteral("keyboard-1")},
             {QStringLiteral("observedKind"), QStringLiteral("keyboard")},
             {QStringLiteral("activeKeymap"), QStringLiteral("English (US)")},
         }}},
        {QStringLiteral("unaddressable"), QJsonObject{
             {QStringLiteral("switches"), 1},
             {QStringLiteral("tabletPads"), 2},
             {QStringLiteral("tabletTools"), 3},
         }},
    });
    document.append('\n');
    return {
        .records = QVector<ConnectedInputDevice>{ConnectedInputDevice{
            .sessionSelector = QStringLiteral("keyboard-1"),
            .observedKind = ConnectedInputDeviceKind::Keyboard,
            .activeKeymap = QStringLiteral("English (US)"),
        }},
        .unaddressable = {
            .switches = 1,
            .tabletPads = 2,
            .tabletTools = 3,
        },
        .inventoryDigest = digest,
        .document = document,
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
    std::function<AuthorityResult(quint64, const QByteArray &)> replaceHook;
    quint64 lastReplaceExpected = 0;
    QByteArray lastReplaceCandidate;
    QByteArray optionCatalogBytes;
    mutable int optionCatalogCalls = 0;
    QByteArray actionCatalogBytes;
    QByteArray configSchemaBytes;
    mutable int actionCatalogCalls = 0;
    mutable int configSchemaCalls = 0;
    ValidationErrors activationSafetyErrors;
    mutable int activationSafetyCalls = 0;
    mutable QStringList calls;

    AuthorityResult initialize() override
    {
        ++initializeCalls;
        calls.append(QStringLiteral("initialize"));
        if (startupTrace) {
            startupTrace->append(QStringLiteral("authority-initialize"));
        }
        if (initializeResult.success) {
            if (initializeResult.snapshot.applyState == QStringLiteral("current")) {
                initializeResult.snapshot.appliedDesiredState =
                    initializeResult.snapshot.desiredState;
            }
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

    QByteArray actionCatalog() const override
    {
        ++actionCatalogCalls;
        return actionCatalogBytes;
    }

    QByteArray configSchema() const override
    {
        ++configSchemaCalls;
        return configSchemaBytes;
    }

    ValidationErrors currentActivationSafetyErrors() const override
    {
        ++activationSafetyCalls;
        calls.append(QStringLiteral("activation-safety"));
        return activationSafetyErrors;
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
        if (replaceHook) {
            auto result = replaceHook(expectedRevision, candidate);
            if (result.success) {
                if (result.snapshot.applyState == QStringLiteral("current")) {
                    result.snapshot.appliedDesiredState =
                        result.snapshot.desiredState;
                }
                current = result.snapshot;
            }
            return result;
        }
        if (replaceResult.success) {
            if (replaceResult.snapshot.applyState == QStringLiteral("current")) {
                replaceResult.snapshot.appliedDesiredState =
                    replaceResult.snapshot.desiredState;
            }
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
            if (commitResult.snapshot.applyState == QStringLiteral("current")) {
                commitResult.snapshot.appliedDesiredState =
                    commitResult.snapshot.desiredState;
            }
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
    ConnectedInputDevicesResult connectedInputResult;
    BackendResult verifyPendingResult;
    bool reconcileResultConfigured = false;
    bool bindResultConfigured = false;
    bool finalizeResultConfigured = false;
    std::function<void()> finalizeHook;
    std::function<void()> rollbackHook;
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
    int connectedInputCalls = 0;
    QVector<QByteArray> connectedInputEpochs;
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
        if (rollbackHook) rollbackHook();
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
        if (finalizeHook) finalizeHook();
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

    ConnectedInputDevicesResult connectedInputDevices(
        const QByteArrayView serviceEpoch
    ) override
    {
        ++connectedInputCalls;
        connectedInputEpochs.append(serviceEpoch.toByteArray());
        return connectedInputResult;
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

class FakeSharedBorderSource final : public SharedVisualSource
{
public:
    int startCalls = 0;
    int refreshCalls = 0;

    void start() override
    {
        ++startCalls;
    }

    void requestRefresh() override
    {
        ++refreshCalls;
    }

    void setProjection(const SharedVisualProjection &projection)
    {
        publishProjection(projection);
    }

    void loseSource(const QString &error = QStringLiteral("source lost"))
    {
        publishUnavailable(error);
    }
};

struct ServiceHarness final {
    QStringList startupTrace;
    FakeAuthority *authority = nullptr;
    FakeActivationBackend *backend = nullptr;
    FakeSharedBorderSource *sharedBorderSource = nullptr;
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
        auto ownedSharedBorderSource =
            std::make_unique<FakeSharedBorderSource>();
        sharedBorderSource = ownedSharedBorderSource.get();
        service = std::make_unique<CompositorService>(
            std::move(ownedBackend),
            QDBusConnection(QStringLiteral("compositor-service-test")),
            nullptr,
            std::move(deadlineRemaining),
            std::move(ownerPresent),
            std::move(ownedSharedBorderSource),
            QByteArray(inputDeviceInventoryEpoch)
        );

        auto ownedAuthority = std::make_unique<FakeAuthority>();
        authority = ownedAuthority.get();
        authority->startupTrace = &startupTrace;
        authority->optionCatalogBytes = protectedCatalog();
        authority->actionCatalogBytes = protectedActionCatalog();
        authority->configSchemaBytes = protectedConfigSchema();
        auto normalizedInitial = initial;
        if (normalizedInitial.appliedDesiredState.isEmpty()
            && normalizedInitial.appliedRevision > 0
            && QJsonDocument::fromJson(normalizedInitial.desiredState).isObject()) {
            normalizedInitial.appliedDesiredState = snapshotAtRevision(
                normalizedInitial.desiredState,
                normalizedInitial.appliedRevision
            );
        }
        authority->initializeResult = {
            .success = true,
            .snapshot = normalizedInitial,
        };
        QString error;
        if (!service->initializeAuthority(std::move(ownedAuthority), error)) {
            qFatal("fake authority initialization failed: %s", qPrintable(error));
        }
    }
};

void setSharedBorderOverride(
    ServiceHarness &harness,
    const quint64 revision = 1
)
{
    harness.sharedBorderSource->setProjection({
        .borderEnabled = true,
        .borderWidth = 1,
        .borderRadius = 0,
        .syncWindowBorders = false,
        .syncWindowSpacing = false,
        .revision = revision,
    });
}

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
    void sharedSpacingEditorOwnsExactDerivedGapsAndProtectedRule()
    {
        const auto catalog = protectedCatalog();
        QVERIFY(!catalog.isEmpty());
        const auto digest = sha256(catalog);
        SharedSpacingReconciler reconciler;
        QString error;
        QVERIFY2(reconciler.configure(catalog, digest, error), qPrintable(error));

        auto snapshot = sharedBorderSnapshot(
            5,
            digest,
            QJsonObject{{QStringLiteral("hyprland.animations.enabled"), false}}
        );
        auto snapshotObject = QJsonDocument::fromJson(snapshot).object();
        snapshotObject.insert(QStringLiteral("workspaceRules"), QJsonArray{});
        snapshot = JsonSupport::canonicalJson(snapshotObject);
        snapshot.append('\n');
        const SharedVisualProjection projection{
            .borderEnabled = true,
            .borderWidth = 1,
            .borderRadius = 15,
            .syncWindowBorders = true,
            .innerSpacing = 8,
            .outerSpacing = 12,
            .syncWindowSpacing = true,
            .revision = 17,
        };
        const auto edit = reconciler.edit(
            snapshot, 5, digest, projection, error
        );
        QVERIFY2(edit.has_value(), qPrintable(error));
        QVERIFY(edit->changed);
        QVERIFY(edit->spacingChanged);
        QVERIFY(edit->protectedRuleChanged);
        auto candidate = QJsonDocument::fromJson(edit->candidate).object();
        const auto overrides = candidate.value(
            QStringLiteral("overrides")
        ).toObject();
        QCOMPARE(
            overrides.value(QStringLiteral("hyprland.general.gaps_in"))
                .toArray(),
            QJsonArray({8, 8, 8, 8})
        );
        QCOMPARE(
            overrides.value(QStringLiteral("hyprland.general.gaps_out"))
                .toArray(),
            QJsonArray({0, 12, 12, 12})
        );
        QCOMPARE(
            overrides.value(QStringLiteral("hyprland.animations.enabled"))
                .toBool(),
            false
        );
        QCOMPARE(
            candidate.value(QStringLiteral("workspaceRules")).toArray(),
            QJsonArray({exactProtectedSpacingRule()})
        );
        QVERIFY(reconciler.hasExactFinalProtectedRule(
            edit->candidate, error
        ));

        const QJsonObject userRule{
            {QStringLiteral("id"), QStringLiteral("user-one")},
            {QStringLiteral("selector"), QStringLiteral("1")},
            {QStringLiteral("enabled"), true},
            {QStringLiteral("monitor"), QString()},
            {QStringLiteral("persistent"), false},
            {QStringLiteral("isDefault"), false},
            {QStringLiteral("layout"), QString()},
            {QStringLiteral("overrides"), QJsonObject{}},
        };
        candidate.insert(
            QStringLiteral("workspaceRules"),
            QJsonArray{exactProtectedSpacingRule(), userRule}
        );
        snapshot = JsonSupport::canonicalJson(candidate) + '\n';
        const auto moved = reconciler.edit(
            snapshot, 5, digest, projection, error
        );
        QVERIFY2(moved.has_value(), qPrintable(error));
        const auto movedRules = QJsonDocument::fromJson(moved->candidate)
                                    .object()
                                    .value(QStringLiteral("workspaceRules"))
                                    .toArray();
        QCOMPARE(movedRules, QJsonArray({userRule, exactProtectedSpacingRule()}));

        candidate.insert(
            QStringLiteral("workspaceRules"),
            QJsonArray{
                userRule,
                exactProtectedSpacingRule(),
                exactProtectedSpacingRule(),
            }
        );
        snapshot = JsonSupport::canonicalJson(candidate) + '\n';
        const auto duplicated = reconciler.edit(
            snapshot, 5, digest, projection, error
        );
        QVERIFY(!duplicated.has_value());
        QVERIFY(error.contains(QStringLiteral("duplicated")));

        auto spoofed = exactProtectedSpacingRule();
        spoofed.insert(QStringLiteral("selector"), QStringLiteral("1"));
        candidate.insert(
            QStringLiteral("workspaceRules"), QJsonArray{spoofed}
        );
        snapshot = JsonSupport::canonicalJson(candidate) + '\n';
        const auto reservedId = reconciler.edit(
            snapshot, 5, digest, projection, error
        );
        QVERIFY(!reservedId.has_value());
        QVERIFY(error.contains(QStringLiteral("invalid")));

        spoofed = exactProtectedSpacingRule();
        spoofed.insert(QStringLiteral("id"), QStringLiteral("user-spoof"));
        candidate.insert(
            QStringLiteral("workspaceRules"), QJsonArray{spoofed}
        );
        snapshot = JsonSupport::canonicalJson(candidate) + '\n';
        const auto reservedSelector = reconciler.edit(
            snapshot, 5, digest, projection, error
        );
        QVERIFY(!reservedSelector.has_value());
        QVERIFY(error.contains(QStringLiteral("invalid")));
    }

    void sharedBorderEditorChangesOnlyOwnedValuesAndElidesDefaults()
    {
        const auto catalog = protectedCatalog();
        QVERIFY(!catalog.isEmpty());
        const auto digest = sha256(catalog);
        SharedBorderReconciler reconciler;
        QString error;
        QVERIFY2(reconciler.configure(catalog, digest, error), qPrintable(error));

        const auto snapshot = sharedBorderSnapshot(
            5,
            digest,
            QJsonObject{
                {QStringLiteral("hyprland.animations.enabled"), false},
                {QStringLiteral("hyprland.general.border_size"), 7},
                {QStringLiteral("hyprland.decoration.rounding"), 9},
            }
        );
        const SharedVisualProjection projection{
            .borderEnabled = false,
            .borderWidth = 20,
            .borderRadius = 0,
            .syncWindowBorders = true,
            .syncWindowSpacing = false,
            .revision = 12,
        };
        const auto edit = reconciler.edit(
            snapshot, 5, digest, projection, error
        );
        QVERIFY2(edit.has_value(), qPrintable(error));
        QVERIFY(edit->changed);
        const auto object = QJsonDocument::fromJson(edit->candidate).object();
        const auto overrides = object.value(QStringLiteral("overrides"))
                                   .toObject();
        QCOMPARE(
            overrides.value(QStringLiteral("hyprland.general.border_size"))
                .toInt(-1),
            0
        );
        QVERIFY(!overrides.contains(
            QStringLiteral("hyprland.decoration.rounding")
        ));
        QCOMPARE(
            overrides.value(QStringLiteral("hyprland.animations.enabled"))
                .toBool(),
            false
        );
        QCOMPARE(object.value(QStringLiteral("revision")).toString(),
                 QStringLiteral("5"));
        const auto resolved = reconciler.resolvedValues(
            edit->candidate, error
        );
        QVERIFY(resolved.has_value());
        QCOMPARE(resolved->borderSize, quint32(0));
        QCOMPARE(resolved->rounding, quint32(0));
    }

    void sharedBorderSyncOnDirtyBaseSavesWithoutApplyingUnrelatedChanges()
    {
        const auto catalog = protectedCatalog();
        QVERIFY(!catalog.isEmpty());
        const auto digest = sha256(catalog);
        auto initial = dirtySnapshot();
        initial.catalogDigest = digest;
        initial.desiredState = sharedBorderSnapshot(
            initial.revision,
            digest,
            QJsonObject{{QStringLiteral("hyprland.animations.enabled"), false}}
        );
        ServiceHarness harness(initial);
        harness.authority->optionCatalogBytes = catalog;
        QCOMPARE(harness.sharedBorderSource->startCalls, 1);

        const SharedVisualProjection projection{
            .borderEnabled = true,
            .borderWidth = 4,
            .borderRadius = 8,
            .syncWindowBorders = true,
            .syncWindowSpacing = false,
            .revision = 17,
        };
        SharedBorderReconciler editor;
        QString error;
        QVERIFY(editor.configure(catalog, digest, error));
        const auto edit = editor.edit(
            initial.desiredState,
            initial.revision,
            digest,
            projection,
            error
        );
        QVERIFY(edit && edit->changed);
        auto saved = initial;
        saved.revision = initial.revision + 1;
        saved.desiredState = snapshotAtRevision(
            edit->candidate, saved.revision
        );
        harness.authority->replaceResult = {
            .success = true,
            .snapshot = saved,
        };

        QSignalSpy published(
            harness.service.get(),
            &CompositorService::propertiesPublished
        );
        harness.sharedBorderSource->setProjection(projection);
        QCOMPARE(harness.service->sharedBorderSyncState(),
                 QStringLiteral("unavailable"));
        QCOMPARE(harness.service->sharedBorderSourceRevision(), qulonglong(0));
        QTRY_COMPARE(harness.service->sharedBorderSyncState(),
                     QStringLiteral("saved"));
        QCOMPARE(harness.service->sharedBorderSourceRevision(),
                 qulonglong(projection.revision));
        QVERIFY(harness.service->sharedBorderSyncError().isEmpty());
        QCOMPARE(harness.authority->replaceCalls, 1);
        QCOMPARE(harness.authority->prepareApplyCalls, 0);
        QCOMPARE(harness.backend->activateCalls, 0);
        QCOMPARE(harness.backend->adoptCalls, 0);
        QCOMPARE(harness.authority->commitCalls, 0);
        bool foundAtomicSourcePublication = false;
        for (const auto &arguments : published) {
            const auto changed = arguments.at(0).toMap();
            if (changed.value(QStringLiteral("SharedBorderSourceRevision"))
                    .toULongLong() == projection.revision) {
                QVERIFY(changed.contains(QStringLiteral("SharedBorderSyncState")));
                QVERIFY(changed.contains(QStringLiteral("SharedBorderSyncError")));
                foundAtomicSourcePublication = true;
            }
        }
        QVERIFY(foundAtomicSourcePublication);
        const auto overrides = QJsonDocument::fromJson(
            harness.authority->lastReplaceCandidate
        ).object().value(QStringLiteral("overrides")).toObject();
        QCOMPARE(overrides.value(
                     QStringLiteral("hyprland.general.border_size")
                 ).toInt(-1), 4);
        QCOMPARE(overrides.value(
                     QStringLiteral("hyprland.decoration.rounding")
                 ).toInt(-1), 8);
        QCOMPARE(overrides.value(
                     QStringLiteral("hyprland.animations.enabled")
                 ).toBool(), false);

        harness.service->RetrySharedBorderSync();
        QTRY_COMPARE(harness.sharedBorderSource->refreshCalls, 1);
        QTest::qWait(20);
        QCOMPARE(harness.authority->replaceCalls, 1);
        QCOMPARE(harness.authority->prepareApplyCalls, 0);
        QCOMPARE(harness.backend->activateCalls, 0);
        QCOMPARE(harness.authority->commitCalls, 0);
    }

    void sharedBorderSyncSavesAndAppliesOneExactManagedRevision()
    {
        const auto catalog = protectedCatalog();
        QVERIFY(!catalog.isEmpty());
        const auto digest = sha256(catalog);
        auto initial = committedSnapshot(
            5,
            sharedBorderSnapshot(
                5,
                digest,
                QJsonObject{
                    {QStringLiteral("hyprland.animations.enabled"), false},
                }
            )
        );
        initial.catalogDigest = digest;
        auto management = managedStatus();
        management.managedGeneration = QString::fromLatin1(generationId);
        ServiceHarness harness(initial, management);
        harness.authority->optionCatalogBytes = catalog;

        const SharedVisualProjection projection{
            .borderEnabled = true,
            .borderWidth = 4,
            .borderRadius = 8,
            .syncWindowBorders = true,
            .syncWindowSpacing = false,
            .revision = 17,
        };
        SharedBorderReconciler editor;
        QString error;
        QVERIFY(editor.configure(catalog, digest, error));
        const auto edit = editor.edit(
            initial.desiredState,
            initial.revision,
            digest,
            projection,
            error
        );
        QVERIFY(edit && edit->changed);
        auto saved = initial;
        saved.revision++;
        saved.appliedRevision = initial.revision;
        saved.applyState = QStringLiteral("retained");
        saved.requiredActivation = ActivationRequirement::Reload;
        saved.desiredState = snapshotAtRevision(edit->candidate, saved.revision);
        harness.authority->replaceResult = {
            .success = true,
            .snapshot = saved,
        };
        harness.authority->prepareApplyResult = {
            .success = true,
            .snapshot = saved,
            .prepared = preparedGeneration(
                ActivationRequirement::Reload, saved.revision
            ),
        };
        auto current = saved;
        current.appliedRevision = current.revision;
        current.applyState = QStringLiteral("current");
        current.requiredActivation.reset();
        current.generationDigest = QString::fromLatin1(generationId);
        current.appliedDesiredState = current.desiredState;
        harness.authority->commitResult = {
            .success = true,
            .snapshot = current,
        };
        harness.backend->activationResult = {
            .success = true,
            .activationMayHaveOccurred = true,
            .generation = QString::fromLatin1(generationId),
            .confirmedRequirement = ActivationRequirement::Reload,
            .receipt = {QByteArrayLiteral("shared-border-rollback")},
            .status = management,
        };

        harness.sharedBorderSource->setProjection(projection);
        QTRY_COMPARE(harness.service->sharedBorderSyncState(),
                     QStringLiteral("current"));
        QCOMPARE(harness.service->sharedBorderSourceRevision(),
                 qulonglong(projection.revision));
        QCOMPARE(harness.authority->replaceCalls, 1);
        QCOMPARE(harness.authority->prepareApplyCalls, 1);
        QCOMPARE(harness.backend->activateCalls, 1);
        QCOMPARE(harness.backend->adoptCalls, 0);
        QCOMPARE(harness.authority->commitCalls, 1);
        const auto overrides = QJsonDocument::fromJson(
            harness.authority->lastReplaceCandidate
        ).object().value(QStringLiteral("overrides")).toObject();
        QCOMPARE(overrides.value(
                     QStringLiteral("hyprland.general.border_size")
                 ).toInt(-1), 4);
        QCOMPARE(overrides.value(
                     QStringLiteral("hyprland.decoration.rounding")
                 ).toInt(-1), 8);
        QCOMPARE(overrides.value(
                     QStringLiteral("hyprland.animations.enabled")
                 ).toBool(), false);
    }

    void sharedVisualSyncCoalescesBorderAndSpacingIntoOneCasAndApply()
    {
        const auto catalog = protectedCatalog();
        QVERIFY(!catalog.isEmpty());
        const auto digest = sha256(catalog);
        auto initial = committedSnapshot(
            5,
            sharedBorderSnapshot(
                5,
                digest,
                QJsonObject{{
                    QStringLiteral("hyprland.animations.enabled"), false
                }}
            )
        );
        initial.catalogDigest = digest;
        auto management = managedStatus();
        management.managedGeneration = QString::fromLatin1(generationId);
        ServiceHarness harness(initial, management);

        const SharedVisualProjection projection{
            .borderEnabled = true,
            .borderWidth = 4,
            .borderRadius = 8,
            .syncWindowBorders = true,
            .innerSpacing = 8,
            .outerSpacing = 12,
            .syncWindowSpacing = true,
            .revision = 31,
        };
        QString error;
        const auto candidate = reconciledSharedVisualSnapshot(
            initial.desiredState,
            initial.revision,
            digest,
            projection,
            true,
            true,
            error
        );
        QVERIFY2(candidate.has_value(), qPrintable(error));

        auto saved = initial;
        saved.revision++;
        saved.appliedRevision = initial.revision;
        saved.applyState = QStringLiteral("retained");
        saved.requiredActivation = ActivationRequirement::Reload;
        saved.desiredState = snapshotAtRevision(*candidate, saved.revision);
        harness.authority->replaceResult = {
            .success = true,
            .snapshot = saved,
        };
        harness.authority->prepareApplyResult = {
            .success = true,
            .snapshot = saved,
            .prepared = preparedGeneration(
                ActivationRequirement::Reload, saved.revision
            ),
        };
        auto current = saved;
        current.appliedRevision = current.revision;
        current.applyState = QStringLiteral("current");
        current.requiredActivation.reset();
        current.generationDigest = QString::fromLatin1(generationId);
        current.appliedDesiredState = current.desiredState;
        harness.authority->commitResult = {
            .success = true,
            .snapshot = current,
        };
        harness.backend->activationResult = {
            .success = true,
            .activationMayHaveOccurred = true,
            .generation = QString::fromLatin1(generationId),
            .confirmedRequirement = ActivationRequirement::Reload,
            .receipt = {QByteArrayLiteral("shared-visual-coalesced")},
            .status = management,
        };

        harness.sharedBorderSource->setProjection(projection);
        QTRY_COMPARE(harness.service->sharedBorderSyncState(),
                     QStringLiteral("current"));
        QTRY_COMPARE(harness.service->sharedSpacingSyncState(),
                     QStringLiteral("current"));
        QCOMPARE(harness.service->sharedBorderSourceRevision(),
                 qulonglong(projection.revision));
        QCOMPARE(harness.service->sharedSpacingSourceRevision(),
                 qulonglong(projection.revision));
        QCOMPARE(harness.authority->replaceCalls, 1);
        QCOMPARE(harness.authority->prepareApplyCalls, 1);
        QCOMPARE(harness.backend->activateCalls, 1);
        QCOMPARE(harness.authority->commitCalls, 1);
        QCOMPARE(harness.authority->lastReplaceCandidate, *candidate);

        const auto object = QJsonDocument::fromJson(
            harness.authority->lastReplaceCandidate
        ).object();
        const auto overrides = object.value(
            QStringLiteral("overrides")
        ).toObject();
        QCOMPARE(overrides.value(
                     QStringLiteral("hyprland.general.gaps_in")
                 ).toArray(), QJsonArray({8, 8, 8, 8}));
        QCOMPARE(overrides.value(
                     QStringLiteral("hyprland.general.gaps_out")
                 ).toArray(), QJsonArray({0, 12, 12, 12}));
        QCOMPARE(object.value(QStringLiteral("workspaceRules")).toArray(),
                 QJsonArray({exactProtectedSpacingRule()}));
    }

    void unsyncedSpacingRepairsProtectedRuleWithoutActivatingDirtyBase()
    {
        const auto catalog = protectedCatalog();
        const auto digest = sha256(catalog);
        QJsonObject overrides{
            {QStringLiteral("hyprland.general.gaps_in"),
             QJsonArray({2, 3, 4, 5})},
            {QStringLiteral("hyprland.general.gaps_out"),
             QJsonArray({6, 7, 8, 9})},
            {QStringLiteral("hyprland.animations.enabled"), false},
        };
        auto withoutRule = QJsonDocument::fromJson(
            sharedBorderSnapshot(5, digest, overrides)
        ).object();
        withoutRule.insert(QStringLiteral("workspaceRules"), QJsonArray{});
        auto desired = JsonSupport::canonicalJson(withoutRule);
        desired.append('\n');

        auto initial = dirtySnapshot();
        initial.catalogDigest = digest;
        initial.desiredState = desired;
        initial.appliedDesiredState = snapshotAtRevision(desired, 4);
        initial.generationDigest = QStringLiteral("old-generation");
        auto management = managedStatus();
        management.managedGeneration = initial.generationDigest;
        ServiceHarness harness(initial, management);

        SharedVisualProjection projection{
            .borderEnabled = true,
            .borderWidth = 1,
            .borderRadius = 0,
            .syncWindowBorders = false,
            .innerSpacing = 8,
            .outerSpacing = 12,
            .syncWindowSpacing = false,
            .revision = 40,
        };
        QString error;
        const auto candidate = reconciledSharedVisualSnapshot(
            initial.desiredState,
            initial.revision,
            digest,
            projection,
            false,
            true,
            error
        );
        QVERIFY2(candidate.has_value(), qPrintable(error));
        auto saved = initial;
        saved.revision++;
        saved.desiredState = snapshotAtRevision(*candidate, saved.revision);
        saved.applyState = QStringLiteral("retained");
        saved.requiredActivation = ActivationRequirement::Reload;
        harness.authority->replaceResult = {
            .success = true,
            .snapshot = saved,
        };

        harness.sharedBorderSource->setProjection(projection);
        QTRY_COMPARE(harness.service->sharedBorderSyncState(),
                     QStringLiteral("override"));
        QTRY_COMPARE(harness.service->sharedSpacingSyncState(),
                     QStringLiteral("saved"));
        QCOMPARE(harness.authority->replaceCalls, 1);
        QCOMPARE(harness.authority->prepareApplyCalls, 0);
        QCOMPARE(harness.backend->activateCalls, 0);
        const auto replacement = QJsonDocument::fromJson(
            harness.authority->lastReplaceCandidate
        ).object();
        const auto replacementOverrides = replacement.value(
            QStringLiteral("overrides")
        ).toObject();
        QCOMPARE(replacementOverrides.value(
                     QStringLiteral("hyprland.general.gaps_in")
                 ).toArray(), QJsonArray({2, 3, 4, 5}));
        QCOMPARE(replacementOverrides.value(
                     QStringLiteral("hyprland.general.gaps_out")
                 ).toArray(), QJsonArray({6, 7, 8, 9}));
        QCOMPARE(replacement.value(
                     QStringLiteral("workspaceRules")
                 ).toArray(), QJsonArray({exactProtectedSpacingRule()}));

        projection.revision++;
        harness.sharedBorderSource->setProjection(projection);
        QTRY_COMPARE(harness.service->sharedSpacingSourceRevision(),
                     qulonglong(projection.revision));
        QCOMPARE(harness.service->sharedSpacingSyncState(),
                 QStringLiteral("saved"));
        QCOMPARE(harness.authority->replaceCalls, 1);
        QCOMPARE(harness.authority->prepareApplyCalls, 0);

        harness.authority->prepareApplyResult = {
            .success = true,
            .snapshot = saved,
            .prepared = preparedGeneration(
                ActivationRequirement::Reload, saved.revision
            ),
        };
        auto current = saved;
        current.appliedRevision = current.revision;
        current.applyState = QStringLiteral("current");
        current.requiredActivation.reset();
        current.generationDigest = QString::fromLatin1(generationId);
        current.appliedDesiredState = current.desiredState;
        harness.authority->commitResult = {
            .success = true,
            .snapshot = current,
        };
        harness.backend->activationResult = {
            .success = true,
            .activationMayHaveOccurred = true,
            .generation = QString::fromLatin1(generationId),
            .confirmedRequirement = ActivationRequirement::Reload,
            .receipt = {QByteArrayLiteral("protected-rule-apply")},
            .status = management,
        };
        QString generation;
        QCOMPARE(harness.service->Apply(
                     saved.revision,
                     digest,
                     saved.actionCatalogDigest,
                     generation
                 ), qulonglong(saved.revision));
        QTRY_COMPARE(harness.service->sharedSpacingSyncState(),
                     QStringLiteral("override"));
        QCOMPARE(harness.authority->prepareApplyCalls, 1);
        QCOMPARE(harness.backend->activateCalls, 1);
        QCOMPARE(harness.authority->commitCalls, 1);

        auto unrelatedObject = QJsonDocument::fromJson(
            current.desiredState
        ).object();
        auto unrelatedOverrides = unrelatedObject.value(
            QStringLiteral("overrides")
        ).toObject();
        unrelatedOverrides.insert(
            QStringLiteral("hyprland.animations.enabled"), true
        );
        unrelatedObject.insert(QStringLiteral("overrides"), unrelatedOverrides);
        auto unrelatedCandidate = JsonSupport::canonicalJson(unrelatedObject);
        unrelatedCandidate.append('\n');
        auto unrelated = current;
        unrelated.revision++;
        unrelated.appliedRevision = current.revision;
        unrelated.applyState = QStringLiteral("retained");
        unrelated.requiredActivation = ActivationRequirement::Reload;
        unrelated.desiredState = snapshotAtRevision(
            unrelatedCandidate, unrelated.revision
        );
        unrelated.appliedDesiredState = current.desiredState;
        harness.authority->replaceResult = {
            .success = true,
            .snapshot = unrelated,
        };
        QCOMPARE(harness.service->ReplaceSnapshot(
                     current.revision,
                     digest,
                     current.actionCatalogDigest,
                     unrelatedCandidate
                 ), qulonglong(unrelated.revision));
        projection.revision++;
        harness.sharedBorderSource->setProjection(projection);
        QTRY_COMPARE(harness.service->sharedSpacingSourceRevision(),
                     qulonglong(projection.revision));
        QCOMPARE(harness.service->sharedSpacingSyncState(),
                 QStringLiteral("override"));

        harness.backend->statusValue = {
            .state = ManagementState::Conflict,
            .entrypointKind = EntrypointKind::Regular,
        };
        projection.revision++;
        harness.sharedBorderSource->setProjection(projection);
        QTRY_COMPARE(harness.service->sharedSpacingSyncState(),
                     QStringLiteral("saved"));
    }

    void sharedVisualCurrentProofUsesAppliedValuesAndLiveGeneration()
    {
        const auto catalog = protectedCatalog();
        const auto digest = sha256(catalog);
        SharedVisualProjection projection{
            .borderEnabled = true,
            .borderWidth = 4,
            .borderRadius = 8,
            .syncWindowBorders = true,
            .innerSpacing = 8,
            .outerSpacing = 12,
            .syncWindowSpacing = true,
            .revision = 50,
        };
        QString error;
        const auto target = reconciledSharedVisualSnapshot(
            sharedBorderSnapshot(5, digest),
            5,
            digest,
            projection,
            true,
            true,
            error
        );
        QVERIFY2(target.has_value(), qPrintable(error));
        auto initial = committedSnapshot(5, *target);
        initial.catalogDigest = digest;
        auto management = managedStatus();
        management.managedGeneration = QString::fromLatin1(generationId);
        ServiceHarness harness(initial, management);

        harness.sharedBorderSource->setProjection(projection);
        QTRY_COMPARE(harness.service->sharedBorderSyncState(),
                     QStringLiteral("current"));
        QTRY_COMPARE(harness.service->sharedSpacingSyncState(),
                     QStringLiteral("current"));

        auto unrelatedObject = QJsonDocument::fromJson(
            initial.desiredState
        ).object();
        auto unrelatedOverrides = unrelatedObject.value(
            QStringLiteral("overrides")
        ).toObject();
        unrelatedOverrides.insert(
            QStringLiteral("hyprland.animations.enabled"), false
        );
        unrelatedObject.insert(QStringLiteral("overrides"), unrelatedOverrides);
        auto unrelatedCandidate = JsonSupport::canonicalJson(unrelatedObject);
        unrelatedCandidate.append('\n');
        auto dirty = initial;
        dirty.revision++;
        dirty.appliedRevision = initial.revision;
        dirty.applyState = QStringLiteral("retained");
        dirty.requiredActivation = ActivationRequirement::Reload;
        dirty.desiredState = snapshotAtRevision(
            unrelatedCandidate, dirty.revision
        );
        dirty.appliedDesiredState = initial.desiredState;
        harness.authority->replaceResult = {
            .success = true,
            .snapshot = dirty,
        };
        QCOMPARE(harness.service->ReplaceSnapshot(
                     initial.revision,
                     digest,
                     initial.actionCatalogDigest,
                     unrelatedCandidate
                 ), qulonglong(dirty.revision));

        projection.revision++;
        harness.sharedBorderSource->setProjection(projection);
        QTRY_COMPARE(harness.service->sharedBorderSourceRevision(),
                     qulonglong(projection.revision));
        QCOMPARE(harness.service->sharedBorderSyncState(),
                 QStringLiteral("current"));
        QCOMPARE(harness.service->sharedSpacingSyncState(),
                 QStringLiteral("current"));
        QCOMPARE(harness.authority->replaceCalls, 1);
        QCOMPARE(harness.backend->activateCalls, 0);

        projection.borderWidth = 6;
        projection.borderRadius = 10;
        projection.innerSpacing = 9;
        projection.outerSpacing = 14;
        projection.revision++;
        const auto correctedCandidate = reconciledSharedVisualSnapshot(
            dirty.desiredState,
            dirty.revision,
            digest,
            projection,
            true,
            true,
            error
        );
        QVERIFY2(correctedCandidate.has_value(), qPrintable(error));
        auto corrected = dirty;
        corrected.revision++;
        corrected.desiredState = snapshotAtRevision(
            *correctedCandidate, corrected.revision
        );
        harness.authority->replaceResult = {
            .success = true,
            .snapshot = corrected,
        };
        harness.sharedBorderSource->setProjection(projection);
        QTRY_COMPARE(harness.service->sharedBorderSyncState(),
                     QStringLiteral("saved"));
        QTRY_COMPARE(harness.service->sharedSpacingSyncState(),
                     QStringLiteral("saved"));
        QCOMPARE(harness.authority->replaceCalls, 2);
        QCOMPARE(harness.authority->prepareApplyCalls, 0);
        QCOMPARE(harness.backend->activateCalls, 0);
    }

    void sharedVisualCurrentProofFailsClosedForEveryManagementDrift()
    {
        const auto catalog = protectedCatalog();
        const auto digest = sha256(catalog);
        SharedVisualProjection baseline{
            .borderEnabled = true,
            .borderWidth = 4,
            .borderRadius = 8,
            .syncWindowBorders = true,
            .innerSpacing = 8,
            .outerSpacing = 12,
            .syncWindowSpacing = true,
            .revision = 60,
        };
        QString error;
        const auto target = reconciledSharedVisualSnapshot(
            sharedBorderSnapshot(5, digest),
            5,
            digest,
            baseline,
            true,
            true,
            error
        );
        QVERIFY2(target.has_value(), qPrintable(error));

        const QVector<ManagementStatus> drifts{
            ManagementStatus{
                .state = ManagementState::Conflict,
                .entrypointKind = EntrypointKind::Regular,
            },
            ManagementStatus{
                .state = ManagementState::Unmanaged,
                .entrypointKind = EntrypointKind::Absent,
            },
            ManagementStatus{
                .state = ManagementState::Managed,
                .entrypointKind = EntrypointKind::Regular,
                .entrypointDigest = QStringLiteral("managed-entrypoint"),
                .managedGeneration = QStringLiteral("different-generation"),
            },
        };
        for (qsizetype index = 0; index < drifts.size(); ++index) {
            auto initial = committedSnapshot(5, *target);
            initial.catalogDigest = digest;
            auto management = managedStatus();
            management.managedGeneration = QString::fromLatin1(generationId);
            ServiceHarness harness(initial, management);
            auto projection = baseline;
            projection.revision += static_cast<quint64>(index);
            harness.sharedBorderSource->setProjection(projection);
            QTRY_COMPARE(harness.service->sharedBorderSyncState(),
                         QStringLiteral("current"));
            QTRY_COMPARE(harness.service->sharedSpacingSyncState(),
                         QStringLiteral("current"));

            harness.backend->statusValue = drifts.at(index);
            projection.revision++;
            harness.sharedBorderSource->setProjection(projection);
            QTRY_COMPARE(harness.service->sharedBorderSyncState(),
                         QStringLiteral("saved"));
            QTRY_COMPARE(harness.service->sharedSpacingSyncState(),
                         QStringLiteral("saved"));
            QCOMPARE(harness.authority->replaceCalls, 0);
            QCOMPARE(harness.backend->activateCalls, 0);
        }
    }

    void sharedBorderSyncRefreshesManagementBeforeAutoApplyClassification()
    {
        const auto catalog = protectedCatalog();
        QVERIFY(!catalog.isEmpty());
        const auto digest = sha256(catalog);
        auto initial = committedSnapshot(
            5, sharedBorderSnapshot(5, digest)
        );
        initial.catalogDigest = digest;
        auto initialManagement = managedStatus();
        initialManagement.managedGeneration = QString::fromLatin1(generationId);
        ServiceHarness harness(initial, initialManagement);
        harness.authority->optionCatalogBytes = catalog;

        const SharedVisualProjection projection{
            .borderEnabled = true,
            .borderWidth = 4,
            .borderRadius = 8,
            .syncWindowBorders = true,
            .syncWindowSpacing = false,
            .revision = 18,
        };
        SharedBorderReconciler editor;
        QString error;
        QVERIFY(editor.configure(catalog, digest, error));
        const auto edit = editor.edit(
            initial.desiredState,
            initial.revision,
            digest,
            projection,
            error
        );
        QVERIFY(edit && edit->changed);
        auto saved = initial;
        saved.revision++;
        saved.appliedRevision = initial.revision;
        saved.applyState = QStringLiteral("retained");
        saved.requiredActivation = ActivationRequirement::Reload;
        saved.desiredState = snapshotAtRevision(edit->candidate, saved.revision);
        harness.authority->replaceResult = {
            .success = true,
            .snapshot = saved,
        };
        harness.backend->statusValue = managedStatus();

        harness.sharedBorderSource->setProjection(projection);
        QTRY_COMPARE(harness.service->sharedBorderSyncState(),
                     QStringLiteral("saved"));
        QCOMPARE(harness.service->managementState(),
                 QStringLiteral("conflict"));
        QCOMPARE(harness.authority->replaceCalls, 1);
        QCOMPARE(harness.authority->prepareApplyCalls, 0);
        QCOMPARE(harness.backend->activateCalls, 0);
        QTest::qWait(20);
        QCOMPARE(harness.authority->replaceCalls, 1);
        QCOMPARE(harness.authority->prepareApplyCalls, 0);
    }

    void sharedBorderSyncSavesButNeverAdoptsAnUnmanagedEntrypoint()
    {
        const auto catalog = protectedCatalog();
        const auto digest = sha256(catalog);
        auto initial = dirtySnapshot();
        initial.catalogDigest = digest;
        initial.desiredState = sharedBorderSnapshot(initial.revision, digest);
        initial.appliedRevision = 0;
        initial.applyState = QStringLiteral("inactive");
        initial.generationDigest.clear();
        ServiceHarness harness(
            initial,
            ManagementStatus{
                .state = ManagementState::Unmanaged,
                .entrypointKind = EntrypointKind::Absent,
            }
        );
        harness.authority->optionCatalogBytes = catalog;
        auto saved = initial;
        saved.revision++;
        saved.desiredState = sharedBorderSnapshot(
            saved.revision,
            digest,
            QJsonObject{
                {QStringLiteral("hyprland.general.border_size"), 6},
                {QStringLiteral("hyprland.decoration.rounding"), 10},
            }
        );
        harness.authority->replaceResult = {
            .success = true,
            .snapshot = saved,
        };

        harness.sharedBorderSource->setProjection({
            .borderEnabled = true,
            .borderWidth = 6,
            .borderRadius = 10,
            .syncWindowBorders = true,
            .syncWindowSpacing = false,
            .revision = 2,
        });
        QTRY_COMPARE(harness.service->sharedBorderSyncState(),
                     QStringLiteral("saved"));
        QCOMPARE(harness.authority->replaceCalls, 1);
        QCOMPARE(harness.authority->prepareApplyCalls, 0);
        QCOMPARE(harness.backend->activateCalls, 0);
        QCOMPARE(harness.backend->adoptCalls, 0);
        QCOMPARE(harness.service->managementState(),
                 QStringLiteral("unmanaged"));
    }

    void sharedBorderOwnershipAllowsOnlyPreservingExternalReplacements()
    {
        const auto catalog = protectedCatalog();
        const auto digest = sha256(catalog);
        auto initial = dirtySnapshot();
        initial.catalogDigest = digest;
        initial.desiredState = sharedBorderSnapshot(
            initial.revision,
            digest,
            QJsonObject{
                {QStringLiteral("hyprland.general.border_size"), 4},
                {QStringLiteral("hyprland.decoration.rounding"), 8},
            }
        );
        ServiceHarness harness(initial);
        harness.authority->optionCatalogBytes = catalog;

        const auto diverging = sharedBorderSnapshot(
            initial.revision,
            digest,
            QJsonObject{
                {QStringLiteral("hyprland.general.border_size"), 5},
                {QStringLiteral("hyprland.decoration.rounding"), 8},
            }
        );
        QCOMPARE(harness.service->ReplaceSnapshot(
                     initial.revision,
                     digest,
                     initial.actionCatalogDigest,
                     diverging
                 ),
                 qulonglong(initial.revision));
        QCOMPARE(harness.authority->replaceCalls, 0);

        const auto preserving = sharedBorderSnapshot(
            initial.revision,
            digest,
            QJsonObject{
                {QStringLiteral("hyprland.general.border_size"), 4},
                {QStringLiteral("hyprland.decoration.rounding"), 8},
                {QStringLiteral("hyprland.animations.enabled"), false},
            }
        );
        auto unrelated = initial;
        unrelated.revision++;
        unrelated.desiredState = snapshotAtRevision(
            preserving, unrelated.revision
        );
        harness.authority->replaceResult = {
            .success = true,
            .snapshot = unrelated,
        };
        QCOMPARE(harness.service->ReplaceSnapshot(
                     initial.revision,
                     digest,
                     initial.actionCatalogDigest,
                     preserving
                 ),
                 qulonglong(unrelated.revision));
        QCOMPARE(harness.authority->replaceCalls, 1);

        harness.sharedBorderSource->setProjection({
            .borderEnabled = true,
            .borderWidth = 4,
            .borderRadius = 8,
            .syncWindowBorders = true,
            .syncWindowSpacing = false,
            .revision = 2,
        });
        QTRY_COMPARE(harness.service->sharedBorderSyncState(),
                     QStringLiteral("saved"));
        QCOMPARE(harness.authority->prepareApplyCalls, 0);
        QCOMPARE(harness.backend->activateCalls, 0);
        const auto controlledDivergence = sharedBorderSnapshot(
            unrelated.revision,
            digest,
            QJsonObject{
                {QStringLiteral("hyprland.general.border_size"), 5},
                {QStringLiteral("hyprland.decoration.rounding"), 8},
            }
        );
        QCOMPARE(harness.service->ReplaceSnapshot(
                     unrelated.revision,
                     digest,
                     initial.actionCatalogDigest,
                     controlledDivergence
                 ),
                 qulonglong(unrelated.revision));
        QCOMPARE(harness.authority->replaceCalls, 1);

        harness.sharedBorderSource->setProjection({
            .borderEnabled = true,
            .borderWidth = 4,
            .borderRadius = 8,
            .syncWindowBorders = false,
            .syncWindowSpacing = false,
            .revision = 3,
        });
        QTRY_COMPARE(harness.service->sharedBorderSyncState(),
                     QStringLiteral("override"));
        const auto overridden = sharedBorderSnapshot(
            unrelated.revision,
            digest,
            QJsonObject{
                {QStringLiteral("hyprland.general.border_size"), 11},
                {QStringLiteral("hyprland.decoration.rounding"), 2},
            }
        );
        auto overrideSaved = unrelated;
        overrideSaved.revision++;
        overrideSaved.desiredState = snapshotAtRevision(
            overridden, overrideSaved.revision
        );
        harness.authority->replaceResult = {
            .success = true,
            .snapshot = overrideSaved,
        };
        QCOMPARE(harness.service->ReplaceSnapshot(
                     unrelated.revision,
                     digest,
                     initial.actionCatalogDigest,
                     overridden
                 ),
                 qulonglong(overrideSaved.revision));
        QCOMPARE(harness.authority->replaceCalls, 2);
    }

    void sharedBorderOwnershipRejectsUnverifiedProtectedCatalog_data()
    {
        QTest::addColumn<QByteArray>("catalogBytes");
        QTest::addColumn<QString>("catalogDigest");
        QTest::addColumn<bool>("syncedPolicy");

        const auto catalog = protectedCatalog();
        QVERIFY(!catalog.isEmpty());
        const auto digest = sha256(catalog);
        const auto mismatchedDigest = QString(64, QLatin1Char('c'));
        QTest::newRow("missing-unknown")
            << QByteArray{} << digest << false;
        QTest::newRow("mismatched-unknown")
            << catalog << mismatchedDigest << false;
        QTest::newRow("missing-synced")
            << QByteArray{} << digest << true;
        QTest::newRow("mismatched-synced")
            << catalog << mismatchedDigest << true;
    }

    void sharedBorderOwnershipRejectsUnverifiedProtectedCatalog()
    {
        QFETCH(QByteArray, catalogBytes);
        QFETCH(QString, catalogDigest);
        QFETCH(bool, syncedPolicy);

        auto initial = dirtySnapshot();
        initial.catalogDigest = catalogDigest;
        initial.desiredState = sharedBorderSnapshot(
            initial.revision,
            catalogDigest,
            QJsonObject{
                {QStringLiteral("hyprland.general.border_size"), 4},
                {QStringLiteral("hyprland.decoration.rounding"), 8},
            }
        );
        ServiceHarness harness(initial);
        harness.authority->optionCatalogBytes = catalogBytes;
        if (syncedPolicy) {
            harness.sharedBorderSource->setProjection({
                .borderEnabled = true,
                .borderWidth = 4,
                .borderRadius = 8,
                .syncWindowBorders = true,
                .syncWindowSpacing = false,
                .revision = 2,
            });
            QTRY_COMPARE(harness.service->sharedBorderSyncState(),
                         QStringLiteral("failed"));
        }

        const auto diverging = sharedBorderSnapshot(
            initial.revision,
            catalogDigest,
            QJsonObject{
                {QStringLiteral("hyprland.general.border_size"), 5},
                {QStringLiteral("hyprland.decoration.rounding"), 8},
            }
        );
        QCOMPARE(harness.service->ReplaceSnapshot(
                     initial.revision,
                     catalogDigest,
                     initial.actionCatalogDigest,
                     diverging
                 ),
                 qulonglong(initial.revision));
        QCOMPARE(harness.authority->replaceCalls, 0);
    }

    void sharedBorderOverrideAllowsReplacementWithoutProtectedCatalog()
    {
        const auto catalog = protectedCatalog();
        QVERIFY(!catalog.isEmpty());
        const auto digest = sha256(catalog);
        auto initial = dirtySnapshot();
        initial.catalogDigest = digest;
        initial.desiredState = sharedBorderSnapshot(
            initial.revision,
            digest,
            QJsonObject{
                {QStringLiteral("hyprland.general.border_size"), 4},
                {QStringLiteral("hyprland.decoration.rounding"), 8},
            }
        );
        ServiceHarness harness(initial);
        QVERIFY(harness.service->available());
        QVERIFY(harness.service->writable());
        QCOMPARE(harness.service->catalogDigest(), digest);
        QCOMPARE(harness.service->actionCatalogDigest(),
                 initial.actionCatalogDigest);
        harness.sharedBorderSource->setProjection({
            .borderEnabled = true,
            .borderWidth = 4,
            .borderRadius = 8,
            .syncWindowBorders = false,
            .syncWindowSpacing = false,
            .revision = 3,
        });
        QTRY_COMPARE(harness.service->sharedBorderSyncState(),
                     QStringLiteral("override"));

        const auto candidate = sharedBorderSnapshot(
            initial.revision,
            digest,
            QJsonObject{
                {QStringLiteral("hyprland.general.border_size"), 11},
                {QStringLiteral("hyprland.decoration.rounding"), 2},
            }
        );
        auto saved = initial;
        saved.revision++;
        saved.desiredState = snapshotAtRevision(candidate, saved.revision);
        harness.authority->replaceResult = {
            .success = true,
            .snapshot = saved,
        };
        QCOMPARE(harness.service->ReplaceSnapshot(
                     initial.revision,
                     digest,
                     initial.actionCatalogDigest,
                     candidate
                 ),
                 qulonglong(saved.revision));
        QCOMPARE(harness.authority->replaceCalls, 1);
    }

    void sharedBorderSyncBlocksApplyUntilRetainedOverrideIsReconciled()
    {
        const auto catalog = protectedCatalog();
        QVERIFY(!catalog.isEmpty());
        const auto digest = sha256(catalog);
        auto initial = committedSnapshot(
            5, sharedBorderSnapshot(5, digest)
        );
        initial.catalogDigest = digest;
        auto management = managedStatus();
        management.managedGeneration = QString::fromLatin1(generationId);
        ServiceHarness harness(initial, management);
        harness.authority->optionCatalogBytes = catalog;
        setSharedBorderOverride(harness);
        QTRY_COMPARE(harness.service->sharedBorderSyncState(),
                     QStringLiteral("override"));

        const auto divergentCandidate = sharedBorderSnapshot(
            initial.revision,
            digest,
            QJsonObject{
                {QStringLiteral("hyprland.general.border_size"), 11},
                {QStringLiteral("hyprland.decoration.rounding"), 2},
            }
        );
        auto divergent = initial;
        divergent.revision++;
        divergent.appliedRevision = initial.revision;
        divergent.applyState = QStringLiteral("retained");
        divergent.requiredActivation = ActivationRequirement::Reload;
        divergent.desiredState = snapshotAtRevision(
            divergentCandidate, divergent.revision
        );
        harness.authority->replaceResult = {
            .success = true,
            .snapshot = divergent,
        };
        QCOMPARE(harness.service->ReplaceSnapshot(
                     initial.revision,
                     digest,
                     initial.actionCatalogDigest,
                     divergentCandidate
                 ),
                 qulonglong(divergent.revision));

        const SharedVisualProjection synced{
            .borderEnabled = true,
            .borderWidth = 4,
            .borderRadius = 8,
            .syncWindowBorders = true,
            .syncWindowSpacing = false,
            .revision = 2,
        };
        SharedBorderReconciler editor;
        QString error;
        QVERIFY(editor.configure(catalog, digest, error));
        const auto edit = editor.edit(
            divergent.desiredState,
            divergent.revision,
            digest,
            synced,
            error
        );
        QVERIFY(edit && edit->changed);
        auto corrected = divergent;
        corrected.revision++;
        corrected.desiredState = snapshotAtRevision(
            edit->candidate, corrected.revision
        );
        harness.authority->replaceResult = {
            .success = true,
            .snapshot = corrected,
        };

        harness.sharedBorderSource->setProjection(synced);
        QString appliedGeneration;
        QCOMPARE(harness.service->Apply(
                     divergent.revision,
                     digest,
                     initial.actionCatalogDigest,
                     appliedGeneration
                 ),
                 qulonglong(initial.appliedRevision));
        QCOMPARE(harness.authority->prepareApplyCalls, 0);
        QCOMPARE(harness.backend->activateCalls, 0);
        QCOMPARE(harness.authority->commitCalls, 0);

        QTRY_COMPARE(harness.service->revision(),
                     qulonglong(corrected.revision));
        QTRY_COMPARE(harness.service->sharedBorderSyncState(),
                     QStringLiteral("saved"));
        QCOMPARE(harness.authority->replaceCalls, 2);
        QCOMPARE(harness.authority->prepareApplyCalls, 0);
        QCOMPARE(harness.backend->activateCalls, 0);

        harness.authority->prepareApplyResult = {
            .success = true,
            .snapshot = corrected,
            .prepared = preparedGeneration(
                ActivationRequirement::Reload, corrected.revision
            ),
        };
        auto current = corrected;
        current.appliedRevision = current.revision;
        current.applyState = QStringLiteral("current");
        current.requiredActivation.reset();
        current.generationDigest = QString::fromLatin1(generationId);
        harness.authority->commitResult = {
            .success = true,
            .snapshot = current,
        };
        harness.backend->activationResult = {
            .success = true,
            .activationMayHaveOccurred = true,
            .generation = QString::fromLatin1(generationId),
            .confirmedRequirement = ActivationRequirement::Reload,
            .receipt = {QByteArrayLiteral("shared-border-apply-race")},
            .status = management,
        };

        QCOMPARE(harness.service->Apply(
                     corrected.revision,
                     digest,
                     initial.actionCatalogDigest,
                     appliedGeneration
                 ),
                 qulonglong(corrected.revision));
        QCOMPARE(harness.authority->prepareApplyCalls, 1);
        QCOMPARE(harness.backend->activateCalls, 1);
        QCOMPARE(harness.authority->commitCalls, 1);
    }

    void sharedBorderSyncBlocksAdoptionUntilRetainedOverrideIsReconciled()
    {
        const auto catalog = protectedCatalog();
        QVERIFY(!catalog.isEmpty());
        const auto digest = sha256(catalog);
        auto initial = dirtySnapshot();
        initial.catalogDigest = digest;
        initial.desiredState = sharedBorderSnapshot(initial.revision, digest);
        initial.appliedRevision = 0;
        initial.applyState = QStringLiteral("inactive");
        initial.generationDigest.clear();
        const ManagementStatus unmanaged{
            .state = ManagementState::Unmanaged,
            .entrypointKind = EntrypointKind::Absent,
        };
        ServiceHarness harness(initial, unmanaged);
        harness.authority->optionCatalogBytes = catalog;
        setSharedBorderOverride(harness);
        QTRY_COMPARE(harness.service->sharedBorderSyncState(),
                     QStringLiteral("override"));

        const auto divergentCandidate = sharedBorderSnapshot(
            initial.revision,
            digest,
            QJsonObject{
                {QStringLiteral("hyprland.general.border_size"), 11},
                {QStringLiteral("hyprland.decoration.rounding"), 2},
            }
        );
        auto divergent = initial;
        divergent.revision++;
        divergent.applyState = QStringLiteral("retained");
        divergent.desiredState = snapshotAtRevision(
            divergentCandidate, divergent.revision
        );
        harness.authority->replaceResult = {
            .success = true,
            .snapshot = divergent,
        };
        QCOMPARE(harness.service->ReplaceSnapshot(
                     initial.revision,
                     digest,
                     initial.actionCatalogDigest,
                     divergentCandidate
                 ),
                 qulonglong(divergent.revision));

        const SharedVisualProjection synced{
            .borderEnabled = true,
            .borderWidth = 4,
            .borderRadius = 8,
            .syncWindowBorders = true,
            .syncWindowSpacing = false,
            .revision = 2,
        };
        SharedBorderReconciler editor;
        QString error;
        QVERIFY(editor.configure(catalog, digest, error));
        const auto edit = editor.edit(
            divergent.desiredState,
            divergent.revision,
            digest,
            synced,
            error
        );
        QVERIFY(edit && edit->changed);
        auto corrected = divergent;
        corrected.revision++;
        corrected.desiredState = snapshotAtRevision(
            edit->candidate, corrected.revision
        );
        harness.authority->replaceResult = {
            .success = true,
            .snapshot = corrected,
        };

        harness.sharedBorderSource->setProjection(synced);
        QString adoptedGeneration;
        QString adoptedEntrypoint;
        QCOMPARE(harness.service->AdoptManagedConfiguration(
                     divergent.revision,
                     digest,
                     initial.actionCatalogDigest,
                     {},
                     adoptedGeneration,
                     adoptedEntrypoint
                 ),
                 qulonglong(initial.appliedRevision));
        QCOMPARE(harness.authority->prepareApplyCalls, 0);
        QCOMPARE(harness.backend->adoptCalls, 0);
        QCOMPARE(harness.authority->commitCalls, 0);

        QTRY_COMPARE(harness.service->revision(),
                     qulonglong(corrected.revision));
        QTRY_COMPARE(harness.service->sharedBorderSyncState(),
                     QStringLiteral("saved"));
        QCOMPARE(harness.authority->replaceCalls, 2);
        QCOMPARE(harness.authority->prepareApplyCalls, 0);
        QCOMPARE(harness.backend->adoptCalls, 0);

        harness.authority->prepareApplyResult = {
            .success = true,
            .snapshot = corrected,
            .prepared = preparedGeneration(
                ActivationRequirement::Reload, corrected.revision
            ),
        };
        auto current = corrected;
        current.appliedRevision = current.revision;
        current.applyState = QStringLiteral("current");
        current.requiredActivation.reset();
        current.generationDigest = QString::fromLatin1(generationId);
        harness.authority->commitResult = {
            .success = true,
            .snapshot = current,
        };
        harness.backend->adoptionResult = {
            .success = true,
            .activationMayHaveOccurred = true,
            .generation = QString::fromLatin1(generationId),
            .confirmedRequirement = ActivationRequirement::Reload,
            .receipt = {QByteArrayLiteral("shared-border-adoption-race")},
            .status = {
                .state = ManagementState::Managed,
                .entrypointKind = EntrypointKind::Regular,
                .entrypointDigest = QStringLiteral("managed-entrypoint"),
            },
        };

        QCOMPARE(harness.service->AdoptManagedConfiguration(
                     corrected.revision,
                     digest,
                     initial.actionCatalogDigest,
                     {},
                     adoptedGeneration,
                     adoptedEntrypoint
                 ),
                 qulonglong(corrected.revision));
        QCOMPARE(harness.authority->prepareApplyCalls, 1);
        QCOMPARE(harness.backend->adoptCalls, 1);
        QCOMPARE(harness.backend->activateCalls, 0);
        QCOMPARE(harness.authority->commitCalls, 1);
    }

    void sharedBorderApplyRequiresCurrentVerifiedSource()
    {
        const auto catalog = protectedCatalog();
        QVERIFY(!catalog.isEmpty());
        const auto digest = sha256(catalog);
        auto initial = dirtySnapshot();
        initial.catalogDigest = digest;
        initial.desiredState = sharedBorderSnapshot(initial.revision, digest);

        for (const bool loseVerifiedOverride : {false, true}) {
            ServiceHarness harness(initial);
            harness.authority->optionCatalogBytes = catalog;
            if (loseVerifiedOverride) {
                setSharedBorderOverride(harness);
                QTRY_COMPARE(harness.service->sharedBorderSyncState(),
                             QStringLiteral("override"));
                harness.sharedBorderSource->loseSource(
                    QStringLiteral("Config1 change is awaiting verification")
                );
                QVERIFY(!harness.sharedBorderSource->available());
            }

            QString generation;
            QCOMPARE(harness.service->Apply(
                         initial.revision,
                         digest,
                         initial.actionCatalogDigest,
                         generation
                     ),
                     qulonglong(initial.appliedRevision));
            QCOMPARE(harness.authority->prepareApplyCalls, 0);
            QCOMPARE(harness.backend->activateCalls, 0);
            QCOMPARE(harness.authority->commitCalls, 0);
        }
    }

    void sharedBorderAdoptionRequiresCurrentVerifiedSource()
    {
        const auto catalog = protectedCatalog();
        QVERIFY(!catalog.isEmpty());
        const auto digest = sha256(catalog);
        auto initial = dirtySnapshot();
        initial.catalogDigest = digest;
        initial.desiredState = sharedBorderSnapshot(initial.revision, digest);
        initial.appliedRevision = 0;
        initial.applyState = QStringLiteral("inactive");
        initial.generationDigest.clear();
        const ManagementStatus unmanaged{
            .state = ManagementState::Unmanaged,
            .entrypointKind = EntrypointKind::Absent,
        };

        for (const bool loseVerifiedOverride : {false, true}) {
            ServiceHarness harness(initial, unmanaged);
            harness.authority->optionCatalogBytes = catalog;
            if (loseVerifiedOverride) {
                setSharedBorderOverride(harness);
                QTRY_COMPARE(harness.service->sharedBorderSyncState(),
                             QStringLiteral("override"));
                harness.sharedBorderSource->loseSource(
                    QStringLiteral("Config1 change is awaiting verification")
                );
                QVERIFY(!harness.sharedBorderSource->available());
            }

            QString generation;
            QString entrypoint;
            QCOMPARE(harness.service->AdoptManagedConfiguration(
                         initial.revision,
                         digest,
                         initial.actionCatalogDigest,
                         {},
                         generation,
                         entrypoint
                     ),
                     qulonglong(initial.appliedRevision));
            QCOMPARE(harness.authority->prepareApplyCalls, 0);
            QCOMPARE(harness.backend->adoptCalls, 0);
            QCOMPARE(harness.authority->commitCalls, 0);
        }
    }

    void sharedBorderFailureDoesNotHotLoopAndHasBoundedRetryTriggers()
    {
        const auto catalog = protectedCatalog();
        const auto digest = sha256(catalog);
        auto initial = dirtySnapshot();
        initial.catalogDigest = digest;
        initial.desiredState = sharedBorderSnapshot(initial.revision, digest);
        ServiceHarness harness(initial);
        harness.authority->optionCatalogBytes = catalog;
        harness.authority->replaceResult = {
            .success = false,
            .errorCode = QStringLiteral("PersistenceFailed"),
            .errorMessage = QStringLiteral("injected shared-border failure"),
            .snapshot = initial,
        };

        auto projection = SharedVisualProjection{
            .borderEnabled = true,
            .borderWidth = 7,
            .borderRadius = 9,
            .syncWindowBorders = true,
            .syncWindowSpacing = false,
            .revision = 4,
        };
        harness.sharedBorderSource->setProjection(projection);
        QTRY_COMPARE(harness.service->sharedBorderSyncState(),
                     QStringLiteral("failed"));
        QCOMPARE(harness.authority->replaceCalls, 1);
        QTest::qWait(20);
        QCOMPARE(harness.authority->replaceCalls, 1);

        harness.service->RetrySharedBorderSync();
        QTRY_COMPARE(harness.authority->replaceCalls, 2);
        QCOMPARE(harness.sharedBorderSource->refreshCalls, 1);
        QTest::qWait(20);
        QCOMPARE(harness.authority->replaceCalls, 2);

        const auto retainedError = harness.service->sharedBorderSyncError();
        QSignalSpy published(
            harness.service.get(),
            &CompositorService::propertiesPublished
        );
        projection.revision++;
        harness.sharedBorderSource->setProjection(projection);
        QTRY_COMPARE(harness.service->sharedBorderSourceRevision(),
                     qulonglong(projection.revision));
        QCOMPARE(harness.service->sharedBorderSyncState(),
                 QStringLiteral("failed"));
        QCOMPARE(harness.service->sharedBorderSyncError(), retainedError);
        QTest::qWait(20);
        QCOMPARE(harness.authority->replaceCalls, 2);
        bool foundSuppressedRevision = false;
        for (const auto &arguments : published) {
            const auto changed = arguments.at(0).toMap();
            if (changed.value(QStringLiteral("SharedBorderSourceRevision"))
                    .toULongLong() == projection.revision) {
                QCOMPARE(changed.value(
                             QStringLiteral("SharedBorderSyncState")
                         ).toString(), QStringLiteral("failed"));
                QCOMPARE(changed.value(
                             QStringLiteral("SharedBorderSyncError")
                         ).toString(), retainedError);
                foundSuppressedRevision = true;
            }
        }
        QVERIFY(foundSuppressedRevision);

        projection.borderWidth++;
        projection.revision++;
        harness.sharedBorderSource->setProjection(projection);
        QTRY_COMPARE(harness.authority->replaceCalls, 3);
        QTest::qWait(20);
        QCOMPARE(harness.authority->replaceCalls, 3);
    }

    void sharedVisualActivationFailureSuppressesBothGroupsUntilEitherRetry()
    {
        const auto catalog = protectedCatalog();
        QVERIFY(!catalog.isEmpty());
        const auto digest = sha256(catalog);
        auto initial = committedSnapshot(
            5, sharedBorderSnapshot(5, digest)
        );
        initial.catalogDigest = digest;
        auto management = managedStatus();
        management.managedGeneration = QString::fromLatin1(generationId);
        ServiceHarness harness(initial, management);
        harness.authority->optionCatalogBytes = catalog;

        auto projection = SharedVisualProjection{
            .borderEnabled = true,
            .borderWidth = 7,
            .borderRadius = 9,
            .syncWindowBorders = true,
            .innerSpacing = 8,
            .outerSpacing = 12,
            .syncWindowSpacing = true,
            .revision = 30,
        };
        QString error;
        const auto candidate = reconciledSharedVisualSnapshot(
            initial.desiredState,
            initial.revision,
            digest,
            projection,
            true,
            true,
            error
        );
        QVERIFY2(candidate.has_value(), qPrintable(error));
        auto saved = initial;
        saved.revision++;
        saved.appliedRevision = initial.revision;
        saved.applyState = QStringLiteral("retained");
        saved.requiredActivation = ActivationRequirement::Reload;
        saved.desiredState = snapshotAtRevision(*candidate, saved.revision);
        harness.authority->replaceResult = {
            .success = true,
            .snapshot = saved,
        };
        harness.authority->prepareApplyResult = {
            .success = true,
            .snapshot = saved,
            .prepared = preparedGeneration(
                ActivationRequirement::Reload, saved.revision
            ),
        };
        harness.authority->abortResult = {
            .success = true,
            .snapshot = saved,
        };
        harness.backend->activationResult = {
            .success = false,
            .activationMayHaveOccurred = false,
            .errorCode = QStringLiteral("ApplyFailed"),
            .errorMessage = QStringLiteral("injected shared-visual activation failure"),
            .status = management,
        };

        harness.sharedBorderSource->setProjection(projection);
        QTRY_COMPARE(harness.service->sharedBorderSyncState(),
                     QStringLiteral("failed"));
        QTRY_COMPARE(harness.service->sharedSpacingSyncState(),
                     QStringLiteral("failed"));
        QCOMPARE(harness.service->sharedBorderSourceRevision(),
                 qulonglong(projection.revision));
        QCOMPARE(harness.authority->replaceCalls, 1);
        QCOMPARE(harness.authority->prepareApplyCalls, 1);
        QCOMPARE(harness.backend->activateCalls, 1);
        QCOMPARE(harness.authority->abortCalls, 1);
        const auto retainedError = harness.service->sharedBorderSyncError();
        QVERIFY(!retainedError.isEmpty());
        QCOMPARE(harness.service->sharedSpacingSyncError(), retainedError);

        QSignalSpy published(
            harness.service.get(),
            &CompositorService::propertiesPublished
        );
        projection.revision++;
        harness.sharedBorderSource->setProjection(projection);
        QTRY_COMPARE(harness.service->sharedBorderSourceRevision(),
                     qulonglong(projection.revision));
        QCOMPARE(harness.service->sharedBorderSyncState(),
                 QStringLiteral("failed"));
        QCOMPARE(harness.service->sharedSpacingSyncState(),
                 QStringLiteral("failed"));
        QCOMPARE(harness.service->sharedBorderSyncError(), retainedError);
        QCOMPARE(harness.service->sharedSpacingSyncError(), retainedError);
        QTest::qWait(20);
        QCOMPARE(harness.authority->prepareApplyCalls, 1);
        QCOMPARE(harness.backend->activateCalls, 1);
        QCOMPARE(harness.authority->abortCalls, 1);
        bool foundSuppressedRevision = false;
        bool foundCompleteSpacingRevision = false;
        for (const auto &arguments : published) {
            const auto changed = arguments.at(0).toMap();
            if (changed.value(QStringLiteral("SharedBorderSourceRevision"))
                    .toULongLong() == projection.revision) {
                QCOMPARE(changed.value(
                             QStringLiteral("SharedBorderSyncState")
                         ).toString(), QStringLiteral("failed"));
                QCOMPARE(changed.value(
                             QStringLiteral("SharedBorderSyncError")
                         ).toString(), retainedError);
                foundSuppressedRevision = true;
            }
            if (changed.value(QStringLiteral("SharedSpacingSourceRevision"))
                    .toULongLong() == projection.revision) {
                QCOMPARE(changed.value(
                             QStringLiteral("SharedSpacingSyncState")
                         ).toString(), QStringLiteral("failed"));
                QCOMPARE(changed.value(
                             QStringLiteral("SharedSpacingSyncError")
                         ).toString(), retainedError);
                foundCompleteSpacingRevision = true;
            }
        }
        QVERIFY(foundSuppressedRevision);
        QVERIFY(foundCompleteSpacingRevision);

        published.clear();
        harness.sharedBorderSource->loseSource(
            QStringLiteral("Config1 owner is refreshing")
        );
        QTRY_COMPARE(harness.service->sharedBorderSyncState(),
                     QStringLiteral("unavailable"));
        QTRY_COMPARE(harness.service->sharedSpacingSyncState(),
                     QStringLiteral("unavailable"));
        QCOMPARE(harness.service->sharedBorderSyncError(),
                 QStringLiteral("Config1 owner is refreshing"));
        QCOMPARE(harness.service->sharedSpacingSyncError(),
                 QStringLiteral("Config1 owner is refreshing"));
        QCOMPARE(harness.authority->prepareApplyCalls, 1);
        QCOMPARE(harness.backend->activateCalls, 1);
        QCOMPARE(harness.authority->abortCalls, 1);

        projection.revision++;
        harness.sharedBorderSource->setProjection(projection);
        QTRY_COMPARE(harness.service->sharedBorderSourceRevision(),
                     qulonglong(projection.revision));
        QTRY_COMPARE(harness.service->sharedBorderSyncState(),
                     QStringLiteral("failed"));
        QTRY_COMPARE(harness.service->sharedSpacingSyncState(),
                     QStringLiteral("failed"));
        QCOMPARE(harness.service->sharedBorderSyncError(), retainedError);
        QCOMPARE(harness.service->sharedSpacingSyncError(), retainedError);
        QTest::qWait(20);
        QCOMPARE(harness.authority->prepareApplyCalls, 1);
        QCOMPARE(harness.backend->activateCalls, 1);
        QCOMPARE(harness.authority->abortCalls, 1);
        bool foundRestoredFailure = false;
        for (const auto &arguments : published) {
            const auto changed = arguments.at(0).toMap();
            if (changed.value(QStringLiteral("SharedBorderSourceRevision"))
                    .toULongLong() == projection.revision) {
                QCOMPARE(changed.value(
                             QStringLiteral("SharedBorderSyncState")
                         ).toString(), QStringLiteral("failed"));
                QCOMPARE(changed.value(
                             QStringLiteral("SharedBorderSyncError")
                         ).toString(), retainedError);
                foundRestoredFailure = true;
            }
        }
        QVERIFY(foundRestoredFailure);

        harness.service->RetrySharedSpacingSync();
        QTRY_COMPARE(harness.authority->prepareApplyCalls, 2);
        QTRY_COMPARE(harness.backend->activateCalls, 2);
        QCOMPARE(harness.authority->abortCalls, 2);
        QCOMPARE(harness.sharedBorderSource->refreshCalls, 1);
        QTest::qWait(20);
        QCOMPARE(harness.authority->prepareApplyCalls, 2);
        QCOMPARE(harness.backend->activateCalls, 2);
        QCOMPARE(harness.authority->abortCalls, 2);
        QCOMPARE(harness.service->sharedBorderSyncState(),
                 QStringLiteral("failed"));
        QCOMPARE(harness.service->sharedSpacingSyncState(),
                 QStringLiteral("failed"));
        QCOMPARE(harness.service->sharedBorderSourceRevision(),
                 qulonglong(projection.revision));
    }

    void sharedVisualSyncDefersForTheWholeDisplayConfirmation()
    {
        const auto catalog = protectedCatalog();
        const auto digest = sha256(catalog);
        const SharedVisualProjection initialProjection{
            .borderEnabled = true,
            .borderWidth = 1,
            .borderRadius = 0,
            .syncWindowBorders = true,
            .innerSpacing = 8,
            .outerSpacing = 12,
            .syncWindowSpacing = true,
            .revision = 1,
        };
        QString error;
        const auto initialCandidate = reconciledSharedVisualSnapshot(
            sharedBorderSnapshot(5, digest),
            5,
            digest,
            initialProjection,
            true,
            true,
            error
        );
        QVERIFY2(initialCandidate.has_value(), qPrintable(error));
        auto initial = committedSnapshot(5, *initialCandidate);
        initial.catalogDigest = digest;
        auto initialManagement = managedStatus();
        initialManagement.managedGeneration = QString::fromLatin1(generationId);
        ServiceHarness harness(initial, initialManagement);
        harness.authority->optionCatalogBytes = catalog;
        harness.sharedBorderSource->setProjection(initialProjection);
        QTRY_COMPARE(harness.service->sharedBorderSyncState(),
                     QStringLiteral("current"));
        QTRY_COMPARE(harness.service->sharedSpacingSyncState(),
                     QStringLiteral("current"));

        const auto topology = connectedDisplayTopology();
        configureDisplayPreview(harness, initial, topology);
        QString token;
        qulonglong deadline = 0;
        QString previewGeneration;
        QCOMPARE(harness.service->PreviewDisplayConfiguration(
                     initial.revision,
                     initial.catalogDigest,
                     initial.actionCatalogDigest,
                     displayProfileBytes(topology),
                     15,
                     token,
                     deadline,
                     previewGeneration
                 ),
                 qulonglong(initial.revision + 1));
        QVERIFY(!token.isEmpty());
        QCOMPARE(harness.service->displayConfirmationState(),
                 QStringLiteral("awaiting-confirmation"));

        const SharedVisualProjection changedProjection{
            .borderEnabled = true,
            .borderWidth = 5,
            .borderRadius = 7,
            .syncWindowBorders = true,
            .innerSpacing = 10,
            .outerSpacing = 14,
            .syncWindowSpacing = true,
            .revision = 2,
        };
        const auto candidate = reconciledSharedVisualSnapshot(
            initial.desiredState,
            initial.revision,
            digest,
            changedProjection,
            true,
            true,
            error
        );
        QVERIFY2(candidate.has_value(), qPrintable(error));
        auto saved = initial;
        saved.revision++;
        saved.appliedRevision = initial.revision;
        saved.applyState = QStringLiteral("retained");
        saved.requiredActivation = ActivationRequirement::Reload;
        saved.desiredState = snapshotAtRevision(*candidate, saved.revision);
        harness.authority->replaceResult = {
            .success = true,
            .snapshot = saved,
        };
        harness.authority->prepareApplyResult = {
            .success = true,
            .snapshot = saved,
            .prepared = preparedGeneration(
                ActivationRequirement::Reload, saved.revision
            ),
        };
        auto current = saved;
        current.appliedRevision = current.revision;
        current.applyState = QStringLiteral("current");
        current.requiredActivation.reset();
        current.generationDigest = QString::fromLatin1(generationId);
        current.appliedDesiredState = current.desiredState;
        harness.authority->commitResult = {
            .success = true,
            .snapshot = current,
        };
        harness.backend->rollbackResult = {
            .success = true,
            .status = initialManagement,
        };

        harness.sharedBorderSource->setProjection(changedProjection);
        QTRY_COMPARE(harness.service->sharedBorderSyncState(),
                     QStringLiteral("pending"));
        QTRY_COMPARE(harness.service->sharedSpacingSyncState(),
                     QStringLiteral("pending"));
        QCOMPARE(harness.authority->replaceCalls, 0);
        QCOMPARE(harness.authority->prepareApplyCalls, 0);

        QCOMPARE(harness.service->RevertDisplayConfiguration(token),
                 qulonglong(initial.revision));
        harness.backend->activationResult = {
            .success = true,
            .activationMayHaveOccurred = true,
            .generation = QString::fromLatin1(generationId),
            .confirmedRequirement = ActivationRequirement::Reload,
            .receipt = {QByteArrayLiteral("shared-border-after-preview")},
            .status = initialManagement,
        };
        QTRY_COMPARE(harness.service->sharedBorderSyncState(),
                     QStringLiteral("current"));
        QTRY_COMPARE(harness.service->sharedSpacingSyncState(),
                     QStringLiteral("current"));
        QCOMPARE(harness.authority->replaceCalls, 1);
        QCOMPARE(harness.authority->prepareApplyCalls, 1);
        QCOMPARE(harness.backend->adoptCalls, 0);
    }

    void recoveryReassertsBothSharedVisualPoliciesAsOneNewRevision()
    {
        const auto catalog = protectedCatalog();
        const auto digest = sha256(catalog);
        const SharedVisualProjection projection{
            .borderEnabled = true,
            .borderWidth = 4,
            .borderRadius = 8,
            .syncWindowBorders = true,
            .innerSpacing = 8,
            .outerSpacing = 12,
            .syncWindowSpacing = true,
            .revision = 9,
        };
        QString error;
        const auto initialCandidate = reconciledSharedVisualSnapshot(
            sharedBorderSnapshot(5, digest),
            5,
            digest,
            projection,
            true,
            true,
            error
        );
        QVERIFY2(initialCandidate.has_value(), qPrintable(error));
        auto initial = committedSnapshot(5, *initialCandidate);
        initial.catalogDigest = digest;
        auto initialManagement = managedStatus();
        initialManagement.managedGeneration = QString::fromLatin1(generationId);
        ServiceHarness harness(initial, initialManagement);
        harness.authority->optionCatalogBytes = catalog;
        harness.sharedBorderSource->setProjection(projection);
        QTRY_COMPARE(harness.service->sharedBorderSyncState(),
                     QStringLiteral("current"));
        QTRY_COMPARE(harness.service->sharedSpacingSyncState(),
                     QStringLiteral("current"));

        auto recovered = initial;
        recovered.revision++;
        recovered.appliedRevision = recovered.revision;
        recovered.desiredState = sharedBorderSnapshot(
            recovered.revision, digest
        );
        recovered.appliedDesiredState = recovered.desiredState;
        harness.authority->prepareRecoveryResult = {
            .success = true,
            .snapshot = initial,
            .prepared = preparedGeneration(
                ActivationRequirement::Reload, recovered.revision
            ),
        };
        harness.authority->commitResult = {
            .success = true,
            .snapshot = recovered,
        };
        harness.backend->activationResult = {
            .success = true,
            .activationMayHaveOccurred = true,
            .generation = QString::fromLatin1(generationId),
            .confirmedRequirement = ActivationRequirement::Reload,
            .receipt = {QByteArrayLiteral("recovery")},
            .status = initialManagement,
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
                 qulonglong(recovered.revision));
        QCOMPARE(harness.authority->prepareRecoveryCalls, 1);

        const auto candidate = reconciledSharedVisualSnapshot(
            recovered.desiredState,
            recovered.revision,
            digest,
            projection,
            true,
            true,
            error
        );
        QVERIFY2(candidate.has_value(), qPrintable(error));
        auto saved = recovered;
        saved.revision++;
        saved.appliedRevision = recovered.revision;
        saved.applyState = QStringLiteral("retained");
        saved.requiredActivation = ActivationRequirement::Reload;
        saved.desiredState = snapshotAtRevision(*candidate, saved.revision);
        harness.authority->replaceResult = {
            .success = true,
            .snapshot = saved,
        };
        harness.authority->prepareApplyResult = {
            .success = true,
            .snapshot = saved,
            .prepared = preparedGeneration(
                ActivationRequirement::Reload, saved.revision
            ),
        };
        auto current = saved;
        current.appliedRevision = current.revision;
        current.applyState = QStringLiteral("current");
        current.requiredActivation.reset();
        current.generationDigest = QString::fromLatin1(generationId);
        current.appliedDesiredState = current.desiredState;
        harness.authority->commitResult = {
            .success = true,
            .snapshot = current,
        };
        harness.backend->activationResult.receipt = {
            QByteArrayLiteral("shared-border-after-recovery")
        };

        QTRY_COMPARE(harness.service->revision(),
                     qulonglong(current.revision));
        QCOMPARE(harness.authority->replaceCalls, 1);
        QCOMPARE(harness.authority->prepareApplyCalls, 1);
        QCOMPARE(harness.authority->commitCalls, 2);
        QCOMPARE(harness.backend->adoptCalls, 0);
    }

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

    void actionAuthorityReturnsTheExactRetainedPairWithoutMutation()
    {
        const auto actionCatalog = protectedActionCatalog();
        const auto schema = protectedConfigSchema();
        QVERIFY(!actionCatalog.isEmpty());
        QVERIFY(!schema.isEmpty());
        auto initial = dirtySnapshot();
        initial.actionCatalogDigest = QString::fromLatin1(
            reviewedActionCatalogDigest
        );
        ServiceHarness harness(initial);
        harness.authority->actionCatalogBytes = actionCatalog;
        harness.authority->configSchemaBytes = schema;

        QString digest = QStringLiteral("must-be-replaced");
        QByteArray replySchema = QByteArrayLiteral("must-be-replaced");
        QString schemaDigest = QStringLiteral("must-be-replaced");
        QCOMPARE(
            harness.service->GetActionCatalog(
                digest, replySchema, schemaDigest
            ),
            actionCatalog
        );
        QCOMPARE(digest, initial.actionCatalogDigest);
        QCOMPARE(replySchema, schema);
        QCOMPARE(schemaDigest, sha256(schema));
        QCOMPARE(harness.authority->actionCatalogCalls, 1);
        QCOMPARE(harness.authority->configSchemaCalls, 1);
        QCOMPARE(harness.authority->replaceCalls, 0);
        QCOMPARE(harness.authority->prepareApplyCalls, 0);
        QCOMPARE(harness.authority->prepareRecoveryCalls, 0);
        QCOMPARE(harness.authority->current, initial);
    }

    void actionAuthorityFailsClosedAndClearsEveryOutput()
    {
        const auto verifyFailure = [](
            const QByteArray &actionCatalog,
            const QByteArray &schema,
            const QString &snapshotDigest = QString::fromLatin1(
                reviewedActionCatalogDigest
            )
        ) {
            auto initial = dirtySnapshot();
            initial.actionCatalogDigest = snapshotDigest;
            ServiceHarness harness(initial);
            harness.authority->actionCatalogBytes = actionCatalog;
            harness.authority->configSchemaBytes = schema;
            QString digest = QStringLiteral("must-be-cleared");
            QByteArray replySchema = QByteArrayLiteral("must-be-cleared");
            QString schemaDigest = QStringLiteral("must-be-cleared");
            QVERIFY(harness.service->GetActionCatalog(
                digest, replySchema, schemaDigest
            ).isEmpty());
            QVERIFY(digest.isEmpty());
            QVERIFY(replySchema.isEmpty());
            QVERIFY(schemaDigest.isEmpty());
            QCOMPARE(harness.authority->actionCatalogCalls, 1);
            QCOMPARE(harness.authority->configSchemaCalls, 1);
            QCOMPARE(harness.authority->replaceCalls, 0);
        };

        const auto canonicalAction = protectedActionCatalog();
        const auto schema = protectedConfigSchema();
        verifyFailure({}, schema);
        verifyFailure(canonicalAction, {});
        auto tampered = canonicalAction;
        tampered.append('\n');
        verifyFailure(tampered, schema);
        verifyFailure(
            QByteArray(maximumActionCatalogBytes + 1, 'x'), schema
        );
        verifyFailure(
            canonicalAction,
            QByteArray(maximumActionSchemaBytes + 1, 'x')
        );
        verifyFailure(
            canonicalAction, schema, QString(64, QLatin1Char('f'))
        );
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
        const auto candidate = sharedBorderSnapshot(
            initial.revision,
            initial.catalogDigest,
            QJsonObject{{QStringLiteral("hyprland.animations.enabled"), false}}
        );

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

        harness.sharedBorderSource->setProjection({
            .borderEnabled = true,
            .borderWidth = 1,
            .borderRadius = 0,
            .syncWindowBorders = false,
            .syncWindowSpacing = false,
            .revision = 1,
        });
        QTRY_COMPARE(harness.service->sharedBorderSyncState(),
                     QStringLiteral("override"));
        auto next = initial;
        next.revision++;
        next.desiredState = snapshotAtRevision(candidate, next.revision);
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

    void restartRequiredApplyStopsBeforePreparationOrPublication()
    {
        auto initial = dirtySnapshot();
        initial.requiredActivation = ActivationRequirement::Restart;
        ServiceHarness harness(initial);
        setSharedBorderOverride(harness);
        QTRY_COMPARE(harness.service->sharedBorderSyncState(),
                     QStringLiteral("override"));
        harness.backend->supported = {ActivationRequirement::Reload};
        QSignalSpy published(
            harness.service.get(), &CompositorService::propertiesPublished
        );
        QVERIFY(published.isValid());

        QString generation = QStringLiteral("sentinel");
        QCOMPARE(harness.service->Apply(
                     initial.revision,
                     initial.catalogDigest,
                     initial.actionCatalogDigest,
                     generation
                 ),
                 qulonglong(initial.appliedRevision));
        QCOMPARE(generation, initial.generationDigest);
        QCOMPARE(harness.backend->capabilityChecks,
                 QVector{ActivationRequirement::Restart});
        QCOMPARE(harness.authority->prepareApplyCalls, 0);
        QCOMPARE(harness.backend->activateCalls, 0);
        QCOMPARE(harness.backend->adoptCalls, 0);
        QCOMPARE(harness.authority->commitCalls, 0);
        QCOMPARE(harness.backend->finalizeCalls, 0);
        QCOMPARE(harness.service->revision(), qulonglong(initial.revision));
        QCOMPARE(harness.service->appliedRevision(),
                 qulonglong(initial.appliedRevision));
        QCOMPARE(harness.service->generationDigest(), initial.generationDigest);
        QVERIFY(published.isEmpty());
    }

    void exactCurrentApplyFailsClosedOnManagedActivationSafety()
    {
        const auto catalog = protectedCatalog();
        const auto digest = sha256(catalog);
        auto initial = committedSnapshot(
            5, sharedBorderSnapshot(5, digest)
        );
        initial.catalogDigest = digest;
        auto management = managedStatus();
        management.managedGeneration = QString::fromLatin1(generationId);
        ServiceHarness harness(initial, management);
        harness.authority->optionCatalogBytes = catalog;
        setSharedBorderOverride(harness);
        QTRY_COMPARE(
            harness.service->sharedBorderSyncState(),
            QStringLiteral("override")
        );
        harness.authority->activationSafetyErrors = {{
            .path = QStringLiteral(
                "$.overrides.hyprland.decoration.glow.range"
            ),
            .code = QStringLiteral("state.unsafe-glow-range"),
            .message = QStringLiteral(
                "Inner glow can be enabled only when its range is at least "
                "10; disable glow or raise the range."
            ),
        }};

        QString generation;
        QCOMPARE(harness.service->Apply(
                     initial.revision,
                     initial.catalogDigest,
                     initial.actionCatalogDigest,
                     generation
                 ),
                 qulonglong(initial.appliedRevision));
        QCOMPARE(generation, initial.generationDigest);
        QCOMPARE(harness.authority->activationSafetyCalls, 1);
        QCOMPARE(harness.authority->prepareApplyCalls, 0);
        QCOMPARE(harness.backend->activateCalls, 0);
        QCOMPARE(harness.authority->commitCalls, 0);

        harness.authority->activationSafetyErrors.clear();
        QCOMPARE(harness.service->Apply(
                     initial.revision,
                     initial.catalogDigest,
                     initial.actionCatalogDigest,
                     generation
                 ),
                 qulonglong(initial.appliedRevision));
        QCOMPARE(harness.authority->activationSafetyCalls, 2);
        QCOMPARE(harness.authority->prepareApplyCalls, 0);
        QCOMPARE(harness.backend->activateCalls, 0);
        QCOMPARE(harness.authority->commitCalls, 0);
    }

    void restartRequiredAdoptionStopsBeforePreparationOrPublication()
    {
        auto initial = dirtySnapshot();
        initial.appliedRevision = 0;
        initial.appliedDesiredState.clear();
        initial.generationDigest.clear();
        initial.requiredActivation = ActivationRequirement::Restart;
        ServiceHarness harness(
            initial,
            {
                .state = ManagementState::Unmanaged,
                .entrypointKind = EntrypointKind::Absent,
            }
        );
        setSharedBorderOverride(harness);
        QTRY_COMPARE(harness.service->sharedBorderSyncState(),
                     QStringLiteral("override"));
        harness.backend->supported = {ActivationRequirement::Reload};
        QSignalSpy published(
            harness.service.get(), &CompositorService::propertiesPublished
        );
        QVERIFY(published.isValid());

        QString generation = QStringLiteral("sentinel-generation");
        QString entrypoint = QStringLiteral("sentinel-entrypoint");
        QCOMPARE(harness.service->AdoptManagedConfiguration(
                     initial.revision,
                     initial.catalogDigest,
                     initial.actionCatalogDigest,
                     QString{},
                     generation,
                     entrypoint
                 ),
                 qulonglong(0));
        QVERIFY(generation.isEmpty());
        QVERIFY(entrypoint.isEmpty());
        QCOMPARE(harness.backend->capabilityChecks,
                 QVector{ActivationRequirement::Restart});
        QCOMPARE(harness.authority->prepareApplyCalls, 0);
        QCOMPARE(harness.backend->adoptCalls, 0);
        QCOMPARE(harness.backend->activateCalls, 0);
        QCOMPARE(harness.authority->commitCalls, 0);
        QCOMPARE(harness.backend->finalizeCalls, 0);
        QCOMPARE(harness.service->revision(), qulonglong(initial.revision));
        QCOMPARE(harness.service->appliedRevision(), qulonglong(0));
        QVERIFY(harness.service->generationDigest().isEmpty());
        QCOMPARE(harness.service->managementState(), QStringLiteral("unmanaged"));
        QVERIFY(published.isEmpty());
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

    void previousTokenRetrySurvivesSharedVisualPolicyChangeExactly()
    {
        const auto catalog = protectedCatalog();
        const auto digest = sha256(catalog);
        const SharedVisualProjection synced{
            .borderEnabled = true,
            .borderWidth = 4,
            .borderRadius = 8,
            .syncWindowBorders = true,
            .innerSpacing = 8,
            .outerSpacing = 12,
            .syncWindowSpacing = true,
            .revision = 2,
        };
        QString error;
        const auto priorCandidate = reconciledSharedVisualSnapshot(
            sharedBorderSnapshot(5, digest),
            5,
            digest,
            synced,
            true,
            true,
            error
        );
        QVERIFY2(priorCandidate.has_value(), qPrintable(error));
        auto current = committedSnapshot(
            6, snapshotAtRevision(*priorCandidate, 6)
        );
        current.catalogDigest = digest;
        auto management = managedStatus();
        management.managedGeneration = QString::fromLatin1(generationId);
        ServiceHarness harness(current, management);
        harness.sharedBorderSource->setProjection({
            .borderEnabled = true,
            .borderWidth = 1,
            .borderRadius = 0,
            .syncWindowBorders = false,
            .syncWindowSpacing = false,
            .revision = 1,
        });
        QTRY_COMPARE(harness.service->sharedBorderSyncState(),
                     QStringLiteral("override"));
        harness.authority->replaceHook = [current, priorCandidate](
            const quint64 expectedRevision,
            const QByteArray &candidate
        ) {
            if (expectedRevision == 5 && candidate == *priorCandidate) {
                return AuthorityResult{
                    .success = true,
                    .snapshot = current,
                };
            }
            return AuthorityResult{
                .success = false,
                .errorCode = QStringLiteral("StaleRevision"),
                .errorMessage = QStringLiteral(
                    "The immediately preceding candidate was not committed"
                ),
                .snapshot = current,
            };
        };

        auto divergentPolicy = synced;
        divergentPolicy.borderWidth = 6;
        divergentPolicy.borderRadius = 10;
        divergentPolicy.innerSpacing = 9;
        divergentPolicy.outerSpacing = 14;
        divergentPolicy.revision++;
        harness.sharedBorderSource->setProjection(divergentPolicy);

        QCOMPARE(harness.service->ReplaceSnapshot(
                     5,
                     current.catalogDigest,
                     current.actionCatalogDigest,
                     *priorCandidate
                 ),
                 qulonglong(6));
        QCOMPARE(harness.authority->replaceCalls, 1);
        QCOMPARE(harness.authority->lastReplaceExpected, quint64(5));
        QCOMPARE(harness.authority->lastReplaceCandidate, *priorCandidate);
        QCOMPARE(harness.authority->replaceCalls, 1);

        auto nonexactObject = QJsonDocument::fromJson(
            *priorCandidate
        ).object();
        auto nonexactOverrides = nonexactObject.value(
            QStringLiteral("overrides")
        ).toObject();
        nonexactOverrides.insert(
            QStringLiteral("hyprland.animations.enabled"), false
        );
        nonexactObject.insert(QStringLiteral("overrides"), nonexactOverrides);
        auto nonexact = JsonSupport::canonicalJson(nonexactObject);
        nonexact.append('\n');
        QCOMPARE(harness.service->ReplaceSnapshot(
                     5,
                     current.catalogDigest,
                     current.actionCatalogDigest,
                     nonexact
                 ), qulonglong(6));
        QCOMPARE(harness.authority->replaceCalls, 2);
        QCOMPARE(harness.service->revision(), qulonglong(6));
        QCOMPARE(harness.backend->activateCalls, 0);

        QCOMPARE(harness.service->ReplaceSnapshot(
                     4,
                     current.catalogDigest,
                     current.actionCatalogDigest,
                     sharedBorderSnapshot(4, current.catalogDigest)
                 ),
                 qulonglong(6));
        QCOMPARE(harness.authority->replaceCalls, 2);
    }

    void activeDisplayConfirmationAllowsOnlyExactPreviousTokenRetry()
    {
        const auto catalog = protectedCatalog();
        const auto digest = sha256(catalog);
        const auto priorCandidate = sharedBorderSnapshot(
            4,
            digest,
            QJsonObject{{QStringLiteral("hyprland.animations.enabled"), false}}
        );
        auto current = committedSnapshot(
            5, snapshotAtRevision(priorCandidate, 5)
        );
        current.catalogDigest = digest;
        auto management = managedStatus();
        management.managedGeneration = QString::fromLatin1(generationId);
        ServiceHarness harness(current, management);
        setSharedBorderOverride(harness);
        QTRY_COMPARE(harness.service->sharedBorderSyncState(),
                     QStringLiteral("override"));

        const auto topology = connectedDisplayTopology();
        configureDisplayPreview(harness, current, topology);
        QString token;
        qulonglong deadline = 0;
        QString previewGeneration;
        QCOMPARE(harness.service->PreviewDisplayConfiguration(
                     current.revision,
                     current.catalogDigest,
                     current.actionCatalogDigest,
                     displayProfileBytes(topology),
                     15,
                     token,
                     deadline,
                     previewGeneration
                 ), qulonglong(current.revision + 1));
        QVERIFY(!harness.service->writable());
        QCOMPARE(harness.service->displayConfirmationState(),
                 QStringLiteral("awaiting-confirmation"));

        auto pendingSnapshot = current;
        pendingSnapshot.writable = false;
        harness.authority->replaceHook = [pendingSnapshot, priorCandidate](
            const quint64 expectedRevision,
            const QByteArray &candidate
        ) {
            if (expectedRevision == 4 && candidate == priorCandidate) {
                return AuthorityResult{
                    .success = true,
                    .snapshot = pendingSnapshot,
                };
            }
            return AuthorityResult{
                .success = false,
                .errorCode = QStringLiteral("StaleRevision"),
                .errorMessage = QStringLiteral(
                    "The immediately preceding candidate was not committed"
                ),
                .snapshot = pendingSnapshot,
            };
        };
        QCOMPARE(harness.service->ReplaceSnapshot(
                     4,
                     current.catalogDigest,
                     current.actionCatalogDigest,
                     priorCandidate
                 ), qulonglong(current.revision));
        QCOMPARE(harness.authority->replaceCalls, 1);
        QVERIFY(!harness.service->writable());
        QCOMPARE(harness.service->displayConfirmationState(),
                 QStringLiteral("awaiting-confirmation"));

        auto nonexactObject = QJsonDocument::fromJson(priorCandidate).object();
        auto nonexactOverrides = nonexactObject.value(
            QStringLiteral("overrides")
        ).toObject();
        nonexactOverrides.insert(
            QStringLiteral("hyprland.animations.enabled"), true
        );
        nonexactObject.insert(QStringLiteral("overrides"), nonexactOverrides);
        auto nonexact = JsonSupport::canonicalJson(nonexactObject);
        nonexact.append('\n');
        QCOMPARE(harness.service->ReplaceSnapshot(
                     4,
                     current.catalogDigest,
                     current.actionCatalogDigest,
                     nonexact
                 ), qulonglong(current.revision));
        QCOMPARE(harness.authority->replaceCalls, 2);
        QCOMPARE(harness.service->displayConfirmationState(),
                 QStringLiteral("awaiting-confirmation"));

        QCOMPARE(harness.service->ReplaceSnapshot(
                     current.revision,
                     current.catalogDigest,
                     current.actionCatalogDigest,
                     current.desiredState
                 ), qulonglong(current.revision));
        QCOMPARE(harness.authority->replaceCalls, 2);
        QCOMPARE(harness.service->displayConfirmationState(),
                 QStringLiteral("awaiting-confirmation"));
    }

    void uncertainReplacePublicationMakesServiceUnavailable()
    {
        const auto initial = dirtySnapshot();
        ServiceHarness harness(initial);
        harness.sharedBorderSource->setProjection({
            .borderEnabled = true,
            .borderWidth = 1,
            .borderRadius = 0,
            .syncWindowBorders = false,
            .syncWindowSpacing = false,
            .revision = 1,
        });
        QTRY_COMPARE(harness.service->sharedBorderSyncState(),
                     QStringLiteral("override"));
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
                     sharedBorderSnapshot(
                         initial.revision,
                         initial.catalogDigest,
                         QJsonObject{{
                             QStringLiteral("hyprland.animations.enabled"),
                             false
                         }}
                     )
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
            setSharedBorderOverride(harness);
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
        setSharedBorderOverride(harness);

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
            setSharedBorderOverride(harness);
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
            setSharedBorderOverride(harness);
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

    void adoptionStopsWhenManagedActivationSafetyRejectsStaging()
    {
        auto initial = dirtySnapshot();
        initial.appliedRevision = 0;
        initial.appliedDesiredState.clear();
        initial.generationDigest.clear();
        ServiceHarness harness(
            initial,
            {
                .state = ManagementState::Unmanaged,
                .entrypointKind = EntrypointKind::Absent,
            }
        );
        setSharedBorderOverride(harness);
        QTRY_COMPARE(
            harness.service->sharedBorderSyncState(),
            QStringLiteral("override")
        );
        harness.authority->prepareApplyResult = {
            .success = false,
            .errorCode = QStringLiteral("VerificationFailed"),
            .errorMessage = QStringLiteral(
                "state.unsafe-glow-range: Inner glow can be enabled only "
                "when its range is at least 10; disable glow or raise the "
                "range."
            ),
            .snapshot = initial,
        };

        QString generation;
        QString entrypoint;
        QCOMPARE(harness.service->AdoptManagedConfiguration(
                     initial.revision,
                     initial.catalogDigest,
                     initial.actionCatalogDigest,
                     QString{},
                     generation,
                     entrypoint
                 ),
                 qulonglong(initial.appliedRevision));
        QCOMPARE(harness.authority->prepareApplyCalls, 1);
        QCOMPARE(harness.backend->adoptCalls, 0);
        QCOMPARE(harness.backend->activateCalls, 0);
        QCOMPARE(harness.authority->commitCalls, 0);
        QCOMPARE(harness.authority->abortCalls, 0);
        QVERIFY(generation.isEmpty());
        QVERIFY(entrypoint.isEmpty());
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
        setSharedBorderOverride(harness);
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
        setSharedBorderOverride(harness);
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
        setSharedBorderOverride(harness);
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
        setSharedBorderOverride(harness);
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
            setSharedBorderOverride(harness);
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
        setSharedBorderOverride(harness);
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
        setSharedBorderOverride(harness);
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

    void inputDeviceDiscoveryIsIndependentAndReceiptBound()
    {
        const auto initial = dirtySnapshot();
        ServiceHarness harness(initial);
        const auto inventory = connectedInputDeviceInventory();
        harness.backend->connectedInputResult = {
            .success = true,
            .runtimeIdentity = QStringLiteral("authenticated-runtime"),
            .inventory = inventory,
        };

        qulonglong observedAtMs = 0;
        QCOMPARE(harness.service->GetConnectedInputDevices(observedAtMs),
                 inventory.document);
        QVERIFY(observedAtMs > 0);
        QCOMPARE(harness.backend->connectedInputCalls, 1);
        QCOMPARE(harness.backend->connectedInputEpochs,
                 QVector<QByteArray>{QByteArray(inputDeviceInventoryEpoch)});

        harness.backend->connectedInputResult = {
            .success = false,
            .errorCode = QStringLiteral("RuntimeUnavailable"),
            .errorMessage = QStringLiteral("device query failed"),
        };
        observedAtMs = 99;
        QVERIFY(harness.service->GetConnectedInputDevices(observedAtMs).isEmpty());
        QCOMPARE(observedAtMs, qulonglong(0));
        QCOMPARE(harness.backend->connectedInputCalls, 2);

    }

    void inputDeviceDiscoveryDoesNotRequireDesiredStateAuthority()
    {
        auto ownedBackend = std::make_unique<FakeActivationBackend>();
        auto *backend = ownedBackend.get();
        const auto inventory = connectedInputDeviceInventory();
        backend->connectedInputResult = {
            .success = true,
            .runtimeIdentity = QStringLiteral("authenticated-runtime"),
            .inventory = inventory,
        };
        CompositorService service(
            std::move(ownedBackend),
            QDBusConnection(QStringLiteral("input-devices-without-authority")),
            nullptr,
            {},
            {},
            {},
            QByteArray(inputDeviceInventoryEpoch)
        );

        qulonglong observedAtMs = 0;
        QCOMPARE(service.GetConnectedInputDevices(observedAtMs),
                 inventory.document);
        QVERIFY(observedAtMs > 0);
        QCOMPARE(backend->connectedInputCalls, 1);
        QCOMPARE(backend->connectedInputEpochs,
                 QVector<QByteArray>{QByteArray(inputDeviceInventoryEpoch)});
        QVERIFY(!service.available());
    }

    void inputDeviceDiscoveryIsBlockedDuringDisplayReconciliation()
    {
        auto initial = committedSnapshot();
        initial.generationDigest = QStringLiteral("old-generation");
        ServiceHarness harness(initial);
        const auto inventory = connectedInputDeviceInventory();
        harness.backend->connectedInputResult = {
            .success = true,
            .runtimeIdentity = QStringLiteral("authenticated-runtime"),
            .inventory = inventory,
        };
        const auto topology = connectedDisplayTopology();
        configureDisplayPreview(harness, initial, topology);

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
                     previewGeneration
                 ),
                 qulonglong(6));

        qulonglong observedAtMs = 99;
        QVERIFY(harness.service->GetConnectedInputDevices(observedAtMs).isEmpty());
        QCOMPARE(observedAtMs, qulonglong(0));
        QCOMPARE(harness.backend->connectedInputCalls, 0);

        auto committed = committedSnapshot(
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
        bool blockedWhileCommitting = false;
        harness.backend->finalizeHook = [&] {
            qulonglong committingObservedAt = 99;
            blockedWhileCommitting =
                harness.service->GetConnectedInputDevices(committingObservedAt)
                    .isEmpty()
                && committingObservedAt == 0
                && harness.backend->connectedInputCalls == 0;
        };

        QString confirmedGeneration;
        QCOMPARE(harness.service->ConfirmDisplayConfiguration(
                     token, confirmedGeneration
                 ),
                 qulonglong(6));
        QVERIFY(blockedWhileCommitting);

        observedAtMs = 0;
        QCOMPARE(harness.service->GetConnectedInputDevices(observedAtMs),
                 inventory.document);
        QVERIFY(observedAtMs > 0);
        QCOMPARE(harness.backend->connectedInputCalls, 1);
    }

    void inputDeviceDiscoveryIsBlockedWhileDisplayRollbackIsRunning()
    {
        auto initial = committedSnapshot();
        initial.generationDigest = QStringLiteral("old-generation");
        ServiceHarness harness(initial);
        const auto inventory = connectedInputDeviceInventory();
        harness.backend->connectedInputResult = {
            .success = true,
            .runtimeIdentity = QStringLiteral("authenticated-runtime"),
            .inventory = inventory,
        };
        const auto topology = connectedDisplayTopology();
        configureDisplayPreview(harness, initial, topology);

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
                     previewGeneration
                 ),
                 qulonglong(6));

        bool blockedWhileReverting = false;
        harness.backend->rollbackHook = [&] {
            qulonglong observedAtMs = 99;
            blockedWhileReverting =
                harness.service->GetConnectedInputDevices(observedAtMs)
                    .isEmpty()
                && observedAtMs == 0
                && harness.backend->connectedInputCalls == 0;
        };

        QCOMPARE(harness.service->RevertDisplayConfiguration(token),
                 qulonglong(initial.revision));
        QVERIFY(blockedWhileReverting);
        QCOMPARE(harness.service->displayConfirmationState(),
                 QStringLiteral("idle"));

        qulonglong observedAtMs = 0;
        QCOMPARE(harness.service->GetConnectedInputDevices(observedAtMs),
                 inventory.document);
        QVERIFY(observedAtMs > 0);
        QCOMPARE(harness.backend->connectedInputCalls, 1);
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
        const auto inventory = connectedInputDeviceInventory();
        harness.backend->connectedInputResult = {
            .success = true,
            .runtimeIdentity = QStringLiteral("authenticated-runtime"),
            .inventory = inventory,
        };
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

        // Terminal display failure has already revoked the preview capability.
        // Device diagnostics remain independent even though desired-state
        // authority is now unavailable.
        qulonglong observedAtMs = 0;
        QCOMPARE(harness.service->GetConnectedInputDevices(observedAtMs),
                 inventory.document);
        QVERIFY(observedAtMs > 0);
        QCOMPARE(harness.backend->connectedInputCalls, 1);

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
