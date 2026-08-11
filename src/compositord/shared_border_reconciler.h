#pragma once

#include "shared_border_source.h"

#include <QByteArray>
#include <QString>
#include <QtTypes>

#include <optional>

namespace HyprShelld::Compositor {

struct SharedBorderValues final {
    quint32 borderSize = 1;
    quint32 rounding = 0;

    friend bool operator==(
        const SharedBorderValues &,
        const SharedBorderValues &
    ) = default;
};

struct SharedBorderEdit final {
    QByteArray candidate;
    bool changed = false;
};

class SharedBorderReconciler final {
public:
    [[nodiscard]] bool configure(
        const QByteArray &catalog,
        const QString &expectedDigest,
        QString &error
    );
    [[nodiscard]] bool configuredFor(const QString &digest) const;
    [[nodiscard]] SharedBorderValues valuesFor(
        const SharedBorderProjection &projection
    ) const;
    [[nodiscard]] std::optional<SharedBorderValues> resolvedValues(
        const QByteArray &snapshot,
        QString &error
    ) const;
    [[nodiscard]] std::optional<SharedBorderEdit> edit(
        const QByteArray &snapshot,
        quint64 expectedRevision,
        const QString &expectedCatalogDigest,
        const SharedBorderProjection &projection,
        QString &error
    ) const;
    [[nodiscard]] bool replacementPreserves(
        const QByteArray &candidate,
        const SharedBorderValues &values,
        QString &error
    ) const;

private:
    QString catalogDigest_;
    quint32 borderDefault_ = 1;
    quint32 roundingDefault_ = 0;
};

} // namespace HyprShelld::Compositor
