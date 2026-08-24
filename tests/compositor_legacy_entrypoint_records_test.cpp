#include "compositord/legacy_entrypoint_records.h"

#include "hyprland/json_support.h"

#include <QCryptographicHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QtTest>

#include <array>
#include <cstring>
#include <functional>
#include <utility>

using namespace HyprShelld::Compositor;

namespace {

[[nodiscard]] QString repeated(const QChar character, const qsizetype count) {
  return QString(count, character);
}

[[nodiscard]] QString hash(const char character) {
  return repeated(QLatin1Char(character), 64);
}

[[nodiscard]] QString nonce(const char character) {
  return repeated(QLatin1Char(character), 32);
}

[[nodiscard]] QString digest(const QByteArrayView bytes) {
  return QString::fromLatin1(
      QCryptographicHash::hash(bytes, QCryptographicHash::Sha256).toHex());
}

[[nodiscard]] QByteArray canonical(const QJsonObject &object) {
  auto bytes = HyprShelld::Hyprland::JsonSupport::canonicalJson(object);
  bytes.append('\n');
  return bytes;
}

[[nodiscard]] QJsonObject objectFrom(const QByteArray &bytes) {
  QJsonParseError error;
  const auto document = QJsonDocument::fromJson(bytes, &error);
  return error.error == QJsonParseError::NoError && document.isObject()
             ? document.object()
             : QJsonObject{};
}

[[nodiscard]] LegacyEntrypointOwnershipRecordV1 absentOwnership() {
  return {
      .generation = hash('a'),
      .activationNonce = QStringLiteral("0123456789abcdef0123456789abcdef"),
      .entrypointDigest = hash('b'),
      .entrypointSize = 123,
      .entrypointDevice = 456,
      .entrypointInode = 789,
      .original = {},
  };
}

[[nodiscard]] LegacyEntrypointOwnershipRecordV1 regularOwnership() {
  return {
      .generation = hash('c'),
      .activationNonce = nonce('1'),
      .entrypointDigest = hash('d'),
      .entrypointSize = 4567,
      .entrypointDevice = 987654321,
      .entrypointInode = 123456789,
      .original =
          {
              .kind = LegacyEntrypointFileKindV1::Regular,
              .digest = hash('e'),
              .size = 901,
              .mode = 0640,
              .device = 88,
              .inode = 99,
              .backupName = QStringLiteral(
                  ".hyprshelld-original-22222222222222222222222222222222.lua"),
          },
  };
}

[[nodiscard]] LegacyLiveActivationBridgeRecordV1 adoptionAbsentStaging() {
  return {
      .phase = LegacyLiveActivationBridgePhaseV1::Staging,
      .token = nonce('3'),
      .adoption = true,
      .targetGeneration = hash('a'),
      .targetNonce = nonce('4'),
      .targetDigest = hash('b'),
      .targetSize = 123,
      .targetDevice = 0,
      .targetInode = 0,
      .swapName = QStringLiteral(
          ".hyprshelld-original-33333333333333333333333333333333.lua"),
      .beforeKind = LegacyEntrypointFileKindV1::Absent,
      .beforeDigest = {},
      .beforeGeneration = {},
      .beforeNonce = {},
      .beforeOwnership = std::nullopt,
      .baselineConfigErrors = QByteArrayLiteral("[]"),
      .baselineProvider = QStringLiteral("hyprlang"),
  };
}

[[nodiscard]] LegacyLiveActivationBridgeRecordV1 adoptionRegularReady() {
  return {
      .phase = LegacyLiveActivationBridgePhaseV1::Ready,
      .token = nonce('5'),
      .adoption = true,
      .targetGeneration = hash('c'),
      .targetNonce = nonce('6'),
      .targetDigest = hash('d'),
      .targetSize = 456,
      .targetDevice = 77,
      .targetInode = 88,
      .swapName = QStringLiteral(
          ".hyprshelld-original-55555555555555555555555555555555.lua"),
      .beforeKind = LegacyEntrypointFileKindV1::Regular,
      .beforeDigest = hash('e'),
      .beforeSize = 90,
      .beforeMode = 0600,
      .beforeDevice = 11,
      .beforeInode = 22,
      .beforeGeneration = {},
      .beforeNonce = {},
      .beforeOwnership = std::nullopt,
      .baselineConfigErrors = QByteArrayLiteral("[]"),
      .baselineProvider = QStringLiteral("lua"),
  };
}

[[nodiscard]] LegacyLiveActivationBridgeRecordV1 managedStaging() {
  const auto ownership = regularOwnership();
  return {
      .phase = LegacyLiveActivationBridgePhaseV1::Staging,
      .token = nonce('7'),
      .adoption = false,
      .targetGeneration = hash('f'),
      .targetNonce = nonce('8'),
      .targetDigest = hash('9'),
      .targetSize = 789,
      .targetDevice = 0,
      .targetInode = 0,
      .swapName = QStringLiteral(
          ".hyprshelld-transition-77777777777777777777777777777777.lua"),
      .beforeKind = LegacyEntrypointFileKindV1::Regular,
      .beforeDigest = ownership.entrypointDigest,
      .beforeSize = ownership.entrypointSize,
      .beforeMode = 0600,
      .beforeDevice = ownership.entrypointDevice,
      .beforeInode = ownership.entrypointInode,
      .beforeGeneration = ownership.generation,
      .beforeNonce = ownership.activationNonce,
      .beforeOwnership = ownership,
      .baselineConfigErrors = QByteArrayLiteral("[]"),
      .baselineProvider = QStringLiteral("lua"),
  };
}

[[nodiscard]] LegacyLiveActivationBridgeRecordV1 managedReady() {
  auto record = managedStaging();
  record.phase = LegacyLiveActivationBridgePhaseV1::Ready;
  record.token = nonce('a');
  record.targetGeneration = hash('b');
  record.targetNonce = nonce('b');
  record.targetDigest = hash('c');
  record.targetSize = 987;
  record.targetDevice = 44;
  record.targetInode = 55;
  record.swapName = QStringLiteral(
      ".hyprshelld-transition-aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa.lua");
  return record;
}

[[nodiscard]] QByteArray ownershipAbsentGolden() {
  return QByteArrayLiteral(
      R"JSON({"activationNonce":"0123456789abcdef0123456789abcdef","entrypointDevice":"456","entrypointDigest":"bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb","entrypointInode":"789","entrypointSize":"123","formatVersion":1,"generation":"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa","originalBackup":"","originalDevice":"0","originalDigest":"","originalInode":"0","originalKind":"absent","originalMode":0,"originalSize":"0"}
)JSON");
}

[[nodiscard]] QByteArray ownershipRegularGolden() {
  return QByteArrayLiteral(
      R"JSON({"activationNonce":"11111111111111111111111111111111","entrypointDevice":"987654321","entrypointDigest":"dddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddd","entrypointInode":"123456789","entrypointSize":"4567","formatVersion":1,"generation":"cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc","originalBackup":".hyprshelld-original-22222222222222222222222222222222.lua","originalDevice":"88","originalDigest":"eeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeee","originalInode":"99","originalKind":"regular","originalMode":416,"originalSize":"901"}
)JSON");
}

[[nodiscard]] QByteArray adoptionAbsentStagingGolden() {
  return QByteArrayLiteral(
      R"JSON({"adoption":true,"baselineConfigErrors":"W10=","baselineProvider":"hyprlang","beforeDevice":"0","beforeDigest":"","beforeGeneration":"","beforeInode":"0","beforeKind":"absent","beforeMode":0,"beforeNonce":"","beforeOwnership":"","beforeSize":"0","formatVersion":1,"phase":"staging","swapName":".hyprshelld-original-33333333333333333333333333333333.lua","targetDevice":"0","targetDigest":"bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb","targetGeneration":"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa","targetInode":"0","targetNonce":"44444444444444444444444444444444","targetSize":"123","token":"33333333333333333333333333333333"}
)JSON");
}

[[nodiscard]] QByteArray adoptionRegularReadyGolden() {
  return QByteArrayLiteral(
      R"JSON({"adoption":true,"baselineConfigErrors":"W10=","baselineProvider":"lua","beforeDevice":"11","beforeDigest":"eeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeee","beforeGeneration":"","beforeInode":"22","beforeKind":"regular","beforeMode":384,"beforeNonce":"","beforeOwnership":"","beforeSize":"90","formatVersion":1,"phase":"ready","swapName":".hyprshelld-original-55555555555555555555555555555555.lua","targetDevice":"77","targetDigest":"dddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddd","targetGeneration":"cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc","targetInode":"88","targetNonce":"66666666666666666666666666666666","targetSize":"456","token":"55555555555555555555555555555555"}
)JSON");
}

constexpr char regularOwnershipBase64[] =
    "eyJhY3RpdmF0aW9uTm9uY2UiOiIxMTExMTExMTExMTExMTExMTExMTExMTExMTExMTExMSIsIm"
    "VudHJ5"
    "cG9pbnREZXZpY2UiOiI5ODc2NTQzMjEiLCJlbnRyeXBvaW50RGlnZXN0IjoiZGRkZGRkZGRkZG"
    "RkZGRk"
    "ZGRkZGRkZGRkZGRkZGRkZGRkZGRkZGRkZGRkZGRkZGRkZGRkZGRkZGRkZGRkZGRkZCIsImVudH"
    "J5cG9p"
    "bnRJbm9kZSI6IjEyMzQ1Njc4OSIsImVudHJ5cG9pbnRTaXplIjoiNDU2NyIsImZvcm1hdFZlcn"
    "Npb24i"
    "OjEsImdlbmVyYXRpb24iOiJjY2NjY2NjY2NjY2NjY2NjY2NjY2NjY2NjY2NjY2NjY2NjY2NjY2"
    "NjY2Nj"
    "Y2NjY2NjY2NjY2NjY2NjY2NjY2NjIiwib3JpZ2luYWxCYWNrdXAiOiIuaHlwcnNoZWxsZC1vcm"
    "lnaW5h"
    "bC0yMjIyMjIyMjIyMjIyMjIyMjIyMjIyMjIyMjIyMjIyMi5sdWEiLCJvcmlnaW5hbERldmljZS"
    "I6Ijg4"
    "Iiwib3JpZ2luYWxEaWdlc3QiOiJlZWVlZWVlZWVlZWVlZWVlZWVlZWVlZWVlZWVlZWVlZWVlZW"
    "VlZWVl"
    "ZWVlZWVlZWVlZWVlZWVlZWVlZWVlZWVlIiwib3JpZ2luYWxJbm9kZSI6Ijk5Iiwib3JpZ2luYW"
    "xLaW5k"
    "IjoicmVndWxhciIsIm9yaWdpbmFsTW9kZSI6NDE2LCJvcmlnaW5hbFNpemUiOiI5MDEifQo=";

[[nodiscard]] QByteArray managedStagingGolden() {
  return QByteArrayLiteral(
             "{\"adoption\":false,\"baselineConfigErrors\":\"W10=\","
             "\"baselineProvider\":\"lua\",\"beforeDevice\":\"987654321\","
             "\"beforeDigest\":"
             "\"ddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddd"
             "d\","
             "\"beforeGeneration\":"
             "\"ccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc"
             "c\","
             "\"beforeInode\":\"123456789\",\"beforeKind\":\"regular\","
             "\"beforeMode\":384,\"beforeNonce\":"
             "\"11111111111111111111111111111111\","
             "\"beforeOwnership\":\"") +
         QByteArrayLiteral(regularOwnershipBase64) +
         QByteArrayLiteral("\",\"beforeSize\":\"4567\",\"formatVersion\":1,"
                           "\"phase\":\"staging\","
                           "\"swapName\":\".hyprshelld-transition-"
                           "77777777777777777777777777777777.lua\","
                           "\"targetDevice\":\"0\","
                           "\"targetDigest\":"
                           "\"9999999999999999999999999999999999999999999999999"
                           "999999999999999\","
                           "\"targetGeneration\":"
                           "\"fffffffffffffffffffffffffffffffffffffffffffffffff"
                           "fffffffffffffff\","
                           "\"targetInode\":\"0\",\"targetNonce\":"
                           "\"88888888888888888888888888888888\","
                           "\"targetSize\":\"789\",\"token\":"
                           "\"77777777777777777777777777777777\"}\n");
}

[[nodiscard]] QByteArray managedReadyGolden() {
  return QByteArrayLiteral(
             "{\"adoption\":false,\"baselineConfigErrors\":\"W10=\","
             "\"baselineProvider\":\"lua\",\"beforeDevice\":\"987654321\","
             "\"beforeDigest\":"
             "\"ddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddd"
             "d\","
             "\"beforeGeneration\":"
             "\"ccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc"
             "c\","
             "\"beforeInode\":\"123456789\",\"beforeKind\":\"regular\","
             "\"beforeMode\":384,\"beforeNonce\":"
             "\"11111111111111111111111111111111\","
             "\"beforeOwnership\":\"") +
         QByteArrayLiteral(regularOwnershipBase64) +
         QByteArrayLiteral("\",\"beforeSize\":\"4567\",\"formatVersion\":1,"
                           "\"phase\":\"ready\","
                           "\"swapName\":\".hyprshelld-transition-"
                           "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa.lua\","
                           "\"targetDevice\":\"44\","
                           "\"targetDigest\":"
                           "\"ccccccccccccccccccccccccccccccccccccccccccccccccc"
                           "ccccccccccccccc\","
                           "\"targetGeneration\":"
                           "\"bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"
                           "bbbbbbbbbbbbbbb\","
                           "\"targetInode\":\"55\",\"targetNonce\":"
                           "\"bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb\","
                           "\"targetSize\":\"987\",\"token\":"
                           "\"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\"}\n");
}

[[nodiscard]] QByteArray goldenBytes(const int index) {
  switch (index) {
  case 0:
    return ownershipAbsentGolden();
  case 1:
    return ownershipRegularGolden();
  case 2:
    return adoptionAbsentStagingGolden();
  case 3:
    return adoptionRegularReadyGolden();
  case 4:
    return managedStagingGolden();
  case 5:
    return managedReadyGolden();
  }
  return {};
}

[[nodiscard]] std::optional<QByteArray> serializedGolden(const int index) {
  switch (index) {
  case 0:
    return serializeLegacyEntrypointOwnershipRecordV1(absentOwnership());
  case 1:
    return serializeLegacyEntrypointOwnershipRecordV1(regularOwnership());
  case 2:
    return serializeLegacyLiveActivationBridgeRecordV1(adoptionAbsentStaging());
  case 3:
    return serializeLegacyLiveActivationBridgeRecordV1(adoptionRegularReady());
  case 4:
    return serializeLegacyLiveActivationBridgeRecordV1(managedStaging());
  case 5:
    return serializeLegacyLiveActivationBridgeRecordV1(managedReady());
  }
  return std::nullopt;
}

[[nodiscard]] bool ownershipRejected(const QByteArray &bytes) {
  return !parseLegacyEntrypointOwnershipRecordV1(QByteArrayView(bytes));
}

[[nodiscard]] bool bridgeRejected(const QByteArray &bytes) {
  return !parseLegacyLiveActivationBridgeRecordV1(QByteArrayView(bytes));
}

using ObjectMutation = std::pair<QString, std::function<void(QJsonObject &)>>;

} // namespace

