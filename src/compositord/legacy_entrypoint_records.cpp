#include "legacy_entrypoint_records.h"

#include "hyprland/json_support.h"

#include <QJsonObject>
#include <QSet>

#include <cstring>
#include <utility>

namespace HyprShelld::Compositor {
namespace {

[[nodiscard]] QSet<QString> keysOf(const QJsonObject &object) {
  QSet<QString> result;
  for (auto iterator = object.constBegin(); iterator != object.constEnd();
       ++iterator) {
    result.insert(iterator.key());
  }
  return result;
}

[[nodiscard]] bool validSha256(const QStringView value) {
  if (value.size() != 64) {
    return false;
  }
  for (const auto character : value) {
    if (!((character >= u'0' && character <= u'9') ||
          (character >= u'a' && character <= u'f'))) {
      return false;
    }
  }
  return true;
}

[[nodiscard]] bool validNonce(const QStringView value) {
  if (value.size() < 32 || value.size() > 128) {
    return false;
  }
  for (const auto character : value) {
    if (!((character >= u'0' && character <= u'9') ||
          (character >= u'a' && character <= u'f'))) {
      return false;
    }
  }
  return true;
}

[[nodiscard]] bool parseUnsigned(const QStringView value, quint64 &result) {
  if (value.isEmpty() || (value.size() > 1 && value.front() == u'0')) {
    return false;
  }
  for (const auto character : value) {
    if (character < u'0' || character > u'9') {
      return false;
    }
  }
  bool converted = false;
  result = value.toString().toULongLong(&converted, 10);
  return converted && QString::number(result) == value;
}

[[nodiscard]] QString decimal(const quint64 value) {
  return QString::number(value);
}

[[nodiscard]] QString fileKindName(const LegacyEntrypointFileKindV1 kind) {
  switch (kind) {
  case LegacyEntrypointFileKindV1::Absent:
    return QStringLiteral("absent");
  case LegacyEntrypointFileKindV1::Regular:
    return QStringLiteral("regular");
  }
  return {};
}

[[nodiscard]] std::optional<LegacyEntrypointFileKindV1>
fileKindFromName(const QStringView name) {
  if (name == QStringLiteral("absent")) {
    return LegacyEntrypointFileKindV1::Absent;
  }
  if (name == QStringLiteral("regular")) {
    return LegacyEntrypointFileKindV1::Regular;
  }
  return std::nullopt;
}

[[nodiscard]] QString
bridgePhaseName(const LegacyLiveActivationBridgePhaseV1 phase) {
  switch (phase) {
  case LegacyLiveActivationBridgePhaseV1::Staging:
    return QStringLiteral("staging");
  case LegacyLiveActivationBridgePhaseV1::Ready:
    return QStringLiteral("ready");
  }
  return {};
}

[[nodiscard]] std::optional<LegacyLiveActivationBridgePhaseV1>
bridgePhaseFromName(const QStringView name) {
  if (name == QStringLiteral("staging")) {
    return LegacyLiveActivationBridgePhaseV1::Staging;
  }
  if (name == QStringLiteral("ready")) {
    return LegacyLiveActivationBridgePhaseV1::Ready;
  }
  return std::nullopt;
}

[[nodiscard]] bool validOriginalBackupName(const QString &name) {
  constexpr auto prefix = ".hyprshelld-original-";
  constexpr auto suffix = ".lua";
  return name.startsWith(QLatin1StringView(prefix)) &&
         name.endsWith(QLatin1StringView(suffix)) &&
         validNonce(QStringView(name).mid(
             static_cast<qsizetype>(std::strlen(prefix)),
             name.size() - static_cast<qsizetype>(std::strlen(prefix)) -
                 static_cast<qsizetype>(std::strlen(suffix))));
}

[[nodiscard]] bool exactSwapName(const QString &name, const QString &token,
                                 const bool adoption) {
  return name == QStringLiteral(".hyprshelld-%1-%2.lua")
                     .arg(adoption ? QStringLiteral("original")
                                   : QStringLiteral("transition"),
                          token);
}

[[nodiscard]] bool safeRegularMode(const quint32 mode) {
  return mode <= 0777U && (mode & 0022U) == 0;
}

[[nodiscard]] bool
validOriginal(const LegacyEntrypointOriginalRecordV1 &original) {
  if (original.kind == LegacyEntrypointFileKindV1::Absent) {
    return original.digest.isEmpty() && original.size == 0 &&
           original.mode == 0 && original.device == 0 && original.inode == 0 &&
           original.backupName.isEmpty();
  }
  if (original.kind != LegacyEntrypointFileKindV1::Regular) {
    return false;
  }
  return validSha256(original.digest) &&
         original.size <= maximumLegacyEntrypointPayloadV1Bytes &&
         safeRegularMode(original.mode) && original.device != 0 &&
         original.inode != 0 && validOriginalBackupName(original.backupName);
}

[[nodiscard]] bool
validOwnership(const LegacyEntrypointOwnershipRecordV1 &record) {
  return validSha256(record.generation) && validNonce(record.activationNonce) &&
         validSha256(record.entrypointDigest) &&
         record.entrypointSize <= maximumLegacyEntrypointPayloadV1Bytes &&
         record.entrypointDevice != 0 && record.entrypointInode != 0 &&
         validOriginal(record.original);
}

[[nodiscard]] QJsonObject
ownershipObject(const LegacyEntrypointOwnershipRecordV1 &record) {
  return {
      {QStringLiteral("formatVersion"),
       static_cast<qint64>(legacyEntrypointRecordV1FormatVersion)},
      {QStringLiteral("generation"), record.generation},
      {QStringLiteral("activationNonce"), record.activationNonce},
      {QStringLiteral("entrypointDigest"), record.entrypointDigest},
      {QStringLiteral("entrypointSize"), decimal(record.entrypointSize)},
      {QStringLiteral("entrypointDevice"), decimal(record.entrypointDevice)},
      {QStringLiteral("entrypointInode"), decimal(record.entrypointInode)},
      {QStringLiteral("originalKind"), fileKindName(record.original.kind)},
      {QStringLiteral("originalDigest"), record.original.digest},
      {QStringLiteral("originalSize"), decimal(record.original.size)},
      {QStringLiteral("originalMode"),
       static_cast<qint64>(record.original.mode)},
      {QStringLiteral("originalDevice"), decimal(record.original.device)},
      {QStringLiteral("originalInode"), decimal(record.original.inode)},
      {QStringLiteral("originalBackup"), record.original.backupName},
  };
}

[[nodiscard]] QByteArray
ownershipBytesUnchecked(const LegacyEntrypointOwnershipRecordV1 &record) {
  auto bytes = Hyprland::JsonSupport::canonicalJson(ownershipObject(record));
  bytes.append('\n');
  return bytes;
}

[[nodiscard]] std::optional<LegacyEntrypointOwnershipRecordV1>
parseOwnershipObject(const QJsonObject &object) {
  static const QSet<QString> expected{
      QStringLiteral("formatVersion"),   QStringLiteral("generation"),
      QStringLiteral("activationNonce"), QStringLiteral("entrypointDigest"),
      QStringLiteral("entrypointSize"),  QStringLiteral("entrypointDevice"),
      QStringLiteral("entrypointInode"), QStringLiteral("originalKind"),
      QStringLiteral("originalDigest"),  QStringLiteral("originalSize"),
      QStringLiteral("originalMode"),    QStringLiteral("originalDevice"),
      QStringLiteral("originalInode"),   QStringLiteral("originalBackup"),
  };
  if (keysOf(object) != expected ||
      object.value(QStringLiteral("formatVersion")).toInt(-1) !=
          static_cast<int>(legacyEntrypointRecordV1FormatVersion)) {
    return std::nullopt;
  }

  const auto originalKind =
      fileKindFromName(object.value(QStringLiteral("originalKind")).toString());
  const auto originalMode = object.value(QStringLiteral("originalMode"));
  quint64 entrypointSize = 0;
  quint64 entrypointDevice = 0;
  quint64 entrypointInode = 0;
  quint64 originalSize = 0;
  quint64 originalDevice = 0;
  quint64 originalInode = 0;
  if (!originalKind || !originalMode.isDouble() ||
      originalMode.toInteger(-1) < 0 || originalMode.toInteger(-1) > 0777 ||
      !parseUnsigned(object.value(QStringLiteral("entrypointSize")).toString(),
                     entrypointSize) ||
      !parseUnsigned(
          object.value(QStringLiteral("entrypointDevice")).toString(),
          entrypointDevice) ||
      !parseUnsigned(object.value(QStringLiteral("entrypointInode")).toString(),
                     entrypointInode) ||
      !parseUnsigned(object.value(QStringLiteral("originalSize")).toString(),
                     originalSize) ||
      !parseUnsigned(object.value(QStringLiteral("originalDevice")).toString(),
                     originalDevice) ||
      !parseUnsigned(object.value(QStringLiteral("originalInode")).toString(),
                     originalInode)) {
    return std::nullopt;
  }

  LegacyEntrypointOwnershipRecordV1 record{
      .generation = object.value(QStringLiteral("generation")).toString(),
      .activationNonce =
          object.value(QStringLiteral("activationNonce")).toString(),
      .entrypointDigest =
          object.value(QStringLiteral("entrypointDigest")).toString(),
      .entrypointSize = entrypointSize,
      .entrypointDevice = entrypointDevice,
      .entrypointInode = entrypointInode,
      .original =
          {
              .kind = *originalKind,
              .digest =
                  object.value(QStringLiteral("originalDigest")).toString(),
              .size = originalSize,
              .mode = static_cast<quint32>(originalMode.toInteger()),
              .device = originalDevice,
              .inode = originalInode,
              .backupName =
                  object.value(QStringLiteral("originalBackup")).toString(),
          },
  };
  return validOwnership(record)
             ? std::optional<LegacyEntrypointOwnershipRecordV1>(
                   std::move(record))
             : std::nullopt;
}

[[nodiscard]] bool
validBridge(const LegacyLiveActivationBridgeRecordV1 &record) {
  if (bridgePhaseName(record.phase).isEmpty() || !validNonce(record.token) ||
      !validSha256(record.targetGeneration) ||
      !validNonce(record.targetNonce) || !validSha256(record.targetDigest) ||
      record.targetSize > maximumLegacyEntrypointPayloadV1Bytes ||
      !exactSwapName(record.swapName, record.token, record.adoption) ||
      record.baselineConfigErrors != QByteArrayLiteral("[]") ||
      (record.baselineProvider != QStringLiteral("lua") &&
       record.baselineProvider != QStringLiteral("hyprlang"))) {
    return false;
  }

  if ((record.phase == LegacyLiveActivationBridgePhaseV1::Staging &&
       (record.targetDevice != 0 || record.targetInode != 0)) ||
      (record.phase == LegacyLiveActivationBridgePhaseV1::Ready &&
       (record.targetDevice == 0 || record.targetInode == 0))) {
    return false;
  }

  if (record.beforeKind == LegacyEntrypointFileKindV1::Absent) {
    if (!record.beforeDigest.isEmpty() || record.beforeSize != 0 ||
        record.beforeMode != 0 || record.beforeDevice != 0 ||
        record.beforeInode != 0) {
      return false;
    }
  } else if (record.beforeKind == LegacyEntrypointFileKindV1::Regular) {
    if (!validSha256(record.beforeDigest) ||
        record.beforeSize > maximumLegacyEntrypointPayloadV1Bytes ||
        !safeRegularMode(record.beforeMode) || record.beforeDevice == 0 ||
        record.beforeInode == 0) {
      return false;
    }
  } else {
    return false;
  }

  if (record.adoption) {
    if (!record.beforeGeneration.isEmpty() || !record.beforeNonce.isEmpty() ||
        record.beforeOwnership.has_value()) {
      return false;
    }
    return record.beforeKind == LegacyEntrypointFileKindV1::Absent
               ? record.baselineProvider == QStringLiteral("hyprlang")
               : record.baselineProvider == QStringLiteral("lua");
  }

  if (record.beforeKind != LegacyEntrypointFileKindV1::Regular ||
      record.baselineProvider != QStringLiteral("lua") ||
      !record.beforeOwnership || !validOwnership(*record.beforeOwnership)) {
    return false;
  }
  const auto &ownership = *record.beforeOwnership;
  return record.beforeGeneration == ownership.generation &&
         record.beforeNonce == ownership.activationNonce &&
         record.beforeDigest == ownership.entrypointDigest &&
         record.beforeSize == ownership.entrypointSize &&
         record.beforeDevice == ownership.entrypointDevice &&
         record.beforeInode == ownership.entrypointInode;
}

[[nodiscard]] QByteArray
bridgeBytesUnchecked(const LegacyLiveActivationBridgeRecordV1 &record) {
  const auto ownershipBytes =
      record.beforeOwnership ? ownershipBytesUnchecked(*record.beforeOwnership)
                             : QByteArray{};
  auto bytes = Hyprland::JsonSupport::canonicalJson(QJsonObject{
      {QStringLiteral("formatVersion"),
       static_cast<qint64>(legacyEntrypointRecordV1FormatVersion)},
      {QStringLiteral("phase"), bridgePhaseName(record.phase)},
      {QStringLiteral("token"), record.token},
      {QStringLiteral("adoption"), record.adoption},
      {QStringLiteral("targetGeneration"), record.targetGeneration},
      {QStringLiteral("targetNonce"), record.targetNonce},
      {QStringLiteral("targetDigest"), record.targetDigest},
      {QStringLiteral("targetSize"), decimal(record.targetSize)},
      {QStringLiteral("targetDevice"), decimal(record.targetDevice)},
      {QStringLiteral("targetInode"), decimal(record.targetInode)},
      {QStringLiteral("swapName"), record.swapName},
      {QStringLiteral("beforeKind"), fileKindName(record.beforeKind)},
      {QStringLiteral("beforeDigest"), record.beforeDigest},
      {QStringLiteral("beforeSize"), decimal(record.beforeSize)},
      {QStringLiteral("beforeMode"), static_cast<qint64>(record.beforeMode)},
      {QStringLiteral("beforeDevice"), decimal(record.beforeDevice)},
      {QStringLiteral("beforeInode"), decimal(record.beforeInode)},
      {QStringLiteral("beforeGeneration"), record.beforeGeneration},
      {QStringLiteral("beforeNonce"), record.beforeNonce},
      {QStringLiteral("beforeOwnership"),
       QString::fromLatin1(ownershipBytes.toBase64())},
      {QStringLiteral("baselineConfigErrors"),
       QString::fromLatin1(record.baselineConfigErrors.toBase64())},
      {QStringLiteral("baselineProvider"), record.baselineProvider},
  });
  bytes.append('\n');
  return bytes;
}

[[nodiscard]] std::optional<LegacyLiveActivationBridgeRecordV1>
parseBridgeObject(const QJsonObject &object) {
  static const QSet<QString> expected{
      QStringLiteral("formatVersion"),
      QStringLiteral("phase"),
      QStringLiteral("token"),
      QStringLiteral("adoption"),
      QStringLiteral("targetGeneration"),
      QStringLiteral("targetNonce"),
      QStringLiteral("targetDigest"),
      QStringLiteral("targetSize"),
      QStringLiteral("targetDevice"),
      QStringLiteral("targetInode"),
      QStringLiteral("swapName"),
      QStringLiteral("beforeKind"),
      QStringLiteral("beforeDigest"),
      QStringLiteral("beforeSize"),
      QStringLiteral("beforeMode"),
      QStringLiteral("beforeDevice"),
      QStringLiteral("beforeInode"),
      QStringLiteral("beforeGeneration"),
      QStringLiteral("beforeNonce"),
      QStringLiteral("beforeOwnership"),
      QStringLiteral("baselineConfigErrors"),
      QStringLiteral("baselineProvider"),
  };
  if (keysOf(object) != expected ||
      object.value(QStringLiteral("formatVersion")).toInt(-1) !=
          static_cast<int>(legacyEntrypointRecordV1FormatVersion) ||
      !object.value(QStringLiteral("adoption")).isBool()) {
    return std::nullopt;
  }

  const auto phase =
      bridgePhaseFromName(object.value(QStringLiteral("phase")).toString());
  const auto beforeKind =
      fileKindFromName(object.value(QStringLiteral("beforeKind")).toString());
  const auto beforeMode = object.value(QStringLiteral("beforeMode"));
  quint64 targetSize = 0;
  quint64 targetDevice = 0;
  quint64 targetInode = 0;
  quint64 beforeSize = 0;
  quint64 beforeDevice = 0;
  quint64 beforeInode = 0;
  if (!phase || !beforeKind || !beforeMode.isDouble() ||
      beforeMode.toInteger(-1) < 0 || beforeMode.toInteger(-1) > 0777 ||
      !parseUnsigned(object.value(QStringLiteral("targetSize")).toString(),
                     targetSize) ||
      !parseUnsigned(object.value(QStringLiteral("targetDevice")).toString(),
                     targetDevice) ||
      !parseUnsigned(object.value(QStringLiteral("targetInode")).toString(),
                     targetInode) ||
      !parseUnsigned(object.value(QStringLiteral("beforeSize")).toString(),
                     beforeSize) ||
      !parseUnsigned(object.value(QStringLiteral("beforeDevice")).toString(),
                     beforeDevice) ||
      !parseUnsigned(object.value(QStringLiteral("beforeInode")).toString(),
                     beforeInode)) {
    return std::nullopt;
  }

  const auto encodedOwnership =
      object.value(QStringLiteral("beforeOwnership")).toString().toLatin1();
  const auto ownershipBytes = QByteArray::fromBase64(
      encodedOwnership, QByteArray::AbortOnBase64DecodingErrors);
  const auto encodedErrors =
      object.value(QStringLiteral("baselineConfigErrors"))
          .toString()
          .toLatin1();
  const auto baselineConfigErrors = QByteArray::fromBase64(
      encodedErrors, QByteArray::AbortOnBase64DecodingErrors);
  if (ownershipBytes.toBase64() != encodedOwnership ||
      baselineConfigErrors.toBase64() != encodedErrors) {
    return std::nullopt;
  }

  std::optional<LegacyEntrypointOwnershipRecordV1> beforeOwnership;
  if (!ownershipBytes.isEmpty()) {
    beforeOwnership =
        parseLegacyEntrypointOwnershipRecordV1(QByteArrayView(ownershipBytes));
    if (!beforeOwnership) {
      return std::nullopt;
    }
  }

  LegacyLiveActivationBridgeRecordV1 record{
      .phase = *phase,
      .token = object.value(QStringLiteral("token")).toString(),
      .adoption = object.value(QStringLiteral("adoption")).toBool(),
      .targetGeneration =
          object.value(QStringLiteral("targetGeneration")).toString(),
      .targetNonce = object.value(QStringLiteral("targetNonce")).toString(),
      .targetDigest = object.value(QStringLiteral("targetDigest")).toString(),
      .targetSize = targetSize,
      .targetDevice = targetDevice,
      .targetInode = targetInode,
      .swapName = object.value(QStringLiteral("swapName")).toString(),
      .beforeKind = *beforeKind,
      .beforeDigest = object.value(QStringLiteral("beforeDigest")).toString(),
      .beforeSize = beforeSize,
      .beforeMode = static_cast<quint32>(beforeMode.toInteger()),
      .beforeDevice = beforeDevice,
      .beforeInode = beforeInode,
      .beforeGeneration =
          object.value(QStringLiteral("beforeGeneration")).toString(),
      .beforeNonce = object.value(QStringLiteral("beforeNonce")).toString(),
      .beforeOwnership = std::move(beforeOwnership),
      .baselineConfigErrors = baselineConfigErrors,
      .baselineProvider =
          object.value(QStringLiteral("baselineProvider")).toString(),
  };
  return validBridge(record)
             ? std::optional<LegacyLiveActivationBridgeRecordV1>(
                   std::move(record))
             : std::nullopt;
}

} // namespace

std::optional<QByteArray> serializeLegacyEntrypointOwnershipRecordV1(
    const LegacyEntrypointOwnershipRecordV1 &record) {
  if (!validOwnership(record)) {
    return std::nullopt;
  }
  auto bytes = ownershipBytesUnchecked(record);
  if (bytes.size() > maximumLegacyEntrypointRecordV1Bytes) {
    return std::nullopt;
  }
  const auto reparsed =
      parseLegacyEntrypointOwnershipRecordV1(QByteArrayView(bytes));
  return reparsed && *reparsed == record
             ? std::optional<QByteArray>(std::move(bytes))
             : std::nullopt;
}

std::optional<LegacyEntrypointOwnershipRecordV1>
parseLegacyEntrypointOwnershipRecordV1(const QByteArrayView bytes) {
  const auto parsed = Hyprland::JsonSupport::parseStrictObject(
      bytes, maximumLegacyEntrypointRecordV1Bytes,
      maximumLegacyEntrypointRecordV1Depth);
  if (!parsed) {
    return std::nullopt;
  }
  const auto record = parseOwnershipObject(*parsed.value);
  if (!record || QByteArrayView(ownershipBytesUnchecked(*record)) != bytes) {
    return std::nullopt;
  }
  return record;
}

std::optional<QByteArray> serializeLegacyLiveActivationBridgeRecordV1(
    const LegacyLiveActivationBridgeRecordV1 &record) {
  if (!validBridge(record)) {
    return std::nullopt;
  }
  auto bytes = bridgeBytesUnchecked(record);
  if (bytes.size() > maximumLegacyEntrypointRecordV1Bytes) {
    return std::nullopt;
  }
  const auto reparsed =
      parseLegacyLiveActivationBridgeRecordV1(QByteArrayView(bytes));
  return reparsed && *reparsed == record
             ? std::optional<QByteArray>(std::move(bytes))
             : std::nullopt;
}

std::optional<LegacyLiveActivationBridgeRecordV1>
parseLegacyLiveActivationBridgeRecordV1(const QByteArrayView bytes) {
  const auto parsed = Hyprland::JsonSupport::parseStrictObject(
      bytes, maximumLegacyEntrypointRecordV1Bytes,
      maximumLegacyEntrypointRecordV1Depth);
  if (!parsed) {
    return std::nullopt;
  }
  const auto record = parseBridgeObject(*parsed.value);
  if (!record || QByteArrayView(bridgeBytesUnchecked(*record)) != bytes) {
    return std::nullopt;
  }
  return record;
}

} // namespace HyprShelld::Compositor
