#pragma once

#include <QByteArray>
#include <QMap>
#include <QString>
#include <QVector>
#include <QtTypes>

#include <optional>
#include <utility>

#if defined(HYPRSHELLD_DORMANT_V2_GENERATION_TREE_CAPTURE_TEST_HOOKS)
#include <functional>
#endif

namespace HyprShelld::Compositor {

inline constexpr qsizetype maximumDormantV2CapturedGenerationTrees = 2;
inline constexpr qsizetype dormantV2GenerationPayloadFileCount = 17;
inline constexpr qsizetype maximumDormantV2GenerationManifestCaptureBytes =
    4 * 1024 * 1024;
inline constexpr qsizetype maximumDormantV2GeneratedFileCaptureBytes =
    16 * 1024 * 1024;
inline constexpr qsizetype dormantV2GenerationCaptureReadBufferBytes =
    64 * 1024;
inline constexpr quint64 maximumDormantV2GenerationRetainedPayloadBytes =
    578813952ULL; // 552 MiB: two manifests plus thirty-four payloads.
inline constexpr quint64 maximumDormantV2GenerationTwoPassPayloadBytes =
    1157627904ULL; // 1,104 MiB.
inline constexpr quint64 maximumDormantV2GenerationPreadReturnedBytes =
    1157627905ULL; // The successful two-pass ceiling plus one growth byte.
inline constexpr quint64 maximumDormantV2GenerationStreamingPayloadBytes =
    595591168ULL; // Retained pass A plus one maximum pass-B payload.
inline constexpr quint64 maximumDormantV2GenerationWorkingPayloadBytes =
    595656704ULL; // Streaming payload plus one conservative 64 KiB chunk.
inline constexpr quint64 maximumDormantV2GenerationEnumerationCalls = 128;
inline constexpr quint64 maximumDormantV2GenerationObservedNonDotDirents = 84;
inline constexpr quint64 maximumDormantV2GenerationDirentNameBytes =
    1024 * 1024;
inline constexpr quint64 maximumDormantV2GenerationProofSyscallAttempts = 65536;
inline constexpr quint64 dormantV2GenerationFinalProofAttemptReserve = 256;
inline constexpr quint64 dormantV2GenerationCleanupAttemptReserve = 256;
inline constexpr quint64 maximumDormantV2GenerationSyscallAttempts =
    maximumDormantV2GenerationProofSyscallAttempts +
    dormantV2GenerationFinalProofAttemptReserve +
    dormantV2GenerationCleanupAttemptReserve;
inline constexpr qsizetype maximumDormantV2GenerationRetainedDescriptors = 41;
inline constexpr qsizetype maximumDormantV2GenerationOwnedDescriptors = 42;

static_assert(maximumDormantV2GenerationRetainedPayloadBytes ==
              maximumDormantV2CapturedGenerationTrees *
                  (maximumDormantV2GenerationManifestCaptureBytes +
                   dormantV2GenerationPayloadFileCount *
                       maximumDormantV2GeneratedFileCaptureBytes));
static_assert(maximumDormantV2GenerationTwoPassPayloadBytes ==
              2 * maximumDormantV2GenerationRetainedPayloadBytes);
static_assert(maximumDormantV2GenerationPreadReturnedBytes ==
              maximumDormantV2GenerationTwoPassPayloadBytes + 1);
static_assert(maximumDormantV2GenerationStreamingPayloadBytes ==
              maximumDormantV2GenerationRetainedPayloadBytes +
                  maximumDormantV2GeneratedFileCaptureBytes);
static_assert(maximumDormantV2GenerationWorkingPayloadBytes ==
              maximumDormantV2GenerationStreamingPayloadBytes +
                  dormantV2GenerationCaptureReadBufferBytes);

enum class DormantV2GenerationTreeCaptureDisposition {
  Captured,
  FailedClosed,
};

enum class DormantV2GenerationTreeCaptureReason {
  None,
  TooManyGenerationReferences,
  InvalidActivationNonce,
  DuplicateActivationNonce,
  InvalidGenerationsDirectoryDescriptor,
  UnsafeGenerationsDirectory,
  GenerationsDirectoryChanged,
  GenerationRootMissing,
  GenerationRootUnsafe,
  ModulesDirectoryMissing,
  ModulesDirectoryUnsafe,
  InventoryMismatch,
  FileMissing,
  FileUnsafe,
  TreeChanged,
  IdentityAlias,
  ReturnedBytesExceeded,
  PerFileReadAttemptsExceeded,
  EnumerationBudgetExceeded,
  ProofBudgetExceeded,
  AllocationFailed,
  CleanupFailed,
};

enum class DormantV2GenerationTreeCaptureSubject {
  None,
  GenerationsDirectory,
  GenerationRoot,
  ModulesDirectory,
  Manifest,
  Payload,
};

enum class DormantV2GenerationTreeFile {
  None,
  Manifest,
  Entrypoint,
  Module00Session,
  Module10Monitors,
  Module20Environment,
  Module30Input,
  Module31Gestures,
  Module32Cursor,
  Module40General,
  Module41Layouts,
  Module42Workspaces,
  Module43Groups,
  Module50Decorations,
  Module51Animations,
  Module60Rules,
  Module70Keybinds,
  Module80Permissions,
  Module90Advanced,
};

class DormantV2GenerationTreeCaptureResult;
[[nodiscard]] DormantV2GenerationTreeCaptureResult
captureDormantV2GenerationTrees(int borrowedGenerationsDirectoryFd,
                                const QVector<QString> &activationNonces);

class DormantV2GenerationTreeCapture final {
public:
  DormantV2GenerationTreeCapture(const DormantV2GenerationTreeCapture &) =
      default;
  // Evidence moves are copy-preserving: a moved-from public value remains an
  // equally valid historical byte observation.
  DormantV2GenerationTreeCapture(
      DormantV2GenerationTreeCapture &&other) noexcept
      : activationNonce_(other.activationNonce_),
        manifestBytes_(other.manifestBytes_), files_(other.files_) {}
  DormantV2GenerationTreeCapture &
  operator=(const DormantV2GenerationTreeCapture &) = default;
  DormantV2GenerationTreeCapture &
  operator=(DormantV2GenerationTreeCapture &&other) noexcept {
    if (this != &other) {
      activationNonce_ = other.activationNonce_;
      manifestBytes_ = other.manifestBytes_;
      files_ = other.files_;
    }
    return *this;
  }

