#pragma once

#include "reachable_v1_preflight.h"

#include <QByteArray>
#include <QByteArrayView>
#include <QMap>
#include <QString>
#include <QVector>
#include <QtTypes>

#include <optional>

#if defined(HYPRSHELLD_DORMANT_V1_CAPTURE_TEST_HOOKS)
#include <functional>
#endif

namespace HyprShelld::Compositor {

inline constexpr qsizetype maximumDormantV1MetadataCaptureBytes =
    4 * 1024 * 1024;
inline constexpr qsizetype maximumDormantV1GeneratedFileCaptureBytes =
    16 * 1024 * 1024;
inline constexpr qsizetype maximumDormantV1CapturedGenerations = 2;
inline constexpr quint64 maximumDormantV1RetainedPayloadBytes =
    603979776ULL; // 576 MiB: six records plus two exact generation trees.
inline constexpr quint64 maximumDormantV1TwoPassPayloadReadBytes =
    1207959552ULL; // 1,152 MiB.
inline constexpr quint64 maximumDormantV1CallerPayloadBytes =
    612368384ULL; // 584 MiB, including two evidence desired snapshots.
inline constexpr qsizetype dormantV1CaptureReadBufferBytes = 64 * 1024;
inline constexpr qsizetype maximumDormantV1UniqueFiles = 42;
inline constexpr qsizetype maximumDormantV1IdentityReceipts = 50;
inline constexpr qsizetype maximumDormantV1ExpectedDirents = 76;
inline constexpr qsizetype maximumDormantV1ObservedDirents = 84;
inline constexpr quint64 maximumDormantV1DirentNameBytes = 1024 * 1024;
inline constexpr quint64 maximumDormantV1EnumerationCalls = 128;
// Hard aggregate ceiling, split into independent initial, body, and final
// phases so body exhaustion cannot consume the mandatory final root guard.
inline constexpr qsizetype maximumDormantV1PathUtf8Bytes = 4096;
inline constexpr qsizetype maximumDormantV1PathCodeUnits = 4096;
// `/a` repeated 2,048 times is the maximum clean one-byte-component path
// admitted by the 4,096-byte absolute-path grammar.
inline constexpr qsizetype maximumDormantV1PathComponents = 2048;
inline constexpr quint64 dormantV1RootCleanupAttemptReserve =
    4ULL * (maximumDormantV1PathComponents + 2ULL);
inline constexpr quint64 dormantV1BodyCleanupAttemptReserve = 512;
inline constexpr quint64 dormantV1InitialRootProofAttemptLimit =
    24ULL * maximumDormantV1PathComponents + 53ULL;
inline constexpr quint64 dormantV1FinalRootProofAttemptLimit =
    24ULL * maximumDormantV1PathComponents + 52ULL;
inline constexpr quint64 dormantV1InitialRootAttemptReserve =
    dormantV1InitialRootProofAttemptLimit + dormantV1RootCleanupAttemptReserve;
inline constexpr quint64 dormantV1BodyAttemptLimit = 28672;
inline constexpr quint64 dormantV1FinalRootAttemptReserve =
    dormantV1FinalRootProofAttemptLimit + dormantV1RootCleanupAttemptReserve;
inline constexpr quint64 maximumDormantV1SyscallAttempts =
    dormantV1InitialRootAttemptReserve + dormantV1BodyAttemptLimit +
    dormantV1FinalRootAttemptReserve;
inline constexpr qsizetype maximumDormantV1RelativePathUtf8Bytes = 64;
inline constexpr qsizetype maximumDormantV1RelativePathCodeUnits = 64;

static_assert(dormantV1InitialRootAttemptReserve + dormantV1BodyAttemptLimit +
                  dormantV1FinalRootAttemptReserve ==
              maximumDormantV1SyscallAttempts);

enum class DormantV1FileCaptureKind {
  Missing,
  ExactRegular,
  Unsafe,
};

struct DormantV1FileIdentity final {
  quint64 device = 0;
  quint64 inode = 0;
  quint64 size = 0;
  quint32 mode = 0; // Permission bits in [0, 0777].
  // Nonnegative kernel uid/link-count/timestamp observations widened into
  // fixed Qt integer types; nanoseconds are in [0, 999999999].
  quint64 owner = 0;
  quint64 linkCount = 0;
  qint64 modifiedSeconds = 0;
  qint64 modifiedNanoseconds = 0;
  qint64 changedSeconds = 0;
  qint64 changedNanoseconds = 0;

