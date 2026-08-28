#include "configd/geoclue_client.h"

#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusContext>
#include <QDBusError>
#include <QDBusMessage>
#include <QDBusObjectPath>
#include <QSignalSpy>
#include <QTest>
#include <QTimer>

namespace {

const QString service = QStringLiteral("org.freedesktop.GeoClue2");
const QString managerPath = QStringLiteral("/org/freedesktop/GeoClue2/Manager");
const QString clientPath = QStringLiteral("/org/freedesktop/GeoClue2/Client/1");
const QString validLocationPath =
    QStringLiteral("/org/freedesktop/GeoClue2/Location/valid");
const QString invalidLocationPath =
    QStringLiteral("/org/freedesktop/GeoClue2/Location/invalid");
const QString clientInterface =
    QStringLiteral("org.freedesktop.GeoClue2.Client");
const QString propertiesInterface =
    QStringLiteral("org.freedesktop.DBus.Properties");
const QString settingsDesktopId =
    QStringLiteral("io.github.CoastLineSec.HyprShelld.Settings");
constexpr quint32 cityAccuracyLevel = 4;

class FakeGeoClueManager final : public QObject {
  Q_OBJECT
  Q_CLASSINFO("D-Bus Interface", "org.freedesktop.GeoClue2.Manager")

public:
  int getClientCalls = 0;
  int deleteClientCalls = 0;
  int failingGetClientCalls = 0;

public slots:
  QDBusObjectPath GetClient() {
    ++getClientCalls;
    if (failingGetClientCalls > 0) {
      --failingGetClientCalls;
      return QDBusObjectPath(QStringLiteral("/"));
    }
    return QDBusObjectPath(clientPath);
  }

  void DeleteClient(const QDBusObjectPath &path) {
    if (path.path() == clientPath) {
      ++deleteClientCalls;
    }
  }
};

class FakeGeoClueClient final : public QObject, protected QDBusContext {
  Q_OBJECT
  Q_CLASSINFO("D-Bus Interface", "org.freedesktop.GeoClue2.Client")
  Q_PROPERTY(QString DesktopId READ desktopId WRITE setDesktopId)
  Q_PROPERTY(quint32 RequestedAccuracyLevel READ requestedAccuracyLevel WRITE
                 setRequestedAccuracyLevel)
  Q_PROPERTY(bool Active READ active)

public:
  explicit FakeGeoClueClient(QDBusConnection connection)
      : connection_(std::move(connection)) {}

  [[nodiscard]] QString desktopId() const { return desktopId_; }
  void setDesktopId(const QString &desktopId) { desktopId_ = desktopId; }
  [[nodiscard]] quint32 requestedAccuracyLevel() const {
    return requestedAccuracyLevel_;
  }
  void setRequestedAccuracyLevel(const quint32 accuracyLevel) {
    requestedAccuracyLevel_ = accuracyLevel;
  }
  [[nodiscard]] bool active() const { return active_; }

  int startCalls = 0;
  int stopCalls = 0;
  int startReplyDelayMs = 0;

  void publishActive(const bool active, const bool invalidate = false) {
    active_ = active;
    auto signal = QDBusMessage::createSignal(
        clientPath, propertiesInterface, QStringLiteral("PropertiesChanged"));
    signal.setArguments({
        clientInterface,
        invalidate ? QVariantMap()
                   : QVariantMap{{QStringLiteral("Active"), active}},
        invalidate ? QStringList{QStringLiteral("Active")} : QStringList(),
    });
    QVERIFY(connection_.send(signal));
  }

  void publishLocation(const QString &path) {
    emit LocationUpdated(QDBusObjectPath(QStringLiteral("/")),
                         QDBusObjectPath(path));
  }

public slots:
  void Start() {
    ++startCalls;
    if (desktopId_ != settingsDesktopId ||
        requestedAccuracyLevel_ != cityAccuracyLevel) {
      sendErrorReply(QDBusError::InvalidArgs,
                     QStringLiteral("GeoClue client is not configured"));
      return;
    }
    active_ = true;
    if (startReplyDelayMs <= 0) {
      return;
    }

    setDelayedReply(true);
    const auto reply = message().createReply();
    QTimer::singleShot(
        startReplyDelayMs, this,
        [connection = connection_, reply] { connection.send(reply); });
  }

