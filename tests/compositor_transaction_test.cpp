#include "compositord/transaction.h"

#include "hyprland/json_support.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QTemporaryDir>
#include <QtTest>

#include <cerrno>
#include <limits>

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

using namespace HyprShelld::Compositor;
using namespace HyprShelld::Hyprland;

namespace {

constexpr auto nonceA = "0123456789abcdef0123456789abcdef";
constexpr auto nonceB = "11111111111111111111111111111111";
constexpr auto nonceC = "22222222222222222222222222222222";
constexpr auto nonceD = "33333333333333333333333333333333";

[[nodiscard]] QByteArray readBytes(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }
    return file.readAll();
}

[[nodiscard]] QJsonObject objectFromBytes(const QByteArrayView bytes)
{
    QJsonParseError error;
    const auto document = QJsonDocument::fromJson(bytes.toByteArray(), &error);
    if (error.error != QJsonParseError::NoError || !document.isObject()) {
        return {};
    }
    return document.object();
}

[[nodiscard]] QByteArray canonicalObject(const QJsonObject &object)
{
    auto bytes = JsonSupport::canonicalJson(object);
    bytes.append('\n');
    return bytes;
}

[[nodiscard]] QString sha256(const QByteArrayView bytes)
{
    return QString::fromLatin1(
        QCryptographicHash::hash(bytes, QCryptographicHash::Sha256).toHex()
    );
}

[[nodiscard]] QString describeErrors(const ValidationErrors &errors)
{
    QStringList descriptions;
    for (const auto &error : errors) {
        descriptions.append(error.path + QLatin1Char(':') + error.code);
    }
    return descriptions.join(QStringLiteral(", "));
}

[[nodiscard]] bool makeDirectory(const QString &path, const mode_t mode = 0700)
{
    return QDir().mkpath(path)
        && ::chmod(QFile::encodeName(path).constData(), mode) == 0;
}

[[nodiscard]] bool writeNew(
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

[[nodiscard]] bool replaceBytes(
    const QString &path,
    const QByteArrayView bytes,
    const mode_t mode = 0600
)
{
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)
        || file.write(bytes.data(), bytes.size()) != bytes.size()
        || !file.flush()) {
        return false;
    }
    file.close();
    return ::chmod(QFile::encodeName(path).constData(), mode) == 0;
}

[[nodiscard]] struct stat metadata(const QString &path)
{
    struct stat information {};
    ::lstat(QFile::encodeName(path).constData(), &information);
    return information;
}

[[nodiscard]] bool sameIdentity(
    const struct stat &left,
    const struct stat &right
)
{
    return left.st_dev == right.st_dev && left.st_ino == right.st_ino;
}

void makeTreeRemovable(const QString &path)
{
    const QFileInfo information(path);
    if (!information.exists() || information.isSymLink()
        || !information.isDir()) {
        return;
    }
    ::chmod(QFile::encodeName(path).constData(), 0700);
    const QDir directory(path);
    for (const auto &entry : directory.entryInfoList(
             QDir::NoDotAndDotDot | QDir::AllEntries | QDir::NoSymLinks
         )) {
        if (entry.isDir()) {
            makeTreeRemovable(entry.absoluteFilePath());
        }
    }
}

struct StoreFixture final {
    QTemporaryDir temporary;
    StorePaths paths;

    StoreFixture()
    {
        if (!temporary.isValid()) {
            return;
        }
        ::chmod(QFile::encodeName(temporary.path()).constData(), 0700);
        paths = {
            .stateRoot = QDir(temporary.path()).filePath(
                QStringLiteral("state/hyprshelld/compositor")
            ),
            .configRoot = QDir(temporary.path()).filePath(
                QStringLiteral("config/hypr")
            ),
        };
        paths.managedConfigRoot = QDir(paths.configRoot).filePath(
            QStringLiteral("hyprshelld")
        );
    }

    ~StoreFixture()
    {
        if (temporary.isValid()) {
            makeTreeRemovable(temporary.path());
        }
    }
};

[[nodiscard]] QDateTime fixedTime(const int offset = 0)
{
    return QDateTime::fromString(
        QStringLiteral("2026-08-09T12:34:56.789Z"),
        Qt::ISODateWithMs
    ).addSecs(offset);
}

[[nodiscard]] ConnectedDisplayTopology oneDisplayTopology()
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

[[nodiscard]] DisplayProfile oneDisplayProfile(
    const ConnectedDisplayTopology &topology
)
{
    return {
        .topologyDigest = topology.topologyDigest,
        .outputs = QJsonArray{QJsonObject{
            {QStringLiteral("selector"), QStringLiteral("DP-1")},
            {QStringLiteral("enabled"), true},
            {QStringLiteral("mode"), QStringLiteral("2560x1440@144")},
            {QStringLiteral("position"), QStringLiteral("0x0")},
            {QStringLiteral("scale"), 1.25},
            {QStringLiteral("reserved"), QJsonArray{0, 0, 0, 0}},
            {QStringLiteral("transform"), 0},
            {QStringLiteral("mirror"), QString()},
            {QStringLiteral("bitdepth"), 8},
            {QStringLiteral("cm"), QStringLiteral("srgb")},
            {QStringLiteral("sdrEotf"), QStringLiteral("default")},
            {QStringLiteral("sdrBrightness"), 1.0},
            {QStringLiteral("sdrSaturation"), 1.0},
            {QStringLiteral("vrr"), -1},
            {QStringLiteral("icc"), QString()},
            {QStringLiteral("supportsWideColor"), 0},
            {QStringLiteral("supportsHdr"), 0},
            {QStringLiteral("sdrMinLuminance"), 0.2},
            {QStringLiteral("sdrMaxLuminance"), 80},
            {QStringLiteral("minLuminance"), -1.0},
            {QStringLiteral("maxLuminance"), -1},
            {QStringLiteral("maxAvgLuminance"), -1},
        }},
    };
}

} // namespace

class CompositorTransactionTest final : public QObject
{
    Q_OBJECT

private:
    Catalog catalog;
    ActionCatalog actionCatalog;
    DesiredState defaults;

    [[nodiscard]] std::unique_ptr<ConfigurationTransaction> transaction(
        const StorePaths &paths
    ) const
    {
        return std::make_unique<ConfigurationTransaction>(
            paths, catalog, actionCatalog
        );
    }

    [[nodiscard]] DesiredState permissionState(const quint64 revision) const
    {
        auto state = defaults;
        state.revision = revision;
        state.permissions.append({
            .id = QStringLiteral("portal-copy"),
            .binary = QStringLiteral("xdg-desktop-portal"),
            .type = QStringLiteral("screencopy"),
            .mode = QStringLiteral("ask"),
        });
        return state;
    }

    [[nodiscard]] DesiredState environmentState(
        const quint64 revision,
        const EnvironmentScope scope = EnvironmentScope::Hyprland
    ) const
    {
        auto state = defaults;
        state.revision = revision;
        state.environment.append({
            .id = QStringLiteral("desktop"),
            .name = QStringLiteral("XDG_CURRENT_DESKTOP"),
            .value = QStringLiteral("Hyprland"),
            .scope = scope,
        });
        return state;
    }

    [[nodiscard]] DesiredState brokerState(
        const quint64 revision,
        const bool enabled
    ) const
    {
        auto state = defaults;
        state.revision = revision;
        state.bindings.append({
            .id = QStringLiteral("launch-terminal"),
            .modifiers = {QStringLiteral("super")},
            .key = QStringLiteral("F1"),
            .actionType = BindingActionType::DefaultApp,
            .action = QStringLiteral("defaultApp.terminal"),
            .description = QStringLiteral("Open a terminal"),
            .enabled = enabled,
        });
        return state;
    }

    [[nodiscard]] DesiredState reloadState(const quint64 revision) const
    {
        auto state = defaults;
        state.revision = revision;
        state.overrides.insert(
            QStringLiteral("hyprland.general.border_size"), 2
        );
        return state;
    }

    [[nodiscard]] bool initializeAppliedDefault(
        ConfigurationTransaction &authority
    ) const
    {
        const auto initialized = authority.initialize();
        if (!initialized.success) {
            return false;
        }
        const auto prepared = authority.prepareApply(
            0, QString::fromLatin1(nonceA), fixedTime()
        );
        return prepared.success
            && prepared.prepared.has_value()
            && authority.commitApply(prepared.prepared->id).success;
    }

private slots:
    void initTestCase()
    {
        const auto parsedCatalog = parseCatalog(
            readBytes(QStringLiteral(HYPRSHELLD_HYPRLAND_CATALOG_FILE))
        );
        QVERIFY2(parsedCatalog, qPrintable(describeErrors(parsedCatalog.errors)));
        catalog = *parsedCatalog.value;

        const auto parsedActions = parseActionCatalog(
            readBytes(QStringLiteral(HYPRSHELLD_HYPRLAND_ACTION_CATALOG_FILE)),
            readBytes(QStringLiteral(HYPRSHELLD_HYPRLAND_CONFIG_SCHEMA_FILE))
        );
        QVERIFY2(parsedActions, qPrintable(describeErrors(parsedActions.errors)));
        actionCatalog = *parsedActions.value;

        const auto parsedDefaults = parseDesiredState(
            readBytes(QStringLiteral(HYPRSHELLD_HYPRLAND_DEFAULTS_FILE)),
            catalog,
            actionCatalog
        );
        QVERIFY2(parsedDefaults, qPrintable(describeErrors(parsedDefaults.errors)));
        defaults = *parsedDefaults.value;
        QVERIFY(fixedTime().isValid());
    }

