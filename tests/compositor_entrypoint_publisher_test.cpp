#include "compositord/activation_backend.h"
#include "compositord/legacy_entrypoint_records.h"
#include "compositord/renderer.h"

#include "hyprland/json_support.h"

#include <QCryptographicHash>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QtTest>

#include <algorithm>
#include <functional>
#include <memory>

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

using namespace HyprShelld::Compositor;

namespace {

constexpr auto nonceA = "0123456789abcdef0123456789abcdef";
constexpr auto nonceB = "11111111111111111111111111111111";
constexpr auto snapshotDigest =
    "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";

[[nodiscard]] QString sha256(const QByteArrayView bytes)
{
    return QString::fromLatin1(
        QCryptographicHash::hash(bytes, QCryptographicHash::Sha256).toHex()
    );
}

[[nodiscard]] QByteArray canonicalObject(const QJsonObject &object)
{
    auto bytes = HyprShelld::Hyprland::JsonSupport::canonicalJson(object);
    bytes.append('\n');
    return bytes;
}

[[nodiscard]] QJsonObject objectFromFile(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) return {};
    QJsonParseError error;
    const auto document = QJsonDocument::fromJson(file.readAll(), &error);
    return error.error == QJsonParseError::NoError && document.isObject()
        ? document.object() : QJsonObject{};
}

[[nodiscard]] QByteArray readBytes(const QString &path)
{
    QFile file(path);
    return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray{};
}

[[nodiscard]] bool makeDirectory(
    const QString &path,
    const mode_t mode = 0700
)
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
        || !file.flush()) return false;
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
        || !file.flush()) return false;
    file.close();
    return ::chmod(QFile::encodeName(path).constData(), mode) == 0;
}

[[nodiscard]] struct stat metadata(const QString &path)
{
    struct stat result {};
    static_cast<void>(::lstat(QFile::encodeName(path).constData(), &result));
    return result;
}

[[nodiscard]] bool sameInode(
    const struct stat &left,
    const struct stat &right
)
{
    return left.st_dev == right.st_dev && left.st_ino == right.st_ino;
}

[[nodiscard]] int openDirectory(const QString &path)
{
    return ::open(
        QFile::encodeName(path).constData(),
        O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW
    );
}

struct PublisherTree final {
    QTemporaryDir temporary;
    QString stateRoot;
    QString configRoot;
    QString managedRoot;
    QString generationsRoot;
    QString stableEntrypoint;
    QString ownershipRecord;

    PublisherTree()
        : stateRoot(QDir(temporary.path()).filePath(QStringLiteral("state")))
        , configRoot(QDir(temporary.path()).filePath(QStringLiteral("config")))
        , managedRoot(QDir(configRoot).filePath(QStringLiteral("hyprshelld")))
        , generationsRoot(
              QDir(managedRoot).filePath(QStringLiteral("generations"))
          )
        , stableEntrypoint(
              QDir(configRoot).filePath(QStringLiteral("hyprland.lua"))
          )
        , ownershipRecord(
              QDir(managedRoot).filePath(
                  QStringLiteral("entrypoint-ownership.json")
              )
          )
    {
        if (!temporary.isValid()
            || ::chmod(QFile::encodeName(temporary.path()).constData(), 0700)
                != 0
            || !makeDirectory(stateRoot) || !makeDirectory(configRoot)
            || !makeDirectory(managedRoot)
            || !makeDirectory(generationsRoot)) {
            stateRoot.clear();
        }
    }

    ~PublisherTree()
    {
        if (!temporary.isValid()) return;
        QStringList paths;
        QDirIterator iterator(
            temporary.path(),
            QDir::AllEntries | QDir::Hidden | QDir::NoDotAndDotDot,
            QDirIterator::Subdirectories
        );
        while (iterator.hasNext()) paths.append(iterator.next());
        std::sort(paths.begin(), paths.end(), [](const QString &left,
                                                const QString &right) {
            return left.size() > right.size();
        });
        for (const auto &path : paths) {
            const auto info = metadata(path);
            static_cast<void>(::chmod(
                QFile::encodeName(path).constData(),
                S_ISDIR(info.st_mode) ? 0700 : 0600
            ));
        }
        static_cast<void>(::chmod(
            QFile::encodeName(temporary.path()).constData(), 0700
        ));
    }

    [[nodiscard]] bool valid() const { return !stateRoot.isEmpty(); }

    [[nodiscard]] ActivationFilesystemContext context() const
    {
        ActivationFilesystemContext context;
        context.stateDirectoryFd = openDirectory(stateRoot);
        context.configDirectoryFd = openDirectory(configRoot);
        context.managedDirectoryFd = openDirectory(managedRoot);
        context.generationsDirectoryFd = openDirectory(generationsRoot);
        context.stateRoot = stateRoot;
        context.configRoot = configRoot;
        context.managedConfigRoot = managedRoot;
        context.stableEntrypoint = stableEntrypoint;
        return context;
    }

