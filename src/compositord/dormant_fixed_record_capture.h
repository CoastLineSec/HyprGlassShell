#pragma once

#include <QByteArray>
#include <QtTypes>

#include <optional>
#include <utility>

#if defined(HYPRSHELLD_DORMANT_FIXED_RECORD_CAPTURE_TEST_HOOKS)
#include <functional>
#endif

namespace HyprShelld::Compositor {

inline constexpr qsizetype maximumDormantAuthorityCaptureBytes = 256;
inline constexpr qsizetype maximumDormantDesiredCaptureBytes = 4 * 1024 * 1024;
inline constexpr qsizetype maximumDormantLastGoodCaptureBytes = 4 * 1024 * 1024;
inline constexpr qsizetype maximumDormantAppliedCaptureBytes = 4 * 1024 * 1024;
// Opaque raw-byte envelope only: this is the larger of the frozen legacy
// ordinary and current ordinary-v2 Pending records. It does not prove future
// Restart/tagged-union completeness and grants no Pending semantic authority.
inline constexpr qsizetype maximumDormantPendingCaptureBytes = 8404992;
inline constexpr quint64 maximumDormantFixedRecordRetainedPayloadBytes =
    20988160ULL;
inline constexpr quint64 maximumDormantFixedRecordTwoPassPayloadBytes =
    41976320ULL;
// Successful capture receives at most the two-pass payload from pread(2). A
// failed EOF growth probe can return one additional byte, at which point
// capture stops.
inline constexpr quint64 maximumDormantFixedRecordPreadReturnedBytes =
    41976321ULL;
inline constexpr quint64 maximumDormantFixedRecordStreamingPayloadBytes =
    29393152ULL;
inline constexpr qsizetype dormantFixedRecordReadBufferBytes = 4096;
inline constexpr quint64 maximumDormantFixedRecordWorkingPayloadBytes =
    maximumDormantFixedRecordStreamingPayloadBytes +
    dormantFixedRecordReadBufferBytes;
inline constexpr quint64 maximumDormantFixedRecordProofSyscallAttempts = 12000;
inline constexpr quint64 dormantFixedRecordCleanupAttemptReserve = 32;
inline constexpr quint64 maximumDormantFixedRecordSyscallAttempts =
    maximumDormantFixedRecordProofSyscallAttempts +
    dormantFixedRecordCleanupAttemptReserve;

static_assert(maximumDormantFixedRecordRetainedPayloadBytes ==
              static_cast<quint64>(maximumDormantAuthorityCaptureBytes) +
                  maximumDormantDesiredCaptureBytes +
                  maximumDormantLastGoodCaptureBytes +
                  maximumDormantAppliedCaptureBytes +
                  maximumDormantPendingCaptureBytes);
static_assert(maximumDormantFixedRecordTwoPassPayloadBytes ==
              2 * maximumDormantFixedRecordRetainedPayloadBytes);
static_assert(maximumDormantFixedRecordPreadReturnedBytes ==
              maximumDormantFixedRecordTwoPassPayloadBytes + 1);
static_assert(maximumDormantFixedRecordStreamingPayloadBytes ==
              maximumDormantFixedRecordRetainedPayloadBytes +
                  maximumDormantPendingCaptureBytes);
static_assert(dormantFixedRecordCleanupAttemptReserve >= 32);

enum class DormantFixedRecordFieldKind {
  Missing,
  PresentBytes,
};

enum class DormantFixedRecordCaptureDisposition {
  Captured,
  FailedClosed,
};

enum class DormantFixedRecordCaptureReason {
  None,
  InvalidStateDirectoryDescriptor,
  UnsafeStateDirectory,
  StateDirectoryChanged,
  UnsafeRecord,
  RecordChanged,
  RecordIdentityAlias,
  ReadBudgetExceeded,
  ProofBudgetExceeded,
  CleanupFailed,
};

enum class DormantFixedRecordCaptureSubject {
  None,
  Authority,
  Desired,
  LastGood,
  Applied,
  Pending,
  StateDirectory,
};

class DormantFixedRecordCaptureResult;
[[nodiscard]] DormantFixedRecordCaptureResult
captureDormantFixedRecords(int borrowedStateDirectoryFd);

class DormantFixedRecordField final {
public:
  DormantFixedRecordField(const DormantFixedRecordField &) = default;
  // Evidence moves are deliberately copy-preserving: the moved-from public
  // value remains an equally valid observation product.
  DormantFixedRecordField(DormantFixedRecordField &&other) noexcept
      : kind_(other.kind_), bytes_(other.bytes_) {}
  DormantFixedRecordField &operator=(const DormantFixedRecordField &) = default;
  DormantFixedRecordField &operator=(DormantFixedRecordField &&other) noexcept {
    if (this != &other) {
      kind_ = other.kind_;
      bytes_ = other.bytes_;
    }
    return *this;
  }