  void Stop() {
    ++stopCalls;
    active_ = false;
  }

signals:
  void LocationUpdated(const QDBusObjectPath &oldLocation,
                       const QDBusObjectPath &newLocation);

private:
  QDBusConnection connection_;
  QString desktopId_;
  quint32 requestedAccuracyLevel_ = 0;
  bool active_ = false;
};

class ValidLocation final : public QObject {
  Q_OBJECT
  Q_CLASSINFO("D-Bus Interface", "org.freedesktop.GeoClue2.Location")
  Q_PROPERTY(double Latitude READ latitude CONSTANT)
  Q_PROPERTY(double Longitude READ longitude CONSTANT)

public:
  [[nodiscard]] double latitude() const { return 40.7128; }
  [[nodiscard]] double longitude() const { return -74.0060; }
};

class InvalidLocation final : public QObject {
  Q_OBJECT
  Q_CLASSINFO("D-Bus Interface", "org.freedesktop.GeoClue2.Location")
  Q_PROPERTY(QString Latitude READ latitude CONSTANT)
  Q_PROPERTY(double Longitude READ longitude CONSTANT)

public:
  [[nodiscard]] QString latitude() const { return QStringLiteral("40.7128"); }
  [[nodiscard]] double longitude() const { return -74.0060; }
};

HyprShelld::GeoClueClient::Options fastOptions(const int maximumAttempts = 3) {
  return {
      .maximumAttempts = maximumAttempts,
      .retryBaseDelayMs = 5,
      .maximumRetryDelayMs = 20,
  };
}

} // namespace

class GeoClueClientTest final : public QObject {
  Q_OBJECT

private slots:
  void initTestCase();
  void init();
  void cleanup();

  void retriesToBoundAndLatchesUntilRestarted();
  void configuresCityAccuracyBeforeStarting();
  void stoppingWhileStartIsInFlightCleansServerClient();
  void rejectsCoordinatesWithWrongDbusTypes();
  void followsActiveChangesAndReacquiresAfterServiceRestart();

private:
  QDBusConnection bus_ = QDBusConnection::sessionBus();
  FakeGeoClueManager manager_;
  FakeGeoClueClient client_{bus_};
  ValidLocation validLocation_;
  InvalidLocation invalidLocation_;
};

void GeoClueClientTest::initTestCase() {
  QVERIFY2(bus_.isConnected(), qPrintable(bus_.lastError().message()));
  QVERIFY(bus_.registerObject(managerPath, &manager_,
                              QDBusConnection::ExportAllSlots));
  QVERIFY(bus_.registerObject(clientPath, &client_,
                              QDBusConnection::ExportAllSlots |
                                  QDBusConnection::ExportAllSignals |
                                  QDBusConnection::ExportAllProperties));
  QVERIFY(bus_.registerObject(validLocationPath, &validLocation_,
                              QDBusConnection::ExportAllProperties));
  QVERIFY(bus_.registerObject(invalidLocationPath, &invalidLocation_,
                              QDBusConnection::ExportAllProperties));
}

void GeoClueClientTest::init() {
  manager_.getClientCalls = 0;
  manager_.deleteClientCalls = 0;
  manager_.failingGetClientCalls = 0;
  client_.startCalls = 0;
  client_.stopCalls = 0;
  client_.startReplyDelayMs = 0;
  client_.setDesktopId(QString());
  client_.setRequestedAccuracyLevel(0);
  QVERIFY(bus_.registerService(service));
}

void GeoClueClientTest::cleanup() {
  if (bus_.interface()->isServiceRegistered(service).value()) {
    QVERIFY(bus_.unregisterService(service));
  }
  QCoreApplication::processEvents();
}

void GeoClueClientTest::retriesToBoundAndLatchesUntilRestarted() {
  manager_.failingGetClientCalls = 10;
  HyprShelld::GeoClueClient geoclue(bus_, fastOptions());
  QSignalSpy changed(&geoclue, &HyprShelld::GeoClueClient::changed);

  geoclue.start();
  QTRY_COMPARE_WITH_TIMEOUT(manager_.getClientCalls, 3, 1000);
  QVERIFY(!geoclue.active());
  QVERIFY(!geoclue.available());
  QVERIFY(geoclue.status().contains(QStringLiteral("no client")));
  QVERIFY(changed.count() >= 5);

  QTest::qWait(100);
  QCOMPARE(manager_.getClientCalls, 3);
  geoclue.start();
  QTest::qWait(50);
  QCOMPARE(manager_.getClientCalls, 3);

  geoclue.stop();
  manager_.failingGetClientCalls = 0;
  geoclue.start();
  QTRY_COMPARE_WITH_TIMEOUT(client_.startCalls, 1, 1000);
  QTRY_VERIFY_WITH_TIMEOUT(geoclue.active(), 1000);
}

