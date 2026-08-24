#include "dormant_v1_filesystem_capture.h"

#include <QChar>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QSet>

#include <algorithm>
#include <array>
#include <cerrno>
#include <memory>
#include <utility>
#include <vector>

#include <dirent.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

namespace HyprShelld::Compositor {
namespace {

constexpr mode_t privateDirectoryMode = 0700;
constexpr mode_t immutableDirectoryMode = 0500;
constexpr mode_t privateFileMode = 0600;
constexpr mode_t immutableFileMode = 0400;
constexpr qsizetype maximumBasenameBytes = 255;
constexpr quint64 extraReadAttempts = 128;
constexpr quint64 maximumEintrRetries = 64;
constexpr quint64 maximumTestHookInvocations = 65536;
// Each phase retains an independent close/closedir allowance. Proof syscalls
// stop before consuming it; cleanup calls are still counted and never retried.
// The public root formulas cover four maximum-component canonical walks.
static_assert(dormantV1InitialRootAttemptReserve -
                  dormantV1RootCleanupAttemptReserve ==
              dormantV1InitialRootProofAttemptLimit);
static_assert(dormantV1FinalRootAttemptReserve -
                  dormantV1RootCleanupAttemptReserve ==
              dormantV1FinalRootProofAttemptLimit);
static_assert(maximumDormantV1UniqueFiles * 2 +
                  maximumDormantV1CapturedGenerations * 6 <=
              dormantV1BodyCleanupAttemptReserve);

struct FileCapturePolicy final {
  qsizetype maximumBytes = 0;
  std::optional<quint32> exactMode;
  std::optional<quint64> expectedDevice;
};

enum class InternalPass { First, Second };

enum class InternalCheckpoint {
  BeforeFstatat,
  AfterFstatat,
  AfterOpen,
  AfterFstat,
  AfterRead,
  BeforeFinalNamedCheck,
  BeforeReaddir,
  AfterReaddir,
  AfterInitialRootGuard,
  AfterFixedRecord,
  AfterGenerationDirectories,
  AfterGenerationInventory,
  AfterGenerationFile,
  BetweenPasses,
  BeforeFinalRootGuard,
};

#if defined(HYPRSHELLD_DORMANT_V1_CAPTURE_TEST_HOOKS)
thread_local DormantV1CaptureTestSupport::CheckpointHook testCheckpointHook;
thread_local quint64 testCheckpointInvocations = 0;
#endif

void invokeCheckpoint(
    const InternalCheckpoint checkpoint, const InternalPass pass,
    const DormantV1CaptureSubject subject = DormantV1CaptureSubject::None,
    const qsizetype generationIndex = -1, const QString &relativePath = {}) {
#if defined(HYPRSHELLD_DORMANT_V1_CAPTURE_TEST_HOOKS)
  if (!testCheckpointHook ||
      ++testCheckpointInvocations > maximumTestHookInvocations)
    return;
  try {
    testCheckpointHook({
        .checkpoint =
            static_cast<DormantV1CaptureTestSupport::Checkpoint>(checkpoint),
        .pass = static_cast<DormantV1CaptureTestSupport::Pass>(pass),
        .subject = subject,
        .generationIndex = generationIndex,
        .relativePath = relativePath,
    });
  } catch (...) {
    // A test seam can never escape into or alter the capture algorithm.
  }
#else
  Q_UNUSED(checkpoint)
  Q_UNUSED(pass)
  Q_UNUSED(subject)
  Q_UNUSED(generationIndex)
  Q_UNUSED(relativePath)
#endif
}

constexpr std::array<const char *, 16> moduleNames{{
    "00-session.lua",
    "10-monitors.lua",
    "20-environment.lua",
    "30-input.lua",
    "31-gestures.lua",
    "32-cursor.lua",
    "40-general.lua",
    "41-layouts.lua",
    "42-workspaces.lua",
    "43-groups.lua",
    "50-decorations.lua",
    "51-animations.lua",
    "60-rules.lua",
    "70-keybinds.lua",
    "80-permissions.lua",
    "90-advanced.lua",
}};

constexpr std::array<DormantV1CaptureSubject, 6> fixedSubjects{{
    DormantV1CaptureSubject::Desired,
    DormantV1CaptureSubject::LastGood,
    DormantV1CaptureSubject::Applied,
    DormantV1CaptureSubject::Pending,
    DormantV1CaptureSubject::Ownership,
    DormantV1CaptureSubject::Bridge,
}};

struct OperationBudget final {
  quint64 proofAttemptsRemaining = 0;
  quint64 cleanupAttemptsRemaining = 0;
  bool exceeded = false;

  explicit OperationBudget(const quint64 phaseLimit,
                           const quint64 cleanupReserve)
      : proofAttemptsRemaining(phaseLimit - cleanupReserve),
        cleanupAttemptsRemaining(cleanupReserve) {}

  [[nodiscard]] bool proofAttempt() {
    if (proofAttemptsRemaining == 0) {
      exceeded = true;
      return false;
    }
    --proofAttemptsRemaining;
    return true;
  }

  void cleanupAttempt() {
    if (cleanupAttemptsRemaining == 0) {
      exceeded = true;
      return;
    }
    --cleanupAttemptsRemaining;
  }
};

struct ReadBudget final {
  quint64 remaining = maximumDormantV1TwoPassPayloadReadBytes;
  OperationBudget operations{dormantV1BodyAttemptLimit,
                             dormantV1BodyCleanupAttemptReserve};
  quint64 enumerationCallsRemaining = maximumDormantV1EnumerationCalls;
  quint64 observedDirentsRemaining = maximumDormantV1ObservedDirents;
  quint64 direntNameBytesRemaining = maximumDormantV1DirentNameBytes;
  bool exceeded = false;

  [[nodiscard]] bool consume(const quint64 count) {
    if (count > remaining) {
      exceeded = true;
      return false;
    }
    remaining -= count;
    return true;
  }

  [[nodiscard]] bool attempt() {
    if (operations.proofAttempt())
      return true;
    exceeded = true;
    return false;
  }

  [[nodiscard]] bool exhausted() const {
    return exceeded || operations.exceeded;
  }
};

struct RootBaseline final {
  std::array<struct stat, 4> info{};
  DormantV1RootCapture capture;
};

struct Failure final {
  DormantV1CaptureReason reason = DormantV1CaptureReason::None;
  DormantV1CaptureSubject subject = DormantV1CaptureSubject::None;
  qsizetype generationIndex = -1;
  QString validatedNonce;
  QString relativePath;
};

struct GenerationReference final {
  const LegacyGenerationEvidenceV1 *evidence = nullptr;
  qsizetype originalIndex = -1;
};

struct GenerationWork final {
  GenerationReference reference;
  OperationBudget *operations = nullptr;
  int rootFd = -1;
  int modulesFd = -1;
  struct stat rootBaseline{};
  struct stat modulesBaseline{};
  DormantV1GenerationTreeCapture capture;

  explicit GenerationWork(GenerationReference value,
                          OperationBudget *operationBudget)
      : reference(value), operations(operationBudget) {}

  ~GenerationWork() {
    if (modulesFd >= 0) {
      operations->cleanupAttempt();
      ::close(modulesFd);
    }
    if (rootFd >= 0) {
      operations->cleanupAttempt();
      ::close(rootFd);
    }
  }