    [[nodiscard]] ActivationGeneration generation(
        const QString &nonce,
        const QByteArray &marker
    ) const
    {
        const auto directory = QDir(generationsRoot).filePath(nonce);
        const auto modules = QDir(directory).filePath(QStringLiteral("modules"));
        if (!makeDirectory(directory) || !makeDirectory(modules)) return {};

        QJsonObject files;
        const auto entrypointBytes = QByteArrayLiteral("-- managed target ")
            + marker + QByteArrayLiteral("\n");
        const auto entrypoint = QDir(directory).filePath(
            QStringLiteral("hyprland.lua")
        );
        if (!writeNew(entrypoint, entrypointBytes, 0400)) return {};
        files.insert(
            QStringLiteral("hyprland.lua"),
            QJsonObject {
                {QStringLiteral("sha256"), sha256(entrypointBytes)},
                {QStringLiteral("size"), entrypointBytes.size()},
            }
        );

        for (const auto &relative : managedModulePaths()) {
            const auto name = relative.section(QLatin1Char('/'), 1);
            const auto bytes = QByteArrayLiteral("-- module ")
                + relative.toUtf8() + QByteArrayLiteral(" ") + marker
                + QByteArrayLiteral("\n");
            if (!writeNew(QDir(modules).filePath(name), bytes, 0400)) return {};
            files.insert(
                relative,
                QJsonObject {
                    {QStringLiteral("sha256"), sha256(bytes)},
                    {QStringLiteral("size"), bytes.size()},
                }
            );
        }

        QJsonObject manifest {
            {QStringLiteral("formatVersion"), 1},
            {QStringLiteral("contractVersion"), 1},
            {QStringLiteral("generation"), QString{}},
            {QStringLiteral("snapshotDigest"),
             QString::fromLatin1(snapshotDigest)},
            {QStringLiteral("catalogDigest"), QStringLiteral("catalog")},
            {QStringLiteral("actionCatalogDigest"), QStringLiteral("actions")},
            {QStringLiteral("revision"), QStringLiteral("8")},
            {QStringLiteral("targetHyprland"), QStringLiteral("0.56.1")},
            {QStringLiteral("compatibleHyprland"),
             QJsonObject {
                 {QStringLiteral("major"), 0},
                 {QStringLiteral("minor"), 56},
                 {QStringLiteral("reviewedVersion"),
                  QStringLiteral("0.56.1")},
                 {QStringLiteral("minimumPatch"), 0},
                 {QStringLiteral("maximumPatch"), 7},
             }},
            {QStringLiteral("rendererVersion"),
             static_cast<qint64>(currentRendererVersion)},
            {QStringLiteral("activationNonce"), nonce},
            {QStringLiteral("createdAt"),
             QStringLiteral("2026-08-09T00:00:00.000Z")},
            {QStringLiteral("entrypoint"), QStringLiteral("hyprland.lua")},
            {QStringLiteral("files"), files},
        };
        auto digestInput = manifest;
        digestInput.remove(QStringLiteral("generation"));
        const auto id = sha256(
            HyprShelld::Hyprland::JsonSupport::canonicalJson(digestInput)
        );
        manifest.insert(QStringLiteral("generation"), id);
        const auto manifestBytes = canonicalObject(manifest);
        if (!writeNew(
                QDir(directory).filePath(QStringLiteral("manifest.json")),
                manifestBytes, 0400
            )
            || ::chmod(QFile::encodeName(modules).constData(), 0500) != 0
            || ::chmod(QFile::encodeName(directory).constData(), 0500) != 0) {
            return {};
        }
        return {
            .id = id,
            .nonce = nonce,
            .snapshotDigest = QString::fromLatin1(snapshotDigest),
            .revision = 8,
            .directory = directory,
            .entrypoint = entrypoint,
            .manifest = manifestBytes,
            .requirement = ActivationRequirement::Reload,
        };
    }

    [[nodiscard]] std::unique_ptr<AtomicEntrypointPublisher> publisher(
        std::function<bool(EntrypointFaultPoint)> hook = {}
    ) const
    {
        return std::make_unique<AtomicEntrypointPublisher>(
            stateRoot, configRoot, managedRoot, stableEntrypoint,
            ownershipRecord, std::move(hook)
        );
    }

    [[nodiscard]] QString rootPath(const QString &name) const
    {
        if (name == QStringLiteral("state")) return stateRoot;
        if (name == QStringLiteral("config")) return configRoot;
        if (name == QStringLiteral("managed")) return managedRoot;
        return generationsRoot;
    }

    [[nodiscard]] QString replaceRoot(const QString &name) const
    {
        const auto source = rootPath(name);
        const auto detached = source + QStringLiteral("-detached");
        if (::rename(
                QFile::encodeName(source).constData(),
                QFile::encodeName(detached).constData()
            ) != 0) return {};
        if (name == QStringLiteral("config")) {
            if (!makeDirectory(configRoot) || !makeDirectory(managedRoot)
                || !makeDirectory(generationsRoot)) return {};
        } else if (name == QStringLiteral("managed")) {
            if (!makeDirectory(managedRoot) || !makeDirectory(generationsRoot)) {
                return {};
            }
        } else if (name == QStringLiteral("generations")) {
            if (!makeDirectory(generationsRoot)) return {};
        } else if (!makeDirectory(stateRoot)) {
            return {};
        }
        return detached;
    }
};

[[nodiscard]] QStringList treeFingerprint(const QString &root)
{
    QStringList paths;
    QDirIterator iterator(
        root, QDir::AllEntries | QDir::Hidden | QDir::NoDotAndDotDot,
        QDirIterator::Subdirectories
    );
    while (iterator.hasNext()) paths.append(iterator.next());
    std::sort(paths.begin(), paths.end());

    QStringList result;
    for (const auto &path : paths) {
        const auto info = metadata(path);
        const auto relative = QDir(root).relativeFilePath(path);
        auto value = QStringLiteral("%1|%2|%3|%4|%5|%6")
            .arg(relative)
            .arg(static_cast<qulonglong>(info.st_dev))
            .arg(static_cast<qulonglong>(info.st_ino))
            .arg(static_cast<qulonglong>(info.st_mode))
            .arg(static_cast<qulonglong>(info.st_nlink))
            .arg(static_cast<qulonglong>(info.st_size));
        if (S_ISREG(info.st_mode)) value += QLatin1Char('|') + sha256(readBytes(path));
        result.append(value);
    }
    return result;
}

[[nodiscard]] EntrypointPublishResult adoptAbsent(
    AtomicEntrypointPublisher &publisher,
    const ActivationGeneration &generation
)
{
    return publisher.publish(
        generation, true, {}, QByteArrayLiteral("[]"),
        QStringLiteral("hyprlang")
    );
}

[[nodiscard]] QString firstManagedEntryWithPrefix(
    const QString &managedRoot,
    const QString &prefix
)
{
    const auto entries = QDir(managedRoot).entryList(
        {prefix + QStringLiteral("*")},
        QDir::Files | QDir::Hidden,
        QDir::Name
    );
    return entries.size() == 1
        ? QDir(managedRoot).filePath(entries.front()) : QString{};
}

