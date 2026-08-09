#pragma once

#include <QByteArray>
#include <QDBusConnection>
#include <QDBusObjectPath>
#include <QMap>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariant>

#include <functional>
#include <memory>

namespace HyprShelld {

struct ComponentInspectorLaunchRequest final {
    QString token;
    QString archiveDigest;
    QString spoolPath;
    QString reportPath;
    QString materializedPath;
};

struct ComponentInspectorLaunchResult final {
    bool success = false;
    QString errorName;
    QString errorMessage;
};

struct ComponentInspectorSystemdContract final {
    QStringList arguments;
    QByteArray startTransientArgumentSignature;
    QMap<QString, QByteArray> propertySignatures;
    QMap<QString, QVariant> propertyValues;
    QMap<QString, QString> openFilePaths;
    QMap<QString, quint64> openFileFlags;
    QMap<QString, QString> readOnlyBindPaths;
};

class ComponentInspectorLauncher : public QObject {
    Q_OBJECT

public:
    using Completion = std::function<void(ComponentInspectorLaunchResult)>;

    using QObject::QObject;
    ~ComponentInspectorLauncher() override = default;

    [[nodiscard]] virtual bool start(
        const ComponentInspectorLaunchRequest &request,
        Completion completion,
        QString &error
    ) = 0;
    virtual void cancel(const QString &token) = 0;
};

class SystemdComponentInspectorLauncher final
    : public ComponentInspectorLauncher {
    Q_OBJECT

public:
    explicit SystemdComponentInspectorLauncher(
        QDBusConnection connection,
        QString inspectorExecutable,
        QString sliceName = QStringLiteral(
            "session-hyprshelld-components.slice"
        ),
        QObject *parent = nullptr
    );
    ~SystemdComponentInspectorLauncher() override;

    [[nodiscard]] bool start(
        const ComponentInspectorLaunchRequest &request,
        Completion completion,
        QString &error
    ) override;
    void cancel(const QString &token) override;

    [[nodiscard]] static QString unitNameForToken(const QString &token);
    // A no-I/O contract probe backed by the same property builder used for
    // StartTransientUnit. Tests use it to pin systemd's non-obvious wire
    // signatures and OpenFile flag values without starting a live user unit.
    [[nodiscard]] static ComponentInspectorSystemdContract
    sandboxContractForTesting(
        const ComponentInspectorLaunchRequest &request,
        const QString &inspectorExecutable,
        const QString &sliceName = QStringLiteral(
            "session-hyprshelld-components.slice"
        )
    );

private slots:
    void jobRemoved(
        quint32 jobId,
        const QDBusObjectPath &jobPath,
        const QString &unitName,
        const QString &result
    );

private:
    struct Private;
    std::unique_ptr<Private> d_;

    void complete(
        const QString &token,
        ComponentInspectorLaunchResult result
    );
    void submitStart(const QString &token);
    void ensureSubscribed();
};

} // namespace HyprShelld