void GeoClueClientTest::configuresCityAccuracyBeforeStarting() {
  HyprShelld::GeoClueClient geoclue(bus_, fastOptions());

  geoclue.start();
  QTRY_COMPARE_WITH_TIMEOUT(client_.startCalls, 1, 1000);
  QCOMPARE(client_.desktopId(), settingsDesktopId);
  QCOMPARE(client_.requestedAccuracyLevel(), cityAccuracyLevel);
  QTRY_VERIFY_WITH_TIMEOUT(geoclue.active(), 1000);
}

void GeoClueClientTest::stoppingWhileStartIsInFlightCleansServerClient() {
  client_.startReplyDelayMs = 150;
  HyprShelld::GeoClueClient geoclue(bus_, fastOptions());

  geoclue.start();
  QTRY_COMPARE_WITH_TIMEOUT(client_.startCalls, 1, 1000);
  QVERIFY(client_.active());
  geoclue.stop();

  QTRY_COMPARE_WITH_TIMEOUT(client_.stopCalls, 1, 1000);
  QTRY_COMPARE_WITH_TIMEOUT(manager_.deleteClientCalls, 1, 1000);
  QVERIFY(!client_.active());
  QVERIFY(!geoclue.active());
  QVERIFY(!geoclue.available());
  QCOMPARE(geoclue.status(), QStringLiteral("idle"));

  QTest::qWait(200);
  QCOMPARE(manager_.getClientCalls, 1);
  QCOMPARE(geoclue.status(), QStringLiteral("idle"));
}

void GeoClueClientTest::rejectsCoordinatesWithWrongDbusTypes() {
  HyprShelld::GeoClueClient geoclue(bus_, fastOptions(1));
  geoclue.start();
  QTRY_VERIFY_WITH_TIMEOUT(geoclue.active(), 1000);

  client_.publishLocation(invalidLocationPath);
  QTRY_COMPARE_WITH_TIMEOUT(manager_.deleteClientCalls, 1, 1000);
  QTRY_COMPARE_WITH_TIMEOUT(client_.stopCalls, 1, 1000);
  QVERIFY(!geoclue.active());
  QVERIFY(!geoclue.available());
  QCOMPARE(geoclue.status(),
           QStringLiteral("GeoClue returned invalid coordinates"));

  QTest::qWait(100);
  QCOMPARE(manager_.getClientCalls, 1);
}

void GeoClueClientTest::followsActiveChangesAndReacquiresAfterServiceRestart() {
  HyprShelld::GeoClueClient geoclue(bus_, fastOptions());
  geoclue.start();
  QTRY_VERIFY_WITH_TIMEOUT(geoclue.active(), 1000);
  client_.publishLocation(validLocationPath);
  QTRY_VERIFY_WITH_TIMEOUT(geoclue.available(), 1000);
  QCOMPARE(geoclue.latitude(), 40.7128);
  QCOMPARE(geoclue.longitude(), -74.0060);

  client_.publishActive(false, true);
  QTRY_VERIFY_WITH_TIMEOUT(client_.startCalls >= 2, 1000);
  QTRY_VERIFY_WITH_TIMEOUT(geoclue.active(), 1000);
  QVERIFY(!geoclue.available());
  client_.publishLocation(validLocationPath);
  QTRY_VERIFY_WITH_TIMEOUT(geoclue.available(), 1000);

  const auto startsBeforeRestart = client_.startCalls;
  QVERIFY(bus_.unregisterService(service));
  QTRY_VERIFY_WITH_TIMEOUT(!geoclue.active(), 1000);
  QVERIFY(!geoclue.available());
  QCOMPARE(geoclue.status(), QStringLiteral("GeoClue service is unavailable"));

  QVERIFY(bus_.registerService(service));
  QTRY_VERIFY_WITH_TIMEOUT(client_.startCalls > startsBeforeRestart, 1000);
  QTRY_VERIFY_WITH_TIMEOUT(geoclue.active(), 1000);
}

QTEST_GUILESS_MAIN(GeoClueClientTest)

#include "geoclue_client_test.moc"