class CompositorLegacyEntrypointRecordsTest final : public QObject {
  Q_OBJECT

private slots:
  void sixExactGoldens_data() {
    QTest::addColumn<int>("index");
    QTest::addColumn<qsizetype>("size");
    QTest::addColumn<QString>("sha256");
    QTest::newRow("ownership-original-absent")
        << 0 << qsizetype(452)
        << QStringLiteral("fef30d7b89ffcbef5c0b3655f29d1a096c2bf92a16b6e1a60440"
                          "7ef59be8f6a3");
    QTest::newRow("ownership-original-regular")
        << 1 << qsizetype(593)
        << QStringLiteral("80fb621864b81f7e346cef25c5f7f795f1235c7509d59adae0a6"
                          "d43633ccde4d");
    QTest::newRow("bridge-adoption-absent-staging")
        << 2 << qsizetype(670)
        << QStringLiteral("a11d28946a21fd3876231a639970f682ad5833b8af80ff0b66c2"
                          "28647703ae08");
    QTest::newRow("bridge-adoption-regular-ready")
        << 3 << qsizetype(735)
        << QStringLiteral("89399e09d0b7d875a0e5e506609430e20784ed1736237e9e3d55"
                          "193f32f22b25");
    QTest::newRow("bridge-managed-update-staging")
        << 4 << qsizetype(1642)
        << QStringLiteral("1a3084903a5b98c0ef9d5d919e028483ff9ea3fe304b18e2e544"
                          "b2f466349701");
    QTest::newRow("bridge-managed-update-ready")
        << 5 << qsizetype(1642)
        << QStringLiteral("1970cb93f0fd03416b927bbd83f7fc451d8a3b4f5d615f50a6b6"
                          "0ac01f09d5a6");
  }

