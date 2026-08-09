#pragma once

#include <QDBusConnection>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>
#include <QUrl>
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
    Q_PROPERTY(bool inspectionBusy READ inspectionBusy NOTIFY inspectionBusyChanged)
    Q_PROPERTY(bool packageOperationBusy READ packageOperationBusy NOTIFY packageOperationBusyChanged)
    Q_PROPERTY(QVariantMap inspectionReview READ inspectionReview NOTIFY inspectionReviewChanged)
    Q_PROPERTY(QString inspectionToken READ inspectionToken NOTIFY inspectionTokenChanged)
    Q_PROPERTY(QString packageError READ packageError NOTIFY packageErrorChanged)

public:
    explicit ComponentManagerClient(QObject *parent = nullptr);
    ComponentManagerClient(QDBusConnection connection, QObject *parent);

    [[nodiscard]] bool available() const;
    [[nodiscard]] bool busy() const;
    [[nodiscard]] QString catalogDigest() const;
    [[nodiscard]] QVariantList components() const;
    [[nodiscard]] QString lastError() const;
    [[nodiscard]] bool inspectionBusy() const;
    [[nodiscard]] bool packageOperationBusy() const;
    [[nodiscard]] QVariantMap inspectionReview() const;
    [[nodiscard]] QString inspectionToken() const;
    [[nodiscard]] QString packageError() const;

    Q_INVOKABLE void inspectPackage(const QUrl &packageUrl);
    Q_INVOKABLE void cancelInspection();
    Q_INVOKABLE void installInspectedPackage();
    Q_INVOKABLE void removeComponent(
        const QString &componentId,
        const QString &packageDigest,
        const QString &expectedCatalogDigest
    );
    Q_INVOKABLE void clearPackageError();

signals:
    void availableChanged();
    void busyChanged();
    void catalogDigestChanged();
    void componentsChanged();
    void lastErrorChanged();
    void inspectionBusyChanged();
    void packageOperationBusyChanged();
    void inspectionReviewChanged();
    void inspectionTokenChanged();
    void packageErrorChanged();
    void packageInstalled(const QString &componentId);
    void packageRemoved(const QString &componentId);

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
    void packageInspectionFinished(
        const QString &inspectionToken,
        const QByteArray &review,
        const QString &errorCode,
        const QString &errorMessage
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
    void setInspectionBusy(bool busy);
    void setPackageOperationBusy(bool busy);
    void setInspectionReview(QVariantMap review);
    void setInspectionToken(QString token);
    void setPackageError(QString error);

    QDBusConnection connection_;
    QDBusServiceWatcher *serviceWatcher_ = nullptr;
    QTimer *retryTimer_ = nullptr;
    QVariantList components_;
    QString catalogDigest_;
    QString lastError_;
    quint64 generation_ = 0;
    quint64 packageGeneration_ = 0;
    int retryDelayMs_ = 250;
    bool available_ = false;
    bool busy_ = false;
    bool inspectionBusy_ = false;
    bool packageOperationBusy_ = false;
    QVariantMap inspectionReview_;
    QString inspectionToken_;
    QString inspectionArchiveDigest_;
    QString inspectionCatalogDigest_;
    QString packageError_;
};

} // namespace HyprShelld
