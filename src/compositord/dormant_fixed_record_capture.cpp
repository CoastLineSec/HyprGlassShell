#include "dormant_fixed_record_capture.h"

#include "authority_records.h"
#include "legacy_transaction_records.h"
#include "ordinary_pending_record.h"

#include "hyprland/desired_state.h"

#include <algorithm>
#include <array>
#include <cerrno>
#include <new>
#include <utility>

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

namespace HyprShelld::Compositor {
namespace {

constexpr mode_t privateDirectoryMode = 0700;
constexpr mode_t privateFileMode = 0600;
constexpr quint64 maximumExtraPreadAttempts = 129;
constexpr quint64 maximumTestHookInvocations = 32768;

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
  AfterRecord,
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
  ConfirmMissingFstatat,
  Openat,
  OpenedFstat,
  Pread,
  AfterReadFstat,
  FinalNamedFstatat,
  HeldFirstPassFstat,
  HeldFirstPassNamedFstatat,
  CloseFile,
  CloseRoot,
};

enum class InternalFault {
  None,
  FailEintr,
  FailIo,
  ShortReadOneByte,
  ExhaustProofBudget,
  ReportCloseFailure,
};

struct RecordSpec final {
  const char *basename = nullptr;
  qsizetype maximumBytes = 0;
  DormantFixedRecordCaptureSubject subject =
      DormantFixedRecordCaptureSubject::None;
};

constexpr std::array<RecordSpec, 5> recordSpecs{{
    {"authority.json", maximumDormantAuthorityCaptureBytes,
     DormantFixedRecordCaptureSubject::Authority},
    {"desired.json", maximumDormantDesiredCaptureBytes,
     DormantFixedRecordCaptureSubject::Desired},
    {"last-good.json", maximumDormantLastGoodCaptureBytes,
     DormantFixedRecordCaptureSubject::LastGood},
    {"activation.json", maximumDormantAppliedCaptureBytes,
     DormantFixedRecordCaptureSubject::Applied},
    {"pending.json", maximumDormantPendingCaptureBytes,
     DormantFixedRecordCaptureSubject::Pending},
}};

constexpr quint64 ceilingChunks(const qsizetype maximumBytes) {
  return (static_cast<quint64>(maximumBytes) +
          dormantFixedRecordReadBufferBytes - 1) /
         dormantFixedRecordReadBufferBytes;
}

constexpr quint64 maximumPreadAttempts(const qsizetype maximumBytes) {
  return ceilingChunks(maximumBytes) + maximumExtraPreadAttempts;
}

constexpr quint64 maximumAllPreadAttempts =
    2 * (maximumPreadAttempts(maximumDormantAuthorityCaptureBytes) +
         maximumPreadAttempts(maximumDormantDesiredCaptureBytes) +
         maximumPreadAttempts(maximumDormantLastGoodCaptureBytes) +
         maximumPreadAttempts(maximumDormantAppliedCaptureBytes) +
         maximumPreadAttempts(maximumDormantPendingCaptureBytes));
constexpr quint64 maximumRootProofAttempts =
    1 +  // F_DUPFD_CLOEXEC
    2 +  // F_GETFD and F_GETFL
    1 +  // geteuid
    1 +  // initial full root snapshot
    20 + // before and after each of ten observations
    1 +  // between passes
    1;   // final root guard
constexpr quint64 maximumRecordNonReadProofAttempts =
    10 * 6; // fstatat/open/fcntl/fstat/fstat/fstatat
constexpr quint64 maximumHeldFirstPassFinalProofAttempts = 5 * 2;
constexpr quint64 computedMaximumProofAttempts =
    maximumAllPreadAttempts + maximumRootProofAttempts +
    maximumRecordNonReadProofAttempts + maximumHeldFirstPassFinalProofAttempts;

static_assert(maximumPreadAttempts(maximumDormantAuthorityCaptureBytes) == 130);
static_assert(maximumPreadAttempts(maximumDormantDesiredCaptureBytes) == 1153);
static_assert(maximumPreadAttempts(maximumDormantLastGoodCaptureBytes) == 1153);
static_assert(maximumPreadAttempts(maximumDormantAppliedCaptureBytes) == 1153);
static_assert(maximumPreadAttempts(maximumDormantPendingCaptureBytes) == 2181);
static_assert(maximumAllPreadAttempts == 11540);
static_assert(maximumRootProofAttempts == 27);
static_assert(maximumRecordNonReadProofAttempts == 60);
static_assert(maximumHeldFirstPassFinalProofAttempts == 10);
static_assert(computedMaximumProofAttempts == 11637);
static_assert(computedMaximumProofAttempts <=
              maximumDormantFixedRecordProofSyscallAttempts);
static_assert(maximumDormantAuthorityCaptureBytes ==
              maximumAuthorityRecordV2Bytes);
static_assert(maximumDormantDesiredCaptureBytes ==
              Hyprland::maximumDesiredStateBytes);
static_assert(maximumDormantLastGoodCaptureBytes ==
              Hyprland::maximumDesiredStateBytes);
static_assert(maximumDormantAppliedCaptureBytes ==
              std::max(maximumLegacyAppliedRecordV1Bytes,
                       maximumAppliedRecordV2Bytes));
static_assert(maximumDormantPendingCaptureBytes ==
              std::max(maximumLegacyOrdinaryPendingRecordV1Bytes,
                       maximumOrdinaryPendingRecordV2Bytes));
// A successful call performs five pass-B closes, five held pass-A closes, and
// one root close. The cleanup allowance is cumulative, not just simultaneous.
static_assert(dormantFixedRecordCleanupAttemptReserve >= 11);

struct OperationBudget final {
  quint64 proofRemaining = maximumDormantFixedRecordProofSyscallAttempts;
  quint64 cleanupRemaining = dormantFixedRecordCleanupAttemptReserve;
  bool proofExceeded = false;
  bool cleanupExceeded = false;

