#include "captured_observed_authority_facts.h"

namespace HyprShelld::Compositor {
namespace {

[[nodiscard]] BorrowedObservedAuthorityRead
borrowField(const DormantFixedRecordField &field) {
  switch (field.kind()) {
  case DormantFixedRecordFieldKind::Missing:
    return {
        .kind = ObservedAuthorityReadKind::Missing,
        .bytes = {},
    };
  case DormantFixedRecordFieldKind::PresentBytes:
    return {
        .kind = ObservedAuthorityReadKind::PresentBytes,
        .bytes = QByteArrayView(field.bytes()),
    };
  }
  return {
      .kind = ObservedAuthorityReadKind::Unsafe,
      .bytes = {},
  };
}

} // namespace

ObservedAuthorityFactsResult buildCapturedObservedAuthorityFacts(
    const DormantFixedRecordCapture &capture,
    const Hyprland::Catalog &catalogV1,
    const Hyprland::ActionCatalog &actionCatalogV1,
    const Hyprland::Catalog &catalogV2,
    const Hyprland::ActionCatalog &actionCatalogV2) {
  const BorrowedObservedAuthorityRecords records{
      .authority = borrowField(capture.authority()),
      .desired = borrowField(capture.desired()),
  };
  return buildObservedAuthorityFacts(records, catalogV1, actionCatalogV1,
                                     catalogV2, actionCatalogV2);
}

} // namespace HyprShelld::Compositor
