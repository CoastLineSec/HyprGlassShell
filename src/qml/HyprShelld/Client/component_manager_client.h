#pragma once

#include <QDBusConnection>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>
#include <QtQml/qqmlregistration.h>

#include <memory>

class QDBusServiceWatcher;
class QTimer;

namespace HyprShelld {

class ComponentManagerClient final : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
    Q_PROPERTY(bool available READ available NOTIFY availableChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
    Q_PROPERTY(QString catalogDigest READ catalogDigest NOTIFY catalogDigestChanged)
    Q_PROPERTY(QVariantList components READ components NOTIFY componentsChanged)
    Q_PROPERTY(QString lastError READ lastError NOTIFY lastErrorChanged)

public:
    explicit ComponentManagerClient(QObject *parent = nullptr);
    ComponentManagerClient(QDBusConnection connection, QObject *parent);

    [[nodiscard]] bool available() const;
    [[nodiscard]] bool busy() const;
    [[nodiscard]] QString catalogDigest() const;
    [[nodiscard]] QVariantList components() const;
    [[nodiscard]] QString lastError() const;

signals:
    void availableChanged();
    void busyChanged();
    void catalogDigestChanged();
    void componentsChanged();
    void lastErrorChanged();

private slots:
    void propertiesChanged(
        const QString &interfaceName,
        const QVariantMap &changed,
        const QStringList &invalidated
    );
    void serviceOwnerChanged(
        const QString &name,
        const QString &oldOwner,
        const QString &newOwner
    );

private:
    struct HydrationState;

    void refresh();
    void fetchNext(quint64 generation, std::shared_ptr<HydrationState> state);
    void accept(quint64 generation, const HydrationState &state);
    void fail(quint64 generation, const QString &error);
    void scheduleRetry();
    void setAvailable(bool available);
    void setBusy(bool busy);
    void setLastError(const QString &error);

    QDBusConnection connection_;
    QDBusServiceWatcher *serviceWatcher_ = nullptr;
    QTimer *retryTimer_ = nullptr;
    QVariantList components_;
    QString catalogDigest_;
    QString lastError_;
    quint64 generation_ = 0;
    int retryDelayMs_ = 250;
    bool available_ = false;
    bool busy_ = false;
};

} // namespace HyprShelld
