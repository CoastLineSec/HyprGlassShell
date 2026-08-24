#include "compositord/ordinary_pending_record.h"

#include "compositord/canonical_json.h"
#include "hyprland/json_support.h"

#include <QCryptographicHash>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QtTest>

#include <array>
#include <limits>

using namespace HyprShelld::Compositor;
using namespace HyprShelld::Hyprland;

namespace {

const QString authorityA = QStringLiteral("11111111111111111111111111111111");
const QString authorityB = QStringLiteral("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa");
const QString beforeNonce = QStringLiteral("22222222222222222222222222222222");
const QString afterNonce = QStringLiteral("33333333333333333333333333333333");
const QString beforeGeneration(64, QLatin1Char('b'));
const QString afterGeneration(64, QLatin1Char('c'));

[[nodiscard]] QByteArray readBytes(const QString &path) {
  QFile file(path);
  if (!file.open(QIODevice::ReadOnly)) {
    return {};
  }
  return file.readAll();
}

[[nodiscard]] QString digestWithoutFinalLf(const QByteArrayView bytes) {
  if (bytes.isEmpty() || bytes.back() != '\n') {
    return {};
  }
  return QString::fromLatin1(
      QCryptographicHash::hash(bytes.first(bytes.size() - 1),
                               QCryptographicHash::Sha256)
          .toHex());
}

[[nodiscard]] QString digestIncludingFinalLf(const QByteArrayView bytes) {
  return QString::fromLatin1(
      QCryptographicHash::hash(bytes, QCryptographicHash::Sha256).toHex());
}

[[nodiscard]] QString kindText(const OrdinaryPendingKind kind) {
  switch (kind) {
  case OrdinaryPendingKind::Apply:
    return QStringLiteral("apply");
  case OrdinaryPendingKind::Recovery:
    return QStringLiteral("recovery");
  case OrdinaryPendingKind::DisplayPreview:
    return QStringLiteral("display-preview");
  }
  return {};
}

[[nodiscard]] QString phaseText(const OrdinaryPendingPhase phase) {
  switch (phase) {
  case OrdinaryPendingPhase::Prepared:
    return QStringLiteral("prepared");
  case OrdinaryPendingPhase::Committing:
    return QStringLiteral("committing");
  }
  return {};
}

[[nodiscard]] CanonicalJson::Limits pendingLimits() {
  return {
      .maximumBytes = maximumOrdinaryPendingRecordV2Bytes,
      .maximumDepth = maximumOrdinaryPendingRecordV2Depth,
      .maximumValues = maximumOrdinaryPendingRecordV2Values,
  };
}

[[nodiscard]] QByteArray canonicalOuter(const QJsonObject &object) {
  const auto encoded = CanonicalJson::serialize(
      object, CanonicalJson::Framing::OneTrailingLineFeed,
      CanonicalJson::TextPolicy::Rfc8785, pendingLimits());
  return encoded ? *encoded.value : QByteArray{};
}

[[nodiscard]] QJsonObject objectFromOuter(const QByteArrayView bytes) {
  const auto parsed = CanonicalJson::parseCanonicalObject(
      bytes, CanonicalJson::Framing::OneTrailingLineFeed,
      CanonicalJson::TextPolicy::Rfc8785, pendingLimits());
  return parsed ? *parsed.value : QJsonObject{};
}

[[nodiscard]] QJsonObject objectFromDesired(const QByteArrayView bytes) {
  const auto parsed =
      JsonSupport::parseStrictObject(bytes, maximumDesiredStateBytes, 64);
  return parsed ? *parsed.value : QJsonObject{};
}

[[nodiscard]] QByteArray bareJcs(const QJsonObject &object) {
  const auto encoded = CanonicalJson::serialize(
      object, CanonicalJson::Framing::Bare, CanonicalJson::TextPolicy::Rfc8785,
      pendingLimits());
  return encoded ? *encoded.value : QByteArray{};
}

[[nodiscard]] QJsonObject appliedObject(const AppliedRecordV2 &record) {
  const auto encoded = serializeAppliedRecordV2(record);
  if (!encoded)
    return {};
  const auto parsed = CanonicalJson::parseCanonicalObject(
      *encoded.value, CanonicalJson::Framing::OneTrailingLineFeed,
      CanonicalJson::TextPolicy::Rfc8785,
      {
          .maximumBytes = maximumAppliedRecordV2Bytes,
          .maximumDepth = maximumAppliedRecordV2Depth,
          .maximumValues = maximumAppliedRecordV2Values,
      });
  return parsed ? *parsed.value : QJsonObject{};
}

} // namespace

class CompositorOrdinaryPendingRecordTest final : public QObject {
  Q_OBJECT

private:
  Catalog catalogV2_;
  ActionCatalog actionCatalogV2_;

  [[nodiscard]] DesiredStateV2 candidate(const quint64 revision,
                                         const bool crossDomain = false) const {
    const auto initial =
        defaultDormantDesiredStateV2(catalogV2_, actionCatalogV2_, authorityA);
    Q_ASSERT(initial);
    auto result = *initial.value;
    result.semanticState.revision = revision;
    if (crossDomain) {
      result.semanticState.overrides.insert(
          QStringLiteral("hyprland.input.sensitivity"), 0.375);
      QString text = QStringLiteral("quote\" slash/ backslash\\ ");
      text.append(QChar(0x2028));
      text.append(QString::fromUcs4(U" emoji \U0001F600"));
      result.semanticState.environment.append({
          .id = QStringLiteral("cross-domain"),
          .name = QStringLiteral("D052_TEXT"),
          .value = text,
          .scope = EnvironmentScope::Hyprland,
      });
    }
    return result;
  }