  [[nodiscard]] DormantFixedRecordFieldKind kind() const { return kind_; }
  [[nodiscard]] const QByteArray &bytes() const { return bytes_; }

  friend bool operator==(const DormantFixedRecordField &,
                         const DormantFixedRecordField &) = default;

private:
  DormantFixedRecordField(DormantFixedRecordFieldKind kind, QByteArray bytes)
      : kind_(kind), bytes_(std::move(bytes)) {}

  DormantFixedRecordFieldKind kind_ = DormantFixedRecordFieldKind::Missing;
  QByteArray bytes_;

  friend DormantFixedRecordCaptureResult captureDormantFixedRecords(int);
};

class DormantFixedRecordCapture final {
public:
  DormantFixedRecordCapture(const DormantFixedRecordCapture &) = default;
  DormantFixedRecordCapture(DormantFixedRecordCapture &&other) noexcept
      : authority_(other.authority_), desired_(other.desired_),
        lastGood_(other.lastGood_), applied_(other.applied_),
        pending_(other.pending_) {}
  DormantFixedRecordCapture &
  operator=(const DormantFixedRecordCapture &) = default;
  DormantFixedRecordCapture &
  operator=(DormantFixedRecordCapture &&other) noexcept {
    if (this != &other) {
      authority_ = other.authority_;
      desired_ = other.desired_;
      lastGood_ = other.lastGood_;
      applied_ = other.applied_;
      pending_ = other.pending_;
    }
    return *this;
  }

  [[nodiscard]] const DormantFixedRecordField &authority() const {
    return authority_;
  }
  [[nodiscard]] const DormantFixedRecordField &desired() const {
    return desired_;
  }
  [[nodiscard]] const DormantFixedRecordField &lastGood() const {
    return lastGood_;
  }
  [[nodiscard]] const DormantFixedRecordField &applied() const {
    return applied_;
  }
  [[nodiscard]] const DormantFixedRecordField &pending() const {
    return pending_;
  }

  friend bool operator==(const DormantFixedRecordCapture &,
                         const DormantFixedRecordCapture &) = default;

private:
  DormantFixedRecordCapture(DormantFixedRecordField authority,
                            DormantFixedRecordField desired,
                            DormantFixedRecordField lastGood,
                            DormantFixedRecordField applied,
                            DormantFixedRecordField pending)
      : authority_(std::move(authority)), desired_(std::move(desired)),
        lastGood_(std::move(lastGood)), applied_(std::move(applied)),
        pending_(std::move(pending)) {}

  DormantFixedRecordField authority_;
  DormantFixedRecordField desired_;
  DormantFixedRecordField lastGood_;
  DormantFixedRecordField applied_;
  DormantFixedRecordField pending_;

  friend DormantFixedRecordCaptureResult captureDormantFixedRecords(int);
};

class DormantFixedRecordCaptureResult final {
public:
  DormantFixedRecordCaptureResult(const DormantFixedRecordCaptureResult &) =
      default;
  DormantFixedRecordCaptureResult(
      DormantFixedRecordCaptureResult &&other) noexcept
      : disposition_(other.disposition_), reason_(other.reason_),
        subject_(other.subject_), capture_(other.capture_) {}
  DormantFixedRecordCaptureResult &
  operator=(const DormantFixedRecordCaptureResult &) = default;
  DormantFixedRecordCaptureResult &
  operator=(DormantFixedRecordCaptureResult &&other) noexcept {
    if (this != &other) {
      disposition_ = other.disposition_;
      reason_ = other.reason_;
      subject_ = other.subject_;
      capture_ = other.capture_;
    }
    return *this;
  }

  [[nodiscard]] DormantFixedRecordCaptureDisposition disposition() const {
    return disposition_;
  }
  [[nodiscard]] DormantFixedRecordCaptureReason reason() const {
    return reason_;
  }
  [[nodiscard]] DormantFixedRecordCaptureSubject subject() const {
    return subject_;
  }
  [[nodiscard]] const std::optional<DormantFixedRecordCapture> &
  capture() const {
    return capture_;
  }

private:
  DormantFixedRecordCaptureResult(
      DormantFixedRecordCaptureDisposition disposition,
      DormantFixedRecordCaptureReason reason,
      DormantFixedRecordCaptureSubject subject,
      std::optional<DormantFixedRecordCapture> capture)
      : disposition_(disposition), reason_(reason), subject_(subject),
        capture_(std::move(capture)) {}

