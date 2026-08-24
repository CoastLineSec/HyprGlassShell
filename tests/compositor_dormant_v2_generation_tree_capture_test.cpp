#include "compositord/dormant_v2_generation_tree_capture.h"
#include "compositord/settled_v2_generation_byte_graph.h"

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QTemporaryDir>
#include <QtTest>

#include <array>
#include <concepts>
#include <type_traits>
#include <utility>

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

using namespace HyprShelld::Compositor;

namespace {

constexpr auto nonceA = "0123456789abcdef0123456789abcdef";
constexpr auto nonceB = "fedcba9876543210fedcba9876543210";

struct TestFileSpec final {
  const char *relativePath = nullptr;
  DormantV2GenerationTreeFile file = DormantV2GenerationTreeFile::None;
};

constexpr std::array<TestFileSpec, 18> testFiles{{
    {"manifest.json", DormantV2GenerationTreeFile::Manifest},
    {"hyprland.lua", DormantV2GenerationTreeFile::Entrypoint},
    {"modules/00-session.lua", DormantV2GenerationTreeFile::Module00Session},
    {"modules/10-monitors.lua", DormantV2GenerationTreeFile::Module10Monitors},
    {"modules/20-environment.lua",
     DormantV2GenerationTreeFile::Module20Environment},
    {"modules/30-input.lua", DormantV2GenerationTreeFile::Module30Input},
    {"modules/31-gestures.lua", DormantV2GenerationTreeFile::Module31Gestures},
    {"modules/32-cursor.lua", DormantV2GenerationTreeFile::Module32Cursor},
    {"modules/40-general.lua", DormantV2GenerationTreeFile::Module40General},
    {"modules/41-layouts.lua", DormantV2GenerationTreeFile::Module41Layouts},
    {"modules/42-workspaces.lua",
     DormantV2GenerationTreeFile::Module42Workspaces},
    {"modules/43-groups.lua", DormantV2GenerationTreeFile::Module43Groups},
    {"modules/50-decorations.lua",
     DormantV2GenerationTreeFile::Module50Decorations},
    {"modules/51-animations.lua",
     DormantV2GenerationTreeFile::Module51Animations},
    {"modules/60-rules.lua", DormantV2GenerationTreeFile::Module60Rules},
    {"modules/70-keybinds.lua", DormantV2GenerationTreeFile::Module70Keybinds},
    {"modules/80-permissions.lua",
     DormantV2GenerationTreeFile::Module80Permissions},
    {"modules/90-advanced.lua", DormantV2GenerationTreeFile::Module90Advanced},
}};

[[nodiscard]] QByteArray bytesFor(const QByteArray &tag,
                                  const QByteArray &path) {
  return tag + ':' + path + '\n';
}

[[nodiscard]] bool writeFile(const QString &path, const QByteArray &bytes,
                             const mode_t mode = 0400) {
  QFile file(path);
  if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate) ||
      file.write(bytes) != bytes.size() || !file.flush()) {
    return false;
  }
  file.close();
  return ::chmod(QFile::encodeName(path).constData(), mode) == 0;
}

class Fixture final {
public:
  Fixture() { temporary_.setAutoRemove(false); }

  ~Fixture() {
    for (const auto descriptor : extraDescriptors_) {
      if (descriptor >= 0)
        ::close(descriptor);
    }
    if (!temporary_.isValid())
      return;
    ::chmod(QFile::encodeName(temporary_.path()).constData(), 0700);
    QDirIterator iterator(temporary_.path(),
                          QDir::AllEntries | QDir::Hidden | QDir::System |
                              QDir::NoDotAndDotDot,
                          QDirIterator::Subdirectories);
    while (iterator.hasNext()) {
      const auto path = iterator.next();
      const QFileInfo info(path);
      if (!info.isSymLink()) {
        ::chmod(QFile::encodeName(path).constData(),
                info.isDir() ? 0700 : 0600);
      }
    }
    QDir(temporary_.path()).removeRecursively();
  }

  [[nodiscard]] bool valid() const { return temporary_.isValid(); }
  [[nodiscard]] QString root() const { return temporary_.path(); }

  [[nodiscard]] QString treeRoot(const QString &nonce) const {
    return QDir(root()).filePath(nonce);
  }

  [[nodiscard]] QString path(const QString &nonce,
                             const QString &relativePath) const {
    return QDir(treeRoot(nonce)).filePath(relativePath);
  }

  [[nodiscard]] int openRoot(const int flags = O_RDONLY | O_DIRECTORY |
                                               O_CLOEXEC) const {
    return ::open(QFile::encodeName(root()).constData(), flags);
  }

  [[nodiscard]] bool addTree(const QString &nonce, const QByteArray &tag,
                             const QString &emptyRelativePath = {}) {
    const auto generation = treeRoot(nonce);
    const auto modules = QDir(generation).filePath(QStringLiteral("modules"));
    if (!QDir().mkpath(modules))
      return false;
    for (const auto &spec : testFiles) {
      const auto relative = QString::fromLatin1(spec.relativePath);
      const auto bytes = relative == emptyRelativePath
                             ? QByteArray{}
                             : bytesFor(tag, spec.relativePath);
      if (!writeFile(path(nonce, relative), bytes))
        return false;
    }
    return ::chmod(QFile::encodeName(modules).constData(), 0500) == 0 &&
           ::chmod(QFile::encodeName(generation).constData(), 0500) == 0;
  }

  [[nodiscard]] bool makeParentWritable(const QString &nonce,
                                        const QString &relativePath,
                                        const bool writable) const {
    const auto parent = QFileInfo(path(nonce, relativePath)).absolutePath();
    return ::chmod(QFile::encodeName(parent).constData(),
                   writable ? 0700 : 0500) == 0;
  }

  [[nodiscard]] bool removeFile(const QString &nonce,
                                const QString &relativePath) const {
    if (!makeParentWritable(nonce, relativePath, true))
      return false;
    const auto removed =
        ::unlink(QFile::encodeName(path(nonce, relativePath)).constData()) == 0;
    const auto restored = makeParentWritable(nonce, relativePath, false);
    return removed && restored;
  }

  [[nodiscard]] bool addExtra(const QString &nonce,
                              const QString &relativePath) const {
    if (!makeParentWritable(nonce, relativePath, true))
      return false;
    const auto written = writeFile(path(nonce, relativePath), QByteArray("x"));
    const auto restored = makeParentWritable(nonce, relativePath, false);
    return written && restored;
  }

  [[nodiscard]] bool replaceWithSymlink(const QString &nonce,
                                        const QString &relativePath) const {
    if (!removeFile(nonce, relativePath) ||
        !makeParentWritable(nonce, relativePath, true)) {
      return false;
    }
    const auto linked =
        ::symlink("/dev/null",
                  QFile::encodeName(path(nonce, relativePath)).constData()) ==
        0;
    const auto restored = makeParentWritable(nonce, relativePath, false);
    return linked && restored;
  }

