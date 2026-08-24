#pragma once

#include "renderer.h"
#include "store.h"

#include <QByteArray>
#include <QString>

#include <optional>

namespace HyprShelld::Compositor {

inline constexpr auto initialUserCustomContents =
    "-- This file is user-owned and is loaded last by HyprShelld.\n"
    "-- HyprShelld will never overwrite or delete this file.\n\n";

struct VerifiedGeneration final {
    QString id;
    QString nonce;
    QString directory;
    QString entrypoint;
    QByteArray manifest;
    QString snapshotDigest;
    quint64 revision = 0;
};

struct GenerationResult final {
    bool success = false;
    QString errorCode;
    QString errorMessage;
    std::optional<VerifiedGeneration> generation;
};

// Pure qualification-time validation. This deliberately has no filesystem or
// GenerationStore path, so a dormant v2 artifact cannot be published by the
// active v1 store.
[[nodiscard]] Hyprland::ValidationErrors validateDormantGenerationV2(
    const DormantRenderedGenerationV2 &rendered,
    const Hyprland::DesiredStateV2 &state,
    const Hyprland::Catalog &catalogV2,
    const Hyprland::ActionCatalog &actionCatalogV2
);

class GenerationStore final {
public:
    explicit GenerationStore(PersistentStore &store);
    ~GenerationStore();

    GenerationStore(const GenerationStore &) = delete;
    GenerationStore &operator=(const GenerationStore &) = delete;

    [[nodiscard]] GenerationResult initialize();
    void shutdown() noexcept;
    [[nodiscard]] QString directoryForNonce(const QString &nonce) const;
    [[nodiscard]] GenerationResult publish(
        const RenderedGeneration &rendered
    );
    [[nodiscard]] GenerationResult verify(const QString &nonce) const;
    [[nodiscard]] int directoryFd() const { return generationsDirectoryFd_; }
    [[nodiscard]] bool directoryStillNamed() const;

private:
    PersistentStore &store_;
    int generationsDirectoryFd_ = -1;
};

} // namespace HyprShelld::Compositor