  [[nodiscard]] QByteArray candidateBytes(const DesiredStateV2 &state) const {
    const auto encoded = serializeDormantDesiredStateV2(state);
    Q_ASSERT(encoded);
    return *encoded.value;
  }

  [[nodiscard]] OrdinaryPendingDesiredMaterialV2
  desiredMaterial(const DesiredStateV2 &state) const {
    return {
        .state = state,
        .bytes = candidateBytes(state),
    };
  }

  [[nodiscard]] AppliedRecordV2 applied(const quint64 revision,
                                        const QString &snapshot,
                                        const bool before) const {
    return {
        .authorityId = authorityA,
        .revision = revision,
        .snapshotDigest = snapshot,
        .generation = before ? beforeGeneration : afterGeneration,
        .activationNonce = before ? beforeNonce : afterNonce,
        .entrypoint = QStringLiteral("hyprland.lua"),
        .requiredActivation = ActivationRequirement::Reload,
    };
  }

  [[nodiscard]] OrdinaryPendingRecordV2
  validRecord(const OrdinaryPendingKind kind,
              const OrdinaryPendingPhase phase = OrdinaryPendingPhase::Prepared,
              const bool applyBeforePresent = false,
              const quint64 expectedRevision = 7, quint64 priorRevision = 3,
              const bool crossDomain = false) const {
    if (priorRevision > expectedRevision) {
      priorRevision = expectedRevision;
    }
    const auto advancing = kind != OrdinaryPendingKind::Apply;
    const auto revision = advancing ? expectedRevision + 1 : expectedRevision;
    auto target = candidate(revision, crossDomain);
    const auto targetBytes = candidateBytes(target);
    const auto targetDigest = digestWithoutFinalLf(targetBytes);

    OrdinaryPendingRecordV2 record{
        .authorityId = authorityA,
        .kind = kind,
        .phase = phase,
        .expectedRevision = expectedRevision,
        .beforeDesiredDigest = targetDigest,
        .beforeActivationDesired = std::nullopt,
        .candidateSnapshot = target,
        .candidateSnapshotBytes = targetBytes,
        .snapshotDigest = targetDigest,
        .afterActivation = applied(revision, targetDigest, false),
        .beforeActivation = std::nullopt,
    };

    if (kind == OrdinaryPendingKind::Apply) {
      if (applyBeforePresent) {
        auto prior = target;
        prior.semanticState.revision = priorRevision;
        const auto priorBytes = candidateBytes(prior);
        const auto priorDigest = digestWithoutFinalLf(priorBytes);
        record.beforeActivation = applied(priorRevision, priorDigest, true);
        record.beforeActivationDesired = OrdinaryPendingDesiredMaterialV2{
            .state = std::move(prior),
            .bytes = priorBytes,
        };
      }
      return record;
    }

    const auto actualPriorRevision = kind == OrdinaryPendingKind::DisplayPreview
                                         ? expectedRevision
                                         : priorRevision;
    auto prior = target;
    prior.semanticState.revision = actualPriorRevision;
    const auto priorBytes = candidateBytes(prior);
    const auto priorDigest = digestWithoutFinalLf(priorBytes);
    record.beforeActivation = applied(actualPriorRevision, priorDigest, true);
    record.beforeActivationDesired = OrdinaryPendingDesiredMaterialV2{
        .state = std::move(prior),
        .bytes = priorBytes,
    };
    if (kind == OrdinaryPendingKind::DisplayPreview) {
      record.beforeDesiredDigest = priorDigest;
    } else {
      const auto currentDesired = candidate(expectedRevision, crossDomain);
      record.beforeDesiredDigest =
          digestWithoutFinalLf(candidateBytes(currentDesired));
    }
    return record;
  }

  [[nodiscard]] QByteArray
  expectedGolden(const OrdinaryPendingRecordV2 &record) const {
    const auto candidateJcs =
        bareJcs(objectFromDesired(record.candidateSnapshotBytes));
    const auto afterJcs = bareJcs(appliedObject(record.afterActivation));
    const auto beforeJcs =
        record.beforeActivation
            ? bareJcs(appliedObject(*record.beforeActivation))
            : QByteArrayLiteral("null");
    const auto beforeDesiredJcs =
        record.beforeActivationDesired
            ? bareJcs(objectFromDesired(record.beforeActivationDesired->bytes))
            : QByteArrayLiteral("null");

    QByteArray expected = QByteArrayLiteral("{\"afterActivation\":");
    expected.append(afterJcs);
    expected.append(QByteArrayLiteral(",\"authorityId\":\""));
    expected.append(record.authorityId.toLatin1());
    expected.append(QByteArrayLiteral("\",\"beforeActivation\":"));
    expected.append(beforeJcs);
    expected.append(QByteArrayLiteral(",\"beforeActivationDesired\":"));
    expected.append(beforeDesiredJcs);
    expected.append(QByteArrayLiteral(",\"beforeDesiredDigest\":\""));
    expected.append(record.beforeDesiredDigest.toLatin1());
    expected.append(QByteArrayLiteral("\",\"candidateSnapshot\":"));
    expected.append(candidateJcs);
    expected.append(QByteArrayLiteral(",\"expectedRevision\":\""));
    expected.append(QByteArray::number(record.expectedRevision));
    expected.append(QByteArrayLiteral("\",\"formatVersion\":2,\"kind\":\""));
    expected.append(kindText(record.kind).toLatin1());
    expected.append(QByteArrayLiteral("\",\"phase\":\""));
    expected.append(phaseText(record.phase).toLatin1());
    expected.append(QByteArrayLiteral("\",\"snapshotDigest\":\""));
    expected.append(record.snapshotDigest.toLatin1());
    expected.append(QByteArrayLiteral("\"}\n"));
    return expected;
  }