  friend bool operator==(const DormantV1FileIdentity &,
                         const DormantV1FileIdentity &) = default;
};

struct DormantV1DirectoryIdentity final {
  quint64 device = 0;
  quint64 inode = 0;
  quint64 size = 0;
  quint32 mode = 0; // Permission bits in [0, 0777].
  quint64 owner = 0;
  quint64 linkCount = 0;
  qint64 modifiedSeconds = 0;
  qint64 modifiedNanoseconds = 0;
  qint64 changedSeconds = 0;
  qint64 changedNanoseconds = 0;

  friend bool operator==(const DormantV1DirectoryIdentity &,
                         const DormantV1DirectoryIdentity &) = default;
};

// One bounded, descriptor-relative observation. Device/inode values are
// diagnostics only: this type is not a receipt and grants no later CAS or
// publication authority.
struct DormantV1FileCapture final {
  DormantV1FileCaptureKind kind = DormantV1FileCaptureKind::Missing;
  QByteArray bytes;
  QString sha256;
  DormantV1FileIdentity identity;

  friend bool operator==(const DormantV1FileCapture &,
                         const DormantV1FileCapture &) = default;
};

struct BorrowedDormantV1FilesystemRoots final {
  // The caller keeps its lifetime store lease and these O_CLOEXEC
  // descriptors alive for the complete synchronous call.
  int stateDirectoryFd = -1;
  int configDirectoryFd = -1;
  int managedDirectoryFd = -1;
  int generationsDirectoryFd = -1;
  QString stateRoot;
  QString configRoot;
  QString managedConfigRoot;
};

struct DormantV1FixedRecordCapture final {
  DormantV1FileCapture desired;
  DormantV1FileCapture lastGood;
  DormantV1FileCapture applied;
  DormantV1FileCapture pending;
  DormantV1FileCapture ownership;
  DormantV1FileCapture bridge;
};

struct DormantV1GenerationTreeCapture final {
  QString activationNonce;
  DormantV1DirectoryIdentity rootIdentity;
  DormantV1DirectoryIdentity modulesIdentity;
  DormantV1FileCapture manifest;
  // Exactly hyprland.lua and the sixteen frozen v1 module paths.
  QMap<QString, DormantV1FileCapture> files;
};

struct DormantV1RootCapture final {
  DormantV1DirectoryIdentity state;
  DormantV1DirectoryIdentity config;
  DormantV1DirectoryIdentity managed;
  DormantV1DirectoryIdentity generations;
};

struct DormantV1FilesystemCapture final {
  // All four root receipts and every file/directory receipt contain the
  // complete identity predicates compared across both aggregate passes.
  DormantV1RootCapture roots;
  DormantV1FixedRecordCapture records;
  // Sorted by activation nonce after duplicate rejection.
  QVector<DormantV1GenerationTreeCapture> generations;
};

enum class DormantV1CaptureDisposition {
  Captured,
  FailedClosed,
};

enum class DormantV1CaptureReason {
  None,
  TooManyGenerationEvidences,
  CallerPayloadTooLarge,
  UnsafeExpectedRead,
  InvalidExpectedRead,
  ExpectedRecordOversized,
  InvalidGenerationReference,
  DuplicateGenerationReference,
  InvalidGenerationPaths,
  InvalidGenerationInventory,
  GenerationEvidenceOversized,
  InvalidRootDescriptors,
  InvalidRootPath,
  InvalidRootLayout,
  UnsafeRootMetadata,
  RootIdentityMismatch,
  RootsChanged,
  FixedRecordUnsafe,
  FixedRecordMismatch,
  CaptureBudgetExceeded,
  GenerationRootMissing,
  GenerationRootUnsafe,
  GenerationIdentityAlias,
  GenerationModulesMissing,
  GenerationModulesUnsafe,
  GenerationInventoryMismatch,
  GenerationFileMissing,
  GenerationFileUnsafe,
  GenerationFileMismatch,
  GenerationTreeChanged,
};

enum class DormantV1CaptureSubject {
  None,
  Desired,
  LastGood,
  Applied,
  Pending,
  Ownership,
  Bridge,
  GenerationRoot,
  GenerationModules,
  GenerationManifest,
  GenerationPayload,
  StateRoot,
  ConfigRoot,
  ManagedRoot,
  GenerationsRoot,
};

struct DormantV1FilesystemCaptureResult final {
  DormantV1CaptureDisposition disposition =
      DormantV1CaptureDisposition::FailedClosed;
  DormantV1CaptureReason reason =
      DormantV1CaptureReason::InvalidRootDescriptors;
  DormantV1CaptureSubject subject = DormantV1CaptureSubject::None;
  // Original index in ReachableV1PreflightInput::referencedGenerations.
  qsizetype generationIndex = -1;
  // Populated only after the referenced nonce passed its closed validation.
  QString validatedNonce;
  // A member of the fixed generation-relative path vocabulary only.
  QString relativePath;
  std::optional<DormantV1FilesystemCapture> capture;
};

// Capture-only binding of a caller-supplied reachable-v1 byte graph to the
// currently named descriptor roots. This parses no record or manifest,
// classifies no bridge side, and promises no freshness after return. It owns
// no Store, publication, journal, action, repair, activation, or filesystem
// mutation path and can never report migration eligibility.
[[nodiscard]] DormantV1FilesystemCaptureResult
captureDormantReachableV1Filesystem(
    const BorrowedDormantV1FilesystemRoots &roots,
    const ReachableV1PreflightInput &expected);

// A successful Captured result has reason/subject None, index -1, empty
// validatedNonce/relativePath, a populated capture, six observations and
// every sorted generation equal in full bytes, raw inventories, and rich
// identities across global pass A then global pass B. Missing fixed-record
// observations are allowed but always have empty bytes/digest and a zero
// identity; Unsafe is never retained. FailedClosed always has no capture. No
// result promises freshness.
//
// Deterministic failure precedence is: caller container cardinality; expected-
// read grammar and per-element bounds in fixed-record order while accumulating
// payload; generation reference/path/inventory/per-element bounds in original
// order while accumulating payload; aggregate payload bounds; root descriptor/
// path/layout/metadata/
// identity; pass-A fixed records then sorted trees; pass-B fixed records then
// sorted trees; final guard. Within a tree the order is root, modules, raw
// inventories, manifest, hyprland.lua, then the frozen module order. The
// final full root guard always overrides any tentative local failure/success.

#if defined(HYPRSHELLD_DORMANT_V1_CAPTURE_TEST_HOOKS)
namespace DormantV1CaptureTestSupport {

enum class Pass { First, Second };

enum class Checkpoint {
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

struct CheckpointEvent final {
  Checkpoint checkpoint = Checkpoint::AfterInitialRootGuard;
  Pass pass = Pass::First;
  DormantV1CaptureSubject subject = DormantV1CaptureSubject::None;
  // Original caller index, never sorted position.
  qsizetype generationIndex = -1;
  QString relativePath;
};

using CheckpointHook = std::function<void(const CheckpointEvent &)>;

// Private focused-test seam; absent from the production-isolated library ABI.
// The hook receives no descriptor, but a focused test may mutate filesystem
// state it owns at the named boundary. Exceptions are caught and invocations
// are bounded internally.
void setCheckpointHook(CheckpointHook hook);
void clearCheckpointHook();

} // namespace DormantV1CaptureTestSupport
#endif

} // namespace HyprShelld::Compositor
