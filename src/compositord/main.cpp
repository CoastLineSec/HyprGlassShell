#include "compositor1_adaptor.h"
#include "activation_backend.h"
#include "compositor_service.h"
#include "transaction.h"

#include <QCoreApplication>
#include <QDBusConnection>
#include <QDebug>
#include <QFile>

#include <cstdlib>
#include <memory>
#include <optional>
#include <utility>

#ifndef HYPRSHELLD_HYPRLAND_CATALOG_FILE
#error "HYPRSHELLD_HYPRLAND_CATALOG_FILE must name the protected scalar catalog"
#endif

#ifndef HYPRSHELLD_HYPRLAND_ACTION_CATALOG_FILE
#error "HYPRSHELLD_HYPRLAND_ACTION_CATALOG_FILE must name the protected action catalog"
#endif

#ifndef HYPRSHELLD_HYPRLAND_CONFIG_SCHEMA_FILE
#error "HYPRSHELLD_HYPRLAND_CONFIG_SCHEMA_FILE must name the protected config schema"
#endif

namespace {

constexpr auto compositorBusName = "org.hyprshelld.Compositor1";
constexpr auto compositorObjectPath = "/org/hyprshelld/Compositor1";

std::optional<QByteArray> readBounded(
    const QString &path,
    const qsizetype maximum,
    QString &error
)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        error = QStringLiteral("Cannot open %1: %2")
                    .arg(path, file.errorString());
        return std::nullopt;
    }
    auto bytes = file.read(maximum + 1);
    if (file.error() != QFileDevice::NoError) {
        error = QStringLiteral("Cannot read %1: %2")
                    .arg(path, file.errorString());
        return std::nullopt;
    }
    if (bytes.size() > maximum || !file.atEnd()) {
        error = QStringLiteral("Protected contract %1 exceeds its size limit")
                    .arg(path);
        return std::nullopt;
    }
    return bytes;
}

QString firstError(const HyprShelld::Hyprland::ValidationErrors &errors)
{
    if (errors.isEmpty()) {
        return QStringLiteral("The protected compositor contract is invalid");
    }
    return QStringLiteral("%1: %2 (%3)")
        .arg(errors.first().path, errors.first().message, errors.first().code);
}

} // namespace

int main(int argc, char *argv[])
{
    QCoreApplication application(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("hyprshelld-compositord"));
    QCoreApplication::setOrganizationName(QStringLiteral("CoastLineSec"));

    auto connection = QDBusConnection::sessionBus();
    if (!connection.isConnected()) {
        qCritical().noquote()
            << QStringLiteral("Cannot connect to the session bus: %1")
                   .arg(connection.lastError().message());
        return EXIT_FAILURE;
    }

    const auto paths = HyprShelld::Compositor::StorePaths::standard();
    auto activationBackend = std::make_unique<
        HyprShelld::Compositor::DeferredActivationBackend
    >(paths.configRoot, paths.stableEntrypointPath());
    HyprShelld::Compositor::CompositorService service(
        std::move(activationBackend),
        connection
    );
    const Compositor1Adaptor adaptor(&service);

    if (!connection.registerObject(
            QString::fromLatin1(compositorObjectPath),
            &service,
            QDBusConnection::ExportAdaptors
        )) {
        qCritical().noquote()
            << QStringLiteral("Cannot register Compositor1 object: %1")
                   .arg(connection.lastError().message());
        return EXIT_FAILURE;
    }
    if (!connection.registerService(QString::fromLatin1(compositorBusName))) {
        qCritical().noquote()
            << QStringLiteral("Cannot register Compositor1 service: %1")
                   .arg(connection.lastError().message());
        return EXIT_FAILURE;
    }

    // Everything below this point is intentionally ordered after acquiring the
    // unique public name. The transaction constructor itself is side-effect
    // free; initializeAuthority() takes the lease and performs recovery.
    QString error;
    const auto catalogBytes = readBounded(
        QStringLiteral(HYPRSHELLD_HYPRLAND_CATALOG_FILE),
        HyprShelld::Hyprland::maximumCatalogBytes,
        error
    );
    const auto actionCatalogBytes = readBounded(
        QStringLiteral(HYPRSHELLD_HYPRLAND_ACTION_CATALOG_FILE),
        HyprShelld::Hyprland::maximumActionCatalogBytes,
        error
    );
    const auto configSchemaBytes = readBounded(
        QStringLiteral(HYPRSHELLD_HYPRLAND_CONFIG_SCHEMA_FILE),
        HyprShelld::Hyprland::maximumActionSchemaBytes,
        error
    );
    if (!catalogBytes || !actionCatalogBytes || !configSchemaBytes) {
        qCritical().noquote() << error;
        return EXIT_FAILURE;
    }

    auto catalog = HyprShelld::Hyprland::parseCatalog(*catalogBytes);
    if (!catalog) {
        qCritical().noquote() << firstError(catalog.errors);
        return EXIT_FAILURE;
    }
    auto actionCatalog = HyprShelld::Hyprland::parseActionCatalog(
        *actionCatalogBytes,
        *configSchemaBytes
    );
    if (!actionCatalog) {
        qCritical().noquote() << firstError(actionCatalog.errors);
        return EXIT_FAILURE;
    }

    auto authority = std::make_unique<
        HyprShelld::Compositor::ConfigurationTransaction
    >(
        paths,
        std::move(*catalog.value),
        std::move(*actionCatalog.value)
    );
    if (!service.initializeAuthority(std::move(authority), error)) {
        qCritical().noquote()
            << QStringLiteral("Cannot initialize compositor authority: %1")
                   .arg(error);
        return EXIT_FAILURE;
    }

    return application.exec();
}
