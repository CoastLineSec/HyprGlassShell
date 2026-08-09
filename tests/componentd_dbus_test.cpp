#include "component_manager1_adaptor.h"
#include "component/component_package_bundle.h"
#include "component/package_inspection_report.h"
#include "component/strict_json.h"
#include "component_inspection_sessions.h"
#include "component_inspector_launcher.h"
#include "component_manager_service.h"
#include "componentd_test_fixture.h"
#include "system_catalog.h"
#include "user_package_store.h"

#include <QBuffer>
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusMessage>
#include <QDBusUnixFileDescriptor>
#include <QDBusVariant>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QRegularExpression>
#include <QTemporaryDir>
#include <QTimer>
#include <QUuid>
#include <QtTest>

#include <algorithm>
#include <cstdlib>
#include <memory>
#include <utility>

namespace {

constexpr auto busName = "org.hyprshelld.ComponentManager1";
constexpr auto objectPath = "/org/hyprshelld/ComponentManager1";
constexpr auto interfaceName = "org.hyprshelld.ComponentManager1";

const QString sourceComponentDirectory = QStringLiteral(
    HYPRSHELLD_SOURCE_COMPONENT_DIR
);

const QString userComponentId = QStringLiteral("org.example.catalog-watch");

QString sha256(const QByteArray &bytes)
{
    return QString::fromLatin1(
        QCryptographicHash::hash(bytes, QCryptographicHash::Sha256).toHex()
    );
}

bool writeInspectionFile(const QString &path, const QByteArray &bytes)
{
    QFile file(path);
    return file.open(QIODevice::WriteOnly | QIODevice::Truncate)
        && file.write(bytes) == bytes.size() && file.flush();
}

bool writeInspectionOutputs(
    const HyprShelld::ComponentInspectorLaunchRequest &request,
    QString &error
)
{
    using namespace HyprShelld::Components;

    const auto payload = QByteArrayLiteral("{\"text\":\"catalog watch\"}\n");
    const auto manifestBytes = QJsonDocument(QJsonObject{
        {QStringLiteral("manifestVersion"), 1},
        {QStringLiteral("id"), userComponentId},
        {QStringLiteral("version"), QStringLiteral("1.0.0")},
        {QStringLiteral("type"), QStringLiteral("bar-widget")},
        {QStringLiteral("name"), QStringLiteral("Catalog watcher fixture")},
        {QStringLiteral("description"),
         QStringLiteral("Exercises catalog change publication.")},
        {QStringLiteral("authors"), QJsonArray{
             QJsonObject{{QStringLiteral("name"), QStringLiteral("Fixture")}},
         }},
        {QStringLiteral("license"), QStringLiteral("MIT")},
        {QStringLiteral("componentApiVersion"), QStringLiteral("1.0")},
        {QStringLiteral("runtime"), QJsonObject{
             {QStringLiteral("kind"), QStringLiteral("declarative-v1")},
             {QStringLiteral("entrypoint"),
              QStringLiteral("payload/widget.json")},
         }},
        {QStringLiteral("requestedCapabilities"), QJsonArray{}},
    }).toJson(QJsonDocument::Compact);
    const auto integrityBytes = QJsonDocument(QJsonObject{
        {QStringLiteral("integrityVersion"), 1},
        {QStringLiteral("algorithm"), QStringLiteral("sha256")},
        {QStringLiteral("files"), QJsonObject{
             {QStringLiteral("manifest.json"), sha256(manifestBytes)},
             {QStringLiteral("payload/widget.json"), sha256(payload)},
         }},
    }).toJson(QJsonDocument::Compact);

    QVector<ComponentPackageBundleFile> files{
        {QStringLiteral("manifest.json"), manifestBytes},
        {QStringLiteral("integrity.json"), integrityBytes},
        {QStringLiteral("payload/widget.json"), payload},
    };
    std::ranges::sort(files, {}, &ComponentPackageBundleFile::path);

    const auto parsedManifest = parseComponentManifest(
        QByteArrayView(manifestBytes),
        ComponentOrigin::User
    );
    const auto parsedObject = parseStrictJsonObject(
        QByteArrayView(manifestBytes),
        {.maximumBytes = 128 * 1024, .maximumDepth = 32}
    );
    const QFileInfo spool(request.spoolPath);
    if (!parsedManifest || !parsedObject || !spool.isFile()
        || spool.size() <= 0) {
        error = QStringLiteral("Cannot build the inspected package fixture");
        return false;
    }

    PackageInspectionReport report{
        .inspectionToken = request.token,
        .archiveSha256 = request.archiveDigest,
        .packageDigest = deriveComponentPackageDigest(files),
        .archiveSize = static_cast<quint64>(spool.size()),
        .manifest = *parsedManifest.value,
        .normalizedManifest = *parsedObject.value,
    };
    for (const auto &file : files) {
        report.expandedSize += static_cast<quint64>(file.contents.size());
        report.files.append({
            .path = file.path,
            .size = static_cast<quint64>(file.contents.size()),
            .sha256 = sha256(file.contents),
        });
    }

    QByteArray bundleBytes;
    QBuffer bundle(&bundleBytes);
    if (!bundle.open(QIODevice::WriteOnly)
        || !writeComponentPackageBundle(bundle, files, error)) {
        return false;
    }
    bundle.close();
    if (!writeInspectionFile(
            request.reportPath,
            serializePackageInspectionReport(report)
        )
        || !writeInspectionFile(request.materializedPath, bundleBytes)) {
        error = QStringLiteral("Cannot write the inspected package fixture");
        return false;
    }
    return true;
}

class FakeInspectorLauncher final
    : public HyprShelld::ComponentInspectorLauncher {
public:
    using ComponentInspectorLauncher::ComponentInspectorLauncher;

    bool start(
        const HyprShelld::ComponentInspectorLaunchRequest &request,
        Completion completion,
        QString &error
    ) override
    {
        if (!writeInspectionOutputs(request, error)) {
            return false;
        }
        QTimer::singleShot(
            0,
            this,
            [completion = std::move(completion)]() mutable {
                completion({.success = true});
            }
        );
        return true;
    }

    void cancel(const QString &) override { }
};

class DbusSignalWatcher final : public QObject {
    Q_OBJECT

public slots:
    void propertiesChanged(
        const QString &interface,
        const QVariantMap &changed,
        const QStringList &invalidated
    )
    {
        emit propertiesChangeObserved(interface, changed, invalidated);
    }