  [[nodiscard]] const QString &activationNonce() const {
    return activationNonce_;
  }
  [[nodiscard]] const QByteArray &manifestBytes() const {
    return manifestBytes_;
  }
  [[nodiscard]] const QMap<QString, QByteArray> &files() const {
    return files_;
  }

  friend bool operator==(const DormantV2GenerationTreeCapture &,
                         const DormantV2GenerationTreeCapture &) = default;

private:
  DormantV2GenerationTreeCapture(QString activationNonce,
                                 QByteArray manifestBytes,
                                 QMap<QString, QByteArray> files)
      : activationNonce_(std::move(activationNonce)),
        manifestBytes_(std::move(manifestBytes)), files_(std::move(files)) {}

  QString activationNonce_;
  QByteArray manifestBytes_;
  QMap<QString, QByteArray> files_;

  friend DormantV2GenerationTreeCaptureResult
  captureDormantV2GenerationTrees(int, const QVector<QString> &);
};

class DormantV2GenerationTreesCapture final {
public:
  DormantV2GenerationTreesCapture(const DormantV2GenerationTreesCapture &) =
      default;
  DormantV2GenerationTreesCapture(
      DormantV2GenerationTreesCapture &&other) noexcept
      : trees_(other.trees_) {}
  DormantV2GenerationTreesCapture &
  operator=(const DormantV2GenerationTreesCapture &) = default;
  DormantV2GenerationTreesCapture &
  operator=(DormantV2GenerationTreesCapture &&other) noexcept {
    if (this != &other)
      trees_ = other.trees_;
    return *this;
  }

  [[nodiscard]] const QVector<DormantV2GenerationTreeCapture> &trees() const {
    return trees_;
  }

  friend bool operator==(const DormantV2GenerationTreesCapture &,
                         const DormantV2GenerationTreesCapture &) = default;

private:
  explicit DormantV2GenerationTreesCapture(
      QVector<DormantV2GenerationTreeCapture> trees)
      : trees_(std::move(trees)) {}

  QVector<DormantV2GenerationTreeCapture> trees_;

  friend DormantV2GenerationTreeCaptureResult
  captureDormantV2GenerationTrees(int, const QVector<QString> &);
};

class DormantV2GenerationTreeCaptureResult final {
public:
  DormantV2GenerationTreeCaptureResult(
      const DormantV2GenerationTreeCaptureResult &) = default;
  DormantV2GenerationTreeCaptureResult(
      DormantV2GenerationTreeCaptureResult &&other) noexcept
      : disposition_(other.disposition_), reason_(other.reason_),
        subject_(other.subject_), generationIndex_(other.generationIndex_),
        file_(other.file_), capture_(other.capture_) {}
  DormantV2GenerationTreeCaptureResult &
  operator=(const DormantV2GenerationTreeCaptureResult &) = default;
  DormantV2GenerationTreeCaptureResult &
  operator=(DormantV2GenerationTreeCaptureResult &&other) noexcept {
    if (this != &other) {
      disposition_ = other.disposition_;
      reason_ = other.reason_;
      subject_ = other.subject_;
      generationIndex_ = other.generationIndex_;
      file_ = other.file_;
      capture_ = other.capture_;
    }
    return *this;
  }

