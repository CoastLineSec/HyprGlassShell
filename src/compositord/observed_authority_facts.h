#pragma once

#include "startup_reducer.h"

#include "hyprland/action_catalog.h"
#include "hyprland/catalog.h"

#include <QByteArrayView>

#include <optional>

namespace HyprShelld::Compositor {

enum class ObservedAuthorityReadKind {
  Missing,
  PresentBytes,
  Unsafe,
};

struct BorrowedObservedAuthorityRead final {
  ObservedAuthorityReadKind kind = ObservedAuthorityReadKind::Unsafe;
  QByteArrayView bytes;
};

struct BorrowedObservedAuthorityRecords final {
  BorrowedObservedAuthorityRead authority;
  BorrowedObservedAuthorityRead desired;
};

enum class ObservedAuthorityFactsStatus {
  Classified,
  InvalidParserAuthorities,
};

// A Classified result always owns one valid closed authority tuple. An
// InvalidParserAuthorities result carries no tuple. Neither result exposes a
// parser product, source bytes, or a Desired state.
class ObservedAuthorityFactsResult final {
public:
  [[nodiscard]] ObservedAuthorityFactsStatus status() const;
  [[nodiscard]] const std::optional<ObservedAuthorityTuple> &tuple() const;

private:
  ObservedAuthorityFactsStatus status_ =
      ObservedAuthorityFactsStatus::InvalidParserAuthorities;
  std::optional<ObservedAuthorityTuple> tuple_;

  friend ObservedAuthorityFactsResult buildObservedAuthorityFacts(
      const BorrowedObservedAuthorityRecords &, const Hyprland::Catalog &,
      const Hyprland::ActionCatalog &, const Hyprland::Catalog &,
      const Hyprland::ActionCatalog &);
};

// Pure classification of two already-captured borrowed observations. Missing
// and Unsafe reads carry no bytes; PresentBytes(empty) is present-invalid.
// Unsafe, malformed read metadata, and record syntax failures classify as the
// closed Unreadable tuple. The views must remain alive and byte-stable for the
// entire call and are never retained.
//
// Both v1 and v2 Catalog, ActionCatalog, and retained schema authorities are
// independently canonicalized and reparsed before either record is inspected.
// Failure there is InvalidParserAuthorities, not a user-store classification.
// Only the fresh reparsed products are used for record inspection.
//
// This function does not inspect migration or source manifests and does not
// assert descriptor identity, capture safety, freshness, a lease,
// protectedContractsExact, or a startup prerequisite. It authorizes no action
// or effect.
[[nodiscard]] ObservedAuthorityFactsResult
buildObservedAuthorityFacts(const BorrowedObservedAuthorityRecords &records,
                            const Hyprland::Catalog &catalogV1,
                            const Hyprland::ActionCatalog &actionCatalogV1,
                            const Hyprland::Catalog &catalogV2,
                            const Hyprland::ActionCatalog &actionCatalogV2);

} // namespace HyprShelld::Compositor