  [[nodiscard]] bool replaceWithFifo(const QString &nonce,
                                     const QString &relativePath) const {
    if (!removeFile(nonce, relativePath) ||
        !makeParentWritable(nonce, relativePath, true)) {
      return false;
    }
    const auto created =
        ::mkfifo(QFile::encodeName(path(nonce, relativePath)).constData(),
                 0400) == 0;
    const auto restored = makeParentWritable(nonce, relativePath, false);
    return created && restored;
  }

  [[nodiscard]] bool
  replaceWithHardLink(const QString &nonce, const QString &sourceRelativePath,
                      const QString &targetRelativePath) const {
    if (!removeFile(nonce, targetRelativePath) ||
        !makeParentWritable(nonce, targetRelativePath, true)) {
      return false;
    }
    const auto linked =
        ::link(
            QFile::encodeName(path(nonce, sourceRelativePath)).constData(),
            QFile::encodeName(path(nonce, targetRelativePath)).constData()) ==
        0;
    const auto restored = makeParentWritable(nonce, targetRelativePath, false);
    return linked && restored;
  }

  [[nodiscard]] bool resizeFile(const QString &nonce,
                                const QString &relativePath,
                                const off_t size) const {
    const auto absolute = path(nonce, relativePath);
    if (::chmod(QFile::encodeName(absolute).constData(), 0600) != 0)
      return false;
    const auto resized =
        ::truncate(QFile::encodeName(absolute).constData(), size) == 0;
    const auto restored =
        ::chmod(QFile::encodeName(absolute).constData(), 0400) == 0;
    return resized && restored;
  }

  [[nodiscard]] int openWritableForMutation(const QString &nonce,
                                            const QString &relativePath) {
    const auto absolute = path(nonce, relativePath);
    if (::chmod(QFile::encodeName(absolute).constData(), 0600) != 0)
      return -1;
    const auto descriptor =
        ::open(QFile::encodeName(absolute).constData(), O_RDWR | O_CLOEXEC);
    const auto restored =
        ::chmod(QFile::encodeName(absolute).constData(), 0400) == 0;
    if (descriptor < 0 || !restored) {
      if (descriptor >= 0)
        ::close(descriptor);
      return -1;
    }
    extraDescriptors_.append(descriptor);
    return descriptor;
  }

private:
  QTemporaryDir temporary_;
  QVector<int> extraDescriptors_;
};

void expectFailure(const DormantV2GenerationTreeCaptureResult &result,
                   const DormantV2GenerationTreeCaptureReason reason,
                   const DormantV2GenerationTreeCaptureSubject subject =
                       DormantV2GenerationTreeCaptureSubject::None,
                   const qsizetype generationIndex = -1,
                   const DormantV2GenerationTreeFile file =
                       DormantV2GenerationTreeFile::None) {
  QCOMPARE(result.disposition(),
           DormantV2GenerationTreeCaptureDisposition::FailedClosed);
  QCOMPARE(result.reason(), reason);
  QCOMPARE(result.subject(), subject);
  QCOMPARE(result.generationIndex(), generationIndex);
  QCOMPARE(result.file(), file);
  QVERIFY(!result.capture());
}

[[nodiscard]] QMap<QString, QByteArray>
expectedFiles(const QByteArray &tag, const QString &emptyRelativePath = {}) {
  QMap<QString, QByteArray> result;
  for (size_t index = 1; index < testFiles.size(); ++index) {
    const auto relative = QString::fromLatin1(testFiles[index].relativePath);
    result.insert(relative, relative == emptyRelativePath
                                ? QByteArray{}
                                : bytesFor(tag, testFiles[index].relativePath));
  }
  return result;
}

class HookReset final {
public:
  ~HookReset() { DormantV2GenerationTreeCaptureTestSupport::clearHooks(); }
};

[[nodiscard]] qsizetype openDescriptorCount() {
  return QDir(QStringLiteral("/proc/self/fd"))
      .entryList(QDir::AllEntries | QDir::NoDotAndDotDot)
      .size();
}

} // namespace

class CompositorDormantV2GenerationTreeCaptureTest final : public QObject {
  Q_OBJECT

private slots:
  void init() { DormantV2GenerationTreeCaptureTestSupport::clearHooks(); }

  void constantsAndSignatureAreFrozen() {
    using Function =
        DormantV2GenerationTreeCaptureResult (*)(int, const QVector<QString> &);
    static_assert(
        std::same_as<decltype(&captureDormantV2GenerationTrees), Function>);
    static_assert(
        std::is_invocable_r_v<DormantV2GenerationTreeCaptureResult, Function,
                              int, const QVector<QString> &>);
    static_assert(!std::is_invocable_v<Function, int>);
    static_assert(!std::is_invocable_v<Function, int, QString>);
    static_assert(
        !std::is_invocable_v<Function, int, QVector<QString>, QString>);
    static_assert(
        !std::is_default_constructible_v<DormantV2GenerationTreeCapture>);
    static_assert(
        !std::is_default_constructible_v<DormantV2GenerationTreesCapture>);
    static_assert(
        !std::is_default_constructible_v<DormantV2GenerationTreeCaptureResult>);
    static_assert(
        !std::is_constructible_v<DormantV2GenerationTreeCapture, QString,
                                 QByteArray, QMap<QString, QByteArray>>);
    static_assert(
        !std::is_constructible_v<DormantV2GenerationTreesCapture,
                                 QVector<DormantV2GenerationTreeCapture>>);
    static_assert(!std::is_constructible_v<SettledV2GenerationEvidence,
                                           DormantV2GenerationTreeCapture>);
    static_assert(!std::is_convertible_v<DormantV2GenerationTreeCapture,
                                         SettledV2GenerationEvidence>);

    QCOMPARE(maximumDormantV2CapturedGenerationTrees, 2);
    QCOMPARE(dormantV2GenerationPayloadFileCount, 17);
    QCOMPARE(maximumDormantV2GenerationManifestCaptureBytes, 4194304);
    QCOMPARE(maximumDormantV2GeneratedFileCaptureBytes, 16777216);
    QCOMPARE(dormantV2GenerationCaptureReadBufferBytes, 65536);
    QCOMPARE(maximumDormantV2GenerationRetainedPayloadBytes, 578813952ULL);
    QCOMPARE(maximumDormantV2GenerationTwoPassPayloadBytes, 1157627904ULL);
    QCOMPARE(maximumDormantV2GenerationPreadReturnedBytes, 1157627905ULL);
    QCOMPARE(maximumDormantV2GenerationStreamingPayloadBytes, 595591168ULL);
    QCOMPARE(maximumDormantV2GenerationWorkingPayloadBytes, 595656704ULL);
    QCOMPARE(maximumDormantV2GenerationEnumerationCalls, 128ULL);
    QCOMPARE(maximumDormantV2GenerationObservedNonDotDirents, 84ULL);
    QCOMPARE(maximumDormantV2GenerationDirentNameBytes, 1048576ULL);
    QCOMPARE(maximumDormantV2GenerationProofSyscallAttempts, 65536ULL);
    QCOMPARE(dormantV2GenerationFinalProofAttemptReserve, 256ULL);
    QCOMPARE(dormantV2GenerationCleanupAttemptReserve, 256ULL);
    QCOMPARE(maximumDormantV2GenerationSyscallAttempts, 66048ULL);
    QCOMPARE(maximumDormantV2GenerationRetainedDescriptors, 41);
    QCOMPARE(maximumDormantV2GenerationOwnedDescriptors, 42);
  }