  void sixExactGoldens() {
    QFETCH(int, index);
    QFETCH(qsizetype, size);
    QFETCH(QString, sha256);
    const auto expected = goldenBytes(index);
    const auto encoded = serializedGolden(index);
    QVERIFY(encoded);
    QCOMPARE(*encoded, expected);
    QCOMPARE(encoded->size(), size);
    QCOMPARE(digest(*encoded), sha256);

    if (index == 0) {
      const auto parsed =
          parseLegacyEntrypointOwnershipRecordV1(QByteArrayView(*encoded));
      QVERIFY(parsed && *parsed == absentOwnership());
    } else if (index == 1) {
      const auto parsed =
          parseLegacyEntrypointOwnershipRecordV1(QByteArrayView(*encoded));
      QVERIFY(parsed && *parsed == regularOwnership());
    } else {
      const auto parsed =
          parseLegacyLiveActivationBridgeRecordV1(QByteArrayView(*encoded));
      QVERIFY(parsed);
      const auto expectedRecord = index == 2   ? adoptionAbsentStaging()
                                  : index == 3 ? adoptionRegularReady()
                                  : index == 4 ? managedStaging()
                                               : managedReady();
      QVERIFY(*parsed == expectedRecord);
    }
  }

  void everyOwnershipKeyAndTypeIsClosed() {
    const auto original = objectFrom(ownershipRegularGolden());
    QVERIFY(!original.isEmpty());
    for (auto iterator = original.constBegin(); iterator != original.constEnd();
         ++iterator) {
      auto missing = original;
      missing.remove(iterator.key());
      QVERIFY2(ownershipRejected(canonical(missing)),
               qPrintable(QStringLiteral("missing %1").arg(iterator.key())));

      auto wrongType = original;
      const auto replacement = iterator.value().isDouble()
                                   ? QJsonValue(QStringLiteral("1"))
                                   : QJsonValue(QJsonArray{});
      wrongType.insert(iterator.key(), replacement);
      QVERIFY2(ownershipRejected(canonical(wrongType)),
               qPrintable(QStringLiteral("type %1").arg(iterator.key())));
    }
    auto unknown = original;
    unknown.insert(QStringLiteral("unknown"), true);
    QVERIFY(ownershipRejected(canonical(unknown)));
  }

