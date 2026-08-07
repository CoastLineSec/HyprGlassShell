#pragma once

#include <QHash>
#include <QSet>
#include <QString>
#include <QStringList>

namespace HyprShelld {

class CoordinatorPolicy final {
public:
    [[nodiscard]] static const QStringList &allowedUnits();
    [[nodiscard]] static bool isKnown(const QString &unitName);

    [[nodiscard]] bool applySnapshot(const QHash<QString, QString> &activeStates);
    [[nodiscard]] bool healthy() const;
    [[nodiscard]] bool isFailed(const QString &unitName) const;
    [[nodiscard]] QStringList failedUnits() const;
    [[nodiscard]] QString failureSummary() const;

private:
    QSet<QString> failedUnits_;
};

} // namespace HyprShelld
