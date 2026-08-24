#pragma once

#include "authority_records.h"
#include "startup_reducer.h"

#include "hyprland/action_catalog.h"
#include "hyprland/catalog.h"
#include "hyprland/desired_state.h"

#include <QByteArray>
#include <QByteArrayView>
#include <QString>
#include <QtTypes>

#include <optional>

namespace HyprShelld::Compositor {

inline constexpr quint32 ordinaryPendingRecordV2FormatVersion = 2;
inline constexpr qsizetype maximumOrdinaryPendingRecordV2Bytes =
    2 * Hyprland::maximumDesiredStateBytes + 16 * 1024;
inline constexpr int maximumOrdinaryPendingRecordV2Depth = 65;
inline constexpr qsizetype maximumOrdinaryPendingRecordV2Values =
    maximumOrdinaryPendingRecordV2Bytes;

enum class OrdinaryPendingKind {
  Apply,
  Recovery,
  DisplayPreview,
};

struct OrdinaryPendingDesiredMaterialV2 final {
  // The typed value is the semantic snapshot. bytes are the authoritative
  // standalone Desired v2 serializer bytes, including exactly one final LF.
  Hyprland::DesiredStateV2 state;
  QByteArray bytes;

  friend bool operator==(const OrdinaryPendingDesiredMaterialV2 &,
                         const OrdinaryPendingDesiredMaterialV2 &) = default;
};

struct OrdinaryPendingRecordV2 final {
  QString authorityId;
  OrdinaryPendingKind kind = OrdinaryPendingKind::Apply;
  OrdinaryPendingPhase phase = OrdinaryPendingPhase::Prepared;
  quint64 expectedRevision = 0;
  QString beforeDesiredDigest;

  // Engaged exactly when beforeActivation is engaged. This retains the
  // exact Desired document required to qualify that referenced generation;
  // it is not necessarily the Desired document at expectedRevision.
  std::optional<OrdinaryPendingDesiredMaterialV2> beforeActivationDesired;

  // The typed value is the semantic candidate. candidateSnapshotBytes are
  // the authoritative standalone Desired v2 serializer bytes, including
  // exactly one final LF. The outer pending JCS object embeds only the
  // semantic JSON object and is never a byte authority for this member.
  Hyprland::DesiredStateV2 candidateSnapshot;
  QByteArray candidateSnapshotBytes;

  QString snapshotDigest;
  AppliedRecordV2 afterActivation;
  std::optional<AppliedRecordV2> beforeActivation;

  friend bool operator==(const OrdinaryPendingRecordV2 &,
                         const OrdinaryPendingRecordV2 &) = default;
};

[[nodiscard]] CanonicalJson::Result<QByteArray>
serializeOrdinaryPendingRecordV2(
    const OrdinaryPendingRecordV2 &record, const Hyprland::Catalog &catalogV2,
    const Hyprland::ActionCatalog &actionCatalogV2);

[[nodiscard]] CanonicalJson::Result<OrdinaryPendingRecordV2>
parseOrdinaryPendingRecordV2(QByteArrayView bytes,
                             const Hyprland::Catalog &catalogV2,
                             const Hyprland::ActionCatalog &actionCatalogV2);

} // namespace HyprShelld::Compositor