    void constructorIsSideEffectFreeAndDefaultIsNotApplied()
    {
        StoreFixture fixture;
        QVERIFY(fixture.temporary.isValid());
        auto authority = transaction(fixture.paths);
        QCOMPARE(authority->snapshot(), AuthoritySnapshot{});
        QVERIFY(!QFileInfo::exists(fixture.paths.stateRoot));
        QVERIFY(!QFileInfo::exists(fixture.paths.configRoot));

        const auto initialized = authority->initialize();
        QVERIFY2(initialized.success, qPrintable(initialized.errorMessage));
        const auto snapshot = initialized.snapshot;
        QVERIFY(snapshot.available);
        QVERIFY(snapshot.writable);
        QCOMPARE(snapshot.desiredState, serializeDesiredState(defaults));
        QCOMPARE(snapshot.revision, quint64(0));
        QCOMPARE(snapshot.catalogDigest, defaults.catalogDigest);
        QCOMPARE(snapshot.actionCatalogDigest, defaults.actionCatalogDigest);
        QCOMPARE(snapshot.loadState, QStringLiteral("defaulted"));
        QCOMPARE(snapshot.appliedRevision, quint64(0));
        QCOMPARE(snapshot.applyState, QStringLiteral("inactive"));
        QVERIFY(snapshot.requiredActivation.has_value());
        QCOMPARE(*snapshot.requiredActivation, ActivationRequirement::Reload);
        QVERIFY(snapshot.generationDigest.isEmpty());
        QCOMPARE(readBytes(fixture.paths.desiredPath()),
                 serializeDesiredState(defaults));
        QVERIFY(!QFileInfo::exists(fixture.paths.lastGoodPath()));
        QVERIFY(!QFileInfo::exists(fixture.paths.activationPath()));
        QVERIFY(!QFileInfo::exists(fixture.paths.pendingPath()));
        QVERIFY(!QFileInfo::exists(fixture.paths.stableEntrypointPath()));
    }

    void optionCatalogIsTheRetainedCanonicalAuthority()
    {
        StoreFixture fixture;
        QVERIFY(fixture.temporary.isValid());
        auto authority = transaction(fixture.paths);

        const auto expected = canonicalCatalogJson(catalog);
        QVERIFY(!expected.isEmpty());
        QCOMPARE(authority->optionCatalog(), expected);
        QVERIFY(!QFileInfo::exists(fixture.paths.stateRoot));

        const auto initialized = authority->initialize();
        QVERIFY2(initialized.success, qPrintable(initialized.errorMessage));
        QCOMPARE(authority->optionCatalog(), expected);
        QCOMPARE(sha256(expected), initialized.snapshot.catalogDigest);
    }

    void activationFilesystemContextDuplicatesExactAuthorityRoots()
    {
        StoreFixture fixture;
        QVERIFY(fixture.temporary.isValid());
        auto authority = transaction(fixture.paths);
        QVERIFY(authority->initialize().success);

        auto duplicated = authority->duplicateActivationFilesystemContext();
        QVERIFY2(duplicated.success, qPrintable(duplicated.errorMessage));
        QVERIFY(duplicated.context.has_value());
        auto context = std::move(*duplicated.context);
        QVERIFY(context.complete());
        QCOMPARE(context.stateRoot, fixture.paths.stateRoot);
        QCOMPARE(context.configRoot, fixture.paths.configRoot);
        QCOMPARE(context.managedConfigRoot, fixture.paths.managedConfigRoot);
        QCOMPARE(context.stableEntrypoint,
                 fixture.paths.stableEntrypointPath());

        const auto requireExactDescriptor = [](const int descriptor,
                                               const QString &path) {
            struct stat opened {};
            struct stat named {};
            return ::fstat(descriptor, &opened) == 0
                && ::lstat(QFile::encodeName(path).constData(), &named) == 0
                && S_ISDIR(opened.st_mode) && S_ISDIR(named.st_mode)
                && sameIdentity(opened, named)
                && (::fcntl(descriptor, F_GETFD) & FD_CLOEXEC) != 0;
        };
        QVERIFY(requireExactDescriptor(
            context.stateDirectoryFd, fixture.paths.stateRoot
        ));
        QVERIFY(requireExactDescriptor(
            context.configDirectoryFd, fixture.paths.configRoot
        ));
        QVERIFY(requireExactDescriptor(
            context.managedDirectoryFd, fixture.paths.managedConfigRoot
        ));
        QVERIFY(requireExactDescriptor(
            context.generationsDirectoryFd, fixture.paths.generationsPath()
        ));

        const auto descriptor = context.stateDirectoryFd;
        context.reset();
        errno = 0;
        QCOMPARE(::fcntl(descriptor, F_GETFD), -1);
        QCOMPARE(errno, EBADF);
    }

    void activationFilesystemContextRejectsCanonicalRootReplacement()
    {
        StoreFixture fixture;
        QVERIFY(fixture.temporary.isValid());
        auto authority = transaction(fixture.paths);
        QVERIFY(authority->initialize().success);
        QVERIFY(authority->duplicateActivationFilesystemContext().success);

        const auto detached = fixture.paths.configRoot
            + QStringLiteral(".detached");
        QVERIFY(::rename(
            QFile::encodeName(fixture.paths.configRoot).constData(),
            QFile::encodeName(detached).constData()
        ) == 0);
        QVERIFY(makeDirectory(fixture.paths.managedConfigRoot));
        QVERIFY(makeDirectory(fixture.paths.generationsPath()));

        const auto rejected =
            authority->duplicateActivationFilesystemContext();
        QVERIFY(!rejected.success);
        QVERIFY(!rejected.context.has_value());
        QCOMPARE(rejected.errorCode, QStringLiteral("PersistenceFailed"));
    }

    void replaceIsMonotonicCasWithLostResponseIdempotency()
    {
        StoreFixture fixture;
        QVERIFY(fixture.temporary.isValid());
        auto authority = transaction(fixture.paths);
        QVERIFY(authority->initialize().success);
        const auto candidate = serializeDesiredState(permissionState(0));

        const auto replaced = authority->replaceSnapshot(0, candidate);
        QVERIFY2(replaced.success, qPrintable(replaced.errorMessage));
        QCOMPARE(replaced.snapshot.revision, quint64(1));
        QCOMPARE(replaced.snapshot.applyState, QStringLiteral("inactive"));
        QCOMPARE(*replaced.snapshot.requiredActivation,
                 ActivationRequirement::Restart);
        const auto committedBytes = readBytes(fixture.paths.desiredPath());
        const auto committedIdentity = metadata(fixture.paths.desiredPath());

        const auto retry = authority->replaceSnapshot(0, candidate);
        QVERIFY2(retry.success, qPrintable(retry.errorMessage));
        QCOMPARE(retry.snapshot.revision, quint64(1));
        QCOMPARE(readBytes(fixture.paths.desiredPath()), committedBytes);
        QVERIFY(sameIdentity(
            committedIdentity, metadata(fixture.paths.desiredPath())
        ));

        const auto staleDifferent = authority->replaceSnapshot(
            0, serializeDesiredState(environmentState(0))
        );
        QVERIFY(!staleDifferent.success);
        QCOMPARE(staleDifferent.errorCode, QStringLiteral("StaleRevision"));
        QCOMPARE(authority->snapshot().revision, quint64(1));

        auto wrongEmbedded = environmentState(2);
        const auto invalid = authority->replaceSnapshot(
            1, serializeDesiredState(wrongEmbedded)
        );
        QVERIFY(!invalid.success);
        QCOMPARE(invalid.errorCode, QStringLiteral("InvalidSnapshot"));

        auto next = environmentState(1);
        const auto advanced = authority->replaceSnapshot(
            1, serializeDesiredState(next)
        );
        QVERIFY2(advanced.success, qPrintable(advanced.errorMessage));
        QCOMPARE(advanced.snapshot.revision, quint64(2));
        QCOMPARE(*advanced.snapshot.requiredActivation,
                 ActivationRequirement::Session);

        const auto farStaleMalformed = authority->replaceSnapshot(
            0, QByteArrayLiteral("not-json")
        );
        QVERIFY(!farStaleMalformed.success);
        QCOMPARE(farStaleMalformed.errorCode, QStringLiteral("StaleRevision"));
        QCOMPARE(authority->snapshot().revision, quint64(2));
    }

    void revisionExhaustionNeverWrites()
    {
        StoreFixture fixture;
        QVERIFY(fixture.temporary.isValid());
        QVERIFY(makeDirectory(fixture.paths.stateRoot));
        auto exhausted = defaults;
        exhausted.revision = std::numeric_limits<quint64>::max();
        const auto persisted = serializeDesiredState(exhausted);
        QVERIFY(writeNew(fixture.paths.desiredPath(), persisted));
        auto authority = transaction(fixture.paths);
        const auto initialized = authority->initialize();
        QVERIFY2(initialized.success, qPrintable(initialized.errorMessage));

        auto candidate = permissionState(exhausted.revision);
        const auto rejected = authority->replaceSnapshot(
            exhausted.revision, serializeDesiredState(candidate)
        );
        QVERIFY(!rejected.success);
        QCOMPARE(rejected.errorCode, QStringLiteral("RevisionExhausted"));
        QCOMPARE(readBytes(fixture.paths.desiredPath()), persisted);
    }

