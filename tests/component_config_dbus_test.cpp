#include "component_config1_adaptor.h"
#include "component_config_client.h"
#include "component_config_service.h"
#include "component_config_test_fixture.h"
#include "config_service.h"

#include <QDBusConnection>
#include <QDBusContext>
#include <QDBusMessage>
#include <QDBusPendingCallWatcher>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>

#include <utility>
#include <algorithm>

#include <sys/stat.h>

using namespace HyprShelld;

namespace {

const QString serviceName = QStringLiteral("org.hyprshelld.Config1");
const QString objectPath = QStringLiteral("/org/hyprshelld/Config1/Components");
const QString interfaceName = QStringLiteral("org.hyprshelld.ComponentConfig1");
const QString propertiesInterface = QStringLiteral(
    "org.freedesktop.DBus.Properties"
);
const QString workspaceId = QString::fromLatin1(
    Components::workspaceSwitcherId
);
const QString previousWorkspaceDigest = QStringLiteral(
    "f4febcab5a093a803d35b93ae5300df3149f9bff5a571c759c771fe61699f0f7"
);

Components::ConfigurationCatalog catalog()
{
    return Tests::configurationCatalog(
        QStringLiteral(HYPRSHELLD_COMPONENT_DEFAULTS_FILE),
        QStringLiteral(HYPRSHELLD_WORKSPACE_SCHEMA_FILE)
    );
}

Components::ComponentConfiguration previousWorkspaceConfiguration()
{
    const auto liveCatalog = catalog();
    const auto parsed = Components::parseComponentConfiguration(
        QByteArrayView(Tests::readBytes(QStringLiteral(
            HYPRSHELLD_COMPONENT_DEFAULTS_FILE
        ))),
        liveCatalog
    );
    Q_ASSERT(parsed);
    auto state = *parsed.value;
    state.revision = 12;
    state.components[workspaceId].packageDigest = previousWorkspaceDigest;
    state.components[workspaceId].enabled = false;
    state.instances.first().enabled = false;
    state.instances.first().settings = {
        {QStringLiteral("labelMode"), QStringLiteral("names")},
        {QStringLiteral("showApplications"), true},
        {QStringLiteral("maximumApplications"), 5},
        {QStringLiteral("occupiedOnly"), true},
        {QStringLiteral("scrollMode"), QStringLiteral("reversed")},
    };
    return state;
}

QByteArray withInstanceSetting(
    const QByteArray &snapshot,
    const QString &key,
    const QJsonValue &value
)
{
    auto root = QJsonDocument::fromJson(snapshot).object();
    auto instances = root.value(QStringLiteral("instances")).toObject();
    const auto instanceId = instances.begin().key();
    auto instance = instances.value(instanceId).toObject();
    auto settings = instance.value(QStringLiteral("settings")).toObject();
    settings.insert(key, value);
    instance.insert(QStringLiteral("settings"), settings);
    instances.insert(instanceId, instance);
    root.insert(QStringLiteral("instances"), instances);
    auto bytes = QJsonDocument(root).toJson(QJsonDocument::Compact);
    bytes.append('\n');
    return bytes;
}

QDBusMessage callMethod(
    const QDBusConnection &bus,
    const QString &method,
    const QVariantList &arguments = {}
)
{
    auto message = QDBusMessage::createMethodCall(
        serviceName,
        objectPath,
        interfaceName,
        method
    );
    message.setArguments(arguments);
    QDBusPendingCallWatcher watcher(bus.asyncCall(message, 3000));
    QSignalSpy finished(&watcher, &QDBusPendingCallWatcher::finished);
    if (!watcher.isFinished()) {
        finished.wait(5000);
    }
    return watcher.reply();
}

QDBusMessage replaceCall(
    const QDBusConnection &bus,
    const qulonglong expectedRevision,
    const QString &expectedDigest,
    const QByteArray &candidate
)
{
    return callMethod(
        bus,
        QStringLiteral("ReplaceSnapshot"),
        {
            QVariant::fromValue<qulonglong>(expectedRevision),
            expectedDigest,
            candidate,
        }
    );
}

} // namespace

class ComponentPropertyRecorder final : public QObject {
    Q_OBJECT

public:
    explicit ComponentPropertyRecorder(QString activeFile = {})
        : activeFile_(std::move(activeFile))
    {
    }

    int revisionSignals = 0;
    QVector<quint64> persistedRevisions;

public slots:
    void propertiesChanged(
        const QString &changedInterface,
        const QVariantMap &changed,
        const QStringList &
    )
    {
        if (changedInterface != QStringLiteral(
                "org.hyprshelld.ComponentConfig1"
            )
            || !changed.contains(QStringLiteral("Revision"))) {
            return;
        }
        ++revisionSignals;
        if (!activeFile_.isEmpty()) {
            const auto root = QJsonDocument::fromJson(
                Tests::readBytes(activeFile_)
            ).object();
            persistedRevisions.append(
                root.value(QStringLiteral("revision")).toString().toULongLong()
            );
        }
        emit revisionPublished();
    }

signals:
    void revisionPublished();

private:
    QString activeFile_;
};

class DelayedSnapshotService final : public QObject, protected QDBusContext {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.hyprshelld.ComponentConfig1")
    Q_PROPERTY(bool Available READ available)
    Q_PROPERTY(bool CatalogAvailable READ catalogAvailable)
    Q_PROPERTY(qulonglong Revision READ revision)
    Q_PROPERTY(QString CatalogDigest READ catalogDigest)
    Q_PROPERTY(QString LoadState READ loadState)

public:
    explicit DelayedSnapshotService(
        QDBusConnection connection,
        QString catalogDigest = QString(64, QLatin1Char('a'))
    )
        : connection_(std::move(connection))
        , snapshot_(Tests::readBytes(QStringLiteral(
              HYPRSHELLD_COMPONENT_DEFAULTS_FILE
          )))
        , catalogDigest_(std::move(catalogDigest))
    {
    }

    [[nodiscard]] bool available() const { return true; }
    [[nodiscard]] bool catalogAvailable() const { return true; }
    [[nodiscard]] qulonglong revision() const { return revision_; }
    [[nodiscard]] QString catalogDigest() const { return catalogDigest_; }
    [[nodiscard]] QString loadState() const { return QStringLiteral("normal"); }
    [[nodiscard]] int replaceCalls() const { return replaceCalls_; }
    [[nodiscard]] int heldSnapshotCalls() const
    {
        return heldSnapshotCalls_.size();
    }
    [[nodiscard]] int snapshotCalls() const { return snapshotCalls_; }
    [[nodiscard]] int heldReplacementCalls() const
    {
        return heldReplacementCalls_.size();
    }

    void setHoldSnapshots(const bool hold) { holdSnapshots_ = hold; }
    void setHoldReplacements(const bool hold) { holdReplacements_ = hold; }
    void setPublishRevisionSignals(const bool publish)
    {
        publishRevisionSignals_ = publish;
    }

    void releaseSnapshots(const bool malformed = false)
    {
        const auto held = std::exchange(heldSnapshotCalls_, {});
        for (const auto &heldCall : held) {
            connection_.send(heldCall.call.createReply(
                malformed
                    ? QVariantList{QByteArrayLiteral("malformed")}
                    : QVariantList{
                        heldCall.snapshot,
                        QVariant::fromValue<qulonglong>(heldCall.revision),
                        heldCall.catalogDigest,
                    }
            ));
        }
    }

    void externallySetOccupiedOnly(const bool invalidateRevision = false)
    {
        auto root = QJsonDocument::fromJson(snapshot_).object();
        auto instances = root.value(QStringLiteral("instances")).toObject();
        auto iterator = instances.begin();
        auto instance = iterator.value().toObject();
        auto settings = instance.value(QStringLiteral("settings")).toObject();
        settings.insert(QStringLiteral("occupiedOnly"), true);
        instance.insert(QStringLiteral("settings"), settings);
        iterator.value() = instance;
        root.insert(QStringLiteral("instances"), instances);
        ++revision_;
        root.insert(QStringLiteral("revision"), QString::number(revision_));
        snapshot_ = QJsonDocument(root).toJson(QJsonDocument::Compact);
        snapshot_.append('\n');
        publishRevision(invalidateRevision);
    }

    void releaseReplacementsAsStale()
    {
        const auto held = std::exchange(heldReplacementCalls_, {});
        for (const auto &call : held) {
            connection_.send(call.createErrorReply(
                QStringLiteral(
                    "org.hyprshelld.ComponentConfig1.Error.StaleRevision"
                ),
                QStringLiteral("The snapshot changed before it was saved")
            ));
        }
    }

public slots:
    QByteArray GetSnapshot(qulonglong &revision, QString &catalogDigest)
    {
        ++snapshotCalls_;
        if (holdSnapshots_ && calledFromDBus()) {
            setDelayedReply(true);
            heldSnapshotCalls_.append({
                .call = message(),
                .snapshot = snapshot_,
                .revision = revision_,
                .catalogDigest = catalogDigest_,
            });
            return {};
        }
        revision = revision_;
        catalogDigest = catalogDigest_;
        return snapshot_;
    }

    qulonglong ReplaceSnapshot(
        const qulonglong expectedRevision,
        const QString &expectedCatalogDigest,
        const QByteArray &candidate
    )
    {
        ++replaceCalls_;
        if (expectedRevision != revision_
            || expectedCatalogDigest != catalogDigest_) {
            if (calledFromDBus()) {
                sendErrorReply(
                    QStringLiteral(
                        "org.hyprshelld.ComponentConfig1.Error.StaleRevision"
                    ),
                    QStringLiteral("The snapshot is stale")
                );
            }
            return revision_;
        }
        if (holdReplacements_ && calledFromDBus()) {
            setDelayedReply(true);
            heldReplacementCalls_.append(message());
            return revision_;
        }
        auto root = QJsonDocument::fromJson(candidate).object();
        ++revision_;
        root.insert(QStringLiteral("revision"), QString::number(revision_));
        snapshot_ = QJsonDocument(root).toJson(QJsonDocument::Compact);
        snapshot_.append('\n');

        if (publishRevisionSignals_) {
            publishRevision();
        }
        return revision_;
    }

private:
    struct HeldSnapshotCall final {
        QDBusMessage call;
        QByteArray snapshot;
        qulonglong revision = 0;
        QString catalogDigest;
    };

    void publishRevision(const bool invalidateRevision = false)
    {
        auto signal = QDBusMessage::createSignal(
            objectPath,
            propertiesInterface,
            QStringLiteral("PropertiesChanged")
        );
        signal.setArguments(invalidateRevision
            ? QVariantList{
                interfaceName,
                QVariantMap{},
                QStringList{QStringLiteral("Revision")},
            }
            : QVariantList{
                interfaceName,
                QVariantMap{
                    {QStringLiteral("Revision"),
                     QVariant::fromValue<qulonglong>(revision_)},
                },
                QStringList{},
            });
        connection_.send(signal);
    }

