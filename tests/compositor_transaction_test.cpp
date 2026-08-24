#include "compositord/transaction.h"
#include "compositord/legacy_transaction_records.h"

#include "hyprland/json_support.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QRegularExpression>
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

    struct LegacyGenerationFixture final {
        QByteArray candidateBytes;
        QByteArray pendingBytes;
        QByteArray activationBytes;
        QString generation;
    };

    [[nodiscard]] QByteArray preSharedSpacingStoredBytes() const
    {
        return canonicalObject(objectFromBytes(readBytes(
            QStringLiteral(
                HYPRSHELLD_HYPRLAND_PRE_SHARED_SPACING_DEFAULT_FILE
            )
        )));
    }

    [[nodiscard]] QByteArray preBindingsQuarantineStoredBytes() const
    {
        return canonicalObject(objectFromBytes(readBytes(
            QStringLiteral(
                HYPRSHELLD_HYPRLAND_PRE_BINDINGS_QUARANTINE_DEFAULT_FILE
            )
        )));
    }

    [[nodiscard]] QByteArray currentAuthorityBytes(
        const QByteArrayView legacyBytes
    ) const
    {
        return authorityTupleBytes(legacyBytes, true, true);
    }

    [[nodiscard]] QByteArray authorityTupleBytes(
        const QByteArrayView legacyBytes,
        const bool currentCatalog,
        const bool currentActionCatalog
    ) const
    {
        auto object = objectFromBytes(legacyBytes);
        if (currentCatalog) {
            object.insert(
                QStringLiteral("catalogDigest"), defaults.catalogDigest
            );
        }
        if (currentActionCatalog) {
            object.insert(
                QStringLiteral("actionCatalogDigest"),
                defaults.actionCatalogDigest
            );
        }
        return canonicalObject(object);
    }

    [[nodiscard]] bool stageLegacyGeneration(
        StoreFixture &fixture,
        const QByteArrayView legacyCandidateBytes,
        const QString &nonce,
        LegacyGenerationFixture &result
    ) const
    {
        const auto legacyObject = objectFromBytes(legacyCandidateBytes);
        bool revisionValid = false;
        const auto revision = legacyObject.value(
            QStringLiteral("revision")
        ).toString().toULongLong(&revisionValid, 10);
        if (legacyObject.isEmpty() || !revisionValid
            || !makeDirectory(fixture.paths.stateRoot)
            || !writeNew(
                fixture.paths.desiredPath(),
                currentAuthorityBytes(legacyCandidateBytes)
            )) {
            return false;
        }

        {
            auto authority = transaction(fixture.paths);
            const auto initialized = authority->initialize();
            if (!initialized.success) {
                return false;
            }
            const auto prepared = authority->prepareApply(
                revision, nonce, fixedTime()
            );
            if (!prepared.success || !prepared.prepared) {
                return false;
            }
        }

        auto pending = objectFromBytes(readBytes(fixture.paths.pendingPath()));
        const auto generationRoot = QDir(fixture.paths.generationsPath())
                                        .filePath(nonce);
        const auto manifestPath = QDir(generationRoot).filePath(
            QStringLiteral("manifest.json")
        );
        if (pending.isEmpty()
            || ::chmod(
                QFile::encodeName(generationRoot).constData(), 0700
            ) != 0
            || ::chmod(
                QFile::encodeName(manifestPath).constData(), 0600
            ) != 0) {
            return false;
        }

        auto manifest = objectFromBytes(readBytes(manifestPath));
        if (manifest.isEmpty()) {
            return false;
        }
        const auto legacyDigest = sha256(legacyCandidateBytes);
        const auto legacyCatalogDigest = legacyObject.value(
            QStringLiteral("catalogDigest")
        ).toString();
        const auto legacyActionDigest = legacyObject.value(
            QStringLiteral("actionCatalogDigest")
        ).toString();
        manifest.insert(QStringLiteral("snapshotDigest"), legacyDigest);
        manifest.insert(
            QStringLiteral("catalogDigest"), legacyCatalogDigest
        );
        manifest.insert(
            QStringLiteral("actionCatalogDigest"), legacyActionDigest
        );
        manifest.remove(QStringLiteral("generation"));
        const auto generation = sha256(JsonSupport::canonicalJson(manifest));
        manifest.insert(QStringLiteral("generation"), generation);
        if (!replaceBytes(manifestPath, canonicalObject(manifest), 0400)
            || ::chmod(
                QFile::encodeName(generationRoot).constData(), 0500
            ) != 0) {
            return false;
        }

        auto after = pending.value(
            QStringLiteral("afterActivation")
        ).toObject();
        after.insert(QStringLiteral("snapshotDigest"), legacyDigest);
        after.insert(QStringLiteral("generation"), generation);
        pending.insert(
            QStringLiteral("candidateSnapshot"), legacyObject
        );
        pending.insert(QStringLiteral("snapshotDigest"), legacyDigest);
        pending.insert(QStringLiteral("beforeDesiredDigest"), legacyDigest);
        pending.insert(QStringLiteral("afterActivation"), after);
        const auto migratedPending = canonicalObject(pending);
        if (!replaceBytes(
                fixture.paths.desiredPath(), legacyCandidateBytes
            )
            || !replaceBytes(
                fixture.paths.pendingPath(), migratedPending
            )) {
            return false;
        }

        result = {
            .candidateBytes = legacyCandidateBytes.toByteArray(),
            .pendingBytes = migratedPending,
            .activationBytes = canonicalObject(after),
            .generation = generation,
        };
        return true;
    }

    [[nodiscard]] bool installLegacyApplied(
        StoreFixture &fixture,
        const QByteArrayView legacyCandidateBytes,
        const QString &nonce,
        LegacyGenerationFixture &result
    ) const
    {
        return stageLegacyGeneration(
                   fixture, legacyCandidateBytes, nonce, result
               )
            && QFile::remove(fixture.paths.pendingPath())
            && writeNew(
                fixture.paths.lastGoodPath(), legacyCandidateBytes
            )
            && writeNew(
                fixture.paths.activationPath(), result.activationBytes
            );
    }

    [[nodiscard]] std::unique_ptr<ConfigurationTransaction> transaction(
        const StorePaths &paths
    ) const
    {
        return std::make_unique<ConfigurationTransaction>(
            paths, catalog, actionCatalog
        );
    }

    [[nodiscard]] bool installAppliedState(
        StoreFixture &fixture,
        const DesiredState &state,
        const QString &nonce
    ) const
    {
        PersistentStore store(fixture.paths);
        const auto stored = store.initialize();
        if (!stored.success) {
            return false;
        }
        GenerationStore generations(store);
        const auto initialized = generations.initialize();
        if (!initialized.success) {
            return false;
        }

        const auto bytes = serializeDesiredState(state);
        const auto rendered = renderGeneration(
            state,
            catalog,
            actionCatalog,
            generations.directoryForNonce(nonce),
            fixture.paths.userCustomPath(),
            nonce,
            fixedTime()
        );
        if (!rendered) {
            return false;
        }
        const auto published = generations.publish(*rendered.value);
        if (!published.success || !published.generation) {
            return false;
        }
        const auto activation = canonicalObject(QJsonObject{
            {QStringLiteral("formatVersion"), 1},
            {QStringLiteral("revision"), QString::number(state.revision)},
            {QStringLiteral("snapshotDigest"), sha256(bytes)},
            {QStringLiteral("generation"), published.generation->id},
            {QStringLiteral("activationNonce"), nonce},
            {QStringLiteral("entrypoint"), published.generation->entrypoint},
            {
                QStringLiteral("requiredActivation"),
                activationRequirementName(
                    rendered.value->activationRequirement
                )
            },
        });
        return store.write(StoreFile::Desired, bytes).success
            && store.write(StoreFile::LastGood, bytes).success
            && store.write(StoreFile::Activation, activation).success;
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

    [[nodiscard]] DesiredState glowState(
        const quint64 revision,
        const bool enabled,
        const int range
    ) const
    {
        auto state = defaults;
        state.revision = revision;
        state.overrides.insert(
            QStringLiteral("hyprland.decoration.glow.enabled"), enabled
        );
        state.overrides.insert(
            QStringLiteral("hyprland.decoration.glow.range"), range
        );
        return state;
    }

    [[nodiscard]] DeviceConfiguration inputDevice(
        QString id = QStringLiteral("device-a"),
        QString selector = QStringLiteral("main-keyboard"),
        QString kind = QStringLiteral("keyboard"),
        const bool enabled = true,
        QJsonObject overrides = QJsonObject{{
            QStringLiteral("sensitivity"), 0.25
        }}
    ) const
    {
        return {
            .id = std::move(id),
            .selector = std::move(selector),
            .kind = std::move(kind),
            .enabled = enabled,
            .overrides = std::move(overrides),
        };
    }

    [[nodiscard]] BindingConfiguration shortcutBinding(
        QString id = QStringLiteral("binding-a"),
        QString key = QStringLiteral("F7"),
        QString submap = {},
        const bool enabled = true
    ) const
    {
        return {
            .id = std::move(id),
            .modifiers = {QStringLiteral("super")},
            .key = std::move(key),
            .actionType = BindingActionType::Dispatcher,
            .action = QStringLiteral("window.close"),
            .arguments = {},
            .description = QStringLiteral("Close the focused window"),
            .enabled = enabled,
            .submap = std::move(submap),
        };
    }

    [[nodiscard]] SubmapConfiguration shortcutSubmap(
        QString id = QStringLiteral("submap-a"),
        QString name = QStringLiteral("resize"),
        QString reset = {},
        const bool enabled = true
    ) const
    {
        return {
            .id = std::move(id),
            .name = std::move(name),
            .reset = std::move(reset),
            .enabled = enabled,
        };
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

    void activeV1TransactionBytesMatchDormantRecoveryCodecs()
    {
        StoreFixture fixture;
        QVERIFY(fixture.temporary.isValid());
        auto authority = transaction(fixture.paths);
        const auto initialized = authority->initialize();
        QVERIFY2(initialized.success, qPrintable(initialized.errorMessage));

        const auto prepared = authority->prepareApply(
            0, QString::fromLatin1(nonceA), fixedTime()
        );
        QVERIFY2(prepared.success, qPrintable(prepared.errorMessage));
        QVERIFY(prepared.prepared);

        const auto activePendingBytes = readBytes(
            fixture.paths.pendingPath()
        );
        const auto recoveredPending =
            parseLegacyOrdinaryPendingRecordV1(
                activePendingBytes, catalog, actionCatalog
            );
        QVERIFY(recoveredPending);
        const auto recoveredPendingBytes =
            serializeLegacyOrdinaryPendingRecordV1(
                *recoveredPending, catalog, actionCatalog
            );
        QVERIFY(recoveredPendingBytes);
        QCOMPARE(*recoveredPendingBytes, activePendingBytes);

        const auto committed = authority->commitApply(prepared.prepared->id);
        QVERIFY2(committed.success, qPrintable(committed.errorMessage));
        const auto activeAppliedBytes = readBytes(
            fixture.paths.activationPath()
        );
        const auto recoveredApplied = parseLegacyAppliedRecordV1(
            activeAppliedBytes
        );
        QVERIFY(recoveredApplied);
        const auto recoveredAppliedBytes = serializeLegacyAppliedRecordV1(
            *recoveredApplied
        );
        QVERIFY(recoveredAppliedBytes);
        QCOMPARE(*recoveredAppliedBytes, activeAppliedBytes);
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

    void preSharedSpacingAuthorityMigrationAcceptsOnlyCanonicalStoreBytes()
    {
        const auto shipped = readBytes(QStringLiteral(
            HYPRSHELLD_HYPRLAND_PRE_SHARED_SPACING_DEFAULT_FILE
        ));
        QCOMPARE(
            sha256(shipped),
            QStringLiteral(
                "dd4a11be44154351601f23816fd521043af60df1bfc58d57bc591a9dc4143d03"
            )
        );
        const auto stored = preSharedSpacingStoredBytes();
        QCOMPARE(stored.size(), qsizetype(431));
        QCOMPARE(
            sha256(stored),
            QStringLiteral(
                "317865343b87292b140b2e1cd92a948c285095ed62ab86f258651b681ba5caeb"
            )
        );

        {
            StoreFixture fixture;
            QVERIFY(fixture.temporary.isValid());
            QVERIFY(makeDirectory(fixture.paths.stateRoot));
            QVERIFY(writeNew(fixture.paths.desiredPath(), shipped));
            auto authority = transaction(fixture.paths);
            const auto rejected = authority->initialize();
            QVERIFY(!rejected.success);
            QVERIFY(!rejected.snapshot.available);
            QCOMPARE(readBytes(fixture.paths.desiredPath()), shipped);
        }

        StoreFixture fixture;
        QVERIFY(fixture.temporary.isValid());
        QVERIFY(makeDirectory(fixture.paths.stateRoot));
        QVERIFY(writeNew(fixture.paths.desiredPath(), stored));
        auto authority = transaction(fixture.paths);
        const auto initialized = authority->initialize();
        QVERIFY2(initialized.success, qPrintable(initialized.errorMessage));
        QCOMPARE(initialized.snapshot.revision, quint64(0));
        QCOMPARE(
            initialized.snapshot.actionCatalogDigest,
            defaults.actionCatalogDigest
        );
        QCOMPARE(initialized.snapshot.applyState, QStringLiteral("inactive"));
        const auto expected = currentAuthorityBytes(stored);
        QCOMPARE(initialized.snapshot.desiredState, expected);
        QCOMPARE(readBytes(fixture.paths.desiredPath()), expected);
        const auto migrated = parseDesiredState(
            QByteArrayView(expected), catalog, actionCatalog
        );
        QVERIFY2(migrated, qPrintable(describeErrors(migrated.errors)));
        QVERIFY(migrated.value->workspaceRules.isEmpty());

        authority.reset();
        auto restarted = transaction(fixture.paths);
        const auto reloaded = restarted->initialize();
        QVERIFY2(reloaded.success, qPrintable(reloaded.errorMessage));
        QCOMPARE(reloaded.snapshot.desiredState, expected);
        QCOMPARE(readBytes(fixture.paths.desiredPath()), expected);
    }

    void preBindingsQuarantineAuthorityMigrationAcceptsOnlyCanonicalStoreBytes()
    {
        const auto shipped = readBytes(QStringLiteral(
            HYPRSHELLD_HYPRLAND_PRE_BINDINGS_QUARANTINE_DEFAULT_FILE
        ));
        QCOMPARE(
            sha256(shipped),
            QStringLiteral(
                "47b5a54029c19991a6c8717e340b42b5a90284f1d907b619cdd9325198ddec9a"
            )
        );
        const auto stored = preBindingsQuarantineStoredBytes();
        QCOMPARE(stored.size(), qsizetype(614));
        QCOMPARE(
            sha256(stored),
            QStringLiteral(
                "388f2effb918728ecc181025434517f07c0a7420aca6d3fae602442dff66c3d0"
            )
        );

        {
            StoreFixture fixture;
            QVERIFY(fixture.temporary.isValid());
            QVERIFY(makeDirectory(fixture.paths.stateRoot));
            QVERIFY(writeNew(fixture.paths.desiredPath(), shipped));
            auto authority = transaction(fixture.paths);
            const auto rejected = authority->initialize();
            QVERIFY(!rejected.success);
            QVERIFY(!rejected.snapshot.available);
            QCOMPARE(readBytes(fixture.paths.desiredPath()), shipped);
        }

        StoreFixture fixture;
        QVERIFY(fixture.temporary.isValid());
        QVERIFY(makeDirectory(fixture.paths.stateRoot));
        QVERIFY(writeNew(fixture.paths.desiredPath(), stored));
        auto authority = transaction(fixture.paths);
        const auto initialized = authority->initialize();
        QVERIFY2(initialized.success, qPrintable(initialized.errorMessage));
        QCOMPARE(initialized.snapshot.revision, quint64(0));
        QCOMPARE(initialized.snapshot.catalogDigest, defaults.catalogDigest);
        QCOMPARE(
            initialized.snapshot.actionCatalogDigest,
            defaults.actionCatalogDigest
        );
        QCOMPARE(initialized.snapshot.applyState, QStringLiteral("inactive"));
        const auto expected = currentAuthorityBytes(stored);
        QCOMPARE(initialized.snapshot.desiredState, expected);
        QCOMPARE(readBytes(fixture.paths.desiredPath()), expected);
        const auto migrated = parseDesiredState(
            QByteArrayView(expected), catalog, actionCatalog
        );
        QVERIFY2(migrated, qPrintable(describeErrors(migrated.errors)));

        authority.reset();
        auto restarted = transaction(fixture.paths);
        const auto reloaded = restarted->initialize();
        QVERIFY2(reloaded.success, qPrintable(reloaded.errorMessage));
        QCOMPARE(reloaded.snapshot.desiredState, expected);
        QCOMPARE(readBytes(fixture.paths.desiredPath()), expected);
    }

    void catalogAndActionAuthorityMigrationAcceptsExactCrossProduct()
    {
        struct Row final {
            QString label;
            bool currentCatalog = false;
            bool currentActionCatalog = false;
        };
        const QVector<Row> rows{
            {QStringLiteral("predecessor catalog and action"), false, false},
            {QStringLiteral("predecessor catalog"), false, true},
            {QStringLiteral("predecessor action"), true, false},
            {QStringLiteral("current catalog and action"), true, true},
        };
        const auto predecessor = preSharedSpacingStoredBytes();

        for (const auto &row : rows) {
            const auto stored = authorityTupleBytes(
                predecessor,
                row.currentCatalog,
                row.currentActionCatalog
            );
            const auto expected = currentAuthorityBytes(stored);
            StoreFixture fixture;
            QVERIFY2(fixture.temporary.isValid(), qPrintable(row.label));
            QVERIFY2(makeDirectory(fixture.paths.stateRoot), qPrintable(row.label));
            QVERIFY2(
                writeNew(fixture.paths.desiredPath(), stored),
                qPrintable(row.label)
            );

            auto authority = transaction(fixture.paths);
            const auto initialized = authority->initialize();
            QVERIFY2(initialized.success, qPrintable(
                row.label + QLatin1String(": ") + initialized.errorMessage
            ));
            QCOMPARE(initialized.snapshot.revision, quint64(0));
            QCOMPARE(initialized.snapshot.catalogDigest, defaults.catalogDigest);
            QCOMPARE(
                initialized.snapshot.actionCatalogDigest,
                defaults.actionCatalogDigest
            );
            QVERIFY2(
                initialized.snapshot.desiredState == expected,
                qPrintable(row.label)
            );
            QVERIFY2(
                readBytes(fixture.paths.desiredPath()) == expected,
                qPrintable(row.label)
            );
            const auto parsed = parseDesiredState(
                QByteArrayView(expected), catalog, actionCatalog
            );
            QVERIFY2(parsed, qPrintable(
                row.label + QLatin1String(": ") + describeErrors(parsed.errors)
            ));

            StoreFixture noncanonicalFixture;
            QVERIFY2(
                noncanonicalFixture.temporary.isValid(), qPrintable(row.label)
            );
            QVERIFY2(
                makeDirectory(noncanonicalFixture.paths.stateRoot),
                qPrintable(row.label)
            );
            const auto noncanonical = QJsonDocument(
                objectFromBytes(stored)
            ).toJson(QJsonDocument::Indented);
            QVERIFY2(noncanonical != stored, qPrintable(row.label));
            QVERIFY2(
                writeNew(
                    noncanonicalFixture.paths.desiredPath(), noncanonical
                ),
                qPrintable(row.label)
            );
            auto noncanonicalAuthority = transaction(
                noncanonicalFixture.paths
            );
            const auto rejected = noncanonicalAuthority->initialize();
            QVERIFY2(!rejected.success, qPrintable(row.label));
            QVERIFY2(!rejected.snapshot.available, qPrintable(row.label));
            QCOMPARE(
                readBytes(noncanonicalFixture.paths.desiredPath()),
                noncanonical
            );
        }
    }

    void intermediateCatalogAndPreSharedActionMigrateWhenSemanticallyCanonical()
    {
        auto object = objectFromBytes(preSharedSpacingStoredBytes());
        object.insert(
            QStringLiteral("catalogDigest"),
            QStringLiteral(
                "10d6c8bfd757407e93ebb379afb7f29df89dacd2a75936ab3d2cbe873d539388"
            )
        );
        const auto stored = canonicalObject(object);
        const auto expected = currentAuthorityBytes(stored);

        StoreFixture fixture;
        QVERIFY(fixture.temporary.isValid());
        QVERIFY(makeDirectory(fixture.paths.stateRoot));
        QVERIFY(writeNew(fixture.paths.desiredPath(), stored));
        auto authority = transaction(fixture.paths);
        const auto initialized = authority->initialize();
        QVERIFY2(initialized.success, qPrintable(initialized.errorMessage));
        QCOMPARE(initialized.snapshot.desiredState, expected);
        QCOMPARE(initialized.snapshot.catalogDigest, defaults.catalogDigest);
        QCOMPARE(
            initialized.snapshot.actionCatalogDigest,
            defaults.actionCatalogDigest
        );
        QCOMPARE(readBytes(fixture.paths.desiredPath()), expected);
    }

    void preSharedSpacingMigrationPreservesRevisionAndUnrelatedState()
    {
        auto object = objectFromBytes(preSharedSpacingStoredBytes());
        object.insert(QStringLiteral("revision"), QStringLiteral("19"));
        object.insert(
            QStringLiteral("overrides"),
            QJsonObject{
                {QStringLiteral("hyprland.general.gaps_in"),
                 QJsonArray{1, 2, 3, 4}},
                {QStringLiteral("hyprland.input.sensitivity"), 0.375},
            }
        );
        object.insert(
            QStringLiteral("environment"),
            QJsonArray{QJsonObject{
                {QStringLiteral("id"), QStringLiteral("desktop")},
                {QStringLiteral("name"),
                 QStringLiteral("XDG_CURRENT_DESKTOP")},
                {QStringLiteral("value"), QStringLiteral("Hyprland")},
                {QStringLiteral("scope"), QStringLiteral("hyprland")},
            }}
        );
        const auto legacy = canonicalObject(object);
        const auto expected = currentAuthorityBytes(legacy);

        StoreFixture fixture;
        QVERIFY(fixture.temporary.isValid());
        QVERIFY(makeDirectory(fixture.paths.stateRoot));
        QVERIFY(writeNew(fixture.paths.desiredPath(), legacy));
        auto authority = transaction(fixture.paths);
        const auto initialized = authority->initialize();
        QVERIFY2(initialized.success, qPrintable(initialized.errorMessage));
        QCOMPARE(initialized.snapshot.revision, quint64(19));
        QCOMPARE(initialized.snapshot.desiredState, expected);
        auto expectedObject = object;
        expectedObject.insert(
            QStringLiteral("catalogDigest"), defaults.catalogDigest
        );
        expectedObject.insert(
            QStringLiteral("actionCatalogDigest"),
            defaults.actionCatalogDigest
        );
        QCOMPARE(objectFromBytes(initialized.snapshot.desiredState), expectedObject);
        QVERIFY(!QFileInfo::exists(fixture.paths.lastGoodPath()));
        QVERIFY(!QFileInfo::exists(fixture.paths.activationPath()));
        QVERIFY(!QFileInfo::exists(fixture.paths.pendingPath()));
    }

    void preSharedSpacingAppliedProofRemainsImmutableAndSemanticallyCurrent()
    {
        struct Row final {
            QString label;
            bool currentCatalog = false;
            bool currentActionCatalog = false;
        };
        const QVector<Row> rows{
            {QStringLiteral("predecessor catalog and action"), false, false},
            {QStringLiteral("predecessor catalog"), false, true},
            {QStringLiteral("predecessor action"), true, false},
            {QStringLiteral("current catalog and action"), true, true},
        };
        const auto predecessor = preSharedSpacingStoredBytes();

        for (const auto &row : rows) {
            StoreFixture fixture;
            QVERIFY2(fixture.temporary.isValid(), qPrintable(row.label));
            const auto stored = authorityTupleBytes(
                predecessor,
                row.currentCatalog,
                row.currentActionCatalog
            );
            LegacyGenerationFixture proof;
            QVERIFY2(
                installLegacyApplied(
                    fixture, stored, QString::fromLatin1(nonceA), proof
                ),
                qPrintable(row.label)
            );
            const auto storedLastGood = readBytes(
                fixture.paths.lastGoodPath()
            );
            const auto storedActivation = readBytes(
                fixture.paths.activationPath()
            );

            auto authority = transaction(fixture.paths);
            const auto initialized = authority->initialize();
            QVERIFY2(initialized.success, qPrintable(
                row.label + QLatin1String(": ") + initialized.errorMessage
            ));
            QCOMPARE(initialized.snapshot.revision, quint64(0));
            QCOMPARE(initialized.snapshot.appliedRevision, quint64(0));
            QCOMPARE(initialized.snapshot.generationDigest, proof.generation);
            QCOMPARE(
                initialized.snapshot.applyState, QStringLiteral("current")
            );
            QCOMPARE(
                initialized.snapshot.desiredState,
                currentAuthorityBytes(stored)
            );
            QCOMPARE(
                initialized.snapshot.appliedDesiredState,
                currentAuthorityBytes(stored)
            );
            QCOMPARE(
                readBytes(fixture.paths.lastGoodPath()), storedLastGood
            );
            QCOMPARE(
                readBytes(fixture.paths.activationPath()), storedActivation
            );
            QVERIFY(!QFileInfo::exists(fixture.paths.pendingPath()));

            authority.reset();
            auto restarted = transaction(fixture.paths);
            const auto reloaded = restarted->initialize();
            QVERIFY2(reloaded.success, qPrintable(
                row.label + QLatin1String(": ") + reloaded.errorMessage
            ));
            QCOMPARE(
                reloaded.snapshot.applyState, QStringLiteral("current")
            );
            QCOMPARE(
                reloaded.snapshot.appliedDesiredState,
                currentAuthorityBytes(stored)
            );
            QCOMPARE(
                readBytes(fixture.paths.lastGoodPath()), storedLastGood
            );
            QCOMPARE(
                readBytes(fixture.paths.activationPath()), storedActivation
            );
        }
    }

    void preBindingsQuarantineAppliedProofRemainsImmutableAndSemanticallyCurrent()
    {
        StoreFixture fixture;
        QVERIFY(fixture.temporary.isValid());
        const auto stored = preBindingsQuarantineStoredBytes();
        LegacyGenerationFixture proof;
        QVERIFY(installLegacyApplied(
            fixture, stored, QString::fromLatin1(nonceA), proof
        ));
        const auto storedLastGood = readBytes(fixture.paths.lastGoodPath());
        const auto storedActivation = readBytes(fixture.paths.activationPath());

        auto authority = transaction(fixture.paths);
        const auto initialized = authority->initialize();
        QVERIFY2(initialized.success, qPrintable(initialized.errorMessage));
        QCOMPARE(initialized.snapshot.revision, quint64(0));
        QCOMPARE(initialized.snapshot.appliedRevision, quint64(0));
        QCOMPARE(initialized.snapshot.generationDigest, proof.generation);
        QCOMPARE(initialized.snapshot.applyState, QStringLiteral("current"));
        QCOMPARE(
            initialized.snapshot.desiredState,
            currentAuthorityBytes(stored)
        );
        QCOMPARE(
            initialized.snapshot.appliedDesiredState,
            currentAuthorityBytes(stored)
        );
        QCOMPARE(readBytes(fixture.paths.lastGoodPath()), storedLastGood);
        QCOMPARE(readBytes(fixture.paths.activationPath()), storedActivation);
        QVERIFY(!QFileInfo::exists(fixture.paths.pendingPath()));

        authority.reset();
        auto restarted = transaction(fixture.paths);
        const auto reloaded = restarted->initialize();
        QVERIFY2(reloaded.success, qPrintable(reloaded.errorMessage));
        QCOMPARE(reloaded.snapshot.applyState, QStringLiteral("current"));
        QCOMPARE(
            reloaded.snapshot.appliedDesiredState,
            currentAuthorityBytes(stored)
        );
        QCOMPARE(readBytes(fixture.paths.lastGoodPath()), storedLastGood);
        QCOMPARE(readBytes(fixture.paths.activationPath()), storedActivation);
    }

    void dirtyPredecessorDesiredOverLegacyAppliedRemainsRetainedAndRecovers()
    {
        StoreFixture fixture;
        QVERIFY(fixture.temporary.isValid());
        const auto legacyApplied = preSharedSpacingStoredBytes();
        LegacyGenerationFixture proof;
        QVERIFY(installLegacyApplied(
            fixture, legacyApplied, QString::fromLatin1(nonceA), proof
        ));
        const auto legacyLastGood = readBytes(fixture.paths.lastGoodPath());
        const auto legacyActivation = readBytes(fixture.paths.activationPath());

        auto dirtyObject = objectFromBytes(legacyApplied);
        dirtyObject.insert(QStringLiteral("revision"), QStringLiteral("1"));
        dirtyObject.insert(
            QStringLiteral("permissions"),
            QJsonArray{QJsonObject{
                {QStringLiteral("id"), QStringLiteral("portal-copy")},
                {QStringLiteral("binary"),
                 QStringLiteral("xdg-desktop-portal")},
                {QStringLiteral("type"), QStringLiteral("screencopy")},
                {QStringLiteral("mode"), QStringLiteral("ask")},
            }}
        );
        const auto dirtyLegacy = canonicalObject(dirtyObject);
        QVERIFY(replaceBytes(fixture.paths.desiredPath(), dirtyLegacy));

        auto authority = transaction(fixture.paths);
        const auto initialized = authority->initialize();
        QVERIFY2(initialized.success, qPrintable(initialized.errorMessage));
        QCOMPARE(initialized.snapshot.revision, quint64(1));
        QCOMPARE(initialized.snapshot.appliedRevision, quint64(0));
        QCOMPARE(initialized.snapshot.applyState, QStringLiteral("retained"));
        QCOMPARE(
            initialized.snapshot.desiredState,
            currentAuthorityBytes(dirtyLegacy)
        );
        QCOMPARE(
            initialized.snapshot.appliedDesiredState,
            currentAuthorityBytes(legacyApplied)
        );
        QCOMPARE(readBytes(fixture.paths.lastGoodPath()), legacyLastGood);
        QCOMPARE(readBytes(fixture.paths.activationPath()), legacyActivation);
        QVERIFY(!QFileInfo::exists(fixture.paths.pendingPath()));

        const auto recovery = authority->prepareRecovery(
            1, QString::fromLatin1(nonceB), fixedTime(1)
        );
        QVERIFY2(recovery.success, qPrintable(recovery.errorMessage));
        QVERIFY(recovery.prepared.has_value());
        QCOMPARE(recovery.prepared->revision, quint64(2));
        const auto committed = authority->commitApply(recovery.prepared->id);
        QVERIFY2(committed.success, qPrintable(committed.errorMessage));
        QCOMPARE(committed.snapshot.revision, quint64(2));
        QCOMPARE(committed.snapshot.applyState, QStringLiteral("current"));
        QCOMPARE(
            committed.snapshot.appliedDesiredState,
            committed.snapshot.desiredState
        );
        const auto recovered = parseDesiredState(
            QByteArrayView(committed.snapshot.desiredState),
            catalog,
            actionCatalog
        );
        QVERIFY2(recovered, qPrintable(describeErrors(recovered.errors)));
        QCOMPARE(recovered.value->actionCatalogDigest, defaults.actionCatalogDigest);
        QVERIFY(recovered.value->workspaceRules.isEmpty());
        QVERIFY(recovered.value->permissions.isEmpty());
        QVERIFY(readBytes(fixture.paths.lastGoodPath()) != legacyLastGood);
        QVERIFY(readBytes(fixture.paths.activationPath()) != legacyActivation);

        authority.reset();
        auto restarted = transaction(fixture.paths);
        const auto reloaded = restarted->initialize();
        QVERIFY2(reloaded.success, qPrintable(reloaded.errorMessage));
        QCOMPARE(reloaded.snapshot.applyState, QStringLiteral("current"));
        QCOMPARE(
            reloaded.snapshot.appliedDesiredState,
            committed.snapshot.desiredState
        );
    }

    void legacyPreparedAndCommittingPendingTransactionsReconcileExactly()
    {
        struct Row final {
            QString label;
            bool currentCatalog = false;
            bool currentActionCatalog = false;
        };
        const QVector<Row> rows{
            {QStringLiteral("predecessor catalog and action"), false, false},
            {QStringLiteral("predecessor catalog"), false, true},
            {QStringLiteral("predecessor action"), true, false},
            {QStringLiteral("current catalog and action"), true, true},
        };
        const auto predecessor = preSharedSpacingStoredBytes();

        for (const auto &row : rows) {
          for (const auto &phase : {
                   QStringLiteral("prepared"),
                   QStringLiteral("committing"),
               }) {
            StoreFixture fixture;
            const auto label = row.label + QLatin1Char(' ') + phase;
            QVERIFY2(fixture.temporary.isValid(), qPrintable(label));
            const auto stored = authorityTupleBytes(
                predecessor,
                row.currentCatalog,
                row.currentActionCatalog
            );
            LegacyGenerationFixture staged;
            QVERIFY2(
                stageLegacyGeneration(
                    fixture,
                    stored,
                    QString::fromLatin1(nonceA),
                    staged
                ),
                qPrintable(label)
            );
            auto pending = objectFromBytes(staged.pendingBytes);
            pending.insert(QStringLiteral("phase"), phase);
            QVERIFY(replaceBytes(
                fixture.paths.pendingPath(), canonicalObject(pending)
            ));

            auto restarted = transaction(fixture.paths);
            const auto initialized = restarted->initialize();
            QVERIFY2(initialized.success, qPrintable(
                label + QLatin1String(": ") + initialized.errorMessage
            ));
            QCOMPARE(initialized.snapshot.loadState, QStringLiteral("recovered"));
            QCOMPARE(
                initialized.snapshot.desiredState,
                currentAuthorityBytes(stored)
            );
            QVERIFY(!QFileInfo::exists(fixture.paths.pendingPath()));
            if (phase == QStringLiteral("prepared")) {
                QCOMPARE(
                    initialized.snapshot.applyState,
                    QStringLiteral("inactive")
                );
                QVERIFY(!QFileInfo::exists(fixture.paths.lastGoodPath()));
                QVERIFY(!QFileInfo::exists(fixture.paths.activationPath()));
            } else {
                QCOMPARE(
                    initialized.snapshot.applyState,
                    QStringLiteral("current")
                );
                QCOMPARE(
                    readBytes(fixture.paths.lastGoodPath()),
                    stored
                );
                QCOMPARE(
                    readBytes(fixture.paths.activationPath()),
                    staged.activationBytes
                );
            }
          }
        }
    }

    void preBindingsQuarantinePendingTransactionsReconcileExactly()
    {
        const auto stored = preBindingsQuarantineStoredBytes();
        for (const auto &phase : {
                 QStringLiteral("prepared"),
                 QStringLiteral("committing"),
             }) {
            StoreFixture fixture;
            QVERIFY2(fixture.temporary.isValid(), qPrintable(phase));
            LegacyGenerationFixture staged;
            QVERIFY2(
                stageLegacyGeneration(
                    fixture,
                    stored,
                    QString::fromLatin1(nonceA),
                    staged
                ),
                qPrintable(phase)
            );
            auto pending = objectFromBytes(staged.pendingBytes);
            pending.insert(QStringLiteral("phase"), phase);
            QVERIFY(replaceBytes(
                fixture.paths.pendingPath(), canonicalObject(pending)
            ));

            auto restarted = transaction(fixture.paths);
            const auto initialized = restarted->initialize();
            QVERIFY2(initialized.success, qPrintable(
                phase + QLatin1String(": ") + initialized.errorMessage
            ));
            QCOMPARE(initialized.snapshot.loadState, QStringLiteral("recovered"));
            QCOMPARE(
                initialized.snapshot.desiredState,
                currentAuthorityBytes(stored)
            );
            QVERIFY(!QFileInfo::exists(fixture.paths.pendingPath()));
            if (phase == QStringLiteral("prepared")) {
                QCOMPARE(
                    initialized.snapshot.applyState,
                    QStringLiteral("inactive")
                );
                QVERIFY(!QFileInfo::exists(fixture.paths.lastGoodPath()));
                QVERIFY(!QFileInfo::exists(fixture.paths.activationPath()));
            } else {
                QCOMPARE(
                    initialized.snapshot.applyState,
                    QStringLiteral("current")
                );
                QCOMPARE(readBytes(fixture.paths.lastGoodPath()), stored);
                QCOMPARE(
                    readBytes(fixture.paths.activationPath()),
                    staged.activationBytes
                );
            }
        }
    }

    void incompatiblePredecessorAuthorityAndProtectedCollisionsFailClosed()
    {
        struct Row final {
            QString label;
            QByteArray bytes;
        };
        const auto base = objectFromBytes(preSharedSpacingStoredBytes());
        QVector<Row> rows;

        auto unknownAction = base;
        unknownAction.insert(
            QStringLiteral("actionCatalogDigest"),
            QString(64, QLatin1Char('0'))
        );
        rows.append({
            QStringLiteral("unknown action digest"),
            canonicalObject(unknownAction),
        });

        auto unknownCatalog = base;
        unknownCatalog.insert(
            QStringLiteral("catalogDigest"),
            QString(64, QLatin1Char('0'))
        );
        rows.append({
            QStringLiteral("unknown catalog digest"),
            canonicalObject(unknownCatalog),
        });

        const auto workspaceRule = [](QString id, QString selector) {
            return QJsonObject{
                {QStringLiteral("id"), std::move(id)},
                {QStringLiteral("selector"), std::move(selector)},
                {QStringLiteral("enabled"), true},
                {QStringLiteral("monitor"), QString()},
                {QStringLiteral("persistent"), false},
                {QStringLiteral("isDefault"), false},
                {QStringLiteral("layout"), QString()},
                {QStringLiteral("overrides"), QJsonObject{}},
            };
        };
        auto reservedId = base;
        reservedId.insert(
            QStringLiteral("workspaceRules"),
            QJsonArray{workspaceRule(
                QStringLiteral("hyprshelld.internal.shared-spacing.maximized"),
                QStringLiteral("1")
            )}
        );
        rows.append({QStringLiteral("reserved id"), canonicalObject(reservedId)});

        auto reservedSelector = base;
        reservedSelector.insert(
            QStringLiteral("workspaceRules"),
            QJsonArray{workspaceRule(
                QStringLiteral("user-maximized"), QStringLiteral("f[1]")
            )}
        );
        rows.append({
            QStringLiteral("reserved selector"),
            canonicalObject(reservedSelector),
        });
        auto intermediateWithPreSharedAction = objectFromBytes(
            preBindingsQuarantineStoredBytes()
        );
        intermediateWithPreSharedAction.insert(
            QStringLiteral("actionCatalogDigest"),
            QStringLiteral(
                "72e063a5476308cefdd2771367ffeed9a8e553b3a2d7141f4e08c3e105a5deb2"
            )
        );
        rows.append({
            QStringLiteral("intermediate catalog with protected pre-shared rule"),
            canonicalObject(intermediateWithPreSharedAction),
        });
        rows.append({
            QStringLiteral("malformed"), QByteArrayLiteral("{broken\n")
        });

        for (const auto &row : rows) {
            StoreFixture fixture;
            QVERIFY2(fixture.temporary.isValid(), qPrintable(row.label));
            QVERIFY(makeDirectory(fixture.paths.stateRoot));
            QVERIFY(writeNew(fixture.paths.desiredPath(), row.bytes));
            auto authority = transaction(fixture.paths);
            const auto rejected = authority->initialize();
            QVERIFY2(!rejected.success, qPrintable(row.label));
            QVERIFY2(!rejected.snapshot.available, qPrintable(row.label));
            QCOMPARE(readBytes(fixture.paths.desiredPath()), row.bytes);
            QVERIFY(!QFileInfo::exists(fixture.paths.lastGoodPath()));
            QVERIFY(!QFileInfo::exists(fixture.paths.activationPath()));
            QVERIFY(!QFileInfo::exists(fixture.paths.pendingPath()));
        }

        auto exhausted = base;
        exhausted.insert(
            QStringLiteral("revision"),
            QString::number(std::numeric_limits<quint64>::max())
        );
        const auto exhaustedLegacy = canonicalObject(exhausted);
        StoreFixture fixture;
        QVERIFY(fixture.temporary.isValid());
        QVERIFY(makeDirectory(fixture.paths.stateRoot));
        QVERIFY(writeNew(fixture.paths.desiredPath(), exhaustedLegacy));
        auto authority = transaction(fixture.paths);
        const auto migrated = authority->initialize();
        QVERIFY2(migrated.success, qPrintable(migrated.errorMessage));
        QCOMPARE(
            migrated.snapshot.revision,
            std::numeric_limits<quint64>::max()
        );
        auto changed = objectFromBytes(migrated.snapshot.desiredState);
        changed.insert(
            QStringLiteral("overrides"),
            QJsonObject{{
                QStringLiteral("hyprland.general.border_size"), 2
            }}
        );
        const auto rejected = authority->replaceSnapshot(
            std::numeric_limits<quint64>::max(), canonicalObject(changed)
        );
        QVERIFY(!rejected.success);
        QCOMPARE(rejected.errorCode, QStringLiteral("RevisionExhausted"));
        QCOMPARE(
            readBytes(fixture.paths.desiredPath()),
            currentAuthorityBytes(exhaustedLegacy)
        );
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

    void unsafeGlowStartupRemainsRepairableAndReplaceKeepsPrecedence()
    {
        StoreFixture fixture;
        QVERIFY(fixture.temporary.isValid());
        QVERIFY(makeDirectory(fixture.paths.stateRoot));
        const auto unsafe = glowState(1, true, 9);
        const auto unsafeBytes = serializeDesiredState(unsafe);
        QVERIFY(writeNew(fixture.paths.desiredPath(), unsafeBytes));

        auto authority = transaction(fixture.paths);
        const auto initialized = authority->initialize();
        QVERIFY2(initialized.success, qPrintable(initialized.errorMessage));
        QCOMPARE(initialized.snapshot.desiredState, unsafeBytes);
        const auto startupErrors = authority->currentActivationSafetyErrors();
        QCOMPARE(startupErrors.size(), qsizetype(1));
        QCOMPARE(
            startupErrors.constFirst().code,
            QStringLiteral("state.unsafe-glow-range")
        );

        const auto originalIdentity = metadata(fixture.paths.desiredPath());
        const auto noOp = authority->replaceSnapshot(1, unsafeBytes);
        QVERIFY2(noOp.success, qPrintable(noOp.errorMessage));
        QCOMPARE(noOp.snapshot.revision, quint64(1));
        QVERIFY(sameIdentity(
            originalIdentity, metadata(fixture.paths.desiredPath())
        ));

        auto priorTokenCandidate = unsafe;
        priorTokenCandidate.revision = 0;
        const auto retry = authority->replaceSnapshot(
            0, serializeDesiredState(priorTokenCandidate)
        );
        QVERIFY2(retry.success, qPrintable(retry.errorMessage));
        QCOMPARE(retry.snapshot.revision, quint64(1));
        QVERIFY(sameIdentity(
            originalIdentity, metadata(fixture.paths.desiredPath())
        ));

        auto changedUnsafe = unsafe;
        changedUnsafe.overrides.insert(
            QStringLiteral("hyprland.general.border_size"), 2
        );
        const auto rejected = authority->replaceSnapshot(
            1, serializeDesiredState(changedUnsafe)
        );
        QVERIFY(!rejected.success);
        QCOMPARE(rejected.errorCode, QStringLiteral("InvalidSnapshot"));
        QVERIFY(rejected.errorMessage.contains(
            QStringLiteral("state.unsafe-glow-range")
        ));
        QVERIFY(rejected.errorMessage.contains(QStringLiteral(
            "Inner glow can be enabled only when its range is at least 10; "
            "disable glow or raise the range."
        )));
        QCOMPARE(readBytes(fixture.paths.desiredPath()), unsafeBytes);
        QVERIFY(sameIdentity(
            originalIdentity, metadata(fixture.paths.desiredPath())
        ));
        QVERIFY(!QFileInfo::exists(fixture.paths.pendingPath()));

        auto repaired = unsafe;
        repaired.overrides.remove(
            QStringLiteral("hyprland.decoration.glow.enabled")
        );
        const auto saved = authority->replaceSnapshot(
            1, serializeDesiredState(repaired)
        );
        QVERIFY2(saved.success, qPrintable(saved.errorMessage));
        QCOMPARE(saved.snapshot.revision, quint64(2));
        QVERIFY(authority->currentActivationSafetyErrors().isEmpty());

        StoreFixture rangeFixture;
        QVERIFY(rangeFixture.temporary.isValid());
        QVERIFY(makeDirectory(rangeFixture.paths.stateRoot));
        const auto lowRange = glowState(1, true, 9);
        QVERIFY(writeNew(
            rangeFixture.paths.desiredPath(), serializeDesiredState(lowRange)
        ));
        auto rangeAuthority = transaction(rangeFixture.paths);
        QVERIFY(rangeAuthority->initialize().success);
        auto raisedRange = lowRange;
        raisedRange.overrides.remove(
            QStringLiteral("hyprland.decoration.glow.range")
        );
        const auto raised = rangeAuthority->replaceSnapshot(
            1, serializeDesiredState(raisedRange)
        );
        QVERIFY2(raised.success, qPrintable(raised.errorMessage));
        QCOMPARE(raised.snapshot.revision, quint64(2));
        QVERIFY(rangeAuthority->currentActivationSafetyErrors().isEmpty());

        StoreFixture exhaustedFixture;
        QVERIFY(exhaustedFixture.temporary.isValid());
        QVERIFY(makeDirectory(exhaustedFixture.paths.stateRoot));
        const auto exhaustedState = glowState(
            std::numeric_limits<quint64>::max(), true, 9
        );
        const auto exhaustedBytes = serializeDesiredState(exhaustedState);
        QVERIFY(writeNew(
            exhaustedFixture.paths.desiredPath(), exhaustedBytes
        ));
        auto exhaustedAuthority = transaction(exhaustedFixture.paths);
        QVERIFY(exhaustedAuthority->initialize().success);
        auto exhaustedCandidate = exhaustedState;
        exhaustedCandidate.overrides.insert(
            QStringLiteral("hyprland.general.border_size"), 2
        );
        const auto exhausted = exhaustedAuthority->replaceSnapshot(
            exhaustedState.revision,
            serializeDesiredState(exhaustedCandidate)
        );
        QVERIFY(!exhausted.success);
        QCOMPARE(exhausted.errorCode, QStringLiteral("RevisionExhausted"));
        QCOMPARE(
            readBytes(exhaustedFixture.paths.desiredPath()), exhaustedBytes
        );
    }

    void previousTokenIdempotencyPrecedesAnInstalledPendingTransaction()
    {
        StoreFixture fixture;
        QVERIFY(fixture.temporary.isValid());
        auto authority = transaction(fixture.paths);
        QVERIFY(initializeAppliedDefault(*authority));

        const auto priorCandidate = serializeDesiredState(permissionState(0));
        const auto replaced = authority->replaceSnapshot(0, priorCandidate);
        QVERIFY2(replaced.success, qPrintable(replaced.errorMessage));
        QCOMPARE(replaced.snapshot.revision, quint64(1));
        const auto committedBytes = readBytes(fixture.paths.desiredPath());
        const auto committedIdentity = metadata(fixture.paths.desiredPath());

        const auto prepared = authority->prepareApply(
            1, QString::fromLatin1(nonceB), fixedTime(1)
        );
        QVERIFY2(prepared.success, qPrintable(prepared.errorMessage));
        QVERIFY(prepared.prepared.has_value());
        QVERIFY(!authority->snapshot().writable);
        QVERIFY(QFileInfo::exists(fixture.paths.pendingPath()));

        const auto exactRetry = authority->replaceSnapshot(0, priorCandidate);
        QVERIFY2(exactRetry.success, qPrintable(exactRetry.errorMessage));
        QCOMPARE(exactRetry.snapshot.revision, quint64(1));
        QCOMPARE(readBytes(fixture.paths.desiredPath()), committedBytes);
        QVERIFY(sameIdentity(
            committedIdentity, metadata(fixture.paths.desiredPath())
        ));
        QVERIFY(QFileInfo::exists(fixture.paths.pendingPath()));

        const auto nonexactRetry = authority->replaceSnapshot(
            0, serializeDesiredState(environmentState(0))
        );
        QVERIFY(!nonexactRetry.success);
        QCOMPARE(nonexactRetry.errorCode, QStringLiteral("StaleRevision"));
        QCOMPARE(readBytes(fixture.paths.desiredPath()), committedBytes);
        QVERIFY(sameIdentity(
            committedIdentity, metadata(fixture.paths.desiredPath())
        ));

        const auto currentMutation = authority->replaceSnapshot(
            1, serializeDesiredState(environmentState(1))
        );
        QVERIFY(!currentMutation.success);
        QCOMPARE(
            currentMutation.errorCode,
            QStringLiteral("ConfirmationPending")
        );
        QCOMPARE(readBytes(fixture.paths.desiredPath()), committedBytes);
        QVERIFY(QFileInfo::exists(fixture.paths.pendingPath()));

        const auto aborted = authority->abortApply(prepared.prepared->id);
        QVERIFY2(aborted.success, qPrintable(aborted.errorMessage));
        QVERIFY(!QFileInfo::exists(fixture.paths.pendingPath()));
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

    void preparedV1PendingBytesMatchRecoveredGoldenDigests()
    {
        enum class CandidateKind {
            Reload,
            Restart,
            Session,
        };
        struct Row final {
            QString label;
            CandidateKind candidateKind = CandidateKind::Reload;
            quint64 expectedRevision = 0;
            ActivationRequirement requirement = ActivationRequirement::Reload;
            qsizetype normalizedSize = 0;
            QString normalizedSha256;
        };

        // These hashes were derived independently from two captures of the
        // untouched qualified hyprshelld-recovery implementation (receipt
        // 14dd8408b3a181b775ac09d17adc17a93162542cb80f217e8de2e3fdb534bda9).
        // The recovered transaction-test binary SHA-256 was
        // c1bdaee0f3c546bac56f7ea9a083919354dbcc27c7aaaf7bca27365ee36600f9.
        // GDB captured the exact Pending write from the recovered binary for
        // prepareAndAbortOnlyTouchPendingAndImmutableGeneration (Reload) and
        // the first prepared writes in
        // deletingRestartAndSessionStateKeepsStrongestRequirement. The two
        // runs used different QTemporaryDir roots and produced the same three
        // hashes after replacing exactly the two proven path-derived values:
        // afterActivation.generation and afterActivation.entrypoint. Every
        // other byte, including canonical framing, remains under the digest.
        const QVector<Row> rows{
            {
                .label = QStringLiteral("reload"),
                .candidateKind = CandidateKind::Reload,
                .expectedRevision = 0,
                .requirement = ActivationRequirement::Reload,
                .normalizedSize = 1274,
                .normalizedSha256 = QStringLiteral(
                    "31ab8ae01865717c75feb0e3c2db336a0afaf1112b7748b1c0fcb849171841b5"
                ),
            },
            {
                .label = QStringLiteral("restart"),
                .candidateKind = CandidateKind::Restart,
                .expectedRevision = 1,
                .requirement = ActivationRequirement::Restart,
                .normalizedSize = 1358,
                .normalizedSha256 = QStringLiteral(
                    "f436a7f91635d26ab9db6147d38b05c93772ec9d752e2a23b05204d282694c0c"
                ),
            },
            {
                .label = QStringLiteral("session"),
                .candidateKind = CandidateKind::Session,
                .expectedRevision = 1,
                .requirement = ActivationRequirement::Session,
                .normalizedSize = 1358,
                .normalizedSha256 = QStringLiteral(
                    "8d3225f4f549df2bab0e863a23bc1354ed90b57d8a41a124d8094f185f5d1274"
                ),
            },
        };
        const QString generationSentinel(64, QLatin1Char('f'));
        const auto entrypointSentinel = QStringLiteral(
            "/__hyprshelld_v1_pending_entrypoint__/hyprland.lua"
        );
        const QRegularExpression canonicalSha256(
            QStringLiteral("^[0-9a-f]{64}$")
        );

        for (const auto &row : rows) {
            StoreFixture fixture;
            QVERIFY2(fixture.temporary.isValid(), qPrintable(row.label));
            auto authority = transaction(fixture.paths);
            const auto initialized = authority->initialize();
            QVERIFY2(initialized.success, qPrintable(
                row.label + QStringLiteral(": ") + initialized.errorMessage
            ));

            if (row.candidateKind != CandidateKind::Reload) {
                const auto candidate = row.candidateKind
                        == CandidateKind::Restart
                    ? permissionState(0)
                    : environmentState(0);
                const auto replaced = authority->replaceSnapshot(
                    0, serializeDesiredState(candidate)
                );
                QVERIFY2(replaced.success, qPrintable(
                    row.label + QStringLiteral(": ") + replaced.errorMessage
                ));
                QCOMPARE(replaced.snapshot.revision, row.expectedRevision);
            }

            const auto prepared = authority->prepareApply(
                row.expectedRevision,
                QString::fromLatin1(nonceA),
                fixedTime()
            );
            QVERIFY2(prepared.success, qPrintable(
                row.label + QStringLiteral(": ") + prepared.errorMessage
            ));
            QVERIFY(prepared.prepared.has_value());
            QCOMPARE(prepared.prepared->revision, row.expectedRevision);
            QCOMPARE(prepared.prepared->requirement, row.requirement);

            const auto durableBytes = readBytes(fixture.paths.pendingPath());
            const auto durableObject = objectFromBytes(durableBytes);
            QVERIFY2(!durableObject.isEmpty(), qPrintable(row.label));
            QCOMPARE(canonicalObject(durableObject), durableBytes);
            QCOMPARE(
                durableBytes.count(QByteArrayLiteral("\"generation\":")), 1
            );
            QCOMPARE(
                durableBytes.count(QByteArrayLiteral("\"entrypoint\":")), 1
            );
            QCOMPARE(
                durableObject.value(QStringLiteral("kind")).toString(),
                QStringLiteral("apply")
            );
            QCOMPARE(
                durableObject.value(QStringLiteral("phase")).toString(),
                QStringLiteral("prepared")
            );
            QCOMPARE(
                durableObject.value(
                    QStringLiteral("expectedRevision")
                ).toString(),
                QString::number(row.expectedRevision)
            );
            QVERIFY(durableObject.value(
                QStringLiteral("beforeActivation")
            ).isNull());
            QCOMPARE(
                canonicalObject(durableObject.value(
                    QStringLiteral("candidateSnapshot")
                ).toObject()),
                prepared.snapshot.desiredState
            );

            auto normalizedObject = durableObject;
            auto afterActivation = normalizedObject.value(
                QStringLiteral("afterActivation")
            ).toObject();
            const auto originalGeneration = afterActivation.value(
                QStringLiteral("generation")
            ).toString();
            QVERIFY2(
                canonicalSha256.match(originalGeneration).hasMatch(),
                qPrintable(row.label)
            );
            QCOMPARE(originalGeneration, prepared.prepared->id);
            const auto expectedEntrypoint = QDir(
                fixture.paths.generationsPath()
            ).filePath(
                QString::fromLatin1(nonceA)
                    + QStringLiteral("/hyprland.lua")
            );
            QCOMPARE(
                afterActivation.value(
                    QStringLiteral("entrypoint")
                ).toString(),
                expectedEntrypoint
            );
            QCOMPARE(
                afterActivation.value(
                    QStringLiteral("requiredActivation")
                ).toString(),
                activationRequirementName(row.requirement)
            );
            afterActivation.insert(
                QStringLiteral("generation"), generationSentinel
            );
            afterActivation.insert(
                QStringLiteral("entrypoint"), entrypointSentinel
            );
            normalizedObject.insert(
                QStringLiteral("afterActivation"), afterActivation
            );
            const auto normalizedBytes = canonicalObject(normalizedObject);
            QCOMPARE(normalizedBytes.size(), row.normalizedSize);
            QCOMPARE(sha256(normalizedBytes), row.normalizedSha256);

            const auto aborted = authority->abortApply(
                prepared.prepared->id
            );
            QVERIFY2(aborted.success, qPrintable(
                row.label + QStringLiteral(": ") + aborted.errorMessage
            ));
        }
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

    void safeRecoveryRepairsUnsafeDesiredState()
    {
        StoreFixture fixture;
        QVERIFY(fixture.temporary.isValid());
        {
            auto authority = transaction(fixture.paths);
            QVERIFY(initializeAppliedDefault(*authority));
        }

        const auto unsafe = glowState(1, true, 9);
        const auto unsafeBytes = serializeDesiredState(unsafe);
        QVERIFY(replaceBytes(fixture.paths.desiredPath(), unsafeBytes));

        auto authority = transaction(fixture.paths);
        const auto initialized = authority->initialize();
        QVERIFY2(initialized.success, qPrintable(initialized.errorMessage));
        QCOMPARE(initialized.snapshot.revision, quint64(1));
        QCOMPARE(initialized.snapshot.desiredState, unsafeBytes);
        QCOMPARE(
            authority->currentActivationSafetyErrors().size(), qsizetype(1)
        );

        const auto apply = authority->prepareApply(
            1, QString::fromLatin1(nonceB), fixedTime(1)
        );
        QVERIFY(!apply.success);
        QCOMPARE(apply.errorCode, QStringLiteral("VerificationFailed"));
        QVERIFY(apply.errorMessage.contains(
            QStringLiteral("state.unsafe-glow-range")
        ));
        QVERIFY(!QFileInfo::exists(fixture.paths.pendingPath()));
        QVERIFY(!QFileInfo::exists(
            QDir(fixture.paths.generationsPath()).filePath(
                QString::fromLatin1(nonceB)
            )
        ));

        const auto recovery = authority->prepareRecovery(
            1, QString::fromLatin1(nonceC), fixedTime(2)
        );
        QVERIFY2(recovery.success, qPrintable(recovery.errorMessage));
        QVERIFY(recovery.prepared.has_value());
        QCOMPARE(recovery.prepared->revision, quint64(2));
        QCOMPARE(readBytes(fixture.paths.desiredPath()), unsafeBytes);

        const auto committed = authority->commitApply(recovery.prepared->id);
        QVERIFY2(committed.success, qPrintable(committed.errorMessage));
        QCOMPARE(committed.snapshot.revision, quint64(2));
        QCOMPARE(committed.snapshot.appliedRevision, quint64(2));
        QCOMPARE(committed.snapshot.applyState, QStringLiteral("current"));
        const auto repaired = parseDesiredState(
            QByteArrayView(committed.snapshot.desiredState),
            catalog,
            actionCatalog
        );
        QVERIFY2(repaired, qPrintable(describeErrors(repaired.errors)));
        QVERIFY(validateManagedActivationSafety(
                    *repaired.value, catalog
                ).isEmpty());
        QVERIFY(!repaired.value->overrides.contains(
            QStringLiteral("hyprland.decoration.glow.enabled")
        ));
        QVERIFY(!repaired.value->overrides.contains(
            QStringLiteral("hyprland.decoration.glow.range")
        ));
    }

    void unsafeAppliedGlowRejectsRecoveryAndDisplayStaging()
    {
        StoreFixture fixture;
        QVERIFY(fixture.temporary.isValid());
        const auto unsafeApplied = glowState(0, true, 9);
        QVERIFY(installAppliedState(
            fixture, unsafeApplied, QString::fromLatin1(nonceA)
        ));

        auto authority = transaction(fixture.paths);
        const auto initialized = authority->initialize();
        QVERIFY2(initialized.success, qPrintable(initialized.errorMessage));
        QCOMPARE(initialized.snapshot.applyState, QStringLiteral("current"));
        QCOMPARE(
            authority->currentActivationSafetyErrors().size(), qsizetype(1)
        );

        const auto recovery = authority->prepareRecovery(
            0, QString::fromLatin1(nonceB), fixedTime(1)
        );
        QVERIFY(!recovery.success);
        QCOMPARE(recovery.errorCode, QStringLiteral("VerificationFailed"));
        QVERIFY(recovery.errorMessage.contains(
            QStringLiteral("state.unsafe-glow-range")
        ));
        QVERIFY(!QFileInfo::exists(fixture.paths.pendingPath()));
        QVERIFY(!QFileInfo::exists(
            QDir(fixture.paths.generationsPath()).filePath(
                QString::fromLatin1(nonceB)
            )
        ));

        const auto topology = oneDisplayTopology();
        const auto display = authority->prepareDisplayApply(
            0,
            oneDisplayProfile(topology),
            topology,
            QString::fromLatin1(nonceC),
            fixedTime(2)
        );
        QVERIFY(!display.success);
        QCOMPARE(display.errorCode, QStringLiteral("VerificationFailed"));
        QVERIFY(display.errorMessage.contains(
            QStringLiteral("state.unsafe-glow-range")
        ));
        QVERIFY(!QFileInfo::exists(fixture.paths.pendingPath()));
        QVERIFY(!QFileInfo::exists(
            QDir(fixture.paths.generationsPath()).filePath(
                QString::fromLatin1(nonceC)
            )
        ));
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

    void deviceCollectionChangesAreBidirectionallyRestartOnly()
    {
        const auto deviceA = inputDevice();
        const auto deviceB = inputDevice(
            QStringLiteral("device-b"),
            QStringLiteral("secondary-pointer"),
            QStringLiteral("pointer")
        );
        const auto requirementAfterApplied = [this](
            const QVector<DeviceConfiguration> &before,
            const QVector<DeviceConfiguration> &after
        ) {
            StoreFixture fixture;
            if (!fixture.temporary.isValid()) {
                return std::optional<ActivationRequirement>{};
            }
            auto authority = transaction(fixture.paths);
            auto baseline = defaults;
            baseline.devices = before;
            auto initialized = authority->initialize();
            if (!initialized.success) {
                return std::optional<ActivationRequirement>{};
            }
            auto replaced = authority->replaceSnapshot(
                0, serializeDesiredState(baseline)
            );
            if (!replaced.success) {
                return std::optional<ActivationRequirement>{};
            }
            const auto baselineRevision = replaced.snapshot.revision;
            auto prepared = authority->prepareApply(
                baselineRevision, QString::fromLatin1(nonceA), fixedTime()
            );
            if (!prepared.success || !prepared.prepared
                || !authority->commitApply(prepared.prepared->id).success) {
                return std::optional<ActivationRequirement>{};
            }
            auto candidate = defaults;
            candidate.revision = baselineRevision;
            candidate.devices = after;
            replaced = authority->replaceSnapshot(
                baselineRevision, serializeDesiredState(candidate)
            );
            return replaced.success ? replaced.snapshot.requiredActivation
                                    : std::nullopt;
        };

        struct Row final {
            QString label;
            QVector<DeviceConfiguration> before;
            QVector<DeviceConfiguration> after;
        };
        QVector<Row> rows;
        const auto appendBothDirections = [&rows](
            QString label,
            QVector<DeviceConfiguration> before,
            QVector<DeviceConfiguration> after
        ) {
            rows.append({label + QStringLiteral(" forward"), before, after});
            rows.append({
                std::move(label) + QStringLiteral(" reverse"),
                std::move(after),
                std::move(before),
            });
        };

        appendBothDirections(
            QStringLiteral("add/remove"), {}, {deviceA}
        );
        appendBothDirections(
            QStringLiteral("second record"), {deviceA}, {deviceA, deviceB}
        );
        appendBothDirections(
            QStringLiteral("reorder"), {deviceA, deviceB}, {deviceB, deviceA}
        );

        auto changed = deviceA;
        changed.id = QStringLiteral("renamed-device-id");
        appendBothDirections(
            QStringLiteral("id"), {deviceA}, {changed}
        );
        changed = deviceA;
        changed.selector = QStringLiteral("replacement-keyboard");
        appendBothDirections(
            QStringLiteral("selector"), {deviceA}, {changed}
        );
        changed = deviceA;
        changed.kind = QStringLiteral("pointer");
        appendBothDirections(
            QStringLiteral("kind"), {deviceA}, {changed}
        );
        changed = deviceA;
        changed.enabled = false;
        appendBothDirections(
            QStringLiteral("enabled"), {deviceA}, {changed}
        );
        changed = deviceA;
        changed.overrides.insert(QStringLiteral("natural_scroll"), true);
        appendBothDirections(
            QStringLiteral("override key"), {deviceA}, {changed}
        );
        changed = deviceA;
        changed.overrides.insert(QStringLiteral("sensitivity"), 0.5);
        appendBothDirections(
            QStringLiteral("override value"), {deviceA}, {changed}
        );

        for (const auto &row : rows) {
            const auto requirement = requirementAfterApplied(
                row.before, row.after
            );
            QVERIFY2(requirement.has_value(), qPrintable(row.label));
            QCOMPARE(*requirement, ActivationRequirement::Restart);
        }

        // A target with no applied baseline cannot establish a rollback-safe
        // live device collection.
        {
            StoreFixture fixture;
            QVERIFY(fixture.temporary.isValid());
            auto authority = transaction(fixture.paths);
            QVERIFY(authority->initialize().success);
            auto candidate = defaults;
            candidate.devices = {deviceA};
            const auto replaced = authority->replaceSnapshot(
                0, serializeDesiredState(candidate)
            );
            QVERIFY2(replaced.success, qPrintable(replaced.errorMessage));
            QCOMPARE(
                *replaced.snapshot.requiredActivation,
                ActivationRequirement::Restart
            );
        }

        // Exact unchanged devices do not elevate an unrelated reloadable
        // scalar delta.
        {
            StoreFixture fixture;
            QVERIFY(fixture.temporary.isValid());
            auto authority = transaction(fixture.paths);
            auto baseline = defaults;
            baseline.devices = {deviceA};
            QVERIFY(authority->initialize().success);
            auto replaced = authority->replaceSnapshot(
                0, serializeDesiredState(baseline)
            );
            QVERIFY(replaced.success);
            auto prepared = authority->prepareApply(
                1, QString::fromLatin1(nonceA), fixedTime()
            );
            QVERIFY(prepared.success);
            QVERIFY(authority->commitApply(prepared.prepared->id).success);

            auto reloadOnly = baseline;
            reloadOnly.revision = 1;
            reloadOnly.overrides.insert(
                QStringLiteral("hyprland.general.border_size"), 2
            );
            replaced = authority->replaceSnapshot(
                1, serializeDesiredState(reloadOnly)
            );
            QVERIFY(replaced.success);
            QCOMPARE(
                *replaced.snapshot.requiredActivation,
                ActivationRequirement::Reload
            );
            prepared = authority->prepareApply(
                2, QString::fromLatin1(nonceB), fixedTime(1)
            );
            QVERIFY(prepared.success);
            QCOMPARE(
                prepared.prepared->requirement,
                ActivationRequirement::Reload
            );
        }

        // Recovery stages the exact applied device vector, so the prepared
        // delta remains Reload even when desired devices diverge.
        {
            StoreFixture fixture;
            QVERIFY(fixture.temporary.isValid());
            auto authority = transaction(fixture.paths);
            auto baseline = defaults;
            baseline.devices = {deviceA};
            QVERIFY(authority->initialize().success);
            auto replaced = authority->replaceSnapshot(
                0, serializeDesiredState(baseline)
            );
            QVERIFY(replaced.success);
            auto prepared = authority->prepareApply(
                1, QString::fromLatin1(nonceA), fixedTime()
            );
            QVERIFY(prepared.success);
            QVERIFY(authority->commitApply(prepared.prepared->id).success);

            auto divergent = baseline;
            divergent.revision = 1;
            divergent.devices = {deviceB};
            replaced = authority->replaceSnapshot(
                1, serializeDesiredState(divergent)
            );
            QVERIFY(replaced.success);
            QCOMPARE(
                *replaced.snapshot.requiredActivation,
                ActivationRequirement::Restart
            );
            const auto recovery = authority->prepareRecovery(
                2, QString::fromLatin1(nonceB), fixedTime(1)
            );
            QVERIFY2(recovery.success, qPrintable(recovery.errorMessage));
            QCOMPARE(
                recovery.prepared->requirement,
                ActivationRequirement::Reload
            );
            const auto pending = objectFromBytes(
                readBytes(fixture.paths.pendingPath())
            );
            const auto candidate = parseDesiredState(
                canonicalObject(
                    pending.value(QStringLiteral("candidateSnapshot")).toObject()
                ),
                catalog,
                actionCatalog
            );
            QVERIFY2(candidate, qPrintable(describeErrors(candidate.errors)));
            QCOMPARE(candidate.value->devices, baseline.devices);
        }

        // Session work remains strongest when combined with a device delta.
        {
            StoreFixture fixture;
            QVERIFY(fixture.temporary.isValid());
            auto authority = transaction(fixture.paths);
            auto baseline = defaults;
            baseline.devices = {deviceA};
            QVERIFY(authority->initialize().success);
            auto replaced = authority->replaceSnapshot(
                0, serializeDesiredState(baseline)
            );
            QVERIFY(replaced.success);
            auto prepared = authority->prepareApply(
                1, QString::fromLatin1(nonceA), fixedTime()
            );
            QVERIFY(prepared.success);
            QVERIFY(authority->commitApply(prepared.prepared->id).success);

            auto session = environmentState(1);
            session.devices = {deviceB};
            replaced = authority->replaceSnapshot(
                1, serializeDesiredState(session)
            );
            QVERIFY(replaced.success);
            QCOMPARE(
                *replaced.snapshot.requiredActivation,
                ActivationRequirement::Session
            );
            prepared = authority->prepareApply(
                2, QString::fromLatin1(nonceB), fixedTime(1)
            );
            QVERIFY(prepared.success);
            QCOMPARE(
                prepared.prepared->requirement,
                ActivationRequirement::Session
            );
        }
    }

    void bindingAndSubmapChangesAreBidirectionallyRestartOnly()
    {
        struct Collections final {
            QVector<SubmapConfiguration> submaps;
            QVector<BindingConfiguration> bindings;
        };
        const auto requirementAfterApplied = [this](
            const Collections &before,
            const Collections &after
        ) {
            StoreFixture fixture;
            if (!fixture.temporary.isValid()) {
                return std::optional<ActivationRequirement>{};
            }
            auto authority = transaction(fixture.paths);
            auto baseline = defaults;
            baseline.submaps = before.submaps;
            baseline.bindings = before.bindings;
            if (!authority->initialize().success) {
                return std::optional<ActivationRequirement>{};
            }
            auto replaced = authority->replaceSnapshot(
                0, serializeDesiredState(baseline)
            );
            if (!replaced.success) {
                return std::optional<ActivationRequirement>{};
            }
            const auto baselineRevision = replaced.snapshot.revision;
            const auto prepared = authority->prepareApply(
                baselineRevision, QString::fromLatin1(nonceA), fixedTime()
            );
            if (!prepared.success || !prepared.prepared
                || !authority->commitApply(prepared.prepared->id).success) {
                return std::optional<ActivationRequirement>{};
            }
            auto candidate = defaults;
            candidate.revision = baselineRevision;
            candidate.submaps = after.submaps;
            candidate.bindings = after.bindings;
            replaced = authority->replaceSnapshot(
                baselineRevision, serializeDesiredState(candidate)
            );
            return replaced.success ? replaced.snapshot.requiredActivation
                                    : std::nullopt;
        };

        struct Row final {
            QString label;
            Collections before;
            Collections after;
        };
        QVector<Row> rows;
        const auto appendBothDirections = [&rows](
            QString label,
            Collections before,
            Collections after
        ) {
            rows.append({label + QStringLiteral(" forward"), before, after});
            rows.append({
                std::move(label) + QStringLiteral(" reverse"),
                std::move(after),
                std::move(before),
            });
        };

        // Disabled submaps are valid without a scoped enabled binding, which
        // lets the structural rows isolate submap-vector changes. Enabling a
        // submap necessarily adds an enabled scoped binding because the
        // desired-state contract rejects active empty submaps.
        const auto submapA = shortcutSubmap(
            QStringLiteral("submap-a"), QStringLiteral("resize"), {}, false
        );
        const auto submapB = shortcutSubmap(
            QStringLiteral("submap-b"), QStringLiteral("media"), {}, false
        );
        const auto bindingA = shortcutBinding();
        const auto bindingB = shortcutBinding(
            QStringLiteral("binding-b"), QStringLiteral("F8")
        );
        appendBothDirections(
            QStringLiteral("submap add/remove"), {}, {{submapA}, {}}
        );
        appendBothDirections(
            QStringLiteral("submap second record"),
            {{submapA}, {}},
            {{submapA, submapB}, {}}
        );
        appendBothDirections(
            QStringLiteral("submap reorder"),
            {{submapA, submapB}, {}},
            {{submapB, submapA}, {}}
        );
        auto changedSubmap = submapA;
        changedSubmap.id = QStringLiteral("renamed-submap-id");
        appendBothDirections(
            QStringLiteral("submap id"),
            {{submapA}, {}},
            {{changedSubmap}, {}}
        );
        changedSubmap = submapA;
        changedSubmap.name = QStringLiteral("move");
        appendBothDirections(
            QStringLiteral("submap name"),
            {{submapA}, {}},
            {{changedSubmap}, {}}
        );
        changedSubmap = submapA;
        changedSubmap.reset = submapB.name;
        appendBothDirections(
            QStringLiteral("submap reset"),
            {{submapA, submapB}, {}},
            {{changedSubmap, submapB}, {}}
        );
        changedSubmap = submapA;
        changedSubmap.enabled = true;
        auto activeSubmapBinding = shortcutBinding(
            QStringLiteral("active-submap-binding"),
            QStringLiteral("F9"),
            changedSubmap.name
        );
        appendBothDirections(
            QStringLiteral("submap enabled with required binding"),
            {{submapA}, {}},
            {{changedSubmap}, {activeSubmapBinding}}
        );

        appendBothDirections(
            QStringLiteral("binding add/remove"), {}, {{}, {bindingA}}
        );
        appendBothDirections(
            QStringLiteral("binding second record"),
            {{}, {bindingA}},
            {{}, {bindingA, bindingB}}
        );
        appendBothDirections(
            QStringLiteral("binding reorder"),
            {{}, {bindingA, bindingB}},
            {{}, {bindingB, bindingA}}
        );
        auto changedBinding = bindingA;
        changedBinding.id = QStringLiteral("renamed-binding-id");
        appendBothDirections(
            QStringLiteral("binding id"),
            {{}, {bindingA}},
            {{}, {changedBinding}}
        );
        changedBinding = bindingA;
        changedBinding.modifiers = {QStringLiteral("alt")};
        appendBothDirections(
            QStringLiteral("binding modifiers"),
            {{}, {bindingA}},
            {{}, {changedBinding}}
        );
        changedBinding = bindingA;
        changedBinding.key = QStringLiteral("F9");
        appendBothDirections(
            QStringLiteral("binding key"),
            {{}, {bindingA}},
            {{}, {changedBinding}}
        );
        changedBinding = bindingA;
        changedBinding.action = QStringLiteral("window.float");
        appendBothDirections(
            QStringLiteral("binding action"),
            {{}, {bindingA}},
            {{}, {changedBinding}}
        );
        auto argumentBinding = bindingA;
        argumentBinding.action = QStringLiteral("window.move");
        argumentBinding.arguments = QJsonObject{
            {QStringLiteral("direction"), QStringLiteral("left")}
        };
        changedBinding = argumentBinding;
        changedBinding.arguments.insert(
            QStringLiteral("direction"), QStringLiteral("right")
        );
        appendBothDirections(
            QStringLiteral("binding arguments"),
            {{}, {argumentBinding}},
            {{}, {changedBinding}}
        );
        changedBinding = bindingA;
        changedBinding.description = QStringLiteral("Different description");
        appendBothDirections(
            QStringLiteral("binding description"),
            {{}, {bindingA}},
            {{}, {changedBinding}}
        );
        changedBinding = bindingA;
        changedBinding.enabled = false;
        appendBothDirections(
            QStringLiteral("binding enabled"),
            {{}, {bindingA}},
            {{}, {changedBinding}}
        );
        auto disabledBinding = bindingA;
        disabledBinding.enabled = false;
        changedBinding = disabledBinding;
        changedBinding.submap = submapA.name;
        appendBothDirections(
            QStringLiteral("binding submap"),
            {{submapA}, {disabledBinding}},
            {{submapA}, {changedBinding}}
        );
        changedBinding = bindingA;
        changedBinding.options.locked = true;
        appendBothDirections(
            QStringLiteral("binding options"),
            {{}, {bindingA}},
            {{}, {changedBinding}}
        );
        auto disabledDispatcher = bindingA;
        disabledDispatcher.enabled = false;
        changedBinding = disabledDispatcher;
        changedBinding.actionType = BindingActionType::DefaultApp;
        changedBinding.action = QStringLiteral("defaultApp.terminal");
        appendBothDirections(
            QStringLiteral("binding action type"),
            {{}, {disabledDispatcher}},
            {{}, {changedBinding}}
        );

        for (const auto &row : rows) {
            const auto requirement = requirementAfterApplied(
                row.before, row.after
            );
            QVERIFY2(requirement.has_value(), qPrintable(row.label));
            QCOMPARE(*requirement, ActivationRequirement::Restart);
        }

        // Without an applied baseline, either nonempty collection must remain
        // restart-only so no live rollback assumption is introduced.
        for (const auto &candidateCollections : {
                 Collections{{submapA}, {}},
                 Collections{{}, {bindingA}},
             }) {
            StoreFixture fixture;
            QVERIFY(fixture.temporary.isValid());
            auto authority = transaction(fixture.paths);
            QVERIFY(authority->initialize().success);
            auto candidate = defaults;
            candidate.submaps = candidateCollections.submaps;
            candidate.bindings = candidateCollections.bindings;
            const auto replaced = authority->replaceSnapshot(
                0, serializeDesiredState(candidate)
            );
            QVERIFY2(replaced.success, qPrintable(replaced.errorMessage));
            QCOMPARE(
                *replaced.snapshot.requiredActivation,
                ActivationRequirement::Restart
            );
        }

        // Exact unchanged collections do not elevate an unrelated reloadable
        // scalar delta.
        {
            StoreFixture fixture;
            QVERIFY(fixture.temporary.isValid());
            auto authority = transaction(fixture.paths);
            auto baseline = defaults;
            baseline.submaps = {submapA};
            baseline.bindings = {bindingA};
            QVERIFY(authority->initialize().success);
            auto replaced = authority->replaceSnapshot(
                0, serializeDesiredState(baseline)
            );
            QVERIFY(replaced.success);
            auto prepared = authority->prepareApply(
                1, QString::fromLatin1(nonceA), fixedTime()
            );
            QVERIFY(prepared.success);
            QVERIFY(authority->commitApply(prepared.prepared->id).success);

            auto reloadOnly = baseline;
            reloadOnly.revision = 1;
            reloadOnly.overrides.insert(
                QStringLiteral("hyprland.general.border_size"), 2
            );
            replaced = authority->replaceSnapshot(
                1, serializeDesiredState(reloadOnly)
            );
            QVERIFY(replaced.success);
            QCOMPARE(
                *replaced.snapshot.requiredActivation,
                ActivationRequirement::Reload
            );
            prepared = authority->prepareApply(
                2, QString::fromLatin1(nonceB), fixedTime(1)
            );
            QVERIFY(prepared.success);
            QCOMPARE(
                prepared.prepared->requirement,
                ActivationRequirement::Reload
            );
        }

        // Recovery restores both exact applied collections. Either desired
        // divergence is Restart, while the recovery candidate itself is Reload.
        for (const auto &divergentCollections : {
                 Collections{{submapB}, {bindingA}},
                 Collections{{submapA}, {bindingB}},
             }) {
            StoreFixture fixture;
            QVERIFY(fixture.temporary.isValid());
            auto authority = transaction(fixture.paths);
            auto baseline = defaults;
            baseline.submaps = {submapA};
            baseline.bindings = {bindingA};
            QVERIFY(authority->initialize().success);
            auto replaced = authority->replaceSnapshot(
                0, serializeDesiredState(baseline)
            );
            QVERIFY(replaced.success);
            auto prepared = authority->prepareApply(
                1, QString::fromLatin1(nonceA), fixedTime()
            );
            QVERIFY(prepared.success);
            QVERIFY(authority->commitApply(prepared.prepared->id).success);

            auto divergent = baseline;
            divergent.revision = 1;
            divergent.submaps = divergentCollections.submaps;
            divergent.bindings = divergentCollections.bindings;
            replaced = authority->replaceSnapshot(
                1, serializeDesiredState(divergent)
            );
            QVERIFY2(replaced.success, qPrintable(replaced.errorMessage));
            QCOMPARE(
                *replaced.snapshot.requiredActivation,
                ActivationRequirement::Restart
            );
            const auto recovery = authority->prepareRecovery(
                2, QString::fromLatin1(nonceB), fixedTime(1)
            );
            QVERIFY2(recovery.success, qPrintable(recovery.errorMessage));
            QCOMPARE(
                recovery.prepared->requirement,
                ActivationRequirement::Reload
            );
            const auto pending = objectFromBytes(
                readBytes(fixture.paths.pendingPath())
            );
            const auto candidate = parseDesiredState(
                canonicalObject(
                    pending.value(QStringLiteral("candidateSnapshot")).toObject()
                ),
                catalog,
                actionCatalog
            );
            QVERIFY2(candidate, qPrintable(describeErrors(candidate.errors)));
            const auto recoveredObject = objectFromBytes(
                serializeDesiredState(*candidate.value)
            );
            const auto baselineObject = objectFromBytes(
                serializeDesiredState(baseline)
            );
            QCOMPARE(
                recoveredObject.value(QStringLiteral("submaps")),
                baselineObject.value(QStringLiteral("submaps"))
            );
            QCOMPARE(
                recoveredObject.value(QStringLiteral("bindings")),
                baselineObject.value(QStringLiteral("bindings"))
            );
        }

        // Session work remains strongest when combined with either shortcut
        // collection delta.
        {
            StoreFixture fixture;
            QVERIFY(fixture.temporary.isValid());
            auto authority = transaction(fixture.paths);
            auto baseline = defaults;
            baseline.submaps = {submapA};
            baseline.bindings = {bindingA};
            QVERIFY(authority->initialize().success);
            auto replaced = authority->replaceSnapshot(
                0, serializeDesiredState(baseline)
            );
            QVERIFY(replaced.success);
            auto prepared = authority->prepareApply(
                1, QString::fromLatin1(nonceA), fixedTime()
            );
            QVERIFY(prepared.success);
            QVERIFY(authority->commitApply(prepared.prepared->id).success);

            auto session = environmentState(1);
            session.submaps = {submapB};
            session.bindings = {bindingB};
            replaced = authority->replaceSnapshot(
                1, serializeDesiredState(session)
            );
            QVERIFY(replaced.success);
            QCOMPARE(
                *replaced.snapshot.requiredActivation,
                ActivationRequirement::Session
            );
            prepared = authority->prepareApply(
                2, QString::fromLatin1(nonceB), fixedTime(1)
            );
            QVERIFY(prepared.success);
            QCOMPARE(
                prepared.prepared->requirement,
                ActivationRequirement::Session
            );
        }
    }

    void curveStructureRequiresRestartButSameNameTypeMapReloads()
    {
        const auto bezier = [](QString id, QString name, double offset) {
            AnimationCurve curve;
            curve.id = std::move(id);
            curve.name = std::move(name);
            BezierCurveParameters parameters;
            parameters.points = {{
                {0.1 + offset, 0.2 + offset},
                {0.8 - offset, 0.9 - offset},
            }};
            curve.parameters = parameters;
            return curve;
        };
        const auto spring = [](QString id, QString name, double stiffness) {
            AnimationCurve curve;
            curve.id = std::move(id);
            curve.name = std::move(name);
            curve.parameters = SpringCurveParameters{
                .stiffness = stiffness,
                .dampening = 25.0,
                .mass = 1.0,
            };
            return curve;
        };
        const auto requirementAfterApplied = [this](
            const QVector<AnimationCurve> &before,
            const QVector<AnimationCurve> &after
        ) {
            StoreFixture fixture;
            if (!fixture.temporary.isValid()) {
                return std::optional<ActivationRequirement>{};
            }
            auto authority = transaction(fixture.paths);
            auto baseline = defaults;
            baseline.revision = 0;
            baseline.curves = before;
            auto initialized = authority->initialize();
            if (!initialized.success) {
                return std::optional<ActivationRequirement>{};
            }
            auto replaced = authority->replaceSnapshot(
                0, serializeDesiredState(baseline)
            );
            if (!replaced.success) {
                return std::optional<ActivationRequirement>{};
            }
            auto prepared = authority->prepareApply(
                1, QString::fromLatin1(nonceA), fixedTime()
            );
            if (!prepared.success || !prepared.prepared
                || !authority->commitApply(prepared.prepared->id).success) {
                return std::optional<ActivationRequirement>{};
            }
            auto candidate = defaults;
            candidate.revision = 1;
            candidate.curves = after;
            replaced = authority->replaceSnapshot(
                1, serializeDesiredState(candidate)
            );
            return replaced.success ? replaced.snapshot.requiredActivation
                                    : std::nullopt;
        };

        const QVector<AnimationCurve> oneBezier{
            bezier(QStringLiteral("curve-a"), QStringLiteral("ease"), 0.0),
        };
        const QVector<AnimationCurve> twoCurves{
            oneBezier.first(),
            spring(QStringLiteral("curve-b"), QStringLiteral("bounce"), 250.0),
        };

        // A previously unapplied transaction has no curve-map rollback target.
        {
            StoreFixture fixture;
            QVERIFY(fixture.temporary.isValid());
            auto authority = transaction(fixture.paths);
            QVERIFY(authority->initialize().success);
            auto candidate = defaults;
            candidate.revision = 0;
            candidate.curves = oneBezier;
            const auto replaced = authority->replaceSnapshot(
                0, serializeDesiredState(candidate)
            );
            QVERIFY(replaced.success);
            QCOMPARE(
                *replaced.snapshot.requiredActivation,
                ActivationRequirement::Restart
            );
        }

        // Name-set deltas are bidirectionally restart-only so rollback can
        // remove neither target-only nor prior-only persistent runtime names.
        QCOMPARE(
            requirementAfterApplied(oneBezier, twoCurves),
            std::optional{ActivationRequirement::Restart}
        );
        QCOMPARE(
            requirementAfterApplied(twoCurves, oneBezier),
            std::optional{ActivationRequirement::Restart}
        );
        const QVector<AnimationCurve> renamed{
            bezier(
                QStringLiteral("curve-a"), QStringLiteral("renamed"), 0.0
            ),
        };
        QCOMPARE(
            requirementAfterApplied(oneBezier, renamed),
            std::optional{ActivationRequirement::Restart}
        );
        const QVector<AnimationCurve> retyped{
            spring(
                QStringLiteral("curve-a"), QStringLiteral("ease"), 250.0
            ),
        };
        QCOMPARE(
            requirementAfterApplied(oneBezier, retyped),
            std::optional{ActivationRequirement::Restart}
        );

        // IDs, ordering, and same-type parameters do not change the runtime
        // logical name-to-type map and remain reload-safe.
        const QVector<AnimationCurve> safeEdit{
            spring(
                QStringLiteral("new-spring-id"),
                QStringLiteral("bounce"),
                275.5
            ),
            bezier(
                QStringLiteral("new-bezier-id"),
                QStringLiteral("ease"),
                0.05
            ),
        };
        QCOMPARE(
            requirementAfterApplied(twoCurves, safeEdit),
            std::optional{ActivationRequirement::Reload}
        );
        QCOMPARE(
            requirementAfterApplied(safeEdit, twoCurves),
            std::optional{ActivationRequirement::Reload}
        );
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
