#include "hyprland/action_catalog.h"
#include "hyprland/catalog.h"
#include "hyprland/desired_state.h"
#include "hyprland/json_support.h"

#include <QCryptographicHash>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMap>
#include <QRegularExpression>
#include <QSet>
#include <QtTest>

#include <algorithm>
#include <array>
#include <limits>
#include <tuple>

using namespace HyprShelld::Hyprland;

namespace {

const QString testAuthorityId =
    QStringLiteral("0123456789abcdef0123456789abcdef");

[[nodiscard]] QByteArray readBytes(const QString &path) {
  QFile file(path);
  if (!file.open(QIODevice::ReadOnly)) {
    return {};
  }
  return file.readAll();
}

[[nodiscard]] QJsonObject readObject(const QString &path) {
  QJsonParseError error;
  const auto document = QJsonDocument::fromJson(readBytes(path), &error);
  if (error.error != QJsonParseError::NoError || !document.isObject()) {
    return {};
  }
  return document.object();
}

[[nodiscard]] QByteArray encode(QJsonObject object) {
  auto bytes = QJsonDocument(std::move(object)).toJson(QJsonDocument::Compact);
  bytes.append('\n');
  return bytes;
}

[[nodiscard]] bool hasCode(const ValidationErrors &errors,
                           const QString &code) {
  return std::ranges::any_of(errors, [&code](const ValidationError &error) {
    return error.code == code;
  });
}

[[nodiscard]] bool hasErrorAt(const ValidationErrors &errors,
                              const QString &path) {
  return std::ranges::any_of(errors, [&path](const ValidationError &error) {
    return error.path == path;
  });
}

[[nodiscard]] QString describeErrors(const ValidationErrors &errors) {
  QStringList descriptions;
  for (const auto &error : errors) {
    descriptions.append(error.path + QStringLiteral(":") + error.code);
  }
  return descriptions.join(QStringLiteral(", "));
}

[[nodiscard]] ValidationResult<Catalog> shippedCatalog() {
  return parseCatalog(
      readBytes(QStringLiteral(HYPRSHELLD_HYPRLAND_CATALOG_FILE)));
}

[[nodiscard]] ValidationResult<ActionCatalog> shippedActionCatalog() {
  return parseActionCatalog(
      readBytes(QStringLiteral(HYPRSHELLD_HYPRLAND_ACTION_CATALOG_FILE)),
      readBytes(QStringLiteral(HYPRSHELLD_HYPRLAND_CONFIG_SCHEMA_FILE)));
}

[[nodiscard]] QString dormantV2ActionCatalogPath() {
  return QStringLiteral(HYPRSHELLD_HYPRLAND_V2_ACTION_CATALOG_FILE);
}

[[nodiscard]] QString dormantV2ConfigSchemaPath() {
  return QStringLiteral(HYPRSHELLD_HYPRLAND_V2_CONFIG_SCHEMA_FILE);
}

[[nodiscard]] QString dormantV2TemplatePath() {
  return QStringLiteral(HYPRSHELLD_HYPRLAND_V2_TEMPLATE_FILE);
}

[[nodiscard]] ValidationResult<Catalog> dormantV2Catalog() {
  return parseDormantCatalogV2(
      readBytes(QStringLiteral(HYPRSHELLD_HYPRLAND_V2_CATALOG_FILE)));
}

[[nodiscard]] ValidationResult<ActionCatalog> dormantV2ActionCatalog() {
  return parseDormantActionCatalogV2(readBytes(dormantV2ActionCatalogPath()),
                                     readBytes(dormantV2ConfigSchemaPath()));
}

[[nodiscard]] QByteArray
actionCatalogForSchema(QJsonObject object, const QByteArrayView schemaBytes) {
  object.insert(QStringLiteral("configSchemaDigest"),
                QString::fromLatin1(QCryptographicHash::hash(
                                        schemaBytes, QCryptographicHash::Sha256)
                                        .toHex()));
  return encode(std::move(object));
}

[[nodiscard]] QByteArray
actionCatalogForSchema(const QByteArrayView schemaBytes) {
  return actionCatalogForSchema(
      readObject(QStringLiteral(HYPRSHELLD_HYPRLAND_ACTION_CATALOG_FILE)),
      schemaBytes);
}

[[nodiscard]] QJsonObject defaultStateObject() {
  return readObject(QStringLiteral(HYPRSHELLD_HYPRLAND_DEFAULTS_FILE));
}

[[nodiscard]] ValidationResult<DesiredState>
parseState(QJsonObject object, const Catalog &catalog) {
  const auto actions = shippedActionCatalog();
  if (!actions) {
    ValidationResult<DesiredState> result;
    result.errors = actions.errors;
    return result;
  }
  return parseDesiredState(encode(std::move(object)), catalog, *actions.value);
}

[[nodiscard]] ValidationResult<DesiredState>
parseStateBytes(const QByteArrayView bytes, const Catalog &catalog) {
  const auto actions = shippedActionCatalog();
  if (!actions) {
    ValidationResult<DesiredState> result;
    result.errors = actions.errors;
    return result;
  }
  return parseDesiredState(bytes, catalog, *actions.value);
}

[[nodiscard]] QJsonObject binding(const QString &id, QJsonArray modifiers,
                                  const QString &key,
                                  const QString &description) {
  return {
      {QStringLiteral("id"), id},
      {QStringLiteral("modifiers"), std::move(modifiers)},
      {QStringLiteral("key"), key},
      {QStringLiteral("actionType"), QStringLiteral("defaultApp")},
      {QStringLiteral("action"), QStringLiteral("defaultApp.terminal")},
      {QStringLiteral("arguments"), QJsonObject{}},
      {QStringLiteral("description"), description},
      {QStringLiteral("enabled"), true},
      {QStringLiteral("submap"), QString()},
      {QStringLiteral("options"),
       QJsonObject{
           {QStringLiteral("repeating"), false},
           {QStringLiteral("locked"), false},
           {QStringLiteral("release"), false},
           {QStringLiteral("nonConsuming"), false},
           {QStringLiteral("autoConsuming"), false},
           {QStringLiteral("transparent"), false},
           {QStringLiteral("ignoreMods"), false},
           {QStringLiteral("dontInhibit"), false},
           {QStringLiteral("longPress"), false},
           {QStringLiteral("submapUniversal"), false},
           {QStringLiteral("click"), false},
           {QStringLiteral("drag"), false},
           {QStringLiteral("allowInputCapture"), false},
       }},
  };
}

[[nodiscard]] QStringList optionPaths(const Catalog &catalog) {
  QStringList result;
  result.reserve(catalog.options.size());
  for (const auto &option : catalog.options) {
    result.append(option.path);
  }
  return result;
}

[[nodiscard]] const OptionDefinition *optionWithPath(const Catalog &catalog,
                                                     const QString &path) {
  const auto iterator =
      std::ranges::find(catalog.options, path, &OptionDefinition::path);
  return iterator == catalog.options.end() ? nullptr : &*iterator;
}

[[nodiscard]] QMap<QString, QJsonObject>
inventoryByPath(const QJsonObject &fixture) {
  QMap<QString, QJsonObject> result;
  for (const auto &value : fixture.value(QStringLiteral("options")).toArray()) {
    const auto option = value.toObject();
    result.insert(option.value(QStringLiteral("path")).toString(), option);
  }
  return result;
}

[[nodiscard]] QSet<QString> jsonStringSet(const QJsonArray &array) {
  QSet<QString> result;
  for (const auto &value : array) {
    result.insert(value.toString());
  }
  return result;
}

[[nodiscard]] QSet<QString> stringSet(const QStringList &values) {
  return QSet<QString>(values.cbegin(), values.cend());
}

} // namespace

class HyprlandConfigurationTest final : public QObject {
  Q_OBJECT

private slots:
  void parsesThePinned0561Catalog() {
    const auto parsed = shippedCatalog();
    QVERIFY2(parsed,
             qPrintable(parsed.errors.isEmpty()
                            ? QStringLiteral("catalog parser returned no value")
                            : parsed.errors.constFirst().message));
    QCOMPARE(parsed.value->contractVersion, quint32(1));
    QCOMPARE(parsed.value->hyprland.major, quint32(0));
    QCOMPARE(parsed.value->hyprland.minor, quint32(56));
    QVERIFY(parsed.value->hyprland.reviewedVersion ==
            (SemanticVersion{0, 56, 1}));
    QCOMPARE(parsed.value->hyprland.reviewedTag, QStringLiteral("v0.56.1"));
    QCOMPARE(parsed.value->hyprland.reviewedCommit,
             QStringLiteral("5c9377c15f85c50648f35ca5a213754f95b93ca0"));
    QCOMPARE(parsed.value->hyprland.repository,
             QStringLiteral("https://github.com/hyprwm/Hyprland"));
    QCOMPARE(parsed.value->hyprland.minimumPatch, quint32(0));
    QVERIFY(!parsed.value->hyprland.maximumPatch.has_value());
    QCOMPARE(parsed.value->options.size(), qsizetype(353));

    const auto paths = optionPaths(*parsed.value);
    auto uniquePaths = paths;
    uniquePaths.removeDuplicates();
    QCOMPARE(uniquePaths.size(), paths.size());
    QVERIFY(std::ranges::is_sorted(paths));

    QStringList ids;
    ids.reserve(parsed.value->options.size());
    for (const auto &option : parsed.value->options) {
      ids.append(option.id);
      QVERIFY2(!option.description.isEmpty(), qPrintable(option.path));
      QVERIFY2((option.since <= SemanticVersion{0, 56, 1}),
               qPrintable(option.path));
    }
    auto uniqueIds = ids;
    uniqueIds.removeDuplicates();
    QCOMPARE(uniqueIds.size(), ids.size());
  }

  void canonicalCatalogAndDigestAreStable() {
    const auto parsed = shippedCatalog();
    QVERIFY(parsed);
    const auto canonical = canonicalCatalogJson(*parsed.value);
    QVERIFY(!canonical.isEmpty());

    const auto digest = catalogDigest(*parsed.value);
    QVERIFY(QRegularExpression(QStringLiteral("^[0-9a-f]{64}$"))
                .match(digest)
                .hasMatch());
    QCOMPARE(
        digest,
        QStringLiteral(
            "402c8a8c570dd3760d4d7bea8c358c7f12021a7c51457e62a4771d69a581254b"
        )
    );
    QCOMPARE(digest, QString::fromLatin1(reviewedCatalogDigest));
    QCOMPARE(parsed.value->digest, digest);
    QCOMPARE(digest,
             QString::fromLatin1(
                 QCryptographicHash::hash(canonical, QCryptographicHash::Sha256)
                     .toHex()));

    const auto reparsed = parseCatalog(canonical);
    QVERIFY(reparsed);
    QCOMPARE(canonicalCatalogJson(*reparsed.value), canonical);
    QCOMPARE(catalogDigest(*reparsed.value), digest);

    const auto indented =
        QJsonDocument::fromJson(canonical).toJson(QJsonDocument::Indented);
    const auto reformatted = parseCatalog(indented);
    QVERIFY(reformatted);
    QCOMPARE(catalogDigest(*reformatted.value), digest);
  }

  void parsesAndBindsThePinnedActionCatalog() {
    const auto parsed = shippedActionCatalog();
    QVERIFY2(parsed,
             qPrintable(
                 parsed.errors.isEmpty()
                     ? QStringLiteral("action catalog parser returned no value")
                     : parsed.errors.constFirst().path + QStringLiteral(": ") +
                           parsed.errors.constFirst().code +
                           QStringLiteral(": ") +
                           parsed.errors.constFirst().message));
    QCOMPARE(parsed.value->contractVersion, quint32(1));
    QVERIFY(parsed.value->reviewedVersion == (SemanticVersion{0, 56, 1}));
    QCOMPARE(parsed.value->dispatcherActions.size(), qsizetype(47));
    QCOMPARE(parsed.value->semanticActions.size(), qsizetype(29));
    QCOMPARE(parsed.value->gestureActions.size(), qsizetype(10));
    QCOMPARE(parsed.value->excluded.size(), qsizetype(9));
    QCOMPARE(parsed.value->reviewedTag, QStringLiteral("v0.56.1"));
    QCOMPARE(parsed.value->reviewedCommit,
             QStringLiteral("5c9377c15f85c50648f35ca5a213754f95b93ca0"));
    QCOMPARE(parsed.value->source.repository,
             QStringLiteral("https://github.com/hyprwm/Hyprland"));
    QCOMPARE(parsed.value->source.tag, parsed.value->reviewedTag);
    QCOMPARE(parsed.value->source.commit, parsed.value->reviewedCommit);

    const auto presentationIsSafe = [](const QString &text) {
      if (text.isEmpty() ||
          text != text.normalized(QString::NormalizationForm_C)) {
        return false;
      }
      return std::ranges::none_of(text, [](const QChar character) {
        return character.unicode() <= 0x1f || character.unicode() == 0x7f;
      });
    };
    const auto verifyPresentation = [&presentationIsSafe](
                                        const ActionDefinition &action) {
      return action.label.size() <= 128 && action.description.size() <= 512 &&
             presentationIsSafe(action.label) &&
             presentationIsSafe(action.description);
    };

    const auto schemaBytes =
        readBytes(QStringLiteral(HYPRSHELLD_HYPRLAND_CONFIG_SCHEMA_FILE));
    QCOMPARE(parsed.value->configSchemaDigest,
             QString::fromLatin1(QCryptographicHash::hash(
                                     schemaBytes, QCryptographicHash::Sha256)
                                     .toHex()));
    const auto canonical = canonicalActionCatalogJson(*parsed.value);
    QVERIFY(!canonical.isEmpty());
    auto digestInput = canonical;
    digestInput.append('\n');
    digestInput.append(schemaBytes);
    QCOMPARE(actionCatalogDigest(*parsed.value),
             QString::fromLatin1(QCryptographicHash::hash(
                                     digestInput, QCryptographicHash::Sha256)
                                     .toHex()));
    QCOMPARE(parsed.value->digest, actionCatalogDigest(*parsed.value));
    QCOMPARE(parsed.value->digest,
             QString::fromLatin1(reviewedActionCatalogDigest));

    QSet<QString> dispatcherIds;
    for (const auto &action : parsed.value->dispatcherActions) {
      QVERIFY2(!dispatcherIds.contains(action.id), qPrintable(action.id));
      dispatcherIds.insert(action.id);
      QVERIFY(action.kind == ActionKind::Dispatcher);
      QVERIFY2(verifyPresentation(action), qPrintable(action.id));
      QVERIFY(action.luaPath.size() >= 2);
      QVERIFY(action.payloadContract != nullptr);
      QVERIFY(action.schemaReference.startsWith(
          QStringLiteral("config.schema.json#/$defs/")));
    }
    QSet<QString> semanticIds;
    for (const auto &action : parsed.value->semanticActions) {
      QVERIFY2(!semanticIds.contains(action.id), qPrintable(action.id));
      semanticIds.insert(action.id);
      QVERIFY(action.kind == ActionKind::DefaultApp ||
              action.kind == ActionKind::HyprShelld);
      QVERIFY2(verifyPresentation(action), qPrintable(action.id));
      QVERIFY(action.luaPath.isEmpty());
      QVERIFY(action.payloadContract != nullptr);
    }
    QSet<QString> gestureIds;
    for (const auto &action : parsed.value->gestureActions) {
      QVERIFY2(!gestureIds.contains(action.id), qPrintable(action.id));
      gestureIds.insert(action.id);
      QVERIFY(action.kind == ActionKind::Gesture);
      QVERIFY2(verifyPresentation(action), qPrintable(action.id));
      QVERIFY(action.luaPath.size() == 1);
      QVERIFY(action.payloadContract != nullptr);
    }
    QVERIFY((dispatcherIds & semanticIds).isEmpty());
    QVERIFY((dispatcherIds & gestureIds).isEmpty());
    QVERIFY((semanticIds & gestureIds).isEmpty());

    QCOMPARE(gestureIds, stringSet({
                             QStringLiteral("close"),
                             QStringLiteral("cursorZoom"),
                             QStringLiteral("float"),
                             QStringLiteral("fullscreen"),
                             QStringLiteral("move"),
                             QStringLiteral("resize"),
                             QStringLiteral("scrollMove"),
                             QStringLiteral("special"),
                             QStringLiteral("unset"),
                             QStringLiteral("workspace"),
                         }));
    QSet<QString> excluded;
    for (const auto &entry : parsed.value->excluded) {
      QVERIFY2(!entry.reason.isEmpty(), qPrintable(entry.id));
      excluded.insert(entry.id);
    }
    QCOMPARE(excluded, stringSet({
                           QStringLiteral("callback"),
                           QStringLiteral("exec_cmd"),
                           QStringLiteral("exec_raw"),
                           QStringLiteral("group"),
                           QStringLiteral("layout"),
                           QStringLiteral("mouse"),
                           QStringLiteral("on_created_empty"),
                           QStringLiteral("switch:*"),
                           QStringLiteral("window.set_prop"),
                       }));
    QVERIFY(!dispatcherIds.contains(QStringLiteral("exec_cmd")));
    QVERIFY(!dispatcherIds.contains(QStringLiteral("exec_raw")));
  }

  void dormantV2AuthorityEnvelopeIsParallelStrictAndSemanticOnly() {
    QCOMPARE(currentCatalogContractVersion, quint32(1));
    QCOMPARE(currentActionCatalogContractVersion, quint32(1));
    QCOMPARE(currentDesiredStateFormatVersion, quint32(1));
    QCOMPARE(dormantCatalogV2ContractVersion, quint32(2));
    QCOMPARE(dormantActionCatalogV2ContractVersion, quint32(2));
    QCOMPARE(dormantDesiredStateV2FormatVersion, quint32(2));
    QCOMPARE(maximumUserWorkspaceRules, qsizetype(1024));
    QCOMPARE(maximumWorkspaceRules, qsizetype(1025));

    const auto v1Catalog = shippedCatalog();
    const auto v2Catalog = dormantV2Catalog();
    const auto v1Actions = shippedActionCatalog();
    const auto v2Actions = dormantV2ActionCatalog();
    QVERIFY(v1Catalog);
    QVERIFY2(v2Catalog, qPrintable(describeErrors(v2Catalog.errors)));
    QVERIFY(v1Actions);
    QVERIFY2(v2Actions, qPrintable(describeErrors(v2Actions.errors)));

    const auto sourceManifest = readObject(
        QStringLiteral(HYPRSHELLD_HYPRLAND_V2_SOURCE_MANIFEST_FILE));
    QVERIFY(!sourceManifest.isEmpty());
    const auto sourceManifestDigest = QString::fromLatin1(
        QCryptographicHash::hash(JsonSupport::canonicalJson(sourceManifest),
                                 QCryptographicHash::Sha256)
            .toHex());
    QCOMPARE(sourceManifestDigest,
             QString::fromLatin1(dormantReviewedSourceManifestDigest));

    QCOMPARE(v2Catalog.value->contractVersion, quint32(2));
    QCOMPARE(v2Catalog.value->digest,
             QString::fromLatin1(dormantReviewedCatalogV2Digest));
    QCOMPARE(v2Catalog.value->sourceManifestDigest, sourceManifestDigest);
    QVERIFY(v2Catalog.value->hyprland.reviewedVersion ==
            (SemanticVersion{0, 56, 2}));
    QCOMPARE(v2Catalog.value->hyprland.reviewedTag,
             QStringLiteral("v0.56.2"));
    QCOMPARE(v2Catalog.value->hyprland.reviewedCommit,
             QStringLiteral(
                 "efb50993780079460b0cbed1363e2166a2de1d9f"));
    QCOMPARE(v2Catalog.value->hyprland.minimumPatch, quint32(2));
    QCOMPARE(v2Catalog.value->hyprland.maximumPatch,
             std::optional<quint32>{2});
    QVERIFY(v2Catalog.value->compatibility.minimumSupported ==
            (SemanticVersion{0, 56, 2}));
    QCOMPARE(v2Catalog.value->compatibility.fullyQualified,
             QStringList{QStringLiteral("0.56.2")});

    QCOMPARE(v2Actions.value->contractVersion, quint32(2));
    QCOMPARE(v2Actions.value->digest,
             QString::fromLatin1(dormantReviewedActionCatalogV2Digest));
    QCOMPARE(v2Actions.value->sourceManifestDigest, sourceManifestDigest);
    QVERIFY(v2Actions.value->reviewedVersion ==
            (SemanticVersion{0, 56, 2}));
    QCOMPARE(v2Actions.value->reviewedTag, QStringLiteral("v0.56.2"));
    QCOMPARE(v2Actions.value->reviewedCommit,
             QStringLiteral(
                 "efb50993780079460b0cbed1363e2166a2de1d9f"));
    QCOMPARE(v2Actions.value->minimumPatch, quint32(2));
    QCOMPARE(v2Actions.value->maximumPatch, std::optional<quint32>{2});
    QCOMPARE(v2Actions.value->source.repository,
             QStringLiteral("https://github.com/hyprwm/Hyprland"));
    QCOMPARE(v2Actions.value->source.tag, QStringLiteral("v0.56.2"));
    QCOMPARE(v2Actions.value->source.commit,
             QStringLiteral(
                 "efb50993780079460b0cbed1363e2166a2de1d9f"));
    QCOMPARE(v2Actions.value->source.path,
             QStringLiteral(
                 "src/config/lua/bindings/LuaBindingsDispatchers.cpp"));
    QCOMPARE(v2Actions.value->source.sha256,
             QStringLiteral(
                 "a109eeb982856e0fe2ac9d88c29115a09984511787e19a20e7b4804e14a9d4de"));
    QCOMPARE(v2Actions.value->configSchemaDocument,
             readBytes(dormantV2ConfigSchemaPath()));
    auto v2Composite = canonicalActionCatalogJson(*v2Actions.value);
    v2Composite.append('\n');
    v2Composite.append(readBytes(dormantV2ConfigSchemaPath()));
    QCOMPARE(v2Actions.value->digest,
             QString::fromLatin1(
                 QCryptographicHash::hash(v2Composite,
                                          QCryptographicHash::Sha256)
                     .toHex()));

    const auto v1CatalogObject = readObject(
        QStringLiteral(HYPRSHELLD_HYPRLAND_CATALOG_FILE));
    const auto v2CatalogObject = readObject(
        QStringLiteral(HYPRSHELLD_HYPRLAND_V2_CATALOG_FILE));
    QCOMPARE(v2CatalogObject.value(QStringLiteral("options")),
             v1CatalogObject.value(QStringLiteral("options")));
    QCOMPARE(v2CatalogObject.value(QStringLiteral("complexSurfaces")),
             v1CatalogObject.value(QStringLiteral("complexSurfaces")));

    auto v1ActionObject = readObject(
        QStringLiteral(HYPRSHELLD_HYPRLAND_ACTION_CATALOG_FILE));
    auto v2ActionObject = readObject(dormantV2ActionCatalogPath());
    for (const auto &key : {
             QStringLiteral("dispatcherActions"),
             QStringLiteral("semanticActions"),
             QStringLiteral("gestureActions"),
             QStringLiteral("excluded"),
         }) {
      QCOMPARE(v2ActionObject.value(key), v1ActionObject.value(key));
    }

    const auto v2CatalogWithProductionParser = parseCatalog(
        readBytes(QStringLiteral(HYPRSHELLD_HYPRLAND_V2_CATALOG_FILE)));
    QVERIFY(!v2CatalogWithProductionParser);
    QVERIFY(hasCode(v2CatalogWithProductionParser.errors,
                    QStringLiteral("catalog.unsupported-contract-version")));
    const auto v1CatalogWithDormantParser = parseDormantCatalogV2(
        readBytes(QStringLiteral(HYPRSHELLD_HYPRLAND_CATALOG_FILE)));
    QVERIFY(!v1CatalogWithDormantParser);
    QVERIFY(hasCode(v1CatalogWithDormantParser.errors,
                    QStringLiteral("catalog.unsupported-contract-version")));

    const auto v2WithProductionParser = parseActionCatalog(
        readBytes(dormantV2ActionCatalogPath()),
        readBytes(dormantV2ConfigSchemaPath()));
    QVERIFY(!v2WithProductionParser);
    QVERIFY(hasCode(v2WithProductionParser.errors,
                    QStringLiteral(
                        "action-catalog.unsupported-contract-version")));
    const auto v1WithDormantParser = parseDormantActionCatalogV2(
        readBytes(QStringLiteral(HYPRSHELLD_HYPRLAND_ACTION_CATALOG_FILE)),
        readBytes(QStringLiteral(HYPRSHELLD_HYPRLAND_CONFIG_SCHEMA_FILE)));
    QVERIFY(!v1WithDormantParser);
    QVERIFY(hasCode(v1WithDormantParser.errors,
                    QStringLiteral(
                        "action-catalog.unsupported-contract-version")));

    auto unboundCatalogObject = v2CatalogObject;
    unboundCatalogObject.insert(QStringLiteral("sourceManifestDigest"),
                                QString(64, QLatin1Char('0')));
    const auto unboundCatalog = parseDormantCatalogV2(
        encode(std::move(unboundCatalogObject)));
    QVERIFY(!unboundCatalog);
    QVERIFY(hasCode(unboundCatalog.errors,
                    QStringLiteral(
                        "catalog.source-manifest-digest-mismatch")));

    auto unboundActionObject = v2ActionObject;
    unboundActionObject.insert(QStringLiteral("sourceManifestDigest"),
                               QString(64, QLatin1Char('0')));
    const auto unboundActions = parseDormantActionCatalogV2(
        encode(std::move(unboundActionObject)),
        readBytes(dormantV2ConfigSchemaPath()));
    QVERIFY(!unboundActions);
    QVERIFY(hasCode(unboundActions.errors,
                    QStringLiteral(
                        "action-catalog.source-manifest-digest-mismatch")));

    QVERIFY(isCanonicalAuthorityId(testAuthorityId));
    for (const auto &authorityId : {
             QString(),
             QString(32, QLatin1Char('0')),
             QStringLiteral("0123456789ABCDEF0123456789ABCDEF"),
             QStringLiteral("0123456789abcdef0123456789abcde"),
             QStringLiteral("g123456789abcdef0123456789abcdef"),
         }) {
      QVERIFY(!isCanonicalAuthorityId(authorityId));
      const auto invalidDefault = defaultDormantDesiredStateV2(
          *v2Catalog.value, *v2Actions.value, authorityId);
      QVERIFY(!invalidDefault);
      QVERIFY(hasCode(invalidDefault.errors,
                      QStringLiteral("state.invalid-authority-id")));
    }

    const auto defaultV2 = defaultDormantDesiredStateV2(
        *v2Catalog.value, *v2Actions.value, testAuthorityId);
    QVERIFY2(defaultV2, qPrintable(describeErrors(defaultV2.errors)));
    QCOMPARE(defaultV2.value->authorityId, testAuthorityId);
    QCOMPARE(defaultV2.value->semanticState.formatVersion, quint32(1));
    QCOMPARE(defaultV2.value->semanticState.targetHyprland,
             QStringLiteral("0.56.2"));
    QCOMPARE(defaultV2.value->semanticState.catalogDigest,
             QString::fromLatin1(dormantReviewedCatalogV2Digest));
    QCOMPARE(defaultV2.value->semanticState.actionCatalogDigest,
             QString::fromLatin1(dormantReviewedActionCatalogV2Digest));
    QCOMPARE(defaultV2.value->semanticState.workspaceRules.size(),
             qsizetype(1));
    QCOMPARE(defaultV2.value->semanticState.workspaceRules.constFirst().id,
             QString::fromLatin1(sharedSpacingWorkspaceRuleId));
    QCOMPARE(
        defaultV2.value->semanticState.workspaceRules.constFirst().selector,
        QString::fromLatin1(sharedSpacingWorkspaceRuleSelector));

    const auto encoded = serializeDormantDesiredStateV2(*defaultV2.value);
    QVERIFY2(encoded, qPrintable(describeErrors(encoded.errors)));
    const auto runtimeObject =
        QJsonDocument::fromJson(*encoded.value).object();
    QCOMPARE(runtimeObject.value(QStringLiteral("formatVersion")).toInt(), 2);
    QCOMPARE(runtimeObject.value(QStringLiteral("authorityId")).toString(),
             testAuthorityId);
    auto templateProjection = runtimeObject;
    templateProjection.remove(QStringLiteral("authorityId"));
    QCOMPARE(templateProjection, readObject(dormantV2TemplatePath()));

    const auto reparsed = parseDormantDesiredStateV2(
        *encoded.value, *v2Catalog.value, *v2Actions.value);
    QVERIFY2(reparsed, qPrintable(describeErrors(reparsed.errors)));
    QCOMPARE(*reparsed.value, *defaultV2.value);

    const auto formatOneBoundToV2 = serializeDesiredState(
        defaultV2.value->semanticState);
    const auto rejectedV2AuthoritiesInProduction = parseDesiredState(
        formatOneBoundToV2, *v2Catalog.value, *v2Actions.value);
    QVERIFY(!rejectedV2AuthoritiesInProduction);
    QVERIFY(hasCode(
        rejectedV2AuthoritiesInProduction.errors,
        QStringLiteral("state.active-v1-catalog-authority-required")));
    QVERIFY(hasCode(
        rejectedV2AuthoritiesInProduction.errors,
        QStringLiteral("state.active-v1-action-authority-required")));

    const auto rejectedByV1 = parseDesiredState(
        *encoded.value, *v1Catalog.value, *v1Actions.value);
    QVERIFY(!rejectedByV1);

    const auto v1CatalogRejectedByDormantState = parseDormantDesiredStateV2(
        *encoded.value, *v1Catalog.value, *v2Actions.value);
    QVERIFY(!v1CatalogRejectedByDormantState);
    QVERIFY(hasCode(v1CatalogRejectedByDormantState.errors,
                    QStringLiteral(
                        "state.dormant-v2-catalog-authority-required")));
    const auto v1ActionsRejectedByDormantState = parseDormantDesiredStateV2(
        *encoded.value, *v2Catalog.value, *v1Actions.value);
    QVERIFY(!v1ActionsRejectedByDormantState);
    QVERIFY(hasCode(v1ActionsRejectedByDormantState.errors,
                    QStringLiteral(
                        "state.dormant-v2-action-authority-required")));

    auto invalidRuntime = runtimeObject;
    for (const auto &authorityId : {
             QString(32, QLatin1Char('0')),
             QStringLiteral("0123456789ABCDEF0123456789ABCDEF"),
             QStringLiteral("0123456789abcdef0123456789abcde"),
         }) {
      invalidRuntime.insert(QStringLiteral("authorityId"), authorityId);
      const auto invalid = parseDormantDesiredStateV2(
          encode(invalidRuntime), *v2Catalog.value, *v2Actions.value);
      QVERIFY(!invalid);
      QVERIFY(hasCode(invalid.errors,
                      QStringLiteral("state.invalid-authority-id")));
    }

    auto invalidEnvelope = *defaultV2.value;
    invalidEnvelope.semanticState.formatVersion = 2;
    const auto invalidSerialization =
        serializeDormantDesiredStateV2(invalidEnvelope);
    QVERIFY(!invalidSerialization);
    QVERIFY(hasCode(invalidSerialization.errors,
                    QStringLiteral("state.invalid-v2-semantic-envelope")));

    invalidEnvelope = *defaultV2.value;
    invalidEnvelope.semanticState.catalogDigest =
        QString::fromLatin1(reviewedCatalogDigest);
    const auto wrongCatalogSerialization =
        serializeDormantDesiredStateV2(invalidEnvelope);
    QVERIFY(!wrongCatalogSerialization);
    QVERIFY(hasCode(wrongCatalogSerialization.errors,
                    QStringLiteral("state.invalid-v2-semantic-envelope")));

    invalidEnvelope = *defaultV2.value;
    invalidEnvelope.semanticState.actionCatalogDigest =
        QString::fromLatin1(reviewedActionCatalogDigest);
    const auto wrongAuthoritySerialization =
        serializeDormantDesiredStateV2(invalidEnvelope);
    QVERIFY(!wrongAuthoritySerialization);
    QVERIFY(hasCode(wrongAuthoritySerialization.errors,
                    QStringLiteral("state.invalid-v2-semantic-envelope")));

    invalidEnvelope = *defaultV2.value;
    invalidEnvelope.semanticState.targetHyprland = QStringLiteral("0.56.1");
    const auto wrongTargetSerialization =
        serializeDormantDesiredStateV2(invalidEnvelope);
    QVERIFY(!wrongTargetSerialization);
    QVERIFY(hasCode(wrongTargetSerialization.errors,
                    QStringLiteral("state.invalid-v2-semantic-envelope")));

    invalidEnvelope = *defaultV2.value;
    invalidEnvelope.semanticState.readOnly = true;
    invalidEnvelope.semanticState.opaqueFutureDocument = QJsonObject{};
    const auto readOnlySerialization =
        serializeDormantDesiredStateV2(invalidEnvelope);
    QVERIFY(!readOnlySerialization);
    QVERIFY(hasCode(readOnlySerialization.errors,
                    QStringLiteral("state.invalid-v2-semantic-envelope")));

    invalidEnvelope = *defaultV2.value;
    invalidEnvelope.semanticState.opaqueFutureDocument = QJsonObject{};
    const auto opaqueSerialization =
        serializeDormantDesiredStateV2(invalidEnvelope);
    QVERIFY(!opaqueSerialization);
    QVERIFY(hasCode(opaqueSerialization.errors,
                    QStringLiteral("state.invalid-v2-semantic-envelope")));
  }

  void dormantV2ProtectedWorkspaceRuleMustBeFinalWhenPresent() {
    const auto catalog = dormantV2Catalog();
    const auto actions = dormantV2ActionCatalog();
    QVERIFY2(catalog, qPrintable(describeErrors(catalog.errors)));
    QVERIFY2(actions, qPrintable(describeErrors(actions.errors)));
    const auto initial = defaultDormantDesiredStateV2(
        *catalog.value, *actions.value, testAuthorityId);
    QVERIFY2(initial, qPrintable(describeErrors(initial.errors)));

    auto nonFinal = *initial.value;
    nonFinal.semanticState.workspaceRules.append(WorkspaceRule{
        .id = QStringLiteral("user-rule-after-protected"),
        .selector = QStringLiteral("1"),
        .enabled = true,
        .monitor = QString(),
        .persistent = false,
        .isDefault = false,
        .layout = QString(),
        .overrides = QJsonObject{},
    });
    const auto rejectedSerialization =
        serializeDormantDesiredStateV2(nonFinal);
    QVERIFY(!rejectedSerialization);
    QCOMPARE(rejectedSerialization.errors.size(), qsizetype(1));
    QCOMPARE(rejectedSerialization.errors.constFirst().path,
             QStringLiteral("$.workspaceRules"));
    QCOMPARE(
        rejectedSerialization.errors.constFirst().code,
        QStringLiteral(
            "state.dormant-v2-protected-workspace-rule-not-final"));

    auto nonFinalObject = QJsonDocument::fromJson(
                              serializeDesiredState(nonFinal.semanticState))
                              .object();
    nonFinalObject.insert(QStringLiteral("formatVersion"), 2);
    nonFinalObject.insert(QStringLiteral("authorityId"), testAuthorityId);
    const auto rejectedParse = parseDormantDesiredStateV2(
        encode(std::move(nonFinalObject)), *catalog.value, *actions.value);
    QVERIFY(!rejectedParse);
    QCOMPARE(rejectedParse.errors.size(), qsizetype(1));
    QCOMPARE(rejectedParse.errors.constFirst().path,
             QStringLiteral("$.workspaceRules"));
    QCOMPARE(
        rejectedParse.errors.constFirst().code,
        QStringLiteral(
            "state.dormant-v2-protected-workspace-rule-not-final"));

    auto absent = *initial.value;
    absent.semanticState.workspaceRules.clear();
    const auto absentEncoded = serializeDormantDesiredStateV2(absent);
    QVERIFY2(absentEncoded,
             qPrintable(describeErrors(absentEncoded.errors)));
    const auto absentReparsed = parseDormantDesiredStateV2(
        *absentEncoded.value, *catalog.value, *actions.value);
    QVERIFY2(absentReparsed,
             qPrintable(describeErrors(absentReparsed.errors)));
    QVERIFY(absentReparsed.value->semanticState.workspaceRules.isEmpty());
  }

  void dormantV2SerializationBoundsTheFinalAuthorityEnvelope() {
    const auto catalog = dormantV2Catalog();
    const auto actions = dormantV2ActionCatalog();
    QVERIFY2(catalog, qPrintable(describeErrors(catalog.errors)));
    QVERIFY2(actions, qPrintable(describeErrors(actions.errors)));
    const auto initial = defaultDormantDesiredStateV2(
        *catalog.value, *actions.value, testAuthorityId);
    QVERIFY2(initial, qPrintable(describeErrors(initial.errors)));

    auto nearCap = *initial.value;
    const auto fillerKey = QStringLiteral("dormant-v2-size-probe");
    nearCap.semanticState.overrides.insert(fillerKey, QString());
    const auto baseSemanticBytes =
        serializeDesiredState(nearCap.semanticState);
    QVERIFY(baseSemanticBytes.size() < maximumDesiredStateBytes);

    const auto fillerLength =
        maximumDesiredStateBytes - baseSemanticBytes.size();
    nearCap.semanticState.overrides.insert(
        fillerKey, QString(fillerLength, QLatin1Char('x')));
    const auto maximumSemanticBytes =
        serializeDesiredState(nearCap.semanticState);
    QCOMPARE(maximumSemanticBytes.size(), maximumDesiredStateBytes);

    const auto rejected = serializeDormantDesiredStateV2(nearCap);
    QVERIFY(!rejected);
    QCOMPARE(rejected.errors.size(), qsizetype(1));
    QCOMPARE(rejected.errors.constFirst().path, QStringLiteral("$"));
    QCOMPARE(rejected.errors.constFirst().code,
             QStringLiteral("json.size-limit"));
  }

  void dormantV2RevisionEndpointsRoundTripAndOverflowIsRejected() {
    const auto catalog = dormantV2Catalog();
    const auto actions = dormantV2ActionCatalog();
    QVERIFY2(catalog, qPrintable(describeErrors(catalog.errors)));
    QVERIFY2(actions, qPrintable(describeErrors(actions.errors)));
    const auto initial = defaultDormantDesiredStateV2(
        *catalog.value, *actions.value, testAuthorityId);
    QVERIFY2(initial, qPrintable(describeErrors(initial.errors)));

    for (const auto revision : std::array<quint64, 2>{
             0,
             std::numeric_limits<quint64>::max(),
         }) {
      auto endpoint = *initial.value;
      endpoint.semanticState.revision = revision;
      const auto encoded = serializeDormantDesiredStateV2(endpoint);
      QVERIFY2(encoded, qPrintable(describeErrors(encoded.errors)));
      const auto object = QJsonDocument::fromJson(*encoded.value).object();
      QCOMPARE(object.value(QStringLiteral("revision")).toString(),
               QString::number(revision));

      const auto reparsed = parseDormantDesiredStateV2(
          *encoded.value, *catalog.value, *actions.value);
      QVERIFY2(reparsed, qPrintable(describeErrors(reparsed.errors)));
      QCOMPARE(reparsed.value->semanticState.revision, revision);
      const auto reencoded = serializeDormantDesiredStateV2(*reparsed.value);
      QVERIFY2(reencoded, qPrintable(describeErrors(reencoded.errors)));
      QCOMPARE(*reencoded.value, *encoded.value);
    }

    const auto encoded = serializeDormantDesiredStateV2(*initial.value);
    QVERIFY2(encoded, qPrintable(describeErrors(encoded.errors)));
    auto overflowObject = QJsonDocument::fromJson(*encoded.value).object();
    overflowObject.insert(QStringLiteral("revision"),
                          QStringLiteral("18446744073709551616"));
    const auto overflow = parseDormantDesiredStateV2(
        encode(std::move(overflowObject)), *catalog.value, *actions.value);
    QVERIFY(!overflow);
    QVERIFY(hasCode(overflow.errors, QStringLiteral("state.invalid-revision")));
    QVERIFY(hasErrorAt(overflow.errors, QStringLiteral("$.revision")));
  }

  void actionCatalogMatchesSchemaAndTaggedProvenance() {
    const auto actionObject =
        readObject(QStringLiteral(HYPRSHELLD_HYPRLAND_ACTION_CATALOG_FILE));
    const auto configSchema =
        readObject(QStringLiteral(HYPRSHELLD_HYPRLAND_CONFIG_SCHEMA_FILE));
    const auto sourceManifest =
        readObject(QStringLiteral(HYPRSHELLD_HYPRLAND_SOURCE_MANIFEST_FILE));
    QVERIFY(!actionObject.isEmpty());
    QVERIFY(!configSchema.isEmpty());
    QVERIFY(!sourceManifest.isEmpty());

    const auto source = actionObject.value(QStringLiteral("source")).toObject();
    QCOMPARE(source.value(QStringLiteral("repository")),
             sourceManifest.value(QStringLiteral("repository")));
    QCOMPARE(source.value(QStringLiteral("tag")),
             QJsonValue(QStringLiteral("v0.56.1")));
    QCOMPARE(
        source.value(QStringLiteral("commit")),
        QJsonValue(QStringLiteral("5c9377c15f85c50648f35ca5a213754f95b93ca0")));
    const auto complexSources =
        sourceManifest.value(QStringLiteral("complexSources")).toArray();
    const auto sourceIterator = std::ranges::find_if(
        complexSources, [&source](const QJsonValue &value) {
          return value.toObject().value(QStringLiteral("path")) ==
                 source.value(QStringLiteral("path"));
        });
    QVERIFY(sourceIterator != complexSources.end());
    QCOMPARE(sourceIterator->toObject().value(QStringLiteral("sha256")),
             source.value(QStringLiteral("sha256")));

    const auto definitions =
        configSchema.value(QStringLiteral("$defs")).toObject();
    const auto dispatcherSchemaIds =
        jsonStringSet(definitions.value(QStringLiteral("dispatcherAction"))
                          .toObject()
                          .value(QStringLiteral("enum"))
                          .toArray());
    QSet<QString> catalogDispatcherIds;
    QMap<QString, qsizetype> argumentCoverage;
    for (const auto &value :
         actionObject.value(QStringLiteral("dispatcherActions")).toArray()) {
      const auto action = value.toObject();
      const auto id = action.value(QStringLiteral("id")).toString();
      catalogDispatcherIds.insert(id);
      const auto reference =
          action.value(QStringLiteral("argumentsSchemaRef")).toString();
      const auto prefix = QStringLiteral("config.schema.json#/$defs/");
      QVERIFY2(reference.startsWith(prefix), qPrintable(reference));
      QVERIFY2(definitions.contains(reference.sliced(prefix.size())),
               qPrintable(reference));
    }
    QCOMPARE(catalogDispatcherIds, dispatcherSchemaIds);

    const auto bindingConditions = definitions.value(QStringLiteral("binding"))
                                       .toObject()
                                       .value(QStringLiteral("allOf"))
                                       .toArray();
    for (const auto &value : bindingConditions) {
      const auto condition = value.toObject();
      const auto actionCondition = condition.value(QStringLiteral("if"))
                                       .toObject()
                                       .value(QStringLiteral("properties"))
                                       .toObject()
                                       .value(QStringLiteral("action"))
                                       .toObject();
      if (actionCondition.isEmpty()) {
        continue;
      }
      QStringList ids;
      if (actionCondition.contains(QStringLiteral("const"))) {
        ids.append(actionCondition.value(QStringLiteral("const")).toString());
      }
      for (const auto &id :
           actionCondition.value(QStringLiteral("enum")).toArray()) {
        ids.append(id.toString());
      }
      for (const auto &id : ids) {
        ++argumentCoverage[id];
      }
    }
    QCOMPARE(stringSet(argumentCoverage.keys()), dispatcherSchemaIds);
    for (auto iterator = argumentCoverage.constBegin();
         iterator != argumentCoverage.constEnd(); ++iterator) {
      QCOMPARE(iterator.value(), qsizetype(1));
    }

    QSet<QString> semanticSchemaIds;
    for (const auto &name : {
             QStringLiteral("defaultAppAction"),
             QStringLiteral("hyprShelldAction"),
         }) {
      semanticSchemaIds.unite(jsonStringSet(definitions.value(name)
                                                .toObject()
                                                .value(QStringLiteral("enum"))
                                                .toArray()));
    }
    QSet<QString> catalogSemanticIds;
    for (const auto &value :
         actionObject.value(QStringLiteral("semanticActions")).toArray()) {
      const auto action = value.toObject();
      catalogSemanticIds.insert(action.value(QStringLiteral("id")).toString());
      QCOMPARE(action.value(QStringLiteral("argumentsSchemaRef")),
               QJsonValue(
                   QStringLiteral("config.schema.json#/$defs/emptyArguments")));
    }
    QCOMPARE(catalogSemanticIds, semanticSchemaIds);
  }

  void actionCatalogRejectsMalformedCatalogAndSchemaBinding() {
    const auto actionBytes =
        readBytes(QStringLiteral(HYPRSHELLD_HYPRLAND_ACTION_CATALOG_FILE));
    const auto schemaBytes =
        readBytes(QStringLiteral(HYPRSHELLD_HYPRLAND_CONFIG_SCHEMA_FILE));
    QVERIFY(!actionBytes.isEmpty());
    QVERIFY(!schemaBytes.isEmpty());

    const QByteArray oversizedAction(maximumActionCatalogBytes + 1, ' ');
    const auto actionSize = parseActionCatalog(oversizedAction, schemaBytes);
    QVERIFY(!actionSize);
    QVERIFY(hasCode(actionSize.errors, QStringLiteral("json.size-limit")));

    const QByteArray oversizedSchema(maximumActionSchemaBytes + 1, ' ');
    const auto schemaSize = parseActionCatalog(actionBytes, oversizedSchema);
    QVERIFY(!schemaSize);
    QVERIFY(hasCode(schemaSize.errors, QStringLiteral("json.size-limit")));

    auto changedSchema = schemaBytes;
    changedSchema.append(' ');
    const auto digestMismatch = parseActionCatalog(actionBytes, changedSchema);
    QVERIFY(!digestMismatch);
    QVERIFY(hasCode(digestMismatch.errors,
                    QStringLiteral("action-catalog.integrity-mismatch")));

    auto actionObject = QJsonDocument::fromJson(actionBytes).object();
    actionObject.insert(QStringLiteral("futureField"), true);
    const auto unknown = parseActionCatalog(encode(actionObject), schemaBytes);
    QVERIFY(!unknown);

    actionObject = QJsonDocument::fromJson(actionBytes).object();
    auto dispatchers =
        actionObject.value(QStringLiteral("dispatcherActions")).toArray();
    QVERIFY(dispatchers.size() == 47);
    dispatchers.removeLast();
    actionObject.insert(QStringLiteral("dispatcherActions"), dispatchers);
    const auto missingAction =
        parseActionCatalog(encode(actionObject), schemaBytes);
    QVERIFY(!missingAction);
    QVERIFY(hasCode(missingAction.errors,
                    QStringLiteral("action-catalog.collection-limit")));

    actionObject = QJsonDocument::fromJson(actionBytes).object();
    dispatchers =
        actionObject.value(QStringLiteral("dispatcherActions")).toArray();
    dispatchers.append(dispatchers.last());
    actionObject.insert(QStringLiteral("dispatcherActions"), dispatchers);
    const auto extraAction =
        parseActionCatalog(encode(actionObject), schemaBytes);
    QVERIFY(!extraAction);
    QVERIFY(hasCode(extraAction.errors,
                    QStringLiteral("action-catalog.collection-limit")));

    actionObject = QJsonDocument::fromJson(actionBytes).object();
    dispatchers =
        actionObject.value(QStringLiteral("dispatcherActions")).toArray();
    QVERIFY(dispatchers.size() >= 2);
    auto duplicateRecord = dispatchers.at(1).toObject();
    duplicateRecord.insert(
        QStringLiteral("id"),
        dispatchers.first().toObject().value(QStringLiteral("id")));
    dispatchers.replace(1, duplicateRecord);
    actionObject.insert(QStringLiteral("dispatcherActions"), dispatchers);
    const auto duplicate =
        parseActionCatalog(encode(actionObject), schemaBytes);
    QVERIFY(!duplicate);
    QVERIFY(hasCode(duplicate.errors,
                    QStringLiteral("action-catalog.duplicate-id")));

    actionObject = QJsonDocument::fromJson(actionBytes).object();
    dispatchers =
        actionObject.value(QStringLiteral("dispatcherActions")).toArray();
    auto malformedPresentation = dispatchers.first().toObject();
    malformedPresentation.insert(QStringLiteral("label"), QString());
    dispatchers.replace(0, malformedPresentation);
    actionObject.insert(QStringLiteral("dispatcherActions"), dispatchers);
    const auto emptyLabel =
        parseActionCatalog(encode(actionObject), schemaBytes);
    QVERIFY(!emptyLabel);

    actionObject = QJsonDocument::fromJson(actionBytes).object();
    dispatchers =
        actionObject.value(QStringLiteral("dispatcherActions")).toArray();
    malformedPresentation = dispatchers.first().toObject();
    malformedPresentation.insert(QStringLiteral("description"),
                                 QStringLiteral("unsafe\ntext"));
    dispatchers.replace(0, malformedPresentation);
    actionObject.insert(QStringLiteral("dispatcherActions"), dispatchers);
    const auto controlText =
        parseActionCatalog(encode(actionObject), schemaBytes);
    QVERIFY(!controlText);

    auto duplicateSchema = schemaBytes;
    const auto needle = QByteArrayLiteral("\"$schema\": ");
    QVERIFY(duplicateSchema.contains(needle));
    duplicateSchema.replace(
        needle, QByteArrayLiteral("\"$schema\": \"duplicate\", \"$schema\": "));
    const auto duplicateSchemaResult = parseActionCatalog(
        actionCatalogForSchema(duplicateSchema), duplicateSchema);
    QVERIFY(!duplicateSchemaResult);

    actionObject = QJsonDocument::fromJson(actionBytes).object();
    auto source = actionObject.value(QStringLiteral("source")).toObject();
    source.insert(QStringLiteral("commit"), QString(40, QLatin1Char('0')));
    actionObject.insert(QStringLiteral("source"), source);
    const auto changedProvenance =
        parseActionCatalog(encode(actionObject), schemaBytes);
    QVERIFY(!changedProvenance);

    actionObject = QJsonDocument::fromJson(actionBytes).object();
    source = actionObject.value(QStringLiteral("source")).toObject();
    source.insert(QStringLiteral("sha256"), QString(64, QLatin1Char('0')));
    actionObject.insert(QStringLiteral("source"), source);
    const auto changedSourceHash =
        parseActionCatalog(encode(actionObject), schemaBytes);
    QVERIFY(!changedSourceHash);
    QVERIFY(hasCode(changedSourceHash.errors,
                    QStringLiteral("action-catalog.invalid-provenance")));

    actionObject = QJsonDocument::fromJson(actionBytes).object();
    dispatchers =
        actionObject.value(QStringLiteral("dispatcherActions")).toArray();
    for (qsizetype index = 0; index < dispatchers.size(); ++index) {
      auto action = dispatchers.at(index).toObject();
      if (action.value(QStringLiteral("id")) !=
          QJsonValue(QStringLiteral("event"))) {
        continue;
      }
      action.insert(QStringLiteral("luaPath"),
                    QJsonArray{QStringLiteral("dsp"),
                               QStringLiteral("exec_raw")});
      dispatchers.replace(index, action);
      break;
    }
    actionObject.insert(QStringLiteral("dispatcherActions"), dispatchers);
    const auto rawInvocation =
        parseActionCatalog(encode(actionObject), schemaBytes);
    QVERIFY(!rawInvocation);
    QVERIFY(hasCode(rawInvocation.errors,
                    QStringLiteral("action-catalog.action-contract-mismatch")));
    QVERIFY(hasCode(rawInvocation.errors,
                    QStringLiteral("action-catalog.integrity-mismatch")));

    actionObject = QJsonDocument::fromJson(actionBytes).object();
    dispatchers =
        actionObject.value(QStringLiteral("dispatcherActions")).toArray();
    for (qsizetype index = 0; index < dispatchers.size(); ++index) {
      auto action = dispatchers.at(index).toObject();
      if (action.value(QStringLiteral("id")) !=
          QJsonValue(QStringLiteral("event"))) {
        continue;
      }
      action.insert(
          QStringLiteral("argumentsSchemaRef"),
          QStringLiteral("config.schema.json#/$defs/emptyArguments"));
      dispatchers.replace(index, action);
      break;
    }
    actionObject.insert(QStringLiteral("dispatcherActions"), dispatchers);
    const auto swappedContract =
        parseActionCatalog(encode(actionObject), schemaBytes);
    QVERIFY(!swappedContract);
    QVERIFY(hasCode(swappedContract.errors,
                    QStringLiteral("action-catalog.action-contract-mismatch")));
    QVERIFY(hasCode(swappedContract.errors,
                    QStringLiteral("action-catalog.integrity-mismatch")));

    actionObject = QJsonDocument::fromJson(actionBytes).object();
    auto exclusions = actionObject.value(QStringLiteral("excluded")).toArray();
    for (qsizetype index = 0; index < exclusions.size(); ++index) {
      auto exclusion = exclusions.at(index).toObject();
      if (exclusion.value(QStringLiteral("id")) !=
          QJsonValue(QStringLiteral("exec_raw"))) {
        continue;
      }
      exclusion.insert(QStringLiteral("id"), QStringLiteral("other_raw"));
      exclusions.replace(index, exclusion);
      break;
    }
    actionObject.insert(QStringLiteral("excluded"), exclusions);
    const auto changedExclusion =
        parseActionCatalog(encode(actionObject), schemaBytes);
    QVERIFY(!changedExclusion);
    QVERIFY(hasCode(
        changedExclusion.errors,
        QStringLiteral("action-catalog.exclusion-contract-mismatch")));
    QVERIFY(hasCode(changedExclusion.errors,
                    QStringLiteral("action-catalog.integrity-mismatch")));
  }

  void actionSchemaCompilerRejectsUnsafeAndUnboundedShapes() {
    const auto originalSchema =
        readObject(QStringLiteral(HYPRSHELLD_HYPRLAND_CONFIG_SCHEMA_FILE));
    QVERIFY(!originalSchema.isEmpty());

    const auto parseMutatedSchema = [](QJsonObject schema) {
      const auto bytes = encode(std::move(schema));
      return parseActionCatalog(actionCatalogForSchema(bytes), bytes);
    };
    const auto schemaWithCursorX = [&originalSchema](QJsonObject replacement) {
      auto schema = originalSchema;
      auto definitions = schema.value(QStringLiteral("$defs")).toObject();
      auto cursorMove =
          definitions.value(QStringLiteral("cursorMoveArguments")).toObject();
      auto properties =
          cursorMove.value(QStringLiteral("properties")).toObject();
      properties.insert(QStringLiteral("x"), std::move(replacement));
      cursorMove.insert(QStringLiteral("properties"), properties);
      definitions.insert(QStringLiteral("cursorMoveArguments"), cursorMove);
      schema.insert(QStringLiteral("$defs"), definitions);
      return schema;
    };

    auto schema = originalSchema;
    auto definitions = schema.value(QStringLiteral("$defs")).toObject();
    auto cursorMove =
        definitions.value(QStringLiteral("cursorMoveArguments")).toObject();
    auto properties = cursorMove.value(QStringLiteral("properties")).toObject();
    auto x = properties.value(QStringLiteral("x")).toObject();
    x.insert(QStringLiteral("default"), 0);
    properties.insert(QStringLiteral("x"), x);
    cursorMove.insert(QStringLiteral("properties"), properties);
    definitions.insert(QStringLiteral("cursorMoveArguments"), cursorMove);
    schema.insert(QStringLiteral("$defs"), definitions);
    const auto unsupportedKeyword = parseMutatedSchema(schema);
    QVERIFY(!unsupportedKeyword);
    QVERIFY(hasCode(unsupportedKeyword.errors,
                    QStringLiteral("action-schema.unsupported-keyword")));

    const auto unconstrained =
        parseMutatedSchema(schemaWithCursorX(QJsonObject{}));
    QVERIFY(!unconstrained);
    QVERIFY(hasCode(unconstrained.errors,
                    QStringLiteral("action-schema.unconstrained-node")));

    const auto arrayWithoutMaximum = parseMutatedSchema(schemaWithCursorX(
        QJsonObject{{QStringLiteral("type"), QStringLiteral("array")},
                    {QStringLiteral("items"),
                     QJsonObject{{QStringLiteral("type"),
                                  QStringLiteral("boolean")}}}}));
    QVERIFY(!arrayWithoutMaximum);
    QVERIFY(hasCode(arrayWithoutMaximum.errors,
                    QStringLiteral("action-schema.unbounded-array")));

    const auto arrayWithoutItems = parseMutatedSchema(schemaWithCursorX(
        QJsonObject{{QStringLiteral("type"), QStringLiteral("array")},
                    {QStringLiteral("maxItems"), 4}}));
    QVERIFY(!arrayWithoutItems);
    QVERIFY(hasCode(arrayWithoutItems.errors,
                    QStringLiteral("action-schema.unbounded-array")));

    const auto unboundedString = parseMutatedSchema(schemaWithCursorX(
        QJsonObject{{QStringLiteral("type"), QStringLiteral("string")}}));
    QVERIFY(!unboundedString);
    QVERIFY(hasCode(unboundedString.errors,
                    QStringLiteral("action-schema.unbounded-string")));

    const auto unboundedNumber = parseMutatedSchema(schemaWithCursorX(
        QJsonObject{{QStringLiteral("type"), QStringLiteral("number")}}));
    QVERIFY(!unboundedNumber);
    QVERIFY(hasCode(unboundedNumber.errors,
                    QStringLiteral("action-schema.unbounded-number")));

    schema = originalSchema;
    definitions = schema.value(QStringLiteral("$defs")).toObject();
    cursorMove =
        definitions.value(QStringLiteral("cursorMoveArguments")).toObject();
    definitions.insert(
        QStringLiteral("cursorMoveArguments"),
        QJsonObject{
            {QStringLiteral("type"), QStringLiteral("object")},
            {QStringLiteral("oneOf"), QJsonArray{cursorMove}},
        });
    schema.insert(QStringLiteral("$defs"), definitions);
    const auto alternativeWithSibling = parseMutatedSchema(schema);
    QVERIFY(!alternativeWithSibling);
    QVERIFY(hasCode(alternativeWithSibling.errors,
                    QStringLiteral("action-schema.alternative-siblings")));

    schema = originalSchema;
    definitions = schema.value(QStringLiteral("$defs")).toObject();
    cursorMove =
        definitions.value(QStringLiteral("cursorMoveArguments")).toObject();
    properties = cursorMove.value(QStringLiteral("properties")).toObject();
    properties.insert(
        QStringLiteral("x"),
        QJsonObject{{QStringLiteral("$ref"),
                     QStringLiteral("https://example.invalid/schema.json")}});
    cursorMove.insert(QStringLiteral("properties"), properties);
    definitions.insert(QStringLiteral("cursorMoveArguments"), cursorMove);
    schema.insert(QStringLiteral("$defs"), definitions);
    const auto remoteReference = parseMutatedSchema(schema);
    QVERIFY(!remoteReference);
    QVERIFY(hasCode(remoteReference.errors,
                    QStringLiteral("action-schema.remote-reference")));

    schema = originalSchema;
    definitions = schema.value(QStringLiteral("$defs")).toObject();
    cursorMove =
        definitions.value(QStringLiteral("cursorMoveArguments")).toObject();
    properties = cursorMove.value(QStringLiteral("properties")).toObject();
    properties.insert(QStringLiteral("x"),
                      QJsonObject{{QStringLiteral("$ref"),
                                   QStringLiteral("#/$defs/doesNotExist")}});
    cursorMove.insert(QStringLiteral("properties"), properties);
    definitions.insert(QStringLiteral("cursorMoveArguments"), cursorMove);
    schema.insert(QStringLiteral("$defs"), definitions);
    const auto missingReference = parseMutatedSchema(schema);
    QVERIFY(!missingReference);
    QVERIFY(hasCode(missingReference.errors,
                    QStringLiteral("action-schema.missing-reference")));

    schema = originalSchema;
    definitions = schema.value(QStringLiteral("$defs")).toObject();
    definitions.insert(
        QStringLiteral("cursorMoveArguments"),
        QJsonObject{{QStringLiteral("$ref"),
                     QStringLiteral("#/$defs/cursorMoveArguments")}});
    schema.insert(QStringLiteral("$defs"), definitions);
    const auto cyclicReference = parseMutatedSchema(schema);
    QVERIFY(!cyclicReference);
    QVERIFY(hasCode(cyclicReference.errors,
                    QStringLiteral("action-schema.reference-cycle")));

    schema = originalSchema;
    definitions = schema.value(QStringLiteral("$defs")).toObject();
    for (int depth = 0; depth <= maximumSchemaReferenceDepth; ++depth) {
      definitions.insert(
          QStringLiteral("Depth%1").arg(depth),
          QJsonObject{{QStringLiteral("$ref"),
                       QStringLiteral("#/$defs/Depth%1").arg(depth + 1)}});
    }
    definitions.insert(
        QStringLiteral("Depth%1").arg(maximumSchemaReferenceDepth + 1),
        QJsonObject{{QStringLiteral("type"), QStringLiteral("boolean")}});
    schema.insert(QStringLiteral("$defs"), definitions);
    auto actionObject =
        readObject(QStringLiteral(HYPRSHELLD_HYPRLAND_ACTION_CATALOG_FILE));
    auto actions =
        actionObject.value(QStringLiteral("dispatcherActions")).toArray();
    auto firstAction = actions.first().toObject();
    firstAction.insert(QStringLiteral("argumentsSchemaRef"),
                       QStringLiteral("config.schema.json#/$defs/Depth0"));
    actions.replace(0, firstAction);
    actionObject.insert(QStringLiteral("dispatcherActions"), actions);
    auto bytes = encode(schema);
    const auto excessiveDepth =
        parseActionCatalog(actionCatalogForSchema(actionObject, bytes), bytes);
    QVERIFY(!excessiveDepth);
    QVERIFY2(hasCode(excessiveDepth.errors,
                     QStringLiteral("action-schema.reference-cycle")),
             qPrintable(describeErrors(excessiveDepth.errors)));

    schema = originalSchema;
    definitions = schema.value(QStringLiteral("$defs")).toObject();
    properties = {};
    for (qsizetype index = 0; index <= maximumSchemaProperties; ++index) {
      properties.insert(
          QStringLiteral("field%1").arg(index),
          QJsonObject{{QStringLiteral("type"), QStringLiteral("boolean")}});
    }
    definitions.insert(QStringLiteral("cursorMoveArguments"),
                       QJsonObject{
                           {QStringLiteral("type"), QStringLiteral("object")},
                           {QStringLiteral("additionalProperties"), false},
                           {QStringLiteral("properties"), properties},
                       });
    schema.insert(QStringLiteral("$defs"), definitions);
    const auto excessiveProperties = parseMutatedSchema(schema);
    QVERIFY(!excessiveProperties);
    QVERIFY(hasCode(excessiveProperties.errors,
                    QStringLiteral("action-schema.property-limit")));

    schema = originalSchema;
    definitions = schema.value(QStringLiteral("$defs")).toObject();
    QJsonObject wideProperties;
    for (qsizetype outer = 0; outer < maximumSchemaProperties; ++outer) {
      QJsonObject nestedProperties;
      for (int inner = 0; inner < 8; ++inner) {
        nestedProperties.insert(QStringLiteral("nested%1").arg(inner),
                                QJsonObject{{
                                    QStringLiteral("type"),
                                    QStringLiteral("boolean"),
                                }});
      }
      wideProperties.insert(
          QStringLiteral("field%1").arg(outer),
          QJsonObject{
              {QStringLiteral("type"), QStringLiteral("object")},
              {QStringLiteral("additionalProperties"), false},
              {QStringLiteral("properties"), nestedProperties},
          });
    }
    definitions.insert(QStringLiteral("cursorMoveArguments"),
                       QJsonObject{
                           {QStringLiteral("type"), QStringLiteral("object")},
                           {QStringLiteral("additionalProperties"), false},
                           {QStringLiteral("properties"), wideProperties},
                       });
    schema.insert(QStringLiteral("$defs"), definitions);
    const auto excessiveNodes = parseMutatedSchema(schema);
    QVERIFY(!excessiveNodes);
    QVERIFY(hasCode(excessiveNodes.errors,
                    QStringLiteral("action-schema.complexity-limit")));
  }

  void compiledActionSchemaControlsPayloadValidation() {
    const auto original = shippedActionCatalog();
    QVERIFY(original);
    const auto *originalMove = findAction(
        *original.value, ActionKind::Dispatcher, QStringLiteral("cursor.move"));
    QVERIFY(originalMove != nullptr);
    QVERIFY(validateActionPayload(*originalMove,
                                  QJsonObject{
                                      {QStringLiteral("x"), -1000000.0},
                                      {QStringLiteral("y"), 2.0},
                                  },
                                  QStringLiteral("$.arguments"))
                .isEmpty());
    QVERIFY(
        !validateActionPayload(*originalMove,
                               QJsonObject{
                                   {QStringLiteral("x"), 1000001.0},
                                   {QStringLiteral("y"), 2.0},
                               },
                               QStringLiteral("$.arguments"))
             .isEmpty());
    QVERIFY(
        !validateActionPayload(*originalMove,
                               QJsonObject{
                                   {QStringLiteral("x"), QStringLiteral("1")},
                                   {QStringLiteral("y"), 2.0},
                               },
                               QStringLiteral("$.arguments"))
             .isEmpty());

    auto schema =
        readObject(QStringLiteral(HYPRSHELLD_HYPRLAND_CONFIG_SCHEMA_FILE));
    auto definitions = schema.value(QStringLiteral("$defs")).toObject();
    auto cursorMove =
        definitions.value(QStringLiteral("cursorMoveArguments")).toObject();
    auto properties = cursorMove.value(QStringLiteral("properties")).toObject();
    properties.insert(
        QStringLiteral("x"),
        QJsonObject{{QStringLiteral("type"), QStringLiteral("string")},
                    {QStringLiteral("maxLength"), 16}});
    cursorMove.insert(QStringLiteral("properties"), properties);
    definitions.insert(QStringLiteral("cursorMoveArguments"), cursorMove);
    schema.insert(QStringLiteral("$defs"), definitions);
    const auto schemaBytes = encode(schema);
    const auto mutated =
        parseActionCatalog(actionCatalogForSchema(schemaBytes), schemaBytes);
    QVERIFY(!mutated);
    QVERIFY(hasCode(mutated.errors,
                    QStringLiteral("action-catalog.integrity-mismatch")));
  }

  void catalogMatchesThePinned0561Inventory() {
    const auto parsed = shippedCatalog();
    QVERIFY(parsed);
    const auto fixture =
        readObject(QStringLiteral(HYPRSHELLD_HYPRLAND_056_FIXTURE_FILE));
    QCOMPARE(fixture.value(QStringLiteral("hyprlandVersion")).toString(),
             QStringLiteral("0.56.1"));
    QCOMPARE(fixture.value(QStringLiteral("optionCount")).toInt(), 353);
    const auto inventory = inventoryByPath(fixture);
    const auto delta = readObject(
        QStringLiteral(HYPRSHELLD_HYPRLAND_DELTA_FIXTURE_FILE));
    const auto added =
        jsonStringSet(delta.value(QStringLiteral("added")).toArray());
    QCOMPARE(inventory.size(), 353);
    QCOMPARE(parsed.value->options.size(), qsizetype(inventory.size()));

    for (const auto &option : parsed.value->options) {
      QVERIFY2(inventory.contains(option.path), qPrintable(option.path));
      const auto source = inventory.value(option.path);
      QCOMPARE(option.defaultValue, source.value(QStringLiteral("default")));
      QCOMPARE(option.module,
               source.value(QStringLiteral("module")).toString());
      QCOMPARE(option.description,
               source.value(QStringLiteral("description")).toString());
      QVERIFY(option.since ==
              (added.contains(option.path) ? SemanticVersion{0, 56, 0}
                                           : SemanticVersion{0, 55, 0}));
      QStringList sourceLuaPath;
      for (const auto &segment :
           source.value(QStringLiteral("luaPath")).toArray()) {
        sourceLuaPath.append(segment.toString());
      }
      QCOMPARE(option.luaPath, sourceLuaPath);

      const auto sourceConstraints =
          source.value(QStringLiteral("constraints")).toObject();
      const auto sourceType = source.value(QStringLiteral("type")).toString();
      if (option.type == OptionType::Enumeration) {
        QCOMPARE(sourceType, QStringLiteral("enum"));
        QVERIFY(!option.constraints.choices.isEmpty());
      } else {
        QCOMPARE(toString(option.type), sourceType);
      }

      if (sourceConstraints.contains(QStringLiteral("min"))) {
        QVERIFY(option.constraints.minimum.has_value());
        QCOMPARE(*option.constraints.minimum,
                 sourceConstraints.value(QStringLiteral("min")));
      }
      if (sourceConstraints.contains(QStringLiteral("max"))) {
        QVERIFY(option.constraints.maximum.has_value());
        QCOMPARE(*option.constraints.maximum,
                 sourceConstraints.value(QStringLiteral("max")));
      }
    }
  }

  void everyScalarHasAnExplicitDeterministicLuaDestination() {
    const auto parsed = shippedCatalog();
    QVERIFY(parsed);
    const QRegularExpression segmentPattern(
        QStringLiteral("^[a-z][a-z0-9_]*$"));
    const QRegularExpression modulePattern(QStringLiteral("^[a-z][a-z0-9-]*$"));
    QSet<QString> destinations;
    for (const auto &option : parsed.value->options) {
      QVERIFY2(modulePattern.match(option.module).hasMatch(),
               qPrintable(option.path));
      QVERIFY2(option.luaPath.size() >= 2, qPrintable(option.path));
      for (const auto &segment : option.luaPath) {
        QVERIFY2(segmentPattern.match(segment).hasMatch(),
                 qPrintable(option.path));
      }
      const auto destination = option.luaPath.join(QLatin1Char('.'));
      QVERIFY2(!destinations.contains(destination), qPrintable(destination));
      destinations.insert(destination);
    }

    const auto *capture = optionWithPath(
        *parsed.value, QStringLiteral("input-capture:capture_modifiers"));
    QVERIFY(capture != nullptr);
    QCOMPARE(capture->module, QStringLiteral("input"));
    QCOMPARE(capture->luaPath, QStringList({
                                   QStringLiteral("input_capture"),
                                   QStringLiteral("capture_modifiers"),
                               }));

    const auto *color = optionWithPath(
        *parsed.value, QStringLiteral("general:col.active_border"));
    QVERIFY(color != nullptr);
    QCOMPARE(color->luaPath, QStringList({
                                 QStringLiteral("general"),
                                 QStringLiteral("col"),
                                 QStringLiteral("active_border"),
                             }));
  }

  void pinned055To0561DeltaIsExact() {
    const auto oldFixture =
        readObject(QStringLiteral(HYPRSHELLD_HYPRLAND_055_FIXTURE_FILE));
    const auto newFixture =
        readObject(QStringLiteral(HYPRSHELLD_HYPRLAND_056_FIXTURE_FILE));
    const auto delta =
        readObject(QStringLiteral(HYPRSHELLD_HYPRLAND_DELTA_FIXTURE_FILE));
    QCOMPARE(oldFixture.value(QStringLiteral("optionCount")).toInt(), 341);
    QCOMPARE(newFixture.value(QStringLiteral("optionCount")).toInt(), 353);

    const auto oldInventory = inventoryByPath(oldFixture);
    const auto newInventory = inventoryByPath(newFixture);
    const auto oldPaths =
        QSet<QString>(oldInventory.keyBegin(), oldInventory.keyEnd());
    const auto newPaths =
        QSet<QString>(newInventory.keyBegin(), newInventory.keyEnd());
    const auto expectedAdded =
        jsonStringSet(delta.value(QStringLiteral("added")).toArray());
    const auto expectedRemoved =
        jsonStringSet(delta.value(QStringLiteral("removed")).toArray());
    QCOMPARE(expectedAdded.size(), 12);
    QVERIFY(expectedRemoved.isEmpty());
    QCOMPARE(newPaths - oldPaths, expectedAdded);
    QCOMPARE(oldPaths - newPaths, expectedRemoved);

    const auto changed = delta.value(QStringLiteral("changed")).toArray();
    QCOMPARE(changed.size(), 7);
    QSet<QString> changedPaths;
    for (const auto &value : changed) {
      const auto record = value.toObject();
      const auto path = record.value(QStringLiteral("path")).toString();
      QVERIFY2(oldInventory.contains(path), qPrintable(path));
      QVERIFY2(newInventory.contains(path), qPrintable(path));
      QVERIFY2(!changedPaths.contains(path), qPrintable(path));
      changedPaths.insert(path);
      const auto changes = record.value(QStringLiteral("changes")).toObject();
      QVERIFY(!changes.isEmpty());
      for (auto iterator = changes.constBegin(); iterator != changes.constEnd();
           ++iterator) {
        const auto transition = iterator.value().toObject();
        QCOMPARE(transition.value(QStringLiteral("from")),
                 oldInventory.value(path).value(iterator.key()));
        QCOMPARE(transition.value(QStringLiteral("to")),
                 newInventory.value(path).value(iterator.key()));
        QVERIFY(transition.value(QStringLiteral("from")) !=
                transition.value(QStringLiteral("to")));
      }
    }
  }

  void sourceManifestPinsImmutableTaggedInputs() {
    const auto manifest =
        readObject(QStringLiteral(HYPRSHELLD_HYPRLAND_SOURCE_MANIFEST_FILE));
    QCOMPARE(manifest.value(QStringLiteral("formatVersion")).toInt(), 1);
    QCOMPARE(manifest.value(QStringLiteral("repository")).toString(),
             QStringLiteral("https://github.com/hyprwm/Hyprland"));
    const auto sources = manifest.value(QStringLiteral("sources")).toArray();
    QCOMPARE(sources.size(), 2);

    const QMap<QString, QString> fixturePaths{
        {QStringLiteral("0.55.0"),
         QStringLiteral(HYPRSHELLD_HYPRLAND_055_FIXTURE_FILE)},
        {QStringLiteral("0.56.1"),
         QStringLiteral(HYPRSHELLD_HYPRLAND_056_FIXTURE_FILE)},
    };
    for (const auto &value : sources) {
      const auto source = value.toObject();
      const auto version = source.value(QStringLiteral("version")).toString();
      QVERIFY(fixturePaths.contains(version));
      const auto fixture = readObject(fixturePaths.value(version));
      QCOMPARE(fixture.value(QStringLiteral("hyprlandVersion")),
               QJsonValue(version));
      for (const auto &key : {
               QStringLiteral("tag"),
               QStringLiteral("commit"),
           }) {
        QCOMPARE(fixture.value(key), source.value(key));
      }
      QCOMPARE(fixture.value(QStringLiteral("optionCount")),
               source.value(QStringLiteral("optionCount")));
      const auto fixtureSource =
          fixture.value(QStringLiteral("source")).toObject();
      QCOMPARE(fixtureSource.value(QStringLiteral("path")),
               source.value(QStringLiteral("path")));
      QCOMPARE(fixtureSource.value(QStringLiteral("sha256")),
               source.value(QStringLiteral("sha256")));
      QVERIFY(QRegularExpression(QStringLiteral("^[0-9a-f]{40}$"))
                  .match(source.value(QStringLiteral("commit")).toString())
                  .hasMatch());
      QVERIFY(QRegularExpression(QStringLiteral("^[0-9a-f]{64}$"))
                  .match(source.value(QStringLiteral("sha256")).toString())
                  .hasMatch());
    }

    const auto versionFiles =
        manifest.value(QStringLiteral("versionFiles")).toArray();
    QCOMPARE(versionFiles.size(), 3);
    QSet<QString> pinnedVersions;
    for (const auto &value : versionFiles) {
      const auto versionFile = value.toObject();
      const auto version =
          versionFile.value(QStringLiteral("version")).toString();
      QVERIFY2(!pinnedVersions.contains(version), qPrintable(version));
      pinnedVersions.insert(version);
      QCOMPARE(versionFile.value(QStringLiteral("path")),
               QJsonValue(QStringLiteral("VERSION")));
      if (version == QStringLiteral("0.56.0")) {
        QCOMPARE(versionFile.value(QStringLiteral("tag")),
                 QJsonValue(QStringLiteral("v0.56.0")));
        QCOMPARE(
            versionFile.value(QStringLiteral("commit")),
            QJsonValue(QStringLiteral(
                "36b2e0cfe0c6094dbc47bd42a437431315bb3087")));
      } else {
        QVERIFY2(fixturePaths.contains(version), qPrintable(version));
        const auto source = std::ranges::find_if(
            sources, [&version](const QJsonValue &candidate) {
              return candidate.toObject().value(QStringLiteral("version")) ==
                     QJsonValue(version);
            });
        QVERIFY(source != sources.end());
        QCOMPARE(versionFile.value(QStringLiteral("tag")),
                 source->toObject().value(QStringLiteral("tag")));
        QCOMPARE(versionFile.value(QStringLiteral("commit")),
                 source->toObject().value(QStringLiteral("commit")));
      }
      QVERIFY(QRegularExpression(QStringLiteral("^[0-9a-f]{64}$"))
                  .match(versionFile.value(QStringLiteral("sha256")).toString())
                  .hasMatch());
    }
    QCOMPARE(pinnedVersions,
             stringSet({QStringLiteral("0.55.0"),
                        QStringLiteral("0.56.0"),
                        QStringLiteral("0.56.1")}));

    const auto startupSources =
        manifest.value(QStringLiteral("startupSources")).toArray();
    QCOMPARE(startupSources.size(), 4);
    QSet<QString> startupPaths;
    for (const auto &value : startupSources) {
      const auto source = value.toObject();
      QCOMPARE(source.value(QStringLiteral("version")),
               QJsonValue(QStringLiteral("0.56.0")));
      QCOMPARE(source.value(QStringLiteral("tag")),
               QJsonValue(QStringLiteral("v0.56.0")));
      QCOMPARE(
          source.value(QStringLiteral("commit")),
          QJsonValue(QStringLiteral(
              "36b2e0cfe0c6094dbc47bd42a437431315bb3087")));
      const auto path = source.value(QStringLiteral("path")).toString();
      QVERIFY2(!path.isEmpty(), "startup source path is empty");
      QVERIFY2(!startupPaths.contains(path), qPrintable(path));
      startupPaths.insert(path);
      QVERIFY(QRegularExpression(QStringLiteral("^[0-9a-f]{64}$"))
                  .match(source.value(QStringLiteral("sha256")).toString())
                  .hasMatch());
    }
    QCOMPARE(
        startupPaths,
        stringSet({
            QStringLiteral("src/Compositor.cpp"),
            QStringLiteral("src/config/lua/ConfigManager.cpp"),
            QStringLiteral("src/config/shared/actions/ConfigActions.cpp"),
            QStringLiteral("src/main.cpp"),
        }));

    const auto monitorSources =
        manifest.value(QStringLiteral("monitorSources")).toArray();
    QCOMPARE(monitorSources.size(), 4);
    const QMap<QString, QString> expectedMonitorTags{
        {QStringLiteral("0.56.0"), QStringLiteral("v0.56.0")},
        {QStringLiteral("0.56.1"), QStringLiteral("v0.56.1")},
    };
    const QMap<QString, QString> expectedMonitorCommits{
        {QStringLiteral("0.56.0"),
         QStringLiteral("36b2e0cfe0c6094dbc47bd42a437431315bb3087")},
        {QStringLiteral("0.56.1"),
         QStringLiteral("5c9377c15f85c50648f35ca5a213754f95b93ca0")},
    };
    const QMap<QString, QString> expectedMonitorHashes{
        {QStringLiteral("0.56.0|src/debug/HyprCtl.cpp"),
         QStringLiteral("17dddd63fca2d367f81eec5f0b6785cc7c971998fab5b786f908905b2327743d")},
        {QStringLiteral("0.56.0|src/output/Monitor.cpp"),
         QStringLiteral("5f6dc48a7cb6cda7b1c0859cbce72023c0102d865d7a67f5210b59587f2b5801")},
        {QStringLiteral("0.56.1|src/debug/HyprCtl.cpp"),
         QStringLiteral("7b96515a4cf13333ca71549053e76fcdd9cf815b18e4ae530dfff169af3ff1d1")},
        {QStringLiteral("0.56.1|src/output/Monitor.cpp"),
         QStringLiteral("9cf88e154eb5dae676c79d37b5b055ca6134838857cecdbb89a3b747a6821927")},
    };
    QSet<QString> monitorSourceKeys;
    for (const auto &value : monitorSources) {
      const auto source = value.toObject();
      const auto version = source.value(QStringLiteral("version")).toString();
      const auto path = source.value(QStringLiteral("path")).toString();
      const auto key = version + QLatin1Char('|') + path;
      QVERIFY2(expectedMonitorHashes.contains(key), qPrintable(key));
      QVERIFY2(!monitorSourceKeys.contains(key), qPrintable(key));
      monitorSourceKeys.insert(key);
      QCOMPARE(source.value(QStringLiteral("tag")),
               QJsonValue(expectedMonitorTags.value(version)));
      QCOMPARE(source.value(QStringLiteral("commit")),
               QJsonValue(expectedMonitorCommits.value(version)));
      QCOMPARE(source.value(QStringLiteral("sha256")),
               QJsonValue(expectedMonitorHashes.value(key)));
    }
    QCOMPARE(monitorSourceKeys,
             QSet<QString>(expectedMonitorHashes.keyBegin(),
                           expectedMonitorHashes.keyEnd()));

    const auto maximizeSources =
        manifest.value(QStringLiteral("maximizeSources")).toArray();
    QCOMPARE(maximizeSources.size(), 14);
    const QMap<QString, QString> expectedMaximizeTags{
        {QStringLiteral("0.56.0"), QStringLiteral("v0.56.0")},
        {QStringLiteral("0.56.1"), QStringLiteral("v0.56.1")},
    };
    const QMap<QString, QString> expectedMaximizeCommits{
        {QStringLiteral("0.56.0"),
         QStringLiteral("36b2e0cfe0c6094dbc47bd42a437431315bb3087")},
        {QStringLiteral("0.56.1"),
         QStringLiteral("5c9377c15f85c50648f35ca5a213754f95b93ca0")},
    };
    const QMap<QString, QString> expectedMaximizeHashes{
        {QStringLiteral("0.56.0|src/desktop/Workspace.cpp"),
         QStringLiteral("413bc18a0d17b1bafc27f956a7103b301fa382088449bbb2422e123255e9fcec")},
        {QStringLiteral("0.56.0|src/config/shared/workspace/WorkspaceRuleManager.cpp"),
         QStringLiteral("06e8dcbaea83e51710bd372a8f264ad3d07bbe061b9720bbe456811451c02d10")},
        {QStringLiteral("0.56.0|src/config/shared/workspace/WorkspaceRule.cpp"),
         QStringLiteral("96e1cc448847b03e7f48dc7e1c2bc4ed6c192fcf7c11c608b33f5760f7788f96")},
        {QStringLiteral("0.56.0|src/layout/space/Space.cpp"),
         QStringLiteral("8ddcd4dd0a90bd59d4fa21f95f53444419188846a3a217284f0d6b008875d376")},
        {QStringLiteral("0.56.0|src/managers/fullscreen/FullscreenController.hpp"),
         QStringLiteral("7f3585f23e4d756f3f165670a604de39717eb3fd704114e2a230bdd6ffba2378")},
        {QStringLiteral("0.56.0|src/managers/fullscreen/FullscreenController.cpp"),
         QStringLiteral("581d92ef70588fce181b4b87a04e37f6de7b4777c24ad7fee34b21f941b706b0")},
        {QStringLiteral("0.56.0|src/managers/fullscreen/handler/FullscreenHandler.cpp"),
         QStringLiteral("5bfed3aa05f2e6f013e7776efaa754e3e107aa7688eb5d523bdbdfa87f51bc85")},
        {QStringLiteral("0.56.1|src/desktop/Workspace.cpp"),
         QStringLiteral("e6c8e44d9f8211a8f56b65b433b5f5e4c3e6565479ecb2d749bc02cf4e926ca9")},
        {QStringLiteral("0.56.1|src/config/shared/workspace/WorkspaceRuleManager.cpp"),
         QStringLiteral("06e8dcbaea83e51710bd372a8f264ad3d07bbe061b9720bbe456811451c02d10")},
        {QStringLiteral("0.56.1|src/config/shared/workspace/WorkspaceRule.cpp"),
         QStringLiteral("96e1cc448847b03e7f48dc7e1c2bc4ed6c192fcf7c11c608b33f5760f7788f96")},
        {QStringLiteral("0.56.1|src/layout/space/Space.cpp"),
         QStringLiteral("8ddcd4dd0a90bd59d4fa21f95f53444419188846a3a217284f0d6b008875d376")},
        {QStringLiteral("0.56.1|src/managers/fullscreen/FullscreenController.hpp"),
         QStringLiteral("7f3585f23e4d756f3f165670a604de39717eb3fd704114e2a230bdd6ffba2378")},
        {QStringLiteral("0.56.1|src/managers/fullscreen/FullscreenController.cpp"),
         QStringLiteral("581d92ef70588fce181b4b87a04e37f6de7b4777c24ad7fee34b21f941b706b0")},
        {QStringLiteral("0.56.1|src/managers/fullscreen/handler/FullscreenHandler.cpp"),
         QStringLiteral("5bfed3aa05f2e6f013e7776efaa754e3e107aa7688eb5d523bdbdfa87f51bc85")},
    };
    QSet<QString> maximizeSourceKeys;
    for (const auto &value : maximizeSources) {
      const auto source = value.toObject();
      const auto version = source.value(QStringLiteral("version")).toString();
      const auto path = source.value(QStringLiteral("path")).toString();
      const auto key = version + QLatin1Char('|') + path;
      QVERIFY2(expectedMaximizeHashes.contains(key), qPrintable(key));
      QVERIFY2(!maximizeSourceKeys.contains(key), qPrintable(key));
      maximizeSourceKeys.insert(key);
      QCOMPARE(source.value(QStringLiteral("tag")),
               QJsonValue(expectedMaximizeTags.value(version)));
      QCOMPARE(source.value(QStringLiteral("commit")),
               QJsonValue(expectedMaximizeCommits.value(version)));
      QCOMPARE(source.value(QStringLiteral("sha256")),
               QJsonValue(expectedMaximizeHashes.value(key)));
    }
    QCOMPARE(maximizeSourceKeys,
             QSet<QString>(expectedMaximizeHashes.keyBegin(),
                           expectedMaximizeHashes.keyEnd()));

    const auto groupBehaviorSources =
        manifest.value(QStringLiteral("groupBehaviorSources")).toArray();
    QCOMPARE(groupBehaviorSources.size(), 12);
    const QMap<QString, QString> expectedGroupBehaviorTags{
        {QStringLiteral("0.55.0"), QStringLiteral("v0.55.0")},
        {QStringLiteral("0.56.1"), QStringLiteral("v0.56.1")},
    };
    const QMap<QString, QString> expectedGroupBehaviorCommits{
        {QStringLiteral("0.55.0"),
         QStringLiteral("af923e30d1d24f1f4a4f5cb8308065173c1d9539")},
        {QStringLiteral("0.56.1"),
         QStringLiteral("5c9377c15f85c50648f35ca5a213754f95b93ca0")},
    };
    const QMap<QString, QString> expectedGroupBehaviorHashes{
        {QStringLiteral("0.55.0|src/desktop/view/Window.cpp"),
         QStringLiteral("ec00fc5ca125163eadaa267eac73032de28d9d7f32a4b03ae4498a89427b2a7e")},
        {QStringLiteral("0.55.0|src/desktop/view/Group.cpp"),
         QStringLiteral("b0f9210c858bcaf74c8f5a44f7f40abfe03af8a096e3fb158afd2346be8b0eed")},
        {QStringLiteral("0.55.0|src/layout/supplementary/DragController.cpp"),
         QStringLiteral("0d1ddc01f506cdbf3dcec5ea4e111c61286d2ce4359100847270f82569b3505f")},
        {QStringLiteral("0.55.0|src/render/decorations/CHyprGroupBarDecoration.cpp"),
         QStringLiteral("3d687c43d5414fb6ad617465f06bb87cda8dc6c2e791dbb0e28de72de1cf4c68")},
        {QStringLiteral("0.55.0|src/config/shared/actions/ConfigActions.cpp"),
         QStringLiteral("2b2b09799b4cca634d544d4103f07b82ef4b6c377131c858d2707a5e0494fe07")},
        {QStringLiteral("0.55.0|src/Compositor.cpp"),
         QStringLiteral("a639c01f628e50d765856001c567aa8f344e6a620f69fca994e4436b3a090125")},
        {QStringLiteral("0.56.1|src/desktop/view/Window.cpp"),
         QStringLiteral("4d9219c87cfba1105a30c2b742b0728808f2339c813d08b6959d00c2ce29d54f")},
        {QStringLiteral("0.56.1|src/desktop/view/Group.cpp"),
         QStringLiteral("f34fec0891e69e0a1b67901e8d1b6b1a1ae8a47eccc92313c2b41660961cd385")},
        {QStringLiteral("0.56.1|src/layout/supplementary/DragController.cpp"),
         QStringLiteral("02560e7a38902cd400c3fa1b229eb6ba2c9494750413116b9277f5e8a818b62c")},
        {QStringLiteral("0.56.1|src/render/decorations/CHyprGroupBarDecoration.cpp"),
         QStringLiteral("39cb87fc2b28c81433bfd34d3900e58c2c58f4f336be728e07461a8f16c095e6")},
        {QStringLiteral("0.56.1|src/config/shared/actions/ConfigActions.cpp"),
         QStringLiteral("29c9339ec15943d685975eb952af207fe52820c20bb15ecb0cc0b19661ac5dab")},
        {QStringLiteral("0.56.1|src/desktop/state/GlobalWindowController.cpp"),
         QStringLiteral("669cb209f2e4efb2b248bbbc00ef8cef84a4638017a30dc085c8f85fdb2d65f8")},
    };
    QSet<QString> groupBehaviorSourceKeys;
    for (const auto &value : groupBehaviorSources) {
      const auto source = value.toObject();
      const auto version = source.value(QStringLiteral("version")).toString();
      const auto path = source.value(QStringLiteral("path")).toString();
      const auto key = version + QLatin1Char('|') + path;
      QVERIFY2(expectedGroupBehaviorHashes.contains(key), qPrintable(key));
      QVERIFY2(!groupBehaviorSourceKeys.contains(key), qPrintable(key));
      groupBehaviorSourceKeys.insert(key);
      QCOMPARE(source.value(QStringLiteral("tag")),
               QJsonValue(expectedGroupBehaviorTags.value(version)));
      QCOMPARE(source.value(QStringLiteral("commit")),
               QJsonValue(expectedGroupBehaviorCommits.value(version)));
      QCOMPARE(source.value(QStringLiteral("sha256")),
               QJsonValue(expectedGroupBehaviorHashes.value(key)));
    }
    QCOMPARE(groupBehaviorSourceKeys,
             QSet<QString>(expectedGroupBehaviorHashes.keyBegin(),
                           expectedGroupBehaviorHashes.keyEnd()));

    const auto appearanceBehaviorSources =
        manifest.value(QStringLiteral("appearanceBehaviorSources")).toArray();
    QCOMPARE(appearanceBehaviorSources.size(), 101);
    const QMap<QString, QString> expectedAppearanceBehaviorHashes{
        {QStringLiteral("0.55.0|src/config/values/ConfigValues.cpp"),
         QStringLiteral("290ac2deca427712edc255989010f30bf7d5c4104c05ec920042436fc07d49ce")},
        {QStringLiteral("0.55.0|src/config/lua/ConfigManager.cpp"),
         QStringLiteral("204bd335c3dde44eb5d528859f8e480159988ef691b166d0ff21c7c184aec642")},
        {QStringLiteral("0.55.0|src/config/supplementary/propRefresher/PropRefresher.cpp"),
         QStringLiteral("6ee949391fa74f713d00b718e0e498fbbc1cc58e2931e1737c4c865fe8f6c679")},
        {QStringLiteral("0.55.0|src/Compositor.cpp"),
         QStringLiteral("a639c01f628e50d765856001c567aa8f344e6a620f69fca994e4436b3a090125")},
        {QStringLiteral("0.55.0|src/desktop/view/Window.cpp"),
         QStringLiteral("ec00fc5ca125163eadaa267eac73032de28d9d7f32a4b03ae4498a89427b2a7e")},
        {QStringLiteral("0.55.0|src/render/Renderer.cpp"),
         QStringLiteral("67febb2393cdad2671d172dbc84f230a735f579411ef0ee4f441421ef318cbf7")},
        {QStringLiteral("0.55.0|src/render/OpenGL.cpp"),
         QStringLiteral("2b1245e9207db6e33191a509fb45c1cdf1f1380146e55d61b56ea920857ea1e3")},
        {QStringLiteral("0.55.0|src/render/pass/Pass.cpp"),
         QStringLiteral("e78eb11477d673ade97ad7a12486ebd97366708bd5e634b5a7f6901fe649c9e1")},
        {QStringLiteral("0.55.0|src/render/ShaderLoader.hpp"),
         QStringLiteral("8e35c2300fa597bdad275dbb1e2c14067a6440d4b4032e0881cdc4765190fb93")},
        {QStringLiteral("0.55.0|src/render/Shader.cpp"),
         QStringLiteral("2e98805b5082cb48e88fa364c3703087f574cb781903de07d833758581684f78")},
        {QStringLiteral("0.55.0|src/render/GLRenderer.cpp"),
         QStringLiteral("a2f5aac8e77ec4a96a87b038d36f72e3b84af3325f27cf9cb1d8e2d6991773c3")},
        {QStringLiteral("0.55.0|src/render/ElementRenderer.cpp"),
         QStringLiteral("5dbe22ebdb367af0d6e3e4fd566c4532b98d949007668249c957de075a28a24e")},
        {QStringLiteral("0.55.0|src/render/gl/GLElementRenderer.cpp"),
         QStringLiteral("1c14d08921b57a051fba09fd272d0efb5b4dd10452b7e8bd675ba548214a0391")},
        {QStringLiteral("0.55.0|src/render/pass/PreBlurElement.hpp"),
         QStringLiteral("50675b156230bc29e1ef11844f360cc5e01d0b514ee10ae817a4a70f3ae38528")},
        {QStringLiteral("0.55.0|src/render/pass/PreBlurElement.cpp"),
         QStringLiteral("308c85d990a3cbe1458aee1990fa91ca76cc6118fde9b3a961e42d40282c77d5")},
        {QStringLiteral("0.55.0|src/render/shaders/glsl/blurprepare.frag"),
         QStringLiteral("e870f47f282e21cc5a8534f1b62ebf6c5dc96fb7ab53e3db0582ec80b9b5d1c3")},
        {QStringLiteral("0.55.0|src/render/shaders/glsl/blurprepare.glsl"),
         QStringLiteral("219d76ec643ce3055d295850f54365412ba2eb4180015c59e1764807ba64a56d")},
        {QStringLiteral("0.55.0|src/render/shaders/glsl/blurfinish.frag"),
         QStringLiteral("77a16587e972ced698da747f29a1c0fd91a70f79b9db75beeab3916391710748")},
        {QStringLiteral("0.55.0|src/render/shaders/glsl/blurFinish.glsl"),
         QStringLiteral("ab3df5ec456f60c83a3af85796816b5d99351cca309eaf7023eddf0dd695c731")},
        {QStringLiteral("0.55.0|src/render/shaders/glsl/blur1.frag"),
         QStringLiteral("ea384ff735b47417aac1873579582dca3a8e57e9bbde0b643e6f15b2ffae3c68")},
        {QStringLiteral("0.55.0|src/render/shaders/glsl/blur1.glsl"),
         QStringLiteral("ff0985d2b95f6d927d39a2058379283ac6118ab75b47b90a536fa4da4cc0fd4d")},
        {QStringLiteral("0.55.0|src/render/shaders/glsl/gain.glsl"),
         QStringLiteral("fae6204d8e51d368534b23988e5de84c5d3bd2e25a8d95c5266a9d0bbc0c4bfb")},
        {QStringLiteral("0.55.0|src/desktop/Workspace.cpp"),
         QStringLiteral("5e7d585513e7d4c79c93edc5303d578ae4cbdb337a2128f37358f754bac0c5f6")},
        {QStringLiteral("0.55.0|src/render/decorations/CHyprBorderDecoration.cpp"),
         QStringLiteral("b03a9c649c158403f88da98fc686514e587cd4a1bf13eb8bd143f1e5edab8d74")},
        {QStringLiteral("0.55.0|src/render/decorations/DecorationPositioner.cpp"),
         QStringLiteral("aea200570600718a05977daf1beb2a2188058e7f0290e36a9ab5b0029b47629d")},
        {QStringLiteral("0.55.0|src/render/decorations/CHyprDropShadowDecoration.cpp"),
         QStringLiteral("d2bf3906c2d63633200b222e03c1f664a8bb4f90fb81f46500af3f18335d56cb")},
        {QStringLiteral("0.55.0|src/desktop/rule/windowRule/WindowRuleApplicator.hpp"),
         QStringLiteral("30a8f21c37fbb2207ba9647d2432916b3f3ba1214a8788fc9a9c599a6cdd931e")},
        {QStringLiteral("0.55.0|src/desktop/view/Window.hpp"),
         QStringLiteral("c52d94684d4dfab1fe80df565a08b560c3fd8e8ca32571436c8fa6f569f76e42")},
        {QStringLiteral("0.55.0|src/render/Renderer.hpp"),
         QStringLiteral("196404adaf7c2af5f9f7ad0383c95c0a89d1d5c4e560941f38ea10b960650d24")},
        {QStringLiteral("0.55.0|src/render/OpenGL.hpp"),
         QStringLiteral("f1d33ee262601cc3a2d5df0d694b7352de5f401d8063a487e16a092f255c5e3e")},
        {QStringLiteral("0.55.0|src/render/decorations/CHyprDropShadowDecoration.hpp"),
         QStringLiteral("aaa43c9c130461a7558f17b78a302ebd9409137d79aebe34417f9fe7c12905e6")},
        {QStringLiteral("0.55.0|src/render/pass/BorderPassElement.hpp"),
         QStringLiteral("0aafff3cbfc3290bd725a6b2ea2f4a10f7f47f5505108ce36569395e7fc53efd")},
        {QStringLiteral("0.55.0|src/render/pass/RectPassElement.hpp"),
         QStringLiteral("29b59dc0f46df48ea6624a1bec7705f359ddb99390458a9d5c8914c63fcb3569")},
        {QStringLiteral("0.55.0|src/render/pass/SurfacePassElement.hpp"),
         QStringLiteral("e5fe6f6213f21197c23773caef98742eb3d27377ff7342f1a75b0c020cc04717")},
        {QStringLiteral("0.55.0|src/render/pass/TexPassElement.hpp"),
         QStringLiteral("d95d048fc0ec7ad06cce2cb1799b1cf46e3b08d632cae3429866f8428ad4d4e2")},
        {QStringLiteral("0.55.0|src/render/shaders/glsl/border.frag"),
         QStringLiteral("c477ed2719e7d4889eb1e7b40dae897da64b9c2b515892f1996303de85556ef3")},
        {QStringLiteral("0.55.0|src/render/shaders/glsl/border.glsl"),
         QStringLiteral("e476ed34604e3e3cebbac55e8d0fb628d8f297eb643db304abd317d79869d112")},
        {QStringLiteral("0.55.0|src/render/shaders/glsl/ext.frag"),
         QStringLiteral("d2164a4529ecbef68ec8ce5f6d459a71762bbe6d9b5d118ddbc478d9e750a3c7")},
        {QStringLiteral("0.55.0|src/render/shaders/glsl/quad.frag"),
         QStringLiteral("28862349478c2ea5d1122aeef9b38e79e230ce358a2c659ec7f074b73b84afee")},
        {QStringLiteral("0.55.0|src/render/shaders/glsl/rounding.glsl"),
         QStringLiteral("67efb089576ed2b8ddb327369e75049cccc822564e0e5d845d9c6f325f4de9dc")},
        {QStringLiteral("0.55.0|src/render/shaders/glsl/shadow.frag"),
         QStringLiteral("16d1f5c7efcde9d31a4690a2c25e2db6b89a61ab97ed0f97b467a4097715ef60")},
        {QStringLiteral("0.55.0|src/render/shaders/glsl/shadow.glsl"),
         QStringLiteral("6f925478b303f8a467c926cb33fb5f6f197e22bfb5819665cf3cf95f785abdb1")},
        {QStringLiteral("0.55.0|src/render/shaders/glsl/surface.frag"),
         QStringLiteral("6830e2db330843fcb4917fa4ed7118767999ca25b4840d135d4d8cea8143bb48")},
        {QStringLiteral("0.55.0|src/render/decorations/CHyprInnerGlowDecoration.cpp"),
         QStringLiteral("0d8fcb1608d6f355e8a8dae63b2dd715c89e7f565a0611e4794982d97d895938")},
        {QStringLiteral("0.55.0|src/render/shaders/glsl/inner_glow.frag"),
         QStringLiteral("d595247c788e9193926b7826224bf4bb49f34579d2fded34c708ebd39bfd27e1")},
        {QStringLiteral("0.55.0|src/render/shaders/glsl/inner_glow.glsl"),
         QStringLiteral("3428e751180963f3c93423ca0cf29898aca481e8869be93a7f080b67d18517dd")},
        {QStringLiteral("0.55.0|src/protocols/OutputManagement.cpp"),
         QStringLiteral("619e3854259769282c2aa07496158af0a80d4cfa340e67bf937a958e097ba96f")},
        {QStringLiteral("0.55.0|src/config/shared/monitor/MonitorRuleManager.cpp"),
         QStringLiteral("fac5ef95c741c8fb6b46802d3711f579cba23b5e2f44d051e5e3a0ed568e8b14")},
        {QStringLiteral("0.55.0|src/helpers/Monitor.cpp"),
         QStringLiteral("c84ad7cafd85bdc8192dd30d8441f8d417f7d379d9deee4ee71ef6666ef26a0f")},
        {QStringLiteral("0.56.1|src/config/values/ConfigValues.cpp"),
         QStringLiteral("a76f05079454e1f6d4402144e1673cc1cb890d285fbeb947d5afdb004ad97754")},
        {QStringLiteral("0.56.1|src/config/lua/ConfigManager.cpp"),
         QStringLiteral("94b5c6891326e7a3e490019d26fd9d6f5b3137fb5318a94dff0a6cb6dfdaf502")},
        {QStringLiteral("0.56.1|src/config/supplementary/propRefresher/PropRefresher.cpp"),
         QStringLiteral("65f01a1a50ef05ecb1630f8fd30bbbac91029b29f6b4b6bc3f353d0bb4b2817b")},
        {QStringLiteral("0.56.1|src/desktop/state/GlobalWindowController.cpp"),
         QStringLiteral("669cb209f2e4efb2b248bbbc00ef8cef84a4638017a30dc085c8f85fdb2d65f8")},
        {QStringLiteral("0.56.1|src/desktop/view/Window.cpp"),
         QStringLiteral("4d9219c87cfba1105a30c2b742b0728808f2339c813d08b6959d00c2ce29d54f")},
        {QStringLiteral("0.56.1|src/render/Renderer.cpp"),
         QStringLiteral("4c3b2a4d42fbb1021a7cbb5013e826b926a9756ecde40ef5e9acdc3a9a16c4a7")},
        {QStringLiteral("0.56.1|src/output/Monitor.cpp"),
         QStringLiteral("9cf88e154eb5dae676c79d37b5b055ca6134838857cecdbb89a3b747a6821927")},
        {QStringLiteral("0.56.1|src/desktop/state/LayerFadeout.cpp"),
         QStringLiteral("913541971166a171d5c922b63fbbda3be51afe2c25c43f54df97cb7fa5b9b254")},
        {QStringLiteral("0.56.1|src/desktop/state/WindowFadeout.cpp"),
         QStringLiteral("510e3f19d645585e55ced3fb090037c7e4f003e67fdba1b65dcbb5ff92ff03fe")},
        {QStringLiteral("0.56.1|src/render/OpenGL.cpp"),
         QStringLiteral("6b58dcffd17364f4f98719f3b3dab342dff1e2ca0b922a65cc2bfb664f81ea16")},
        {QStringLiteral("0.56.1|src/render/pass/Pass.cpp"),
         QStringLiteral("529c5c9d55dd707c4bc98da38832b0006880d3948592678a31db62e876e34820")},
        {QStringLiteral("0.56.1|src/desktop/state/PopupFadeout.cpp"),
         QStringLiteral("d3498272769a78ef6f700f858f454c1bed056425df23a2c3003a6fb43b08879f")},
        {QStringLiteral("0.56.1|src/render/ShaderLoader.hpp"),
         QStringLiteral("7f3c109c8ac1c044c0b48b56e98b977ad29b6138fed9f45041163a83eea23663")},
        {QStringLiteral("0.56.1|src/render/Shader.cpp"),
         QStringLiteral("dcf574e0b64c246d12fe03bff6af4c9631b95baa18d56e481d6d37ffa7b64f64")},
        {QStringLiteral("0.56.1|src/render/GLRenderer.cpp"),
         QStringLiteral("2072807043b5d53a0910ebcc3936e69905abdf5905457d363f70d8c1043a4a81")},
        {QStringLiteral("0.56.1|src/render/ElementRenderer.cpp"),
         QStringLiteral("32f79c359a5ae6a265f7b2feee5c190d643371dd984d2bdda6a4ef203ccec828")},
        {QStringLiteral("0.56.1|src/render/gl/GLElementRenderer.cpp"),
         QStringLiteral("d7db3c34fbd94c3acdef3dc821d906a48583ac188704ec1943959df1c88239f8")},
        {QStringLiteral("0.56.1|src/render/pass/PreBlurElement.hpp"),
         QStringLiteral("50675b156230bc29e1ef11844f360cc5e01d0b514ee10ae817a4a70f3ae38528")},
        {QStringLiteral("0.56.1|src/render/pass/PreBlurElement.cpp"),
         QStringLiteral("308c85d990a3cbe1458aee1990fa91ca76cc6118fde9b3a961e42d40282c77d5")},
        {QStringLiteral("0.56.1|src/render/shaders/glsl/blurprepare.frag"),
         QStringLiteral("e870f47f282e21cc5a8534f1b62ebf6c5dc96fb7ab53e3db0582ec80b9b5d1c3")},
        {QStringLiteral("0.56.1|src/render/shaders/glsl/blurprepare.glsl"),
         QStringLiteral("1e359628ecdae3bd769e6a2c00b13d09df9319c88442730f5d8510c78feaeb36")},
        {QStringLiteral("0.56.1|src/render/shaders/glsl/blurfinish.frag"),
         QStringLiteral("77a16587e972ced698da747f29a1c0fd91a70f79b9db75beeab3916391710748")},
        {QStringLiteral("0.56.1|src/render/shaders/glsl/blurFinish.glsl"),
         QStringLiteral("f3c1f38430bb362552584c8f8ad5b52ac2838b18d576eb1d87fdbbbba6258c27")},
        {QStringLiteral("0.56.1|src/render/shaders/glsl/blur1.frag"),
         QStringLiteral("ea384ff735b47417aac1873579582dca3a8e57e9bbde0b643e6f15b2ffae3c68")},
        {QStringLiteral("0.56.1|src/render/shaders/glsl/blur1.glsl"),
         QStringLiteral("ff0985d2b95f6d927d39a2058379283ac6118ab75b47b90a536fa4da4cc0fd4d")},
        {QStringLiteral("0.56.1|src/render/shaders/glsl/gain.glsl"),
         QStringLiteral("fae6204d8e51d368534b23988e5de84c5d3bd2e25a8d95c5266a9d0bbc0c4bfb")},
        {QStringLiteral("0.56.1|src/desktop/Workspace.cpp"),
         QStringLiteral("e6c8e44d9f8211a8f56b65b433b5f5e4c3e6565479ecb2d749bc02cf4e926ca9")},
        {QStringLiteral("0.56.1|src/render/decorations/CHyprBorderDecoration.cpp"),
         QStringLiteral("23ef95cfd33ec3116dbcc0de0a521809a8752df80d83df926ae2771631f48b46")},
        {QStringLiteral("0.56.1|src/render/decorations/DecorationPositioner.cpp"),
         QStringLiteral("2435b97c90534fd3cf8a859f8c23d31a9ba89e4b5b7eed529e10bc82f7676b6c")},
        {QStringLiteral("0.56.1|src/render/decorations/CHyprDropShadowDecoration.cpp"),
         QStringLiteral("f83870a33d8d7b4d28f2c03ecdf0673d4bfef2192da189a1fe03684c9a6fce41")},
        {QStringLiteral("0.56.1|src/desktop/rule/windowRule/WindowRuleApplicator.hpp"),
         QStringLiteral("334e1254a66e2df8ef243544e3ff99db398ed5ec6e48996d496d05ff7bf50f44")},
        {QStringLiteral("0.56.1|src/desktop/view/Window.hpp"),
         QStringLiteral("1f60b8ff0bc6b5a48af5c03cb2cac1108db677efc70b4424b56ef6f1f2d1cc81")},
        {QStringLiteral("0.56.1|src/render/Renderer.hpp"),
         QStringLiteral("0787f259d788e979094ebb5f3b40140c642118c634356a431e2aca17ecf4d5a2")},
        {QStringLiteral("0.56.1|src/render/OpenGL.hpp"),
         QStringLiteral("e08060838c88d6bf650fd2e2b695acbda48e10f04a317a397d35f2f1dff9c813")},
        {QStringLiteral("0.56.1|src/render/decorations/CHyprDropShadowDecoration.hpp"),
         QStringLiteral("013d55c2923012159989fc06cecd655ed75e51578c6f086254e5d840238c33bc")},
        {QStringLiteral("0.56.1|src/render/pass/BorderPassElement.hpp"),
         QStringLiteral("0aafff3cbfc3290bd725a6b2ea2f4a10f7f47f5505108ce36569395e7fc53efd")},
        {QStringLiteral("0.56.1|src/render/pass/RectPassElement.hpp"),
         QStringLiteral("29b59dc0f46df48ea6624a1bec7705f359ddb99390458a9d5c8914c63fcb3569")},
        {QStringLiteral("0.56.1|src/render/pass/SurfacePassElement.hpp"),
         QStringLiteral("74bd3d56f5d6e75b43fa9d8291141869849341f4b5ec9d10f6c7ed3f5b55c2d5")},
        {QStringLiteral("0.56.1|src/render/pass/TexPassElement.hpp"),
         QStringLiteral("9f3a858292d9b2892df0f8b64ee605d2d4322cb57dd3bb5b3ed12e1f0638de3a")},
        {QStringLiteral("0.56.1|src/render/shaders/glsl/border.frag"),
         QStringLiteral("bbcd85dd46b97d995418aedc6bb42e0f7e2500a855a777452f2ecb61598d8c49")},
        {QStringLiteral("0.56.1|src/render/shaders/glsl/border.glsl"),
         QStringLiteral("730875c35be7bc002940f8db0b363a65cbc6659f7630a21eb0a46410d1fa619a")},
        {QStringLiteral("0.56.1|src/render/shaders/glsl/ext.frag"),
         QStringLiteral("d2164a4529ecbef68ec8ce5f6d459a71762bbe6d9b5d118ddbc478d9e750a3c7")},
        {QStringLiteral("0.56.1|src/render/shaders/glsl/quad.frag"),
         QStringLiteral("28862349478c2ea5d1122aeef9b38e79e230ce358a2c659ec7f074b73b84afee")},
        {QStringLiteral("0.56.1|src/render/shaders/glsl/rounding.glsl"),
         QStringLiteral("67efb089576ed2b8ddb327369e75049cccc822564e0e5d845d9c6f325f4de9dc")},
        {QStringLiteral("0.56.1|src/render/shaders/glsl/shadow.frag"),
         QStringLiteral("2f2c44a1d3a0733fc1557c3789bae26188410ba6e177baf1648976fb824b4c14")},
        {QStringLiteral("0.56.1|src/render/shaders/glsl/shadow.glsl"),
         QStringLiteral("c4ca78af1d094eb0f996a06ac14bdda4c45a8d993da392b1b505ed513714e15d")},
        {QStringLiteral("0.56.1|src/render/shaders/glsl/surface.frag"),
         QStringLiteral("26fb659bf56460aa051b28ee8b3ce9fed4ced34fcc836aede291058dd8195970")},
        {QStringLiteral("0.56.1|src/render/decorations/CHyprInnerGlowDecoration.cpp"),
         QStringLiteral("c857934b45dfe308c56f0a45defd25d9b7e0c1a07e05ff52255c9c9441986c69")},
        {QStringLiteral("0.56.1|src/render/shaders/glsl/inner_glow.frag"),
         QStringLiteral("60b5bf2771382af07a19cee5e66fe909030bbe087217f9d9baaf122bcd395e02")},
        {QStringLiteral("0.56.1|src/render/shaders/glsl/inner_glow.glsl"),
         QStringLiteral("7858f0226aebfabdc78e6eaae3cba524ec3173b2d59a021d5cdc0bb54a66293b")},
        {QStringLiteral("0.56.1|src/protocols/OutputManagement.cpp"),
         QStringLiteral("148f12d73ced4e045bc864ebc3cc5022e3681b82bbec9936f2cd8b32d0bbd588")},
        {QStringLiteral("0.56.1|src/config/shared/monitor/MonitorRuleManager.cpp"),
         QStringLiteral("8946d306e66afc42c1d3b49fbbadd5ccf931c630896bda8430e6aaf7b3a7ea65")},
    };
    const QList<QString> expectedAppearanceBehaviorOrder{
        QStringLiteral("0.55.0|src/config/values/ConfigValues.cpp"),
        QStringLiteral("0.55.0|src/config/lua/ConfigManager.cpp"),
        QStringLiteral("0.55.0|src/config/supplementary/propRefresher/PropRefresher.cpp"),
        QStringLiteral("0.55.0|src/Compositor.cpp"),
        QStringLiteral("0.55.0|src/desktop/view/Window.cpp"),
        QStringLiteral("0.55.0|src/render/Renderer.cpp"),
        QStringLiteral("0.55.0|src/render/OpenGL.cpp"),
        QStringLiteral("0.55.0|src/render/pass/Pass.cpp"),
        QStringLiteral("0.55.0|src/render/ShaderLoader.hpp"),
        QStringLiteral("0.55.0|src/render/Shader.cpp"),
        QStringLiteral("0.55.0|src/render/GLRenderer.cpp"),
        QStringLiteral("0.55.0|src/render/ElementRenderer.cpp"),
        QStringLiteral("0.55.0|src/render/gl/GLElementRenderer.cpp"),
        QStringLiteral("0.55.0|src/render/pass/PreBlurElement.hpp"),
        QStringLiteral("0.55.0|src/render/pass/PreBlurElement.cpp"),
        QStringLiteral("0.55.0|src/render/shaders/glsl/blurprepare.frag"),
        QStringLiteral("0.55.0|src/render/shaders/glsl/blurprepare.glsl"),
        QStringLiteral("0.55.0|src/render/shaders/glsl/blurfinish.frag"),
        QStringLiteral("0.55.0|src/render/shaders/glsl/blurFinish.glsl"),
        QStringLiteral("0.55.0|src/render/shaders/glsl/blur1.frag"),
        QStringLiteral("0.55.0|src/render/shaders/glsl/blur1.glsl"),
        QStringLiteral("0.55.0|src/render/shaders/glsl/gain.glsl"),
        QStringLiteral("0.55.0|src/desktop/Workspace.cpp"),
        QStringLiteral("0.55.0|src/render/decorations/CHyprBorderDecoration.cpp"),
        QStringLiteral("0.55.0|src/render/decorations/DecorationPositioner.cpp"),
        QStringLiteral("0.55.0|src/render/decorations/CHyprDropShadowDecoration.cpp"),
        QStringLiteral("0.55.0|src/desktop/rule/windowRule/WindowRuleApplicator.hpp"),
        QStringLiteral("0.55.0|src/desktop/view/Window.hpp"),
        QStringLiteral("0.55.0|src/render/Renderer.hpp"),
        QStringLiteral("0.55.0|src/render/OpenGL.hpp"),
        QStringLiteral("0.55.0|src/render/decorations/CHyprDropShadowDecoration.hpp"),
        QStringLiteral("0.55.0|src/render/pass/BorderPassElement.hpp"),
        QStringLiteral("0.55.0|src/render/pass/RectPassElement.hpp"),
        QStringLiteral("0.55.0|src/render/pass/SurfacePassElement.hpp"),
        QStringLiteral("0.55.0|src/render/pass/TexPassElement.hpp"),
        QStringLiteral("0.55.0|src/render/shaders/glsl/border.frag"),
        QStringLiteral("0.55.0|src/render/shaders/glsl/border.glsl"),
        QStringLiteral("0.55.0|src/render/shaders/glsl/ext.frag"),
        QStringLiteral("0.55.0|src/render/shaders/glsl/quad.frag"),
        QStringLiteral("0.55.0|src/render/shaders/glsl/rounding.glsl"),
        QStringLiteral("0.55.0|src/render/shaders/glsl/shadow.frag"),
        QStringLiteral("0.55.0|src/render/shaders/glsl/shadow.glsl"),
        QStringLiteral("0.55.0|src/render/shaders/glsl/surface.frag"),
        QStringLiteral("0.55.0|src/render/decorations/CHyprInnerGlowDecoration.cpp"),
        QStringLiteral("0.55.0|src/render/shaders/glsl/inner_glow.frag"),
        QStringLiteral("0.55.0|src/render/shaders/glsl/inner_glow.glsl"),
        QStringLiteral("0.55.0|src/protocols/OutputManagement.cpp"),
        QStringLiteral("0.55.0|src/config/shared/monitor/MonitorRuleManager.cpp"),
        QStringLiteral("0.55.0|src/helpers/Monitor.cpp"),
        QStringLiteral("0.56.1|src/config/values/ConfigValues.cpp"),
        QStringLiteral("0.56.1|src/config/lua/ConfigManager.cpp"),
        QStringLiteral("0.56.1|src/config/supplementary/propRefresher/PropRefresher.cpp"),
        QStringLiteral("0.56.1|src/desktop/state/GlobalWindowController.cpp"),
        QStringLiteral("0.56.1|src/desktop/view/Window.cpp"),
        QStringLiteral("0.56.1|src/render/Renderer.cpp"),
        QStringLiteral("0.56.1|src/output/Monitor.cpp"),
        QStringLiteral("0.56.1|src/desktop/state/LayerFadeout.cpp"),
        QStringLiteral("0.56.1|src/desktop/state/WindowFadeout.cpp"),
        QStringLiteral("0.56.1|src/render/OpenGL.cpp"),
        QStringLiteral("0.56.1|src/render/pass/Pass.cpp"),
        QStringLiteral("0.56.1|src/desktop/state/PopupFadeout.cpp"),
        QStringLiteral("0.56.1|src/render/ShaderLoader.hpp"),
        QStringLiteral("0.56.1|src/render/Shader.cpp"),
        QStringLiteral("0.56.1|src/render/GLRenderer.cpp"),
        QStringLiteral("0.56.1|src/render/ElementRenderer.cpp"),
        QStringLiteral("0.56.1|src/render/gl/GLElementRenderer.cpp"),
        QStringLiteral("0.56.1|src/render/pass/PreBlurElement.hpp"),
        QStringLiteral("0.56.1|src/render/pass/PreBlurElement.cpp"),
        QStringLiteral("0.56.1|src/render/shaders/glsl/blurprepare.frag"),
        QStringLiteral("0.56.1|src/render/shaders/glsl/blurprepare.glsl"),
        QStringLiteral("0.56.1|src/render/shaders/glsl/blurfinish.frag"),
        QStringLiteral("0.56.1|src/render/shaders/glsl/blurFinish.glsl"),
        QStringLiteral("0.56.1|src/render/shaders/glsl/blur1.frag"),
        QStringLiteral("0.56.1|src/render/shaders/glsl/blur1.glsl"),
        QStringLiteral("0.56.1|src/render/shaders/glsl/gain.glsl"),
        QStringLiteral("0.56.1|src/desktop/Workspace.cpp"),
        QStringLiteral("0.56.1|src/render/decorations/CHyprBorderDecoration.cpp"),
        QStringLiteral("0.56.1|src/render/decorations/DecorationPositioner.cpp"),
        QStringLiteral("0.56.1|src/render/decorations/CHyprDropShadowDecoration.cpp"),
        QStringLiteral("0.56.1|src/desktop/rule/windowRule/WindowRuleApplicator.hpp"),
        QStringLiteral("0.56.1|src/desktop/view/Window.hpp"),
        QStringLiteral("0.56.1|src/render/Renderer.hpp"),
        QStringLiteral("0.56.1|src/render/OpenGL.hpp"),
        QStringLiteral("0.56.1|src/render/decorations/CHyprDropShadowDecoration.hpp"),
        QStringLiteral("0.56.1|src/render/pass/BorderPassElement.hpp"),
        QStringLiteral("0.56.1|src/render/pass/RectPassElement.hpp"),
        QStringLiteral("0.56.1|src/render/pass/SurfacePassElement.hpp"),
        QStringLiteral("0.56.1|src/render/pass/TexPassElement.hpp"),
        QStringLiteral("0.56.1|src/render/shaders/glsl/border.frag"),
        QStringLiteral("0.56.1|src/render/shaders/glsl/border.glsl"),
        QStringLiteral("0.56.1|src/render/shaders/glsl/ext.frag"),
        QStringLiteral("0.56.1|src/render/shaders/glsl/quad.frag"),
        QStringLiteral("0.56.1|src/render/shaders/glsl/rounding.glsl"),
        QStringLiteral("0.56.1|src/render/shaders/glsl/shadow.frag"),
        QStringLiteral("0.56.1|src/render/shaders/glsl/shadow.glsl"),
        QStringLiteral("0.56.1|src/render/shaders/glsl/surface.frag"),
        QStringLiteral("0.56.1|src/render/decorations/CHyprInnerGlowDecoration.cpp"),
        QStringLiteral("0.56.1|src/render/shaders/glsl/inner_glow.frag"),
        QStringLiteral("0.56.1|src/render/shaders/glsl/inner_glow.glsl"),
        QStringLiteral("0.56.1|src/protocols/OutputManagement.cpp"),
        QStringLiteral("0.56.1|src/config/shared/monitor/MonitorRuleManager.cpp"),
    };
    QSet<QString> appearanceBehaviorSourceKeys;
    QList<QString> appearanceBehaviorSourceOrder;
    for (const auto &value : appearanceBehaviorSources) {
      const auto source = value.toObject();
      QCOMPARE(source.size(), 5);
      const auto version = source.value(QStringLiteral("version")).toString();
      const auto path = source.value(QStringLiteral("path")).toString();
      const auto key = version + QLatin1Char('|') + path;
      QVERIFY2(expectedAppearanceBehaviorHashes.contains(key), qPrintable(key));
      QVERIFY2(!appearanceBehaviorSourceKeys.contains(key), qPrintable(key));
      appearanceBehaviorSourceKeys.insert(key);
      appearanceBehaviorSourceOrder.append(key);
      QCOMPARE(source.value(QStringLiteral("tag")),
               QJsonValue(expectedGroupBehaviorTags.value(version)));
      QCOMPARE(source.value(QStringLiteral("commit")),
               QJsonValue(expectedGroupBehaviorCommits.value(version)));
      QCOMPARE(source.value(QStringLiteral("sha256")),
               QJsonValue(expectedAppearanceBehaviorHashes.value(key)));
    }
    QCOMPARE(appearanceBehaviorSourceKeys,
             QSet<QString>(expectedAppearanceBehaviorHashes.keyBegin(),
                           expectedAppearanceBehaviorHashes.keyEnd()));
    QCOMPARE(appearanceBehaviorSourceOrder, expectedAppearanceBehaviorOrder);

    const auto advancedRuntimeSources =
        manifest.value(QStringLiteral("advancedRuntimeSources")).toArray();
    QCOMPARE(advancedRuntimeSources.size(), 42);
    const QMap<QString, QString> expectedAdvancedRuntimeTags{
        {QStringLiteral("0.55.0"), QStringLiteral("v0.55.0")},
        {QStringLiteral("0.56.0"), QStringLiteral("v0.56.0")},
        {QStringLiteral("0.56.1"), QStringLiteral("v0.56.1")},
    };
    const QMap<QString, QString> expectedAdvancedRuntimeCommits{
        {QStringLiteral("0.55.0"),
         QStringLiteral("af923e30d1d24f1f4a4f5cb8308065173c1d9539")},
        {QStringLiteral("0.56.0"),
         QStringLiteral("36b2e0cfe0c6094dbc47bd42a437431315bb3087")},
        {QStringLiteral("0.56.1"),
         QStringLiteral("5c9377c15f85c50648f35ca5a213754f95b93ca0")},
    };
    const QMap<QString, QString> expectedAdvancedRuntimeHashes{
        {QStringLiteral("0.55.0|src/config/values/ConfigValues.cpp"),
         QStringLiteral("290ac2deca427712edc255989010f30bf7d5c4104c05ec920042436fc07d49ce")},
        {QStringLiteral("0.55.0|src/config/lua/ConfigManager.cpp"),
         QStringLiteral("204bd335c3dde44eb5d528859f8e480159988ef691b166d0ff21c7c184aec642")},
        {QStringLiteral("0.55.0|src/config/supplementary/propRefresher/PropRefresher.cpp"),
         QStringLiteral("6ee949391fa74f713d00b718e0e498fbbc1cc58e2931e1737c4c865fe8f6c679")},
        {QStringLiteral("0.55.0|src/managers/SessionLockManager.cpp"),
         QStringLiteral("7bcea37c191c54401e743b718ea6475183a9dbcc33123bb6e26d88693db1227c")},
        {QStringLiteral("0.55.0|src/render/Renderer.cpp"),
         QStringLiteral("67febb2393cdad2671d172dbc84f230a735f579411ef0ee4f441421ef318cbf7")},
        {QStringLiteral("0.55.0|src/render/ElementRenderer.cpp"),
         QStringLiteral("5dbe22ebdb367af0d6e3e4fd566c4532b98d949007668249c957de075a28a24e")},
        {QStringLiteral("0.55.0|src/render/pass/TexPassElement.hpp"),
         QStringLiteral("d95d048fc0ec7ad06cce2cb1799b1cf46e3b08d632cae3429866f8428ad4d4e2")},
        {QStringLiteral("0.55.0|src/render/pass/TexPassElement.cpp"),
         QStringLiteral("b3b482a7d4c121a0163ff3120642a6490d237d84812e7d01db82a09fbc250b71")},
        {QStringLiteral("0.55.0|src/render/gl/GLElementRenderer.cpp"),
         QStringLiteral("1c14d08921b57a051fba09fd272d0efb5b4dd10452b7e8bd675ba548214a0391")},
        {QStringLiteral("0.55.0|src/render/OpenGL.cpp"),
         QStringLiteral("2b1245e9207db6e33191a509fb45c1cdf1f1380146e55d61b56ea920857ea1e3")},
        {QStringLiteral("0.55.0|src/render/pass/SurfacePassElement.hpp"),
         QStringLiteral("e5fe6f6213f21197c23773caef98742eb3d27377ff7342f1a75b0c020cc04717")},
        {QStringLiteral("0.55.0|src/render/pass/SurfacePassElement.cpp"),
         QStringLiteral("c5e3729400ca779e73b516ea9c9367962d5358c28bb4fc396e348ad7aa4d0f46")},
        {QStringLiteral("0.55.0|src/render/shaders/glsl/surface.frag"),
         QStringLiteral("6830e2db330843fcb4917fa4ed7118767999ca25b4840d135d4d8cea8143bb48")},
        {QStringLiteral("0.55.0|src/helpers/Monitor.cpp"),
         QStringLiteral("c84ad7cafd85bdc8192dd30d8441f8d417f7d379d9deee4ee71ef6666ef26a0f")},
        {QStringLiteral("0.55.0|src/managers/screenshare/ScreenshareSession.cpp"),
         QStringLiteral("865998b9669353b1653b8f0f78583931d017331c53f71b0956474bb3bd3d6f10")},
        {QStringLiteral("0.55.0|src/helpers/cm/ColorManagement.hpp"),
         QStringLiteral("a3eaf1dffb8bef9d18ce0be5bc5b270b9d07f6e605f7161b6c2c7bcd91dfa434")},
        {QStringLiteral("0.55.0|src/render/Framebuffer.cpp"),
         QStringLiteral("80ba41c93bb068a85ca94be6f95a6bd43b33f2da52618f2c7fb78c229f787648")},
        {QStringLiteral("0.55.0|src/helpers/MonitorResources.cpp"),
         QStringLiteral("18ff58ce9ed2268f361a49f22c7e5d9b951234b0cab5335accf628b6ac92bb4c")},
        {QStringLiteral("0.56.0|src/config/values/ConfigValues.cpp"),
         QStringLiteral("a76f05079454e1f6d4402144e1673cc1cb890d285fbeb947d5afdb004ad97754")},
        {QStringLiteral("0.56.0|src/config/lua/ConfigManager.cpp"),
         QStringLiteral("bf295818d6ad5a1f01aa708a6843a968b9cbb14228482421bbe0e4e5b26600ed")},
        {QStringLiteral("0.56.0|src/managers/input/InputManager.cpp"),
         QStringLiteral("030d4f734e1303b5ae92c5bbfbdf8ece1bc7debecbee7abaf28f1e53f6fca966")},
        {QStringLiteral("0.56.0|src/protocols/InputCapture.cpp"),
         QStringLiteral("d034a7f2dd7c5010bfb52cc4db7717119a6c1482efe2dfeeae9da2fadab1a0fb")},
        {QStringLiteral("0.56.1|src/config/values/ConfigValues.cpp"),
         QStringLiteral("a76f05079454e1f6d4402144e1673cc1cb890d285fbeb947d5afdb004ad97754")},
        {QStringLiteral("0.56.1|src/config/lua/ConfigManager.cpp"),
         QStringLiteral("94b5c6891326e7a3e490019d26fd9d6f5b3137fb5318a94dff0a6cb6dfdaf502")},
        {QStringLiteral("0.56.1|src/config/supplementary/propRefresher/PropRefresher.cpp"),
         QStringLiteral("65f01a1a50ef05ecb1630f8fd30bbbac91029b29f6b4b6bc3f353d0bb4b2817b")},
        {QStringLiteral("0.56.1|src/managers/input/InputManager.cpp"),
         QStringLiteral("07de27ef0f4c9a5c3bf14f42c07af29574d428a7b02412f785f90db30b03125e")},
        {QStringLiteral("0.56.1|src/protocols/InputCapture.cpp"),
         QStringLiteral("d034a7f2dd7c5010bfb52cc4db7717119a6c1482efe2dfeeae9da2fadab1a0fb")},
        {QStringLiteral("0.56.1|src/managers/SessionLockManager.cpp"),
         QStringLiteral("221e45b09e75ced356bf229169c207f43b4adf8e93147288c47de0cd04e9a9c0")},
        {QStringLiteral("0.56.1|src/render/Renderer.cpp"),
         QStringLiteral("4c3b2a4d42fbb1021a7cbb5013e826b926a9756ecde40ef5e9acdc3a9a16c4a7")},
        {QStringLiteral("0.56.1|src/render/ElementRenderer.cpp"),
         QStringLiteral("32f79c359a5ae6a265f7b2feee5c190d643371dd984d2bdda6a4ef203ccec828")},
        {QStringLiteral("0.56.1|src/render/pass/TexPassElement.hpp"),
         QStringLiteral("9f3a858292d9b2892df0f8b64ee605d2d4322cb57dd3bb5b3ed12e1f0638de3a")},
        {QStringLiteral("0.56.1|src/render/pass/TexPassElement.cpp"),
         QStringLiteral("60dcded5bef532e26ee1d4e57d1cd0d653abea406114d71c72d9e49ae83f526e")},
        {QStringLiteral("0.56.1|src/render/gl/GLElementRenderer.cpp"),
         QStringLiteral("d7db3c34fbd94c3acdef3dc821d906a48583ac188704ec1943959df1c88239f8")},
        {QStringLiteral("0.56.1|src/render/OpenGL.cpp"),
         QStringLiteral("6b58dcffd17364f4f98719f3b3dab342dff1e2ca0b922a65cc2bfb664f81ea16")},
        {QStringLiteral("0.56.1|src/render/pass/SurfacePassElement.hpp"),
         QStringLiteral("74bd3d56f5d6e75b43fa9d8291141869849341f4b5ec9d10f6c7ed3f5b55c2d5")},
        {QStringLiteral("0.56.1|src/render/pass/SurfacePassElement.cpp"),
         QStringLiteral("ea559690633f8f2e0da7709cb3c00f12b54bc20fbe631437633744676eff24f8")},
        {QStringLiteral("0.56.1|src/render/shaders/glsl/surface.frag"),
         QStringLiteral("26fb659bf56460aa051b28ee8b3ce9fed4ced34fcc836aede291058dd8195970")},
        {QStringLiteral("0.56.1|src/output/Monitor.cpp"),
         QStringLiteral("9cf88e154eb5dae676c79d37b5b055ca6134838857cecdbb89a3b747a6821927")},
        {QStringLiteral("0.56.1|src/managers/screenshare/ScreenshareSession.cpp"),
         QStringLiteral("b6b2ce5d682080131f374510e7cecae54925356bea541ef99feeff3aba394c99")},
        {QStringLiteral("0.56.1|src/helpers/cm/ColorManagement.hpp"),
         QStringLiteral("c9b4823032e12cb45907aac714a19c5de89e5565a773b4eccbecd83ddb5d4de6")},
        {QStringLiteral("0.56.1|src/render/Framebuffer.cpp"),
         QStringLiteral("80ba41c93bb068a85ca94be6f95a6bd43b33f2da52618f2c7fb78c229f787648")},
        {QStringLiteral("0.56.1|src/output/MonitorResources.cpp"),
         QStringLiteral("90d39fd2c43852611b4f90ca14cc2d213f97c4770123021d2733cd1970488b8b")},
    };
    const QList<QString> expectedAdvancedRuntimeOrder{
        QStringLiteral("0.55.0|src/config/values/ConfigValues.cpp"),
        QStringLiteral("0.55.0|src/config/lua/ConfigManager.cpp"),
        QStringLiteral("0.55.0|src/config/supplementary/propRefresher/PropRefresher.cpp"),
        QStringLiteral("0.55.0|src/managers/SessionLockManager.cpp"),
        QStringLiteral("0.55.0|src/render/Renderer.cpp"),
        QStringLiteral("0.55.0|src/render/ElementRenderer.cpp"),
        QStringLiteral("0.55.0|src/render/pass/TexPassElement.hpp"),
        QStringLiteral("0.55.0|src/render/pass/TexPassElement.cpp"),
        QStringLiteral("0.55.0|src/render/gl/GLElementRenderer.cpp"),
        QStringLiteral("0.55.0|src/render/OpenGL.cpp"),
        QStringLiteral("0.55.0|src/render/pass/SurfacePassElement.hpp"),
        QStringLiteral("0.55.0|src/render/pass/SurfacePassElement.cpp"),
        QStringLiteral("0.55.0|src/render/shaders/glsl/surface.frag"),
        QStringLiteral("0.55.0|src/helpers/Monitor.cpp"),
        QStringLiteral("0.55.0|src/managers/screenshare/ScreenshareSession.cpp"),
        QStringLiteral("0.55.0|src/helpers/cm/ColorManagement.hpp"),
        QStringLiteral("0.55.0|src/render/Framebuffer.cpp"),
        QStringLiteral("0.55.0|src/helpers/MonitorResources.cpp"),
        QStringLiteral("0.56.0|src/config/values/ConfigValues.cpp"),
        QStringLiteral("0.56.0|src/config/lua/ConfigManager.cpp"),
        QStringLiteral("0.56.0|src/managers/input/InputManager.cpp"),
        QStringLiteral("0.56.0|src/protocols/InputCapture.cpp"),
        QStringLiteral("0.56.1|src/config/values/ConfigValues.cpp"),
        QStringLiteral("0.56.1|src/config/lua/ConfigManager.cpp"),
        QStringLiteral("0.56.1|src/config/supplementary/propRefresher/PropRefresher.cpp"),
        QStringLiteral("0.56.1|src/managers/input/InputManager.cpp"),
        QStringLiteral("0.56.1|src/protocols/InputCapture.cpp"),
        QStringLiteral("0.56.1|src/managers/SessionLockManager.cpp"),
        QStringLiteral("0.56.1|src/render/Renderer.cpp"),
        QStringLiteral("0.56.1|src/render/ElementRenderer.cpp"),
        QStringLiteral("0.56.1|src/render/pass/TexPassElement.hpp"),
        QStringLiteral("0.56.1|src/render/pass/TexPassElement.cpp"),
        QStringLiteral("0.56.1|src/render/gl/GLElementRenderer.cpp"),
        QStringLiteral("0.56.1|src/render/OpenGL.cpp"),
        QStringLiteral("0.56.1|src/render/pass/SurfacePassElement.hpp"),
        QStringLiteral("0.56.1|src/render/pass/SurfacePassElement.cpp"),
        QStringLiteral("0.56.1|src/render/shaders/glsl/surface.frag"),
        QStringLiteral("0.56.1|src/output/Monitor.cpp"),
        QStringLiteral("0.56.1|src/managers/screenshare/ScreenshareSession.cpp"),
        QStringLiteral("0.56.1|src/helpers/cm/ColorManagement.hpp"),
        QStringLiteral("0.56.1|src/render/Framebuffer.cpp"),
        QStringLiteral("0.56.1|src/output/MonitorResources.cpp"),
    };
    QSet<QString> advancedRuntimeSourceKeys;
    QList<QString> advancedRuntimeSourceOrder;
    for (const auto &value : advancedRuntimeSources) {
      const auto source = value.toObject();
      const auto version = source.value(QStringLiteral("version")).toString();
      const auto path = source.value(QStringLiteral("path")).toString();
      const auto key = version + QLatin1Char('|') + path;
      QVERIFY2(expectedAdvancedRuntimeHashes.contains(key), qPrintable(key));
      QVERIFY2(!advancedRuntimeSourceKeys.contains(key), qPrintable(key));
      advancedRuntimeSourceKeys.insert(key);
      advancedRuntimeSourceOrder.append(key);
      QCOMPARE(source.value(QStringLiteral("tag")),
               QJsonValue(expectedAdvancedRuntimeTags.value(version)));
      QCOMPARE(source.value(QStringLiteral("commit")),
               QJsonValue(expectedAdvancedRuntimeCommits.value(version)));
      QCOMPARE(source.value(QStringLiteral("sha256")),
               QJsonValue(expectedAdvancedRuntimeHashes.value(key)));
    }
    QCOMPARE(advancedRuntimeSourceKeys,
             QSet<QString>(expectedAdvancedRuntimeHashes.keyBegin(),
                           expectedAdvancedRuntimeHashes.keyEnd()));
    QCOMPARE(advancedRuntimeSourceOrder, expectedAdvancedRuntimeOrder);

    const auto windowBehaviorSources =
        manifest.value(QStringLiteral("windowBehaviorSources")).toArray();
    QCOMPARE(windowBehaviorSources.size(), 27);
    const QMap<QString, QString> expectedWindowBehaviorHashes{
        {QStringLiteral("0.55.0|src/config/values/ConfigValues.cpp"),
         QStringLiteral("290ac2deca427712edc255989010f30bf7d5c4104c05ec920042436fc07d49ce")},
        {QStringLiteral("0.55.0|src/config/shared/actions/ConfigActions.cpp"),
         QStringLiteral("2b2b09799b4cca634d544d4103f07b82ef4b6c377131c858d2707a5e0494fe07")},
        {QStringLiteral("0.55.0|src/Compositor.cpp"),
         QStringLiteral("a639c01f628e50d765856001c567aa8f344e6a620f69fca994e4436b3a090125")},
        {QStringLiteral("0.55.0|src/managers/ANRManager.cpp"),
         QStringLiteral("67276cc0d2dccf516a1075112a5eb3592871c6d88d8e7665a4503194b4ae558b")},
        {QStringLiteral("0.55.0|src/layout/algorithm/tiled/dwindle/DwindleAlgorithm.cpp"),
         QStringLiteral("d5ec852d1b26fb55574ae184b6bfc86dd2a047f8c7d4c8c7e4748277c0e841cb")},
        {QStringLiteral("0.55.0|src/layout/algorithm/tiled/master/MasterAlgorithm.cpp"),
         QStringLiteral("7a95cbac2074f7cde31e3181e84bf146337dea586fce56900eb1fb55af5864c0")},
        {QStringLiteral("0.55.0|src/layout/algorithm/tiled/scrolling/ScrollingAlgorithm.cpp"),
         QStringLiteral("f1f95b5e53f048ef52f6a87be7ab3040da74ce6361dee17bd5d97ef583ee2f0f")},
        {QStringLiteral("0.55.0|src/layout/algorithm/tiled/monocle/MonocleAlgorithm.cpp"),
         QStringLiteral("00d5563d7c49f7df81378496e6e35a95356bac3f4369d5de7782ae7012f2990f")},
        {QStringLiteral("0.55.0|src/managers/input/InputManager.cpp"),
         QStringLiteral("410836e5695062779cf525ed35b53900d199490f0db5eeba85f12fb894053835")},
        {QStringLiteral("0.55.0|src/desktop/view/Window.cpp"),
         QStringLiteral("ec00fc5ca125163eadaa267eac73032de28d9d7f32a4b03ae4498a89427b2a7e")},
        {QStringLiteral("0.55.0|src/desktop/state/FocusState.cpp"),
         QStringLiteral("677c243ada837fda14aed11c55b435de0fd877549709158dda1dfb2c8530dc43")},
        {QStringLiteral("0.55.0|src/desktop/rule/windowRule/WindowRuleApplicator.cpp"),
         QStringLiteral("35e330cba1af5e07968f9b968c2719a46aa28728537e3d0e97d13f6ab4dcb247")},
        {QStringLiteral("0.55.0|src/layout/target/WindowTarget.cpp"),
         QStringLiteral("33f6f6dcbfb4e140f2cce04a8e06b8042c8e780617960618b023f9a9591b9a9d")},
        {QStringLiteral("0.56.1|src/config/values/ConfigValues.cpp"),
         QStringLiteral("a76f05079454e1f6d4402144e1673cc1cb890d285fbeb947d5afdb004ad97754")},
        {QStringLiteral("0.56.1|src/config/shared/actions/ConfigActions.cpp"),
         QStringLiteral("29c9339ec15943d685975eb952af207fe52820c20bb15ecb0cc0b19661ac5dab")},
        {QStringLiteral("0.56.1|src/desktop/state/WindowQuery.cpp"),
         QStringLiteral("5eede566e031769aa13247830f68a10077c78f4db864fdf17dcfa58f222a9beb")},
        {QStringLiteral("0.56.1|src/managers/fullscreen/FullscreenController.cpp"),
         QStringLiteral("581d92ef70588fce181b4b87a04e37f6de7b4777c24ad7fee34b21f941b706b0")},
        {QStringLiteral("0.56.1|src/managers/ANRManager.cpp"),
         QStringLiteral("842b795210ea83c735cb9a75c5e2f104507fa9285a460730dcd809e87892803e")},
        {QStringLiteral("0.56.1|src/layout/algorithm/tiled/dwindle/DwindleAlgorithm.cpp"),
         QStringLiteral("218c4d4ba7e7b34d1113da818347e260868451de0e5a6d699934871febc03b32")},
        {QStringLiteral("0.56.1|src/layout/algorithm/tiled/master/MasterAlgorithm.cpp"),
         QStringLiteral("ad3494f7292fbd717c1e9ab608e0b2fbd488cc85e88763317c563162c9e978df")},
        {QStringLiteral("0.56.1|src/layout/algorithm/tiled/scrolling/ScrollingAlgorithm.cpp"),
         QStringLiteral("bfec45e79d26bb21dea2c61809ac37baffffa48b6e0202a56e7522b22a1815f3")},
        {QStringLiteral("0.56.1|src/layout/algorithm/tiled/monocle/MonocleAlgorithm.cpp"),
         QStringLiteral("4a5735c12b4e2f1b1d29fc655b75c9bd04ff278138dea9013a3cb569566306fb")},
        {QStringLiteral("0.56.1|src/managers/input/InputManager.cpp"),
         QStringLiteral("07de27ef0f4c9a5c3bf14f42c07af29574d428a7b02412f785f90db30b03125e")},
        {QStringLiteral("0.56.1|src/desktop/view/Window.cpp"),
         QStringLiteral("4d9219c87cfba1105a30c2b742b0728808f2339c813d08b6959d00c2ce29d54f")},
        {QStringLiteral("0.56.1|src/desktop/state/FocusState.cpp"),
         QStringLiteral("9b971362bba97f200f0ef786237deed6b7b863066590d243cac11787e2a56ebd")},
        {QStringLiteral("0.56.1|src/desktop/rule/windowRule/WindowRuleApplicator.cpp"),
         QStringLiteral("fec33f9a0bc279ffc94ca27a6e9843964840d6269bc28d330d566e05fed3307b")},
        {QStringLiteral("0.56.1|src/layout/target/WindowTarget.cpp"),
         QStringLiteral("8bcaf37000cf55e1d2084bf732f0589b4ab49d36c778d8d4efa98949dd14734d")},
    };
    QSet<QString> windowBehaviorSourceKeys;
    for (const auto &value : windowBehaviorSources) {
      const auto source = value.toObject();
      const auto version = source.value(QStringLiteral("version")).toString();
      const auto path = source.value(QStringLiteral("path")).toString();
      const auto key = version + QLatin1Char('|') + path;
      QVERIFY2(expectedWindowBehaviorHashes.contains(key), qPrintable(key));
      QVERIFY2(!windowBehaviorSourceKeys.contains(key), qPrintable(key));
      windowBehaviorSourceKeys.insert(key);
      QCOMPARE(source.value(QStringLiteral("tag")),
               QJsonValue(expectedGroupBehaviorTags.value(version)));
      QCOMPARE(source.value(QStringLiteral("commit")),
               QJsonValue(expectedGroupBehaviorCommits.value(version)));
      QCOMPARE(source.value(QStringLiteral("sha256")),
               QJsonValue(expectedWindowBehaviorHashes.value(key)));
    }
    QCOMPARE(windowBehaviorSourceKeys,
             QSet<QString>(expectedWindowBehaviorHashes.keyBegin(),
                           expectedWindowBehaviorHashes.keyEnd()));

    const auto groupBarSources =
        manifest.value(QStringLiteral("groupBarSources")).toArray();
    QCOMPARE(groupBarSources.size(), 10);
    const QMap<QString, QString> expectedGroupBarTags{
        {QStringLiteral("0.55.0"), QStringLiteral("v0.55.0")},
        {QStringLiteral("0.56.1"), QStringLiteral("v0.56.1")},
    };
    const QMap<QString, QString> expectedGroupBarCommits{
        {QStringLiteral("0.55.0"),
         QStringLiteral("af923e30d1d24f1f4a4f5cb8308065173c1d9539")},
        {QStringLiteral("0.56.1"),
         QStringLiteral("5c9377c15f85c50648f35ca5a213754f95b93ca0")},
    };
    const QMap<QString, QString> expectedGroupBarHashes{
        {QStringLiteral("0.55.0|src/config/values/ConfigValues.cpp"),
         QStringLiteral("290ac2deca427712edc255989010f30bf7d5c4104c05ec920042436fc07d49ce")},
        {QStringLiteral("0.55.0|src/desktop/view/Group.cpp"),
         QStringLiteral("b0f9210c858bcaf74c8f5a44f7f40abfe03af8a096e3fb158afd2346be8b0eed")},
        {QStringLiteral("0.55.0|src/render/decorations/CHyprGroupBarDecoration.cpp"),
         QStringLiteral("3d687c43d5414fb6ad617465f06bb87cda8dc6c2e791dbb0e28de72de1cf4c68")},
        {QStringLiteral("0.55.0|src/config/supplementary/propRefresher/PropRefresher.cpp"),
         QStringLiteral("6ee949391fa74f713d00b718e0e498fbbc1cc58e2931e1737c4c865fe8f6c679")},
        {QStringLiteral("0.55.0|src/config/lua/types/LuaConfigFontWeight.cpp"),
         QStringLiteral("da094c9f2e041586b9138280339dfdb24b78712b324462722e86186c8b274c05")},
        {QStringLiteral("0.56.1|src/config/values/ConfigValues.cpp"),
         QStringLiteral("a76f05079454e1f6d4402144e1673cc1cb890d285fbeb947d5afdb004ad97754")},
        {QStringLiteral("0.56.1|src/desktop/view/Group.cpp"),
         QStringLiteral("f34fec0891e69e0a1b67901e8d1b6b1a1ae8a47eccc92313c2b41660961cd385")},
        {QStringLiteral("0.56.1|src/render/decorations/CHyprGroupBarDecoration.cpp"),
         QStringLiteral("39cb87fc2b28c81433bfd34d3900e58c2c58f4f336be728e07461a8f16c095e6")},
        {QStringLiteral("0.56.1|src/config/supplementary/propRefresher/PropRefresher.cpp"),
         QStringLiteral("65f01a1a50ef05ecb1630f8fd30bbbac91029b29f6b4b6bc3f353d0bb4b2817b")},
        {QStringLiteral("0.56.1|src/config/lua/types/LuaConfigFontWeight.cpp"),
         QStringLiteral("da094c9f2e041586b9138280339dfdb24b78712b324462722e86186c8b274c05")},
    };
    QSet<QString> groupBarSourceKeys;
    for (const auto &value : groupBarSources) {
      const auto source = value.toObject();
      const auto version = source.value(QStringLiteral("version")).toString();
      const auto path = source.value(QStringLiteral("path")).toString();
      const auto key = version + QLatin1Char('|') + path;
      QVERIFY2(expectedGroupBarHashes.contains(key), qPrintable(key));
      QVERIFY2(!groupBarSourceKeys.contains(key), qPrintable(key));
      groupBarSourceKeys.insert(key);
      QCOMPARE(source.value(QStringLiteral("tag")),
               QJsonValue(expectedGroupBarTags.value(version)));
      QCOMPARE(source.value(QStringLiteral("commit")),
               QJsonValue(expectedGroupBarCommits.value(version)));
      QCOMPARE(source.value(QStringLiteral("sha256")),
               QJsonValue(expectedGroupBarHashes.value(key)));
    }
    QCOMPARE(groupBarSourceKeys,
             QSet<QString>(expectedGroupBarHashes.keyBegin(),
                           expectedGroupBarHashes.keyEnd()));

    const auto workspaceBehaviorSources =
        manifest.value(QStringLiteral("workspaceBehaviorSources")).toArray();
    QCOMPARE(workspaceBehaviorSources.size(), 13);
    const QMap<QString, QString> expectedWorkspaceBehaviorHashes{
        {QStringLiteral("0.55.0|src/config/values/ConfigValues.cpp"),
         QStringLiteral("290ac2deca427712edc255989010f30bf7d5c4104c05ec920042436fc07d49ce")},
        {QStringLiteral("0.55.0|src/config/shared/actions/ConfigActions.cpp"),
         QStringLiteral("2b2b09799b4cca634d544d4103f07b82ef4b6c377131c858d2707a5e0494fe07")},
        {QStringLiteral("0.55.0|src/desktop/history/WorkspaceHistoryTracker.cpp"),
         QStringLiteral("628b33ca5fcfdb20e21daed845732ef812eb77bc6e0dddfd6db55cb9f41ca989")},
        {QStringLiteral("0.55.0|src/desktop/history/WorkspaceHistoryTracker.hpp"),
         QStringLiteral("89afe0b8578d193b6636af422911922a39b7bfdf130bfb286d0708d4a489d009")},
        {QStringLiteral("0.55.0|src/desktop/view/Window.cpp"),
         QStringLiteral("ec00fc5ca125163eadaa267eac73032de28d9d7f32a4b03ae4498a89427b2a7e")},
        {QStringLiteral("0.55.0|src/Compositor.cpp"),
         QStringLiteral("a639c01f628e50d765856001c567aa8f344e6a620f69fca994e4436b3a090125")},
        {QStringLiteral("0.56.1|src/config/values/ConfigValues.cpp"),
         QStringLiteral("a76f05079454e1f6d4402144e1673cc1cb890d285fbeb947d5afdb004ad97754")},
        {QStringLiteral("0.56.1|src/config/shared/actions/ConfigActions.cpp"),
         QStringLiteral("29c9339ec15943d685975eb952af207fe52820c20bb15ecb0cc0b19661ac5dab")},
        {QStringLiteral("0.56.1|src/desktop/history/WorkspaceHistoryTracker.cpp"),
         QStringLiteral("33a0cf8d26540870b66d6aacb6fe7eaad0c767e8b21184016c12de5135cea678")},
        {QStringLiteral("0.56.1|src/desktop/history/WorkspaceHistoryTracker.hpp"),
         QStringLiteral("89afe0b8578d193b6636af422911922a39b7bfdf130bfb286d0708d4a489d009")},
        {QStringLiteral("0.56.1|src/desktop/view/Window.cpp"),
         QStringLiteral("4d9219c87cfba1105a30c2b742b0728808f2339c813d08b6959d00c2ce29d54f")},
        {QStringLiteral("0.56.1|src/state/WorkspacePlacementController.cpp"),
         QStringLiteral("cea25fc71ef2d54e2a4eec3e6d04fa7abfbdd25bdafbe2d742026e37dd438420")},
        {QStringLiteral("0.56.1|src/pointer/PointerController.cpp"),
         QStringLiteral("4aef766cf4205222ef143b1432d2598c7beab1527aee5e19aa58f17a1890d899")},
    };
    QSet<QString> workspaceBehaviorSourceKeys;
    for (const auto &value : workspaceBehaviorSources) {
      const auto source = value.toObject();
      const auto version = source.value(QStringLiteral("version")).toString();
      const auto path = source.value(QStringLiteral("path")).toString();
      const auto key = version + QLatin1Char('|') + path;
      QVERIFY2(expectedWorkspaceBehaviorHashes.contains(key), qPrintable(key));
      QVERIFY2(!workspaceBehaviorSourceKeys.contains(key), qPrintable(key));
      workspaceBehaviorSourceKeys.insert(key);
      QCOMPARE(source.value(QStringLiteral("tag")),
               QJsonValue(expectedGroupBehaviorTags.value(version)));
      QCOMPARE(source.value(QStringLiteral("commit")),
               QJsonValue(expectedGroupBehaviorCommits.value(version)));
      QCOMPARE(source.value(QStringLiteral("sha256")),
               QJsonValue(expectedWorkspaceBehaviorHashes.value(key)));
    }
    QCOMPARE(workspaceBehaviorSourceKeys,
             QSet<QString>(expectedWorkspaceBehaviorHashes.keyBegin(),
                           expectedWorkspaceBehaviorHashes.keyEnd()));

    const auto bindingRuntimeSources =
        manifest.value(QStringLiteral("bindingRuntimeSources")).toArray();
    QCOMPARE(bindingRuntimeSources.size(), 14);
    const QMap<QString, QString> expectedBindingRuntimeHashes{
        {QStringLiteral("0.55.0|src/config/lua/ConfigManager.cpp"),
         QStringLiteral("204bd335c3dde44eb5d528859f8e480159988ef691b166d0ff21c7c184aec642")},
        {QStringLiteral("0.55.0|src/config/lua/bindings/LuaBindingsRegistration.cpp"),
         QStringLiteral("c0e0533d07bc75b48ba469df2523b3f1335bcea17922ac0dae01aed81487e5d3")},
        {QStringLiteral("0.55.0|src/config/lua/bindings/LuaBindingsToplevel.cpp"),
         QStringLiteral("bf1f9e9bdd94a2403ecab41ceea6ffb6feecd3083715ec4e8086e84089e9a006")},
        {QStringLiteral("0.55.0|src/config/shared/actions/ConfigActions.hpp"),
         QStringLiteral("14b6d07d7720e45e099e469ed53ac2db1584e408705869b71a92a14876767b2c")},
        {QStringLiteral("0.55.0|src/config/shared/actions/ConfigActions.cpp"),
         QStringLiteral("2b2b09799b4cca634d544d4103f07b82ef4b6c377131c858d2707a5e0494fe07")},
        {QStringLiteral("0.55.0|src/managers/KeybindManager.cpp"),
         QStringLiteral("2fde298fd690c5c3b72741be1c4b11cba220996649ba560e3112a81112531491")},
        {QStringLiteral("0.55.0|src/debug/HyprCtl.cpp"),
         QStringLiteral("a88c1517da9fa2934d740dd1fd9cbca4d5d0bdd5b569a63bdba64e5e36c2d886")},
        {QStringLiteral("0.56.1|src/config/lua/ConfigManager.cpp"),
         QStringLiteral("94b5c6891326e7a3e490019d26fd9d6f5b3137fb5318a94dff0a6cb6dfdaf502")},
        {QStringLiteral("0.56.1|src/config/lua/bindings/LuaBindingsRegistration.cpp"),
         QStringLiteral("377e617ca0400722dea581e1ca740500a6057cff3842998c8f8dd0a218f77770")},
        {QStringLiteral("0.56.1|src/config/lua/bindings/LuaBindingsToplevel.cpp"),
         QStringLiteral("706b29eb52de087c1d6e64770cf85fb3ebc0f2fbe9ddcff086905499bcd332f5")},
        {QStringLiteral("0.56.1|src/config/shared/actions/ConfigActions.hpp"),
         QStringLiteral("746c6f517620eb039afbfbb9a4a0e2d5ba1c5eb117bcc8f396173f63091b7073")},
        {QStringLiteral("0.56.1|src/config/shared/actions/ConfigActions.cpp"),
         QStringLiteral("29c9339ec15943d685975eb952af207fe52820c20bb15ecb0cc0b19661ac5dab")},
        {QStringLiteral("0.56.1|src/managers/KeybindManager.cpp"),
         QStringLiteral("8d8f35fc84c4a2f8de63ac2bf6f6531c651c6fa6d37b1aac7c8fc5d9342d4e40")},
        {QStringLiteral("0.56.1|src/debug/HyprCtl.cpp"),
         QStringLiteral("7b96515a4cf13333ca71549053e76fcdd9cf815b18e4ae530dfff169af3ff1d1")},
    };
    const QList<QString> expectedBindingRuntimeOrder{
        QStringLiteral("0.55.0|src/config/lua/ConfigManager.cpp"),
        QStringLiteral("0.55.0|src/config/lua/bindings/LuaBindingsRegistration.cpp"),
        QStringLiteral("0.55.0|src/config/lua/bindings/LuaBindingsToplevel.cpp"),
        QStringLiteral("0.55.0|src/config/shared/actions/ConfigActions.hpp"),
        QStringLiteral("0.55.0|src/config/shared/actions/ConfigActions.cpp"),
        QStringLiteral("0.55.0|src/managers/KeybindManager.cpp"),
        QStringLiteral("0.55.0|src/debug/HyprCtl.cpp"),
        QStringLiteral("0.56.1|src/config/lua/ConfigManager.cpp"),
        QStringLiteral("0.56.1|src/config/lua/bindings/LuaBindingsRegistration.cpp"),
        QStringLiteral("0.56.1|src/config/lua/bindings/LuaBindingsToplevel.cpp"),
        QStringLiteral("0.56.1|src/config/shared/actions/ConfigActions.hpp"),
        QStringLiteral("0.56.1|src/config/shared/actions/ConfigActions.cpp"),
        QStringLiteral("0.56.1|src/managers/KeybindManager.cpp"),
        QStringLiteral("0.56.1|src/debug/HyprCtl.cpp"),
    };
    QSet<QString> bindingRuntimeSourceKeys;
    QList<QString> bindingRuntimeSourceOrder;
    for (const auto &value : bindingRuntimeSources) {
      const auto source = value.toObject();
      QCOMPARE(source.size(), 5);
      const auto version = source.value(QStringLiteral("version")).toString();
      const auto path = source.value(QStringLiteral("path")).toString();
      const auto key = version + QLatin1Char('|') + path;
      QVERIFY2(expectedBindingRuntimeHashes.contains(key), qPrintable(key));
      QVERIFY2(!bindingRuntimeSourceKeys.contains(key), qPrintable(key));
      bindingRuntimeSourceKeys.insert(key);
      bindingRuntimeSourceOrder.append(key);
      QCOMPARE(source.value(QStringLiteral("tag")),
               QJsonValue(expectedGroupBehaviorTags.value(version)));
      QCOMPARE(source.value(QStringLiteral("commit")),
               QJsonValue(expectedGroupBehaviorCommits.value(version)));
      QCOMPARE(source.value(QStringLiteral("sha256")),
               QJsonValue(expectedBindingRuntimeHashes.value(key)));
    }
    QCOMPARE(bindingRuntimeSourceKeys,
             QSet<QString>(expectedBindingRuntimeHashes.keyBegin(),
                           expectedBindingRuntimeHashes.keyEnd()));
    QCOMPARE(bindingRuntimeSourceOrder, expectedBindingRuntimeOrder);

    const auto miscExclusionSources =
        manifest.value(QStringLiteral("miscExclusionSources")).toArray();
    QCOMPARE(miscExclusionSources.size(), 6);
    const QMap<QString, QString> expectedMiscExclusionHashes{
        {QStringLiteral("0.55.0|src/config/values/ConfigValues.cpp"),
         QStringLiteral("290ac2deca427712edc255989010f30bf7d5c4104c05ec920042436fc07d49ce")},
        {QStringLiteral("0.55.0|src/layout/algorithm/tiled/dwindle/DwindleAlgorithm.cpp"),
         QStringLiteral("d5ec852d1b26fb55574ae184b6bfc86dd2a047f8c7d4c8c7e4748277c0e841cb")},
        {QStringLiteral("0.55.0|src/layout/target/WindowTarget.cpp"),
         QStringLiteral("33f6f6dcbfb4e140f2cce04a8e06b8042c8e780617960618b023f9a9591b9a9d")},
        {QStringLiteral("0.56.1|src/config/values/ConfigValues.cpp"),
         QStringLiteral("a76f05079454e1f6d4402144e1673cc1cb890d285fbeb947d5afdb004ad97754")},
        {QStringLiteral("0.56.1|src/layout/algorithm/tiled/dwindle/DwindleAlgorithm.cpp"),
         QStringLiteral("218c4d4ba7e7b34d1113da818347e260868451de0e5a6d699934871febc03b32")},
        {QStringLiteral("0.56.1|src/layout/target/WindowTarget.cpp"),
         QStringLiteral("8bcaf37000cf55e1d2084bf732f0589b4ab49d36c778d8d4efa98949dd14734d")},
    };
    const QList<QString> expectedMiscExclusionOrder{
        QStringLiteral("0.55.0|src/config/values/ConfigValues.cpp"),
        QStringLiteral("0.55.0|src/layout/algorithm/tiled/dwindle/DwindleAlgorithm.cpp"),
        QStringLiteral("0.55.0|src/layout/target/WindowTarget.cpp"),
        QStringLiteral("0.56.1|src/config/values/ConfigValues.cpp"),
        QStringLiteral("0.56.1|src/layout/algorithm/tiled/dwindle/DwindleAlgorithm.cpp"),
        QStringLiteral("0.56.1|src/layout/target/WindowTarget.cpp"),
    };
    QSet<QString> miscExclusionSourceKeys;
    QList<QString> miscExclusionSourceOrder;
    for (const auto &value : miscExclusionSources) {
      const auto source = value.toObject();
      QCOMPARE(source.size(), 5);
      const auto version = source.value(QStringLiteral("version")).toString();
      const auto path = source.value(QStringLiteral("path")).toString();
      const auto key = version + QLatin1Char('|') + path;
      QVERIFY2(expectedMiscExclusionHashes.contains(key), qPrintable(key));
      QVERIFY2(!miscExclusionSourceKeys.contains(key), qPrintable(key));
      miscExclusionSourceKeys.insert(key);
      miscExclusionSourceOrder.append(key);
      QCOMPARE(source.value(QStringLiteral("tag")),
               QJsonValue(expectedGroupBehaviorTags.value(version)));
      QCOMPARE(source.value(QStringLiteral("commit")),
               QJsonValue(expectedGroupBehaviorCommits.value(version)));
      QCOMPARE(source.value(QStringLiteral("sha256")),
               QJsonValue(expectedMiscExclusionHashes.value(key)));
    }
    QCOMPARE(miscExclusionSourceKeys,
             QSet<QString>(expectedMiscExclusionHashes.keyBegin(),
                           expectedMiscExclusionHashes.keyEnd()));
    QCOMPARE(miscExclusionSourceOrder, expectedMiscExclusionOrder);

    const auto inputBehaviorSources =
        manifest.value(QStringLiteral("inputBehaviorSources")).toArray();
    QCOMPARE(inputBehaviorSources.size(), 38);
    const QMap<QString, QString> expectedInputBehaviorTags{
        {QStringLiteral("0.55.0"), QStringLiteral("v0.55.0")},
        {QStringLiteral("0.56.1"), QStringLiteral("v0.56.1")},
    };
    const QMap<QString, QString> expectedInputBehaviorCommits{
        {QStringLiteral("0.55.0"),
         QStringLiteral("af923e30d1d24f1f4a4f5cb8308065173c1d9539")},
        {QStringLiteral("0.56.1"),
         QStringLiteral("5c9377c15f85c50648f35ca5a213754f95b93ca0")},
    };
    const QMap<QString, QString> expectedInputBehaviorHashes{
        {QStringLiteral("0.55.0|src/config/values/ConfigValues.cpp"),
         QStringLiteral("290ac2deca427712edc255989010f30bf7d5c4104c05ec920042436fc07d49ce")},
        {QStringLiteral("0.55.0|src/config/lua/ConfigManager.cpp"),
         QStringLiteral("204bd335c3dde44eb5d528859f8e480159988ef691b166d0ff21c7c184aec642")},
        {QStringLiteral("0.55.0|src/config/lua/bindings/LuaBindingsConfigRules.cpp"),
         QStringLiteral("2693dd89945b35c650b0bcc91d8da3441e690ca2a4f20705cc9be713e9314c94")},
        {QStringLiteral("0.55.0|src/config/supplementary/propRefresher/PropRefresher.cpp"),
         QStringLiteral("6ee949391fa74f713d00b718e0e498fbbc1cc58e2931e1737c4c865fe8f6c679")},
        {QStringLiteral("0.55.0|src/managers/input/InputManager.hpp"),
         QStringLiteral("c4479cdf4b1dc6a5f1234d86a034fb5f1f517a140afb33dc5995c65d20ac0dbf")},
        {QStringLiteral("0.55.0|src/managers/input/InputManager.cpp"),
         QStringLiteral("410836e5695062779cf525ed35b53900d199490f0db5eeba85f12fb894053835")},
        {QStringLiteral("0.55.0|src/managers/input/Tablets.cpp"),
         QStringLiteral("b875f1c65775ead92dcaae26612290e5b1728895f659f933de283086b8fdb683")},
        {QStringLiteral("0.55.0|src/protocols/PrimarySelection.cpp"),
         QStringLiteral("f4989a7770e13d351a7db362d0539e8d96d23b9f7c524d278d1e529f68e0391b")},
        {QStringLiteral("0.55.0|src/render/Renderer.cpp"),
         QStringLiteral("67febb2393cdad2671d172dbc84f230a735f579411ef0ee4f441421ef318cbf7")},
        {QStringLiteral("0.55.0|src/render/Renderer.hpp"),
         QStringLiteral("196404adaf7c2af5f9f7ad0383c95c0a89d1d5c4e560941f38ea10b960650d24")},
        {QStringLiteral("0.55.0|src/managers/input/Touch.cpp"),
         QStringLiteral("c7cc52a8da21286035b144366ad82aafd93610d4ae5827f0b4246034acd6549c")},
        {QStringLiteral("0.55.0|src/devices/Mouse.cpp"),
         QStringLiteral("aebdf0fd1765d25d943cc79f4e5bbdce09e0c437ae3e9cdc0985195df54a5470")},
        {QStringLiteral("0.55.0|src/managers/PointerManager.hpp"),
         QStringLiteral("c2a0a22603230fcaaed62afade6f0dbc8a6e8381906b8a5063e65651cad43718")},
        {QStringLiteral("0.55.0|src/managers/PointerManager.cpp"),
         QStringLiteral("3bdbb256f39b8a892f70ff6795a366068997e335c1258dc6baf3dd8dcd34fcb3")},
        {QStringLiteral("0.55.0|src/config/shared/actions/ConfigActions.cpp"),
         QStringLiteral("2b2b09799b4cca634d544d4103f07b82ef4b6c377131c858d2707a5e0494fe07")},
        {QStringLiteral("0.55.0|src/desktop/view/Window.cpp"),
         QStringLiteral("ec00fc5ca125163eadaa267eac73032de28d9d7f32a4b03ae4498a89427b2a7e")},
        {QStringLiteral("0.55.0|src/desktop/view/Window.hpp"),
         QStringLiteral("c52d94684d4dfab1fe80df565a08b560c3fd8e8ca32571436c8fa6f569f76e42")},
        {QStringLiteral("0.55.0|src/Compositor.cpp"),
         QStringLiteral("a639c01f628e50d765856001c567aa8f344e6a620f69fca994e4436b3a090125")},
        {QStringLiteral("0.55.0|src/Compositor.hpp"),
         QStringLiteral("472006be35fdf03c09566e4a371a05b8ba2e39f9a4dd13eb7313e24d2575f4f2")},
        {QStringLiteral("0.56.1|src/config/values/ConfigValues.cpp"),
         QStringLiteral("a76f05079454e1f6d4402144e1673cc1cb890d285fbeb947d5afdb004ad97754")},
        {QStringLiteral("0.56.1|src/config/lua/ConfigManager.cpp"),
         QStringLiteral("94b5c6891326e7a3e490019d26fd9d6f5b3137fb5318a94dff0a6cb6dfdaf502")},
        {QStringLiteral("0.56.1|src/config/lua/bindings/LuaBindingsConfigRules.cpp"),
         QStringLiteral("157c3e45b364f3e41b6fd7cf13fd19e7477e8c03a6a75c6c46fde5e2b5715be9")},
        {QStringLiteral("0.56.1|src/config/supplementary/propRefresher/PropRefresher.cpp"),
         QStringLiteral("65f01a1a50ef05ecb1630f8fd30bbbac91029b29f6b4b6bc3f353d0bb4b2817b")},
        {QStringLiteral("0.56.1|src/managers/input/InputManager.hpp"),
         QStringLiteral("bb8ceaf61e274bedae10555173760055e632e0ff2ac050636e06c29dffcb188d")},
        {QStringLiteral("0.56.1|src/managers/input/InputManager.cpp"),
         QStringLiteral("07de27ef0f4c9a5c3bf14f42c07af29574d428a7b02412f785f90db30b03125e")},
        {QStringLiteral("0.56.1|src/managers/input/Tablets.cpp"),
         QStringLiteral("ad276a2e23f8792ff5f2fb43944a4c3c40e7218e77f66047dd754417a6bbc90f")},
        {QStringLiteral("0.56.1|src/protocols/PrimarySelection.cpp"),
         QStringLiteral("f4989a7770e13d351a7db362d0539e8d96d23b9f7c524d278d1e529f68e0391b")},
        {QStringLiteral("0.56.1|src/render/Renderer.cpp"),
         QStringLiteral("4c3b2a4d42fbb1021a7cbb5013e826b926a9756ecde40ef5e9acdc3a9a16c4a7")},
        {QStringLiteral("0.56.1|src/render/Renderer.hpp"),
         QStringLiteral("0787f259d788e979094ebb5f3b40140c642118c634356a431e2aca17ecf4d5a2")},
        {QStringLiteral("0.56.1|src/managers/input/Touch.cpp"),
         QStringLiteral("69a5459b254751a28d2f6df1afb03781ea9b4086a43078d8275c9b165f5e3f7e")},
        {QStringLiteral("0.56.1|src/devices/Mouse.cpp"),
         QStringLiteral("aebdf0fd1765d25d943cc79f4e5bbdce09e0c437ae3e9cdc0985195df54a5470")},
        {QStringLiteral("0.56.1|src/pointer/PointerManager.hpp"),
         QStringLiteral("a18050d377c96b6bcee1d3d591b5573515473a507aaa464de611e29124bfbf88")},
        {QStringLiteral("0.56.1|src/pointer/PointerManager.cpp"),
         QStringLiteral("e7fb86bc7cd0420e0a225dfc8ca43da561638416380d398f67a200849e738981")},
        {QStringLiteral("0.56.1|src/config/shared/actions/ConfigActions.cpp"),
         QStringLiteral("29c9339ec15943d685975eb952af207fe52820c20bb15ecb0cc0b19661ac5dab")},
        {QStringLiteral("0.56.1|src/desktop/view/Window.cpp"),
         QStringLiteral("4d9219c87cfba1105a30c2b742b0728808f2339c813d08b6959d00c2ce29d54f")},
        {QStringLiteral("0.56.1|src/desktop/view/Window.hpp"),
         QStringLiteral("1f60b8ff0bc6b5a48af5c03cb2cac1108db677efc70b4424b56ef6f1f2d1cc81")},
        {QStringLiteral("0.56.1|src/pointer/PointerController.cpp"),
         QStringLiteral("4aef766cf4205222ef143b1432d2598c7beab1527aee5e19aa58f17a1890d899")},
        {QStringLiteral("0.56.1|src/pointer/PointerController.hpp"),
         QStringLiteral("260fa1c172d0f469d420dc1b7e5311c0dfec0777fa6a6cf08af69ea854fdbb55")},
    };
    const QStringList expectedInputBehaviorOrder{
        QStringLiteral("0.55.0|src/config/values/ConfigValues.cpp"),
        QStringLiteral("0.55.0|src/config/lua/ConfigManager.cpp"),
        QStringLiteral(
            "0.55.0|src/config/lua/bindings/LuaBindingsConfigRules.cpp"),
        QStringLiteral(
            "0.55.0|src/config/supplementary/propRefresher/PropRefresher.cpp"),
        QStringLiteral("0.55.0|src/managers/input/InputManager.hpp"),
        QStringLiteral("0.55.0|src/managers/input/InputManager.cpp"),
        QStringLiteral("0.55.0|src/managers/input/Tablets.cpp"),
        QStringLiteral("0.55.0|src/protocols/PrimarySelection.cpp"),
        QStringLiteral("0.55.0|src/render/Renderer.cpp"),
        QStringLiteral("0.55.0|src/render/Renderer.hpp"),
        QStringLiteral("0.55.0|src/managers/input/Touch.cpp"),
        QStringLiteral("0.55.0|src/devices/Mouse.cpp"),
        QStringLiteral("0.55.0|src/managers/PointerManager.hpp"),
        QStringLiteral("0.55.0|src/managers/PointerManager.cpp"),
        QStringLiteral("0.55.0|src/config/shared/actions/ConfigActions.cpp"),
        QStringLiteral("0.55.0|src/desktop/view/Window.cpp"),
        QStringLiteral("0.55.0|src/desktop/view/Window.hpp"),
        QStringLiteral("0.55.0|src/Compositor.cpp"),
        QStringLiteral("0.55.0|src/Compositor.hpp"),
        QStringLiteral("0.56.1|src/config/values/ConfigValues.cpp"),
        QStringLiteral("0.56.1|src/config/lua/ConfigManager.cpp"),
        QStringLiteral(
            "0.56.1|src/config/lua/bindings/LuaBindingsConfigRules.cpp"),
        QStringLiteral(
            "0.56.1|src/config/supplementary/propRefresher/PropRefresher.cpp"),
        QStringLiteral("0.56.1|src/managers/input/InputManager.hpp"),
        QStringLiteral("0.56.1|src/managers/input/InputManager.cpp"),
        QStringLiteral("0.56.1|src/managers/input/Tablets.cpp"),
        QStringLiteral("0.56.1|src/protocols/PrimarySelection.cpp"),
        QStringLiteral("0.56.1|src/render/Renderer.cpp"),
        QStringLiteral("0.56.1|src/render/Renderer.hpp"),
        QStringLiteral("0.56.1|src/managers/input/Touch.cpp"),
        QStringLiteral("0.56.1|src/devices/Mouse.cpp"),
        QStringLiteral("0.56.1|src/pointer/PointerManager.hpp"),
        QStringLiteral("0.56.1|src/pointer/PointerManager.cpp"),
        QStringLiteral("0.56.1|src/config/shared/actions/ConfigActions.cpp"),
        QStringLiteral("0.56.1|src/desktop/view/Window.cpp"),
        QStringLiteral("0.56.1|src/desktop/view/Window.hpp"),
        QStringLiteral("0.56.1|src/pointer/PointerController.cpp"),
        QStringLiteral("0.56.1|src/pointer/PointerController.hpp"),
    };
    QSet<QString> inputBehaviorSourceKeys;
    QStringList inputBehaviorSourceOrder;
    for (const auto &value : inputBehaviorSources) {
      const auto source = value.toObject();
      const auto version = source.value(QStringLiteral("version")).toString();
      const auto path = source.value(QStringLiteral("path")).toString();
      const auto key = version + QLatin1Char('|') + path;
      QVERIFY2(expectedInputBehaviorHashes.contains(key), qPrintable(key));
      QVERIFY2(!inputBehaviorSourceKeys.contains(key), qPrintable(key));
      inputBehaviorSourceKeys.insert(key);
      inputBehaviorSourceOrder.append(key);
      QCOMPARE(source.value(QStringLiteral("tag")),
               QJsonValue(expectedInputBehaviorTags.value(version)));
      QCOMPARE(source.value(QStringLiteral("commit")),
               QJsonValue(expectedInputBehaviorCommits.value(version)));
      QCOMPARE(source.value(QStringLiteral("sha256")),
               QJsonValue(expectedInputBehaviorHashes.value(key)));
    }
    QCOMPARE(inputBehaviorSourceKeys,
             QSet<QString>(expectedInputBehaviorHashes.keyBegin(),
                           expectedInputBehaviorHashes.keyEnd()));
    QCOMPARE(inputBehaviorSourceOrder, expectedInputBehaviorOrder);

    const auto inputBehaviorDependencySources =
        manifest.value(QStringLiteral("inputBehaviorDependencySources"))
            .toArray();
    QCOMPARE(inputBehaviorDependencySources.size(), 6);
    const QString hyprlandRepository =
        QStringLiteral("https://github.com/hyprwm/Hyprland");
    const QString hyprutilsRepository =
        QStringLiteral("https://github.com/hyprwm/hyprutils");
    const QStringList expectedInputBehaviorDependencyOrder{
        QStringLiteral("0.55.0|") + hyprlandRepository +
            QStringLiteral("|flake.lock"),
        QStringLiteral("0.55.0|") + hyprutilsRepository +
            QStringLiteral("|include/hyprutils/math/Vector2D.hpp"),
        QStringLiteral("0.55.0|") + hyprutilsRepository +
            QStringLiteral("|src/math/Box.cpp"),
        QStringLiteral("0.56.1|") + hyprlandRepository +
            QStringLiteral("|flake.lock"),
        QStringLiteral("0.56.1|") + hyprutilsRepository +
            QStringLiteral("|include/hyprutils/math/Vector2D.hpp"),
        QStringLiteral("0.56.1|") + hyprutilsRepository +
            QStringLiteral("|src/math/Box.cpp"),
    };
    const QMap<QString, QString> expectedInputBehaviorDependencyRevisions{
        {expectedInputBehaviorDependencyOrder.at(0),
         QStringLiteral("af923e30d1d24f1f4a4f5cb8308065173c1d9539")},
        {expectedInputBehaviorDependencyOrder.at(1),
         QStringLiteral("a2dbd8a4cc51f7cbe4224732668392bb1aa79df2")},
        {expectedInputBehaviorDependencyOrder.at(2),
         QStringLiteral("a2dbd8a4cc51f7cbe4224732668392bb1aa79df2")},
        {expectedInputBehaviorDependencyOrder.at(3),
         QStringLiteral("5c9377c15f85c50648f35ca5a213754f95b93ca0")},
        {expectedInputBehaviorDependencyOrder.at(4),
         QStringLiteral("5f03477ab3a005ff27c527486f551883535aea2f")},
        {expectedInputBehaviorDependencyOrder.at(5),
         QStringLiteral("5f03477ab3a005ff27c527486f551883535aea2f")},
    };
    const QMap<QString, QString> expectedInputBehaviorDependencyHashes{
        {expectedInputBehaviorDependencyOrder.at(0),
         QStringLiteral("0978f7573968480e977764eb309a4426018afd53e60b7636752bc05e3b77956d")},
        {expectedInputBehaviorDependencyOrder.at(1),
         QStringLiteral("26079ea62f7a4eca1e3792e7a37c2ca6d1736e3ec879dd35997d29758c9098aa")},
        {expectedInputBehaviorDependencyOrder.at(2),
         QStringLiteral("2d04de99ba977e5c3d99546606aede0d3eacc819049e16da5e0fd0d2004d083c")},
        {expectedInputBehaviorDependencyOrder.at(3),
         QStringLiteral("a44dd68728466027d72e434856186f56610c46ed0ea3826b370827a23c77b74e")},
        {expectedInputBehaviorDependencyOrder.at(4),
         QStringLiteral("26079ea62f7a4eca1e3792e7a37c2ca6d1736e3ec879dd35997d29758c9098aa")},
        {expectedInputBehaviorDependencyOrder.at(5),
         QStringLiteral("2d04de99ba977e5c3d99546606aede0d3eacc819049e16da5e0fd0d2004d083c")},
    };
    QSet<QString> inputBehaviorDependencyKeys;
    QStringList inputBehaviorDependencyOrder;
    for (const auto &value : inputBehaviorDependencySources) {
      const auto source = value.toObject();
      const auto key =
          source.value(QStringLiteral("hyprlandVersion")).toString() +
          QLatin1Char('|') +
          source.value(QStringLiteral("repository")).toString() +
          QLatin1Char('|') + source.value(QStringLiteral("path")).toString();
      QVERIFY2(expectedInputBehaviorDependencyHashes.contains(key),
               qPrintable(key));
      QVERIFY2(!inputBehaviorDependencyKeys.contains(key), qPrintable(key));
      inputBehaviorDependencyKeys.insert(key);
      inputBehaviorDependencyOrder.append(key);
      QCOMPARE(source.value(QStringLiteral("revision")),
               QJsonValue(expectedInputBehaviorDependencyRevisions.value(key)));
      QCOMPARE(source.value(QStringLiteral("sha256")),
               QJsonValue(expectedInputBehaviorDependencyHashes.value(key)));
    }
    QCOMPARE(inputBehaviorDependencyKeys,
             QSet<QString>(expectedInputBehaviorDependencyHashes.keyBegin(),
                           expectedInputBehaviorDependencyHashes.keyEnd()));
    QCOMPARE(inputBehaviorDependencyOrder,
             expectedInputBehaviorDependencyOrder);

    const auto inputDeviceSources =
        manifest.value(QStringLiteral("inputDeviceSources")).toArray();
    QCOMPARE(inputDeviceSources.size(), 34);
    const QMap<QString, QString> expectedInputDeviceHashes{
        {QStringLiteral("0.55.0|src/debug/HyprCtl.cpp"),
         QStringLiteral("a88c1517da9fa2934d740dd1fd9cbca4d5d0bdd5b569a63bdba64e5e36c2d886")},
        {QStringLiteral("0.55.0|src/helpers/MiscFunctions.cpp"),
         QStringLiteral("b11ed4a52bebcdcfd4f934b94b2a5f324bf66b2b035c5d7f119c6c027800f8aa")},
        {QStringLiteral("0.55.0|src/config/lua/ConfigManager.cpp"),
         QStringLiteral("204bd335c3dde44eb5d528859f8e480159988ef691b166d0ff21c7c184aec642")},
        {QStringLiteral("0.55.0|src/config/lua/bindings/LuaBindingsConfigRules.cpp"),
         QStringLiteral("2693dd89945b35c650b0bcc91d8da3441e690ca2a4f20705cc9be713e9314c94")},
        {QStringLiteral("0.55.0|src/config/supplementary/propRefresher/PropRefresher.cpp"),
         QStringLiteral("6ee949391fa74f713d00b718e0e498fbbc1cc58e2931e1737c4c865fe8f6c679")},
        {QStringLiteral("0.55.0|src/managers/input/InputManager.hpp"),
         QStringLiteral("c4479cdf4b1dc6a5f1234d86a034fb5f1f517a140afb33dc5995c65d20ac0dbf")},
        {QStringLiteral("0.55.0|src/managers/input/InputManager.cpp"),
         QStringLiteral("410836e5695062779cf525ed35b53900d199490f0db5eeba85f12fb894053835")},
        {QStringLiteral("0.55.0|src/managers/input/Tablets.cpp"),
         QStringLiteral("b875f1c65775ead92dcaae26612290e5b1728895f659f933de283086b8fdb683")},
        {QStringLiteral("0.55.0|src/devices/IHID.cpp"),
         QStringLiteral("a58a4b44e5947e82ab29cab414d92511f953a0feb87b662516496a869f1c7ee3")},
        {QStringLiteral("0.55.0|src/devices/IPointer.cpp"),
         QStringLiteral("ffb4ca03e3d4cdd0d7f615d27c0c01b42d360206e471bc66d50322f87ac36ccc")},
        {QStringLiteral("0.55.0|src/devices/IKeyboard.cpp"),
         QStringLiteral("f1bccbbb227ac7808f25a941601eba327a498f3556ba12514196035eac5d55cb")},
        {QStringLiteral("0.55.0|src/devices/VirtualKeyboard.cpp"),
         QStringLiteral("bc6d984a4c9e62313501c1cdb4ace3659c16d3b77e029a3c082a77f21500ed86")},
        {QStringLiteral("0.55.0|src/devices/VirtualPointer.cpp"),
         QStringLiteral("b1425a95edb425a229ffd61b266f9c02c8882a5b3dee6cd4d3b6d0e5427b516f")},
        {QStringLiteral("0.55.0|src/protocols/VirtualKeyboard.cpp"),
         QStringLiteral("43cbf7b39bf0910df6d933cef95f029fa336d5b432b0202280e6fc40d8db2f50")},
        {QStringLiteral("0.55.0|src/protocols/VirtualPointer.cpp"),
         QStringLiteral("eb6d8ea90209d6fe1899ae075465cbd8b0b6be62d301c06c493212a801e308bb")},
        {QStringLiteral("0.55.0|src/Compositor.cpp"),
         QStringLiteral("a639c01f628e50d765856001c567aa8f344e6a620f69fca994e4436b3a090125")},
        {QStringLiteral("0.55.0|src/managers/PointerManager.cpp"),
         QStringLiteral("3bdbb256f39b8a892f70ff6795a366068997e335c1258dc6baf3dd8dcd34fcb3")},
        {QStringLiteral("0.56.1|src/debug/HyprCtl.cpp"),
         QStringLiteral("7b96515a4cf13333ca71549053e76fcdd9cf815b18e4ae530dfff169af3ff1d1")},
        {QStringLiteral("0.56.1|src/helpers/MiscFunctions.cpp"),
         QStringLiteral("065418241a3b40e21273bca1fd29036221942554cddb61e08422d86f0a13c1d9")},
        {QStringLiteral("0.56.1|src/config/lua/ConfigManager.cpp"),
         QStringLiteral("94b5c6891326e7a3e490019d26fd9d6f5b3137fb5318a94dff0a6cb6dfdaf502")},
        {QStringLiteral("0.56.1|src/config/lua/bindings/LuaBindingsConfigRules.cpp"),
         QStringLiteral("157c3e45b364f3e41b6fd7cf13fd19e7477e8c03a6a75c6c46fde5e2b5715be9")},
        {QStringLiteral("0.56.1|src/config/supplementary/propRefresher/PropRefresher.cpp"),
         QStringLiteral("65f01a1a50ef05ecb1630f8fd30bbbac91029b29f6b4b6bc3f353d0bb4b2817b")},
        {QStringLiteral("0.56.1|src/managers/input/InputManager.hpp"),
         QStringLiteral("bb8ceaf61e274bedae10555173760055e632e0ff2ac050636e06c29dffcb188d")},
        {QStringLiteral("0.56.1|src/managers/input/InputManager.cpp"),
         QStringLiteral("07de27ef0f4c9a5c3bf14f42c07af29574d428a7b02412f785f90db30b03125e")},
        {QStringLiteral("0.56.1|src/managers/input/Tablets.cpp"),
         QStringLiteral("ad276a2e23f8792ff5f2fb43944a4c3c40e7218e77f66047dd754417a6bbc90f")},
        {QStringLiteral("0.56.1|src/devices/IHID.cpp"),
         QStringLiteral("a58a4b44e5947e82ab29cab414d92511f953a0feb87b662516496a869f1c7ee3")},
        {QStringLiteral("0.56.1|src/devices/IPointer.cpp"),
         QStringLiteral("ffb4ca03e3d4cdd0d7f615d27c0c01b42d360206e471bc66d50322f87ac36ccc")},
        {QStringLiteral("0.56.1|src/devices/IKeyboard.cpp"),
         QStringLiteral("8a800efe9baa7f375a3781744372eecd98a88be2c4bedaae96c90a1102a1aa3d")},
        {QStringLiteral("0.56.1|src/devices/VirtualKeyboard.cpp"),
         QStringLiteral("bc6d984a4c9e62313501c1cdb4ace3659c16d3b77e029a3c082a77f21500ed86")},
        {QStringLiteral("0.56.1|src/devices/VirtualPointer.cpp"),
         QStringLiteral("b1425a95edb425a229ffd61b266f9c02c8882a5b3dee6cd4d3b6d0e5427b516f")},
        {QStringLiteral("0.56.1|src/protocols/VirtualKeyboard.cpp"),
         QStringLiteral("43cbf7b39bf0910df6d933cef95f029fa336d5b432b0202280e6fc40d8db2f50")},
        {QStringLiteral("0.56.1|src/protocols/VirtualPointer.cpp"),
         QStringLiteral("a07b12a920fe46beab3840245b818b474a376aa736b566069014d8eb4fd82fb1")},
        {QStringLiteral("0.56.1|src/Compositor.cpp"),
         QStringLiteral("74833ecbf0e2b6f8ad84345ac0716a3295a0e347420a188be0ab4f6a684af7c0")},
        {QStringLiteral("0.56.1|src/pointer/PointerManager.cpp"),
         QStringLiteral("e7fb86bc7cd0420e0a225dfc8ca43da561638416380d398f67a200849e738981")},
    };
    QSet<QString> inputDeviceSourceKeys;
    for (const auto &value : inputDeviceSources) {
      const auto source = value.toObject();
      const auto version = source.value(QStringLiteral("version")).toString();
      const auto path = source.value(QStringLiteral("path")).toString();
      const auto key = version + QLatin1Char('|') + path;
      QVERIFY2(expectedInputDeviceHashes.contains(key), qPrintable(key));
      QVERIFY2(!inputDeviceSourceKeys.contains(key), qPrintable(key));
      inputDeviceSourceKeys.insert(key);
      QCOMPARE(source.value(QStringLiteral("tag")),
               QJsonValue(expectedInputBehaviorTags.value(version)));
      QCOMPARE(source.value(QStringLiteral("commit")),
               QJsonValue(expectedInputBehaviorCommits.value(version)));
      QCOMPARE(source.value(QStringLiteral("sha256")),
               QJsonValue(expectedInputDeviceHashes.value(key)));
    }
    QCOMPARE(inputDeviceSourceKeys,
             QSet<QString>(expectedInputDeviceHashes.keyBegin(),
                           expectedInputDeviceHashes.keyEnd()));

    const auto gestureSources =
        manifest.value(QStringLiteral("gestureSources")).toArray();
    QCOMPARE(gestureSources.size(), 38);
    const QMap<QString, QString> expectedGestureTags{
        {QStringLiteral("0.55.0"), QStringLiteral("v0.55.0")},
        {QStringLiteral("0.56.1"), QStringLiteral("v0.56.1")},
    };
    const QMap<QString, QString> expectedGestureCommits{
        {QStringLiteral("0.55.0"),
         QStringLiteral("af923e30d1d24f1f4a4f5cb8308065173c1d9539")},
        {QStringLiteral("0.56.1"),
         QStringLiteral("5c9377c15f85c50648f35ca5a213754f95b93ca0")},
    };
    const QMap<QString, QString> expectedGestureHashes{
        {QStringLiteral("0.55.0|src/config/lua/bindings/LuaBindingsConfigRules.cpp"),
         QStringLiteral("2693dd89945b35c650b0bcc91d8da3441e690ca2a4f20705cc9be713e9314c94")},
        {QStringLiteral("0.55.0|src/config/lua/ConfigManager.cpp"),
         QStringLiteral("204bd335c3dde44eb5d528859f8e480159988ef691b166d0ff21c7c184aec642")},
        {QStringLiteral("0.55.0|src/managers/input/trackpad/GestureTypes.hpp"),
         QStringLiteral("87ef4e338afd1ba8f16f2d4a9c03dfccedef3da0f5cdb4464da0c3ec903f23c1")},
        {QStringLiteral("0.55.0|src/managers/input/trackpad/TrackpadGestures.cpp"),
         QStringLiteral("5e23524d8a6a0778fc8199173978488e4b54e70611ec354b036a99ae6b9610b7")},
        {QStringLiteral("0.55.0|src/managers/input/trackpad/TrackpadGestures.hpp"),
         QStringLiteral("f0e2a20af8b5c7e41d1c7a7e3bf650b9a6ac89f883adeefca0f39ff060cdce09")},
        {QStringLiteral("0.55.0|src/managers/input/trackpad/gestures/ITrackpadGesture.cpp"),
         QStringLiteral("c2ce8e076a7bc326316d3c9ef0221e5ad7699fe87669fa9188a2fc872c1ebf5e")},
        {QStringLiteral("0.55.0|src/managers/input/trackpad/gestures/ITrackpadGesture.hpp"),
         QStringLiteral("3e89e749eeebee1050907830432221963e4ad9c2e7b76cebf784ef993e445890")},
        {QStringLiteral("0.55.0|src/managers/input/trackpad/gestures/WorkspaceSwipeGesture.cpp"),
         QStringLiteral("fc54ad123f0406bcb9b8e42b8241c4a208b377c107ad7820f0b26ada58cd3d54")},
        {QStringLiteral("0.55.0|src/managers/input/trackpad/gestures/ResizeGesture.cpp"),
         QStringLiteral("97c13e3889e744b5b3617f16620033fc0d4ce43cceeb014cdfcf7f5b85e7adc1")},
        {QStringLiteral("0.55.0|src/managers/input/trackpad/gestures/MoveGesture.cpp"),
         QStringLiteral("2fee93dc6b90c8638f00062685dd0185de07a4f3701cc3d6a78064da50fe3fe0")},
        {QStringLiteral("0.55.0|src/managers/input/trackpad/gestures/CloseGesture.cpp"),
         QStringLiteral("1963b7c441bab5a9ce9e7605f16c5d3289c927208cc11c1f3895aecd31116d85")},
        {QStringLiteral("0.55.0|src/managers/input/trackpad/gestures/ScrollMoveGesture.cpp"),
         QStringLiteral("e78caa506e078c6710338f9e2419c9af22668fbcb5ed078ac7e0256bb8906517")},
        {QStringLiteral("0.55.0|src/managers/input/trackpad/gestures/SpecialWorkspaceGesture.cpp"),
         QStringLiteral("5a8080a4c4d2f81824bf37192f450bf48deba2de37a0fe39e80e86d8af80ef42")},
        {QStringLiteral("0.55.0|src/managers/input/trackpad/gestures/FloatGesture.cpp"),
         QStringLiteral("92d3eed9c053b2da300f67aeed974ab21b59068a2dd27aeedce6a41548c1acf1")},
        {QStringLiteral("0.55.0|src/managers/input/trackpad/gestures/FloatGesture.hpp"),
         QStringLiteral("dfc7559d79b9109ed36f202483d4bcd64d1f08b4e2daef182fd44d5433b0e307")},
        {QStringLiteral("0.55.0|src/managers/input/trackpad/gestures/FullscreenGesture.cpp"),
         QStringLiteral("c3c20c47e1421e567bf9ad9376a37062b2863409f3bbd6046b3e55272b4e9417")},
        {QStringLiteral("0.55.0|src/managers/input/trackpad/gestures/FullscreenGesture.hpp"),
         QStringLiteral("97121208913016b8f423a830e046b2704c473c466e8fb0a2cca6b301e32a5f6a")},
        {QStringLiteral("0.55.0|src/managers/input/trackpad/gestures/CursorZoomGesture.cpp"),
         QStringLiteral("c8acc40765d52a0581a72536ae5489aeb43a9d275f4c7ccd8c36346e8b4f9002")},
        {QStringLiteral("0.55.0|src/managers/input/trackpad/gestures/CursorZoomGesture.hpp"),
         QStringLiteral("a070dfb875fdb80559c6278afe1ed9c6d83237c9fc58d356639c7d9c17b6d779")},
        {QStringLiteral("0.56.1|src/config/lua/bindings/LuaBindingsConfigRules.cpp"),
         QStringLiteral("157c3e45b364f3e41b6fd7cf13fd19e7477e8c03a6a75c6c46fde5e2b5715be9")},
        {QStringLiteral("0.56.1|src/config/lua/ConfigManager.cpp"),
         QStringLiteral("94b5c6891326e7a3e490019d26fd9d6f5b3137fb5318a94dff0a6cb6dfdaf502")},
        {QStringLiteral("0.56.1|src/managers/input/trackpad/GestureTypes.hpp"),
         QStringLiteral("87ef4e338afd1ba8f16f2d4a9c03dfccedef3da0f5cdb4464da0c3ec903f23c1")},
        {QStringLiteral("0.56.1|src/managers/input/trackpad/TrackpadGestures.cpp"),
         QStringLiteral("5e23524d8a6a0778fc8199173978488e4b54e70611ec354b036a99ae6b9610b7")},
        {QStringLiteral("0.56.1|src/managers/input/trackpad/TrackpadGestures.hpp"),
         QStringLiteral("f0e2a20af8b5c7e41d1c7a7e3bf650b9a6ac89f883adeefca0f39ff060cdce09")},
        {QStringLiteral("0.56.1|src/managers/input/trackpad/gestures/ITrackpadGesture.cpp"),
         QStringLiteral("c2ce8e076a7bc326316d3c9ef0221e5ad7699fe87669fa9188a2fc872c1ebf5e")},
        {QStringLiteral("0.56.1|src/managers/input/trackpad/gestures/ITrackpadGesture.hpp"),
         QStringLiteral("3e89e749eeebee1050907830432221963e4ad9c2e7b76cebf784ef993e445890")},
        {QStringLiteral("0.56.1|src/managers/input/trackpad/gestures/WorkspaceSwipeGesture.cpp"),
         QStringLiteral("2f12d2b682af060e23a8d4e0aee1ee1366cd9dce9bf05f8e287b52740cb14ad0")},
        {QStringLiteral("0.56.1|src/managers/input/trackpad/gestures/ResizeGesture.cpp"),
         QStringLiteral("9a0b156e42f0938614395a198b382a3388056b2b211c1679658f24686d9a372a")},
        {QStringLiteral("0.56.1|src/managers/input/trackpad/gestures/MoveGesture.cpp"),
         QStringLiteral("617e22260d144832639fa811e24025c52b7404955b1f7879f5d0af38e44f4f9c")},
        {QStringLiteral("0.56.1|src/managers/input/trackpad/gestures/CloseGesture.cpp"),
         QStringLiteral("9da24b306cd803a5517cee5527ae8a1f3ee3d313b09876d43434b6be7607ef32")},
        {QStringLiteral("0.56.1|src/managers/input/trackpad/gestures/ScrollMoveGesture.cpp"),
         QStringLiteral("f3c1434ac10e187102a3c5c5bfff759f04d990f0cc82fd46914a8244aac500ed")},
        {QStringLiteral("0.56.1|src/managers/input/trackpad/gestures/SpecialWorkspaceGesture.cpp"),
         QStringLiteral("10e6bdece28c998bfbf574d6f455ba53f7286b61a243d13122cb2086ab81bbae")},
        {QStringLiteral("0.56.1|src/managers/input/trackpad/gestures/FloatGesture.cpp"),
         QStringLiteral("d10abdf34e7e966777c91e225ed63038da72511e5eb73f81ee3acfd93859934e")},
        {QStringLiteral("0.56.1|src/managers/input/trackpad/gestures/FloatGesture.hpp"),
         QStringLiteral("dfc7559d79b9109ed36f202483d4bcd64d1f08b4e2daef182fd44d5433b0e307")},
        {QStringLiteral("0.56.1|src/managers/input/trackpad/gestures/FullscreenGesture.cpp"),
         QStringLiteral("b72fb2976e43899884afd9f8d9f6c6b3c6da04c9797482ef6df614e1bdc483e5")},
        {QStringLiteral("0.56.1|src/managers/input/trackpad/gestures/FullscreenGesture.hpp"),
         QStringLiteral("576dd48493e13dc26c56b7fb028084debe2080ac02f7ef644b38675f1217889e")},
        {QStringLiteral("0.56.1|src/managers/input/trackpad/gestures/CursorZoomGesture.cpp"),
         QStringLiteral("5967e293ccdc7e99b6f6201a1d2e0aeff95b1d763ce5ad3d7fb50fabd76c6753")},
        {QStringLiteral("0.56.1|src/managers/input/trackpad/gestures/CursorZoomGesture.hpp"),
         QStringLiteral("9861e8cf3c6d26632731f164da5a4a016553d4f8ea6f71d68855cc7a682a6962")},
    };
    QSet<QString> gestureSourceKeys;
    for (const auto &value : gestureSources) {
      const auto source = value.toObject();
      const auto version = source.value(QStringLiteral("version")).toString();
      const auto path = source.value(QStringLiteral("path")).toString();
      const auto key = version + QLatin1Char('|') + path;
      QVERIFY2(expectedGestureHashes.contains(key), qPrintable(key));
      QVERIFY2(!gestureSourceKeys.contains(key), qPrintable(key));
      gestureSourceKeys.insert(key);
      QCOMPARE(source.value(QStringLiteral("tag")),
               QJsonValue(expectedGestureTags.value(version)));
      QCOMPARE(source.value(QStringLiteral("commit")),
               QJsonValue(expectedGestureCommits.value(version)));
      QCOMPARE(source.value(QStringLiteral("sha256")),
               QJsonValue(expectedGestureHashes.value(key)));
    }
    QCOMPARE(gestureSourceKeys,
             QSet<QString>(expectedGestureHashes.keyBegin(),
                           expectedGestureHashes.keyEnd()));

    const auto animationSources =
        manifest.value(QStringLiteral("animationSources")).toArray();
    QCOMPARE(animationSources.size(), 16);
    const QMap<QString, QString> expectedAnimationTags{
        {QStringLiteral("0.55.0"), QStringLiteral("v0.55.0")},
        {QStringLiteral("0.56.1"), QStringLiteral("v0.56.1")},
    };
    const QMap<QString, QString> expectedAnimationCommits{
        {QStringLiteral("0.55.0"),
         QStringLiteral("af923e30d1d24f1f4a4f5cb8308065173c1d9539")},
        {QStringLiteral("0.56.1"),
         QStringLiteral("5c9377c15f85c50648f35ca5a213754f95b93ca0")},
    };
    const QMap<QString, QString> expectedAnimationHashes{
        {QStringLiteral("0.55.0|src/config/lua/bindings/LuaBindingsConfigRules.cpp"),
         QStringLiteral("2693dd89945b35c650b0bcc91d8da3441e690ca2a4f20705cc9be713e9314c94")},
        {QStringLiteral("0.55.0|src/config/lua/ConfigManager.cpp"),
         QStringLiteral("204bd335c3dde44eb5d528859f8e480159988ef691b166d0ff21c7c184aec642")},
        {QStringLiteral("0.55.0|src/config/shared/animation/AnimationTree.cpp"),
         QStringLiteral("f264fc3fe7de93237af06122600949984064e75e240d195ce07113d526edcf06")},
        {QStringLiteral("0.55.0|src/managers/animation/AnimationManager.hpp"),
         QStringLiteral("601532457f29435879665f6ecd92adfb86f9f8e16f7fe48abd068f041e2edb14")},
        {QStringLiteral("0.55.0|src/managers/animation/AnimationManager.cpp"),
         QStringLiteral("92702589150c0264d109d5ae74915a778368d471eb935ae4ae7a6de0d083f596")},
        {QStringLiteral("0.55.0|src/managers/animation/DesktopAnimationManager.cpp"),
         QStringLiteral("437b5a34fa6b462e68890679abf38f7d806ebc56f9fb6ebc814bfc40e44c3ad6")},
        {QStringLiteral("0.55.0|src/desktop/view/Window.cpp"),
         QStringLiteral("ec00fc5ca125163eadaa267eac73032de28d9d7f32a4b03ae4498a89427b2a7e")},
        {QStringLiteral("0.56.1|src/config/lua/bindings/LuaBindingsConfigRules.cpp"),
         QStringLiteral("157c3e45b364f3e41b6fd7cf13fd19e7477e8c03a6a75c6c46fde5e2b5715be9")},
        {QStringLiteral("0.56.1|src/config/lua/ConfigManager.cpp"),
         QStringLiteral("94b5c6891326e7a3e490019d26fd9d6f5b3137fb5318a94dff0a6cb6dfdaf502")},
        {QStringLiteral("0.56.1|src/config/shared/animation/AnimationTree.cpp"),
         QStringLiteral("313ed20167618dfe163fceee8753e9a49d8ef356bc3f580963e07039839e7bae")},
        {QStringLiteral("0.56.1|src/animation/AnimationManager.hpp"),
         QStringLiteral("e7834aef3b0f3259a109e5407c27bd31d5ad8ed05b698e4726e5972bacd479fa")},
        {QStringLiteral("0.56.1|src/animation/AnimationManager.cpp"),
         QStringLiteral("8c37cd0d1e972e8789468fdf063456a6a40ed17d482b898b0639a6d2a1fa7985")},
        {QStringLiteral("0.56.1|src/desktop/view/animationControllers/WindowAnimationController.cpp"),
         QStringLiteral("9a8c3c2961a1a55fca7b8783e1c3b808b424291e832bf4cdf88ed7a73e5bab08")},
        {QStringLiteral("0.56.1|src/desktop/view/animationControllers/LayerSurfaceAnimationController.cpp"),
         QStringLiteral("22b223f086b454c54f475e7f16b2d499d92cfd071f2083ea58a9f6a4412f0b1d")},
        {QStringLiteral("0.56.1|src/animation/WorkspaceAnimationController.cpp"),
         QStringLiteral("0698720a19698186197a0f1c98893b839502ad454d751d3e590f2eb0ae2b5e5b")},
        {QStringLiteral("0.56.1|src/desktop/view/Window.cpp"),
         QStringLiteral("4d9219c87cfba1105a30c2b742b0728808f2339c813d08b6959d00c2ce29d54f")},
    };
    QSet<QString> animationSourceKeys;
    for (const auto &value : animationSources) {
      const auto source = value.toObject();
      const auto version = source.value(QStringLiteral("version")).toString();
      const auto path = source.value(QStringLiteral("path")).toString();
      const auto key = version + QLatin1Char('|') + path;
      QVERIFY2(expectedAnimationHashes.contains(key), qPrintable(key));
      QVERIFY2(!animationSourceKeys.contains(key), qPrintable(key));
      animationSourceKeys.insert(key);
      QCOMPARE(source.value(QStringLiteral("tag")),
               QJsonValue(expectedAnimationTags.value(version)));
      QCOMPARE(source.value(QStringLiteral("commit")),
               QJsonValue(expectedAnimationCommits.value(version)));
      QCOMPARE(source.value(QStringLiteral("sha256")),
               QJsonValue(expectedAnimationHashes.value(key)));
    }
    QCOMPARE(animationSourceKeys,
             QSet<QString>(expectedAnimationHashes.keyBegin(),
                           expectedAnimationHashes.keyEnd()));

    const auto animationDependencySources =
        manifest.value(QStringLiteral("animationDependencySources")).toArray();
    QCOMPARE(animationDependencySources.size(), 8);
    const QMap<QString, QString> expectedAnimationDependencyRepositories{
        {QStringLiteral("0.55.0|flake.lock"),
         QStringLiteral("https://github.com/hyprwm/Hyprland")},
        {QStringLiteral("0.55.0|include/hyprutils/animation/AnimationManager.hpp"),
         QStringLiteral("https://github.com/hyprwm/hyprutils")},
        {QStringLiteral("0.55.0|src/animation/AnimationManager.cpp"),
         QStringLiteral("https://github.com/hyprwm/hyprutils")},
        {QStringLiteral("0.55.0|src/animation/AnimatedVariable.cpp"),
         QStringLiteral("https://github.com/hyprwm/hyprutils")},
        {QStringLiteral("0.56.1|flake.lock"),
         QStringLiteral("https://github.com/hyprwm/Hyprland")},
        {QStringLiteral("0.56.1|include/hyprutils/animation/AnimationManager.hpp"),
         QStringLiteral("https://github.com/hyprwm/hyprutils")},
        {QStringLiteral("0.56.1|src/animation/AnimationManager.cpp"),
         QStringLiteral("https://github.com/hyprwm/hyprutils")},
        {QStringLiteral("0.56.1|src/animation/AnimatedVariable.cpp"),
         QStringLiteral("https://github.com/hyprwm/hyprutils")},
    };
    const QMap<QString, QString> expectedAnimationDependencyRevisions{
        {QStringLiteral("0.55.0|flake.lock"),
         QStringLiteral("af923e30d1d24f1f4a4f5cb8308065173c1d9539")},
        {QStringLiteral("0.55.0|include/hyprutils/animation/AnimationManager.hpp"),
         QStringLiteral("a2dbd8a4cc51f7cbe4224732668392bb1aa79df2")},
        {QStringLiteral("0.55.0|src/animation/AnimationManager.cpp"),
         QStringLiteral("a2dbd8a4cc51f7cbe4224732668392bb1aa79df2")},
        {QStringLiteral("0.55.0|src/animation/AnimatedVariable.cpp"),
         QStringLiteral("a2dbd8a4cc51f7cbe4224732668392bb1aa79df2")},
        {QStringLiteral("0.56.1|flake.lock"),
         QStringLiteral("5c9377c15f85c50648f35ca5a213754f95b93ca0")},
        {QStringLiteral("0.56.1|include/hyprutils/animation/AnimationManager.hpp"),
         QStringLiteral("5f03477ab3a005ff27c527486f551883535aea2f")},
        {QStringLiteral("0.56.1|src/animation/AnimationManager.cpp"),
         QStringLiteral("5f03477ab3a005ff27c527486f551883535aea2f")},
        {QStringLiteral("0.56.1|src/animation/AnimatedVariable.cpp"),
         QStringLiteral("5f03477ab3a005ff27c527486f551883535aea2f")},
    };
    const QMap<QString, QString> expectedAnimationDependencyHashes{
        {QStringLiteral("0.55.0|flake.lock"),
         QStringLiteral("0978f7573968480e977764eb309a4426018afd53e60b7636752bc05e3b77956d")},
        {QStringLiteral("0.55.0|include/hyprutils/animation/AnimationManager.hpp"),
         QStringLiteral("472747d817e167041c51bb7f4853aca3895450ac2b38464fc60a42acecb9e3c4")},
        {QStringLiteral("0.55.0|src/animation/AnimationManager.cpp"),
         QStringLiteral("cd46df7bd7f8bfb193ace37b32370c99056e9c633cb88793045c32d6d4cdb097")},
        {QStringLiteral("0.55.0|src/animation/AnimatedVariable.cpp"),
         QStringLiteral("e7e6184fa21c03be8bc4248553fed06e588f6600f8969795f4dfb595f09e7622")},
        {QStringLiteral("0.56.1|flake.lock"),
         QStringLiteral("a44dd68728466027d72e434856186f56610c46ed0ea3826b370827a23c77b74e")},
        {QStringLiteral("0.56.1|include/hyprutils/animation/AnimationManager.hpp"),
         QStringLiteral("472747d817e167041c51bb7f4853aca3895450ac2b38464fc60a42acecb9e3c4")},
        {QStringLiteral("0.56.1|src/animation/AnimationManager.cpp"),
         QStringLiteral("cd46df7bd7f8bfb193ace37b32370c99056e9c633cb88793045c32d6d4cdb097")},
        {QStringLiteral("0.56.1|src/animation/AnimatedVariable.cpp"),
         QStringLiteral("e9aa6712d9987e9415a0644a7e9521fd3813992f32043650ffdbb7f008d7c16a")},
    };
    QSet<QString> animationDependencyKeys;
    for (const auto &value : animationDependencySources) {
      const auto source = value.toObject();
      const auto key =
          source.value(QStringLiteral("hyprlandVersion")).toString()
          + QLatin1Char('|')
          + source.value(QStringLiteral("path")).toString();
      QVERIFY2(expectedAnimationDependencyHashes.contains(key), qPrintable(key));
      QVERIFY2(!animationDependencyKeys.contains(key), qPrintable(key));
      animationDependencyKeys.insert(key);
      QCOMPARE(source.value(QStringLiteral("repository")),
               QJsonValue(expectedAnimationDependencyRepositories.value(key)));
      QCOMPARE(source.value(QStringLiteral("revision")),
               QJsonValue(expectedAnimationDependencyRevisions.value(key)));
      QCOMPARE(source.value(QStringLiteral("sha256")),
               QJsonValue(expectedAnimationDependencyHashes.value(key)));
    }
    QCOMPARE(animationDependencyKeys,
             QSet<QString>(expectedAnimationDependencyHashes.keyBegin(),
                           expectedAnimationDependencyHashes.keyEnd()));

    const auto complexSources =
        manifest.value(QStringLiteral("complexSources")).toArray();
    QCOMPARE(complexSources.size(), 48);
    QSet<QString> complexPaths;
    for (const auto &value : complexSources) {
      const auto source = value.toObject();
      QCOMPARE(source.value(QStringLiteral("version")),
               QJsonValue(QStringLiteral("0.56.1")));
      QCOMPARE(source.value(QStringLiteral("tag")),
               QJsonValue(QStringLiteral("v0.56.1")));
      QCOMPARE(source.value(QStringLiteral("commit")),
               QJsonValue(
                   QStringLiteral("5c9377c15f85c50648f35ca5a213754f95b93ca0")));
      const auto path = source.value(QStringLiteral("path")).toString();
      QVERIFY2(!path.isEmpty(), "complex source path is empty");
      QVERIFY2(!complexPaths.contains(path), qPrintable(path));
      complexPaths.insert(path);
      QVERIFY(QRegularExpression(QStringLiteral("^[0-9a-f]{64}$"))
                  .match(source.value(QStringLiteral("sha256")).toString())
                  .hasMatch());
    }
    QCOMPARE(
        complexPaths,
        stringSet({
            QStringLiteral("src/Compositor.cpp"),
            QStringLiteral("src/animation/AnimationManager.cpp"),
            QStringLiteral("src/animation/WorkspaceAnimationController.cpp"),
            QStringLiteral("src/config/lua/ConfigManager.cpp"),
            QStringLiteral(
                "src/config/lua/bindings/LuaBindingsConfigRules.cpp"),
            QStringLiteral(
                "src/config/lua/bindings/LuaBindingsDispatchers.cpp"),
            QStringLiteral("src/config/lua/bindings/LuaBindingsInternal.cpp"),
            QStringLiteral("src/config/lua/bindings/LuaBindingsInternal.hpp"),
            QStringLiteral("src/config/lua/bindings/LuaBindingsToplevel.cpp"),
            QStringLiteral("src/config/shared/Types.hpp"),
            QStringLiteral("src/config/shared/actions/ConfigActions.cpp"),
            QStringLiteral("src/config/shared/animation/AnimationTree.cpp"),
            QStringLiteral("src/config/shared/monitor/Parser.cpp"),
            QStringLiteral("src/desktop/rule/Rule.cpp"),
            QStringLiteral("src/desktop/rule/windowRule/WindowRule.cpp"),
            QStringLiteral("src/desktop/rule/windowRule/WindowRule.hpp"),
            QStringLiteral(
                "src/desktop/rule/windowRule/WindowRuleApplicator.cpp"),
            QStringLiteral("src/desktop/state/ViewQuery.cpp"),
            QStringLiteral("src/desktop/state/ViewQuery.hpp"),
            QStringLiteral("src/desktop/types/OverridableVar.hpp"),
            QStringLiteral("src/desktop/view/Window.cpp"),
            QStringLiteral(
                "src/desktop/view/animationControllers/LayerSurfaceAnimationController.cpp"),
            QStringLiteral(
                "src/desktop/view/animationControllers/WindowAnimationController.cpp"),
            QStringLiteral("src/devices/IPointer.hpp"),
            QStringLiteral("src/helpers/CMType.cpp"),
            QStringLiteral("src/helpers/MiscFunctions.cpp"),
            QStringLiteral("src/helpers/MiscFunctions.hpp"),
            QStringLiteral("src/helpers/TransferFunction.cpp"),
            QStringLiteral("src/main.cpp"),
            QStringLiteral("src/managers/KeybindManager.cpp"),
            QStringLiteral(
                "src/managers/fullscreen/FullscreenController.hpp"),
            QStringLiteral("src/managers/input/InputManager.cpp"),
            QStringLiteral("src/managers/input/trackpad/TrackpadGestures.cpp"),
            QStringLiteral(
                "src/managers/input/trackpad/gestures/CursorZoomGesture.cpp"),
            QStringLiteral(
                "src/managers/input/trackpad/gestures/CursorZoomGesture.hpp"),
            QStringLiteral(
                "src/managers/input/trackpad/gestures/FloatGesture.cpp"),
            QStringLiteral(
                "src/managers/input/trackpad/gestures/FloatGesture.hpp"),
            QStringLiteral(
                "src/managers/input/trackpad/gestures/FullscreenGesture.cpp"),
            QStringLiteral(
                "src/managers/input/trackpad/gestures/FullscreenGesture.hpp"),
            QStringLiteral(
                "src/managers/input/trackpad/gestures/SpecialWorkspaceGesture.cpp"),
            QStringLiteral(
                "src/managers/input/trackpad/gestures/SpecialWorkspaceGesture.hpp"),
            QStringLiteral("src/protocols/types/ContentType.cpp"),
            QStringLiteral("src/protocols/types/ContentType.hpp"),
            QStringLiteral(
                "src/render/decorations/CHyprGroupBarDecoration.cpp"),
            QStringLiteral("src/state/MonitorQueryCore.cpp"),
            QStringLiteral("src/state/MonitorQueryCore.hpp"),
            QStringLiteral("src/state/WorkspaceQueryCore.cpp"),
            QStringLiteral("src/state/WorkspaceQueryCore.hpp"),
        }));

    const auto catalogObject =
        readObject(QStringLiteral(HYPRSHELLD_HYPRLAND_CATALOG_FILE));
    const auto release =
        catalogObject.value(QStringLiteral("hyprland")).toObject();
    const auto reviewedSource =
        std::ranges::find_if(sources, [](const QJsonValue &value) {
          return value.toObject().value(QStringLiteral("version")) ==
                 QJsonValue(QStringLiteral("0.56.1"));
        });
    QVERIFY(reviewedSource != sources.end());
    QCOMPARE(release.value(QStringLiteral("repository")),
             manifest.value(QStringLiteral("repository")));
    QCOMPARE(release.value(QStringLiteral("reviewedTag")),
             reviewedSource->toObject().value(QStringLiteral("tag")));
    QCOMPARE(release.value(QStringLiteral("reviewedCommit")),
             reviewedSource->toObject().value(QStringLiteral("commit")));
  }

  void schemasAndDefaultSnapshotDescribeTheSameClosedWorld() {
    const auto catalogSchema =
        readObject(QStringLiteral(HYPRSHELLD_HYPRLAND_CATALOG_SCHEMA_FILE));
    const auto actionCatalogSchema = readObject(
        QStringLiteral(HYPRSHELLD_HYPRLAND_ACTION_CATALOG_SCHEMA_FILE));
    const auto configSchema =
        readObject(QStringLiteral(HYPRSHELLD_HYPRLAND_CONFIG_SCHEMA_FILE));
    const auto generationSchema = readObject(
        QStringLiteral(HYPRSHELLD_HYPRLAND_GENERATION_MANIFEST_SCHEMA_FILE));
    const auto sourceManifestSchema = readObject(
        QStringLiteral(HYPRSHELLD_HYPRLAND_SOURCE_MANIFEST_SCHEMA_FILE));
    QVERIFY(!catalogSchema.isEmpty());
    QVERIFY(!actionCatalogSchema.isEmpty());
    QVERIFY(!configSchema.isEmpty());
    QVERIFY(!generationSchema.isEmpty());
    QVERIFY(!sourceManifestSchema.isEmpty());
    for (const auto &schema : {
             catalogSchema,
             actionCatalogSchema,
             configSchema,
             generationSchema,
             sourceManifestSchema,
         }) {
      QCOMPARE(schema.value(QStringLiteral("$schema")).toString(),
               QStringLiteral("https://json-schema.org/draft/2020-12/schema"));
      QCOMPARE(schema.value(QStringLiteral("type")).toString(),
               QStringLiteral("object"));
      QCOMPARE(schema.value(QStringLiteral("additionalProperties")),
               QJsonValue(false));
    }

    const auto defaultObject = defaultStateObject();
    const auto required =
        jsonStringSet(configSchema.value(QStringLiteral("required")).toArray());
    QCOMPARE(required, stringSet(defaultObject.keys()));
    QCOMPARE(
        stringSet(
            configSchema.value(QStringLiteral("properties")).toObject().keys()),
        required);

    const auto catalogProperties =
        catalogSchema.value(QStringLiteral("properties")).toObject();
    QCOMPARE(catalogProperties.value(QStringLiteral("options"))
                 .toObject()
                 .value(QStringLiteral("maxItems"))
                 .toInteger(),
             qint64(maximumCatalogOptions));
    QCOMPARE(catalogProperties.value(QStringLiteral("complexSurfaces"))
                 .toObject()
                 .value(QStringLiteral("maxItems"))
                 .toInteger(),
             qint64(12));
    QVERIFY(qint64(12) <= qint64(maximumComplexSurfaces));

    const auto configProperties =
        configSchema.value(QStringLiteral("properties")).toObject();
    QCOMPARE(configProperties.value(QStringLiteral("overrides"))
                 .toObject()
                 .value(QStringLiteral("maxProperties"))
                 .toInteger(),
             qint64(maximumOverrides));
    const QMap<QString, qint64> surfaceLimits{
        {QStringLiteral("monitors"), maximumMonitors},
        {QStringLiteral("devices"), maximumDevices},
        {QStringLiteral("curves"), maximumCurves},
        {QStringLiteral("animations"), maximumAnimations},
        {QStringLiteral("gestures"), maximumGestures},
        {QStringLiteral("workspaceRules"), maximumWorkspaceRules},
        {QStringLiteral("windowRules"), maximumWindowRules},
        {QStringLiteral("layerRules"), maximumLayerRules},
        {QStringLiteral("submaps"), maximumSubmaps},
        {QStringLiteral("bindings"), maximumBindings},
        {QStringLiteral("permissions"), maximumPermissions},
        {QStringLiteral("environment"), maximumEnvironmentVariables},
    };
    for (auto iterator = surfaceLimits.constBegin();
         iterator != surfaceLimits.constEnd(); ++iterator) {
      QCOMPARE(configProperties.value(iterator.key())
                   .toObject()
                   .value(QStringLiteral("maxItems"))
                   .toInteger(),
               iterator.value());
    }
    const auto configDefinitions =
        configSchema.value(QStringLiteral("$defs")).toObject();
    const auto bindingOptions =
        configDefinitions.value(QStringLiteral("bindingOptions")).toObject();
    const auto optionProperties = stringSet(
        bindingOptions.value(QStringLiteral("properties")).toObject().keys());
    const auto requiredOptions = jsonStringSet(
        bindingOptions.value(QStringLiteral("required")).toArray());
    QVERIFY(!optionProperties.contains(QStringLiteral("mouse")));
    QVERIFY(!requiredOptions.contains(QStringLiteral("mouse")));
    QCOMPARE(optionProperties - QSet<QString>{QStringLiteral("device")},
             requiredOptions);

    const auto actionProperties =
        actionCatalogSchema.value(QStringLiteral("properties")).toObject();
    QCOMPARE(actionProperties.value(QStringLiteral("dispatcherActions"))
                 .toObject()
                 .value(QStringLiteral("minItems"))
                 .toInteger(),
             qint64(47));
    QCOMPARE(actionProperties.value(QStringLiteral("dispatcherActions"))
                 .toObject()
                 .value(QStringLiteral("maxItems"))
                 .toInteger(),
             qint64(47));
    QCOMPARE(actionProperties.value(QStringLiteral("gestureActions"))
                 .toObject()
                 .value(QStringLiteral("minItems"))
                 .toInteger(),
             qint64(10));
    for (const auto &[key, count] : std::array{
             std::pair{QStringLiteral("semanticActions"), qint64(29)},
             std::pair{QStringLiteral("gestureActions"), qint64(10)},
             std::pair{QStringLiteral("excluded"), qint64(9)},
         }) {
      const auto definition = actionProperties.value(key).toObject();
      QCOMPARE(definition.value(QStringLiteral("minItems")).toInteger(), count);
      QCOMPARE(definition.value(QStringLiteral("maxItems")).toInteger(), count);
    }

    const auto generationRequired = jsonStringSet(
        generationSchema.value(QStringLiteral("required")).toArray());
    QVERIFY(generationRequired.contains(QStringLiteral("actionCatalogDigest")));
    QVERIFY(generationSchema.value(QStringLiteral("properties"))
                .toObject()
                .contains(QStringLiteral("actionCatalogDigest")));
  }

  void everyShippedContractDocumentUsesStrictJson() {
    for (const auto &path : {
             QStringLiteral(HYPRSHELLD_HYPRLAND_CATALOG_FILE),
             QStringLiteral(HYPRSHELLD_HYPRLAND_ACTION_CATALOG_FILE),
             QStringLiteral(HYPRSHELLD_HYPRLAND_DEFAULTS_FILE),
             QStringLiteral(HYPRSHELLD_HYPRLAND_CATALOG_SCHEMA_FILE),
             QStringLiteral(HYPRSHELLD_HYPRLAND_ACTION_CATALOG_SCHEMA_FILE),
             QStringLiteral(HYPRSHELLD_HYPRLAND_CONFIG_SCHEMA_FILE),
             QStringLiteral(
                 HYPRSHELLD_HYPRLAND_GENERATION_MANIFEST_SCHEMA_FILE),
             QStringLiteral(
                 HYPRSHELLD_HYPRLAND_SOURCE_MANIFEST_SCHEMA_FILE),
             QStringLiteral(HYPRSHELLD_HYPRLAND_056_FIXTURE_FILE),
             QStringLiteral(HYPRSHELLD_HYPRLAND_055_FIXTURE_FILE),
             QStringLiteral(HYPRSHELLD_HYPRLAND_DELTA_FIXTURE_FILE),
             QStringLiteral(HYPRSHELLD_HYPRLAND_SOURCE_MANIFEST_FILE),
         }) {
      const auto bytes = readBytes(path);
      QVERIFY2(!bytes.isEmpty(), qPrintable(path));
      const auto parsed =
          JsonSupport::parseStrictObject(bytes, 4 * 1024 * 1024, 128);
      QVERIFY2(parsed,
               qPrintable(parsed.errors.isEmpty()
                              ? path
                              : path + QStringLiteral(": ") +
                                    parsed.errors.constFirst().message));
    }
  }

  void complexSurfaceCatalogReferencesClosedRecordSchemas() {
    const auto parsed = shippedCatalog();
    QVERIFY(parsed);
    QCOMPARE(parsed.value->complexSurfaces.size(), 12);
    const auto configSchema =
        readObject(QStringLiteral(HYPRSHELLD_HYPRLAND_CONFIG_SCHEMA_FILE));
    const auto definitions =
        configSchema.value(QStringLiteral("$defs")).toObject();
    const auto properties =
        configSchema.value(QStringLiteral("properties")).toObject();
    QSet<QString> seenIds;
    QSet<QString> luaDestinations;
    for (const auto &surface : parsed.value->complexSurfaces) {
      QVERIFY(surface.ordered);
      QCOMPARE(surface.identityField, QStringLiteral("id"));
      QVERIFY2(!surface.module.isEmpty(), qPrintable(surface.id));
      QVERIFY2(!surface.luaPath.isEmpty(), qPrintable(surface.id));
      for (const auto &segment : surface.luaPath) {
        QVERIFY2(!segment.isEmpty(), qPrintable(surface.id));
      }
      const auto destination = surface.module + QLatin1Char(':') +
                               surface.luaPath.join(QLatin1Char('.'));
      QVERIFY2(!luaDestinations.contains(destination), qPrintable(destination));
      luaDestinations.insert(destination);
      QVERIFY(!seenIds.contains(surface.id));
      seenIds.insert(surface.id);
      QVERIFY(properties.contains(surface.id));
      const auto expectedReference = QStringLiteral("#/$defs/") + surface.kind;
      QCOMPARE(properties.value(surface.id)
                   .toObject()
                   .value(QStringLiteral("items"))
                   .toObject()
                   .value(QStringLiteral("$ref"))
                   .toString(),
               expectedReference);
      QVERIFY(definitions.contains(surface.kind));
      const auto recordSchema = definitions.value(surface.kind).toObject();
      if (recordSchema.contains(QStringLiteral("oneOf"))) {
        const auto alternatives =
            recordSchema.value(QStringLiteral("oneOf")).toArray();
        if (surface.kind == QStringLiteral("workspaceRule")) {
          QCOMPARE(alternatives.size(), 2);
          const auto ordinary = alternatives.at(0).toObject();
          const auto protectedRule = alternatives.at(1).toObject();
          QCOMPARE(ordinary.value(QStringLiteral("type")).toString(),
                   QStringLiteral("object"));
          QCOMPARE(ordinary.value(QStringLiteral("additionalProperties")),
                   QJsonValue(false));
          QCOMPARE(protectedRule.value(QStringLiteral("type")).toString(),
                   QStringLiteral("object"));
          QCOMPARE(protectedRule.value(QStringLiteral("additionalProperties")),
                   QJsonValue(false));
          const auto ordinaryProperties =
              ordinary.value(QStringLiteral("properties")).toObject();
          QCOMPARE(ordinaryProperties.value(QStringLiteral("id")).toObject()
                       .value(QStringLiteral("$ref")).toString(),
                   QStringLiteral("#/$defs/userWorkspaceRuleId"));
          QCOMPARE(ordinaryProperties.value(QStringLiteral("selector")).toObject()
                       .value(QStringLiteral("$ref")).toString(),
                   QStringLiteral("#/$defs/workspaceSelector"));
          QCOMPARE(ordinaryProperties.value(QStringLiteral("overrides")).toObject()
                       .value(QStringLiteral("$ref")).toString(),
                   QStringLiteral("#/$defs/workspaceOverrides"));
          const auto protectedProperties =
              protectedRule.value(QStringLiteral("properties")).toObject();
          QCOMPARE(protectedProperties.value(QStringLiteral("id")).toObject()
                       .value(QStringLiteral("const")).toString(),
                   QStringLiteral(
                       "hyprshelld.internal.shared-spacing.maximized"
                   ));
          QCOMPARE(protectedProperties.value(QStringLiteral("selector")).toObject()
                       .value(QStringLiteral("const")).toString(),
                   QStringLiteral("f[1]"));
          QCOMPARE(protectedProperties.value(QStringLiteral("enabled")).toObject()
                       .value(QStringLiteral("const")),
                   QJsonValue(true));
          const QJsonObject exactProtectedOverrides{{
              QStringLiteral("gaps_out"), QJsonArray{0, 0, 0, 0}
          }};
          QCOMPARE(protectedProperties.value(QStringLiteral("overrides")).toObject()
                       .value(QStringLiteral("const")).toObject(),
                   exactProtectedOverrides);
        } else {
          QVERIFY(!alternatives.isEmpty());
          for (const auto &alternative : alternatives) {
            const auto reference = alternative.toObject()
                                       .value(QStringLiteral("$ref")).toString();
            const auto prefix = QStringLiteral("#/$defs/");
            QVERIFY2(reference.startsWith(prefix), qPrintable(reference));
            const auto resolved = definitions.value(
                reference.sliced(prefix.size())
            ).toObject();
            QCOMPARE(resolved.value(QStringLiteral("additionalProperties")),
                     QJsonValue(false));
          }
        }
      } else {
        QCOMPARE(recordSchema.value(QStringLiteral("additionalProperties")),
                 QJsonValue(false));
      }
    }
    QCOMPARE(seenIds, stringSet({
                          QStringLiteral("animations"),
                          QStringLiteral("bindings"),
                          QStringLiteral("curves"),
                          QStringLiteral("devices"),
                          QStringLiteral("environment"),
                          QStringLiteral("gestures"),
                          QStringLiteral("layerRules"),
                          QStringLiteral("monitors"),
                          QStringLiteral("permissions"),
                          QStringLiteral("submaps"),
                          QStringLiteral("windowRules"),
                          QStringLiteral("workspaceRules"),
                      }));

    const auto deviceSurface = std::ranges::find_if(
        parsed.value->complexSurfaces,
        [](const ComplexSurfaceDefinition &surface) {
          return surface.id == QStringLiteral("devices");
        }
    );
    QVERIFY(deviceSurface != parsed.value->complexSurfaces.cend());
    QVERIFY(deviceSurface->applyMode == ApplyMode::Restart);
    QVERIFY(deviceSurface->risk == RiskLevel::Caution);
    QCOMPARE(
        deviceSurface->description,
        QStringLiteral(
            "Compatibility-preserved per-device input overrides selected by "
            "a session-assigned Hyprland name; generic changes require "
            "restart until a dedicated transaction can prove them."
        )
    );

    const auto bindingSurface = std::ranges::find_if(
        parsed.value->complexSurfaces,
        [](const ComplexSurfaceDefinition &surface) {
          return surface.id == QStringLiteral("bindings");
        }
    );
    QVERIFY(bindingSurface != parsed.value->complexSurfaces.cend());
    QVERIFY(bindingSurface->applyMode == ApplyMode::Restart);
    QVERIFY(bindingSurface->risk == RiskLevel::Caution);
    QCOMPARE(
        bindingSurface->description,
        QStringLiteral(
            "Compatibility-preserved normalized key chords mapped to closed "
            "semantic actions; duplicate chords are rejected, and generic "
            "changes require restart until a dedicated receipt-bound shortcut "
            "transaction can prove them."
        )
    );

    const auto submapSurface = std::ranges::find_if(
        parsed.value->complexSurfaces,
        [](const ComplexSurfaceDefinition &surface) {
          return surface.id == QStringLiteral("submaps");
        }
    );
    QVERIFY(submapSurface != parsed.value->complexSurfaces.cend());
    QVERIFY(submapSurface->applyMode == ApplyMode::Restart);
    QVERIFY(submapSurface->risk == RiskLevel::Caution);
    QCOMPARE(
        submapSurface->description,
        QStringLiteral(
            "Compatibility-preserved named binding submaps with an explicit "
            "reset target; bindings reference the submap name, and generic "
            "changes require restart until a dedicated receipt-bound shortcut "
            "transaction can prove them."
        )
    );
  }

  void classifiesRuntimeVersionsFailClosed() {
    const auto parsed = shippedCatalog();
    QVERIFY(parsed);
    QCOMPARE(compatibilityForVersion(*parsed.value, {0, 56, 1}),
             CompatibilityDecision::Exact);
    QCOMPARE(compatibilityForVersion(*parsed.value, {0, 56, 99}),
             CompatibilityDecision::SupportedMinor);
    QCOMPARE(compatibilityForVersion(*parsed.value, {0, 55, 4}),
             CompatibilityDecision::UnsupportedOlder);
    QCOMPARE(compatibilityForVersion(*parsed.value, {0, 57, 0}),
             CompatibilityDecision::UnsupportedFuture);
    QCOMPARE(compatibilityForVersion(*parsed.value, {1, 0, 0}),
             CompatibilityDecision::UnsupportedMajor);
  }

  void semanticVersionsAreStrict_data() {
    QTest::addColumn<QString>("text");
    QTest::addColumn<bool>("valid");
    QTest::newRow("release") << QStringLiteral("0.56.1") << true;
    QTest::newRow("zero") << QStringLiteral("0.0.0") << true;
    QTest::newRow("missing-patch") << QStringLiteral("0.56") << false;
    QTest::newRow("leading-zero") << QStringLiteral("0.056.1") << false;
    QTest::newRow("prefix") << QStringLiteral("v0.56.1") << false;
    QTest::newRow("suffix") << QStringLiteral("0.56.1-git") << false;
    QTest::newRow("padded") << QStringLiteral(" 0.56.1") << false;
    QTest::newRow("negative") << QStringLiteral("0.56.-1") << false;
    QTest::newRow("overflow") << QStringLiteral("0.4294967296.0") << false;
  }

  void semanticVersionsAreStrict() {
    QFETCH(QString, text);
    QFETCH(bool, valid);
    QCOMPARE(semanticVersionFromString(text).has_value(), valid);
  }

  void catalogRejectsUnknownAndDuplicateFields() {
    const auto original =
        readBytes(QStringLiteral(HYPRSHELLD_HYPRLAND_CATALOG_FILE));
    QVERIFY(!original.isEmpty());

    auto duplicate = original;
    const auto needle = QByteArrayLiteral("\"contractVersion\": 1");
    QVERIFY(duplicate.contains(needle));
    duplicate.replace(
        needle, QByteArrayLiteral(
                    "\"contractVersion\": 1, \"contract\\u0056ersion\": 1"));
    const auto duplicateResult = parseCatalog(duplicate);
    QVERIFY(!duplicateResult);
    QVERIFY(
        hasCode(duplicateResult.errors, QStringLiteral("json.duplicate-key")));

    auto object = QJsonDocument::fromJson(original).object();
    object.insert(QStringLiteral("futureField"), true);
    const auto unknown = parseCatalog(encode(object));
    QVERIFY(!unknown);
    QVERIFY(hasCode(unknown.errors, QStringLiteral("catalog.unknown-field")));

    for (const auto &value : {QJsonValue(QJsonValue::Undefined),
                              QJsonValue(QStringLiteral("true"))}) {
      auto malformed = QJsonDocument::fromJson(original).object();
      auto options = malformed.value(QStringLiteral("options")).toArray();
      auto option = options.first().toObject();
      if (value.isUndefined()) {
        option.remove(QStringLiteral("writable"));
      } else {
        option.insert(QStringLiteral("writable"), value);
      }
      options.replace(0, option);
      malformed.insert(QStringLiteral("options"), options);
      const auto parsed = parseCatalog(encode(malformed));
      QVERIFY(!parsed);
      QVERIFY(hasCode(parsed.errors,
                      QStringLiteral("catalog.boolean-required")));
    }
  }

  void catalogRejectsChangedReviewedProvenance() {
    const auto original =
        readObject(QStringLiteral(HYPRSHELLD_HYPRLAND_CATALOG_FILE));
    QVERIFY(!original.isEmpty());

    const QMap<QString, QJsonValue> mutations{
        {
            QStringLiteral("reviewedTag"),
            QStringLiteral("v0.56.0"),
        },
        {
            QStringLiteral("reviewedCommit"),
            QString(40, QLatin1Char('0')),
        },
        {
            QStringLiteral("repository"),
            QStringLiteral("https://example.invalid/fork"),
        },
    };
    for (auto iterator = mutations.constBegin();
         iterator != mutations.constEnd(); ++iterator) {
      auto object = original;
      auto release = object.value(QStringLiteral("hyprland")).toObject();
      release.insert(iterator.key(), iterator.value());
      object.insert(QStringLiteral("hyprland"), release);
      const auto parsed = parseCatalog(encode(object));
      QVERIFY2(!parsed, qPrintable(iterator.key()));
    }
  }

  void catalogBoundsInputAndCollections() {
    const QByteArray oversized(maximumCatalogBytes + 1, ' ');
    const auto tooLarge = parseCatalog(oversized);
    QVERIFY(!tooLarge);
    QVERIFY(hasCode(tooLarge.errors, QStringLiteral("json.size-limit")));

    auto object = readObject(QStringLiteral(HYPRSHELLD_HYPRLAND_CATALOG_FILE));
    QVERIFY(!object.isEmpty());
    const auto prototype =
        object.value(QStringLiteral("options")).toArray().first();
    QJsonArray options;
    for (qsizetype index = 0; index <= maximumCatalogOptions; ++index) {
      auto option = prototype.toObject();
      option.insert(QStringLiteral("id"),
                    QStringLiteral("test.option.%1").arg(index));
      option.insert(QStringLiteral("path"),
                    QStringLiteral("test:option:%1").arg(index));
      options.append(option);
    }
    object.insert(QStringLiteral("options"), options);
    const auto tooMany = parseCatalog(encode(object));
    QVERIFY(!tooMany);
    QVERIFY(
        hasCode(tooMany.errors, QStringLiteral("catalog.collection-limit")));
  }

  void catalogRejectsDuplicateOptionsAndInvalidDefaults() {
    auto object = readObject(QStringLiteral(HYPRSHELLD_HYPRLAND_CATALOG_FILE));
    auto options = object.value(QStringLiteral("options")).toArray();
    QVERIFY(!options.isEmpty());
    options.append(options.first());
    object.insert(QStringLiteral("options"), options);
    const auto duplicate = parseCatalog(encode(object));
    QVERIFY(!duplicate);
    QVERIFY(hasCode(duplicate.errors, QStringLiteral("catalog.duplicate-id")));
    QVERIFY(
        hasCode(duplicate.errors, QStringLiteral("catalog.duplicate-path")));

    object = readObject(QStringLiteral(HYPRSHELLD_HYPRLAND_CATALOG_FILE));
    options = object.value(QStringLiteral("options")).toArray();
    auto booleanIndex = qsizetype(-1);
    for (qsizetype index = 0; index < options.size(); ++index) {
      if (options.at(index).toObject().value(QStringLiteral("type")) ==
          QJsonValue(QStringLiteral("boolean"))) {
        booleanIndex = index;
        break;
      }
    }
    QVERIFY(booleanIndex >= 0);
    auto malformed = options.at(booleanIndex).toObject();
    malformed.insert(QStringLiteral("default"), QStringLiteral("false"));
    options.replace(booleanIndex, malformed);
    object.insert(QStringLiteral("options"), options);
    const auto wrongType = parseCatalog(encode(object));
    QVERIFY(!wrongType);
    QVERIFY2(hasCode(wrongType.errors, QStringLiteral("catalog.default-type")),
             qPrintable(describeErrors(wrongType.errors)));
  }

  void catalogRejectsInvalidRangesEnumsAndLuaDestinations() {
    auto object = readObject(QStringLiteral(HYPRSHELLD_HYPRLAND_CATALOG_FILE));
    auto options = object.value(QStringLiteral("options")).toArray();
    auto numericIndex = qsizetype(-1);
    auto enumIndex = qsizetype(-1);
    for (qsizetype index = 0; index < options.size(); ++index) {
      const auto option = options.at(index).toObject();
      if (numericIndex < 0 &&
          option.value(QStringLiteral("constraints"))
              .toObject()
              .value(QStringLiteral("min"))
              .isDouble() &&
          option.value(QStringLiteral("constraints"))
              .toObject()
              .value(QStringLiteral("max"))
              .isDouble()) {
        numericIndex = index;
      }
      if (enumIndex < 0 && option.value(QStringLiteral("type")) ==
                               QJsonValue(QStringLiteral("enum"))) {
        enumIndex = index;
      }
    }
    QVERIFY(numericIndex >= 0);
    QVERIFY(enumIndex >= 0);

    auto malformed = options.at(numericIndex).toObject();
    auto constraints =
        malformed.value(QStringLiteral("constraints")).toObject();
    constraints.insert(QStringLiteral("min"), 10);
    constraints.insert(QStringLiteral("max"), 1);
    malformed.insert(QStringLiteral("constraints"), constraints);
    options.replace(numericIndex, malformed);
    object.insert(QStringLiteral("options"), options);
    const auto invalidRange = parseCatalog(encode(object));
    QVERIFY(!invalidRange);
    QVERIFY(
        hasCode(invalidRange.errors, QStringLiteral("catalog.invalid-range")));

    object = readObject(QStringLiteral(HYPRSHELLD_HYPRLAND_CATALOG_FILE));
    options = object.value(QStringLiteral("options")).toArray();
    malformed = options.at(enumIndex).toObject();
    malformed.insert(QStringLiteral("default"),
                     QStringLiteral("not-in-the-closed-choice-set"));
    options.replace(enumIndex, malformed);
    object.insert(QStringLiteral("options"), options);
    const auto invalidEnum = parseCatalog(encode(object));
    QVERIFY(!invalidEnum);
    QVERIFY(hasCode(invalidEnum.errors,
                    QStringLiteral("catalog.default-out-of-range")));

    object = readObject(QStringLiteral(HYPRSHELLD_HYPRLAND_CATALOG_FILE));
    options = object.value(QStringLiteral("options")).toArray();
    malformed = options.first().toObject();
    malformed.insert(QStringLiteral("luaPath"),
                     QJsonArray{QStringLiteral("single")});
    options.replace(0, malformed);
    object.insert(QStringLiteral("options"), options);
    const auto invalidLuaPath = parseCatalog(encode(object));
    QVERIFY(!invalidLuaPath);
    QVERIFY(hasCode(invalidLuaPath.errors,
                    QStringLiteral("catalog.invalid-lua-path")));
  }

  void catalogPinsInheritedDefaultsAndComplexDestinations() {
    const auto parsed = shippedCatalog();
    QVERIFY(parsed);
    QMap<QString, QString> inherited;
    for (const auto &option : parsed.value->options) {
      if (option.inheritedDefaultFrom) {
        inherited.insert(option.path, *option.inheritedDefaultFrom);
        QVERIFY(option.type == OptionType::Color ||
                option.type == OptionType::Gradient);
        QVERIFY(option.defaultPolicy == DefaultPolicy::Hyprland);
        QVERIFY(!validateOptionValue(option, option.defaultValue).isEmpty());
      }
    }
    QCOMPARE(inherited.size(), 5);
    QCOMPARE(inherited.value(
                 QStringLiteral("decoration:shadow:color_inactive")),
             QStringLiteral("decoration:shadow:color"));
    QCOMPARE(inherited.value(
                 QStringLiteral("group:groupbar:text_color_locked_inactive")),
             QStringLiteral("group:groupbar:text_color_inactive"));

    const auto withDefault = [](QJsonObject catalog, const QString &path,
                                QJsonValue value) {
      auto options = catalog.value(QStringLiteral("options")).toArray();
      for (qsizetype index = 0; index < options.size(); ++index) {
        auto option = options.at(index).toObject();
        if (option.value(QStringLiteral("path")) != QJsonValue(path)) {
          continue;
        }
        option.insert(QStringLiteral("default"), std::move(value));
        options.replace(index, option);
        break;
      }
      catalog.insert(QStringLiteral("options"), options);
      return catalog;
    };
    const auto original =
        readObject(QStringLiteral(HYPRSHELLD_HYPRLAND_CATALOG_FILE));

    auto malformed = withDefault(
        original, QStringLiteral("decoration:shadow:color_inactive"),
        QJsonObject{{QStringLiteral("kind"), QStringLiteral("inherit")},
                    {QStringLiteral("from"),
                     QStringLiteral("general:missing_option")}});
    const auto missingSource = parseCatalog(encode(malformed));
    QVERIFY(!missingSource);
    QVERIFY(hasCode(
        missingSource.errors,
        QStringLiteral("catalog.inherited-default-missing-source")));

    malformed = withDefault(
        original, QStringLiteral("decoration:shadow:color_inactive"),
        QJsonObject{
            {QStringLiteral("kind"), QStringLiteral("inherit")},
            {QStringLiteral("from"),
             QStringLiteral("decoration:shadow:color_inactive")}});
    const auto selfReference = parseCatalog(encode(malformed));
    QVERIFY(!selfReference);
    QVERIFY(hasCode(selfReference.errors,
                    QStringLiteral("catalog.inherited-default-cycle")));

    malformed = withDefault(
        original, QStringLiteral("decoration:shadow:color_inactive"),
        QJsonObject{{QStringLiteral("kind"), QStringLiteral("inherit")},
                    {QStringLiteral("from"),
                     QStringLiteral("general:border_size")}});
    const auto wrongType = parseCatalog(encode(malformed));
    QVERIFY(!wrongType);
    QVERIFY(hasCode(
        wrongType.errors,
        QStringLiteral("catalog.inherited-default-type-mismatch")));

    malformed = withDefault(
        original, QStringLiteral("group:groupbar:text_color_inactive"),
        QJsonObject{
            {QStringLiteral("kind"), QStringLiteral("inherit")},
            {QStringLiteral("from"),
             QStringLiteral("group:groupbar:text_color_locked_inactive")}});
    const auto cycle = parseCatalog(encode(malformed));
    QVERIFY(!cycle);
    QVERIFY(hasCode(cycle.errors,
                    QStringLiteral("catalog.inherited-default-cycle")));

    auto surfaces =
        original.value(QStringLiteral("complexSurfaces")).toArray();
    QVERIFY(surfaces.size() == maximumComplexSurfaces);
    auto firstSurface = surfaces.first().toObject();
    firstSurface.insert(QStringLiteral("id"), QStringLiteral("animationz"));
    surfaces.replace(0, firstSurface);
    malformed = original;
    malformed.insert(QStringLiteral("complexSurfaces"), surfaces);
    const auto renamedSurface = parseCatalog(encode(malformed));
    QVERIFY(!renamedSurface);
    QVERIFY(hasCode(renamedSurface.errors,
                    QStringLiteral("catalog.unknown-surface")));

    surfaces = original.value(QStringLiteral("complexSurfaces")).toArray();
    firstSurface = surfaces.first().toObject();
    firstSurface.insert(QStringLiteral("kind"), QStringLiteral("binding"));
    firstSurface.insert(QStringLiteral("module"), QStringLiteral("bindings"));
    firstSurface.insert(QStringLiteral("luaPath"),
                        QJsonArray{QStringLiteral("bind")});
    firstSurface.insert(
        QStringLiteral("schemaRef"),
        QStringLiteral("config.schema.json#/$defs/binding"));
    surfaces.replace(0, firstSurface);
    malformed = original;
    malformed.insert(QStringLiteral("complexSurfaces"), surfaces);
    const auto swappedSurface = parseCatalog(encode(malformed));
    QVERIFY(!swappedSurface);
    QVERIFY(hasCode(swappedSurface.errors,
                    QStringLiteral("catalog.surface-contract-mismatch")));
  }

  void catalogParserMatchesPublishedLexicalAndPolicyBounds() {
    auto object =
        readObject(QStringLiteral(HYPRSHELLD_HYPRLAND_CATALOG_FILE));
    auto options = object.value(QStringLiteral("options")).toArray();
    auto option = options.first().toObject();
    option.insert(QStringLiteral("id"),
                  QStringLiteral("hyprland.Invalid.option"));
    options.replace(0, option);
    object.insert(QStringLiteral("options"), options);
    const auto id = parseCatalog(encode(object));
    QVERIFY(!id);
    QVERIFY(hasCode(id.errors, QStringLiteral("catalog.invalid-id")));

    object = readObject(QStringLiteral(HYPRSHELLD_HYPRLAND_CATALOG_FILE));
    options = object.value(QStringLiteral("options")).toArray();
    option = options.first().toObject();
    option.insert(QStringLiteral("luaPath"),
                  QJsonArray{QString(65, QLatin1Char('a')),
                             QStringLiteral("value")});
    options.replace(0, option);
    object.insert(QStringLiteral("options"), options);
    const auto luaSegment = parseCatalog(encode(object));
    QVERIFY(!luaSegment);
    QVERIFY(hasCode(luaSegment.errors,
                    QStringLiteral("catalog.invalid-lua-identifier")));

    object = readObject(QStringLiteral(HYPRSHELLD_HYPRLAND_CATALOG_FILE));
    options = object.value(QStringLiteral("options")).toArray();
    option = options.first().toObject();
    auto constraints = option.value(QStringLiteral("constraints")).toObject();
    constraints.insert(QStringLiteral("choices"), QJsonArray{});
    option.insert(QStringLiteral("constraints"), constraints);
    options.replace(0, option);
    object.insert(QStringLiteral("options"), options);
    const auto emptyChoices = parseCatalog(encode(object));
    QVERIFY(!emptyChoices);
    QVERIFY(hasCode(emptyChoices.errors,
                    QStringLiteral("catalog.collection-limit")));

    object = readObject(QStringLiteral(HYPRSHELLD_HYPRLAND_CATALOG_FILE));
    auto release = object.value(QStringLiteral("hyprland")).toObject();
    release.insert(QStringLiteral("minimumPatch"), 1);
    object.insert(QStringLiteral("hyprland"), release);
    const auto releaseRange = parseCatalog(encode(object));
    QVERIFY(!releaseRange);
    QVERIFY(hasCode(releaseRange.errors,
                    QStringLiteral("catalog.invalid-release-range")));

    object = readObject(QStringLiteral(HYPRSHELLD_HYPRLAND_CATALOG_FILE));
    auto compatibility =
        object.value(QStringLiteral("compatibility")).toObject();
    compatibility.insert(QStringLiteral("newerMinor"),
                         QStringLiteral("unsupported"));
    object.insert(QStringLiteral("compatibility"), compatibility);
    const auto policy = parseCatalog(encode(object));
    QVERIFY(!policy);
    QVERIFY(hasCode(policy.errors,
                    QStringLiteral("catalog.invalid-compatibility-policy")));
  }

  void shippedDefaultStateIsCanonicalAndComplete() {
    const auto catalogResult = shippedCatalog();
    QVERIFY(catalogResult);
    const auto bytes =
        readBytes(QStringLiteral(HYPRSHELLD_HYPRLAND_DEFAULTS_FILE));
    QVERIFY(!bytes.isEmpty());
    const auto parsed = parseStateBytes(bytes, *catalogResult.value);
    QVERIFY2(parsed,
             qPrintable(
                 parsed.errors.isEmpty()
                     ? QStringLiteral("desired-state parser returned no value")
                     : parsed.errors.constFirst().message));
    QCOMPARE(parsed.value->formatVersion, quint32(1));
    QCOMPARE(parsed.value->revision, quint64(0));
    QCOMPARE(parsed.value->targetHyprland, QStringLiteral("0.56.1"));
    QCOMPARE(parsed.value->catalogDigest, catalogResult.value->digest);
    const auto actionCatalogResult = shippedActionCatalog();
    QVERIFY(actionCatalogResult);
    QCOMPARE(parsed.value->actionCatalogDigest,
             actionCatalogResult.value->digest);
    QCOMPARE(parsed.value->compatibility, CompatibilityDecision::Exact);
    QVERIFY(!parsed.value->readOnly);
    QVERIFY(parsed.value->overrides.isEmpty());

    const auto generated =
        defaultDesiredState(*catalogResult.value, *actionCatalogResult.value);
    QCOMPARE(serializeDesiredState(*parsed.value),
             serializeDesiredState(generated));
    const auto reparsed = parseStateBytes(serializeDesiredState(*parsed.value),
                                          *catalogResult.value);
    QVERIFY(reparsed);
    QCOMPARE(serializeDesiredState(*reparsed.value),
             serializeDesiredState(*parsed.value));

    auto minorTarget = defaultStateObject();
    minorTarget.insert(QStringLiteral("targetHyprland"),
                       QStringLiteral("0.56"));
    const auto supportedMinor = parseState(minorTarget, *catalogResult.value);
    QVERIFY(supportedMinor);
    QCOMPARE(supportedMinor.value->compatibility,
             CompatibilityDecision::SupportedMinor);
    QVERIFY(!supportedMinor.value->readOnly);
  }

  void managedGlowActivationSafetyIsCrossFieldAndStructurallyPermissive() {
    const auto catalogResult = shippedCatalog();
    const auto actionCatalogResult = shippedActionCatalog();
    QVERIFY(catalogResult);
    QVERIFY(actionCatalogResult);

    const auto base = defaultDesiredState(
        *catalogResult.value, *actionCatalogResult.value
    );
    QVERIFY(validateManagedActivationSafety(
                base, *catalogResult.value
            ).isEmpty());

    const auto stateWith = [&base](
        const bool enabled,
        const std::optional<int> range
    ) {
      auto state = base;
      state.overrides.insert(
          QStringLiteral("hyprland.decoration.glow.enabled"), enabled
      );
      if (range.has_value()) {
        state.overrides.insert(
            QStringLiteral("hyprland.decoration.glow.range"), *range
        );
      }
      return state;
    };

    for (const auto range : {0, 9}) {
      QVERIFY(validateManagedActivationSafety(
                  stateWith(false, range), *catalogResult.value
              ).isEmpty());
    }
    QVERIFY(validateManagedActivationSafety(
                stateWith(true, std::nullopt), *catalogResult.value
            ).isEmpty());
    for (const auto range : {10, 100}) {
      QVERIFY(validateManagedActivationSafety(
                  stateWith(true, range), *catalogResult.value
              ).isEmpty());
    }

    for (const auto range : {0, 1, 9}) {
      const auto unsafe = stateWith(true, range);
      const auto errors = validateManagedActivationSafety(
          unsafe, *catalogResult.value
      );
      QCOMPARE(errors.size(), qsizetype(1));
      QCOMPARE(
          errors.constFirst().path,
          QStringLiteral("$.overrides.hyprland.decoration.glow.range")
      );
      QCOMPARE(
          errors.constFirst().code,
          QStringLiteral("state.unsafe-glow-range")
      );
      QCOMPARE(
          errors.constFirst().message,
          QStringLiteral(
              "Inner glow can be enabled only when its range is at least 10; "
              "disable glow or raise the range."
          )
      );

      const auto bytes = serializeDesiredState(unsafe);
      const auto parsed = parseDesiredState(
          QByteArrayView(bytes), *catalogResult.value,
          *actionCatalogResult.value
      );
      QVERIFY2(parsed, qPrintable(describeErrors(parsed.errors)));
      QCOMPARE(serializeDesiredState(*parsed.value), bytes);
      QCOMPARE(parsed.value->overrides, unsafe.overrides);
    }
  }

  void desiredStateRejectsUnknownAndDuplicateRootFields() {
    const auto catalogResult = shippedCatalog();
    QVERIFY(catalogResult);
    auto object = defaultStateObject();
    object.insert(QStringLiteral("futureField"), true);
    const auto unknown = parseState(object, *catalogResult.value);
    QVERIFY(!unknown);
    QVERIFY(hasCode(unknown.errors, QStringLiteral("state.unknown-field")));

    auto bytes = readBytes(QStringLiteral(HYPRSHELLD_HYPRLAND_DEFAULTS_FILE));
    const auto needle = QByteArrayLiteral("\"formatVersion\": 1");
    QVERIFY(bytes.contains(needle));
    bytes.replace(
        needle,
        QByteArrayLiteral("\"formatVersion\": 1, \"format\\u0056ersion\": 1"));
    const auto duplicate = parseStateBytes(bytes, *catalogResult.value);
    QVERIFY(!duplicate);
    QVERIFY(hasCode(duplicate.errors, QStringLiteral("json.duplicate-key")));
  }

  void desiredStateRejectsInvalidFormatAndRevision_data() {
    QTest::addColumn<QString>("field");
    QTest::addColumn<QJsonValue>("value");
    QTest::addColumn<QString>("code");
    QTest::newRow("missing-format")
        << QStringLiteral("formatVersion") << QJsonValue(QJsonValue::Undefined)
        << QStringLiteral("state.integer-out-of-range");
    QTest::newRow("future-format")
        << QStringLiteral("formatVersion") << QJsonValue(2)
        << QStringLiteral("state.unsupported-format-version");
    QTest::newRow("fractional-format")
        << QStringLiteral("formatVersion") << QJsonValue(1.5)
        << QStringLiteral("state.integer-out-of-range");
    QTest::newRow("numeric-revision")
        << QStringLiteral("revision") << QJsonValue(1)
        << QStringLiteral("state.string-required");
    QTest::newRow("leading-zero-revision")
        << QStringLiteral("revision") << QJsonValue(QStringLiteral("01"))
        << QStringLiteral("state.invalid-revision");
    QTest::newRow("overflow-revision")
        << QStringLiteral("revision")
        << QJsonValue(QStringLiteral("18446744073709551616"))
        << QStringLiteral("state.invalid-revision");
  }

  void desiredStateRejectsInvalidFormatAndRevision() {
    QFETCH(QString, field);
    QFETCH(QJsonValue, value);
    QFETCH(QString, code);
    const auto catalogResult = shippedCatalog();
    QVERIFY(catalogResult);
    auto object = defaultStateObject();
    if (value.isUndefined()) {
      object.remove(field);
    } else {
      object.insert(field, value);
    }
    const auto parsed = parseState(object, *catalogResult.value);
    QVERIFY(!parsed);
    QVERIFY2(hasCode(parsed.errors, code), qPrintable(code));
  }

  void curatedAppearanceOptionsRemainSafeAndReloadOnly() {
    const auto catalogResult = shippedCatalog();
    QVERIFY(catalogResult);
    const auto &catalog = *catalogResult.value;

    struct ExpectedOption final {
      const char *id;
      OptionType type;
      ControlKind control;
      QJsonValue defaultValue;
    };
    const std::array expected{
        ExpectedOption{"hyprland.animations.enabled", OptionType::Boolean,
                       ControlKind::Toggle, true},
        ExpectedOption{"hyprland.decoration.blur.enabled",
                       OptionType::Boolean, ControlKind::Toggle, true},
        ExpectedOption{"hyprland.decoration.rounding", OptionType::Integer,
                       ControlKind::SpinBox, 0},
        ExpectedOption{"hyprland.decoration.shadow.enabled",
                       OptionType::Boolean, ControlKind::Toggle, true},
        ExpectedOption{"hyprland.general.border_size", OptionType::Integer,
                       ControlKind::SpinBox, 1},
        ExpectedOption{"hyprland.general.layout", OptionType::Enumeration,
                       ControlKind::Select, QStringLiteral("dwindle")},
        ExpectedOption{"hyprland.general.resize_on_border",
                       OptionType::Boolean, ControlKind::Toggle, false},
        ExpectedOption{"hyprland.general.snap.enabled", OptionType::Boolean,
                       ControlKind::Toggle, false},
    };

    for (const auto &entry : expected) {
      const auto id = QString::fromLatin1(entry.id);
      const auto *option = findOption(catalog, id);
      QVERIFY2(option != nullptr, qPrintable(id));
      QVERIFY(option->type == entry.type);
      QVERIFY(option->control == entry.control);
      QCOMPARE(option->defaultValue, entry.defaultValue);
      QVERIFY(option->defaultPolicy == DefaultPolicy::Hyprland);
      QVERIFY(option->writable);
      QVERIFY(option->uiTier == UiTier::Common);
      QVERIFY(option->applyMode == ApplyMode::Reload);
      QVERIFY(option->risk == RiskLevel::Safe);
      QVERIFY(!option->inheritedDefaultFrom.has_value());
    }

    for (const auto *id : {"hyprland.decoration.rounding",
                           "hyprland.general.border_size"}) {
      const auto *option = findOption(catalog, QString::fromLatin1(id));
      QVERIFY(option != nullptr);
      QVERIFY(option->constraints.minimum.has_value());
      QVERIFY(option->constraints.maximum.has_value());
      QCOMPARE(option->constraints.minimum->toInt(), 0);
      QCOMPARE(option->constraints.maximum->toInt(), 20);
    }

    const auto *layout = findOption(
        catalog, QStringLiteral("hyprland.general.layout"));
    QVERIFY(layout != nullptr);
    QStringList choices;
    for (const auto &choiceValue : layout->constraints.choices) {
      choices.append(choiceValue.toObject()
                         .value(QStringLiteral("value"))
                         .toString());
    }
    QCOMPARE(choices,
             QStringList({QStringLiteral("dwindle"),
                          QStringLiteral("master"),
                          QStringLiteral("scrolling"),
                          QStringLiteral("monocle")}));
  }

  void validatesExplicitScalarOverrides() {
    const auto catalogResult = shippedCatalog();
    QVERIFY(catalogResult);
    const auto &catalog = *catalogResult.value;
    const auto boolean = std::ranges::find_if(
        catalog.options, [](const OptionDefinition &option) {
          return option.type == OptionType::Boolean;
        });
    const auto numeric = std::ranges::find_if(
        catalog.options, [](const OptionDefinition &option) {
          return option.type == OptionType::Integer &&
                 option.constraints.minimum && option.constraints.maximum &&
                 option.constraints.minimum->isDouble() &&
                 option.constraints.maximum->isDouble() &&
                 option.constraints.minimum->toDouble() <
                     option.constraints.maximum->toDouble();
        });
    const auto enumeration = std::ranges::find_if(
        catalog.options, [](const OptionDefinition &option) {
          return option.type == OptionType::Enumeration &&
                 !option.constraints.choices.isEmpty();
        });
    QVERIFY(boolean != catalog.options.end());
    QVERIFY(numeric != catalog.options.end());
    QVERIFY(enumeration != catalog.options.end());
    QSet<QString> nonWritablePaths;
    for (const auto &option : catalog.options) {
      if (!option.writable) {
        nonWritablePaths.insert(option.path);
      }
    }
    QCOMPARE(nonWritablePaths,
             stringSet({
                 QStringLiteral("input:scroll_points"),
                 QStringLiteral("input:tablet:output"),
                 QStringLiteral("input:touchdevice:output"),
                 QStringLiteral("scrolling:explicit_column_widths"),
    }));
    for (const auto &path : nonWritablePaths) {
      auto id = QStringLiteral("hyprland.") + path;
      id.replace(QLatin1Char(':'), QLatin1Char('.'));
      const auto *option = findOption(catalog, id);
      QVERIFY2(option != nullptr, qPrintable(path));
      auto readOnlyObject = defaultStateObject();
      readOnlyObject.insert(
          QStringLiteral("overrides"),
          QJsonObject{{option->id, QStringLiteral("managed-denied")}});
      const auto readOnlyOverride = parseState(readOnlyObject, catalog);
      QVERIFY(!readOnlyOverride);
      QVERIFY(hasCode(readOnlyOverride.errors,
                      QStringLiteral("state.read-only-option")));
    }

    const auto *swallowRegex = findOption(
        catalog, QStringLiteral("hyprland.misc.swallow_regex"));
    const auto *swallowExceptionRegex = findOption(
        catalog,
        QStringLiteral("hyprland.misc.swallow_exception_regex"));
    const auto *defaultMonitor = findOption(
        catalog, QStringLiteral("hyprland.cursor.default_monitor"));
    const auto *layout = findOption(
        catalog, QStringLiteral("hyprland.general.layout"));
    QVERIFY(swallowRegex != nullptr);
    QVERIFY(swallowExceptionRegex != nullptr);
    QVERIFY(defaultMonitor != nullptr);
    QVERIFY(layout != nullptr);

    for (const auto *swallowPattern :
         {swallowRegex, swallowExceptionRegex}) {
      auto semanticObject = defaultStateObject();
      semanticObject.insert(
          QStringLiteral("overrides"),
          QJsonObject{{swallowPattern->id, QStringLiteral("^foo$")}});
      const auto semanticAccepted = parseState(semanticObject, catalog);
      QVERIFY2(semanticAccepted,
               qPrintable(describeErrors(semanticAccepted.errors)));

      for (const auto &regex : {QString(), QStringLiteral("[")}) {
        semanticObject = defaultStateObject();
        semanticObject.insert(
            QStringLiteral("overrides"),
            QJsonObject{{swallowPattern->id, regex}});
        const auto invalid = parseState(semanticObject, catalog);
        QVERIFY(!invalid);
        QVERIFY(hasCode(invalid.errors,
                        QStringLiteral("state.invalid-regex")));
      }
    }

    auto semanticObject = defaultStateObject();
    semanticObject.insert(
        QStringLiteral("overrides"),
        QJsonObject{{defaultMonitor->id, QStringLiteral("DP-1")}});
    const auto staticDefaultMonitor = parseState(semanticObject, catalog);
    QVERIFY2(staticDefaultMonitor,
             qPrintable(describeErrors(staticDefaultMonitor.errors)));

    semanticObject = defaultStateObject();
    semanticObject.insert(
        QStringLiteral("overrides"),
        QJsonObject{{defaultMonitor->id, QStringLiteral("current")}});
    const auto dynamicDefaultMonitor = parseState(semanticObject, catalog);
    QVERIFY(!dynamicDefaultMonitor);
    QVERIFY(hasCode(
        dynamicDefaultMonitor.errors,
        QStringLiteral("state.invalid-static-monitor-selector")));

    semanticObject = defaultStateObject();
    semanticObject.insert(
        QStringLiteral("overrides"),
        QJsonObject{{layout->id, QStringLiteral("plugin-layout")}});
    const auto pluginLayout = parseState(semanticObject, catalog);
    QVERIFY(!pluginLayout);
    QVERIFY(hasCode(pluginLayout.errors,
                    QStringLiteral("option.constraint-violation")));

    auto object = defaultStateObject();
    QJsonObject overrides{
        {boolean->id, !boolean->defaultValue.toBool()},
    };
    const auto firstChoice =
        enumeration->constraints.choices.constFirst().toObject().value(
            QStringLiteral("value"));
    if (firstChoice != enumeration->defaultValue) {
      overrides.insert(enumeration->id, firstChoice);
    }
    const auto minimum = numeric->constraints.minimum->toDouble();
    const auto maximum = numeric->constraints.maximum->toDouble();
    const auto numericValue =
        numeric->defaultValue.toDouble() != minimum ? minimum : maximum;
    overrides.insert(numeric->id, numericValue);
    object.insert(QStringLiteral("overrides"), overrides);
    const auto accepted = parseState(object, catalog);
    QVERIFY2(accepted,
             qPrintable(accepted.errors.isEmpty()
                            ? QStringLiteral("valid overrides rejected")
                            : accepted.errors.constFirst().message));
    QCOMPARE(accepted.value->overrides.size(), overrides.size());

    overrides.insert(boolean->id, QStringLiteral("true"));
    object.insert(QStringLiteral("overrides"), overrides);
    const auto wrongType = parseState(object, catalog);
    QVERIFY(!wrongType);
    QVERIFY(hasCode(wrongType.errors, QStringLiteral("option.type-mismatch")));

    overrides.insert(boolean->id, !boolean->defaultValue.toBool());
    overrides.insert(numeric->id, maximum + 1.0);
    object.insert(QStringLiteral("overrides"), overrides);
    const auto outOfRange = parseState(object, catalog);
    QVERIFY(!outOfRange);
    QVERIFY(hasCode(outOfRange.errors,
                    QStringLiteral("option.constraint-violation")));

    overrides.remove(numeric->id);
    overrides.insert(enumeration->id, QStringLiteral("not-a-real-choice"));
    object.insert(QStringLiteral("overrides"), overrides);
    const auto invalidEnum = parseState(object, catalog);
    QVERIFY(!invalidEnum);
    QVERIFY(hasCode(invalidEnum.errors,
                    QStringLiteral("option.constraint-violation")));

    const auto *gradient = findOption(
        catalog, QStringLiteral("hyprland.general.col.active_border"));
    const auto *gap = findOption(
        catalog, QStringLiteral("hyprland.general.gaps_in"));
    const auto *fontWeight = findOption(
        catalog,
        QStringLiteral("hyprland.group.groupbar.font_weight_active"));
    QVERIFY(gradient != nullptr);
    QVERIFY(gap != nullptr);
    QVERIFY(fontWeight != nullptr);
    object = defaultStateObject();
    object.insert(
        QStringLiteral("overrides"),
        QJsonObject{
            {gradient->id,
             QJsonObject{{QStringLiteral("colors"),
                          QJsonArray{QStringLiteral("0xFFFFFFFF")}},
                         {QStringLiteral("angle"), -90.0}}},
            {gap->id, QJsonArray{200000, -200000, 0, 0}},
            {fontWeight->id, 0},
        });
    const auto structured = parseState(object, catalog);
    QVERIFY2(structured, qPrintable(describeErrors(structured.errors)));
    const auto structuredRoundTrip = parseStateBytes(
        serializeDesiredState(*structured.value), catalog);
    QVERIFY(structuredRoundTrip);
    QCOMPARE(*structuredRoundTrip.value, *structured.value);
  }

  void rejectsRedundantOverridesAndCurrentDigestMismatch() {
    const auto catalogResult = shippedCatalog();
    QVERIFY(catalogResult);
    const auto &catalog = *catalogResult.value;
    QVERIFY(!catalog.options.isEmpty());

    auto object = defaultStateObject();
    object.insert(QStringLiteral("overrides"),
                  QJsonObject{{
                      catalog.options.constFirst().id,
                      catalog.options.constFirst().defaultValue,
                  }});
    const auto redundant = parseState(object, catalog);
    QVERIFY(!redundant);
    QVERIFY(
        hasCode(redundant.errors, QStringLiteral("state.redundant-override")));

    object = defaultStateObject();
    object.insert(QStringLiteral("catalogDigest"),
                  QString(64, QLatin1Char('a')));
    const auto mismatch = parseState(object, catalog);
    QVERIFY(!mismatch);
    QVERIFY(hasCode(mismatch.errors,
                    QStringLiteral("state.catalog-digest-mismatch")));
  }

  void actionCatalogDigestIsRequiredAndChangesWithEitherAuthority() {
    const auto catalogResult = shippedCatalog();
    const auto actionCatalogResult = shippedActionCatalog();
    QVERIFY(catalogResult);
    QVERIFY(actionCatalogResult);

    auto object = defaultStateObject();
    object.remove(QStringLiteral("actionCatalogDigest"));
    const auto missing = parseState(object, *catalogResult.value);
    QVERIFY(!missing);
    QVERIFY(
        hasErrorAt(missing.errors, QStringLiteral("$.actionCatalogDigest")));

    object = defaultStateObject();
    object.insert(QStringLiteral("actionCatalogDigest"),
                  QStringLiteral("not-a-digest"));
    const auto malformed = parseState(object, *catalogResult.value);
    QVERIFY(!malformed);
    QVERIFY(hasCode(malformed.errors,
                    QStringLiteral("state.invalid-action-catalog-digest")));

    object = defaultStateObject();
    object.insert(QStringLiteral("actionCatalogDigest"),
                  QString(64, QLatin1Char('a')));
    const auto mismatch = parseState(object, *catalogResult.value);
    QVERIFY(!mismatch);
    QVERIFY(hasCode(mismatch.errors,
                    QStringLiteral("state.action-catalog-digest-mismatch")));

    auto actionObject =
        readObject(QStringLiteral(HYPRSHELLD_HYPRLAND_ACTION_CATALOG_FILE));
    auto actions =
        actionObject.value(QStringLiteral("dispatcherActions")).toArray();
    auto first = actions.first().toObject();
    first.insert(QStringLiteral("description"),
                 first.value(QStringLiteral("description")).toString() +
                     QStringLiteral(" Reviewed."));
    actions.replace(0, first);
    actionObject.insert(QStringLiteral("dispatcherActions"), actions);
    const auto schemaBytes =
        readBytes(QStringLiteral(HYPRSHELLD_HYPRLAND_CONFIG_SCHEMA_FILE));
    const auto changedActionCatalog =
        parseActionCatalog(encode(actionObject), schemaBytes);
    QVERIFY(!changedActionCatalog);
    QVERIFY(hasCode(changedActionCatalog.errors,
                    QStringLiteral("action-catalog.integrity-mismatch")));
    object = defaultStateObject();
    auto changedAuthority = *actionCatalogResult.value;
    changedAuthority.digest = QString(64, QLatin1Char('a'));
    const auto staleAction = parseDesiredState(
        encode(object), *catalogResult.value, changedAuthority);
    QVERIFY(!staleAction);
    QVERIFY(hasCode(staleAction.errors,
                    QStringLiteral("state.action-catalog-digest-mismatch")));

    auto changedSchemaBytes = schemaBytes;
    changedSchemaBytes.append(' ');
    const auto changedSchemaCatalog = parseActionCatalog(
        actionCatalogForSchema(changedSchemaBytes), changedSchemaBytes);
    QVERIFY(!changedSchemaCatalog);
    QVERIFY(hasCode(changedSchemaCatalog.errors,
                    QStringLiteral("action-catalog.integrity-mismatch")));
  }

  void exactTargetRejectsUnknownOverrides() {
    const auto catalogResult = shippedCatalog();
    QVERIFY(catalogResult);
    auto object = defaultStateObject();
    object.insert(QStringLiteral("overrides"),
                  QJsonObject{{QStringLiteral("future:option"), true}});
    const auto parsed = parseState(object, *catalogResult.value);
    QVERIFY(!parsed);
    QVERIFY(hasCode(parsed.errors, QStringLiteral("state.unknown-option")));
  }

  void dispatcherArgumentsAreTypedAndActionsFailClosed() {
    const auto catalogResult = shippedCatalog();
    QVERIFY(catalogResult);

    auto record =
        binding(QStringLiteral("move-cursor"), {QStringLiteral("super")},
                QStringLiteral("F8"), QStringLiteral("Move cursor"));
    record.insert(QStringLiteral("actionType"), QStringLiteral("dispatcher"));
    record.insert(QStringLiteral("action"), QStringLiteral("cursor.move"));
    record.insert(QStringLiteral("arguments"), QJsonObject{
                                                   {QStringLiteral("x"), 12.5},
                                                   {QStringLiteral("y"), -3.0},
                                               });
    auto object = defaultStateObject();
    object.insert(QStringLiteral("bindings"), QJsonArray{record});
    const auto accepted = parseState(object, *catalogResult.value);
    QVERIFY2(accepted,
             qPrintable(accepted.errors.isEmpty()
                            ? QStringLiteral(
                                  "typed dispatcher arguments were rejected")
                            : accepted.errors.constFirst().message));
    QCOMPARE(accepted.value->bindings.constFirst().arguments,
             record.value(QStringLiteral("arguments")).toObject());

    auto malformed = record;
    malformed.insert(QStringLiteral("arguments"),
                     QJsonObject{{QStringLiteral("x"), 12.5}});
    object.insert(QStringLiteral("bindings"), QJsonArray{malformed});
    const auto missing = parseState(object, *catalogResult.value);
    QVERIFY(!missing);
    QVERIFY(hasErrorAt(missing.errors,
                       QStringLiteral("$.bindings[0].arguments.y")));

    malformed = record;
    malformed.insert(QStringLiteral("arguments"),
                     QJsonObject{
                         {QStringLiteral("x"), QStringLiteral("12.5")},
                         {QStringLiteral("y"), -3.0},
                     });
    object.insert(QStringLiteral("bindings"), QJsonArray{malformed});
    const auto wrongType = parseState(object, *catalogResult.value);
    QVERIFY(!wrongType);
    QVERIFY(hasErrorAt(wrongType.errors,
                       QStringLiteral("$.bindings[0].arguments.x")));

    malformed = record;
    auto arguments = malformed.value(QStringLiteral("arguments")).toObject();
    arguments.insert(QStringLiteral("command"), QStringLiteral("whoami"));
    malformed.insert(QStringLiteral("arguments"), arguments);
    object.insert(QStringLiteral("bindings"), QJsonArray{malformed});
    const auto unknownArgument = parseState(object, *catalogResult.value);
    QVERIFY(!unknownArgument);
    QVERIFY(hasErrorAt(unknownArgument.errors,
                       QStringLiteral("$.bindings[0].arguments.command")));

    malformed =
        binding(QStringLiteral("closed-action"), {QStringLiteral("super")},
                QStringLiteral("F9"), QStringLiteral("Closed action"));
    malformed.insert(QStringLiteral("actionType"), QStringLiteral("shell"));
    malformed.insert(QStringLiteral("action"), QStringLiteral("exec"));
    object.insert(QStringLiteral("bindings"), QJsonArray{malformed});
    const auto shell = parseState(object, *catalogResult.value);
    QVERIFY(!shell);
    QVERIFY(hasCode(shell.errors, QStringLiteral("state.invalid-action-type")));

    malformed =
        binding(QStringLiteral("closed-action"), {QStringLiteral("super")},
                QStringLiteral("F9"), QStringLiteral("Closed action"));
    malformed.insert(QStringLiteral("action"),
                     QStringLiteral("exec_raw rm -rf /"));
    object.insert(QStringLiteral("bindings"), QJsonArray{malformed});
    const auto raw = parseState(object, *catalogResult.value);
    QVERIFY(!raw);
    QVERIFY(
        hasCode(raw.errors, QStringLiteral("state.unknown-binding-action")));

    malformed =
        binding(QStringLiteral("closed-action"), {QStringLiteral("super")},
                QStringLiteral("F9"), QStringLiteral("Closed action"));
    malformed.insert(
        QStringLiteral("arguments"),
        QJsonObject{{QStringLiteral("command"), QStringLiteral("whoami")}});
    object.insert(QStringLiteral("bindings"), QJsonArray{malformed});
    const auto semanticArguments = parseState(object, *catalogResult.value);
    QVERIFY(!semanticArguments);
    QVERIFY(hasCode(semanticArguments.errors,
                    QStringLiteral("state.unexpected-action-arguments")));

    auto fullscreenState = binding(
        QStringLiteral("fullscreen-state"), {QStringLiteral("super")},
        QStringLiteral("F10"), QStringLiteral("Fullscreen state"));
    fullscreenState.insert(QStringLiteral("actionType"),
                           QStringLiteral("dispatcher"));
    fullscreenState.insert(QStringLiteral("action"),
                           QStringLiteral("window.fullscreen_state"));
    fullscreenState.insert(
        QStringLiteral("arguments"),
        QJsonObject{{QStringLiteral("internal"), 0},
                    {QStringLiteral("client"), 2}});
    object = defaultStateObject();
    object.insert(QStringLiteral("bindings"), QJsonArray{fullscreenState});
    const auto validFullscreenModes =
        parseState(object, *catalogResult.value);
    QVERIFY(validFullscreenModes);

    for (const auto &[field, value] :
         std::array<std::pair<QString, int>, 2>{
             std::pair{QStringLiteral("internal"), -1},
             std::pair{QStringLiteral("client"), 3},
         }) {
      auto invalidFullscreenState = fullscreenState;
      auto fullscreenArguments = invalidFullscreenState
                                     .value(QStringLiteral("arguments"))
                                     .toObject();
      fullscreenArguments.insert(field, value);
      invalidFullscreenState.insert(QStringLiteral("arguments"),
                                    fullscreenArguments);
      object.insert(QStringLiteral("bindings"),
                    QJsonArray{invalidFullscreenState});
      const auto rejectedMode = parseState(object, *catalogResult.value);
      QVERIFY(!rejectedMode);
      QVERIFY(hasCode(rejectedMode.errors,
                      QStringLiteral("state.value-out-of-range")));
    }

    auto signal = binding(QStringLiteral("signal-window"),
                          {QStringLiteral("super")}, QStringLiteral("F11"),
                          QStringLiteral("Signal window"));
    signal.insert(QStringLiteral("actionType"), QStringLiteral("dispatcher"));
    signal.insert(QStringLiteral("action"), QStringLiteral("window.signal"));
    signal.insert(QStringLiteral("arguments"),
                  QJsonObject{{QStringLiteral("signal"), 31}});
    object = defaultStateObject();
    object.insert(QStringLiteral("bindings"), QJsonArray{signal});
    const auto maximumSignal = parseState(object, *catalogResult.value);
    QVERIFY(maximumSignal);

    signal.insert(QStringLiteral("arguments"),
                  QJsonObject{{QStringLiteral("signal"), 32}});
    object.insert(QStringLiteral("bindings"), QJsonArray{signal});
    const auto excessiveSignal = parseState(object, *catalogResult.value);
    QVERIFY(!excessiveSignal);
    QVERIFY(hasCode(excessiveSignal.errors,
                    QStringLiteral("state.value-out-of-range")));

    auto shortcut = binding(QStringLiteral("send-shortcut"), {},
                            QStringLiteral("F6"),
                            QStringLiteral("Send shortcut"));
    shortcut.insert(QStringLiteral("actionType"),
                    QStringLiteral("dispatcher"));
    shortcut.insert(QStringLiteral("action"),
                    QStringLiteral("send_shortcut"));
    shortcut.insert(
        QStringLiteral("arguments"),
        QJsonObject{{QStringLiteral("mods"), QStringLiteral("SHIFT CTRL")},
                    {QStringLiteral("key"), QStringLiteral("F2")},
                    {QStringLiteral("window"), QStringLiteral("active")}});
    object = defaultStateObject();
    object.insert(QStringLiteral("bindings"), QJsonArray{shortcut});
    const auto validShortcut = parseState(object, *catalogResult.value);
    QVERIFY2(validShortcut, qPrintable(describeErrors(validShortcut.errors)));

    for (const auto &mods : {QStringLiteral("CTRL SHIFT"),
                             QStringLiteral("CONTROL"),
                             QStringLiteral("SHIFT SHIFT")}) {
      auto invalidShortcut = shortcut;
      auto args = invalidShortcut.value(QStringLiteral("arguments")).toObject();
      args.insert(QStringLiteral("mods"), mods);
      invalidShortcut.insert(QStringLiteral("arguments"), args);
      object.insert(QStringLiteral("bindings"), QJsonArray{invalidShortcut});
      const auto invalid = parseState(object, *catalogResult.value);
      QVERIFY2(!invalid, qPrintable(mods));
      QVERIFY2(hasCode(invalid.errors,
                       QStringLiteral("state.non-canonical-action-modifiers"))
                   || hasCode(invalid.errors,
                              QStringLiteral("state.invalid-string")),
               qPrintable(mods));
    }
    for (const auto &key : {QStringLiteral("NOTAREALKEY"),
                            QStringLiteral("mouse_down"),
                            QStringLiteral("catchall"),
                            QStringLiteral("code:2147483648")}) {
      auto invalidShortcut = shortcut;
      auto args = invalidShortcut.value(QStringLiteral("arguments")).toObject();
      args.insert(QStringLiteral("key"), key);
      invalidShortcut.insert(QStringLiteral("arguments"), args);
      object.insert(QStringLiteral("bindings"), QJsonArray{invalidShortcut});
      const auto invalid = parseState(object, *catalogResult.value);
      QVERIFY2(!invalid, qPrintable(key));
      QVERIFY2(hasCode(invalid.errors, QStringLiteral("state.invalid-key"))
                   || hasCode(invalid.errors,
                              QStringLiteral("state.invalid-action-key"))
                   || hasCode(invalid.errors,
                              QStringLiteral("state.invalid-string")),
               qPrintable(key));
    }

    auto pass = binding(QStringLiteral("pass-window"), {},
                        QStringLiteral("F7"), QStringLiteral("Pass"));
    pass.insert(QStringLiteral("actionType"), QStringLiteral("dispatcher"));
    pass.insert(QStringLiteral("action"), QStringLiteral("pass"));
    for (const auto &selector : {
             QStringLiteral("active"), QStringLiteral("class:^(foo)$"),
             QStringLiteral("stableid:1a"),
             QStringLiteral("address:0x10"), QStringLiteral("pid:42")}) {
      pass.insert(QStringLiteral("arguments"),
                  QJsonObject{{QStringLiteral("window"), selector}});
      object = defaultStateObject();
      object.insert(QStringLiteral("bindings"), QJsonArray{pass});
      const auto validSelector = parseState(object, *catalogResult.value);
      QVERIFY2(validSelector,
               qPrintable(selector + QLatin1Char(':') +
                          describeErrors(validSelector.errors)));
    }
    for (const auto &selector : {QStringLiteral("bogus"),
                                 QStringLiteral("class:[")}) {
      pass.insert(QStringLiteral("arguments"),
                  QJsonObject{{QStringLiteral("window"), selector}});
      object.insert(QStringLiteral("bindings"), QJsonArray{pass});
      const auto invalidSelector = parseState(object, *catalogResult.value);
      QVERIFY2(!invalidSelector, qPrintable(selector));
      QVERIFY2(hasCode(invalidSelector.errors,
                       QStringLiteral("state.invalid-window-selector"))
                   || hasCode(invalidSelector.errors,
                              QStringLiteral("state.invalid-string")),
               qPrintable(selector));
    }
    auto targetedSignal = signal;
    targetedSignal.insert(
        QStringLiteral("arguments"),
        QJsonObject{{QStringLiteral("signal"), 15},
                    {QStringLiteral("window"), QStringLiteral("active")}});
    object = defaultStateObject();
    object.insert(QStringLiteral("bindings"), QJsonArray{targetedSignal});
    const auto failOpenTarget = parseState(object, *catalogResult.value);
    QVERIFY(!failOpenTarget);
    QVERIFY(hasCode(failOpenTarget.errors, QStringLiteral("state.unknown-field")));

    auto dpms = binding(QStringLiteral("dpms-targeted"), {},
                        QStringLiteral("F5"), QStringLiteral("DPMS"));
    dpms.insert(QStringLiteral("actionType"), QStringLiteral("dispatcher"));
    dpms.insert(QStringLiteral("action"), QStringLiteral("dpms"));
    dpms.insert(QStringLiteral("arguments"),
                QJsonObject{{QStringLiteral("enabled"), false},
                            {QStringLiteral("monitor"),
                             QStringLiteral("DP-1")}});
    object.insert(QStringLiteral("bindings"), QJsonArray{dpms});
    const auto targetedDpms = parseState(object, *catalogResult.value);
    QVERIFY(!targetedDpms);
    QVERIFY(hasCode(targetedDpms.errors, QStringLiteral("state.unknown-field")));

    auto moveWorkspace = binding(QStringLiteral("move-workspace"), {},
                                 QStringLiteral("F4"),
                                 QStringLiteral("Move workspace"));
    moveWorkspace.insert(QStringLiteral("actionType"),
                         QStringLiteral("dispatcher"));
    moveWorkspace.insert(QStringLiteral("action"),
                         QStringLiteral("workspace.move"));
    moveWorkspace.insert(
        QStringLiteral("arguments"),
        QJsonObject{{QStringLiteral("monitor"), QStringLiteral("current")},
                    {QStringLiteral("workspace"), QStringLiteral("1")}});
    object = defaultStateObject();
    object.insert(QStringLiteral("bindings"), QJsonArray{moveWorkspace});
    const auto validWorkspaceSpec = parseState(object, *catalogResult.value);
    QVERIFY2(validWorkspaceSpec,
             qPrintable(describeErrors(validWorkspaceSpec.errors)));

    auto invalidWorkspace = moveWorkspace;
    auto workspaceArguments =
        invalidWorkspace.value(QStringLiteral("arguments")).toObject();
    workspaceArguments.insert(QStringLiteral("workspace"),
                              QStringLiteral("r[1-3]"));
    invalidWorkspace.insert(QStringLiteral("arguments"), workspaceArguments);
    object.insert(QStringLiteral("bindings"), QJsonArray{invalidWorkspace});
    const auto invalidWorkspaceSpec = parseState(object, *catalogResult.value);
    QVERIFY(!invalidWorkspaceSpec);
    QVERIFY(hasCode(invalidWorkspaceSpec.errors,
                    QStringLiteral("state.invalid-workspace-spec"))
            || hasCode(invalidWorkspaceSpec.errors,
                       QStringLiteral("state.invalid-string")));

    auto invalidMonitor = moveWorkspace;
    auto monitorArguments =
        invalidMonitor.value(QStringLiteral("arguments")).toObject();
    monitorArguments.insert(QStringLiteral("monitor"),
                            QStringLiteral("desc:"));
    invalidMonitor.insert(QStringLiteral("arguments"), monitorArguments);
    object.insert(QStringLiteral("bindings"), QJsonArray{invalidMonitor});
    const auto invalidMonitorSpec = parseState(object, *catalogResult.value);
    QVERIFY(!invalidMonitorSpec);
    QVERIFY(hasCode(invalidMonitorSpec.errors,
                    QStringLiteral("state.invalid-monitor-spec"))
            || hasCode(invalidMonitorSpec.errors,
                       QStringLiteral("state.invalid-string")));

    auto tagWindow = binding(QStringLiteral("tag-window"), {},
                             QStringLiteral("F3"),
                             QStringLiteral("Tag window"));
    tagWindow.insert(QStringLiteral("actionType"),
                     QStringLiteral("dispatcher"));
    tagWindow.insert(QStringLiteral("action"), QStringLiteral("window.tag"));
    tagWindow.insert(QStringLiteral("arguments"),
                     QJsonObject{{QStringLiteral("tag"),
                                  QStringLiteral("+browser")}});
    object = defaultStateObject();
    object.insert(QStringLiteral("bindings"), QJsonArray{tagWindow});
    QVERIFY(parseState(object, *catalogResult.value));
    for (const auto &invalidTag : {QStringLiteral("+"), QStringLiteral("*")}) {
      auto malformedTag = tagWindow;
      malformedTag.insert(QStringLiteral("arguments"),
                          QJsonObject{{QStringLiteral("tag"), invalidTag}});
      object.insert(QStringLiteral("bindings"), QJsonArray{malformedTag});
      const auto parsedTag = parseState(object, *catalogResult.value);
      QVERIFY(!parsedTag);
      QVERIFY(hasCode(parsedTag.errors,
                      QStringLiteral("state.invalid-window-tag"))
              || hasCode(parsedTag.errors,
                         QStringLiteral("state.invalid-string")));
    }

    for (const auto &excludedAction : {QStringLiteral("layout"),
                                       QStringLiteral("window.set_prop")}) {
      auto excluded = binding(QStringLiteral("excluded-action"), {},
                              QStringLiteral("F1"),
                              QStringLiteral("Excluded"));
      excluded.insert(QStringLiteral("actionType"),
                      QStringLiteral("dispatcher"));
      excluded.insert(QStringLiteral("action"), excludedAction);
      object = defaultStateObject();
      object.insert(QStringLiteral("bindings"), QJsonArray{excluded});
      const auto rejected = parseState(object, *catalogResult.value);
      QVERIFY2(!rejected, qPrintable(excludedAction));
      QVERIFY(hasCode(rejected.errors,
                      QStringLiteral("state.unknown-binding-action")));
    }
  }

  void bindingOptionsAndDeviceFiltersAreClosedAndRoundTrip() {
    const auto catalogResult = shippedCatalog();
    QVERIFY(catalogResult);
    auto record =
        binding(QStringLiteral("mouse-launcher"), {QStringLiteral("super")},
                QStringLiteral("mouse:272"), QStringLiteral("Mouse launcher"));
    auto options = record.value(QStringLiteral("options")).toObject();
    options.insert(QStringLiteral("locked"), true);
    options.insert(QStringLiteral("nonConsuming"), true);
    options.insert(QStringLiteral("autoConsuming"), true);
    options.insert(QStringLiteral("transparent"), true);
    options.insert(QStringLiteral("ignoreMods"), true);
    options.insert(QStringLiteral("dontInhibit"), true);
    options.insert(QStringLiteral("longPress"), true);
    options.insert(QStringLiteral("submapUniversal"), true);
    options.insert(QStringLiteral("click"), true);
    options.insert(QStringLiteral("release"), true);
    options.insert(QStringLiteral("allowInputCapture"), true);
    options.insert(QStringLiteral("device"),
                   QJsonObject{
                       {QStringLiteral("inclusive"), false},
                       {QStringLiteral("list"),
                        QJsonArray{
                            QStringLiteral("mouse-a"),
                            QStringLiteral("mouse-b"),
                        }},
                   });
    record.insert(QStringLiteral("options"), options);

    auto object = defaultStateObject();
    object.insert(QStringLiteral("bindings"), QJsonArray{record});
    const auto accepted = parseState(object, *catalogResult.value);
    QVERIFY2(accepted,
             qPrintable(
                 accepted.errors.isEmpty()
                     ? QStringLiteral("complete binding options were rejected")
                     : accepted.errors.constFirst().message));
    const auto &parsedOptions = accepted.value->bindings.constFirst().options;
    QVERIFY(parsedOptions.locked);
    QVERIFY(parsedOptions.nonConsuming);
    QVERIFY(parsedOptions.autoConsuming);
    QVERIFY(parsedOptions.transparent);
    QVERIFY(parsedOptions.ignoreMods);
    QVERIFY(parsedOptions.dontInhibit);
    QVERIFY(parsedOptions.longPress);
    QVERIFY(parsedOptions.submapUniversal);
    QVERIFY(parsedOptions.click);
    QVERIFY(parsedOptions.allowInputCapture);
    QVERIFY(parsedOptions.device.has_value());
    QVERIFY(!parsedOptions.device->inclusive);
    QCOMPARE(parsedOptions.device->list, QStringList({
                                             QStringLiteral("mouse-a"),
                                             QStringLiteral("mouse-b"),
                                         }));
    const auto reparsed = parseStateBytes(
        serializeDesiredState(*accepted.value), *catalogResult.value);
    QVERIFY(reparsed);
    QCOMPARE(*reparsed.value, *accepted.value);

    auto malformed = record;
    options = malformed.value(QStringLiteral("options")).toObject();
    options.insert(QStringLiteral("mouse"), true);
    malformed.insert(QStringLiteral("options"), options);
    object.insert(QStringLiteral("bindings"), QJsonArray{malformed});
    const auto noOpMouseFlag = parseState(object, *catalogResult.value);
    QVERIFY(!noOpMouseFlag);
    QVERIFY(
        hasCode(noOpMouseFlag.errors, QStringLiteral("state.unknown-field")));

    malformed = record;
    options = malformed.value(QStringLiteral("options")).toObject();
    options.insert(QStringLiteral("drag"), true);
    malformed.insert(QStringLiteral("options"), options);
    object.insert(QStringLiteral("bindings"), QJsonArray{malformed});
    const auto clickAndDrag = parseState(object, *catalogResult.value);
    QVERIFY(!clickAndDrag);
    QVERIFY(hasCode(clickAndDrag.errors,
                    QStringLiteral("state.incompatible-bind-options")));

    malformed = record;
    options = malformed.value(QStringLiteral("options")).toObject();
    options.insert(QStringLiteral("release"), false);
    malformed.insert(QStringLiteral("options"), options);
    object.insert(QStringLiteral("bindings"), QJsonArray{malformed});
    const auto clickWithoutRelease = parseState(object, *catalogResult.value);
    QVERIFY(!clickWithoutRelease);
    QVERIFY(hasCode(clickWithoutRelease.errors,
                    QStringLiteral("state.incompatible-bind-options")));

    malformed = record;
    options = malformed.value(QStringLiteral("options")).toObject();
    options.insert(QStringLiteral("click"), false);
    options.insert(QStringLiteral("repeating"), true);
    options.insert(QStringLiteral("release"), true);
    malformed.insert(QStringLiteral("options"), options);
    object.insert(QStringLiteral("bindings"), QJsonArray{malformed});
    const auto repeatAndRelease = parseState(object, *catalogResult.value);
    QVERIFY(!repeatAndRelease);
    QVERIFY(hasCode(repeatAndRelease.errors,
                    QStringLiteral("state.incompatible-bind-options")));

    malformed = record;
    options = malformed.value(QStringLiteral("options")).toObject();
    options.insert(QStringLiteral("device"),
                   QJsonObject{
                       {QStringLiteral("inclusive"), true},
                       {QStringLiteral("list"),
                        QJsonArray{
                            QStringLiteral("mouse-a"),
                            QStringLiteral("mouse-a"),
                        }},
                   });
    malformed.insert(QStringLiteral("options"), options);
    object.insert(QStringLiteral("bindings"), QJsonArray{malformed});
    const auto duplicateDevice = parseState(object, *catalogResult.value);
    QVERIFY(!duplicateDevice);
    QVERIFY(hasCode(duplicateDevice.errors,
                    QStringLiteral("state.duplicate-device")));
  }

  void curvesAreTypedAndAnimationsResolveByName() {
    const auto catalogResult = shippedCatalog();
    QVERIFY(catalogResult);
    const QJsonObject bezier{
        {QStringLiteral("id"), QStringLiteral("curve-bezier")},
        {QStringLiteral("name"), QStringLiteral("ease-custom")},
        {QStringLiteral("type"), QStringLiteral("bezier")},
        {QStringLiteral("points"),
         QJsonArray{
             QJsonArray{0.2, 0.0},
             QJsonArray{0.8, 1.0},
         }},
    };
    const QJsonObject spring{
        {QStringLiteral("id"), QStringLiteral("curve-spring")},
        {QStringLiteral("name"), QStringLiteral("spring-custom")},
        {QStringLiteral("type"), QStringLiteral("spring")},
        {QStringLiteral("stiffness"), 100.0},
        {QStringLiteral("dampening"), 12.0},
        {QStringLiteral("mass"), 1.5},
    };
    const QJsonObject animation{
        {QStringLiteral("id"), QStringLiteral("animation-windows")},
        {QStringLiteral("name"), QStringLiteral("windows")},
        {QStringLiteral("enabled"), true},
        {QStringLiteral("speed"), 6.0},
        {QStringLiteral("curve"), QStringLiteral("spring-custom")},
        {QStringLiteral("style"), QStringLiteral("slide")},
    };
    auto object = defaultStateObject();
    object.insert(QStringLiteral("curves"), QJsonArray{bezier, spring});
    object.insert(QStringLiteral("animations"), QJsonArray{animation});
    const auto accepted = parseState(object, *catalogResult.value);
    QVERIFY2(accepted,
             qPrintable(accepted.errors.isEmpty()
                            ? QStringLiteral("valid curves were rejected")
                            : accepted.errors.constFirst().message));
    QCOMPARE(accepted.value->curves.size(), 2);
    QVERIFY(std::holds_alternative<BezierCurveParameters>(
        accepted.value->curves.at(0).parameters));
    QVERIFY(std::holds_alternative<SpringCurveParameters>(
        accepted.value->curves.at(1).parameters));
    const auto reparsed = parseStateBytes(
        serializeDesiredState(*accepted.value), *catalogResult.value);
    QVERIFY(reparsed);
    QCOMPARE(*reparsed.value, *accepted.value);

    auto malformedBezier = bezier;
    malformedBezier.insert(
        QStringLiteral("points"),
        QJsonArray{QJsonArray{0.2, 0.0}, QJsonArray{2.1, 1.0}});
    object = defaultStateObject();
    object.insert(QStringLiteral("curves"), QJsonArray{malformedBezier});
    const auto badBezier = parseState(object, *catalogResult.value);
    QVERIFY(!badBezier);
    QVERIFY(hasCode(badBezier.errors, QStringLiteral("state.invalid-curve")));

    auto malformedSpring = spring;
    malformedSpring.insert(QStringLiteral("mass"), 0.5);
    object.insert(QStringLiteral("curves"), QJsonArray{malformedSpring});
    const auto badSpring = parseState(object, *catalogResult.value);
    QVERIFY(!badSpring);
    QVERIFY(hasCode(badSpring.errors, QStringLiteral("state.invalid-curve")));

    malformedSpring = spring;
    malformedSpring.insert(QStringLiteral("stiffness"), 1000000.1);
    object = defaultStateObject();
    object.insert(QStringLiteral("curves"), QJsonArray{malformedSpring});
    const auto excessiveSpring = parseState(object, *catalogResult.value);
    QVERIFY(!excessiveSpring);
    QVERIFY(hasCode(excessiveSpring.errors,
                    QStringLiteral("state.number-out-of-range")));

    auto boundarySpring = spring;
    boundarySpring.insert(QStringLiteral("stiffness"), 1000000.0);
    object = defaultStateObject();
    object.insert(QStringLiteral("curves"), QJsonArray{boundarySpring});
    const auto springBoundary = parseState(object, *catalogResult.value);
    QVERIFY(springBoundary);

    object = defaultStateObject();
    object.insert(QStringLiteral("curves"), QJsonArray{bezier});
    auto duplicateName = spring;
    duplicateName.insert(QStringLiteral("name"), QStringLiteral("ease-custom"));
    object.insert(QStringLiteral("curves"), QJsonArray{bezier, duplicateName});
    const auto duplicate = parseState(object, *catalogResult.value);
    QVERIFY(!duplicate);
    QVERIFY(hasCode(duplicate.errors, QStringLiteral("state.duplicate-name")));

    auto unknownAnimation = animation;
    unknownAnimation.insert(QStringLiteral("curve"),
                            QStringLiteral("not-declared"));
    object = defaultStateObject();
    object.insert(QStringLiteral("animations"), QJsonArray{unknownAnimation});
    const auto unknownCurve = parseState(object, *catalogResult.value);
    QVERIFY(!unknownCurve);
    QVERIFY(
        hasCode(unknownCurve.errors, QStringLiteral("state.unknown-curve")));

    auto invalidAnimation = animation;
    invalidAnimation.insert(QStringLiteral("name"),
                            QStringLiteral("madeUpAnimation"));
    object = defaultStateObject();
    object.insert(QStringLiteral("curves"), QJsonArray{spring});
    object.insert(QStringLiteral("animations"), QJsonArray{invalidAnimation});
    const auto unknownLeaf = parseState(object, *catalogResult.value);
    QVERIFY(!unknownLeaf);
    QVERIFY(hasCode(unknownLeaf.errors,
                    QStringLiteral("state.invalid-animation-name")));

    invalidAnimation = animation;
    invalidAnimation.insert(QStringLiteral("enabled"), false);
    invalidAnimation.insert(QStringLiteral("speed"), 0.0);
    object.insert(QStringLiteral("animations"), QJsonArray{invalidAnimation});
    const auto zeroDisabledSpeed = parseState(object, *catalogResult.value);
    QVERIFY(!zeroDisabledSpeed);
    QVERIFY(hasCode(zeroDisabledSpeed.errors,
                    QStringLiteral("state.animation-speed")));

    auto invalidStyle = animation;
    invalidStyle.insert(QStringLiteral("style"), QStringLiteral("bogus"));
    object = defaultStateObject();
    object.insert(QStringLiteral("curves"), QJsonArray{spring});
    object.insert(QStringLiteral("animations"), QJsonArray{invalidStyle});
    const auto invalidWindowStyle = parseState(object, *catalogResult.value);
    QVERIFY(!invalidWindowStyle);
    QVERIFY(hasCode(invalidWindowStyle.errors,
                    QStringLiteral("state.invalid-animation-style")));

    invalidStyle = animation;
    invalidStyle.insert(QStringLiteral("name"), QStringLiteral("global"));
    invalidStyle.insert(QStringLiteral("style"), QStringLiteral("slide"));
    object.insert(QStringLiteral("animations"), QJsonArray{invalidStyle});
    const auto globalWithStyle = parseState(object, *catalogResult.value);
    QVERIFY(!globalWithStyle);
    QVERIFY(hasCode(globalWithStyle.errors,
                    QStringLiteral("state.invalid-animation-style")));

    auto globalAnimation = invalidStyle;
    globalAnimation.insert(QStringLiteral("style"), QString());
    object.insert(QStringLiteral("animations"), QJsonArray{globalAnimation});
    const auto validGlobalStyle = parseState(object, *catalogResult.value);
    QVERIFY(validGlobalStyle);

    auto angleAnimation = animation;
    angleAnimation.insert(QStringLiteral("name"),
                          QStringLiteral("borderangle"));
    angleAnimation.insert(QStringLiteral("style"), QStringLiteral("loop"));
    object.insert(QStringLiteral("animations"), QJsonArray{angleAnimation});
    const auto validAngleStyle = parseState(object, *catalogResult.value);
    QVERIFY(validAngleStyle);
  }

  void gesturesUseTheClosedTypedActionUnion() {
    const auto catalogResult = shippedCatalog();
    QVERIFY(catalogResult);
    QJsonObject gesture{
        {QStringLiteral("id"), QStringLiteral("gesture-special")},
        {QStringLiteral("fingers"), 4},
        {QStringLiteral("direction"), QStringLiteral("left")},
        {QStringLiteral("modifiers"), QJsonArray{QStringLiteral("super")}},
        {QStringLiteral("scale"), 1.25},
        {QStringLiteral("disableInhibit"), false},
        {QStringLiteral("action"),
         QJsonObject{
             {QStringLiteral("type"), QStringLiteral("special")},
             {QStringLiteral("workspace"), QStringLiteral("magic")},
         }},
    };
    auto object = defaultStateObject();
    object.insert(QStringLiteral("gestures"), QJsonArray{gesture});
    const auto accepted = parseState(object, *catalogResult.value);
    QVERIFY(accepted);
    QCOMPARE(accepted.value->gestures.constFirst().action.id,
             QStringLiteral("special"));
    QCOMPARE(accepted.value->gestures.constFirst().action.payload.value(
                 QStringLiteral("workspace")),
             QJsonValue(QStringLiteral("magic")));

    auto malformed = gesture;
    malformed.insert(QStringLiteral("action"),
                     QJsonObject{
                         {QStringLiteral("type"), QStringLiteral("callback")},
                         {QStringLiteral("command"), QStringLiteral("whoami")},
                     });
    object.insert(QStringLiteral("gestures"), QJsonArray{malformed});
    const auto callback = parseState(object, *catalogResult.value);
    QVERIFY(!callback);
    QVERIFY(hasCode(callback.errors,
                    QStringLiteral("state.unknown-gesture-action")));

    malformed = gesture;
    malformed.insert(QStringLiteral("fingers"), 1);
    object.insert(QStringLiteral("gestures"), QJsonArray{malformed});
    const auto fingers = parseState(object, *catalogResult.value);
    QVERIFY(!fingers);
    QVERIFY(
        hasCode(fingers.errors, QStringLiteral("state.integer-out-of-range")));

    malformed = gesture;
    malformed.insert(QStringLiteral("action"),
                     QJsonObject{
                         {QStringLiteral("type"), QStringLiteral("workspace")},
                         {QStringLiteral("command"), QStringLiteral("whoami")},
                     });
    object.insert(QStringLiteral("gestures"), QJsonArray{malformed});
    const auto extra = parseState(object, *catalogResult.value);
    QVERIFY(!extra);
    QVERIFY(hasCode(extra.errors, QStringLiteral("state.unknown-field")));

    auto typedGesture = gesture;
    typedGesture.insert(
        QStringLiteral("action"),
        QJsonObject{
            {QStringLiteral("type"), QStringLiteral("cursorZoom")},
            {QStringLiteral("zoomLevel"), 1.5},
            {QStringLiteral("mode"), QStringLiteral("mult")},
        });
    object = defaultStateObject();
    object.insert(QStringLiteral("gestures"), QJsonArray{typedGesture});
    const auto cursorZoom = parseState(object, *catalogResult.value);
    QVERIFY2(cursorZoom, qPrintable(describeErrors(cursorZoom.errors)));

    auto typedAction = typedGesture.value(QStringLiteral("action")).toObject();
    typedAction.insert(QStringLiteral("mode"), QStringLiteral("bogus"));
    typedGesture.insert(QStringLiteral("action"), typedAction);
    object.insert(QStringLiteral("gestures"), QJsonArray{typedGesture});
    const auto invalidZoomMode = parseState(object, *catalogResult.value);
    QVERIFY(!invalidZoomMode);
    QVERIFY(hasCode(invalidZoomMode.errors,
                    QStringLiteral("state.enum-mismatch")));

    typedAction.insert(QStringLiteral("mode"), QStringLiteral("live"));
    typedAction.insert(QStringLiteral("zoomLevel"), 100.01);
    typedGesture.insert(QStringLiteral("action"), typedAction);
    object.insert(QStringLiteral("gestures"), QJsonArray{typedGesture});
    const auto invalidZoomLevel = parseState(object, *catalogResult.value);
    QVERIFY(!invalidZoomLevel);
    QVERIFY(hasCode(invalidZoomLevel.errors,
                    QStringLiteral("state.value-out-of-range")));

    typedGesture = gesture;
    typedGesture.insert(
        QStringLiteral("action"),
        QJsonObject{
            {QStringLiteral("type"), QStringLiteral("float")},
            {QStringLiteral("mode"), QStringLiteral("toggle")},
        });
    object.insert(QStringLiteral("gestures"), QJsonArray{typedGesture});
    const auto validFloatToggle = parseState(object, *catalogResult.value);
    QVERIFY(validFloatToggle);

    typedGesture.insert(
        QStringLiteral("action"),
        QJsonObject{
            {QStringLiteral("type"), QStringLiteral("fullscreen")},
            {QStringLiteral("mode"), QStringLiteral("toggle")},
        });
    object.insert(QStringLiteral("gestures"), QJsonArray{typedGesture});
    const auto invalidFullscreenMode =
        parseState(object, *catalogResult.value);
    QVERIFY(!invalidFullscreenMode);
    QVERIFY2(hasCode(invalidFullscreenMode.errors,
                     QStringLiteral("state.schema-one-of")),
             qPrintable(describeErrors(invalidFullscreenMode.errors)));

    auto horizontal = gesture;
    horizontal.insert(QStringLiteral("id"), QStringLiteral("horizontal"));
    horizontal.insert(QStringLiteral("direction"),
                      QStringLiteral("horizontal"));
    horizontal.insert(QStringLiteral("action"),
                      QJsonObject{{QStringLiteral("type"),
                                   QStringLiteral("close")}});
    auto left = horizontal;
    left.insert(QStringLiteral("id"), QStringLiteral("left"));
    left.insert(QStringLiteral("direction"), QStringLiteral("left"));
    object = defaultStateObject();
    object.insert(QStringLiteral("gestures"), QJsonArray{horizontal, left});
    const auto shadowed = parseState(object, *catalogResult.value);
    QVERIFY(!shadowed);
    QVERIFY(hasCode(shadowed.errors,
                    QStringLiteral("state.shadowed-gesture")));

    auto unset = left;
    unset.insert(QStringLiteral("id"), QStringLiteral("unset"));
    unset.insert(QStringLiteral("action"),
                 QJsonObject{{QStringLiteral("type"),
                              QStringLiteral("unset")}});
    object = defaultStateObject();
    object.insert(QStringLiteral("gestures"), QJsonArray{unset});
    const auto unmatchedUnset = parseState(object, *catalogResult.value);
    QVERIFY(!unmatchedUnset);
    QVERIFY(hasCode(unmatchedUnset.errors,
                    QStringLiteral("state.unmatched-gesture-unset")));

    auto leftAgain = left;
    leftAgain.insert(QStringLiteral("id"), QStringLiteral("left-again"));
    object.insert(QStringLiteral("gestures"),
                  QJsonArray{left, unset, leftAgain});
    const auto removedThenAdded = parseState(object, *catalogResult.value);
    QVERIFY2(removedThenAdded,
             qPrintable(describeErrors(removedThenAdded.errors)));
  }

  void nestedDeviceWorkspaceAndRuleMapsMatchTheTaggedSchema() {
    const auto catalogResult = shippedCatalog();
    QVERIFY(catalogResult);
    const QJsonObject device{
        {QStringLiteral("id"), QStringLiteral("keyboard-main")},
        {QStringLiteral("selector"), QStringLiteral("name:Main Keyboard")},
        {QStringLiteral("kind"), QStringLiteral("keyboard")},
        {QStringLiteral("enabled"), true},
        {QStringLiteral("overrides"),
         QJsonObject{
             {QStringLiteral("sensitivity"), 0.25},
             {QStringLiteral("kb_layout"), QStringLiteral("us")},
             {QStringLiteral("repeat_rate"), 40},
         }},
    };
    const QJsonObject workspace{
        {QStringLiteral("id"), QStringLiteral("workspace-one")},
        {QStringLiteral("selector"), QStringLiteral("1")},
        {QStringLiteral("enabled"), true},
        {QStringLiteral("monitor"), QString()},
        {QStringLiteral("persistent"), true},
        {QStringLiteral("isDefault"), false},
        {QStringLiteral("layout"), QStringLiteral("dwindle")},
        {QStringLiteral("overrides"),
         QJsonObject{
             {QStringLiteral("gaps_in"), QJsonArray{4, 4, 4, 4}},
             {QStringLiteral("layout_opts"),
              QJsonObject{
                  {QStringLiteral("orientation"), QStringLiteral("center")},
                  {QStringLiteral("direction"), QStringLiteral("left")},
              }},
             {QStringLiteral("animation"), QStringLiteral("slide")},
         }},
    };
    const QJsonObject windowRule{
        {QStringLiteral("id"), QStringLiteral("window-browser")},
        {QStringLiteral("name"), QStringLiteral("Browser")},
        {QStringLiteral("enabled"), true},
        {QStringLiteral("match"),
         QJsonObject{
             {QStringLiteral("class"), QStringLiteral("^(firefox)$")},
             {QStringLiteral("float"), false},
             {QStringLiteral("pin"), false},
             {QStringLiteral("fullscreen_state_internal"), 0},
             {QStringLiteral("fullscreen_state_client"), 2},
         }},
        {QStringLiteral("effects"),
         QJsonObject{
             {QStringLiteral("rounding"), 4},
             {QStringLiteral("tonemap"), QStringLiteral("on")},
             {QStringLiteral("fullscreen_state"),
              QJsonObject{{QStringLiteral("internal"), 2},
                          {QStringLiteral("client"), 0}}},
             {QStringLiteral("opacity"),
              QJsonObject{{QStringLiteral("active"), 0.9},
                          {QStringLiteral("inactive"), 0.8},
                          {QStringLiteral("overrideActive"), true},
                          {QStringLiteral("overrideInactive"), false},
                          {QStringLiteral("overrideFullscreen"), false}}},
             {QStringLiteral("suppress_event"),
              QJsonArray{QStringLiteral("fullscreen"),
                         QStringLiteral("activate")}},
             {QStringLiteral("content"), QStringLiteral("video")},
             {QStringLiteral("no_close_for"), 500},
             {QStringLiteral("move"), QJsonArray{1000000.0, -1000000.0}},
             {QStringLiteral("monitor"),
              QJsonObject{{QStringLiteral("target"),
                           QStringLiteral("DP-1")},
                          {QStringLiteral("silent"), false}}},
             {QStringLiteral("workspace"),
              QJsonObject{{QStringLiteral("target"), QStringLiteral("1")},
                          {QStringLiteral("silent"), true}}},
             {QStringLiteral("border_color"),
              QJsonObject{
                  {QStringLiteral("colors"),
                   QJsonArray{
                       QStringLiteral("0xFF00FFFF"),
                   }},
                  {QStringLiteral("angle"), 90.0},
              }},
         }},
    };
    const QJsonObject layerRule{
        {QStringLiteral("id"), QStringLiteral("layer-panel")},
        {QStringLiteral("name"), QStringLiteral("Panel")},
        {QStringLiteral("enabled"), true},
        {QStringLiteral("match"),
         QJsonObject{
             {QStringLiteral("namespace"), QStringLiteral("^panel$")},
         }},
        {QStringLiteral("effects"),
         QJsonObject{
             {QStringLiteral("ignore_alpha"), 0.5},
             {QStringLiteral("above_lock"), 1},
         }},
    };

    auto object = defaultStateObject();
    object.insert(QStringLiteral("devices"), QJsonArray{device});
    object.insert(QStringLiteral("workspaceRules"), QJsonArray{workspace});
    object.insert(QStringLiteral("windowRules"), QJsonArray{windowRule});
    object.insert(QStringLiteral("layerRules"), QJsonArray{layerRule});
    const auto accepted = parseState(object, *catalogResult.value);
    QVERIFY2(
        accepted,
        qPrintable(accepted.errors.isEmpty()
                       ? QStringLiteral(
                             "schema-authority nested records were rejected")
                       : accepted.errors.constFirst().message));
    const auto reparsed = parseStateBytes(
        serializeDesiredState(*accepted.value), *catalogResult.value);
    QVERIFY(reparsed);
    QCOMPARE(*reparsed.value, *accepted.value);

    auto malformedDevice = device;
    auto nested = malformedDevice.value(QStringLiteral("overrides")).toObject();
    nested.insert(QStringLiteral("enabled"), false);
    malformedDevice.insert(QStringLiteral("overrides"), nested);
    object = defaultStateObject();
    object.insert(QStringLiteral("devices"), QJsonArray{malformedDevice});
    const auto nestedEnabled = parseState(object, *catalogResult.value);
    QVERIFY(!nestedEnabled);
    QVERIFY(
        hasCode(nestedEnabled.errors, QStringLiteral("state.unknown-field")));

    malformedDevice = device;
    malformedDevice.insert(QStringLiteral("kind"), QStringLiteral("spaceship"));
    object.insert(QStringLiteral("devices"), QJsonArray{malformedDevice});
    const auto deviceKind = parseState(object, *catalogResult.value);
    QVERIFY(!deviceKind);
    QVERIFY(hasCode(deviceKind.errors,
                    QStringLiteral("state.invalid-device-kind")));

    malformedDevice = device;
    nested = malformedDevice.value(QStringLiteral("overrides")).toObject();
    nested.insert(QStringLiteral("sensitivity"), 2.0);
    malformedDevice.insert(QStringLiteral("overrides"), nested);
    object.insert(QStringLiteral("devices"), QJsonArray{malformedDevice});
    const auto deviceRange = parseState(object, *catalogResult.value);
    QVERIFY(!deviceRange);
    QVERIFY(hasCode(deviceRange.errors,
                    QStringLiteral("state.nested-value-range")));

    for (const auto &removedField : {QStringLiteral("output"),
                                     QStringLiteral("scroll_points"),
                                     QStringLiteral("tags")}) {
      malformedDevice = device;
      nested = malformedDevice.value(QStringLiteral("overrides")).toObject();
      nested.insert(removedField, QStringLiteral("opaque grammar"));
      malformedDevice.insert(QStringLiteral("overrides"), nested);
      object = defaultStateObject();
      object.insert(QStringLiteral("devices"), QJsonArray{malformedDevice});
      const auto removed = parseState(object, *catalogResult.value);
      QVERIFY2(!removed, qPrintable(removedField));
      QVERIFY2(hasCode(removed.errors, QStringLiteral("state.unknown-field")),
               qPrintable(removedField));
    }
    malformedDevice = device;
    nested = malformedDevice.value(QStringLiteral("overrides")).toObject();
    nested.insert(QStringLiteral("accel_profile"),
                  QStringLiteral("custom"));
    malformedDevice.insert(QStringLiteral("overrides"), nested);
    object.insert(QStringLiteral("devices"), QJsonArray{malformedDevice});
    const auto customAcceleration = parseState(object, *catalogResult.value);
    QVERIFY(!customAcceleration);
    QVERIFY(hasCode(customAcceleration.errors,
                    QStringLiteral("state.nested-value-choice")));

    for (const auto &[field, value] :
         std::array<std::pair<QString, int>, 2>{
             std::pair{QStringLiteral("fullscreen_state_internal"), -1},
             std::pair{QStringLiteral("fullscreen_state_client"), 3},
         }) {
      auto malformedWindow = windowRule;
      auto match = malformedWindow.value(QStringLiteral("match")).toObject();
      match.insert(field, value);
      malformedWindow.insert(QStringLiteral("match"), match);
      object = defaultStateObject();
      object.insert(QStringLiteral("windowRules"),
                    QJsonArray{malformedWindow});
      const auto invalidFullscreenMode =
          parseState(object, *catalogResult.value);
      QVERIFY(!invalidFullscreenMode);
      QVERIFY(hasCode(invalidFullscreenMode.errors,
                      QStringLiteral("state.nested-value-range")));
    }

    auto malformedWorkspace = workspace;
    nested = malformedWorkspace.value(QStringLiteral("overrides")).toObject();
    nested.insert(QStringLiteral("on_created_empty"),
                  QStringLiteral("rm -rf /"));
    malformedWorkspace.insert(QStringLiteral("overrides"), nested);
    object = defaultStateObject();
    object.insert(QStringLiteral("workspaceRules"),
                  QJsonArray{malformedWorkspace});
    const auto command = parseState(object, *catalogResult.value);
    QVERIFY(!command);
    QVERIFY(hasCode(command.errors, QStringLiteral("state.unknown-field")));

    malformedWorkspace = workspace;
    nested = malformedWorkspace.value(QStringLiteral("overrides")).toObject();
    nested.insert(QStringLiteral("layout_opts"),
                  QJsonObject{{QStringLiteral("mystery"), true}});
    malformedWorkspace.insert(QStringLiteral("overrides"), nested);
    object.insert(QStringLiteral("workspaceRules"),
                  QJsonArray{malformedWorkspace});
    const auto layoutOptions = parseState(object, *catalogResult.value);
    QVERIFY(!layoutOptions);
    QVERIFY(hasCode(layoutOptions.errors,
                    QStringLiteral("state.nested-value-type")));

    malformedWorkspace = workspace;
    nested = malformedWorkspace.value(QStringLiteral("overrides")).toObject();
    nested.insert(QStringLiteral("animation"), QStringLiteral("bogus"));
    malformedWorkspace.insert(QStringLiteral("overrides"), nested);
    object.insert(QStringLiteral("workspaceRules"),
                  QJsonArray{malformedWorkspace});
    const auto workspaceAnimation = parseState(object, *catalogResult.value);
    QVERIFY(!workspaceAnimation);
    QVERIFY(hasCode(workspaceAnimation.errors,
                    QStringLiteral("state.invalid-animation-style")));

    for (const auto &monitorSelector : {
             QStringLiteral("current"), QStringLiteral("+1"),
             QStringLiteral("0"), QStringLiteral("desc:")}) {
      malformedWorkspace = workspace;
      malformedWorkspace.insert(QStringLiteral("monitor"), monitorSelector);
      object.insert(QStringLiteral("workspaceRules"),
                    QJsonArray{malformedWorkspace});
      const auto workspaceMonitor = parseState(object, *catalogResult.value);
      QVERIFY2(!workspaceMonitor, qPrintable(monitorSelector));
      QVERIFY2(hasCode(
                   workspaceMonitor.errors,
                   QStringLiteral("state.invalid-static-monitor-selector")),
               qPrintable(monitorSelector));
    }

    for (const auto &legacyKey : {
             QStringLiteral("floating"),
             QStringLiteral("pinned"),
             QStringLiteral("initial_namespace"),
         }) {
      auto malformedWindow = windowRule;
      nested = malformedWindow.value(QStringLiteral("match")).toObject();
      nested.insert(legacyKey, true);
      malformedWindow.insert(QStringLiteral("match"), nested);
      object = defaultStateObject();
      object.insert(QStringLiteral("windowRules"), QJsonArray{malformedWindow});
      const auto legacy = parseState(object, *catalogResult.value);
      QVERIFY2(!legacy, qPrintable(legacyKey));
      QVERIFY2(hasCode(legacy.errors, QStringLiteral("state.unknown-field")),
               qPrintable(legacyKey));
    }

    auto malformedWindow = windowRule;
    nested = malformedWindow.value(QStringLiteral("effects")).toObject();
    nested.insert(QStringLiteral("rounding"), 21);
    malformedWindow.insert(QStringLiteral("effects"), nested);
    object = defaultStateObject();
    object.insert(QStringLiteral("windowRules"), QJsonArray{malformedWindow});
    const auto windowRange = parseState(object, *catalogResult.value);
    QVERIFY(!windowRange);
    QVERIFY(hasCode(windowRange.errors,
                    QStringLiteral("state.nested-value-range")));

    for (const auto &invalidMatcher : {QStringLiteral("["),
                                       QStringLiteral("negative:[")}) {
      auto invalidRegexRule = windowRule;
      auto match = invalidRegexRule.value(QStringLiteral("match")).toObject();
      match.insert(QStringLiteral("class"), invalidMatcher);
      invalidRegexRule.insert(QStringLiteral("match"), match);
      object = defaultStateObject();
      object.insert(QStringLiteral("windowRules"),
                    QJsonArray{invalidRegexRule});
      const auto invalidRegex = parseState(object, *catalogResult.value);
      QVERIFY(!invalidRegex);
      QVERIFY(hasCode(invalidRegex.errors,
                      QStringLiteral("state.invalid-regex")));
    }

    const std::array<std::pair<QString, QJsonValue>, 5> invalidEffects{
        std::pair{QStringLiteral("fullscreen_state"),
                  QJsonValue(QStringLiteral("2,0"))},
        std::pair{QStringLiteral("opacity"),
                  QJsonValue(QJsonObject{
                      {QStringLiteral("active"), 1.1},
                      {QStringLiteral("overrideActive"), false},
                      {QStringLiteral("overrideInactive"), false},
                      {QStringLiteral("overrideFullscreen"), false}})},
        std::pair{QStringLiteral("content"),
                  QJsonValue(QStringLiteral("document"))},
        std::pair{QStringLiteral("suppress_event"),
                  QJsonValue(QJsonArray{QStringLiteral("fullscreen"),
                                        QStringLiteral("fullscreen")})},
        std::pair{QStringLiteral("no_close_for"),
                  QJsonValue(2147483648.0)},
    };
    for (const auto &[field, value] : invalidEffects) {
      auto invalidEffectRule = windowRule;
      auto effects = invalidEffectRule.value(QStringLiteral("effects"))
                         .toObject();
      effects.insert(field, value);
      invalidEffectRule.insert(QStringLiteral("effects"), effects);
      object = defaultStateObject();
      object.insert(QStringLiteral("windowRules"),
                    QJsonArray{invalidEffectRule});
      const auto invalidEffect = parseState(object, *catalogResult.value);
      QVERIFY2(!invalidEffect, qPrintable(field));
      QVERIFY2(hasCode(invalidEffect.errors,
                       QStringLiteral("state.nested-value-type"))
                   || hasCode(invalidEffect.errors,
                              QStringLiteral("state.nested-value-range"))
                   || hasCode(invalidEffect.errors,
                              QStringLiteral("state.nested-value-choice")),
               qPrintable(field));
    }

    for (const auto &[field, value, code] :
         std::array<std::tuple<QString, QJsonValue, QString>, 4>{
             std::tuple{QStringLiteral("group"),
                        QJsonValue(QStringLiteral("set")),
                        QStringLiteral("state.unknown-field")},
             std::tuple{QStringLiteral("monitor"),
                        QJsonValue(QStringLiteral("DP-1")),
                        QStringLiteral("state.nested-value-type")},
             std::tuple{QStringLiteral("tag"), QJsonValue(QStringLiteral("+")),
                        QStringLiteral("state.invalid-window-tag")},
             std::tuple{QStringLiteral("animation"),
                        QJsonValue(QStringLiteral("bogus")),
                        QStringLiteral("state.invalid-animation-style")},
         }) {
      auto invalidEffectRule = windowRule;
      auto effects = invalidEffectRule.value(QStringLiteral("effects"))
                         .toObject();
      effects.insert(field, value);
      invalidEffectRule.insert(QStringLiteral("effects"), effects);
      object = defaultStateObject();
      object.insert(QStringLiteral("windowRules"),
                    QJsonArray{invalidEffectRule});
      const auto invalidEffect = parseState(object, *catalogResult.value);
      QVERIFY2(!invalidEffect, qPrintable(field));
      QVERIFY2(hasCode(invalidEffect.errors, code), qPrintable(field));
    }

    auto invalidWorkspaceEffect = windowRule;
    auto workspaceEffects =
        invalidWorkspaceEffect.value(QStringLiteral("effects")).toObject();
    workspaceEffects.insert(
        QStringLiteral("workspace"),
        QJsonObject{{QStringLiteral("target"), QStringLiteral("r[1-3]")},
                    {QStringLiteral("silent"), false}});
    invalidWorkspaceEffect.insert(QStringLiteral("effects"), workspaceEffects);
    object = defaultStateObject();
    object.insert(QStringLiteral("windowRules"),
                  QJsonArray{invalidWorkspaceEffect});
    const auto invalidWorkspaceTarget =
        parseState(object, *catalogResult.value);
    QVERIFY(!invalidWorkspaceTarget);
    QVERIFY(hasCode(invalidWorkspaceTarget.errors,
                    QStringLiteral("state.invalid-workspace-spec")));

    auto invalidWorkspaceMatch = windowRule;
    auto workspaceMatch =
        invalidWorkspaceMatch.value(QStringLiteral("match")).toObject();
    workspaceMatch.insert(QStringLiteral("workspace"),
                          QStringLiteral("r[1-3]"));
    invalidWorkspaceMatch.insert(QStringLiteral("match"), workspaceMatch);
    object.insert(QStringLiteral("windowRules"),
                  QJsonArray{invalidWorkspaceMatch});
    const auto invalidWorkspaceMatcher =
        parseState(object, *catalogResult.value);
    QVERIFY(!invalidWorkspaceMatcher);
    QVERIFY(hasCode(invalidWorkspaceMatcher.errors,
                    QStringLiteral("state.invalid-workspace-selector")));

    auto malformedLayer = layerRule;
    nested = malformedLayer.value(QStringLiteral("match")).toObject();
    nested.insert(QStringLiteral("initial_namespace"), QStringLiteral("x"));
    malformedLayer.insert(QStringLiteral("match"), nested);
    object = defaultStateObject();
    object.insert(QStringLiteral("layerRules"), QJsonArray{malformedLayer});
    const auto layerMatcher = parseState(object, *catalogResult.value);
    QVERIFY(!layerMatcher);
    QVERIFY(
        hasCode(layerMatcher.errors, QStringLiteral("state.unknown-field")));

    malformedLayer = layerRule;
    malformedLayer.insert(
        QStringLiteral("match"),
        QJsonObject{{QStringLiteral("namespace"), QStringLiteral("[")}});
    object = defaultStateObject();
    object.insert(QStringLiteral("layerRules"), QJsonArray{malformedLayer});
    const auto invalidLayerRegex = parseState(object, *catalogResult.value);
    QVERIFY(!invalidLayerRegex);
    QVERIFY(hasCode(invalidLayerRegex.errors,
                    QStringLiteral("state.invalid-regex")));

    auto invalidLayerAnimation = layerRule;
    auto layerEffects =
        invalidLayerAnimation.value(QStringLiteral("effects")).toObject();
    layerEffects.insert(QStringLiteral("animation"),
                        QStringLiteral("bogus"));
    invalidLayerAnimation.insert(QStringLiteral("effects"), layerEffects);
    object.insert(QStringLiteral("layerRules"),
                  QJsonArray{invalidLayerAnimation});
    const auto badLayerAnimation = parseState(object, *catalogResult.value);
    QVERIFY(!badLayerAnimation);
    QVERIFY(hasCode(badLayerAnimation.errors,
                    QStringLiteral("state.invalid-animation-style")));
  }

  void workspaceRulesCoverEveryManagedOverrideAndFailClosed() {
    const auto catalogResult = shippedCatalog();
    QVERIFY(catalogResult);
    constexpr auto safeInteger = 9007199254740991.0;
    const QJsonObject overrides{
        {QStringLiteral("gaps_in"),
         QJsonArray{-safeInteger, 0, 1, safeInteger}},
        {QStringLiteral("gaps_out"), QJsonArray{1, 2, 3, 4}},
        {QStringLiteral("float_gaps"), QJsonArray{4, 3, 2, 1}},
        {QStringLiteral("border_size"), safeInteger},
        {QStringLiteral("no_border"), true},
        {QStringLiteral("no_rounding"), false},
        {QStringLiteral("decorate"), true},
        {QStringLiteral("no_shadow"), false},
        {QStringLiteral("default_name"), QString(256, QLatin1Char('n'))},
        {QStringLiteral("animation"),
         QStringLiteral("slidefadevert left 37%")},
        {QStringLiteral("layout_opts"),
         QJsonObject{
             {QStringLiteral("orientation"), QStringLiteral("center")},
             {QStringLiteral("direction"), QStringLiteral("up")},
         }},
    };
    const QJsonObject workspace{
        {QStringLiteral("id"), QStringLiteral("workspace-complete")},
        {QStringLiteral("selector"), QStringLiteral("special:music")},
        {QStringLiteral("enabled"), false},
        {QStringLiteral("monitor"), QStringLiteral("desc:Studio Display")},
        {QStringLiteral("persistent"), true},
        {QStringLiteral("isDefault"), true},
        {QStringLiteral("layout"), QStringLiteral("scrolling")},
        {QStringLiteral("overrides"), overrides},
    };
    auto object = defaultStateObject();
    object.insert(QStringLiteral("workspaceRules"), QJsonArray{workspace});
    const auto accepted = parseState(object, *catalogResult.value);
    QVERIFY2(accepted, qPrintable(describeErrors(accepted.errors)));
    const auto reparsed = parseStateBytes(
        serializeDesiredState(*accepted.value), *catalogResult.value
    );
    QVERIFY2(reparsed, qPrintable(describeErrors(reparsed.errors)));
    QCOMPARE(*reparsed.value, *accepted.value);

    for (const auto &layout : {
             QString{}, QStringLiteral("dwindle"),
             QStringLiteral("master"), QStringLiteral("scrolling"),
             QStringLiteral("monocle"),
         }) {
      auto candidate = workspace;
      candidate.insert(QStringLiteral("layout"), layout);
      object.insert(QStringLiteral("workspaceRules"), QJsonArray{candidate});
      QVERIFY2(parseState(object, *catalogResult.value), qPrintable(layout));
    }
    for (const auto &animation : {
             QString{}, QStringLiteral("fade"), QStringLiteral("slide"),
             QStringLiteral("slide 0%"), QStringLiteral("slide right 100%"),
             QStringLiteral("slidevert top"),
             QStringLiteral("slidefade 7%"),
             QStringLiteral("slidefadevert left 37%"),
         }) {
      auto candidate = workspace;
      auto nextOverrides = overrides;
      nextOverrides.insert(QStringLiteral("animation"), animation);
      candidate.insert(QStringLiteral("overrides"), nextOverrides);
      object.insert(QStringLiteral("workspaceRules"), QJsonArray{candidate});
      QVERIFY2(
          parseState(object, *catalogResult.value), qPrintable(animation)
      );
    }

    const auto rejectedOverride = [&](const QString &key,
                                      const QJsonValue &value) {
      auto candidate = workspace;
      auto nextOverrides = overrides;
      nextOverrides.insert(key, value);
      candidate.insert(QStringLiteral("overrides"), nextOverrides);
      object.insert(QStringLiteral("workspaceRules"), QJsonArray{candidate});
      return !parseState(object, *catalogResult.value);
    };
    QVERIFY(rejectedOverride(
        QStringLiteral("gaps_in"), QJsonArray{1, 2, 3}
    ));
    QVERIFY(rejectedOverride(
        QStringLiteral("gaps_out"), QJsonArray{1, 2, 3, 4, 5}
    ));
    QVERIFY(rejectedOverride(
        QStringLiteral("float_gaps"), QJsonArray{1, 2.5, 3, 4}
    ));
    QVERIFY(rejectedOverride(
        QStringLiteral("border_size"), safeInteger + 1.0
    ));
    QVERIFY(rejectedOverride(
        QStringLiteral("no_border"), QStringLiteral("true")
    ));
    QVERIFY(rejectedOverride(
        QStringLiteral("default_name"), QString(257, QLatin1Char('n'))
    ));
    QVERIFY(rejectedOverride(
        QStringLiteral("animation"), QStringLiteral("slide 00%")
    ));
    QVERIFY(rejectedOverride(
        QStringLiteral("animation"), QStringLiteral("slide 101%")
    ));
    QVERIFY(rejectedOverride(
        QStringLiteral("layout_opts"),
        QJsonObject{{QStringLiteral("orientation"),
                     QStringLiteral("diagonal")}}
    ));
    QVERIFY(rejectedOverride(
        QStringLiteral("on_created_empty"), QStringLiteral("exec evil")
    ));

    auto missing = workspace;
    missing.remove(QStringLiteral("selector"));
    object.insert(QStringLiteral("workspaceRules"), QJsonArray{missing});
    QVERIFY(!parseState(object, *catalogResult.value));
    auto extra = workspace;
    extra.insert(QStringLiteral("name"), QStringLiteral("invented"));
    object.insert(QStringLiteral("workspaceRules"), QJsonArray{extra});
    QVERIFY(!parseState(object, *catalogResult.value));
  }

  void selectorsRegexesAndNaturalIdentitiesFailClosed() {
    const auto catalogResult = shippedCatalog();
    QVERIFY(catalogResult);

    const QJsonObject workspace{
        {QStringLiteral("id"), QStringLiteral("workspace-selector")},
        {QStringLiteral("selector"), QStringLiteral("1")},
        {QStringLiteral("enabled"), true},
        {QStringLiteral("monitor"), QString()},
        {QStringLiteral("persistent"), false},
        {QStringLiteral("isDefault"), false},
        {QStringLiteral("layout"), QString()},
        {QStringLiteral("overrides"), QJsonObject{}},
    };
    for (const auto &selector : {
             QStringLiteral("1"), QStringLiteral("2147483647"),
             QStringLiteral("name:dev"), QStringLiteral("special"),
             QStringLiteral("special:music")}) {
      auto candidate = workspace;
      candidate.insert(QStringLiteral("selector"), selector);
      auto object = defaultStateObject();
      object.insert(QStringLiteral("workspaceRules"), QJsonArray{candidate});
      const auto parsed = parseState(object, *catalogResult.value);
      QVERIFY2(parsed, qPrintable(selector + QLatin1Char(':') +
                                  describeErrors(parsed.errors)));
    }
    for (const auto &selector : {
             QStringLiteral("0"), QStringLiteral("2147483648"),
             QStringLiteral("name:"), QStringLiteral("special:"),
             QStringLiteral("r[1-3]"), QStringLiteral("1 2"),
             QStringLiteral("bogus")}) {
      auto candidate = workspace;
      candidate.insert(QStringLiteral("selector"), selector);
      auto object = defaultStateObject();
      object.insert(QStringLiteral("workspaceRules"), QJsonArray{candidate});
      const auto parsed = parseState(object, *catalogResult.value);
      QVERIFY2(!parsed, qPrintable(selector));
      QVERIFY2(hasCode(parsed.errors,
                       QStringLiteral("state.invalid-workspace-selector")),
               qPrintable(selector));
    }
    const auto maximumDescription = QString(256, QLatin1Char('D'));
    auto describedWorkspace = workspace;
    describedWorkspace.insert(
        QStringLiteral("monitor"),
        QStringLiteral("desc:") + maximumDescription
    );
    auto workspaceObject = defaultStateObject();
    workspaceObject.insert(
        QStringLiteral("workspaceRules"), QJsonArray{describedWorkspace}
    );
    QVERIFY(parseState(workspaceObject, *catalogResult.value));
    describedWorkspace.insert(
        QStringLiteral("monitor"),
        QStringLiteral("desc:") + maximumDescription + QLatin1Char('x')
    );
    workspaceObject.insert(
        QStringLiteral("workspaceRules"), QJsonArray{describedWorkspace}
    );
    const auto excessiveWorkspaceMonitor =
        parseState(workspaceObject, *catalogResult.value);
    QVERIFY(!excessiveWorkspaceMonitor);

    const QJsonObject permission{
        {QStringLiteral("id"), QStringLiteral("permission-one")},
        {QStringLiteral("binary"), QStringLiteral("^/usr/bin/foo$")},
        {QStringLiteral("type"), QStringLiteral("screencopy")},
        {QStringLiteral("mode"), QStringLiteral("deny")},
    };
    auto object = defaultStateObject();
    object.insert(QStringLiteral("permissions"), QJsonArray{permission});
    QVERIFY(parseState(object, *catalogResult.value));
    auto invalidPermission = permission;
    invalidPermission.insert(QStringLiteral("binary"), QStringLiteral("["));
    object.insert(QStringLiteral("permissions"),
                  QJsonArray{invalidPermission});
    const auto invalidPermissionRegex =
        parseState(object, *catalogResult.value);
    QVERIFY(!invalidPermissionRegex);
    QVERIFY(hasCode(invalidPermissionRegex.errors,
                    QStringLiteral("state.invalid-regex")));
    auto duplicatePermission = permission;
    duplicatePermission.insert(QStringLiteral("id"),
                               QStringLiteral("permission-two"));
    duplicatePermission.insert(QStringLiteral("mode"),
                               QStringLiteral("allow"));
    object.insert(QStringLiteral("permissions"),
                  QJsonArray{permission, duplicatePermission});
    const auto duplicatePermissionIdentity =
        parseState(object, *catalogResult.value);
    QVERIFY(!duplicatePermissionIdentity);
    QVERIFY(hasCode(duplicatePermissionIdentity.errors,
                    QStringLiteral("state.duplicate-natural-identity")));

    const QJsonObject monitor{
        {QStringLiteral("id"), QStringLiteral("monitor-one")},
        {QStringLiteral("selector"), QStringLiteral("DP-1")},
        {QStringLiteral("enabled"), true},
        {QStringLiteral("mode"), QStringLiteral("preferred")},
        {QStringLiteral("position"), QStringLiteral("auto")},
        {QStringLiteral("scale"), 1.0},
        {QStringLiteral("reserved"), QJsonArray{0, 0, 0, 0}},
        {QStringLiteral("transform"), 0},
        {QStringLiteral("mirror"), QString()},
        {QStringLiteral("bitdepth"), 8},
        {QStringLiteral("cm"), QStringLiteral("auto")},
        {QStringLiteral("sdrEotf"), QStringLiteral("default")},
        {QStringLiteral("sdrBrightness"), 1.0},
        {QStringLiteral("sdrSaturation"), 1.0},
        {QStringLiteral("vrr"), 0},
        {QStringLiteral("icc"), QString()},
        {QStringLiteral("supportsWideColor"), 0},
        {QStringLiteral("supportsHdr"), 0},
        {QStringLiteral("sdrMinLuminance"), 0.2},
        {QStringLiteral("sdrMaxLuminance"), 80},
        {QStringLiteral("minLuminance"), -1.0},
        {QStringLiteral("maxLuminance"), -1},
        {QStringLiteral("maxAvgLuminance"), -1},
    };
    auto monitorTwo = monitor;
    monitorTwo.insert(QStringLiteral("id"), QStringLiteral("monitor-two"));
    monitorTwo.insert(QStringLiteral("enabled"), false);
    object = defaultStateObject();
    object.insert(QStringLiteral("monitors"), QJsonArray{monitor, monitorTwo});
    const auto duplicateMonitor = parseState(object, *catalogResult.value);
    QVERIFY(!duplicateMonitor);
    QVERIFY(hasCode(duplicateMonitor.errors,
                    QStringLiteral("state.duplicate-natural-identity")));

    auto describedMonitor = monitor;
    describedMonitor.insert(QStringLiteral("selector"),
                            QStringLiteral("desc:Dell Inc."));
    object = defaultStateObject();
    object.insert(QStringLiteral("monitors"), QJsonArray{describedMonitor});
    QVERIFY(parseState(object, *catalogResult.value));
    describedMonitor.insert(
        QStringLiteral("selector"),
        QStringLiteral("desc:") + maximumDescription
    );
    object.insert(QStringLiteral("monitors"), QJsonArray{describedMonitor});
    QVERIFY(parseState(object, *catalogResult.value));
    describedMonitor.insert(
        QStringLiteral("selector"),
        QStringLiteral("desc:") + maximumDescription + QLatin1Char('x')
    );
    object.insert(QStringLiteral("monitors"), QJsonArray{describedMonitor});
    const auto excessiveMonitorDescription =
        parseState(object, *catalogResult.value);
    QVERIFY(!excessiveMonitorDescription);
    for (const auto &selector : {QStringLiteral("current"),
                                 QStringLiteral("+1"), QStringLiteral("0"),
                                 QStringLiteral("desc:")}) {
      auto invalidMonitor = monitor;
      invalidMonitor.insert(QStringLiteral("selector"), selector);
      object.insert(QStringLiteral("monitors"), QJsonArray{invalidMonitor});
      const auto parsed = parseState(object, *catalogResult.value);
      QVERIFY2(!parsed, qPrintable(selector));
      QVERIFY2(hasCode(
                   parsed.errors,
                   QStringLiteral("state.invalid-static-monitor-selector")),
               qPrintable(selector));
    }
    auto invalidMirror = monitor;
    invalidMirror.insert(QStringLiteral("mirror"), QStringLiteral("current"));
    object.insert(QStringLiteral("monitors"), QJsonArray{invalidMirror});
    const auto dynamicMirror = parseState(object, *catalogResult.value);
    QVERIFY(!dynamicMirror);
    QVERIFY(hasCode(
        dynamicMirror.errors,
        QStringLiteral("state.invalid-static-monitor-selector")));

    auto mirrorSource = monitor;
    mirrorSource.insert(
        QStringLiteral("mirror"),
        QStringLiteral("desc:") + maximumDescription
    );
    auto mirrorTarget = monitor;
    mirrorTarget.insert(QStringLiteral("id"), QStringLiteral("monitor-desc"));
    mirrorTarget.insert(
        QStringLiteral("selector"),
        QStringLiteral("desc:") + maximumDescription
    );
    mirrorTarget.insert(QStringLiteral("enabled"), false);
    object.insert(
        QStringLiteral("monitors"), QJsonArray{mirrorSource, mirrorTarget}
    );
    QVERIFY(parseState(object, *catalogResult.value));
    mirrorSource.insert(
        QStringLiteral("mirror"),
        QStringLiteral("desc:") + maximumDescription + QLatin1Char('x')
    );
    object.insert(
        QStringLiteral("monitors"), QJsonArray{mirrorSource, mirrorTarget}
    );
    const auto excessiveMirrorDescription =
        parseState(object, *catalogResult.value);
    QVERIFY(!excessiveMirrorDescription);

    auto selfMirror = monitor;
    selfMirror.insert(QStringLiteral("mirror"), QStringLiteral("DP-1"));
    object.insert(QStringLiteral("monitors"), QJsonArray{selfMirror});
    const auto selfMirrored = parseState(object, *catalogResult.value);
    QVERIFY(!selfMirrored);
    QVERIFY(hasCode(selfMirrored.errors,
                    QStringLiteral("state.monitor-mirror-cycle")));

    auto cycleA = monitor;
    cycleA.insert(QStringLiteral("mirror"), QStringLiteral("DP-2"));
    auto cycleB = monitor;
    cycleB.insert(QStringLiteral("id"), QStringLiteral("monitor-cycle-b"));
    cycleB.insert(QStringLiteral("selector"), QStringLiteral("DP-2"));
    cycleB.insert(QStringLiteral("mirror"), QStringLiteral("DP-1"));
    object.insert(QStringLiteral("monitors"), QJsonArray{cycleA, cycleB});
    const auto mirrorCycle = parseState(object, *catalogResult.value);
    QVERIFY(!mirrorCycle);
    QVERIFY(hasCode(mirrorCycle.errors,
                    QStringLiteral("state.monitor-mirror-cycle")));

    const QJsonObject device{
        {QStringLiteral("id"), QStringLiteral("device-one")},
        {QStringLiteral("selector"), QStringLiteral("Main Device")},
        {QStringLiteral("kind"), QStringLiteral("pointer")},
        {QStringLiteral("enabled"), true},
        {QStringLiteral("overrides"), QJsonObject{}},
    };
    auto deviceTwo = device;
    deviceTwo.insert(QStringLiteral("id"), QStringLiteral("device-two"));
    deviceTwo.insert(QStringLiteral("selector"), QStringLiteral("Main-Device"));
    object = defaultStateObject();
    object.insert(QStringLiteral("devices"), QJsonArray{device, deviceTwo});
    const auto duplicateDevice = parseState(object, *catalogResult.value);
    QVERIFY(!duplicateDevice);
    QVERIFY(hasCode(duplicateDevice.errors,
                    QStringLiteral("state.duplicate-natural-identity")));

    auto workspaceTwo = workspace;
    workspaceTwo.insert(QStringLiteral("id"), QStringLiteral("workspace-two"));
    workspaceTwo.insert(QStringLiteral("enabled"), false);
    object = defaultStateObject();
    object.insert(QStringLiteral("workspaceRules"),
                  QJsonArray{workspace, workspaceTwo});
    const auto duplicateWorkspace = parseState(object, *catalogResult.value);
    QVERIFY(!duplicateWorkspace);
    QVERIFY(hasCode(duplicateWorkspace.errors,
                    QStringLiteral("state.duplicate-natural-identity")));

    const QJsonObject environment{
        {QStringLiteral("id"), QStringLiteral("environment-one")},
        {QStringLiteral("name"), QStringLiteral("XCURSOR_SIZE")},
        {QStringLiteral("value"), QStringLiteral("24")},
        {QStringLiteral("scope"), QStringLiteral("hyprland")},
    };
    auto environmentTwo = environment;
    environmentTwo.insert(QStringLiteral("id"),
                          QStringLiteral("environment-two"));
    environmentTwo.insert(QStringLiteral("scope"), QStringLiteral("uwsm"));
    object = defaultStateObject();
    object.insert(QStringLiteral("environment"),
                  QJsonArray{environment, environmentTwo});
    const auto duplicateEnvironment =
        parseState(object, *catalogResult.value);
    QVERIFY(!duplicateEnvironment);
    QVERIFY(hasCode(duplicateEnvironment.errors,
                    QStringLiteral("state.duplicate-natural-identity")));

    const QJsonObject animation{
        {QStringLiteral("id"), QStringLiteral("animation-one")},
        {QStringLiteral("name"), QStringLiteral("windows")},
        {QStringLiteral("enabled"), true},
        {QStringLiteral("speed"), 1.0},
        {QStringLiteral("curve"), QStringLiteral("default")},
        {QStringLiteral("style"), QString()},
    };
    auto animationTwo = animation;
    animationTwo.insert(QStringLiteral("id"),
                        QStringLiteral("animation-two"));
    animationTwo.insert(QStringLiteral("enabled"), false);
    object = defaultStateObject();
    object.insert(QStringLiteral("animations"),
                  QJsonArray{animation, animationTwo});
    const auto duplicateAnimation = parseState(object, *catalogResult.value);
    QVERIFY(!duplicateAnimation);
    QVERIFY(hasCode(duplicateAnimation.errors,
                    QStringLiteral("state.duplicate-natural-identity")));

    const QJsonObject windowRule{
        {QStringLiteral("id"), QStringLiteral("window-one")},
        {QStringLiteral("name"), QStringLiteral("Browser")},
        {QStringLiteral("enabled"), true},
        {QStringLiteral("match"),
         QJsonObject{{QStringLiteral("class"), QStringLiteral("^foo$")}}},
        {QStringLiteral("effects"),
         QJsonObject{{QStringLiteral("float"), true}}},
    };
    auto windowTwo = windowRule;
    windowTwo.insert(QStringLiteral("id"), QStringLiteral("window-two"));
    windowTwo.insert(QStringLiteral("enabled"), false);
    object = defaultStateObject();
    object.insert(QStringLiteral("windowRules"),
                  QJsonArray{windowRule, windowTwo});
    const auto duplicateWindowRule =
        parseState(object, *catalogResult.value);
    QVERIFY(!duplicateWindowRule);
    QVERIFY(hasCode(duplicateWindowRule.errors,
                    QStringLiteral("state.duplicate-natural-identity")));

    const QJsonObject layerRule{
        {QStringLiteral("id"), QStringLiteral("layer-one")},
        {QStringLiteral("name"), QStringLiteral("Panel")},
        {QStringLiteral("enabled"), true},
        {QStringLiteral("match"),
         QJsonObject{{QStringLiteral("namespace"), QStringLiteral("^panel$")}}},
        {QStringLiteral("effects"),
         QJsonObject{{QStringLiteral("blur"), true}}},
    };
    auto layerTwo = layerRule;
    layerTwo.insert(QStringLiteral("id"), QStringLiteral("layer-two"));
    layerTwo.insert(QStringLiteral("enabled"), false);
    object = defaultStateObject();
    object.insert(QStringLiteral("layerRules"),
                  QJsonArray{layerRule, layerTwo});
    const auto duplicateLayerRule =
        parseState(object, *catalogResult.value);
    QVERIFY(!duplicateLayerRule);
    QVERIFY(hasCode(duplicateLayerRule.errors,
                    QStringLiteral("state.duplicate-natural-identity")));
  }

  void taggedNumericAndLexicalBoundsPreventRuntimeNarrowing() {
    const auto catalogResult = shippedCatalog();
    QVERIFY(catalogResult);

    const QJsonObject monitor{
        {QStringLiteral("id"), QStringLiteral("monitor-wide-values")},
        {QStringLiteral("selector"), QStringLiteral("DP-1")},
        {QStringLiteral("enabled"), true},
        {QStringLiteral("mode"), QStringLiteral("preferred")},
        {QStringLiteral("position"), QStringLiteral("auto")},
        {QStringLiteral("scale"), 1.0},
        {QStringLiteral("reserved"), QJsonArray{50000, -50000, 0, 0}},
        {QStringLiteral("transform"), 0},
        {QStringLiteral("mirror"), QString()},
        {QStringLiteral("bitdepth"), 8},
        {QStringLiteral("cm"), QStringLiteral("auto")},
        {QStringLiteral("sdrEotf"), QStringLiteral("default")},
        {QStringLiteral("sdrBrightness"), 1.0},
        {QStringLiteral("sdrSaturation"), 1.0},
        {QStringLiteral("vrr"), 0},
        {QStringLiteral("icc"), QString()},
        {QStringLiteral("supportsWideColor"), 0},
        {QStringLiteral("supportsHdr"), 0},
        {QStringLiteral("sdrMinLuminance"), 0.2},
        {QStringLiteral("sdrMaxLuminance"), 80},
        {QStringLiteral("minLuminance"), -1.0},
        {QStringLiteral("maxLuminance"), -1},
        {QStringLiteral("maxAvgLuminance"), -1},
    };
    const QJsonObject device{
        {QStringLiteral("id"), QStringLiteral("device-transform")},
        {QStringLiteral("selector"), QStringLiteral("name:Pointer")},
        {QStringLiteral("kind"), QStringLiteral("pointer")},
        {QStringLiteral("enabled"), true},
        {QStringLiteral("overrides"),
         QJsonObject{{QStringLiteral("transform"), 7}}},
    };
    const QJsonObject workspace{
        {QStringLiteral("id"), QStringLiteral("workspace-wide-values")},
        {QStringLiteral("selector"), QStringLiteral("1")},
        {QStringLiteral("enabled"), true},
        {QStringLiteral("monitor"), QString()},
        {QStringLiteral("persistent"), false},
        {QStringLiteral("isDefault"), false},
        {QStringLiteral("layout"), QString()},
        {QStringLiteral("overrides"),
         QJsonObject{
             {QStringLiteral("gaps_in"),
              QJsonArray{50000, -50000, 0, 0}},
             {QStringLiteral("border_size"), 5000},
         }},
    };
    const QJsonObject windowRule{
        {QStringLiteral("id"), QStringLiteral("window-wide-values")},
        {QStringLiteral("name"), QStringLiteral("Wide values")},
        {QStringLiteral("enabled"), true},
        {QStringLiteral("match"),
         QJsonObject{{QStringLiteral("class"), QStringLiteral("^wide$")}}},
        {QStringLiteral("effects"),
         QJsonObject{{QStringLiteral("no_close_for"), 2147483647},
                     {QStringLiteral("border_size"), -5000}}},
    };

    auto object = defaultStateObject();
    object.insert(QStringLiteral("monitors"), QJsonArray{monitor});
    object.insert(QStringLiteral("devices"), QJsonArray{device});
    object.insert(QStringLiteral("workspaceRules"), QJsonArray{workspace});
    object.insert(QStringLiteral("windowRules"), QJsonArray{windowRule});
    const auto accepted = parseState(object, *catalogResult.value);
    QVERIFY2(accepted, qPrintable(describeErrors(accepted.errors)));
    const auto reparsed = parseStateBytes(
        serializeDesiredState(*accepted.value), *catalogResult.value);
    QVERIFY2(reparsed, qPrintable(describeErrors(reparsed.errors)));
    QCOMPARE(*reparsed.value, *accepted.value);

    auto invalidDeviceTransform = device;
    invalidDeviceTransform.insert(
        QStringLiteral("overrides"),
        QJsonObject{{QStringLiteral("transform"), 8}});
    object = defaultStateObject();
    object.insert(QStringLiteral("devices"),
                  QJsonArray{invalidDeviceTransform});
    const auto transformOverflow =
        parseState(object, *catalogResult.value);
    QVERIFY(!transformOverflow);
    QVERIFY(hasCode(transformOverflow.errors,
                    QStringLiteral("state.nested-value-range")));

    auto excessiveLuminance = monitor;
    excessiveLuminance.insert(QStringLiteral("sdrMaxLuminance"),
                              2147483648.0);
    object = defaultStateObject();
    object.insert(QStringLiteral("monitors"), QJsonArray{excessiveLuminance});
    const auto luminanceOverflow = parseState(object, *catalogResult.value);
    QVERIFY(!luminanceOverflow);
    QVERIFY(hasCode(luminanceOverflow.errors,
                    QStringLiteral("state.integer-out-of-range")));

    auto excessiveScale = monitor;
    excessiveScale.insert(QStringLiteral("scale"), 1.0e308);
    object.insert(QStringLiteral("monitors"), QJsonArray{excessiveScale});
    const auto floatOverflow = parseState(object, *catalogResult.value);
    QVERIFY(!floatOverflow);
    QVERIFY(hasCode(floatOverflow.errors,
                    QStringLiteral("state.invalid-monitor-scale")));

    auto explicitMonitor = monitor;
    explicitMonitor.insert(QStringLiteral("mode"),
                           QStringLiteral("1920x1080@60"));
    explicitMonitor.insert(QStringLiteral("position"),
                           QStringLiteral("+1x-1"));
    object = defaultStateObject();
    object.insert(QStringLiteral("monitors"), QJsonArray{explicitMonitor});
    const auto explicitMonitorAccepted =
        parseState(object, *catalogResult.value);
    QVERIFY2(explicitMonitorAccepted,
             qPrintable(describeErrors(explicitMonitorAccepted.errors)));

    auto malformedMonitor = monitor;
    malformedMonitor.insert(QStringLiteral("mode"),
                            QStringLiteral("preferred-extra"));
    object.insert(QStringLiteral("monitors"), QJsonArray{malformedMonitor});
    const auto badMonitorMode = parseState(object, *catalogResult.value);
    QVERIFY(!badMonitorMode);
    QVERIFY(hasCode(badMonitorMode.errors,
                    QStringLiteral("state.invalid-monitor-mode")));

    malformedMonitor = monitor;
    malformedMonitor.insert(QStringLiteral("position"),
                            QStringLiteral("1000001x0"));
    object.insert(QStringLiteral("monitors"), QJsonArray{malformedMonitor});
    const auto badMonitorPosition = parseState(object, *catalogResult.value);
    QVERIFY(!badMonitorPosition);
    QVERIFY(hasCode(badMonitorPosition.errors,
                    QStringLiteral("state.invalid-monitor-position")));

    auto invalidDevice = device;
    invalidDevice.insert(
        QStringLiteral("overrides"),
        QJsonObject{{QStringLiteral("kb_layout"), QString(257, QLatin1Char('a'))}});
    object = defaultStateObject();
    object.insert(QStringLiteral("devices"), QJsonArray{invalidDevice});
    const auto longShortString = parseState(object, *catalogResult.value);
    QVERIFY(!longShortString);
    QVERIFY(hasCode(longShortString.errors,
                    QStringLiteral("state.nested-value-type")));

    auto invalidWindow = windowRule;
    invalidWindow.insert(
        QStringLiteral("match"),
        QJsonObject{{QStringLiteral("class"), QString()}});
    object = defaultStateObject();
    object.insert(QStringLiteral("windowRules"), QJsonArray{invalidWindow});
    const auto emptyRegex = parseState(object, *catalogResult.value);
    QVERIFY(!emptyRegex);
    QVERIFY(hasCode(emptyRegex.errors,
                    QStringLiteral("state.nested-value-type")));

    invalidWindow = windowRule;
    invalidWindow.insert(
        QStringLiteral("effects"),
        QJsonObject{{QStringLiteral("move"), QJsonArray{QString(), 1}}});
    object.insert(QStringLiteral("windowRules"), QJsonArray{invalidWindow});
    const auto emptyExpression = parseState(object, *catalogResult.value);
    QVERIFY(!emptyExpression);
    QVERIFY(hasCode(emptyExpression.errors,
                    QStringLiteral("state.nested-value-type")));

    invalidWindow = windowRule;
    invalidWindow.insert(
        QStringLiteral("effects"),
        QJsonObject{{QStringLiteral("move"),
                     QJsonArray{1000000.1, 0.0}}});
    object.insert(QStringLiteral("windowRules"), QJsonArray{invalidWindow});
    const auto excessiveExpression =
        parseState(object, *catalogResult.value);
    QVERIFY(!excessiveExpression);
    QVERIFY(hasCode(excessiveExpression.errors,
                    QStringLiteral("state.nested-value-type")));
  }

  void submapsEnforceAcyclicNamedResetGraphs() {
    const auto catalogResult = shippedCatalog();
    QVERIFY(catalogResult);
    const QJsonObject leader{
        {QStringLiteral("id"), QStringLiteral("submap-leader")},
        {QStringLiteral("name"), QStringLiteral("leader")},
        {QStringLiteral("reset"), QStringLiteral("reset")},
        {QStringLiteral("enabled"), true},
    };
    const QJsonObject graphics{
        {QStringLiteral("id"), QStringLiteral("submap-graphics")},
        {QStringLiteral("name"), QStringLiteral("graphics")},
        {QStringLiteral("reset"), QStringLiteral("leader")},
        {QStringLiteral("enabled"), true},
    };
    auto scoped =
        binding(QStringLiteral("graphics-terminal"), {QStringLiteral("super")},
                QStringLiteral("F2"), QStringLiteral("Scoped terminal"));
    scoped.insert(QStringLiteral("submap"), QStringLiteral("graphics"));
    auto catchall =
        binding(QStringLiteral("graphics-catchall"), {},
                QStringLiteral("catchall"), QStringLiteral("Reset catchall"));
    catchall.insert(QStringLiteral("submap"), QStringLiteral("graphics"));
    auto leaderBinding =
        binding(QStringLiteral("leader-terminal"), {}, QStringLiteral("F3"),
                QStringLiteral("Leader terminal"));
    leaderBinding.insert(QStringLiteral("submap"), QStringLiteral("leader"));

    auto object = defaultStateObject();
    object.insert(QStringLiteral("submaps"), QJsonArray{leader, graphics});
    object.insert(QStringLiteral("bindings"),
                  QJsonArray{leaderBinding, scoped, catchall});
    const auto accepted = parseState(object, *catalogResult.value);
    QVERIFY2(
        accepted,
        qPrintable(accepted.errors.isEmpty()
                       ? QStringLiteral("valid submap reset chain was rejected")
                       : accepted.errors.constFirst().message));

    auto dormant = leader;
    dormant.insert(QStringLiteral("id"), QStringLiteral("submap-dormant"));
    dormant.insert(QStringLiteral("name"), QStringLiteral("dormant"));
    dormant.insert(QStringLiteral("reset"), QString());
    dormant.insert(QStringLiteral("enabled"), false);
    object = defaultStateObject();
    object.insert(QStringLiteral("submaps"), QJsonArray{dormant});
    const auto dormantAccepted = parseState(object, *catalogResult.value);
    QVERIFY2(dormantAccepted,
             qPrintable(describeErrors(dormantAccepted.errors)));

    auto activeInDormant = binding(QStringLiteral("dormant-binding"), {},
                                   QStringLiteral("F4"),
                                   QStringLiteral("Dormant binding"));
    activeInDormant.insert(QStringLiteral("submap"),
                           QStringLiteral("dormant"));
    object.insert(QStringLiteral("bindings"), QJsonArray{activeInDormant});
    const auto activeToDisabled = parseState(object, *catalogResult.value);
    QVERIFY(!activeToDisabled);
    QVERIFY(hasCode(activeToDisabled.errors,
                    QStringLiteral("state.disabled-submap-target")));

    auto malformed = graphics;
    malformed.insert(QStringLiteral("reset"), QStringLiteral("missing"));
    object = defaultStateObject();
    object.insert(QStringLiteral("submaps"), QJsonArray{leader, malformed});
    const auto missingReset = parseState(object, *catalogResult.value);
    QVERIFY(!missingReset);
    QVERIFY(hasCode(missingReset.errors,
                    QStringLiteral("state.unknown-submap-reset")));

    malformed = leader;
    malformed.insert(QStringLiteral("reset"), QStringLiteral("leader"));
    object.insert(QStringLiteral("submaps"), QJsonArray{malformed});
    const auto selfCycle = parseState(object, *catalogResult.value);
    QVERIFY(!selfCycle);
    QVERIFY(hasCode(selfCycle.errors, QStringLiteral("state.submap-cycle")));

    auto leaderCycle = leader;
    leaderCycle.insert(QStringLiteral("reset"), QStringLiteral("graphics"));
    object.insert(QStringLiteral("submaps"), QJsonArray{leaderCycle, graphics});
    const auto cycle = parseState(object, *catalogResult.value);
    QVERIFY(!cycle);
    QVERIFY(hasCode(cycle.errors, QStringLiteral("state.submap-cycle")));

    malformed = graphics;
    malformed.insert(QStringLiteral("name"), QStringLiteral("leader"));
    object.insert(QStringLiteral("submaps"), QJsonArray{leader, malformed});
    const auto duplicateName = parseState(object, *catalogResult.value);
    QVERIFY(!duplicateName);
    QVERIFY(
        hasCode(duplicateName.errors, QStringLiteral("state.duplicate-name")));

    scoped.insert(QStringLiteral("submap"), QStringLiteral("missing"));
    object = defaultStateObject();
    object.insert(QStringLiteral("submaps"), QJsonArray{leader});
    object.insert(QStringLiteral("bindings"), QJsonArray{scoped});
    const auto unknownBindingSubmap = parseState(object, *catalogResult.value);
    QVERIFY(!unknownBindingSubmap);
    QVERIFY(hasCode(unknownBindingSubmap.errors,
                    QStringLiteral("state.unknown-submap")));

    catchall.insert(QStringLiteral("submap"), QString());
    object = defaultStateObject();
    object.insert(QStringLiteral("bindings"), QJsonArray{catchall});
    const auto unscopedCatchall = parseState(object, *catalogResult.value);
    QVERIFY(!unscopedCatchall);
    QVERIFY(hasCode(unscopedCatchall.errors,
                    QStringLiteral("state.catchall-outside-submap")));
  }

  void normalizesChordsAndRejectsEnabledDuplicates() {
    const auto schema =
        readObject(QStringLiteral(HYPRSHELLD_HYPRLAND_CONFIG_SCHEMA_FILE));
    const auto modifierEnum = schema.value(QStringLiteral("$defs"))
                                  .toObject()
                                  .value(QStringLiteral("modifiers"))
                                  .toObject()
                                  .value(QStringLiteral("items"))
                                  .toObject()
                                  .value(QStringLiteral("enum"))
                                  .toArray();
    QCOMPARE(modifierEnum,
             QJsonArray({QStringLiteral("shift"), QStringLiteral("caps"),
                         QStringLiteral("ctrl"), QStringLiteral("alt"),
                         QStringLiteral("mod2"), QStringLiteral("mod3"),
                         QStringLiteral("super"), QStringLiteral("mod5")}));

    const auto first = normalizeBindingChord(
        {QStringLiteral("SHIFT"), QStringLiteral("SUPER")},
        QStringLiteral("f2"));
    const auto second = normalizeBindingChord(
        {QStringLiteral("super"), QStringLiteral("shift")},
        QStringLiteral("F2"));
    QVERIFY(first);
    QVERIFY(second);
    QCOMPARE(*first.value, *second.value);

    const auto duplicateModifier = normalizeBindingChord(
        {QStringLiteral("SUPER"), QStringLiteral("super")},
        QStringLiteral("F2"));
    QVERIFY(!duplicateModifier);
    QVERIFY(hasCode(duplicateModifier.errors,
                    QStringLiteral("state.duplicate-modifier")));

    const auto sourceAliases = normalizeBindingChord(
        {QStringLiteral("META"), QStringLiteral("CAPS")},
        QStringLiteral("plus"));
    QVERIFY(sourceAliases);
    QCOMPARE(*sourceAliases.value, QStringLiteral("SUPER+CAPS+PLUS"));

    const auto catalogResult = shippedCatalog();
    QVERIFY(catalogResult);
    auto object = defaultStateObject();
    object.insert(
        QStringLiteral("bindings"),
        QJsonArray{
            binding(QStringLiteral("quick-terminal-a"),
                    {QStringLiteral("shift"), QStringLiteral("super")},
                    QStringLiteral("F2"), QStringLiteral("Terminal A")),
            binding(QStringLiteral("quick-terminal-b"),
                    {QStringLiteral("super"), QStringLiteral("shift")},
                    QStringLiteral("F2"), QStringLiteral("Terminal B")),
        });
    const auto duplicate = parseState(object, *catalogResult.value);
    QVERIFY(!duplicate);
    QVERIFY(hasCode(duplicate.errors, QStringLiteral("state.duplicate-chord")));

    object = defaultStateObject();
    object.insert(QStringLiteral("bindings"),
                  QJsonArray{binding(QStringLiteral("caps-binding"),
                                     {QStringLiteral("caps")},
                                     QStringLiteral("comma"),
                                     QStringLiteral("Caps binding"))});
    const auto canonical = parseState(object, *catalogResult.value);
    QVERIFY(canonical);
    const auto canonicalRoundTrip = parseStateBytes(
        serializeDesiredState(*canonical.value), *catalogResult.value);
    QVERIFY(canonicalRoundTrip);

    const QStringList validKeys{
        QStringLiteral("comma"), QStringLiteral("plus"), QStringLiteral("F2"),
        QStringLiteral("XF86AudioRaiseVolume"), QStringLiteral("mouse:272"),
        QStringLiteral("mouse:767"), QStringLiteral("code:0"),
        QStringLiteral("code:42"), QStringLiteral("code:4294967294"),
        QStringLiteral("mouse_down"), QStringLiteral("mouse_up"),
        QStringLiteral("mouse_left"), QStringLiteral("mouse_right"),
    };
    for (qsizetype index = 0; index < validKeys.size(); ++index) {
      object = defaultStateObject();
      object.insert(
          QStringLiteral("bindings"),
          QJsonArray{binding(QStringLiteral("valid-key-%1").arg(index),
                             {QStringLiteral("super")}, validKeys.at(index),
                             QStringLiteral("Valid key"))});
      const auto valid = parseState(object, *catalogResult.value);
      QVERIFY2(valid, qPrintable(validKeys.at(index) + QStringLiteral(": ") +
                                 describeErrors(valid.errors)));
    }

    const QStringList invalidKeys{
        QStringLiteral(","), QStringLiteral("+"),
        QStringLiteral("NOTAREALKEY"), QStringLiteral("mouse:271"),
        QStringLiteral("mouse:768"), QStringLiteral("code:4294967295"),
        QStringLiteral("code:01"), QStringLiteral("f2"),
        QStringLiteral("Catchall"),
    };
    for (qsizetype index = 0; index < invalidKeys.size(); ++index) {
      object = defaultStateObject();
      object.insert(
          QStringLiteral("bindings"),
          QJsonArray{binding(QStringLiteral("invalid-key-%1").arg(index),
                             {QStringLiteral("super")}, invalidKeys.at(index),
                             QStringLiteral("Invalid key"))});
      const auto invalid = parseState(object, *catalogResult.value);
      QVERIFY2(!invalid, qPrintable(invalidKeys.at(index)));
      QVERIFY2(hasCode(invalid.errors, QStringLiteral("state.invalid-key")) ||
                   hasCode(invalid.errors,
                           QStringLiteral("state.non-canonical-key")),
               qPrintable(invalidKeys.at(index) + QStringLiteral(": ") +
                          describeErrors(invalid.errors)));
    }

    for (const auto &nonCanonical : {
             QStringLiteral("CAPS"), QStringLiteral("meta"),
             QStringLiteral("hyper"),
         }) {
      object = defaultStateObject();
      object.insert(QStringLiteral("bindings"),
                    QJsonArray{binding(QStringLiteral("bad-modifier"),
                                       {nonCanonical}, QStringLiteral("F2"),
                                       QStringLiteral("Bad modifier"))});
      const auto rejected = parseState(object, *catalogResult.value);
      QVERIFY(!rejected);
      QVERIFY(hasCode(rejected.errors, QStringLiteral("state.invalid-modifier")));
    }
  }

  void orderedRecordsSurviveCanonicalRoundTrip() {
    const auto catalogResult = shippedCatalog();
    QVERIFY(catalogResult);
    auto object = defaultStateObject();
    object.insert(
        QStringLiteral("bindings"),
        QJsonArray{
            binding(QStringLiteral("quick-browser"), {QStringLiteral("super")},
                    QStringLiteral("F3"), QStringLiteral("Browser")),
            binding(QStringLiteral("quick-terminal"), {QStringLiteral("super")},
                    QStringLiteral("F2"), QStringLiteral("Terminal")),
        });
    const auto parsed = parseState(object, *catalogResult.value);
    QVERIFY(parsed);
    QCOMPARE(parsed.value->bindings.size(), 2);
    QCOMPARE(parsed.value->bindings.at(0).id, QStringLiteral("quick-browser"));
    QCOMPARE(parsed.value->bindings.at(1).id, QStringLiteral("quick-terminal"));

    const auto reparsed = parseStateBytes(serializeDesiredState(*parsed.value),
                                          *catalogResult.value);
    QVERIFY(reparsed);
    QCOMPARE(reparsed.value->bindings.size(), 2);
    QCOMPARE(reparsed.value->bindings.at(0).id,
             QStringLiteral("quick-browser"));
    QCOMPARE(reparsed.value->bindings.at(1).id,
             QStringLiteral("quick-terminal"));
  }

  void rejectsDuplicateRecordIdsAndUnknownCurrentRecordFields() {
    const auto catalogResult = shippedCatalog();
    QVERIFY(catalogResult);
    auto first =
        binding(QStringLiteral("duplicate-id"), {QStringLiteral("super")},
                QStringLiteral("F2"), QStringLiteral("First"));
    auto second =
        binding(QStringLiteral("duplicate-id"), {QStringLiteral("super")},
                QStringLiteral("F3"), QStringLiteral("Second"));
    auto object = defaultStateObject();
    object.insert(QStringLiteral("bindings"), QJsonArray{first, second});
    const auto duplicate = parseState(object, *catalogResult.value);
    QVERIFY(!duplicate);
    QVERIFY(hasCode(duplicate.errors, QStringLiteral("state.duplicate-id")));

    first.insert(QStringLiteral("futureFlag"), true);
    object.insert(QStringLiteral("bindings"), QJsonArray{first});
    const auto unknown = parseState(object, *catalogResult.value);
    QVERIFY(!unknown);
    QVERIFY(hasCode(unknown.errors, QStringLiteral("state.unknown-field")));
  }

  void boundsBindingStringsAndDeviceFilters() {
    const auto catalogResult = shippedCatalog();
    QVERIFY(catalogResult);
    auto object = defaultStateObject();
    auto record =
        binding(QStringLiteral("bounded-binding"), {QStringLiteral("super")},
                QStringLiteral("F2"),
                QString(maximumStateStringLength + 1, QLatin1Char('x')));
    object.insert(QStringLiteral("bindings"), QJsonArray{record});
    const auto longString = parseState(object, *catalogResult.value);
    QVERIFY(!longString);
    QVERIFY(hasCode(longString.errors, QStringLiteral("state.invalid-string")));

    QJsonArray devices;
    for (qsizetype index = 0; index <= maximumGenericArrayItems; ++index) {
      devices.append(QStringLiteral("device-%1").arg(index));
    }
    record =
        binding(QStringLiteral("bounded-binding"), {QStringLiteral("super")},
                QStringLiteral("F2"), QStringLiteral("Terminal"));
    auto options = record.value(QStringLiteral("options")).toObject();
    options.insert(QStringLiteral("device"),
                   QJsonObject{
                       {QStringLiteral("inclusive"), true},
                       {QStringLiteral("list"), devices},
                   });
    record.insert(QStringLiteral("options"), options);
    object.insert(QStringLiteral("bindings"), QJsonArray{record});
    const auto tooManyDevices = parseState(object, *catalogResult.value);
    QVERIFY(!tooManyDevices);
    QVERIFY(hasCode(tooManyDevices.errors,
                    QStringLiteral("state.collection-limit")));
  }

  void boundsStateDocumentAndCollections() {
    const auto catalogResult = shippedCatalog();
    QVERIFY(catalogResult);
    const QByteArray oversized(maximumDesiredStateBytes + 1, ' ');
    const auto tooLarge = parseStateBytes(oversized, *catalogResult.value);
    QVERIFY(!tooLarge);
    QVERIFY(hasCode(tooLarge.errors, QStringLiteral("json.size-limit")));

    auto object = defaultStateObject();
    QJsonArray records;
    for (qsizetype index = 0; index <= maximumBindings; ++index) {
      records.append(binding(QStringLiteral("binding-%1").arg(index),
                             {QStringLiteral("super")},
                             QStringLiteral("F%1").arg(index + 1),
                             QStringLiteral("Binding %1").arg(index)));
    }
    object.insert(QStringLiteral("bindings"), records);
    const auto tooMany = parseState(object, *catalogResult.value);
    QVERIFY(!tooMany);
    QVERIFY(hasCode(tooMany.errors, QStringLiteral("state.collection-limit")));

    object = defaultStateObject();
    QJsonObject overrides;
    for (qsizetype index = 0; index <= maximumOverrides; ++index) {
      overrides.insert(QStringLiteral("future:option:%1").arg(index), true);
    }
    object.insert(QStringLiteral("overrides"), overrides);
    const auto tooManyOverrides = parseState(object, *catalogResult.value);
    QVERIFY(!tooManyOverrides);
    QVERIFY(hasCode(tooManyOverrides.errors,
                    QStringLiteral("state.collection-limit")));
  }

  void boundsStateDocumentDepth() {
    const auto catalogResult = shippedCatalog();
    QVERIFY(catalogResult);
    auto bytes = readBytes(QStringLiteral(HYPRSHELLD_HYPRLAND_DEFAULTS_FILE));
    QByteArray nested;
    for (int depth = 0; depth < 80; ++depth) {
      nested.append("{\"nested\":");
    }
    nested.append("true");
    nested.append(QByteArray(80, '}'));
    const auto emptyOverrides = QByteArrayLiteral("\"overrides\": {}");
    QVERIFY(bytes.contains(emptyOverrides));
    bytes.replace(emptyOverrides,
                  QByteArrayLiteral("\"overrides\": {\"future:option\":") +
                      nested + QByteArrayLiteral("}"));
    const auto tooDeep = parseStateBytes(bytes, *catalogResult.value);
    QVERIFY(!tooDeep);
    QVERIFY(hasCode(tooDeep.errors, QStringLiteral("json.depth-limit")));
  }

  void futureMinorPreservesExactJsonSemanticsCanonically() {
    const auto catalogResult = shippedCatalog();
    QVERIFY(catalogResult);
    auto object = defaultStateObject();
    object.insert(QStringLiteral("targetHyprland"), QStringLiteral("0.57"));
    object.insert(QStringLiteral("catalogDigest"),
                  QString(64, QLatin1Char('a')));
    object.insert(
        QStringLiteral("overrides"),
        QJsonObject{{QStringLiteral("future:option"), QStringLiteral("kept")}});
    auto futureBinding =
        binding(QStringLiteral("future-binding"), {QStringLiteral("super")},
                QStringLiteral("F12"), QStringLiteral("Future action"));
    futureBinding.insert(QStringLiteral("futureFlag"),
                         QJsonObject{{QStringLiteral("nested"), true}});
    object.insert(QStringLiteral("bindings"), QJsonArray{futureBinding});

    const auto parsed = parseState(object, *catalogResult.value);
    QVERIFY2(parsed,
             qPrintable(parsed.errors.isEmpty()
                            ? QStringLiteral("future read-only state rejected")
                            : parsed.errors.constFirst().message));
    QCOMPARE(parsed.value->compatibility,
             CompatibilityDecision::UnsupportedFuture);
    QVERIFY(parsed.value->readOnly);
    QVERIFY(parsed.value->opaqueFutureDocument.has_value());
    QCOMPARE(*parsed.value->opaqueFutureDocument, object);

    const auto serialized = serializeDesiredState(*parsed.value);
    QCOMPARE(QJsonDocument::fromJson(serialized).object(), object);
    const auto reparsed = parseStateBytes(serialized, *catalogResult.value);
    QVERIFY(reparsed);
    QVERIFY(reparsed.value->readOnly);
    QVERIFY(reparsed.value->opaqueFutureDocument ==
            parsed.value->opaqueFutureDocument);

    // Opaque means schema-opaque, not byte-opaque: the strict reader rejects
    // numbers that QJsonValue cannot represent and re-emit exactly.
    auto lossyNumber = encode(object);
    const auto exactNeedle = QByteArrayLiteral("\"nested\":true");
    QVERIFY(lossyNumber.contains(exactNeedle));
    lossyNumber.replace(exactNeedle,
                        QByteArrayLiteral("\"nested\":9007199254740993"));
    const auto lossy = parseStateBytes(lossyNumber, *catalogResult.value);
    QVERIFY(!lossy);
    QVERIFY(hasCode(lossy.errors, QStringLiteral("json.lossy-number")));
  }

  void unknownMajorFailsClosed() {
    const auto catalogResult = shippedCatalog();
    QVERIFY(catalogResult);
    auto object = defaultStateObject();
    object.insert(QStringLiteral("targetHyprland"), QStringLiteral("1.0"));
    const auto parsed = parseState(object, *catalogResult.value);
    QVERIFY(!parsed);
    QVERIFY(hasCode(parsed.errors,
                    QStringLiteral("state.unsupported-target-version")));
  }

  void olderMinorRequiresMigration() {
    const auto catalogResult = shippedCatalog();
    QVERIFY(catalogResult);
    auto object = defaultStateObject();
    object.insert(QStringLiteral("targetHyprland"), QStringLiteral("0.55"));
    const auto parsed = parseState(object, *catalogResult.value);
    QVERIFY(!parsed);
    QVERIFY(hasCode(parsed.errors,
                    QStringLiteral("state.unsupported-target-version")));
  }
};

QTEST_MAIN(HyprlandConfigurationTest)

#include "hyprland_configuration_test.moc"