  void everyBridgeKeyAndTypeIsClosed() {
    const auto original = objectFrom(managedReadyGolden());
    QVERIFY(!original.isEmpty());
    for (auto iterator = original.constBegin(); iterator != original.constEnd();
         ++iterator) {
      auto missing = original;
      missing.remove(iterator.key());
      QVERIFY2(bridgeRejected(canonical(missing)),
               qPrintable(QStringLiteral("missing %1").arg(iterator.key())));

      auto wrongType = original;
      QJsonValue replacement = QJsonArray{};
      if (iterator.value().isDouble()) {
        replacement = QStringLiteral("1");
      } else if (iterator.value().isBool()) {
        replacement = QStringLiteral("false");
      }
      wrongType.insert(iterator.key(), replacement);
      QVERIFY2(bridgeRejected(canonical(wrongType)),
               qPrintable(QStringLiteral("type %1").arg(iterator.key())));
    }
    auto unknown = original;
    unknown.insert(QStringLiteral("unknown"), true);
    QVERIFY(bridgeRejected(canonical(unknown)));
  }

  void historicalParserOnlyEmptyCoercionsAreRepairOnly() {
    const std::array<QJsonValue, 5> wrongTypes{
        QJsonValue::Null,
        QJsonValue(false),
        QJsonValue(0),
        QJsonValue(QJsonArray{}),
        QJsonValue(QJsonObject{}),
    };

    const auto absentOwnershipObject = objectFrom(ownershipAbsentGolden());
    for (const auto &field : {
             QStringLiteral("originalDigest"),
             QStringLiteral("originalBackup"),
         }) {
      for (const auto &wrongType : wrongTypes) {
        auto candidate = absentOwnershipObject;
        candidate.insert(field, wrongType);
        QVERIFY2(ownershipRejected(canonical(candidate)), qPrintable(field));
      }
    }

    const auto absentAdoptionObject = objectFrom(
        adoptionAbsentStagingGolden()
    );
    for (const auto &field : {
             QStringLiteral("beforeDigest"),
             QStringLiteral("beforeGeneration"),
             QStringLiteral("beforeNonce"),
             QStringLiteral("beforeOwnership"),
         }) {
      for (const auto &wrongType : wrongTypes) {
        auto candidate = absentAdoptionObject;
        candidate.insert(field, wrongType);
        QVERIFY2(bridgeRejected(canonical(candidate)), qPrintable(field));
      }
    }

    auto managed = managedReady();
    const auto ownership = absentOwnership();
    managed.beforeGeneration = ownership.generation;
    managed.beforeNonce = ownership.activationNonce;
    managed.beforeDigest = ownership.entrypointDigest;
    managed.beforeSize = ownership.entrypointSize;
    managed.beforeDevice = ownership.entrypointDevice;
    managed.beforeInode = ownership.entrypointInode;
    managed.beforeOwnership = ownership;
    const auto managedBytes = serializeLegacyLiveActivationBridgeRecordV1(
        managed
    );
    QVERIFY(managedBytes);
    const auto managedObject = objectFrom(*managedBytes);
    QVERIFY(!managedObject.isEmpty());

    for (const auto &field : {
             QStringLiteral("originalDigest"),
             QStringLiteral("originalBackup"),
         }) {
      for (const auto &wrongType : wrongTypes) {
        auto nested = absentOwnershipObject;
        nested.insert(field, wrongType);
        auto candidate = managedObject;
        candidate.insert(
            QStringLiteral("beforeOwnership"),
            QString::fromLatin1(canonical(nested).toBase64())
        );
        QVERIFY2(bridgeRejected(canonical(candidate)), qPrintable(field));
      }
    }
  }