    void uncertainDesiredPublicationFailsClosedAndRestartsAtVisibleRevision()
    {
        StoreFixture fixture;
        QVERIFY(fixture.temporary.isValid());
        int desiredPublications = 0;
        fixture.paths.faultHook = [&desiredPublications](
            const StoreFaultPoint point,
            const StoreFile file
        ) {
            return file == StoreFile::Desired
                && point
                    == StoreFaultPoint::AfterPublishRenameBeforeDirectorySync
                && ++desiredPublications == 2;
        };
        QString appliedGeneration;
        {
            auto authority = transaction(fixture.paths);
            QVERIFY(authority->initialize().success);
            const auto prepared = authority->prepareApply(
                0, QString::fromLatin1(nonceA), fixedTime()
            );
            QVERIFY(prepared.success);
            appliedGeneration = prepared.prepared->id;
            const auto applied = authority->commitApply(appliedGeneration);
            QVERIFY(applied.success);
            QCOMPARE(applied.snapshot.applyState, QStringLiteral("current"));
            const auto replaced = authority->replaceSnapshot(
                0, serializeDesiredState(permissionState(0))
            );
            QVERIFY(!replaced.success);
            QCOMPARE(replaced.errorCode, QStringLiteral("PersistenceFailed"));
            QVERIFY(!replaced.snapshot.available);
            QCOMPARE(replaced.snapshot.loadState,
                     QStringLiteral("unavailable"));
            QCOMPARE(replaced.snapshot.appliedRevision, quint64(0));
            QCOMPARE(replaced.snapshot.generationDigest, appliedGeneration);
            QVERIFY(!authority->snapshot().available);
            QCOMPARE(authority->snapshot().generationDigest,
                     appliedGeneration);
            const auto visible = parseDesiredState(
                readBytes(fixture.paths.desiredPath()), catalog, actionCatalog
            );
            QVERIFY(visible);
            QCOMPARE(visible.value->revision, quint64(1));
        }

        fixture.paths.faultHook = {};
        auto restarted = transaction(fixture.paths);
        const auto initialized = restarted->initialize();
        QVERIFY2(initialized.success, qPrintable(initialized.errorMessage));
        QCOMPARE(initialized.snapshot.revision, quint64(1));
        QCOMPARE(initialized.snapshot.appliedRevision, quint64(0));
        QCOMPARE(initialized.snapshot.generationDigest, appliedGeneration);
        QCOMPARE(initialized.snapshot.applyState, QStringLiteral("retained"));
    }

    void uncertainPreparedJournalPublicationFailsClosedAndIsDiscarded()
    {
        StoreFixture fixture;
        QVERIFY(fixture.temporary.isValid());
        fixture.paths.faultHook = [](
            const StoreFaultPoint point,
            const StoreFile file
        ) {
            return file == StoreFile::Pending
                && point
                    == StoreFaultPoint::AfterPublishRenameBeforeDirectorySync;
        };
        {
            auto authority = transaction(fixture.paths);
            QVERIFY(authority->initialize().success);
            const auto prepared = authority->prepareApply(
                0, QString::fromLatin1(nonceA), fixedTime()
            );
            QVERIFY(!prepared.success);
            QCOMPARE(prepared.errorCode, QStringLiteral("PersistenceFailed"));
            QVERIFY(!prepared.snapshot.available);
            QCOMPARE(prepared.snapshot.loadState,
                     QStringLiteral("unavailable"));
            const auto pending = objectFromBytes(
                readBytes(fixture.paths.pendingPath())
            );
            QCOMPARE(pending.value(QStringLiteral("phase")).toString(),
                     QStringLiteral("prepared"));
        }

        fixture.paths.faultHook = {};
        auto restarted = transaction(fixture.paths);
        const auto initialized = restarted->initialize();
        QVERIFY2(initialized.success, qPrintable(initialized.errorMessage));
        QCOMPARE(initialized.snapshot.revision, quint64(0));
        QCOMPARE(initialized.snapshot.applyState, QStringLiteral("inactive"));
        QVERIFY(!QFileInfo::exists(fixture.paths.pendingPath()));
    }

    void prepareAndAbortOnlyTouchPendingAndImmutableGeneration()
    {
        StoreFixture fixture;
        QVERIFY(fixture.temporary.isValid());
        auto authority = transaction(fixture.paths);
        QVERIFY(authority->initialize().success);
        const auto desiredBefore = readBytes(fixture.paths.desiredPath());
        const auto desiredIdentity = metadata(fixture.paths.desiredPath());

        const auto prepared = authority->prepareApply(
            0, QString::fromLatin1(nonceA), fixedTime()
        );
        QVERIFY2(prepared.success, qPrintable(prepared.errorMessage));
        QVERIFY(prepared.prepared.has_value());
        QCOMPARE(prepared.prepared->revision, quint64(0));
        QCOMPARE(prepared.prepared->requirement, ActivationRequirement::Reload);
        QVERIFY(!prepared.snapshot.writable);
        QVERIFY(QFileInfo::exists(fixture.paths.pendingPath()));
        QCOMPARE(readBytes(fixture.paths.desiredPath()), desiredBefore);
        QVERIFY(sameIdentity(
            desiredIdentity, metadata(fixture.paths.desiredPath())
        ));
        QVERIFY(!QFileInfo::exists(fixture.paths.lastGoodPath()));
        QVERIFY(!QFileInfo::exists(fixture.paths.activationPath()));
        const auto verified = authority->verifyGeneration(
            QString::fromLatin1(nonceA)
        );
        QVERIFY(verified.success);
        QCOMPARE(verified.generation->id, prepared.prepared->id);

        const auto wrong = authority->abortApply(
            QStringLiteral("ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff")
        );
        QVERIFY(!wrong.success);
        QCOMPARE(wrong.errorCode, QStringLiteral("VerificationFailed"));
        QVERIFY(QFileInfo::exists(fixture.paths.pendingPath()));

        const auto aborted = authority->abortApply(prepared.prepared->id);
        QVERIFY2(aborted.success, qPrintable(aborted.errorMessage));
        QVERIFY(aborted.snapshot.writable);
        QVERIFY(!QFileInfo::exists(fixture.paths.pendingPath()));
        QCOMPARE(readBytes(fixture.paths.desiredPath()), desiredBefore);
        QVERIFY(authority->verifyGeneration(QString::fromLatin1(nonceA)).success);
    }

    void commitPublishesOneCoherentAppliedTuple()
    {
        StoreFixture fixture;
        QVERIFY(fixture.temporary.isValid());
        auto authority = transaction(fixture.paths);
        QVERIFY(authority->initialize().success);
        const auto prepared = authority->prepareApply(
            0, QString::fromLatin1(nonceA), fixedTime()
        );
        QVERIFY(prepared.success);
        const auto committed = authority->commitApply(prepared.prepared->id);
        QVERIFY2(committed.success, qPrintable(committed.errorMessage));
        QVERIFY(committed.commitDecisionDurable);
        QVERIFY(committed.commitDecisionMayExist);
        const auto snapshot = committed.snapshot;
        QVERIFY(snapshot.available);
        QVERIFY(snapshot.writable);
        QCOMPARE(snapshot.revision, quint64(0));
        QCOMPARE(snapshot.appliedRevision, quint64(0));
        QCOMPARE(snapshot.applyState, QStringLiteral("current"));
        QVERIFY(!snapshot.requiredActivation.has_value());
        QCOMPARE(snapshot.generationDigest, prepared.prepared->id);
        QCOMPARE(readBytes(fixture.paths.lastGoodPath()), snapshot.desiredState);
        QVERIFY(!readBytes(fixture.paths.activationPath()).isEmpty());
        QVERIFY(!QFileInfo::exists(fixture.paths.pendingPath()));
        QVERIFY(!QFileInfo::exists(fixture.paths.stableEntrypointPath()));
    }

    void deletingRestartAndSessionStateKeepsStrongestRequirement()
    {
        {
            StoreFixture fixture;
            QVERIFY(fixture.temporary.isValid());
            auto authority = transaction(fixture.paths);
            QVERIFY(authority->initialize().success);
            auto replaced = authority->replaceSnapshot(
                0, serializeDesiredState(permissionState(0))
            );
            QVERIFY(replaced.success);
            auto prepared = authority->prepareApply(
                1, QString::fromLatin1(nonceA), fixedTime()
            );
            QVERIFY(prepared.success);
            QCOMPARE(prepared.prepared->requirement,
                     ActivationRequirement::Restart);
            QVERIFY(authority->commitApply(prepared.prepared->id).success);

            auto deletion = defaults;
            deletion.revision = 1;
            replaced = authority->replaceSnapshot(
                1, serializeDesiredState(deletion)
            );
            QVERIFY(replaced.success);
            QCOMPARE(*replaced.snapshot.requiredActivation,
                     ActivationRequirement::Restart);
            prepared = authority->prepareApply(
                2, QString::fromLatin1(nonceB), fixedTime(1)
            );
            QVERIFY(prepared.success);
            QCOMPARE(prepared.prepared->requirement,
                     ActivationRequirement::Restart);
        }
        {
            StoreFixture fixture;
            QVERIFY(fixture.temporary.isValid());
            auto authority = transaction(fixture.paths);
            QVERIFY(authority->initialize().success);
            auto replaced = authority->replaceSnapshot(
                0, serializeDesiredState(environmentState(0))
            );
            QVERIFY(replaced.success);
            auto prepared = authority->prepareApply(
                1, QString::fromLatin1(nonceA), fixedTime()
            );
            QVERIFY(prepared.success);
            QCOMPARE(prepared.prepared->requirement,
                     ActivationRequirement::Session);
            QVERIFY(authority->commitApply(prepared.prepared->id).success);

            auto deletion = defaults;
            deletion.revision = 1;
            replaced = authority->replaceSnapshot(
                1, serializeDesiredState(deletion)
            );
            QVERIFY(replaced.success);
            QCOMPARE(*replaced.snapshot.requiredActivation,
                     ActivationRequirement::Session);
            prepared = authority->prepareApply(
                2, QString::fromLatin1(nonceB), fixedTime(1)
            );
            QVERIFY(prepared.success);
            QCOMPARE(prepared.prepared->requirement,
                     ActivationRequirement::Session);
        }
    }

