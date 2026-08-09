#pragma once

#include "component/surface_plan.h"

#include <QDBusConnection>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>
#include <QtQml/qqmlregistration.h>

#include <optional>

class QDBusServiceWatcher;

namespace HyprShelld {

class ComponentRuntimeClient final : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
    Q_PROPERTY(bool available READ available NOTIFY availableChanged)
    Q_PROPERTY(bool planCurrent READ planCurrent NOTIFY planStateChanged)
    Q_PROPERTY(bool usingFallback READ usingFallback NOTIFY planChanged)
    Q_PROPERTY(qulonglong planRevision READ planRevision NOTIFY planChanged)
    Q_PROPERTY(QString planDigest READ planDigest NOTIFY planChanged)
    Q_PROPERTY(QString planState READ planState NOTIFY planStateChanged)
    Q_PROPERTY(QString lastError READ lastError NOTIFY lastErrorChanged)

public:
    explicit ComponentRuntimeClient(QObject *parent = nullptr);
    ComponentRuntimeClient(QDBusConnection connection, QObject *parent);

    [[nodiscard]] bool available() const;
    [[nodiscard]] bool planCurrent() const;
    [[nodiscard]] bool usingFallback() const;
    [[nodiscard]] qulonglong planRevision() const;
    [[nodiscard]] QString planDigest() const;
    [[nodiscard]] QString planState() const;
    [[nodiscard]] QString lastError() const;

    Q_INVOKABLE QVariantList barInstances(
        const QString &layoutId,
        const QString &outputName,
        const QString &region
    ) const;

signals:
    void availableChanged();
    void planStateChanged();
    void planChanged();
    void lastErrorChanged();

private slots:
    void propertiesChanged(
        const QString &interfaceName,
        const QVariantMap &changed,
        const QStringList &invalidated
    );
    void serviceOwnerChanged(
        const QString &name,
        const QString &oldOwner,
        const QString &newOwner
    );

private:
    void refreshProperties();
    bool applyProperties(const QVariantMap &properties, bool requireComplete);
    void fetchPlan(quint64 revision, const QString &digest);
    void acceptPlan(
        Components::SurfacePlan plan,
        quint64 revision,
        const QString &digest
    );
    void setAvailable(bool available);
    void setLastError(const QString &error);
    void invalidateRuntime(const QString &error);
    void publishStateIfChanged(bool previousCurrent, const QString &previousState);
    [[nodiscard]] QVariantList fallbackBarInstances(
        const QString &layoutId,
        const QString &region
    ) const;
    [[nodiscard]] QVariantMap instanceMap(
        const QString &instanceId,
        const Components::SurfaceInstance &instance,
        bool compiledFallback
    ) const;

    QDBusConnection connection_;
    QDBusServiceWatcher *serviceWatcher_ = nullptr;
    quint64 ownerGeneration_ = 0;
    quint64 refreshGeneration_ = 0;
    bool available_ = false;
    bool usingFallback_ = true;
    quint64 serverRevision_ = 0;
    QString serverDigest_;
    QString serverState_ = QStringLiteral("hydrating");
    quint64 acceptedRevision_ = 0;
    QString acceptedDigest_;
    std::optional<Components::SurfacePlan> acceptedPlan_;
    QString lastError_;
};

} // namespace HyprShelld
