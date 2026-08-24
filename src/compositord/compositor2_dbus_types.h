#pragma once

#include <QByteArray>
#include <QDBusArgument>
#include <QList>
#include <QMetaType>
#include <QString>
#include <QtTypes>

namespace HyprShelld::Compositor2 {

struct RestartResultRow final {
    quint64 sequence = 0;
    QString authorityId;
    quint64 revision = 0;
    QString operationId;
    QString resultDigest;
    QByteArray status;

    friend bool operator==(const RestartResultRow &, const RestartResultRow &)
        = default;
};

using RestartResultRows = QList<RestartResultRow>;

inline QDBusArgument &operator<<(
    QDBusArgument &argument,
    const RestartResultRow &row
)
{
    argument.beginStructure();
    argument << row.sequence
             << row.authorityId
             << row.revision
             << row.operationId
             << row.resultDigest
             << row.status;
    argument.endStructure();
    return argument;
}

inline const QDBusArgument &operator>>(
    const QDBusArgument &argument,
    RestartResultRow &row
)
{
    argument.beginStructure();
    argument >> row.sequence
             >> row.authorityId
             >> row.revision
             >> row.operationId
             >> row.resultDigest
             >> row.status;
    argument.endStructure();
    return argument;
}

struct RepairResultRow final {
    quint64 sequence = 0;
    QString repairResultId;
    QString repairId;
    QString requestId;
    QString outcome;
    QString authorityKind;
    QString authorityId;
    quint64 revision = 0;
    QString resultDigest;
    QByteArray result;

    friend bool operator==(const RepairResultRow &, const RepairResultRow &)
        = default;
};

using RepairResultRows = QList<RepairResultRow>;

inline QDBusArgument &operator<<(
    QDBusArgument &argument,
    const RepairResultRow &row
)
{
    argument.beginStructure();
    argument << row.sequence
             << row.repairResultId
             << row.repairId
             << row.requestId
             << row.outcome
             << row.authorityKind
             << row.authorityId
             << row.revision
             << row.resultDigest
             << row.result;
    argument.endStructure();
    return argument;
}

inline const QDBusArgument &operator>>(
    const QDBusArgument &argument,
    RepairResultRow &row
)
{
    argument.beginStructure();
    argument >> row.sequence
             >> row.repairResultId
             >> row.repairId
             >> row.requestId
             >> row.outcome
             >> row.authorityKind
             >> row.authorityId
             >> row.revision
             >> row.resultDigest
             >> row.result;
    argument.endStructure();
    return argument;
}

} // namespace HyprShelld::Compositor2

Q_DECLARE_METATYPE(HyprShelld::Compositor2::RestartResultRow)
Q_DECLARE_METATYPE(HyprShelld::Compositor2::RestartResultRows)
Q_DECLARE_METATYPE(HyprShelld::Compositor2::RepairResultRow)
Q_DECLARE_METATYPE(HyprShelld::Compositor2::RepairResultRows)