  DormantFixedRecordCaptureDisposition disposition_ =
      DormantFixedRecordCaptureDisposition::FailedClosed;
  DormantFixedRecordCaptureReason reason_ =
      DormantFixedRecordCaptureReason::InvalidStateDirectoryDescriptor;
  DormantFixedRecordCaptureSubject subject_ =
      DormantFixedRecordCaptureSubject::None;
  std::optional<DormantFixedRecordCapture> capture_;

  friend DormantFixedRecordCaptureResult captureDormantFixedRecords(int);
};

// Strictly pathless, descriptor-relative repeat-observation of exactly
// authority.json, desired.json, last-good.json, activation.json, and
// pending.json, in that global order for pass A and then pass B. The borrowed
// retained state-directory descriptor is duplicated immediately; all later
// work uses the owned duplicate. The borrowed descriptor must be an O_RDONLY,
// non-O_PATH directory descriptor; O_PATH and non-readable descriptors fail
// closed.
//
// Captured owns five Missing/PresentBytes raw-byte fields. Present empty bytes
// remain distinct from Missing. FailedClosed owns no capture or partial bytes.
// Scrubbing means destruction/disengagement of partial objects, not allocator
// zeroization.
//
// This is repeat-observation, not an atomic, co-temporal, interval,
// end-of-call, or post-return-fresh snapshot. It proves no canonical
// root/current name, lease, CAS, same-filesystem bind-mount or mount
// provenance, parser/JCS or protected contract, transition/Pending kind,
// generation, ownership, Bridge, stable entrypoint, prerequisite, or action
// authority. No wall-time bound is claimed for untrusted or FUSE-backed
// filesystems. On success reason/subject are None and capture is engaged. Each
// present pass-A descriptor stays open through all pass-B observations and
// final named/descriptor checks. Both passes match in full bytes and exactly
// dev/inode/full mode/uid/gid/nlink/size/mtime/ctime metadata; atime is
// excluded. The retained root was an euid-owned exact-0700 directory. Every
// PresentBytes value was an euid-owned, exact-0600, single-link regular file on
// the root device, within its subject ceiling, read through its observed size
// followed by a one-byte EOF probe that returned zero bytes. No descriptor,
// identity, digest, basename/path, or capability is returned.

#if defined(HYPRSHELLD_DORMANT_FIXED_RECORD_CAPTURE_TEST_HOOKS)
namespace DormantFixedRecordCaptureTestSupport {

enum class Pass { First, Second };

enum class Checkpoint {
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

struct CheckpointEvent final {
  Checkpoint checkpoint = Checkpoint::AfterRootDuplicate;
  Pass pass = Pass::First;
  DormantFixedRecordCaptureSubject subject =
      DormantFixedRecordCaptureSubject::None;
  qsizetype recordIndex = -1;
  quint64 preadInvocation = 0;
  qsizetype heldFirstPassDescriptors = 0;
};

using CheckpointHook = std::function<void(const CheckpointEvent &)>;

enum class Syscall {
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

enum class Fault {
  None,
  FailEintr,
  FailIo,
  ShortReadOneByte,
  ExhaustProofBudget,
  ReportCloseFailure,
};

struct SyscallEvent final {
  Syscall syscall = Syscall::RootFstat;
  Pass pass = Pass::First;
  DormantFixedRecordCaptureSubject subject =
      DormantFixedRecordCaptureSubject::None;
  quint64 invocation = 0;
};

using SyscallHook = std::function<Fault(const SyscallEvent &)>;
using PayloadAllocationFailureHook =
    std::function<bool(Pass, DormantFixedRecordCaptureSubject)>;

// Focused-test-only deterministic seam. It is absent from the normal archive
// ABI. Checkpoint exceptions are swallowed; hook invocation counts are
// bounded. ReportCloseFailure closes the real descriptor once, then reports a
// synthetic cleanup failure so leak assertions remain meaningful.
void setCheckpointHook(CheckpointHook hook);
void setSyscallHook(SyscallHook hook);
void setPayloadAllocationFailureHook(PayloadAllocationFailureHook hook);
void clearHooks();

} // namespace DormantFixedRecordCaptureTestSupport
#endif

} // namespace HyprShelld::Compositor