    void recoveryStagesMonotonicCopyWithoutChangingDesired()
    {
        StoreFixture fixture;
        QVERIFY(fixture.temporary.isValid());
        auto authority = transaction(fixture.paths);
        QVERIFY(authority->initialize().success);
        auto replaced = authority->replaceSnapshot(
            0, serializeDesiredState(permissionState(0))
        );
        QVERIFY(replaced.success);
        auto applied = authority->prepareApply(
            1, QString::fromLatin1(nonceA), fixedTime()
        );
        QVERIFY(applied.success);
        QVERIFY(authority->commitApply(applied.prepared->id).success);

        replaced = authority->replaceSnapshot(
            1, serializeDesiredState(environmentState(1))
        );
        QVERIFY(replaced.success);
        QCOMPARE(replaced.snapshot.revision, quint64(2));
        QCOMPARE(*replaced.snapshot.requiredActivation,
                 ActivationRequirement::Session);
        const auto desiredBefore = readBytes(fixture.paths.desiredPath());
        const auto desiredIdentity = metadata(fixture.paths.desiredPath());
        const auto lastGoodBefore = readBytes(fixture.paths.lastGoodPath());
        const auto activationBefore = readBytes(fixture.paths.activationPath());

        auto recovery = authority->prepareRecovery(
            2, QString::fromLatin1(nonceB), fixedTime(1)
        );
        QVERIFY2(recovery.success, qPrintable(recovery.errorMessage));
        QCOMPARE(recovery.snapshot.revision, quint64(2));
        QCOMPARE(recovery.prepared->revision, quint64(3));
        QCOMPARE(recovery.prepared->requirement, ActivationRequirement::Reload);
        QCOMPARE(readBytes(fixture.paths.desiredPath()), desiredBefore);
        QVERIFY(sameIdentity(
            desiredIdentity, metadata(fixture.paths.desiredPath())
        ));
        QCOMPARE(readBytes(fixture.paths.lastGoodPath()), lastGoodBefore);
        QCOMPARE(readBytes(fixture.paths.activationPath()), activationBefore);

        const auto aborted = authority->abortApply(recovery.prepared->id);
        QVERIFY(aborted.success);
        QCOMPARE(aborted.snapshot.revision, quint64(2));
        QCOMPARE(aborted.snapshot.desiredState, desiredBefore);
        QCOMPARE(*aborted.snapshot.requiredActivation,
                 ActivationRequirement::Session);

        recovery = authority->prepareRecovery(
            2, QString::fromLatin1(nonceC), fixedTime(2)
        );
        QVERIFY(recovery.success);
        const auto committed = authority->commitApply(recovery.prepared->id);
        QVERIFY2(committed.success, qPrintable(committed.errorMessage));
        QCOMPARE(committed.snapshot.revision, quint64(3));
        QCOMPARE(committed.snapshot.appliedRevision, quint64(3));
        QCOMPARE(committed.snapshot.applyState, QStringLiteral("current"));
        QVERIFY(!committed.snapshot.requiredActivation.has_value());
        const auto recoveredState = parseDesiredState(
            committed.snapshot.desiredState, catalog, actionCatalog
        );
        QVERIFY(recoveredState);
        QCOMPARE(recoveredState.value->revision, quint64(3));
        QCOMPARE(recoveredState.value->permissions,
                 permissionState(0).permissions);
        QVERIFY(recoveredState.value->environment.isEmpty());
    }

    void recoveryWithoutLastAppliedStateFailsBeforeStaging()
    {
        StoreFixture fixture;
        QVERIFY(fixture.temporary.isValid());
        auto authority = transaction(fixture.paths);
        QVERIFY(authority->initialize().success);
        const auto rejected = authority->prepareRecovery(
            0, QString::fromLatin1(nonceA), fixedTime()
        );
        QVERIFY(!rejected.success);
        QCOMPARE(rejected.errorCode, QStringLiteral("RecoveryUnavailable"));
        QVERIFY(!QFileInfo::exists(fixture.paths.pendingPath()));
        QVERIFY(!QFileInfo::exists(QDir(fixture.paths.generationsPath()).filePath(
            QString::fromLatin1(nonceA)
        )));
    }

    void displayPreviewStagesOnlyMonitorsAtNPlusOne()
    {
        StoreFixture fixture;
        QVERIFY(fixture.temporary.isValid());
        auto authority = transaction(fixture.paths);
        QVERIFY(initializeAppliedDefault(*authority));
        const auto baseline = authority->snapshot();
        QCOMPARE(baseline.revision, quint64(0));
        QCOMPARE(baseline.appliedRevision, quint64(0));
        QCOMPARE(baseline.applyState, QStringLiteral("current"));
        const auto desiredBefore = readBytes(fixture.paths.desiredPath());
        const auto desiredIdentity = metadata(fixture.paths.desiredPath());
        const auto lastGoodBefore = readBytes(fixture.paths.lastGoodPath());
        const auto activationBefore = readBytes(fixture.paths.activationPath());
        const auto topology = oneDisplayTopology();
        const auto profile = oneDisplayProfile(topology);

        auto prepared = authority->prepareDisplayApply(
            0, profile, topology, QString::fromLatin1(nonceB), fixedTime(1)
        );
        QVERIFY2(prepared.success, qPrintable(prepared.errorMessage));
        QVERIFY(prepared.prepared.has_value());
        QCOMPARE(prepared.prepared->revision, quint64(1));
        QCOMPARE(prepared.prepared->requirement, ActivationRequirement::Reload);
        QCOMPARE(prepared.snapshot.revision, quint64(0));
        QCOMPARE(prepared.snapshot.appliedRevision, quint64(0));
        QCOMPARE(prepared.snapshot.applyState, QStringLiteral("retained"));
        QVERIFY(!prepared.snapshot.writable);
        QCOMPARE(prepared.snapshot.desiredState, desiredBefore);
        QCOMPARE(readBytes(fixture.paths.desiredPath()), desiredBefore);
        QVERIFY(sameIdentity(
            desiredIdentity, metadata(fixture.paths.desiredPath())
        ));
        QCOMPARE(readBytes(fixture.paths.lastGoodPath()), lastGoodBefore);
        QCOMPARE(readBytes(fixture.paths.activationPath()), activationBefore);

        const auto pending = objectFromBytes(
            readBytes(fixture.paths.pendingPath())
        );
        QCOMPARE(pending.value(QStringLiteral("kind")).toString(),
                 QStringLiteral("display-preview"));
        QCOMPARE(pending.value(QStringLiteral("phase")).toString(),
                 QStringLiteral("prepared"));
        QCOMPARE(pending.value(QStringLiteral("expectedRevision")).toString(),
                 QStringLiteral("0"));
        auto candidateRoot = pending.value(
            QStringLiteral("candidateSnapshot")
        ).toObject();
        QCOMPARE(candidateRoot.value(QStringLiteral("revision")).toString(),
                 QStringLiteral("1"));
        QCOMPARE(candidateRoot.value(QStringLiteral("monitors")).toArray().size(),
                 1);
        QCOMPARE(
            candidateRoot.value(QStringLiteral("monitors")).toArray().first()
                .toObject().value(QStringLiteral("selector")).toString(),
            QStringLiteral("DP-1")
        );
        auto baselineRoot = objectFromBytes(desiredBefore);
        candidateRoot.remove(QStringLiteral("revision"));
        candidateRoot.remove(QStringLiteral("monitors"));
        baselineRoot.remove(QStringLiteral("revision"));
        baselineRoot.remove(QStringLiteral("monitors"));
        QCOMPARE(candidateRoot, baselineRoot);

        const auto aborted = authority->abortApply(prepared.prepared->id);
        QVERIFY2(aborted.success, qPrintable(aborted.errorMessage));
        QCOMPARE(aborted.snapshot, baseline);
        QCOMPARE(readBytes(fixture.paths.desiredPath()), desiredBefore);
        QVERIFY(sameIdentity(
            desiredIdentity, metadata(fixture.paths.desiredPath())
        ));
        QCOMPARE(readBytes(fixture.paths.lastGoodPath()), lastGoodBefore);
        QCOMPARE(readBytes(fixture.paths.activationPath()), activationBefore);

        prepared = authority->prepareDisplayApply(
            0, profile, topology, QString::fromLatin1(nonceC), fixedTime(2)
        );
        QVERIFY2(prepared.success, qPrintable(prepared.errorMessage));
        QCOMPARE(readBytes(fixture.paths.desiredPath()), desiredBefore);
        const auto committed = authority->commitApply(prepared.prepared->id);
        QVERIFY2(committed.success, qPrintable(committed.errorMessage));
        QVERIFY(committed.commitDecisionDurable);
        QVERIFY(committed.commitDecisionMayExist);
        QCOMPARE(committed.snapshot.revision, quint64(1));
        QCOMPARE(committed.snapshot.appliedRevision, quint64(1));
        QCOMPARE(committed.snapshot.applyState, QStringLiteral("current"));
        QVERIFY(committed.snapshot.writable);
        const auto desiredAfter = parseDesiredState(
            committed.snapshot.desiredState, catalog, actionCatalog
        );
        QVERIFY2(desiredAfter, qPrintable(describeErrors(desiredAfter.errors)));
        QCOMPARE(desiredAfter.value->revision, quint64(1));
        QCOMPARE(desiredAfter.value->monitors.size(), 1);
        QCOMPARE(desiredAfter.value->monitors.front().selector,
                 QStringLiteral("DP-1"));
        QCOMPARE(readBytes(fixture.paths.desiredPath()),
                 committed.snapshot.desiredState);
        QCOMPARE(readBytes(fixture.paths.lastGoodPath()),
                 committed.snapshot.desiredState);
        QVERIFY(!QFileInfo::exists(fixture.paths.pendingPath()));
    }