  void zeroReferencesStillQualifiesTheRoot() {
    Fixture fixture;
    QVERIFY(fixture.valid());
    const auto descriptor = fixture.openRoot();
    QVERIFY(descriptor >= 0);

    const auto result = captureDormantV2GenerationTrees(descriptor, {});
    ::close(descriptor);

    QCOMPARE(result.disposition(),
             DormantV2GenerationTreeCaptureDisposition::Captured);
    QCOMPARE(result.reason(), DormantV2GenerationTreeCaptureReason::None);
    QCOMPARE(result.subject(), DormantV2GenerationTreeCaptureSubject::None);
    QCOMPARE(result.generationIndex(), -1);
    QCOMPARE(result.file(), DormantV2GenerationTreeFile::None);
    QVERIFY(result.capture());
    QVERIFY(result.capture()->trees().isEmpty());

    const auto invalid = captureDormantV2GenerationTrees(-1, {});
    expectFailure(invalid,
                  DormantV2GenerationTreeCaptureReason::
                      InvalidGenerationsDirectoryDescriptor,
                  DormantV2GenerationTreeCaptureSubject::GenerationsDirectory);
  }

  void inputGrammarPrecedesEverySyscall() {
    using namespace DormantV2GenerationTreeCaptureTestSupport;
    HookReset reset;
    quint64 syscallCount = 0;
    setSyscallHook([&](const SyscallEvent &) {
      ++syscallCount;
      return Fault::None;
    });

    expectFailure(
        captureDormantV2GenerationTrees(
            -1, {QString::fromLatin1(nonceA), QString::fromLatin1(nonceB),
                 QStringLiteral("11111111111111111111111111111111")}),
        DormantV2GenerationTreeCaptureReason::TooManyGenerationReferences);
    QCOMPARE(syscallCount, 0ULL);

    const std::array<QString, 6> invalid{{
        QString(),
        QString(31, QLatin1Char('a')),
        QString(33, QLatin1Char('a')),
        QString(32, QLatin1Char('0')),
        QStringLiteral("0123456789ABCDEF0123456789abcdef"),
        QStringLiteral("0123456789abcdef0123456789abcdeg"),
    }};
    for (const auto &nonce : invalid) {
      expectFailure(
          captureDormantV2GenerationTrees(-1, {nonce}),
          DormantV2GenerationTreeCaptureReason::InvalidActivationNonce,
          DormantV2GenerationTreeCaptureSubject::None, 0);
      QCOMPARE(syscallCount, 0ULL);
    }
    expectFailure(
        captureDormantV2GenerationTrees(
            -1, {QString::fromLatin1(nonceA), QString::fromLatin1(nonceA)}),
        DormantV2GenerationTreeCaptureReason::DuplicateActivationNonce,
        DormantV2GenerationTreeCaptureSubject::None, 1);
    QCOMPARE(syscallCount, 0ULL);
  }

  void capturesOneExactTreeAndPreservesEmptyBytes() {
    Fixture fixture;
    QVERIFY(fixture.addTree(QString::fromLatin1(nonceA), QByteArray("A"),
                            QStringLiteral("modules/31-gestures.lua")));
    const auto descriptor = fixture.openRoot();
    QVERIFY(descriptor >= 0);
    QVector<QString> requests{QString::fromLatin1(nonceA)};
    const auto result = captureDormantV2GenerationTrees(descriptor, requests);
    ::close(descriptor);
    requests[0].fill(QLatin1Char('f'));

    QCOMPARE(result.disposition(),
             DormantV2GenerationTreeCaptureDisposition::Captured);
    QVERIFY(result.capture());
    QCOMPARE(result.capture()->trees().size(), 1);
    const auto &tree = result.capture()->trees().first();
    QCOMPARE(tree.activationNonce(), QString::fromLatin1(nonceA));
    QCOMPARE(tree.manifestBytes(), bytesFor("A", "manifest.json"));
    QCOMPARE(tree.files(),
             expectedFiles("A", QStringLiteral("modules/31-gestures.lua")));
    QCOMPARE(tree.files().size(), 17);
    QVERIFY(tree.files()
                .value(QStringLiteral("modules/31-gestures.lua"))
                .isEmpty());
  }

  void reversedRequestsProduceSortedOwnedTrees() {
    Fixture fixture;
    QVERIFY(fixture.addTree(QString::fromLatin1(nonceA), QByteArray("A")));
    QVERIFY(fixture.addTree(QString::fromLatin1(nonceB), QByteArray("B")));
    const auto descriptor = fixture.openRoot();
    QVERIFY(descriptor >= 0);
    const auto result = captureDormantV2GenerationTrees(
        descriptor, {QString::fromLatin1(nonceB), QString::fromLatin1(nonceA)});
    ::close(descriptor);

    QVERIFY(result.capture());
    QCOMPARE(result.capture()->trees().size(), 2);
    QCOMPARE(result.capture()->trees().at(0).activationNonce(),
             QString::fromLatin1(nonceA));
    QCOMPARE(result.capture()->trees().at(1).activationNonce(),
             QString::fromLatin1(nonceB));
    QCOMPARE(result.capture()->trees().at(0).manifestBytes(),
             bytesFor("A", "manifest.json"));
    QCOMPARE(result.capture()->trees().at(1).manifestBytes(),
             bytesFor("B", "manifest.json"));
  }

  void failureDiagnosticsKeepOriginalRequestIndex() {
    Fixture fixture;
    QVERIFY(fixture.addTree(QString::fromLatin1(nonceB), QByteArray("B")));
    const auto descriptor = fixture.openRoot();
    QVERIFY(descriptor >= 0);
    const auto result = captureDormantV2GenerationTrees(
        descriptor, {QString::fromLatin1(nonceB), QString::fromLatin1(nonceA)});
    ::close(descriptor);

    expectFailure(result,
                  DormantV2GenerationTreeCaptureReason::GenerationRootMissing,
                  DormantV2GenerationTreeCaptureSubject::GenerationRoot, 1);
  }

