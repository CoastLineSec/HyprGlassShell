#include "component/component_contract.h"
#include "component/component_configuration.h"
#include "component/package_inspection_report.h"
#include "component_manager_client.h"

#include <QCryptographicHash>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusVirtualObject>
#include <QHash>
#include <QMap>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtEndian>
#include <QtTest>

#include <array>
#include <optional>
#include <utility>

using namespace HyprShelld;

namespace {

const QString serviceName = QStringLiteral("org.hyprshelld.ComponentManager1");
const QString objectPath = QStringLiteral("/org/hyprshelld/ComponentManager1");
const QString interfaceName = QStringLiteral("org.hyprshelld.ComponentManager1");
const QString propertiesInterface = QStringLiteral(
    "org.freedesktop.DBus.Properties"
);
const QString workspaceId = QString::fromLatin1(
    Components::workspaceSwitcherId
);
const QString serviceId = QStringLiteral("org.example.background-service");

void addDigestField(
    QCryptographicHash &hash,
    const QByteArray &name,
    const QByteArray &value
)
{
    std::array<uchar, sizeof(quint64)> length{};
    qToBigEndian<quint64>(static_cast<quint64>(name.size()), length.data());
    hash.addData(QByteArrayView(
        reinterpret_cast<const char *>(length.data()),
        length.size()
    ));
    hash.addData(name);
    qToBigEndian<quint64>(static_cast<quint64>(value.size()), length.data());
    hash.addData(QByteArrayView(
        reinterpret_cast<const char *>(length.data()),
        length.size()
    ));
    hash.addData(value);
}

QString catalogDigest(const QMap<QString, QVariantList> &records)
{
    QCryptographicHash hash(QCryptographicHash::Sha256);
    for (auto iterator = records.cbegin(); iterator != records.cend(); ++iterator) {
        addDigestField(
            hash,
            iterator.key().toUtf8(),
            iterator.value().at(22).toString().toLatin1()
        );
    }
    return QString::fromLatin1(hash.result().toHex());
}

QVariantList workspaceRecord(
    const QString &version = QStringLiteral("0.1.0"),
    const QString &name = QStringLiteral("Workspace Switcher")
)
{
    return {
        1U,
        QStringLiteral("bar-widget"),
        version,
        name,
        QStringLiteral("Shows and activates workspaces."),
        QStringList{QStringLiteral("CoastLineSec")},
        QStringList{QString()},
        QStringList{QStringLiteral(
            "https://github.com/CoastLineSec/HyprShelld"
        )},
        QStringLiteral("LicenseRef-HyprShelld"),
        QStringLiteral("https://github.com/CoastLineSec/HyprShelld"),
        QStringLiteral("https://github.com/CoastLineSec/HyprShelld"),
        QStringLiteral("https://github.com/CoastLineSec/HyprShelld/issues"),
        QStringLiteral("1.0"),
        QStringLiteral("builtin-v1"),
        QStringLiteral("workspace-switcher"),
        QString(),
        QStringList(),
        QByteArrayLiteral("{\"schemaVersion\":1,\"settings\":[]}\n"),
        QStringList{
            QString::fromLatin1(Components::workspacesActivateCapability),
            QString::fromLatin1(Components::workspacesReadCapability),
        },
        QStringList{
            QStringLiteral("Activate a selected workspace."),
            QStringLiteral("Read workspace state."),
        },
        QStringList(),
        QStringList(),
        QString(64, QLatin1Char('a')),
        QStringLiteral("system"),
        false,
    };
}

QVariantList serviceRecord(
    const QString &version = QStringLiteral("1.2.3"),
    const QString &packageDigest = QString(64, QLatin1Char('b'))
)
{
    return {
        1U,
        QStringLiteral("shell-service"),
        version,
        QStringLiteral("Background Service"),
        QStringLiteral("Performs an example background task."),
        QStringList{QStringLiteral("Example Author")},
        QStringList{QStringLiteral("author@example.org")},
        QStringList{QStringLiteral("https://example.org")},
        QStringLiteral("MIT"),
        QStringLiteral("https://example.org"),
        QStringLiteral("https://example.org/source"),
        QStringLiteral("https://example.org/issues"),
        QStringLiteral("1.0"),
        QStringLiteral("process-v1"),
        QString(),
        QStringLiteral("payload/bin/example-service"),
        QStringList{QStringLiteral("--user")},
        QByteArray(),
        QStringList{QStringLiteral("example.background.read")},
        QStringList{QStringLiteral("Read example state.")},
        QStringList(),
        QStringList(),
        packageDigest,
        QStringLiteral("user"),
        true,
    };
}

Components::PackageInspectionReport packageReport(
    const QString &token,
    const QString &version,
    const QString &packageDigest
)
{
    const QJsonObject manifestObject{
        {QStringLiteral("manifestVersion"), 1},
        {QStringLiteral("id"), serviceId},
        {QStringLiteral("version"), version},
        {QStringLiteral("type"), QStringLiteral("shell-service")},
        {QStringLiteral("name"), QStringLiteral("Background Service")},
        {QStringLiteral("description"),
         QStringLiteral("Performs an example background task.")},
        {QStringLiteral("authors"), QJsonArray{QJsonObject{
             {QStringLiteral("name"), QStringLiteral("Example Author")},
         }}},
        {QStringLiteral("license"), QStringLiteral("MIT")},
        {QStringLiteral("componentApiVersion"), QStringLiteral("1.0")},
        {QStringLiteral("runtime"), QJsonObject{
             {QStringLiteral("kind"), QStringLiteral("process-v1")},
             {QStringLiteral("entrypoint"),
              QStringLiteral("payload/bin/example-service")},
             {QStringLiteral("arguments"),
              QJsonArray{QStringLiteral("--user")}},
         }},
        {QStringLiteral("requestedCapabilities"), QJsonArray{QJsonObject{
             {QStringLiteral("id"),
              QStringLiteral("example.background.read")},
             {QStringLiteral("reason"),
              QStringLiteral("Read example state.")},
         }}},
    };
    const auto manifestBytes = QJsonDocument(manifestObject)
                                   .toJson(QJsonDocument::Compact);
    const auto parsed = Components::parseComponentManifest(
        QByteArrayView(manifestBytes),
        Components::ComponentOrigin::User
    );
    Q_ASSERT(parsed);

    Components::PackageInspectionReport report{
        .inspectionToken = token,
        .archiveSha256 = QString(64, QLatin1Char('d')),
        .packageDigest = packageDigest,
        .archiveSize = 3,
        .expandedSize = 3,
        .manifest = *parsed.value,
        .normalizedManifest = manifestObject,
        .normalizedSettingsSchema = std::nullopt,
        .files = {
            {QStringLiteral("integrity.json"), 1,
             QString(64, QLatin1Char('1'))},
            {QStringLiteral("manifest.json"), 1,
             QString(64, QLatin1Char('2'))},
            {QStringLiteral("payload/bin/example-service"), 1,
             QString(64, QLatin1Char('3'))},
        },
    };
    return report;
}

class FakeManager final : public QDBusVirtualObject {
public:
    explicit FakeManager(QDBusConnection connection)
        : connection_(std::move(connection))
    {
        setRecords({{workspaceId, workspaceRecord()}});
    }