  GenerationWork(const GenerationWork &) = delete;
  GenerationWork &operator=(const GenerationWork &) = delete;
  GenerationWork(GenerationWork &&) = delete;
  GenerationWork &operator=(GenerationWork &&) = delete;
};

[[nodiscard]] bool descriptorCloseOnExec(const int descriptor,
                                         OperationBudget &budget) {
  if (descriptor < 0 || !budget.proofAttempt())
    return false;
  const auto flags = ::fcntl(descriptor, F_GETFD);
  return flags >= 0 && (flags & FD_CLOEXEC) != 0;
}

void closeDescriptor(const int descriptor, OperationBudget &budget) {
  if (descriptor < 0)
    return;
  budget.cleanupAttempt();
  ::close(descriptor);
}

[[nodiscard]] bool sameNode(const struct stat &left, const struct stat &right) {
  return left.st_dev == right.st_dev && left.st_ino == right.st_ino &&
         left.st_mode == right.st_mode && left.st_uid == right.st_uid &&
         left.st_nlink == right.st_nlink;
}

[[nodiscard]] bool sameSnapshot(const struct stat &left,
                                const struct stat &right) {
  return sameNode(left, right) && left.st_size == right.st_size &&
         left.st_mtim.tv_sec == right.st_mtim.tv_sec &&
         left.st_mtim.tv_nsec == right.st_mtim.tv_nsec &&
         left.st_ctim.tv_sec == right.st_ctim.tv_sec &&
         left.st_ctim.tv_nsec == right.st_ctim.tv_nsec;
}

[[nodiscard]] bool validIdentityRanges(const struct stat &value) {
  return value.st_size >= 0 && value.st_nlink > 0 &&
         value.st_mtim.tv_nsec >= 0 && value.st_mtim.tv_nsec <= 999999999 &&
         value.st_ctim.tv_nsec >= 0 && value.st_ctim.tv_nsec <= 999999999;
}

[[nodiscard]] DormantV1FileIdentity fileIdentity(const struct stat &value) {
  return {
      .device = static_cast<quint64>(value.st_dev),
      .inode = static_cast<quint64>(value.st_ino),
      .size = static_cast<quint64>(value.st_size),
      .mode = static_cast<quint32>(value.st_mode & 0777),
      .owner = static_cast<quint64>(value.st_uid),
      .linkCount = static_cast<quint64>(value.st_nlink),
      .modifiedSeconds = static_cast<qint64>(value.st_mtim.tv_sec),
      .modifiedNanoseconds = static_cast<qint64>(value.st_mtim.tv_nsec),
      .changedSeconds = static_cast<qint64>(value.st_ctim.tv_sec),
      .changedNanoseconds = static_cast<qint64>(value.st_ctim.tv_nsec),
  };
}

[[nodiscard]] DormantV1DirectoryIdentity
directoryIdentity(const struct stat &value) {
  return {
      .device = static_cast<quint64>(value.st_dev),
      .inode = static_cast<quint64>(value.st_ino),
      .size = static_cast<quint64>(value.st_size),
      .mode = static_cast<quint32>(value.st_mode & 0777),
      .owner = static_cast<quint64>(value.st_uid),
      .linkCount = static_cast<quint64>(value.st_nlink),
      .modifiedSeconds = static_cast<qint64>(value.st_mtim.tv_sec),
      .modifiedNanoseconds = static_cast<qint64>(value.st_mtim.tv_nsec),
      .changedSeconds = static_cast<qint64>(value.st_ctim.tv_sec),
      .changedNanoseconds = static_cast<qint64>(value.st_ctim.tv_nsec),
  };
}

[[nodiscard]] QString sha256(const QByteArrayView bytes) {
  return QString::fromLatin1(
      QCryptographicHash::hash(bytes, QCryptographicHash::Sha256).toHex());
}

[[nodiscard]] QString owningString(const QStringView value) {
  return QString(value.data(), value.size());
}

[[nodiscard]] bool safeBasename(const QByteArrayView value) {
  if (value.isEmpty() || value.size() > maximumBasenameBytes ||
      value == QByteArrayView(".") || value == QByteArrayView("..")) {
    return false;
  }
  for (const auto character : value) {
    if (character == '/' || character == '\0')
      return false;
  }
  return true;
}

[[nodiscard]] DormantV1FileCapture
captureFileAt(const int directoryFd, const QByteArrayView basename,
              const FileCapturePolicy &policy, ReadBudget &budget,
              const uid_t expectedOwner, const InternalPass pass,
              const DormantV1CaptureSubject subject,
              const qsizetype generationIndex = -1,
              const QString &relativePath = {}) {
  DormantV1FileCapture result;
  if (!descriptorCloseOnExec(directoryFd, budget.operations) ||
      !safeBasename(basename) || policy.maximumBytes <= 0 ||
      policy.maximumBytes > maximumDormantV1GeneratedFileCaptureBytes ||
      (policy.exactMode && *policy.exactMode > 0777)) {
    result.kind = DormantV1FileCaptureKind::Unsafe;
    return result;
  }
  struct stat directoryInfo{};
  if (!budget.attempt() || ::fstat(directoryFd, &directoryInfo) != 0 ||
      !S_ISDIR(directoryInfo.st_mode)) {
    result.kind = DormantV1FileCaptureKind::Unsafe;
    return result;
  }

  const QByteArray name(basename.data(), basename.size());
  struct stat named{};
  invokeCheckpoint(InternalCheckpoint::BeforeFstatat, pass, subject,
                   generationIndex, relativePath);
  if (!budget.attempt()) {
    result.kind = DormantV1FileCaptureKind::Unsafe;
    return result;
  }
  if (::fstatat(directoryFd, name.constData(), &named, AT_SYMLINK_NOFOLLOW) !=
      0) {
    if (errno != ENOENT)
      result.kind = DormantV1FileCaptureKind::Unsafe;
    return result;
  }
  invokeCheckpoint(InternalCheckpoint::AfterFstatat, pass, subject,
                   generationIndex, relativePath);
  if (!S_ISREG(named.st_mode) || !validIdentityRanges(named) ||
      named.st_uid != expectedOwner || named.st_nlink != 1 ||
      named.st_size < 0 || named.st_size > policy.maximumBytes ||
      (named.st_mode & (S_IWGRP | S_IWOTH | S_ISUID | S_ISGID | S_ISVTX)) !=
          0 ||
      (policy.exactMode &&
       static_cast<quint32>(named.st_mode & 0777) != *policy.exactMode) ||
      (policy.expectedDevice &&
       static_cast<quint64>(named.st_dev) != *policy.expectedDevice)) {
    result.kind = DormantV1FileCaptureKind::Unsafe;
    return result;
  }

  const auto descriptor = budget.attempt()
                              ? ::openat(directoryFd, name.constData(),
                                         O_RDONLY | O_CLOEXEC | O_NOFOLLOW)
                              : -1;
  if (descriptor < 0 || !descriptorCloseOnExec(descriptor, budget.operations)) {
    closeDescriptor(descriptor, budget.operations);
    result.kind = DormantV1FileCaptureKind::Unsafe;
    return result;
  }
  invokeCheckpoint(InternalCheckpoint::AfterOpen, pass, subject,
                   generationIndex, relativePath);
  struct stat opened{};
  if (!budget.attempt() || ::fstat(descriptor, &opened) != 0 ||
      !sameSnapshot(named, opened)) {
    closeDescriptor(descriptor, budget.operations);
    result.kind = DormantV1FileCaptureKind::Unsafe;
    return result;
  }
  invokeCheckpoint(InternalCheckpoint::AfterFstat, pass, subject,
                   generationIndex, relativePath);
  const auto openedSize = static_cast<quint64>(opened.st_size);
  if (!budget.consume(openedSize)) {
    closeDescriptor(descriptor, budget.operations);
    result.kind = DormantV1FileCaptureKind::Unsafe;
    return result;
  }

  QByteArray bytes;
  bytes.reserve(static_cast<qsizetype>(opened.st_size));
  std::array<char, dormantV1CaptureReadBufferBytes> buffer{};
  const auto maximumAttempts =
      static_cast<quint64>((policy.maximumBytes + 4095) / 4096) +
      extraReadAttempts;
  quint64 attempts = 0;
  quint64 interrupted = 0;
  auto readFailed = false;
  quint64 fileBytesRemaining = openedSize;
  while (fileBytesRemaining > 0) {
    if (++attempts > maximumAttempts || !budget.attempt()) {
      readFailed = true;
      break;
    }
    const auto requested = static_cast<size_t>(std::min<quint64>(
        static_cast<quint64>(buffer.size()), fileBytesRemaining));
    const auto count = ::read(descriptor, buffer.data(), requested);
    if (count == 0) {
      readFailed = true;
      break;
    }
    if (count < 0) {
      if (errno == EINTR && ++interrupted <= maximumEintrRetries) {
        continue;
      }
      readFailed = true;
      break;
    }
    const auto appended = static_cast<qsizetype>(count);
    if (bytes.size() > policy.maximumBytes - appended ||
        static_cast<quint64>(count) > fileBytesRemaining) {
      readFailed = true;
      break;
    }
    bytes.append(buffer.data(), appended);
    fileBytesRemaining -= static_cast<quint64>(count);
  }
  invokeCheckpoint(InternalCheckpoint::AfterRead, pass, subject,
                   generationIndex, relativePath);

  struct stat after{};
  struct stat finalNamed{};
  invokeCheckpoint(InternalCheckpoint::BeforeFinalNamedCheck, pass, subject,
                   generationIndex, relativePath);
  const auto stable = !readFailed && budget.attempt() &&
                      ::fstat(descriptor, &after) == 0 &&
                      sameSnapshot(opened, after) && budget.attempt() &&
                      ::fstatat(directoryFd, name.constData(), &finalNamed,
                                AT_SYMLINK_NOFOLLOW) == 0 &&
                      sameSnapshot(after, finalNamed) &&
                      fileBytesRemaining == 0 && bytes.size() == after.st_size;
  closeDescriptor(descriptor, budget.operations);
  if (!stable) {
    result.kind = DormantV1FileCaptureKind::Unsafe;
    return result;
  }
  result.kind = DormantV1FileCaptureKind::ExactRegular;
  result.sha256 = sha256(bytes);
  result.identity = fileIdentity(after);
  result.bytes = std::move(bytes);
  return result;
}

[[nodiscard]] bool safeAbsolutePath(const QString &path) {
  if (path.size() > maximumDormantV1PathCodeUnits ||
      !QDir::isAbsolutePath(path) || QDir::cleanPath(path) != path ||
      path != path.normalized(QString::NormalizationForm_C) ||
      path.toUtf8().size() > maximumDormantV1PathUtf8Bytes ||
      path.split(QLatin1Char('/'), Qt::SkipEmptyParts).size() >
          maximumDormantV1PathComponents) {
    return false;
  }
  for (const auto point : path.toUcs4()) {
    const auto category = QChar::category(static_cast<char32_t>(point));
    if (category == QChar::Other_Control || category == QChar::Other_Format ||
        category == QChar::Separator_Line ||
        category == QChar::Separator_Paragraph) {
      return false;
    }
  }
  return true;
}

[[nodiscard]] bool trustedDirectoryMetadata(const struct stat &metadata,
                                            const uid_t rootOwner,
                                            const uid_t expectedOwner) {
  const auto ownerTrusted =
      metadata.st_uid == expectedOwner || metadata.st_uid == rootOwner;
  const auto writableByOthers = (metadata.st_mode & (S_IWGRP | S_IWOTH)) != 0;
  const auto protectedTemporary =
      metadata.st_uid == rootOwner && (metadata.st_mode & S_ISVTX) != 0;
  return S_ISDIR(metadata.st_mode) && validIdentityRanges(metadata) &&
         ownerTrusted && (!writableByOthers || protectedTemporary);
}

[[nodiscard]] int openTrustedDirectoryTree(const QString &path,
                                           OperationBudget &budget,
                                           const uid_t expectedOwner) {
  if (!safeAbsolutePath(path))
    return -1;
  const auto root =
      budget.proofAttempt()
          ? ::open("/", O_RDONLY | O_CLOEXEC | O_NOFOLLOW | O_DIRECTORY)
          : -1;
  if (root < 0 || !descriptorCloseOnExec(root, budget)) {
    closeDescriptor(root, budget);
    return -1;
  }
  struct stat rootInfo{};
  if (!budget.proofAttempt() || ::fstat(root, &rootInfo) != 0 ||
      !S_ISDIR(rootInfo.st_mode)) {
    closeDescriptor(root, budget);
    return -1;
  }
  const auto rootOwner = rootInfo.st_uid;
  if (!trustedDirectoryMetadata(rootInfo, rootOwner, expectedOwner)) {
    closeDescriptor(root, budget);
    return -1;
  }
  std::vector<int> chain{root};
  std::vector<QByteArray> names;
  std::vector<struct stat> trustedSnapshots;
  names.reserve(static_cast<size_t>(maximumDormantV1PathComponents));
  trustedSnapshots.reserve(static_cast<size_t>(maximumDormantV1PathComponents));
  const auto closeChain = [&] {
    for (const auto descriptor : chain) {
      closeDescriptor(descriptor, budget);
    }
    chain.clear();
  };
  for (const auto &component :
       path.split(QLatin1Char('/'), Qt::SkipEmptyParts)) {
    const auto name = QFile::encodeName(component);
    if (!safeBasename(name)) {
      closeChain();
      return -1;
    }
    struct stat named{};
    if (!budget.proofAttempt() ||
        ::fstatat(chain.back(), name.constData(), &named,
                  AT_SYMLINK_NOFOLLOW) != 0 ||
        !trustedDirectoryMetadata(named, rootOwner, expectedOwner)) {
      closeChain();
      return -1;
    }
    const auto next =
        budget.proofAttempt()
            ? ::openat(chain.back(), name.constData(),
                       O_RDONLY | O_CLOEXEC | O_NOFOLLOW | O_DIRECTORY)
            : -1;
    struct stat opened{};
    if (next < 0 || !descriptorCloseOnExec(next, budget) ||
        !budget.proofAttempt() || ::fstat(next, &opened) != 0 ||
        !sameNode(named, opened)) {
      closeDescriptor(next, budget);
      closeChain();
      return -1;
    }
    names.push_back(name);
    trustedSnapshots.push_back(opened);
    chain.push_back(next);
  }

  // Hold every parent/child descriptor until a complete named-chain
  // recapture. This closes the intermediate-component rename gap that a
  // walk which closes parents as it proceeds would leave behind.
  for (size_t index = 0; index < names.size(); ++index) {
    struct stat named{};
    struct stat opened{};
    if (!budget.proofAttempt() ||
        ::fstatat(chain[index], names[index].constData(), &named,
                  AT_SYMLINK_NOFOLLOW) != 0 ||
        !budget.proofAttempt() || ::fstat(chain[index + 1], &opened) != 0 ||
        !S_ISDIR(named.st_mode) || !S_ISDIR(opened.st_mode) ||
        !trustedDirectoryMetadata(named, rootOwner, expectedOwner) ||
        !sameNode(named, trustedSnapshots[index]) ||
        !sameNode(opened, trustedSnapshots[index])) {
      closeChain();
      return -1;
    }
  }

  const auto finalRoot =
      budget.proofAttempt()
          ? ::open("/", O_RDONLY | O_CLOEXEC | O_NOFOLLOW | O_DIRECTORY)
          : -1;
  struct stat finalRootInfo{};
  struct stat heldRootInfo{};
  const auto rootStable =
      finalRoot >= 0 && descriptorCloseOnExec(finalRoot, budget) &&
      budget.proofAttempt() && ::fstat(finalRoot, &finalRootInfo) == 0 &&
      budget.proofAttempt() && ::fstat(chain.front(), &heldRootInfo) == 0 &&
      trustedDirectoryMetadata(finalRootInfo, rootOwner, expectedOwner) &&
      trustedDirectoryMetadata(heldRootInfo, rootOwner, expectedOwner) &&
      sameSnapshot(finalRootInfo, rootInfo) &&
      sameSnapshot(heldRootInfo, rootInfo);
  closeDescriptor(finalRoot, budget);
  if (!rootStable) {
    closeChain();
    return -1;
  }
  const auto result = chain.back();
  chain.pop_back();
  closeChain();
  return result;
}

[[nodiscard]] bool canonicalDirectoryMatches(const QString &path,
                                             const int retained,
                                             OperationBudget &budget,
                                             const uid_t expectedOwner) {
  const auto canonical = openTrustedDirectoryTree(path, budget, expectedOwner);
  if (canonical < 0)
    return false;
  struct stat expected{};
  struct stat current{};
  const auto matches =
      budget.proofAttempt() && ::fstat(retained, &expected) == 0 &&
      budget.proofAttempt() && ::fstat(canonical, &current) == 0 &&
      sameNode(expected, current);
  closeDescriptor(canonical, budget);
  return matches;
}

[[nodiscard]] bool descriptorStillNamed(const int parent,
                                        const QByteArrayView nameView,
                                        const int descriptor,
                                        OperationBudget &budget) {
  const QByteArray name(nameView.data(), nameView.size());
  struct stat opened{};
  struct stat named{};
  return descriptorCloseOnExec(parent, budget) &&
         descriptorCloseOnExec(descriptor, budget) && budget.proofAttempt() &&
         ::fstat(descriptor, &opened) == 0 && budget.proofAttempt() &&
         ::fstatat(parent, name.constData(), &named, AT_SYMLINK_NOFOLLOW) ==
             0 &&
         S_ISDIR(opened.st_mode) && S_ISDIR(named.st_mode) &&
         sameNode(opened, named);
}

[[nodiscard]] DormantV1CaptureSubject rootSubject(const size_t index) {
  constexpr std::array<DormantV1CaptureSubject, 4> subjects{{
      DormantV1CaptureSubject::StateRoot,
      DormantV1CaptureSubject::ConfigRoot,
      DormantV1CaptureSubject::ManagedRoot,
      DormantV1CaptureSubject::GenerationsRoot,
  }};
  return index < subjects.size() ? subjects[index]
                                 : DormantV1CaptureSubject::None;
}

[[nodiscard]] bool safeRootMetadataAt(const struct stat &info,
                                      const size_t index,
                                      const uid_t expectedOwner) {
  const auto special = S_ISUID | S_ISGID | S_ISVTX;
  if (!S_ISDIR(info.st_mode) || !validIdentityRanges(info) ||
      info.st_uid != expectedOwner)
    return false;
  if (index == 1) {
    return (info.st_mode & (S_IWGRP | S_IWOTH | special)) == 0;
  }
  return (info.st_mode & 07777) == privateDirectoryMode;
}

[[nodiscard]] bool
safeRootMetadata(const std::array<struct stat, 4> &info,
                 const uid_t expectedOwner,
                 DormantV1CaptureSubject *failedSubject = nullptr) {
  for (size_t index = 0; index < info.size(); ++index) {
    if (!safeRootMetadataAt(info[index], index, expectedOwner)) {
      if (failedSubject)
        *failedSubject = rootSubject(index);
      return false;
    }
  }
  return true;
}

[[nodiscard]] std::array<int, 4>
rootDescriptors(const BorrowedDormantV1FilesystemRoots &roots) {
  return {
      roots.stateDirectoryFd,
      roots.configDirectoryFd,
      roots.managedDirectoryFd,
      roots.generationsDirectoryFd,
  };
}

[[nodiscard]] std::array<QString, 4>
rootPaths(const BorrowedDormantV1FilesystemRoots &roots) {
  return {
      roots.stateRoot,
      roots.configRoot,
      roots.managedConfigRoot,
      QDir(roots.managedConfigRoot).filePath(QStringLiteral("generations")),
  };
}

[[nodiscard]] bool
pairwiseDistinctRoots(const std::array<struct stat, 4> &info,
                      DormantV1CaptureSubject *failedSubject = nullptr) {
  for (size_t left = 0; left < info.size(); ++left) {
    for (size_t right = left + 1; right < info.size(); ++right) {
      if (info[left].st_dev == info[right].st_dev &&
          info[left].st_ino == info[right].st_ino) {
        if (failedSubject)
          *failedSubject = rootSubject(right);
        return false;
      }
    }
  }
  return true;
}

[[nodiscard]] std::optional<Failure>
establishRootBaseline(const BorrowedDormantV1FilesystemRoots &roots,
                      RootBaseline &baseline, OperationBudget &budget,
                      const uid_t expectedOwner) {
  const auto descriptors = rootDescriptors(roots);
  for (size_t index = 0; index < descriptors.size(); ++index) {
    if (!descriptorCloseOnExec(descriptors[index], budget)) {
      return Failure{
          .reason = DormantV1CaptureReason::InvalidRootDescriptors,
          .subject = rootSubject(index),
      };
    }
  }
  const std::array<QStringView, 3> suppliedPaths{{
      roots.stateRoot,
      roots.configRoot,
      roots.managedConfigRoot,
  }};
  for (size_t index = 0; index < suppliedPaths.size(); ++index) {
    if (suppliedPaths[index].size() > maximumDormantV1PathCodeUnits) {
      return Failure{
          .reason = DormantV1CaptureReason::InvalidRootPath,
          .subject = rootSubject(index),
      };
    }
  }
  const auto paths = rootPaths(roots);
  for (size_t index = 0; index < paths.size(); ++index) {
    if (!safeAbsolutePath(paths[index])) {
      return Failure{
          .reason = DormantV1CaptureReason::InvalidRootPath,
          .subject = rootSubject(index),
      };
    }
  }
  if (roots.managedConfigRoot !=
      QDir(roots.configRoot).filePath(QStringLiteral("hyprshelld"))) {
    return Failure{
        .reason = DormantV1CaptureReason::InvalidRootLayout,
        .subject = DormantV1CaptureSubject::ManagedRoot,
    };
  }
  for (size_t index = 0; index < descriptors.size(); ++index) {
    if (!budget.proofAttempt() ||
        ::fstat(descriptors[index], &baseline.info[index]) != 0) {
      return Failure{
          .reason = DormantV1CaptureReason::InvalidRootDescriptors,
          .subject = rootSubject(index),
      };
    }
  }
  DormantV1CaptureSubject failedSubject = DormantV1CaptureSubject::None;
  if (!safeRootMetadata(baseline.info, expectedOwner, &failedSubject)) {
    return Failure{
        .reason = DormantV1CaptureReason::UnsafeRootMetadata,
        .subject = failedSubject,
    };
  }
  for (size_t index = 0; index < paths.size(); ++index) {
    if (!canonicalDirectoryMatches(paths[index], descriptors[index], budget,
                                   expectedOwner)) {
      return Failure{
          .reason = DormantV1CaptureReason::RootIdentityMismatch,
          .subject = rootSubject(index),
      };
    }
  }
  if (!descriptorStillNamed(roots.configDirectoryFd,
                            QByteArrayView("hyprshelld"),
                            roots.managedDirectoryFd, budget)) {
    return Failure{
        .reason = DormantV1CaptureReason::RootIdentityMismatch,
        .subject = DormantV1CaptureSubject::ManagedRoot,
    };
  }
  if (!descriptorStillNamed(roots.managedDirectoryFd,
                            QByteArrayView("generations"),
                            roots.generationsDirectoryFd, budget)) {
    return Failure{
        .reason = DormantV1CaptureReason::RootIdentityMismatch,
        .subject = DormantV1CaptureSubject::GenerationsRoot,
    };
  }
  if (baseline.info[1].st_dev != baseline.info[2].st_dev) {
    return Failure{
        .reason = DormantV1CaptureReason::RootIdentityMismatch,
        .subject = DormantV1CaptureSubject::ManagedRoot,
    };
  }
  if (baseline.info[1].st_dev != baseline.info[3].st_dev) {
    return Failure{
        .reason = DormantV1CaptureReason::RootIdentityMismatch,
        .subject = DormantV1CaptureSubject::GenerationsRoot,
    };
  }
  if (!pairwiseDistinctRoots(baseline.info, &failedSubject)) {
    return Failure{
        .reason = DormantV1CaptureReason::RootIdentityMismatch,
        .subject = failedSubject,
    };
  }
  baseline.capture = {
      .state = directoryIdentity(baseline.info[0]),
      .config = directoryIdentity(baseline.info[1]),
      .managed = directoryIdentity(baseline.info[2]),
      .generations = directoryIdentity(baseline.info[3]),
  };
  return std::nullopt;
}

[[nodiscard]] bool
rootGuardMatches(const BorrowedDormantV1FilesystemRoots &roots,
                 const RootBaseline &baseline, OperationBudget &budget,
                 const uid_t expectedOwner,
                 DormantV1CaptureSubject &failedSubject) {
  const auto descriptors = rootDescriptors(roots);
  std::array<struct stat, 4> current{};
  for (size_t index = 0; index < descriptors.size(); ++index) {
    if (!descriptorCloseOnExec(descriptors[index], budget) ||
        !budget.proofAttempt() ||
        ::fstat(descriptors[index], &current[index]) != 0 ||
        !sameSnapshot(current[index], baseline.info[index])) {
      failedSubject = rootSubject(index);
      return false;
    }
  }
  if (!safeRootMetadata(current, expectedOwner, &failedSubject)) {
    return false;
  }
  const auto paths = rootPaths(roots);
  for (size_t index = 0; index < paths.size(); ++index) {
    if (!canonicalDirectoryMatches(paths[index], descriptors[index], budget,
                                   expectedOwner)) {
      failedSubject = rootSubject(index);
      return false;
    }
  }
  if (!descriptorStillNamed(roots.configDirectoryFd,
                            QByteArrayView("hyprshelld"),
                            roots.managedDirectoryFd, budget) ||
      current[1].st_dev != current[2].st_dev) {
    failedSubject = DormantV1CaptureSubject::ManagedRoot;
    return false;
  }
  if (!descriptorStillNamed(roots.managedDirectoryFd,
                            QByteArrayView("generations"),
                            roots.generationsDirectoryFd, budget) ||
      current[1].st_dev != current[3].st_dev) {
    failedSubject = DormantV1CaptureSubject::GenerationsRoot;
    return false;
  }
  return pairwiseDistinctRoots(current, &failedSubject);
}

[[nodiscard]] bool
retainedRootGuardMatches(const BorrowedDormantV1FilesystemRoots &roots,
                         const RootBaseline &baseline, OperationBudget &budget,
                         const uid_t expectedOwner,
                         DormantV1CaptureSubject &failedSubject) {
  const auto descriptors = rootDescriptors(roots);
  std::array<struct stat, 4> current{};
  for (size_t index = 0; index < descriptors.size(); ++index) {
    if (!descriptorCloseOnExec(descriptors[index], budget) ||
        !budget.proofAttempt() ||
        ::fstat(descriptors[index], &current[index]) != 0 ||
        !sameSnapshot(current[index], baseline.info[index])) {
      failedSubject = rootSubject(index);
      return false;
    }
  }
  if (!safeRootMetadata(current, expectedOwner, &failedSubject)) {
    return false;
  }
  if (!descriptorStillNamed(roots.configDirectoryFd,
                            QByteArrayView("hyprshelld"),
                            roots.managedDirectoryFd, budget) ||
      current[1].st_dev != current[2].st_dev) {
    failedSubject = DormantV1CaptureSubject::ManagedRoot;
    return false;
  }
  if (!descriptorStillNamed(roots.managedDirectoryFd,
                            QByteArrayView("generations"),
                            roots.generationsDirectoryFd, budget) ||
      current[1].st_dev != current[3].st_dev) {
    failedSubject = DormantV1CaptureSubject::GenerationsRoot;
    return false;
  }
  return pairwiseDistinctRoots(current, &failedSubject);
}

[[nodiscard]] bool validNonce(const QStringView value) {
  if (value.size() < 32 || value.size() > 128)
    return false;
  for (const auto character : value) {
    if (!((character >= u'0' && character <= u'9') ||
          (character >= u'a' && character <= u'f')))
      return false;
  }
  return true;
}

[[nodiscard]] bool boundedRelativePath(const QStringView value) {
  return value.size() <= maximumDormantV1RelativePathCodeUnits &&
         value.toUtf8().size() <= maximumDormantV1RelativePathUtf8Bytes;
}

[[nodiscard]] QSet<QString> expectedPayloadPaths() {
  QSet<QString> result{QStringLiteral("hyprland.lua")};
  for (const auto *name : moduleNames) {
    result.insert(QStringLiteral("modules/") + QString::fromLatin1(name));
  }
  return result;
}

[[nodiscard]] QSet<QByteArray> expectedRootNames() {
  return {
      QByteArrayLiteral("manifest.json"),
      QByteArrayLiteral("hyprland.lua"),
      QByteArrayLiteral("modules"),
  };
}

[[nodiscard]] QSet<QByteArray> expectedModuleNames() {
  QSet<QByteArray> result;
  for (const auto *name : moduleNames)
    result.insert(QByteArray(name));
  return result;
}

enum class InventoryResult { Exact, Mismatch };

[[nodiscard]] InventoryResult
exactRawInventory(const int descriptor, const QSet<QByteArray> &expected,
                  ReadBudget &budget, const InternalPass pass,
                  const DormantV1CaptureSubject subject,
                  const qsizetype generationIndex) {
  const auto reopened =
      budget.attempt()
          ? ::openat(descriptor, ".",
                     O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW)
          : -1;
  if (reopened < 0 || !descriptorCloseOnExec(reopened, budget.operations)) {
    closeDescriptor(reopened, budget.operations);
    return InventoryResult::Mismatch;
  }
  auto *directory = budget.attempt() ? ::fdopendir(reopened) : nullptr;
  if (!directory) {
    closeDescriptor(reopened, budget.operations);
    return InventoryResult::Mismatch;
  }
  QSet<QByteArray> actual;
  const auto maximumCalls =
      static_cast<quint64>(expected.size()) + 3 + maximumEintrRetries;
  quint64 calls = 0;
  quint64 interrupted = 0;
  auto mismatch = false;
  while (true) {
    if (++calls > maximumCalls) {
      budget.exceeded = true;
      mismatch = true;
      break;
    }
    if (budget.enumerationCallsRemaining == 0) {
      budget.exceeded = true;
      mismatch = true;
      break;
    }
    if (!budget.attempt()) {
      mismatch = true;
      break;
    }
    --budget.enumerationCallsRemaining;
    invokeCheckpoint(InternalCheckpoint::BeforeReaddir, pass, subject,
                     generationIndex);
    errno = 0;
    auto *entry = ::readdir(directory);
    const auto readdirError = errno;
    invokeCheckpoint(InternalCheckpoint::AfterReaddir, pass, subject,
                     generationIndex);
    if (!entry) {
      if (readdirError == EINTR && ++interrupted <= maximumEintrRetries) {
        continue;
      }
      if (readdirError != 0)
        mismatch = true;
      break;
    }
    const QByteArray name(entry->d_name);
    if (name == "." || name == "..")
      continue;
    if (budget.observedDirentsRemaining == 0) {
      budget.exceeded = true;
      mismatch = true;
      break;
    }
    if (static_cast<quint64>(name.size()) > budget.direntNameBytesRemaining) {
      budget.exceeded = true;
      mismatch = true;
      break;
    }
    if (!safeBasename(name) || actual.size() >= expected.size()) {
      mismatch = true;
      break;
    }
    --budget.observedDirentsRemaining;
    budget.direntNameBytesRemaining -= static_cast<quint64>(name.size());
    actual.insert(name);
  }
  budget.operations.cleanupAttempt();
  ::closedir(directory);
  return !mismatch && actual == expected ? InventoryResult::Exact
                                         : InventoryResult::Mismatch;
}

[[nodiscard]] const LegacyReadV1 &
expectedRecord(const ReachableV1PreflightInput &input,
               const DormantV1CaptureSubject subject) {
  switch (subject) {
  case DormantV1CaptureSubject::Desired:
    return input.desired;
  case DormantV1CaptureSubject::LastGood:
    return input.lastGood;
  case DormantV1CaptureSubject::Applied:
    return input.applied;
  case DormantV1CaptureSubject::Pending:
    return input.pending;
  case DormantV1CaptureSubject::Ownership:
    return input.ownership;
  case DormantV1CaptureSubject::Bridge:
    return input.bridge;
  default:
    return input.desired;
  }
}

[[nodiscard]] DormantV1FileCapture &
capturedRecord(DormantV1FixedRecordCapture &capture,
               const DormantV1CaptureSubject subject) {
  switch (subject) {
  case DormantV1CaptureSubject::Desired:
    return capture.desired;
  case DormantV1CaptureSubject::LastGood:
    return capture.lastGood;
  case DormantV1CaptureSubject::Applied:
    return capture.applied;
  case DormantV1CaptureSubject::Pending:
    return capture.pending;
  case DormantV1CaptureSubject::Ownership:
    return capture.ownership;
  case DormantV1CaptureSubject::Bridge:
    return capture.bridge;
  default:
    return capture.desired;
  }
}

[[nodiscard]] const char *recordName(const DormantV1CaptureSubject subject) {
  switch (subject) {
  case DormantV1CaptureSubject::Desired:
    return "desired.json";
  case DormantV1CaptureSubject::LastGood:
    return "last-good.json";
  case DormantV1CaptureSubject::Applied:
    return "activation.json";
  case DormantV1CaptureSubject::Pending:
    return "pending.json";
  case DormantV1CaptureSubject::Ownership:
    return "entrypoint-ownership.json";
  case DormantV1CaptureSubject::Bridge:
    return "live-activation.pending.json";
  default:
    return "";
  }
}

[[nodiscard]] int recordDirectory(const BorrowedDormantV1FilesystemRoots &roots,
                                  const DormantV1CaptureSubject subject) {
  return subject == DormantV1CaptureSubject::Ownership ||
                 subject == DormantV1CaptureSubject::Bridge
             ? roots.managedDirectoryFd
             : roots.stateDirectoryFd;
}

[[nodiscard]] bool captureMatchesExpected(const DormantV1FileCapture &captured,
                                          const LegacyReadV1 &expected) {
  switch (expected.kind) {
  case LegacyReadKindV1::Missing:
    return captured.kind == DormantV1FileCaptureKind::Missing;
  case LegacyReadKindV1::ExactRegular:
    return captured.kind == DormantV1FileCaptureKind::ExactRegular &&
           captured.bytes == expected.bytes;
  case LegacyReadKindV1::Unsafe:
    return false;
  }
  return false;
}

[[nodiscard]] bool addBounded(quint64 &total, const qsizetype count,
                              const quint64 maximum) {
  if (count < 0 || static_cast<quint64>(count) > maximum - total) {
    return false;
  }
  total += static_cast<quint64>(count);
  return true;
}

[[nodiscard]] DormantV1FilesystemCaptureResult failedResult(Failure failure) {
  return {
      .disposition = DormantV1CaptureDisposition::FailedClosed,
      .reason = failure.reason,
      .subject = failure.subject,
      .generationIndex = failure.generationIndex,
      .validatedNonce = owningString(failure.validatedNonce),
      .relativePath = owningString(failure.relativePath),
  };
}

[[nodiscard]] bool exactImmutableDirectory(const int descriptor,
                                           struct stat &result,
                                           OperationBudget &budget,
                                           const uid_t expectedOwner) {
  return descriptorCloseOnExec(descriptor, budget) && budget.proofAttempt() &&
         ::fstat(descriptor, &result) == 0 && S_ISDIR(result.st_mode) &&
         validIdentityRanges(result) && result.st_uid == expectedOwner &&
         (result.st_mode & 07777) == immutableDirectoryMode;
}

[[nodiscard]] bool
generationDirectoriesStillExact(const BorrowedDormantV1FilesystemRoots &roots,
                                const GenerationWork &work, ReadBudget &budget,
                                const uid_t expectedOwner) {
  struct stat rootCurrent{};
  struct stat modulesCurrent{};
  const auto nonce =
      work.reference.evidence->expected.activationNonce.toLatin1();
  return exactImmutableDirectory(work.rootFd, rootCurrent, budget.operations,
                                 expectedOwner) &&
         exactImmutableDirectory(work.modulesFd, modulesCurrent,
                                 budget.operations, expectedOwner) &&
         sameSnapshot(rootCurrent, work.rootBaseline) &&
         sameSnapshot(modulesCurrent, work.modulesBaseline) &&
         descriptorStillNamed(roots.generationsDirectoryFd, nonce, work.rootFd,
                              budget.operations) &&
         descriptorStillNamed(work.rootFd, QByteArrayView("modules"),
                              work.modulesFd, budget.operations);
}

[[nodiscard]] QString identityKey(const quint64 device, const quint64 inode) {
  return QString::number(device) + QLatin1Char(':') + QString::number(inode);
}

} // namespace

#if defined(HYPRSHELLD_DORMANT_V1_CAPTURE_TEST_HOOKS)
namespace DormantV1CaptureTestSupport {

void setCheckpointHook(CheckpointHook hook) {
  testCheckpointHook = std::move(hook);
  testCheckpointInvocations = 0;
}

void clearCheckpointHook() {
  testCheckpointHook = {};
  testCheckpointInvocations = 0;
}

} // namespace DormantV1CaptureTestSupport
#endif

DormantV1FilesystemCaptureResult captureDormantReachableV1Filesystem(
    const BorrowedDormantV1FilesystemRoots &roots,
    const ReachableV1PreflightInput &expected) {
  if (expected.referencedGenerations.size() >
      maximumDormantV1CapturedGenerations) {
    return failedResult(
        {.reason = DormantV1CaptureReason::TooManyGenerationEvidences});
  }

  quint64 callerPayloadBytes = 0;
  quint64 retainedBytes = 0;
  for (const auto subject : fixedSubjects) {
    const auto &record = expectedRecord(expected, subject);
    switch (record.kind) {
    case LegacyReadKindV1::Unsafe:
      return failedResult({
          .reason = DormantV1CaptureReason::UnsafeExpectedRead,
          .subject = subject,
      });
    case LegacyReadKindV1::Missing:
      if (!record.bytes.isEmpty()) {
        return failedResult({
            .reason = DormantV1CaptureReason::InvalidExpectedRead,
            .subject = subject,
        });
      }
      break;
    case LegacyReadKindV1::ExactRegular:
      if (record.bytes.size() > maximumDormantV1MetadataCaptureBytes) {
        return failedResult({
            .reason = DormantV1CaptureReason::ExpectedRecordOversized,
            .subject = subject,
        });
      }
      break;
    default:
      return failedResult({
          .reason = DormantV1CaptureReason::InvalidExpectedRead,
          .subject = subject,
      });
    }
    if (!addBounded(callerPayloadBytes, record.bytes.size(),
                    maximumDormantV1CallerPayloadBytes) ||
        !addBounded(retainedBytes, record.bytes.size(),
                    maximumDormantV1RetainedPayloadBytes)) {
      return failedResult({
          .reason = DormantV1CaptureReason::CallerPayloadTooLarge,
          .subject = subject,
      });
    }
  }

  const auto payloadPaths = expectedPayloadPaths();
  QVector<GenerationReference> references;
  QSet<QString> nonces;
  for (qsizetype index = 0; index < expected.referencedGenerations.size();
       ++index) {
    const auto &evidence = expected.referencedGenerations.at(index);
    const auto &nonce = evidence.expected.activationNonce;
    if (!validNonce(nonce)) {
      return failedResult({
          .reason = DormantV1CaptureReason::InvalidGenerationReference,
          .generationIndex = index,
      });
    }
    if (nonces.contains(nonce)) {
      return failedResult({
          .reason = DormantV1CaptureReason::DuplicateGenerationReference,
          .generationIndex = index,
      });
    }
    nonces.insert(nonce);
    constexpr auto generationSuffixCodeUnits = 13;
    constexpr auto customSuffixCodeUnits = 16;
    if (roots.managedConfigRoot.size() > maximumDormantV1PathCodeUnits -
                                             generationSuffixCodeUnits -
                                             nonce.size() ||
        roots.configRoot.size() >
            maximumDormantV1PathCodeUnits - customSuffixCodeUnits ||
        !safeAbsolutePath(evidence.expected.generationRoot) ||
        !safeAbsolutePath(evidence.expected.userCustomPath)) {
      return failedResult({
          .reason = DormantV1CaptureReason::InvalidGenerationPaths,
          .generationIndex = index,
          .validatedNonce = nonce,
      });
    }
    const auto exactRoot =
        QDir(roots.managedConfigRoot)
            .filePath(QStringLiteral("generations/%1").arg(nonce));
    const auto exactCustom =
        QDir(roots.configRoot).filePath(QStringLiteral("user-custom.lua"));
    if (evidence.expected.generationRoot != exactRoot ||
        evidence.expected.userCustomPath != exactCustom) {
      return failedResult({
          .reason = DormantV1CaptureReason::InvalidGenerationPaths,
          .generationIndex = index,
          .validatedNonce = nonce,
      });
    }
    if (evidence.files.size() != payloadPaths.size()) {
      return failedResult({
          .reason = DormantV1CaptureReason::InvalidGenerationInventory,
          .generationIndex = index,
          .validatedNonce = nonce,
      });
    }
    QSet<QString> actualPaths;
    for (auto iterator = evidence.files.constBegin();
         iterator != evidence.files.constEnd(); ++iterator) {
      if (!boundedRelativePath(iterator.key())) {
        return failedResult({
            .reason = DormantV1CaptureReason::InvalidGenerationInventory,
            .generationIndex = index,
            .validatedNonce = nonce,
        });
      }
      actualPaths.insert(iterator.key());
    }
    if (actualPaths != payloadPaths) {
      return failedResult({
          .reason = DormantV1CaptureReason::InvalidGenerationInventory,
          .generationIndex = index,
          .validatedNonce = nonce,
      });
    }
    if (evidence.desiredBytes.size() > maximumDormantV1MetadataCaptureBytes ||
        evidence.manifestBytes.size() > maximumDormantV1MetadataCaptureBytes) {
      return failedResult({
          .reason = DormantV1CaptureReason::GenerationEvidenceOversized,
          .generationIndex = index,
          .validatedNonce = nonce,
      });
    }
    if (!addBounded(callerPayloadBytes, evidence.desiredBytes.size(),
                    maximumDormantV1CallerPayloadBytes) ||
        !addBounded(callerPayloadBytes, evidence.manifestBytes.size(),
                    maximumDormantV1CallerPayloadBytes) ||
        !addBounded(retainedBytes, evidence.manifestBytes.size(),
                    maximumDormantV1RetainedPayloadBytes)) {
      return failedResult({
          .reason = DormantV1CaptureReason::CallerPayloadTooLarge,
          .generationIndex = index,
          .validatedNonce = nonce,
      });
    }
    for (auto iterator = evidence.files.constBegin();
         iterator != evidence.files.constEnd(); ++iterator) {
      if (iterator.value().size() > maximumDormantV1GeneratedFileCaptureBytes) {
        return failedResult({
            .reason = DormantV1CaptureReason::GenerationEvidenceOversized,
            .subject = DormantV1CaptureSubject::GenerationPayload,
            .generationIndex = index,
            .validatedNonce = nonce,
            .relativePath = iterator.key(),
        });
      }
      if (!addBounded(callerPayloadBytes, iterator.value().size(),
                      maximumDormantV1CallerPayloadBytes) ||
          !addBounded(retainedBytes, iterator.value().size(),
                      maximumDormantV1RetainedPayloadBytes)) {
        return failedResult({
            .reason = DormantV1CaptureReason::CallerPayloadTooLarge,
            .subject = DormantV1CaptureSubject::GenerationPayload,
            .generationIndex = index,
            .validatedNonce = nonce,
            .relativePath = iterator.key(),
        });
      }
    }
    references.append({.evidence = &evidence, .originalIndex = index});
  }
  if (retainedBytes > maximumDormantV1RetainedPayloadBytes ||
      retainedBytes > maximumDormantV1TwoPassPayloadReadBytes / 2) {
    return failedResult(
        {.reason = DormantV1CaptureReason::CallerPayloadTooLarge});
  }
  std::sort(
      references.begin(), references.end(),
      [](const GenerationReference &left, const GenerationReference &right) {
        return left.evidence->expected.activationNonce <
               right.evidence->expected.activationNonce;
      });

  RootBaseline rootBaseline;
  OperationBudget initialRootBudget{dormantV1InitialRootAttemptReserve,
                                    dormantV1RootCleanupAttemptReserve};
  if (!initialRootBudget.proofAttempt()) {
    return failedResult(
        {.reason = DormantV1CaptureReason::CaptureBudgetExceeded});
  }
  const auto expectedOwner = ::geteuid();
  if (auto failure = establishRootBaseline(roots, rootBaseline,
                                           initialRootBudget, expectedOwner)) {
    if (initialRootBudget.exceeded) {
      failure->reason = DormantV1CaptureReason::CaptureBudgetExceeded;
    }
    return failedResult(std::move(*failure));
  }
  invokeCheckpoint(InternalCheckpoint::AfterInitialRootGuard,
                   InternalPass::First);

  ReadBudget readBudget;
  std::optional<Failure> tentative;
  const auto fail = [&tentative, &expected](Failure value) {
    if (tentative)
      return;
    if (value.validatedNonce.isEmpty() && value.generationIndex >= 0 &&
        value.generationIndex < expected.referencedGenerations.size()) {
      value.validatedNonce =
          expected.referencedGenerations.at(value.generationIndex)
              .expected.activationNonce;
    }
    value.validatedNonce = owningString(value.validatedNonce);
    value.relativePath = owningString(value.relativePath);
    tentative = std::move(value);
  };
  const auto guard = [&] {
    DormantV1CaptureSubject failedSubject = DormantV1CaptureSubject::None;
    if (retainedRootGuardMatches(roots, rootBaseline, readBudget.operations,
                                 expectedOwner, failedSubject))
      return true;
    fail({
        .reason = readBudget.exhausted()
                      ? DormantV1CaptureReason::CaptureBudgetExceeded
                      : DormantV1CaptureReason::RootsChanged,
        .subject = failedSubject,
    });
    return false;
  };
  static_cast<void>(guard());

  DormantV1FilesystemCapture captured;
  captured.roots = rootBaseline.capture;
  std::vector<std::unique_ptr<GenerationWork>> works;
  works.reserve(static_cast<size_t>(references.size()));
  for (const auto reference : references) {
    works.push_back(
        std::make_unique<GenerationWork>(reference, &readBudget.operations));
  }

  const auto captureFixedPass = [&](const InternalPass pass) {
    for (const auto subject : fixedSubjects) {
      if (tentative || !guard())
        return;
      const auto current = captureFileAt(
          recordDirectory(roots, subject), QByteArrayView(recordName(subject)),
          {.maximumBytes = maximumDormantV1MetadataCaptureBytes,
           .exactMode = privateFileMode},
          readBudget, expectedOwner, pass, subject);
      invokeCheckpoint(InternalCheckpoint::AfterFixedRecord, pass, subject);
      if (!guard())
        return;
      if (current.kind == DormantV1FileCaptureKind::Unsafe) {
        fail({
            .reason = readBudget.exhausted()
                          ? DormantV1CaptureReason::CaptureBudgetExceeded
                          : DormantV1CaptureReason::FixedRecordUnsafe,
            .subject = subject,
        });
        return;
      }
      if (!captureMatchesExpected(current, expectedRecord(expected, subject))) {
        fail({
            .reason = DormantV1CaptureReason::FixedRecordMismatch,
            .subject = subject,
        });
        return;
      }
      if (pass == InternalPass::First) {
        capturedRecord(captured.records, subject) = current;
      } else if (current != capturedRecord(captured.records, subject)) {
        fail({
            .reason = DormantV1CaptureReason::FixedRecordMismatch,
            .subject = subject,
        });
        return;
      }
    }
  };

  captureFixedPass(InternalPass::First);

  const auto inventoryExact = [&](const GenerationWork &work,
                                  const InternalPass pass) {
    if (exactRawInventory(work.rootFd, expectedRootNames(), readBudget, pass,
                          DormantV1CaptureSubject::GenerationRoot,
                          work.reference.originalIndex) !=
        InventoryResult::Exact) {
      return DormantV1CaptureSubject::GenerationRoot;
    }
    if (exactRawInventory(work.modulesFd, expectedModuleNames(), readBudget,
                          pass, DormantV1CaptureSubject::GenerationModules,
                          work.reference.originalIndex) !=
        InventoryResult::Exact) {
      return DormantV1CaptureSubject::GenerationModules;
    }
    return DormantV1CaptureSubject::None;
  };

  const auto captureGenerationFile = [&](GenerationWork &work,
                                         const InternalPass pass,
                                         const QString &relativePath,
                                         const QByteArray &expectedBytes,
                                         const bool manifest) {
    if (tentative || !guard())
      return;
    const auto subject = manifest ? DormantV1CaptureSubject::GenerationManifest
                                  : DormantV1CaptureSubject::GenerationPayload;
    const auto sourceDirectory =
        manifest || !relativePath.contains('/') ? work.rootFd : work.modulesFd;
    const auto name =
        manifest
            ? QByteArrayLiteral("manifest.json")
            : QFile::encodeName(relativePath.section(QLatin1Char('/'), -1));
    const auto current = captureFileAt(
        sourceDirectory, name,
        {.maximumBytes = manifest ? maximumDormantV1MetadataCaptureBytes
                                  : maximumDormantV1GeneratedFileCaptureBytes,
         .exactMode = immutableFileMode,
         .expectedDevice = static_cast<quint64>(
             manifest                     ? work.rootBaseline.st_dev
             : relativePath.contains('/') ? work.modulesBaseline.st_dev
                                          : work.rootBaseline.st_dev)},
        readBudget, expectedOwner, pass, subject, work.reference.originalIndex,
        relativePath);
    invokeCheckpoint(InternalCheckpoint::AfterGenerationFile, pass, subject,
                     work.reference.originalIndex, relativePath);
    if (!guard())
      return;
    if (current.kind == DormantV1FileCaptureKind::Missing) {
      fail({
          .reason = DormantV1CaptureReason::GenerationFileMissing,
          .subject = subject,
          .generationIndex = work.reference.originalIndex,
          .relativePath = relativePath,
      });
      return;
    }
    if (current.kind == DormantV1FileCaptureKind::Unsafe) {
      fail({
          .reason = readBudget.exhausted()
                        ? DormantV1CaptureReason::CaptureBudgetExceeded
                        : DormantV1CaptureReason::GenerationFileUnsafe,
          .subject = subject,
          .generationIndex = work.reference.originalIndex,
          .relativePath = relativePath,
      });
      return;
    }
    if (current.bytes != expectedBytes) {
      fail({
          .reason = DormantV1CaptureReason::GenerationFileMismatch,
          .subject = subject,
          .generationIndex = work.reference.originalIndex,
          .relativePath = relativePath,
      });
      return;
    }
    DormantV1FileCapture *first = nullptr;
    if (manifest) {
      first = &work.capture.manifest;
    } else {
      first = &work.capture.files[relativePath];
    }
    if (pass == InternalPass::First) {
      *first = current;
    } else if (current != *first) {
      fail({
          .reason = DormantV1CaptureReason::GenerationFileMismatch,
          .subject = subject,
          .generationIndex = work.reference.originalIndex,
          .relativePath = relativePath,
      });
    }
  };

  // Aggregate pass A: all fixed records were captured above; now open and
  // capture every sorted generation while retaining the directory fds.
  for (auto &ownedWork : works) {
    if (tentative || !guard())
      break;
    auto &work = *ownedWork;
    const auto &evidence = *work.reference.evidence;
    const auto nonce = evidence.expected.activationNonce.toLatin1();
    auto rootOpenError = 0;
    if (readBudget.attempt()) {
      work.rootFd = ::openat(roots.generationsDirectoryFd, nonce.constData(),
                             O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW);
      if (work.rootFd < 0)
        rootOpenError = errno;
    }
    if (work.rootFd < 0) {
      fail({
          .reason = readBudget.exhausted()
                        ? DormantV1CaptureReason::CaptureBudgetExceeded
                    : rootOpenError == ENOENT
                        ? DormantV1CaptureReason::GenerationRootMissing
                        : DormantV1CaptureReason::GenerationRootUnsafe,
          .subject = DormantV1CaptureSubject::GenerationRoot,
          .generationIndex = work.reference.originalIndex,
      });
      break;
    }
    if (!exactImmutableDirectory(work.rootFd, work.rootBaseline,
                                 readBudget.operations, expectedOwner) ||
        work.rootBaseline.st_dev != rootBaseline.info[3].st_dev ||
        !descriptorStillNamed(roots.generationsDirectoryFd, nonce, work.rootFd,
                              readBudget.operations)) {
      fail({
          .reason = readBudget.exhausted()
                        ? DormantV1CaptureReason::CaptureBudgetExceeded
                        : DormantV1CaptureReason::GenerationRootUnsafe,
          .subject = DormantV1CaptureSubject::GenerationRoot,
          .generationIndex = work.reference.originalIndex,
      });
      break;
    }
    for (const auto &prior : works) {
      if (prior.get() == &work || prior->rootFd < 0)
        break;
      if (prior->rootBaseline.st_dev == work.rootBaseline.st_dev &&
          prior->rootBaseline.st_ino == work.rootBaseline.st_ino) {
        fail({
            .reason = DormantV1CaptureReason::GenerationIdentityAlias,
            .subject = DormantV1CaptureSubject::GenerationRoot,
            .generationIndex = work.reference.originalIndex,
        });
        break;
      }
    }
    if (tentative)
      break;
    auto modulesOpenError = 0;
    if (readBudget.attempt()) {
      work.modulesFd =
          ::openat(work.rootFd, "modules",
                   O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW);
      if (work.modulesFd < 0)
        modulesOpenError = errno;
    }
    if (work.modulesFd < 0) {
      fail({
          .reason = readBudget.exhausted()
                        ? DormantV1CaptureReason::CaptureBudgetExceeded
                    : modulesOpenError == ENOENT
                        ? DormantV1CaptureReason::GenerationModulesMissing
                        : DormantV1CaptureReason::GenerationModulesUnsafe,
          .subject = DormantV1CaptureSubject::GenerationModules,
          .generationIndex = work.reference.originalIndex,
      });
      break;
    }
    if (!exactImmutableDirectory(work.modulesFd, work.modulesBaseline,
                                 readBudget.operations, expectedOwner) ||
        work.modulesBaseline.st_dev != work.rootBaseline.st_dev ||
        !descriptorStillNamed(work.rootFd, QByteArrayView("modules"),
                              work.modulesFd, readBudget.operations)) {
      fail({
          .reason = readBudget.exhausted()
                        ? DormantV1CaptureReason::CaptureBudgetExceeded
                        : DormantV1CaptureReason::GenerationModulesUnsafe,
          .subject = DormantV1CaptureSubject::GenerationModules,
          .generationIndex = work.reference.originalIndex,
      });
      break;
    }
    invokeCheckpoint(InternalCheckpoint::AfterGenerationDirectories,
                     InternalPass::First,
                     DormantV1CaptureSubject::GenerationModules,
                     work.reference.originalIndex);
    if (!guard())
      break;
    const auto inventoryFailure = inventoryExact(work, InternalPass::First);
    if (inventoryFailure != DormantV1CaptureSubject::None) {
      fail({
          .reason = readBudget.exhausted()
                        ? DormantV1CaptureReason::CaptureBudgetExceeded
                        : DormantV1CaptureReason::GenerationInventoryMismatch,
          .subject = inventoryFailure,
          .generationIndex = work.reference.originalIndex,
      });
      break;
    }
    invokeCheckpoint(
        InternalCheckpoint::AfterGenerationInventory, InternalPass::First,
        DormantV1CaptureSubject::GenerationRoot, work.reference.originalIndex);
    if (!guard())
      break;
    work.capture.activationNonce =
        owningString(evidence.expected.activationNonce);
    work.capture.rootIdentity = directoryIdentity(work.rootBaseline);
    work.capture.modulesIdentity = directoryIdentity(work.modulesBaseline);
    captureGenerationFile(work, InternalPass::First,
                          QStringLiteral("manifest.json"),
                          evidence.manifestBytes, true);
    if (tentative)
      break;
    captureGenerationFile(
        work, InternalPass::First, QStringLiteral("hyprland.lua"),
        evidence.files.value(QStringLiteral("hyprland.lua")), false);
    for (const auto *name : moduleNames) {
      if (tentative)
        break;
      const auto path = QStringLiteral("modules/") + QString::fromLatin1(name);
      captureGenerationFile(work, InternalPass::First, path,
                            evidence.files.value(path), false);
    }
    if (!tentative && !generationDirectoriesStillExact(roots, work, readBudget,
                                                       expectedOwner)) {
      fail({
          .reason = readBudget.exhausted()
                        ? DormantV1CaptureReason::CaptureBudgetExceeded
                        : DormantV1CaptureReason::GenerationTreeChanged,
          .subject = DormantV1CaptureSubject::GenerationRoot,
          .generationIndex = work.reference.originalIndex,
      });
    }
    if (!tentative)
      static_cast<void>(guard());
  }

  // Distinct nonce roots and every distinct generation node must remain
  // distinct filesystem identities. Byte-identical independent trees are
  // allowed; semantic generation aliases remain the pure preflight's job.
  if (!tentative) {
    QSet<QString> identities;
    for (const auto &ownedWork : works) {
      const auto &work = *ownedWork;
      const auto rejectAlias = [&](const quint64 device, const quint64 inode,
                                   const DormantV1CaptureSubject subject,
                                   const QString &path = {}) {
        const auto key = identityKey(device, inode);
        if (identities.contains(key)) {
          fail({
              .reason = DormantV1CaptureReason::GenerationIdentityAlias,
              .subject = subject,
              .generationIndex = work.reference.originalIndex,
              .relativePath = path,
          });
          return false;
        }
        identities.insert(key);
        return true;
      };
      if (!rejectAlias(work.capture.rootIdentity.device,
                       work.capture.rootIdentity.inode,
                       DormantV1CaptureSubject::GenerationRoot) ||
          !rejectAlias(work.capture.modulesIdentity.device,
                       work.capture.modulesIdentity.inode,
                       DormantV1CaptureSubject::GenerationModules) ||
          !rejectAlias(work.capture.manifest.identity.device,
                       work.capture.manifest.identity.inode,
                       DormantV1CaptureSubject::GenerationManifest,
                       QStringLiteral("manifest.json")))
        break;
      for (auto iterator = work.capture.files.constBegin();
           iterator != work.capture.files.constEnd(); ++iterator) {
        if (!rejectAlias(iterator->identity.device, iterator->identity.inode,
                         DormantV1CaptureSubject::GenerationPayload,
                         iterator.key()))
          break;
      }
      if (tentative)
        break;
    }
  }

  invokeCheckpoint(InternalCheckpoint::BetweenPasses, InternalPass::First);
  if (!tentative)
    static_cast<void>(guard());

  // Aggregate pass B starts only after every pass-A tree has completed.
  if (!tentative)
    captureFixedPass(InternalPass::Second);
  for (auto &ownedWork : works) {
    if (tentative || !guard())
      break;
    auto &work = *ownedWork;
    const auto &evidence = *work.reference.evidence;
    if (!generationDirectoriesStillExact(roots, work, readBudget,
                                         expectedOwner)) {
      fail({
          .reason = readBudget.exhausted()
                        ? DormantV1CaptureReason::CaptureBudgetExceeded
                        : DormantV1CaptureReason::GenerationTreeChanged,
          .subject = DormantV1CaptureSubject::GenerationRoot,
          .generationIndex = work.reference.originalIndex,
      });
      break;
    }
    const auto inventoryFailure = inventoryExact(work, InternalPass::Second);
    if (inventoryFailure != DormantV1CaptureSubject::None) {
      fail({
          .reason = readBudget.exhausted()
                        ? DormantV1CaptureReason::CaptureBudgetExceeded
                        : DormantV1CaptureReason::GenerationInventoryMismatch,
          .subject = inventoryFailure,
          .generationIndex = work.reference.originalIndex,
      });
      break;
    }
    invokeCheckpoint(
        InternalCheckpoint::AfterGenerationInventory, InternalPass::Second,
        DormantV1CaptureSubject::GenerationRoot, work.reference.originalIndex);
    if (!guard())
      break;
    captureGenerationFile(work, InternalPass::Second,
                          QStringLiteral("manifest.json"),
                          evidence.manifestBytes, true);
    if (tentative)
      break;
    captureGenerationFile(
        work, InternalPass::Second, QStringLiteral("hyprland.lua"),
        evidence.files.value(QStringLiteral("hyprland.lua")), false);
    for (const auto *name : moduleNames) {
      if (tentative)
        break;
      const auto path = QStringLiteral("modules/") + QString::fromLatin1(name);
      captureGenerationFile(work, InternalPass::Second, path,
                            evidence.files.value(path), false);
    }
    if (!tentative && !generationDirectoriesStillExact(roots, work, readBudget,
                                                       expectedOwner)) {
      fail({
          .reason = readBudget.exhausted()
                        ? DormantV1CaptureReason::CaptureBudgetExceeded
                        : DormantV1CaptureReason::GenerationTreeChanged,
          .subject = DormantV1CaptureSubject::GenerationRoot,
          .generationIndex = work.reference.originalIndex,
      });
    }
    if (!tentative)
      static_cast<void>(guard());
  }

  invokeCheckpoint(InternalCheckpoint::BeforeFinalRootGuard,
                   InternalPass::Second);
  // Root drift deliberately overrides every tentative local result.
  OperationBudget finalRootBudget{dormantV1FinalRootAttemptReserve,
                                  dormantV1RootCleanupAttemptReserve};
  DormantV1CaptureSubject finalRootSubject = DormantV1CaptureSubject::None;
  if (!rootGuardMatches(roots, rootBaseline, finalRootBudget, expectedOwner,
                        finalRootSubject)) {
    return failedResult({
        .reason = finalRootBudget.exceeded
                      ? DormantV1CaptureReason::CaptureBudgetExceeded
                      : DormantV1CaptureReason::RootsChanged,
        .subject = finalRootSubject,
    });
  }
  if (!tentative && readBudget.exhausted()) {
    tentative = Failure{
        .reason = DormantV1CaptureReason::CaptureBudgetExceeded,
    };
  }
  if (tentative)
    return failedResult(std::move(*tentative));

  captured.generations.reserve(static_cast<qsizetype>(works.size()));
  for (const auto &work : works)
    captured.generations.append(work->capture);
  return {
      .disposition = DormantV1CaptureDisposition::Captured,
      .reason = DormantV1CaptureReason::None,
      .capture = std::move(captured),
  };
}

} // namespace HyprShelld::Compositor
