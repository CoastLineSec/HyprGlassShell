#pragma once

#include "dormant_fixed_record_capture.h"
#include "settled_v2_generation_byte_graph.h"

namespace HyprShelld::Compositor {

// Adapts all five fields of one already-successful historical fixed-record
// repeat capture to the existing settled-v2 generation content byte graph.
// Missing Authority or Desired maps to an empty invalid required view, while
// PresentBytes, including an empty payload, maps to a required borrowed view
// of the exact capture-owned bytes. Missing LastGood, Applied, or Pending maps
// to a disengaged optional; their PresentBytes maps to an engaged borrowed
// view of the exact capture-owned bytes. Defensively, an unknown future kind
// maps to an empty invalid required view or an engaged empty invalid optional,
// never to optional-record absence. The views are consumed synchronously and
// are never retained. DormantFixedRecordCaptureResult and FailedClosed are not
// accepted; a failed result has no capture payload that can be passed here.
//
// Generation evidence and the two parser authorities remain caller-supplied.
// They are not part of the fixed-record capture, and this adapter neither
// observes nor revalidates their provenance. The raw graph's cardinality-first
// Incoherent result and its within-bound DelegatePendingOwner precedence are
// returned unchanged; this adapter does not parse or content-classify Pending
// bytes.
//
// The capture is a pathless historical value, not a fresh, atomic,
// co-temporal, interval, end-of-call, or post-return-fresh snapshot. This
// adapter proves no canonical root/current name, descriptor-backed generation
// tree or inventory, mount provenance, ownership, exclusive lease, CAS,
// protected-contract identity, complete Pending/Restart classification,
// transition reconciliation, Bridge state, stable runtime entrypoint, Store
// state, repair, migration, startup decision, action, effect, or publication.
// In particular it supplies none of safeRootAndExclusiveLease,
// pendingClassifiedAsAbsentOrOrdinary, or referencedGenerationsVerified. It
// performs no filesystem operation, grants no wall-time bound on untrusted
// filesystems, and mints no capability or result type.
[[nodiscard]] SettledV2GenerationByteGraphResult
classifyCapturedSettledV2GenerationContentByteGraph(
    const DormantFixedRecordCapture &capture,
    const QVector<SettledV2GenerationEvidence> &evidence,
    const Hyprland::Catalog &catalogV2,
    const Hyprland::ActionCatalog &actionCatalogV2);

} // namespace HyprShelld::Compositor