[[nodiscard]] bool isFinalizeFault(const EntrypointFaultPoint point)
{
    return point == EntrypointFaultPoint::BeforeOwnershipRename
        || point == EntrypointFaultPoint::AfterOwnershipRenameBeforeDirectorySync
        || point == EntrypointFaultPoint::BeforeBridgeRemoval
        || point == EntrypointFaultPoint::AfterBridgeRemovalBeforeDirectorySync;
}

} // namespace

class CompositorEntrypointPublisherTest final : public QObject
{
    Q_OBJECT

private slots:
    void activeV1PublisherBytesMatchDormantRecoveryCodecs()
    {
        const auto bridgePath = [](const PublisherTree &tree) {
            return QDir(tree.managedRoot).filePath(
                QStringLiteral("live-activation.pending.json")
            );
        };

        // Capture the staging spelling directly from the active writer.
        PublisherTree stagingTree;
        QVERIFY(stagingTree.valid());
        const auto stagingGeneration = stagingTree.generation(
            QString::fromLatin1(nonceA), QByteArrayLiteral("staging")
        );
        auto stagingPublisher = stagingTree.publisher(
            [](const EntrypointFaultPoint point) {
                return point
                    == EntrypointFaultPoint::AfterTargetFileSyncBeforeDirectorySync;
            }
        );
        QVERIFY(stagingPublisher->initialize(stagingTree.context()).success);
        const auto interrupted = adoptAbsent(
            *stagingPublisher, stagingGeneration
        );
        QVERIFY(!interrupted.success);
        const auto activeStagingBytes = readBytes(bridgePath(stagingTree));
        const auto recoveredStaging =
            parseLegacyLiveActivationBridgeRecordV1(activeStagingBytes);
        QVERIFY(recoveredStaging);
        const auto recoveredStagingBytes =
            serializeLegacyLiveActivationBridgeRecordV1(*recoveredStaging);
        QVERIFY(recoveredStagingBytes);
        QCOMPARE(*recoveredStagingBytes, activeStagingBytes);

        // Capture adoption-ready, ownership, and managed-update-ready bytes.
        PublisherTree managedTree;
        QVERIFY(managedTree.valid());
        const auto first = managedTree.generation(
            QString::fromLatin1(nonceA), QByteArrayLiteral("first")
        );
        const auto second = managedTree.generation(
            QString::fromLatin1(nonceB), QByteArrayLiteral("second")
        );
        auto managedPublisher = managedTree.publisher();
        QVERIFY(managedPublisher->initialize(managedTree.context()).success);
        const auto adopted = adoptAbsent(*managedPublisher, first);
        QVERIFY(adopted.success);

        const auto activeAdoptionBytes = readBytes(bridgePath(managedTree));
        const auto recoveredAdoption =
            parseLegacyLiveActivationBridgeRecordV1(activeAdoptionBytes);
        QVERIFY(recoveredAdoption);
        const auto recoveredAdoptionBytes =
            serializeLegacyLiveActivationBridgeRecordV1(*recoveredAdoption);
        QVERIFY(recoveredAdoptionBytes);
        QCOMPARE(*recoveredAdoptionBytes, activeAdoptionBytes);

        QVERIFY(managedPublisher->finalize(adopted.receipt, true).success);
        const auto activeOwnershipBytes = readBytes(
            managedTree.ownershipRecord
        );
        const auto recoveredOwnership =
            parseLegacyEntrypointOwnershipRecordV1(activeOwnershipBytes);
        QVERIFY(recoveredOwnership);
        const auto recoveredOwnershipBytes =
            serializeLegacyEntrypointOwnershipRecordV1(*recoveredOwnership);
        QVERIFY(recoveredOwnershipBytes);
        QCOMPARE(*recoveredOwnershipBytes, activeOwnershipBytes);

        const auto updated = managedPublisher->publish(
            second, false, {}, QByteArrayLiteral("[]"),
            QStringLiteral("lua")
        );
        QVERIFY(updated.success);
        const auto activeUpdateBytes = readBytes(bridgePath(managedTree));
        const auto recoveredUpdate =
            parseLegacyLiveActivationBridgeRecordV1(activeUpdateBytes);
        QVERIFY(recoveredUpdate);
        const auto recoveredUpdateBytes =
            serializeLegacyLiveActivationBridgeRecordV1(*recoveredUpdate);
        QVERIFY(recoveredUpdateBytes);
        QCOMPARE(*recoveredUpdateBytes, activeUpdateBytes);

        // Also bind the regular-original adoption and ownership spellings.
        PublisherTree regularTree;
        QVERIFY(regularTree.valid());
        const QByteArray original{"-- exact user original\n"};
        QVERIFY(writeNew(regularTree.stableEntrypoint, original, 0640));
        const auto regularGeneration = regularTree.generation(
            QString::fromLatin1(nonceA), QByteArrayLiteral("regular")
        );
        auto regularPublisher = regularTree.publisher();
        QVERIFY(regularPublisher->initialize(regularTree.context()).success);
        const auto regularAdoption = regularPublisher->publish(
            regularGeneration, true, sha256(original), QByteArrayLiteral("[]"),
            QStringLiteral("lua")
        );
        QVERIFY(regularAdoption.success);
        const auto activeRegularBridgeBytes = readBytes(
            bridgePath(regularTree)
        );
        const auto recoveredRegularBridge =
            parseLegacyLiveActivationBridgeRecordV1(
                activeRegularBridgeBytes
            );
        QVERIFY(recoveredRegularBridge);
        const auto recoveredRegularBridgeBytes =
            serializeLegacyLiveActivationBridgeRecordV1(
                *recoveredRegularBridge
            );
        QVERIFY(recoveredRegularBridgeBytes);
        QCOMPARE(*recoveredRegularBridgeBytes, activeRegularBridgeBytes);

        QVERIFY(regularPublisher->finalize(
            regularAdoption.receipt, true
        ).success);
        const auto activeRegularOwnershipBytes = readBytes(
            regularTree.ownershipRecord
        );
        const auto recoveredRegularOwnership =
            parseLegacyEntrypointOwnershipRecordV1(
                activeRegularOwnershipBytes
            );
        QVERIFY(recoveredRegularOwnership);
        const auto recoveredRegularOwnershipBytes =
            serializeLegacyEntrypointOwnershipRecordV1(
                *recoveredRegularOwnership
            );
        QVERIFY(recoveredRegularOwnershipBytes);
        QCOMPARE(
            *recoveredRegularOwnershipBytes, activeRegularOwnershipBytes
        );
    }

