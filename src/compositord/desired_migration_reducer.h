#pragma once

#include "hyprland/action_catalog.h"
#include "hyprland/catalog.h"
#include "hyprland/desired_state.h"

#include <QByteArray>
#include <QByteArrayView>
#include <QString>
#include <QtTypes>

#include <optional>

namespace HyprShelld::Compositor {

enum class DesiredMigrationDisposition {
  Eligible,
  UnsupportedNewerPatch,
  InvalidV1,
  UnqualifiedEvidence,
};

enum class DesiredMigrationRoute {
  None,
  M01ThenM12,
  M12,
  M22,
};

struct ExactDesiredV1Observation final {
  quint64 revision = 0;
  quint32 sourcePatch = 0;

  friend bool operator==(const ExactDesiredV1Observation &,
                         const ExactDesiredV1Observation &) = default;
};

// This plan is an in-memory inspection product. It deliberately exposes only
// the immutable source evidence and closed dispatch result. The normalized v1
// parser product is private so a current-digest or 0.56.1 intermediate cannot
// become a persistence format or public repair explanation.
class DesiredMigrationPlan final {
public:
  [[nodiscard]] DesiredMigrationDisposition disposition() const;
  [[nodiscard]] DesiredMigrationRoute route() const;
  [[nodiscard]] quint32 sourcePatch() const;
  [[nodiscard]] const QByteArray &sourceBytes() const;
  [[nodiscard]] const QString &sourceBytesSha256() const;

private:
  DesiredMigrationDisposition disposition_ =
      DesiredMigrationDisposition::InvalidV1;
  DesiredMigrationRoute route_ = DesiredMigrationRoute::None;
  quint32 sourcePatch_ = 0;
  QByteArray sourceBytes_;
  QString sourceBytesSha256_;
  // Populated only for an Eligible result after every frozen source and
  // destination authority has passed. Other dispositions retain no
  // normalized migration product, even privately.
  std::optional<Hyprland::DesiredState> normalizedState_;

  friend DesiredMigrationPlan inspectDesiredV1ToV2Migration(
      QByteArrayView, QByteArrayView, QByteArrayView, const Hyprland::Catalog &,
      const Hyprland::ActionCatalog &, const Hyprland::Catalog &,
      const Hyprland::ActionCatalog &);
  friend std::optional<struct DesiredMigrationMaterialization>
  materializeDesiredV1ToV2Migration(const DesiredMigrationPlan &, QString,
                                    QByteArrayView, QByteArrayView,
                                    const Hyprland::Catalog &,
                                    const Hyprland::ActionCatalog &,
                                    const Hyprland::Catalog &,
                                    const Hyprland::ActionCatalog &);
};

struct DesiredMigrationMaterialization final {
  DesiredMigrationRoute route = DesiredMigrationRoute::None;
  Hyprland::DesiredStateV2 state;
  QByteArray bytes;
  // The recovered live-v1 snapshot domain is the complete exact canonical
  // stored input, including its sole final LF.
  QString sourceV1StoredBytesSha256;
  // The destination stored-byte receipt also includes its sole final LF.
  // It is deliberately distinct from the v2 snapshot domain below.
  QString destinationV2StoredBytesSha256;
  // The destination-v2 snapshot domain hashes the exact output with only
  // its sole final LF removed. Migration recomputes it; it is never copied
  // or compared across the v1/v2 domains.
  QString destinationV2SnapshotDigest;

  friend bool operator==(const DesiredMigrationMaterialization &,
                         const DesiredMigrationMaterialization &) = default;
};

// Pure source-only inspection. The returned value proves exact canonical v1
// bytes against one of the frozen predecessor digest pairs and exposes only
// the revision and canonical 0.56 patch. No input bytes or parser product are
// retained by the observation.
[[nodiscard]] std::optional<ExactDesiredV1Observation>
inspectExactDesiredV1Observation(
    QByteArrayView sourceBytes, const Hyprland::Catalog &catalogV1,
    const Hyprland::ActionCatalog &actionCatalogV1);

// Pure inspection only: no file access, ID creation, generation planning,
// activation classification, or effects. For a fully valid patch 3+ source,
// destination evidence is intentionally not consulted.
[[nodiscard]] DesiredMigrationPlan inspectDesiredV1ToV2Migration(
    QByteArrayView sourceBytes, QByteArrayView migrationManifestBytes,
    QByteArrayView sourceManifestV2Bytes, const Hyprland::Catalog &catalogV1,
    const Hyprland::ActionCatalog &actionCatalogV1,
    const Hyprland::Catalog &catalogV2,
    const Hyprland::ActionCatalog &actionCatalogV2);

// Re-inspects the retained immutable source and all supplied authorities. The
// caller supplies the already-minted canonical authority ID. Activation
// records are outside this reducer: later planning must recompute them with
// the extracted classifier and must never copy v1 activation metadata.
[[nodiscard]] std::optional<DesiredMigrationMaterialization>
materializeDesiredV1ToV2Migration(
    const DesiredMigrationPlan &plan, QString authorityId,
    QByteArrayView migrationManifestBytes, QByteArrayView sourceManifestV2Bytes,
    const Hyprland::Catalog &catalogV1,
    const Hyprland::ActionCatalog &actionCatalogV1,
    const Hyprland::Catalog &catalogV2,
    const Hyprland::ActionCatalog &actionCatalogV2);

} // namespace HyprShelld::Compositor