  [[nodiscard]] CanonicalJson::Result<QByteArray>
  encode(const OrdinaryPendingRecordV2 &record) const {
    return serializeOrdinaryPendingRecordV2(record, catalogV2_,
                                            actionCatalogV2_);
  }

  [[nodiscard]] CanonicalJson::Result<OrdinaryPendingRecordV2>
  decode(const QByteArrayView bytes) const {
    return parseOrdinaryPendingRecordV2(bytes, catalogV2_, actionCatalogV2_);
  }

private slots:
  void initTestCase();
  void constantsAndAllKindPhaseGoldenBytesAreExact();
  void applySupportsNullAndPresentBeforeActivation();
  void beforeActivationDesiredPresenceAndCoherenceAreExact();
  void revisionEndpointsAndKindRelationsAreClosed();
  void rejectsEveryFieldAndCanonicalFramingMutation();
  void rejectsClosedEnumIdentifierDigestAuthorityAndNonceMutations();
  void rejectsInvalidCandidateAuthoritiesAndEnvelopeContracts();
  void snapshotDigestExcludesExactlyTheFinalLf();
  void onlyPreparedToCommittingIsLegal();
  void byteCapAndNearLargeCandidateRoundTrip();
  void numericAndUnicodeCandidateRoundTripsAcrossBothDomains();
};

void CompositorOrdinaryPendingRecordTest::initTestCase() {
  const auto catalog = parseDormantCatalogV2(
      readBytes(QStringLiteral(HYPRSHELLD_HYPRLAND_V2_CATALOG_FILE)));
  QVERIFY(catalog);
  catalogV2_ = *catalog.value;

  const auto actions = parseDormantActionCatalogV2(
      readBytes(QStringLiteral(HYPRSHELLD_HYPRLAND_V2_ACTION_CATALOG_FILE)),
      readBytes(QStringLiteral(HYPRSHELLD_HYPRLAND_V2_CONFIG_SCHEMA_FILE)));
  QVERIFY(actions);
  actionCatalogV2_ = *actions.value;
}

void CompositorOrdinaryPendingRecordTest::
    constantsAndAllKindPhaseGoldenBytesAreExact() {
  QCOMPARE(ordinaryPendingRecordV2FormatVersion, quint32(2));
  QCOMPARE(maximumOrdinaryPendingRecordV2Bytes,
           2 * maximumDesiredStateBytes + qsizetype(16 * 1024));
  QCOMPARE(maximumOrdinaryPendingRecordV2Bytes, qsizetype(8'404'992));
  QCOMPARE(maximumOrdinaryPendingRecordV2Depth, 65);
  QCOMPARE(maximumOrdinaryPendingRecordV2Values, qsizetype(8'404'992));

  const std::array kinds{
      OrdinaryPendingKind::Apply,
      OrdinaryPendingKind::Recovery,
      OrdinaryPendingKind::DisplayPreview,
  };
  const std::array phases{
      OrdinaryPendingPhase::Prepared,
      OrdinaryPendingPhase::Committing,
  };
  for (const auto kind : kinds) {
    for (const auto phase : phases) {
      const auto record = validRecord(kind, phase);
      const auto encoded = encode(record);
      QVERIFY2(encoded, qPrintable(kindText(kind) + QLatin1Char('/') +
                                   phaseText(phase)));
      QCOMPARE(*encoded.value, expectedGolden(record));
      const auto parsed = decode(*encoded.value);
      QVERIFY(parsed);
      QCOMPARE(*parsed.value, record);
    }
  }
}

void CompositorOrdinaryPendingRecordTest::
    applySupportsNullAndPresentBeforeActivation() {
  for (const auto beforePresent : {false, true}) {
    const auto record =
        validRecord(OrdinaryPendingKind::Apply, OrdinaryPendingPhase::Prepared,
                    beforePresent);
    const auto encoded = encode(record);
    QVERIFY(encoded);
    const auto parsed = decode(*encoded.value);
    QVERIFY(parsed);
    QCOMPARE(parsed.value->beforeActivation.has_value(), beforePresent);
    QCOMPARE(parsed.value->beforeActivationDesired.has_value(), beforePresent);
    QCOMPARE(*parsed.value, record);
  }

  for (const auto kind : {
           OrdinaryPendingKind::Recovery,
           OrdinaryPendingKind::DisplayPreview,
       }) {
    auto record = validRecord(kind);
    record.beforeActivation.reset();
    QVERIFY(!encode(record));
  }
}