  void rootDescriptorFlagsAndMetadataFailClosed() {
    Fixture fixture;
    QVERIFY(fixture.valid());

    const auto pathDescriptor =
        fixture.openRoot(O_PATH | O_DIRECTORY | O_CLOEXEC);
    QVERIFY(pathDescriptor >= 0);
    const auto pathResult = captureDormantV2GenerationTrees(pathDescriptor, {});
    ::close(pathDescriptor);
    expectFailure(
        pathResult,
        DormantV2GenerationTreeCaptureReason::UnsafeGenerationsDirectory,
        DormantV2GenerationTreeCaptureSubject::GenerationsDirectory);

    QVERIFY(::chmod(QFile::encodeName(fixture.root()).constData(), 0755) == 0);
    const auto wrongMode = fixture.openRoot();
    QVERIFY(wrongMode >= 0);
    const auto modeResult = captureDormantV2GenerationTrees(wrongMode, {});
    ::close(wrongMode);
    expectFailure(
        modeResult,
        DormantV2GenerationTreeCaptureReason::UnsafeGenerationsDirectory,
        DormantV2GenerationTreeCaptureSubject::GenerationsDirectory);

    QFile ordinary(QDir(fixture.root()).filePath(QStringLiteral("ordinary")));
    QVERIFY(ordinary.open(QIODevice::WriteOnly));
    ordinary.close();
    const auto fileDescriptor =
        ::open(QFile::encodeName(ordinary.fileName()).constData(),
               O_RDONLY | O_CLOEXEC);
    QVERIFY(fileDescriptor >= 0);
    const auto fileResult = captureDormantV2GenerationTrees(fileDescriptor, {});
    ::close(fileDescriptor);
    expectFailure(
        fileResult,
        DormantV2GenerationTreeCaptureReason::UnsafeGenerationsDirectory,
        DormantV2GenerationTreeCaptureSubject::GenerationsDirectory);
  }

  void callerMayCloseAndReuseBorrowedFdAfterDuplicate() {
    using namespace DormantV2GenerationTreeCaptureTestSupport;
    HookReset reset;
    Fixture fixture;
    QVERIFY(fixture.addTree(QString::fromLatin1(nonceA), QByteArray("A")));
    const auto borrowed = fixture.openRoot();
    QVERIFY(borrowed >= 0);
    auto reused = -1;
    setCheckpointHook([&](const CheckpointEvent &event) {
      if (event.checkpoint != Checkpoint::AfterRootDuplicate)
        return;
      QVERIFY(::close(borrowed) == 0);
      reused = ::open("/dev/null", O_RDONLY | O_CLOEXEC);
      QVERIFY(reused >= 0);
    });

    const auto result = captureDormantV2GenerationTrees(
        borrowed, {QString::fromLatin1(nonceA)});
    if (reused >= 0)
      ::close(reused);
    QVERIFY(result.capture());
    QCOMPARE(result.capture()->trees().first().manifestBytes(),
             bytesFor("A", "manifest.json"));
  }

  void directoryAndInventoryViolationsFailClosed() {
    {
      Fixture fixture;
      QVERIFY(fixture.addTree(QString::fromLatin1(nonceA), QByteArray("A")));
      QVERIFY(::chmod(QFile::encodeName(
                          fixture.treeRoot(QString::fromLatin1(nonceA)))
                          .constData(),
                      0700) == 0);
      const auto descriptor = fixture.openRoot();
      const auto result = captureDormantV2GenerationTrees(
          descriptor, {QString::fromLatin1(nonceA)});
      ::close(descriptor);
      expectFailure(result,
                    DormantV2GenerationTreeCaptureReason::GenerationRootUnsafe,
                    DormantV2GenerationTreeCaptureSubject::GenerationRoot, 0);
    }
    {
      Fixture fixture;
      QVERIFY(fixture.addTree(QString::fromLatin1(nonceA), QByteArray("A")));
      const auto modules =
          fixture.path(QString::fromLatin1(nonceA), QStringLiteral("modules"));
      QVERIFY(::chmod(QFile::encodeName(
                          fixture.treeRoot(QString::fromLatin1(nonceA)))
                          .constData(),
                      0700) == 0);
      const auto movedModules = modules + QStringLiteral(".gone");
      QVERIFY(::rename(QFile::encodeName(modules).constData(),
                       QFile::encodeName(movedModules).constData()) == 0);
      QVERIFY(::chmod(QFile::encodeName(
                          fixture.treeRoot(QString::fromLatin1(nonceA)))
                          .constData(),
                      0500) == 0);
      const auto descriptor = fixture.openRoot();
      const auto result = captureDormantV2GenerationTrees(
          descriptor, {QString::fromLatin1(nonceA)});
      ::close(descriptor);
      expectFailure(
          result, DormantV2GenerationTreeCaptureReason::ModulesDirectoryMissing,
          DormantV2GenerationTreeCaptureSubject::ModulesDirectory, 0);
    }
    {
      Fixture fixture;
      QVERIFY(fixture.addTree(QString::fromLatin1(nonceA), QByteArray("A")));
      QVERIFY(fixture.addExtra(QString::fromLatin1(nonceA),
                               QStringLiteral("unexpected.lua")));
      const auto descriptor = fixture.openRoot();
      const auto result = captureDormantV2GenerationTrees(
          descriptor, {QString::fromLatin1(nonceA)});
      ::close(descriptor);
      expectFailure(result,
                    DormantV2GenerationTreeCaptureReason::InventoryMismatch,
                    DormantV2GenerationTreeCaptureSubject::GenerationRoot, 0);
    }
    {
      Fixture fixture;
      QVERIFY(fixture.addTree(QString::fromLatin1(nonceA), QByteArray("A")));
      QVERIFY(fixture.addExtra(QString::fromLatin1(nonceA),
                               QStringLiteral("modules/unexpected.lua")));
      const auto descriptor = fixture.openRoot();
      const auto result = captureDormantV2GenerationTrees(
          descriptor, {QString::fromLatin1(nonceA)});
      ::close(descriptor);
      expectFailure(result,
                    DormantV2GenerationTreeCaptureReason::InventoryMismatch,
                    DormantV2GenerationTreeCaptureSubject::ModulesDirectory, 0);
    }
  }

  void everyMissingRequiredInventoryEntryHasFixedDirectoryDiagnostics() {
    for (const auto &spec : testFiles) {
      Fixture fixture;
      QVERIFY(fixture.addTree(QString::fromLatin1(nonceA), QByteArray("A")));
      QVERIFY(fixture.removeFile(QString::fromLatin1(nonceA),
                                 QString::fromLatin1(spec.relativePath)));
      const auto descriptor = fixture.openRoot();
      QVERIFY(descriptor >= 0);
      const auto result = captureDormantV2GenerationTrees(
          descriptor, {QString::fromLatin1(nonceA)});
      ::close(descriptor);
      expectFailure(
          result, DormantV2GenerationTreeCaptureReason::InventoryMismatch,
          spec.file == DormantV2GenerationTreeFile::Manifest ||
                  spec.file == DormantV2GenerationTreeFile::Entrypoint
              ? DormantV2GenerationTreeCaptureSubject::GenerationRoot
              : DormantV2GenerationTreeCaptureSubject::ModulesDirectory,
          0);
    }
  }

