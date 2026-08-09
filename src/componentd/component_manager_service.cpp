#include "component_manager_service.h"

#include <algorithm>
#include <utility>

namespace HyprShelld {
namespace {

const QString staleCatalogDigestError = QStringLiteral(
    "org.hyprshelld.ComponentManager1.Error.StaleCatalogDigest"
);
const QString unknownComponentError = QStringLiteral(
    "org.hyprshelld.ComponentManager1.Error.UnknownComponent"
);

QString optionalString(const std::optional<QString> &value)
{
    return value.value_or(QString());
}

} // namespace

ComponentManagerService::ComponentManagerService(
    Components::SystemCatalog catalog,
    QObject *parent
)
    : QObject(parent)
    , catalog_(std::move(catalog))
{
}

QString ComponentManagerService::catalogDigest() const
{
    return catalog_.catalogDigest();
}

QStringList ComponentManagerService::ListComponents(
    QString &catalogDigest
) const
{
    catalogDigest = catalog_.catalogDigest();
    return catalog_.componentIds();
}

uint ComponentManagerService::GetComponent(
    const QString &componentId,
    const QString &expectedCatalogDigest,
    QString &componentType,
    QString &version,
    QString &name,
    QString &description,
    QStringList &authorNames,
    QStringList &authorEmails,
    QStringList &authorHomepages,
    QString &license,
    QString &homepage,
    QString &source,
    QString &issues,
    QString &componentApiVersion,
    QString &runtimeKind,
    QString &runtimeFactory,
    QString &runtimeEntryPoint,
    QStringList &runtimeArguments,
    QByteArray &settingsSchema,
    QStringList &capabilityIds,
    QStringList &capabilityReasons,
    QStringList &dependencyIds,
    QStringList &dependencyVersionRequirements,
    QString &packageDigest,
    QString &origin,
    bool &removable
) const
{
    if (expectedCatalogDigest != catalog_.catalogDigest()) {
        reportError(
            staleCatalogDigestError,
            QStringLiteral("The component catalog changed; list it again")
        );
        return 0;
    }

    const auto *entry = catalog_.find(componentId);
    if (entry == nullptr) {
        reportError(
            unknownComponentError,
            QStringLiteral("The requested component is not installed")
        );
        return 0;
    }

    const auto &manifest = entry->manifest;
    componentType = Components::toString(manifest.type);
    version = manifest.version;
    name = manifest.name;
    description = manifest.description;

    authorNames.clear();
    authorEmails.clear();
    authorHomepages.clear();
    authorNames.reserve(manifest.authors.size());
    authorEmails.reserve(manifest.authors.size());
    authorHomepages.reserve(manifest.authors.size());
    for (const auto &author : manifest.authors) {
        authorNames.append(author.name);
        authorEmails.append(optionalString(author.email));
        authorHomepages.append(optionalString(author.homepage));
    }

    license = manifest.license;
    homepage = optionalString(manifest.homepage);
    source = optionalString(manifest.source);
    issues = optionalString(manifest.issues);
    componentApiVersion = manifest.componentApiVersion;
    runtimeKind = Components::toString(manifest.runtime.kind);
    runtimeFactory = manifest.runtime.factory;
    runtimeEntryPoint = manifest.runtime.entrypoint;
    runtimeArguments = manifest.runtime.arguments;
    settingsSchema = entry->settingsSchema;

    capabilityIds.clear();
    capabilityReasons.clear();
    capabilityIds.reserve(manifest.requestedCapabilities.size());
    capabilityReasons.reserve(manifest.requestedCapabilities.size());
    auto capabilities = manifest.requestedCapabilities;
    std::ranges::sort(capabilities, {}, &Components::CapabilityRequest::id);
    for (const auto &capability : capabilities) {
        capabilityIds.append(capability.id);
        capabilityReasons.append(capability.reason);
    }

    dependencyIds.clear();
    dependencyVersionRequirements.clear();
    dependencyIds.reserve(manifest.dependencies.size());
    dependencyVersionRequirements.reserve(manifest.dependencies.size());
    for (const auto &dependency : manifest.dependencies) {
        dependencyIds.append(dependency.id);
        dependencyVersionRequirements.append(dependency.versionRequirement);
    }

    packageDigest = entry->packageDigest;
    origin = Components::toString(manifest.origin);
    removable = manifest.origin == Components::ComponentOrigin::User;
    return manifest.manifestVersion;
}

void ComponentManagerService::reportError(
    const QString &name,
    const QString &message
) const
{
    if (calledFromDBus()) {
        sendErrorReply(name, message);
    }
}

} // namespace HyprShelld
