#include "component_plan_controller.h"

namespace HyprShelld {
namespace {

QString describeErrors(const Components::ValidationErrors &errors)
{
    QStringList details;
    details.reserve(errors.size());
    for (const auto &error : errors) {
        details.append(
            QStringLiteral("%1 [%2]: %3")
                .arg(error.path, error.code, error.message)
        );
    }
    return details.join(QStringLiteral("; "));
}

} // namespace

ComponentPlanController::ComponentPlanController(QObject *parent)
    : QObject(parent)
{
}

ComponentPlanController::State ComponentPlanController::state() const
{
    return state_;
}

QString ComponentPlanController::stateName() const
{
    switch (state_) {
    case State::Hydrating:
        return QStringLiteral("hydrating");
    case State::Authoritative:
        return QStringLiteral("authoritative");
    case State::Retained:
        return QStringLiteral("retained");
    case State::Unavailable:
        return QStringLiteral("unavailable");
    }

    Q_UNREACHABLE_RETURN(QString());
}

quint64 ComponentPlanController::revision() const
{
    return artifact_ ? artifact_->revision : 0;
}

QString ComponentPlanController::digest() const
{
    return artifact_ ? artifact_->digest : QString();
}

const Components::SurfacePlanArtifact *ComponentPlanController::artifact() const
{
    return artifact_ ? &*artifact_ : nullptr;
}

QString ComponentPlanController::lastError() const
{
    return lastError_;
}

void ComponentPlanController::beginHydration()
{
    const auto nextState = artifact_ ? State::Retained : State::Hydrating;
    if (state_ == nextState && lastError_.isEmpty()) {
        return;
    }

    state_ = nextState;
    lastError_.clear();
    emit runtimeChanged();
}

bool ComponentPlanController::acceptSnapshots(
    const Components::RuntimeCatalogSnapshot &catalog,
    const Components::RuntimeConfigurationSnapshot &configuration
)
{
    auto plan = Components::buildSurfacePlan(catalog, configuration);
    if (!plan) {
        setFailedState(describeErrors(plan.errors));
        return false;
    }

    auto artifact = Components::makeSurfacePlanArtifact(*plan.value);
    if (!artifact) {
        setFailedState(describeErrors(artifact.errors));
        return false;
    }

    const auto changed = !artifact_
        || artifact_->digest != artifact.value->digest
        || state_ != State::Authoritative
        || !lastError_.isEmpty();
    artifact_ = std::move(*artifact.value);
    state_ = State::Authoritative;
    lastError_.clear();
    if (changed) {
        emit runtimeChanged();
    }
    return true;
}

void ComponentPlanController::hydrationFailed(const QString &error)
{
    setFailedState(error);
}

void ComponentPlanController::sourceUnavailable(const QString &error)
{
    setFailedState(error);
}

void ComponentPlanController::setFailedState(const QString &error)
{
    const auto nextState = artifact_ ? State::Retained : State::Unavailable;
    if (state_ == nextState && lastError_ == error) {
        return;
    }

    state_ = nextState;
    lastError_ = error;
    emit runtimeChanged();
}

} // namespace HyprShelld