    void displayPreviewRequiresExactAppliedBaselineAndNoPending()
    {
        const auto topology = oneDisplayTopology();
        const auto profile = oneDisplayProfile(topology);
        {
            StoreFixture fixture;
            QVERIFY(fixture.temporary.isValid());
            auto authority = transaction(fixture.paths);
            QVERIFY(authority->initialize().success);
            const auto rejected = authority->prepareDisplayApply(
                0, profile, topology, QString::fromLatin1(nonceA), fixedTime()
            );
            QVERIFY(!rejected.success);
            QCOMPARE(rejected.errorCode,
                     QStringLiteral("DisplayScopeConflict"));
            QVERIFY(!QFileInfo::exists(fixture.paths.pendingPath()));
        }
        {
            StoreFixture fixture;
            QVERIFY(fixture.temporary.isValid());
            auto authority = transaction(fixture.paths);
            QVERIFY(initializeAppliedDefault(*authority));
            const auto stale = authority->prepareDisplayApply(
                1, profile, topology, QString::fromLatin1(nonceB), fixedTime(1)
            );
            QVERIFY(!stale.success);
            QCOMPARE(stale.errorCode, QStringLiteral("StaleRevision"));
            QVERIFY(!QFileInfo::exists(fixture.paths.pendingPath()));

            const auto ordinary = authority->prepareApply(
                0, QString::fromLatin1(nonceB), fixedTime(1)
            );
            QVERIFY(ordinary.success);
            const auto pending = authority->prepareDisplayApply(
                0, profile, topology, QString::fromLatin1(nonceC), fixedTime(2)
            );
            QVERIFY(!pending.success);
            QCOMPARE(pending.errorCode, QStringLiteral("ConfirmationPending"));
            QVERIFY(authority->abortApply(ordinary.prepared->id).success);

            const auto replaced = authority->replaceSnapshot(
                0, serializeDesiredState(permissionState(0))
            );
            QVERIFY(replaced.success);
            QCOMPARE(replaced.snapshot.revision, quint64(1));
            const auto dirty = authority->prepareDisplayApply(
                1, profile, topology, QString::fromLatin1(nonceD), fixedTime(3)
            );
            QVERIFY(!dirty.success);
            QCOMPARE(dirty.errorCode,
                     QStringLiteral("DisplayScopeConflict"));
            QVERIFY(!QFileInfo::exists(fixture.paths.pendingPath()));
        }
    }

    void preparedDisplayPreviewCrashKeepsAuthorityAtN()
    {
        StoreFixture fixture;
        QVERIFY(fixture.temporary.isValid());
        QByteArray desiredBefore;
        QByteArray lastGoodBefore;
        QByteArray activationBefore;
        QString baselineGeneration;
        struct stat desiredIdentity {};
        {
            auto authority = transaction(fixture.paths);
            QVERIFY(initializeAppliedDefault(*authority));
            desiredBefore = readBytes(fixture.paths.desiredPath());
            lastGoodBefore = readBytes(fixture.paths.lastGoodPath());
            activationBefore = readBytes(fixture.paths.activationPath());
            desiredIdentity = metadata(fixture.paths.desiredPath());
            baselineGeneration = authority->snapshot().generationDigest;
            const auto topology = oneDisplayTopology();
            const auto prepared = authority->prepareDisplayApply(
                0, oneDisplayProfile(topology), topology,
                QString::fromLatin1(nonceB), fixedTime(1)
            );
            QVERIFY2(prepared.success, qPrintable(prepared.errorMessage));
            QVERIFY(QFileInfo::exists(fixture.paths.pendingPath()));
            QCOMPARE(readBytes(fixture.paths.desiredPath()), desiredBefore);
        }

        auto restarted = transaction(fixture.paths);
        const auto initialized = restarted->initialize();
        QVERIFY2(initialized.success, qPrintable(initialized.errorMessage));
        QCOMPARE(initialized.snapshot.loadState, QStringLiteral("recovered"));
        QCOMPARE(initialized.snapshot.revision, quint64(0));
        QCOMPARE(initialized.snapshot.appliedRevision, quint64(0));
        QCOMPARE(initialized.snapshot.applyState, QStringLiteral("current"));
        QCOMPARE(initialized.snapshot.generationDigest, baselineGeneration);
        QCOMPARE(initialized.snapshot.desiredState, desiredBefore);
        QCOMPARE(readBytes(fixture.paths.desiredPath()), desiredBefore);
        QVERIFY(sameIdentity(
            desiredIdentity, metadata(fixture.paths.desiredPath())
        ));
        QCOMPARE(readBytes(fixture.paths.lastGoodPath()), lastGoodBefore);
        QCOMPARE(readBytes(fixture.paths.activationPath()), activationBefore);
        QVERIFY(!QFileInfo::exists(fixture.paths.pendingPath()));
    }

    void uncertainDisplayCommitCrashRollsAuthorityForward()
    {
        StoreFixture fixture;
        QVERIFY(fixture.temporary.isValid());
        QString previewGeneration;
        QByteArray candidateBytes;
        {
            auto authority = transaction(fixture.paths);
            QVERIFY(initializeAppliedDefault(*authority));
        }
        int pendingPublications = 0;
        fixture.paths.faultHook = [&pendingPublications](
            const StoreFaultPoint point, const StoreFile file
        ) {
            return file == StoreFile::Pending
                && point
                    == StoreFaultPoint::AfterPublishRenameBeforeDirectorySync
                && ++pendingPublications == 2;
        };
        {
            auto authority = transaction(fixture.paths);
            const auto initialized = authority->initialize();
            QVERIFY2(initialized.success, qPrintable(initialized.errorMessage));
            const auto topology = oneDisplayTopology();
            const auto prepared = authority->prepareDisplayApply(
                0, oneDisplayProfile(topology), topology,
                QString::fromLatin1(nonceB), fixedTime(1)
            );
            QVERIFY2(prepared.success, qPrintable(prepared.errorMessage));
            previewGeneration = prepared.prepared->id;
            auto pending = objectFromBytes(readBytes(fixture.paths.pendingPath()));
            candidateBytes = canonicalObject(
                pending.value(QStringLiteral("candidateSnapshot")).toObject()
            );
            const auto committed = authority->commitApply(previewGeneration);
            QVERIFY(!committed.success);
            QCOMPARE(committed.errorCode, QStringLiteral("PersistenceFailed"));
            QVERIFY(!committed.commitDecisionDurable);
            QVERIFY(committed.commitDecisionMayExist);
            QVERIFY(!committed.snapshot.available);
            pending = objectFromBytes(readBytes(fixture.paths.pendingPath()));
            QCOMPARE(pending.value(QStringLiteral("kind")).toString(),
                     QStringLiteral("display-preview"));
            QCOMPARE(pending.value(QStringLiteral("phase")).toString(),
                     QStringLiteral("committing"));
            QCOMPARE(readBytes(fixture.paths.desiredPath()) == candidateBytes,
                     false);
        }

        fixture.paths.faultHook = {};
        auto restarted = transaction(fixture.paths);
        const auto initialized = restarted->initialize();
        QVERIFY2(initialized.success, qPrintable(initialized.errorMessage));
        QCOMPARE(initialized.snapshot.loadState, QStringLiteral("recovered"));
        QCOMPARE(initialized.snapshot.revision, quint64(1));
        QCOMPARE(initialized.snapshot.appliedRevision, quint64(1));
        QCOMPARE(initialized.snapshot.applyState, QStringLiteral("current"));
        QCOMPARE(initialized.snapshot.generationDigest, previewGeneration);
        QCOMPARE(initialized.snapshot.desiredState, candidateBytes);
        QCOMPARE(readBytes(fixture.paths.desiredPath()), candidateBytes);
        QCOMPARE(readBytes(fixture.paths.lastGoodPath()), candidateBytes);
        QVERIFY(!readBytes(fixture.paths.activationPath()).isEmpty());
        QVERIFY(!QFileInfo::exists(fixture.paths.pendingPath()));
    }

    void displayPreparedJournalUncertaintyFailsClosedAndRestartsAtN()
    {
        StoreFixture fixture;
        QVERIFY(fixture.temporary.isValid());
        QByteArray desiredBefore;
        QString baselineGeneration;
        {
            auto authority = transaction(fixture.paths);
            QVERIFY(initializeAppliedDefault(*authority));
            desiredBefore = authority->snapshot().desiredState;
            baselineGeneration = authority->snapshot().generationDigest;
        }
        fixture.paths.faultHook = [](
                const StoreFaultPoint point, const StoreFile file
            ) {
                return file == StoreFile::Pending
                    && point
                        == StoreFaultPoint::AfterPublishRenameBeforeDirectorySync;
            };
        {
            auto authority = transaction(fixture.paths);
            QVERIFY(authority->initialize().success);
            const auto topology = oneDisplayTopology();
            const auto prepared = authority->prepareDisplayApply(
                0, oneDisplayProfile(topology), topology,
                QString::fromLatin1(nonceB), fixedTime(1)
            );
            QVERIFY(!prepared.success);
            QCOMPARE(prepared.errorCode, QStringLiteral("PersistenceFailed"));
            QVERIFY(!prepared.snapshot.available);
            QCOMPARE(prepared.snapshot.loadState, QStringLiteral("unavailable"));
            const auto pending = objectFromBytes(
                readBytes(fixture.paths.pendingPath())
            );
            QCOMPARE(pending.value(QStringLiteral("kind")).toString(),
                     QStringLiteral("display-preview"));
            QCOMPARE(pending.value(QStringLiteral("phase")).toString(),
                     QStringLiteral("prepared"));
            QCOMPARE(readBytes(fixture.paths.desiredPath()), desiredBefore);
        }

        fixture.paths.faultHook = {};
        auto restarted = transaction(fixture.paths);
        const auto initialized = restarted->initialize();
        QVERIFY2(initialized.success, qPrintable(initialized.errorMessage));
        QCOMPARE(initialized.snapshot.loadState, QStringLiteral("recovered"));
        QCOMPARE(initialized.snapshot.revision, quint64(0));
        QCOMPARE(initialized.snapshot.appliedRevision, quint64(0));
        QCOMPARE(initialized.snapshot.applyState, QStringLiteral("current"));
        QCOMPARE(initialized.snapshot.generationDigest, baselineGeneration);
        QCOMPARE(initialized.snapshot.desiredState, desiredBefore);
        QCOMPARE(readBytes(fixture.paths.desiredPath()), desiredBefore);
        QVERIFY(!QFileInfo::exists(fixture.paths.pendingPath()));
    }