  [[nodiscard]] DormantV2GenerationTreeCaptureDisposition disposition() const {
    return disposition_;
  }
  [[nodiscard]] DormantV2GenerationTreeCaptureReason reason() const {
    return reason_;
  }
  [[nodiscard]] DormantV2GenerationTreeCaptureSubject subject() const {
    return subject_;
  }
  // Original caller index, never sorted position.
  [[nodiscard]] qsizetype generationIndex() const { return generationIndex_; }
  [[nodiscard]] DormantV2GenerationTreeFile file() const { return file_; }
  [[nodiscard]] const std::optional<DormantV2GenerationTreesCapture> &
  capture() const {
    return capture_;
  }

private:
  DormantV2GenerationTreeCaptureResult(
      DormantV2GenerationTreeCaptureDisposition disposition,
      DormantV2GenerationTreeCaptureReason reason,
      DormantV2GenerationTreeCaptureSubject subject, qsizetype generationIndex,
      DormantV2GenerationTreeFile file,
      std::optional<DormantV2GenerationTreesCapture> capture)
      : disposition_(disposition), reason_(reason), subject_(subject),
        generationIndex_(generationIndex), file_(file),
        capture_(std::move(capture)) {}

  DormantV2GenerationTreeCaptureDisposition disposition_ =
      DormantV2GenerationTreeCaptureDisposition::FailedClosed;
  DormantV2GenerationTreeCaptureReason reason_ =
      DormantV2GenerationTreeCaptureReason::
          InvalidGenerationsDirectoryDescriptor;
  DormantV2GenerationTreeCaptureSubject subject_ =
      DormantV2GenerationTreeCaptureSubject::None;
  qsizetype generationIndex_ = -1;
  DormantV2GenerationTreeFile file_ = DormantV2GenerationTreeFile::None;
  std::optional<DormantV2GenerationTreesCapture> capture_;

  friend DormantV2GenerationTreeCaptureResult
  captureDormantV2GenerationTrees(int, const QVector<QString> &);
};

// Strictly pathless descriptor-relative repeat capture of at most two exact
// immutable v2 generation trees. Requests are bounded, validated, and copied
// in caller order before any hook or syscall. The borrowed generations-
// directory descriptor is then duplicated before filesystem observation;
// later work uses only that owned duplicate. Trees are observed in sorted
// nonce order, with every pass-A tree complete before any pass-B tree.
//
// Captured owns only each nonce, manifest bytes, and the exact seventeen-file
// generated map. FailedClosed owns no bytes, identity, digest, path, or partial
// capture. Disengagement destroys partial containers but does not promise
// allocator-storage zeroization. The owned outer generations-directory
// descriptor is an euid-owned exact-0700 directory. Every successful pass-A
// file descriptor remains open through pass B and final named checks. Both
// observations match in bytes and full
// dev/inode/mode/uid/gid/nlink/size/mtime/ctime metadata (not atime). Raw root
// and modules inventories for each captured generation are exact in both
// passes. Every captured generation-root/modules directory and generated file
// is euid-owned, on the outer root device, pairwise identity-distinct, and has
// exact 0500 directory or exact 0400 single-link regular-file mode.
//
// This proves no canonical root or current name, config/managed path, mount or
// bind-mount provenance, fixed-record reference, Desired bytes, renderer path,
// parser/content result, co-temporality, freshness after return, lease, CAS,
// startup prerequisite, repair, action, publication, or activation authority.
// Endpoint-equal metadata/content ABA (including timestamp collisions,
// privileged mutation, and adversarial FUSE behavior) is outside this
// observation. No wall-time bound is claimed for untrusted or FUSE-backed
// filesystem operations.
// A successful capture cannot be converted to SettledV2GenerationEvidence by
// this API and cannot set referencedGenerationsVerified.

#if defined(HYPRSHELLD_DORMANT_V2_GENERATION_TREE_CAPTURE_TEST_HOOKS)
namespace DormantV2GenerationTreeCaptureTestSupport {

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
  BeforeReaddir,
  AfterReaddir,
  AfterGenerationDirectories,
  AfterInventory,
  AfterFile,
  BetweenPasses,
  BeforeHeldFirstPassFinalization,
  BeforeFinalRootGuard,
};

struct CheckpointEvent final {
  Checkpoint checkpoint = Checkpoint::AfterRootDuplicate;
  Pass pass = Pass::First;
  DormantV2GenerationTreeCaptureSubject subject =
      DormantV2GenerationTreeCaptureSubject::None;
  qsizetype generationIndex = -1;
  DormantV2GenerationTreeFile file = DormantV2GenerationTreeFile::None;
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

enum class Fault {
  None,
  FailEintr,
  FailIo,
  ShortReadOneByte,
  ExhaustProofBudget,
  ReportCleanupFailure,
};

struct SyscallEvent final {
  Syscall syscall = Syscall::RootFstat;
  Pass pass = Pass::First;
  DormantV2GenerationTreeCaptureSubject subject =
      DormantV2GenerationTreeCaptureSubject::None;
  qsizetype generationIndex = -1;
  DormantV2GenerationTreeFile file = DormantV2GenerationTreeFile::None;
  quint64 invocation = 0;
};

using SyscallHook = std::function<Fault(const SyscallEvent &)>;
using PayloadAllocationFailureHook =
    std::function<bool(Pass, qsizetype, DormantV2GenerationTreeFile)>;

void setCheckpointHook(CheckpointHook hook);
void setSyscallHook(SyscallHook hook);
void setPayloadAllocationFailureHook(PayloadAllocationFailureHook hook);
void clearHooks();

} // namespace DormantV2GenerationTreeCaptureTestSupport
#endif

} // namespace HyprShelld::Compositor
