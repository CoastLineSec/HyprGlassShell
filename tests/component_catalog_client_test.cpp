#include "component_catalog_client.h"
#include "component/component_contract.h"
#include "componentd/system_catalog.h"

#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusVirtualObject>
#include <QCryptographicHash>
#include <QFile>
#include <QSignalSpy>
#include <QtTest>
#include <QtEndian>

#include <array>
#include <utility>

using namespace HyprShelld;

namespace {

const QString serviceName = QStringLiteral("org.hyprshelld.ComponentManager1");
const QString objectPath = QStringLiteral("/org/hyprshelld/ComponentManager1");
const QString interfaceName = QStringLiteral("org.hyprshelld.ComponentManager1");

QString singleEntryCatalogDigest(
    const QString &componentId,
    const QString &packageDigest
)
{
    QCryptographicHash hash(QCryptographicHash::Sha256);
    std::array<uchar, sizeof(quint64)> length{};
    const auto name = componentId.toUtf8();
    const auto value = packageDigest.toLatin1();
    qToBigEndian<quint64>(static_cast<quint64>(name.size()), length.data());
    hash.addData(QByteArrayView(
        reinterpret_cast<const char *>(length.data()), length.size()
    ));
    hash.addData(name);
    qToBigEndian<quint64>(static_cast<quint64>(value.size()), length.data());
    hash.addData(QByteArrayView(
        reinterpret_cast<const char *>(length.data()), length.size()
    ));
    hash.addData(value);
    return QString::fromLatin1(hash.result().toHex());
}

enum class FailureMode {
    ListError,
    MalformedGetReply,
};

class FlakyManager final : public QDBusVirtualObject {
public:
    FlakyManager(
        QDBusConnection connection,
        QString catalogDigest,
        QString packageDigest,
        QByteArray settingsSchema,
        const FailureMode failureMode
    )
        : connection_(std::move(connection))
        , catalogDigest_(std::move(catalogDigest))
        , packageDigest_(std::move(packageDigest))
        , settingsSchema_(std::move(settingsSchema))
        , failureMode_(failureMode)
    {
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
        if (message.member() == QStringLiteral("ListComponents")) {
            ++listCalls;
            if (failureMode_ == FailureMode::ListError && listCalls == 1) {
                connection_.send(message.createErrorReply(
                    QStringLiteral("org.hyprshelld.ComponentManager1.Error.Temporary"),
                    QStringLiteral("injected transient failure")
                ));
            } else {
                connection_.send(message.createReply({
                    QStringList{QString::fromLatin1(
                        Components::workspaceSwitcherId
                    )},
                    catalogDigest_,
                }));
            }
            return true;
        }
        if (message.member() == QStringLiteral("GetComponent")) {
            ++getCalls;
            QVariantList reply{
                1U,
                QStringLiteral("bar-widget"),
                QStringLiteral("0.1.0"),
                QStringLiteral("Workspace Switcher"),
                QStringLiteral("Description"),
                QStringList{QStringLiteral("CoastLineSec")},
                QStringList{QString()},
                QStringList{QString()},
                QStringLiteral("LicenseRef-HyprShelld"),
                QString(),
                QString(),
                QString(),
                QStringLiteral("1.0"),
                QStringLiteral("builtin-v1"),
                QStringLiteral("workspace-switcher"),
                QString(),
                QStringList(),
                settingsSchema_,
                QStringList{
                    QString::fromLatin1(
                        Components::workspacesActivateCapability
                    ),
                    QString::fromLatin1(Components::workspacesReadCapability),
                },
                QStringList{
                    QStringLiteral("Activate a workspace."),
                    QStringLiteral("Read workspace state."),
                },
                QStringList(),
                QStringList(),
                packageDigest_,
                QStringLiteral("system"),
                false,
            };
            if (failureMode_ == FailureMode::MalformedGetReply
                && getCalls == 1) {
                reply[0] = QStringLiteral("1");
            }
            connection_.send(message.createReply(reply));
            return true;
        }
        return false;
    }

    int listCalls = 0;
    int getCalls = 0;

private:
    QDBusConnection connection_;
    QString catalogDigest_;
    QString packageDigest_;
    QByteArray settingsSchema_;
    FailureMode failureMode_;
};

class DeclarativeManager final : public QDBusVirtualObject {
public:
    explicit DeclarativeManager(QDBusConnection connection)
        : connection_(std::move(connection))
        , catalogDigest_(singleEntryCatalogDigest(componentId, packageDigest))
    {
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
        if (message.member() == QStringLiteral("ListComponents")) {
            ++listCalls;
            connection_.send(message.createReply({
                QStringList{componentId}, catalogDigest_,
            }));
            return true;
        }
        if (message.member() == QStringLiteral("GetComponent")) {
            ++getCalls;
            connection_.send(message.createReply(QVariantList{
                1U,
                QStringLiteral("bar-widget"),
                QStringLiteral("1.0.0"),
                QStringLiteral("Clock Widget"),
                QStringLiteral("Shows a trusted data-only label."),
                QStringList{QStringLiteral("Example Author")},
                QStringList{QString()},
                QStringList{QString()},
                QStringLiteral("MIT"),
                QString(),
                QString(),
                QString(),
                QStringLiteral("1.0"),
                QStringLiteral("declarative-v1"),
                QString(),
                QStringLiteral("payload/widget.json"),
                QStringList(),
                QByteArrayLiteral("{\"schemaVersion\":1,\"settings\":[]}\n"),
                QStringList(),
                QStringList(),
                QStringList(),
                QStringList(),
                packageDigest,
                QStringLiteral("user"),
                true,
            }));
            return true;
        }
        if (message.member() == QStringLiteral("GetDeclarativeRuntime")) {
            ++runtimeCalls;
            lastRuntimeArguments = message.arguments();
            const auto definition = std::exchange(
                malformedRuntimeOnce, false
            ) ? QByteArrayLiteral("{}") : runtimeDefinition;
            connection_.send(message.createReply(QVariantList{
                definition,
            }));
            return true;
        }
        return false;
    }