  void ownershipFieldConstraintsAreExact() {
    const auto regular = objectFrom(ownershipRegularGolden());
    const auto absent = objectFrom(ownershipAbsentGolden());
    QVERIFY(!regular.isEmpty() && !absent.isEmpty());

    const QVector<ObjectMutation> mutations{
        {QStringLiteral("version"),
         [](QJsonObject &o) { o.insert(QStringLiteral("formatVersion"), 2); }},
        {QStringLiteral("generation-short"),
         [](QJsonObject &o) {
           o.insert(QStringLiteral("generation"), hash('a').chopped(1));
         }},
        {QStringLiteral("generation-uppercase"),
         [](QJsonObject &o) {
           o.insert(QStringLiteral("generation"), hash('A'));
         }},
        {QStringLiteral("generation-nonhex"),
         [](QJsonObject &o) {
           o.insert(QStringLiteral("generation"), hash('z'));
         }},
        {QStringLiteral("nonce-short"),
         [](QJsonObject &o) {
           o.insert(QStringLiteral("activationNonce"), nonce('1').chopped(1));
         }},
        {QStringLiteral("nonce-long"),
         [](QJsonObject &o) {
           o.insert(QStringLiteral("activationNonce"), repeated(u'1', 129));
         }},
        {QStringLiteral("nonce-uppercase"),
         [](QJsonObject &o) {
           o.insert(QStringLiteral("activationNonce"), nonce('A'));
         }},
        {QStringLiteral("entrypoint-digest"),
         [](QJsonObject &o) {
           o.insert(QStringLiteral("entrypointDigest"), hash('z'));
         }},
        {QStringLiteral("entrypoint-size-leading-zero"),
         [](QJsonObject &o) {
           o.insert(QStringLiteral("entrypointSize"), QStringLiteral("01"));
         }},
        {QStringLiteral("entrypoint-size-bound"),
         [](QJsonObject &o) {
           o.insert(QStringLiteral("entrypointSize"),
                    QString::number(maximumLegacyEntrypointPayloadV1Bytes + 1));
         }},
        {QStringLiteral("entrypoint-device-zero"),
         [](QJsonObject &o) {
           o.insert(QStringLiteral("entrypointDevice"), QStringLiteral("0"));
         }},
        {QStringLiteral("entrypoint-device-overflow"),
         [](QJsonObject &o) {
           o.insert(QStringLiteral("entrypointDevice"),
                    QStringLiteral("18446744073709551616"));
         }},
        {QStringLiteral("entrypoint-inode-zero"),
         [](QJsonObject &o) {
           o.insert(QStringLiteral("entrypointInode"), QStringLiteral("0"));
         }},
        {QStringLiteral("original-kind"),
         [](QJsonObject &o) {
           o.insert(QStringLiteral("originalKind"), QStringLiteral("unsafe"));
         }},
        {QStringLiteral("original-digest"),
         [](QJsonObject &o) {
           o.insert(QStringLiteral("originalDigest"), hash('A'));
         }},
        {QStringLiteral("original-size-bound"),
         [](QJsonObject &o) {
           o.insert(QStringLiteral("originalSize"),
                    QString::number(maximumLegacyEntrypointPayloadV1Bytes + 1));
         }},
        {QStringLiteral("original-mode-group-write"),
         [](QJsonObject &o) {
           o.insert(QStringLiteral("originalMode"), 0660);
         }},
        {QStringLiteral("original-mode-range"),
         [](QJsonObject &o) {
           o.insert(QStringLiteral("originalMode"), 01000);
         }},
        {QStringLiteral("original-device-zero"),
         [](QJsonObject &o) {
           o.insert(QStringLiteral("originalDevice"), QStringLiteral("0"));
         }},
        {QStringLiteral("original-inode-zero"),
         [](QJsonObject &o) {
           o.insert(QStringLiteral("originalInode"), QStringLiteral("0"));
         }},
        {QStringLiteral("original-backup-prefix"),
         [](QJsonObject &o) {
           o.insert(
               QStringLiteral("originalBackup"),
               QStringLiteral("original-22222222222222222222222222222222.lua"));
         }},
        {QStringLiteral("original-backup-nonce"),
         [](QJsonObject &o) {
           o.insert(QStringLiteral("originalBackup"),
                    QStringLiteral(".hyprshelld-original-"
                                   "zzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzz.lua"));
         }},
        {QStringLiteral("original-backup-suffix"),
         [](QJsonObject &o) {
           o.insert(QStringLiteral("originalBackup"),
                    QStringLiteral(".hyprshelld-original-"
                                   "22222222222222222222222222222222.conf"));
         }},
    };
    for (const auto &[label, mutation] : mutations) {
      auto candidate = regular;
      mutation(candidate);
      QVERIFY2(ownershipRejected(canonical(candidate)), qPrintable(label));
    }

    const QStringList decimalFields{
        QStringLiteral("entrypointSize"),  QStringLiteral("entrypointDevice"),
        QStringLiteral("entrypointInode"), QStringLiteral("originalSize"),
        QStringLiteral("originalDevice"),  QStringLiteral("originalInode"),
    };
    for (const auto &field : decimalFields) {
      auto candidate = regular;
      candidate.insert(field, QStringLiteral("00"));
      QVERIFY2(ownershipRejected(canonical(candidate)),
               qPrintable(QStringLiteral("decimal %1").arg(field)));
    }

    const QStringList absentFields{
        QStringLiteral("originalDigest"),
        QStringLiteral("originalBackup"),
    };
    for (const auto &field : absentFields) {
      auto candidate = absent;
      candidate.insert(field, QStringLiteral("x"));
      QVERIFY2(ownershipRejected(canonical(candidate)), qPrintable(field));
    }
    const QStringList absentDecimals{
        QStringLiteral("originalSize"),
        QStringLiteral("originalDevice"),
        QStringLiteral("originalInode"),
    };
    for (const auto &field : absentDecimals) {
      auto candidate = absent;
      candidate.insert(field, QStringLiteral("1"));
      QVERIFY2(ownershipRejected(canonical(candidate)), qPrintable(field));
    }
    auto absentMode = absent;
    absentMode.insert(QStringLiteral("originalMode"), 0400);
    QVERIFY(ownershipRejected(canonical(absentMode)));
  }