    QDBusConnection connection_;
    QVector<HeldSnapshotCall> heldSnapshotCalls_;
    QVector<QDBusMessage> heldReplacementCalls_;
    QByteArray snapshot_;
    qulonglong revision_ = 0;
    QString catalogDigest_;
    int replaceCalls_ = 0;
    int snapshotCalls_ = 0;
    bool holdSnapshots_ = false;
    bool holdReplacements_ = false;
    bool publishRevisionSignals_ = true;
};

class ComponentConfigDbusTest final : public QObject {
    Q_OBJECT

private slots:
    void cleanup()
    {
        auto bus = QDBusConnection::sessionBus();
        bus.unregisterService(serviceName);
        bus.unregisterObject(objectPath);
        QDBusConnection::disconnectFromBus(
            QStringLiteral("hyprshelld-component-config-cas-a")
        );
        QDBusConnection::disconnectFromBus(
            QStringLiteral("hyprshelld-component-config-cas-b")
        );
        for (const auto &name : {
                 QStringLiteral("hyprshelld-component-config-delayed"),
                 QStringLiteral("hyprshelld-component-config-stale"),
                 QStringLiteral("hyprshelld-component-config-invalid-getall"),
                 QStringLiteral("hyprshelld-component-config-queued-snapshot"),
             }) {
            QDBusConnection::disconnectFromBus(name);
        }
    }

    void publishesAndMutatesAtomicSnapshot()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        auto bus = QDBusConnection::sessionBus();
        QVERIFY(bus.isConnected());

        ComponentConfigService service(
            ComponentStore(Tests::componentPathsFor(
                directory.path(),
                QStringLiteral(HYPRSHELLD_COMPONENT_DEFAULTS_FILE)
            )),
            bus
        );
        const ComponentConfig1Adaptor adaptor(&service);
        QVERIFY(bus.registerObject(
            objectPath,
            &service,
            QDBusConnection::ExportAdaptors
        ));
        QVERIFY(bus.registerService(serviceName));
        service.applyCatalog(catalog());

        ComponentConfigClient client(bus, nullptr);
        QTRY_VERIFY_WITH_TIMEOUT(client.available(), 3000);
        QVERIFY(client.catalogAvailable());
        QCOMPARE(client.loadState(), QStringLiteral("normal"));
        QCOMPARE(client.revision(), 0ULL);
        QVERIFY(!client.snapshot().isEmpty());

        auto snapshot = client.snapshot();
        auto instances = snapshot.value(QStringLiteral("instances")).toMap();
        const auto instanceId = instances.constBegin().key();
        auto instance = instances.value(instanceId).toMap();
        auto settings = instance.value(QStringLiteral("settings")).toMap();
        settings.insert(QStringLiteral("occupiedOnly"), true);
        instance.insert(QStringLiteral("settings"), settings);
        instances.insert(instanceId, instance);
        snapshot.insert(QStringLiteral("instances"), instances);

        QSignalSpy snapshotSpy(&client, &ComponentConfigClient::snapshotChanged);
        client.replaceSnapshot(snapshot);
        QTRY_COMPARE_WITH_TIMEOUT(client.revision(), 1ULL, 3000);
        QTRY_VERIFY_WITH_TIMEOUT(!client.busy(), 3000);
        QVERIFY(snapshotSpy.count() >= 1);
        QCOMPARE(
            client.snapshot()
                .value(QStringLiteral("instances")).toMap()
                .value(instanceId).toMap()
                .value(QStringLiteral("settings")).toMap()
                .value(QStringLiteral("occupiedOnly")).toBool(),
            true
        );

        auto invalidClientSnapshot = client.snapshot();
        invalidClientSnapshot.remove(QStringLiteral("layouts"));
        client.replaceSnapshot(invalidClientSnapshot);
        QTRY_VERIFY_WITH_TIMEOUT(!client.busy(), 3000);
        QVERIFY(client.lastErrorComponentId().isEmpty());
        QCOMPARE(
            client.lastErrorName(),
            QStringLiteral("org.hyprshelld.ComponentConfig1.Error.InvalidSnapshot")
        );
        client.replaceSnapshot(client.snapshot());
        QTRY_VERIFY_WITH_TIMEOUT(!client.busy(), 3000);
        QVERIFY(client.lastErrorName().isEmpty());
        QVERIFY(client.lastErrorMessage().isEmpty());

        const auto replaceVariantMap = [&bus](
            const qulonglong expectedRevision,
            const QString &expectedDigest,
            const QVariantMap &candidate
        ) {
            auto object = QJsonObject::fromVariantMap(candidate);
            object.insert(
                QStringLiteral("revision"),
                QString::number(expectedRevision)
            );
            auto bytes = QJsonDocument(object).toJson(QJsonDocument::Compact);
            bytes.append('\n');
            return replaceCall(
                bus, expectedRevision, expectedDigest, bytes
            );
        };

        auto staleRevision = replaceVariantMap(
            0,
            client.catalogDigest(),
            client.snapshot()
        );
        QCOMPARE(staleRevision.type(), QDBusMessage::ErrorMessage);
        QCOMPARE(
            staleRevision.errorName(),
            QStringLiteral("org.hyprshelld.ComponentConfig1.Error.StaleRevision")
        );

        auto staleCatalog = replaceVariantMap(
            client.revision(),
            QString(64, QLatin1Char('f')),
            client.snapshot()
        );
        QCOMPARE(staleCatalog.type(), QDBusMessage::ErrorMessage);
        QCOMPARE(
            staleCatalog.errorName(),
            QStringLiteral("org.hyprshelld.ComponentConfig1.Error.StaleCatalogDigest")
        );

        service.setCatalogUnavailable();
        QTRY_VERIFY_WITH_TIMEOUT(!client.catalogAvailable(), 3000);
        QVERIFY(client.available());
        auto noCatalog = replaceVariantMap(
            client.revision(),
            client.catalogDigest(),
            client.snapshot()
        );
        QCOMPARE(noCatalog.type(), QDBusMessage::ErrorMessage);
        QCOMPARE(
            noCatalog.errorName(),
            QStringLiteral("org.hyprshelld.ComponentConfig1.Error.CatalogUnavailable")
        );

        auto changedCatalog = catalog();
        changedCatalog.digest = QString(64, QLatin1Char('b'));
        changedCatalog.entries.first().packageDigest = QString(
            64, QLatin1Char('c')
        );
        service.applyCatalog(changedCatalog);
        QTRY_VERIFY_WITH_TIMEOUT(client.catalogAvailable(), 3000);
        QTRY_COMPARE_WITH_TIMEOUT(
            client.catalogDigest(), changedCatalog.digest, 3000
        );
        auto dormantCandidate = client.snapshot();
        auto dormantComponents = dormantCandidate
                                     .value(QStringLiteral("components")).toMap();
        const auto dormantId = dormantComponents.constBegin().key();
        auto dormantRecord = dormantComponents.value(dormantId).toMap();
        dormantRecord.insert(QStringLiteral("enabled"), false);
        dormantComponents.insert(dormantId, dormantRecord);
        dormantCandidate.insert(
            QStringLiteral("components"), dormantComponents
        );
        auto dormantMutation = replaceVariantMap(
            client.revision(), changedCatalog.digest, dormantCandidate
        );
        QCOMPARE(dormantMutation.type(), QDBusMessage::ErrorMessage);
        QCOMPARE(
            dormantMutation.errorName(),
            QStringLiteral("org.hyprshelld.ComponentConfig1.Error.InvalidSnapshot")
        );

