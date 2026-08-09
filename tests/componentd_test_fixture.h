#pragma once

#include "component/component_contract.h"

#include <QDir>
#include <QFile>
#include <QString>

namespace HyprShelld::Tests {

inline QString componentDirectory(const QString &catalogRoot)
{
    return catalogRoot + QLatin1Char('/')
        + QString::fromLatin1(Components::workspaceSwitcherId);
}

inline bool copyFile(
    const QString &source,
    const QString &destination,
    QString &error
)
{
    QFile input(source);
    if (!input.open(QIODevice::ReadOnly)) {
        error = QStringLiteral("Cannot read %1: %2")
                    .arg(source, input.errorString());
        return false;
    }

    QFile output(destination);
    if (!output.open(QIODevice::WriteOnly | QIODevice::NewOnly)) {
        error = QStringLiteral("Cannot create %1: %2")
                    .arg(destination, output.errorString());
        return false;
    }
    if (output.write(input.readAll()) < 0 || !output.flush()) {
        error = QStringLiteral("Cannot write %1: %2")
                    .arg(destination, output.errorString());
        return false;
    }
    return true;
}

inline bool createValidCatalog(
    const QString &catalogRoot,
    const QString &sourceComponentDirectory,
    QString &error
)
{
    const auto destination = componentDirectory(catalogRoot);
    if (!QDir().mkpath(destination)) {
        error = QStringLiteral("Cannot create fixture directory: %1")
                    .arg(destination);
        return false;
    }

    return copyFile(
               sourceComponentDirectory + QStringLiteral("/manifest.json"),
               destination + QStringLiteral("/manifest.json"),
               error
           )
        && copyFile(
               sourceComponentDirectory
                   + QStringLiteral("/settings.schema.json"),
               destination + QStringLiteral("/settings.schema.json"),
               error
           );
}

inline bool replaceFile(
    const QString &path,
    const QByteArray &contents,
    QString &error
)
{
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        error = QStringLiteral("Cannot replace %1: %2")
                    .arg(path, file.errorString());
        return false;
    }
    if (file.write(contents) != contents.size() || !file.flush()) {
        error = QStringLiteral("Cannot write %1: %2")
                    .arg(path, file.errorString());
        return false;
    }
    return true;
}

inline QByteArray readFile(const QString &path, QString &error)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        error = QStringLiteral("Cannot read %1: %2")
                    .arg(path, file.errorString());
        return {};
    }
    return file.readAll();
}

} // namespace HyprShelld::Tests
