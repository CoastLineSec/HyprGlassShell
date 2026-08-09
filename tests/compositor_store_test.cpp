#include "compositord/generation.h"
#include "compositord/renderer.h"
#include "compositord/store.h"

#include "hyprland/action_catalog.h"
#include "hyprland/catalog.h"
#include "hyprland/desired_state.h"

#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QTemporaryDir>
#include <QtTest>

#include <fcntl.h>
#include <sys/file.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <unistd.h>

using namespace HyprShelld::Compositor;
using namespace HyprShelld::Hyprland;

namespace {

constexpr auto nonceA = "0123456789abcdef0123456789abcdef";

[[nodiscard]] QByteArray readBytes(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }
    return file.readAll();
}

[[nodiscard]] QByteArray encode(QJsonObject object)
{
    auto bytes = QJsonDocument(std::move(object)).toJson(QJsonDocument::Compact);
    bytes.append('\n');
    return bytes;
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

[[nodiscard]] bool writeFile(
    const QString &path,
    const QByteArrayView contents,
    const mode_t mode = 0600
)
{
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::NewOnly)
        || file.write(contents.data(), contents.size()) != contents.size()
        || !file.flush()) {
        return false;
    }
    file.close();
    return ::chmod(QFile::encodeName(path).constData(), mode) == 0;
}

