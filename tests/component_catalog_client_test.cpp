#include "component_catalog_client.h"
#include "component/component_contract.h"
#include "componentd/system_catalog.h"

#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusVirtualObject>
#include <QFile>
#include <QSignalSpy>
#include <QtTest>

using namespace HyprShelld;

namespace {

const QString serviceName = QStringLiteral("org.hyprshelld.ComponentManager1");
const QString objectPath = QStringLiteral("/org/hyprshelld/ComponentManager1");
const QString interfaceName = QStringLiteral("org.hyprshelld.ComponentManager1");

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
};

QTEST_GUILESS_MAIN(ComponentCatalogClientTest)
#include "component_catalog_client_test.moc"
