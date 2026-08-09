#pragma once

#include "component/surface_plan.h"
#include "component_plan_builder.h"

#include <QObject>
#include <QString>

#include <optional>

namespace HyprShelld {

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

    [[nodiscard]] State state() const;
    [[nodiscard]] QString stateName() const;
    [[nodiscard]] quint64 revision() const;
    [[nodiscard]] QString digest() const;
    [[nodiscard]] const Components::SurfacePlanArtifact *artifact() const;
    [[nodiscard]] QString lastError() const;

    void beginHydration();
    [[nodiscard]] bool acceptSnapshots(
        const Components::RuntimeCatalogSnapshot &catalog,
        const Components::RuntimeConfigurationSnapshot &configuration
    );
    void hydrationFailed(const QString &error);
    void sourceUnavailable(const QString &error);

signals:
    void runtimeChanged();

private:
    void setFailedState(const QString &error);

    State state_ = State::Hydrating;
    std::optional<Components::SurfacePlanArtifact> artifact_;
    QString lastError_;
};

} // namespace HyprShelld
