#pragma once

#include "component/package_inspection_report.h"
#include "system_catalog.h"

#include <QString>
#include <QStringList>
#include <QVector>

#include <optional>

namespace HyprShelld::Components {

inline constexpr quint64 maximumUserPackageCatalogBytes =
    512ULL * 1024ULL * 1024ULL;

struct UserPackageStoreResult final {
    bool success = false;
    QString errorCode;
    QString errorMessage;
    QString componentId;
    QString packageDigest;
    QVector<CatalogEntry> catalogEntries;
};

struct UserPackageLoadResult final {
    QVector<CatalogEntry> entries;
    QStringList warnings;
    quint64 totalExpandedBytes = 0;
    qsizetype receiptCount = 0;
    QString error;

    [[nodiscard]] bool ok() const { return error.isEmpty(); }
};

// Owns only per-user immutable package bytes and manager-generated receipts.
// Desired enablement, grants, instances, and placements remain configd data.
class UserPackageStore final {
public:
    UserPackageStore(QString dataRoot, QString stateRoot);
    ~UserPackageStore();

    UserPackageStore(const UserPackageStore &) = delete;
    UserPackageStore &operator=(const UserPackageStore &) = delete;
    UserPackageStore(UserPackageStore &&) = delete;
    UserPackageStore &operator=(UserPackageStore &&) = delete;

    [[nodiscard]] UserPackageLoadResult load() const;

    // Call only after the manager owns its unique D-Bus name. This removes
    // leftovers from interrupted package transactions and prunes version
    // trees that cannot be reached from a receipt.
    [[nodiscard]] bool recover(
        QString &error,
        QStringList *warnings = nullptr
    );

    [[nodiscard]] UserPackageStoreResult install(
        const PackageInspectionReport &report,
        const QString &materializedBundlePath
    );

    [[nodiscard]] UserPackageStoreResult remove(
        const QString &componentId,
        const QString &expectedPackageDigest
    );

    [[nodiscard]] const QString &dataRoot() const { return dataRoot_; }
    [[nodiscard]] const QString &stateRoot() const { return stateRoot_; }

private:
    [[nodiscard]] QString receiptPath(const QString &componentId) const;

    QString dataRoot_;
    QString stateRoot_;
    int leaseFd_ = -1;
    QString leaseError_;
};

} // namespace HyprShelld::Components
