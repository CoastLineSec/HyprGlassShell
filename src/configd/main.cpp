#include "config1_adaptor.h"
#include "config_service.h"
#include "config_store.h"

#include <QCoreApplication>
#include <QDBusConnection>
#include <QDebug>

#include <cstdlib>
#include <utility>

namespace {

constexpr auto configBusName = "org.hyprshelld.Config1";
constexpr auto configObjectPath = "/org/hyprshelld/Config1";

} // namespace

int main(int argc, char *argv[])
{
    QCoreApplication application(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("hyprshelld-configd"));
    QCoreApplication::setOrganizationName(QStringLiteral("CoastLineSec"));

    auto connection = QDBusConnection::sessionBus();
    if (!connection.isConnected()) {
        qCritical().noquote() << QStringLiteral("Cannot connect to the session bus: %1")
                                    .arg(connection.lastError().message());
        return EXIT_FAILURE;
    }

    HyprShelld::ConfigStore store(HyprShelld::ConfigPaths::standard());
    const auto loaded = store.load();
    if (!loaded.success) {
        qCritical().noquote() << loaded.error;
        return EXIT_FAILURE;
    }

    HyprShelld::ConfigService service(
        std::move(store),
        loaded,
        connection
    );
    const Config1Adaptor adaptor(&service);

    if (!connection.registerObject(
            QString::fromLatin1(configObjectPath),
            &service,
            QDBusConnection::ExportAdaptors
        )) {
        qCritical().noquote() << QStringLiteral("Cannot register Config1 object: %1")
                                    .arg(connection.lastError().message());
        return EXIT_FAILURE;
    }

    if (!connection.registerService(QString::fromLatin1(configBusName))) {
        qCritical().noquote() << QStringLiteral("Cannot register Config1 service: %1")
                                    .arg(connection.lastError().message());
        return EXIT_FAILURE;
    }

    return application.exec();
}
