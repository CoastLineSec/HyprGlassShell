#include "component_runtime_service.h"

#include "component_plan_controller.h"

#include <QDBusMessage>
#include <QDebug>
#include <QVariantMap>

#include <utility>

namespace HyprShelld {
namespace {

const QString runtimeInterface = QStringLiteral(
    "org.hyprshelld.ComponentRuntime1"
);
const QString runtimePath = QStringLiteral(
    "/org/hyprshelld/Coordinator1/Components"
);
const QString unavailableError = QStringLiteral(
    "org.hyprshelld.ComponentRuntime1.Error.PlanUnavailable"
);
const QString staleRevisionError = QStringLiteral(
    "org.hyprshelld.ComponentRuntime1.Error.StaleSurfacePlanRevision"
);
const QString staleHealthRevisionError = QStringLiteral(
    "org.hyprshelld.ComponentRuntime1.Error.StaleRuntimeHealthRevision"
);
const QString invalidActivationError = QStringLiteral(
    "org.hyprshelld.ComponentRuntime1.Error.InvalidActivation"
);
const QString retryUnavailableError = QStringLiteral(
    "org.hyprshelld.ComponentRuntime1.Error.RetryUnavailable"
);

} // namespace

ComponentRuntimeService::ComponentRuntimeService(
    ComponentPlanController *controller,
    QDBusConnection connection,
    QObject *parent
)
    : QObject(parent)
    , controller_(controller)
    , connection_(std::move(connection))
{
    Q_ASSERT(controller_ != nullptr);
    connect(
        controller_,
        &ComponentPlanController::runtimeChanged,
        this,
        &ComponentRuntimeService::publishChange
    );
    connect(
        controller_,
        &ComponentPlanController::runtimeHealthChanged,
        this,
        &ComponentRuntimeService::publishHealthChange
    );
}

qulonglong ComponentRuntimeService::surfacePlanRevision() const
{
    return controller_->revision();
}

QString ComponentRuntimeService::surfacePlanDigest() const
{
    return controller_->digest();
}

QString ComponentRuntimeService::surfacePlanState() const
{
    return controller_->stateName();
}

qulonglong ComponentRuntimeService::runtimeHealthRevision() const
{
    return controller_->runtimeHealthRevision();
}

bool ComponentRuntimeService::thirdPartySafeMode() const
{
    return controller_->thirdPartySafeMode();
}

QByteArray ComponentRuntimeService::GetSurfacePlan(
    qulonglong expectedSurfacePlanRevision,
    QString &surfacePlanDigest
) const
{
    const auto *artifact = controller_->artifact();
    if (artifact == nullptr) {
        reportError(
            unavailableError,
            QStringLiteral("No validated surface plan is available")
        );
        surfacePlanDigest.clear();
        return {};
    }
    if (expectedSurfacePlanRevision != artifact->revision) {
        reportError(
            staleRevisionError,
            QStringLiteral("The surface plan changed; refresh it again")
        );
        surfacePlanDigest.clear();
        return {};
    }

    surfacePlanDigest = artifact->digest;
    return artifact->bytes;
}

QStringList ComponentRuntimeService::ListComponentRuntimeStates(
    const qulonglong expectedRuntimeHealthRevision,
    QStringList &packageDigests,
    QStringList &states,
    QStringList &reasons,
    QList<uint> &failureCounts
) const
{
    QStringList componentIds;
    packageDigests.clear();
    states.clear();
    reasons.clear();
    failureCounts.clear();
    if (expectedRuntimeHealthRevision != controller_->runtimeHealthRevision()) {
        reportError(
            staleHealthRevisionError,
            QStringLiteral("The component runtime health state changed")
        );
        return {};
    }
    const auto records = controller_->runtimeHealthRecords();
    componentIds.reserve(records.size());
    packageDigests.reserve(records.size());
    states.reserve(records.size());
    reasons.reserve(records.size());
    failureCounts.reserve(records.size());
    for (const auto &record : records) {
        componentIds.append(record.componentId);
        packageDigests.append(record.packageDigest);
        states.append(record.state);
        reasons.append(record.reason);
        failureCounts.append(record.failureCount);
    }
    return componentIds;
}

qulonglong ComponentRuntimeService::RetryComponent(
    const QString &componentId,
    const QString &expectedPackageDigest,
    const qulonglong expectedRuntimeHealthRevision
)
{
    QString error;
    if (!controller_->retryComponent(
            componentId,
            expectedPackageDigest,
            expectedRuntimeHealthRevision,
            error
        )) {
        reportError(
            expectedRuntimeHealthRevision
                    != controller_->runtimeHealthRevision()
                ? staleHealthRevisionError
                : retryUnavailableError,
            error
        );
        return controller_->runtimeHealthRevision();
    }
    return controller_->runtimeHealthRevision();
}

bool ComponentRuntimeService::AuthorizeSurfacePlan(
    const qulonglong surfacePlanRevision
)
{
    QString error;
    if (!controller_->authorizeSurfacePlan(
            surfacePlanRevision,
            error
        )) {
        reportError(invalidActivationError, error);
        return false;
    }
    return true;
}

bool ComponentRuntimeService::CancelSurfacePlanAuthorization(
    const qulonglong surfacePlanRevision
)
{
    QString error;
    if (!controller_->cancelSurfacePlanAuthorization(
            surfacePlanRevision,
            error
        )) {
        reportError(invalidActivationError, error);
        return false;
    }
    return true;
}

void ComponentRuntimeService::ActivationStable(
    const QString &instanceId,
    const QString &componentId,
    const QString &packageDigest,
    const qulonglong surfacePlanRevision
)
{
    QString error;
    if (!controller_->activationStable(
            instanceId,
            componentId,
            packageDigest,
            surfacePlanRevision,
            error
        )) {
        reportError(invalidActivationError, error);
    }
}

void ComponentRuntimeService::ActivationFailed(
    const QString &instanceId,
    const QString &componentId,
    const QString &packageDigest,
    const qulonglong surfacePlanRevision,
    const QString &reason
)
{
    QString error;
    if (!controller_->activationFailed(
            instanceId,
            componentId,
            packageDigest,
            surfacePlanRevision,
            reason,
            error
        )) {
        reportError(invalidActivationError, error);
    }
}

void ComponentRuntimeService::publishChange() const
{
    QVariantMap changed;
    changed.insert(
        QStringLiteral("SurfacePlanRevision"),
        QVariant::fromValue<qulonglong>(surfacePlanRevision())
    );
    changed.insert(QStringLiteral("SurfacePlanDigest"), surfacePlanDigest());
    changed.insert(QStringLiteral("SurfacePlanState"), surfacePlanState());

    auto signal = QDBusMessage::createSignal(
        runtimePath,
        QStringLiteral("org.freedesktop.DBus.Properties"),
        QStringLiteral("PropertiesChanged")
    );
    signal.setArguments({runtimeInterface, changed, QStringList()});
    if (!connection_.send(signal)) {
        qWarning() << "Failed to publish component runtime plan change";
    }
}

void ComponentRuntimeService::publishHealthChange() const
{
    QVariantMap changed;
    changed.insert(
        QStringLiteral("RuntimeHealthRevision"),
        QVariant::fromValue<qulonglong>(runtimeHealthRevision())
    );
    changed.insert(
        QStringLiteral("ThirdPartySafeMode"),
        thirdPartySafeMode()
    );

    auto signal = QDBusMessage::createSignal(
        runtimePath,
        QStringLiteral("org.freedesktop.DBus.Properties"),
        QStringLiteral("PropertiesChanged")
    );
    signal.setArguments({runtimeInterface, changed, QStringList()});
    if (!connection_.send(signal)) {
        qWarning() << "Failed to publish component runtime health change";
    }
}

void ComponentRuntimeService::reportError(
    const QString &name,
    const QString &message
) const
{
    if (calledFromDBus()) {
        sendErrorReply(name, message);
    }
}

} // namespace HyprShelld