  void unsafeRequiredFileReportsItsFixedOrdinal() {
    for (const auto &spec : testFiles) {
      Fixture fixture;
      QVERIFY(fixture.addTree(QString::fromLatin1(nonceA), QByteArray("A")));
      const auto absolute = fixture.path(
          QString::fromLatin1(nonceA), QString::fromLatin1(spec.relativePath));
      QVERIFY(::chmod(QFile::encodeName(absolute).constData(), 0600) == 0);
      const auto descriptor = fixture.openRoot();
      const auto result = captureDormantV2GenerationTrees(
          descriptor, {QString::fromLatin1(nonceA)});
      ::close(descriptor);
      const auto subject = spec.file == DormantV2GenerationTreeFile::Manifest
                               ? DormantV2GenerationTreeCaptureSubject::Manifest
                               : DormantV2GenerationTreeCaptureSubject::Payload;
      expectFailure(result, DormantV2GenerationTreeCaptureReason::FileUnsafe,
                    subject, 0, spec.file);
    }
  }

  void symlinkedRequiredFileIsUnsafeNotMissing() {
    Fixture fixture;
    QVERIFY(fixture.addTree(QString::fromLatin1(nonceA), QByteArray("A")));
    QVERIFY(fixture.replaceWithSymlink(QString::fromLatin1(nonceA),
                                       QStringLiteral("hyprland.lua")));
    const auto descriptor = fixture.openRoot();
    const auto result = captureDormantV2GenerationTrees(
        descriptor, {QString::fromLatin1(nonceA)});
    ::close(descriptor);
    expectFailure(result, DormantV2GenerationTreeCaptureReason::FileUnsafe,
                  DormantV2GenerationTreeCaptureSubject::Payload, 0,
                  DormantV2GenerationTreeFile::Entrypoint);
  }

  void hardLinksAndSpecialFilesAreRejected() {
    {
      Fixture fixture;
      QVERIFY(fixture.addTree(QString::fromLatin1(nonceA), QByteArray("A")));
      QVERIFY(fixture.replaceWithHardLink(
          QString::fromLatin1(nonceA), QStringLiteral("modules/00-session.lua"),
          QStringLiteral("modules/10-monitors.lua")));
      const auto descriptor = fixture.openRoot();
      const auto result = captureDormantV2GenerationTrees(
          descriptor, {QString::fromLatin1(nonceA)});
      ::close(descriptor);
      expectFailure(result, DormantV2GenerationTreeCaptureReason::FileUnsafe,
                    DormantV2GenerationTreeCaptureSubject::Payload, 0,
                    DormantV2GenerationTreeFile::Module00Session);
    }
    {
      Fixture fixture;
      QVERIFY(fixture.addTree(QString::fromLatin1(nonceA), QByteArray("A")));
      QVERIFY(fixture.replaceWithFifo(QString::fromLatin1(nonceA),
                                      QStringLiteral("hyprland.lua")));
      const auto descriptor = fixture.openRoot();
      const auto result = captureDormantV2GenerationTrees(
          descriptor, {QString::fromLatin1(nonceA)});
      ::close(descriptor);
      expectFailure(result, DormantV2GenerationTreeCaptureReason::FileUnsafe,
                    DormantV2GenerationTreeCaptureSubject::Payload, 0,
                    DormantV2GenerationTreeFile::Entrypoint);
    }
  }

  void exactFileCeilingsSucceedAndOneByteOverFailsBeforeReading() {
    {
      Fixture fixture;
      QVERIFY(fixture.addTree(QString::fromLatin1(nonceA), QByteArray("A")));
      const auto manifestPath = fixture.path(QString::fromLatin1(nonceA),
                                             QStringLiteral("manifest.json"));
      const auto payloadPath = fixture.path(QString::fromLatin1(nonceA),
                                            QStringLiteral("hyprland.lua"));
      QVERIFY(::chmod(QFile::encodeName(manifestPath).constData(), 0600) == 0);
      QVERIFY(writeFile(
          manifestPath,
          QByteArray(maximumDormantV2GenerationManifestCaptureBytes, 'm')));
      QVERIFY(::chmod(QFile::encodeName(payloadPath).constData(), 0600) == 0);
      QVERIFY(writeFile(
          payloadPath,
          QByteArray(maximumDormantV2GeneratedFileCaptureBytes, 'p')));
      const auto descriptor = fixture.openRoot();
      const auto result = captureDormantV2GenerationTrees(
          descriptor, {QString::fromLatin1(nonceA)});
      ::close(descriptor);
      QVERIFY(result.capture());
      QCOMPARE(result.capture()->trees().first().manifestBytes().size(),
               maximumDormantV2GenerationManifestCaptureBytes);
      QCOMPARE(result.capture()
                   ->trees()
                   .first()
                   .files()
                   .value(QStringLiteral("hyprland.lua"))
                   .size(),
               maximumDormantV2GeneratedFileCaptureBytes);
    }
    {
      Fixture fixture;
      QVERIFY(fixture.addTree(QString::fromLatin1(nonceA), QByteArray("A")));
      QVERIFY(fixture.resizeFile(
          QString::fromLatin1(nonceA), QStringLiteral("manifest.json"),
          maximumDormantV2GenerationManifestCaptureBytes + 1));
      const auto descriptor = fixture.openRoot();
      const auto result = captureDormantV2GenerationTrees(
          descriptor, {QString::fromLatin1(nonceA)});
      ::close(descriptor);
      expectFailure(result, DormantV2GenerationTreeCaptureReason::FileUnsafe,
                    DormantV2GenerationTreeCaptureSubject::Manifest, 0,
                    DormantV2GenerationTreeFile::Manifest);
    }
    {
      Fixture fixture;
      QVERIFY(fixture.addTree(QString::fromLatin1(nonceA), QByteArray("A")));
      QVERIFY(fixture.resizeFile(
          QString::fromLatin1(nonceA), QStringLiteral("hyprland.lua"),
          maximumDormantV2GeneratedFileCaptureBytes + 1));
      const auto descriptor = fixture.openRoot();
      const auto result = captureDormantV2GenerationTrees(
          descriptor, {QString::fromLatin1(nonceA)});
      ::close(descriptor);
      expectFailure(result, DormantV2GenerationTreeCaptureReason::FileUnsafe,
                    DormantV2GenerationTreeCaptureSubject::Payload, 0,
                    DormantV2GenerationTreeFile::Entrypoint);
    }
  }

