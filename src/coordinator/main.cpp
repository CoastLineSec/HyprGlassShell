#include "coordinator1_adaptor.h"
#include "coordinator_service.h"
#include "systemd_user_manager.h"

#include <QCoreApplication>
#include <QDBusConnection>
#include <QDebug>

#include <cstdlib>

namespace {

constexpr auto coordinatorBusName = "org.hyprshelld.Coordinator1";
constexpr auto coordinatorObjectPath = "/org/hyprshelld/Coordinator1";

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

    QObject::connect(
        &service,
        &HyprShelld::CoordinatorService::initialized,
        &application,
        [&application, &connection, &service]() {
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

            if (!connection.registerService(QString::fromLatin1(coordinatorBusName))) {
                qCritical().noquote() << QStringLiteral("Cannot register Coordinator1 service: %1")
                                            .arg(connection.lastError().message());
                application.exit(EXIT_FAILURE);
            }
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
