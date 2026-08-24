#pragma once

#include <QByteArray>
#include <QJsonObject>
#include <QString>
#include <QVariantList>
#include <QVariantMap>
#include <QtTypes>

#include <optional>

namespace HyprShelld {

class CompositorOptionCatalog;
class CompositorActionCatalog;

struct CompositorSnapshotEdit final {
    QByteArray candidate;
    bool changed = false;
};

using AppearanceSnapshotEdit = CompositorSnapshotEdit;

class CompositorSnapshotEditor final {
public:
    [[nodiscard]] static bool isExactV1Envelope(
        const QJsonObject &snapshot,
        qulonglong expectedRevision,
        const QString &expectedCatalogDigest,
        const QString &expectedActionCatalogDigest
    );

    [[nodiscard]] static std::optional<CompositorSnapshotEdit> replaceAllOptions(
        const QJsonObject &snapshot,
        qulonglong expectedRevision,
        const QString &expectedCatalogDigest,
        const QString &expectedActionCatalogDigest,
        const CompositorOptionCatalog &catalog,
        const CompositorActionCatalog &actionCatalog,
        const QVariantMap &values,
        QString &error
    );

    [[nodiscard]] static std::optional<CompositorSnapshotEdit> replaceBindings(
        const QJsonObject &snapshot,
        qulonglong expectedRevision,
        const QString &expectedCatalogDigest,
        const QString &expectedActionCatalogDigest,
        const CompositorOptionCatalog &catalog,
        const CompositorActionCatalog &actionCatalog,
        const QVariantList &bindings,
        const QVariantList &submaps,
        QString &error
    );

    [[nodiscard]] static std::optional<CompositorSnapshotEdit> replaceEnvironment(
        const QJsonObject &snapshot,
        qulonglong expectedRevision,
        const QString &expectedCatalogDigest,
        const QString &expectedActionCatalogDigest,
        const CompositorOptionCatalog &catalog,
        const CompositorActionCatalog &actionCatalog,
        const QVariantList &environment,
        QString &error
    );

    [[nodiscard]] static std::optional<CompositorSnapshotEdit> replacePermissions(
        const QJsonObject &snapshot,
        qulonglong expectedRevision,
        const QString &expectedCatalogDigest,
        const QString &expectedActionCatalogDigest,
        const CompositorOptionCatalog &catalog,
        const CompositorActionCatalog &actionCatalog,
        const QVariantList &permissions,
        QString &error
    );

    [[nodiscard]] static std::optional<CompositorSnapshotEdit> replaceInputDevices(
        const QJsonObject &snapshot,
        qulonglong expectedRevision,
        const QString &expectedCatalogDigest,
        const QString &expectedActionCatalogDigest,
        const CompositorOptionCatalog &catalog,
        const CompositorActionCatalog &actionCatalog,
        const QVariantList &devices,
        QString &error
    );

    [[nodiscard]] static std::optional<AppearanceSnapshotEdit> replaceAppearance(
        const QJsonObject &snapshot,
        qulonglong expectedRevision,
        const QString &expectedCatalogDigest,
        const QString &expectedActionCatalogDigest,
        const CompositorOptionCatalog &catalog,
        const CompositorActionCatalog &actionCatalog,
        const QVariantMap &values,
        const QVariantList &curves,
        const QVariantList &animations,
        QString &error
    );

    [[nodiscard]] static std::optional<CompositorSnapshotEdit> replaceInput(
        const QJsonObject &snapshot,
        qulonglong expectedRevision,
        const QString &expectedCatalogDigest,
        const QString &expectedActionCatalogDigest,
        const CompositorOptionCatalog &catalog,
        const CompositorActionCatalog &actionCatalog,
        const QVariantMap &values,
        const QVariantList &gestures,
        QString &error
    );

    [[nodiscard]] static std::optional<CompositorSnapshotEdit> replaceWindows(
        const QJsonObject &snapshot,
        qulonglong expectedRevision,
        const QString &expectedCatalogDigest,
        const QString &expectedActionCatalogDigest,
        const CompositorOptionCatalog &catalog,
        const CompositorActionCatalog &actionCatalog,
        const QVariantMap &values,
        QString &error
    );

    [[nodiscard]] static std::optional<CompositorSnapshotEdit> replaceWorkspaces(
        const QJsonObject &snapshot,
        qulonglong expectedRevision,
        const QString &expectedCatalogDigest,
        const QString &expectedActionCatalogDigest,
        const CompositorOptionCatalog &catalog,
        const CompositorActionCatalog &actionCatalog,
        const QVariantMap &values,
        const QVariantList &workspaceRules,
        QString &error
    );

    [[nodiscard]] static std::optional<CompositorSnapshotEdit> replaceAdvanced(
        const QJsonObject &snapshot,
        qulonglong expectedRevision,
        const QString &expectedCatalogDigest,
        const QString &expectedActionCatalogDigest,
        const CompositorOptionCatalog &catalog,
        const CompositorActionCatalog &actionCatalog,
        const QVariantMap &values,
        QString &error
    );

    [[nodiscard]] static std::optional<CompositorSnapshotEdit> replaceRules(
        const QJsonObject &snapshot,
        qulonglong expectedRevision,
        const QString &expectedCatalogDigest,
        const QString &expectedActionCatalogDigest,
        const CompositorOptionCatalog &catalog,
        const CompositorActionCatalog &actionCatalog,
        const QVariantList &windowRules,
        const QVariantList &layerRules,
        QString &error
    );
};

} // namespace HyprShelld