    QString introspect(const QString &) const override
    {
        return QStringLiteral(
            "<interface name=\"org.hyprshelld.ComponentManager1\"/>"
        );
    }

    bool handleMessage(
        const QDBusMessage &message,
        const QDBusConnection &
    ) override
    {
        if (message.interface() != interfaceName) {
            return false;
        }
        if (message.member() == QStringLiteral("BeginPackageInspection")) {
            ++beginCalls;
            connection_.send(message.createReply(
                QVariantList{inspectionToken}
            ));
            return true;
        }
        if (message.member() == QStringLiteral("CancelPackageInspection")) {
            ++cancelCalls;
            connection_.send(message.createReply());
            return true;
        }
        if (message.member() == QStringLiteral("InstallInspectedPackage")) {
            ++installCalls;
            lastInstallCatalogDigest = message.arguments().value(2).toString();
            if (failInstallOnce) {
                failInstallOnce = false;
                connection_.send(message.createErrorReply(
                    QStringLiteral(
                        "org.hyprshelld.ComponentManager1.Error.PackageTransactionFailed"
                    ),
                    QStringLiteral("injected install failure")
                ));
                return true;
            }
            const auto report = pendingReport.value();
            auto records = records_;
            records.insert(
                report.manifest.id,
                serviceRecord(report.manifest.version, report.packageDigest)
            );
            setRecords(std::move(records));
            connection_.send(message.createReply({
                report.manifest.id,
                report.packageDigest,
                digest_,
            }));
            return true;
        }
        if (message.member() == QStringLiteral("RemovePackage")) {
            ++removeCalls;
            const auto componentId = message.arguments().value(0).toString();
            auto records = records_;
            records.remove(componentId);
            setRecords(std::move(records));
            connection_.send(message.createReply(QVariantList{digest_}));
            return true;
        }
        if (message.member() == QStringLiteral("ListComponents")) {
            ++listCalls;
            if (malformedListOnce) {
                malformedListOnce = false;
                connection_.send(message.createReply({
                    QStringLiteral("not-a-string-list"),
                    digest_,
                }));
            } else {
                connection_.send(message.createReply({
                    records_.keys(),
                    digest_,
                }));
            }
            return true;
        }
        if (message.member() != QStringLiteral("GetComponent")) {
            return false;
        }

        ++getCalls;
        const auto arguments = message.arguments();
        if (arguments.size() != 2) {
            connection_.send(message.createErrorReply(
                QStringLiteral("org.example.Error.InvalidArguments"),
                QStringLiteral("invalid arguments")
            ));
            return true;
        }
        const auto componentId = arguments.at(0).toString();
        if (malformedGetOnce) {
            malformedGetOnce = false;
            auto malformed = records_.value(componentId);
            malformed[0] = QStringLiteral("1");
            connection_.send(message.createReply(malformed));
            return true;
        }
        if (invalidSchemaOnce) {
            invalidSchemaOnce = false;
            ++invalidSchemaReplies;
            auto invalid = records_.value(componentId);
            invalid[17] = QByteArrayLiteral(
                "{\"schemaVersion\":1,\"settings\":[],\"settings\":[]}"
            );
            connection_.send(message.createReply(invalid));
            return true;
        }
        if (staleGetOnce) {
            staleGetOnce = false;
            ++staleReplies;
            connection_.send(message.createErrorReply(
                QStringLiteral(
                    "org.hyprshelld.ComponentManager1.Error.StaleCatalogDigest"
                ),
                QStringLiteral("injected stale generation")
            ));
            return true;
        }
        if (holdId_ == componentId && !heldMessage_.has_value()) {
            heldMessage_ = message;
            return true;
        }
        connection_.send(message.createReply(records_.value(componentId)));
        return true;
    }