[[nodiscard]] mode_t permissions(const QString &path)
{
    struct stat information {};
    if (::lstat(QFile::encodeName(path).constData(), &information) != 0) {
        return 0;
    }
    return information.st_mode & 0777;
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

[[nodiscard]] int descriptorForIdentity(const struct stat &identity)
{
    struct rlimit limit {};
    if (::getrlimit(RLIMIT_NOFILE, &limit) != 0) {
        return -1;
    }
    const auto maximum = static_cast<int>(
        std::min<rlim_t>(limit.rlim_cur, 4096)
    );
    for (int descriptor = 0; descriptor < maximum; ++descriptor) {
        struct stat opened {};
        if (::fstat(descriptor, &opened) == 0
            && sameIdentity(identity, opened)) {
            return descriptor;
        }
    }
    return -1;
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

[[nodiscard]] QDateTime fixedTime()
{
    return QDateTime::fromString(
        QStringLiteral("2026-08-09T12:34:56.789Z"),
        Qt::ISODateWithMs
    );
}

} // namespace

class CompositorStoreTest final : public QObject
{
    Q_OBJECT

private:
    Catalog catalog;
    ActionCatalog actionCatalog;
    DesiredState defaultState;

    [[nodiscard]] RenderResult renderDefault(const StorePaths &paths) const
    {
        return renderGeneration(
            defaultState,
            catalog,
            actionCatalog,
            QDir(paths.generationsPath()).filePath(QString::fromLatin1(nonceA)),
            paths.userCustomPath(),
            QString::fromLatin1(nonceA),
            fixedTime()
        );
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

        const auto parsedState = parseDesiredState(
            readBytes(QStringLiteral(HYPRSHELLD_HYPRLAND_DEFAULTS_FILE)),
            catalog,
            actionCatalog
        );
        QVERIFY2(parsedState, qPrintable(describeErrors(parsedState.errors)));
        defaultState = *parsedState.value;
        QVERIFY(fixedTime().isValid());
    }

    void pathLayoutIsExactAndConstructionIsSideEffectFree()
    {
        StoreFixture fixture;
        QVERIFY(fixture.temporary.isValid());
        QCOMPARE(fixture.paths.desiredPath(),
                 QDir(fixture.paths.stateRoot).filePath(
                     QStringLiteral("desired.json")));
        QCOMPARE(fixture.paths.lastGoodPath(),
                 QDir(fixture.paths.stateRoot).filePath(
                     QStringLiteral("last-good.json")));
        QCOMPARE(fixture.paths.activationPath(),
                 QDir(fixture.paths.stateRoot).filePath(
                     QStringLiteral("activation.json")));
        QCOMPARE(fixture.paths.pendingPath(),
                 QDir(fixture.paths.stateRoot).filePath(
                     QStringLiteral("pending.json")));
        QCOMPARE(fixture.paths.lockPath(),
                 QDir(fixture.paths.stateRoot).filePath(QStringLiteral(".lock")));
        QCOMPARE(fixture.paths.userCustomPath(),
                 QDir(fixture.paths.configRoot).filePath(
                     QStringLiteral("user-custom.lua")));
        QCOMPARE(fixture.paths.stableEntrypointPath(),
                 QDir(fixture.paths.configRoot).filePath(
                     QStringLiteral("hyprland.lua")));
        QCOMPARE(fixture.paths.generationsPath(),
                 QDir(fixture.paths.managedConfigRoot).filePath(
                     QStringLiteral("generations")));

        PersistentStore store(fixture.paths);
        QVERIFY(!store.initialized());
        QVERIFY(!QFileInfo::exists(fixture.paths.stateRoot));
        QVERIFY(!QFileInfo::exists(fixture.paths.configRoot));
        QCOMPARE(store.stateDirectoryFd(), -1);
        QCOMPARE(store.configDirectoryFd(), -1);
        QCOMPARE(store.managedDirectoryFd(), -1);
    }

    void initializeAcquiresPrivateNamedLifetimeLease()
    {
        StoreFixture fixture;
        QVERIFY(fixture.temporary.isValid());
        PersistentStore store(fixture.paths);
        const auto initialized = store.initialize();
        QVERIFY2(initialized.success, qPrintable(initialized.errorMessage));
        QVERIFY(store.initialized());
        QCOMPARE(permissions(fixture.paths.stateRoot), mode_t(0700));
        QCOMPARE(permissions(fixture.paths.configRoot), mode_t(0700));
        QCOMPARE(permissions(fixture.paths.managedConfigRoot), mode_t(0700));
        QCOMPARE(permissions(fixture.paths.lockPath()), mode_t(0600));

        const auto named = metadata(fixture.paths.lockPath());
        QVERIFY(S_ISREG(named.st_mode));
        QCOMPARE(named.st_nlink, nlink_t(1));
        QCOMPARE(named.st_uid, ::geteuid());
        const auto descriptor = descriptorForIdentity(named);
        QVERIFY(descriptor >= 0);
        struct stat opened {};
        QVERIFY(::fstat(descriptor, &opened) == 0);
        QVERIFY(sameIdentity(named, opened));
        QVERIFY((::fcntl(descriptor, F_GETFD) & FD_CLOEXEC) != 0);
        const auto independent = ::open(
            QFile::encodeName(fixture.paths.lockPath()).constData(),
            O_RDWR | O_CLOEXEC | O_NOFOLLOW
        );
        QVERIFY(independent >= 0);
        QVERIFY(::flock(independent, LOCK_EX | LOCK_NB) != 0);
        ::close(independent);
    }

    void secondOwnerIsRejectedWithoutRepairingAnything()
    {
        StoreFixture fixture;
        QVERIFY(fixture.temporary.isValid());
        PersistentStore first(fixture.paths);
        QVERIFY(first.initialize().success);
        QVERIFY(writeFile(
            QDir(fixture.paths.stateRoot).filePath(QStringLiteral("sentinel")),
            QByteArrayLiteral("unchanged")
        ));
        const auto before = metadata(fixture.paths.lockPath());

        PersistentStore second(fixture.paths);
        const auto rejected = second.initialize();
        QVERIFY(!rejected.success);
        QCOMPARE(rejected.errorCode, QStringLiteral("store.busy"));
        QVERIFY(!second.initialized());
        QCOMPARE(readBytes(QDir(fixture.paths.stateRoot).filePath(
                     QStringLiteral("sentinel"))),
                 QByteArrayLiteral("unchanged"));
        QVERIFY(sameIdentity(before, metadata(fixture.paths.lockPath())));
    }

    void replacedLockPathCannotCreateASecondOwner()
    {
        StoreFixture fixture;
        QVERIFY(fixture.temporary.isValid());
        PersistentStore first(fixture.paths);
        QVERIFY(first.initialize().success);
        const auto firstIdentity = metadata(fixture.paths.lockPath());

        QVERIFY(QFile::remove(fixture.paths.lockPath()));
        QVERIFY(writeFile(fixture.paths.lockPath(), QByteArrayLiteral("replacement")));
        QVERIFY(!sameIdentity(firstIdentity, metadata(fixture.paths.lockPath())));

        PersistentStore second(fixture.paths);
        const auto rejected = second.initialize();
        QVERIFY2(!rejected.success,
                 "Replacing .lock must not create two apparent store owners");
        QVERIFY(!second.initialized());
    }

    void replacedStateRootCannotCreateASecondOwner()
    {
        StoreFixture fixture;
        QVERIFY(fixture.temporary.isValid());
        PersistentStore first(fixture.paths);
        QVERIFY(first.initialize().success);
        const QByteArray original{"detached authority\n"};
        QVERIFY(first.write(StoreFile::Desired, original).success);

        const auto detached = fixture.paths.stateRoot
            + QStringLiteral("-detached");
        QVERIFY(::rename(
                    QFile::encodeName(fixture.paths.stateRoot).constData(),
                    QFile::encodeName(detached).constData()
                ) == 0);
        QVERIFY(makeDirectory(fixture.paths.stateRoot));

        const auto detachedRead = first.read(StoreFile::Desired);
        QCOMPARE(detachedRead.status, StoreReadStatus::Unreadable);
        QCOMPARE(readBytes(QDir(detached).filePath(QStringLiteral("desired.json"))),
                 original);

        PersistentStore second(fixture.paths);
        const auto rejected = second.initialize();
        QVERIFY2(!rejected.success,
                 "Replacing the whole state root must not create two authorities");
        QVERIFY(!second.initialized());
        QCOMPARE(readBytes(QDir(detached).filePath(QStringLiteral("desired.json"))),
                 original);
    }

    void failedInitializeReleasesEveryPartialDescriptor()
    {
        StoreFixture fixture;
        QVERIFY(fixture.temporary.isValid());
        QVERIFY(makeDirectory(fixture.paths.configRoot, 0770));
        PersistentStore failing(fixture.paths);
        const auto rejected = failing.initialize();
        QVERIFY(!rejected.success);
        QVERIFY(!failing.initialized());
        QCOMPARE(failing.stateDirectoryFd(), -1);
        QCOMPARE(failing.configDirectoryFd(), -1);
        QCOMPARE(failing.managedDirectoryFd(), -1);

        QVERIFY(::chmod(QFile::encodeName(fixture.paths.configRoot).constData(),
                        0700) == 0);
        PersistentStore retry(fixture.paths);
        const auto initialized = retry.initialize();
        QVERIFY2(initialized.success, qPrintable(initialized.errorMessage));
    }

    void symlinkedRootsFailClosedWithoutTouchingTargets()
    {
        StoreFixture fixture;
        QVERIFY(fixture.temporary.isValid());
        const auto target = QDir(fixture.temporary.path()).filePath(
            QStringLiteral("state-target")
        );
        QVERIFY(makeDirectory(target));
        const auto stateParent = QFileInfo(fixture.paths.stateRoot)
                                     .dir().absolutePath();
        QVERIFY(makeDirectory(stateParent));
        QVERIFY(::symlink(
                    QFile::encodeName(target).constData(),
                    QFile::encodeName(fixture.paths.stateRoot).constData()
                ) == 0);
        PersistentStore store(fixture.paths);
        QVERIFY(!store.initialize().success);
        QCOMPARE(QDir(target).entryList(QDir::NoDotAndDotDot | QDir::AllEntries),
                 QStringList{});
    }

    void durableFilesAreBoundedPrivateAndAtomicallyReplaced()
    {
        StoreFixture fixture;
        QVERIFY(fixture.temporary.isValid());
        PersistentStore store(fixture.paths);
        QVERIFY(store.initialize().success);
        const QByteArray first{"first payload\n"};
        const QByteArray second{"second payload\n"};

        for (const auto file : {
                 StoreFile::Desired,
                 StoreFile::LastGood,
                 StoreFile::Activation,
                 StoreFile::Pending,
             }) {
            auto operation = store.write(file, first);
            QVERIFY2(operation.success, qPrintable(operation.errorMessage));
            const auto read = store.read(file);
            QCOMPARE(read.status, StoreReadStatus::Present);
            QCOMPARE(read.bytes, first);
        }
        for (const auto &path : {
                 fixture.paths.desiredPath(),
                 fixture.paths.lastGoodPath(),
                 fixture.paths.activationPath(),
                 fixture.paths.pendingPath(),
             }) {
            QCOMPARE(permissions(path), mode_t(0600));
            const auto info = metadata(path);
            QCOMPARE(info.st_uid, ::geteuid());
            QCOMPARE(info.st_nlink, nlink_t(1));
        }

        const auto oldIdentity = metadata(fixture.paths.desiredPath());
        QVERIFY(store.write(StoreFile::Desired, second).success);
        QCOMPARE(store.read(StoreFile::Desired).bytes, second);
        QVERIFY(!sameIdentity(oldIdentity, metadata(fixture.paths.desiredPath())));

        QVERIFY(!store.write(StoreFile::Desired, QByteArrayView{}).success);
        QCOMPARE(store.read(StoreFile::Desired).bytes, second);
        const QByteArray oversized(maximumDesiredStateBytes + 1, 'x');
        QVERIFY(!store.write(StoreFile::Desired, oversized).success);
        QCOMPARE(store.read(StoreFile::Desired).bytes, second);

        QVERIFY(store.remove(StoreFile::Pending).success);
        QCOMPARE(store.read(StoreFile::Pending).status, StoreReadStatus::Missing);
        QVERIFY(store.remove(StoreFile::Pending).success);
    }

    void namespaceMutationFaultsReportExactTriState()
    {
        const QByteArray payload{"authority payload\n"};
        {
            StoreFixture fixture;
            QVERIFY(fixture.temporary.isValid());
            fixture.paths.faultHook = [](
                const StoreFaultPoint point,
                const StoreFile file
            ) {
                return file == StoreFile::Desired
                    && point == StoreFaultPoint::BeforePublishRename;
            };
            PersistentStore store(fixture.paths);
            QVERIFY(store.initialize().success);
            const auto failed = store.write(StoreFile::Desired, payload);
            QVERIFY(!failed.success);
            QVERIFY(!failed.committedButNotDurable);
            QCOMPARE(store.read(StoreFile::Desired).status,
                     StoreReadStatus::Missing);
        }
        {
            StoreFixture fixture;
            QVERIFY(fixture.temporary.isValid());
            fixture.paths.faultHook = [](
                const StoreFaultPoint point,
                const StoreFile file
            ) {
                return file == StoreFile::Desired
                    && point
                        == StoreFaultPoint::AfterPublishRenameBeforeDirectorySync;
            };
            PersistentStore store(fixture.paths);
            QVERIFY(store.initialize().success);
            const auto uncertain = store.write(StoreFile::Desired, payload);
            QVERIFY(!uncertain.success);
            QVERIFY(uncertain.committedButNotDurable);
            QCOMPARE(store.read(StoreFile::Desired).bytes, payload);
        }
        {
            StoreFixture fixture;
            QVERIFY(fixture.temporary.isValid());
            bool injectRemove = false;
            fixture.paths.faultHook = [&injectRemove](
                const StoreFaultPoint point,
                const StoreFile file
            ) {
                return injectRemove && file == StoreFile::Pending
                    && point
                        == StoreFaultPoint::AfterRemoveBeforeDirectorySync;
            };
            PersistentStore store(fixture.paths);
            QVERIFY(store.initialize().success);
            QVERIFY(store.write(StoreFile::Pending, payload).success);
            injectRemove = true;
            const auto uncertain = store.remove(StoreFile::Pending);
            QVERIFY(!uncertain.success);
            QVERIFY(uncertain.committedButNotDurable);
            QCOMPARE(store.read(StoreFile::Pending).status,
                     StoreReadStatus::Missing);
        }
    }

    void unsafePersistentTargetsAreNeverFollowedOrReplaced()
    {
        StoreFixture fixture;
        QVERIFY(fixture.temporary.isValid());
        PersistentStore store(fixture.paths);
        QVERIFY(store.initialize().success);
        const auto external = QDir(fixture.temporary.path()).filePath(
            QStringLiteral("external.json")
        );
        QVERIFY(writeFile(external, QByteArrayLiteral("external")));
        QVERIFY(::symlink(
                    QFile::encodeName(external).constData(),
                    QFile::encodeName(fixture.paths.desiredPath()).constData()
                ) == 0);
        QCOMPARE(store.read(StoreFile::Desired).status, StoreReadStatus::Unsafe);
        QCOMPARE(store.write(StoreFile::Desired, QByteArrayLiteral("new")).errorCode,
                 QStringLiteral("store.unsafe-target"));
        QCOMPARE(store.remove(StoreFile::Desired).errorCode,
                 QStringLiteral("store.unsafe-target"));
        QCOMPARE(readBytes(external), QByteArrayLiteral("external"));
        QVERIFY(QFileInfo(fixture.paths.desiredPath()).isSymLink());
    }

    void hardLinkedAndMutablePersistentFilesAreRejected()
    {
        {
            StoreFixture fixture;
            QVERIFY(fixture.temporary.isValid());
            PersistentStore store(fixture.paths);
            QVERIFY(store.initialize().success);
            const auto source = QDir(fixture.temporary.path()).filePath(
                QStringLiteral("source.json")
            );
            QVERIFY(writeFile(source, QByteArrayLiteral("hardlink")));
            QVERIFY(::link(
                        QFile::encodeName(source).constData(),
                        QFile::encodeName(fixture.paths.desiredPath()).constData()
                    ) == 0);
            QCOMPARE(store.read(StoreFile::Desired).status,
                     StoreReadStatus::Unsafe);
            QCOMPARE(store.write(StoreFile::Desired, QByteArrayLiteral("new"))
                         .errorCode,
                     QStringLiteral("store.unsafe-target"));
        }
        {
            StoreFixture fixture;
            QVERIFY(fixture.temporary.isValid());
            PersistentStore store(fixture.paths);
            QVERIFY(store.initialize().success);
            QVERIFY(writeFile(
                fixture.paths.desiredPath(), QByteArrayLiteral("mutable"), 0620
            ));
            QCOMPARE(store.read(StoreFile::Desired).status,
                     StoreReadStatus::Unsafe);
            QCOMPARE(store.remove(StoreFile::Desired).errorCode,
                     QStringLiteral("store.unsafe-target"));
        }
    }

    void generationInitializationCreatesCustomOnlyWhenAbsent()
    {
        StoreFixture fixture;
        QVERIFY(fixture.temporary.isValid());
        PersistentStore store(fixture.paths);
        QVERIFY(store.initialize().success);
        GenerationStore generations(store);
        const auto initialized = generations.initialize();
        QVERIFY2(initialized.success, qPrintable(initialized.errorMessage));
        QCOMPARE(readBytes(fixture.paths.userCustomPath()),
                 QByteArray(initialUserCustomContents));
        QCOMPARE(permissions(fixture.paths.userCustomPath()), mode_t(0600));
        QCOMPARE(permissions(fixture.paths.generationsPath()), mode_t(0700));
    }

    void existingCustomRegularFileAndSymlinkArePreservedExactly()
    {
        {
            StoreFixture fixture;
            QVERIFY(fixture.temporary.isValid());
            PersistentStore store(fixture.paths);
            QVERIFY(store.initialize().success);
            const QByteArray authored{"-- user content\n"};
            QVERIFY(writeFile(fixture.paths.userCustomPath(), authored, 0660));
            const auto before = metadata(fixture.paths.userCustomPath());
            GenerationStore generations(store);
            QVERIFY(generations.initialize().success);
            QCOMPARE(readBytes(fixture.paths.userCustomPath()), authored);
            const auto after = metadata(fixture.paths.userCustomPath());
            QVERIFY(sameIdentity(before, after));
            QCOMPARE(after.st_mode, before.st_mode);
        }
        {
            StoreFixture fixture;
            QVERIFY(fixture.temporary.isValid());
            PersistentStore store(fixture.paths);
            QVERIFY(store.initialize().success);
            const auto target = QDir(fixture.temporary.path()).filePath(
                QStringLiteral("custom-target.lua")
            );
            const QByteArray authored{"-- target content\n"};
            QVERIFY(writeFile(target, authored, 0640));
            QVERIFY(::symlink(
                        QFile::encodeName(target).constData(),
                        QFile::encodeName(fixture.paths.userCustomPath()).constData()
                    ) == 0);
            const auto before = metadata(fixture.paths.userCustomPath());
            GenerationStore generations(store);
            QVERIFY(generations.initialize().success);
            const auto after = metadata(fixture.paths.userCustomPath());
            QVERIFY(S_ISLNK(after.st_mode));
            QVERIFY(sameIdentity(before, after));
            QCOMPARE(readBytes(target), authored);
            QCOMPARE(permissions(target), mode_t(0640));
        }
    }

    void startupCleanupRemovesOnlyValidPendingNonceTrees()
    {
        StoreFixture fixture;
        QVERIFY(fixture.temporary.isValid());
        PersistentStore store(fixture.paths);
        QVERIFY(store.initialize().success);
        QVERIFY(makeDirectory(fixture.paths.generationsPath()));
        const auto valid = QDir(fixture.paths.generationsPath()).filePath(
            QStringLiteral(".pending-") + QString::fromLatin1(nonceA)
        );
        const auto arbitrary = QDir(fixture.paths.generationsPath()).filePath(
            QStringLiteral(".pending-not-a-nonce")
        );
        const auto uppercase = QDir(fixture.paths.generationsPath()).filePath(
            QStringLiteral(".pending-ABCDEF0123456789ABCDEF0123456789")
        );
        QVERIFY(makeDirectory(valid));
        QVERIFY(makeDirectory(arbitrary));
        QVERIFY(makeDirectory(uppercase));
        QVERIFY(writeFile(QDir(valid).filePath(QStringLiteral("partial")),
                          QByteArrayLiteral("partial")));
        QVERIFY(writeFile(QDir(arbitrary).filePath(QStringLiteral("keep")),
                          QByteArrayLiteral("keep")));
        QVERIFY(writeFile(QDir(uppercase).filePath(QStringLiteral("keep")),
                          QByteArrayLiteral("keep")));

        GenerationStore generations(store);
        const auto initialized = generations.initialize();
        QVERIFY2(initialized.success, qPrintable(initialized.errorMessage));
        QVERIFY(!QFileInfo::exists(valid));
        QVERIFY(QFileInfo(arbitrary).isDir());
        QVERIFY(QFileInfo(uppercase).isDir());
        QCOMPARE(readBytes(QDir(arbitrary).filePath(QStringLiteral("keep"))),
                 QByteArrayLiteral("keep"));
    }

    void publishedGenerationHasExactImmutableTreeAndIsIdempotent()
    {
        StoreFixture fixture;
        QVERIFY(fixture.temporary.isValid());
        PersistentStore store(fixture.paths);
        QVERIFY(store.initialize().success);
        GenerationStore generations(store);
        QVERIFY(generations.initialize().success);
        const auto rendered = renderDefault(fixture.paths);
        QVERIFY2(rendered, qPrintable(describeErrors(rendered.errors)));

        const auto published = generations.publish(*rendered.value);
        QVERIFY2(published.success, qPrintable(published.errorMessage));
        QVERIFY(published.generation.has_value());
        QCOMPARE(published.generation->id, rendered.value->generation);
        QCOMPARE(published.generation->nonce, QString::fromLatin1(nonceA));
        QCOMPARE(published.generation->manifest, rendered.value->manifestBytes);
        QCOMPARE(published.generation->snapshotDigest,
                 rendered.value->snapshotDigest);

        const auto root = generations.directoryForNonce(
            QString::fromLatin1(nonceA)
        );
        QCOMPARE(permissions(root), mode_t(0500));
        QCOMPARE(permissions(QDir(root).filePath(QStringLiteral("modules"))),
                 mode_t(0500));
        QCOMPARE(permissions(QDir(root).filePath(QStringLiteral("manifest.json"))),
                 mode_t(0400));
        for (auto iterator = rendered.value->files.constBegin();
             iterator != rendered.value->files.constEnd(); ++iterator) {
            const auto path = QDir(root).filePath(iterator.key());
            QCOMPARE(permissions(path), mode_t(0400));
            QCOMPARE(readBytes(path), iterator->contents);
            const auto info = metadata(path);
            QCOMPARE(info.st_uid, ::geteuid());
            QCOMPARE(info.st_nlink, nlink_t(1));
        }
        QCOMPARE(
            QDir(root).entryList(
                QDir::NoDotAndDotDot | QDir::AllEntries,
                QDir::Name
            ),
            (QStringList{
                QStringLiteral("hyprland.lua"),
                QStringLiteral("manifest.json"),
                QStringLiteral("modules"),
            })
        );

        const auto retry = generations.publish(*rendered.value);
        QVERIFY2(retry.success, qPrintable(retry.errorMessage));
        QCOMPARE(retry.generation->id, rendered.value->generation);
        QCOMPARE(retry.generation->manifest, rendered.value->manifestBytes);
    }

    void verificationRejectsPayloadTamperingAndExtraEntries()
    {
        StoreFixture fixture;
        QVERIFY(fixture.temporary.isValid());
        PersistentStore store(fixture.paths);
        QVERIFY(store.initialize().success);
        GenerationStore generations(store);
        QVERIFY(generations.initialize().success);
        const auto rendered = renderDefault(fixture.paths);
        QVERIFY(rendered);
        QVERIFY(generations.publish(*rendered.value).success);
        const auto root = generations.directoryForNonce(
            QString::fromLatin1(nonceA)
        );
        const auto entrypoint = QDir(root).filePath(QStringLiteral("hyprland.lua"));
        const auto original = readBytes(entrypoint);

        QVERIFY(::chmod(QFile::encodeName(root).constData(), 0700) == 0);
        QVERIFY(::chmod(QFile::encodeName(entrypoint).constData(), 0600) == 0);
        QFile changed(entrypoint);
        QVERIFY(changed.open(QIODevice::WriteOnly | QIODevice::Truncate));
        QCOMPARE(changed.write(QByteArrayLiteral("-- tampered\n")), qint64(12));
        changed.close();
        QVERIFY(::chmod(QFile::encodeName(entrypoint).constData(), 0400) == 0);
        QVERIFY(::chmod(QFile::encodeName(root).constData(), 0500) == 0);
        QVERIFY(!generations.verify(QString::fromLatin1(nonceA)).success);

        QVERIFY(::chmod(QFile::encodeName(root).constData(), 0700) == 0);
        QVERIFY(::chmod(QFile::encodeName(entrypoint).constData(), 0600) == 0);
        QVERIFY(QFile::remove(entrypoint));
        QVERIFY(writeFile(entrypoint, original, 0400));
        QVERIFY(writeFile(QDir(root).filePath(QStringLiteral("extra.lua")),
                          QByteArrayLiteral("-- extra\n"), 0400));
        QVERIFY(::chmod(QFile::encodeName(root).constData(), 0500) == 0);
        const auto extra = generations.verify(QString::fromLatin1(nonceA));
        QVERIFY(!extra.success);
        QCOMPARE(extra.errorCode,
                 QStringLiteral("generation.unexpected-tree-entry"));
    }

    void nonceConflictNeverReplacesAnExistingGeneration()
    {
        StoreFixture fixture;
        QVERIFY(fixture.temporary.isValid());
        PersistentStore store(fixture.paths);
        QVERIFY(store.initialize().success);
        GenerationStore generations(store);
        QVERIFY(generations.initialize().success);
        const auto first = renderDefault(fixture.paths);
        QVERIFY(first);
        QVERIFY(generations.publish(*first.value).success);

        const auto different = renderGeneration(
            defaultState,
            catalog,
            actionCatalog,
            QDir(fixture.paths.generationsPath()).filePath(
                QString::fromLatin1(nonceA)
            ),
            fixture.paths.userCustomPath(),
            QString::fromLatin1(nonceA),
            fixedTime().addSecs(1)
        );
        QVERIFY(different);
        QVERIFY(different.value->generation != first.value->generation);
        const auto rejected = generations.publish(*different.value);
        QVERIFY(!rejected.success);
        QCOMPARE(rejected.errorCode, QStringLiteral("generation.nonce-conflict"));
        const auto stillFirst = generations.verify(QString::fromLatin1(nonceA));
        QVERIFY(stillFirst.success);
        QCOMPARE(stillFirst.generation->id, first.value->generation);
    }
};

QTEST_MAIN(CompositorStoreTest)

#include "compositor_store_test.moc"
