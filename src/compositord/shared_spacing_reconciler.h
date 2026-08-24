#pragma once

#include "shared_visual_source.h"

#include <QByteArray>
#include <QString>
#include <QtTypes>

#include <array>
#include <optional>

namespace HyprShelld::Compositor {

using SharedGap = std::array<qint64, 4>;

struct SharedSpacingValues final {
    SharedGap gapsIn{};
    SharedGap gapsOut{};

    friend bool operator==(
        const SharedSpacingValues &,
        const SharedSpacingValues &
    ) = default;
};

struct SharedSpacingEdit final {
    QByteArray candidate;
    bool changed = false;
    bool spacingChanged = false;
    bool protectedRuleChanged = false;
};

class SharedSpacingReconciler final {
public:
    [[nodiscard]] bool configure(
        const QByteArray &catalog,
        const QString &expectedDigest,
        QString &error
    );
    [[nodiscard]] bool configuredFor(const QString &digest) const;
    [[nodiscard]] SharedSpacingValues valuesFor(
        const SharedVisualProjection &projection
    ) const;
    [[nodiscard]] std::optional<SharedSpacingValues> resolvedValues(
        const QByteArray &snapshot,
        QString &error
    ) const;
    [[nodiscard]] std::optional<SharedSpacingEdit> edit(
        const QByteArray &snapshot,
        quint64 expectedRevision,
        const QString &expectedCatalogDigest,
        const SharedVisualProjection &projection,
        QString &error
    ) const;
    [[nodiscard]] bool replacementPreservesSpacing(
        const QByteArray &candidate,
        const SharedSpacingValues &values,
        QString &error
    ) const;
    [[nodiscard]] bool hasExactFinalProtectedRule(
        const QByteArray &candidate,
        QString &error
    ) const;

private:
    QString catalogDigest_;
    SharedGap gapsInDefault_{};
    SharedGap gapsOutDefault_{};
};

} // namespace HyprShelld::Compositor
