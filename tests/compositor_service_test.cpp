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
#include <tuple>

using namespace HyprShelld::Compositor;

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

[[nodiscard]] ManagementStatus managedStatus()
{
    return {
        .state = ManagementState::Managed,
        .entrypointKind = EntrypointKind::Regular,
        .entrypointDigest = QStringLiteral("managed-entrypoint"),
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

class FakeAuthority final : public ConfigurationAuthority
{
public:
    AuthorityResult initializeResult;
    AuthorityResult replaceResult;
    AuthorityResult prepareApplyResult;
    AuthorityResult prepareRecoveryResult;
    AuthorityResult commitResult;
    AuthorityResult abortResult;
    AuthoritySnapshot current;
    int initializeCalls = 0;
    int replaceCalls = 0;
    int prepareApplyCalls = 0;
    int prepareRecoveryCalls = 0;
    int commitCalls = 0;
    int abortCalls = 0;
    quint64 lastReplaceExpected = 0;
    QByteArray lastReplaceCandidate;
    QStringList calls;

    AuthorityResult initialize() override
    {
        ++initializeCalls;
        calls.append(QStringLiteral("initialize"));
        if (initializeResult.success) {
            current = initializeResult.snapshot;
        }
        return initializeResult;
    }

    AuthoritySnapshot snapshot() const override
    {
        return current;
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
    mutable int statusCalls = 0;
    mutable QVector<ActivationRequirement> capabilityChecks;
    int adoptCalls = 0;
    int activateCalls = 0;
    int rollbackCalls = 0;
    QStringList calls;
    QByteArray lastRollbackToken;

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

    ActivationResult adopt(
        const ActivationGeneration &,
        QStringView
    ) override
    {
        ++adoptCalls;
        calls.append(QStringLiteral("adopt"));
        statusValue = adoptionResult.status;
        return adoptionResult;
    }

    ActivationResult activate(const ActivationGeneration &) override
    {
        ++activateCalls;
        calls.append(QStringLiteral("activate"));
        statusValue = activationResult.status;
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
};

struct ServiceHarness final {
    FakeAuthority *authority = nullptr;
    FakeActivationBackend *backend = nullptr;
    std::unique_ptr<CompositorService> service;

    explicit ServiceHarness(
        const AuthoritySnapshot &initial,
        const ManagementStatus &management = managedStatus()
    )
    {
        auto ownedBackend = std::make_unique<FakeActivationBackend>();
        backend = ownedBackend.get();
        backend->statusValue = management;
        service = std::make_unique<CompositorService>(
            std::move(ownedBackend),
            QDBusConnection(QStringLiteral("compositor-service-test"))
        );

        auto ownedAuthority = std::make_unique<FakeAuthority>();
        authority = ownedAuthority.get();
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

} // namespace

class CompositorServiceTest final : public QObject
{
    Q_OBJECT

private slots:
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

    void adoptionProofDistinguishesAbsenceFromExactRegularFile()
    {
        const auto initial = dirtySnapshot();
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
};

QTEST_MAIN(CompositorServiceTest)

#include "compositor_service_test.moc"