    void authorityRootReplacementFailsClosed_data()
    {
        QTest::addColumn<QString>("root");
        QTest::newRow("state") << QStringLiteral("state");
        QTest::newRow("config") << QStringLiteral("config");
        QTest::newRow("managed") << QStringLiteral("managed");
        QTest::newRow("generations") << QStringLiteral("generations");
    }

    void authorityRootReplacementFailsClosed()
    {
        QFETCH(QString, root);
        PublisherTree tree;
        QVERIFY(tree.valid());
        const auto generation = tree.generation(
            QString::fromLatin1(nonceA), QByteArrayLiteral("A")
        );
        QVERIFY(!generation.id.isEmpty());
        auto publisher = tree.publisher();
        QVERIFY(publisher->initialize(tree.context()).success);

        const auto detached = tree.replaceRoot(root);
        QVERIFY(!detached.isEmpty());
        const auto before = treeFingerprint(detached);
        QCOMPARE(publisher->status().state, ManagementState::Conflict);
        const auto published = adoptAbsent(*publisher, generation);
        QVERIFY(!published.success);
        QCOMPARE(treeFingerprint(detached), before);
        QVERIFY(!QFileInfo::exists(tree.stableEntrypoint));
        QVERIFY(!QFileInfo::exists(
            QDir(tree.managedRoot).filePath(
                QStringLiteral("live-activation.pending.json")
            )
        ));
    }

    void everyFaultBoundaryRechecksCanonicalRoots_data()
    {
        QTest::addColumn<int>("faultPoint");
        const QList<QPair<const char *, EntrypointFaultPoint>> points {
            {"target-file-sync",
             EntrypointFaultPoint::AfterTargetFileSyncBeforeDirectorySync},
            {"before-journal", EntrypointFaultPoint::BeforeJournalRename},
            {"after-journal",
             EntrypointFaultPoint::AfterJournalRenameBeforeDirectorySync},
            {"before-ready", EntrypointFaultPoint::BeforeReadyJournalRename},
            {"after-ready",
             EntrypointFaultPoint::AfterReadyJournalRenameBeforeDirectorySync},
            {"before-entrypoint",
             EntrypointFaultPoint::BeforeEntrypointExchange},
            {"after-entrypoint",
             EntrypointFaultPoint::AfterEntrypointExchangeBeforeDirectorySync},
            {"before-ownership", EntrypointFaultPoint::BeforeOwnershipRename},
            {"after-ownership",
             EntrypointFaultPoint::AfterOwnershipRenameBeforeDirectorySync},
            {"before-bridge-remove",
             EntrypointFaultPoint::BeforeBridgeRemoval},
            {"after-bridge-remove",
             EntrypointFaultPoint::AfterBridgeRemovalBeforeDirectorySync},
        };
        for (const auto &[name, point] : points) {
            QTest::newRow(name) << static_cast<int>(point);
        }
    }

    void publicationCasRechecksEveryAuthorityRoot_data()
    {
        QTest::addColumn<QString>("root");
        QTest::newRow("state") << QStringLiteral("state");
        QTest::newRow("config") << QStringLiteral("config");
        QTest::newRow("managed") << QStringLiteral("managed");
        QTest::newRow("generations") << QStringLiteral("generations");
    }

    void publicationCasRechecksEveryAuthorityRoot()
    {
        QFETCH(QString, root);
        PublisherTree tree;
        QVERIFY(tree.valid());
        const auto generation = tree.generation(
            QString::fromLatin1(nonceA), QByteArrayLiteral("A")
        );
        bool fired = false;
        QString detached;
        QStringList atFault;
        auto publisher = tree.publisher([&](const EntrypointFaultPoint point) {
            if (!fired
                && point == EntrypointFaultPoint::BeforeEntrypointExchange) {
                fired = true;
                detached = tree.replaceRoot(root);
                atFault = treeFingerprint(detached);
            }
            return false;
        });
        QVERIFY(publisher->initialize(tree.context()).success);
        const auto published = adoptAbsent(*publisher, generation);
        QVERIFY(!published.success);
        QVERIFY(fired);
        QVERIFY(!detached.isEmpty());
        QCOMPARE(treeFingerprint(detached), atFault);
        QVERIFY(!QFileInfo::exists(tree.stableEntrypoint));
    }

    void everyFaultBoundaryRechecksCanonicalRoots()
    {
        QFETCH(int, faultPoint);
        const auto selected = static_cast<EntrypointFaultPoint>(faultPoint);
        PublisherTree tree;
        QVERIFY(tree.valid());
        const auto generation = tree.generation(
            QString::fromLatin1(nonceA), QByteArrayLiteral("A")
        );
        QVERIFY(!generation.id.isEmpty());

        bool fired = false;
        QString detached;
        QStringList atFault;
        auto publisher = tree.publisher([&](const EntrypointFaultPoint point) {
            if (!fired && point == selected) {
                fired = true;
                detached = tree.replaceRoot(QStringLiteral("managed"));
                atFault = treeFingerprint(detached);
            }
            // Returning false models a mutation that is observable only by
            // the mandatory post-hook root identity check.
            return false;
        });
        QVERIFY(publisher->initialize(tree.context()).success);
        const auto published = adoptAbsent(*publisher, generation);
        if (isFinalizeFault(selected)) {
            QVERIFY(published.success);
            const auto finalized = publisher->finalize(
                published.receipt, true
            );
            QVERIFY(!finalized.success);
        } else {
            QVERIFY(!published.success);
        }
        QVERIFY(fired);
        QVERIFY(!detached.isEmpty());
        QCOMPARE(treeFingerprint(detached), atFault);
        QCOMPARE(
            QDir(tree.managedRoot).entryList(
                QDir::AllEntries | QDir::Hidden | QDir::NoDotAndDotDot
            ),
            QStringList({QStringLiteral("generations")})
        );
    }

