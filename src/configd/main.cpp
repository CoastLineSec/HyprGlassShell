#include "component_catalog_client.h"
#include "component_config1_adaptor.h"
#include "component_config_service.h"
#include "config1_adaptor.h"
#include "config_service.h"
#include "config_store.h"
#include "geoclue_client.h"
#include "hyprsunset_controller.h"

#include <QCoreApplication>
#include <QDBusConnection>
#include <QDebug>

#include <cstdlib>
#include <utility>

namespace {

constexpr auto configBusName = "org.hyprshelld.Config1";
constexpr auto configObjectPath = "/org/hyprshelld/Config1";
constexpr auto componentConfigObjectPath = "/org/hyprshelld/Config1/Components";

#ifndef HYPRSHELLD_COMPONENT_DEFAULTS_FILE
#error                                                                         \
    "HYPRSHELLD_COMPONENT_DEFAULTS_FILE must name the protected component defaults"
#endif

#ifndef HYPRSUNSET_EXECUTABLE
#error "HYPRSUNSET_EXECUTABLE must name the required Hypr ecosystem daemon"
#endif
#ifndef HYPRCTL_EXECUTABLE
#error "HYPRCTL_EXECUTABLE must name the Hyprland IPC client"
#endif
#ifndef PGREP_EXECUTABLE
#error "PGREP_EXECUTABLE must name the process ownership probe"
#endif

} // namespace

int main(int argc, char *argv[]) {
  QCoreApplication application(argc, argv);
  QCoreApplication::setApplicationName(QStringLiteral("hyprshelld-configd"));
  QCoreApplication::setOrganizationName(QStringLiteral("CoastLineSec"));

  auto connection = QDBusConnection::sessionBus();
  if (!connection.isConnected()) {
    qCritical().noquote() << QStringLiteral(
                                 "Cannot connect to the session bus: %1")
                                 .arg(connection.lastError().message());
    return EXIT_FAILURE;
  }

  HyprShelld::ConfigStore store(HyprShelld::ConfigPaths::standard());
  const auto loaded = store.load();
  if (!loaded.success) {
    qCritical().noquote() << loaded.error;
    return EXIT_FAILURE;
  }

  // Runtime helpers must outlive ConfigService because the service keeps
  // non-owning pointers to them and reacts to their signals during shutdown.
  HyprShelld::GeoClueClient geoClue;
  HyprShelld::HyprsunsetController hyprsunset({
      .hyprsunsetExecutable = QStringLiteral(HYPRSUNSET_EXECUTABLE),
      .hyprctlExecutable = QStringLiteral(HYPRCTL_EXECUTABLE),
      .processProbeExecutable = QStringLiteral(PGREP_EXECUTABLE),
  });
  HyprShelld::ConfigService service(std::move(store), loaded, connection);
  const Config1Adaptor adaptor(&service);

  HyprShelld::ComponentConfigService componentService(
      HyprShelld::ComponentStore(HyprShelld::ComponentPaths::standard(
          QStringLiteral(HYPRSHELLD_COMPONENT_DEFAULTS_FILE))),
      connection, loaded.legacyWorkspaceSettings);
  const ComponentConfig1Adaptor componentAdaptor(&componentService);

  if (!connection.registerObject(QString::fromLatin1(configObjectPath),
                                 &service, QDBusConnection::ExportAdaptors)) {
    qCritical().noquote() << QStringLiteral(
                                 "Cannot register Config1 object: %1")
                                 .arg(connection.lastError().message());
    return EXIT_FAILURE;
  }

  if (!connection.registerObject(QString::fromLatin1(componentConfigObjectPath),
                                 &componentService,
                                 QDBusConnection::ExportAdaptors)) {
    qCritical().noquote() << QStringLiteral(
                                 "Cannot register ComponentConfig1 object: %1")
                                 .arg(connection.lastError().message());
    return EXIT_FAILURE;
  }

  if (!connection.registerService(QString::fromLatin1(configBusName))) {
    qCritical().noquote() << QStringLiteral(
                                 "Cannot register Config1 service: %1")
                                 .arg(connection.lastError().message());
    return EXIT_FAILURE;
  }

  // Do not start location or companion processes until this instance has
  // exclusive ownership of Config1. A manually launched duplicate exits
  // without racing the authoritative service for hyprsunset ownership.
  service.attachAppearanceRuntime(&geoClue, &hyprsunset);

  HyprShelld::ComponentCatalogClient catalogClient(connection);
  QObject::connect(&catalogClient,
                   &HyprShelld::ComponentCatalogClient::catalogChanged,
                   &componentService, [&catalogClient, &componentService] {
                     componentService.applyCatalog(catalogClient.catalog());
                   });
  QObject::connect(&catalogClient,
                   &HyprShelld::ComponentCatalogClient::catalogUnavailable,
                   &componentService,
                   &HyprShelld::ComponentConfigService::setCatalogUnavailable);
  QObject::connect(
      &componentService,
      &HyprShelld::ComponentConfigService::authoritativeSnapshotEstablished,
      &service, &HyprShelld::ConfigService::authorizeLegacyWorkspaceRetirement);
  catalogClient.start();

  return application.exec();
}