  void passOrderIsGlobalSortedAndRetainsFortyOneDescriptors() {
    using namespace DormantV2GenerationTreeCaptureTestSupport;
    HookReset reset;
    Fixture fixture;
    QVERIFY(fixture.addTree(QString::fromLatin1(nonceA), QByteArray("A")));
    QVERIFY(fixture.addTree(QString::fromLatin1(nonceB), QByteArray("B")));
    struct Observation final {
      Pass pass = Pass::First;
      qsizetype originalIndex = -1;
    };
    QVector<Observation> observations;
    qsizetype maximumRetained = 0;
    setCheckpointHook([&](const CheckpointEvent &event) {
      maximumRetained =
          std::max(maximumRetained, event.heldFirstPassDescriptors);
      if (event.checkpoint == Checkpoint::AfterFile) {
        observations.append({
            .pass = event.pass,
            .originalIndex = event.generationIndex,
        });
      }
    });
    const auto descriptor = fixture.openRoot();
    QVERIFY(descriptor >= 0);
    const auto result = captureDormantV2GenerationTrees(
        descriptor, {QString::fromLatin1(nonceB), QString::fromLatin1(nonceA)});
    QVERIFY(::fcntl(descriptor, F_GETFD) >= 0);
    ::close(descriptor);

    QVERIFY(result.capture());
    QCOMPARE(observations.size(), 72);
    for (qsizetype index = 0; index < observations.size(); ++index) {
      const auto expectedPass = index < 36 ? Pass::First : Pass::Second;
      const auto withinPass = index % 36;
      const auto expectedOriginalIndex = withinPass < 18 ? 1 : 0;
      QCOMPARE(observations.at(index).pass, expectedPass);
      QCOMPARE(observations.at(index).originalIndex, expectedOriginalIndex);
    }
    QCOMPARE(maximumRetained, maximumDormantV2GenerationRetainedDescriptors);
  }

  void aPostInventoryLookupRaceHasFixedFileDiagnostics() {
    using namespace DormantV2GenerationTreeCaptureTestSupport;
    HookReset reset;
    Fixture fixture;
    QVERIFY(fixture.addTree(QString::fromLatin1(nonceA), QByteArray("A")));
    auto removed = false;
    setCheckpointHook([&](const CheckpointEvent &event) {
      if (removed || event.checkpoint != Checkpoint::BeforeLookup ||
          event.pass != Pass::First ||
          event.file != DormantV2GenerationTreeFile::Entrypoint) {
        return;
      }
      QVERIFY(fixture.removeFile(QString::fromLatin1(nonceA),
                                 QStringLiteral("hyprland.lua")));
      removed = true;
    });
    const auto descriptor = fixture.openRoot();
    const auto result = captureDormantV2GenerationTrees(
        descriptor, {QString::fromLatin1(nonceA)});
    ::close(descriptor);
    QVERIFY(removed);
    expectFailure(result, DormantV2GenerationTreeCaptureReason::FileMissing,
                  DormantV2GenerationTreeCaptureSubject::Payload, 0,
                  DormantV2GenerationTreeFile::Entrypoint);
  }

  void passBMutationIsDetectedWhilePassAFileRemainsHeld() {
    using namespace DormantV2GenerationTreeCaptureTestSupport;
    HookReset reset;
    Fixture fixture;
    QVERIFY(fixture.addTree(QString::fromLatin1(nonceA), QByteArray("A")));
    const auto mutationFd = fixture.openWritableForMutation(
        QString::fromLatin1(nonceA), QStringLiteral("hyprland.lua"));
    QVERIFY(mutationFd >= 0);
    auto changed = false;
    setCheckpointHook([&](const CheckpointEvent &event) {
      if (changed || event.checkpoint != Checkpoint::BetweenPasses)
        return;
      const QByteArray replacement("Z:hyprland.lua\n");
      QCOMPARE(::pwrite(mutationFd, replacement.constData(),
                        static_cast<size_t>(replacement.size()), 0),
               replacement.size());
      changed = true;
    });
    const auto descriptor = fixture.openRoot();
    const auto result = captureDormantV2GenerationTrees(
        descriptor, {QString::fromLatin1(nonceA)});
    ::close(descriptor);
    QVERIFY(changed);
    expectFailure(result, DormantV2GenerationTreeCaptureReason::TreeChanged,
                  DormantV2GenerationTreeCaptureSubject::Payload, 0,
                  DormantV2GenerationTreeFile::Entrypoint);
  }

  void appendAtTheEofBoundaryFailsClosed() {
    using namespace DormantV2GenerationTreeCaptureTestSupport;
    HookReset reset;
    Fixture fixture;
    QVERIFY(fixture.addTree(QString::fromLatin1(nonceA), QByteArray("A")));
    const auto mutationFd = fixture.openWritableForMutation(
        QString::fromLatin1(nonceA), QStringLiteral("manifest.json"));
    QVERIFY(mutationFd >= 0);
    const auto originalSize = bytesFor("A", "manifest.json").size();
    auto appended = false;
    setCheckpointHook([&](const CheckpointEvent &event) {
      if (appended || event.checkpoint != Checkpoint::AfterPread ||
          event.pass != Pass::First ||
          event.file != DormantV2GenerationTreeFile::Manifest ||
          event.preadInvocation != 1) {
        return;
      }
      const char extra = 'x';
      QCOMPARE(::pwrite(mutationFd, &extra, 1, originalSize), 1);
      appended = true;
    });
    const auto descriptor = fixture.openRoot();
    const auto result = captureDormantV2GenerationTrees(
        descriptor, {QString::fromLatin1(nonceA)});
    ::close(descriptor);
    QVERIFY(appended);
    expectFailure(result, DormantV2GenerationTreeCaptureReason::FileUnsafe,
                  DormantV2GenerationTreeCaptureSubject::Manifest, 0,
                  DormantV2GenerationTreeFile::Manifest);
  }

  void boundedEintrCanRecoverAndBoundedShortReadsStop() {
    using namespace DormantV2GenerationTreeCaptureTestSupport;
    {
      HookReset reset;
      Fixture fixture;
      QVERIFY(fixture.addTree(QString::fromLatin1(nonceA), QByteArray("A")));
      auto interrupted = false;
      setSyscallHook([&](const SyscallEvent &event) {
        if (!interrupted && event.syscall == Syscall::Pread &&
            event.file == DormantV2GenerationTreeFile::Manifest) {
          interrupted = true;
          return Fault::FailEintr;
        }
        return Fault::None;
      });
      const auto descriptor = fixture.openRoot();
      const auto result = captureDormantV2GenerationTrees(
          descriptor, {QString::fromLatin1(nonceA)});
      ::close(descriptor);
      QVERIFY(interrupted);
      QVERIFY(result.capture());
    }
    {
      HookReset reset;
      Fixture fixture;
      QVERIFY(fixture.addTree(QString::fromLatin1(nonceA), QByteArray("A")));
      const QByteArray largeManifest(1024, 'm');
      const auto manifestPath = fixture.path(QString::fromLatin1(nonceA),
                                             QStringLiteral("manifest.json"));
      QVERIFY(::chmod(QFile::encodeName(manifestPath).constData(), 0600) == 0);
      QVERIFY(writeFile(manifestPath, largeManifest));
      setSyscallHook([](const SyscallEvent &event) {
        return event.syscall == Syscall::Pread &&
                       event.file == DormantV2GenerationTreeFile::Manifest
                   ? Fault::ShortReadOneByte
                   : Fault::None;
      });
      const auto descriptor = fixture.openRoot();
      const auto result = captureDormantV2GenerationTrees(
          descriptor, {QString::fromLatin1(nonceA)});
      ::close(descriptor);
      expectFailure(
          result,
          DormantV2GenerationTreeCaptureReason::PerFileReadAttemptsExceeded,
          DormantV2GenerationTreeCaptureSubject::Manifest, 0,
          DormantV2GenerationTreeFile::Manifest);
    }
  }