void CompositorOrdinaryPendingRecordTest::
    beforeActivationDesiredPresenceAndCoherenceAreExact() {
  const auto olderApply = validRecord(
      OrdinaryPendingKind::Apply, OrdinaryPendingPhase::Prepared, true, 7, 3);
  QVERIFY(olderApply.beforeActivation);
  QVERIFY(olderApply.beforeActivationDesired);
  QCOMPARE(olderApply.beforeActivation->revision, quint64(3));
  QCOMPARE(olderApply.beforeActivationDesired->state.semanticState.revision,
           quint64(3));
  QVERIFY(olderApply.beforeActivationDesired->bytes !=
          olderApply.candidateSnapshotBytes);
  const auto olderEncoded = encode(olderApply);
  QVERIFY(olderEncoded);
  const auto olderParsed = decode(*olderEncoded.value);
  QVERIFY(olderParsed);
  QCOMPARE(*olderParsed.value, olderApply);
  const auto olderObject = objectFromOuter(*olderEncoded.value);
  QVERIFY(
      olderObject.value(QStringLiteral("beforeActivationDesired")).isObject());

  auto record = validRecord(OrdinaryPendingKind::Apply);
  record.beforeActivation =
      applied(record.expectedRevision, record.snapshotDigest, true);
  QVERIFY(!encode(record));

  record = olderApply;
  record.beforeActivationDesired.reset();
  QVERIFY(!encode(record));

  record = validRecord(OrdinaryPendingKind::Apply);
  record.beforeActivationDesired = desiredMaterial(record.candidateSnapshot);
  QVERIFY(!encode(record));

  for (const auto kind : {
           OrdinaryPendingKind::Recovery,
           OrdinaryPendingKind::DisplayPreview,
       }) {
    record = validRecord(kind);
    record.beforeActivation.reset();
    QVERIFY(!encode(record));
    record = validRecord(kind);
    record.beforeActivationDesired.reset();
    QVERIFY(!encode(record));
  }

  auto wire = olderObject;
  wire.insert(QStringLiteral("beforeActivationDesired"), QJsonValue::Null);
  QVERIFY(!decode(canonicalOuter(wire)));
  wire = olderObject;
  wire.insert(QStringLiteral("beforeActivation"), QJsonValue::Null);
  QVERIFY(!decode(canonicalOuter(wire)));

  const auto withoutBefore =
      objectFromOuter(*encode(validRecord(OrdinaryPendingKind::Apply)).value);
  wire = withoutBefore;
  wire.insert(QStringLiteral("beforeActivationDesired"),
              wire.value(QStringLiteral("candidateSnapshot")));
  QVERIFY(!decode(canonicalOuter(wire)));

  record = olderApply;
  record.beforeActivationDesired->bytes.chop(1);
  QVERIFY(!encode(record));
  record = olderApply;
  record.beforeActivationDesired->bytes.append('\n');
  QVERIFY(!encode(record));

  record = olderApply;
  record.beforeActivationDesired->state.authorityId = authorityB;
  record.beforeActivationDesired->bytes =
      candidateBytes(record.beforeActivationDesired->state);
  QVERIFY(!encode(record));

  record = olderApply;
  ++record.beforeActivationDesired->state.semanticState.revision;
  record.beforeActivationDesired->bytes =
      candidateBytes(record.beforeActivationDesired->state);
  QVERIFY(!encode(record));

  record = olderApply;
  record.beforeActivationDesired->state.semanticState.overrides.insert(
      QStringLiteral("hyprland.input.sensitivity"), 0.125);
  record.beforeActivationDesired->bytes =
      candidateBytes(record.beforeActivationDesired->state);
  QVERIFY(!encode(record));

  auto sameRevisionApply = validRecord(
      OrdinaryPendingKind::Apply, OrdinaryPendingPhase::Prepared, true, 7, 7);
  sameRevisionApply.beforeActivationDesired->state.semanticState.overrides
      .insert(QStringLiteral("hyprland.input.sensitivity"), 0.125);
  sameRevisionApply.beforeActivationDesired->bytes =
      candidateBytes(sameRevisionApply.beforeActivationDesired->state);
  sameRevisionApply.beforeActivation->snapshotDigest =
      digestWithoutFinalLf(sameRevisionApply.beforeActivationDesired->bytes);
  QVERIFY(!encode(sameRevisionApply));

  auto recovery = validRecord(OrdinaryPendingKind::Recovery);
  recovery.beforeActivationDesired->state.semanticState.overrides.insert(
      QStringLiteral("hyprland.input.sensitivity"), 0.125);
  recovery.beforeActivationDesired->bytes =
      candidateBytes(recovery.beforeActivationDesired->state);
  recovery.beforeActivation->snapshotDigest =
      digestWithoutFinalLf(recovery.beforeActivationDesired->bytes);
  QVERIFY(!encode(recovery));

  wire = olderObject;
  auto priorObject =
      wire.value(QStringLiteral("beforeActivationDesired")).toObject();
  priorObject.insert(QStringLiteral("authorityId"), authorityB);
  wire.insert(QStringLiteral("beforeActivationDesired"), priorObject);
  QVERIFY(!decode(canonicalOuter(wire)));

  wire = olderObject;
  priorObject =
      wire.value(QStringLiteral("beforeActivationDesired")).toObject();
  priorObject.insert(QStringLiteral("revision"), QStringLiteral("4"));
  wire.insert(QStringLiteral("beforeActivationDesired"), priorObject);
  QVERIFY(!decode(canonicalOuter(wire)));
}

