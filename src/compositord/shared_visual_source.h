#pragma once

#include <QDBusConnection>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantMap>

class QDBusServiceWatcher;

namespace HyprShelld::Compositor {

struct SharedVisualProjection final {
    bool borderEnabled = true;
    quint32 borderWidth = 1;
    quint32 borderRadius = 0;
    bool syncWindowBorders = true;
    quint32 innerSpacing = 8;
    quint32 outerSpacing = 12;
    bool syncWindowSpacing = true;
    quint64 revision = 0;

    friend bool operator==(
        const SharedVisualProjection &,
        const SharedVisualProjection &
    ) = default;
};

class SharedVisualSource : public QObject {
    Q_OBJECT

public:
    explicit SharedVisualSource(QObject *parent = nullptr);

    [[nodiscard]] bool available() const;
    [[nodiscard]] const SharedVisualProjection &projection() const;
    [[nodiscard]] QString error() const;

    virtual void start() = 0;
    virtual void requestRefresh() = 0;

signals:
    void changed();

protected:
    void publishProjection(const SharedVisualProjection &projection);
    void publishUnavailable(const QString &error);

private:
    bool available_ = false;
    SharedVisualProjection projection_;
    QString error_ = QStringLiteral("Shared visual settings are unavailable");
};

class DbusSharedVisualSource final : public SharedVisualSource {
    Q_OBJECT

public:
    explicit DbusSharedVisualSource(
        QDBusConnection connection,
        QObject *parent = nullptr
    );

    void start() override;
    void requestRefresh() override;

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

    QDBusConnection connection_;
    QDBusServiceWatcher *serviceWatcher_ = nullptr;
    quint64 generation_ = 0;
    bool projectionEstablished_ = false;
    bool started_ = false;
};

} // namespace HyprShelld::Compositor