    void setRecords(QMap<QString, QVariantList> records)
    {
        records_ = std::move(records);
        digest_ = catalogDigest(records_);
    }

    void announceCatalogChange()
    {
        auto signal = QDBusMessage::createSignal(
            objectPath,
            propertiesInterface,
            QStringLiteral("PropertiesChanged")
        );
        signal.setArguments({
            interfaceName,
            QVariantMap{{QStringLiteral("CatalogDigest"), digest_}},
            QStringList(),
        });
        QVERIFY(connection_.send(signal));
    }

    void finishInspection(Components::PackageInspectionReport report)
    {
        pendingReport = report;
        auto signal = QDBusMessage::createSignal(
            objectPath,
            interfaceName,
            QStringLiteral("PackageInspectionFinished")
        );
        signal.setArguments({
            report.inspectionToken,
            Components::serializePackageInspectionReport(report),
            QString(),
            QString(),
        });
        QVERIFY(connection_.send(signal));
    }

    void hold(const QString &componentId)
    {
        holdId_ = componentId;
        heldMessage_.reset();
    }

    [[nodiscard]] bool hasHeldReply() const
    {
        return heldMessage_.has_value();
    }

    void releaseHeldReply()
    {
        QVERIFY(heldMessage_.has_value());
        const auto message = *heldMessage_;
        heldMessage_.reset();
        holdId_.clear();
        const auto componentId = message.arguments().at(0).toString();
        QVERIFY(connection_.send(
            message.createReply(records_.value(componentId))
        ));
    }

