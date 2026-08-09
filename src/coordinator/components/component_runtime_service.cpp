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
