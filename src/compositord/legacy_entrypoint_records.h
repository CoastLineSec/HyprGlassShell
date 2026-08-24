#pragma once

#include <QByteArray>
#include <QByteArrayView>
#include <QString>
#include <QtTypes>

#include <optional>

namespace HyprShelld::Compositor {

inline constexpr quint32 legacyEntrypointRecordV1FormatVersion = 1;
inline constexpr qsizetype maximumLegacyEntrypointRecordV1Bytes =
    4 * 1024 * 1024;
inline constexpr int maximumLegacyEntrypointRecordV1Depth = 32;
inline constexpr quint64 maximumLegacyEntrypointPayloadV1Bytes =
    16ULL * 1024ULL * 1024ULL;

enum class LegacyEntrypointFileKindV1 {
  Absent,
  Regular,
};

// These device and inode values are recovered wire observations only. A
// successfully decoded record does not prove descriptor, device, inode, path,
// canonical-root, or directory-tree identity and grants no filesystem
// authority to its caller.
struct LegacyEntrypointOriginalRecordV1 final {
  LegacyEntrypointFileKindV1 kind = LegacyEntrypointFileKindV1::Absent;
  QString digest;
  quint64 size = 0;
  quint32 mode = 0;
  quint64 device = 0;
  quint64 inode = 0;
  QString backupName;

  friend bool operator==(const LegacyEntrypointOriginalRecordV1 &,
                         const LegacyEntrypointOriginalRecordV1 &) = default;
};

// This is the strict active-writer-originating entrypoint-ownership.json v1
// migration grammar. The historical reader incidentally coerced some
// non-string JSON values to empty strings; those parser-only bytes were never
// emitted by the writer and are deliberately repair-only here. This dormant
// pure codec proves only syntax and closed cross-field relationships. It does
// not prove descriptor, device, inode, path, stable entrypoint, backup,
// canonical-root, or directory-tree identity.
struct LegacyEntrypointOwnershipRecordV1 final {
  QString generation;
  QString activationNonce;
  QString entrypointDigest;
  quint64 entrypointSize = 0;
  quint64 entrypointDevice = 0;
  quint64 entrypointInode = 0;
  LegacyEntrypointOriginalRecordV1 original;

  friend bool operator==(const LegacyEntrypointOwnershipRecordV1 &,
                         const LegacyEntrypointOwnershipRecordV1 &) = default;
};

enum class LegacyLiveActivationBridgePhaseV1 {
  Staging,
  Ready,
};

// This is the strict active-writer-originating live-activation.pending.json v1
// migration grammar. Parser-only non-string-to-empty coercions are excluded.
// beforeOwnership is present only for a managed update and is decoded through
// the strict Ownership codec. Numeric identity fields remain untrusted wire
// observations: this record owns no descriptor and proves no device, inode,
// path, swap name, canonical-root, or directory-tree identity.
struct LegacyLiveActivationBridgeRecordV1 final {
  LegacyLiveActivationBridgePhaseV1 phase =
      LegacyLiveActivationBridgePhaseV1::Staging;
  QString token;
  bool adoption = false;

  QString targetGeneration;
  QString targetNonce;
  QString targetDigest;
  quint64 targetSize = 0;
  quint64 targetDevice = 0;
  quint64 targetInode = 0;
  QString swapName;

  LegacyEntrypointFileKindV1 beforeKind = LegacyEntrypointFileKindV1::Absent;
  QString beforeDigest;
  quint64 beforeSize = 0;
  quint32 beforeMode = 0;
  quint64 beforeDevice = 0;
  quint64 beforeInode = 0;
  QString beforeGeneration;
  QString beforeNonce;
  std::optional<LegacyEntrypointOwnershipRecordV1> beforeOwnership;

  QByteArray baselineConfigErrors = QByteArrayLiteral("[]");
  QString baselineProvider;

  friend bool operator==(const LegacyLiveActivationBridgeRecordV1 &,
                         const LegacyLiveActivationBridgeRecordV1 &) = default;
};

// Serializers are partial: they never manufacture durable bytes from a typed
// value that the exact recovered decoder would reject. Wire output uses the
// recovered JsonSupport canonicalJson spelling plus exactly one final LF.
[[nodiscard]] std::optional<QByteArray>
serializeLegacyEntrypointOwnershipRecordV1(
    const LegacyEntrypointOwnershipRecordV1 &record);

[[nodiscard]] std::optional<LegacyEntrypointOwnershipRecordV1>
parseLegacyEntrypointOwnershipRecordV1(QByteArrayView bytes);

[[nodiscard]] std::optional<QByteArray>
serializeLegacyLiveActivationBridgeRecordV1(
    const LegacyLiveActivationBridgeRecordV1 &record);

[[nodiscard]] std::optional<LegacyLiveActivationBridgeRecordV1>
parseLegacyLiveActivationBridgeRecordV1(QByteArrayView bytes);

} // namespace HyprShelld::Compositor
