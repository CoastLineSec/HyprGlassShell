#pragma once

#include "component/surface_plan.h"
#include "component_plan_builder.h"
#include "component_runtime_health_store.h"

#include <QObject>
#include <QString>

#include <memory>
#include <optional>

class QTimer;

namespace HyprShelld {

inline constexpr int componentActivationProbationMs = 8000;

class ComponentPlanController final : public QObject {
    Q_OBJECT

public:
    enum class State {
        Hydrating,
        Authoritative,
        Retained,
        Unavailable,
    };

    explicit ComponentPlanController(QObject *parent = nullptr);
    ComponentPlanController(
        ComponentRuntimeHealthPaths healthPaths,
        QObject *parent = nullptr
    );
    ComponentPlanController(
        ComponentRuntimeHealthPaths healthPaths,
        ComponentRuntimeHealthStore::PersistFaultInjector faultInjector,
        QObject *parent = nullptr
    );

    [[nodiscard]] State state() const;
    [[nodiscard]] QString stateName() const;
    [[nodiscard]] quint64 revision() const;
    [[nodiscard]] QString digest() const;
    [[nodiscard]] const Components::SurfacePlanArtifact *artifact() const;
    [[nodiscard]] QString lastError() const;
    [[nodiscard]] quint64 runtimeHealthRevision() const;
    [[nodiscard]] bool thirdPartySafeMode() const;
    [[nodiscard]] QList<ComponentRuntimeHealthRecord> runtimeHealthRecords() const;

    [[nodiscard]] bool initializeRuntimeHealth(QString &error);
    [[nodiscard]] bool activationStable(
        const QString &instanceId,
        const QString &componentId,
        const QString &packageDigest,
        quint64 surfacePlanRevision,
        QString &error
    );
    [[nodiscard]] bool authorizeSurfacePlan(
        quint64 surfacePlanRevision,
        QString &error
    );
    [[nodiscard]] bool cancelSurfacePlanAuthorization(
        quint64 surfacePlanRevision,
        QString &error
    );
    [[nodiscard]] bool activationFailed(
        const QString &instanceId,
        const QString &componentId,
        const QString &packageDigest,
        quint64 surfacePlanRevision,
        const QString &reason,
        QString &error
    );
    [[nodiscard]] bool retryComponent(
        const QString &componentId,
        const QString &packageDigest,
        quint64 expectedRuntimeHealthRevision,
        QString &error
    );

    void beginHydration();
    [[nodiscard]] bool acceptSnapshots(
        const Components::RuntimeCatalogSnapshot &catalog,
        const Components::RuntimeConfigurationSnapshot &configuration
    );
    void hydrationFailed(const QString &error);
    void sourceUnavailable(const QString &error);

signals:
    void runtimeChanged();
    void runtimeHealthChanged();

private:
    void setFailedState(const QString &error);
    bool publishPlan(
        Components::SurfacePlan plan,
        State state,
        bool allowThirdParty,
        const QString &error
    );
    void stripThirdParty(Components::SurfacePlan &plan) const;
    void filterQuarantined(Components::SurfacePlan &plan) const;
    bool preparePending(
        const Components::SurfacePlan &plan,
        QString &error
    );
    [[nodiscard]] bool commitHealth(
        ComponentRuntimeHealthState next,
        QString &error
    );
    void quarantinePending(const QString &reason);
    void rebuildAuthoritativePlan();
    [[nodiscard]] const Components::SurfaceInstance *matchingInstance(
        const QString &instanceId,
        const QString &componentId,
        const QString &packageDigest,
        quint64 surfacePlanRevision
    ) const;

    State state_ = State::Hydrating;
    std::optional<Components::SurfacePlanArtifact> artifact_;
    std::optional<Components::RuntimeCatalogSnapshot> catalog_;
    std::optional<Components::RuntimeConfigurationSnapshot> configuration_;
    std::unique_ptr<ComponentRuntimeHealthStore> healthStore_;
    ComponentRuntimeHealthState health_;
    QTimer *probationTimer_ = nullptr;
    bool healthInitialized_ = true;
    QString lastError_;
};

} // namespace HyprShelld
