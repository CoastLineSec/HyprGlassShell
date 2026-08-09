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
#include <tuple>

using namespace HyprShelld::Hyprland;

namespace {

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
        QVERIFY(!alternatives.isEmpty());
        for (const auto &alternative : alternatives) {
          const auto reference =
              alternative.toObject().value(QStringLiteral("$ref")).toString();
          const auto prefix = QStringLiteral("#/$defs/");
          QVERIFY2(reference.startsWith(prefix), qPrintable(reference));
          const auto resolved =
              definitions.value(reference.sliced(prefix.size())).toObject();
          QCOMPARE(resolved.value(QStringLiteral("additionalProperties")),
                   QJsonValue(false));
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
    const auto *defaultMonitor = findOption(
        catalog, QStringLiteral("hyprland.cursor.default_monitor"));
    const auto *layout = findOption(
        catalog, QStringLiteral("hyprland.general.layout"));
    QVERIFY(swallowRegex != nullptr);
    QVERIFY(defaultMonitor != nullptr);
    QVERIFY(layout != nullptr);

    auto semanticObject = defaultStateObject();
    semanticObject.insert(
        QStringLiteral("overrides"),
        QJsonObject{{swallowRegex->id, QStringLiteral("^foo$")},
                    {defaultMonitor->id, QStringLiteral("DP-1")}});
    const auto semanticAccepted = parseState(semanticObject, catalog);
    QVERIFY2(semanticAccepted,
             qPrintable(describeErrors(semanticAccepted.errors)));

    for (const auto &regex : {QString(), QStringLiteral("[")}) {
      semanticObject = defaultStateObject();
      semanticObject.insert(
          QStringLiteral("overrides"),
          QJsonObject{{swallowRegex->id, regex}});
      const auto invalid = parseState(semanticObject, catalog);
      QVERIFY(!invalid);
      QVERIFY(hasCode(invalid.errors, QStringLiteral("state.invalid-regex")));
    }
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