  [[nodiscard]] bool proofAttempt() {
    if (proofRemaining == 0) {
      proofExceeded = true;
      errno = EIO;
      return false;
    }
    --proofRemaining;
    return true;
  }

  void exhaustProof() {
    proofRemaining = 0;
    proofExceeded = true;
  }

  void cleanupAttempt() {
    if (cleanupRemaining == 0) {
      cleanupExceeded = true;
      return;
    }
    --cleanupRemaining;
  }
};

struct ReadBudget final {
  quint64 remaining = maximumDormantFixedRecordPreadReturnedBytes;
  bool returnedBytesExceeded = false;
  bool perFileAttemptsExceeded = false;

  [[nodiscard]] bool consume(const quint64 count) {
    if (count > remaining) {
      returnedBytesExceeded = true;
      return false;
    }
    remaining -= count;
    return true;
  }

  [[nodiscard]] bool exceeded() const {
    return returnedBytesExceeded || perFileAttemptsExceeded;
  }
};

struct Failure final {
  DormantFixedRecordCaptureReason reason =
      DormantFixedRecordCaptureReason::None;
  DormantFixedRecordCaptureSubject subject =
      DormantFixedRecordCaptureSubject::None;

  [[nodiscard]] bool set() const {
    return reason != DormantFixedRecordCaptureReason::None;
  }
};

struct InternalRecord final {
  DormantFixedRecordFieldKind kind = DormantFixedRecordFieldKind::Missing;
  QByteArray bytes;
  struct stat metadata{};
  int heldFirstPassFd = -1;
};

#if defined(HYPRSHELLD_DORMANT_FIXED_RECORD_CAPTURE_TEST_HOOKS)
thread_local DormantFixedRecordCaptureTestSupport::CheckpointHook
    testCheckpointHook;
thread_local DormantFixedRecordCaptureTestSupport::SyscallHook testSyscallHook;
thread_local DormantFixedRecordCaptureTestSupport::PayloadAllocationFailureHook
    testPayloadAllocationFailureHook;
thread_local quint64 testCheckpointInvocations = 0;
thread_local quint64 testSyscallInvocations = 0;
#endif

#if defined(HYPRSHELLD_DORMANT_FIXED_RECORD_CAPTURE_TEST_HOOKS)
[[nodiscard]] constexpr DormantFixedRecordCaptureTestSupport::Pass
testPass(const InternalPass pass) {
  return pass == InternalPass::First
             ? DormantFixedRecordCaptureTestSupport::Pass::First
             : DormantFixedRecordCaptureTestSupport::Pass::Second;
}
#endif

void invokeCheckpoint(const InternalCheckpoint checkpoint,
                      const InternalPass pass,
                      const DormantFixedRecordCaptureSubject subject =
                          DormantFixedRecordCaptureSubject::None,
                      const qsizetype recordIndex = -1,
                      const quint64 preadInvocation = 0,
                      const qsizetype heldFirstPassDescriptors = 0) {
#if defined(HYPRSHELLD_DORMANT_FIXED_RECORD_CAPTURE_TEST_HOOKS)
  if (!testCheckpointHook ||
      ++testCheckpointInvocations > maximumTestHookInvocations) {
    return;
  }
  try {
    testCheckpointHook({
        .checkpoint =
            static_cast<DormantFixedRecordCaptureTestSupport::Checkpoint>(
                checkpoint),
        .pass = testPass(pass),
        .subject = subject,
        .recordIndex = recordIndex,
        .preadInvocation = preadInvocation,
        .heldFirstPassDescriptors = heldFirstPassDescriptors,
    });
  } catch (...) {
    // A deterministic test seam can never escape into the algorithm.
  }
#else
  Q_UNUSED(checkpoint)
  Q_UNUSED(pass)
  Q_UNUSED(subject)
  Q_UNUSED(recordIndex)
  Q_UNUSED(preadInvocation)
  Q_UNUSED(heldFirstPassDescriptors)
#endif
}

[[nodiscard]] bool
shouldFailPayloadAllocation(const InternalPass pass,
                            const DormantFixedRecordCaptureSubject subject) {
#if defined(HYPRSHELLD_DORMANT_FIXED_RECORD_CAPTURE_TEST_HOOKS)
  if (!testPayloadAllocationFailureHook)
    return false;
  try {
    return testPayloadAllocationFailureHook(testPass(pass), subject);
  } catch (...) {
    return true;
  }
#else
  Q_UNUSED(pass)
  Q_UNUSED(subject)
  return false;
#endif
}

[[nodiscard]] InternalFault
injectedFault(const InternalSyscall syscall, const InternalPass pass,
              const DormantFixedRecordCaptureSubject subject) {
#if defined(HYPRSHELLD_DORMANT_FIXED_RECORD_CAPTURE_TEST_HOOKS)
  if (!testSyscallHook ||
      ++testSyscallInvocations > maximumTestHookInvocations) {
    return InternalFault::None;
  }
  try {
    return static_cast<InternalFault>(testSyscallHook({
        .syscall =
            static_cast<DormantFixedRecordCaptureTestSupport::Syscall>(syscall),
        .pass = testPass(pass),
        .subject = subject,
        .invocation = testSyscallInvocations,
    }));
  } catch (...) {
    return InternalFault::FailIo;
  }
#else
  Q_UNUSED(syscall)
  Q_UNUSED(pass)
  Q_UNUSED(subject)
  return InternalFault::None;
#endif
}

[[nodiscard]] bool
beginProofSyscall(OperationBudget &budget, const InternalSyscall syscall,
                  const InternalPass pass,
                  const DormantFixedRecordCaptureSubject subject,
                  InternalFault &fault) {
  if (!budget.proofAttempt())
    return false;
  fault = injectedFault(syscall, pass, subject);
  if (fault == InternalFault::ExhaustProofBudget) {
    budget.exhaustProof();
    errno = EIO;
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

[[nodiscard]] bool sameIdentity(const struct stat &left,
                                const struct stat &right) {
  return left.st_dev == right.st_dev && left.st_ino == right.st_ino;
}

[[nodiscard]] bool safeRoot(const struct stat &value, const uid_t owner) {
  return S_ISDIR(value.st_mode) && saneSnapshotRanges(value) &&
         value.st_uid == owner &&
         (value.st_mode & 07777) == privateDirectoryMode;
}

[[nodiscard]] bool safeRecord(const struct stat &value, const uid_t owner,
                              const dev_t rootDevice,
                              const qsizetype maximumBytes) {
  return S_ISREG(value.st_mode) && saneSnapshotRanges(value) &&
         value.st_dev == rootDevice && value.st_uid == owner &&
         value.st_nlink == 1 && (value.st_mode & 07777) == privateFileMode &&
         value.st_size <= maximumBytes;
}

[[nodiscard]] bool closeOnce(int &descriptor, OperationBudget &budget,
                             const InternalSyscall syscall,
                             const InternalPass pass,
                             const DormantFixedRecordCaptureSubject subject) {
  if (descriptor < 0)
    return true;
  const auto closing = descriptor;
  descriptor = -1; // invalidate before the one and only close attempt
  budget.cleanupAttempt();
  const auto closeResult = ::close(closing);
  const auto fault = injectedFault(syscall, pass, subject);
  if (fault == InternalFault::ReportCloseFailure) {
    errno = EIO;
    return false;
  }
  return closeResult == 0;
}

void chooseFailure(Failure &failure,
                   const DormantFixedRecordCaptureReason reason,
                   const DormantFixedRecordCaptureSubject subject) {
  if (!failure.set())
    failure = {.reason = reason, .subject = subject};
}

void chooseBudgetFailure(Failure &failure, const OperationBudget &operations,
                         const ReadBudget &reads,
                         const DormantFixedRecordCaptureSubject subject) {
  if (operations.proofExceeded) {
    chooseFailure(failure, DormantFixedRecordCaptureReason::ProofBudgetExceeded,
                  subject);
  } else if (reads.exceeded()) {
    chooseFailure(failure, DormantFixedRecordCaptureReason::ReadBudgetExceeded,
                  subject);
  } else {
    chooseFailure(failure, DormantFixedRecordCaptureReason::UnsafeRecord,
                  subject);
  }
}

[[nodiscard]] bool
guardedRootFstat(const int rootFd, struct stat &value, OperationBudget &budget,
                 const InternalPass pass,
                 const InternalSyscall syscall = InternalSyscall::RootFstat) {
  InternalFault fault{};
  if (!beginProofSyscall(budget, syscall, pass,
                         DormantFixedRecordCaptureSubject::StateDirectory,
                         fault)) {
    return false;
  }
  return ::fstat(rootFd, &value) == 0;
}

[[nodiscard]] bool rootGuard(const int rootFd, const struct stat &baseline,
                             OperationBudget &budget, const InternalPass pass,
                             const DormantFixedRecordCaptureSubject subject,
                             const qsizetype recordIndex,
                             const qsizetype heldCount) {
  invokeCheckpoint(InternalCheckpoint::BeforeRootGuard, pass, subject,
                   recordIndex, 0, heldCount);
  struct stat current{};
  const auto stable = guardedRootFstat(rootFd, current, budget, pass) &&
                      sameSnapshot(current, baseline);
  invokeCheckpoint(InternalCheckpoint::AfterRootGuard, pass, subject,
                   recordIndex, 0, heldCount);
  return stable;
}

[[nodiscard]] ssize_t
boundedPread(const int descriptor, void *buffer, size_t count,
             const off_t offset, OperationBudget &operations, ReadBudget &reads,
             const InternalPass pass,
             const DormantFixedRecordCaptureSubject subject) {
  InternalFault fault{};
  if (!beginProofSyscall(operations, InternalSyscall::Pread, pass, subject,
                         fault)) {
    return -1;
  }
  if (fault == InternalFault::ShortReadOneByte) {
    count = std::min<size_t>(count, 1);
  }
  const auto returned = ::pread(descriptor, buffer, count, offset);
  if (returned > 0 && !reads.consume(static_cast<quint64>(returned))) {
    errno = EFBIG;
    return -1;
  }
  return returned;
}

[[nodiscard]] bool initialFstatat(const int rootFd, const RecordSpec &spec,
                                  struct stat &value, OperationBudget &budget,
                                  const InternalPass pass) {
  InternalFault fault{};
  if (!beginProofSyscall(budget, InternalSyscall::InitialFstatat, pass,
                         spec.subject, fault)) {
    return false;
  }
  return ::fstatat(rootFd, spec.basename, &value, AT_SYMLINK_NOFOLLOW) == 0;
}

[[nodiscard]] bool confirmMissing(const int rootFd, const RecordSpec &spec,
                                  OperationBudget &budget,
                                  const InternalPass pass) {
  struct stat ignored{};
  InternalFault fault{};
  if (!beginProofSyscall(budget, InternalSyscall::ConfirmMissingFstatat, pass,
                         spec.subject, fault)) {
    return false;
  }
  if (::fstatat(rootFd, spec.basename, &ignored, AT_SYMLINK_NOFOLLOW) == 0)
    return false;
  return errno == ENOENT;
}

[[nodiscard]] bool openRecord(const int rootFd, const RecordSpec &spec,
                              int &descriptor, OperationBudget &budget,
                              const InternalPass pass) {
  InternalFault fault{};
  if (!beginProofSyscall(budget, InternalSyscall::Openat, pass, spec.subject,
                         fault)) {
    return false;
  }
  descriptor = ::openat(rootFd, spec.basename,
                        O_RDONLY | O_CLOEXEC | O_NOFOLLOW | O_NONBLOCK |
                            O_NOCTTY | O_NOATIME);
  return descriptor >= 0;
}

[[nodiscard]] bool
descriptorHasCloseOnExec(const int descriptor, OperationBudget &budget,
                         const InternalPass pass,
                         const DormantFixedRecordCaptureSubject subject) {
  InternalFault fault{};
  if (!beginProofSyscall(budget, InternalSyscall::GetDescriptorFlags, pass,
                         subject, fault)) {
    return false;
  }
  const auto flags = ::fcntl(descriptor, F_GETFD);
  return flags >= 0 && (flags & FD_CLOEXEC) != 0;
}

[[nodiscard]] bool fstatRecord(const int descriptor, struct stat &value,
                               OperationBudget &budget, const InternalPass pass,
                               const DormantFixedRecordCaptureSubject subject,
                               const InternalSyscall syscall) {
  InternalFault fault{};
  if (!beginProofSyscall(budget, syscall, pass, subject, fault))
    return false;
  return ::fstat(descriptor, &value) == 0;
}

[[nodiscard]] bool finalNamedFstatat(
    const int rootFd, const RecordSpec &spec, struct stat &value,
    OperationBudget &budget, const InternalPass pass,
    const InternalSyscall syscall = InternalSyscall::FinalNamedFstatat) {
  InternalFault fault{};
  if (!beginProofSyscall(budget, syscall, pass, spec.subject, fault))
    return false;
  return ::fstatat(rootFd, spec.basename, &value, AT_SYMLINK_NOFOLLOW) == 0;
}

[[nodiscard]] bool captureOne(const int rootFd, const struct stat &rootBaseline,
                              const RecordSpec &spec,
                              const qsizetype recordIndex,
                              const InternalPass pass,
                              OperationBudget &operations, ReadBudget &reads,
                              InternalRecord &result, Failure &failure,
                              Failure &cleanupFailure) {
  invokeCheckpoint(InternalCheckpoint::BeforeLookup, pass, spec.subject,
                   recordIndex);
  struct stat named{};
  errno = 0;
  if (!initialFstatat(rootFd, spec, named, operations, pass)) {
    const auto lookupError = errno;
    if (operations.proofExceeded) {
      chooseBudgetFailure(failure, operations, reads, spec.subject);
      return false;
    }
    if (lookupError != ENOENT) {
      chooseFailure(failure, DormantFixedRecordCaptureReason::UnsafeRecord,
                    spec.subject);
      return false;
    }
    invokeCheckpoint(InternalCheckpoint::AfterLookup, pass, spec.subject,
                     recordIndex);
    if (!confirmMissing(rootFd, spec, operations, pass)) {
      chooseBudgetFailure(failure, operations, reads, spec.subject);
      return false;
    }
    result.kind = DormantFixedRecordFieldKind::Missing;
    result.bytes.clear();
    return true;
  }
  invokeCheckpoint(InternalCheckpoint::AfterLookup, pass, spec.subject,
                   recordIndex);
  if (!safeRecord(named, rootBaseline.st_uid, rootBaseline.st_dev,
                  spec.maximumBytes)) {
    chooseFailure(failure, DormantFixedRecordCaptureReason::UnsafeRecord,
                  spec.subject);
    return false;
  }

  invokeCheckpoint(InternalCheckpoint::BeforeOpen, pass, spec.subject,
                   recordIndex);
  int descriptor = -1;
  if (!openRecord(rootFd, spec, descriptor, operations, pass)) {
    chooseBudgetFailure(failure, operations, reads, spec.subject);
    return false;
  }
  invokeCheckpoint(InternalCheckpoint::AfterOpen, pass, spec.subject,
                   recordIndex);
  bool descriptorNeedsCleanup = true;
  const auto closeTransient = [&] {
    if (!descriptorNeedsCleanup)
      return true;
    descriptorNeedsCleanup = false;
    return closeOnce(descriptor, operations, InternalSyscall::CloseFile, pass,
                     spec.subject);
  };

  struct stat opened{};
  if (!descriptorHasCloseOnExec(descriptor, operations, pass, spec.subject) ||
      !fstatRecord(descriptor, opened, operations, pass, spec.subject,
                   InternalSyscall::OpenedFstat) ||
      !sameSnapshot(named, opened) ||
      !safeRecord(opened, rootBaseline.st_uid, rootBaseline.st_dev,
                  spec.maximumBytes)) {
    const auto closed = closeTransient();
    chooseBudgetFailure(failure, operations, reads, spec.subject);
    if (!closed)
      chooseFailure(cleanupFailure,
                    DormantFixedRecordCaptureReason::CleanupFailed,
                    spec.subject);
    if (!closed)
      failure = cleanupFailure;
    return false;
  }
  invokeCheckpoint(InternalCheckpoint::AfterOpenedFstat, pass, spec.subject,
                   recordIndex);

  QByteArray bytes;
  try {
    if (shouldFailPayloadAllocation(pass, spec.subject))
      throw std::bad_alloc();
    bytes =
        QByteArray(static_cast<qsizetype>(opened.st_size), Qt::Uninitialized);
  } catch (const std::bad_alloc &) {
    const auto closed = closeTransient();
    chooseFailure(failure, DormantFixedRecordCaptureReason::UnsafeRecord,
                  spec.subject);
    if (!closed) {
      chooseFailure(cleanupFailure,
                    DormantFixedRecordCaptureReason::CleanupFailed,
                    spec.subject);
      failure = cleanupFailure;
    }
    return false;
  }
  quint64 offset = 0;
  quint64 calls = 0;
  bool readOkay = true;
  while (offset < static_cast<quint64>(opened.st_size)) {
    if (++calls > maximumPreadAttempts(spec.maximumBytes)) {
      reads.perFileAttemptsExceeded = true;
      readOkay = false;
      break;
    }
    const auto requested = static_cast<size_t>(
        std::min<quint64>(dormantFixedRecordReadBufferBytes,
                          static_cast<quint64>(opened.st_size) - offset));
    const auto returned = boundedPread(
        descriptor, bytes.data() + static_cast<qsizetype>(offset), requested,
        static_cast<off_t>(offset), operations, reads, pass, spec.subject);
    invokeCheckpoint(InternalCheckpoint::AfterPread, pass, spec.subject,
                     recordIndex, calls);
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
      const auto returned = boundedPread(descriptor, &growthProbe, 1,
                                         static_cast<off_t>(opened.st_size),
                                         operations, reads, pass, spec.subject);
      invokeCheckpoint(InternalCheckpoint::AfterPread, pass, spec.subject,
                       recordIndex, calls);
      if (returned < 0 && errno == EINTR)
        continue;
      readOkay = returned == 0;
      break;
    }
  }

  struct stat after{};
  struct stat finalNamed{};
  invokeCheckpoint(InternalCheckpoint::BeforeFinalNamedCheck, pass,
                   spec.subject, recordIndex);
  const auto stable =
      readOkay &&
      fstatRecord(descriptor, after, operations, pass, spec.subject,
                  InternalSyscall::AfterReadFstat) &&
      sameSnapshot(opened, after) &&
      finalNamedFstatat(rootFd, spec, finalNamed, operations, pass) &&
      sameSnapshot(after, finalNamed);
  if (pass == InternalPass::First && stable) {
    result.heldFirstPassFd = descriptor;
    descriptor = -1;
    descriptorNeedsCleanup = false;
  }
  const auto closed = closeTransient();
  if (!stable || !closed) {
    if (!closed) {
      chooseFailure(cleanupFailure,
                    DormantFixedRecordCaptureReason::CleanupFailed,
                    spec.subject);
      failure = cleanupFailure;
    } else {
      chooseBudgetFailure(failure, operations, reads, spec.subject);
    }
    return false;
  }
  result.kind = DormantFixedRecordFieldKind::PresentBytes;
  result.bytes = std::move(bytes);
  result.metadata = after;
  return true;
}

[[nodiscard]] bool recordsEqual(const InternalRecord &first,
                                const InternalRecord &second) {
  if (first.kind != second.kind)
    return false;
  if (first.kind == DormantFixedRecordFieldKind::Missing) {
    return first.bytes.isEmpty() && second.bytes.isEmpty();
  }
  return first.bytes == second.bytes &&
         sameSnapshot(first.metadata, second.metadata);
}

[[nodiscard]] qsizetype
heldCount(const std::array<InternalRecord, 5> &records) {
  return static_cast<qsizetype>(
      std::count_if(records.cbegin(), records.cend(), [](const auto &record) {
        return record.heldFirstPassFd >= 0;
      }));
}

[[nodiscard]] bool
hasIdentityAlias(const std::array<InternalRecord, 5> &records,
                 const size_t through) {
  const auto &candidate = records[through];
  if (candidate.kind != DormantFixedRecordFieldKind::PresentBytes)
    return false;
  for (size_t index = 0; index < through; ++index) {
    const auto &prior = records[index];
    if (prior.kind == DormantFixedRecordFieldKind::PresentBytes &&
        sameIdentity(prior.metadata, candidate.metadata)) {
      return true;
    }
  }
  return false;
}

} // namespace

DormantFixedRecordCaptureResult
captureDormantFixedRecords(const int borrowedStateDirectoryFd) {
  using TestCheckpoint = InternalCheckpoint;
  using TestSyscall = InternalSyscall;
  using TestFault = InternalFault;

#if defined(HYPRSHELLD_DORMANT_FIXED_RECORD_CAPTURE_TEST_HOOKS)
  testCheckpointInvocations = 0;
  testSyscallInvocations = 0;
#endif

  OperationBudget operations;
  ReadBudget reads;
  Failure failure;
  Failure cleanupFailure;
  std::array<InternalRecord, 5> first{};
  int rootFd = -1;
  bool rootBaselineValid = false;
  struct stat rootBaseline{};

  TestFault fault{};
  if (beginProofSyscall(
          operations, TestSyscall::DuplicateRoot, InternalPass::First,
          DormantFixedRecordCaptureSubject::StateDirectory, fault)) {
    rootFd = ::fcntl(borrowedStateDirectoryFd, F_DUPFD_CLOEXEC, 0);
  }
  if (rootFd < 0) {
    chooseFailure(
        failure,
        operations.proofExceeded
            ? DormantFixedRecordCaptureReason::ProofBudgetExceeded
            : DormantFixedRecordCaptureReason::InvalidStateDirectoryDescriptor,
        DormantFixedRecordCaptureSubject::StateDirectory);
  } else {
    invokeCheckpoint(TestCheckpoint::AfterRootDuplicate, InternalPass::First,
                     DormantFixedRecordCaptureSubject::StateDirectory);
  }

  if (!failure.set()) {
    TestFault descriptorFault{};
    auto descriptorFlags = -1;
    if (beginProofSyscall(operations, TestSyscall::GetDescriptorFlags,
                          InternalPass::First,
                          DormantFixedRecordCaptureSubject::StateDirectory,
                          descriptorFault)) {
      descriptorFlags = ::fcntl(rootFd, F_GETFD);
    }
    const auto descriptorFlagsOkay =
        descriptorFlags >= 0 && (descriptorFlags & FD_CLOEXEC) != 0;
    TestFault statusFault{};
    auto statusFlags = -1;
    if (beginProofSyscall(
            operations, TestSyscall::GetStatusFlags, InternalPass::First,
            DormantFixedRecordCaptureSubject::StateDirectory, statusFault)) {
      statusFlags = ::fcntl(rootFd, F_GETFL);
    }
    uid_t expectedOwner = 0;
    auto ownerObserved = false;
    TestFault ownerFault{};
    if (beginProofSyscall(
            operations, TestSyscall::GetEffectiveUid, InternalPass::First,
            DormantFixedRecordCaptureSubject::StateDirectory, ownerFault)) {
      expectedOwner = ::geteuid();
      ownerObserved = true;
    }
    invokeCheckpoint(TestCheckpoint::BeforeRootGuard, InternalPass::First,
                     DormantFixedRecordCaptureSubject::StateDirectory);
    const auto snapshotOkay =
        guardedRootFstat(rootFd, rootBaseline, operations, InternalPass::First);
    invokeCheckpoint(TestCheckpoint::AfterRootGuard, InternalPass::First,
                     DormantFixedRecordCaptureSubject::StateDirectory);
    const auto readable = statusFlags >= 0 &&
                          (statusFlags & O_ACCMODE) == O_RDONLY &&
                          (statusFlags & O_PATH) == 0;
    if (!descriptorFlagsOkay || !readable || !snapshotOkay || !ownerObserved ||
        !safeRoot(rootBaseline, expectedOwner)) {
      chooseFailure(failure,
                    operations.proofExceeded
                        ? DormantFixedRecordCaptureReason::ProofBudgetExceeded
                        : DormantFixedRecordCaptureReason::UnsafeStateDirectory,
                    DormantFixedRecordCaptureSubject::StateDirectory);
    } else {
      rootBaselineValid = true;
    }
  }

  if (!failure.set()) {
    for (size_t index = 0; index < recordSpecs.size(); ++index) {
      const auto &spec = recordSpecs[index];
      if (!rootGuard(rootFd, rootBaseline, operations, InternalPass::First,
                     spec.subject, static_cast<qsizetype>(index),
                     heldCount(first))) {
        chooseFailure(
            failure,
            operations.proofExceeded
                ? DormantFixedRecordCaptureReason::ProofBudgetExceeded
                : DormantFixedRecordCaptureReason::StateDirectoryChanged,
            DormantFixedRecordCaptureSubject::StateDirectory);
        break;
      }
      if (!captureOne(rootFd, rootBaseline, spec, static_cast<qsizetype>(index),
                      InternalPass::First, operations, reads, first[index],
                      failure, cleanupFailure)) {
        break;
      }
      if (hasIdentityAlias(first, index)) {
        chooseFailure(failure,
                      DormantFixedRecordCaptureReason::RecordIdentityAlias,
                      spec.subject);
        break;
      }
      invokeCheckpoint(TestCheckpoint::AfterRecord, InternalPass::First,
                       spec.subject, static_cast<qsizetype>(index), 0,
                       heldCount(first));
      if (!rootGuard(rootFd, rootBaseline, operations, InternalPass::First,
                     spec.subject, static_cast<qsizetype>(index),
                     heldCount(first))) {
        chooseFailure(
            failure,
            operations.proofExceeded
                ? DormantFixedRecordCaptureReason::ProofBudgetExceeded
                : DormantFixedRecordCaptureReason::StateDirectoryChanged,
            DormantFixedRecordCaptureSubject::StateDirectory);
        break;
      }
    }
  }

  if (!failure.set()) {
    invokeCheckpoint(TestCheckpoint::BetweenPasses, InternalPass::First,
                     DormantFixedRecordCaptureSubject::StateDirectory, -1, 0,
                     heldCount(first));
    if (!rootGuard(rootFd, rootBaseline, operations, InternalPass::Second,
                   DormantFixedRecordCaptureSubject::StateDirectory, -1,
                   heldCount(first))) {
      chooseFailure(
          failure,
          operations.proofExceeded
              ? DormantFixedRecordCaptureReason::ProofBudgetExceeded
              : DormantFixedRecordCaptureReason::StateDirectoryChanged,
          DormantFixedRecordCaptureSubject::StateDirectory);
    }
  }

  if (!failure.set()) {
    std::array<struct stat, 5> secondMetadata{};
    std::array<bool, 5> secondPresent{};
    for (size_t index = 0; index < recordSpecs.size(); ++index) {
      const auto &spec = recordSpecs[index];
      if (!rootGuard(rootFd, rootBaseline, operations, InternalPass::Second,
                     spec.subject, static_cast<qsizetype>(index),
                     heldCount(first))) {
        chooseFailure(
            failure,
            operations.proofExceeded
                ? DormantFixedRecordCaptureReason::ProofBudgetExceeded
                : DormantFixedRecordCaptureReason::StateDirectoryChanged,
            DormantFixedRecordCaptureSubject::StateDirectory);
        break;
      }
      InternalRecord second;
      if (!captureOne(rootFd, rootBaseline, spec, static_cast<qsizetype>(index),
                      InternalPass::Second, operations, reads, second, failure,
                      cleanupFailure)) {
        break;
      }
      if (second.heldFirstPassFd >= 0) {
        chooseFailure(failure, DormantFixedRecordCaptureReason::UnsafeRecord,
                      spec.subject);
        break;
      }
      if (second.kind == DormantFixedRecordFieldKind::PresentBytes) {
        for (size_t prior = 0; prior < index; ++prior) {
          if (secondPresent[prior] &&
              sameIdentity(secondMetadata[prior], second.metadata)) {
            chooseFailure(failure,
                          DormantFixedRecordCaptureReason::RecordIdentityAlias,
                          spec.subject);
            break;
          }
        }
        if (failure.set())
          break;
        secondPresent[index] = true;
        secondMetadata[index] = second.metadata;
      }
      if (!recordsEqual(first[index], second)) {
        chooseFailure(failure, DormantFixedRecordCaptureReason::RecordChanged,
                      spec.subject);
        break;
      }
      invokeCheckpoint(TestCheckpoint::AfterRecord, InternalPass::Second,
                       spec.subject, static_cast<qsizetype>(index), 0,
                       heldCount(first));
      if (!rootGuard(rootFd, rootBaseline, operations, InternalPass::Second,
                     spec.subject, static_cast<qsizetype>(index),
                     heldCount(first))) {
        chooseFailure(
            failure,
            operations.proofExceeded
                ? DormantFixedRecordCaptureReason::ProofBudgetExceeded
                : DormantFixedRecordCaptureReason::StateDirectoryChanged,
            DormantFixedRecordCaptureSubject::StateDirectory);
        break;
      }
    }
  }

  if (!failure.set()) {
    for (size_t index = 0; index < recordSpecs.size(); ++index) {
      auto &record = first[index];
      if (record.kind != DormantFixedRecordFieldKind::PresentBytes)
        continue;
      const auto &spec = recordSpecs[index];
      invokeCheckpoint(TestCheckpoint::BeforeHeldFirstPassFinalization,
                       InternalPass::Second, spec.subject,
                       static_cast<qsizetype>(index), 0, heldCount(first));
      struct stat held{};
      struct stat named{};
      const auto stable =
          fstatRecord(record.heldFirstPassFd, held, operations,
                      InternalPass::Second, spec.subject,
                      TestSyscall::HeldFirstPassFstat) &&
          sameSnapshot(held, record.metadata) &&
          finalNamedFstatat(rootFd, spec, named, operations,
                            InternalPass::Second,
                            TestSyscall::HeldFirstPassNamedFstatat) &&
          sameSnapshot(named, record.metadata) &&
          safeRecord(held, rootBaseline.st_uid, rootBaseline.st_dev,
                     spec.maximumBytes);
      if (!stable) {
        chooseBudgetFailure(failure, operations, reads, spec.subject);
        break;
      }
    }
  }

  // A final full root guard has deterministic precedence over every tentative
  // local result whenever a safe baseline was established.
  if (rootBaselineValid) {
    invokeCheckpoint(TestCheckpoint::BeforeFinalRootGuard, InternalPass::Second,
                     DormantFixedRecordCaptureSubject::StateDirectory, -1, 0,
                     heldCount(first));
    if (!rootGuard(rootFd, rootBaseline, operations, InternalPass::Second,
                   DormantFixedRecordCaptureSubject::StateDirectory, -1,
                   heldCount(first))) {
      failure = {
          .reason =
              operations.proofExceeded
                  ? DormantFixedRecordCaptureReason::ProofBudgetExceeded
                  : DormantFixedRecordCaptureReason::StateDirectoryChanged,
          .subject = DormantFixedRecordCaptureSubject::StateDirectory,
      };
    }
  }

  for (size_t index = 0; index < first.size(); ++index) {
    if (!closeOnce(first[index].heldFirstPassFd, operations,
                   TestSyscall::CloseFile, InternalPass::Second,
                   recordSpecs[index].subject)) {
      chooseFailure(cleanupFailure,
                    DormantFixedRecordCaptureReason::CleanupFailed,
                    recordSpecs[index].subject);
    }
  }
  if (!closeOnce(rootFd, operations, TestSyscall::CloseRoot,
                 InternalPass::Second,
                 DormantFixedRecordCaptureSubject::StateDirectory)) {
    chooseFailure(cleanupFailure,
                  DormantFixedRecordCaptureReason::CleanupFailed,
                  DormantFixedRecordCaptureSubject::StateDirectory);
  }
  if (operations.cleanupExceeded) {
    chooseFailure(cleanupFailure,
                  DormantFixedRecordCaptureReason::CleanupFailed,
                  DormantFixedRecordCaptureSubject::StateDirectory);
  }
  if (cleanupFailure.set()) {
    failure = cleanupFailure;
  }

  if (failure.set()) {
    // Destruction/disengagement scrubs the partial graph. QByteArray allocator
    // storage is not promised to be overwritten.
    first = {};
    return DormantFixedRecordCaptureResult(
        DormantFixedRecordCaptureDisposition::FailedClosed, failure.reason,
        failure.subject, std::nullopt);
  }

  auto capture = DormantFixedRecordCapture(
      DormantFixedRecordField(first[0].kind, std::move(first[0].bytes)),
      DormantFixedRecordField(first[1].kind, std::move(first[1].bytes)),
      DormantFixedRecordField(first[2].kind, std::move(first[2].bytes)),
      DormantFixedRecordField(first[3].kind, std::move(first[3].bytes)),
      DormantFixedRecordField(first[4].kind, std::move(first[4].bytes)));
  return DormantFixedRecordCaptureResult(
      DormantFixedRecordCaptureDisposition::Captured,
      DormantFixedRecordCaptureReason::None,
      DormantFixedRecordCaptureSubject::None, std::move(capture));
}

#if defined(HYPRSHELLD_DORMANT_FIXED_RECORD_CAPTURE_TEST_HOOKS)
namespace DormantFixedRecordCaptureTestSupport {

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

} // namespace DormantFixedRecordCaptureTestSupport
#endif

} // namespace HyprShelld::Compositor
