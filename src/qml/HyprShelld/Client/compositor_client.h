#pragma once

#include "input_device_projection.h"

#include <QDBusConnection>
#include <QByteArray>
#include <QJsonObject>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>
#include <QtQml/qqmlregistration.h>

#include <array>
#include <memory>
#include <optional>

class QDBusPendingCallWatcher;
class QDBusServiceWatcher;
class QDBusMessage;

namespace HyprShelld {

class CompositorOptionCatalog;
class CompositorActionCatalog;

class CompositorClient final : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(bool available READ available NOTIFY availableChanged)
    Q_PROPERTY(bool writable READ writable NOTIFY writableChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
    Q_PROPERTY(QString busyOperation READ busyOperation NOTIFY busyOperationChanged)
    Q_PROPERTY(qulonglong revision READ revision NOTIFY snapshotChanged)
    Q_PROPERTY(QString revisionToken READ revisionToken NOTIFY snapshotChanged)
    Q_PROPERTY(QString loadState READ loadState NOTIFY loadStateChanged)
    Q_PROPERTY(QString managementState READ managementState NOTIFY managementStateChanged)
    Q_PROPERTY(QString entrypointDigest READ entrypointDigest NOTIFY managementStateChanged)
    Q_PROPERTY(QString catalogDigest READ catalogDigest NOTIFY catalogDigestChanged)
    Q_PROPERTY(QString actionCatalogDigest READ actionCatalogDigest NOTIFY catalogDigestChanged)
    Q_PROPERTY(qulonglong appliedRevision READ appliedRevision NOTIFY applyStateChanged)
    Q_PROPERTY(QString applyState READ applyState NOTIFY applyStateChanged)
    Q_PROPERTY(QString requiredActivation READ requiredActivation NOTIFY applyStateChanged)
    Q_PROPERTY(QString generationDigest READ generationDigest NOTIFY applyStateChanged)
    Q_PROPERTY(QVariantMap snapshot READ snapshot NOTIFY snapshotChanged)
    Q_PROPERTY(bool catalogAvailable READ catalogAvailable NOTIFY appearanceChanged)
    Q_PROPERTY(bool allOptionsAvailable READ allOptionsAvailable NOTIFY allOptionsChanged)
    Q_PROPERTY(QVariantList allOptions READ allOptions NOTIFY allOptionsChanged)
    Q_PROPERTY(QVariantMap allValues READ allValues NOTIFY allOptionsChanged)
    Q_PROPERTY(QString allOptionsErrorName READ allOptionsErrorName NOTIFY allOptionsChanged)
    Q_PROPERTY(QString allOptionsErrorMessage READ allOptionsErrorMessage NOTIFY allOptionsChanged)
    Q_PROPERTY(bool actionCatalogAvailable READ actionCatalogAvailable NOTIFY bindingsChanged)
    Q_PROPERTY(bool bindingsAvailable READ bindingsAvailable NOTIFY bindingsChanged)
    Q_PROPERTY(bool bindingsProjectionAvailable READ bindingsProjectionAvailable NOTIFY bindingsChanged)
    Q_PROPERTY(QVariantList bindings READ bindings NOTIFY bindingsChanged)
    Q_PROPERTY(QVariantList submaps READ submaps NOTIFY bindingsChanged)
    Q_PROPERTY(QVariantList bindingActions READ bindingActions NOTIFY bindingsChanged)
    Q_PROPERTY(QString bindingsErrorName READ bindingsErrorName NOTIFY bindingsChanged)
    Q_PROPERTY(QString bindingsErrorMessage READ bindingsErrorMessage NOTIFY bindingsChanged)
    Q_PROPERTY(bool environmentAvailable READ environmentAvailable NOTIFY environmentChanged)
    Q_PROPERTY(bool environmentProjectionAvailable READ environmentProjectionAvailable NOTIFY environmentChanged)
    Q_PROPERTY(QVariantList environmentVariables READ environmentVariables NOTIFY environmentChanged)
    Q_PROPERTY(QString environmentErrorName READ environmentErrorName NOTIFY environmentChanged)
    Q_PROPERTY(QString environmentErrorMessage READ environmentErrorMessage NOTIFY environmentChanged)
    Q_PROPERTY(bool permissionsAvailable READ permissionsAvailable NOTIFY permissionsChanged)
    Q_PROPERTY(bool permissionsProjectionAvailable READ permissionsProjectionAvailable NOTIFY permissionsChanged)
    Q_PROPERTY(QVariantList permissions READ permissions NOTIFY permissionsChanged)
    Q_PROPERTY(QString permissionErrorName READ permissionErrorName NOTIFY permissionsChanged)
    Q_PROPERTY(QString permissionErrorMessage READ permissionErrorMessage NOTIFY permissionsChanged)
    Q_PROPERTY(bool displayDiscoveryAvailable READ displayDiscoveryAvailable NOTIFY displayDiscoveryAvailableChanged)
    Q_PROPERTY(bool appearanceAvailable READ appearanceAvailable NOTIFY appearanceChanged)
    Q_PROPERTY(bool appearanceProjectionAvailable READ appearanceProjectionAvailable NOTIFY appearanceChanged)
    Q_PROPERTY(bool appearanceAnimationProjectionAvailable READ appearanceAnimationProjectionAvailable NOTIFY appearanceChanged)
    Q_PROPERTY(bool retryApplyAvailable READ retryApplyAvailable NOTIFY appearanceChanged)
    Q_PROPERTY(bool recoveryAvailable READ recoveryAvailable NOTIFY appearanceChanged)
    Q_PROPERTY(QVariantList appearanceOptions READ appearanceOptions NOTIFY appearanceChanged)
    Q_PROPERTY(QVariantMap appearanceValues READ appearanceValues NOTIFY appearanceChanged)
    Q_PROPERTY(QVariantList appearanceCurves READ appearanceCurves NOTIFY appearanceChanged)
    Q_PROPERTY(QVariantList appearanceAnimations READ appearanceAnimations NOTIFY appearanceChanged)
    Q_PROPERTY(QString appearanceErrorName READ appearanceErrorName NOTIFY appearanceChanged)
    Q_PROPERTY(QString appearanceErrorMessage READ appearanceErrorMessage NOTIFY appearanceChanged)
    Q_PROPERTY(bool inputAvailable READ inputAvailable NOTIFY inputChanged)
    Q_PROPERTY(bool inputProjectionAvailable READ inputProjectionAvailable NOTIFY inputChanged)
    Q_PROPERTY(QVariantList inputOptions READ inputOptions NOTIFY inputChanged)
    Q_PROPERTY(QVariantMap inputValues READ inputValues NOTIFY inputChanged)
    Q_PROPERTY(bool inputGesturesProjectionAvailable READ inputGesturesProjectionAvailable NOTIFY inputChanged)
    Q_PROPERTY(QVariantList inputGestures READ inputGestures NOTIFY inputChanged)
    Q_PROPERTY(QVariantList inputGestureCompatibility READ inputGestureCompatibility NOTIFY inputChanged)
    Q_PROPERTY(QVariantList inputGestureActions READ inputGestureActions NOTIFY inputChanged)
    Q_PROPERTY(QString inputErrorName READ inputErrorName NOTIFY inputChanged)
    Q_PROPERTY(QString inputErrorMessage READ inputErrorMessage NOTIFY inputChanged)
    Q_PROPERTY(bool inputDeviceDiscoveryAvailable READ inputDeviceDiscoveryAvailable NOTIFY inputDeviceDiscoveryChanged)
    Q_PROPERTY(bool inputDeviceDiscoveryBusy READ inputDeviceDiscoveryBusy NOTIFY inputDeviceDiscoveryChanged)
    Q_PROPERTY(QVariantList connectedInputDevices READ connectedInputDevices NOTIFY inputDeviceDiscoveryChanged)
    Q_PROPERTY(qulonglong inputDevicesObservedAtMs READ inputDevicesObservedAtMs NOTIFY inputDeviceDiscoveryChanged)
    Q_PROPERTY(QString inputDeviceInventoryDigest READ inputDeviceInventoryDigest NOTIFY inputDeviceDiscoveryChanged)
    Q_PROPERTY(QVariantMap inputDeviceUnaddressableCounts READ inputDeviceUnaddressableCounts NOTIFY inputDeviceDiscoveryChanged)
    Q_PROPERTY(QString inputDeviceDiscoveryErrorName READ inputDeviceDiscoveryErrorName NOTIFY inputDeviceDiscoveryChanged)
    Q_PROPERTY(QString inputDeviceDiscoveryErrorMessage READ inputDeviceDiscoveryErrorMessage NOTIFY inputDeviceDiscoveryChanged)
    Q_PROPERTY(bool inputDeviceProjectionAvailable READ inputDeviceProjectionAvailable NOTIFY inputDeviceProjectionChanged)
    Q_PROPERTY(QVariantList savedInputDevices READ savedInputDevices NOTIFY inputDeviceProjectionChanged)
    Q_PROPERTY(QVariantList otherSavedInputDevices READ otherSavedInputDevices NOTIFY inputDeviceProjectionChanged)
    Q_PROPERTY(QString inputDeviceProjectionRevisionToken READ inputDeviceProjectionRevisionToken NOTIFY inputDeviceProjectionChanged)
    Q_PROPERTY(QString inputDeviceProjectionInventoryDigest READ inputDeviceProjectionInventoryDigest NOTIFY inputDeviceProjectionChanged)
    Q_PROPERTY(QString inputDeviceProjectionErrorName READ inputDeviceProjectionErrorName NOTIFY inputDeviceProjectionChanged)
    Q_PROPERTY(QString inputDeviceProjectionErrorMessage READ inputDeviceProjectionErrorMessage NOTIFY inputDeviceProjectionChanged)
    Q_PROPERTY(bool inputDevicesAvailable READ inputDevicesAvailable NOTIFY inputDevicesChanged)
    Q_PROPERTY(bool inputDevicesProjectionAvailable READ inputDevicesProjectionAvailable NOTIFY inputDevicesChanged)
    Q_PROPERTY(QVariantList inputDevices READ inputDevices NOTIFY inputDevicesChanged)
    Q_PROPERTY(QString inputDevicesErrorName READ inputDevicesErrorName NOTIFY inputDevicesChanged)
    Q_PROPERTY(QString inputDevicesErrorMessage READ inputDevicesErrorMessage NOTIFY inputDevicesChanged)
    Q_PROPERTY(bool windowsAvailable READ windowsAvailable NOTIFY windowsChanged)
    Q_PROPERTY(bool windowsProjectionAvailable READ windowsProjectionAvailable NOTIFY windowsChanged)
    Q_PROPERTY(QVariantList windowsOptions READ windowsOptions NOTIFY windowsChanged)
    Q_PROPERTY(QVariantMap windowsValues READ windowsValues NOTIFY windowsChanged)
    Q_PROPERTY(QString windowsErrorName READ windowsErrorName NOTIFY windowsChanged)
    Q_PROPERTY(QString windowsErrorMessage READ windowsErrorMessage NOTIFY windowsChanged)
    Q_PROPERTY(bool workspacesAvailable READ workspacesAvailable NOTIFY workspacesChanged)
    Q_PROPERTY(bool workspacesProjectionAvailable READ workspacesProjectionAvailable NOTIFY workspacesChanged)
    Q_PROPERTY(QVariantList workspacesOptions READ workspacesOptions NOTIFY workspacesChanged)
    Q_PROPERTY(QVariantMap workspacesValues READ workspacesValues NOTIFY workspacesChanged)
    Q_PROPERTY(bool workspaceRulesProjectionAvailable READ workspaceRulesProjectionAvailable NOTIFY workspacesChanged)
    Q_PROPERTY(QVariantList workspaceRules READ workspaceRules NOTIFY workspacesChanged)
    Q_PROPERTY(QString workspacesErrorName READ workspacesErrorName NOTIFY workspacesChanged)
    Q_PROPERTY(QString workspacesErrorMessage READ workspacesErrorMessage NOTIFY workspacesChanged)
    Q_PROPERTY(bool advancedAvailable READ advancedAvailable NOTIFY advancedChanged)
    Q_PROPERTY(bool advancedProjectionAvailable READ advancedProjectionAvailable NOTIFY advancedChanged)
    Q_PROPERTY(QVariantList advancedOptions READ advancedOptions NOTIFY advancedChanged)
    Q_PROPERTY(QVariantMap advancedValues READ advancedValues NOTIFY advancedChanged)
    Q_PROPERTY(QString advancedErrorName READ advancedErrorName NOTIFY advancedChanged)
    Q_PROPERTY(QString advancedErrorMessage READ advancedErrorMessage NOTIFY advancedChanged)
    Q_PROPERTY(bool rulesAvailable READ rulesAvailable NOTIFY rulesChanged)
    Q_PROPERTY(bool rulesProjectionAvailable READ rulesProjectionAvailable NOTIFY rulesChanged)
    Q_PROPERTY(QVariantList windowRules READ windowRules NOTIFY rulesChanged)
    Q_PROPERTY(QVariantList layerRules READ layerRules NOTIFY rulesChanged)
    Q_PROPERTY(QString rulesErrorName READ rulesErrorName NOTIFY rulesChanged)
    Q_PROPERTY(QString rulesErrorMessage READ rulesErrorMessage NOTIFY rulesChanged)
    Q_PROPERTY(QVariantList connectedDisplays READ connectedDisplays NOTIFY connectedDisplaysChanged)
    Q_PROPERTY(qulonglong displaysObservedAtMs READ displaysObservedAtMs NOTIFY connectedDisplaysChanged)
    Q_PROPERTY(QString topologyDigest READ topologyDigest NOTIFY connectedDisplaysChanged)
    Q_PROPERTY(QString displayConfirmationState READ displayConfirmationState NOTIFY displayConfirmationChanged)
    Q_PROPERTY(qulonglong displayConfirmationRevision READ displayConfirmationRevision NOTIFY displayConfirmationChanged)
    Q_PROPERTY(qulonglong displayConfirmationDeadlineMs READ displayConfirmationDeadlineMs NOTIFY displayConfirmationChanged)
    Q_PROPERTY(QString displayConfirmationGeneration READ displayConfirmationGeneration NOTIFY displayConfirmationChanged)
    Q_PROPERTY(bool displayConfirmationOwned READ displayConfirmationOwned NOTIFY displayConfirmationChanged)
    Q_PROPERTY(QString sharedBorderSyncState READ sharedBorderSyncState NOTIFY sharedBorderSyncChanged)
    Q_PROPERTY(qulonglong sharedBorderSourceRevision READ sharedBorderSourceRevision NOTIFY sharedBorderSyncChanged)
    Q_PROPERTY(QString sharedBorderSourceRevisionToken READ sharedBorderSourceRevisionToken NOTIFY sharedBorderSyncChanged)
    Q_PROPERTY(QString sharedBorderSyncError READ sharedBorderSyncError NOTIFY sharedBorderSyncChanged)
    Q_PROPERTY(QString sharedSpacingSyncState READ sharedSpacingSyncState NOTIFY sharedSpacingSyncChanged)
    Q_PROPERTY(qulonglong sharedSpacingSourceRevision READ sharedSpacingSourceRevision NOTIFY sharedSpacingSyncChanged)
    Q_PROPERTY(QString sharedSpacingSourceRevisionToken READ sharedSpacingSourceRevisionToken NOTIFY sharedSpacingSyncChanged)
    Q_PROPERTY(QString sharedSpacingSyncError READ sharedSpacingSyncError NOTIFY sharedSpacingSyncChanged)
    Q_PROPERTY(QString lastErrorOperation READ lastErrorOperation NOTIFY lastErrorChanged)
    Q_PROPERTY(QString lastErrorName READ lastErrorName NOTIFY lastErrorChanged)
    Q_PROPERTY(QString lastErrorMessage READ lastErrorMessage NOTIFY lastErrorChanged)

public:
    explicit CompositorClient(QObject *parent = nullptr);
    CompositorClient(QDBusConnection connection, QObject *parent);
    ~CompositorClient() override;

    [[nodiscard]] bool available() const;
    [[nodiscard]] bool writable() const;
    [[nodiscard]] bool busy() const;
    [[nodiscard]] QString busyOperation() const;
    [[nodiscard]] qulonglong revision() const;
    [[nodiscard]] QString revisionToken() const;
    [[nodiscard]] QString loadState() const;
    [[nodiscard]] QString managementState() const;
    [[nodiscard]] QString entrypointDigest() const;
    [[nodiscard]] QString catalogDigest() const;
    [[nodiscard]] QString actionCatalogDigest() const;
    [[nodiscard]] qulonglong appliedRevision() const;
    [[nodiscard]] QString applyState() const;
    [[nodiscard]] QString requiredActivation() const;
    [[nodiscard]] QString generationDigest() const;
    [[nodiscard]] QVariantMap snapshot() const;
    [[nodiscard]] bool catalogAvailable() const;
    [[nodiscard]] bool allOptionsAvailable() const;
    [[nodiscard]] QVariantList allOptions() const;
    [[nodiscard]] QVariantMap allValues() const;
    [[nodiscard]] QString allOptionsErrorName() const;
    [[nodiscard]] QString allOptionsErrorMessage() const;
    [[nodiscard]] bool actionCatalogAvailable() const;
    [[nodiscard]] bool bindingsAvailable() const;
    [[nodiscard]] bool bindingsProjectionAvailable() const;
    [[nodiscard]] QVariantList bindings() const;
    [[nodiscard]] QVariantList submaps() const;
    [[nodiscard]] QVariantList bindingActions() const;
    [[nodiscard]] QString bindingsErrorName() const;
    [[nodiscard]] QString bindingsErrorMessage() const;
    [[nodiscard]] bool environmentAvailable() const;
    [[nodiscard]] bool environmentProjectionAvailable() const;
    [[nodiscard]] QVariantList environmentVariables() const;
    [[nodiscard]] QString environmentErrorName() const;
    [[nodiscard]] QString environmentErrorMessage() const;
    [[nodiscard]] bool permissionsAvailable() const;
    [[nodiscard]] bool permissionsProjectionAvailable() const;
    [[nodiscard]] QVariantList permissions() const;
    [[nodiscard]] QString permissionErrorName() const;
    [[nodiscard]] QString permissionErrorMessage() const;
    [[nodiscard]] bool displayDiscoveryAvailable() const;
    [[nodiscard]] bool appearanceAvailable() const;
    [[nodiscard]] bool appearanceProjectionAvailable() const;
    [[nodiscard]] bool appearanceAnimationProjectionAvailable() const;
    [[nodiscard]] bool retryApplyAvailable() const;
    [[nodiscard]] bool recoveryAvailable() const;
    [[nodiscard]] QVariantList appearanceOptions() const;
    [[nodiscard]] QVariantMap appearanceValues() const;
    [[nodiscard]] QVariantList appearanceCurves() const;
    [[nodiscard]] QVariantList appearanceAnimations() const;
    [[nodiscard]] QString appearanceErrorName() const;
    [[nodiscard]] QString appearanceErrorMessage() const;
    [[nodiscard]] bool inputAvailable() const;
    [[nodiscard]] bool inputProjectionAvailable() const;
    [[nodiscard]] QVariantList inputOptions() const;
    [[nodiscard]] QVariantMap inputValues() const;
    [[nodiscard]] bool inputGesturesProjectionAvailable() const;
    [[nodiscard]] QVariantList inputGestures() const;
    [[nodiscard]] QVariantList inputGestureCompatibility() const;
    [[nodiscard]] QVariantList inputGestureActions() const;
    [[nodiscard]] QString inputErrorName() const;
    [[nodiscard]] QString inputErrorMessage() const;
    [[nodiscard]] bool inputDeviceDiscoveryAvailable() const;
    [[nodiscard]] bool inputDeviceDiscoveryBusy() const;
    [[nodiscard]] QVariantList connectedInputDevices() const;
    [[nodiscard]] qulonglong inputDevicesObservedAtMs() const;
    [[nodiscard]] QString inputDeviceInventoryDigest() const;
    [[nodiscard]] QVariantMap inputDeviceUnaddressableCounts() const;
    [[nodiscard]] QString inputDeviceDiscoveryErrorName() const;
    [[nodiscard]] QString inputDeviceDiscoveryErrorMessage() const;
    [[nodiscard]] bool inputDeviceProjectionAvailable() const;
    [[nodiscard]] QVariantList savedInputDevices() const;
    [[nodiscard]] QVariantList otherSavedInputDevices() const;
    [[nodiscard]] QString inputDeviceProjectionRevisionToken() const;
    [[nodiscard]] QString inputDeviceProjectionInventoryDigest() const;
    [[nodiscard]] QString inputDeviceProjectionErrorName() const;
    [[nodiscard]] QString inputDeviceProjectionErrorMessage() const;
    [[nodiscard]] bool inputDevicesAvailable() const;
    [[nodiscard]] bool inputDevicesProjectionAvailable() const;
    [[nodiscard]] QVariantList inputDevices() const;
    [[nodiscard]] QString inputDevicesErrorName() const;
    [[nodiscard]] QString inputDevicesErrorMessage() const;
    [[nodiscard]] bool windowsAvailable() const;
    [[nodiscard]] bool windowsProjectionAvailable() const;
    [[nodiscard]] QVariantList windowsOptions() const;
    [[nodiscard]] QVariantMap windowsValues() const;
    [[nodiscard]] QString windowsErrorName() const;
    [[nodiscard]] QString windowsErrorMessage() const;
    [[nodiscard]] bool workspacesAvailable() const;
    [[nodiscard]] bool workspacesProjectionAvailable() const;
    [[nodiscard]] QVariantList workspacesOptions() const;
    [[nodiscard]] QVariantMap workspacesValues() const;
    [[nodiscard]] bool workspaceRulesProjectionAvailable() const;
    [[nodiscard]] QVariantList workspaceRules() const;
    [[nodiscard]] QString workspacesErrorName() const;
    [[nodiscard]] QString workspacesErrorMessage() const;
    [[nodiscard]] bool advancedAvailable() const;
    [[nodiscard]] bool advancedProjectionAvailable() const;
    [[nodiscard]] QVariantList advancedOptions() const;
    [[nodiscard]] QVariantMap advancedValues() const;
    [[nodiscard]] QString advancedErrorName() const;
    [[nodiscard]] QString advancedErrorMessage() const;
    [[nodiscard]] bool rulesAvailable() const;
    [[nodiscard]] bool rulesProjectionAvailable() const;
    [[nodiscard]] QVariantList windowRules() const;
    [[nodiscard]] QVariantList layerRules() const;
    [[nodiscard]] QString rulesErrorName() const;
    [[nodiscard]] QString rulesErrorMessage() const;
    [[nodiscard]] QVariantList connectedDisplays() const;
    [[nodiscard]] qulonglong displaysObservedAtMs() const;
    [[nodiscard]] QString topologyDigest() const;
    [[nodiscard]] QString displayConfirmationState() const;
    [[nodiscard]] qulonglong displayConfirmationRevision() const;
    [[nodiscard]] qulonglong displayConfirmationDeadlineMs() const;
    [[nodiscard]] QString displayConfirmationGeneration() const;
    [[nodiscard]] bool displayConfirmationOwned() const;
    [[nodiscard]] QString sharedBorderSyncState() const;
    [[nodiscard]] qulonglong sharedBorderSourceRevision() const;
    [[nodiscard]] QString sharedBorderSourceRevisionToken() const;
    [[nodiscard]] QString sharedBorderSyncError() const;
    [[nodiscard]] QString sharedSpacingSyncState() const;
    [[nodiscard]] qulonglong sharedSpacingSourceRevision() const;
    [[nodiscard]] QString sharedSpacingSourceRevisionToken() const;
    [[nodiscard]] QString sharedSpacingSyncError() const;
    [[nodiscard]] QString lastErrorOperation() const;
    [[nodiscard]] QString lastErrorName() const;
    [[nodiscard]] QString lastErrorMessage() const;

    Q_INVOKABLE void refresh();
    Q_INVOKABLE void refreshConnectedInputDevices();
    Q_INVOKABLE void adoptManagedConfiguration();
    Q_INVOKABLE void applyConfiguration();
    Q_INVOKABLE void saveOptions(const QVariantMap &values);
    Q_INVOKABLE void saveBindings(
        const QVariantList &bindings,
        const QVariantList &submaps
    );
    Q_INVOKABLE void saveEnvironment(const QVariantList &environment);
    Q_INVOKABLE void savePermissions(const QVariantList &permissions);
    Q_INVOKABLE void saveInputDevices(const QVariantList &devices);
    Q_INVOKABLE void saveAppearance(
        const QVariantMap &values,
        const QVariantList &curves,
        const QVariantList &animations
    );
    Q_INVOKABLE void saveInput(
        const QVariantMap &values,
        const QVariantList &gestures
    );
    Q_INVOKABLE void saveWindows(const QVariantMap &values);
    Q_INVOKABLE void saveWorkspaces(
        const QVariantMap &values,
        const QVariantList &workspaceRules
    );
    Q_INVOKABLE void saveAdvanced(const QVariantMap &values);
    Q_INVOKABLE void saveRules(
        const QVariantList &windowRules,
        const QVariantList &layerRules
    );
    Q_INVOKABLE void retryApply();
    Q_INVOKABLE void recoverConfiguration();
    Q_INVOKABLE void previewDisplayConfiguration(
        const QVariantList &outputs,
        uint timeoutSeconds = 15
    );
    Q_INVOKABLE void confirmDisplayConfiguration();
    Q_INVOKABLE void revertDisplayConfiguration();
    Q_INVOKABLE void retrySharedBorderSync();
    Q_INVOKABLE void retrySharedSpacingSync();
    Q_INVOKABLE void clearError();

signals:
    void availableChanged();
    void writableChanged();
    void busyChanged();
    void busyOperationChanged();
    void snapshotChanged();
    void allOptionsChanged();
    void bindingsChanged();
    void environmentChanged();
    void permissionsChanged();
    void inputDevicesChanged();
    void appearanceChanged();
    void inputChanged();
    void inputDeviceDiscoveryChanged();
    void inputDeviceProjectionChanged();
    void windowsChanged();
    void workspacesChanged();
    void advancedChanged();
    void rulesChanged();
    void displayDiscoveryAvailableChanged();
    void loadStateChanged();
    void managementStateChanged();
    void catalogDigestChanged();
    void applyStateChanged();
    void connectedDisplaysChanged();
    void displayConfirmationChanged();
    void sharedBorderSyncChanged();
    void sharedSpacingSyncChanged();
    void lastErrorChanged();
    void operationFailed(const QString &name, const QString &message);

private slots:
    void propertiesChanged(
        const QString &interfaceName,
        const QVariantMap &changed,
        const QStringList &invalidated
    );
    void serviceOwnerChanged(
        const QString &name,
        const QString &oldOwner,
        const QString &newOwner
    );

private:
    enum class OptionGroup {
        None,
        AllOptions,
        Appearance,
        Input,
        Windows,
        Workspaces,
        Advanced,
        Rules,
        Bindings,
        Environment,
        Permissions,
        InputDevices,
    };

    enum class Mutation {
        Adopt,
        Apply,
        Preview,
        Confirm,
        Revert,
        Recover,
        SharedBorderSync,
        SharedSpacingSync,
    };

    struct SnapshotSaveRequest final {
        OptionGroup group = OptionGroup::None;
        QByteArray candidate;
        qulonglong expectedRevision = 0;
        QString catalogDigest;
        QString actionCatalogDigest;
    };

    struct ApplyRequest final {
        OptionGroup group = OptionGroup::None;
        qulonglong revision = 0;
        QString catalogDigest;
        QString actionCatalogDigest;
    };

    struct OptionGroupState final {
        QVariantMap values;
        QVariantList appearanceCurves;
        QVariantList appearanceAnimations;
        QVariantList inputGestures;
        QVariantList inputGestureCompatibility;
        QVariantList workspaceRules;
        QVariantList windowRules;
        QVariantList layerRules;
        QVariantList bindings;
        QVariantList submaps;
        QVariantList environment;
        QVariantList permissions;
        QVariantList inputDevices;
        QString projectionErrorName;
        QString projectionErrorMessage;
        QString complexProjectionErrorName;
        QString complexProjectionErrorMessage;
        QString authorityErrorName;
        QString authorityErrorMessage;
        QString operationErrorName;
        QString operationErrorMessage;
        bool projectionValid = false;
        bool appearanceAnimationProjectionValid = false;
        bool inputGesturesProjectionValid = false;
        bool workspaceRulesProjectionValid = false;
    };

    struct InputDeviceProjectionChanges final {
        bool discovery = false;
        bool projection = false;
    };

    void fetchSnapshot(quint64 generation);
    void fetchOptionCatalog(quint64 generation);
    void fetchActionCatalog(quint64 generation);
    void fetchConnectedDisplays(quint64 generation);
    void fetchConnectedInputDevices(quint64 generation);
    void fetchPendingDisplayConfirmation(quint64 generation);
    [[nodiscard]] bool applyProperties(
        const QVariantMap &properties,
        bool requireAll = false
    );
    void beginMutation(Mutation mutation, const QDBusMessage &message, int timeoutMs);
    void saveOptionGroup(OptionGroup group, const QVariantMap &values);
    void saveCollectionGroup(
        OptionGroup group,
        const QVariantList &first,
        const QVariantList &second = {}
    );
    void sendSnapshotReplace(
        const SnapshotSaveRequest &request,
        bool retry
    );
    void verifySnapshotReplacement(const SnapshotSaveRequest &request);
    void sendApplyRequest(const ApplyRequest &request, bool retry);
    void reconcileApplyOutcome(const ApplyRequest &request);
    void finishMutation();
    void finishHydration(bool accepted);
    void updateOptionGroupProjection(OptionGroup group);
    void updateAllOptionGroupProjections();
    [[nodiscard]] InputDeviceProjectionChanges updateInputDeviceProjection();
    void clearInputDeviceSavedProjection(
        const QString &errorName,
        const QString &errorMessage
    );
    void clearInputDeviceDiscovery(
        const QString &errorName,
        const QString &errorMessage
    );
    void clearInputDeviceAuthorities(
        const QString &errorName,
        const QString &errorMessage
    );
    void setAvailable(bool available);
    void setCatalogAvailable(bool available);
    void setActionCatalogAvailable(bool available);
    void setDisplayDiscoveryAvailable(bool available);
    void setBusy(bool busy);
    void setBusyOperation(const QString &operation);
    void setProjectionError(
        OptionGroup group,
        const QString &name,
        const QString &message
    );
    void setOperationError(
        OptionGroup group,
        const QString &name,
        const QString &message
    );
    void setAuthorityError(
        OptionGroup group,
        const QString &name,
        const QString &message
    );
    void clearAuthorityErrors();
    void clearOperationError(OptionGroup group);
    [[nodiscard]] QString scopedErrorName(OptionGroup group) const;
    [[nodiscard]] QString scopedErrorMessage(OptionGroup group) const;
    [[nodiscard]] bool optionGroupAvailable(OptionGroup group) const;
    [[nodiscard]] OptionGroupState *optionGroupState(OptionGroup group);
    [[nodiscard]] const OptionGroupState *optionGroupState(
        OptionGroup group
    ) const;
    [[nodiscard]] static QString optionGroupName(OptionGroup group);
    [[nodiscard]] static QString optionGroupErrorSuffix(OptionGroup group);
    [[nodiscard]] static QString optionGroupSaveOperation(OptionGroup group);
    [[nodiscard]] static QString optionGroupApplyOperation(OptionGroup group);
    void emitOptionGroupChanged(OptionGroup group);
    void emitAllOptionGroupsChanged();
    void setError(
        const QString &operation,
        const QString &name,
        const QString &message
    );

    QDBusConnection connection_;
    QDBusServiceWatcher *serviceWatcher_ = nullptr;
    QVariantMap snapshot_;
    QJsonObject snapshotObject_;
    std::unique_ptr<CompositorOptionCatalog> optionCatalog_;
    std::unique_ptr<CompositorActionCatalog> actionCatalog_;
    std::array<OptionGroupState, 11> optionGroups_;
    QVariantList connectedDisplays_;
    std::optional<QVector<Hyprland::DeviceConfiguration>> savedInputDevices_;
    std::optional<Hyprland::ConnectedInputDeviceInventory>
        connectedInputDeviceInventory_;
    InputDeviceProjection inputDeviceProjection_;
    quint64 ownerGeneration_ = 0;
    quint64 refreshGeneration_ = 0;
    quint64 inputDeviceRefreshGeneration_ = 0;
    qulonglong advertisedRevision_ = 0;
    qulonglong revision_ = 0;
    qulonglong appliedRevision_ = 0;
    qulonglong displaysObservedAtMs_ = 0;
    qulonglong inputDevicesObservedAtMs_ = 0;
    qulonglong inputDeviceProjectionRevision_ = 0;
    qulonglong displayConfirmationRevision_ = 0;
    qulonglong displayConfirmationDeadlineMs_ = 0;
    qulonglong sharedBorderSourceRevision_ = 0;
    qulonglong sharedSpacingSourceRevision_ = 0;
    QString loadState_ = QStringLiteral("unavailable");
    QString managementState_ = QStringLiteral("unmanaged");
    QString entrypointDigest_;
    QString advertisedCatalogDigest_;
    QString advertisedActionCatalogDigest_;
    QString catalogDigest_;
    QString actionCatalogDigest_;
    QString topologyDigest_;
    QString inputDeviceDiscoveryErrorName_;
    QString inputDeviceDiscoveryErrorMessage_;
    QString inputDeviceProjectionErrorName_;
    QString inputDeviceProjectionErrorMessage_;
    QString applyState_ = QStringLiteral("unavailable");
    QString requiredActivation_ = QStringLiteral("none");
    QString generationDigest_;
    QString displayConfirmationState_ = QStringLiteral("idle");
    QString displayConfirmationToken_;
    QString displayConfirmationGeneration_;
    QString sharedBorderSyncState_ = QStringLiteral("unavailable");
    QString sharedBorderSyncError_ = QStringLiteral(
        "Shared visual settings are unavailable"
    );
    QString sharedSpacingSyncState_ = QStringLiteral("unavailable");
    QString sharedSpacingSyncError_ = QStringLiteral(
        "Shared visual settings are unavailable"
    );
    QString lastErrorOperation_;
    QString lastErrorName_;
    QString lastErrorMessage_;
    QString busyOperation_;
    bool advertisedAvailable_ = false;
    bool writable_ = false;
    bool available_ = false;
    bool catalogAvailable_ = false;
    bool actionCatalogAvailable_ = false;
    bool completeSnapshotValid_ = false;
    bool displayDiscoveryAvailable_ = false;
    bool inputDeviceDiscoveryAvailable_ = false;
    bool inputDeviceDiscoveryBusy_ = false;
    bool busy_ = false;
    bool refreshQueued_ = false;
    bool displayConfirmationOwned_ = false;
    quint64 catalogOwnerGeneration_ = 0;
    quint64 actionCatalogOwnerGeneration_ = 0;
};

} // namespace HyprShelld