        bus.unregisterService(serviceName);
        bus.unregisterObject(objectPath);
    }

    void reviewedUserPackageUpdateAdoptsInertlyAndPreservesPlacement()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const auto componentId = QStringLiteral("org.example.clock");
        const auto oldPackageDigest = QString(64, QLatin1Char('b'));
        const auto newPackageDigest = QString(64, QLatin1Char('c'));
        const auto instanceId = QStringLiteral(
            "d9a61b25-670b-44cb-a824-9e52772e79f1"
        );

        const auto baseCatalog = catalog();
        const auto parsedDefaults = Components::parseComponentConfiguration(
            QByteArrayView(Tests::readBytes(QStringLiteral(
                HYPRSHELLD_COMPONENT_DEFAULTS_FILE
            ))),
            baseCatalog
        );
        QVERIFY(parsedDefaults);
        auto seeded = *parsedDefaults.value;
        seeded.revision = 7;
        seeded.components.insert(componentId, {
            .packageDigest = oldPackageDigest,
            .enabled = true,
            .grantedCapabilities = {QStringLiteral("old.permission")},
            .settings = {{QStringLiteral("oldValue"), QStringLiteral("Old")}},
        });
        seeded.instances.insert(instanceId, {
            .componentId = componentId,
            .enabled = true,
            .settings = {},
        });
        seeded.bars[QStringLiteral("main")].end.append(instanceId);
        const auto paths = Tests::componentPathsFor(
            directory.path(),
            QStringLiteral(HYPRSHELLD_COMPONENT_DEFAULTS_FILE)
        );
        const auto seededBytes = Components::serializeComponentConfiguration(
            seeded
        );
        QVERIFY(Tests::writeBytes(paths.activeFile, seededBytes));
        QVERIFY(Tests::writeBytes(paths.recoveryFile, seededBytes));

        const auto parsedSchema = Components::parseSettingsSchema(
            QByteArrayLiteral(R"({
              "schemaVersion":1,
              "settings":[{
                "key":"label",
                "scope":"component",
                "type":"string",
                "label":"Label",
                "description":"Text shown in the pill.",
                "group":"general",
                "order":1,
                "default":"New Clock",
                "minimumLength":1,
                "maximumLength":128
              }]
            })")
        );
        QVERIFY(parsedSchema);
        auto newCatalog = baseCatalog;
        newCatalog.digest = QString(64, QLatin1Char('e'));
        newCatalog.entries.insert(
            componentId,
            {
                .packageDigest = newPackageDigest,
                .type = Components::ComponentType::BarWidget,
                .origin = Components::ComponentOrigin::User,
                .settingsSchema = *parsedSchema.value,
                .requestedCapabilities = {},
                .componentApiVersion = QStringLiteral("1.0"),
                .runtimeKind = Components::RuntimeKind::DeclarativeV1,
                .activationSupported = true,
                .declarativeRuntime = QByteArrayLiteral(
                    R"({"documentVersion":1,"text":{"setting":"label"},"type":"text-pill"})"
                ),
            }
        );

        auto bus = QDBusConnection::sessionBus();
        QVERIFY(bus.isConnected());
        ComponentConfigService service(ComponentStore(paths), bus);
        const ComponentConfig1Adaptor adaptor(&service);
        QVERIFY(bus.registerObject(
            objectPath,
            &service,
            QDBusConnection::ExportAdaptors
        ));
        QVERIFY(bus.registerService(serviceName));
        service.applyCatalog(newCatalog);

        ComponentConfigClient client(bus, nullptr);
        QTRY_VERIFY_WITH_TIMEOUT(client.available(), 3000);
        QTRY_COMPARE_WITH_TIMEOUT(
            client.catalogDigest(), newCatalog.digest, 3000
        );
        QCOMPARE(client.revision(), 7ULL);
        const auto activeOldSnapshot = client.snapshot();
        QCOMPARE(
            activeOldSnapshot.value(QStringLiteral("components")).toMap()
                .value(componentId).toMap()
                .value(QStringLiteral("enabled")).toBool(),
            true
        );
        QCOMPARE(
            activeOldSnapshot.value(QStringLiteral("components")).toMap()
                .value(componentId).toMap()
                .value(QStringLiteral("grantedCapabilities")).toList(),
            QVariantList{QStringLiteral("old.permission")}
        );
        QCOMPARE(
            activeOldSnapshot.value(QStringLiteral("components")).toMap()
                .value(componentId).toMap()
                .value(QStringLiteral("packageDigest")).toString(),
            oldPackageDigest
        );

        auto activatingAdoption = activeOldSnapshot;
        auto activatingComponents = activatingAdoption.value(
            QStringLiteral("components")
        ).toMap();
        auto activatingRecord = activatingComponents.value(
            componentId
        ).toMap();
        activatingRecord.insert(
            QStringLiteral("packageDigest"),
            newPackageDigest
        );
        activatingRecord.insert(QStringLiteral("enabled"), true);
        activatingRecord.insert(
            QStringLiteral("grantedCapabilities"),
            QVariantList{}
        );
        activatingRecord.insert(
            QStringLiteral("settings"),
            QVariantMap{{QStringLiteral("label"), QStringLiteral("New Clock")}}
        );
        activatingComponents.insert(componentId, activatingRecord);
        activatingAdoption.insert(
            QStringLiteral("components"),
            activatingComponents
        );
        auto activatingBytes = QJsonDocument::fromVariant(
            activatingAdoption
        ).toJson(QJsonDocument::Compact);
        activatingBytes.append('\n');
        const auto rejectedActivation = replaceCall(
            bus,
            client.revision(),
            newCatalog.digest,
            activatingBytes
        );
        QCOMPARE(rejectedActivation.type(), QDBusMessage::ErrorMessage);
        QCOMPARE(
            rejectedActivation.errorName(),
            QStringLiteral("org.hyprshelld.ComponentConfig1.Error.InvalidSnapshot")
        );
        QCOMPARE(client.revision(), 7ULL);

        client.adoptComponentPackage(
            componentId,
            newPackageDigest,
            {{QStringLiteral("label"), QStringLiteral("New Clock")}}
        );
        QTRY_COMPARE_WITH_TIMEOUT(client.revision(), 8ULL, 3000);
        QTRY_VERIFY_WITH_TIMEOUT(!client.busy(), 3000);
        const auto adopted = client.snapshot()
                                 .value(QStringLiteral("components")).toMap()
                                 .value(componentId).toMap();
        QCOMPARE(
            adopted.value(QStringLiteral("packageDigest")).toString(),
            newPackageDigest
        );
        QCOMPARE(adopted.value(QStringLiteral("enabled")).toBool(), false);
        QVERIFY(
            adopted.value(QStringLiteral("grantedCapabilities"))
                .toList().isEmpty()
        );
        QCOMPARE(
            adopted.value(QStringLiteral("settings")).toMap().value(
                QStringLiteral("label")
            ).toString(),
            QStringLiteral("New Clock")
        );
        QCOMPARE(
            client.snapshot().value(QStringLiteral("instances")),
            activeOldSnapshot.value(QStringLiteral("instances"))
        );
        QCOMPARE(
            client.snapshot().value(QStringLiteral("layouts")),
            activeOldSnapshot.value(QStringLiteral("layouts"))
        );
        QVERIFY(client.lastErrorName().isEmpty());

        client.setComponentEnabled(componentId, newPackageDigest, true);
        QTRY_COMPARE_WITH_TIMEOUT(client.revision(), 9ULL, 3000);
        QTRY_VERIFY_WITH_TIMEOUT(!client.busy(), 3000);
        QCOMPARE(
            client.snapshot().value(QStringLiteral("components")).toMap()
                .value(componentId).toMap()
                .value(QStringLiteral("enabled")).toBool(),
            true
        );

        bus.unregisterService(serviceName);
        bus.unregisterObject(objectPath);
    }

    void hotCatalogMigrationPublishesOneDurableRevision()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const auto paths = Tests::componentPathsFor(
            directory.path(),
            QStringLiteral(HYPRSHELLD_COMPONENT_DEFAULTS_FILE)
        );
        const auto previous = previousWorkspaceConfiguration();
        const auto previousBytes = Components::serializeComponentConfiguration(
            previous
        );
        QVERIFY(Tests::writeBytes(paths.activeFile, previousBytes));
        QVERIFY(Tests::writeBytes(paths.recoveryFile, previousBytes));

        auto bus = QDBusConnection::sessionBus();
        QVERIFY(bus.isConnected());
        ComponentConfigService service(ComponentStore(paths), bus);
        const ComponentConfig1Adaptor adaptor(&service);
        QVERIFY(bus.registerObject(
            objectPath,
            &service,
            QDBusConnection::ExportAdaptors
        ));
        QVERIFY(bus.registerService(serviceName));

        Components::ConfigurationCatalog dormantCatalog;
        dormantCatalog.digest = QString(64, QLatin1Char('d'));
        service.applyCatalog(dormantCatalog);
        QCOMPARE(service.revision(), 12ULL);

        ComponentPropertyRecorder recorder(paths.activeFile);
        QSignalSpy published(
            &recorder, &ComponentPropertyRecorder::revisionPublished
        );
        QVERIFY(bus.connect(
            serviceName,
            objectPath,
            propertiesInterface,
            QStringLiteral("PropertiesChanged"),
            &recorder,
            SLOT(propertiesChanged(QString,QVariantMap,QStringList))
        ));

        const auto liveCatalog = catalog();
        service.applyCatalog(liveCatalog);
        QTRY_COMPARE_WITH_TIMEOUT(published.count(), 1, 3000);
        QCOMPARE(recorder.revisionSignals, 1);
        QCOMPARE(recorder.persistedRevisions, QVector<quint64>{13});
        QCOMPARE(service.revision(), 13ULL);
        QCOMPARE(service.catalogDigest(), liveCatalog.digest);
        QCOMPARE(service.loadState(), QStringLiteral("normal"));

        qulonglong revision = 0;
        QString snapshotCatalogDigest;
        const auto snapshot = QJsonDocument::fromJson(
            service.GetSnapshot(revision, snapshotCatalogDigest)
        ).object();
        QCOMPARE(revision, 13ULL);
        QCOMPARE(snapshotCatalogDigest, liveCatalog.digest);
        const auto desired = snapshot.value(QStringLiteral("components"))
                                 .toObject()
                                 .value(workspaceId)
                                 .toObject();
        QCOMPARE(
            desired.value(QStringLiteral("packageDigest")).toString(),
            liveCatalog.entries.value(workspaceId).packageDigest
        );
        QCOMPARE(desired.value(QStringLiteral("enabled")).toBool(), false);
        const auto instances = snapshot.value(
            QStringLiteral("instances")
        ).toObject();
        const auto settings = instances.begin()
                                  .value()
                                  .toObject()
                                  .value(QStringLiteral("settings"))
                                  .toObject();
        QCOMPARE(settings.size(), 6);
        QVERIFY(!settings.contains(QStringLiteral("labelMode")));
        QCOMPARE(settings.value(QStringLiteral("showIdentifiers")).toBool(), true);
        QCOMPARE(settings.value(QStringLiteral("showNames")).toBool(), true);
        QCOMPARE(settings.value(QStringLiteral("showApplications")).toBool(), true);
        QCOMPARE(settings.value(QStringLiteral("maximumApplications")).toInt(), 5);
        QCOMPARE(settings.value(QStringLiteral("occupiedOnly")).toBool(), true);
        QCOMPARE(
            settings.value(QStringLiteral("scrollMode")).toString(),
            QStringLiteral("reversed")
        );
        QCOMPARE(
            Tests::readBytes(paths.activeFile),
            Tests::readBytes(paths.recoveryFile)
        );

        service.applyCatalog(liveCatalog);
        QTest::qWait(20);
        QCOMPARE(recorder.revisionSignals, 1);
        QCOMPARE(service.revision(), 13ULL);

        bus.unregisterService(serviceName);
        bus.unregisterObject(objectPath);
    }

    void hotCatalogMigrationFailureKeepsOldSnapshotDormantAndReadOnly()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const auto paths = Tests::componentPathsFor(
            directory.path(),
            QStringLiteral(HYPRSHELLD_COMPONENT_DEFAULTS_FILE)
        );
        const auto previous = previousWorkspaceConfiguration();
        const auto previousBytes = Components::serializeComponentConfiguration(
            previous
        );
        QVERIFY(Tests::writeBytes(paths.activeFile, previousBytes));
        QVERIFY(Tests::writeBytes(paths.recoveryFile, previousBytes));

        int writes = 0;
        ComponentConfigService service(
            ComponentStore(
                paths,
                [&writes](
                    const QString &path,
                    const Components::ComponentConfiguration &state,
                    QString &error
                ) {
                    ++writes;
                    if (writes == 1) {
                        error = QStringLiteral("injected recovery failure");
                        return false;
                    }
                    return Tests::writeBytes(
                        path,
                        Components::serializeComponentConfiguration(state)
                    );
                }
            ),
            QDBusConnection::sessionBus()
        );
        Components::ConfigurationCatalog dormantCatalog;
        dormantCatalog.digest = QString(64, QLatin1Char('d'));
        service.applyCatalog(dormantCatalog);
        QCOMPARE(service.revision(), 12ULL);

        const auto liveCatalog = catalog();
        service.applyCatalog(liveCatalog);
        QCOMPARE(writes, 1);
        QCOMPARE(service.revision(), 12ULL);
        QCOMPARE(service.catalogDigest(), liveCatalog.digest);
        QCOMPARE(service.loadState(), QStringLiteral("unavailable"));
        QVERIFY(service.catalogAvailable());
        QCOMPARE(Tests::readBytes(paths.activeFile), previousBytes);
        QCOMPARE(Tests::readBytes(paths.recoveryFile), previousBytes);

        qulonglong revision = 0;
        QString snapshotCatalogDigest;
        const auto snapshot = QJsonDocument::fromJson(
            service.GetSnapshot(revision, snapshotCatalogDigest)
        ).object();
        QCOMPARE(revision, 12ULL);
        QCOMPARE(snapshotCatalogDigest, liveCatalog.digest);
        const auto desired = snapshot.value(QStringLiteral("components"))
                                 .toObject()
                                 .value(workspaceId)
                                 .toObject();
        QCOMPARE(
            desired.value(QStringLiteral("packageDigest")).toString(),
            previousWorkspaceDigest
        );
        const auto instances = snapshot.value(
            QStringLiteral("instances")
        ).toObject();
        const auto settings = instances.begin()
                                  .value()
                                  .toObject()
                                  .value(QStringLiteral("settings"))
                                  .toObject();
        QCOMPARE(
            settings.value(QStringLiteral("labelMode")).toString(),
            QStringLiteral("names")
        );

        service.applyCatalog(liveCatalog);
        QCOMPARE(writes, 3);
        QCOMPARE(service.revision(), 13ULL);
        QCOMPARE(service.catalogDigest(), liveCatalog.digest);
        QCOMPARE(service.loadState(), QStringLiteral("normal"));
        QCOMPARE(
            Tests::readBytes(paths.activeFile),
            Tests::readBytes(paths.recoveryFile)
        );
        const auto migrated = QJsonDocument::fromJson(
            Tests::readBytes(paths.activeFile)
        ).object();
        const auto migratedInstances = migrated.value(
            QStringLiteral("instances")
        ).toObject();
        const auto migratedSettings = migratedInstances.begin()
                                          .value()
                                          .toObject()
                                          .value(QStringLiteral("settings"))
                                          .toObject();
        QVERIFY(!migratedSettings.contains(QStringLiteral("labelMode")));
        QCOMPARE(
            migratedSettings.value(QStringLiteral("showIdentifiers")).toBool(),
            true
        );
        QCOMPARE(
            migratedSettings.value(QStringLiteral("showNames")).toBool(),
            true
        );
    }

    void readonlyRecoveryBlocksRepeatedHotMigration_data()
    {
        QTest::addColumn<QString>("scenario");
        QTest::addColumn<QString>("loadState");
        QTest::newRow("future-format")
            << QStringLiteral("future") << QStringLiteral("unsupported");
        QTest::newRow("unreadable-symlink")
            << QStringLiteral("symlink") << QStringLiteral("unavailable");
    }

    void readonlyRecoveryBlocksRepeatedHotMigration()
    {
        QFETCH(QString, scenario);
        QFETCH(QString, loadState);
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const auto paths = Tests::componentPathsFor(
            directory.path(),
            QStringLiteral(HYPRSHELLD_COMPONENT_DEFAULTS_FILE)
        );
        const auto previous = previousWorkspaceConfiguration();
        const auto previousBytes = Components::serializeComponentConfiguration(
            previous
        );
        QVERIFY(Tests::writeBytes(paths.activeFile, previousBytes));

        const QByteArray protectedBytes(
            "{\"formatVersion\":2,\"revision\":\"99\","
            "\"components\":{},\"instances\":{},"
            "\"layouts\":{\"bars\":{},\"desktops\":{}}}\n"
        );
        QString protectedTarget;
        if (scenario == QStringLiteral("future")) {
            QVERIFY(Tests::writeBytes(paths.recoveryFile, protectedBytes));
        } else {
            protectedTarget = directory.path()
                + QStringLiteral("/protected-recovery.json");
            QVERIFY(Tests::writeBytes(protectedTarget, protectedBytes));
            QVERIFY(QDir().mkpath(QFileInfo(paths.recoveryFile).absolutePath()));
            QVERIFY(QFile::link(protectedTarget, paths.recoveryFile));
            QVERIFY(QFileInfo(paths.recoveryFile).isSymLink());
        }

        ComponentConfigService service(
            ComponentStore(paths), QDBusConnection::sessionBus()
        );
        Components::ConfigurationCatalog dormantCatalog;
        dormantCatalog.digest = QString(64, QLatin1Char('d'));
        service.applyCatalog(dormantCatalog);
        QVERIFY(service.available());
        QCOMPARE(service.revision(), 12ULL);
        QCOMPARE(service.loadState(), loadState);

        const auto liveCatalog = catalog();
        service.applyCatalog(liveCatalog);
        service.applyCatalog(liveCatalog);
        QCOMPARE(service.revision(), 12ULL);
        QCOMPARE(service.catalogDigest(), liveCatalog.digest);
        QCOMPARE(service.loadState(), loadState);
        QCOMPARE(Tests::readBytes(paths.activeFile), previousBytes);
        if (scenario == QStringLiteral("future")) {
            QCOMPARE(Tests::readBytes(paths.recoveryFile), protectedBytes);
        } else {
            const QFileInfo link(paths.recoveryFile);
            QVERIFY(link.isSymLink());
            QCOMPARE(link.symLinkTarget(), protectedTarget);
            QCOMPARE(Tests::readBytes(protectedTarget), protectedBytes);
        }

        qulonglong revision = 0;
        QString snapshotCatalogDigest;
        const auto snapshot = QJsonDocument::fromJson(
            service.GetSnapshot(revision, snapshotCatalogDigest)
        ).object();
        QCOMPARE(revision, 12ULL);
        QCOMPARE(snapshotCatalogDigest, liveCatalog.digest);
        QCOMPARE(
            snapshot.value(QStringLiteral("components"))
                .toObject()
                .value(workspaceId)
                .toObject()
                .value(QStringLiteral("packageDigest"))
                .toString(),
            previousWorkspaceDigest
        );
    }

    void componentEnableMutationIsDigestBoundAndPreservesSnapshot()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        auto bus = QDBusConnection::sessionBus();
        QVERIFY(bus.isConnected());

        ComponentConfigService service(
            ComponentStore(Tests::componentPathsFor(
                directory.path(),
                QStringLiteral(HYPRSHELLD_COMPONENT_DEFAULTS_FILE)
            )),
            bus
        );
        const ComponentConfig1Adaptor adaptor(&service);
        QVERIFY(bus.registerObject(
            objectPath,
            &service,
            QDBusConnection::ExportAdaptors
        ));
        QVERIFY(bus.registerService(serviceName));
        const auto liveCatalog = catalog();
        service.applyCatalog(liveCatalog);

        ComponentConfigClient client(bus, nullptr);
        QTRY_VERIFY_WITH_TIMEOUT(client.available(), 3000);
        const auto componentId = QString::fromLatin1(
            Components::workspaceSwitcherId
        );
        const auto packageDigest = liveCatalog.entries
                                       .value(componentId)
                                       .packageDigest;
        const auto before = client.snapshot();

        client.setComponentEnabled(
            componentId,
            QString(64, QLatin1Char('f')),
            false
        );
        QCOMPARE(client.busy(), false);
        QCOMPARE(client.lastErrorComponentId(), componentId);
        QCOMPARE(
            client.lastErrorName(),
            QStringLiteral(
                "org.hyprshelld.Client.ComponentConfig.Error.PackageDigestMismatch"
            )
        );
        QCOMPARE(client.snapshot(), before);
        QCOMPARE(client.revision(), 0ULL);

        QSignalSpy pendingSpy(
            &client,
            &ComponentConfigClient::pendingComponentIdChanged
        );
        client.setComponentEnabled(componentId, packageDigest, false);
        QCOMPARE(client.busy(), true);
        QCOMPARE(client.pendingComponentId(), componentId);
        QVERIFY(client.lastErrorName().isEmpty());
        QVERIFY(client.lastErrorComponentId().isEmpty());
        QTRY_COMPARE_WITH_TIMEOUT(client.revision(), 1ULL, 3000);
        QTRY_VERIFY_WITH_TIMEOUT(!client.busy(), 3000);
        QVERIFY(client.pendingComponentId().isEmpty());
        QCOMPARE(pendingSpy.count(), 2);

        auto expected = before;
        expected.insert(QStringLiteral("revision"), QStringLiteral("1"));
        auto expectedComponents = expected
                                      .value(QStringLiteral("components"))
                                      .toMap();
        auto expectedRecord = expectedComponents.value(componentId).toMap();
        expectedRecord.insert(QStringLiteral("enabled"), false);
        expectedComponents.insert(componentId, expectedRecord);
        expected.insert(QStringLiteral("components"), expectedComponents);
        QCOMPARE(client.snapshot(), expected);

        client.setComponentEnabled(componentId, packageDigest, false);
        QCOMPARE(client.busy(), false);
        QCOMPARE(client.revision(), 1ULL);

        service.setCatalogUnavailable();
        client.setComponentEnabled(componentId, packageDigest, true);
        QTRY_VERIFY_WITH_TIMEOUT(!client.busy(), 3000);
        QCOMPARE(client.lastErrorComponentId(), componentId);
        QCOMPARE(
            client.lastErrorName(),
            QStringLiteral(
                "org.hyprshelld.ComponentConfig1.Error.CatalogUnavailable"
            )
        );
        QCOMPARE(client.snapshot(), expected);

        bus.unregisterService(serviceName);
        bus.unregisterObject(objectPath);
    }

    void addToBarBuildsOneDigestBoundAtomicPlacement()
    {
        auto clientBus = QDBusConnection::sessionBus();
        QVERIFY(clientBus.isConnected());
        const auto connectionName = QStringLiteral(
            "hyprshelld-component-config-add-to-bar"
        );
        auto serviceBus = QDBusConnection::connectToBus(
            QDBusConnection::SessionBus, connectionName
        );
        QVERIFY(serviceBus.isConnected());
        DelayedSnapshotService service(serviceBus);
        QVERIFY(serviceBus.registerObject(
            objectPath,
            &service,
            QDBusConnection::ExportAllProperties
                | QDBusConnection::ExportAllSlots
        ));
        QVERIFY(serviceBus.registerService(serviceName));

        ComponentConfigClient client(clientBus, nullptr);
        QTRY_VERIFY_WITH_TIMEOUT(client.available(), 3000);
        const auto componentId = QStringLiteral("org.example.clock-widget");
        const auto packageDigest = QString(64, QLatin1Char('d'));
        client.addComponentToBar(
            componentId,
            packageDigest,
            {{QStringLiteral("label"), QStringLiteral("Clock")}}
        );
        QVERIFY(client.busy());
        QCOMPARE(client.pendingComponentId(), componentId);
        QTRY_COMPARE_WITH_TIMEOUT(client.revision(), 1ULL, 3000);
        QTRY_VERIFY_WITH_TIMEOUT(!client.busy(), 3000);

        const auto snapshot = client.snapshot();
        const auto desired = snapshot.value(QStringLiteral("components"))
                                 .toMap().value(componentId).toMap();
        QCOMPARE(
            desired.value(QStringLiteral("packageDigest")).toString(),
            packageDigest
        );
        QCOMPARE(desired.value(QStringLiteral("enabled")).toBool(), true);
        QCOMPARE(
            desired.value(QStringLiteral("grantedCapabilities")).toList(),
            QVariantList{}
        );
        QCOMPARE(
            desired.value(QStringLiteral("settings")).toMap().value(
                QStringLiteral("label")
            ).toString(),
            QStringLiteral("Clock")
        );

        const auto instances = snapshot.value(QStringLiteral("instances")).toMap();
        QString instanceId;
        for (auto iterator = instances.constBegin();
             iterator != instances.constEnd(); ++iterator) {
            if (iterator.value().toMap().value(
                    QStringLiteral("componentId")
                ).toString() == componentId) {
                instanceId = iterator.key();
                QCOMPARE(
                    iterator.value().toMap().value(
                        QStringLiteral("enabled")
                    ).toBool(),
                    true
                );
                QCOMPARE(
                    iterator.value().toMap().value(
                        QStringLiteral("settings")
                    ).toMap(),
                    QVariantMap{}
                );
            }
        }
        QVERIFY(Components::isLowercaseUuidV4(instanceId));
        const auto main = snapshot.value(QStringLiteral("layouts")).toMap()
                              .value(QStringLiteral("bars")).toMap()
                              .value(QStringLiteral("main")).toMap();
        const auto end = main.value(QStringLiteral("regions")).toMap()
                             .value(QStringLiteral("end")).toList();
        QCOMPARE(std::ranges::count(end, QVariant(instanceId)), 1);

        client.addComponentToBar(
            componentId,
            packageDigest,
            {{QStringLiteral("label"), QStringLiteral("Ignored")}}
        );
        QCOMPARE(service.replaceCalls(), 1);
        QCOMPARE(client.revision(), 1ULL);
        QCOMPARE(client.snapshot(), snapshot);

        client.addComponentToBar(
            componentId,
            QString(64, QLatin1Char('e')),
            {{QStringLiteral("label"), QStringLiteral("Wrong")}}
        );
        QCOMPARE(service.replaceCalls(), 1);
        QCOMPARE(client.snapshot(), snapshot);
        QCOMPARE(
            client.lastErrorName(),
            QStringLiteral(
                "org.hyprshelld.Client.ComponentConfig.Error.PackageDigestMismatch"
            )
        );

        client.setComponentEnabled(componentId, packageDigest, false);
        QTRY_COMPARE_WITH_TIMEOUT(client.revision(), 2ULL, 3000);
        const auto disabled = client.snapshot();
        QCOMPARE(
            disabled.value(QStringLiteral("components")).toMap()
                .value(componentId).toMap()
                .value(QStringLiteral("enabled")).toBool(),
            false
        );
        QCOMPARE(
            disabled.value(QStringLiteral("instances")).toMap(),
            snapshot.value(QStringLiteral("instances")).toMap()
        );
        QCOMPARE(
            disabled.value(QStringLiteral("layouts")).toMap(),
            snapshot.value(QStringLiteral("layouts")).toMap()
        );

        auto offMain = disabled;
        auto offMainLayouts = offMain.value(QStringLiteral("layouts")).toMap();
        auto offMainBars = offMainLayouts.value(QStringLiteral("bars")).toMap();
        auto offMainMain = offMainBars.value(QStringLiteral("main")).toMap();
        auto offMainMainRegions = offMainMain.value(
            QStringLiteral("regions")
        ).toMap();
        auto offMainEnd = offMainMainRegions.value(
            QStringLiteral("end")
        ).toList();
        offMainEnd.removeAll(instanceId);
        offMainMainRegions.insert(QStringLiteral("end"), offMainEnd);
        offMainMain.insert(QStringLiteral("regions"), offMainMainRegions);
        offMainBars.insert(QStringLiteral("main"), offMainMain);
        offMainBars.insert(QStringLiteral("secondary"), QVariantMap{
            {QStringLiteral("outputs"), QVariantMap{
                {QStringLiteral("mode"), QStringLiteral("all")},
            }},
            {QStringLiteral("regions"), QVariantMap{
                {QStringLiteral("start"), QVariantList{}},
                {QStringLiteral("center"), QVariantList{}},
                {QStringLiteral("end"), QVariantList{instanceId}},
            }},
        });
        offMainLayouts.insert(QStringLiteral("bars"), offMainBars);
        offMain.insert(QStringLiteral("layouts"), offMainLayouts);
        client.replaceSnapshot(offMain);
        QTRY_COMPARE_WITH_TIMEOUT(client.revision(), 3ULL, 3000);

        client.addComponentToBar(
            componentId,
            packageDigest,
            {{QStringLiteral("label"), QStringLiteral("Ignored")}}
        );
        QTRY_COMPARE_WITH_TIMEOUT(client.revision(), 4ULL, 3000);
        const auto relocated = client.snapshot();
        const auto relocatedBars = relocated.value(QStringLiteral("layouts"))
                                       .toMap().value(QStringLiteral("bars"))
                                       .toMap();
        QCOMPARE(
            relocatedBars.value(QStringLiteral("secondary")).toMap()
                .value(QStringLiteral("regions")).toMap()
                .value(QStringLiteral("end")).toList(),
            QVariantList{}
        );
        QCOMPARE(
            std::ranges::count(
                relocatedBars.value(QStringLiteral("main")).toMap()
                    .value(QStringLiteral("regions")).toMap()
                    .value(QStringLiteral("end")).toList(),
                QVariant(instanceId)
            ),
            1
        );

        client.setComponentEnabled(componentId, packageDigest, false);
        QTRY_COMPARE_WITH_TIMEOUT(client.revision(), 5ULL, 3000);

        auto disabledPlaced = client.snapshot();
        auto disabledPlacedInstances = disabledPlaced.value(
            QStringLiteral("instances")
        ).toMap();
        auto disabledPlacedInstance = disabledPlacedInstances.value(
            instanceId
        ).toMap();
        disabledPlacedInstance.insert(QStringLiteral("enabled"), false);
        disabledPlacedInstances.insert(instanceId, disabledPlacedInstance);
        disabledPlaced.insert(
            QStringLiteral("instances"),
            disabledPlacedInstances
        );
        client.replaceSnapshot(disabledPlaced);
        QTRY_COMPARE_WITH_TIMEOUT(client.revision(), 6ULL, 3000);

        client.addComponentToBar(
            componentId,
            packageDigest,
            {{QStringLiteral("label"), QStringLiteral("Ignored")}}
        );
        QTRY_COMPARE_WITH_TIMEOUT(client.revision(), 7ULL, 3000);
        const auto reenabledPlaced = client.snapshot();
        QCOMPARE(
            reenabledPlaced.value(QStringLiteral("instances")).toMap()
                .value(instanceId).toMap()
                .value(QStringLiteral("enabled")).toBool(),
            true
        );
        QCOMPARE(
            std::ranges::count(
                reenabledPlaced.value(QStringLiteral("layouts")).toMap()
                    .value(QStringLiteral("bars")).toMap()
                    .value(QStringLiteral("main")).toMap()
                    .value(QStringLiteral("regions")).toMap()
                    .value(QStringLiteral("end")).toList(),
                QVariant(instanceId)
            ),
            1
        );

        client.setComponentEnabled(componentId, packageDigest, false);
        QTRY_COMPARE_WITH_TIMEOUT(client.revision(), 8ULL, 3000);
        auto ambiguous = client.snapshot();
        auto ambiguousInstances = ambiguous.value(
            QStringLiteral("instances")
        ).toMap();
        const auto secondInstanceId = QStringLiteral(
            "a7e2d4d8-2de4-4da7-8d4c-430b8c73f55a"
        );
        ambiguousInstances.insert(secondInstanceId, QVariantMap{
            {QStringLiteral("componentId"), componentId},
            {QStringLiteral("enabled"), false},
            {QStringLiteral("settings"), QVariantMap{}},
        });
        ambiguous.insert(QStringLiteral("instances"), ambiguousInstances);
        auto ambiguousLayouts = ambiguous.value(
            QStringLiteral("layouts")
        ).toMap();
        auto ambiguousBars = ambiguousLayouts.value(
            QStringLiteral("bars")
        ).toMap();
        auto secondary = ambiguousBars.value(
            QStringLiteral("secondary")
        ).toMap();
        auto secondaryRegions = secondary.value(
            QStringLiteral("regions")
        ).toMap();
        secondaryRegions.insert(
            QStringLiteral("end"),
            QVariantList{secondInstanceId}
        );
        secondary.insert(QStringLiteral("regions"), secondaryRegions);
        ambiguousBars.insert(QStringLiteral("secondary"), secondary);
        ambiguousLayouts.insert(QStringLiteral("bars"), ambiguousBars);
        ambiguous.insert(QStringLiteral("layouts"), ambiguousLayouts);
        client.replaceSnapshot(ambiguous);
        QTRY_COMPARE_WITH_TIMEOUT(client.revision(), 9ULL, 3000);
        const auto callsBeforeAmbiguousAdd = service.replaceCalls();
        client.addComponentToBar(
            componentId,
            packageDigest,
            {{QStringLiteral("label"), QStringLiteral("Ignored")}}
        );
        QCOMPARE(service.replaceCalls(), callsBeforeAmbiguousAdd);
        QCOMPARE(
            client.lastErrorName(),
            QStringLiteral(
                "org.hyprshelld.Client.ComponentConfig.Error.InvalidComponent"
            )
        );

        serviceBus.unregisterService(serviceName);
        serviceBus.unregisterObject(objectPath);
        QDBusConnection::disconnectFromBus(connectionName);
    }

    void staleAddToBarRollsBackWithoutPartialWriteOrReplay()
    {
        auto clientBus = QDBusConnection::sessionBus();
        QVERIFY(clientBus.isConnected());
        const auto connectionName = QStringLiteral(
            "hyprshelld-component-config-stale-add-to-bar"
        );
        auto serviceBus = QDBusConnection::connectToBus(
            QDBusConnection::SessionBus, connectionName
        );
        QVERIFY(serviceBus.isConnected());
        DelayedSnapshotService service(serviceBus);
        service.setHoldReplacements(true);
        QVERIFY(serviceBus.registerObject(
            objectPath,
            &service,
            QDBusConnection::ExportAllProperties
                | QDBusConnection::ExportAllSlots
        ));
        QVERIFY(serviceBus.registerService(serviceName));

        ComponentConfigClient client(clientBus, nullptr);
        QTRY_VERIFY_WITH_TIMEOUT(client.available(), 3000);
        const auto before = client.snapshot();
        const auto componentId = QStringLiteral("org.example.clock-widget");
        client.addComponentToBar(
            componentId,
            QString(64, QLatin1Char('d')),
            {{QStringLiteral("label"), QStringLiteral("Clock")}}
        );
        QTRY_COMPARE_WITH_TIMEOUT(service.heldReplacementCalls(), 1, 3000);
        QVERIFY(client.busy());

        service.externallySetOccupiedOnly(true);
        QTRY_VERIFY_WITH_TIMEOUT(!client.available(), 3000);
        service.releaseReplacementsAsStale();
        QTRY_VERIFY_WITH_TIMEOUT(client.available(), 3000);
        QTRY_VERIFY_WITH_TIMEOUT(!client.busy(), 3000);
        QCOMPARE(service.replaceCalls(), 1);
        QCOMPARE(client.revision(), 1ULL);
        QVERIFY(!client.snapshot().value(QStringLiteral("components"))
                     .toMap().contains(componentId));
        QCOMPARE(
            client.snapshot().value(QStringLiteral("instances")).toMap().size(),
            before.value(QStringLiteral("instances")).toMap().size()
        );
        const auto workspaceInstance = client.snapshot()
            .value(QStringLiteral("instances")).toMap().constBegin().value()
            .toMap();
        QCOMPARE(
            workspaceInstance.value(QStringLiteral("settings")).toMap()
                .value(QStringLiteral("occupiedOnly")).toBool(),
            true
        );

        serviceBus.unregisterService(serviceName);
        serviceBus.unregisterObject(objectPath);
        QDBusConnection::disconnectFromBus(connectionName);
    }

    void clientBlocksRapidMutationUntilDelayedSnapshotHydrates()
    {
        auto bus = QDBusConnection::sessionBus();
        QVERIFY(bus.isConnected());
        const auto connectionName = QStringLiteral(
            "hyprshelld-component-config-delayed"
        );
        auto serviceBus = QDBusConnection::connectToBus(
            QDBusConnection::SessionBus, connectionName
        );
        QVERIFY(serviceBus.isConnected());
        DelayedSnapshotService service(serviceBus);
        QVERIFY(serviceBus.registerObject(
            objectPath,
            &service,
            QDBusConnection::ExportAllProperties
                | QDBusConnection::ExportAllSlots
        ));
        QVERIFY(serviceBus.registerService(serviceName));

        ComponentConfigClient client(bus, nullptr);
        QTRY_VERIFY_WITH_TIMEOUT(client.available(), 3000);
        const auto componentId = QString::fromLatin1(
            Components::workspaceSwitcherId
        );
        const auto packageDigest = client.snapshot()
                                       .value(QStringLiteral("components"))
                                       .toMap()
                                       .value(componentId)
                                       .toMap()
                                       .value(QStringLiteral("packageDigest"))
                                       .toString();
        QVERIFY(Components::isFullSha256Digest(packageDigest));

        service.setHoldSnapshots(true);
        client.setComponentEnabled(componentId, packageDigest, false);
        QTRY_COMPARE_WITH_TIMEOUT(service.replaceCalls(), 1, 3000);
        QTRY_VERIFY_WITH_TIMEOUT(service.heldSnapshotCalls() > 0, 3000);
        QVERIFY(client.busy());
        QVERIFY(!client.available());
        QCOMPARE(client.pendingComponentId(), componentId);

        client.setComponentEnabled(componentId, packageDigest, true);
        QCOMPARE(service.replaceCalls(), 1);
        QVERIFY(client.busy());
        QCOMPARE(client.pendingComponentId(), componentId);

        service.setHoldSnapshots(false);
        service.releaseSnapshots();
        QTRY_COMPARE_WITH_TIMEOUT(client.revision(), 1ULL, 3000);
        QTRY_VERIFY_WITH_TIMEOUT(client.available(), 3000);
        QTRY_VERIFY_WITH_TIMEOUT(!client.busy(), 3000);
        QVERIFY(client.pendingComponentId().isEmpty());
        QCOMPARE(
            client.snapshot()
                .value(QStringLiteral("components")).toMap()
                .value(componentId).toMap()
                .value(QStringLiteral("enabled")).toBool(),
            false
        );

        service.setHoldSnapshots(true);
        service.setPublishRevisionSignals(false);
        client.setComponentEnabled(componentId, packageDigest, true);
        QTRY_COMPARE_WITH_TIMEOUT(service.replaceCalls(), 2, 3000);
        QTRY_VERIFY_WITH_TIMEOUT(service.heldSnapshotCalls() > 0, 3000);
        QVERIFY(client.busy());
        service.releaseSnapshots(true);
        QTRY_VERIFY_WITH_TIMEOUT(!client.busy(), 3000);
        QVERIFY(!client.available());
        QVERIFY(client.pendingComponentId().isEmpty());

        serviceBus.unregisterService(serviceName);
        serviceBus.unregisterObject(objectPath);
        QDBusConnection::disconnectFromBus(connectionName);
    }

    void queuedInvalidationDuringHeldSnapshotForcesFreshHydration()
    {
        auto bus = QDBusConnection::sessionBus();
        QVERIFY(bus.isConnected());
        const auto connectionName = QStringLiteral(
            "hyprshelld-component-config-queued-snapshot"
        );
        auto serviceBus = QDBusConnection::connectToBus(
            QDBusConnection::SessionBus, connectionName
        );
        QVERIFY(serviceBus.isConnected());
        DelayedSnapshotService service(serviceBus);
        QVERIFY(serviceBus.registerObject(
            objectPath,
            &service,
            QDBusConnection::ExportAllProperties
                | QDBusConnection::ExportAllSlots
        ));
        QVERIFY(serviceBus.registerService(serviceName));

        ComponentConfigClient client(bus, nullptr);
        QTRY_VERIFY_WITH_TIMEOUT(client.available(), 3000);
        const auto componentId = QString::fromLatin1(
            Components::workspaceSwitcherId
        );
        const auto packageDigest = client.snapshot()
                                       .value(QStringLiteral("components"))
                                       .toMap()
                                       .value(componentId).toMap()
                                       .value(QStringLiteral("packageDigest"))
                                       .toString();

        service.setHoldSnapshots(true);
        client.setComponentEnabled(componentId, packageDigest, false);
        QTRY_COMPARE_WITH_TIMEOUT(service.replaceCalls(), 1, 3000);
        QTRY_VERIFY_WITH_TIMEOUT(service.heldSnapshotCalls() > 0, 3000);
        service.externallySetOccupiedOnly(true);
        QTRY_VERIFY_WITH_TIMEOUT(!client.available(), 3000);

        service.releaseSnapshots();
        QTRY_VERIFY_WITH_TIMEOUT(service.heldSnapshotCalls() > 0, 3000);
        QVERIFY(!client.available());
        service.setHoldSnapshots(false);
        service.releaseSnapshots();

        QTRY_VERIFY_WITH_TIMEOUT(client.available(), 3000);
        QTRY_COMPARE_WITH_TIMEOUT(client.revision(), 2ULL, 3000);
        QCOMPARE(service.replaceCalls(), 1);
        QVERIFY(!client.busy());
        QCOMPARE(
            client.snapshot()
                .value(QStringLiteral("components")).toMap()
                .value(componentId).toMap()
                .value(QStringLiteral("enabled")).toBool(),
            false
        );
        const auto instances = client.snapshot()
                                   .value(QStringLiteral("instances")).toMap();
        QCOMPARE(
            instances.constBegin().value().toMap()
                .value(QStringLiteral("settings")).toMap()
                .value(QStringLiteral("occupiedOnly")).toBool(),
            true
        );

        serviceBus.unregisterService(serviceName);
        serviceBus.unregisterObject(objectPath);
        QDBusConnection::disconnectFromBus(connectionName);
    }

    void queuedExternalRevisionRecoversAfterStaleMutationWithoutReplay()
    {
        auto bus = QDBusConnection::sessionBus();
        QVERIFY(bus.isConnected());
        const auto connectionName = QStringLiteral(
            "hyprshelld-component-config-stale"
        );
        auto serviceBus = QDBusConnection::connectToBus(
            QDBusConnection::SessionBus, connectionName
        );
        QVERIFY(serviceBus.isConnected());
        DelayedSnapshotService service(serviceBus);
        QVERIFY(serviceBus.registerObject(
            objectPath,
            &service,
            QDBusConnection::ExportAllProperties
                | QDBusConnection::ExportAllSlots
        ));
        QVERIFY(serviceBus.registerService(serviceName));

        ComponentConfigClient client(bus, nullptr);
        QTRY_VERIFY_WITH_TIMEOUT(client.available(), 3000);
        const auto componentId = QString::fromLatin1(
            Components::workspaceSwitcherId
        );
        const auto packageDigest = client.snapshot()
                                       .value(QStringLiteral("components"))
                                       .toMap()
                                       .value(componentId).toMap()
                                       .value(QStringLiteral("packageDigest"))
                                       .toString();

        service.setHoldReplacements(true);
        client.setComponentEnabled(componentId, packageDigest, false);
        QTRY_COMPARE_WITH_TIMEOUT(service.heldReplacementCalls(), 1, 3000);
        QVERIFY(client.busy());

        service.externallySetOccupiedOnly(true);
        QTRY_VERIFY_WITH_TIMEOUT(!client.available(), 3000);
        QVERIFY(client.busy());
        service.releaseReplacementsAsStale();

        QTRY_VERIFY_WITH_TIMEOUT(client.available(), 3000);
        QTRY_VERIFY_WITH_TIMEOUT(!client.busy(), 3000);
        QCOMPARE(client.revision(), 1ULL);
        QCOMPARE(service.replaceCalls(), 1);
        QVERIFY(client.pendingComponentId().isEmpty());
        QVERIFY(client.lastErrorName().isEmpty());
        QCOMPARE(
            client.snapshot()
                .value(QStringLiteral("components")).toMap()
                .value(componentId).toMap()
                .value(QStringLiteral("enabled")).toBool(),
            true
        );
        const auto instances = client.snapshot()
                                   .value(QStringLiteral("instances")).toMap();
        QCOMPARE(
            instances.constBegin().value().toMap()
                .value(QStringLiteral("settings")).toMap()
                .value(QStringLiteral("occupiedOnly")).toBool(),
            true
        );

        serviceBus.unregisterService(serviceName);
        serviceBus.unregisterObject(objectPath);
        QDBusConnection::disconnectFromBus(connectionName);
    }

    void clientRejectsAvailableGetAllWithEmptyCatalogDigest()
    {
        auto bus = QDBusConnection::sessionBus();
        QVERIFY(bus.isConnected());
        const auto connectionName = QStringLiteral(
            "hyprshelld-component-config-invalid-getall"
        );
        auto serviceBus = QDBusConnection::connectToBus(
            QDBusConnection::SessionBus, connectionName
        );
        QVERIFY(serviceBus.isConnected());
        DelayedSnapshotService service(serviceBus, {});
        QVERIFY(serviceBus.registerObject(
            objectPath,
            &service,
            QDBusConnection::ExportAllProperties
                | QDBusConnection::ExportAllSlots
        ));
        QVERIFY(serviceBus.registerService(serviceName));

        ComponentConfigClient client(bus, nullptr);
        QTRY_COMPARE_WITH_TIMEOUT(
            client.loadState(), QStringLiteral("normal"), 3000
        );
        QVERIFY(!client.available());
        QVERIFY(client.catalogDigest().isEmpty());
        QCOMPARE(service.snapshotCalls(), 0);

        client.setComponentEnabled(
            QString::fromLatin1(Components::workspaceSwitcherId),
            QString(64, QLatin1Char('f')),
            false
        );
        QCOMPARE(service.replaceCalls(), 0);
        QCOMPARE(
            client.lastErrorName(),
            QStringLiteral(
                "org.hyprshelld.Client.ComponentConfig.Error.Unavailable"
            )
        );

        serviceBus.unregisterService(serviceName);
        serviceBus.unregisterObject(objectPath);
        QDBusConnection::disconnectFromBus(connectionName);
    }

    void commitsLegacyImportBeforeRetiringCoreBridge()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const ConfigPaths corePaths{
            .activeFile = directory.path()
                + QStringLiteral("/config/hyprshelld/settings.json"),
            .recoveryFile = directory.path()
                + QStringLiteral("/state/hyprshelld/settings.last-good.json"),
        };
        QVERIFY(Tests::writeBytes(
            corePaths.activeFile,
            QByteArrayLiteral(
                "{\"formatVersion\":1,\"revision\":\"7\",\"barHeight\":40,"
                "\"workspaceSwitcher\":{\"labelMode\":\"names\","
                "\"showApplications\":true,\"maximumApplications\":5,"
                "\"paddingEnabled\":true,\"occupiedOnly\":true,"
                "\"scrollMode\":\"reversed\"}}\n"
            )
        ));
        ConfigStore coreStore(corePaths);
        const auto coreLoaded = coreStore.load();
        QVERIFY2(coreLoaded.success, qPrintable(coreLoaded.error));
        QVERIFY(coreLoaded.legacyWorkspaceSettings.has_value());
        QVERIFY(coreLoaded.legacyWorkspaceRetirementPending);
        for (const auto &path : {corePaths.activeFile, corePaths.recoveryFile}) {
            QVERIFY(QJsonDocument::fromJson(Tests::readBytes(path))
                        .object()
                        .contains(QStringLiteral("workspaceSwitcher")));
        }

        auto bus = QDBusConnection::sessionBus();
        ConfigService coreService(std::move(coreStore), coreLoaded, bus);
        const auto componentPaths = Tests::componentPathsFor(
            directory.path(),
            QStringLiteral(HYPRSHELLD_COMPONENT_DEFAULTS_FILE)
        );
        ComponentConfigService componentService(
            ComponentStore(componentPaths),
            bus,
            coreLoaded.legacyWorkspaceSettings
        );

        bool componentFilesCommittedAtSignal = false;
        connect(
            &componentService,
            &ComponentConfigService::authoritativeSnapshotEstablished,
            this,
            [&] {
                componentFilesCommittedAtSignal =
                    QFileInfo::exists(componentPaths.activeFile)
                    && QFileInfo::exists(componentPaths.recoveryFile)
                    && Tests::readBytes(componentPaths.activeFile)
                        == Tests::readBytes(componentPaths.recoveryFile);
            }
        );
        connect(
            &componentService,
            &ComponentConfigService::authoritativeSnapshotEstablished,
            &coreService,
            &ConfigService::authorizeLegacyWorkspaceRetirement
        );
        QSignalSpy authoritative(
            &componentService,
            &ComponentConfigService::authoritativeSnapshotEstablished
        );

        QVERIFY(!componentService.available());
        componentService.applyCatalog(catalog());
        QVERIFY(componentService.available());
        QCOMPARE(componentService.revision(), 0ULL);
        QCOMPARE(authoritative.count(), 1);
        QVERIFY(componentFilesCommittedAtSignal);

        for (const auto &path : {corePaths.activeFile, corePaths.recoveryFile}) {
            QVERIFY(!QJsonDocument::fromJson(Tests::readBytes(path))
                         .object()
                         .contains(QStringLiteral("workspaceSwitcher")));
        }

        qulonglong revision = 99;
        QString digest;
        const auto snapshot = QJsonDocument::fromJson(
            componentService.GetSnapshot(revision, digest)
        ).object();
        QCOMPARE(revision, 0ULL);
        const auto instances = snapshot.value(
            QStringLiteral("instances")
        ).toObject();
        const auto settings = instances.begin()
                                  .value()
                                  .toObject()
                                  .value(QStringLiteral("settings"))
                                  .toObject();
        QCOMPARE(settings.size(), 6);
        QCOMPARE(
            settings.value(QStringLiteral("showIdentifiers")).toBool(),
            true
        );
        QCOMPARE(
            settings.value(QStringLiteral("showNames")).toBool(),
            true
        );
        QCOMPARE(
            settings.value(QStringLiteral("showApplications")).toBool(),
            true
        );
        QCOMPARE(
            settings.value(QStringLiteral("maximumApplications")).toInteger(),
            5
        );
        QCOMPARE(
            settings.value(QStringLiteral("occupiedOnly")).toBool(),
            true
        );
        QCOMPARE(
            settings.value(QStringLiteral("scrollMode")).toString(),
            QStringLiteral("reversed")
        );
    }

    void failedCoreRetirementRetriesWithoutLosingBridgeBytes()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const auto coreRoot = directory.path() + QStringLiteral("/core");
        const ConfigPaths corePaths{
            .activeFile = coreRoot
                + QStringLiteral("/config/hyprshelld/settings.json"),
            .recoveryFile = coreRoot
                + QStringLiteral("/state/hyprshelld/settings.last-good.json"),
        };
        QVERIFY(Tests::writeBytes(
            corePaths.activeFile,
            QByteArrayLiteral(
                "{\"formatVersion\":1,\"revision\":\"7\",\"barHeight\":40,"
                "\"workspaceSwitcher\":{\"labelMode\":\"compact\","
                "\"showApplications\":true,\"maximumApplications\":4,"
                "\"occupiedOnly\":false,\"scrollMode\":\"normal\"}}\n"
            )
        ));
        ConfigStore coreStore(corePaths);
        const auto coreLoaded = coreStore.load();
        QVERIFY2(coreLoaded.success, qPrintable(coreLoaded.error));

        auto bus = QDBusConnection::sessionBus();
        ConfigService coreService(std::move(coreStore), coreLoaded, bus);
        const auto componentPaths = Tests::componentPathsFor(
            directory.path() + QStringLiteral("/components"),
            QStringLiteral(HYPRSHELLD_COMPONENT_DEFAULTS_FILE)
        );
        ComponentConfigService componentService(
            ComponentStore(componentPaths),
            bus,
            coreLoaded.legacyWorkspaceSettings
        );
        connect(
            &componentService,
            &ComponentConfigService::authoritativeSnapshotEstablished,
            &coreService,
            &ConfigService::authorizeLegacyWorkspaceRetirement
        );

        const auto configDirectory = QFileInfo(
            corePaths.activeFile
        ).absolutePath();
        const auto heldDirectory = configDirectory + QStringLiteral(".held");
        QVERIFY(QDir().rename(configDirectory, heldDirectory));
        QFile blocker(configDirectory);
        QVERIFY(blocker.open(QIODevice::WriteOnly));
        blocker.close();

        componentService.applyCatalog(catalog());
        QVERIFY(componentService.available());
        QVERIFY(QFileInfo::exists(componentPaths.activeFile));
        QVERIFY(QFileInfo::exists(componentPaths.recoveryFile));
        QVERIFY(!QJsonDocument::fromJson(
                     Tests::readBytes(corePaths.recoveryFile)
                 ).object().contains(QStringLiteral("workspaceSwitcher")));

        QVERIFY(blocker.remove());
        QVERIFY(QDir().rename(heldDirectory, configDirectory));
        QCOMPARE(coreService.SetBarHeight(72), 8ULL);
        for (const auto &path : {corePaths.activeFile, corePaths.recoveryFile}) {
            QVERIFY(QJsonDocument::fromJson(Tests::readBytes(path))
                        .object()
                        .contains(QStringLiteral("workspaceSwitcher")));
        }

        const auto bridgeRetired = [&] {
            for (const auto &path : {
                     corePaths.activeFile,
                     corePaths.recoveryFile,
                 }) {
                if (QJsonDocument::fromJson(Tests::readBytes(path))
                        .object()
                        .contains(QStringLiteral("workspaceSwitcher"))) {
                    return false;
                }
            }
            return true;
        };
        QTRY_VERIFY_WITH_TIMEOUT(bridgeRetired(), 3000);

        const auto restarted = ConfigStore(corePaths).load();
        QVERIFY2(restarted.success, qPrintable(restarted.error));
        QCOMPARE(restarted.state.barHeight, 72U);
        QCOMPARE(restarted.state.revision, quint64(8));
        QVERIFY(!restarted.legacyWorkspaceSettings.has_value());
        QVERIFY(!restarted.legacyWorkspaceRetirementPending);
    }

    void nonRegularComponentSnapshotCannotBlockCoreConfig()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const ConfigPaths corePaths{
            .activeFile = directory.path()
                + QStringLiteral("/core/config/settings.json"),
            .recoveryFile = directory.path()
                + QStringLiteral("/core/state/settings.last-good.json"),
        };
        ConfigStore coreStore(corePaths);
        const auto coreLoaded = coreStore.load();
        QVERIFY2(coreLoaded.success, qPrintable(coreLoaded.error));
        auto bus = QDBusConnection::sessionBus();
        ConfigService coreService(std::move(coreStore), coreLoaded, bus);

        const auto componentPaths = Tests::componentPathsFor(
            directory.path() + QStringLiteral("/components"),
            QStringLiteral(HYPRSHELLD_COMPONENT_DEFAULTS_FILE)
        );
        QVERIFY(QDir().mkpath(QFileInfo(
            componentPaths.activeFile
        ).absolutePath()));
        const auto encodedFifo = QFile::encodeName(componentPaths.activeFile);
        QCOMPARE(::mkfifo(encodedFifo.constData(), 0600), 0);
        ComponentConfigService componentService(
            ComponentStore(componentPaths),
            bus
        );

        QElapsedTimer timer;
        timer.start();
        componentService.applyCatalog(catalog());
        QVERIFY(timer.elapsed() < 1000);
        QVERIFY(!componentService.available());
        QCOMPARE(componentService.loadState(), QStringLiteral("unavailable"));

        timer.restart();
        QCOMPARE(coreService.SetBarHeight(64), 1ULL);
        QVERIFY(timer.elapsed() < 1000);
        QCOMPARE(coreService.barHeight(), 64U);
        QCOMPARE(coreService.revision(), 1ULL);
    }

    void rejectsStaleCatalogAndInvalidBytesBeforeIdempotence()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        auto bus = QDBusConnection::sessionBus();
        const auto paths = Tests::componentPathsFor(
            directory.path(),
            QStringLiteral(HYPRSHELLD_COMPONENT_DEFAULTS_FILE)
        );
        ComponentConfigService service(ComponentStore(paths), bus);
        const ComponentConfig1Adaptor adaptor(&service);
        QVERIFY(bus.registerObject(
            objectPath, &service, QDBusConnection::ExportAdaptors
        ));
        QVERIFY(bus.registerService(serviceName));
        service.applyCatalog(catalog());

        qulonglong revision = 0;
        QString digest;
        const auto initial = service.GetSnapshot(revision, digest);
        const auto changed = withInstanceSetting(
            initial, QStringLiteral("occupiedOnly"), true
        );
        const auto first = replaceCall(bus, revision, digest, changed);
        QCOMPARE(first.type(), QDBusMessage::ReplyMessage);
        QCOMPARE(service.revision(), 1ULL);

        const auto current = service.GetSnapshot(revision, digest);
        QCOMPARE(revision, 1ULL);
        const auto stale = replaceCall(bus, 0, digest, current);
        QCOMPARE(stale.type(), QDBusMessage::ErrorMessage);
        QCOMPARE(
            stale.errorName(),
            QStringLiteral("org.hyprshelld.ComponentConfig1.Error.StaleRevision")
        );

        const auto staleCatalog = replaceCall(
            bus, revision, QString(64, QLatin1Char('f')), current
        );
        QCOMPARE(staleCatalog.type(), QDBusMessage::ErrorMessage);
        QCOMPARE(
            staleCatalog.errorName(),
            QStringLiteral(
                "org.hyprshelld.ComponentConfig1.Error.StaleCatalogDigest"
            )
        );

        const auto invalid = replaceCall(
            bus, revision, digest, QByteArrayLiteral("not json\n")
        );
        QCOMPARE(invalid.type(), QDBusMessage::ErrorMessage);
        QCOMPARE(
            invalid.errorName(),
            QStringLiteral("org.hyprshelld.ComponentConfig1.Error.InvalidSnapshot")
        );
        QCOMPARE(service.revision(), 1ULL);
        qulonglong finalRevision = 0;
        QString finalDigest;
        QCOMPARE(service.GetSnapshot(finalRevision, finalDigest), current);
        QCOMPARE(finalRevision, 1ULL);
    }

    void persistenceFailurePublishesNothingAndKeepsMemory()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const auto paths = Tests::componentPathsFor(
            directory.path(),
            QStringLiteral(HYPRSHELLD_COMPONENT_DEFAULTS_FILE)
        );
        const auto configurationCatalog = catalog();
        const auto seeded = ComponentStore(paths).load(configurationCatalog);
        QVERIFY2(seeded.available, qPrintable(seeded.error));
        const auto activeBefore = Tests::readBytes(paths.activeFile);
        const auto recoveryBefore = Tests::readBytes(paths.recoveryFile);

        int writes = 0;
        ComponentStore failingStore(
            paths,
            [&writes](
                const QString &,
                const Components::ComponentConfiguration &,
                QString &error
            ) {
                ++writes;
                error = QStringLiteral("injected persistence failure");
                return false;
            }
        );
        auto bus = QDBusConnection::sessionBus();
        ComponentConfigService service(std::move(failingStore), bus);
        const ComponentConfig1Adaptor adaptor(&service);
        QVERIFY(bus.registerObject(
            objectPath, &service, QDBusConnection::ExportAdaptors
        ));
        QVERIFY(bus.registerService(serviceName));
        service.applyCatalog(configurationCatalog);

        ComponentPropertyRecorder recorder(paths.activeFile);
        QVERIFY(bus.connect(
            serviceName,
            objectPath,
            propertiesInterface,
            QStringLiteral("PropertiesChanged"),
            &recorder,
            SLOT(propertiesChanged(QString,QVariantMap,QStringList))
        ));
        qulonglong revision = 0;
        QString digest;
        const auto snapshotBefore = service.GetSnapshot(revision, digest);
        const auto candidate = withInstanceSetting(
            snapshotBefore, QStringLiteral("occupiedOnly"), true
        );
        const auto failed = replaceCall(bus, revision, digest, candidate);
        QCOMPARE(failed.type(), QDBusMessage::ErrorMessage);
        QCOMPARE(
            failed.errorName(),
            QStringLiteral(
                "org.hyprshelld.ComponentConfig1.Error.PersistenceFailed"
            )
        );
        QTest::qWait(50);
        QCOMPARE(writes, 1);
        QCOMPARE(recorder.revisionSignals, 0);
        QCOMPARE(service.revision(), 0ULL);
        QCOMPARE(Tests::readBytes(paths.activeFile), activeBefore);
        QCOMPARE(Tests::readBytes(paths.recoveryFile), recoveryBefore);
        qulonglong finalRevision = 99;
        QString finalDigest;
        QCOMPARE(
            service.GetSnapshot(finalRevision, finalDigest), snapshotBefore
        );
        QCOMPARE(finalRevision, 0ULL);
    }

    void concurrentCallersCommitExactlyOneSnapshot()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        auto serviceBus = QDBusConnection::sessionBus();
        const auto paths = Tests::componentPathsFor(
            directory.path(),
            QStringLiteral(HYPRSHELLD_COMPONENT_DEFAULTS_FILE)
        );
        ComponentConfigService service(ComponentStore(paths), serviceBus);
        const ComponentConfig1Adaptor adaptor(&service);
        QVERIFY(serviceBus.registerObject(
            objectPath, &service, QDBusConnection::ExportAdaptors
        ));
        QVERIFY(serviceBus.registerService(serviceName));
        service.applyCatalog(catalog());

        ComponentPropertyRecorder recorder(paths.activeFile);
        QSignalSpy published(
            &recorder, &ComponentPropertyRecorder::revisionPublished
        );
        QVERIFY(serviceBus.connect(
            serviceName,
            objectPath,
            propertiesInterface,
            QStringLiteral("PropertiesChanged"),
            &recorder,
            SLOT(propertiesChanged(QString,QVariantMap,QStringList))
        ));

        qulonglong revision = 0;
        QString digest;
        const auto current = service.GetSnapshot(revision, digest);
        const auto candidateA = withInstanceSetting(
            current, QStringLiteral("occupiedOnly"), true
        );
        const auto candidateB = withInstanceSetting(
            current, QStringLiteral("showIdentifiers"), false
        );

        auto clientA = QDBusConnection::connectToBus(
            QDBusConnection::SessionBus,
            QStringLiteral("hyprshelld-component-config-cas-a")
        );
        auto clientB = QDBusConnection::connectToBus(
            QDBusConnection::SessionBus,
            QStringLiteral("hyprshelld-component-config-cas-b")
        );
        QVERIFY(clientA.isConnected());
        QVERIFY(clientB.isConnected());

        auto messageA = QDBusMessage::createMethodCall(
            serviceName, objectPath, interfaceName, QStringLiteral("ReplaceSnapshot")
        );
        messageA.setArguments({
            QVariant::fromValue<qulonglong>(revision), digest, candidateA,
        });
        auto messageB = messageA;
        messageB.setArguments({
            QVariant::fromValue<qulonglong>(revision), digest, candidateB,
        });
        QDBusPendingCallWatcher watcherA(clientA.asyncCall(messageA, 3000));
        QDBusPendingCallWatcher watcherB(clientB.asyncCall(messageB, 3000));
        QSignalSpy finishedA(&watcherA, &QDBusPendingCallWatcher::finished);
        QSignalSpy finishedB(&watcherB, &QDBusPendingCallWatcher::finished);
        if (!watcherA.isFinished()) {
            QVERIFY(finishedA.wait(5000));
        }
        if (!watcherB.isFinished()) {
            QVERIFY(finishedB.wait(5000));
        }
        const QList<QDBusMessage> replies{watcherA.reply(), watcherB.reply()};
        int commits = 0;
        int stale = 0;
        for (const auto &reply : replies) {
            if (reply.type() == QDBusMessage::ReplyMessage) {
                ++commits;
                QCOMPARE(reply.arguments().size(), 1);
                QCOMPARE(reply.arguments().first().toULongLong(), 1ULL);
            } else if (reply.type() == QDBusMessage::ErrorMessage
                       && reply.errorName() == QStringLiteral(
                           "org.hyprshelld.ComponentConfig1.Error.StaleRevision"
                       )) {
                ++stale;
            }
        }
        QCOMPARE(commits, 1);
        QCOMPARE(stale, 1);
        QCOMPARE(service.revision(), 1ULL);
        QTRY_COMPARE_WITH_TIMEOUT(recorder.revisionSignals, 1, 3000);
        QCOMPARE(published.count(), 1);
        QCOMPARE(recorder.persistedRevisions, QVector<quint64>{1});
        const auto active = QJsonDocument::fromJson(
            Tests::readBytes(paths.activeFile)
        ).object();
        QCOMPARE(active.value(QStringLiteral("revision")).toString(), QStringLiteral("1"));
    }

    void validActiveWithFutureRecoveryRemainsReadableButNotWritable()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const auto paths = Tests::componentPathsFor(
            directory.path(),
            QStringLiteral(HYPRSHELLD_COMPONENT_DEFAULTS_FILE)
        );
        const auto configurationCatalog = catalog();
        QVERIFY(ComponentStore(paths).load(configurationCatalog).available);
        const QByteArray future(
            "{\"formatVersion\":2,\"revision\":\"8\","
            "\"components\":{},\"instances\":{},"
            "\"layouts\":{\"bars\":{},\"desktops\":{}}}\n"
        );
        QVERIFY(Tests::writeBytes(paths.recoveryFile, future));

        auto bus = QDBusConnection::sessionBus();
        ComponentConfigService service(ComponentStore(paths), bus);
        const ComponentConfig1Adaptor adaptor(&service);
        QVERIFY(bus.registerObject(
            objectPath, &service, QDBusConnection::ExportAdaptors
        ));
        QVERIFY(bus.registerService(serviceName));
        service.applyCatalog(configurationCatalog);
        QVERIFY(service.available());
        QVERIFY(service.catalogAvailable());
        QCOMPARE(service.loadState(), QStringLiteral("unsupported"));

        const auto snapshotReply = callMethod(
            bus, QStringLiteral("GetSnapshot")
        );
        QCOMPARE(snapshotReply.type(), QDBusMessage::ReplyMessage);
        QCOMPARE(snapshotReply.arguments().size(), 3);
        const auto snapshot = snapshotReply.arguments().at(0).toByteArray();
        QVERIFY(!snapshot.isEmpty());
        const auto digest = snapshotReply.arguments().at(2).toString();
        const auto rejected = replaceCall(bus, 0, digest, snapshot);
        QCOMPARE(rejected.type(), QDBusMessage::ErrorMessage);
        QCOMPARE(
            rejected.errorName(),
            QStringLiteral("org.hyprshelld.ComponentConfig1.Error.Unavailable")
        );
        QCOMPARE(Tests::readBytes(paths.recoveryFile), future);
    }
};

QTEST_GUILESS_MAIN(ComponentConfigDbusTest)
#include "component_config_dbus_test.moc"
