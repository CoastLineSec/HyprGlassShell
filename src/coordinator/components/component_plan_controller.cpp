#include "component_plan_controller.h"

#include <QDebug>
#include <QSet>
#include <QTimer>

#include <algorithm>
#include <limits>
#include <tuple>
#include <utility>

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

void removeInstances(
    Components::SurfacePlan &plan,
    const QSet<QString> &removed
)
{
    if (removed.isEmpty()) {
        return;
    }
    for (const auto &instanceId : removed) {
        plan.instances.remove(instanceId);
    }
    const auto filter = [&removed](QStringList &instances) {
        instances.removeIf([&removed](const QString &instanceId) {
            return removed.contains(instanceId);
        });
    };
    for (auto iterator = plan.barLayouts.begin();
         iterator != plan.barLayouts.end(); ++iterator) {
        filter(iterator->start);
        filter(iterator->center);
        filter(iterator->end);
    }
}

} // namespace

ComponentPlanController::ComponentPlanController(QObject *parent)
    : QObject(parent)
{
    probationTimer_ = new QTimer(this);
    probationTimer_->setSingleShot(true);
    probationTimer_->setInterval(componentActivationProbationMs);
    connect(probationTimer_, &QTimer::timeout, this, [this] {
        quarantinePending(QStringLiteral("timeout"));
    });
}

ComponentPlanController::ComponentPlanController(
    ComponentRuntimeHealthPaths healthPaths,
    QObject *parent
)
    : ComponentPlanController(
        std::move(healthPaths),
        ComponentRuntimeHealthStore::PersistFaultInjector{},
        parent
    )
{
}