void CompositorOrdinaryPendingRecordTest::
    revisionEndpointsAndKindRelationsAreClosed() {
  for (const auto expected : {
           quint64{0},
           std::numeric_limits<quint64>::max(),
       }) {
    const auto apply =
        validRecord(OrdinaryPendingKind::Apply, OrdinaryPendingPhase::Prepared,
                    true, expected, expected);
    const auto encoded = encode(apply);
    QVERIFY(encoded);
    QVERIFY(decode(*encoded.value));
  }

  for (const auto expected : {
           quint64{0},
           std::numeric_limits<quint64>::max() - 1,
       }) {
    for (const auto kind : {
             OrdinaryPendingKind::Recovery,
             OrdinaryPendingKind::DisplayPreview,
         }) {
      const auto record = validRecord(kind, OrdinaryPendingPhase::Prepared,
                                      false, expected, expected);
      const auto encoded = encode(record);
      QVERIFY(encoded);
      const auto parsed = decode(*encoded.value);
      QVERIFY(parsed);
      QCOMPARE(parsed.value->candidateSnapshot.semanticState.revision,
               expected + 1);
    }
  }

  auto invalid = validRecord(OrdinaryPendingKind::Recovery);
  invalid.expectedRevision = std::numeric_limits<quint64>::max();
  invalid.candidateSnapshot.semanticState.revision =
      std::numeric_limits<quint64>::max();
  invalid.candidateSnapshotBytes = candidateBytes(invalid.candidateSnapshot);
  invalid.snapshotDigest = digestWithoutFinalLf(invalid.candidateSnapshotBytes);
  invalid.afterActivation.revision =
      invalid.candidateSnapshot.semanticState.revision;
  invalid.afterActivation.snapshotDigest = invalid.snapshotDigest;
  QVERIFY(!encode(invalid));

  invalid = validRecord(OrdinaryPendingKind::Apply);
  ++invalid.candidateSnapshot.semanticState.revision;
  invalid.candidateSnapshotBytes = candidateBytes(invalid.candidateSnapshot);
  invalid.snapshotDigest = digestWithoutFinalLf(invalid.candidateSnapshotBytes);
  invalid.afterActivation.revision =
      invalid.candidateSnapshot.semanticState.revision;
  invalid.afterActivation.snapshotDigest = invalid.snapshotDigest;
  invalid.beforeDesiredDigest = invalid.snapshotDigest;
  QVERIFY(!encode(invalid));

  auto bytes = *encode(validRecord(OrdinaryPendingKind::Apply)).value;
  auto object = objectFromOuter(bytes);
  for (const auto &revision : {
           QStringLiteral("00"),
           QStringLiteral("+1"),
           QStringLiteral("18446744073709551616"),
       }) {
    auto mutated = object;
    mutated.insert(QStringLiteral("expectedRevision"), revision);
    QVERIFY(!decode(canonicalOuter(mutated)));
  }
}

void CompositorOrdinaryPendingRecordTest::
    rejectsEveryFieldAndCanonicalFramingMutation() {
  const auto valid = *encode(validRecord(OrdinaryPendingKind::Apply)).value;
  const auto base = objectFromOuter(valid);
  QVERIFY(!base.isEmpty());

  for (const auto &key : base.keys()) {
    auto missing = base;
    missing.remove(key);
    QVERIFY2(!decode(canonicalOuter(missing)), qPrintable(key));

    auto wrong = base;
    if (key == QStringLiteral("beforeActivation") ||
        key == QStringLiteral("beforeActivationDesired")) {
      wrong.insert(key, true);
    } else {
      wrong.insert(key, QJsonValue::Null);
    }
    QVERIFY2(!decode(canonicalOuter(wrong)), qPrintable(key));
  }

  auto extra = base;
  extra.insert(QStringLiteral("unknown"), true);
  QVERIFY(!decode(canonicalOuter(extra)));

  auto duplicate = valid;
  duplicate.insert(
      1, QByteArrayLiteral(
             "\"authorityId\":\"11111111111111111111111111111111\","));
  QVERIFY(!decode(duplicate));

  auto duplicateCandidateField = valid;
  const auto candidateStart = duplicateCandidateField.indexOf(
      QByteArrayLiteral("\"candidateSnapshot\":{"));
  QVERIFY(candidateStart >= 0);
  const auto candidateAuthority = duplicateCandidateField.indexOf(
      QByteArrayLiteral("\"authorityId\":\"11111111111111111111111111111111\""),
      candidateStart);
  QVERIFY(candidateAuthority > candidateStart);
  duplicateCandidateField.insert(
      candidateAuthority,
      QByteArrayLiteral(
          "\"authorityId\":\"11111111111111111111111111111111\","));
  QVERIFY(!decode(duplicateCandidateField));

  auto duplicateAppliedField = valid;
  const auto appliedStart =
      duplicateAppliedField.indexOf(QByteArrayLiteral("\"afterActivation\":{"));
  QVERIFY(appliedStart >= 0);
  const auto appliedAuthority = duplicateAppliedField.indexOf(
      QByteArrayLiteral("\"authorityId\":\"11111111111111111111111111111111\""),
      appliedStart);
  QVERIFY(appliedAuthority > appliedStart);
  duplicateAppliedField.insert(
      appliedAuthority,
      QByteArrayLiteral(
          "\"authorityId\":\"11111111111111111111111111111111\","));
  QVERIFY(!decode(duplicateAppliedField));

  QVERIFY(!decode(valid.chopped(1)));
  QVERIFY(!decode(valid + '\n'));
  QVERIFY(!decode(QByteArrayLiteral(" ") + valid));
  QVERIFY(!decode(QByteArrayLiteral("\xef\xbb\xbf") + valid));

  auto invalidUtf8 = valid;
  const auto idOffset = invalidUtf8.indexOf(authorityA.toLatin1());
  QVERIFY(idOffset >= 0);
  invalidUtf8[idOffset] = static_cast<char>(0xff);
  QVERIFY(!decode(invalidUtf8));
}

