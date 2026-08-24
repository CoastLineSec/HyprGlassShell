#pragma once

#include "dormant_fixed_record_capture.h"
#include "observed_authority_facts.h"

namespace HyprShelld::Compositor {

// Classifies only the Authority and Desired fields of one already-successful
// historical repeat capture. Missing maps to a borrowed Missing observation;
// PresentBytes, including an empty payload, maps to a borrowed view of the
// exact capture-owned bytes. LastGood, Applied, and Pending are deliberately
// ignored. The borrowed views are used synchronously and are never retained.
// DormantFixedRecordCaptureResult and FailedClosed are not accepted; a failed
// result has no capture payload that can be passed to this API.
//
// The four parser authorities remain caller-supplied. The delegated raw facts
// builder independently canonicalizes and reparses all four before inspecting
// either record; its Classified or InvalidParserAuthorities result is returned
// unchanged. V1 is an observation, not migration eligibility. V2 is not a
// settled-v2 state. Absent is relative only to these captured descriptor
// observations.
//
// The input is a pathless historical value, not a fresh, atomic, co-temporal,
// interval, end-of-call, or post-return-fresh snapshot. This adapter proves no
// canonical root/current name, exclusive lease, CAS, mount provenance,
// protected-contract provenance, transition reconciliation, Pending kind,
// settled mirrors, generation verification, ownership, Bridge side/residue,
// stable entrypoint, migration/source-manifest authority or eligibility,
// repair state, startup prerequisite, action, effect, or filesystem
// authority. It performs no filesystem operation and mints no new capability
// or result type.
[[nodiscard]] ObservedAuthorityFactsResult buildCapturedObservedAuthorityFacts(
    const DormantFixedRecordCapture &capture,
    const Hyprland::Catalog &catalogV1,
    const Hyprland::ActionCatalog &actionCatalogV1,
    const Hyprland::Catalog &catalogV2,
    const Hyprland::ActionCatalog &actionCatalogV2);

} // namespace HyprShelld::Compositor
