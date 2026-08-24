#pragma once

#include "startup_reducer.h"

#include "hyprland/action_catalog.h"
#include "hyprland/catalog.h"

#include <QByteArrayView>

#include <optional>

namespace HyprShelld::Compositor {

// Borrowed byte observations only. A disengaged optional means the fixed
// record was observed missing; an engaged empty, malformed, or oversized view
// means it was present but invalid. The caller must keep every view stable for
// the duration of the call. These views convey no descriptor identity,
// freshness, lease, filesystem, or generation evidence.
struct SettledV2CurrentRecordBytes final {
  QByteArrayView authority;
  QByteArrayView desired;
  std::optional<QByteArrayView> lastGood;
  std::optional<QByteArrayView> applied;
};

// Pure byte-graph derivation for the already-classified no-Pending branch.
// It parses every present record under the exact supplied v2 authorities and
// never supplies startup prerequisites or makes a Ready/RepairOnly decision.
[[nodiscard]] NoPendingCoherenceFacts
buildNoPendingCoherenceFactsV2(const SettledV2CurrentRecordBytes &current,
                               const Hyprland::Catalog &catalogV2,
                               const Hyprland::ActionCatalog &actionCatalogV2);

// Pure byte-graph derivation for the already-classified ordinary-Pending
// branch. Disengaged means the expected ordinary record is missing; engaged
// malformed bytes remain an ordinary-branch failure. Invalid evidence returns
// recordCoherent=false with all mirror relations normalized to Neither and a
// non-authorizing Prepared phase. This function performs no cleanup or other
// action and never supplies startup prerequisites.
[[nodiscard]] OrdinaryPendingStartupFacts buildOrdinaryPendingStartupFactsV2(
    const SettledV2CurrentRecordBytes &current,
    std::optional<QByteArrayView> ordinaryPending,
    const Hyprland::Catalog &catalogV2,
    const Hyprland::ActionCatalog &actionCatalogV2);

} // namespace HyprShelld::Compositor