void CompositorOrdinaryPendingRecordTest::
    rejectsClosedEnumIdentifierDigestAuthorityAndNonceMutations() {
  auto record = validRecord(OrdinaryPendingKind::Apply,
                            OrdinaryPendingPhase::Prepared, true);
  record.kind = static_cast<OrdinaryPendingKind>(999);
  QVERIFY(!encode(record));
  record = validRecord(OrdinaryPendingKind::Apply);
  record.phase = static_cast<OrdinaryPendingPhase>(999);
  QVERIFY(!encode(record));

  const auto valid = *encode(validRecord(OrdinaryPendingKind::Apply,
                                         OrdinaryPendingPhase::Prepared, true))
                          .value;
  const auto base = objectFromOuter(valid);
  for (const auto &[key, value] : std::array{
           std::pair{QStringLiteral("kind"),
                     QJsonValue(QStringLiteral("Apply"))},
           std::pair{QStringLiteral("kind"),
                     QJsonValue(QStringLiteral("restart"))},
           std::pair{QStringLiteral("phase"),
                     QJsonValue(QStringLiteral("Prepared"))},
           std::pair{QStringLiteral("phase"),
                     QJsonValue(QStringLiteral("complete"))},
           std::pair{QStringLiteral("authorityId"),
                     QJsonValue(QString(32, QLatin1Char('0')))},
           std::pair{QStringLiteral("authorityId"),
                     QJsonValue(QString(32, QLatin1Char('A')))},
           std::pair{QStringLiteral("beforeDesiredDigest"),
                     QJsonValue(QString(63, QLatin1Char('a')))},
           std::pair{QStringLiteral("snapshotDigest"),
                     QJsonValue(QString(64, QLatin1Char('A')))},
       }) {
    auto mutated = base;
    mutated.insert(key, value);
    QVERIFY2(!decode(canonicalOuter(mutated)), qPrintable(key));
  }

  auto mutated = base;
  auto candidateObject =
      mutated.value(QStringLiteral("candidateSnapshot")).toObject();
  candidateObject.insert(QStringLiteral("authorityId"), authorityB);
  mutated.insert(QStringLiteral("candidateSnapshot"), candidateObject);
  QVERIFY(!decode(canonicalOuter(mutated)));

  mutated = base;
  auto after = mutated.value(QStringLiteral("afterActivation")).toObject();
  after.insert(QStringLiteral("authorityId"), authorityB);
  mutated.insert(QStringLiteral("afterActivation"), after);
  QVERIFY(!decode(canonicalOuter(mutated)));

  for (const auto &entrypoint : {
           QStringLiteral("/tmp/hyprland.lua"),
           QStringLiteral("../hyprland.lua"),
       }) {
    mutated = base;
    after = mutated.value(QStringLiteral("afterActivation")).toObject();
    after.insert(QStringLiteral("entrypoint"), entrypoint);
    mutated.insert(QStringLiteral("afterActivation"), after);
    QVERIFY(!decode(canonicalOuter(mutated)));
  }
  for (const auto &requirement : {
           QStringLiteral("none"),
           QStringLiteral("unknown"),
       }) {
    mutated = base;
    after = mutated.value(QStringLiteral("afterActivation")).toObject();
    after.insert(QStringLiteral("requiredActivation"), requirement);
    mutated.insert(QStringLiteral("afterActivation"), after);
    QVERIFY(!decode(canonicalOuter(mutated)));
  }

  mutated = base;
  auto before = mutated.value(QStringLiteral("beforeActivation")).toObject();
  before.insert(QStringLiteral("authorityId"), authorityB);
  mutated.insert(QStringLiteral("beforeActivation"), before);
  QVERIFY(!decode(canonicalOuter(mutated)));

  mutated = base;
  before = mutated.value(QStringLiteral("beforeActivation")).toObject();
  after = mutated.value(QStringLiteral("afterActivation")).toObject();
  before.insert(QStringLiteral("activationNonce"),
                after.value(QStringLiteral("activationNonce")));
  mutated.insert(QStringLiteral("beforeActivation"), before);
  QVERIFY(!decode(canonicalOuter(mutated)));

  record = validRecord(OrdinaryPendingKind::Apply,
                       OrdinaryPendingPhase::Prepared, true);
  record.beforeActivation->activationNonce =
      record.afterActivation.activationNonce;
  QVERIFY(!encode(record));

  record = validRecord(OrdinaryPendingKind::Apply,
                       OrdinaryPendingPhase::Prepared, true);
  record.beforeActivation->revision = record.expectedRevision + 1;
  QVERIFY(!encode(record));

  record = validRecord(OrdinaryPendingKind::Apply);
  record.snapshotDigest = QString(64, QLatin1Char('d'));
  QVERIFY(!encode(record));
  record = validRecord(OrdinaryPendingKind::Apply);
  record.afterActivation.snapshotDigest = QString(64, QLatin1Char('d'));
  QVERIFY(!encode(record));

  record = validRecord(OrdinaryPendingKind::Recovery);
  record.beforeActivation->snapshotDigest = QString(64, QLatin1Char('d'));
  QVERIFY(!encode(record));

  record = validRecord(OrdinaryPendingKind::DisplayPreview);
  --record.beforeActivation->revision;
  QVERIFY(!encode(record));
  record = validRecord(OrdinaryPendingKind::DisplayPreview);
  record.beforeDesiredDigest = QString(64, QLatin1Char('d'));
  QVERIFY(!encode(record));
}