    bool malformedListOnce = false;
    bool malformedGetOnce = false;
    bool invalidSchemaOnce = false;
    bool staleGetOnce = false;
    bool failInstallOnce = false;
    int listCalls = 0;
    int getCalls = 0;
    int invalidSchemaReplies = 0;
    int staleReplies = 0;
    int beginCalls = 0;
    int cancelCalls = 0;
    int installCalls = 0;
    int removeCalls = 0;
    QString lastInstallCatalogDigest;
    QString inspectionToken = QStringLiteral(
        "00112233445566778899aabbccddeeff"
    );
    std::optional<Components::PackageInspectionReport> pendingReport;

private:
    QDBusConnection connection_;
    QMap<QString, QVariantList> records_;
    QString digest_;
    QString holdId_;
    std::optional<QDBusMessage> heldMessage_;
};

} // namespace

class ComponentManagerClientTest final : public QObject {
    Q_OBJECT

private slots:
    void hydratesOnlyCompleteValidatedGenerationsAndRecovers()
    {
        const auto serviceConnectionName = QStringLiteral(
            "component-manager-client-test-service"
        );
        const auto clientConnectionName = QStringLiteral(
            "component-manager-client-test-client"
        );
        auto serviceBus = QDBusConnection::connectToBus(
            QDBusConnection::SessionBus,
            serviceConnectionName
        );
        auto clientBus = QDBusConnection::connectToBus(
            QDBusConnection::SessionBus,
            clientConnectionName
        );
        QVERIFY(serviceBus.isConnected());
        QVERIFY(clientBus.isConnected());

        FakeManager manager(serviceBus);
        manager.malformedListOnce = true;
        manager.malformedGetOnce = true;
        manager.invalidSchemaOnce = true;
        manager.staleGetOnce = true;
        QVERIFY(serviceBus.registerVirtualObject(objectPath, &manager));
        QVERIFY(serviceBus.registerService(serviceName));

        ComponentManagerClient client(clientBus, nullptr);
        QSignalSpy componentsChanged(
            &client,
            &ComponentManagerClient::componentsChanged
        );
        QSignalSpy availableChanged(
            &client,
            &ComponentManagerClient::availableChanged
        );

        QVERIFY(client.components().isEmpty());
        QTRY_VERIFY_WITH_TIMEOUT(client.available(), 10000);
        QCOMPARE(client.busy(), false);
        QCOMPARE(client.lastError(), QString());
        QCOMPARE(client.components().size(), 1);
        QCOMPARE(componentsChanged.count(), 1);
        QVERIFY(availableChanged.count() >= 1);
        QVERIFY(manager.listCalls >= 5);
        QVERIFY(manager.getCalls >= 4);
        QCOMPARE(manager.invalidSchemaReplies, 1);
        QCOMPARE(manager.staleReplies, 1);

        const auto workspace = client.components().constFirst().toMap();
        QCOMPARE(workspace.value(QStringLiteral("id")).toString(), workspaceId);
        QCOMPARE(
            workspace.value(QStringLiteral("type")).toString(),
            QStringLiteral("bar-widget")
        );
        QCOMPARE(
            workspace.value(QStringLiteral("origin")).toString(),
            QStringLiteral("system")
        );
        QCOMPARE(workspace.value(QStringLiteral("removable")).toBool(), false);
        QCOMPARE(workspace.value(QStringLiteral("hasSettings")).toBool(), true);
        QCOMPARE(
            workspace.value(QStringLiteral("packageDigest")).toString(),
            QString(64, QLatin1Char('a'))
        );
        const auto authors = workspace.value(QStringLiteral("authors")).toList();
        QCOMPARE(authors.size(), 1);
        QCOMPARE(
            authors.constFirst().toMap().value(QStringLiteral("name")).toString(),
            QStringLiteral("CoastLineSec")
        );
        QCOMPARE(
            workspace.value(QStringLiteral("runtime")).toMap().value(
                QStringLiteral("factory")
            ).toString(),
            QStringLiteral("workspace-switcher")
        );
        QCOMPARE(
            workspace.value(QStringLiteral("requestedCapabilities"))
                .toList().size(),
            2
        );

        const auto retained = client.components();
        const auto previousChanges = componentsChanged.count();
        manager.setRecords({
            {workspaceId,
             workspaceRecord(
                 QStringLiteral("0.2.0"),
                 QStringLiteral("Workspace Switcher Updated")
             )},
            {serviceId, serviceRecord()},
        });
        manager.hold(serviceId);
        manager.announceCatalogChange();

        QTRY_VERIFY_WITH_TIMEOUT(manager.hasHeldReply(), 3000);
        QCOMPARE(client.available(), false);
        QCOMPARE(client.busy(), true);
        QCOMPARE(client.components(), retained);
        QCOMPARE(componentsChanged.count(), previousChanges);

        manager.releaseHeldReply();
        QTRY_VERIFY_WITH_TIMEOUT(client.available(), 3000);
        QCOMPARE(client.busy(), false);
        QCOMPARE(client.components().size(), 2);
        QCOMPARE(componentsChanged.count(), previousChanges + 1);
        QCOMPARE(
            client.components().at(0).toMap().value(
                QStringLiteral("name")
            ).toString(),
            QStringLiteral("Workspace Switcher Updated")
        );
        const auto service = client.components().at(1).toMap();
        QCOMPARE(service.value(QStringLiteral("id")).toString(), serviceId);
        QCOMPARE(
            service.value(QStringLiteral("origin")).toString(),
            QStringLiteral("user")
        );
        QCOMPARE(service.value(QStringLiteral("removable")).toBool(), true);
        QCOMPARE(service.value(QStringLiteral("hasSettings")).toBool(), false);

        const auto acceptedRows = client.components();
        const auto changesBeforeLoss = componentsChanged.count();
        QVERIFY(serviceBus.unregisterService(serviceName));
        QTRY_VERIFY_WITH_TIMEOUT(!client.available(), 3000);
        QCOMPARE(client.components(), acceptedRows);
        QCOMPARE(componentsChanged.count(), changesBeforeLoss);
        QVERIFY(!client.lastError().isEmpty());

        manager.setRecords({
            {workspaceId,
             workspaceRecord(
                 QStringLiteral("0.3.0"),
                 QStringLiteral("Workspace Switcher Recovered")
             )},
            {serviceId, serviceRecord()},
        });
        QVERIFY(serviceBus.registerService(serviceName));
        QTRY_VERIFY_WITH_TIMEOUT(client.available(), 5000);
        QCOMPARE(client.lastError(), QString());
        QCOMPARE(client.components().size(), 2);
        QCOMPARE(
            client.components().at(0).toMap().value(
                QStringLiteral("version")
            ).toString(),
            QStringLiteral("0.3.0")
        );
        QCOMPARE(componentsChanged.count(), changesBeforeLoss + 1);

        QVERIFY(serviceBus.unregisterService(serviceName));
        serviceBus.unregisterObject(objectPath);
    }

