#include "component_manager1_adaptor.h"
#include "component_manager_service.h"
#include "componentd_test_fixture.h"
#include "system_catalog.h"

#include <QCoreApplication>
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusMessage>
#include <QDBusVariant>
#include <QElapsedTimer>
#include <QProcess>
#include <QRegularExpression>
#include <QTemporaryDir>
#include <QtTest>

#include <cstdlib>
#include <utility>

namespace {

constexpr auto busName = "org.hyprshelld.ComponentManager1";
constexpr auto objectPath = "/org/hyprshelld/ComponentManager1";
constexpr auto interfaceName = "org.hyprshelld.ComponentManager1";

const QString sourceComponentDirectory = QStringLiteral(
    HYPRSHELLD_SOURCE_COMPONENT_DIR
);

int serveCatalog(const QString &root)
{
    auto connection = QDBusConnection::sessionBus();
    if (!connection.isConnected()) {
        return EXIT_FAILURE;
    }

    auto loaded = HyprShelld::Components::SystemCatalog::load(root);
    if (!loaded.ok()) {
        return EXIT_FAILURE;
    }

    HyprShelld::ComponentManagerService service(
        std::move(*loaded.catalog)
    );
    const ComponentManager1Adaptor adaptor(&service);
    if (!connection.registerObject(
            QString::fromLatin1(objectPath),
            &service,
            QDBusConnection::ExportAdaptors
        )
        || !connection.registerService(QString::fromLatin1(busName))) {
        return EXIT_FAILURE;
    }

    return QCoreApplication::exec();
}

bool isRegistered()
{
    const auto *busInterface = QDBusConnection::sessionBus().interface();
    if (busInterface == nullptr) {
        return false;
    }
    const auto reply = busInterface->isServiceRegistered(
        QString::fromLatin1(busName)
    );
    return reply.isValid() && reply.value();
}

} // namespace

class ComponentdDbusTest final : public QObject {
    Q_OBJECT

private slots:
    void cleanup()
    {
        stopService();
    }

