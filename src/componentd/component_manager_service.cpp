#include "component_manager_service.h"

#include "component_inspection_sessions.h"

#include <QDBusMessage>
#include <QDebug>
#include <QRegularExpression>
#include <QVariantMap>

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
const QString runtimeUnavailableError = QStringLiteral(
    "org.hyprshelld.ComponentManager1.Error.RuntimeUnavailable"
);
const QString packageDigestMismatchError = QStringLiteral(
    "org.hyprshelld.ComponentManager1.Error.PackageDigestMismatch"
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
    , systemCatalog_(catalog)
    , catalog_(std::move(catalog))
    , connection_(QDBusConnection::sessionBus())
{
}

ComponentManagerService::~ComponentManagerService() = default;

ComponentManagerService::ComponentManagerService(
    Components::SystemCatalog systemCatalog,
    std::unique_ptr<Components::UserPackageStore> userPackageStore,
    std::unique_ptr<ComponentInspectionSessions> inspectionSessions,
    QDBusConnection connection,
    QObject *parent
)
    : QObject(parent)
    , systemCatalog_(std::move(systemCatalog))
    , catalog_(systemCatalog_)
    , connection_(std::move(connection))
{
    connection_.connect(
        QStringLiteral("org.freedesktop.DBus"),
        QStringLiteral("/org/freedesktop/DBus"),
        QStringLiteral("org.freedesktop.DBus"),
        QStringLiteral("NameOwnerChanged"),
        this,
        SLOT(nameOwnerChanged(QString,QString,QString))
    );
    initializePackageManagement(
        std::move(userPackageStore),
        std::move(inspectionSessions)
    );
}

void ComponentManagerService::initializePackageManagement(
    std::unique_ptr<Components::UserPackageStore> userPackageStore,
    std::unique_ptr<ComponentInspectionSessions> inspectionSessions
)
{
    if (userPackageStore_ || inspectionSessions_) {
        qWarning() << "Component package management was already initialized";
        return;
    }
    userPackageStore_ = std::move(userPackageStore);
    inspectionSessions_ = std::move(inspectionSessions);

    QString error;
    if (!reloadUserCatalog(error)) {
        qWarning().noquote() << QStringLiteral(
            "User component catalog is unavailable; protected components remain usable: %1"
        ).arg(error);
    }
    if (inspectionSessions_) {
        connect(
            inspectionSessions_.get(),
            &ComponentInspectionSessions::inspectionFinished,
            this,
            &ComponentManagerService::inspectionFinished
        );
    }
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

QByteArray ComponentManagerService::GetDeclarativeRuntime(
    const QString &componentId,
    const QString &expectedPackageDigest,
    const QString &expectedCatalogDigest
) const
{
    if (expectedCatalogDigest != catalog_.catalogDigest()) {
        reportError(
            staleCatalogDigestError,
            QStringLiteral("The component catalog changed; list it again")
        );
        return {};
    }

    const auto *entry = catalog_.find(componentId);
    if (entry == nullptr) {
        reportError(
            unknownComponentError,
            QStringLiteral("The requested component is not installed")
        );
        return {};
    }
    if (entry->packageDigest != expectedPackageDigest) {
        reportError(
            packageDigestMismatchError,
            QStringLiteral("The installed component digest changed")
        );
        return {};
    }
    if (entry->manifest.runtime.kind
            != Components::RuntimeKind::DeclarativeV1
        || entry->declarativeRuntime.isEmpty()) {
        reportError(
            runtimeUnavailableError,
            QStringLiteral("The requested component has no declarative runtime")
        );
        return {};
    }
    return entry->declarativeRuntime;
}

QString ComponentManagerService::BeginPackageInspection(
    const QDBusUnixFileDescriptor &packageFile
)
{
    const auto sender = caller();
    if (sender.isEmpty() || !packageFile.isValid()) {
        reportError(
            managerErrorName(QStringLiteral("InvalidPackageDescriptor")),
            QStringLiteral("A valid local package descriptor is required")
        );
        return {};
    }
    if (!inspectionSessions_) {
        reportError(
            managerErrorName(QStringLiteral("InspectionUnavailable")),
            QStringLiteral("Local package inspection is unavailable")
        );
        return {};
    }
    auto result = inspectionSessions_->begin(
        sender,
        packageFile.fileDescriptor()
    );
    if (!result.success) {
        reportError(
            result.errorName.isEmpty()
                ? managerErrorName(QStringLiteral("InspectionUnavailable"))
                : result.errorName,
            result.errorMessage.isEmpty()
                ? QStringLiteral("Package inspection could not be started")
                : result.errorMessage
        );
        return {};
    }
    return result.token;
}

void ComponentManagerService::CancelPackageInspection(
    const QString &inspectionToken
)
{
    const auto sender = caller();
    if (sender.isEmpty() || !inspectionSessions_) {
        reportError(
            managerErrorName(QStringLiteral("UnknownInspection")),
            QStringLiteral("The package inspection does not exist")
        );
        return;
    }
    const auto result = inspectionSessions_->cancel(sender, inspectionToken);
    if (!result.success) {
        reportError(
            result.errorName.isEmpty()
                ? managerErrorName(QStringLiteral("UnknownInspection"))
                : result.errorName,
            result.errorMessage.isEmpty()
                ? QStringLiteral("The package inspection is unavailable")
                : result.errorMessage
        );
    }
}

QString ComponentManagerService::InstallInspectedPackage(
    const QString &inspectionToken,
    const QString &expectedArchiveDigest,
    const QString &expectedCatalogDigest,
    QString &packageDigest,
    QString &catalogDigest
)
{
    if (expectedCatalogDigest != catalog_.catalogDigest()) {
        reportError(
            staleCatalogDigestError,
            QStringLiteral(
                "The component catalog changed after review; inspect the package again"
            )
        );
        return {};
    }
    const auto sender = caller();
    if (sender.isEmpty() || !inspectionSessions_ || !userPackageStore_) {
        reportError(
            managerErrorName(QStringLiteral("InspectionUnavailable")),
            QStringLiteral("Local package management is unavailable")
        );
        return {};
    }
    auto taken = inspectionSessions_->takeForInstall(
        sender,
        inspectionToken,
        expectedArchiveDigest
    );
    if (!taken.success || !taken.artifact.has_value()) {
        const auto prefix = QStringLiteral(
            "org.hyprshelld.ComponentManager1.Error."
        );
        const auto typedError = taken.errorName.startsWith(prefix)
            ? taken.errorName
            : managerErrorName(QStringLiteral("UnknownInspection"));
        reportError(
            typedError,
            taken.errorMessage.isEmpty()
                ? QStringLiteral("The inspected package is unavailable")
                : taken.errorMessage
        );
        return {};
    }

    auto artifact = std::move(*taken.artifact);
    if (catalog_.find(artifact.report.manifest.id) == nullptr
        && catalog_.componentIds().size() >= 512) {
        reportError(
            managerErrorName(QStringLiteral("PackageCatalogFull")),
            QStringLiteral(
                "The combined component catalog already contains 512 packages"
            )
        );
        return {};
    }
    const auto installed = userPackageStore_->install(
        artifact.report,
        artifact.materializedPath
    );
    if (!installed.success) {
        reportError(
            managerErrorName(installed.errorCode),
            installed.errorMessage
        );
        return {};
    }

    QString error;
    if (!acceptUserCatalog(
            installed.catalogEntries,
            error,
            true
        )) {
        reportError(
            managerErrorName(QStringLiteral("PackageTransactionFailed")),
            QStringLiteral("The package was stored but the catalog could not be published: %1")
                .arg(error)
        );
        return {};
    }
    packageDigest = installed.packageDigest;
    catalogDigest = catalog_.catalogDigest();
    return installed.componentId;
}

QString ComponentManagerService::RemovePackage(
    const QString &componentId,
    const QString &expectedPackageDigest,
    const QString &expectedCatalogDigest
)
{
    if (expectedCatalogDigest != catalog_.catalogDigest()) {
        reportError(
            staleCatalogDigestError,
            QStringLiteral("The component catalog changed; refresh it before removal")
        );
        return {};
    }
    const auto *entry = catalog_.find(componentId);
    if (entry == nullptr) {
        reportError(
            unknownComponentError,
            QStringLiteral("The requested component is not installed")
        );
        return {};
    }
    if (entry->manifest.origin != Components::ComponentOrigin::User) {
        reportError(
            managerErrorName(QStringLiteral("BuiltinRemovalForbidden")),
            QStringLiteral("Built-in components cannot be removed")
        );
        return {};
    }
    if (entry->packageDigest != expectedPackageDigest) {
        reportError(
            managerErrorName(QStringLiteral("PackageDigestMismatch")),
            QStringLiteral("The installed package changed; refresh it before removal")
        );
        return {};
    }
    for (const auto &otherId : catalog_.componentIds()) {
        if (otherId == componentId) {
            continue;
        }
        const auto *other = catalog_.find(otherId);
        if (other != nullptr
            && std::ranges::any_of(
                other->manifest.dependencies,
                [&componentId](const Components::ComponentDependency &dependency) {
                    return dependency.id == componentId;
                }
            )) {
            reportError(
                managerErrorName(QStringLiteral("PackageHasDependents")),
                QStringLiteral("Another installed component depends on this package")
            );
            return {};
        }
    }
    if (!userPackageStore_) {
        reportError(
            managerErrorName(QStringLiteral("PackageTransactionFailed")),
            QStringLiteral("Local package management is unavailable")
        );
        return {};
    }

    const auto removed = userPackageStore_->remove(
        componentId,
        expectedPackageDigest
    );
    if (!removed.success) {
        reportError(
            managerErrorName(removed.errorCode),
            removed.errorMessage
        );
        return {};
    }
    QString error;
    if (!acceptUserCatalog(
            removed.catalogEntries,
            error,
            true
        )) {
        reportError(
            managerErrorName(QStringLiteral("PackageTransactionFailed")),
            QStringLiteral("The package was removed but the catalog could not be refreshed: %1")
                .arg(error)
        );
        return {};
    }
    return catalog_.catalogDigest();
}

void ComponentManagerService::inspectionFinished(
    const QString &sender,
    const QString &token,
    const QByteArray &reportBytes,
    const QString &spoolPath,
    const QString &materializedPath,
    const QString &errorName,
    const QString &errorMessage
)
{
    Q_UNUSED(spoolPath)
    Q_UNUSED(materializedPath)
    auto signal = QDBusMessage::createTargetedSignal(
        sender,
        QStringLiteral("/org/hyprshelld/ComponentManager1"),
        QStringLiteral("org.hyprshelld.ComponentManager1"),
        QStringLiteral("PackageInspectionFinished")
    );
    signal.setArguments({token, reportBytes, errorName, errorMessage});
    if (!connection_.send(signal)) {
        const auto cancelled = inspectionSessions_->cancel(sender, token);
        Q_UNUSED(cancelled)
    }
}

void ComponentManagerService::nameOwnerChanged(
    const QString &name,
    const QString &oldOwner,
    const QString &newOwner
)
{
    Q_UNUSED(oldOwner)
    if (inspectionSessions_ && name.startsWith(QLatin1Char(':'))
        && newOwner.isEmpty()) {
        inspectionSessions_->cancelAllForSender(name);
    }
}

void ComponentManagerService::publishCatalogDigestChanged() const
{
    auto signal = QDBusMessage::createSignal(
        QStringLiteral("/org/hyprshelld/ComponentManager1"),
        QStringLiteral("org.freedesktop.DBus.Properties"),
        QStringLiteral("PropertiesChanged")
    );
    signal.setArguments({
        QStringLiteral("org.hyprshelld.ComponentManager1"),
        QVariantMap{{QStringLiteral("CatalogDigest"), catalog_.catalogDigest()}},
        QStringList(),
    });
    if (!connection_.send(signal)) {
        qWarning() << "Failed to publish component-manager catalog change";
    }
}

bool ComponentManagerService::reloadUserCatalog(
    QString &error,
    const bool publishDigestChange
)
{
    error.clear();
    if (!userPackageStore_) {
        catalog_ = systemCatalog_;
        return true;
    }
    auto loaded = userPackageStore_->load();
    if (!loaded.ok()) {
        error = loaded.error;
        return false;
    }
    for (const auto &warning : std::as_const(loaded.warnings)) {
        qWarning().noquote() << warning;
    }
    return acceptUserCatalog(
        std::move(loaded.entries),
        error,
        publishDigestChange
    );
}

bool ComponentManagerService::acceptUserCatalog(
    QVector<Components::CatalogEntry> entries,
    QString &error,
    const bool publishDigestChange
)
{
    error.clear();
    auto merged = Components::SystemCatalog::withUserEntries(
        systemCatalog_,
        std::move(entries)
    );
    if (!merged.ok()) {
        error = merged.error;
        return false;
    }
    const auto changed = catalog_.catalogDigest()
        != merged.catalog->catalogDigest();
    catalog_ = std::move(*merged.catalog);
    if (changed) {
        emit catalogDigestChanged();
        if (publishDigestChange) {
            publishCatalogDigestChanged();
        }
    }
    return true;
}

QString ComponentManagerService::caller() const
{
    return calledFromDBus() ? message().service() : QString();
}

QString ComponentManagerService::managerErrorName(const QString &suffix)
{
    static const QRegularExpression valid(
        QStringLiteral("^[A-Za-z][A-Za-z0-9]{0,63}$")
    );
    const auto safe = valid.match(suffix).hasMatch()
        ? suffix : QStringLiteral("PackageTransactionFailed");
    return QStringLiteral("org.hyprshelld.ComponentManager1.Error.%1")
        .arg(safe);
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