    void reviewsDowngradeAndReconcilesPackageLifecycleFailures()
    {
        const auto serviceConnectionName = QStringLiteral(
            "component-manager-package-test-service"
        );
        const auto clientConnectionName = QStringLiteral(
            "component-manager-package-test-client"
        );
        auto serviceBus = QDBusConnection::connectToBus(
            QDBusConnection::SessionBus,
            serviceConnectionName
        );
        auto clientBus = QDBusConnection::connectToBus(
            QDBusConnection::SessionBus,
            clientConnectionName
        );
        QVERIFY(serviceBus.isConnected());
        QVERIFY(clientBus.isConnected());

        FakeManager manager(serviceBus);
        manager.setRecords({
            {workspaceId, workspaceRecord()},
            {serviceId, serviceRecord(
                QStringLiteral("2.0.0"),
                QString(64, QLatin1Char('b'))
            )},
        });
        QVERIFY(serviceBus.registerVirtualObject(objectPath, &manager));
        QVERIFY(serviceBus.registerService(serviceName));

        ComponentManagerClient client(clientBus, nullptr);
        QTRY_VERIFY_WITH_TIMEOUT(client.available(), 3000);
        const auto reviewedCatalogDigest = client.catalogDigest();
        QVERIFY(Components::isFullSha256Digest(reviewedCatalogDigest));

        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const auto packagePath = directory.filePath(
            QStringLiteral("fixture.hyprshelld-component")
        );
        {
            QFile package(packagePath);
            QVERIFY(package.open(QIODevice::WriteOnly | QIODevice::NewOnly));
            QCOMPARE(package.write("fixture\n"), 8LL);
        }

        const auto inspect = [&] {
            client.inspectPackage(QUrl::fromLocalFile(packagePath));
            QTRY_VERIFY_WITH_TIMEOUT(!client.inspectionToken().isEmpty(), 3000);
            auto report = packageReport(
                client.inspectionToken(),
                QStringLiteral("1.0.0"),
                QString(64, QLatin1Char('c'))
            );
            manager.finishInspection(std::move(report));
            QTRY_VERIFY_WITH_TIMEOUT(
                !client.inspectionReview().isEmpty(), 3000
            );
            QCOMPARE(
                client.inspectionReview().value(
                    QStringLiteral("operation")
                ).toString(),
                QStringLiteral("downgrade")
            );
        };

        inspect();
        client.inspectPackage(QUrl::fromLocalFile(packagePath));
        QTRY_COMPARE_WITH_TIMEOUT(manager.cancelCalls, 1, 3000);
        QTRY_COMPARE_WITH_TIMEOUT(manager.beginCalls, 2, 3000);
        QTRY_VERIFY_WITH_TIMEOUT(!client.inspectionToken().isEmpty(), 3000);
        manager.finishInspection(packageReport(
            client.inspectionToken(),
            QStringLiteral("1.0.0"),
            QString(64, QLatin1Char('c'))
        ));
        QTRY_VERIFY_WITH_TIMEOUT(
            !client.inspectionReview().isEmpty(), 3000
        );
        manager.failInstallOnce = true;
        client.installInspectedPackage();
        QTRY_VERIFY_WITH_TIMEOUT(!client.packageOperationBusy(), 3000);
        QTRY_VERIFY_WITH_TIMEOUT(!client.packageError().isEmpty(), 3000);
        QVERIFY(client.inspectionToken().isEmpty());
        QVERIFY(client.inspectionReview().isEmpty());
        QTRY_COMPARE_WITH_TIMEOUT(manager.cancelCalls, 2, 3000);

        inspect();
        QSignalSpy installed(&client, &ComponentManagerClient::packageInstalled);
        client.installInspectedPackage();
        QTRY_COMPARE_WITH_TIMEOUT(installed.size(), 1, 3000);
        QCOMPARE(manager.lastInstallCatalogDigest, reviewedCatalogDigest);
        QTRY_VERIFY_WITH_TIMEOUT(client.available(), 3000);
        QTRY_COMPARE_WITH_TIMEOUT(client.components().size(), 2, 3000);
        QCOMPARE(
            client.components().at(1).toMap().value(
                QStringLiteral("packageDigest")
            ).toString(),
            QString(64, QLatin1Char('c'))
        );

        QSignalSpy removed(&client, &ComponentManagerClient::packageRemoved);
        client.removeComponent(
            serviceId,
            QString(64, QLatin1Char('c')),
            client.catalogDigest()
        );
        QTRY_COMPARE_WITH_TIMEOUT(removed.size(), 1, 3000);
        QTRY_VERIFY_WITH_TIMEOUT(client.available(), 3000);
        QTRY_COMPARE_WITH_TIMEOUT(client.components().size(), 1, 3000);
        QCOMPARE(manager.installCalls, 2);
        QCOMPARE(manager.removeCalls, 1);

        QVERIFY(serviceBus.unregisterService(serviceName));
        serviceBus.unregisterObject(objectPath);
        QDBusConnection::disconnectFromBus(clientConnectionName);
        QDBusConnection::disconnectFromBus(serviceConnectionName);
    }
};

QTEST_GUILESS_MAIN(ComponentManagerClientTest)

#include "component_manager_client_test.moc"