    void packageInspectionFinished(
        const QString &token,
        const QByteArray &review,
        const QString &errorCode,
        const QString &errorMessage
    )
    {
        emit inspectionObserved(token, review, errorCode, errorMessage);
    }

signals:
    void propertiesChangeObserved(
        const QString &interface,
        const QVariantMap &changed,
        const QStringList &invalidated
    );
    void inspectionObserved(
        const QString &token,
        const QByteArray &review,
        const QString &errorCode,
        const QString &errorMessage
    );
};

int executeService(HyprShelld::ComponentManagerService &service)
{
    auto connection = QDBusConnection::sessionBus();
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
    return executeService(service);
}

int serveUserCatalog(const QString &root, const QString &storageRoot)
{
    auto connection = QDBusConnection::sessionBus();
    if (!connection.isConnected()) {
        return EXIT_FAILURE;
    }

    auto loaded = HyprShelld::Components::SystemCatalog::load(root);
    if (!loaded.ok()) {
        return EXIT_FAILURE;
    }

    auto store = std::make_unique<
        HyprShelld::Components::UserPackageStore
    >(
        QDir(storageRoot).filePath(QStringLiteral("data")),
        QDir(storageRoot).filePath(QStringLiteral("state"))
    );
    auto sessions = std::make_unique<
        HyprShelld::ComponentInspectionSessions
    >(
        QDir(storageRoot).filePath(QStringLiteral("spool")),
        std::make_unique<FakeInspectorLauncher>()
    );
    HyprShelld::ComponentManagerService service(
        std::move(*loaded.catalog),
        std::move(store),
        std::move(sessions),
        connection
    );
    return executeService(service);
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
        QCOMPARE(fields.at(2).toString(), QStringLiteral("0.2.0"));
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

    void catalogDigestChangesReachIndependentWatcherOnInstallAndRemove()
    {
        QTemporaryDir root;
        QVERIFY(root.isValid());
        const auto catalogRoot = root.filePath(QStringLiteral("catalog"));
        const auto storageRoot = root.filePath(QStringLiteral("storage"));

        QString error;
        QVERIFY2(
            HyprShelld::Tests::createValidCatalog(
                catalogRoot,
                sourceComponentDirectory,
                error
            ),
            qPrintable(error)
        );
        QVERIFY2(
            startUserService(catalogRoot, storageRoot, error),
            qPrintable(error)
        );

        auto client = QDBusConnection::sessionBus();
        const auto watcherConnectionName = QStringLiteral(
            "hyprshelld-componentd-properties-%1"
        ).arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
        auto watcherConnection = QDBusConnection::connectToBus(
            QDBusConnection::SessionBus,
            watcherConnectionName
        );
        QVERIFY(watcherConnection.isConnected());
        QVERIFY(client.baseService() != watcherConnection.baseService());

        DbusSignalWatcher propertyWatcher;
        DbusSignalWatcher inspectionWatcher;
        QVERIFY(watcherConnection.connect(
            QString::fromLatin1(busName),
            QString::fromLatin1(objectPath),
            QStringLiteral("org.freedesktop.DBus.Properties"),
            QStringLiteral("PropertiesChanged"),
            &propertyWatcher,
            SLOT(propertiesChanged(QString,QVariantMap,QStringList))
        ));
        QVERIFY(client.connect(
            QString::fromLatin1(busName),
            QString::fromLatin1(objectPath),
            QString::fromLatin1(interfaceName),
            QStringLiteral("PackageInspectionFinished"),
            &inspectionWatcher,
            SLOT(packageInspectionFinished(QString,QByteArray,QString,QString))
        ));
        QSignalSpy propertySpy(
            &propertyWatcher,
            &DbusSignalWatcher::propertiesChangeObserved
        );
        QSignalSpy inspectionSpy(
            &inspectionWatcher,
            &DbusSignalWatcher::inspectionObserved
        );
        QVERIFY(propertySpy.isValid());
        QVERIFY(inspectionSpy.isValid());

        auto listCall = QDBusMessage::createMethodCall(
            QString::fromLatin1(busName),
            QString::fromLatin1(objectPath),
            QString::fromLatin1(interfaceName),
            QStringLiteral("ListComponents")
        );
        const auto listReply = client.call(listCall);
        QCOMPARE(listReply.type(), QDBusMessage::ReplyMessage);
        const auto initialDigest = listReply.arguments().at(1).toString();

        const auto archivePath = root.filePath(QStringLiteral("fixture.pkg"));
        const auto archiveBytes = QByteArrayLiteral("local package fixture\n");
        {
            QFile archive(archivePath);
            QVERIFY(archive.open(QIODevice::WriteOnly | QIODevice::NewOnly));
            QCOMPARE(archive.write(archiveBytes), archiveBytes.size());
            QVERIFY(archive.flush());
        }
        QFile archive(archivePath);
        QVERIFY(archive.open(QIODevice::ReadOnly));
        const QDBusUnixFileDescriptor descriptor(archive.handle());
        QVERIFY(descriptor.isValid());

        auto beginCall = QDBusMessage::createMethodCall(
            QString::fromLatin1(busName),
            QString::fromLatin1(objectPath),
            QString::fromLatin1(interfaceName),
            QStringLiteral("BeginPackageInspection")
        );
        beginCall.setArguments({QVariant::fromValue(descriptor)});
        const auto beginReply = client.call(beginCall);
        QCOMPARE(beginReply.type(), QDBusMessage::ReplyMessage);
        const auto token = beginReply.arguments().constFirst().toString();
        QVERIFY(
            QRegularExpression(QStringLiteral("^[0-9a-f]{32}$"))
                .match(token)
                .hasMatch()
        );

        QTRY_COMPARE_WITH_TIMEOUT(inspectionSpy.size(), 1, 3000);
        const auto inspection = inspectionSpy.takeFirst();
        QCOMPARE(inspection.at(0).toString(), token);
        QCOMPARE(inspection.at(2).toString(), QString());
        QCOMPARE(inspection.at(3).toString(), QString());
        const auto report =
            HyprShelld::Components::parsePackageInspectionReport(
                QByteArrayView(inspection.at(1).toByteArray())
            );
        QVERIFY(report);
        QCOMPARE(report.value->archiveSha256, sha256(archiveBytes));

        auto installCall = QDBusMessage::createMethodCall(
            QString::fromLatin1(busName),
            QString::fromLatin1(objectPath),
            QString::fromLatin1(interfaceName),
            QStringLiteral("InstallInspectedPackage")
        );
        auto staleCatalogDigest = initialDigest;
        staleCatalogDigest[0] = staleCatalogDigest.at(0) == QLatin1Char('0')
            ? QLatin1Char('1') : QLatin1Char('0');
        installCall.setArguments({
            token,
            report.value->archiveSha256,
            staleCatalogDigest,
        });
        auto staleInstallReply = client.call(installCall);
        QCOMPARE(staleInstallReply.type(), QDBusMessage::ErrorMessage);
        QCOMPARE(
            staleInstallReply.errorName(),
            QStringLiteral(
                "org.hyprshelld.ComponentManager1.Error.StaleCatalogDigest"
            )
        );

        // The stale generation is checked before consuming the sender-bound
        // inspection, so the exact reviewed token remains usable against its
        // original generation.
        installCall.setArguments({
            token,
            report.value->archiveSha256,
            initialDigest,
        });
        const auto installReply = client.call(installCall);
        QCOMPARE(installReply.type(), QDBusMessage::ReplyMessage);
        QCOMPARE(installReply.arguments().size(), 3);
        QCOMPARE(installReply.arguments().at(0).toString(), userComponentId);
        const auto packageDigest = installReply.arguments().at(1).toString();
        const auto installedCatalogDigest =
            installReply.arguments().at(2).toString();
        QCOMPARE(packageDigest, report.value->packageDigest);
        QVERIFY(installedCatalogDigest != initialDigest);

        QTRY_COMPARE_WITH_TIMEOUT(propertySpy.size(), 1, 3000);
        auto change = propertySpy.takeFirst();
        QCOMPARE(change.at(0).toString(), QString::fromLatin1(interfaceName));
        QCOMPARE(change.at(2).toStringList(), QStringList());
        auto changed = change.at(1).toMap();
        QCOMPARE(changed.size(), 1);
        QCOMPARE(
            changed.value(QStringLiteral("CatalogDigest")).toString(),
            installedCatalogDigest
        );

        auto removeCall = QDBusMessage::createMethodCall(
            QString::fromLatin1(busName),
            QString::fromLatin1(objectPath),
            QString::fromLatin1(interfaceName),
            QStringLiteral("RemovePackage")
        );
        removeCall.setArguments({
            userComponentId,
            packageDigest,
            installedCatalogDigest,
        });
        const auto removeReply = client.call(removeCall);
        QCOMPARE(removeReply.type(), QDBusMessage::ReplyMessage);
        QCOMPARE(removeReply.arguments().size(), 1);
        const auto removedCatalogDigest =
            removeReply.arguments().constFirst().toString();
        QCOMPARE(removedCatalogDigest, initialDigest);

        QTRY_COMPARE_WITH_TIMEOUT(propertySpy.size(), 1, 3000);
        change = propertySpy.takeFirst();
        QCOMPARE(change.at(0).toString(), QString::fromLatin1(interfaceName));
        QCOMPARE(change.at(2).toStringList(), QStringList());
        changed = change.at(1).toMap();
        QCOMPARE(changed.size(), 1);
        QCOMPARE(
            changed.value(QStringLiteral("CatalogDigest")).toString(),
            removedCatalogDigest
        );

        QDBusConnection::disconnectFromBus(watcherConnectionName);
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
        return startServiceProcess(
            {QStringLiteral("--serve"), root},
            error
        );
    }

    bool startUserService(
        const QString &root,
        const QString &storageRoot,
        QString &error
    )
    {
        return startServiceProcess(
            {QStringLiteral("--serve-user"), root, storageRoot},
            error
        );
    }

    bool startServiceProcess(const QStringList &arguments, QString &error)
    {
        service_.setProgram(QCoreApplication::applicationFilePath());
        service_.setArguments(arguments);
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
    if (application.arguments().size() == 4
        && application.arguments().at(1) == QStringLiteral("--serve-user")) {
        return serveUserCatalog(
            application.arguments().at(2),
            application.arguments().at(3)
        );
    }

    ComponentdDbusTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "componentd_dbus_test.moc"
