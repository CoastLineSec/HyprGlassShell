#include "coordinator_policy.h"

namespace HyprShelld {

const QStringList &CoordinatorPolicy::allowedUnits()
{
    static const QStringList units {
        QStringLiteral("hyprshelld-configd.service"),
        QStringLiteral("hyprshelld-componentd.service"),
        QStringLiteral("hyprshelld-surfaced.service"),
    };
    return units;
}

bool CoordinatorPolicy::isKnown(const QString &unitName)
{
    return allowedUnits().contains(unitName);
}

bool CoordinatorPolicy::applySnapshot(
    const QHash<QString, QString> &activeStates
)
{
    const auto previous = failedUnits_;

    for (const auto &unitName : allowedUnits()) {
        const auto state = activeStates.constFind(unitName);
        if (state == activeStates.cend()) {
            continue;
        }

        if (*state == QStringLiteral("failed")) {
            failedUnits_.insert(unitName);
        } else if (*state == QStringLiteral("active")) {
            failedUnits_.remove(unitName);
        }
    }

    return previous != failedUnits_;
}

bool CoordinatorPolicy::healthy() const
{
    return failedUnits_.isEmpty();
}

bool CoordinatorPolicy::isFailed(const QString &unitName) const
{
    return failedUnits_.contains(unitName);
}

QStringList CoordinatorPolicy::failedUnits() const
{
    auto units = failedUnits_.values();
    units.sort();
    return units;
}

QString CoordinatorPolicy::failureSummary() const
{
    if (failedUnits_.isEmpty()) {
        return {};
    }

    if (failedUnits_.size() == 1) {
        return QStringLiteral("A HyprShelld component needs attention.");
    }

    return QStringLiteral("%1 HyprShelld components need attention.")
        .arg(failedUnits_.size());
}

} // namespace HyprShelld
