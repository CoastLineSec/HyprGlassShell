#pragma once

#include <QByteArray>
#include <QDBusConnection>
#include <QDBusContext>
#include <QObject>
#include <QString>
#include <QStringList>

namespace HyprShelld {

class ComponentPlanController;

class ComponentRuntimeService final : public QObject, protected QDBusContext {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.hyprshelld.ComponentRuntime1")
    Q_PROPERTY(qulonglong SurfacePlanRevision READ surfacePlanRevision)
    Q_PROPERTY(QString SurfacePlanDigest READ surfacePlanDigest)
    Q_PROPERTY(QString SurfacePlanState READ surfacePlanState)
    Q_PROPERTY(qulonglong RuntimeHealthRevision READ runtimeHealthRevision)
    Q_PROPERTY(bool ThirdPartySafeMode READ thirdPartySafeMode)

public:
    ComponentRuntimeService(
        ComponentPlanController *controller,
        QDBusConnection connection,
        QObject *parent = nullptr
    );

    [[nodiscard]] qulonglong surfacePlanRevision() const;
    [[nodiscard]] QString surfacePlanDigest() const;
    [[nodiscard]] QString surfacePlanState() const;
    [[nodiscard]] qulonglong runtimeHealthRevision() const;
    [[nodiscard]] bool thirdPartySafeMode() const;

public slots:
    QByteArray GetSurfacePlan(
        qulonglong expectedSurfacePlanRevision,
        QString &surfacePlanDigest
    ) const;
    QStringList ListComponentRuntimeStates(
        qulonglong expectedRuntimeHealthRevision,
        QStringList &packageDigests,
        QStringList &states,
        QStringList &reasons,
        QList<uint> &failureCounts
    ) const;
    qulonglong RetryComponent(
        const QString &componentId,
        const QString &expectedPackageDigest,
        qulonglong expectedRuntimeHealthRevision
    );
    bool AuthorizeSurfacePlan(
        qulonglong surfacePlanRevision
    );
    bool CancelSurfacePlanAuthorization(
        qulonglong surfacePlanRevision
    );
    void ActivationStable(
        const QString &instanceId,
        const QString &componentId,
        const QString &packageDigest,
        qulonglong surfacePlanRevision
    );
    void ActivationFailed(
        const QString &instanceId,
        const QString &componentId,
        const QString &packageDigest,
        qulonglong surfacePlanRevision,
        const QString &reason
    );

private:
    void publishChange() const;
    void publishHealthChange() const;
    void reportError(const QString &name, const QString &message) const;

    ComponentPlanController *controller_;
    QDBusConnection connection_;
};

} // namespace HyprShelld
