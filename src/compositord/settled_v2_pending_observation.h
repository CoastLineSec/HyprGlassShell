#pragma once

#include "settled_v2_startup_facts.h"

#include "hyprland/action_catalog.h"
#include "hyprland/catalog.h"

#include <QByteArrayView>

#include <optional>

namespace HyprShelld::Compositor {

// Pure classification of one borrowed pending.json observation. A disengaged
// observation means the caller observed the fixed record missing and produces
// no-Pending facts. An engaged observation produces ordinary-Pending facts
// only when its bytes are a bounded exact canonical Apply, Recovery, or
// display-preview record. Every other present observation returns nullopt.
// The engaged pendingObservation view must remain alive and byte-stable for
// the entire call, including both bounded parse passes. The adapter retains no
// view or bytes. Unsafe or unstable capture must stop before this API; it
// cannot assess descriptor identity, capture safety, temporal stability, or
// freshness.
//
// A null result deliberately does not distinguish Restart-shaped or other
// nonordinary bytes from malformed or incomplete bytes. This function neither
// proves descriptor-backed absence or freshness nor supplies the
// pendingClassifiedAsAbsentOrOrdinary startup prerequisite.
[[nodiscard]] std::optional<SettledV2PendingFacts>
tryBuildSettledV2PendingFactsV2(
    const SettledV2CurrentRecordBytes &current,
    std::optional<QByteArrayView> pendingObservation,
    const Hyprland::Catalog &catalogV2,
    const Hyprland::ActionCatalog &actionCatalogV2);

} // namespace HyprShelld::Compositor