    void stagingRollbackNeverDeletesForeignSwapReplacement()
    {
        PublisherTree tree;
        QVERIFY(tree.valid());
        const auto generation = tree.generation(
            QString::fromLatin1(nonceA), QByteArrayLiteral("A")
        );
        auto publisher = tree.publisher([](const EntrypointFaultPoint point) {
            return point
                == EntrypointFaultPoint::AfterTargetFileSyncBeforeDirectorySync;
        });
        QVERIFY(publisher->initialize(tree.context()).success);
        const auto published = adoptAbsent(*publisher, generation);
        QVERIFY(!published.success);
        QVERIFY(!published.receipt.rollbackToken.isEmpty());

        const auto bridgePath = QDir(tree.managedRoot).filePath(
            QStringLiteral("live-activation.pending.json")
        );
        const auto bridge = objectFromFile(bridgePath);
        QVERIFY(!bridge.isEmpty());
        QCOMPARE(bridge.value(QStringLiteral("phase")).toString(),
                 QStringLiteral("staging"));
        const auto swapPath = QDir(tree.managedRoot).filePath(
            bridge.value(QStringLiteral("swapName")).toString()
        );
        QVERIFY(QFile::remove(swapPath));
        const QByteArray foreign{"foreign swap payload\n"};
        QVERIFY(writeNew(swapPath, foreign));
        const auto foreignIdentity = metadata(swapPath);

        const auto restored = publisher->rollback(published.receipt);
        QVERIFY(restored.success);
        const auto finalized = publisher->finalize(published.receipt, false);
        QVERIFY(!finalized.success);
        QCOMPARE(readBytes(swapPath), foreign);
        QVERIFY(sameInode(metadata(swapPath), foreignIdentity));
        QVERIFY(QFileInfo::exists(bridgePath));
    }

    void preRenameReadyJournalTempIsRestartRecoverable_data()
    {
        QTest::addColumn<int>("variant");
        QTest::newRow("exact-ready-temp") << 0;
        QTest::newRow("partial-ready-temp") << 1;
        QTest::newRow("foreign-ready-temp") << 2;
    }

    void preRenameReadyJournalTempIsRestartRecoverable()
    {
        QFETCH(int, variant);
        PublisherTree tree;
        QVERIFY(tree.valid());
        const auto generation = tree.generation(
            QString::fromLatin1(nonceA), QByteArrayLiteral("A")
        );
        QString capturedPath;
        QByteArray capturedBytes;
        auto publisher = tree.publisher([&](const EntrypointFaultPoint point) {
            if (point == EntrypointFaultPoint::BeforeReadyJournalRename) {
                capturedPath = firstManagedEntryWithPrefix(
                    tree.managedRoot,
                    QStringLiteral(".hyprshelld-ready-journal-")
                );
                capturedBytes = readBytes(capturedPath);
                return true;
            }
            return false;
        });
        QVERIFY(publisher->initialize(tree.context()).success);
        const auto published = adoptAbsent(*publisher, generation);
        QVERIFY(!published.success);
        QVERIFY(!published.receipt.rollbackToken.isEmpty());
        QVERIFY(!capturedPath.isEmpty());
        QVERIFY(!capturedBytes.isEmpty());
        QVERIFY(!QFileInfo::exists(capturedPath));
        const auto seeded = variant == 0
            ? capturedBytes
            : variant == 1
                ? capturedBytes.first(
                      std::max<qsizetype>(1, capturedBytes.size() / 2)
                  )
                : capturedBytes + QByteArrayLiteral("foreign");
        QVERIFY(writeNew(capturedPath, seeded));
        const auto seededIdentity = metadata(capturedPath);

        const auto restored = publisher->rollback(published.receipt);
        QVERIFY(restored.success);
        const auto finalized = publisher->finalize(published.receipt, false);
        if (variant != 0) {
            QVERIFY(!finalized.success);
            QCOMPARE(readBytes(capturedPath), seeded);
            QVERIFY(sameInode(metadata(capturedPath), seededIdentity));
            QVERIFY(publisher->pendingReconciliation().value.has_value());
        } else {
            QVERIFY(finalized.success);
            QVERIFY(!QFileInfo::exists(capturedPath));
            QVERIFY(!publisher->pendingReconciliation().value.has_value());
        }
    }

    void managedTargetFinalizeIsRestartIdempotent()
    {
        PublisherTree tree;
        QVERIFY(tree.valid());
        const auto first = tree.generation(
            QString::fromLatin1(nonceA), QByteArrayLiteral("A")
        );
        const auto second = tree.generation(
            QString::fromLatin1(nonceB), QByteArrayLiteral("B")
        );
        bool armed = false;
        auto publisher = tree.publisher([&](const EntrypointFaultPoint point) {
            return armed && point == EntrypointFaultPoint::BeforeBridgeRemoval;
        });
        QVERIFY(publisher->initialize(tree.context()).success);
        const auto adopted = adoptAbsent(*publisher, first);
        QVERIFY(adopted.success);
        QVERIFY(publisher->finalize(adopted.receipt, true).success);

        const auto updated = publisher->publish(
            second, false, {}, QByteArrayLiteral("[]"), QStringLiteral("lua")
        );
        QVERIFY(updated.success);
        armed = true;
        const auto interrupted = publisher->finalize(updated.receipt, true);
        QVERIFY(!interrupted.success);
        QVERIFY(QFileInfo::exists(QDir(tree.managedRoot).filePath(
            QStringLiteral("live-activation.pending.json")
        )));
        armed = false;
        const auto retried = publisher->finalize(updated.receipt, true);
        QVERIFY(retried.success);
        QCOMPARE(retried.status.state, ManagementState::Managed);
        QCOMPARE(retried.status.managedGeneration, second.id);
    }

    void preRenameOwnershipTempIsRestartRecoverable_data()
    {
        QTest::addColumn<bool>("managedUpdate");
        QTest::addColumn<int>("variant");
        QTest::newRow("adoption-exact") << false << 0;
        QTest::newRow("adoption-partial") << false << 1;
        QTest::newRow("adoption-foreign") << false << 2;
        QTest::newRow("managed-update-exact") << true << 0;
        QTest::newRow("managed-update-partial") << true << 1;
        QTest::newRow("managed-update-foreign") << true << 2;
    }