void CompositorOrdinaryPendingRecordTest::
    rejectsInvalidCandidateAuthoritiesAndEnvelopeContracts() {
  const auto valid = *encode(validRecord(OrdinaryPendingKind::Apply)).value;
  const auto base = objectFromOuter(valid);
  const auto candidateKey = QStringLiteral("candidateSnapshot");
  const auto original = base.value(candidateKey).toObject();

  QList<QJsonObject> invalidCandidates;
  for (const auto version : {1, 3}) {
    auto value = original;
    value.insert(QStringLiteral("formatVersion"), version);
    invalidCandidates.append(value);
  }
  {
    auto value = original;
    value.insert(QStringLiteral("authorityId"), authorityB);
    invalidCandidates.append(value);
  }
  {
    auto value = original;
    value.insert(QStringLiteral("targetHyprland"), QStringLiteral("0.56.1"));
    invalidCandidates.append(value);
  }
  {
    auto value = original;
    value.insert(QStringLiteral("catalogDigest"),
                 QString(64, QLatin1Char('a')));
    invalidCandidates.append(value);
  }
  {
    auto value = original;
    value.insert(QStringLiteral("actionCatalogDigest"),
                 QString(64, QLatin1Char('b')));
    invalidCandidates.append(value);
  }
  {
    auto value = original;
    value.insert(QStringLiteral("unknown"), true);
    invalidCandidates.append(value);
  }
  {
    auto value = original;
    auto overrides = value.value(QStringLiteral("overrides")).toObject();
    overrides.insert(QStringLiteral("hyprland.input.sensitivity"), 1.5);
    value.insert(QStringLiteral("overrides"), overrides);
    invalidCandidates.append(value);
  }
  {
    auto value = original;
    auto environment = value.value(QStringLiteral("environment")).toArray();
    environment.append(QJsonObject{
        {QStringLiteral("id"), QStringLiteral("invalid-name")},
        {QStringLiteral("name"), QStringLiteral("1BAD")},
        {QStringLiteral("value"), QStringLiteral("x")},
        {QStringLiteral("scope"), QStringLiteral("hyprland")},
    });
    value.insert(QStringLiteral("environment"), environment);
    invalidCandidates.append(value);
  }
  {
    auto value = original;
    auto rules = value.value(QStringLiteral("workspaceRules")).toArray();
    rules.append(QJsonObject{
        {QStringLiteral("id"), QStringLiteral("after-protected")},
        {QStringLiteral("selector"), QStringLiteral("1")},
        {QStringLiteral("enabled"), true},
        {QStringLiteral("monitor"), QString()},
        {QStringLiteral("persistent"), false},
        {QStringLiteral("isDefault"), false},
        {QStringLiteral("layout"), QString()},
        {QStringLiteral("overrides"), QJsonObject{}},
    });
    value.insert(QStringLiteral("workspaceRules"), rules);
    invalidCandidates.append(value);
  }

  for (const auto &candidateObject : invalidCandidates) {
    auto mutated = base;
    mutated.insert(candidateKey, candidateObject);
    QVERIFY(!decode(canonicalOuter(mutated)));
  }

  auto record = validRecord(OrdinaryPendingKind::Apply);
  record.candidateSnapshotBytes.chop(1);
  QVERIFY(!encode(record));
  record = validRecord(OrdinaryPendingKind::Apply);
  record.candidateSnapshot.semanticState.targetHyprland =
      QStringLiteral("0.56.1");
  QVERIFY(!encode(record));
}

void CompositorOrdinaryPendingRecordTest::
    snapshotDigestExcludesExactlyTheFinalLf() {
  auto record = validRecord(OrdinaryPendingKind::Apply);
  const auto excluded = digestWithoutFinalLf(record.candidateSnapshotBytes);
  const auto included = digestIncludingFinalLf(record.candidateSnapshotBytes);
  QVERIFY(!excluded.isEmpty());
  QVERIFY(!included.isEmpty());
  QVERIFY(excluded != included);
  QCOMPARE(record.snapshotDigest, excluded);
  QCOMPARE(record.afterActivation.snapshotDigest, excluded);

  const auto encoded = encode(record);
  QVERIFY(encoded);
  const auto parsed = decode(*encoded.value);
  QVERIFY(parsed);
  QCOMPARE(parsed.value->snapshotDigest, excluded);

  record.snapshotDigest = included;
  record.beforeDesiredDigest = included;
  record.afterActivation.snapshotDigest = included;
  QVERIFY(!encode(record));
}

