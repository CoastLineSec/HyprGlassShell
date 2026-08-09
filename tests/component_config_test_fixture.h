#pragma once

#include "component/component_configuration.h"
#include "component/component_contract.h"
#include "component/settings_schema.h"
#include "component_store.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QString>

namespace HyprShelld::Tests {

inline QByteArray readBytes(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }
    return file.readAll();
}

inline bool writeBytes(const QString &path, const QByteArray &bytes)
{
    if (!QDir().mkpath(QFileInfo(path).absolutePath())) {
        return false;
    }
    QFile file(path);
    return file.open(QIODevice::WriteOnly | QIODevice::Truncate)
        && file.write(bytes) == bytes.size();
}

inline ComponentPaths componentPathsFor(
    const QString &root,
    const QString &defaultsFile
)
{
    return {
        .activeFile = root + QStringLiteral("/config/hyprshelld/components.json"),
        .recoveryFile = root
            + QStringLiteral("/state/hyprshelld/components.last-good.json"),
        .defaultsFile = defaultsFile,
    };
}

inline Components::ConfigurationCatalog configurationCatalog(
    const QString &defaultsFile,
    const QString &schemaFile
)
{
    const auto root = QJsonDocument::fromJson(readBytes(defaultsFile)).object();
    const auto desired = root.value(QStringLiteral("components"))
                             .toObject()
                             .value(QString::fromLatin1(
                                 Components::workspaceSwitcherId
                             ))
                             .toObject();
    const auto parsedSchema = Components::parseSettingsSchema(
        QByteArrayView(readBytes(schemaFile))
    );
    Q_ASSERT(parsedSchema);

    Components::ConfigurationCatalog catalog;
    catalog.digest = QString(64, QLatin1Char('a'));
    catalog.entries.insert(
        QString::fromLatin1(Components::workspaceSwitcherId),
        {
            .packageDigest = desired.value(
                QStringLiteral("packageDigest")
            ).toString(),
            .type = Components::ComponentType::BarWidget,
            .origin = Components::ComponentOrigin::System,
            .settingsSchema = *parsedSchema.value,
            .requestedCapabilities = {
                QString::fromLatin1(Components::workspacesActivateCapability),
                QString::fromLatin1(Components::workspacesReadCapability),
            },
        }
    );
    return catalog;
}

} // namespace HyprShelld::Tests
