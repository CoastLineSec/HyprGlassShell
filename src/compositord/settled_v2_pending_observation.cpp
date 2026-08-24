#include "settled_v2_pending_observation.h"

#include "ordinary_pending_record.h"

namespace HyprShelld::Compositor {
namespace {

[[nodiscard]] bool validPendingObservationView(const QByteArrayView bytes) {
  return bytes.size() >= 0 &&
         bytes.size() <= maximumOrdinaryPendingRecordV2Bytes &&
         (bytes.size() == 0 || bytes.data() != nullptr);
}

[[nodiscard]] bool isExactOrdinaryPendingObservation(
    const QByteArrayView bytes, const Hyprland::Catalog &catalogV2,
    const Hyprland::ActionCatalog &actionCatalogV2) {
  if (!validPendingObservationView(bytes)) {
    return false;
  }
  const auto parsed =
      parseOrdinaryPendingRecordV2(bytes, catalogV2, actionCatalogV2);
  if (!parsed) {
    return false;
  }
  const auto serialized = serializeOrdinaryPendingRecordV2(
      *parsed.value, catalogV2, actionCatalogV2);
  return serialized && QByteArrayView(*serialized.value) == bytes;
}

} // namespace

std::optional<SettledV2PendingFacts> tryBuildSettledV2PendingFactsV2(
    const SettledV2CurrentRecordBytes &current,
    const std::optional<QByteArrayView> pendingObservation,
    const Hyprland::Catalog &catalogV2,
    const Hyprland::ActionCatalog &actionCatalogV2) {
  if (!pendingObservation) {
    return SettledV2PendingFacts{
        buildNoPendingCoherenceFactsV2(current, catalogV2, actionCatalogV2)};
  }
  if (!isExactOrdinaryPendingObservation(*pendingObservation, catalogV2,
                                         actionCatalogV2)) {
    return std::nullopt;
  }
  return SettledV2PendingFacts{buildOrdinaryPendingStartupFactsV2(
      current, pendingObservation, catalogV2, actionCatalogV2)};
}

} // namespace HyprShelld::Compositor