    const QString componentId = QStringLiteral("org.example.clock-widget");
    const QString packageDigest = QString(64, QLatin1Char('d'));
    const QByteArray runtimeDefinition = QByteArrayLiteral(
        R"({"documentVersion":1,"text":{"literal":"Clock"},"type":"text-pill"})"
    );
    bool malformedRuntimeOnce = true;
    int listCalls = 0;
    int getCalls = 0;
    int runtimeCalls = 0;
    QVariantList lastRuntimeArguments;

private:
    QDBusConnection connection_;
    QString catalogDigest_;
};

QByteArray readFile(const QString &path)
{
    QFile file(path);
    return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
}

} // namespace

class ComponentCatalogClientTest final : public QObject {
    Q_OBJECT

private slots:
    void retriesTransientFailureWithoutOwnerChange()
    {
        auto loaded = Components::SystemCatalog::load(
            QStringLiteral(HYPRSHELLD_SOURCE_COMPONENT_DIR)
        );
        QVERIFY2(loaded.ok(), qPrintable(loaded.error));
        const auto *workspace = loaded.catalog->find(QString::fromLatin1(
            Components::workspaceSwitcherId
        ));
        QVERIFY(workspace != nullptr);

        auto bus = QDBusConnection::sessionBus();
        QVERIFY(bus.isConnected());
        FlakyManager manager(
            bus,
            loaded.catalog->catalogDigest(),
            workspace->packageDigest,
            readFile(QStringLiteral(HYPRSHELLD_WORKSPACE_SCHEMA_FILE)),
            FailureMode::ListError
        );
        QVERIFY(bus.registerVirtualObject(objectPath, &manager));
        QVERIFY(bus.registerService(serviceName));

        ComponentCatalogClient client(bus);
        QSignalSpy changed(&client, &ComponentCatalogClient::catalogChanged);
        client.start();
        QTRY_VERIFY_WITH_TIMEOUT(client.available(), 5000);
        QVERIFY(changed.count() >= 1);
        QVERIFY(manager.listCalls >= 2);
        QCOMPARE(client.catalog().digest, loaded.catalog->catalogDigest());
        QCOMPARE(client.catalog().entries.size(), 1);

        bus.unregisterService(serviceName);
        bus.unregisterObject(objectPath);
    }

    void rejectsMalformedReplyBeforeRetrying()
    {
        auto loaded = Components::SystemCatalog::load(
            QStringLiteral(HYPRSHELLD_SOURCE_COMPONENT_DIR)
        );
        QVERIFY2(loaded.ok(), qPrintable(loaded.error));
        const auto *workspace = loaded.catalog->find(QString::fromLatin1(
            Components::workspaceSwitcherId
        ));
        QVERIFY(workspace != nullptr);

        auto bus = QDBusConnection::sessionBus();
        QVERIFY(bus.isConnected());
        FlakyManager manager(
            bus,
            loaded.catalog->catalogDigest(),
            workspace->packageDigest,
            readFile(QStringLiteral(HYPRSHELLD_WORKSPACE_SCHEMA_FILE)),
            FailureMode::MalformedGetReply
        );
        QVERIFY(bus.registerVirtualObject(objectPath, &manager));
        QVERIFY(bus.registerService(serviceName));

        ComponentCatalogClient client(bus);
        client.start();
        QTRY_VERIFY_WITH_TIMEOUT(client.available(), 5000);
        QVERIFY(manager.listCalls >= 2);
        QVERIFY(manager.getCalls >= 2);
        QCOMPARE(client.catalog().digest, loaded.catalog->catalogDigest());

        bus.unregisterService(serviceName);
        bus.unregisterObject(objectPath);
    }

    void verifiesDigestBoundDeclarativeRuntimeBeforeActivation()
    {
        auto bus = QDBusConnection::sessionBus();
        QVERIFY(bus.isConnected());
        DeclarativeManager manager(bus);
        QVERIFY(bus.registerVirtualObject(objectPath, &manager));
        QVERIFY(bus.registerService(serviceName));

        ComponentCatalogClient client(bus);
        client.start();
        QTRY_VERIFY_WITH_TIMEOUT(client.available(), 5000);
        QVERIFY(manager.listCalls >= 2);
        QVERIFY(manager.getCalls >= 2);
        QVERIFY(manager.runtimeCalls >= 2);
        QCOMPARE(manager.lastRuntimeArguments, QVariantList({
            manager.componentId,
            manager.packageDigest,
            client.catalog().digest,
        }));

        const auto entry = client.catalog().entries.value(
            manager.componentId
        );
        QVERIFY(entry.origin == Components::ComponentOrigin::User);
        QVERIFY(entry.type == Components::ComponentType::BarWidget);
        QVERIFY(entry.runtimeKind == Components::RuntimeKind::DeclarativeV1);
        QCOMPARE(entry.activationSupported, true);
        QCOMPARE(entry.declarativeRuntime, manager.runtimeDefinition);
        QVERIFY(entry.requestedCapabilities.isEmpty());
        QVERIFY(entry.dependencyIds.isEmpty());

        bus.unregisterService(serviceName);
        bus.unregisterObject(objectPath);
    }
};

QTEST_GUILESS_MAIN(ComponentCatalogClientTest)
#include "component_catalog_client_test.moc"
