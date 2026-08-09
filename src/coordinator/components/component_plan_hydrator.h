#pragma once

#include "component_plan_source.h"

#include <QDBusConnection>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantMap>
#include <QVector>
#include <QtTypes>

class QDBusMessage;
class QDBusServiceWatcher;
class QTimer;

namespace HyprShelld {

class ComponentPlanController;

// Joins ComponentManager1 and ComponentConfig1 without ever exposing a
// partially hydrated plan. Every asynchronous leg is generation-tagged.
class ComponentPlanHydrator final : public QObject {
    Q_OBJECT

public:
    ComponentPlanHydrator(
        ComponentPlanController *controller,
        QDBusConnection connection,
        QObject *parent = nullptr
    );

    void start();

private slots:
    void sourceOwnerChanged(
        const QString &name,
        const QString &oldOwner,
        const QString &newOwner
    );
    void managerPropertiesChanged(
        const QString &interfaceName,
        const QVariantMap &changed,
        const QStringList &invalidated
    );
    void configPropertiesChanged(
        const QString &interfaceName,
        const QVariantMap &changed,
        const QStringList &invalidated
    );

private:
    void refresh();
    void fetchCatalogRecord(
        quint64 generation,
        QStringList componentIds,
        QString catalogDigest,
        qsizetype index,
        QVector<Components::RuntimeCatalogComponentRecord> records
    );
    void fetchConfigurationProperties(
        quint64 generation,
        Components::HydratedRuntimeCatalog catalog
    );
    void fetchConfigurationSnapshot(
        quint64 generation,
        Components::HydratedRuntimeCatalog catalog,
        quint64 expectedRevision,
        QString expectedCatalogDigest
    );
    void fail(quint64 generation, const QString &error);
    void scheduleRetry();

    ComponentPlanController *controller_;
    QDBusConnection connection_;
    QDBusServiceWatcher *managerWatcher_ = nullptr;
    QDBusServiceWatcher *configWatcher_ = nullptr;
    QTimer *retryTimer_ = nullptr;
    quint64 generation_ = 0;
    int retryDelayMs_ = 250;
    bool started_ = false;
};

} // namespace HyprShelld