    void catalogIsDigestGuardedAndFullyTyped()
    {
        QTemporaryDir root;
        QVERIFY(root.isValid());

        QString error;
        QVERIFY2(
            HyprShelld::Tests::createValidCatalog(
                root.path(),
                sourceComponentDirectory,
                error
            ),
            qPrintable(error)
        );
        QVERIFY2(startService(root.path(), error), qPrintable(error));

        auto listCall = QDBusMessage::createMethodCall(
            QString::fromLatin1(busName),
            QString::fromLatin1(objectPath),
            QString::fromLatin1(interfaceName),
            QStringLiteral("ListComponents")
        );
        const auto listReply = QDBusConnection::sessionBus().call(listCall);
        QCOMPARE(listReply.type(), QDBusMessage::ReplyMessage);
        QCOMPARE(listReply.arguments().size(), 2);

        const auto id = QString::fromLatin1(
            HyprShelld::Components::workspaceSwitcherId
        );
        QCOMPARE(listReply.arguments().at(0).toStringList(), QStringList{id});
        const auto catalogDigest = listReply.arguments().at(1).toString();
        QVERIFY(
            QRegularExpression(QStringLiteral("^[0-9a-f]{64}$"))
                .match(catalogDigest)
                .hasMatch()
        );

        auto getCall = QDBusMessage::createMethodCall(
            QString::fromLatin1(busName),
            QString::fromLatin1(objectPath),
            QString::fromLatin1(interfaceName),
            QStringLiteral("GetComponent")
        );
        getCall.setArguments({id, catalogDigest});
        const auto getReply = QDBusConnection::sessionBus().call(getCall);
        QCOMPARE(getReply.type(), QDBusMessage::ReplyMessage);
        const auto fields = getReply.arguments();
        QCOMPARE(fields.size(), 25);
        QCOMPARE(fields.at(0).toUInt(), 1U);
        QCOMPARE(fields.at(1).toString(), QStringLiteral("bar-widget"));
        QCOMPARE(fields.at(2).toString(), QStringLiteral("0.1.0"));
        QCOMPARE(fields.at(3).toString(), QStringLiteral("Workspace Switcher"));
        QCOMPARE(
            fields.at(4).toString(),
            QStringLiteral(
                "Shows the workspaces on each monitor and lets the user activate them."
            )
        );
        QCOMPARE(fields.at(5).toStringList(), QStringList{QStringLiteral("CoastLineSec")});
        QCOMPARE(fields.at(6).toStringList(), QStringList{QString()});
        QCOMPARE(
            fields.at(7).toStringList(),
            QStringList{QStringLiteral("https://github.com/CoastLineSec/HyprShelld")}
        );
        QCOMPARE(
            fields.at(11).toString(),
            QStringLiteral(
                "https://github.com/CoastLineSec/HyprShelld/issues"
            )
        );
        QCOMPARE(fields.at(12).toString(), QStringLiteral("1.0"));
        QCOMPARE(fields.at(13).toString(), QStringLiteral("builtin-v1"));
        QCOMPARE(fields.at(14).toString(), QStringLiteral("workspace-switcher"));
        QCOMPARE(fields.at(15).toString(), QString());
        QVERIFY(!fields.at(17).toByteArray().isEmpty());
        QCOMPARE(
            fields.at(18).toStringList(),
            QStringList({
                QStringLiteral("shell.workspaces.activate"),
                QStringLiteral("shell.workspaces.read"),
            })
        );
        QVERIFY(
            QRegularExpression(QStringLiteral("^[0-9a-f]{64}$"))
                .match(fields.at(22).toString())
                .hasMatch()
        );
        QCOMPARE(fields.at(23).toString(), QStringLiteral("system"));
        QCOMPARE(fields.at(24).toBool(), false);

        auto propertyCall = QDBusMessage::createMethodCall(
            QString::fromLatin1(busName),
            QString::fromLatin1(objectPath),
            QStringLiteral("org.freedesktop.DBus.Properties"),
            QStringLiteral("Get")
        );
        propertyCall.setArguments({
            QString::fromLatin1(interfaceName),
            QStringLiteral("CatalogDigest"),
        });
        const auto propertyReply = QDBusConnection::sessionBus().call(
            propertyCall
        );
        QCOMPARE(propertyReply.type(), QDBusMessage::ReplyMessage);
        QCOMPARE(propertyReply.arguments().size(), 1);
        QCOMPARE(
            qvariant_cast<QDBusVariant>(propertyReply.arguments().at(0))
                .variant()
                .toString(),
            catalogDigest
        );
    }

