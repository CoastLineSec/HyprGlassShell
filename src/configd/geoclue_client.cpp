#include "geoclue_client.h"

#include <QDBusMessage>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDBusServiceWatcher>
#include <QDBusVariant>
#include <QMetaType>
#include <QTimer>

#include <algorithm>
#include <cmath>
#include <utility>

namespace HyprShelld {
namespace {

const QString service = QStringLiteral("org.freedesktop.GeoClue2");
const QString managerPath = QStringLiteral("/org/freedesktop/GeoClue2/Manager");
const QString managerInterface =
    QStringLiteral("org.freedesktop.GeoClue2.Manager");
const QString clientInterface =
    QStringLiteral("org.freedesktop.GeoClue2.Client");
const QString locationInterface =
    QStringLiteral("org.freedesktop.GeoClue2.Location");
const QString propertiesInterface =
    QStringLiteral("org.freedesktop.DBus.Properties");
const QString activeProperty = QStringLiteral("Active");
constexpr quint32 cityAccuracyLevel = 4;

QDBusMessage methodCall(const QString &path, const QString &interface,
                        const QString &method) {
  return QDBusMessage::createMethodCall(service, path, interface, method);
}

int retryDelay(const GeoClueClient::Options &options, const int attempts) {
  auto delay = std::min(options.retryBaseDelayMs, options.maximumRetryDelayMs);
  for (auto completed = 1; completed < attempts; ++completed) {
    if (delay >= options.maximumRetryDelayMs / 2) {
      return options.maximumRetryDelayMs;
    }
    delay *= 2;
  }
  return std::min(delay, options.maximumRetryDelayMs);
}

} // namespace

GeoClueClient::GeoClueClient(QDBusConnection connection, QObject *parent)
    : GeoClueClient(std::move(connection), Options(), parent) {}

GeoClueClient::GeoClueClient(QDBusConnection connection, Options options,
                             QObject *parent)
    : QObject(parent), connection_(std::move(connection)),
      options_(std::move(options)),
      serviceWatcher_(new QDBusServiceWatcher(
          service, connection_, QDBusServiceWatcher::WatchForOwnerChange,
          this)),
      retryTimer_(new QTimer(this)) {
  options_.maximumAttempts = std::max(1, options_.maximumAttempts);
  options_.retryBaseDelayMs = std::max(1, options_.retryBaseDelayMs);
  options_.maximumRetryDelayMs =
      std::max(options_.retryBaseDelayMs, options_.maximumRetryDelayMs);

  retryTimer_->setSingleShot(true);
  connect(retryTimer_, &QTimer::timeout, this, &GeoClueClient::beginAttempt);
  connect(serviceWatcher_, &QDBusServiceWatcher::serviceOwnerChanged, this,
          &GeoClueClient::serviceOwnerChanged);
}

GeoClueClient::~GeoClueClient() {
  shuttingDown_ = true;
  requested_ = false;
  retryTimer_->stop();
  ++generation_;
  attemptInFlight_ = false;
  cleanupClient(true);
}

bool GeoClueClient::active() const { return active_; }
bool GeoClueClient::available() const { return available_; }
double GeoClueClient::latitude() const { return latitude_; }
double GeoClueClient::longitude() const { return longitude_; }
QString GeoClueClient::status() const { return status_; }

void GeoClueClient::start() {
  if (shuttingDown_ || requested_) {
    return;
  }

  requested_ = true;
  attempts_ = 0;
  failureLatched_ = false;
  beginAttempt();
}

void GeoClueClient::stop() {
  if (shuttingDown_) {
    return;
  }

  requested_ = false;
  attempts_ = 0;
  failureLatched_ = false;
  retryTimer_->stop();
  ++generation_;
  attemptInFlight_ = false;
  cleanupClient(true);
  setState(false, false, 0.0, 0.0, QStringLiteral("idle"));
}

void GeoClueClient::beginAttempt() {
  if (!requested_ || shuttingDown_ || active_ || attemptInFlight_ ||
      failureLatched_ || retryTimer_->isActive()) {
    return;
  }
  if (attempts_ >= options_.maximumAttempts) {
    failureLatched_ = true;
    return;
  }

  const auto generation = ++generation_;
  const auto serviceEpoch = serviceEpoch_;
  ++attempts_;
  attemptInFlight_ = true;
  setState(false, false, 0.0, 0.0, QStringLiteral("locating"));

  if (!connection_.isConnected()) {
    fail(QStringLiteral("GeoClue system bus is unavailable"), generation);
    return;
  }

  auto message =
      methodCall(managerPath, managerInterface, QStringLiteral("GetClient"));
  auto *watcher =
      new QDBusPendingCallWatcher(connection_.asyncCall(message), this);
  connect(
      watcher, &QDBusPendingCallWatcher::finished, this,
      [this, watcher, generation, serviceEpoch] {
        const QDBusPendingReply<QDBusObjectPath> reply = *watcher;
        watcher->deleteLater();
        if (generation != generation_) {
          const auto path = reply.value();
          if (!reply.isError() && serviceEpoch == serviceEpoch_ &&
              !path.path().isEmpty() && path.path() != QStringLiteral("/")) {
            requestServerCleanup(path);
          }
          return;
        }
        if (reply.isError() || reply.value().path().isEmpty() ||
            reply.value().path() == QStringLiteral("/")) {
          fail(reply.isError() ? reply.error().message()
                               : QStringLiteral("GeoClue returned no client"),
               generation);
          return;
        }
        configureClient(reply.value(), generation);
      });
}

void GeoClueClient::configureClient(const QDBusObjectPath &path,
                                    const quint64 generation) {
  clientPath_ = path;
  const auto locationConnected = connection_.connect(
      service, clientPath_.path(), clientInterface,
      QStringLiteral("LocationUpdated"), this,
      SLOT(locationUpdated(QDBusObjectPath, QDBusObjectPath)));
  const auto propertiesConnected = connection_.connect(
      service, clientPath_.path(), propertiesInterface,
      QStringLiteral("PropertiesChanged"), this,
      SLOT(clientPropertiesChanged(QString, QVariantMap, QStringList)));
  if (!locationConnected || !propertiesConnected) {
    fail(QStringLiteral("Cannot subscribe to GeoClue client updates"),
         generation);
    return;
  }

  auto setDesktopId = methodCall(clientPath_.path(), propertiesInterface,
                                 QStringLiteral("Set"));
  setDesktopId.setArguments({
      clientInterface,
      QStringLiteral("DesktopId"),
      QVariant::fromValue(QDBusVariant(
          QStringLiteral("io.github.CoastLineSec.HyprShelld.Settings"))),
  });
  auto *watcher =
      new QDBusPendingCallWatcher(connection_.asyncCall(setDesktopId), this);
  connect(watcher, &QDBusPendingCallWatcher::finished, this,
          [this, watcher, generation] {
            const QDBusPendingReply<> reply = *watcher;
            watcher->deleteLater();
            if (generation != generation_) {
              return;
            }
            if (reply.isError()) {
              fail(reply.error().message(), generation);
              return;
            }

            auto setAccuracy = methodCall(clientPath_.path(),
                                          propertiesInterface,
                                          QStringLiteral("Set"));
            setAccuracy.setArguments({
                clientInterface,
                QStringLiteral("RequestedAccuracyLevel"),
                QVariant::fromValue(QDBusVariant(
                    QVariant::fromValue(cityAccuracyLevel))),
            });
            auto *accuracyWatcher = new QDBusPendingCallWatcher(
                connection_.asyncCall(setAccuracy), this);
            connect(
                accuracyWatcher, &QDBusPendingCallWatcher::finished, this,
                [this, accuracyWatcher, generation] {
                  const QDBusPendingReply<> accuracyReply = *accuracyWatcher;
                  accuracyWatcher->deleteLater();
                  if (generation != generation_) {
                    return;
                  }
                  if (accuracyReply.isError()) {
                    fail(accuracyReply.error().message(), generation);
                    return;
                  }

                  auto startMessage =
                      methodCall(clientPath_.path(), clientInterface,
                                 QStringLiteral("Start"));
                  auto *startWatcher = new QDBusPendingCallWatcher(
                      connection_.asyncCall(startMessage), this);
                  connect(
                      startWatcher, &QDBusPendingCallWatcher::finished, this,
                      [this, startWatcher, generation] {
                        const QDBusPendingReply<> startReply = *startWatcher;
                        startWatcher->deleteLater();
                        if (generation != generation_) {
                          return;
                        }
                        if (startReply.isError()) {
                          fail(startReply.error().message(), generation);
                          return;
                        }
                        attemptInFlight_ = false;
                        setState(
                            true, available_, latitude_, longitude_,
                            available_
                                ? QStringLiteral("ready")
                                : QStringLiteral("waiting-for-location"));
                      });
                });
          });
}

void GeoClueClient::locationUpdated(const QDBusObjectPath &oldLocation,
                                    const QDBusObjectPath &newLocation) {
  Q_UNUSED(oldLocation)
  if (!requested_ || newLocation.path().isEmpty() ||
      newLocation.path() == QStringLiteral("/")) {
    return;
  }
  readLocation(newLocation, generation_);
}

void GeoClueClient::readLocation(const QDBusObjectPath &path,
                                 const quint64 generation) {
  auto message =
      methodCall(path.path(), propertiesInterface, QStringLiteral("GetAll"));
  message.setArguments({locationInterface});
  auto *watcher =
      new QDBusPendingCallWatcher(connection_.asyncCall(message), this);
  connect(watcher, &QDBusPendingCallWatcher::finished, this,
          [this, watcher, generation] {
            const QDBusPendingReply<QVariantMap> reply = *watcher;
            watcher->deleteLater();
            if (generation != generation_) {
              return;
            }
            if (reply.isError()) {
              fail(reply.error().message(), generation);
              return;
            }

            const auto properties = reply.value();
            const auto latitudeValue =
                properties.constFind(QStringLiteral("Latitude"));
            const auto longitudeValue =
                properties.constFind(QStringLiteral("Longitude"));
            if (latitudeValue == properties.cend() ||
                longitudeValue == properties.cend() ||
                latitudeValue->metaType().id() != QMetaType::Double ||
                longitudeValue->metaType().id() != QMetaType::Double) {
              fail(QStringLiteral("GeoClue returned invalid coordinates"),
                   generation);
              return;
            }

            const auto latitude = latitudeValue->toDouble();
            const auto longitude = longitudeValue->toDouble();
            if (!std::isfinite(latitude) || latitude < -90.0 ||
                latitude > 90.0 || !std::isfinite(longitude) ||
                longitude < -180.0 || longitude > 180.0) {
              fail(QStringLiteral("GeoClue returned invalid coordinates"),
                   generation);
              return;
            }

            attempts_ = 0;
            failureLatched_ = false;
            retryTimer_->stop();
            setState(true, true, latitude, longitude, QStringLiteral("ready"));
          });
}

void GeoClueClient::clientPropertiesChanged(const QString &interface,
                                            const QVariantMap &changed,
                                            const QStringList &invalidated) {
  if (interface != clientInterface || !requested_) {
    return;
  }

  const auto active = changed.constFind(activeProperty);
  if (active != changed.cend()) {
    if (active->metaType().id() != QMetaType::Bool) {
      fail(QStringLiteral("GeoClue returned an invalid Active property"),
           generation_);
      return;
    }
    applyClientActive(active->toBool());
    return;
  }
  if (invalidated.contains(activeProperty)) {
    readClientActive(generation_);
  }
}

void GeoClueClient::readClientActive(const quint64 generation) {
  if (clientPath_.path().isEmpty()) {
    return;
  }
  auto message = methodCall(clientPath_.path(), propertiesInterface,
                            QStringLiteral("Get"));
  message.setArguments({clientInterface, activeProperty});
  auto *watcher =
      new QDBusPendingCallWatcher(connection_.asyncCall(message), this);
  connect(watcher, &QDBusPendingCallWatcher::finished, this,
          [this, watcher, generation] {
            const QDBusPendingReply<QDBusVariant> reply = *watcher;
            watcher->deleteLater();
            if (generation != generation_) {
              return;
            }
            if (reply.isError() ||
                reply.value().variant().metaType().id() != QMetaType::Bool) {
              fail(reply.isError()
                       ? reply.error().message()
                       : QStringLiteral(
                             "GeoClue returned an invalid Active property"),
                   generation);
              return;
            }
            applyClientActive(reply.value().variant().toBool());
          });
}

void GeoClueClient::applyClientActive(const bool active) {
  if (!requested_) {
    return;
  }
  if (!active) {
    if (active_ || available_) {
      fail(QStringLiteral("GeoClue client is inactive"), generation_);
    }
    return;
  }

  setState(true, false, 0.0, 0.0, QStringLiteral("waiting-for-location"));
}

void GeoClueClient::serviceOwnerChanged(const QString &name,
                                        const QString &oldOwner,
                                        const QString &newOwner) {
  Q_UNUSED(name)
  if (oldOwner == newOwner) {
    return;
  }

  ++serviceEpoch_;
  attempts_ = 0;
  failureLatched_ = false;
  retryTimer_->stop();

  if (!oldOwner.isEmpty()) {
    ++generation_;
    attemptInFlight_ = false;
    cleanupClient(false);
    setState(false, false, 0.0, 0.0,
             newOwner.isEmpty()
                 ? QStringLiteral("GeoClue service is unavailable")
                 : QStringLiteral("GeoClue service owner changed"));
  }

  if (!requested_) {
    return;
  }
  if (newOwner.isEmpty()) {
    if (oldOwner.isEmpty()) {
      setState(false, false, 0.0, 0.0,
               QStringLiteral("GeoClue service is unavailable"));
    }
    return;
  }
  if (!attemptInFlight_) {
    QTimer::singleShot(0, this, &GeoClueClient::beginAttempt);
  }
}

void GeoClueClient::disconnectClientSignals(const QDBusObjectPath &path) {
  if (path.path().isEmpty()) {
    return;
  }
  connection_.disconnect(
      service, path.path(), clientInterface, QStringLiteral("LocationUpdated"),
      this, SLOT(locationUpdated(QDBusObjectPath, QDBusObjectPath)));
  connection_.disconnect(
      service, path.path(), propertiesInterface,
      QStringLiteral("PropertiesChanged"), this,
      SLOT(clientPropertiesChanged(QString, QVariantMap, QStringList)));
}

void GeoClueClient::cleanupClient(const bool notifyServer) {
  const auto path = clientPath_;
  clientPath_ = QDBusObjectPath();
  disconnectClientSignals(path);
  if (notifyServer) {
    requestServerCleanup(path);
  }
}

void GeoClueClient::requestServerCleanup(const QDBusObjectPath &path) const {
  if (!connection_.isConnected() || path.path().isEmpty() ||
      path.path() == QStringLiteral("/")) {
    return;
  }

  connection_.asyncCall(
      methodCall(path.path(), clientInterface, QStringLiteral("Stop")));
  auto remove =
      methodCall(managerPath, managerInterface, QStringLiteral("DeleteClient"));
  remove.setArguments({QVariant::fromValue(path)});
  connection_.asyncCall(remove);
}

void GeoClueClient::scheduleRetry(const QString &message) {
  const auto status =
      message.isEmpty() ? QStringLiteral("location-unavailable") : message;
  setState(false, false, 0.0, 0.0, status);
  if (!requested_ || shuttingDown_) {
    return;
  }
  if (attempts_ >= options_.maximumAttempts) {
    failureLatched_ = true;
    retryTimer_->stop();
    return;
  }
  retryTimer_->start(retryDelay(options_, attempts_));
}

void GeoClueClient::setState(const bool active, const bool available,
                             const double latitude, const double longitude,
                             const QString &status) {
  if (active_ == active && available_ == available && latitude_ == latitude &&
      longitude_ == longitude && status_ == status) {
    return;
  }
  active_ = active;
  available_ = available;
  latitude_ = latitude;
  longitude_ = longitude;
  status_ = status;
  emit changed();
}

void GeoClueClient::fail(const QString &message, const quint64 generation) {
  if (generation != generation_) {
    return;
  }

  attemptInFlight_ = false;
  attempts_ = std::max(1, attempts_);
  ++generation_;
  cleanupClient(true);
  scheduleRetry(message);
}

} // namespace HyprShelld
