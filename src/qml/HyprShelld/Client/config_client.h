#pragma once

#include <QDBusConnection>
#include <QDBusPendingCall>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantMap>
#include <QtQml/qqmlregistration.h>

class OrgHyprshelldConfig1Interface;
class QDBusServiceWatcher;

namespace HyprShelld {

class ConfigClient final : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
    Q_PROPERTY(bool available READ available NOTIFY availableChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
    Q_PROPERTY(uint barHeight READ barHeight NOTIFY barHeightChanged)
    Q_PROPERTY(
        bool shellBorderEnabled READ shellBorderEnabled
        NOTIFY sharedBorderChanged
    )
    Q_PROPERTY(
        uint shellBorderWidth READ shellBorderWidth NOTIFY sharedBorderChanged
    )
    Q_PROPERTY(
        uint shellBorderRadius READ shellBorderRadius NOTIFY sharedBorderChanged
    )
    Q_PROPERTY(
        bool syncHyprlandWindowBorders READ syncHyprlandWindowBorders
        NOTIFY sharedBorderChanged
    )
    Q_PROPERTY(
        uint shellInnerSpacing READ shellInnerSpacing
        NOTIFY sharedSpacingChanged
    )
    Q_PROPERTY(
        uint shellOuterSpacing READ shellOuterSpacing
        NOTIFY sharedSpacingChanged
    )
    Q_PROPERTY(
        bool syncHyprlandWindowSpacing READ syncHyprlandWindowSpacing
        NOTIFY sharedSpacingChanged
    )
    Q_PROPERTY(qulonglong revision READ revision NOTIFY revisionChanged)
    Q_PROPERTY(QString revisionToken READ revisionToken NOTIFY revisionChanged)
    Q_PROPERTY(QString recoveryState READ recoveryState NOTIFY recoveryStateChanged)
    Q_PROPERTY(uint minimumBarHeight READ minimumBarHeight CONSTANT)
    Q_PROPERTY(uint maximumBarHeight READ maximumBarHeight CONSTANT)
    Q_PROPERTY(uint defaultBarHeight READ defaultBarHeight CONSTANT)
    Q_PROPERTY(
        bool defaultShellBorderEnabled READ defaultShellBorderEnabled CONSTANT
    )
    Q_PROPERTY(
        uint minimumShellBorderWidth READ minimumShellBorderWidth CONSTANT
    )
    Q_PROPERTY(
        uint maximumShellBorderWidth READ maximumShellBorderWidth CONSTANT
    )
    Q_PROPERTY(
        uint defaultShellBorderWidth READ defaultShellBorderWidth CONSTANT
    )
    Q_PROPERTY(
        uint minimumShellBorderRadius READ minimumShellBorderRadius CONSTANT
    )
    Q_PROPERTY(
        uint maximumShellBorderRadius READ maximumShellBorderRadius CONSTANT
    )
    Q_PROPERTY(
        uint defaultShellBorderRadius READ defaultShellBorderRadius CONSTANT
    )
    Q_PROPERTY(
        bool defaultSyncHyprlandWindowBorders
        READ defaultSyncHyprlandWindowBorders CONSTANT
    )
    Q_PROPERTY(uint minimumShellSpacing READ minimumShellSpacing CONSTANT)
    Q_PROPERTY(uint maximumShellSpacing READ maximumShellSpacing CONSTANT)
    Q_PROPERTY(
        uint defaultShellInnerSpacing READ defaultShellInnerSpacing CONSTANT
    )
    Q_PROPERTY(
        uint defaultShellOuterSpacing READ defaultShellOuterSpacing CONSTANT
    )
    Q_PROPERTY(
        bool defaultSyncHyprlandWindowSpacing
        READ defaultSyncHyprlandWindowSpacing CONSTANT
    )
    Q_PROPERTY(QString lastErrorName READ lastErrorName NOTIFY lastErrorChanged)
    Q_PROPERTY(QString lastErrorMessage READ lastErrorMessage NOTIFY lastErrorChanged)

public:
    explicit ConfigClient(QObject *parent = nullptr);
    ConfigClient(QDBusConnection connection, QObject *parent);

    [[nodiscard]] bool available() const;
    [[nodiscard]] bool busy() const;
    [[nodiscard]] uint barHeight() const;
    [[nodiscard]] bool shellBorderEnabled() const;
    [[nodiscard]] uint shellBorderWidth() const;
    [[nodiscard]] uint shellBorderRadius() const;
    [[nodiscard]] bool syncHyprlandWindowBorders() const;
    [[nodiscard]] uint shellInnerSpacing() const;
    [[nodiscard]] uint shellOuterSpacing() const;
    [[nodiscard]] bool syncHyprlandWindowSpacing() const;
    [[nodiscard]] qulonglong revision() const;
    [[nodiscard]] QString revisionToken() const;
    [[nodiscard]] QString recoveryState() const;
    [[nodiscard]] uint minimumBarHeight() const;
    [[nodiscard]] uint maximumBarHeight() const;
    [[nodiscard]] uint defaultBarHeight() const;
    [[nodiscard]] bool defaultShellBorderEnabled() const;
    [[nodiscard]] uint minimumShellBorderWidth() const;
    [[nodiscard]] uint maximumShellBorderWidth() const;
    [[nodiscard]] uint defaultShellBorderWidth() const;
    [[nodiscard]] uint minimumShellBorderRadius() const;
    [[nodiscard]] uint maximumShellBorderRadius() const;
    [[nodiscard]] uint defaultShellBorderRadius() const;
    [[nodiscard]] bool defaultSyncHyprlandWindowBorders() const;
    [[nodiscard]] uint minimumShellSpacing() const;
    [[nodiscard]] uint maximumShellSpacing() const;
    [[nodiscard]] uint defaultShellInnerSpacing() const;
    [[nodiscard]] uint defaultShellOuterSpacing() const;
    [[nodiscard]] bool defaultSyncHyprlandWindowSpacing() const;
    [[nodiscard]] QString lastErrorName() const;
    [[nodiscard]] QString lastErrorMessage() const;

    Q_INVOKABLE void setBarHeight(uint height);
    Q_INVOKABLE void resetBarHeight();
    Q_INVOKABLE void setSharedBorder(
        bool enabled,
        uint width,
        uint radius,
        bool syncHyprlandWindowBorders
    );
    Q_INVOKABLE void resetSharedBorder();
    Q_INVOKABLE void setSharedSpacing(
        uint inner,
        uint outer,
        bool syncHyprlandWindowSpacing
    );
    Q_INVOKABLE void resetSharedSpacing();
    Q_INVOKABLE void clearError();

signals:
    void availableChanged();
    void busyChanged();
    void barHeightChanged();
    void sharedBorderChanged();
    void sharedSpacingChanged();
    void revisionChanged();
    void recoveryStateChanged();
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
    void refresh();
    [[nodiscard]] bool applyProperties(
        const QVariantMap &properties,
        bool requireComplete
    );
    void beginMutation(const QDBusPendingCall &call);
    void setAvailable(bool available);
    void setError(const QString &name, const QString &message);

    QDBusConnection connection_;
    OrgHyprshelldConfig1Interface *interface_ = nullptr;
    QDBusServiceWatcher *serviceWatcher_ = nullptr;
    quint64 ownerGeneration_ = 0;
    int pendingOperations_ = 0;
    bool available_ = false;
    bool projectionEstablished_ = false;
    uint barHeight_ = 0;
    bool shellBorderEnabled_ = true;
    uint shellBorderWidth_ = 0;
    uint shellBorderRadius_ = 0;
    bool syncHyprlandWindowBorders_ = true;
    uint shellInnerSpacing_ = 0;
    uint shellOuterSpacing_ = 0;
    bool syncHyprlandWindowSpacing_ = true;
    qulonglong revision_ = 0;
    QString recoveryState_;
    QString lastErrorName_;
    QString lastErrorMessage_;
};

} // namespace HyprShelld