  void bridgeFieldConstraintsAreExact() {
    const auto ready = objectFrom(managedReadyGolden());
    QVERIFY(!ready.isEmpty());
    const QVector<ObjectMutation> mutations{
        {QStringLiteral("version"),
         [](QJsonObject &o) { o.insert(QStringLiteral("formatVersion"), 2); }},
        {QStringLiteral("phase"),
         [](QJsonObject &o) {
           o.insert(QStringLiteral("phase"), QStringLiteral("committing"));
         }},
        {QStringLiteral("token-short"),
         [](QJsonObject &o) {
           o.insert(QStringLiteral("token"), nonce('a').chopped(1));
         }},
        {QStringLiteral("token-uppercase"),
         [](QJsonObject &o) { o.insert(QStringLiteral("token"), nonce('A')); }},
        {QStringLiteral("target-generation"),
         [](QJsonObject &o) {
           o.insert(QStringLiteral("targetGeneration"), hash('z'));
         }},
        {QStringLiteral("target-nonce"),
         [](QJsonObject &o) {
           o.insert(QStringLiteral("targetNonce"), nonce('Z'));
         }},
        {QStringLiteral("target-digest"),
         [](QJsonObject &o) {
           o.insert(QStringLiteral("targetDigest"), hash('A'));
         }},
        {QStringLiteral("target-size-leading-zero"),
         [](QJsonObject &o) {
           o.insert(QStringLiteral("targetSize"), QStringLiteral("0987"));
         }},
        {QStringLiteral("target-size-bound"),
         [](QJsonObject &o) {
           o.insert(QStringLiteral("targetSize"),
                    QString::number(maximumLegacyEntrypointPayloadV1Bytes + 1));
         }},
        {QStringLiteral("target-device-overflow"),
         [](QJsonObject &o) {
           o.insert(QStringLiteral("targetDevice"),
                    QStringLiteral("18446744073709551616"));
         }},
        {QStringLiteral("target-inode-zero-ready"),
         [](QJsonObject &o) {
           o.insert(QStringLiteral("targetInode"), QStringLiteral("0"));
         }},
        {QStringLiteral("swap-name-category"),
         [](QJsonObject &o) {
           o.insert(QStringLiteral("swapName"),
                    QStringLiteral(".hyprshelld-original-"
                                   "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa.lua"));
         }},
        {QStringLiteral("swap-name-token"),
         [](QJsonObject &o) {
           o.insert(QStringLiteral("swapName"),
                    QStringLiteral(".hyprshelld-transition-"
                                   "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb.lua"));
         }},
        {QStringLiteral("swap-name-suffix"),
         [](QJsonObject &o) {
           o.insert(QStringLiteral("swapName"),
                    QStringLiteral(".hyprshelld-transition-"
                                   "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa.conf"));
         }},
        {QStringLiteral("before-kind"),
         [](QJsonObject &o) {
           o.insert(QStringLiteral("beforeKind"), QStringLiteral("unsafe"));
         }},
        {QStringLiteral("before-mode-group-write"),
         [](QJsonObject &o) { o.insert(QStringLiteral("beforeMode"), 0666); }},
        {QStringLiteral("before-mode-range"),
         [](QJsonObject &o) { o.insert(QStringLiteral("beforeMode"), 01000); }},
        {QStringLiteral("baseline-errors"),
         [](QJsonObject &o) {
           o.insert(QStringLiteral("baselineConfigErrors"),
                    QStringLiteral("e30="));
         }},
        {QStringLiteral("baseline-provider-enum"),
         [](QJsonObject &o) {
           o.insert(QStringLiteral("baselineProvider"), QStringLiteral("none"));
         }},
    };
    for (const auto &[label, mutation] : mutations) {
      auto candidate = ready;
      mutation(candidate);
      QVERIFY2(bridgeRejected(canonical(candidate)), qPrintable(label));
    }

    const QStringList decimalFields{
        QStringLiteral("targetSize"),   QStringLiteral("targetDevice"),
        QStringLiteral("targetInode"),  QStringLiteral("beforeSize"),
        QStringLiteral("beforeDevice"), QStringLiteral("beforeInode"),
    };
    for (const auto &field : decimalFields) {
      auto candidate = ready;
      candidate.insert(field, QStringLiteral("00"));
      QVERIFY2(bridgeRejected(canonical(candidate)),
               qPrintable(QStringLiteral("decimal %1").arg(field)));
    }

    auto staging = objectFrom(managedStagingGolden());
    staging.insert(QStringLiteral("targetDevice"), QStringLiteral("1"));
    QVERIFY(bridgeRejected(canonical(staging)));
    staging = objectFrom(managedStagingGolden());
    staging.insert(QStringLiteral("targetInode"), QStringLiteral("1"));
    QVERIFY(bridgeRejected(canonical(staging)));

    auto regular = objectFrom(adoptionRegularReadyGolden());
    const QStringList regularZeroFields{
        QStringLiteral("beforeDigest"),
        QStringLiteral("beforeSize"),
        QStringLiteral("beforeDevice"),
        QStringLiteral("beforeInode"),
    };
    for (const auto &field : regularZeroFields) {
      auto candidate = regular;
      candidate.insert(
          field, field == QStringLiteral("beforeDigest") ? QJsonValue(QString{})
                 : field == QStringLiteral("beforeSize")
                     ? QJsonValue(QString::number(
                           maximumLegacyEntrypointPayloadV1Bytes + 1))
                     : QJsonValue(QStringLiteral("0")));
      QVERIFY2(bridgeRejected(canonical(candidate)), qPrintable(field));
    }

    const auto absent = objectFrom(adoptionAbsentStagingGolden());
    const QVector<ObjectMutation> absentMutations{
        {QStringLiteral("before-digest"),
         [](QJsonObject &o) {
           o.insert(QStringLiteral("beforeDigest"), hash('a'));
         }},
        {QStringLiteral("before-size"),
         [](QJsonObject &o) {
           o.insert(QStringLiteral("beforeSize"), QStringLiteral("1"));
         }},
        {QStringLiteral("before-mode"),
         [](QJsonObject &o) { o.insert(QStringLiteral("beforeMode"), 0400); }},
        {QStringLiteral("before-device"),
         [](QJsonObject &o) {
           o.insert(QStringLiteral("beforeDevice"), QStringLiteral("1"));
         }},
        {QStringLiteral("before-inode"),
         [](QJsonObject &o) {
           o.insert(QStringLiteral("beforeInode"), QStringLiteral("1"));
         }},
        {QStringLiteral("absent-provider"),
         [](QJsonObject &o) {
           o.insert(QStringLiteral("baselineProvider"), QStringLiteral("lua"));
         }},
        {QStringLiteral("adoption-before-generation"),
         [](QJsonObject &o) {
           o.insert(QStringLiteral("beforeGeneration"), hash('a'));
         }},
        {QStringLiteral("adoption-before-nonce"),
         [](QJsonObject &o) {
           o.insert(QStringLiteral("beforeNonce"), nonce('1'));
         }},
    };
    for (const auto &[label, mutation] : absentMutations) {
      auto candidate = absent;
      mutation(candidate);
      QVERIFY2(bridgeRejected(canonical(candidate)), qPrintable(label));
    }

    regular.insert(QStringLiteral("baselineProvider"),
                   QStringLiteral("hyprlang"));
    QVERIFY(bridgeRejected(canonical(regular)));

    auto nonAdoptionAbsent = absent;
    nonAdoptionAbsent.insert(QStringLiteral("adoption"), false);
    nonAdoptionAbsent.insert(
        QStringLiteral("swapName"),
        QStringLiteral(
            ".hyprshelld-transition-33333333333333333333333333333333.lua"));
    nonAdoptionAbsent.insert(QStringLiteral("baselineProvider"),
                             QStringLiteral("lua"));
    QVERIFY(bridgeRejected(canonical(nonAdoptionAbsent)));
  }