    void reloadOnlyLastGoodRecoversWithReloadRequirement()
    {
        StoreFixture fixture;
        QVERIFY(fixture.temporary.isValid());
        auto authority = transaction(fixture.paths);
        QVERIFY(initializeAppliedDefault(*authority));
        const auto replaced = authority->replaceSnapshot(
            0, serializeDesiredState(reloadState(0))
        );
        QVERIFY(replaced.success);
        QCOMPARE(*replaced.snapshot.requiredActivation,
                 ActivationRequirement::Reload);
        const auto recovery = authority->prepareRecovery(
            1, QString::fromLatin1(nonceB), fixedTime(1)
        );
        QVERIFY2(recovery.success, qPrintable(recovery.errorMessage));
        QCOMPARE(recovery.prepared->requirement,
                 ActivationRequirement::Reload);
        QCOMPARE(recovery.prepared->revision, quint64(2));
    }

    void unchangedRestartOrSessionStateDoesNotElevateReloadDelta()
    {
        {
            StoreFixture fixture;
            QVERIFY(fixture.temporary.isValid());
            auto authority = transaction(fixture.paths);
            QVERIFY(authority->initialize().success);
            auto replaced = authority->replaceSnapshot(
                0, serializeDesiredState(permissionState(0))
            );
            QVERIFY(replaced.success);
            auto prepared = authority->prepareApply(
                1, QString::fromLatin1(nonceA), fixedTime()
            );
            QVERIFY(prepared.success);
            QVERIFY(authority->commitApply(prepared.prepared->id).success);

            auto reloadOnly = permissionState(1);
            reloadOnly.overrides.insert(
                QStringLiteral("hyprland.general.border_size"), 2
            );
            replaced = authority->replaceSnapshot(
                1, serializeDesiredState(reloadOnly)
            );
            QVERIFY(replaced.success);
            QCOMPARE(*replaced.snapshot.requiredActivation,
                     ActivationRequirement::Reload);
            prepared = authority->prepareApply(
                2, QString::fromLatin1(nonceB), fixedTime(1)
            );
            QVERIFY(prepared.success);
            QCOMPARE(prepared.prepared->requirement,
                     ActivationRequirement::Reload);
        }
        {
            StoreFixture fixture;
            QVERIFY(fixture.temporary.isValid());
            auto authority = transaction(fixture.paths);
            QVERIFY(authority->initialize().success);
            auto replaced = authority->replaceSnapshot(
                0, serializeDesiredState(environmentState(0))
            );
            QVERIFY(replaced.success);
            auto prepared = authority->prepareApply(
                1, QString::fromLatin1(nonceA), fixedTime()
            );
            QVERIFY(prepared.success);
            QVERIFY(authority->commitApply(prepared.prepared->id).success);

            auto reloadOnly = environmentState(1);
            reloadOnly.overrides.insert(
                QStringLiteral("hyprland.general.border_size"), 2
            );
            replaced = authority->replaceSnapshot(
                1, serializeDesiredState(reloadOnly)
            );
            QVERIFY(replaced.success);
            QCOMPARE(*replaced.snapshot.requiredActivation,
                     ActivationRequirement::Reload);
            prepared = authority->prepareApply(
                2, QString::fromLatin1(nonceB), fixedTime(1)
            );
            QVERIFY(prepared.success);
            QCOMPARE(prepared.prepared->requirement,
                     ActivationRequirement::Reload);
        }
    }

    void preparedCrashIsAbortedOnStartupWithoutChangingAuthority()
    {
        StoreFixture fixture;
        QVERIFY(fixture.temporary.isValid());
        QByteArray desiredBefore;
        QString orphanNonce = QString::fromLatin1(nonceB);
        {
            auto authority = transaction(fixture.paths);
            QVERIFY(initializeAppliedDefault(*authority));
            const auto replaced = authority->replaceSnapshot(
                0, serializeDesiredState(permissionState(0))
            );
            QVERIFY(replaced.success);
            desiredBefore = replaced.snapshot.desiredState;
            const auto recovery = authority->prepareRecovery(
                1, orphanNonce, fixedTime(1)
            );
            QVERIFY(recovery.success);
            QVERIFY(QFileInfo::exists(fixture.paths.pendingPath()));
        }

        auto restarted = transaction(fixture.paths);
        const auto initialized = restarted->initialize();
        QVERIFY2(initialized.success, qPrintable(initialized.errorMessage));
        QCOMPARE(initialized.snapshot.loadState, QStringLiteral("recovered"));
        QCOMPARE(initialized.snapshot.revision, quint64(1));
        QCOMPARE(initialized.snapshot.desiredState, desiredBefore);
        QCOMPARE(initialized.snapshot.appliedRevision, quint64(0));
        QCOMPARE(initialized.snapshot.applyState, QStringLiteral("retained"));
        QCOMPARE(*initialized.snapshot.requiredActivation,
                 ActivationRequirement::Restart);
        QVERIFY(!QFileInfo::exists(fixture.paths.pendingPath()));
        QVERIFY(restarted->verifyGeneration(orphanNonce).success);
    }

    void committingRecoveryCrashRollsForwardEveryMirrorPhase()
    {
        for (int mirrors = 0; mirrors <= 3; ++mirrors) {
            StoreFixture fixture;
            QVERIFY(fixture.temporary.isValid());
            QJsonObject pending;
            QByteArray candidateBytes;
            QByteArray activationBytes;
            {
                auto authority = transaction(fixture.paths);
                QVERIFY(initializeAppliedDefault(*authority));
                const auto replaced = authority->replaceSnapshot(
                    0, serializeDesiredState(permissionState(0))
                );
                QVERIFY(replaced.success);
                const auto recovery = authority->prepareRecovery(
                    1, QString::fromLatin1(nonceB), fixedTime(1)
                );
                QVERIFY(recovery.success);
                pending = objectFromBytes(readBytes(fixture.paths.pendingPath()));
                QVERIFY(!pending.isEmpty());
                pending.insert(QStringLiteral("phase"),
                               QStringLiteral("committing"));
                QVERIFY(replaceBytes(
                    fixture.paths.pendingPath(), canonicalObject(pending)
                ));
                candidateBytes = canonicalObject(
                    pending.value(QStringLiteral("candidateSnapshot")).toObject()
                );
                activationBytes = canonicalObject(
                    pending.value(QStringLiteral("afterActivation")).toObject()
                );
            }

            if (mirrors >= 1) {
                QVERIFY(replaceBytes(fixture.paths.desiredPath(), candidateBytes));
            }
            if (mirrors >= 2) {
                if (QFileInfo::exists(fixture.paths.lastGoodPath())) {
                    QVERIFY(replaceBytes(
                        fixture.paths.lastGoodPath(), candidateBytes
                    ));
                } else {
                    QVERIFY(writeNew(fixture.paths.lastGoodPath(), candidateBytes));
                }
            }
            if (mirrors >= 3) {
                QVERIFY(replaceBytes(
                    fixture.paths.activationPath(), activationBytes
                ));
            }

            auto restarted = transaction(fixture.paths);
            const auto initialized = restarted->initialize();
            QVERIFY2(initialized.success,
                     qPrintable(QStringLiteral("phase %1: %2")
                                    .arg(mirrors)
                                    .arg(initialized.errorMessage)));
            QCOMPARE(initialized.snapshot.revision, quint64(2));
            QCOMPARE(initialized.snapshot.appliedRevision, quint64(2));
            QCOMPARE(initialized.snapshot.applyState, QStringLiteral("current"));
            QCOMPARE(initialized.snapshot.loadState, QStringLiteral("recovered"));
            QCOMPARE(initialized.snapshot.desiredState, candidateBytes);
            QCOMPARE(readBytes(fixture.paths.lastGoodPath()), candidateBytes);
            QCOMPARE(readBytes(fixture.paths.activationPath()), activationBytes);
            QVERIFY(!QFileInfo::exists(fixture.paths.pendingPath()));
        }
    }

    void committingApplyCrashRollsForwardFromJournalAlone()
    {
        StoreFixture fixture;
        QVERIFY(fixture.temporary.isValid());
        QByteArray candidateBytes;
        {
            auto authority = transaction(fixture.paths);
            QVERIFY(authority->initialize().success);
            const auto replaced = authority->replaceSnapshot(
                0, serializeDesiredState(permissionState(0))
            );
            QVERIFY(replaced.success);
            candidateBytes = replaced.snapshot.desiredState;
            const auto prepared = authority->prepareApply(
                1, QString::fromLatin1(nonceA), fixedTime()
            );
            QVERIFY(prepared.success);
            auto pending = objectFromBytes(
                readBytes(fixture.paths.pendingPath())
            );
            pending.insert(QStringLiteral("phase"),
                           QStringLiteral("committing"));
            QVERIFY(replaceBytes(
                fixture.paths.pendingPath(), canonicalObject(pending)
            ));
        }

        auto restarted = transaction(fixture.paths);
        const auto initialized = restarted->initialize();
        QVERIFY2(initialized.success, qPrintable(initialized.errorMessage));
        QCOMPARE(initialized.snapshot.revision, quint64(1));
        QCOMPARE(initialized.snapshot.appliedRevision, quint64(1));
        QCOMPARE(initialized.snapshot.applyState, QStringLiteral("current"));
        QCOMPARE(initialized.snapshot.desiredState, candidateBytes);
        QCOMPARE(readBytes(fixture.paths.lastGoodPath()), candidateBytes);
        QVERIFY(!readBytes(fixture.paths.activationPath()).isEmpty());
        QVERIFY(!QFileInfo::exists(fixture.paths.pendingPath()));
    }

