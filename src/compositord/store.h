#pragma once

#include <QByteArray>
#include <QByteArrayView>
#include <QString>

#include <functional>

namespace HyprShelld::Compositor {

enum class StoreFile {
    Desired,
    LastGood,
    Activation,
    Pending,
};

enum class StoreFaultPoint {
    BeforePublishRename,
    AfterPublishRenameBeforeDirectorySync,
    AfterRemoveBeforeDirectorySync,
};

struct StorePaths final {
    QString stateRoot;
    QString configRoot;
    QString managedConfigRoot;
    // Null in production. Focused tests use this narrow seam to distinguish
    // pre-namespace failures from post-rename/unlink durability uncertainty.
    std::function<bool(StoreFaultPoint, StoreFile)> faultHook;

    [[nodiscard]] static StorePaths standard();

    [[nodiscard]] QString desiredPath() const;
    [[nodiscard]] QString lastGoodPath() const;
    [[nodiscard]] QString activationPath() const;
    [[nodiscard]] QString pendingPath() const;
    [[nodiscard]] QString lockPath() const;
    [[nodiscard]] QString userCustomPath() const;
    [[nodiscard]] QString stableEntrypointPath() const;
    [[nodiscard]] QString generationsPath() const;
};

enum class StoreReadStatus {
    Missing,
    Present,
    Unsafe,
    Unreadable,
    Oversized,
};

struct StoreReadResult final {
    StoreReadStatus status = StoreReadStatus::Missing;
    QByteArray bytes;
    QString errorCode;
    QString errorMessage;

    [[nodiscard]] bool present() const
    {
        return status == StoreReadStatus::Present;
    }
};

struct StoreOperationResult final {
    bool success = false;
    // The namespace mutation happened, but syncing its parent directory
    // failed. Callers must treat their in-memory view as unavailable and must
    // not retry a non-idempotent transition in the current process.
    bool committedButNotDurable = false;
    QString errorCode;
    QString errorMessage;
};

// Descriptor-anchored durable storage. initialize() is deliberately separate
// from construction so compositord can acquire its public D-Bus name first.
class PersistentStore final {
public:
    explicit PersistentStore(StorePaths paths);
    ~PersistentStore();

    PersistentStore(const PersistentStore &) = delete;
    PersistentStore &operator=(const PersistentStore &) = delete;
    PersistentStore(PersistentStore &&) = delete;
    PersistentStore &operator=(PersistentStore &&) = delete;

    [[nodiscard]] StoreOperationResult initialize();
    void shutdown() noexcept;
    [[nodiscard]] StoreReadResult read(StoreFile file) const;
    [[nodiscard]] StoreOperationResult write(
        StoreFile file,
        QByteArrayView bytes
    );
    [[nodiscard]] StoreOperationResult remove(StoreFile file);

    [[nodiscard]] bool initialized() const { return leaseFd_ >= 0; }
    [[nodiscard]] const StorePaths &paths() const { return paths_; }
    [[nodiscard]] int stateDirectoryFd() const { return stateDirectoryFd_; }
    [[nodiscard]] int configDirectoryFd() const { return configDirectoryFd_; }
    [[nodiscard]] int managedDirectoryFd() const { return managedDirectoryFd_; }
    [[nodiscard]] bool managedDirectoryStillNamed() const;
    [[nodiscard]] bool rootsStillNamed() const;

private:
    [[nodiscard]] const char *fileName(StoreFile file) const;
    [[nodiscard]] bool leaseStillNamed() const;

    StorePaths paths_;
    QByteArray stateRootName_;
    int stateParentDirectoryFd_ = -1;
    int stateDirectoryFd_ = -1;
    int configDirectoryFd_ = -1;
    int managedDirectoryFd_ = -1;
    int leaseFd_ = -1;
};

} // namespace HyprShelld::Compositor
