#include "builtin_component_defaults.h"

#include <QStringList>

#include <QJsonValue>

namespace HyprShelld::Components {
namespace {

bool hasExactKeys(const QJsonObject &object, const QStringList &keys)
{
    auto actual = object.keys();
    auto expected = keys;
    actual.sort();
    expected.sort();
    return actual == expected;
}

bool isExactInteger(const QJsonValue &value, const qint64 minimum, const qint64 maximum)
{
    if (!value.isDouble()) {
        return false;
    }
    const auto integer = value.toInteger(minimum - 1);
    return integer >= minimum
        && integer <= maximum
        && value.toDouble() == static_cast<double>(integer);
}

} // namespace

QJsonObject workspaceSwitcherDefaultSettings()
{
    return {
        {QStringLiteral("labelMode"), QStringLiteral("numbers")},
        {QStringLiteral("showApplications"), false},
        {QStringLiteral("maximumApplications"), 3},
        {QStringLiteral("occupiedOnly"), false},
        {QStringLiteral("scrollMode"), QStringLiteral("disabled")},
    };
}

bool isValidWorkspaceSwitcherSettings(const QJsonObject &settings)
{
    if (!hasExactKeys(
            settings,
            {
                QStringLiteral("labelMode"),
                QStringLiteral("showApplications"),
                QStringLiteral("maximumApplications"),
                QStringLiteral("occupiedOnly"),
                QStringLiteral("scrollMode"),
            }
        )) {
        return false;
    }

    const auto labelMode = settings.value(QStringLiteral("labelMode"));
    const auto showApplications = settings.value(
        QStringLiteral("showApplications")
    );
    const auto maximumApplications = settings.value(
        QStringLiteral("maximumApplications")
    );
    const auto occupiedOnly = settings.value(QStringLiteral("occupiedOnly"));
    const auto scrollMode = settings.value(QStringLiteral("scrollMode"));

    if (!labelMode.isString()
        || (labelMode.toString() != QStringLiteral("numbers")
            && labelMode.toString() != QStringLiteral("compact")
            && labelMode.toString() != QStringLiteral("names"))) {
        return false;
    }
    if (!showApplications.isBool()
        || !isExactInteger(maximumApplications, 1, 5)
        || !occupiedOnly.isBool()
        || !scrollMode.isString()) {
        return false;
    }

    const auto scroll = scrollMode.toString();
    return scroll == QStringLiteral("disabled")
        || scroll == QStringLiteral("normal")
        || scroll == QStringLiteral("reversed");
}

} // namespace HyprShelld::Components