ComponentPlanController::ComponentPlanController(
    ComponentRuntimeHealthPaths healthPaths,
    ComponentRuntimeHealthStore::PersistFaultInjector faultInjector,
    QObject *parent
)
    : ComponentPlanController(parent)
{
    healthStore_ = std::make_unique<ComponentRuntimeHealthStore>(
        std::move(healthPaths),
        std::move(faultInjector)
    );
    healthInitialized_ = false;
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

quint64 ComponentPlanController::runtimeHealthRevision() const
{
    return health_.revision;
}

bool ComponentPlanController::thirdPartySafeMode() const
{
    return health_.safeMode || !healthInitialized_;
}

QList<ComponentRuntimeHealthRecord>
ComponentPlanController::runtimeHealthRecords() const
{
    QList<ComponentRuntimeHealthRecord> result = health_.records.values();
    QSet<QString> represented;
    for (const auto &record : result) {
        represented.insert(ComponentRuntimeHealthStore::recordKey(
            record.componentId,
            record.packageDigest
        ));
    }
    for (const auto &activation : health_.pending) {
        const auto key = ComponentRuntimeHealthStore::recordKey(
            activation.componentId,
            activation.packageDigest
        );
        if (represented.contains(key)) {
            continue;
        }
        represented.insert(key);
        result.append({
            .componentId = activation.componentId,
            .packageDigest = activation.packageDigest,
            .state = QStringLiteral("probation"),
            .reason = {},
            .failureCount = 0,
        });
    }
    std::ranges::sort(result, [](const auto &left, const auto &right) {
        return std::tie(left.componentId, left.packageDigest)
            < std::tie(right.componentId, right.packageDigest);
    });
    return result;
}

bool ComponentPlanController::initializeRuntimeHealth(QString &error)
{
    if (healthInitialized_) {
        error.clear();
        return true;
    }
    const auto loaded = healthStore_->load();
    health_ = loaded.state;
    healthInitialized_ = true;
    if (!loaded.success) {
        health_.safeMode = true;
        error = loaded.error;
        emit runtimeHealthChanged();
        return false;
    }

    if (!health_.pending.isEmpty()) {
        auto next = health_;
        for (const auto &activation : std::as_const(next.pending)) {
            const auto key = ComponentRuntimeHealthStore::recordKey(
                activation.componentId,
                activation.packageDigest
            );
            const auto previous = next.records.value(key);
            next.records.insert(key, {
                .componentId = activation.componentId,
                .packageDigest = activation.packageDigest,
                .state = QStringLiteral("quarantined"),
                .reason = QStringLiteral("incomplete-startup"),
                .failureCount = qMax<quint32>(1, previous.failureCount + 1),
            });
        }
        next.pending.clear();
        if (!commitHealth(std::move(next), error)) {
            return false;
        }
    } else {
        emit runtimeHealthChanged();
    }
    error.clear();
    return true;
}

void ComponentPlanController::beginHydration()
{
    if (artifact_) {
        auto retained = artifact_->plan;
        publishPlan(
            std::move(retained),
            State::Retained,
            false,
            QString()
        );
        return;
    }
    const auto changed = state_ != State::Hydrating || !lastError_.isEmpty();
    state_ = State::Hydrating;
    lastError_.clear();
    if (changed) {
        emit runtimeChanged();
    }
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

    catalog_ = catalog;
    configuration_ = configuration;
    return publishPlan(
        std::move(*plan.value),
        State::Authoritative,
        true,
        QString()
    );
}

void ComponentPlanController::hydrationFailed(const QString &error)
{
    setFailedState(error);
}

void ComponentPlanController::sourceUnavailable(const QString &error)
{
    setFailedState(error);
}

bool ComponentPlanController::publishPlan(
    Components::SurfacePlan plan,
    const State state,
    const bool allowThirdParty,
    const QString &error
)
{
    if (!allowThirdParty || thirdPartySafeMode()) {
        stripThirdParty(plan);
    } else {
        filterQuarantined(plan);
    }

    QString healthError;
    if (!preparePending(plan, healthError)) {
        stripThirdParty(plan);
    }

    auto artifact = Components::makeSurfacePlanArtifact(plan);
    if (!artifact) {
        const auto artifactError = describeErrors(artifact.errors);
        if (state == State::Authoritative) {
            setFailedState(artifactError);
        }
        return false;
    }

    const auto changed = !artifact_
        || artifact_->digest != artifact.value->digest
        || state_ != state
        || lastError_ != error;
    artifact_ = std::move(*artifact.value);
    state_ = state;
    lastError_ = !healthError.isEmpty() ? healthError : error;
    if (changed || !healthError.isEmpty()) {
        emit runtimeChanged();
    }
    return healthError.isEmpty();
}

void ComponentPlanController::stripThirdParty(
    Components::SurfacePlan &plan
) const
{
    QSet<QString> removed;
    for (auto iterator = plan.instances.cbegin();
         iterator != plan.instances.cend(); ++iterator) {
        if (iterator->runtimeKind != QStringLiteral("builtin-v1")) {
            removed.insert(iterator.key());
        }
    }
    removeInstances(plan, removed);
}

void ComponentPlanController::filterQuarantined(
    Components::SurfacePlan &plan
) const
{
    QSet<QString> removed;
    for (auto iterator = plan.instances.cbegin();
         iterator != plan.instances.cend(); ++iterator) {
        if (iterator->runtimeKind == QStringLiteral("builtin-v1")) {
            continue;
        }
        const auto key = ComponentRuntimeHealthStore::recordKey(
            iterator->componentId,
            iterator->packageDigest
        );
        const auto record = health_.records.constFind(key);
        if (record != health_.records.cend()
            && record->state == QStringLiteral("quarantined")) {
            removed.insert(iterator.key());
        }
    }
    removeInstances(plan, removed);
}

bool ComponentPlanController::preparePending(
    const Components::SurfacePlan &plan,
    QString &error
)
{
    auto next = health_;
    if (artifact_ && artifact_->plan != plan) {
        next.pending.clear();
    }
    for (auto iterator = next.pending.begin();
         iterator != next.pending.end();) {
        const auto instance = plan.instances.constFind(iterator.key());
        if (instance == plan.instances.cend()
            || instance->runtimeKind != QStringLiteral("declarative-v1")
            || instance->componentId != iterator->componentId
            || instance->packageDigest != iterator->packageDigest) {
            iterator = next.pending.erase(iterator);
        } else {
            ++iterator;
        }
    }

    if (next == health_) {
        if (health_.pending.isEmpty()) {
            probationTimer_->stop();
        } else if (!probationTimer_->isActive()) {
            probationTimer_->start();
        }
        error.clear();
        return true;
    }
    if (!commitHealth(std::move(next), error)) {
        probationTimer_->stop();
        return false;
    }
    if (health_.pending.isEmpty()) {
        probationTimer_->stop();
    } else {
        probationTimer_->start();
    }
    return true;
}

bool ComponentPlanController::commitHealth(
    ComponentRuntimeHealthState next,
    QString &error
)
{
    QSet<QString> packageStates;
    for (auto iterator = next.records.cbegin();
         iterator != next.records.cend(); ++iterator) {
        packageStates.insert(iterator.key());
    }
    for (const auto &activation : std::as_const(next.pending)) {
        packageStates.insert(ComponentRuntimeHealthStore::recordKey(
            activation.componentId,
            activation.packageDigest
        ));
    }
    if (health_.revision == std::numeric_limits<quint64>::max()
        || next.records.size() > 512 || next.pending.size() > 512
        || packageStates.size() > 512) {
        health_.safeMode = true;
        probationTimer_->stop();
        error = QStringLiteral("Runtime health storage bounds are exhausted");
        emit runtimeHealthChanged();
        return false;
    }
    next.revision = health_.revision + 1;
    if (healthStore_) {
        const auto persisted = healthStore_->persist(next);
        if (!persisted.durable()) {
            health_.safeMode = true;
            probationTimer_->stop();
            error = persisted.error;
            emit runtimeHealthChanged();
            return false;
        }
        if (persisted.durability
            == ComponentRuntimeHealthPersistDurability::RecoveryDurable) {
            qWarning().noquote()
                << "Runtime health recovery committed; active mirror will be"
                   " repaired on the next load:"
                << persisted.error;
        }
    }
    health_ = std::move(next);
    emit runtimeHealthChanged();
    error.clear();
    return true;
}

bool ComponentPlanController::authorizeSurfacePlan(
    const quint64 surfacePlanRevision,
    QString &error
)
{
    if (thirdPartySafeMode() || state_ != State::Authoritative
        || !artifact_ || artifact_->revision != surfacePlanRevision) {
        error = QStringLiteral("The surface plan is stale or unavailable");
        return false;
    }

    auto next = health_;
    QSet<QString> renderableIds;
    const auto mainLayout = artifact_->plan.barLayouts.constFind(
        QStringLiteral("main")
    );
    if (mainLayout != artifact_->plan.barLayouts.cend()) {
        for (const auto &instanceId : mainLayout->start) {
            renderableIds.insert(instanceId);
        }
        for (const auto &instanceId : mainLayout->center) {
            renderableIds.insert(instanceId);
        }
        for (const auto &instanceId : mainLayout->end) {
            renderableIds.insert(instanceId);
        }
    }
    for (const auto &instanceId : renderableIds) {
        const auto iterator = artifact_->plan.instances.constFind(instanceId);
        if (iterator == artifact_->plan.instances.cend()) {
            error = QStringLiteral("The main surface plan is inconsistent");
            return false;
        }
        if (iterator->runtimeKind != QStringLiteral("declarative-v1")) {
            continue;
        }
        const auto key = ComponentRuntimeHealthStore::recordKey(
            iterator->componentId,
            iterator->packageDigest
        );
        const auto record = health_.records.constFind(key);
        if (record != health_.records.cend()
            && record->state == QStringLiteral("quarantined")) {
            error = QStringLiteral("The surface plan contains a quarantined package");
            return false;
        }
        const auto pending = next.pending.constFind(instanceId);
        if (pending != next.pending.cend()
            && (pending->componentId != iterator->componentId
                || pending->packageDigest != iterator->packageDigest)) {
            error = QStringLiteral("A pending activation belongs to another package");
            return false;
        }
        next.pending.insert(instanceId, {
            .instanceId = instanceId,
            .componentId = iterator->componentId,
            .packageDigest = iterator->packageDigest,
        });
    }
    if (next == health_) {
        if (!health_.pending.isEmpty() && !probationTimer_->isActive()) {
            probationTimer_->start();
        }
        error.clear();
        return true;
    }
    if (!commitHealth(std::move(next), error)) {
        rebuildAuthoritativePlan();
        return false;
    }
    probationTimer_->start();
    return true;
}

bool ComponentPlanController::cancelSurfacePlanAuthorization(
    const quint64 surfacePlanRevision,
    QString &error
)
{
    if (state_ != State::Authoritative || !artifact_
        || artifact_->revision != surfacePlanRevision) {
        error = QStringLiteral("The surface plan is stale or unavailable");
        return false;
    }
    if (health_.pending.isEmpty()) {
        error.clear();
        return true;
    }
    auto next = health_;
    next.pending.clear();
    if (!commitHealth(std::move(next), error)) {
        rebuildAuthoritativePlan();
        return false;
    }
    probationTimer_->stop();
    return true;
}

const Components::SurfaceInstance *ComponentPlanController::matchingInstance(
    const QString &instanceId,
    const QString &componentId,
    const QString &packageDigest,
    const quint64 surfacePlanRevision
) const
{
    if (state_ != State::Authoritative || !artifact_
        || artifact_->revision != surfacePlanRevision) {
        return nullptr;
    }
    const auto instance = artifact_->plan.instances.constFind(instanceId);
    if (instance == artifact_->plan.instances.cend()
        || instance->runtimeKind != QStringLiteral("declarative-v1")
        || instance->componentId != componentId
        || instance->packageDigest != packageDigest) {
        return nullptr;
    }
    return &*instance;
}

bool ComponentPlanController::activationStable(
    const QString &instanceId,
    const QString &componentId,
    const QString &packageDigest,
    const quint64 surfacePlanRevision,
    QString &error
)
{
    if (matchingInstance(
            instanceId,
            componentId,
            packageDigest,
            surfacePlanRevision
        ) == nullptr) {
        error = QStringLiteral("The activation tuple is stale or invalid");
        return false;
    }
    const auto pending = health_.pending.constFind(instanceId);
    if (pending == health_.pending.cend()) {
        error = QStringLiteral("The activation is not in probation");
        return false;
    }
    if (pending->componentId != componentId
        || pending->packageDigest != packageDigest) {
        error = QStringLiteral("The activation tuple does not match probation");
        return false;
    }

    auto next = health_;
    next.pending.remove(instanceId);
    if (!commitHealth(std::move(next), error)) {
        rebuildAuthoritativePlan();
        return false;
    }
    if (health_.pending.isEmpty()) {
        probationTimer_->stop();
    }
    return true;
}

bool ComponentPlanController::activationFailed(
    const QString &instanceId,
    const QString &componentId,
    const QString &packageDigest,
    const quint64 surfacePlanRevision,
    const QString &reason,
    QString &error
)
{
    if (reason != QStringLiteral("render-failed")
        && reason != QStringLiteral("protocol-invalid")) {
        error = QStringLiteral("The activation failure reason is invalid");
        return false;
    }
    if (matchingInstance(
            instanceId,
            componentId,
            packageDigest,
            surfacePlanRevision
        ) == nullptr) {
        error = QStringLiteral("The activation tuple is stale or invalid");
        return false;
    }
    const auto key = ComponentRuntimeHealthStore::recordKey(
        componentId,
        packageDigest
    );
    auto next = health_;
    const auto previous = next.records.value(key);
    next.records.insert(key, {
        .componentId = componentId,
        .packageDigest = packageDigest,
        .state = QStringLiteral("quarantined"),
        .reason = reason,
        .failureCount = qMax<quint32>(1, previous.failureCount + 1),
    });
    for (auto iterator = next.pending.begin();
         iterator != next.pending.end();) {
        if (iterator->componentId == componentId
            && iterator->packageDigest == packageDigest) {
            iterator = next.pending.erase(iterator);
        } else {
            ++iterator;
        }
    }
    if (!commitHealth(std::move(next), error)) {
        rebuildAuthoritativePlan();
        return false;
    }
    rebuildAuthoritativePlan();
    return true;
}

bool ComponentPlanController::retryComponent(
    const QString &componentId,
    const QString &packageDigest,
    const quint64 expectedRuntimeHealthRevision,
    QString &error
)
{
    if (expectedRuntimeHealthRevision != health_.revision) {
        error = QStringLiteral("The runtime health state changed");
        return false;
    }
    const auto key = ComponentRuntimeHealthStore::recordKey(
        componentId,
        packageDigest
    );
    const auto record = health_.records.constFind(key);
    if (record == health_.records.cend()
        || record->state != QStringLiteral("quarantined")) {
        error = QStringLiteral("The exact component package is not quarantined");
        return false;
    }
    auto next = health_;
    next.records.remove(key);
    if (!commitHealth(std::move(next), error)) {
        rebuildAuthoritativePlan();
        return false;
    }
    rebuildAuthoritativePlan();
    return true;
}

void ComponentPlanController::quarantinePending(const QString &reason)
{
    if (health_.pending.isEmpty()) {
        return;
    }
    auto next = health_;
    for (const auto &activation : std::as_const(next.pending)) {
        const auto key = ComponentRuntimeHealthStore::recordKey(
            activation.componentId,
            activation.packageDigest
        );
        const auto previous = next.records.value(key);
        next.records.insert(key, {
            .componentId = activation.componentId,
            .packageDigest = activation.packageDigest,
            .state = QStringLiteral("quarantined"),
            .reason = reason,
            .failureCount = qMax<quint32>(1, previous.failureCount + 1),
        });
    }
    next.pending.clear();
    QString error;
    const auto persisted = commitHealth(std::move(next), error);
    Q_UNUSED(persisted)
    rebuildAuthoritativePlan();
}

void ComponentPlanController::rebuildAuthoritativePlan()
{
    if (!catalog_ || !configuration_) {
        return;
    }
    auto plan = Components::buildSurfacePlan(*catalog_, *configuration_);
    if (!plan) {
        setFailedState(describeErrors(plan.errors));
        return;
    }
    publishPlan(
        std::move(*plan.value),
        State::Authoritative,
        true,
        QString()
    );
}

void ComponentPlanController::setFailedState(const QString &error)
{
    if (artifact_) {
        auto retained = artifact_->plan;
        publishPlan(
            std::move(retained),
            State::Retained,
            false,
            error
        );
        return;
    }

    Components::SurfacePlan empty;
    QString healthError;
    preparePending(empty, healthError);
    const auto changed = state_ != State::Unavailable
        || lastError_ != error;
    state_ = State::Unavailable;
    lastError_ = !healthError.isEmpty() ? healthError : error;
    if (changed || !healthError.isEmpty()) {
        emit runtimeChanged();
    }
}

} // namespace HyprShelld
