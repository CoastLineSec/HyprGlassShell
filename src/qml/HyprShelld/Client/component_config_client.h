#pragma once

#include <QDBusConnection>
#include <QObject>
#include <QString>
#include <QVariantMap>
#include <QtQml/qqmlregistration.h>

class QDBusServiceWatcher;

namespace HyprShelld {

class ComponentConfigClient final : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
    Q_PROPERTY(bool available READ available NOTIFY availableChanged)
    Q_PROPERTY(bool catalogAvailable READ catalogAvailable NOTIFY catalogAvailableChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
    Q_PROPERTY(qulonglong revision READ revision NOTIFY snapshotChanged)
    Q_PROPERTY(QString catalogDigest READ catalogDigest NOTIFY catalogDigestChanged)
    Q_PROPERTY(QString loadState READ loadState NOTIFY loadStateChanged)
    Q_PROPERTY(QVariantMap snapshot READ snapshot NOTIFY snapshotChanged)
    Q_PROPERTY(QString pendingComponentId READ pendingComponentId NOTIFY pendingComponentIdChanged)
    Q_PROPERTY(QString lastErrorComponentId READ lastErrorComponentId NOTIFY lastErrorChanged)
    Q_PROPERTY(QString lastErrorName READ lastErrorName NOTIFY lastErrorChanged)
    Q_PROPERTY(QString lastErrorMessage READ lastErrorMessage NOTIFY lastErrorChanged)

public:
    explicit ComponentConfigClient(QObject *parent = nullptr);
    ComponentConfigClient(QDBusConnection connection, QObject *parent);

    [[nodiscard]] bool available() const;
    [[nodiscard]] bool catalogAvailable() const;
    [[nodiscard]] bool busy() const;
    [[nodiscard]] qulonglong revision() const;
    [[nodiscard]] QString catalogDigest() const;
    [[nodiscard]] QString loadState() const;
    [[nodiscard]] QVariantMap snapshot() const;
    [[nodiscard]] QString pendingComponentId() const;
    [[nodiscard]] QString lastErrorComponentId() const;
    [[nodiscard]] QString lastErrorName() const;
    [[nodiscard]] QString lastErrorMessage() const;

    Q_INVOKABLE void replaceSnapshot(const QVariantMap &snapshot);
    Q_INVOKABLE void setComponentEnabled(
        const QString &componentId,
        const QString &expectedPackageDigest,
        bool enabled
    );
    Q_INVOKABLE void setComponentSettings(
        const QString &componentId,
        const QString &expectedPackageDigest,
        const QVariantMap &settings
    );
    Q_INVOKABLE void adoptComponentPackage(
        const QString &componentId,
        const QString &expectedPackageDigest,
        const QVariantMap &defaultComponentSettings
    );
    Q_INVOKABLE void addComponentToBar(
        const QString &componentId,
        const QString &expectedPackageDigest,
        const QVariantMap &defaultComponentSettings
    );
    Q_INVOKABLE void preparePackageChange(
        const QString &componentId,
        const QString &expectedPackageDigest
    );
    Q_INVOKABLE void clearError();

signals:
    void availableChanged();
    void catalogAvailableChanged();
    void busyChanged();
    void snapshotChanged();
    void catalogDigestChanged();
    void loadStateChanged();
    void pendingComponentIdChanged();
    void lastErrorChanged();
    void operationFailed(const QString &name, const QString &message);

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
    void beginReplaceSnapshot(
        const QVariantMap &snapshot,
        const QString &componentId
    );
    void refresh();
    void fetchSnapshot(quint64 generation);
    [[nodiscard]] bool applyProperties(
        const QVariantMap &properties,
        bool requireAll = false
    );
    void setAvailable(bool available);
    void setBusy(bool busy);
    void setPendingComponentId(const QString &componentId);
    void finishMutation();
    void finishHydration(bool accepted);
    void setError(
        const QString &componentId,
        const QString &name,
        const QString &message
    );

    QDBusConnection connection_;
    QDBusServiceWatcher *serviceWatcher_ = nullptr;
    QVariantMap snapshot_;
    quint64 ownerGeneration_ = 0;
    quint64 refreshGeneration_ = 0;
    qulonglong advertisedRevision_ = 0;
    qulonglong revision_ = 0;
    QString advertisedCatalogDigest_;
    QString catalogDigest_;
    QString loadState_ = QStringLiteral("unavailable");
    QString pendingComponentId_;
    QString lastErrorComponentId_;
    QString lastErrorName_;
    QString lastErrorMessage_;
    bool advertisedAvailable_ = false;
    bool available_ = false;
    bool catalogAvailable_ = false;
    bool busy_ = false;
    bool refreshQueued_ = false;
};

} // namespace HyprShelld
