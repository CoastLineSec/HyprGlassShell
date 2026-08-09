#include "component_manager1_adaptor.h"
#include "component_manager_service.h"
#include "system_catalog.h"

#include <QCoreApplication>
#include <QDBusConnection>
#include <QDebug>

#include <cstdlib>
#include <utility>

#ifndef HYPRSHELLD_SYSTEM_COMPONENT_DIR
#error "HYPRSHELLD_SYSTEM_COMPONENT_DIR must name the protected component catalog"
#endif

namespace {

constexpr auto componentManagerBusName = "org.hyprshelld.ComponentManager1";
constexpr auto componentManagerObjectPath =
    "/org/hyprshelld/ComponentManager1";

} // namespace

int main(int argc, char *argv[])
{
    QCoreApplication application(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("hyprshelld-componentd"));
    QCoreApplication::setOrganizationName(QStringLiteral("CoastLineSec"));

    auto connection = QDBusConnection::sessionBus();
    if (!connection.isConnected()) {
        qCritical().noquote() << QStringLiteral("Cannot connect to the session bus: %1")
                                    .arg(connection.lastError().message());
        return EXIT_FAILURE;
    }

    auto loaded = HyprShelld::Components::SystemCatalog::load(
        QStringLiteral(HYPRSHELLD_SYSTEM_COMPONENT_DIR)
    );
    if (!loaded.ok()) {
        qCritical().noquote() << loaded.error;
        return EXIT_FAILURE;
    }

    HyprShelld::ComponentManagerService service(
        std::move(*loaded.catalog)
    );
    const ComponentManager1Adaptor adaptor(&service);

    if (!connection.registerObject(
            QString::fromLatin1(componentManagerObjectPath),
            &service,
            QDBusConnection::ExportAdaptors
        )) {
        qCritical().noquote()
            << QStringLiteral("Cannot register ComponentManager1 object: %1")
                   .arg(connection.lastError().message());
        return EXIT_FAILURE;
    }

    if (!connection.registerService(
            QString::fromLatin1(componentManagerBusName)
        )) {
        qCritical().noquote()
            << QStringLiteral("Cannot register ComponentManager1 service: %1")
                   .arg(connection.lastError().message());
        return EXIT_FAILURE;
    }

    return application.exec();
}