  void enumerationEintrIsRecoverableButGloballyBounded() {
    using namespace DormantV2GenerationTreeCaptureTestSupport;
    {
      HookReset reset;
      Fixture fixture;
      QVERIFY(fixture.addTree(QString::fromLatin1(nonceA), QByteArray("A")));
      auto interrupted = false;
      setSyscallHook([&](const SyscallEvent &event) {
        if (!interrupted && event.syscall == Syscall::Readdir) {
          interrupted = true;
          return Fault::FailEintr;
        }
        return Fault::None;
      });
      const auto descriptor = fixture.openRoot();
      const auto result = captureDormantV2GenerationTrees(
          descriptor, {QString::fromLatin1(nonceA)});
      ::close(descriptor);
      QVERIFY(interrupted);
      QVERIFY(result.capture());
    }
    {
      HookReset reset;
      Fixture fixture;
      QVERIFY(fixture.addTree(QString::fromLatin1(nonceA), QByteArray("A")));
      setSyscallHook([](const SyscallEvent &event) {
        return event.syscall == Syscall::Readdir ? Fault::FailEintr
                                                 : Fault::None;
      });
      const auto descriptor = fixture.openRoot();
      const auto result = captureDormantV2GenerationTrees(
          descriptor, {QString::fromLatin1(nonceA)});
      ::close(descriptor);
      expectFailure(
          result,
          DormantV2GenerationTreeCaptureReason::EnumerationBudgetExceeded,
          DormantV2GenerationTreeCaptureSubject::GenerationRoot, 0);
    }
  }

  void heldPassAFileIsRecheckedAfterAllPassBReads() {
    using namespace DormantV2GenerationTreeCaptureTestSupport;
    HookReset reset;
    Fixture fixture;
    QVERIFY(fixture.addTree(QString::fromLatin1(nonceA), QByteArray("A")));
    const auto mutationFd = fixture.openWritableForMutation(
        QString::fromLatin1(nonceA), QStringLiteral("hyprland.lua"));
    QVERIFY(mutationFd >= 0);
    auto changed = false;
    setCheckpointHook([&](const CheckpointEvent &event) {
      if (changed ||
          event.checkpoint != Checkpoint::BeforeHeldFirstPassFinalization ||
          event.file != DormantV2GenerationTreeFile::Entrypoint) {
        return;
      }
      const QByteArray replacement("Y:hyprland.lua\n");
      QCOMPARE(::pwrite(mutationFd, replacement.constData(),
                        static_cast<size_t>(replacement.size()), 0),
               replacement.size());
      changed = true;
    });
    const auto descriptor = fixture.openRoot();
    const auto result = captureDormantV2GenerationTrees(
        descriptor, {QString::fromLatin1(nonceA)});
    ::close(descriptor);
    QVERIFY(changed);
    expectFailure(result, DormantV2GenerationTreeCaptureReason::TreeChanged,
                  DormantV2GenerationTreeCaptureSubject::Payload, 0,
                  DormantV2GenerationTreeFile::Entrypoint);
  }

  void successFailureAndCleanupAmbiguityLeakNoDescriptors() {
    using namespace DormantV2GenerationTreeCaptureTestSupport;
    HookReset reset;
    Fixture fixture;
    QVERIFY(fixture.addTree(QString::fromLatin1(nonceA), QByteArray("A")));
    QVERIFY(fixture.addTree(QString::fromLatin1(nonceB), QByteArray("B")));
    const auto descriptor = fixture.openRoot();
    QVERIFY(descriptor >= 0);
    const auto baseline = openDescriptorCount();

    const auto success = captureDormantV2GenerationTrees(
        descriptor, {QString::fromLatin1(nonceA), QString::fromLatin1(nonceB)});
    QVERIFY(success.capture());
    QCOMPARE(openDescriptorCount(), baseline);

    setPayloadAllocationFailureHook([](const Pass pass, const qsizetype,
                                       const DormantV2GenerationTreeFile file) {
      return pass == Pass::First &&
             file == DormantV2GenerationTreeFile::Module51Animations;
    });
    const auto allocationFailure = captureDormantV2GenerationTrees(
        descriptor, {QString::fromLatin1(nonceA), QString::fromLatin1(nonceB)});
    expectFailure(allocationFailure,
                  DormantV2GenerationTreeCaptureReason::AllocationFailed,
                  DormantV2GenerationTreeCaptureSubject::Payload, 0,
                  DormantV2GenerationTreeFile::Module51Animations);
    QCOMPARE(openDescriptorCount(), baseline);

    clearHooks();
    setSyscallHook([](const SyscallEvent &event) {
      return event.syscall == Syscall::CloseFile &&
                     event.file == DormantV2GenerationTreeFile::Entrypoint
                 ? Fault::ReportCleanupFailure
                 : Fault::None;
    });
    const auto cleanupFailure = captureDormantV2GenerationTrees(
        descriptor, {QString::fromLatin1(nonceA)});
    expectFailure(cleanupFailure,
                  DormantV2GenerationTreeCaptureReason::CleanupFailed,
                  DormantV2GenerationTreeCaptureSubject::Payload, 0,
                  DormantV2GenerationTreeFile::Entrypoint);
    QCOMPARE(openDescriptorCount(), baseline);
    ::close(descriptor);
  }

