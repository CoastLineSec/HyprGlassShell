#pragma once

#include "renderer.h"

#include "hyprland/action_catalog.h"
#include "hyprland/catalog.h"
#include "hyprland/desired_state.h"
#include "hyprland/validation.h"

#include <QByteArray>
#include <QByteArrayView>
#include <QMap>
#include <QString>
#include <QtTypes>

namespace HyprShelld::Compositor {

// Every path-dependent renderer input is explicit. This expectation is not a
// publication descriptor and has no filesystem authority.
struct DormantGenerationV2Expectation final {
    QString authorityId;
    quint64 revision = 0;
    QString snapshotDigest;
    QString generation;
    QString activationNonce;
    QString generationRoot;
    QString userCustomPath;
};

// The rendered product is always the verifier's fresh byte-exact rerender,
// never an activation requirement or metadata field supplied by the caller.
struct VerifiedDormantGenerationV2 final {
    DormantRenderedGenerationV2 rendered;
    QString generationRoot;
    QString userCustomPath;
};

// Pure byte-level qualification. The file map contains the exact untrusted
// bytes keyed by generation-relative path. This function neither reads nor
// writes the filesystem and cannot select, publish, or activate a generation.
[[nodiscard]] Hyprland::ValidationResult<VerifiedDormantGenerationV2>
verifyDormantGenerationV2(
    QByteArrayView manifestBytes,
    const QMap<QString, QByteArray> &fileBytes,
    const DormantGenerationV2Expectation &expected,
    const Hyprland::DesiredStateV2 &state,
    const Hyprland::Catalog &catalogV2,
    const Hyprland::ActionCatalog &actionCatalogV2
);

} // namespace HyprShelld::Compositor
