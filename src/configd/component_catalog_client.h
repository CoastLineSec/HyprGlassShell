#pragma once

#include "component/component_configuration.h"

#include <QDBusConnection>
#include <QObject>
#include <QStringList>
#include <QVariantMap>

class QDBusPendingCallWatcher;
class QDBusServiceWatcher;
class QTimer;

namespace HyprShelld {

class ComponentCatalogClient final : public QObject {
    Q_OBJECT

public:
    explicit ComponentCatalogClient(
        QDBusConnection connection,
        QObject *parent = nullptr
    );

    [[nodiscard]] bool available() const;
    [[nodiscard]] const Components::ConfigurationCatalog &catalog() const;
    void start();

signals:
    void catalogChanged();
    void catalogUnavailable();

private slots:
    void serviceOwnerChanged(
        const QString &name,
        const QString &oldOwner,
        const QString &newOwner
    );
    void propertiesChanged(
        const QString &interfaceName,
        const QVariantMap &changed,
        const QStringList &invalidated
    );

private:
    void refresh();
    void fetchNext(
        quint64 generation,
        QStringList componentIds,
        QString catalogDigest,
        qsizetype index,
        Components::ConfigurationCatalog catalog
    );
    void fetchDeclarativeRuntime(
        quint64 generation,
        QStringList componentIds,
        QString catalogDigest,
        qsizetype index,
        Components::ConfigurationCatalog catalog,
        Components::ConfigurationCatalogEntry entry
    );
    void fail(quint64 generation);

    QDBusConnection connection_;
    QDBusServiceWatcher *serviceWatcher_ = nullptr;
    QTimer *retryTimer_ = nullptr;
    Components::ConfigurationCatalog catalog_;
    quint64 generation_ = 0;
    int retryDelayMs_ = 250;
    bool available_ = false;
};

} // namespace HyprShelld
