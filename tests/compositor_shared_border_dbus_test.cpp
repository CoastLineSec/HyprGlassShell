#include "compositord/shared_border_source.h"

#include <QDBusConnection>
#include <QDBusMessage>
#include <QSignalSpy>
#include <QtTest>

using namespace HyprShelld::Compositor;

namespace {

const QString serviceName = QStringLiteral("org.hyprshelld.Config1");
const QString objectPath = QStringLiteral("/org/hyprshelld/Config1");
const QString interfaceName = QStringLiteral("org.hyprshelld.Config1");

class FakeConfigService final : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.hyprshelld.Config1")
    Q_PROPERTY(uint BarHeight READ barHeight)
    Q_PROPERTY(bool ShellBorderEnabled READ shellBorderEnabled)
    Q_PROPERTY(uint ShellBorderWidth READ shellBorderWidth)
    Q_PROPERTY(uint ShellBorderRadius READ shellBorderRadius)
    Q_PROPERTY(bool SyncHyprlandWindowBorders READ syncWindowBorders)
    Q_PROPERTY(qulonglong Revision READ revision)
    Q_PROPERTY(QString RecoveryState READ recoveryState)

public:
    [[nodiscard]] uint barHeight() const { return 40; }
    [[nodiscard]] bool shellBorderEnabled() const { return enabled_; }
    [[nodiscard]] uint shellBorderWidth() const { return width_; }
    [[nodiscard]] uint shellBorderRadius() const { return radius_; }
    [[nodiscard]] bool syncWindowBorders() const { return sync_; }
    [[nodiscard]] qulonglong revision() const
    {
        ++revisionReads_;
        return revision_;
    }
    [[nodiscard]] int revisionReads() const { return revisionReads_; }
    [[nodiscard]] QString recoveryState() const
    {
        return QStringLiteral("normal");
    }

    void publish(
        const bool enabled,
        const uint width,
        const uint radius,
        const bool sync,
        const qulonglong revision,
        const QDBusConnection &connection
    )
    {
        enabled_ = enabled;
        width_ = width;
        radius_ = radius;
        sync_ = sync;
        revision_ = revision;
        sendPropertiesChanged(
            QVariantMap{
                {QStringLiteral("ShellBorderEnabled"), enabled_},
                {QStringLiteral("ShellBorderWidth"), width_},
                {QStringLiteral("ShellBorderRadius"), radius_},
                {QStringLiteral("SyncHyprlandWindowBorders"), sync_},
                {QStringLiteral("Revision"),
                 QVariant::fromValue<qulonglong>(revision_)},
            },
            {},
            connection
        );
    }

    void sendPropertiesChanged(
        const QVariantMap &changed,
        const QStringList &invalidated,
        const QDBusConnection &connection
    )
    {
        auto signal = QDBusMessage::createSignal(
            objectPath,
            QStringLiteral("org.freedesktop.DBus.Properties"),
            QStringLiteral("PropertiesChanged")
        );
        signal.setArguments({
            interfaceName,
            changed,
            invalidated,
        });
        QVERIFY(connection.send(signal));
    }

    void publishRevision(
        const qulonglong revision,
        const QDBusConnection &connection
    )
    {
        revision_ = revision;
        sendPropertiesChanged(
            QVariantMap{
                {QStringLiteral("Revision"),
                 QVariant::fromValue<qulonglong>(revision_)},
            },
            {},
            connection
        );
    }

private:
    bool enabled_ = true;
    uint width_ = 1;
    uint radius_ = 0;
    bool sync_ = true;
    qulonglong revision_ = 0;
    mutable int revisionReads_ = 0;
};

} // namespace

class CompositorSharedBorderDbusTest final : public QObject
{
    Q_OBJECT

private slots:
    void observerAcceptsOnlyCompleteGetAllAndRetainsProjectionOnOwnerLoss()
    {
        auto connection = QDBusConnection::sessionBus();
        QVERIFY(connection.isConnected());
        FakeConfigService service;
        QVERIFY(connection.registerObject(
            objectPath,
            &service,
            QDBusConnection::ExportAllProperties
        ));
        QVERIFY(connection.registerService(serviceName));

        DbusSharedBorderSource source(connection);
        QSignalSpy changed(&source, &SharedBorderSource::changed);
        source.start();
        QTRY_VERIFY(source.available());
        QCOMPARE(source.projection(), SharedBorderProjection{});

        const auto readsBeforeCompleteSignal = service.revisionReads();
        service.publish(false, 12, 9, false, 41, connection);
        QTRY_COMPARE(source.projection().revision, quint64(41));
        QCOMPARE(source.projection().borderEnabled, false);
        QCOMPARE(source.projection().borderWidth, quint32(12));
        QCOMPARE(source.projection().borderRadius, quint32(9));
        QCOMPARE(source.projection().syncWindowBorders, false);
        QTest::qWait(20);
        QCOMPARE(service.revisionReads(), readsBeforeCompleteSignal);

        const auto revisionReads = service.revisionReads();
        service.publishRevision(42, connection);
        QTRY_COMPARE(source.projection().revision, quint64(42));
        QCOMPARE(source.projection().borderEnabled, false);
        QCOMPARE(source.projection().borderWidth, quint32(12));
        QCOMPARE(source.projection().borderRadius, quint32(9));
        QCOMPARE(source.projection().syncWindowBorders, false);
        QTest::qWait(20);
        QCOMPARE(service.revisionReads(), revisionReads);

        QVector<bool> availabilityChanges;
        connect(&source, &SharedBorderSource::changed, &source, [&] {
            availabilityChanges.append(source.available());
        });
        service.sendPropertiesChanged(
            QVariantMap{
                {QStringLiteral("SyncHyprlandWindowBorders"), true},
                {QStringLiteral("Revision"),
                 QVariant::fromValue<qulonglong>(43)},
            },
            {},
            connection
        );
        QTRY_VERIFY(availabilityChanges.contains(false));
        QTRY_VERIFY(source.available());
        QCOMPARE(source.projection().revision, quint64(42));
        QCOMPARE(source.projection().syncWindowBorders, false);

        service.publish(true, 21, 0, true, 44, connection);
        QTRY_VERIFY(!source.available());
        QCOMPARE(source.projection().revision, quint64(42));
        QVERIFY(source.error().contains(QStringLiteral("invalid")));

        service.publish(true, 12, 9, true, 45, connection);
        QTRY_VERIFY(source.available());
        QTRY_COMPARE(source.projection().revision, quint64(45));

        availabilityChanges.clear();
        service.sendPropertiesChanged(
            {},
            QStringList{QStringLiteral("ShellBorderWidth")},
            connection
        );
        QTRY_VERIFY(availabilityChanges.contains(false));
        QTRY_VERIFY(source.available());
        QCOMPARE(source.projection().revision, quint64(45));

        QVERIFY(connection.unregisterService(serviceName));
        QTRY_VERIFY(!source.available());
        QCOMPARE(source.projection().revision, quint64(45));
        QCOMPARE(source.projection().borderWidth, quint32(12));
        QVERIFY(!source.error().isEmpty());

        QVERIFY(connection.registerService(serviceName));
        QTRY_VERIFY(source.available());
        QCOMPARE(source.projection().revision, quint64(45));
        QVERIFY(changed.count() >= 10);

        QVERIFY(connection.unregisterService(serviceName));
        connection.unregisterObject(objectPath);
    }
};

QTEST_GUILESS_MAIN(CompositorSharedBorderDbusTest)

#include "compositor_shared_border_dbus_test.moc"
