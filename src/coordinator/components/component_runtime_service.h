#pragma once

#include <QByteArray>
#include <QDBusConnection>
#include <QDBusContext>
#include <QObject>
#include <QString>

namespace HyprShelld {

class ComponentPlanController;

class ComponentRuntimeService final : public QObject, protected QDBusContext {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.hyprshelld.ComponentRuntime1")
    Q_PROPERTY(qulonglong SurfacePlanRevision READ surfacePlanRevision)
    Q_PROPERTY(QString SurfacePlanDigest READ surfacePlanDigest)
    Q_PROPERTY(QString SurfacePlanState READ surfacePlanState)

public:
    ComponentRuntimeService(
        ComponentPlanController *controller,
        QDBusConnection connection,
        QObject *parent = nullptr
    );

    [[nodiscard]] qulonglong surfacePlanRevision() const;
    [[nodiscard]] QString surfacePlanDigest() const;
    [[nodiscard]] QString surfacePlanState() const;

public slots:
    QByteArray GetSurfacePlan(
        qulonglong expectedSurfacePlanRevision,
        QString &surfacePlanDigest
    ) const;

private:
    void publishChange() const;
    void reportError(const QString &name, const QString &message) const;

    ComponentPlanController *controller_;
    QDBusConnection connection_;
};

} // namespace HyprShelld