    void preRenameOwnershipTempIsRestartRecoverable()
    {
        QFETCH(bool, managedUpdate);
        QFETCH(int, variant);
        PublisherTree tree;
        QVERIFY(tree.valid());
        const auto first = tree.generation(
            QString::fromLatin1(nonceA), QByteArrayLiteral("A")
        );
        const auto second = tree.generation(
            QString::fromLatin1(nonceB), QByteArrayLiteral("B")
        );
        bool armed = false;
        QString capturedPath;
        QByteArray capturedBytes;
        auto publisher = tree.publisher([&](const EntrypointFaultPoint point) {
            if (armed && point == EntrypointFaultPoint::BeforeOwnershipRename) {
                capturedPath = firstManagedEntryWithPrefix(
                    tree.managedRoot,
                    QStringLiteral(".hyprshelld-ownership-")
                );
                capturedBytes = readBytes(capturedPath);
                return true;
            }
            return false;
        });
        QVERIFY(publisher->initialize(tree.context()).success);
        auto target = adoptAbsent(*publisher, first);
        QVERIFY(target.success);
        if (managedUpdate) {
            QVERIFY(publisher->finalize(target.receipt, true).success);
            target = publisher->publish(
                second, false, {}, QByteArrayLiteral("[]"),
                QStringLiteral("lua")
            );
            QVERIFY(target.success);
        }

        armed = true;
        const auto interrupted = publisher->finalize(target.receipt, true);
        QVERIFY(!interrupted.success);
        QVERIFY(!capturedPath.isEmpty());
        QVERIFY(!capturedBytes.isEmpty());
        QVERIFY(!QFileInfo::exists(capturedPath));
        const auto seeded = variant == 0
            ? capturedBytes
            : variant == 1
                ? capturedBytes.first(
                      std::max<qsizetype>(1, capturedBytes.size() / 2)
                  )
                : capturedBytes + QByteArrayLiteral("foreign");
        QVERIFY(writeNew(capturedPath, seeded));
        const auto seededIdentity = metadata(capturedPath);
        armed = false;
        const auto retried = publisher->finalize(target.receipt, true);
        if (variant != 0) {
            QVERIFY(!retried.success);
            QCOMPARE(readBytes(capturedPath), seeded);
            QVERIFY(sameInode(metadata(capturedPath), seededIdentity));
            QVERIFY(publisher->pendingReconciliation().value.has_value());
        } else {
            QVERIFY(retried.success);
            QVERIFY(!QFileInfo::exists(capturedPath));
            QCOMPARE(retried.status.managedGeneration,
                     managedUpdate ? second.id : first.id);
        }
    }

    void preRenameCandidateTempRejectsPartialOrForeign_data()
    {
        QTest::addColumn<int>("variant");
        QTest::newRow("exact-candidate") << 0;
        QTest::newRow("partial-candidate") << 1;
        QTest::newRow("foreign-candidate") << 2;
    }

    void preRenameCandidateTempRejectsPartialOrForeign()
    {
        QFETCH(int, variant);
        PublisherTree tree;
        QVERIFY(tree.valid());
        const auto generation = tree.generation(
            QString::fromLatin1(nonceA), QByteArrayLiteral("A")
        );
        auto publisher = tree.publisher([](const EntrypointFaultPoint point) {
            return point
                == EntrypointFaultPoint::AfterTargetFileSyncBeforeDirectorySync;
        });
        QVERIFY(publisher->initialize(tree.context()).success);
        const auto published = adoptAbsent(*publisher, generation);
        QVERIFY(!published.success);
        QVERIFY(!published.receipt.rollbackToken.isEmpty());

        const auto bridgePath = QDir(tree.managedRoot).filePath(
            QStringLiteral("live-activation.pending.json")
        );
        const auto bridge = objectFromFile(bridgePath);
        QVERIFY(!bridge.isEmpty());
        const auto candidatePath = QDir(tree.managedRoot).filePath(
            bridge.value(QStringLiteral("swapName")).toString()
        );
        const auto exactCandidate = readBytes(candidatePath);
        QVERIFY(!exactCandidate.isEmpty());
        if (variant == 1) {
            QVERIFY(replaceBytes(
                candidatePath,
                exactCandidate.first(
                    std::max<qsizetype>(1, exactCandidate.size() / 2)
                )
            ));
        } else if (variant == 2) {
            QVERIFY(replaceBytes(
                candidatePath,
                exactCandidate + QByteArrayLiteral("foreign")
            ));
        }
        const auto seededBytes = readBytes(candidatePath);
        const auto seededIdentity = metadata(candidatePath);

        const auto restored = publisher->rollback(published.receipt);
        QVERIFY(restored.success);
        const auto finalized = publisher->finalize(published.receipt, false);
        if (variant == 0) {
            QVERIFY(finalized.success);
            QVERIFY(!QFileInfo::exists(candidatePath));
            QVERIFY(!QFileInfo::exists(bridgePath));
        } else {
            QVERIFY(!finalized.success);
            QCOMPARE(readBytes(candidatePath), seededBytes);
            QVERIFY(sameInode(metadata(candidatePath), seededIdentity));
            QVERIFY(QFileInfo::exists(bridgePath));
        }
    }

    void partialScratchOrphansAreIgnored_data()
    {
        QTest::addColumn<QString>("phase");
        QTest::newRow("candidate-write-crash") << QStringLiteral("candidate");
        QTest::newRow("ready-write-crash") << QStringLiteral("ready");
        QTest::newRow("ownership-write-crash") << QStringLiteral("ownership");
    }

