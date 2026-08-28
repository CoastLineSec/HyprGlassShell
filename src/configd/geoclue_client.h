#pragma once

#include <QDBusConnection>
#include <QDBusObjectPath>
#include <QObject>
#include <QString>
#include <QVariantMap>

class QDBusServiceWatcher;
class QTimer;

namespace HyprShelld {

class GeoClueClient final : public QObject {
  Q_OBJECT

public:
  struct Options final {
    int maximumAttempts = 4;
    int retryBaseDelayMs = 1000;
    int maximumRetryDelayMs = 30000;
  };

  explicit GeoClueClient(
      QDBusConnection connection = QDBusConnection::systemBus(),
      QObject *parent = nullptr);
  GeoClueClient(QDBusConnection connection, Options options,
                QObject *parent = nullptr);
  ~GeoClueClient() override;

  [[nodiscard]] bool active() const;
  [[nodiscard]] bool available() const;
  [[nodiscard]] double latitude() const;
  [[nodiscard]] double longitude() const;
  [[nodiscard]] QString status() const;

  void start();
  void stop();

signals:
  void changed();

private slots:
  void locationUpdated(const QDBusObjectPath &oldLocation,
                       const QDBusObjectPath &newLocation);
  void clientPropertiesChanged(const QString &interface,
                               const QVariantMap &changed,
                               const QStringList &invalidated);
  void serviceOwnerChanged(const QString &name, const QString &oldOwner,
                           const QString &newOwner);

private:
  void beginAttempt();
  void configureClient(const QDBusObjectPath &path, quint64 generation);
  void readLocation(const QDBusObjectPath &path, quint64 generation);
  void readClientActive(quint64 generation);
  void applyClientActive(bool active);
  void disconnectClientSignals(const QDBusObjectPath &path);
  void cleanupClient(bool notifyServer);
  void requestServerCleanup(const QDBusObjectPath &path) const;
  void scheduleRetry(const QString &message);
  void setState(bool active, bool available, double latitude, double longitude,
                const QString &status);
  void fail(const QString &message, quint64 generation);

  QDBusConnection connection_;
  Options options_;
  QDBusServiceWatcher *serviceWatcher_ = nullptr;
  QTimer *retryTimer_ = nullptr;
  QDBusObjectPath clientPath_;
  quint64 generation_ = 0;
  quint64 serviceEpoch_ = 0;
  int attempts_ = 0;
  bool requested_ = false;
  bool attemptInFlight_ = false;
  bool failureLatched_ = false;
  bool shuttingDown_ = false;
  bool active_ = false;
  bool available_ = false;
  double latitude_ = 0.0;
  double longitude_ = 0.0;
  QString status_ = QStringLiteral("idle");
};

} // namespace HyprShelld
