#include "dormant_v2_generation_tree_capture.h"

#include <algorithm>
#include <array>
#include <cerrno>
#include <cstring>
#include <new>
#include <utility>

#include <dirent.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

namespace HyprShelld::Compositor {
namespace {

constexpr mode_t privateDirectoryMode = 0700;
constexpr mode_t immutableDirectoryMode = 0500;
constexpr mode_t immutableFileMode = 0400;
constexpr quint64 maximumExtraPreadAttempts = 129;
constexpr quint64 maximumTestHookInvocations = 131072;
constexpr size_t maximumDirentBasenameBytes = 255;

enum class InternalPass { First, Second };

enum class InternalCheckpoint {
  AfterRootDuplicate,
  BeforeRootGuard,
  AfterRootGuard,
  BeforeLookup,
  AfterLookup,
  BeforeOpen,
  AfterOpen,
  AfterOpenedFstat,
  AfterPread,
  BeforeFinalNamedCheck,
  BeforeReaddir,
  AfterReaddir,
  AfterGenerationDirectories,
  AfterInventory,
  AfterFile,
  BetweenPasses,
  BeforeHeldFirstPassFinalization,
  BeforeFinalRootGuard,
};

enum class InternalSyscall {
  DuplicateRoot,
  GetDescriptorFlags,
  GetStatusFlags,
  GetEffectiveUid,
  RootFstat,
  InitialFstatat,
  Openat,
  GetOpenedDescriptorFlags,
  OpenedFstat,
  Pread,
  AfterReadFstat,
  FinalNamedFstatat,
  ReopenDirectory,
  Fdopendir,
  Readdir,
  HeldFirstPassFstat,
  HeldFirstPassNamedFstatat,
  CloseFile,
  CloseDirectory,
  CloseRoot,
  Closedir,
};

enum class InternalFault {
  None,
  FailEintr,
  FailIo,
  ShortReadOneByte,
  ExhaustProofBudget,
  ReportCleanupFailure,
};

struct FileSpec final {
  const char *basename = nullptr;
  const char *relativePath = nullptr;
  DormantV2GenerationTreeFile file = DormantV2GenerationTreeFile::None;
  DormantV2GenerationTreeCaptureSubject subject =
      DormantV2GenerationTreeCaptureSubject::None;
  qsizetype maximumBytes = 0;
  bool modules = false;
};

constexpr std::array<FileSpec, 18> fileSpecs{{
    {"manifest.json", "manifest.json", DormantV2GenerationTreeFile::Manifest,
     DormantV2GenerationTreeCaptureSubject::Manifest,
     maximumDormantV2GenerationManifestCaptureBytes, false},
    {"hyprland.lua", "hyprland.lua", DormantV2GenerationTreeFile::Entrypoint,
     DormantV2GenerationTreeCaptureSubject::Payload,
     maximumDormantV2GeneratedFileCaptureBytes, false},
    {"00-session.lua", "modules/00-session.lua",
     DormantV2GenerationTreeFile::Module00Session,
     DormantV2GenerationTreeCaptureSubject::Payload,
     maximumDormantV2GeneratedFileCaptureBytes, true},
    {"10-monitors.lua", "modules/10-monitors.lua",
     DormantV2GenerationTreeFile::Module10Monitors,
     DormantV2GenerationTreeCaptureSubject::Payload,
     maximumDormantV2GeneratedFileCaptureBytes, true},
    {"20-environment.lua", "modules/20-environment.lua",
     DormantV2GenerationTreeFile::Module20Environment,
     DormantV2GenerationTreeCaptureSubject::Payload,
     maximumDormantV2GeneratedFileCaptureBytes, true},
    {"30-input.lua", "modules/30-input.lua",
     DormantV2GenerationTreeFile::Module30Input,
     DormantV2GenerationTreeCaptureSubject::Payload,
     maximumDormantV2GeneratedFileCaptureBytes, true},
    {"31-gestures.lua", "modules/31-gestures.lua",
     DormantV2GenerationTreeFile::Module31Gestures,
     DormantV2GenerationTreeCaptureSubject::Payload,
     maximumDormantV2GeneratedFileCaptureBytes, true},
    {"32-cursor.lua", "modules/32-cursor.lua",
     DormantV2GenerationTreeFile::Module32Cursor,
     DormantV2GenerationTreeCaptureSubject::Payload,
     maximumDormantV2GeneratedFileCaptureBytes, true},
    {"40-general.lua", "modules/40-general.lua",
     DormantV2GenerationTreeFile::Module40General,
     DormantV2GenerationTreeCaptureSubject::Payload,
     maximumDormantV2GeneratedFileCaptureBytes, true},
    {"41-layouts.lua", "modules/41-layouts.lua",
     DormantV2GenerationTreeFile::Module41Layouts,
     DormantV2GenerationTreeCaptureSubject::Payload,
     maximumDormantV2GeneratedFileCaptureBytes, true},
    {"42-workspaces.lua", "modules/42-workspaces.lua",
     DormantV2GenerationTreeFile::Module42Workspaces,
     DormantV2GenerationTreeCaptureSubject::Payload,
     maximumDormantV2GeneratedFileCaptureBytes, true},
    {"43-groups.lua", "modules/43-groups.lua",
     DormantV2GenerationTreeFile::Module43Groups,
     DormantV2GenerationTreeCaptureSubject::Payload,
     maximumDormantV2GeneratedFileCaptureBytes, true},
    {"50-decorations.lua", "modules/50-decorations.lua",
     DormantV2GenerationTreeFile::Module50Decorations,
     DormantV2GenerationTreeCaptureSubject::Payload,
     maximumDormantV2GeneratedFileCaptureBytes, true},
    {"51-animations.lua", "modules/51-animations.lua",
     DormantV2GenerationTreeFile::Module51Animations,
     DormantV2GenerationTreeCaptureSubject::Payload,
     maximumDormantV2GeneratedFileCaptureBytes, true},
    {"60-rules.lua", "modules/60-rules.lua",
     DormantV2GenerationTreeFile::Module60Rules,
     DormantV2GenerationTreeCaptureSubject::Payload,
     maximumDormantV2GeneratedFileCaptureBytes, true},
    {"70-keybinds.lua", "modules/70-keybinds.lua",
     DormantV2GenerationTreeFile::Module70Keybinds,
     DormantV2GenerationTreeCaptureSubject::Payload,
     maximumDormantV2GeneratedFileCaptureBytes, true},
    {"80-permissions.lua", "modules/80-permissions.lua",
     DormantV2GenerationTreeFile::Module80Permissions,
     DormantV2GenerationTreeCaptureSubject::Payload,
     maximumDormantV2GeneratedFileCaptureBytes, true},
    {"90-advanced.lua", "modules/90-advanced.lua",
     DormantV2GenerationTreeFile::Module90Advanced,
     DormantV2GenerationTreeCaptureSubject::Payload,
     maximumDormantV2GeneratedFileCaptureBytes, true},
}};

constexpr std::array<const char *, 3> rootInventory{{
    "manifest.json",
    "hyprland.lua",
    "modules",
}};

constexpr std::array<const char *, 16> modulesInventory{{
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

constexpr quint64 ceilingChunks(const qsizetype maximumBytes) {
  return (static_cast<quint64>(maximumBytes) +
          dormantV2GenerationCaptureReadBufferBytes - 1) /
         dormantV2GenerationCaptureReadBufferBytes;
}

constexpr quint64 maximumPreadAttempts(const qsizetype maximumBytes) {
  return ceilingChunks(maximumBytes) + maximumExtraPreadAttempts;
}

constexpr quint64 maximumAllPreadAttempts =
    2 * maximumDormantV2CapturedGenerationTrees *
    (maximumPreadAttempts(maximumDormantV2GenerationManifestCaptureBytes) +
     dormantV2GenerationPayloadFileCount *
         maximumPreadAttempts(maximumDormantV2GeneratedFileCaptureBytes));
constexpr quint64 maximumRootSetupProofAttempts = 5;
constexpr quint64 maximumDirectoryOpenProofAttempts =
    maximumDormantV2CapturedGenerationTrees * 2 * 5;
constexpr quint64 maximumInventorySetupProofAttempts =
    2 * maximumDormantV2CapturedGenerationTrees * 2 * 4;
constexpr quint64 maximumFileNonReadProofAttempts =
    2 * maximumDormantV2CapturedGenerationTrees * fileSpecs.size() * 6;
constexpr quint64 maximumObservationGuardProofAttempts =
    maximumDormantV2CapturedGenerationTrees * (197 + 195 + 94) + 1;
constexpr quint64 maximumHeldFileFinalProofAttempts =
    maximumDormantV2CapturedGenerationTrees * fileSpecs.size() * 2;
constexpr quint64 maximumNonReadProofAttempts =
    maximumRootSetupProofAttempts + maximumDirectoryOpenProofAttempts +
    maximumInventorySetupProofAttempts +
    maximumDormantV2GenerationEnumerationCalls +
    maximumFileNonReadProofAttempts + maximumObservationGuardProofAttempts +
    maximumHeldFileFinalProofAttempts;
constexpr quint64 conservativeMaximumSuccessfulCleanupAttempts = 85;

static_assert(fileSpecs.size() == 18);
static_assert(modulesInventory.size() == 16);
static_assert(dormantV2GenerationPayloadFileCount == 17);
static_assert(maximumPreadAttempts(
                  maximumDormantV2GenerationManifestCaptureBytes) == 193);
static_assert(maximumPreadAttempts(maximumDormantV2GeneratedFileCaptureBytes) ==
              385);
static_assert(maximumAllPreadAttempts == 26952);
static_assert(maximumNonReadProofAttempts == 1662);
static_assert(maximumAllPreadAttempts + maximumNonReadProofAttempts <=
              maximumDormantV2GenerationProofSyscallAttempts);
static_assert(conservativeMaximumSuccessfulCleanupAttempts <=
              dormantV2GenerationCleanupAttemptReserve);
static_assert(maximumDormantV2GenerationRetainedDescriptors ==
              1 + 2 * maximumDormantV2CapturedGenerationTrees +
                  18 * maximumDormantV2CapturedGenerationTrees);
static_assert(maximumDormantV2GenerationOwnedDescriptors ==
              maximumDormantV2GenerationRetainedDescriptors + 1);

struct CleanupBudget final {
  quint64 remaining = dormantV2GenerationCleanupAttemptReserve;
  bool exceeded = false;

  void attempt() {
    if (remaining == 0) {
      exceeded = true;
      return;
    }
    --remaining;
  }
};

struct OperationBudget final {
  quint64 remaining = maximumDormantV2GenerationProofSyscallAttempts;
  bool exceeded = false;

  explicit OperationBudget(
      const quint64 limit = maximumDormantV2GenerationProofSyscallAttempts)
      : remaining(limit) {}

  [[nodiscard]] bool attempt() {
    if (remaining == 0) {
      exceeded = true;
      errno = EIO;
      return false;
    }
    --remaining;
    return true;
  }

  void exhaust() {
    remaining = 0;
    exceeded = true;
    errno = EIO;
  }
};

struct ReadBudget final {
  quint64 returnedBytesRemaining = maximumDormantV2GenerationPreadReturnedBytes;
  bool returnedBytesExceeded = false;
  bool perFileAttemptsExceeded = false;

  [[nodiscard]] bool consume(const quint64 count) {
    if (count > returnedBytesRemaining) {
      returnedBytesExceeded = true;
      return false;
    }
    returnedBytesRemaining -= count;
    return true;
  }
};

struct EnumerationBudget final {
  quint64 callsRemaining = maximumDormantV2GenerationEnumerationCalls;
  quint64 direntsRemaining = maximumDormantV2GenerationObservedNonDotDirents;
  quint64 nameBytesRemaining = maximumDormantV2GenerationDirentNameBytes;
  bool exceeded = false;
};

struct Failure final {
  DormantV2GenerationTreeCaptureReason reason =
      DormantV2GenerationTreeCaptureReason::None;
  DormantV2GenerationTreeCaptureSubject subject =
      DormantV2GenerationTreeCaptureSubject::None;
  qsizetype generationIndex = -1;
  DormantV2GenerationTreeFile file = DormantV2GenerationTreeFile::None;

  [[nodiscard]] bool set() const {
    return reason != DormantV2GenerationTreeCaptureReason::None;
  }
};

struct Reference final {
  QString activationNonce;
  std::array<char, 33> encodedNonce{};
  qsizetype originalIndex = -1;
};

struct InternalFile final {
  QByteArray bytes;
  struct stat metadata{};
  int heldFirstPassFd = -1;
};

struct InternalTree final {
  Reference reference;
  int rootFd = -1;
  int modulesFd = -1;
  struct stat rootBaseline{};
  struct stat modulesBaseline{};
  std::array<InternalFile, fileSpecs.size()> files{};
};

struct IdentitySet final {
  struct Identity final {
    dev_t device = 0;
    ino_t inode = 0;
  };

  std::array<Identity, 41> values{};
  size_t count = 0;

  [[nodiscard]] bool add(const struct stat &value) {
    for (size_t index = 0; index < count; ++index) {
      if (values[index].device == value.st_dev &&
          values[index].inode == value.st_ino) {
        return false;
      }
    }
    if (count >= values.size())
      return false;
    values[count++] = {.device = value.st_dev, .inode = value.st_ino};
    return true;
  }
};

#if defined(HYPRSHELLD_DORMANT_V2_GENERATION_TREE_CAPTURE_TEST_HOOKS)
thread_local DormantV2GenerationTreeCaptureTestSupport::CheckpointHook
    testCheckpointHook;
thread_local DormantV2GenerationTreeCaptureTestSupport::SyscallHook
    testSyscallHook;
thread_local DormantV2GenerationTreeCaptureTestSupport::
    PayloadAllocationFailureHook testPayloadAllocationFailureHook;
thread_local quint64 testCheckpointInvocations = 0;
thread_local quint64 testSyscallInvocations = 0;
#endif

#if defined(HYPRSHELLD_DORMANT_V2_GENERATION_TREE_CAPTURE_TEST_HOOKS)
[[nodiscard]] constexpr DormantV2GenerationTreeCaptureTestSupport::Pass
testPass(const InternalPass pass) {
  return pass == InternalPass::First
             ? DormantV2GenerationTreeCaptureTestSupport::Pass::First
             : DormantV2GenerationTreeCaptureTestSupport::Pass::Second;
}
#endif

void invokeCheckpoint(
    const InternalCheckpoint checkpoint, const InternalPass pass,
    const DormantV2GenerationTreeCaptureSubject subject =
        DormantV2GenerationTreeCaptureSubject::None,
    const qsizetype generationIndex = -1,
    const DormantV2GenerationTreeFile file = DormantV2GenerationTreeFile::None,
    const quint64 preadInvocation = 0,
    const qsizetype heldFirstPassDescriptors = 0) {
#if defined(HYPRSHELLD_DORMANT_V2_GENERATION_TREE_CAPTURE_TEST_HOOKS)
  if (!testCheckpointHook ||
      ++testCheckpointInvocations > maximumTestHookInvocations) {
    return;
  }
  try {
    testCheckpointHook({
        .checkpoint =
            static_cast<DormantV2GenerationTreeCaptureTestSupport::Checkpoint>(
                checkpoint),
        .pass = testPass(pass),
        .subject = subject,
        .generationIndex = generationIndex,
        .file = file,
        .preadInvocation = preadInvocation,
        .heldFirstPassDescriptors = heldFirstPassDescriptors,
    });
  } catch (...) {
    // A focused-test seam cannot escape into the capture algorithm.
  }
#else
  Q_UNUSED(checkpoint)
  Q_UNUSED(pass)
  Q_UNUSED(subject)
  Q_UNUSED(generationIndex)
  Q_UNUSED(file)
  Q_UNUSED(preadInvocation)
  Q_UNUSED(heldFirstPassDescriptors)
#endif
}

[[nodiscard]] InternalFault
injectedFault(const InternalSyscall syscall, const InternalPass pass,
              const DormantV2GenerationTreeCaptureSubject subject,
              const qsizetype generationIndex,
              const DormantV2GenerationTreeFile file) {
#if defined(HYPRSHELLD_DORMANT_V2_GENERATION_TREE_CAPTURE_TEST_HOOKS)
  if (!testSyscallHook ||
      ++testSyscallInvocations > maximumTestHookInvocations) {
    return InternalFault::None;
  }
  try {
    return static_cast<InternalFault>(testSyscallHook({
        .syscall =
            static_cast<DormantV2GenerationTreeCaptureTestSupport::Syscall>(
                syscall),
        .pass = testPass(pass),
        .subject = subject,
        .generationIndex = generationIndex,
        .file = file,
        .invocation = testSyscallInvocations,
    }));
  } catch (...) {
    return InternalFault::FailIo;
  }
#else
  Q_UNUSED(syscall)
  Q_UNUSED(pass)
  Q_UNUSED(subject)
  Q_UNUSED(generationIndex)
  Q_UNUSED(file)
  return InternalFault::None;
#endif
}

[[nodiscard]] bool
shouldFailPayloadAllocation(const InternalPass pass,
                            const qsizetype generationIndex,
                            const DormantV2GenerationTreeFile file) {
#if defined(HYPRSHELLD_DORMANT_V2_GENERATION_TREE_CAPTURE_TEST_HOOKS)
  if (!testPayloadAllocationFailureHook)
    return false;
  try {
    return testPayloadAllocationFailureHook(testPass(pass), generationIndex,
                                            file);
  } catch (...) {
    return true;
  }
#else
  Q_UNUSED(pass)
  Q_UNUSED(generationIndex)
  Q_UNUSED(file)
  return false;
#endif
}

[[nodiscard]] bool
beginProof(OperationBudget &budget, const InternalSyscall syscall,
           const InternalPass pass,
           const DormantV2GenerationTreeCaptureSubject subject,
           const qsizetype generationIndex,
           const DormantV2GenerationTreeFile file, InternalFault &fault) {
  if (!budget.attempt())
    return false;
  fault = injectedFault(syscall, pass, subject, generationIndex, file);
  if (fault == InternalFault::ExhaustProofBudget) {
    budget.exhaust();
    return false;
  }
  if (fault == InternalFault::FailEintr) {
    errno = EINTR;
    return false;
  }
  if (fault == InternalFault::FailIo) {
    errno = EIO;
    return false;
  }
  return true;
}

void chooseFailure(Failure &failure,
                   const DormantV2GenerationTreeCaptureReason reason,
                   const DormantV2GenerationTreeCaptureSubject subject,
                   const qsizetype generationIndex = -1,
                   const DormantV2GenerationTreeFile file =
                       DormantV2GenerationTreeFile::None) {
  if (!failure.set()) {
    failure = {
        .reason = reason,
        .subject = subject,
        .generationIndex = generationIndex,
        .file = file,
    };
  }
}

void chooseLocalFailure(
    Failure &failure, const OperationBudget &operations,
    const ReadBudget &reads, const EnumerationBudget &enumeration,
    const DormantV2GenerationTreeCaptureReason ordinaryReason,
    const DormantV2GenerationTreeCaptureSubject subject,
    const qsizetype generationIndex = -1,
    const DormantV2GenerationTreeFile file =
        DormantV2GenerationTreeFile::None) {
  if (operations.exceeded) {
    chooseFailure(failure,
                  DormantV2GenerationTreeCaptureReason::ProofBudgetExceeded,
                  subject, generationIndex, file);
  } else if (reads.returnedBytesExceeded) {
    chooseFailure(failure,
                  DormantV2GenerationTreeCaptureReason::ReturnedBytesExceeded,
                  subject, generationIndex, file);
  } else if (reads.perFileAttemptsExceeded) {
    chooseFailure(
        failure,
        DormantV2GenerationTreeCaptureReason::PerFileReadAttemptsExceeded,
        subject, generationIndex, file);
  } else if (enumeration.exceeded) {
    chooseFailure(
        failure,
        DormantV2GenerationTreeCaptureReason::EnumerationBudgetExceeded,
        subject, generationIndex, file);
  } else {
    chooseFailure(failure, ordinaryReason, subject, generationIndex, file);
  }
}

[[nodiscard]] bool saneSnapshotRanges(const struct stat &value) {
  return value.st_size >= 0 && value.st_nlink > 0 &&
         value.st_mtim.tv_nsec >= 0 && value.st_mtim.tv_nsec <= 999999999 &&
         value.st_ctim.tv_nsec >= 0 && value.st_ctim.tv_nsec <= 999999999;
}

[[nodiscard]] bool sameSnapshot(const struct stat &left,
                                const struct stat &right) {
  return left.st_dev == right.st_dev && left.st_ino == right.st_ino &&
         left.st_mode == right.st_mode && left.st_uid == right.st_uid &&
         left.st_gid == right.st_gid && left.st_nlink == right.st_nlink &&
         left.st_size == right.st_size &&
         left.st_mtim.tv_sec == right.st_mtim.tv_sec &&
         left.st_mtim.tv_nsec == right.st_mtim.tv_nsec &&
         left.st_ctim.tv_sec == right.st_ctim.tv_sec &&
         left.st_ctim.tv_nsec == right.st_ctim.tv_nsec;
}

[[nodiscard]] bool safeRoot(const struct stat &value, const uid_t owner) {
  return S_ISDIR(value.st_mode) && saneSnapshotRanges(value) &&
         value.st_uid == owner &&
         (value.st_mode & 07777) == privateDirectoryMode;
}

[[nodiscard]] bool safeImmutableDirectory(const struct stat &value,
                                          const uid_t owner,
                                          const dev_t rootDevice) {
  return S_ISDIR(value.st_mode) && saneSnapshotRanges(value) &&
         value.st_uid == owner && value.st_dev == rootDevice &&
         (value.st_mode & 07777) == immutableDirectoryMode;
}

[[nodiscard]] bool safeImmutableFile(const struct stat &value,
                                     const uid_t owner, const dev_t rootDevice,
                                     const qsizetype maximumBytes) {
  return S_ISREG(value.st_mode) && saneSnapshotRanges(value) &&
         value.st_uid == owner && value.st_dev == rootDevice &&
         value.st_nlink == 1 && (value.st_mode & 07777) == immutableFileMode &&
         value.st_size <= maximumBytes;
}

[[nodiscard]] bool validActivationNonce(const QStringView value) {
  if (value.size() != 32)
    return false;
  auto nonzero = false;
  for (const auto character : value) {
    if (!((character >= u'0' && character <= u'9') ||
          (character >= u'a' && character <= u'f'))) {
      return false;
    }
    nonzero = nonzero || character != u'0';
  }
  return nonzero;
}

[[nodiscard]] bool
closeOnce(int &descriptor, CleanupBudget &cleanup,
          const InternalSyscall syscall, const InternalPass pass,
          const DormantV2GenerationTreeCaptureSubject subject,
          const qsizetype generationIndex,
          const DormantV2GenerationTreeFile file) {
  if (descriptor < 0)
    return true;
  const auto closing = descriptor;
  descriptor = -1; // Invalidate before the one ambiguous close attempt.
  cleanup.attempt();
  const auto result = ::close(closing);
  const auto fault =
      injectedFault(syscall, pass, subject, generationIndex, file);
  if (fault == InternalFault::ReportCleanupFailure) {
    errno = EIO;
    return false;
  }
  return result == 0;
}

[[nodiscard]] qsizetype heldFirstPassCount(
    const std::array<InternalTree, maximumDormantV2CapturedGenerationTrees>
        &trees,
    const qsizetype treeCount, const int rootFd) {
  qsizetype count = rootFd >= 0 ? 1 : 0;
  for (qsizetype treeIndex = 0; treeIndex < treeCount; ++treeIndex) {
    const auto &tree = trees[static_cast<size_t>(treeIndex)];
    count += tree.rootFd >= 0 ? 1 : 0;
    count += tree.modulesFd >= 0 ? 1 : 0;
    for (const auto &file : tree.files)
      count += file.heldFirstPassFd >= 0 ? 1 : 0;
  }
  return count;
}

[[nodiscard]] bool descriptorHasCloseOnExec(
    int descriptor, OperationBudget &operations, InternalPass pass,
    DormantV2GenerationTreeCaptureSubject subject, qsizetype generationIndex,
    DormantV2GenerationTreeFile file, InternalSyscall syscall);
[[nodiscard]] bool guardedFstat(int descriptor, struct stat &value,
                                OperationBudget &operations, InternalPass pass,
                                DormantV2GenerationTreeCaptureSubject subject,
                                qsizetype generationIndex,
                                DormantV2GenerationTreeFile file,
                                InternalSyscall syscall);
[[nodiscard]] bool
guardedFstatat(int parent, const char *name, struct stat &value,
               OperationBudget &operations, InternalPass pass,
               DormantV2GenerationTreeCaptureSubject subject,
               qsizetype generationIndex, DormantV2GenerationTreeFile file,
               InternalSyscall syscall);
[[nodiscard]] bool rootGuard(int rootFd, const struct stat &baseline,
                             uid_t owner, OperationBudget &operations,
                             InternalPass pass,
                             DormantV2GenerationTreeCaptureSubject subject,
                             qsizetype generationIndex,
                             DormantV2GenerationTreeFile file,
                             qsizetype heldDescriptors);
[[nodiscard]] bool retainedDirectoryStillExact(
    int parentFd, const char *name, int descriptor, const struct stat &baseline,
    uid_t owner, dev_t rootDevice, OperationBudget &operations,
    InternalPass pass, DormantV2GenerationTreeCaptureSubject subject,
    qsizetype generationIndex);
[[nodiscard]] bool openRequiredDirectory(
    int parentFd, const char *name, uid_t owner, dev_t rootDevice,
    OperationBudget &operations, InternalPass pass,
    DormantV2GenerationTreeCaptureSubject subject, qsizetype generationIndex,
    int &descriptor, struct stat &baseline, bool &missing);
template <size_t Count>
[[nodiscard]] bool
exactInventory(int directoryFd, const struct stat &directoryBaseline,
               const std::array<const char *, Count> &expected,
               OperationBudget &operations, EnumerationBudget &enumeration,
               CleanupBudget &cleanup, Failure &cleanupFailure,
               InternalPass pass, DormantV2GenerationTreeCaptureSubject subject,
               qsizetype generationIndex);
[[nodiscard]] bool captureFile(int parentFd, const FileSpec &spec, uid_t owner,
                               dev_t rootDevice, InternalPass pass,
                               qsizetype generationIndex,
                               OperationBudget &operations, ReadBudget &reads,
                               EnumerationBudget &enumeration,
                               CleanupBudget &cleanup, InternalFile &result,
                               Failure &failure, Failure &cleanupFailure);
[[nodiscard]] bool filesEqual(const InternalFile &left,
                              const InternalFile &right);

} // namespace

DormantV2GenerationTreeCaptureResult
captureDormantV2GenerationTrees(const int borrowedGenerationsDirectoryFd,
                                const QVector<QString> &activationNonces) {
  using Reason = DormantV2GenerationTreeCaptureReason;
  using Subject = DormantV2GenerationTreeCaptureSubject;
  using File = DormantV2GenerationTreeFile;

  const auto failedResult = [](const Failure &failure) {
    return DormantV2GenerationTreeCaptureResult(
        DormantV2GenerationTreeCaptureDisposition::FailedClosed, failure.reason,
        failure.subject, failure.generationIndex, failure.file, std::nullopt);
  };

  if (activationNonces.size() > maximumDormantV2CapturedGenerationTrees) {
    return failedResult({
        .reason = Reason::TooManyGenerationReferences,
    });
  }

  std::array<InternalTree, maximumDormantV2CapturedGenerationTrees> trees{};
  const auto treeCount = activationNonces.size();
  try {
    for (qsizetype index = 0; index < treeCount; ++index) {
      const auto &nonce = activationNonces.at(index);
      if (!validActivationNonce(nonce)) {
        return failedResult({
            .reason = Reason::InvalidActivationNonce,
            .generationIndex = index,
        });
      }
      for (qsizetype prior = 0; prior < index; ++prior) {
        if (activationNonces.at(prior) == nonce) {
          return failedResult({
              .reason = Reason::DuplicateActivationNonce,
              .generationIndex = index,
          });
        }
      }
      auto &reference = trees[static_cast<size_t>(index)].reference;
      reference.activationNonce = nonce;
      for (qsizetype character = 0; character < nonce.size(); ++character) {
        reference.encodedNonce[static_cast<size_t>(character)] =
            static_cast<char>(nonce.at(character).unicode());
      }
      reference.originalIndex = index;
    }
  } catch (const std::bad_alloc &) {
    return failedResult({.reason = Reason::AllocationFailed});
  }
  std::sort(trees.begin(), trees.begin() + treeCount,
            [](const InternalTree &left, const InternalTree &right) {
              return left.reference.activationNonce <
                     right.reference.activationNonce;
            });

#if defined(HYPRSHELLD_DORMANT_V2_GENERATION_TREE_CAPTURE_TEST_HOOKS)
  testCheckpointInvocations = 0;
  testSyscallInvocations = 0;
#endif

  OperationBudget operations;
  OperationBudget finalOperations(dormantV2GenerationFinalProofAttemptReserve);
  ReadBudget reads;
  EnumerationBudget enumeration;
  CleanupBudget cleanup;
  Failure failure;
  Failure cleanupFailure;
  int rootFd = -1;
  struct stat rootBaseline{};
  auto rootBaselineValid = false;
  uid_t owner = 0;

  InternalFault fault{};
  if (beginProof(operations, InternalSyscall::DuplicateRoot,
                 InternalPass::First, Subject::GenerationsDirectory, -1,
                 File::None, fault)) {
    rootFd = ::fcntl(borrowedGenerationsDirectoryFd, F_DUPFD_CLOEXEC, 0);
  }
  if (rootFd < 0) {
    chooseFailure(failure,
                  operations.exceeded
                      ? Reason::ProofBudgetExceeded
                      : Reason::InvalidGenerationsDirectoryDescriptor,
                  Subject::GenerationsDirectory);
  } else {
    invokeCheckpoint(InternalCheckpoint::AfterRootDuplicate,
                     InternalPass::First, Subject::GenerationsDirectory);
  }

  if (!failure.set()) {
    const auto descriptorFlagsOkay = descriptorHasCloseOnExec(
        rootFd, operations, InternalPass::First, Subject::GenerationsDirectory,
        -1, File::None, InternalSyscall::GetDescriptorFlags);
    auto statusFlags = -1;
    if (beginProof(operations, InternalSyscall::GetStatusFlags,
                   InternalPass::First, Subject::GenerationsDirectory, -1,
                   File::None, fault)) {
      statusFlags = ::fcntl(rootFd, F_GETFL);
    }
    auto ownerObserved = false;
    if (beginProof(operations, InternalSyscall::GetEffectiveUid,
                   InternalPass::First, Subject::GenerationsDirectory, -1,
                   File::None, fault)) {
      owner = ::geteuid();
      ownerObserved = true;
    }
    invokeCheckpoint(InternalCheckpoint::BeforeRootGuard, InternalPass::First,
                     Subject::GenerationsDirectory);
    const auto snapshotOkay =
        guardedFstat(rootFd, rootBaseline, operations, InternalPass::First,
                     Subject::GenerationsDirectory, -1, File::None,
                     InternalSyscall::RootFstat);
    invokeCheckpoint(InternalCheckpoint::AfterRootGuard, InternalPass::First,
                     Subject::GenerationsDirectory);
    const auto readable = statusFlags >= 0 &&
                          (statusFlags & O_ACCMODE) == O_RDONLY &&
                          (statusFlags & O_PATH) == 0;
    if (!descriptorFlagsOkay || statusFlags < 0 || !ownerObserved) {
      chooseFailure(failure,
                    operations.exceeded
                        ? Reason::ProofBudgetExceeded
                        : Reason::InvalidGenerationsDirectoryDescriptor,
                    Subject::GenerationsDirectory);
    } else if (!readable || !snapshotOkay || !safeRoot(rootBaseline, owner)) {
      chooseFailure(failure,
                    operations.exceeded ? Reason::ProofBudgetExceeded
                                        : Reason::UnsafeGenerationsDirectory,
                    Subject::GenerationsDirectory);
    } else {
      rootBaselineValid = true;
    }
  }

  IdentitySet firstIdentities;
  IdentitySet secondIdentities;
  if (rootBaselineValid) {
    if (!firstIdentities.add(rootBaseline) ||
        !secondIdentities.add(rootBaseline)) {
      chooseFailure(failure, Reason::IdentityAlias,
                    Subject::GenerationsDirectory);
    }
  }

  const auto heldCount = [&] {
    return heldFirstPassCount(trees, treeCount, rootFd);
  };
  const auto ensureRoot = [&](const InternalPass pass, const Subject subject,
                              const qsizetype generationIndex,
                              const File file = File::None) {
    if (rootGuard(rootFd, rootBaseline, owner, operations, pass, subject,
                  generationIndex, file, heldCount())) {
      return true;
    }
    chooseLocalFailure(failure, operations, reads, enumeration,
                       Reason::GenerationsDirectoryChanged,
                       Subject::GenerationsDirectory);
    return false;
  };
  const auto ensureTree = [&](const InternalTree &tree,
                              const InternalPass pass) {
    if (!retainedDirectoryStillExact(
            rootFd, tree.reference.encodedNonce.data(), tree.rootFd,
            tree.rootBaseline, owner, rootBaseline.st_dev, operations, pass,
            Subject::GenerationRoot, tree.reference.originalIndex)) {
      chooseLocalFailure(failure, operations, reads, enumeration,
                         Reason::TreeChanged, Subject::GenerationRoot,
                         tree.reference.originalIndex);
      return false;
    }
    if (!retainedDirectoryStillExact(
            tree.rootFd, "modules", tree.modulesFd, tree.modulesBaseline, owner,
            rootBaseline.st_dev, operations, pass, Subject::ModulesDirectory,
            tree.reference.originalIndex)) {
      chooseLocalFailure(failure, operations, reads, enumeration,
                         Reason::TreeChanged, Subject::ModulesDirectory,
                         tree.reference.originalIndex);
      return false;
    }
    return true;
  };

  // Global pass A: establish and retain every directory and file descriptor
  // for each sorted tree before beginning any pass-B observation.
  if (!failure.set()) {
    for (qsizetype treeIndex = 0; treeIndex < treeCount; ++treeIndex) {
      auto &tree = trees[static_cast<size_t>(treeIndex)];
      const auto originalIndex = tree.reference.originalIndex;
      if (!ensureRoot(InternalPass::First, Subject::GenerationRoot,
                      originalIndex)) {
        break;
      }
      auto missing = false;
      if (!openRequiredDirectory(rootFd, tree.reference.encodedNonce.data(),
                                 owner, rootBaseline.st_dev, operations,
                                 InternalPass::First, Subject::GenerationRoot,
                                 originalIndex, tree.rootFd, tree.rootBaseline,
                                 missing)) {
        chooseLocalFailure(failure, operations, reads, enumeration,
                           missing ? Reason::GenerationRootMissing
                                   : Reason::GenerationRootUnsafe,
                           Subject::GenerationRoot, originalIndex);
        break;
      }
      if (!firstIdentities.add(tree.rootBaseline)) {
        chooseFailure(failure, Reason::IdentityAlias, Subject::GenerationRoot,
                      originalIndex);
        break;
      }
      if (!ensureRoot(InternalPass::First, Subject::GenerationRoot,
                      originalIndex)) {
        break;
      }
      if (!openRequiredDirectory(
              tree.rootFd, "modules", owner, rootBaseline.st_dev, operations,
              InternalPass::First, Subject::ModulesDirectory, originalIndex,
              tree.modulesFd, tree.modulesBaseline, missing)) {
        chooseLocalFailure(failure, operations, reads, enumeration,
                           missing ? Reason::ModulesDirectoryMissing
                                   : Reason::ModulesDirectoryUnsafe,
                           Subject::ModulesDirectory, originalIndex);
        break;
      }
      if (!firstIdentities.add(tree.modulesBaseline)) {
        chooseFailure(failure, Reason::IdentityAlias, Subject::ModulesDirectory,
                      originalIndex);
        break;
      }
      invokeCheckpoint(InternalCheckpoint::AfterGenerationDirectories,
                       InternalPass::First, Subject::ModulesDirectory,
                       originalIndex, File::None, 0, heldCount());
      if (!ensureRoot(InternalPass::First, Subject::GenerationRoot,
                      originalIndex) ||
          !ensureTree(tree, InternalPass::First)) {
        break;
      }

      if (!exactInventory(tree.rootFd, tree.rootBaseline, rootInventory,
                          operations, enumeration, cleanup, cleanupFailure,
                          InternalPass::First, Subject::GenerationRoot,
                          originalIndex)) {
        chooseLocalFailure(failure, operations, reads, enumeration,
                           Reason::InventoryMismatch, Subject::GenerationRoot,
                           originalIndex);
        break;
      }
      invokeCheckpoint(InternalCheckpoint::AfterInventory, InternalPass::First,
                       Subject::GenerationRoot, originalIndex, File::None, 0,
                       heldCount());
      if (cleanupFailure.set() ||
          !ensureRoot(InternalPass::First, Subject::GenerationRoot,
                      originalIndex) ||
          !ensureTree(tree, InternalPass::First)) {
        break;
      }
      if (!exactInventory(tree.modulesFd, tree.modulesBaseline,
                          modulesInventory, operations, enumeration, cleanup,
                          cleanupFailure, InternalPass::First,
                          Subject::ModulesDirectory, originalIndex)) {
        chooseLocalFailure(failure, operations, reads, enumeration,
                           Reason::InventoryMismatch, Subject::ModulesDirectory,
                           originalIndex);
        break;
      }
      invokeCheckpoint(InternalCheckpoint::AfterInventory, InternalPass::First,
                       Subject::ModulesDirectory, originalIndex, File::None, 0,
                       heldCount());
      if (cleanupFailure.set() ||
          !ensureRoot(InternalPass::First, Subject::ModulesDirectory,
                      originalIndex) ||
          !ensureTree(tree, InternalPass::First)) {
        break;
      }

      for (size_t fileIndex = 0; fileIndex < fileSpecs.size(); ++fileIndex) {
        const auto &spec = fileSpecs[fileIndex];
        if (!ensureRoot(InternalPass::First, spec.subject, originalIndex,
                        spec.file) ||
            !ensureTree(tree, InternalPass::First)) {
          break;
        }
        const auto parentFd = spec.modules ? tree.modulesFd : tree.rootFd;
        if (!captureFile(parentFd, spec, owner, rootBaseline.st_dev,
                         InternalPass::First, originalIndex, operations, reads,
                         enumeration, cleanup, tree.files[fileIndex], failure,
                         cleanupFailure)) {
          break;
        }
        if (!firstIdentities.add(tree.files[fileIndex].metadata)) {
          chooseFailure(failure, Reason::IdentityAlias, spec.subject,
                        originalIndex, spec.file);
          break;
        }
        invokeCheckpoint(InternalCheckpoint::AfterFile, InternalPass::First,
                         spec.subject, originalIndex, spec.file, 0,
                         heldCount());
        if (cleanupFailure.set() ||
            !ensureRoot(InternalPass::First, spec.subject, originalIndex,
                        spec.file) ||
            !ensureTree(tree, InternalPass::First)) {
          break;
        }
      }
      if (failure.set() || cleanupFailure.set())
        break;
    }
  }

  if (!failure.set() && !cleanupFailure.set()) {
    invokeCheckpoint(InternalCheckpoint::BetweenPasses, InternalPass::First,
                     Subject::GenerationsDirectory, -1, File::None, 0,
                     heldCount());
    static_cast<void>(
        ensureRoot(InternalPass::Second, Subject::GenerationsDirectory, -1));
  }

  // Global pass B reuses the retained pass-A directories but opens one fresh
  // transient file at a time. Pass-A payloads therefore remain owned while
  // peak raw pass-B storage is bounded by one maximum file.
  if (!failure.set() && !cleanupFailure.set()) {
    for (qsizetype treeIndex = 0; treeIndex < treeCount; ++treeIndex) {
      auto &tree = trees[static_cast<size_t>(treeIndex)];
      const auto originalIndex = tree.reference.originalIndex;
      if (!ensureRoot(InternalPass::Second, Subject::GenerationRoot,
                      originalIndex) ||
          !ensureTree(tree, InternalPass::Second)) {
        break;
      }
      if (!secondIdentities.add(tree.rootBaseline)) {
        chooseFailure(failure, Reason::IdentityAlias, Subject::GenerationRoot,
                      originalIndex);
        break;
      }
      if (!secondIdentities.add(tree.modulesBaseline)) {
        chooseFailure(failure, Reason::IdentityAlias, Subject::ModulesDirectory,
                      originalIndex);
        break;
      }
      invokeCheckpoint(InternalCheckpoint::AfterGenerationDirectories,
                       InternalPass::Second, Subject::ModulesDirectory,
                       originalIndex, File::None, 0, heldCount());

      if (!exactInventory(tree.rootFd, tree.rootBaseline, rootInventory,
                          operations, enumeration, cleanup, cleanupFailure,
                          InternalPass::Second, Subject::GenerationRoot,
                          originalIndex)) {
        chooseLocalFailure(failure, operations, reads, enumeration,
                           Reason::InventoryMismatch, Subject::GenerationRoot,
                           originalIndex);
        break;
      }
      invokeCheckpoint(InternalCheckpoint::AfterInventory, InternalPass::Second,
                       Subject::GenerationRoot, originalIndex, File::None, 0,
                       heldCount());
      if (cleanupFailure.set() ||
          !ensureRoot(InternalPass::Second, Subject::GenerationRoot,
                      originalIndex) ||
          !ensureTree(tree, InternalPass::Second)) {
        break;
      }
      if (!exactInventory(tree.modulesFd, tree.modulesBaseline,
                          modulesInventory, operations, enumeration, cleanup,
                          cleanupFailure, InternalPass::Second,
                          Subject::ModulesDirectory, originalIndex)) {
        chooseLocalFailure(failure, operations, reads, enumeration,
                           Reason::InventoryMismatch, Subject::ModulesDirectory,
                           originalIndex);
        break;
      }
      invokeCheckpoint(InternalCheckpoint::AfterInventory, InternalPass::Second,
                       Subject::ModulesDirectory, originalIndex, File::None, 0,
                       heldCount());
      if (cleanupFailure.set() ||
          !ensureRoot(InternalPass::Second, Subject::ModulesDirectory,
                      originalIndex) ||
          !ensureTree(tree, InternalPass::Second)) {
        break;
      }

      for (size_t fileIndex = 0; fileIndex < fileSpecs.size(); ++fileIndex) {
        const auto &spec = fileSpecs[fileIndex];
        if (!ensureRoot(InternalPass::Second, spec.subject, originalIndex,
                        spec.file) ||
            !ensureTree(tree, InternalPass::Second)) {
          break;
        }
        InternalFile second;
        const auto parentFd = spec.modules ? tree.modulesFd : tree.rootFd;
        if (!captureFile(parentFd, spec, owner, rootBaseline.st_dev,
                         InternalPass::Second, originalIndex, operations, reads,
                         enumeration, cleanup, second, failure,
                         cleanupFailure)) {
          break;
        }
        if (!secondIdentities.add(second.metadata)) {
          chooseFailure(failure, Reason::IdentityAlias, spec.subject,
                        originalIndex, spec.file);
          break;
        }
        if (!filesEqual(tree.files[fileIndex], second)) {
          chooseFailure(failure, Reason::TreeChanged, spec.subject,
                        originalIndex, spec.file);
          break;
        }
        invokeCheckpoint(InternalCheckpoint::AfterFile, InternalPass::Second,
                         spec.subject, originalIndex, spec.file, 0,
                         heldCount());
        if (cleanupFailure.set() ||
            !ensureRoot(InternalPass::Second, spec.subject, originalIndex,
                        spec.file) ||
            !ensureTree(tree, InternalPass::Second)) {
          break;
        }
      }
      if (failure.set() || cleanupFailure.set())
        break;
    }
  }

  // Keep every pass-A file descriptor live until all pass-B trees have been
  // observed, then prove each retained node still has its original name and
  // full snapshot.
  if (!failure.set() && !cleanupFailure.set()) {
    for (qsizetype treeIndex = 0; treeIndex < treeCount; ++treeIndex) {
      auto &tree = trees[static_cast<size_t>(treeIndex)];
      const auto originalIndex = tree.reference.originalIndex;
      for (size_t fileIndex = 0; fileIndex < fileSpecs.size(); ++fileIndex) {
        const auto &spec = fileSpecs[fileIndex];
        auto &file = tree.files[fileIndex];
        invokeCheckpoint(InternalCheckpoint::BeforeHeldFirstPassFinalization,
                         InternalPass::Second, spec.subject, originalIndex,
                         spec.file, 0, heldCount());
        if (!ensureRoot(InternalPass::Second, spec.subject, originalIndex,
                        spec.file) ||
            !ensureTree(tree, InternalPass::Second)) {
          break;
        }
        struct stat held{};
        struct stat named{};
        const auto parentFd = spec.modules ? tree.modulesFd : tree.rootFd;
        const auto stable =
            guardedFstat(file.heldFirstPassFd, held, operations,
                         InternalPass::Second, spec.subject, originalIndex,
                         spec.file, InternalSyscall::HeldFirstPassFstat) &&
            guardedFstatat(parentFd, spec.basename, named, operations,
                           InternalPass::Second, spec.subject, originalIndex,
                           spec.file,
                           InternalSyscall::HeldFirstPassNamedFstatat) &&
            sameSnapshot(held, file.metadata) &&
            sameSnapshot(named, file.metadata) &&
            safeImmutableFile(held, owner, rootBaseline.st_dev,
                              spec.maximumBytes);
        if (!stable) {
          chooseLocalFailure(failure, operations, reads, enumeration,
                             Reason::TreeChanged, spec.subject, originalIndex,
                             spec.file);
          break;
        }
      }
      if (failure.set())
        break;
      if (!ensureTree(tree, InternalPass::Second))
        break;
    }
  }

  // A fresh, separately reserved final root proof overrides every tentative
  // body result whenever a safe root baseline was established.
  if (rootBaselineValid) {
    invokeCheckpoint(InternalCheckpoint::BeforeFinalRootGuard,
                     InternalPass::Second, Subject::GenerationsDirectory, -1,
                     File::None, 0, heldCount());
    if (!rootGuard(rootFd, rootBaseline, owner, finalOperations,
                   InternalPass::Second, Subject::GenerationsDirectory, -1,
                   File::None, heldCount())) {
      failure = {
          .reason = finalOperations.exceeded
                        ? Reason::ProofBudgetExceeded
                        : Reason::GenerationsDirectoryChanged,
          .subject = Subject::GenerationsDirectory,
      };
    }
  }

  // All descriptor ownership is invalidated before exactly one cleanup call.
  // Continue cleanup after errors; the first cleanup ambiguity overrides all
  // capture/body/final-guard results.
  for (qsizetype treeIndex = 0; treeIndex < treeCount; ++treeIndex) {
    auto &tree = trees[static_cast<size_t>(treeIndex)];
    const auto originalIndex = tree.reference.originalIndex;
    for (size_t fileIndex = 0; fileIndex < fileSpecs.size(); ++fileIndex) {
      auto &file = tree.files[fileIndex];
      const auto &spec = fileSpecs[fileIndex];
      if (!closeOnce(file.heldFirstPassFd, cleanup, InternalSyscall::CloseFile,
                     InternalPass::Second, spec.subject, originalIndex,
                     spec.file)) {
        chooseFailure(cleanupFailure, Reason::CleanupFailed, spec.subject,
                      originalIndex, spec.file);
      }
    }
    if (!closeOnce(tree.modulesFd, cleanup, InternalSyscall::CloseDirectory,
                   InternalPass::Second, Subject::ModulesDirectory,
                   originalIndex, File::None)) {
      chooseFailure(cleanupFailure, Reason::CleanupFailed,
                    Subject::ModulesDirectory, originalIndex);
    }
    if (!closeOnce(tree.rootFd, cleanup, InternalSyscall::CloseDirectory,
                   InternalPass::Second, Subject::GenerationRoot, originalIndex,
                   File::None)) {
      chooseFailure(cleanupFailure, Reason::CleanupFailed,
                    Subject::GenerationRoot, originalIndex);
    }
  }
  if (!closeOnce(rootFd, cleanup, InternalSyscall::CloseRoot,
                 InternalPass::Second, Subject::GenerationsDirectory, -1,
                 File::None)) {
    chooseFailure(cleanupFailure, Reason::CleanupFailed,
                  Subject::GenerationsDirectory);
  }
  if (cleanup.exceeded) {
    chooseFailure(cleanupFailure, Reason::CleanupFailed,
                  Subject::GenerationsDirectory);
  }
  if (cleanupFailure.set())
    failure = cleanupFailure;

  if (failure.set()) {
    // Disengagement destroys partial byte containers. Allocator storage is not
    // promised to be overwritten.
    trees = {};
    return failedResult(failure);
  }

  try {
    QVector<DormantV2GenerationTreeCapture> capturedTrees;
    capturedTrees.reserve(treeCount);
    for (qsizetype treeIndex = 0; treeIndex < treeCount; ++treeIndex) {
      auto &tree = trees[static_cast<size_t>(treeIndex)];
      QMap<QString, QByteArray> files;
      for (size_t fileIndex = 1; fileIndex < fileSpecs.size(); ++fileIndex) {
        files.insert(QString::fromLatin1(fileSpecs[fileIndex].relativePath),
                     std::move(tree.files[fileIndex].bytes));
      }
      capturedTrees.append(DormantV2GenerationTreeCapture(
          std::move(tree.reference.activationNonce),
          std::move(tree.files[0].bytes), std::move(files)));
    }
    auto capture = DormantV2GenerationTreesCapture(std::move(capturedTrees));
    return DormantV2GenerationTreeCaptureResult(
        DormantV2GenerationTreeCaptureDisposition::Captured, Reason::None,
        Subject::None, -1, File::None, std::move(capture));
  } catch (const std::bad_alloc &) {
    trees = {};
    return failedResult({.reason = Reason::AllocationFailed});
  }
}

#if defined(HYPRSHELLD_DORMANT_V2_GENERATION_TREE_CAPTURE_TEST_HOOKS)
namespace DormantV2GenerationTreeCaptureTestSupport {

void setCheckpointHook(CheckpointHook hook) {
  testCheckpointHook = std::move(hook);
  testCheckpointInvocations = 0;
}

void setSyscallHook(SyscallHook hook) {
  testSyscallHook = std::move(hook);
  testSyscallInvocations = 0;
}

void setPayloadAllocationFailureHook(PayloadAllocationFailureHook hook) {
  testPayloadAllocationFailureHook = std::move(hook);
}

void clearHooks() {
  testCheckpointHook = {};
  testSyscallHook = {};
  testPayloadAllocationFailureHook = {};
  testCheckpointInvocations = 0;
  testSyscallInvocations = 0;
}

} // namespace DormantV2GenerationTreeCaptureTestSupport
#endif

} // namespace HyprShelld::Compositor

namespace HyprShelld::Compositor {
namespace {

[[nodiscard]] bool descriptorHasCloseOnExec(
    const int descriptor, OperationBudget &operations, const InternalPass pass,
    const DormantV2GenerationTreeCaptureSubject subject,
    const qsizetype generationIndex, const DormantV2GenerationTreeFile file,
    const InternalSyscall syscall = InternalSyscall::GetOpenedDescriptorFlags) {
  InternalFault fault{};
  if (!beginProof(operations, syscall, pass, subject, generationIndex, file,
                  fault)) {
    return false;
  }
  const auto flags = ::fcntl(descriptor, F_GETFD);
  return flags >= 0 && (flags & FD_CLOEXEC) != 0;
}

[[nodiscard]] bool
guardedFstat(const int descriptor, struct stat &value,
             OperationBudget &operations, const InternalPass pass,
             const DormantV2GenerationTreeCaptureSubject subject,
             const qsizetype generationIndex,
             const DormantV2GenerationTreeFile file,
             const InternalSyscall syscall) {
  InternalFault fault{};
  if (!beginProof(operations, syscall, pass, subject, generationIndex, file,
                  fault)) {
    return false;
  }
  return ::fstat(descriptor, &value) == 0;
}

[[nodiscard]] bool guardedFstatat(
    const int parent, const char *name, struct stat &value,
    OperationBudget &operations, const InternalPass pass,
    const DormantV2GenerationTreeCaptureSubject subject,
    const qsizetype generationIndex, const DormantV2GenerationTreeFile file,
    const InternalSyscall syscall = InternalSyscall::InitialFstatat) {
  InternalFault fault{};
  if (!beginProof(operations, syscall, pass, subject, generationIndex, file,
                  fault)) {
    return false;
  }
  return ::fstatat(parent, name, &value, AT_SYMLINK_NOFOLLOW) == 0;
}

[[nodiscard]] bool
rootGuard(const int rootFd, const struct stat &baseline, const uid_t owner,
          OperationBudget &operations, const InternalPass pass,
          const DormantV2GenerationTreeCaptureSubject subject,
          const qsizetype generationIndex,
          const DormantV2GenerationTreeFile file,
          const qsizetype heldDescriptors) {
  invokeCheckpoint(InternalCheckpoint::BeforeRootGuard, pass, subject,
                   generationIndex, file, 0, heldDescriptors);
  struct stat current{};
  const auto stable =
      guardedFstat(rootFd, current, operations, pass,
                   DormantV2GenerationTreeCaptureSubject::GenerationsDirectory,
                   generationIndex, file, InternalSyscall::RootFstat) &&
      sameSnapshot(current, baseline) && safeRoot(current, owner);
  invokeCheckpoint(InternalCheckpoint::AfterRootGuard, pass, subject,
                   generationIndex, file, 0, heldDescriptors);
  return stable;
}

[[nodiscard]] bool retainedDirectoryStillExact(
    const int parentFd, const char *name, const int descriptor,
    const struct stat &baseline, const uid_t owner, const dev_t rootDevice,
    OperationBudget &operations, const InternalPass pass,
    const DormantV2GenerationTreeCaptureSubject subject,
    const qsizetype generationIndex) {
  struct stat opened{};
  struct stat named{};
  return guardedFstat(descriptor, opened, operations, pass, subject,
                      generationIndex, DormantV2GenerationTreeFile::None,
                      InternalSyscall::HeldFirstPassFstat) &&
         guardedFstatat(parentFd, name, named, operations, pass, subject,
                        generationIndex, DormantV2GenerationTreeFile::None,
                        InternalSyscall::HeldFirstPassNamedFstatat) &&
         sameSnapshot(opened, baseline) && sameSnapshot(named, baseline) &&
         safeImmutableDirectory(opened, owner, rootDevice);
}

[[nodiscard]] bool
openRequiredDirectory(const int parentFd, const char *name, const uid_t owner,
                      const dev_t rootDevice, OperationBudget &operations,
                      const InternalPass pass,
                      const DormantV2GenerationTreeCaptureSubject subject,
                      const qsizetype generationIndex, int &descriptor,
                      struct stat &baseline, bool &missing) {
  missing = false;
  invokeCheckpoint(InternalCheckpoint::BeforeLookup, pass, subject,
                   generationIndex);
  struct stat named{};
  errno = 0;
  if (!guardedFstatat(parentFd, name, named, operations, pass, subject,
                      generationIndex, DormantV2GenerationTreeFile::None)) {
    missing = !operations.exceeded && errno == ENOENT;
    return false;
  }
  invokeCheckpoint(InternalCheckpoint::AfterLookup, pass, subject,
                   generationIndex);
  if (!safeImmutableDirectory(named, owner, rootDevice))
    return false;