    void partialScratchOrphansAreIgnored()
    {
        QFETCH(QString, phase);
        PublisherTree tree;
        QVERIFY(tree.valid());
        const auto generation = tree.generation(
            QString::fromLatin1(nonceA), QByteArrayLiteral("A")
        );
        const auto scratchPath = QDir(tree.managedRoot).filePath(
            QStringLiteral(
                ".hyprshelld-scratch-ffffffffffffffffffffffffffffffff"
            )
        );
        const QByteArray partial{"partial scratch from a crashed write"};
        bool seeded = false;
        struct stat scratchIdentity {};
        const auto seedScratch = [&] {
            if (seeded) return true;
            seeded = writeNew(scratchPath, partial);
            if (seeded) scratchIdentity = metadata(scratchPath);
            return seeded;
        };
        auto publisher = tree.publisher([&](const EntrypointFaultPoint point) {
            if (phase == QStringLiteral("ready")
                && point
                    == EntrypointFaultPoint::AfterTargetFileSyncBeforeDirectorySync) {
                static_cast<void>(seedScratch());
            }
            return false;
        });
        QVERIFY(publisher->initialize(tree.context()).success);
        if (phase == QStringLiteral("candidate")) QVERIFY(seedScratch());

        const auto published = adoptAbsent(*publisher, generation);
        QVERIFY(published.success);
        if (phase == QStringLiteral("ownership")) QVERIFY(seedScratch());
        QVERIFY(publisher->finalize(published.receipt, true).success);
        QVERIFY(seeded);
        QCOMPARE(readBytes(scratchPath), partial);
        QVERIFY(sameInode(metadata(scratchPath), scratchIdentity));
    }

    void regularAdoptionSyncFailurePrecedesEveryMutation()
    {
        PublisherTree tree;
        QVERIFY(tree.valid());
        const QByteArray original{"-- user original requiring sync\n"};
        QVERIFY(writeNew(tree.stableEntrypoint, original, 0640));
        const auto originalIdentity = metadata(tree.stableEntrypoint);
        const auto generation = tree.generation(
            QString::fromLatin1(nonceA), QByteArrayLiteral("A")
        );
        bool syncAttempted = false;
        auto publisher = tree.publisher([&](const EntrypointFaultPoint point) {
            if (point == EntrypointFaultPoint::BeforeOriginalEntrypointSync) {
                syncAttempted = true;
                return true;
            }
            return false;
        });
        QVERIFY(publisher->initialize(tree.context()).success);
        const auto published = publisher->publish(
            generation, true, sha256(original), QByteArrayLiteral("[]"),
            QStringLiteral("lua")
        );
        QVERIFY(syncAttempted);
        QVERIFY(!published.success);
        QVERIFY(!published.namespaceMayHaveChanged);
        QVERIFY(published.receipt.rollbackToken.isEmpty());
        QCOMPARE(readBytes(tree.stableEntrypoint), original);
        QVERIFY(sameInode(metadata(tree.stableEntrypoint), originalIdentity));
        QCOMPARE(metadata(tree.stableEntrypoint).st_mode & 0777, mode_t(0640));
        QVERIFY(!QFileInfo::exists(QDir(tree.managedRoot).filePath(
            QStringLiteral("live-activation.pending.json")
        )));
        QVERIFY(!QFileInfo::exists(tree.ownershipRecord));
        QCOMPARE(
            QDir(tree.managedRoot).entryList(
                QDir::AllEntries | QDir::Hidden | QDir::NoDotAndDotDot
            ),
            QStringList({QStringLiteral("generations")})
        );
    }

    void wrongSideFinalizeCannotConsumeBridge()
    {
        PublisherTree tree;
        QVERIFY(tree.valid());
        const auto generation = tree.generation(
            QString::fromLatin1(nonceA), QByteArrayLiteral("A")
        );
        auto publisher = tree.publisher();
        QVERIFY(publisher->initialize(tree.context()).success);
        const auto published = adoptAbsent(*publisher, generation);
        QVERIFY(published.success);
        const auto wrong = publisher->finalize(published.receipt, false);
        QVERIFY(!wrong.success);
        QVERIFY(publisher->pendingReconciliation().value.has_value());
        const auto correct = publisher->finalize(published.receipt, true);
        QVERIFY(correct.success);
        QCOMPARE(correct.status.managedGeneration, generation.id);
    }

    void regularAdoptionPreservesOriginalInodeModeAcrossUpdates()
    {
        PublisherTree tree;
        QVERIFY(tree.valid());
        const QByteArray original{"-- user original\n"};
        QVERIFY(writeNew(tree.stableEntrypoint, original, 0640));
        const auto originalIdentity = metadata(tree.stableEntrypoint);
        const auto first = tree.generation(
            QString::fromLatin1(nonceA), QByteArrayLiteral("A")
        );
        const auto second = tree.generation(
            QString::fromLatin1(nonceB), QByteArrayLiteral("B")
        );
        auto publisher = tree.publisher();
        QVERIFY(publisher->initialize(tree.context()).success);
        const auto adopted = publisher->publish(
            first, true, sha256(original), QByteArrayLiteral("[]"),
            QStringLiteral("lua")
        );
        QVERIFY(adopted.success);
        QVERIFY(publisher->finalize(adopted.receipt, true).success);

        auto ownership = objectFromFile(tree.ownershipRecord);
        const auto backupName = ownership.value(
            QStringLiteral("originalBackup")
        ).toString();
        QVERIFY(!backupName.isEmpty());
        const auto backupPath = QDir(tree.managedRoot).filePath(backupName);
        QCOMPARE(readBytes(backupPath), original);
        const auto backupIdentity = metadata(backupPath);
        QVERIFY(sameInode(backupIdentity, originalIdentity));
        QCOMPARE(backupIdentity.st_mode & 0777, mode_t(0640));

        const auto updated = publisher->publish(
            second, false, {}, QByteArrayLiteral("[]"), QStringLiteral("lua")
        );
        QVERIFY(updated.success);
        QVERIFY(publisher->finalize(updated.receipt, true).success);
        ownership = objectFromFile(tree.ownershipRecord);
        QCOMPARE(ownership.value(QStringLiteral("originalBackup")).toString(),
                 backupName);
        QVERIFY(sameInode(metadata(backupPath), originalIdentity));
        QCOMPARE(metadata(backupPath).st_mode & 0777, mode_t(0640));
    }

