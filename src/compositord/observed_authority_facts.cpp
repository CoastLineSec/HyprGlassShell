#include "observed_authority_facts.h"

#include "authority_records.h"
#include "desired_migration_reducer.h"

#include "hyprland/desired_state.h"
#include "hyprland/json_support.h"

#include <QByteArray>

#include <utility>

namespace HyprShelld::Compositor {
namespace {

struct ParserAuthorities final {
  Hyprland::Catalog catalogV1;
  Hyprland::ActionCatalog actionCatalogV1;
  Hyprland::Catalog catalogV2;
  Hyprland::ActionCatalog actionCatalogV2;
};

[[nodiscard]] bool
schemaAuthorityIsSelfConsistent(const Hyprland::ActionCatalog &actions) {
  const auto canonicalSchema =
      Hyprland::JsonSupport::canonicalJson(actions.canonicalConfigSchema);
  const auto reparsedCanonical = Hyprland::JsonSupport::parseStrictObject(
      canonicalSchema, Hyprland::maximumActionSchemaBytes, 64);
  if (!reparsedCanonical || Hyprland::JsonSupport::canonicalJson(
                                *reparsedCanonical.value) != canonicalSchema) {
    return false;
  }

  const auto reparsedRetained = Hyprland::JsonSupport::parseStrictObject(
      actions.configSchemaDocument, Hyprland::maximumActionSchemaBytes, 64);
  return reparsedRetained && Hyprland::JsonSupport::canonicalJson(
                                 *reparsedRetained.value) == canonicalSchema;
}

[[nodiscard]] std::optional<Hyprland::Catalog>
reparseCatalogV1(const Hyprland::Catalog &catalog) {
  const auto bytes = Hyprland::canonicalCatalogJson(catalog);
  const auto parsed = Hyprland::parseCatalog(bytes);
  return parsed ? std::optional<Hyprland::Catalog>{*parsed.value}
                : std::nullopt;
}

[[nodiscard]] std::optional<Hyprland::Catalog>
reparseCatalogV2(const Hyprland::Catalog &catalog) {
  const auto bytes = Hyprland::canonicalCatalogJson(catalog);
  const auto parsed = Hyprland::parseDormantCatalogV2(bytes);
  return parsed ? std::optional<Hyprland::Catalog>{*parsed.value}
                : std::nullopt;
}

[[nodiscard]] std::optional<Hyprland::ActionCatalog>
reparseActionCatalogV1(const Hyprland::ActionCatalog &actions) {
  if (!schemaAuthorityIsSelfConsistent(actions)) {
    return std::nullopt;
  }
  const auto bytes = Hyprland::canonicalActionCatalogJson(actions);
  const auto parsed =
      Hyprland::parseActionCatalog(bytes, actions.configSchemaDocument);
  return parsed ? std::optional<Hyprland::ActionCatalog>{*parsed.value}
                : std::nullopt;
}

[[nodiscard]] std::optional<Hyprland::ActionCatalog>
reparseActionCatalogV2(const Hyprland::ActionCatalog &actions) {
  if (!schemaAuthorityIsSelfConsistent(actions)) {
    return std::nullopt;
  }
  const auto bytes = Hyprland::canonicalActionCatalogJson(actions);
  const auto parsed = Hyprland::parseDormantActionCatalogV2(
      bytes, actions.configSchemaDocument);
  return parsed ? std::optional<Hyprland::ActionCatalog>{*parsed.value}
                : std::nullopt;
}

[[nodiscard]] std::optional<ParserAuthorities>
reparseParserAuthorities(const Hyprland::Catalog &catalogV1,
                         const Hyprland::ActionCatalog &actionCatalogV1,
                         const Hyprland::Catalog &catalogV2,
                         const Hyprland::ActionCatalog &actionCatalogV2) {
  // Do not short-circuit these independent authority checks.
  auto reparsedCatalogV1 = reparseCatalogV1(catalogV1);
  auto reparsedActionsV1 = reparseActionCatalogV1(actionCatalogV1);
  auto reparsedCatalogV2 = reparseCatalogV2(catalogV2);
  auto reparsedActionsV2 = reparseActionCatalogV2(actionCatalogV2);
  if (!reparsedCatalogV1 || !reparsedActionsV1 || !reparsedCatalogV2 ||
      !reparsedActionsV2) {
    return std::nullopt;
  }
  return ParserAuthorities{
      .catalogV1 = std::move(*reparsedCatalogV1),
      .actionCatalogV1 = std::move(*reparsedActionsV1),
      .catalogV2 = std::move(*reparsedCatalogV2),
      .actionCatalogV2 = std::move(*reparsedActionsV2),
  };
}

[[nodiscard]] bool isKnownReadKind(const ObservedAuthorityReadKind kind) {
  switch (kind) {
  case ObservedAuthorityReadKind::Missing:
  case ObservedAuthorityReadKind::PresentBytes:
  case ObservedAuthorityReadKind::Unsafe:
    return true;
  }
  return false;
}

[[nodiscard]] bool hasValidViewMetadata(const QByteArrayView bytes) {
  return bytes.size() >= 0 && (bytes.size() == 0 || bytes.data() != nullptr);
}

[[nodiscard]] bool
hasValidReadShape(const BorrowedObservedAuthorityRead &read) {
  if (!isKnownReadKind(read.kind) || !hasValidViewMetadata(read.bytes)) {
    return false;
  }
  return read.kind == ObservedAuthorityReadKind::PresentBytes ||
         read.bytes.size() == 0;
}

[[nodiscard]] AuthorityAnchorObservation
inspectAuthority(const BorrowedObservedAuthorityRead &read) {
  if (read.kind == ObservedAuthorityReadKind::Missing) {
    return {.kind = AuthorityAnchorObservationKind::Missing};
  }
  if (read.bytes.size() > maximumAuthorityRecordV2Bytes) {
    return {.kind = AuthorityAnchorObservationKind::PresentInvalid};
  }

  const auto parsed = parseAuthorityRecordV2(read.bytes);
  if (!parsed) {
    return {.kind = AuthorityAnchorObservationKind::PresentInvalid};
  }
  const auto serialized = serializeAuthorityRecordV2(*parsed.value);
  if (!serialized || QByteArrayView(*serialized.value) != read.bytes) {
    return {.kind = AuthorityAnchorObservationKind::PresentInvalid};
  }
  return {
      .kind = AuthorityAnchorObservationKind::ExactV2,
      .authorityId = parsed.value->authorityId,
  };
}

[[nodiscard]] constexpr DesiredAuthorityObservationKind
resolvedExactDesiredKind(const bool exactV1, const bool exactV2) {
  if (exactV1 == exactV2) {
    return DesiredAuthorityObservationKind::PresentInvalid;
  }
  return exactV1 ? DesiredAuthorityObservationKind::ExactV1
                 : DesiredAuthorityObservationKind::ExactV2;
}

static_assert(resolvedExactDesiredKind(false, false) ==
              DesiredAuthorityObservationKind::PresentInvalid);
static_assert(resolvedExactDesiredKind(true, false) ==
              DesiredAuthorityObservationKind::ExactV1);
static_assert(resolvedExactDesiredKind(false, true) ==
              DesiredAuthorityObservationKind::ExactV2);
static_assert(resolvedExactDesiredKind(true, true) ==
              DesiredAuthorityObservationKind::PresentInvalid);

[[nodiscard]] DesiredAuthorityObservation
inspectDesired(const BorrowedObservedAuthorityRead &read,
               const ParserAuthorities &authorities) {
  if (read.kind == ObservedAuthorityReadKind::Missing) {
    return {.kind = DesiredAuthorityObservationKind::Missing};
  }
  if (read.bytes.size() > Hyprland::maximumDesiredStateBytes) {
    return {.kind = DesiredAuthorityObservationKind::PresentInvalid};
  }

  const auto exactV1 = inspectExactDesiredV1Observation(
      read.bytes, authorities.catalogV1, authorities.actionCatalogV1);

  const auto parsedV2 = Hyprland::parseDormantDesiredStateV2(
      read.bytes, authorities.catalogV2, authorities.actionCatalogV2);
  const auto serializedV2 =
      parsedV2 ? Hyprland::serializeDormantDesiredStateV2(*parsedV2.value)
               : Hyprland::ValidationResult<QByteArray>{};
  const auto exactV2 = parsedV2 && serializedV2 &&
                       QByteArrayView(*serializedV2.value) == read.bytes;

  // The formats are designed to be mutually exclusive. Keep the defensive
  // dual-success branch closed if that invariant ever regresses.
  const auto exactKind = resolvedExactDesiredKind(exactV1.has_value(), exactV2);
  if (exactKind == DesiredAuthorityObservationKind::PresentInvalid) {
    return {.kind = DesiredAuthorityObservationKind::PresentInvalid};
  }
  if (exactKind == DesiredAuthorityObservationKind::ExactV1) {
    return {
        .kind = DesiredAuthorityObservationKind::ExactV1,
        .authorityId = {},
        .revision = exactV1->revision,
    };
  }
  return {
      .kind = DesiredAuthorityObservationKind::ExactV2,
      .authorityId = parsedV2.value->authorityId,
      .revision = parsedV2.value->semanticState.revision,
  };
}

[[nodiscard]] ObservedAuthorityTuple unreadableTuple() {
  return {
      .kind = ObservedAuthorityKind::Unreadable,
      .authorityId = {},
      .revision = 0,
  };
}

} // namespace

ObservedAuthorityFactsStatus ObservedAuthorityFactsResult::status() const {
  return status_;
}

const std::optional<ObservedAuthorityTuple> &
ObservedAuthorityFactsResult::tuple() const {
  return tuple_;
}

ObservedAuthorityFactsResult
buildObservedAuthorityFacts(const BorrowedObservedAuthorityRecords &records,
                            const Hyprland::Catalog &catalogV1,
                            const Hyprland::ActionCatalog &actionCatalogV1,
                            const Hyprland::Catalog &catalogV2,
                            const Hyprland::ActionCatalog &actionCatalogV2) {
  const auto authorities = reparseParserAuthorities(catalogV1, actionCatalogV1,
                                                    catalogV2, actionCatalogV2);
  if (!authorities) {
    return {};
  }

  const auto classifiedResult = [](ObservedAuthorityTuple tuple) {
    Q_ASSERT(isValidObservedAuthorityTuple(tuple));
    if (!isValidObservedAuthorityTuple(tuple)) {
      tuple = unreadableTuple();
    }
    ObservedAuthorityFactsResult result;
    result.status_ = ObservedAuthorityFactsStatus::Classified;
    result.tuple_ = std::move(tuple);
    return result;
  };

  if (!hasValidReadShape(records.authority) ||
      !hasValidReadShape(records.desired) ||
      records.authority.kind == ObservedAuthorityReadKind::Unsafe ||
      records.desired.kind == ObservedAuthorityReadKind::Unsafe) {
    return classifiedResult(unreadableTuple());
  }

  const auto anchor = inspectAuthority(records.authority);
  const auto desired = inspectDesired(records.desired, *authorities);
  return classifiedResult(classifyObservedAuthority(anchor, desired));
}

} // namespace HyprShelld::Compositor