    void staleAndUnknownRequestsUseTypedErrors()
    {
        QTemporaryDir root;
        QVERIFY(root.isValid());

        QString error;
        QVERIFY2(
            HyprShelld::Tests::createValidCatalog(
                root.path(),
                sourceComponentDirectory,
                error
            ),
            qPrintable(error)
        );
        QVERIFY2(startService(root.path(), error), qPrintable(error));

        auto listCall = QDBusMessage::createMethodCall(
            QString::fromLatin1(busName),
            QString::fromLatin1(objectPath),
            QString::fromLatin1(interfaceName),
            QStringLiteral("ListComponents")
        );
        const auto listReply = QDBusConnection::sessionBus().call(listCall);
        QCOMPARE(listReply.type(), QDBusMessage::ReplyMessage);
        const auto catalogDigest = listReply.arguments().at(1).toString();
        QVERIFY(
            QRegularExpression(QStringLiteral("^[0-9a-f]{64}$"))
                .match(catalogDigest)
                .hasMatch()
        );
        auto staleDigest = catalogDigest;
        staleDigest[0] = staleDigest.at(0) == QLatin1Char('0')
            ? QLatin1Char('1')
            : QLatin1Char('0');

        auto getCall = QDBusMessage::createMethodCall(
            QString::fromLatin1(busName),
            QString::fromLatin1(objectPath),
            QString::fromLatin1(interfaceName),
            QStringLiteral("GetComponent")
        );
        getCall.setArguments({
            QString::fromLatin1(HyprShelld::Components::workspaceSwitcherId),
            staleDigest,
        });
        auto reply = QDBusConnection::sessionBus().call(getCall);
        QCOMPARE(reply.type(), QDBusMessage::ErrorMessage);
        QCOMPARE(
            reply.errorName(),
            QStringLiteral(
                "org.hyprshelld.ComponentManager1.Error.StaleCatalogDigest"
            )
        );

        getCall.setArguments({
            QStringLiteral("org.example.missing"),
            catalogDigest,
        });
        reply = QDBusConnection::sessionBus().call(getCall);
        QCOMPARE(reply.type(), QDBusMessage::ErrorMessage);
        QCOMPARE(
            reply.errorName(),
            QStringLiteral(
                "org.hyprshelld.ComponentManager1.Error.UnknownComponent"
            )
        );

        auto unsupportedCall = QDBusMessage::createMethodCall(
            QString::fromLatin1(busName),
            QString::fromLatin1(objectPath),
            QString::fromLatin1(interfaceName),
            QStringLiteral("Install")
        );
        reply = QDBusConnection::sessionBus().call(unsupportedCall);
        QCOMPARE(reply.type(), QDBusMessage::ErrorMessage);
        QCOMPARE(
            reply.errorName(),
            QStringLiteral("org.freedesktop.DBus.Error.UnknownMethod")
        );
    }

    void invalidCatalogExitsWithoutClaimingBus()
    {
        QTemporaryDir root;
        QVERIFY(root.isValid());

        QString error;
        QVERIFY2(
            HyprShelld::Tests::createValidCatalog(
                root.path(),
                sourceComponentDirectory,
                error
            ),
            qPrintable(error)
        );
        const auto manifestPath = HyprShelld::Tests::componentDirectory(
                                      root.path()
                                  )
            + QStringLiteral("/manifest.json");
        QVERIFY2(
            HyprShelld::Tests::replaceFile(
                manifestPath,
                QByteArrayLiteral("{not-json"),
                error
            ),
            qPrintable(error)
        );

        service_.setProgram(QCoreApplication::applicationFilePath());
        service_.setArguments({QStringLiteral("--serve"), root.path()});
        service_.start();
        QVERIFY(service_.waitForStarted(3000));
        QVERIFY(service_.waitForFinished(3000));
        QCOMPARE(service_.exitStatus(), QProcess::NormalExit);
        QVERIFY(service_.exitCode() != EXIT_SUCCESS);
        QVERIFY(!isRegistered());
    }

private:
    bool startService(const QString &root, QString &error)
    {
        service_.setProgram(QCoreApplication::applicationFilePath());
        service_.setArguments({QStringLiteral("--serve"), root});
        service_.setProcessChannelMode(QProcess::MergedChannels);
        service_.start();
        if (!service_.waitForStarted(3000)) {
            error = service_.errorString();
            return false;
        }

        QElapsedTimer timeout;
        timeout.start();
        while (timeout.elapsed() < 3000) {
            if (isRegistered()) {
                return true;
            }
            if (service_.state() == QProcess::NotRunning) {
                error = QString::fromUtf8(service_.readAll());
                return false;
            }
            QTest::qWait(10);
        }

        error = QStringLiteral("ComponentManager1 did not claim its bus name");
        return false;
    }

    void stopService()
    {
        if (service_.state() == QProcess::NotRunning) {
            return;
        }
        service_.terminate();
        if (!service_.waitForFinished(2000)) {
            service_.kill();
            service_.waitForFinished(2000);
        }
    }

    QProcess service_;
};

int main(int argc, char *argv[])
{
    QCoreApplication application(argc, argv);
    if (application.arguments().size() == 3
        && application.arguments().at(1) == QStringLiteral("--serve")) {
        return serveCatalog(application.arguments().at(2));
    }

    ComponentdDbusTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "componentd_dbus_test.moc"