    void failedCommittingMarkerWriteRemainsAbortable()
    {
        StoreFixture fixture;
        QVERIFY(fixture.temporary.isValid());
        int pendingPublications = 0;
        fixture.paths.faultHook = [&pendingPublications](
            const StoreFaultPoint point,
            const StoreFile file
        ) {
            return file == StoreFile::Pending
                && point == StoreFaultPoint::BeforePublishRename
                && ++pendingPublications == 2;
        };
        auto authority = transaction(fixture.paths);
        QVERIFY(authority->initialize().success);
        const auto prepared = authority->prepareApply(
            0, QString::fromLatin1(nonceA), fixedTime()
        );
        QVERIFY(prepared.success);

        const auto committed = authority->commitApply(prepared.prepared->id);
        QVERIFY(!committed.success);
        QCOMPARE(committed.errorCode, QStringLiteral("PersistenceFailed"));
        QVERIFY(!committed.commitDecisionDurable);
        QVERIFY(!committed.commitDecisionMayExist);
        const auto pending = objectFromBytes(
            readBytes(fixture.paths.pendingPath())
        );
        QCOMPARE(pending.value(QStringLiteral("phase")).toString(),
                 QStringLiteral("prepared"));

        const auto aborted = authority->abortApply(prepared.prepared->id);
        QVERIFY2(aborted.success, qPrintable(aborted.errorMessage));
        QVERIFY(!QFileInfo::exists(fixture.paths.pendingPath()));
    }

    void uncertainCommittingMarkerCannotRollbackAndRollsForwardOnRestart()
    {
        StoreFixture fixture;
        QVERIFY(fixture.temporary.isValid());
        int pendingPublications = 0;
        fixture.paths.faultHook = [&pendingPublications](
            const StoreFaultPoint point,
            const StoreFile file
        ) {
            return file == StoreFile::Pending
                && point
                    == StoreFaultPoint::AfterPublishRenameBeforeDirectorySync
                && ++pendingPublications == 2;
        };
        QString preparedId;
        {
            auto authority = transaction(fixture.paths);
            QVERIFY(authority->initialize().success);
            const auto prepared = authority->prepareApply(
                0, QString::fromLatin1(nonceA), fixedTime()
            );
            QVERIFY(prepared.success);
            preparedId = prepared.prepared->id;
            const auto committed = authority->commitApply(preparedId);
            QVERIFY(!committed.success);
            QCOMPARE(committed.errorCode, QStringLiteral("PersistenceFailed"));
            QVERIFY(!committed.commitDecisionDurable);
            QVERIFY(committed.commitDecisionMayExist);
            QVERIFY(!committed.snapshot.available);
            QCOMPARE(committed.snapshot.loadState,
                     QStringLiteral("unavailable"));
            const auto pending = objectFromBytes(
                readBytes(fixture.paths.pendingPath())
            );
            QCOMPARE(pending.value(QStringLiteral("phase")).toString(),
                     QStringLiteral("committing"));
            const auto abort = authority->abortApply(preparedId);
            QVERIFY(!abort.success);
            QVERIFY(QFileInfo::exists(fixture.paths.pendingPath()));
        }

        fixture.paths.faultHook = {};
        auto restarted = transaction(fixture.paths);
        const auto initialized = restarted->initialize();
        QVERIFY2(initialized.success, qPrintable(initialized.errorMessage));
        QCOMPARE(initialized.snapshot.applyState, QStringLiteral("current"));
        QCOMPARE(initialized.snapshot.appliedRevision, quint64(0));
        QCOMPARE(initialized.snapshot.generationDigest, preparedId);
        QVERIFY(!QFileInfo::exists(fixture.paths.pendingPath()));
    }

    void uncertainPreparedJournalRemovalFailsClosedButRestartIsSafe()
    {
        StoreFixture fixture;
        QVERIFY(fixture.temporary.isValid());
        fixture.paths.faultHook = [](
            const StoreFaultPoint point,
            const StoreFile file
        ) {
            return file == StoreFile::Pending
                && point
                    == StoreFaultPoint::AfterRemoveBeforeDirectorySync;
        };
        {
            auto authority = transaction(fixture.paths);
            QVERIFY(authority->initialize().success);
            const auto prepared = authority->prepareApply(
                0, QString::fromLatin1(nonceA), fixedTime()
            );
            QVERIFY(prepared.success);
            const auto aborted = authority->abortApply(prepared.prepared->id);
            QVERIFY(!aborted.success);
            QCOMPARE(aborted.errorCode, QStringLiteral("PersistenceFailed"));
            QVERIFY(!aborted.snapshot.available);
            QVERIFY(!QFileInfo::exists(fixture.paths.pendingPath()));
        }

        fixture.paths.faultHook = {};
        auto restarted = transaction(fixture.paths);
        const auto initialized = restarted->initialize();
        QVERIFY2(initialized.success, qPrintable(initialized.errorMessage));
        QCOMPARE(initialized.snapshot.applyState, QStringLiteral("inactive"));
    }

    void uncertainCommittedJournalRemovalRemainsOneWayAndRestartsCurrent()
    {
        StoreFixture fixture;
        QVERIFY(fixture.temporary.isValid());
        fixture.paths.faultHook = [](
            const StoreFaultPoint point,
            const StoreFile file
        ) {
            return file == StoreFile::Pending
                && point
                    == StoreFaultPoint::AfterRemoveBeforeDirectorySync;
        };
        QString generation;
        {
            auto authority = transaction(fixture.paths);
            QVERIFY(authority->initialize().success);
            const auto prepared = authority->prepareApply(
                0, QString::fromLatin1(nonceA), fixedTime()
            );
            QVERIFY(prepared.success);
            generation = prepared.prepared->id;
            const auto committed = authority->commitApply(generation);
            QVERIFY(!committed.success);
            QCOMPARE(committed.errorCode, QStringLiteral("PersistenceFailed"));
            QVERIFY(committed.commitDecisionDurable);
            QVERIFY(committed.commitDecisionMayExist);
            QVERIFY(!committed.snapshot.available);
            QVERIFY(!QFileInfo::exists(fixture.paths.pendingPath()));
        }

        fixture.paths.faultHook = {};
        auto restarted = transaction(fixture.paths);
        const auto initialized = restarted->initialize();
        QVERIFY2(initialized.success, qPrintable(initialized.errorMessage));
        QCOMPARE(initialized.snapshot.applyState, QStringLiteral("current"));
        QCOMPARE(initialized.snapshot.generationDigest, generation);
    }

    void confirmedCommitFailureLeavesOneWayRecoveryJournal()
    {
        StoreFixture fixture;
        QVERIFY(fixture.temporary.isValid());
        auto authority = transaction(fixture.paths);
        QVERIFY(authority->initialize().success);
        const auto prepared = authority->prepareApply(
            0, QString::fromLatin1(nonceA), fixedTime()
        );
        QVERIFY(prepared.success);
        const auto external = QDir(fixture.temporary.path()).filePath(
            QStringLiteral("external")
        );
        QVERIFY(writeNew(external, QByteArrayLiteral("external")));
        QVERIFY(::symlink(
                    QFile::encodeName(external).constData(),
                    QFile::encodeName(fixture.paths.lastGoodPath()).constData()
                ) == 0);

        const auto committed = authority->commitApply(prepared.prepared->id);
        QVERIFY(!committed.success);
        QCOMPARE(committed.errorCode, QStringLiteral("PersistenceFailed"));
        QVERIFY(committed.commitDecisionDurable);
        QVERIFY(committed.commitDecisionMayExist);
        QVERIFY(!committed.snapshot.available);
        QCOMPARE(committed.snapshot.applyState, QStringLiteral("failed"));
        const auto pending = objectFromBytes(
            readBytes(fixture.paths.pendingPath())
        );
        QCOMPARE(pending.value(QStringLiteral("phase")).toString(),
                 QStringLiteral("committing"));
        QCOMPARE(readBytes(external), QByteArrayLiteral("external"));
        const auto abort = authority->abortApply(prepared.prepared->id);
        QVERIFY(!abort.success);
        QVERIFY(QFileInfo::exists(fixture.paths.pendingPath()));
    }

    void corruptOrInconsistentStartupStateIsPreservedUnavailable()
    {
        {
            StoreFixture fixture;
            QVERIFY(fixture.temporary.isValid());
            QVERIFY(makeDirectory(fixture.paths.stateRoot));
            const QByteArray corrupt{"{ not json\n"};
            QVERIFY(writeNew(fixture.paths.desiredPath(), corrupt));
            auto authority = transaction(fixture.paths);
            const auto initialized = authority->initialize();
            QVERIFY(!initialized.success);
            QVERIFY(!initialized.snapshot.available);
            QCOMPARE(initialized.snapshot.applyState, QStringLiteral("failed"));
            QCOMPARE(readBytes(fixture.paths.desiredPath()), corrupt);
        }
        {
            StoreFixture fixture;
            QVERIFY(fixture.temporary.isValid());
            QVERIFY(makeDirectory(fixture.paths.stateRoot));
            const auto desired = serializeDesiredState(defaults);
            QVERIFY(writeNew(fixture.paths.desiredPath(), desired));
            QVERIFY(writeNew(fixture.paths.lastGoodPath(), desired));
            auto authority = transaction(fixture.paths);
            const auto initialized = authority->initialize();
            QVERIFY(!initialized.success);
            QVERIFY(!initialized.snapshot.available);
            QCOMPARE(readBytes(fixture.paths.lastGoodPath()), desired);
            QVERIFY(!QFileInfo::exists(fixture.paths.activationPath()));
        }
        {
            StoreFixture fixture;
            QVERIFY(fixture.temporary.isValid());
            QVERIFY(makeDirectory(fixture.paths.stateRoot));
            QVERIFY(writeNew(
                fixture.paths.desiredPath(), serializeDesiredState(defaults)
            ));
            const QByteArray activation{"{}\n"};
            QVERIFY(writeNew(fixture.paths.activationPath(), activation));
            auto authority = transaction(fixture.paths);
            const auto initialized = authority->initialize();
            QVERIFY(!initialized.success);
            QCOMPARE(readBytes(fixture.paths.activationPath()), activation);
            QVERIFY(!QFileInfo::exists(fixture.paths.lastGoodPath()));
        }
    }

