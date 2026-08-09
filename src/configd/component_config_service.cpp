#include "component_config_service.h"

#include <QDBusMessage>
#include <QVariantMap>

#include <limits>
#include <utility>

namespace HyprShelld {
namespace {

const QString interfaceName = QStringLiteral("org.hyprshelld.ComponentConfig1");
const QString objectPath = QStringLiteral("/org/hyprshelld/Config1/Components");
const QString errorPrefix = QStringLiteral("org.hyprshelld.ComponentConfig1.Error.");

QString placementOf(
    const Components::ComponentConfiguration &configuration,
    const QString &instanceId
)
{
    for (auto layout = configuration.bars.constBegin();
         layout != configuration.bars.constEnd(); ++layout) {
        for (const auto &[name, instances] : {
                 std::pair{QStringLiteral("start"), layout->start},
                 std::pair{QStringLiteral("center"), layout->center},
                 std::pair{QStringLiteral("end"), layout->end},
             }) {
            const auto index = instances.indexOf(instanceId);
            if (index >= 0) {
                return layout.key() + QLatin1Char('/')
                    + layout->outputs.mode + QLatin1Char('/')
                    + layout->outputs.names.join(QLatin1Char('\n'))
                    + QLatin1Char('/') + name
                    + QLatin1Char('/') + QString::number(index);
            }
        }
    }
    return {};
}

} // namespace

ComponentConfigService::ComponentConfigService(
    ComponentStore store,
    QDBusConnection connection,
    std::optional<LegacyWorkspaceSettings> legacyWorkspaceSettings,
    QObject *parent
)
    : QObject(parent)
    , store_(std::move(store))
    , connection_(std::move(connection))
    , legacyWorkspaceSettings_(std::move(legacyWorkspaceSettings))
{
}

bool ComponentConfigService::available() const
{
    return available_;
}

bool ComponentConfigService::catalogAvailable() const
{
    return catalogAvailable_;
}

qulonglong ComponentConfigService::revision() const
{
    return available_ ? state_.revision : 0;
}

QString ComponentConfigService::catalogDigest() const
{
    return catalogDigest_;
}

QString ComponentConfigService::loadState() const
{
    return loadState_;
}

void ComponentConfigService::applyCatalog(
    Components::ConfigurationCatalog catalog
)
{
    if (!Components::isFullSha256Digest(catalog.digest)) {
        setCatalogUnavailable();
        return;
    }

    QVariantMap changed;
    if (!loadedOnce_) {
        const auto loaded = store_.load(catalog, legacyWorkspaceSettings_);
        state_ = loaded.state;
        writable_ = loaded.writable;
        loadedOnce_ = loaded.available;
        if (loadedOnce_) {
            legacyWorkspaceSettings_.reset();
        }
        if (available_ != loaded.available) {
            available_ = loaded.available;
            changed.insert(QStringLiteral("Available"), available_);
        }
        const auto nextLoadState = componentLoadStateName(loaded.loadState);
        if (loadState_ != nextLoadState) {
            loadState_ = nextLoadState;
            changed.insert(QStringLiteral("LoadState"), loadState_);
        }
        if (available_) {
            changed.insert(
                QStringLiteral("Revision"),
                QVariant::fromValue<qulonglong>(state_.revision)
            );
        }
    } else if (available_ && !writable_) {
        const auto retried = store_.load(catalog);
        if (retried.available
            && (retried.state == state_
                || store_.recognizesMigration(
                    state_, retried.state, catalog
                ))) {
            if (retried.state != state_) {
                state_ = retried.state;
                changed.insert(
                    QStringLiteral("Revision"),
                    QVariant::fromValue<qulonglong>(state_.revision)
                );
            }
            writable_ = retried.writable;
            const auto nextLoadState = componentLoadStateName(
                retried.loadState
            );
            if (loadState_ != nextLoadState) {
                loadState_ = nextLoadState;
                changed.insert(QStringLiteral("LoadState"), loadState_);
            }
        }
    } else if (available_) {
        const auto migrated = store_.migrate(state_, catalog);
        if (migrated.changed) {
            state_ = migrated.state;
            changed.insert(
                QStringLiteral("Revision"),
                QVariant::fromValue<qulonglong>(state_.revision)
            );
        }
        if (!migrated.writable) {
            writable_ = false;
            const auto unavailable = componentLoadStateName(
                ComponentLoadState::Unavailable
            );
            if (loadState_ != unavailable) {
                loadState_ = unavailable;
                changed.insert(QStringLiteral("LoadState"), loadState_);
            }
        }
    }

    if (loadedOnce_ && available_) {
        const auto revalidated = Components::parseComponentConfiguration(
            QByteArrayView(Components::serializeComponentConfiguration(state_)),
            catalog
        );
        if (!revalidated || *revalidated.value != state_) {
            setCatalogUnavailable();
            return;
        }
    }

    if (!catalogAvailable_) {
        catalogAvailable_ = true;
        changed.insert(QStringLiteral("CatalogAvailable"), true);
    }
    if (catalogDigest_ != catalog.digest) {
        catalogDigest_ = catalog.digest;
        changed.insert(QStringLiteral("CatalogDigest"), catalogDigest_);
    }
    catalog_ = std::move(catalog);

    publishProperties(changed);
    if (available_) {
        emit authoritativeSnapshotEstablished();
    }
}

void ComponentConfigService::setCatalogUnavailable()
{
    if (!catalogAvailable_) {
        return;
    }
    catalogAvailable_ = false;
    publishProperties({{QStringLiteral("CatalogAvailable"), false}});
}

QByteArray ComponentConfigService::GetSnapshot(
    qulonglong &snapshotRevision,
    QString &snapshotCatalogDigest
) const
{
    if (!available_) {
        reportError(
            errorPrefix + QStringLiteral("Unavailable"),
            QStringLiteral("Component configuration is unavailable")
        );
        snapshotRevision = 0;
        snapshotCatalogDigest.clear();
        return {};
    }
    snapshotRevision = state_.revision;
    snapshotCatalogDigest = catalogDigest_;
    return Components::serializeComponentConfiguration(state_);
}

qulonglong ComponentConfigService::ReplaceSnapshot(
    const qulonglong expectedRevision,
    const QString &expectedCatalogDigest,
    const QByteArray &candidateSnapshot
)
{
    if (!available_ || !writable_) {
        reportError(
            errorPrefix + QStringLiteral("Unavailable"),
            QStringLiteral("Component configuration is not writable")
        );
        return revision();
    }
    if (!catalogAvailable_) {
        reportError(
            errorPrefix + QStringLiteral("CatalogUnavailable"),
            QStringLiteral("The authoritative component catalog is unavailable")
        );
        return revision();
    }
    if (expectedRevision != state_.revision) {
        reportError(
            errorPrefix + QStringLiteral("StaleRevision"),
            QStringLiteral("The component configuration changed; read it again")
        );
        return revision();
    }
    if (expectedCatalogDigest != catalog_.digest) {
        reportError(
            errorPrefix + QStringLiteral("StaleCatalogDigest"),
            QStringLiteral("The component catalog changed; read it again")
        );
        return revision();
    }

    auto parsed = Components::parseComponentConfiguration(
        QByteArrayView(candidateSnapshot),
        catalog_
    );
    if (!parsed || parsed.value->revision != expectedRevision) {
        reportError(
            errorPrefix + QStringLiteral("InvalidSnapshot"),
            !parsed
                ? QStringLiteral("The proposed component configuration is invalid")
                : QStringLiteral("The embedded revision does not match the expected revision")
        );
        return revision();
    }

    QString dormantError;
    if (!preservesDormantState(*parsed.value, dormantError)) {
        reportError(
            errorPrefix + QStringLiteral("InvalidSnapshot"),
            dormantError
        );
        return revision();
    }

    if (*parsed.value == state_) {
        return revision();
    }
    if (state_.revision == std::numeric_limits<quint64>::max()) {
        reportError(
            errorPrefix + QStringLiteral("RevisionExhausted"),
            QStringLiteral("The component configuration revision is exhausted")
        );
        return revision();
    }

    auto next = std::move(*parsed.value);
    next.revision = state_.revision + 1;
    QString error;
    if (!store_.persist(state_, next, error)) {
        reportError(
            errorPrefix + QStringLiteral("PersistenceFailed"),
            error
        );
        return revision();
    }
    state_ = std::move(next);
    publishProperties({
        {QStringLiteral("Revision"),
         QVariant::fromValue<qulonglong>(state_.revision)},
    });
    return revision();
}

void ComponentConfigService::reportError(
    const QString &name,
    const QString &message
) const
{
    if (calledFromDBus()) {
        sendErrorReply(name, message);
    }
}

void ComponentConfigService::publishProperties(
    const QVariantMap &changed
) const
{
    if (changed.isEmpty()) {
        return;
    }
    auto signal = QDBusMessage::createSignal(
        objectPath,
        QStringLiteral("org.freedesktop.DBus.Properties"),
        QStringLiteral("PropertiesChanged")
    );
    signal.setArguments({interfaceName, changed, QStringList()});
    connection_.send(signal);
}

bool ComponentConfigService::preservesDormantState(
    const Components::ComponentConfiguration &candidate,
    QString &error
) const
{
    QSet<QString> dormantIds;
    for (auto desired = state_.components.constBegin();
         desired != state_.components.constEnd(); ++desired) {
        const auto live = catalog_.entries.constFind(desired.key());
        const auto dormant = live == catalog_.entries.cend()
            || live->packageDigest != desired->packageDigest;
        if (!dormant) {
            continue;
        }
        dormantIds.insert(desired.key());
        const auto proposed = candidate.components.constFind(desired.key());
        const auto adoptsUpdatedUserPackage =
            proposed != candidate.components.cend()
            && live != catalog_.entries.cend()
            && live->origin == Components::ComponentOrigin::User
            && desired->packageDigest != live->packageDigest
            && proposed->packageDigest == live->packageDigest
            && !proposed->enabled
            && proposed->grantedCapabilities.isEmpty();
        if (proposed == candidate.components.cend()
            || (*proposed != *desired && !adoptsUpdatedUserPackage)) {
            error = QStringLiteral(
                "Dormant component records must be preserved unchanged"
            );
            return false;
        }
        for (auto instance = state_.instances.constBegin();
             instance != state_.instances.constEnd(); ++instance) {
            if (instance->componentId != desired.key()) {
                continue;
            }
            const auto proposedInstance = candidate.instances.constFind(instance.key());
            if (proposedInstance == candidate.instances.cend()
                || *proposedInstance != *instance
                || placementOf(candidate, instance.key())
                    != placementOf(state_, instance.key())) {
                error = QStringLiteral(
                    "Dormant instances and placements must be preserved unchanged"
                );
                return false;
            }
            for (auto layout = state_.bars.constBegin();
                 layout != state_.bars.constEnd(); ++layout) {
                if (!layout->start.contains(instance.key())
                    && !layout->center.contains(instance.key())
                    && !layout->end.contains(instance.key())) {
                    continue;
                }
                const auto proposedLayout = candidate.bars.constFind(
                    layout.key()
                );
                if (proposedLayout == candidate.bars.cend()
                    || *proposedLayout != *layout) {
                    error = QStringLiteral(
                        "A layout containing a dormant instance must be preserved unchanged"
                    );
                    return false;
                }
            }
        }
    }

    for (auto desired = candidate.components.constBegin();
         desired != candidate.components.constEnd(); ++desired) {
        const auto live = catalog_.entries.constFind(desired.key());
        if ((live == catalog_.entries.cend()
             || live->packageDigest != desired->packageDigest)
            && !dormantIds.contains(desired.key())) {
            error = QStringLiteral("New dormant component records are not accepted");
            return false;
        }
    }
    for (auto instance = candidate.instances.constBegin();
         instance != candidate.instances.constEnd(); ++instance) {
        if (!dormantIds.contains(instance->componentId)) {
            continue;
        }
        const auto currentInstance = state_.instances.constFind(instance.key());
        if (currentInstance == state_.instances.cend()) {
            error = QStringLiteral("New dormant component instances are not accepted");
            return false;
        }
    }
    return true;
}

} // namespace HyprShelld
