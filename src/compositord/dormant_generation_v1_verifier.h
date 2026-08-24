#pragma once

#include "renderer.h"

#include "hyprland/action_catalog.h"
#include "hyprland/catalog.h"
#include "hyprland/validation.h"

#include <QByteArray>
#include <QByteArrayView>
#include <QJsonObject>
#include <QMap>
#include <QString>
#include <QtTypes>

namespace HyprShelld::Compositor {

// Every path-dependent renderer input and every immutable generation identity
// is explicit. This is qualification input, not a publication descriptor.
struct DormantGenerationV1Expectation final {
    quint64 revision = 0;
    QString snapshotDigest;
    QString generation;
    QString activationNonce;
    QString generationRoot;
    QString userCustomPath;
};

// A migration verifier returns only its fresh, byte-exact reconstruction.
// Deliberately unlike RenderedGeneration, this type has no activation
// requirement and cannot be passed to the active v1 GenerationStore.
struct VerifiedDormantGenerationV1 final {
    QString generation;
    QString snapshotDigest;
    QString activationNonce;
    QString createdAt;
    QString entrypoint = QStringLiteral("hyprland.lua");
    QMap<QString, GeneratedFile> files;
    QJsonObject manifest;
    QByteArray manifestBytes;
    QString generationRoot;
    QString userCustomPath;
};

// Pure migration-time qualification of recovered v1 bytes. The desired-state
// snapshot domain is exactDesiredBytes in full, including its sole final LF.
// The supplied file map contains exact untrusted generation-relative bytes.
// This function performs no filesystem, Store, publication, RNG, D-Bus,
// journal, v2, or activation operation.
[[nodiscard]] Hyprland::ValidationResult<VerifiedDormantGenerationV1>
verifyDormantGenerationV1ForMigration(
    QByteArrayView exactDesiredBytes,
    QByteArrayView manifestBytes,
    const QMap<QString, QByteArray> &fileBytes,
    const DormantGenerationV1Expectation &expected,
    const Hyprland::Catalog &currentV1Catalog,
    const Hyprland::ActionCatalog &currentV1Actions
);

} // namespace HyprShelld::Compositor