  void faultBudgetsAllocationAndCleanupStayClosed() {
    using namespace DormantV2GenerationTreeCaptureTestSupport;
    {
      HookReset reset;
      Fixture fixture;
      QVERIFY(fixture.addTree(QString::fromLatin1(nonceA), QByteArray("A")));
      setSyscallHook([](const SyscallEvent &event) {
        return event.syscall == Syscall::Openat &&
                       event.file == DormantV2GenerationTreeFile::Manifest
                   ? Fault::ExhaustProofBudget
                   : Fault::None;
      });
      const auto descriptor = fixture.openRoot();
      const auto result = captureDormantV2GenerationTrees(
          descriptor, {QString::fromLatin1(nonceA)});
      ::close(descriptor);
      expectFailure(result,
                    DormantV2GenerationTreeCaptureReason::ProofBudgetExceeded,
                    DormantV2GenerationTreeCaptureSubject::Manifest, 0,
                    DormantV2GenerationTreeFile::Manifest);
    }
    {
      HookReset reset;
      Fixture fixture;
      QVERIFY(fixture.addTree(QString::fromLatin1(nonceA), QByteArray("A")));
      setPayloadAllocationFailureHook(
          [](const Pass pass, const qsizetype,
             const DormantV2GenerationTreeFile file) {
            return pass == Pass::First &&
                   file == DormantV2GenerationTreeFile::Manifest;
          });
      const auto descriptor = fixture.openRoot();
      const auto result = captureDormantV2GenerationTrees(
          descriptor, {QString::fromLatin1(nonceA)});
      ::close(descriptor);
      expectFailure(result,
                    DormantV2GenerationTreeCaptureReason::AllocationFailed,
                    DormantV2GenerationTreeCaptureSubject::Manifest, 0,
                    DormantV2GenerationTreeFile::Manifest);
    }
    {
      HookReset reset;
      Fixture fixture;
      QVERIFY(fixture.addTree(QString::fromLatin1(nonceA), QByteArray("A")));
      setSyscallHook([](const SyscallEvent &event) {
        return event.syscall == Syscall::Closedir ? Fault::ReportCleanupFailure
                                                  : Fault::None;
      });
      const auto descriptor = fixture.openRoot();
      const auto result = captureDormantV2GenerationTrees(
          descriptor, {QString::fromLatin1(nonceA)});
      ::close(descriptor);
      expectFailure(result, DormantV2GenerationTreeCaptureReason::CleanupFailed,
                    DormantV2GenerationTreeCaptureSubject::GenerationRoot, 0);
    }
    {
      HookReset reset;
      Fixture fixture;
      QVERIFY(fixture.addTree(QString::fromLatin1(nonceA), QByteArray("A")));
      setSyscallHook([](const SyscallEvent &event) {
        return event.syscall == Syscall::CloseRoot ? Fault::ReportCleanupFailure
                                                   : Fault::None;
      });
      const auto descriptor = fixture.openRoot();
      const auto result = captureDormantV2GenerationTrees(
          descriptor, {QString::fromLatin1(nonceA)});
      ::close(descriptor);
      expectFailure(
          result, DormantV2GenerationTreeCaptureReason::CleanupFailed,
          DormantV2GenerationTreeCaptureSubject::GenerationsDirectory);
    }
  }

  void finalRootGuardOverridesAnEarlierLocalFailure() {
    using namespace DormantV2GenerationTreeCaptureTestSupport;
    HookReset reset;
    Fixture fixture;
    QVERIFY(fixture.addTree(QString::fromLatin1(nonceA), QByteArray("A")));
    QVERIFY(fixture.removeFile(QString::fromLatin1(nonceA),
                               QStringLiteral("hyprland.lua")));
    auto changed = false;
    setCheckpointHook([&](const CheckpointEvent &event) {
      if (changed || event.checkpoint != Checkpoint::BeforeFinalRootGuard)
        return;
      QCOMPARE(::chmod(QFile::encodeName(fixture.root()).constData(), 0755), 0);
      changed = true;
    });
    const auto descriptor = fixture.openRoot();
    const auto result = captureDormantV2GenerationTrees(
        descriptor, {QString::fromLatin1(nonceA)});
    ::close(descriptor);
    QVERIFY(changed);
    expectFailure(
        result,
        DormantV2GenerationTreeCaptureReason::GenerationsDirectoryChanged,
        DormantV2GenerationTreeCaptureSubject::GenerationsDirectory);
  }

  void copyAndMovePreserveBothEvidenceValues() {
    Fixture fixture;
    QVERIFY(fixture.addTree(QString::fromLatin1(nonceA), QByteArray("A")));
    const auto descriptor = fixture.openRoot();
    auto original = captureDormantV2GenerationTrees(
        descriptor, {QString::fromLatin1(nonceA)});
    ::close(descriptor);
    QVERIFY(original.capture());

    const auto copied = original;
    const auto moved = std::move(original);
    QVERIFY(original.capture());
    QVERIFY(copied.capture());
    QVERIFY(moved.capture());
    QCOMPARE(*original.capture(), *copied.capture());
    QCOMPARE(*original.capture(), *moved.capture());

    auto assignedResult = copied;
    assignedResult = std::move(original);
    QVERIFY(original.capture());
    QCOMPARE(*assignedResult.capture(), *original.capture());

    auto batch = *moved.capture();
    auto movedBatch = std::move(batch);
    QCOMPARE(batch, movedBatch);
    auto assignedBatch = movedBatch;
    assignedBatch = std::move(batch);
    QCOMPARE(batch, assignedBatch);

    auto tree = moved.capture()->trees().first();
    const auto movedTree = std::move(tree);
    QCOMPARE(tree, movedTree);
    QCOMPARE(tree.manifestBytes(), bytesFor("A", "manifest.json"));
    auto assignedTree = movedTree;
    assignedTree = std::move(tree);
    QCOMPARE(tree, assignedTree);
  }

  void postReturnMutationCannotAlterOwnedCapture() {
    Fixture fixture;
    QVERIFY(fixture.addTree(QString::fromLatin1(nonceA), QByteArray("A")));
    const auto descriptor = fixture.openRoot();
    const auto captured = captureDormantV2GenerationTrees(
        descriptor, {QString::fromLatin1(nonceA)});
    ::close(descriptor);
    QVERIFY(captured.capture());

    const auto mutationFd = fixture.openWritableForMutation(
        QString::fromLatin1(nonceA), QStringLiteral("hyprland.lua"));
    QVERIFY(mutationFd >= 0);
    const QByteArray replacement("Z:hyprland.lua\n");
    QCOMPARE(::pwrite(mutationFd, replacement.constData(),
                      static_cast<size_t>(replacement.size()), 0),
             replacement.size());

    QCOMPARE(captured.capture()->trees().first().files().value(
                 QStringLiteral("hyprland.lua")),
             bytesFor("A", "hyprland.lua"));
    const auto freshDescriptor = fixture.openRoot();
    const auto fresh = captureDormantV2GenerationTrees(
        freshDescriptor, {QString::fromLatin1(nonceA)});
    ::close(freshDescriptor);
    QVERIFY(fresh.capture());
    QCOMPARE(fresh.capture()->trees().first().files().value(
                 QStringLiteral("hyprland.lua")),
             replacement);
  }
};

QTEST_APPLESS_MAIN(CompositorDormantV2GenerationTreeCaptureTest)

#include "compositor_dormant_v2_generation_tree_capture_test.moc"
