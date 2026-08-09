#pragma once

#include <QDBusConnection>
#include <QObject>
#include <QString>
#include <QTimer>
#include <QtQml/qqmlregistration.h>

namespace HyprShelld {

class ShellRuntimeStatus : public QObject {
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(bool active READ active WRITE setActive NOTIFY activeChanged)
    Q_PROPERTY(bool available READ available NOTIFY availableChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
    Q_PROPERTY(QString targetState READ targetState NOTIFY statesChanged)
    Q_PROPERTY(QString coordinatorState READ coordinatorState NOTIFY statesChanged)
    Q_PROPERTY(QString configurationState READ configurationState NOTIFY statesChanged)
    Q_PROPERTY(QString componentManagerState READ componentManagerState NOTIFY statesChanged)
    Q_PROPERTY(QString compositorState READ compositorState NOTIFY statesChanged)
    Q_PROPERTY(QString surfaceState READ surfaceState NOTIFY statesChanged)
    Q_PROPERTY(QString lastErrorName READ lastErrorName NOTIFY lastErrorChanged)
    Q_PROPERTY(QString lastErrorMessage READ lastErrorMessage NOTIFY lastErrorChanged)

public:
    explicit ShellRuntimeStatus(QObject *parent = nullptr);
    ShellRuntimeStatus(QDBusConnection connection, QObject *parent);

    [[nodiscard]] bool active() const;
    void setActive(bool active);

    [[nodiscard]] bool available() const;
    [[nodiscard]] bool busy() const;
    [[nodiscard]] QString targetState() const;
    [[nodiscard]] QString coordinatorState() const;
    [[nodiscard]] QString configurationState() const;
    [[nodiscard]] QString componentManagerState() const;
    [[nodiscard]] QString compositorState() const;
    [[nodiscard]] QString surfaceState() const;
    [[nodiscard]] QString lastErrorName() const;
    [[nodiscard]] QString lastErrorMessage() const;

    Q_INVOKABLE void refresh();

signals:
    void activeChanged();
    void availableChanged();
    void busyChanged();
    void statesChanged();
    void lastErrorChanged();

private:
    void setAvailable(bool available);
    void setBusy(bool busy);
    void setError(const QString &name, const QString &message);
    void clearError();

    QDBusConnection connection_;
    QTimer pollTimer_;
    quint64 generation_ = 0;
    bool active_ = false;
    bool available_ = false;
    bool busy_ = false;
    QString targetState_ = QStringLiteral("unknown");
    QString coordinatorState_ = QStringLiteral("unknown");
    QString configurationState_ = QStringLiteral("unknown");
    QString componentManagerState_ = QStringLiteral("unknown");
    QString compositorState_ = QStringLiteral("unknown");
    QString surfaceState_ = QStringLiteral("unknown");
    QString lastErrorName_;
    QString lastErrorMessage_;
};

} // namespace HyprShelld
