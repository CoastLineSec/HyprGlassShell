#pragma once

#include "settled_v2_startup_facts.h"

#include "hyprland/action_catalog.h"
#include "hyprland/catalog.h"

#include <QByteArray>
#include <QByteArrayView>
#include <QMap>
#include <QString>
#include <QVector>

#include <optional>

namespace HyprShelld::Compositor {

inline constexpr qsizetype maximumSettledV2GenerationEvidence = 2;

// Source-only evidence for one untrusted activation-nonce lookup. Every byte
// and renderer path input is owned by the evidence; none is a filesystem
// descriptor, freshness observation, publication capability, or live-store
// authority.
struct SettledV2GenerationEvidence final {
  QString activationNonce;
  QByteArray desiredBytes;
  QByteArray manifestBytes;
  QMap<QString, QByteArray> files;
  QString generationRoot;
  QString userCustomPath;
};

enum class SettledV2GenerationByteGraphResult {
  GenerationContentByteCoherent,
  DelegatePendingOwner,
  Incoherent,
};

// Pure qualification of at most two generation byte products referenced by
// one already-captured settled-v2 record graph. A disengaged Pending view
// means caller-observed absence. Every current view and any engaged Pending
// view must remain alive and byte-stable for the complete call; unsafe capture
// stops earlier.
//
// Evidence cardinality greater than two is Incoherent before Pending is
// inspected. Within the bound, DelegatePendingOwner is deliberately closed
// over every present Pending observation that is not an exact ordinary record,
// including Restart-shaped, malformed, noncanonical, and oversized bytes.
// Content coherence proves no descriptor identity, inventory, store root,
// ownership, Bridge state, freshness, startup prerequisite, action, or
// publication eligibility. It neither proves an Applied record's historical
// requiredActivation nor sets referencedGenerationsVerified. Repeated
// generation+nonce identities nevertheless must carry one equal full Applied
// record, including requiredActivation and entrypoint.
//
// A coherent result is relative to the caller-supplied Catalog and
// ActionCatalog, which must already be separately qualified immutable parser
// products. This helper cannot reconstruct, bound, or prove their protected
// contract identity and cannot supply protectedContractsExact.
[[nodiscard]] SettledV2GenerationByteGraphResult
classifySettledV2GenerationContentByteGraph(
    const SettledV2CurrentRecordBytes &current,
    std::optional<QByteArrayView> pendingObservation,
    const QVector<SettledV2GenerationEvidence> &evidence,
    const Hyprland::Catalog &catalogV2,
    const Hyprland::ActionCatalog &actionCatalogV2);

} // namespace HyprShelld::Compositor
