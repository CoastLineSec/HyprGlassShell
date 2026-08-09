#pragma once

#include "validation_result.h"

#include <QByteArray>
#include <QByteArrayView>
#include <QHash>
#include <QJsonObject>
#include <QString>
#include <QStringList>
#include <QtTypes>

namespace HyprShelld::Components {

inline constexpr qsizetype maximumSurfacePlanBytes = 4 * 1024 * 1024;
inline constexpr qsizetype maximumSurfacePlanInstances = 512;
inline constexpr qsizetype maximumSurfacePlanBarLayouts = 64;
inline constexpr quint32 defaultDeclarativeMaximumWidth = 240;

struct SurfaceInstance final {
    QString componentId;
    QString componentType;
    QString packageDigest;
    QString runtimeKind;
    QString factory;
    QString declarativeText;
    QString declarativeTooltip;
    quint32 declarativeMaximumWidth = 0;
    QJsonObject settings;

    friend bool operator==(const SurfaceInstance &, const SurfaceInstance &) = default;
};

struct SurfaceBarLayout final {
    QString outputMode;
    QStringList start;
    QStringList center;
    QStringList end;

    friend bool operator==(
        const SurfaceBarLayout &,
        const SurfaceBarLayout &
    ) = default;
};

struct SurfacePlan final {
    QString catalogDigest;
    quint64 configurationRevision = 0;
    QHash<QString, SurfaceInstance> instances;
    QHash<QString, SurfaceBarLayout> barLayouts;

    friend bool operator==(const SurfacePlan &, const SurfacePlan &) = default;
};

struct SurfacePlanArtifact final {
    SurfacePlan plan;
    QByteArray bytes;
    QString digest;
    quint64 revision = 0;

    friend bool operator==(
        const SurfacePlanArtifact &,
        const SurfacePlanArtifact &
    ) = default;
};

[[nodiscard]] ValidationResult<SurfacePlan> parseSurfacePlan(
    QByteArrayView bytes
);

[[nodiscard]] ValidationResult<SurfacePlanArtifact> makeSurfacePlanArtifact(
    const SurfacePlan &plan
);

[[nodiscard]] QString surfacePlanDigest(QByteArrayView bytes);
[[nodiscard]] quint64 surfacePlanRevision(const QString &digest);

} // namespace HyprShelld::Components