  void base64AndNestedOwnershipAreExact() {
    const auto managed = objectFrom(managedReadyGolden());
    QVERIFY(!managed.isEmpty());
    const QVector<ObjectMutation> base64Mutations{
        {QStringLiteral("baseline-invalid"),
         [](QJsonObject &o) {
           o.insert(QStringLiteral("baselineConfigErrors"),
                    QStringLiteral("!"));
         }},
        {QStringLiteral("baseline-unpadded"),
         [](QJsonObject &o) {
           o.insert(QStringLiteral("baselineConfigErrors"),
                    QStringLiteral("W10"));
         }},
        {QStringLiteral("ownership-invalid"),
         [](QJsonObject &o) {
           o.insert(QStringLiteral("beforeOwnership"), QStringLiteral("!"));
         }},
        {QStringLiteral("ownership-unpadded"),
         [](QJsonObject &o) {
           auto value = o.value(QStringLiteral("beforeOwnership")).toString();
           value.chop(1);
           o.insert(QStringLiteral("beforeOwnership"), value);
         }},
        {QStringLiteral("ownership-empty"),
         [](QJsonObject &o) {
           o.insert(QStringLiteral("beforeOwnership"), QString{});
         }},
    };
    for (const auto &[label, mutation] : base64Mutations) {
      auto candidate = managed;
      mutation(candidate);
      QVERIFY2(bridgeRejected(canonical(candidate)), qPrintable(label));
    }

    auto nestedNoLf = ownershipRegularGolden();
    nestedNoLf.chop(1);
    auto candidate = managed;
    candidate.insert(QStringLiteral("beforeOwnership"),
                     QString::fromLatin1(nestedNoLf.toBase64()));
    QVERIFY(bridgeRejected(canonical(candidate)));

    auto nestedUnknown = objectFrom(ownershipRegularGolden());
    nestedUnknown.insert(QStringLiteral("unknown"), true);
    candidate = managed;
    candidate.insert(QStringLiteral("beforeOwnership"),
                     QString::fromLatin1(canonical(nestedUnknown).toBase64()));
    QVERIFY(bridgeRejected(canonical(candidate)));

    const auto adoption = objectFrom(adoptionRegularReadyGolden());
    candidate = adoption;
    candidate.insert(QStringLiteral("beforeOwnership"),
                     QString::fromLatin1(ownershipRegularGolden().toBase64()));
    QVERIFY(bridgeRejected(canonical(candidate)));

    const QStringList crosslinks{
        QStringLiteral("beforeGeneration"), QStringLiteral("beforeNonce"),
        QStringLiteral("beforeDigest"),     QStringLiteral("beforeSize"),
        QStringLiteral("beforeDevice"),     QStringLiteral("beforeInode"),
    };
    for (const auto &field : crosslinks) {
      candidate = managed;
      const auto current = candidate.value(field).toString();
      candidate.insert(
          field,
          field == QStringLiteral("beforeSize") ||
                  field == QStringLiteral("beforeDevice") ||
                  field == QStringLiteral("beforeInode")
              ? QJsonValue(QString::number(current.toULongLong() + 1))
              : QJsonValue(current == hash('a') ? hash('b') : hash('a')));
      QVERIFY2(bridgeRejected(canonical(candidate)), qPrintable(field));
    }
  }

