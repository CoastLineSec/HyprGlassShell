#pragma once

#include "coordinator_policy.h"

#include <QDBusConnection>
#include <QDBusContext>
#include <QObject>
#include <QString>
#include <QStringList>

namespace HyprShelld {

class SystemdUserManager;

class CoordinatorService final : public QObject, protected QDBusContext {
    Q_OBJECT
    Q_PROPERTY(bool Healthy READ healthy)
    Q_PROPERTY(QStringList FailedUnits READ failedUnits)
    Q_PROPERTY(QString FailureSummary READ failureSummary)

public:
    CoordinatorService(
        SystemdUserManager *manager,
        QDBusConnection connection,
        QObject *parent = nullptr
    );

    [[nodiscard]] bool healthy() const;
    [[nodiscard]] QStringList failedUnits() const;
    [[nodiscard]] QString failureSummary() const;
    [[nodiscard]] bool start(QString &error);

public slots:
    void RestartComponent(const QString &unitName);

signals:
    void initialized();
    void fatalError(const QString &error);

private:
    void applySnapshot(const QHash<QString, QString> &activeStates);
    void reportError(const QString &name, const QString &message) const;
    void publishChange() const;

    SystemdUserManager *manager_;
    CoordinatorPolicy policy_;
    QDBusConnection connection_;
    bool initialized_ = false;
};

} // namespace HyprShelld