  invokeCheckpoint(InternalCheckpoint::BeforeOpen, pass, subject,
                   generationIndex);
  InternalFault fault{};
  if (!beginProof(operations, InternalSyscall::Openat, pass, subject,
                  generationIndex, DormantV2GenerationTreeFile::None, fault)) {
    return false;
  }
  descriptor = ::openat(parentFd, name,
                        O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW |
                            O_NONBLOCK | O_NOCTTY | O_NOATIME);
  if (descriptor < 0)
    return false;
  invokeCheckpoint(InternalCheckpoint::AfterOpen, pass, subject,
                   generationIndex);

  struct stat opened{};
  struct stat finalNamed{};
  const auto exact =
      descriptorHasCloseOnExec(descriptor, operations, pass, subject,
                               generationIndex,
                               DormantV2GenerationTreeFile::None) &&
      guardedFstat(descriptor, opened, operations, pass, subject,
                   generationIndex, DormantV2GenerationTreeFile::None,
                   InternalSyscall::OpenedFstat) &&
      sameSnapshot(named, opened) &&
      safeImmutableDirectory(opened, owner, rootDevice) &&
      guardedFstatat(parentFd, name, finalNamed, operations, pass, subject,
                     generationIndex, DormantV2GenerationTreeFile::None,
                     InternalSyscall::FinalNamedFstatat) &&
      sameSnapshot(opened, finalNamed);
  invokeCheckpoint(InternalCheckpoint::AfterOpenedFstat, pass, subject,
                   generationIndex);
  if (!exact)
    return false;
  baseline = opened;
  return true;
}

template <size_t Count>
[[nodiscard]] bool
exactInventory(const int directoryFd, const struct stat &directoryBaseline,
               const std::array<const char *, Count> &expected,
               OperationBudget &operations, EnumerationBudget &enumeration,
               CleanupBudget &cleanup, Failure &cleanupFailure,
               const InternalPass pass,
               const DormantV2GenerationTreeCaptureSubject subject,
               const qsizetype generationIndex) {
  InternalFault fault{};
  if (!beginProof(operations, InternalSyscall::ReopenDirectory, pass, subject,
                  generationIndex, DormantV2GenerationTreeFile::None, fault)) {
    return false;
  }
  auto reopened = ::openat(directoryFd, ".",
                           O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW |
                               O_NONBLOCK | O_NOCTTY | O_NOATIME);
  if (reopened < 0)
    return false;
  const auto closeReopened = [&] {
    if (!closeOnce(reopened, cleanup, InternalSyscall::CloseDirectory, pass,
                   subject, generationIndex,
                   DormantV2GenerationTreeFile::None)) {
      chooseFailure(cleanupFailure,
                    DormantV2GenerationTreeCaptureReason::CleanupFailed,
                    subject, generationIndex);
      return false;
    }
    return true;
  };

  struct stat reopenedMetadata{};
  if (!descriptorHasCloseOnExec(reopened, operations, pass, subject,
                                generationIndex,
                                DormantV2GenerationTreeFile::None) ||
      !guardedFstat(reopened, reopenedMetadata, operations, pass, subject,
                    generationIndex, DormantV2GenerationTreeFile::None,
                    InternalSyscall::OpenedFstat) ||
      !sameSnapshot(reopenedMetadata, directoryBaseline)) {
    static_cast<void>(closeReopened());
    return false;
  }

  if (!beginProof(operations, InternalSyscall::Fdopendir, pass, subject,
                  generationIndex, DormantV2GenerationTreeFile::None, fault)) {
    static_cast<void>(closeReopened());
    return false;
  }
  auto *stream = ::fdopendir(reopened);
  if (!stream) {
    static_cast<void>(closeReopened());
    return false;
  }
  reopened = -1; // fdopendir owns it from here.

  std::array<bool, Count> seen{};
  auto inventoryExact = true;
  while (true) {
    if (enumeration.callsRemaining == 0) {
      enumeration.exceeded = true;
      inventoryExact = false;
      break;
    }
    --enumeration.callsRemaining;
    if (!beginProof(operations, InternalSyscall::Readdir, pass, subject,
                    generationIndex, DormantV2GenerationTreeFile::None,
                    fault)) {
      if (fault == InternalFault::FailEintr)
        continue;
      inventoryExact = false;
      break;
    }
    invokeCheckpoint(InternalCheckpoint::BeforeReaddir, pass, subject,
                     generationIndex);
    errno = 0;
    auto *entry = ::readdir(stream);
    const auto readError = errno;
    invokeCheckpoint(InternalCheckpoint::AfterReaddir, pass, subject,
                     generationIndex);
    if (!entry) {
      if (readError == EINTR)
        continue;
      if (readError != 0)
        inventoryExact = false;
      break;
    }
    const auto nameLength =
        ::strnlen(entry->d_name, maximumDirentBasenameBytes + 1);
    if (nameLength > maximumDirentBasenameBytes) {
      inventoryExact = false;
      break;
    }
    if ((nameLength == 1 && entry->d_name[0] == '.') ||
        (nameLength == 2 && entry->d_name[0] == '.' &&
         entry->d_name[1] == '.')) {
      continue;
    }
    if (enumeration.direntsRemaining == 0 ||
        nameLength > enumeration.nameBytesRemaining) {
      enumeration.exceeded = true;
      inventoryExact = false;
      break;
    }
    --enumeration.direntsRemaining;
    enumeration.nameBytesRemaining -= nameLength;

    std::optional<size_t> match;
    for (size_t index = 0; index < expected.size(); ++index) {
      if (std::strlen(expected[index]) == nameLength &&
          std::memcmp(expected[index], entry->d_name, nameLength) == 0) {
        match = index;
        break;
      }
    }
    if (!match || seen[*match]) {
      inventoryExact = false;
      break;
    }
    seen[*match] = true;
  }

  auto *closingStream = stream;
  stream = nullptr; // Invalidate ownership before the one closedir attempt.
  cleanup.attempt();
  const auto closeResult = ::closedir(closingStream);
  const auto closeFault =
      injectedFault(InternalSyscall::Closedir, pass, subject, generationIndex,
                    DormantV2GenerationTreeFile::None);
  if (closeResult != 0 || closeFault == InternalFault::ReportCleanupFailure) {
    chooseFailure(cleanupFailure,
                  DormantV2GenerationTreeCaptureReason::CleanupFailed, subject,
                  generationIndex);
    inventoryExact = false;
  }
  if (!std::all_of(seen.cbegin(), seen.cend(),
                   [](const bool value) { return value; })) {
    inventoryExact = false;
  }
  return inventoryExact;
}

[[nodiscard]] ssize_t
boundedPread(const int descriptor, void *buffer, size_t count,
             const off_t offset, OperationBudget &operations, ReadBudget &reads,
             const InternalPass pass,
             const DormantV2GenerationTreeCaptureSubject subject,
             const qsizetype generationIndex,
             const DormantV2GenerationTreeFile file, InternalFault &fault) {
  if (!beginProof(operations, InternalSyscall::Pread, pass, subject,
                  generationIndex, file, fault)) {
    return -1;
  }
  if (fault == InternalFault::ShortReadOneByte)
    count = std::min<size_t>(count, 1);
  const auto returned = ::pread(descriptor, buffer, count, offset);
  if (returned > 0 && !reads.consume(static_cast<quint64>(returned))) {
    errno = EFBIG;
    return -1;
  }
  return returned;
}

[[nodiscard]] bool captureFile(const int parentFd, const FileSpec &spec,
                               const uid_t owner, const dev_t rootDevice,
                               const InternalPass pass,
                               const qsizetype generationIndex,
                               OperationBudget &operations, ReadBudget &reads,
                               EnumerationBudget &enumeration,
                               CleanupBudget &cleanup, InternalFile &result,
                               Failure &failure, Failure &cleanupFailure) {
  invokeCheckpoint(InternalCheckpoint::BeforeLookup, pass, spec.subject,
                   generationIndex, spec.file);
  struct stat named{};
  errno = 0;
  if (!guardedFstatat(parentFd, spec.basename, named, operations, pass,
                      spec.subject, generationIndex, spec.file)) {
    const auto lookupError = errno;
    if (!operations.exceeded && lookupError == ENOENT) {
      chooseFailure(failure, DormantV2GenerationTreeCaptureReason::FileMissing,
                    spec.subject, generationIndex, spec.file);
    } else {
      chooseLocalFailure(failure, operations, reads, enumeration,
                         DormantV2GenerationTreeCaptureReason::FileUnsafe,
                         spec.subject, generationIndex, spec.file);
    }
    return false;
  }
  invokeCheckpoint(InternalCheckpoint::AfterLookup, pass, spec.subject,
                   generationIndex, spec.file);
  if (!safeImmutableFile(named, owner, rootDevice, spec.maximumBytes)) {
    chooseFailure(failure, DormantV2GenerationTreeCaptureReason::FileUnsafe,
                  spec.subject, generationIndex, spec.file);
    return false;
  }

  invokeCheckpoint(InternalCheckpoint::BeforeOpen, pass, spec.subject,
                   generationIndex, spec.file);
  InternalFault fault{};
  if (!beginProof(operations, InternalSyscall::Openat, pass, spec.subject,
                  generationIndex, spec.file, fault)) {
    chooseLocalFailure(failure, operations, reads, enumeration,
                       DormantV2GenerationTreeCaptureReason::FileUnsafe,
                       spec.subject, generationIndex, spec.file);
    return false;
  }
  auto descriptor = ::openat(parentFd, spec.basename,
                             O_RDONLY | O_CLOEXEC | O_NOFOLLOW | O_NONBLOCK |
                                 O_NOCTTY | O_NOATIME);
  if (descriptor < 0) {
    chooseLocalFailure(failure, operations, reads, enumeration,
                       DormantV2GenerationTreeCaptureReason::FileUnsafe,
                       spec.subject, generationIndex, spec.file);
    return false;
  }
  invokeCheckpoint(InternalCheckpoint::AfterOpen, pass, spec.subject,
                   generationIndex, spec.file);
  const auto closeTransient = [&] {
    if (!closeOnce(descriptor, cleanup, InternalSyscall::CloseFile, pass,
                   spec.subject, generationIndex, spec.file)) {
      chooseFailure(cleanupFailure,
                    DormantV2GenerationTreeCaptureReason::CleanupFailed,
                    spec.subject, generationIndex, spec.file);
      return false;
    }
    return true;
  };

  struct stat opened{};
  if (!descriptorHasCloseOnExec(descriptor, operations, pass, spec.subject,
                                generationIndex, spec.file) ||
      !guardedFstat(descriptor, opened, operations, pass, spec.subject,
                    generationIndex, spec.file, InternalSyscall::OpenedFstat) ||
      !sameSnapshot(named, opened) ||
      !safeImmutableFile(opened, owner, rootDevice, spec.maximumBytes)) {
    static_cast<void>(closeTransient());
    chooseLocalFailure(failure, operations, reads, enumeration,
                       DormantV2GenerationTreeCaptureReason::FileUnsafe,
                       spec.subject, generationIndex, spec.file);
    return false;
  }
  invokeCheckpoint(InternalCheckpoint::AfterOpenedFstat, pass, spec.subject,
                   generationIndex, spec.file);

  QByteArray bytes;
  try {
    if (shouldFailPayloadAllocation(pass, generationIndex, spec.file))
      throw std::bad_alloc();
    bytes =
        QByteArray(static_cast<qsizetype>(opened.st_size), Qt::Uninitialized);
  } catch (const std::bad_alloc &) {
    static_cast<void>(closeTransient());
    chooseFailure(failure,
                  DormantV2GenerationTreeCaptureReason::AllocationFailed,
                  spec.subject, generationIndex, spec.file);
    return false;
  }

  quint64 offset = 0;
  quint64 calls = 0;
  auto readOkay = true;
  while (offset < static_cast<quint64>(opened.st_size)) {
    if (++calls > maximumPreadAttempts(spec.maximumBytes)) {
      reads.perFileAttemptsExceeded = true;
      readOkay = false;
      break;
    }
    const auto requested = static_cast<size_t>(
        std::min<quint64>(dormantV2GenerationCaptureReadBufferBytes,
                          static_cast<quint64>(opened.st_size) - offset));
    InternalFault readFault{};
    const auto returned =
        boundedPread(descriptor, bytes.data() + static_cast<qsizetype>(offset),
                     requested, static_cast<off_t>(offset), operations, reads,
                     pass, spec.subject, generationIndex, spec.file, readFault);
    invokeCheckpoint(InternalCheckpoint::AfterPread, pass, spec.subject,
                     generationIndex, spec.file, calls);
    if (returned < 0) {
      if (errno == EINTR)
        continue;
      readOkay = false;
      break;
    }
    if (returned == 0 || static_cast<size_t>(returned) > requested) {
      readOkay = false;
      break;
    }
    offset += static_cast<quint64>(returned);
  }
  char growthProbe = 0;
  if (readOkay) {
    while (true) {
      if (++calls > maximumPreadAttempts(spec.maximumBytes)) {
        reads.perFileAttemptsExceeded = true;
        readOkay = false;
        break;
      }
      InternalFault readFault{};
      const auto returned = boundedPread(descriptor, &growthProbe, 1,
                                         static_cast<off_t>(opened.st_size),
                                         operations, reads, pass, spec.subject,
                                         generationIndex, spec.file, readFault);
      invokeCheckpoint(InternalCheckpoint::AfterPread, pass, spec.subject,
                       generationIndex, spec.file, calls);
      if (returned < 0 && errno == EINTR)
        continue;
      readOkay = returned == 0;
      break;
    }
  }

  struct stat after{};
  struct stat finalNamed{};
  invokeCheckpoint(InternalCheckpoint::BeforeFinalNamedCheck, pass,
                   spec.subject, generationIndex, spec.file);
  const auto stable =
      readOkay &&
      guardedFstat(descriptor, after, operations, pass, spec.subject,
                   generationIndex, spec.file,
                   InternalSyscall::AfterReadFstat) &&
      sameSnapshot(opened, after) &&
      guardedFstatat(parentFd, spec.basename, finalNamed, operations, pass,
                     spec.subject, generationIndex, spec.file,
                     InternalSyscall::FinalNamedFstatat) &&
      sameSnapshot(after, finalNamed) &&
      safeImmutableFile(after, owner, rootDevice, spec.maximumBytes);
  if (pass == InternalPass::First && stable) {
    result.heldFirstPassFd = descriptor;
    descriptor = -1;
  }
  const auto closed = closeTransient();
  if (!stable || !closed) {
    chooseLocalFailure(failure, operations, reads, enumeration,
                       DormantV2GenerationTreeCaptureReason::FileUnsafe,
                       spec.subject, generationIndex, spec.file);
    return false;
  }
  result.bytes = std::move(bytes);
  result.metadata = after;
  return true;
}

[[nodiscard]] bool filesEqual(const InternalFile &left,
                              const InternalFile &right) {
  return left.bytes == right.bytes &&
         sameSnapshot(left.metadata, right.metadata);
}

} // namespace

} // namespace HyprShelld::Compositor
