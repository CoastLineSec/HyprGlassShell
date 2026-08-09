#include "coordinator1_adaptor.h"
#include "coordinator_service.h"
#include "component_runtime1_adaptor.h"
#include "components/component_plan_controller.h"
#include "components/component_plan_hydrator.h"
#include "components/component_runtime_service.h"
#include "systemd_user_manager.h"

#include <QCoreApplication>
#include <QDBusConnection>
#include <QDebug>

#include <cstdlib>

namespace {

constexpr auto coordinatorBusName = "org.hyprshelld.Coordinator1";
constexpr auto coordinatorObjectPath = "/org/hyprshelld/Coordinator1";
constexpr auto componentRuntimeObjectPath =
    "/org/hyprshelld/Coordinator1/Components";

} // namespace

int main(int argc, char *argv[])
{
    QCoreApplication application(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("hyprshelld"));
    QCoreApplication::setOrganizationName(QStringLiteral("CoastLineSec"));

    auto connection = QDBusConnection::sessionBus();
    if (!connection.isConnected()) {
        qCritical().noquote() << QStringLiteral("Cannot connect to the session bus: %1")
                                    .arg(connection.lastError().message());
        return EXIT_FAILURE;
    }

    HyprShelld::SystemdUserManager manager(connection);
    HyprShelld::CoordinatorService service(&manager, connection);
    const Coordinator1Adaptor adaptor(&service);
    HyprShelld::ComponentPlanController componentPlanController;
    HyprShelld::ComponentRuntimeService componentRuntimeService(
        &componentPlanController,
        connection
    );
    const ComponentRuntime1Adaptor componentRuntimeAdaptor(
        &componentRuntimeService
    );
    HyprShelld::ComponentPlanHydrator componentPlanHydrator(
        &componentPlanController,
        connection
    );

    QObject::connect(
        &service,
        &HyprShelld::CoordinatorService::initialized,
        &application,
        [
            &application,
            &connection,
            &service,
            &componentRuntimeService,
            &componentPlanHydrator
        ]() {
            if (!connection.registerObject(
                    QString::fromLatin1(coordinatorObjectPath),
                    &service,
                    QDBusConnection::ExportAdaptors
                )) {
                qCritical().noquote() << QStringLiteral("Cannot register Coordinator1 object: %1")
                                            .arg(connection.lastError().message());
                application.exit(EXIT_FAILURE);
                return;
            }

            if (!connection.registerObject(
                    QString::fromLatin1(componentRuntimeObjectPath),
                    &componentRuntimeService,
                    QDBusConnection::ExportAdaptors
                )) {
                qCritical().noquote()
                    << QStringLiteral("Cannot register ComponentRuntime1 object: %1")
                           .arg(connection.lastError().message());
                application.exit(EXIT_FAILURE);
                return;
            }

            if (!connection.registerService(QString::fromLatin1(coordinatorBusName))) {
                qCritical().noquote() << QStringLiteral("Cannot register Coordinator1 service: %1")
                                            .arg(connection.lastError().message());
                application.exit(EXIT_FAILURE);
                return;
            }

            componentPlanHydrator.start();
        }
    );

    QObject::connect(
        &service,
        &HyprShelld::CoordinatorService::fatalError,
        &application,
        [&application](const QString &error) {
            qCritical().noquote() << error;
            application.exit(EXIT_FAILURE);
        }
    );

    QString error;
    if (!service.start(error)) {
        qCritical().noquote() << error;
        return EXIT_FAILURE;
    }

    return application.exec();
}
