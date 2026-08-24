#include "compositord/shared_visual_source.h"

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
    Q_PROPERTY(uint ShellInnerSpacing READ shellInnerSpacing)
    Q_PROPERTY(uint ShellOuterSpacing READ shellOuterSpacing)
    Q_PROPERTY(bool SyncHyprlandWindowSpacing READ syncWindowSpacing)
    Q_PROPERTY(qulonglong Revision READ revision)
    Q_PROPERTY(QString RecoveryState READ recoveryState)

public:
    [[nodiscard]] uint barHeight() const { return 40; }
    [[nodiscard]] bool shellBorderEnabled() const { return enabled_; }
    [[nodiscard]] uint shellBorderWidth() const { return width_; }
    [[nodiscard]] uint shellBorderRadius() const { return radius_; }
    [[nodiscard]] bool syncWindowBorders() const { return sync_; }
    [[nodiscard]] uint shellInnerSpacing() const { return innerSpacing_; }
    [[nodiscard]] uint shellOuterSpacing() const { return outerSpacing_; }
    [[nodiscard]] bool syncWindowSpacing() const { return syncSpacing_; }
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

    void setProjection(const SharedVisualProjection &projection)
    {
        enabled_ = projection.borderEnabled;
        width_ = projection.borderWidth;
        radius_ = projection.borderRadius;
        sync_ = projection.syncWindowBorders;
        innerSpacing_ = projection.innerSpacing;
        outerSpacing_ = projection.outerSpacing;
        syncSpacing_ = projection.syncWindowSpacing;
        revision_ = projection.revision;
    }

    void publishBorder(
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

    void publishSpacing(
        const uint inner,
        const uint outer,
        const bool sync,
        const qulonglong revision,
        const QDBusConnection &connection
    )
    {
        innerSpacing_ = inner;
        outerSpacing_ = outer;
        syncSpacing_ = sync;
        revision_ = revision;
        sendPropertiesChanged(
            QVariantMap{
                {QStringLiteral("ShellInnerSpacing"), innerSpacing_},
                {QStringLiteral("ShellOuterSpacing"), outerSpacing_},
                {QStringLiteral("SyncHyprlandWindowSpacing"), syncSpacing_},
                {QStringLiteral("Revision"),
                 QVariant::fromValue<qulonglong>(revision_)},
            },
            {},
            connection
        );
    }

    void publishBoth(
        const SharedVisualProjection &projection,
        const QDBusConnection &connection
    )
    {
        setProjection(projection);
        sendPropertiesChanged(
            QVariantMap{
                {QStringLiteral("ShellBorderEnabled"), enabled_},
                {QStringLiteral("ShellBorderWidth"), width_},
                {QStringLiteral("ShellBorderRadius"), radius_},
                {QStringLiteral("SyncHyprlandWindowBorders"), sync_},
                {QStringLiteral("ShellInnerSpacing"), innerSpacing_},
                {QStringLiteral("ShellOuterSpacing"), outerSpacing_},
                {QStringLiteral("SyncHyprlandWindowSpacing"), syncSpacing_},
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
    uint innerSpacing_ = 8;
    uint outerSpacing_ = 12;
    bool syncSpacing_ = true;
    qulonglong revision_ = 0;
    mutable int revisionReads_ = 0;
};

} // namespace

class CompositorSharedVisualDbusTest final : public QObject
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

        DbusSharedVisualSource source(connection);
        QSignalSpy changed(&source, &SharedVisualSource::changed);
        source.start();
        QTRY_VERIFY(source.available());
        QCOMPARE(source.projection(), SharedVisualProjection{});

        const auto readsBeforeCompleteSignal = service.revisionReads();
        service.publishBorder(false, 12, 9, false, 41, connection);
        QTRY_COMPARE(source.projection().revision, quint64(41));
        QCOMPARE(source.projection().borderEnabled, false);
        QCOMPARE(source.projection().borderWidth, quint32(12));
        QCOMPARE(source.projection().borderRadius, quint32(9));
        QCOMPARE(source.projection().syncWindowBorders, false);
        QCOMPARE(source.projection().innerSpacing, quint32(8));
        QCOMPARE(source.projection().outerSpacing, quint32(12));
        QCOMPARE(source.projection().syncWindowSpacing, true);
        QTest::qWait(20);
        QCOMPARE(service.revisionReads(), readsBeforeCompleteSignal);

        service.publishSpacing(0, 32, false, 42, connection);
        QTRY_COMPARE(source.projection().revision, quint64(42));
        QCOMPARE(source.projection().borderEnabled, false);
        QCOMPARE(source.projection().borderWidth, quint32(12));
        QCOMPARE(source.projection().borderRadius, quint32(9));
        QCOMPARE(source.projection().syncWindowBorders, false);
        QCOMPARE(source.projection().innerSpacing, quint32(0));
        QCOMPARE(source.projection().outerSpacing, quint32(32));
        QCOMPARE(source.projection().syncWindowSpacing, false);

        const auto revisionReads = service.revisionReads();
        service.publishRevision(43, connection);
        QTRY_COMPARE(source.projection().revision, quint64(43));
        QTest::qWait(20);
        QCOMPARE(service.revisionReads(), revisionReads);

        QVector<bool> availabilityChanges;
        connect(&source, &SharedVisualSource::changed, &source, [&] {
            availabilityChanges.append(source.available());
        });
        service.sendPropertiesChanged(
            QVariantMap{
                {QStringLiteral("SyncHyprlandWindowBorders"), true},
                {QStringLiteral("Revision"),
                 QVariant::fromValue<qulonglong>(44)},
            },
            {},
            connection
        );
        QTRY_VERIFY(availabilityChanges.contains(false));
        QTRY_VERIFY(source.available());
        QCOMPARE(source.projection().revision, quint64(43));
        QCOMPARE(source.projection().syncWindowBorders, false);

        availabilityChanges.clear();
        service.sendPropertiesChanged(
            QVariantMap{
                {QStringLiteral("ShellInnerSpacing"), 8U},
                {QStringLiteral("Revision"),
                 QVariant::fromValue<qulonglong>(44)},
            },
            {},
            connection
        );
        QTRY_VERIFY(availabilityChanges.contains(false));
        QTRY_VERIFY(source.available());
        QCOMPARE(source.projection().revision, quint64(43));
        QCOMPARE(source.projection().innerSpacing, quint32(0));

        availabilityChanges.clear();
        service.sendPropertiesChanged(
            QVariantMap{
                {QStringLiteral("ShellInnerSpacing"), 8U},
                {QStringLiteral("ShellOuterSpacing"), 12U},
                {QStringLiteral("SyncHyprlandWindowSpacing"), true},
            },
            {},
            connection
        );
        QTRY_VERIFY(availabilityChanges.contains(false));
        QTRY_VERIFY(source.available());
        QCOMPARE(source.projection().revision, quint64(43));

        service.publishSpacing(8, 33, true, 44, connection);
        QTRY_VERIFY(!source.available());
        QCOMPARE(source.projection().revision, quint64(43));
        QVERIFY(source.error().contains(QStringLiteral("invalid")));

        service.publishBoth(
            SharedVisualProjection{
                .borderEnabled = true,
                .borderWidth = 12,
                .borderRadius = 9,
                .syncWindowBorders = true,
                .innerSpacing = 8,
                .outerSpacing = 12,
                .syncWindowSpacing = true,
                .revision = 45,
            },
            connection
        );
        QTRY_VERIFY(source.available());
        QTRY_COMPARE(source.projection().revision, quint64(45));

        service.publishBorder(false, 7, 6, false, 44, connection);
        QTRY_VERIFY(!source.available());
        QCOMPARE(source.projection().revision, quint64(45));
        QCOMPARE(source.projection().borderWidth, quint32(12));
        QTest::qWait(50);
        QVERIFY(!source.available());
        QCOMPARE(source.projection().revision, quint64(45));

        const SharedVisualProjection restored{
            .borderEnabled = true,
            .borderWidth = 12,
            .borderRadius = 9,
            .syncWindowBorders = true,
            .innerSpacing = 8,
            .outerSpacing = 12,
            .syncWindowSpacing = true,
            .revision = 46,
        };
        service.publishBoth(restored, connection);
        QTRY_VERIFY(source.available());
        QCOMPARE(source.projection(), restored);

        auto equalRevisionChange = restored;
        equalRevisionChange.outerSpacing = 13;
        service.publishBoth(equalRevisionChange, connection);
        QTRY_VERIFY(!source.available());
        QCOMPARE(source.projection(), restored);
        QTest::qWait(50);
        QVERIFY(!source.available());
        QCOMPARE(source.projection(), restored);

        service.publishBoth(restored, connection);
        QTRY_VERIFY(source.available());
        QCOMPARE(source.projection(), restored);

        availabilityChanges.clear();
        service.sendPropertiesChanged(
            {},
            QStringList{QStringLiteral("ShellBorderWidth")},
            connection
        );
        QTRY_VERIFY(availabilityChanges.contains(false));
        QTRY_VERIFY(source.available());
        QCOMPARE(source.projection(), restored);

        QVERIFY(connection.unregisterService(serviceName));
        QTRY_VERIFY(!source.available());
        QCOMPARE(source.projection(), restored);
        QCOMPARE(source.projection().borderWidth, quint32(12));
        QCOMPARE(source.projection().outerSpacing, quint32(12));
        QVERIFY(!source.error().isEmpty());

        auto lowerOwnerProjection = restored;
        lowerOwnerProjection.borderWidth = 2;
        lowerOwnerProjection.outerSpacing = 3;
        lowerOwnerProjection.revision = 2;
        service.setProjection(lowerOwnerProjection);
        QVERIFY(connection.registerService(serviceName));
        QTest::qWait(100);
        QVERIFY(!source.available());
        QCOMPARE(source.projection(), restored);

        QVERIFY(connection.unregisterService(serviceName));
        QTRY_VERIFY(!source.available());
        service.setProjection(restored);
        QVERIFY(connection.registerService(serviceName));
        QTRY_VERIFY(source.available());
        QCOMPARE(source.projection(), restored);

        QVERIFY(connection.unregisterService(serviceName));
        QTRY_VERIFY(!source.available());
        auto higherOwnerProjection = restored;
        higherOwnerProjection.borderWidth = 13;
        higherOwnerProjection.outerSpacing = 14;
        higherOwnerProjection.revision = 47;
        service.setProjection(higherOwnerProjection);
        QVERIFY(connection.registerService(serviceName));
        QTRY_VERIFY(source.available());
        QCOMPARE(source.projection(), higherOwnerProjection);
        QVERIFY(changed.count() >= 20);

        QVERIFY(connection.unregisterService(serviceName));
        connection.unregisterObject(objectPath);
    }
};

QTEST_GUILESS_MAIN(CompositorSharedVisualDbusTest)

#include "compositor_shared_visual_dbus_test.moc"
