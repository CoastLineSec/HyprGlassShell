#pragma once

#include "component_inspector_launcher.h"

#include "component/package_inspection_report.h"

#include <QByteArray>
#include <QObject>
#include <QString>

#include <functional>
#include <memory>
#include <optional>

namespace HyprShelld {

enum class ComponentInspectionState {
    Pending,
    Complete,
    Failed,
};

struct ComponentInspectionOperationResult {
    bool success = false;
    QString errorName;
    QString errorMessage;
};

struct ComponentInspectionBeginResult final
    : ComponentInspectionOperationResult {
    QString token;
    QString archiveDigest;
    quint64 archiveSize = 0;
    qint64 expiresAtMs = 0;
};

struct ComponentInspectionLookupResult final
    : ComponentInspectionOperationResult {
    ComponentInspectionState state = ComponentInspectionState::Pending;
    QString token;
    QString archiveDigest;
    quint64 archiveSize = 0;
    qint64 expiresAtMs = 0;
    QByteArray reportBytes;
    QString spoolPath;
    QString materializedPath;
};

// A successfully taken inspection is a scoped lease over the private staged
// files. Dropping the lease deletes only that token's three fixed files and
// session directory. Installation therefore cannot accidentally leave a
// reusable inspected bundle behind.
class ComponentInspectionArtifact final {
public:
    ComponentInspectionArtifact() = default;
    ~ComponentInspectionArtifact();

    ComponentInspectionArtifact(const ComponentInspectionArtifact &) = delete;
    ComponentInspectionArtifact &operator=(
        const ComponentInspectionArtifact &
    ) = delete;
    ComponentInspectionArtifact(ComponentInspectionArtifact &&other) noexcept;
    ComponentInspectionArtifact &operator=(
        ComponentInspectionArtifact &&other
    ) noexcept;

    QString token;
    QString archiveDigest;
    quint64 archiveSize = 0;
    QByteArray reportBytes;
    Components::PackageInspectionReport report;
    QString spoolPath;
    QString materializedPath;

private:
    friend class ComponentInspectionSessions;
    QString sessionDirectory_;
    int rootDescriptor_ = -1;
    int lockDescriptor_ = -1;
    void cleanup();
};

struct ComponentInspectionTakeResult final
    : ComponentInspectionOperationResult {
    std::optional<ComponentInspectionArtifact> artifact;
};

class ComponentInspectionSessions final : public QObject {
    Q_OBJECT

public:
    using Clock = std::function<qint64()>;

    explicit ComponentInspectionSessions(
        QString spoolRoot,
        std::unique_ptr<ComponentInspectorLauncher> launcher,
        qint64 timeToLiveMs = 5 * 60 * 1000,
        Clock clock = {},
        QObject *parent = nullptr
    );
    ~ComponentInspectionSessions() override;

    [[nodiscard]] ComponentInspectionBeginResult begin(
        const QString &sender,
        int packageFileDescriptor
    );
    [[nodiscard]] ComponentInspectionLookupResult lookup(
        const QString &sender,
        const QString &token
    );
    [[nodiscard]] ComponentInspectionTakeResult takeForInstall(
        const QString &sender,
        const QString &token,
        const QString &expectedArchiveDigest
    );
    [[nodiscard]] ComponentInspectionOperationResult cancel(
        const QString &sender,
        const QString &token
    );

    void cancelAllForSender(const QString &sender);
    void expireNow();

signals:
    // The receiver must direct this completion only to `sender`; paths are
    // empty on failure. This is an internal daemon signal, not a D-Bus signal.
    void inspectionFinished(
        const QString &sender,
        const QString &token,
        const QByteArray &reportBytes,
        const QString &spoolPath,
        const QString &materializedPath,
        const QString &errorName,
        const QString &errorMessage
    );

private:
    struct Private;
    std::unique_ptr<Private> d_;

    void launcherFinished(
        const QString &token,
        ComponentInspectorLaunchResult result
    );
};

} // namespace HyprShelld