void CompositorOrdinaryPendingRecordTest::onlyPreparedToCommittingIsLegal() {
  QVERIFY(isLegalOrdinaryPendingPhaseTransition(
      OrdinaryPendingPhase::Prepared, OrdinaryPendingPhase::Committing));
  QVERIFY(!isLegalOrdinaryPendingPhaseTransition(
      OrdinaryPendingPhase::Prepared, OrdinaryPendingPhase::Prepared));
  QVERIFY(!isLegalOrdinaryPendingPhaseTransition(
      OrdinaryPendingPhase::Committing, OrdinaryPendingPhase::Prepared));
  QVERIFY(!isLegalOrdinaryPendingPhaseTransition(
      OrdinaryPendingPhase::Committing, OrdinaryPendingPhase::Committing));

  auto prepared = validRecord(OrdinaryPendingKind::Recovery);
  auto committing = prepared;
  committing.phase = OrdinaryPendingPhase::Committing;
  const auto preparedObject = objectFromOuter(*encode(prepared).value);
  const auto committingObject = objectFromOuter(*encode(committing).value);
  auto normalized = committingObject;
  normalized.insert(QStringLiteral("phase"), QStringLiteral("prepared"));
  QCOMPARE(normalized, preparedObject);
}

void CompositorOrdinaryPendingRecordTest::
    byteCapAndNearLargeCandidateRoundTrip() {
  const QByteArray oversized(maximumOrdinaryPendingRecordV2Bytes + 1, ' ');
  const auto rejected = decode(oversized);
  QVERIFY(!rejected);
  QVERIFY(!rejected.value.has_value());

  auto record = validRecord(OrdinaryPendingKind::Apply,
                            OrdinaryPendingPhase::Prepared, true, 7, 7);
  record.candidateSnapshot.semanticState.environment.clear();
  const QString largeValue(maximumStateStringLength, QLatin1Char('x'));
  for (qsizetype index = 0; index < maximumEnvironmentVariables; ++index) {
    record.candidateSnapshot.semanticState.environment.append({
        .id = QStringLiteral("large-%1").arg(index),
        .name = QStringLiteral("D052_LARGE_%1").arg(index),
        .value = largeValue,
        .scope = EnvironmentScope::Hyprland,
    });
  }
  record.candidateSnapshotBytes = candidateBytes(record.candidateSnapshot);
  QVERIFY(record.candidateSnapshotBytes.size() > 2 * 1024 * 1024);
  record.snapshotDigest = digestWithoutFinalLf(record.candidateSnapshotBytes);
  record.beforeDesiredDigest = record.snapshotDigest;
  record.afterActivation.snapshotDigest = record.snapshotDigest;
  record.beforeActivation->snapshotDigest = record.snapshotDigest;
  record.beforeActivationDesired = OrdinaryPendingDesiredMaterialV2{
      .state = record.candidateSnapshot,
      .bytes = record.candidateSnapshotBytes,
  };

  const auto encoded = encode(record);
  QVERIFY(encoded);
  QVERIFY(encoded.value->size() >
          maximumDesiredStateBytes + qsizetype(16 * 1024));
  QVERIFY(encoded.value->size() <= maximumOrdinaryPendingRecordV2Bytes);
  const auto parsed = decode(*encoded.value);
  QVERIFY(parsed);
  QCOMPARE(*parsed.value, record);
  QCOMPARE(parsed.value->beforeActivationDesired->bytes,
           parsed.value->candidateSnapshotBytes);
}

void CompositorOrdinaryPendingRecordTest::
    numericAndUnicodeCandidateRoundTripsAcrossBothDomains() {
  const auto record =
      validRecord(OrdinaryPendingKind::Apply, OrdinaryPendingPhase::Prepared,
                  false, 23, 0, true);
  const auto encoded = encode(record);
  QVERIFY2(encoded,
           qPrintable(encoded.errors.isEmpty()
                          ? QStringLiteral("encode failed without an error")
                          : encoded.errors.constFirst().code +
                                QStringLiteral(" at ") +
                                encoded.errors.constFirst().path +
                                QStringLiteral(": ") +
                                encoded.errors.constFirst().message));
  const auto parsed = decode(*encoded.value);
  QVERIFY(parsed);
  QCOMPARE(*parsed.value, record);
  QCOMPARE(
      parsed.value->candidateSnapshotBytes,
      *serializeDormantDesiredStateV2(parsed.value->candidateSnapshot).value);
  QCOMPARE(parsed.value->candidateSnapshot.semanticState.overrides
               .value(QStringLiteral("hyprland.input.sensitivity"))
               .toDouble(),
           0.375);
  QCOMPARE(
      parsed.value->candidateSnapshot.semanticState.environment.constFirst()
          .value,
      record.candidateSnapshot.semanticState.environment.constFirst().value);

  const auto outer = objectFromOuter(*encoded.value);
  QVERIFY(outer.value(QStringLiteral("candidateSnapshot")).isObject());
  const auto semanticCandidate =
      outer.value(QStringLiteral("candidateSnapshot")).toObject();
  QVERIFY(!semanticCandidate.isEmpty());
  // Deliberately no assertion compares this outer JCS spelling with the
  // standalone Desired serializer bytes. Equality is semantic and the
  // standalone bytes above are reconstructed only through the typed codec.
}

QTEST_APPLESS_MAIN(CompositorOrdinaryPendingRecordTest)

#include "compositor_ordinary_pending_record_test.moc"
