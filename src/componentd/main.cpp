#include "component_manager1_adaptor.h"
#include "component_inspection_sessions.h"
#include "component_inspector_launcher.h"
#include "component_manager_service.h"
#include "system_catalog.h"
#include "user_package_store.h"

#include <QCoreApplication>
#include <QDBusConnection>
#include <QDebug>
#include <QDir>
#include <QStandardPaths>
#include <QStringList>

#include <cstdlib>
#include <memory>
#include <utility>

#ifndef HYPRSHELLD_SYSTEM_COMPONENT_DIR
#error "HYPRSHELLD_SYSTEM_COMPONENT_DIR must name the protected component catalog"
#endif

#ifndef HYPRSHELLD_COMPONENT_INSPECTOR_EXECUTABLE
#error "HYPRSHELLD_COMPONENT_INSPECTOR_EXECUTABLE must name the installed inspector helper"
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

    const auto dataHome = QStandardPaths::writableLocation(
        QStandardPaths::GenericDataLocation
    );
    const auto stateHome = QStandardPaths::writableLocation(
        QStandardPaths::GenericStateLocation
    );
    const auto userDataRoot = dataHome.isEmpty()
        ? QString()
        : QDir(dataHome).filePath(QStringLiteral("hyprshelld/components"));
    const auto userStateRoot = stateHome.isEmpty()
        ? QString()
        : QDir(stateHome).filePath(QStringLiteral("hyprshelld/componentd"));
    if (userDataRoot.isEmpty() || userStateRoot.isEmpty()) {
        qWarning().noquote() << QStringLiteral(
            "User component storage is unavailable; protected components remain usable"
        );
    }

    const auto runtimeHome = QStandardPaths::writableLocation(
        QStandardPaths::RuntimeLocation
    );

    HyprShelld::ComponentManagerService service(
        std::move(*loaded.catalog),
        nullptr,
        nullptr,
        connection
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

    std::unique_ptr<HyprShelld::Components::UserPackageStore> userStore;
    if (!userDataRoot.isEmpty() && !userStateRoot.isEmpty()) {
        userStore = std::make_unique<
            HyprShelld::Components::UserPackageStore
        >(userDataRoot, userStateRoot);
        QString recoveryError;
        QStringList recoveryWarnings;
        if (!userStore->recover(recoveryError, &recoveryWarnings)) {
            qWarning().noquote() << QStringLiteral(
                "User component recovery failed; protected components remain usable: %1"
            ).arg(recoveryError);
            userStore.reset();
        } else {
            for (const auto &warning : recoveryWarnings) {
                qWarning().noquote()
                    << QStringLiteral("User component recovery warning: %1")
                           .arg(warning);
            }
        }
    }

    std::unique_ptr<HyprShelld::ComponentInspectionSessions> inspections;
    if (!runtimeHome.isEmpty()) {
        auto launcher = std::make_unique<
            HyprShelld::SystemdComponentInspectorLauncher
        >(
            connection,
            QStringLiteral(HYPRSHELLD_COMPONENT_INSPECTOR_EXECUTABLE)
        );
        inspections = std::make_unique<
            HyprShelld::ComponentInspectionSessions
        >(
            QDir(runtimeHome).filePath(
                QStringLiteral("hyprshelld/component-inspections")
            ),
            std::move(launcher)
        );
    } else {
        qWarning().noquote() << QStringLiteral(
            "Package inspection is unavailable because no runtime directory exists"
        );
    }
    service.initializePackageManagement(
        std::move(userStore),
        std::move(inspections)
    );

    return application.exec();
}