    void missingDesiredWithLastGoodFailsClosedAgainstRevisionAba()
    {
        StoreFixture fixture;
        QVERIFY(fixture.temporary.isValid());
        QByteArray lastGood;
        {
            auto authority = transaction(fixture.paths);
            QVERIFY(initializeAppliedDefault(*authority));
            lastGood = readBytes(fixture.paths.lastGoodPath());
        }
        QVERIFY(QFile::remove(fixture.paths.desiredPath()));

        const auto activation = readBytes(fixture.paths.activationPath());
        auto restarted = transaction(fixture.paths);
        const auto initialized = restarted->initialize();
        QVERIFY(!initialized.success);
        QVERIFY(!initialized.snapshot.available);
        QCOMPARE(initialized.snapshot.applyState, QStringLiteral("failed"));
        QVERIFY(!QFileInfo::exists(fixture.paths.desiredPath()));
        QCOMPARE(readBytes(fixture.paths.lastGoodPath()), lastGood);
        QCOMPARE(readBytes(fixture.paths.activationPath()), activation);
    }

    void futureMinorStateIsRetainedReadOnlyWithoutRendering()
    {
        StoreFixture fixture;
        QVERIFY(fixture.temporary.isValid());
        QVERIFY(makeDirectory(fixture.paths.stateRoot));
        auto future = objectFromBytes(serializeDesiredState(defaults));
        future.insert(QStringLiteral("targetHyprland"), QStringLiteral("0.57"));
        future.insert(QStringLiteral("catalogDigest"),
                      QString(64, QLatin1Char('a')));
        future.insert(
            QStringLiteral("overrides"),
            QJsonObject{{QStringLiteral("future:option"),
                         QStringLiteral("preserved")}}
        );
        const auto bytes = canonicalObject(future);
        QVERIFY(writeNew(fixture.paths.desiredPath(), bytes));

        auto authority = transaction(fixture.paths);
        const auto initialized = authority->initialize();
        QVERIFY2(initialized.success, qPrintable(initialized.errorMessage));
        QVERIFY(initialized.snapshot.available);
        QVERIFY(!initialized.snapshot.writable);
        QCOMPARE(initialized.snapshot.loadState, QStringLiteral("unsupported"));
        QCOMPARE(initialized.snapshot.applyState, QStringLiteral("retained"));
        QCOMPARE(initialized.snapshot.desiredState, bytes);
        QVERIFY(!initialized.snapshot.requiredActivation.has_value());

        const auto replace = authority->replaceSnapshot(0, bytes);
        QVERIFY(!replace.success);
        QCOMPARE(replace.errorCode, QStringLiteral("ReadOnly"));
        const auto prepare = authority->prepareApply(
            0, QString::fromLatin1(nonceA), fixedTime()
        );
        QVERIFY(!prepare.success);
        QCOMPARE(prepare.errorCode, QStringLiteral("ReadOnly"));
        QCOMPARE(readBytes(fixture.paths.desiredPath()), bytes);
        QVERIFY(!QFileInfo::exists(fixture.paths.pendingPath()));
    }

    void manifestWithValidSelfHashButWrongAuthorityIsRejected()
    {
        for (const auto &mismatch : {
                 QStringLiteral("snapshot"),
                 QStringLiteral("catalog"),
                 QStringLiteral("actions"),
                 QStringLiteral("target"),
                 QStringLiteral("range"),
             }) {
            StoreFixture fixture;
            QVERIFY(fixture.temporary.isValid());
            {
                auto authority = transaction(fixture.paths);
                QVERIFY(authority->initialize().success);
                const auto prepared = authority->prepareApply(
                    0, QString::fromLatin1(nonceA), fixedTime()
                );
                QVERIFY(prepared.success);
            }
            const auto generationRoot = QDir(fixture.paths.generationsPath())
                                            .filePath(QString::fromLatin1(nonceA));
            const auto manifestPath = QDir(generationRoot).filePath(
                QStringLiteral("manifest.json")
            );
            QVERIFY(::chmod(
                        QFile::encodeName(generationRoot).constData(), 0700
                    ) == 0);
            QVERIFY(::chmod(
                        QFile::encodeName(manifestPath).constData(), 0600
                    ) == 0);
            auto manifest = objectFromBytes(readBytes(manifestPath));
            const auto differentDigest = [&manifest](const QString &field) {
                const auto current = manifest.value(field).toString();
                const auto zero = QString(64, QLatin1Char('0'));
                return current == zero
                    ? QString(64, QLatin1Char('1')) : zero;
            };
            if (mismatch == QStringLiteral("snapshot")) {
                manifest.insert(
                    QStringLiteral("snapshotDigest"),
                    differentDigest(QStringLiteral("snapshotDigest"))
                );
            } else if (mismatch == QStringLiteral("catalog")) {
                manifest.insert(
                    QStringLiteral("catalogDigest"),
                    differentDigest(QStringLiteral("catalogDigest"))
                );
            } else if (mismatch == QStringLiteral("actions")) {
                manifest.insert(
                    QStringLiteral("actionCatalogDigest"),
                    differentDigest(QStringLiteral("actionCatalogDigest"))
                );
            } else if (mismatch == QStringLiteral("target")) {
                manifest.insert(
                    QStringLiteral("targetHyprland"),
                    QStringLiteral("999.999")
                );
            } else {
                auto compatible = manifest.value(
                    QStringLiteral("compatibleHyprland")
                ).toObject();
                compatible.insert(QStringLiteral("minor"), 999);
                manifest.insert(
                    QStringLiteral("compatibleHyprland"), compatible
                );
            }
            manifest.remove(QStringLiteral("generation"));
            const auto changedGeneration = sha256(
                JsonSupport::canonicalJson(manifest)
            );
            manifest.insert(QStringLiteral("generation"), changedGeneration);
            QVERIFY(replaceBytes(
                manifestPath, canonicalObject(manifest), 0400
            ));
            QVERIFY(::chmod(
                        QFile::encodeName(generationRoot).constData(), 0500
                    ) == 0);

            auto pending = objectFromBytes(
                readBytes(fixture.paths.pendingPath())
            );
            auto after = pending.value(
                QStringLiteral("afterActivation")
            ).toObject();
            after.insert(QStringLiteral("generation"), changedGeneration);
            pending.insert(QStringLiteral("afterActivation"), after);
            QVERIFY(replaceBytes(
                fixture.paths.pendingPath(), canonicalObject(pending)
            ));

            auto restarted = transaction(fixture.paths);
            const auto initialized = restarted->initialize();
            QVERIFY2(!initialized.success, qPrintable(mismatch));
            QVERIFY(!initialized.snapshot.available);
            QVERIFY(QFileInfo::exists(fixture.paths.pendingPath()));
            QCOMPARE(readBytes(manifestPath), canonicalObject(manifest));
        }
    }

    void deferredBrokerAndUwsmStateRemainSavedButCannotStage()
    {
        {
            StoreFixture fixture;
            QVERIFY(fixture.temporary.isValid());
            auto authority = transaction(fixture.paths);
            QVERIFY(authority->initialize().success);
            const auto candidate = serializeDesiredState(brokerState(0, true));
            const auto replaced = authority->replaceSnapshot(0, candidate);
            QVERIFY(replaced.success);
            const auto rejected = authority->prepareApply(
                1, QString::fromLatin1(nonceA), fixedTime()
            );
            QVERIFY(!rejected.success);
            QCOMPARE(rejected.errorCode, QStringLiteral("ActivationRequired"));
            QCOMPARE(authority->snapshot().desiredState, replaced.snapshot.desiredState);
            QVERIFY(authority->snapshot().writable);
            QVERIFY(!QFileInfo::exists(fixture.paths.pendingPath()));
        }
        {
            StoreFixture fixture;
            QVERIFY(fixture.temporary.isValid());
            auto authority = transaction(fixture.paths);
            QVERIFY(authority->initialize().success);
            const auto candidate = serializeDesiredState(
                environmentState(0, EnvironmentScope::Uwsm)
            );
            const auto replaced = authority->replaceSnapshot(0, candidate);
            QVERIFY(replaced.success);
            const auto rejected = authority->prepareApply(
                1, QString::fromLatin1(nonceA), fixedTime()
            );
            QVERIFY(!rejected.success);
            QCOMPARE(rejected.errorCode, QStringLiteral("ActivationRequired"));
            QCOMPARE(authority->snapshot().desiredState, replaced.snapshot.desiredState);
            QVERIFY(!QFileInfo::exists(fixture.paths.pendingPath()));
        }
    }

    void userCustomIsNeverReadRewrittenOrReplacedByTransactions()
    {
        StoreFixture fixture;
        QVERIFY(fixture.temporary.isValid());
        QVERIFY(makeDirectory(fixture.paths.configRoot));
        const QByteArray authored{
            "-- user-owned content\nerror('only loaded by Hyprland')\n"
        };
        QVERIFY(writeNew(fixture.paths.userCustomPath(), authored, 0660));
        const auto before = metadata(fixture.paths.userCustomPath());

        auto authority = transaction(fixture.paths);
        QVERIFY(authority->initialize().success);
        auto replaced = authority->replaceSnapshot(
            0, serializeDesiredState(permissionState(0))
        );
        QVERIFY(replaced.success);
        auto prepared = authority->prepareApply(
            1, QString::fromLatin1(nonceA), fixedTime()
        );
        QVERIFY(prepared.success);
        QVERIFY(authority->abortApply(prepared.prepared->id).success);
        prepared = authority->prepareApply(
            1, QString::fromLatin1(nonceB), fixedTime(1)
        );
        QVERIFY(prepared.success);
        QVERIFY(authority->commitApply(prepared.prepared->id).success);

        QCOMPARE(readBytes(fixture.paths.userCustomPath()), authored);
        const auto after = metadata(fixture.paths.userCustomPath());
        QVERIFY(sameIdentity(before, after));
        QCOMPARE(after.st_mode, before.st_mode);
        QVERIFY(!QFileInfo::exists(fixture.paths.stableEntrypointPath()));
    }
};

QTEST_MAIN(CompositorTransactionTest)

#include "compositor_transaction_test.moc"