  void canonicalFramingAndParserBoundsAreExact() {
    const QList<QPair<QByteArray, std::function<bool(const QByteArray &)>>>
        rows{
            {ownershipRegularGolden(), ownershipRejected},
            {managedReadyGolden(), bridgeRejected},
        };
    for (const auto &[golden, rejected] : rows) {
      auto bytes = golden;
      bytes.chop(1);
      QVERIFY(rejected(bytes));

      bytes = golden + QByteArrayLiteral("\n");
      QVERIFY(rejected(bytes));

      bytes = golden;
      bytes.chop(1);
      bytes.append("\r\n");
      QVERIFY(rejected(bytes));

      bytes = QByteArrayLiteral("\xEF\xBB\xBF") + golden;
      QVERIFY(rejected(bytes));

      bytes = golden;
      bytes.insert(1, ' ');
      QVERIFY(rejected(bytes));

      bytes = golden;
      bytes.insert(1, QByteArrayLiteral("\"formatVersion\":1,"));
      QVERIFY(rejected(bytes));

      bytes = golden;
      const auto version = bytes.indexOf("\"formatVersion\":1");
      QVERIFY(version >= 0);
      bytes.replace(version, qsizetype(std::strlen("\"formatVersion\":1")),
                    QByteArrayLiteral("\"formatVersion\":1.0"));
      QVERIFY(rejected(bytes));
    }

    auto noncanonicalMode = ownershipRegularGolden();
    auto mode = noncanonicalMode.indexOf("\"originalMode\":416");
    QVERIFY(mode >= 0);
    noncanonicalMode.replace(mode,
                             qsizetype(std::strlen("\"originalMode\":416")),
                             QByteArrayLiteral("\"originalMode\":416.0"));
    QVERIFY(ownershipRejected(noncanonicalMode));

    noncanonicalMode = managedReadyGolden();
    mode = noncanonicalMode.indexOf("\"beforeMode\":384");
    QVERIFY(mode >= 0);
    noncanonicalMode.replace(mode, qsizetype(std::strlen("\"beforeMode\":384")),
                             QByteArrayLiteral("\"beforeMode\":384.0"));
    QVERIFY(bridgeRejected(noncanonicalMode));

    auto escaped = ownershipRegularGolden();
    const auto key = escaped.indexOf("\"activationNonce\"");
    QVERIFY(key >= 0);
    escaped.replace(key, qsizetype(std::strlen("\"activationNonce\"")),
                    QByteArrayLiteral("\"\\u0061ctivationNonce\""));
    QVERIFY(ownershipRejected(escaped));

    const QByteArray oversized(maximumLegacyEntrypointRecordV1Bytes + 1, ' ');
    QVERIFY(ownershipRejected(oversized));
    QVERIFY(bridgeRejected(oversized));

    QByteArray tooDeep = QByteArrayLiteral("{\"x\":");
    tooDeep.append(QByteArray(33, '['));
    tooDeep.append('0');
    tooDeep.append(QByteArray(33, ']'));
    tooDeep.append("}\n");
    QVERIFY(ownershipRejected(tooDeep));
    QVERIFY(bridgeRejected(tooDeep));
  }

  void typedSerializersRejectInvalidRecords() {
    auto ownership = regularOwnership();
    ownership.original.mode = 0660;
    QVERIFY(!serializeLegacyEntrypointOwnershipRecordV1(ownership));

    ownership = regularOwnership();
    ownership.entrypointDevice = 0;
    QVERIFY(!serializeLegacyEntrypointOwnershipRecordV1(ownership));

    auto bridge = managedStaging();
    bridge.targetDevice = 1;
    QVERIFY(!serializeLegacyLiveActivationBridgeRecordV1(bridge));

    bridge = managedReady();
    bridge.beforeGeneration = hash('a');
    QVERIFY(!serializeLegacyLiveActivationBridgeRecordV1(bridge));

    bridge = adoptionAbsentStaging();
    bridge.baselineProvider = QStringLiteral("lua");
    QVERIFY(!serializeLegacyLiveActivationBridgeRecordV1(bridge));
  }
};

QTEST_GUILESS_MAIN(CompositorLegacyEntrypointRecordsTest)

#include "compositor_legacy_entrypoint_records_test.moc"