    void sameDigestExternalReplacementNeverRetainsManagedOwnership()
    {
        PublisherTree tree;
        QVERIFY(tree.valid());
        const auto first = tree.generation(
            QString::fromLatin1(nonceA), QByteArrayLiteral("A")
        );
        const auto second = tree.generation(
            QString::fromLatin1(nonceB), QByteArrayLiteral("B")
        );
        auto publisher = tree.publisher();
        QVERIFY(publisher->initialize(tree.context()).success);
        const auto adopted = adoptAbsent(*publisher, first);
        QVERIFY(adopted.success);
        QVERIFY(publisher->finalize(adopted.receipt, true).success);
        const auto managedBytes = readBytes(tree.stableEntrypoint);
        const auto oldIdentity = metadata(tree.stableEntrypoint);

        const auto replacement = tree.stableEntrypoint
            + QStringLiteral(".replacement");
        QVERIFY(writeNew(replacement, managedBytes));
        const auto replacementIdentity = metadata(replacement);
        QVERIFY(!sameInode(oldIdentity, replacementIdentity));
        QVERIFY(::rename(
            QFile::encodeName(replacement).constData(),
            QFile::encodeName(tree.stableEntrypoint).constData()
        ) == 0);
        QCOMPARE(publisher->status().state, ManagementState::Conflict);

        const auto update = publisher->publish(
            second, false, {}, QByteArrayLiteral("[]"), QStringLiteral("lua")
        );
        QVERIFY(!update.success);
        QCOMPARE(readBytes(tree.stableEntrypoint), managedBytes);
        QVERIFY(sameInode(metadata(tree.stableEntrypoint), replacementIdentity));
    }

    void publicationCasPreservesConcurrentEntrypoint_data()
    {
        QTest::addColumn<bool>("priorAbsent");
        QTest::newRow("absent-create-race") << true;
        QTest::newRow("regular-replace-race") << false;
    }

    void publicationCasPreservesConcurrentEntrypoint()
    {
        QFETCH(bool, priorAbsent);
        PublisherTree tree;
        QVERIFY(tree.valid());
        const QByteArray original{"-- original\n"};
        if (!priorAbsent) QVERIFY(writeNew(tree.stableEntrypoint, original));
        const auto generation = tree.generation(
            QString::fromLatin1(nonceA), QByteArrayLiteral("A")
        );
        const QByteArray foreign{"-- concurrent foreign\n"};
        struct stat foreignIdentity {};
        bool raced = false;
        auto publisher = tree.publisher([&](const EntrypointFaultPoint point) {
            if (!raced && point == EntrypointFaultPoint::BeforeEntrypointExchange) {
                raced = true;
                if (priorAbsent) {
                    if (!writeNew(tree.stableEntrypoint, foreign)) return true;
                } else {
                    const auto temporary = tree.stableEntrypoint
                        + QStringLiteral(".foreign");
                    if (!writeNew(temporary, foreign)
                        || ::rename(
                               QFile::encodeName(temporary).constData(),
                               QFile::encodeName(tree.stableEntrypoint).constData()
                           ) != 0) return true;
                }
                foreignIdentity = metadata(tree.stableEntrypoint);
            }
            return false;
        });
        QVERIFY(publisher->initialize(tree.context()).success);
        const auto published = publisher->publish(
            generation, true, priorAbsent ? QString{} : sha256(original),
            QByteArrayLiteral("[]"),
            priorAbsent ? QStringLiteral("hyprlang") : QStringLiteral("lua")
        );
        QVERIFY(raced);
        QVERIFY(!published.success);
        QCOMPARE(readBytes(tree.stableEntrypoint), foreign);
        QVERIFY(sameInode(metadata(tree.stableEntrypoint), foreignIdentity));
    }

    void strictBridgeAndOwnershipMetadataRejectCorruption()
    {
        PublisherTree bridgeTree;
        QVERIFY(bridgeTree.valid());
        const auto bridgeGeneration = bridgeTree.generation(
            QString::fromLatin1(nonceA), QByteArrayLiteral("A")
        );
        auto bridgePublisher = bridgeTree.publisher(
            [](const EntrypointFaultPoint point) {
                return point
                    == EntrypointFaultPoint::AfterTargetFileSyncBeforeDirectorySync;
            }
        );
        QVERIFY(bridgePublisher->initialize(bridgeTree.context()).success);
        const auto staged = adoptAbsent(*bridgePublisher, bridgeGeneration);
        QVERIFY(!staged.success);
        const auto bridgePath = QDir(bridgeTree.managedRoot).filePath(
            QStringLiteral("live-activation.pending.json")
        );
        auto bridge = objectFromFile(bridgePath);
        QVERIFY(!bridge.isEmpty());
        // Absence is recoverable only from the legacy provider. A canonical
        // but semantically impossible absent+Lua record must not be trusted.
        bridge.insert(QStringLiteral("baselineProvider"), QStringLiteral("lua"));
        QVERIFY(replaceBytes(bridgePath, canonicalObject(bridge)));
        QVERIFY(!bridgePublisher->pendingReconciliation().success);

        PublisherTree ownershipTree;
        QVERIFY(ownershipTree.valid());
        const auto ownedGeneration = ownershipTree.generation(
            QString::fromLatin1(nonceA), QByteArrayLiteral("A")
        );
        auto ownershipPublisher = ownershipTree.publisher();
        QVERIFY(ownershipPublisher->initialize(ownershipTree.context()).success);
        const auto adopted = adoptAbsent(*ownershipPublisher, ownedGeneration);
        QVERIFY(adopted.success);
        QVERIFY(ownershipPublisher->finalize(adopted.receipt, true).success);
        auto ownership = objectFromFile(ownershipTree.ownershipRecord);
        QVERIFY(!ownership.isEmpty());
        ownership.insert(QStringLiteral("entrypointInode"), QStringLiteral("0"));
        QVERIFY(replaceBytes(
            ownershipTree.ownershipRecord, canonicalObject(ownership)
        ));
        QCOMPARE(ownershipPublisher->status().state, ManagementState::Conflict);
        QCOMPARE(readBytes(ownershipTree.stableEntrypoint),
                 readBytes(ownedGeneration.entrypoint));
    }
};

QTEST_GUILESS_MAIN(CompositorEntrypointPublisherTest)

#include "compositor_entrypoint_publisher_test.moc"
