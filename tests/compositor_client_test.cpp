#include "compositor_client.h"

#include "hyprland/catalog.h"

#include <QDBusConnection>
#include <QDBusContext>
#include <QDBusMessage>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSignalSpy>
#include <QTimer>
#include <QtTest>

#include <limits>
#include <utility>

namespace {

const QString busName = QStringLiteral("org.hyprshelld.Compositor1");
const QString objectPath = QStringLiteral("/org/hyprshelld/Compositor1");
const QString interfaceName = QStringLiteral("org.hyprshelld.Compositor1");
const QString propertiesInterface = QStringLiteral(
    "org.freedesktop.DBus.Properties"
);
const QString catalogDigest = QString::fromLatin1(
    HyprShelld::Hyprland::reviewedCatalogDigest
);
const QString actionCatalogDigest = QStringLiteral(
    "72e063a5476308cefdd2771367ffeed9a8e553b3a2d7141f4e08c3e105a5deb2"
);
const QString generationDigest(64, QLatin1Char('c'));
const QString previewGeneration(64, QLatin1Char('d'));
const QString topologyDigest(64, QLatin1Char('e'));
const QString confirmationToken(32, QLatin1Char('f'));
constexpr qulonglong baselineRevision = 7;
constexpr qulonglong previewRevision = 8;
constexpr qulonglong previewDeadlineMs = 4'102'444'800'000ULL;
constexpr qulonglong initialSharedBorderSourceRevision = 17;

QByteArray readBytes(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) return {};
    return file.readAll();
}

QByteArray optionCatalogBytes()
{
    const auto document = QJsonDocument::fromJson(
        readBytes(QStringLiteral(HYPRSHELLD_HYPRLAND_CATALOG_FILE))
    );
    return document.toJson(QJsonDocument::Compact);
}

QByteArray snapshotBytes(const qulonglong revision = baselineRevision)
{
    auto object = QJsonDocument::fromJson(
        readBytes(QStringLiteral(HYPRSHELLD_HYPRLAND_DEFAULTS_FILE))
    ).object();
    object.insert(QStringLiteral("revision"), QString::number(revision));
    object.insert(QStringLiteral("catalogDigest"), catalogDigest);
    object.insert(
        QStringLiteral("actionCatalogDigest"), actionCatalogDigest
    );
    auto bytes = QJsonDocument(object).toJson(QJsonDocument::Compact);
    bytes.append('\n');
    return bytes;
}

QByteArray topologyBytes()
{
    auto bytes = QJsonDocument(QJsonObject{
        {QStringLiteral("formatVersion"), 1},
        {QStringLiteral("topologyDigest"), topologyDigest},
        {
            QStringLiteral("outputs"),
            QJsonArray{
                QJsonObject{
                    {QStringLiteral("selector"), QStringLiteral("DP-1")},
                    {QStringLiteral("enabled"), true},
                    {QStringLiteral("modes"), QJsonArray{}},
                },
            }
        },
    }).toJson(QJsonDocument::Compact);
    bytes.append('\n');
    return bytes;
}

QVariantList validSnapshotReply()
{
    return {
        snapshotBytes(),
        QVariant::fromValue<qulonglong>(baselineRevision),
        catalogDigest,
        actionCatalogDigest,
    };
}

QVariantList validPreviewReply()
{
    return {
        QVariant::fromValue<qulonglong>(previewRevision),
        confirmationToken,
        QVariant::fromValue<qulonglong>(previewDeadlineMs),
        previewGeneration,
    };
}

class FakeCompositor final : public QObject, protected QDBusContext {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.hyprshelld.Compositor1")
    Q_PROPERTY(bool Available READ available)
    Q_PROPERTY(bool Writable READ writable)
    Q_PROPERTY(qulonglong Revision READ revision)
    Q_PROPERTY(QString LoadState READ loadState)
    Q_PROPERTY(QString ManagementState READ managementState)
    Q_PROPERTY(QString EntrypointDigest READ entrypointDigest)
    Q_PROPERTY(QString CatalogDigest READ currentCatalogDigest)
    Q_PROPERTY(QString ActionCatalogDigest READ currentActionCatalogDigest)
    Q_PROPERTY(qulonglong AppliedRevision READ appliedRevision)
    Q_PROPERTY(QString ApplyState READ applyState)
    Q_PROPERTY(QString RequiredActivation READ requiredActivation)
    Q_PROPERTY(QString GenerationDigest READ currentGenerationDigest)
    Q_PROPERTY(QString DisplayConfirmationState READ confirmationState)
    Q_PROPERTY(qulonglong DisplayConfirmationRevision READ confirmationRevision)
    Q_PROPERTY(qulonglong DisplayConfirmationDeadlineMs READ confirmationDeadlineMs)
    Q_PROPERTY(QString DisplayConfirmationGeneration READ confirmationGeneration)
    Q_PROPERTY(QString SharedBorderSyncState READ sharedBorderSyncState)
    Q_PROPERTY(qulonglong SharedBorderSourceRevision READ sharedBorderSourceRevision)
    Q_PROPERTY(QString SharedBorderSyncError READ sharedBorderSyncError)

public:
    enum class PendingBehavior {
        Success,
        NoDisplayConfirmation,
        UnexpectedError,
    };

    explicit FakeCompositor(QDBusConnection connection)
        : connection_(std::move(connection))
    {
        reset();
    }

    [[nodiscard]] bool available() const { return true; }
    [[nodiscard]] bool writable() const { return true; }
    [[nodiscard]] qulonglong revision() const { return authorityRevision_; }
    [[nodiscard]] QString loadState() const
    {
        return QStringLiteral("normal");
    }
    [[nodiscard]] QString managementState() const
    {
        return managementState_;
    }
    [[nodiscard]] QString entrypointDigest() const
    {
        return QString(64, QLatin1Char('9'));
    }
    [[nodiscard]] QString currentCatalogDigest() const
    {
        return catalogDigest;
    }
    [[nodiscard]] QString currentActionCatalogDigest() const
    {
        return actionCatalogDigest;
    }
    [[nodiscard]] qulonglong appliedRevision() const
    {
        return appliedRevision_;
    }
    [[nodiscard]] QString applyState() const
    {
        return applyState_;
    }
    [[nodiscard]] QString requiredActivation() const
    {
        return requiredActivation_;
    }
    [[nodiscard]] QString currentGenerationDigest() const
    {
        return authorityGenerationDigest_;
    }
    [[nodiscard]] QString confirmationState() const
    {
        return confirmationState_;
    }
    [[nodiscard]] qulonglong confirmationRevision() const
    {
        return confirmationRevision_;
    }
    [[nodiscard]] qulonglong confirmationDeadlineMs() const
    {
        return confirmationDeadlineMs_;
    }
    [[nodiscard]] QString confirmationGeneration() const
    {
        return confirmationGeneration_;
    }
    [[nodiscard]] QString sharedBorderSyncState() const
    {
        return sharedBorderSyncState_;
    }
    [[nodiscard]] qulonglong sharedBorderSourceRevision() const
    {
        return sharedBorderSourceRevision_;
    }
    [[nodiscard]] QString sharedBorderSyncError() const
    {
        return sharedBorderSyncError_;
    }
    [[nodiscard]] qsizetype pendingCallCount() const
    {
        return pendingCallCount_;
    }
    [[nodiscard]] qsizetype heldSnapshotCount() const
    {
        return heldSnapshots_.size();
    }
    [[nodiscard]] int snapshotCallCount() const { return snapshotCallCount_; }
    [[nodiscard]] qsizetype heldPreviewCount() const
    {
        return heldPreviews_.size();
    }

    bool start()
    {
        if (running_) return true;
        running_ = connection_.registerService(busName);
        return running_;
    }

    void stop()
    {
        if (!running_) return;
        connection_.unregisterService(busName);
        running_ = false;
    }

    void reset()
    {
        running_ = false;
        managementState_ = QStringLiteral("managed");
        confirmationState_ = QStringLiteral("idle");
        confirmationRevision_ = 0;
        confirmationDeadlineMs_ = 0;
        confirmationGeneration_.clear();
        sharedBorderSyncState_ = QStringLiteral("current");
        sharedBorderSourceRevision_ = initialSharedBorderSourceRevision;
        sharedBorderSyncError_.clear();
        retrySharedBorderSyncCallCount_ = 0;
        retrySharedBorderSyncErrorName_.clear();
        retrySharedBorderSyncErrorMessage_.clear();
        pendingBehavior_ = PendingBehavior::NoDisplayConfirmation;
        pendingCallCount_ = 0;
        holdSnapshots_ = false;
        heldSnapshots_.clear();
        snapshotCallCount_ = 0;
        holdPreviews_ = false;
        heldPreviews_.clear();
        previewReply_ = validPreviewReply();
        projectPreviewReply_ = false;
        authorityRevision_ = baselineRevision;
        appliedRevision_ = baselineRevision;
        applyState_ = QStringLiteral("current");
        requiredActivation_ = QStringLiteral("none");
        authorityGenerationDigest_ = generationDigest;
        snapshotBytes_ = snapshotBytes(baselineRevision);
        replaceCallCount_ = 0;
        applyCallCount_ = 0;
        recoverCallCount_ = 0;
        ambiguousFirstReplace_ = false;
        failNextApply_ = false;
        ambiguousFirstApplyWithoutProperties_ = false;
        ambiguousFirstApplyWithProperties_ = false;
        ambiguousRecover_ = false;
        holdReplaces_ = false;
        heldReplaces_.clear();
        optionCatalog_ = optionCatalogBytes();
        optionCatalogDigest_ = catalogDigest;
        connectedDisplaysFail_ = false;
    }

    void setPendingBehavior(const PendingBehavior behavior)
    {
        pendingBehavior_ = behavior;
    }

    void setAwaitingConfirmation(const bool publish = false)
    {
        managementState_ = QStringLiteral("preview");
        confirmationState_ = QStringLiteral("awaiting-confirmation");
        confirmationRevision_ = previewRevision;
        confirmationDeadlineMs_ = previewDeadlineMs;
        confirmationGeneration_ = previewGeneration;
        if (publish) publishConfirmation();
    }

    void replaceAwaitingConfirmationWithForeignTuple()
    {
        confirmationDeadlineMs_ = previewDeadlineMs + 1000;
        confirmationGeneration_ = QString(64, QLatin1Char('1'));
        publishConfirmation();
    }

    void setHoldSnapshots(const bool hold)
    {
        holdSnapshots_ = hold;
    }

    void setOptionCatalogReply(QByteArray bytes, QString digest)
    {
        optionCatalog_ = std::move(bytes);
        optionCatalogDigest_ = std::move(digest);
    }

    void setConnectedDisplaysFail(const bool fail)
    {
        connectedDisplaysFail_ = fail;
    }

    void setRevision(const qulonglong revision)
    {
        authorityRevision_ = revision;
        appliedRevision_ = revision;
        snapshotBytes_ = snapshotBytes(revision);
    }

    void setSnapshotBytes(QByteArray bytes)
    {
        snapshotBytes_ = std::move(bytes);
    }

    void setAmbiguousFirstReplace(const bool enabled)
    {
        ambiguousFirstReplace_ = enabled;
    }

    void setFailNextApply(const bool enabled)
    {
        failNextApply_ = enabled;
    }

    void setAmbiguousFirstApplyWithoutProperties(const bool enabled)
    {
        ambiguousFirstApplyWithoutProperties_ = enabled;
    }

    void setAmbiguousFirstApplyWithProperties(const bool enabled)
    {
        ambiguousFirstApplyWithProperties_ = enabled;
    }

    void setHoldReplaces(const bool hold)
    {
        holdReplaces_ = hold;
    }

    void setAmbiguousRecover(const bool enabled)
    {
        ambiguousRecover_ = enabled;
    }

    bool setSharedBorderSync(
        QString state,
        const qulonglong sourceRevision,
        QString error,
        const bool publish = false
    )
    {
        sharedBorderSyncState_ = std::move(state);
        sharedBorderSourceRevision_ = sourceRevision;
        sharedBorderSyncError_ = std::move(error);
        return !publish || publishSharedBorderProperties({
            {QStringLiteral("SharedBorderSyncState"), sharedBorderSyncState_},
            {
                QStringLiteral("SharedBorderSourceRevision"),
                QVariant::fromValue<qulonglong>(sharedBorderSourceRevision_)
            },
            {QStringLiteral("SharedBorderSyncError"), sharedBorderSyncError_},
        });
    }

    bool publishSharedBorderProperties(const QVariantMap &changed)
    {
        auto signal = QDBusMessage::createSignal(
            objectPath,
            propertiesInterface,
            QStringLiteral("PropertiesChanged")
        );
        signal.setArguments({interfaceName, changed, QStringList{}});
        return connection_.send(signal);
    }

    void setRetrySharedBorderSyncError(QString name, QString message)
    {
        retrySharedBorderSyncErrorName_ = std::move(name);
        retrySharedBorderSyncErrorMessage_ = std::move(message);
    }

    [[nodiscard]] int replaceCallCount() const { return replaceCallCount_; }
    [[nodiscard]] int applyCallCount() const { return applyCallCount_; }
    [[nodiscard]] int recoverCallCount() const { return recoverCallCount_; }
    [[nodiscard]] int retrySharedBorderSyncCallCount() const
    {
        return retrySharedBorderSyncCallCount_;
    }
    [[nodiscard]] qsizetype heldReplaceCount() const
    {
        return heldReplaces_.size();
    }

    bool releaseNextSnapshot(
        QVariantList reply = {},
        const bool duplicate = false
    )
    {
        if (heldSnapshots_.isEmpty()) return false;
        const auto call = heldSnapshots_.takeFirst();
        if (reply.isEmpty()) reply = validSnapshotReply();
        const auto response = call.createReply(reply);
        const auto sent = connection_.send(response);
        if (duplicate) connection_.send(response);
        return sent;
    }

    void setPreviewReply(QVariantList reply, const bool project)
    {
        previewReply_ = std::move(reply);
        projectPreviewReply_ = project;
    }

    void setHoldPreviews(const bool hold)
    {
        holdPreviews_ = hold;
    }

    bool releaseNextPreview(const bool duplicate = false)
    {
        if (heldPreviews_.isEmpty()) return false;
        const auto held = heldPreviews_.takeFirst();
        if (held.project) projectPreview(held.reply);
        const auto response = held.call.createReply(held.reply);
        const auto sent = connection_.send(response);
        if (duplicate) connection_.send(response);
        return sent;
    }

public slots:
    QByteArray GetOptionCatalog(QString &replyCatalogDigest)
    {
        replyCatalogDigest = optionCatalogDigest_;
        return optionCatalog_;
    }

    QByteArray GetSnapshot(
        qulonglong &snapshotRevision,
        QString &snapshotCatalogDigest,
        QString &snapshotActionCatalogDigest
    )
    {
        ++snapshotCallCount_;
        if (holdSnapshots_ && calledFromDBus()) {
            setDelayedReply(true);
            heldSnapshots_.append(message());
            return {};
        }
        snapshotRevision = authorityRevision_;
        snapshotCatalogDigest = catalogDigest;
        snapshotActionCatalogDigest = actionCatalogDigest;
        return snapshotBytes_;
    }

    QByteArray GetConnectedDisplays(qulonglong &observedAtMs)
    {
        if (connectedDisplaysFail_) {
            sendErrorReply(
                QStringLiteral("org.hyprshelld.Compositor1.Error.RuntimeUnavailable"),
                QStringLiteral("Injected display discovery failure")
            );
            return {};
        }
        observedAtMs = 1'800'000'000'000ULL;
        return topologyBytes();
    }

    QString GetPendingDisplayConfirmation(
        qulonglong &pendingRevision,
        qulonglong &pendingDeadlineMs,
        QString &pendingGeneration
    )
    {
        ++pendingCallCount_;
        if (pendingBehavior_ == PendingBehavior::NoDisplayConfirmation) {
            sendErrorReply(
                QStringLiteral(
                    "org.hyprshelld.Compositor1.Error.NoDisplayConfirmation"
                ),
                QStringLiteral("No display confirmation belongs to this caller")
            );
            return {};
        }
        if (pendingBehavior_ == PendingBehavior::UnexpectedError) {
            sendErrorReply(
                QStringLiteral("org.freedesktop.DBus.Error.UnknownMethod"),
                QStringLiteral("Injected pending-confirmation failure")
            );
            return {};
        }
        pendingRevision = confirmationRevision_;
        pendingDeadlineMs = confirmationDeadlineMs_;
        pendingGeneration = confirmationGeneration_;
        return confirmationToken;
    }

    qulonglong ReplaceSnapshot(
        const qulonglong expectedRevision,
        const QString &expectedCatalogDigest,
        const QString &expectedActionCatalogDigest,
        const QByteArray &candidateSnapshot
    )
    {
        ++replaceCallCount_;
        if (holdReplaces_ && calledFromDBus()) {
            setDelayedReply(true);
            heldReplaces_.append(message());
            return authorityRevision_;
        }
        if (expectedCatalogDigest != catalogDigest
            || expectedActionCatalogDigest != actionCatalogDigest) {
            sendErrorReply(
                QStringLiteral("org.hyprshelld.Compositor1.Error.StaleCatalogDigest"),
                QStringLiteral("Injected stale catalog")
            );
            return authorityRevision_;
        }
        auto object = QJsonDocument::fromJson(candidateSnapshot).object();
        if (object.isEmpty()) {
            sendErrorReply(
                QStringLiteral("org.hyprshelld.Compositor1.Error.InvalidSnapshot"),
                QStringLiteral("Injected invalid candidate")
            );
            return authorityRevision_;
        }
        object.insert(
            QStringLiteral("revision"),
            QString::number(authorityRevision_)
        );
        auto exactCurrent = QJsonDocument(object).toJson(
            QJsonDocument::Compact
        );
        exactCurrent.append('\n');
        if (expectedRevision + 1 == authorityRevision_
            && exactCurrent == snapshotBytes_) {
            publishAuthority();
            return authorityRevision_;
        }
        if (expectedRevision != authorityRevision_) {
            sendErrorReply(
                QStringLiteral("org.hyprshelld.Compositor1.Error.StaleRevision"),
                QStringLiteral("Injected stale revision")
            );
            return authorityRevision_;
        }
        object.insert(
            QStringLiteral("revision"),
            QString::number(authorityRevision_ + 1)
        );
        snapshotBytes_ = QJsonDocument(object).toJson(QJsonDocument::Compact);
        snapshotBytes_.append('\n');
        ++authorityRevision_;
        applyState_ = QStringLiteral("retained");
        requiredActivation_ = QStringLiteral("reload");
        publishAuthority();
        if (ambiguousFirstReplace_ && replaceCallCount_ == 1) {
            sendErrorReply(
                QStringLiteral("org.freedesktop.DBus.Error.NoReply"),
                QStringLiteral("Injected lost replacement reply")
            );
        }
        return authorityRevision_;
    }

    qulonglong Apply(
        const qulonglong expectedRevision,
        const QString &expectedCatalogDigest,
        const QString &expectedActionCatalogDigest,
        QString &appliedGenerationDigest
    )
    {
        ++applyCallCount_;
        appliedGenerationDigest = authorityGenerationDigest_;
        if (expectedRevision != authorityRevision_
            || expectedCatalogDigest != catalogDigest
            || expectedActionCatalogDigest != actionCatalogDigest) {
            sendErrorReply(
                QStringLiteral("org.hyprshelld.Compositor1.Error.StaleRevision"),
                QStringLiteral("Injected stale apply")
            );
            return appliedRevision_;
        }
        if (failNextApply_) {
            failNextApply_ = false;
            sendErrorReply(
                QStringLiteral("org.hyprshelld.Compositor1.Error.ApplyFailed"),
                QStringLiteral("Injected apply failure")
            );
            return appliedRevision_;
        }
        appliedRevision_ = authorityRevision_;
        applyState_ = QStringLiteral("current");
        requiredActivation_ = QStringLiteral("none");
        authorityGenerationDigest_ = QString(64, QLatin1Char('8'));
        appliedGenerationDigest = authorityGenerationDigest_;
        if (ambiguousFirstApplyWithProperties_ && applyCallCount_ == 1) {
            publishAuthority();
            sendErrorReply(
                QStringLiteral("org.freedesktop.DBus.Error.NoReply"),
                QStringLiteral("Injected lost apply reply after properties")
            );
            return appliedRevision_;
        }
        if (ambiguousFirstApplyWithoutProperties_ && applyCallCount_ == 1) {
            sendErrorReply(
                QStringLiteral("org.freedesktop.DBus.Error.NoReply"),
                QStringLiteral("Injected lost apply reply")
            );
            return appliedRevision_;
        }
        publishAuthority();
        return appliedRevision_;
    }

    qulonglong Recover(
        const qulonglong expectedRevision,
        const QString &expectedCatalogDigest,
        const QString &expectedActionCatalogDigest,
        qulonglong &recoveredAppliedRevision,
        QString &recoveredGenerationDigest
    )
    {
        ++recoverCallCount_;
        if (expectedRevision != authorityRevision_
            || expectedCatalogDigest != catalogDigest
            || expectedActionCatalogDigest != actionCatalogDigest
            || appliedRevision_ == authorityRevision_) {
            sendErrorReply(
                QStringLiteral("org.hyprshelld.Compositor1.Error.RecoveryUnavailable"),
                QStringLiteral("Injected recovery unavailable")
            );
            recoveredAppliedRevision = appliedRevision_;
            recoveredGenerationDigest = authorityGenerationDigest_;
            return authorityRevision_;
        }
        ++authorityRevision_;
        appliedRevision_ = authorityRevision_;
        snapshotBytes_ = snapshotBytes(authorityRevision_);
        applyState_ = QStringLiteral("current");
        requiredActivation_ = QStringLiteral("none");
        authorityGenerationDigest_ = QString(64, QLatin1Char('7'));
        recoveredAppliedRevision = appliedRevision_;
        recoveredGenerationDigest = authorityGenerationDigest_;
        if (ambiguousRecover_) {
            sendErrorReply(
                QStringLiteral("org.freedesktop.DBus.Error.NoReply"),
                QStringLiteral("Injected lost recovery reply")
            );
            return authorityRevision_;
        }
        publishAuthority();
        return authorityRevision_;
    }

    qulonglong PreviewDisplayConfiguration(
        qulonglong expectedRevision,
        const QString &expectedCatalogDigest,
        const QString &expectedActionCatalogDigest,
        const QByteArray &profile,
        uint timeoutSeconds,
        QString &token,
        qulonglong &deadlineMs,
        QString &previewGenerationDigest
    )
    {
        Q_UNUSED(expectedRevision)
        Q_UNUSED(expectedCatalogDigest)
        Q_UNUSED(expectedActionCatalogDigest)
        Q_UNUSED(profile)
        Q_UNUSED(timeoutSeconds)
        Q_UNUSED(token)
        Q_UNUSED(deadlineMs)
        Q_UNUSED(previewGenerationDigest)
        if (!calledFromDBus()) return 0;
        setDelayedReply(true);
        heldPreviews_.append({
            .call = message(),
            .reply = previewReply_,
            .project = projectPreviewReply_,
        });
        if (!holdPreviews_) {
            QTimer::singleShot(0, this, [this] {
                releaseNextPreview();
            });
        }
        return 0;
    }

    qulonglong ConfirmDisplayConfiguration(
        const QString &token,
        QString &confirmedGeneration
    )
    {
        Q_UNUSED(token)
        Q_UNUSED(confirmedGeneration)
        if (!calledFromDBus()) return 0;
        setDelayedReply(true);
        const auto call = message();
        QTimer::singleShot(0, this, [this, call] {
            managementState_ = QStringLiteral("preview");
            confirmationState_ = QStringLiteral("committing");
            clearConfirmationTuple();
            publishConfirmation();
            managementState_ = QStringLiteral("managed");
            confirmationState_ = QStringLiteral("idle");
            publishConfirmation();
            connection_.send(call.createReply({
                QVariant::fromValue<qulonglong>(previewRevision),
                previewGeneration,
            }));
        });
        return 0;
    }

    qulonglong RevertDisplayConfiguration(const QString &token)
    {
        Q_UNUSED(token)
        if (!calledFromDBus()) return 0;
        setDelayedReply(true);
        const auto call = message();
        QTimer::singleShot(0, this, [this, call] {
            managementState_ = QStringLiteral("preview");
            confirmationState_ = QStringLiteral("reverting");
            clearConfirmationTuple();
            publishConfirmation();
            managementState_ = QStringLiteral("managed");
            confirmationState_ = QStringLiteral("idle");
            publishConfirmation();
            connection_.send(call.createReply({
                QVariant::fromValue<qulonglong>(baselineRevision),
            }));
        });
        return 0;
    }

    void RetrySharedBorderSync()
    {
        ++retrySharedBorderSyncCallCount_;
        if (retrySharedBorderSyncErrorName_.isEmpty()) return;
        const auto errorName = std::exchange(
            retrySharedBorderSyncErrorName_,
            {}
        );
        const auto errorMessage = std::exchange(
            retrySharedBorderSyncErrorMessage_,
            {}
        );
        sendErrorReply(errorName, errorMessage);
    }

private:
    struct HeldPreview final {
        QDBusMessage call;
        QVariantList reply;
        bool project = false;
    };

    void projectPreview(const QVariantList &reply)
    {
        if (reply.size() != 4) return;
        managementState_ = QStringLiteral("preview");
        confirmationState_ = QStringLiteral("awaiting-confirmation");
        confirmationRevision_ = reply.at(0).toULongLong();
        confirmationDeadlineMs_ = reply.at(2).toULongLong();
        confirmationGeneration_ = reply.at(3).toString();
    }

    void clearConfirmationTuple()
    {
        confirmationRevision_ = 0;
        confirmationDeadlineMs_ = 0;
        confirmationGeneration_.clear();
    }

    bool publishAuthority()
    {
        auto signal = QDBusMessage::createSignal(
            objectPath,
            propertiesInterface,
            QStringLiteral("PropertiesChanged")
        );
        signal.setArguments({
            interfaceName,
            QVariantMap{
                {
                    QStringLiteral("Revision"),
                    QVariant::fromValue<qulonglong>(authorityRevision_)
                },
                {
                    QStringLiteral("AppliedRevision"),
                    QVariant::fromValue<qulonglong>(appliedRevision_)
                },
                {QStringLiteral("ApplyState"), applyState_},
                {QStringLiteral("RequiredActivation"), requiredActivation_},
                {
                    QStringLiteral("GenerationDigest"),
                    authorityGenerationDigest_
                },
            },
            QStringList{},
        });
        return connection_.send(signal);
    }

    bool publishConfirmation()
    {
        auto signal = QDBusMessage::createSignal(
            objectPath,
            propertiesInterface,
            QStringLiteral("PropertiesChanged")
        );
        signal.setArguments({
            interfaceName,
            QVariantMap{
                {QStringLiteral("ManagementState"), managementState_},
                {QStringLiteral("DisplayConfirmationState"), confirmationState_},
                {
                    QStringLiteral("DisplayConfirmationRevision"),
                    QVariant::fromValue<qulonglong>(confirmationRevision_)
                },
                {
                    QStringLiteral("DisplayConfirmationDeadlineMs"),
                    QVariant::fromValue<qulonglong>(confirmationDeadlineMs_)
                },
                {
                    QStringLiteral("DisplayConfirmationGeneration"),
                    confirmationGeneration_
                },
            },
            QStringList{},
        });
        return connection_.send(signal);
    }

    QDBusConnection connection_;
    QString managementState_;
    QString confirmationState_;
    qulonglong confirmationRevision_ = 0;
    qulonglong confirmationDeadlineMs_ = 0;
    QString confirmationGeneration_;
    QString sharedBorderSyncState_;
    qulonglong sharedBorderSourceRevision_ = 0;
    QString sharedBorderSyncError_;
    int retrySharedBorderSyncCallCount_ = 0;
    QString retrySharedBorderSyncErrorName_;
    QString retrySharedBorderSyncErrorMessage_;
    PendingBehavior pendingBehavior_ = PendingBehavior::NoDisplayConfirmation;
    qsizetype pendingCallCount_ = 0;
    bool holdSnapshots_ = false;
    QList<QDBusMessage> heldSnapshots_;
    int snapshotCallCount_ = 0;
    bool holdPreviews_ = false;
    QList<HeldPreview> heldPreviews_;
    QVariantList previewReply_;
    bool projectPreviewReply_ = false;
    QByteArray optionCatalog_;
    QString optionCatalogDigest_;
    bool connectedDisplaysFail_ = false;
    bool running_ = false;
    qulonglong authorityRevision_ = baselineRevision;
    qulonglong appliedRevision_ = baselineRevision;
    QString applyState_ = QStringLiteral("current");
    QString requiredActivation_ = QStringLiteral("none");
    QString authorityGenerationDigest_ = generationDigest;
    QByteArray snapshotBytes_ = snapshotBytes();
    int replaceCallCount_ = 0;
    int applyCallCount_ = 0;
    int recoverCallCount_ = 0;
    bool ambiguousFirstReplace_ = false;
    bool failNextApply_ = false;
    bool ambiguousFirstApplyWithoutProperties_ = false;
    bool ambiguousFirstApplyWithProperties_ = false;
    bool ambiguousRecover_ = false;
    bool holdReplaces_ = false;
    QList<QDBusMessage> heldReplaces_;
};

} // namespace

class CompositorClientTest final : public QObject {
    Q_OBJECT

private slots:
    void initTestCase()
    {
        QVERIFY2(bus_.isConnected(), qPrintable(bus_.lastError().message()));
        QVERIFY2(
            serviceBus_.isConnected(),
            qPrintable(serviceBus_.lastError().message())
        );
        QVERIFY(serviceBus_.registerObject(
            objectPath,
            &service_,
            QDBusConnection::ExportAllProperties
                | QDBusConnection::ExportAllSlots
        ));
    }

    void cleanup()
    {
        service_.stop();
        serviceBus_.unregisterService(busName);
        service_.reset();
        QCoreApplication::processEvents();
    }

    void cleanupTestCase()
    {
        serviceBus_.unregisterObject(objectPath);
        QDBusConnection::disconnectFromBus(
            QStringLiteral("compositor-client-test")
        );
        QDBusConnection::disconnectFromBus(
            QStringLiteral("compositor-service-test")
        );
    }

    void hydratesTheTrustedAppearanceCatalogAndValues()
    {
        QVERIFY(service_.start());

        HyprShelld::CompositorClient client(bus_, nullptr);
        QTRY_VERIFY_WITH_TIMEOUT(client.available(), 3000);
        QTRY_VERIFY_WITH_TIMEOUT(client.catalogAvailable(), 3000);
        QTRY_VERIFY_WITH_TIMEOUT(client.displayDiscoveryAvailable(), 3000);
        QTRY_VERIFY_WITH_TIMEOUT(client.appearanceAvailable(), 3000);
        QCOMPARE(client.appearanceOptions().size(), 8);
        QCOMPARE(client.appearanceValues().size(), 8);
        QCOMPARE(
            client.appearanceValues().value(
                QStringLiteral("hyprland.general.border_size")
            ).toInt(),
            1
        );
        QCOMPARE(
            client.appearanceValues().value(
                QStringLiteral("hyprland.general.layout")
            ).toString(),
            QStringLiteral("dwindle")
        );
        QCOMPARE(
            client.sharedBorderSyncState(),
            QStringLiteral("current")
        );
        QCOMPARE(
            client.sharedBorderSourceRevision(),
            initialSharedBorderSourceRevision
        );
        QCOMPARE(
            client.sharedBorderSourceRevisionToken(),
            QStringLiteral("17")
        );
        QVERIFY(client.sharedBorderSyncError().isEmpty());
        QVERIFY(client.appearanceErrorName().isEmpty());
        QVERIFY(client.lastErrorName().isEmpty());
    }

    void projectsSharedBorderPropertiesWithoutRegressingAppearance()
    {
        QVERIFY(service_.start());
        HyprShelld::CompositorClient client(bus_, nullptr);
        QTRY_VERIFY_WITH_TIMEOUT(client.appearanceAvailable(), 3000);
        const auto appearanceValues = client.appearanceValues();
        QSignalSpy sharedBorderChanges(
            &client,
            &HyprShelld::CompositorClient::sharedBorderSyncChanged
        );
        QVERIFY(sharedBorderChanges.isValid());

        QVERIFY(service_.setSharedBorderSync(
            QStringLiteral("pending"),
            initialSharedBorderSourceRevision + 1,
            {},
            true
        ));
        QTRY_COMPARE_WITH_TIMEOUT(
            client.sharedBorderSyncState(),
            QStringLiteral("pending"),
            3000
        );
        QCOMPARE(
            client.sharedBorderSourceRevision(),
            initialSharedBorderSourceRevision + 1
        );
        QCOMPARE(
            client.sharedBorderSourceRevisionToken(),
            QStringLiteral("18")
        );
        QVERIFY(client.sharedBorderSyncError().isEmpty());
        QTRY_VERIFY_WITH_TIMEOUT(client.appearanceAvailable(), 3000);
        QCOMPARE(client.appearanceValues(), appearanceValues);

        QVERIFY(service_.setSharedBorderSync(
            QStringLiteral("failed"),
            initialSharedBorderSourceRevision + 1,
            QStringLiteral("Injected synchronization failure"),
            true
        ));
        QTRY_COMPARE_WITH_TIMEOUT(
            client.sharedBorderSyncState(),
            QStringLiteral("failed"),
            3000
        );
        QCOMPARE(
            client.sharedBorderSyncError(),
            QStringLiteral("Injected synchronization failure")
        );
        QTRY_COMPARE_WITH_TIMEOUT(sharedBorderChanges.size(), 2, 3000);
        QTRY_VERIFY_WITH_TIMEOUT(client.appearanceAvailable(), 3000);
        QCOMPARE(client.appearanceValues(), appearanceValues);
    }

    void rejectsMalformedSharedBorderProperties_data()
    {
        QTest::addColumn<QVariantMap>("changed");

        const auto validRevision = QVariant::fromValue<qulonglong>(
            initialSharedBorderSourceRevision
        );
        QTest::newRow("state-wrong-type") << QVariantMap{
            {QStringLiteral("SharedBorderSyncState"), 1U},
            {QStringLiteral("SharedBorderSourceRevision"), validRevision},
            {QStringLiteral("SharedBorderSyncError"), QString{}},
        };
        QTest::newRow("revision-wrong-type") << QVariantMap{
            {QStringLiteral("SharedBorderSyncState"), QStringLiteral("current")},
            {
                QStringLiteral("SharedBorderSourceRevision"),
                QStringLiteral("17")
            },
            {QStringLiteral("SharedBorderSyncError"), QString{}},
        };
        QTest::newRow("error-wrong-type") << QVariantMap{
            {QStringLiteral("SharedBorderSyncState"), QStringLiteral("current")},
            {QStringLiteral("SharedBorderSourceRevision"), validRevision},
            {QStringLiteral("SharedBorderSyncError"), false},
        };
        QTest::newRow("unknown-state") << QVariantMap{
            {QStringLiteral("SharedBorderSyncState"), QStringLiteral("retrying")},
            {QStringLiteral("SharedBorderSourceRevision"), validRevision},
            {QStringLiteral("SharedBorderSyncError"), QString{}},
        };
        QTest::newRow("current-with-error") << QVariantMap{
            {QStringLiteral("SharedBorderSyncState"), QStringLiteral("current")},
            {QStringLiteral("SharedBorderSourceRevision"), validRevision},
            {
                QStringLiteral("SharedBorderSyncError"),
                QStringLiteral("Unexpected error")
            },
        };
        QTest::newRow("failed-without-error") << QVariantMap{
            {QStringLiteral("SharedBorderSyncState"), QStringLiteral("failed")},
            {QStringLiteral("SharedBorderSourceRevision"), validRevision},
            {QStringLiteral("SharedBorderSyncError"), QString{}},
        };
    }

    void rejectsMalformedSharedBorderProperties()
    {
        QFETCH(QVariantMap, changed);

        QVERIFY(service_.start());
        HyprShelld::CompositorClient client(bus_, nullptr);
        QTRY_VERIFY_WITH_TIMEOUT(client.appearanceAvailable(), 3000);
        QCOMPARE(
            client.sharedBorderSyncState(),
            QStringLiteral("current")
        );

        QVERIFY(service_.publishSharedBorderProperties(changed));
        QTRY_VERIFY_WITH_TIMEOUT(!client.available(), 3000);
        QCOMPARE(
            client.sharedBorderSyncState(),
            QStringLiteral("current")
        );
        QCOMPARE(
            client.sharedBorderSourceRevision(),
            initialSharedBorderSourceRevision
        );
        QVERIFY(client.sharedBorderSyncError().isEmpty());
    }

    void ownerLossResetsSharedBorderProjection()
    {
        QVERIFY(service_.start());
        HyprShelld::CompositorClient client(bus_, nullptr);
        QTRY_VERIFY_WITH_TIMEOUT(client.available(), 3000);
        QCOMPARE(
            client.sharedBorderSyncState(),
            QStringLiteral("current")
        );
        QSignalSpy sharedBorderChanges(
            &client,
            &HyprShelld::CompositorClient::sharedBorderSyncChanged
        );
        QVERIFY(sharedBorderChanges.isValid());

        service_.stop();
        QTRY_VERIFY_WITH_TIMEOUT(!client.available(), 3000);
        QCOMPARE(
            client.sharedBorderSyncState(),
            QStringLiteral("unavailable")
        );
        QCOMPARE(client.sharedBorderSourceRevision(), 0ULL);
        QCOMPARE(
            client.sharedBorderSourceRevisionToken(),
            QStringLiteral("0")
        );
        QCOMPARE(
            client.sharedBorderSyncError(),
            QStringLiteral("Shared visual settings are unavailable")
        );
        QCOMPARE(sharedBorderChanges.size(), 1);
    }

    void retriesSharedBorderSynchronizationWithAnEmptyReplyAndCopiesErrors()
    {
        QVERIFY(service_.setSharedBorderSync(
            QStringLiteral("failed"),
            initialSharedBorderSourceRevision,
            QStringLiteral("Injected synchronization failure")
        ));
        QVERIFY(service_.start());
        HyprShelld::CompositorClient client(bus_, nullptr);
        QTRY_VERIFY_WITH_TIMEOUT(client.appearanceAvailable(), 3000);
        const auto appearanceValues = client.appearanceValues();
        QSignalSpy failures(
            &client,
            &HyprShelld::CompositorClient::operationFailed
        );
        QVERIFY(failures.isValid());

        client.retrySharedBorderSync();
        QVERIFY(client.busy());
        QCOMPARE(
            client.busyOperation(),
            QStringLiteral("shared-border-sync")
        );
        QTRY_VERIFY_WITH_TIMEOUT(!client.busy(), 3000);
        QCOMPARE(service_.retrySharedBorderSyncCallCount(), 1);
        QCOMPARE(failures.size(), 0);
        QVERIFY(client.lastErrorName().isEmpty());
        QTRY_VERIFY_WITH_TIMEOUT(client.appearanceAvailable(), 3000);
        QCOMPARE(client.appearanceValues(), appearanceValues);

        service_.setRetrySharedBorderSyncError(
            QStringLiteral("org.hyprshelld.Compositor1.Error.Unavailable"),
            QStringLiteral("Injected retry failure")
        );
        client.retrySharedBorderSync();
        QVERIFY(client.busy());
        QTRY_VERIFY_WITH_TIMEOUT(!client.busy(), 3000);
        QCOMPARE(service_.retrySharedBorderSyncCallCount(), 2);
        QTRY_COMPARE_WITH_TIMEOUT(failures.size(), 1, 3000);
        QCOMPARE(
            client.lastErrorName(),
            QStringLiteral("org.hyprshelld.Compositor1.Error.Unavailable")
        );
        QCOMPARE(
            client.lastErrorMessage(),
            QStringLiteral("Injected retry failure")
        );
        QTRY_VERIFY_WITH_TIMEOUT(client.appearanceAvailable(), 3000);
        QCOMPARE(client.appearanceValues(), appearanceValues);
        QCOMPARE(
            client.sharedBorderSyncState(),
            QStringLiteral("failed")
        );
    }

    void catalogFailureDisablesOnlyAppearance()
    {
        service_.setOptionCatalogReply(
            optionCatalogBytes(),
            QString(64, QLatin1Char('a'))
        );
        QVERIFY(service_.start());

        HyprShelld::CompositorClient client(bus_, nullptr);
        QTRY_VERIFY_WITH_TIMEOUT(client.available(), 3000);
        QTRY_VERIFY_WITH_TIMEOUT(client.displayDiscoveryAvailable(), 3000);
        QTRY_VERIFY_WITH_TIMEOUT(!client.appearanceErrorName().isEmpty(), 3000);
        QCOMPARE(client.catalogAvailable(), false);
        QCOMPARE(client.appearanceAvailable(), false);
        QVERIFY(client.lastErrorName().isEmpty());
    }

    void displayDiscoveryFailureDoesNotDisableAppearance()
    {
        service_.setConnectedDisplaysFail(true);
        QVERIFY(service_.start());

        HyprShelld::CompositorClient client(bus_, nullptr);
        QTRY_VERIFY_WITH_TIMEOUT(client.available(), 3000);
        QTRY_VERIFY_WITH_TIMEOUT(client.catalogAvailable(), 3000);
        QTRY_VERIFY_WITH_TIMEOUT(client.appearanceAvailable(), 3000);
        QCOMPARE(client.displayDiscoveryAvailable(), false);
        QVERIFY(client.lastErrorName().isEmpty());
    }

    void displayPreviewCannotUseStaleTopologyAfterDiscoveryFails()
    {
        QVERIFY(service_.start());
        HyprShelld::CompositorClient client(bus_, nullptr);
        QTRY_VERIFY_WITH_TIMEOUT(client.displayDiscoveryAvailable(), 3000);
        QCOMPARE(client.connectedDisplays().size(), 1);
        QVERIFY(!client.topologyDigest().isEmpty());

        service_.setConnectedDisplaysFail(true);
        client.refresh();
        QTRY_VERIFY_WITH_TIMEOUT(client.available(), 3000);
        QTRY_VERIFY_WITH_TIMEOUT(!client.displayDiscoveryAvailable(), 3000);
        const QVariantList output{
            QVariantMap{
                {QStringLiteral("id"), QStringLiteral("display-DP-1")},
                {QStringLiteral("selector"), QStringLiteral("DP-1")},
                {QStringLiteral("enabled"), true},
            }
        };
        client.previewDisplayConfiguration(output, 15);
        QCOMPARE(client.busy(), false);
        QCOMPARE(service_.heldPreviewCount(), 0);
        QCOMPARE(
            client.lastErrorName(),
            QStringLiteral(
                "org.hyprshelld.Client.Compositor.Error.Unavailable"
            )
        );
    }

    void exposesAnExactRevisionTokenBeyondQmlIntegerPrecision()
    {
        constexpr qulonglong exactRevision = 9'007'199'254'740'993ULL;
        service_.setRevision(exactRevision);
        QVERIFY(service_.start());

        HyprShelld::CompositorClient client(bus_, nullptr);
        QTRY_VERIFY_WITH_TIMEOUT(client.available(), 3000);
        QCOMPARE(client.revision(), exactRevision);
        QCOMPARE(
            client.revisionToken(),
            QStringLiteral("9007199254740993")
        );
    }

    void exposesAnExactSharedBorderSourceRevisionTokenBeyondQmlIntegerPrecision()
    {
        constexpr qulonglong exactRevision = 9'007'199'254'740'993ULL;
        QVERIFY(service_.setSharedBorderSync(
            QStringLiteral("current"), exactRevision, {}
        ));
        QVERIFY(service_.start());

        HyprShelld::CompositorClient client(bus_, nullptr);
        QTRY_VERIFY_WITH_TIMEOUT(client.available(), 3000);
        QCOMPARE(client.sharedBorderSourceRevision(), exactRevision);
        QCOMPARE(
            client.sharedBorderSourceRevisionToken(),
            QStringLiteral("9007199254740993")
        );
    }

    void rejectsANonCanonicalSnapshotRevisionDuringHydration()
    {
        auto object = QJsonDocument::fromJson(snapshotBytes()).object();
        object.insert(QStringLiteral("revision"), QStringLiteral("07"));
        auto bytes = QJsonDocument(object).toJson(QJsonDocument::Compact);
        bytes.append('\n');
        service_.setSnapshotBytes(bytes);
        QVERIFY(service_.start());

        HyprShelld::CompositorClient client(bus_, nullptr);
        QTRY_COMPARE_WITH_TIMEOUT(service_.snapshotCallCount(), 1, 3000);
        QTest::qWait(20);
        QCOMPARE(client.available(), false);
        QCOMPARE(client.catalogAvailable(), false);
        QCOMPARE(client.appearanceAvailable(), false);
    }

    void rejectsAPartialV1SnapshotDuringHydration()
    {
        auto object = QJsonDocument::fromJson(snapshotBytes()).object();
        object.remove(QStringLiteral("devices"));
        auto bytes = QJsonDocument(object).toJson(QJsonDocument::Compact);
        bytes.append('\n');
        service_.setSnapshotBytes(bytes);
        QVERIFY(service_.start());

        HyprShelld::CompositorClient client(bus_, nullptr);
        QTRY_COMPARE_WITH_TIMEOUT(service_.snapshotCallCount(), 1, 3000);
        QTest::qWait(20);
        QCOMPARE(client.available(), false);
        QCOMPARE(client.catalogAvailable(), false);
        QCOMPARE(client.appearanceAvailable(), false);
    }

    void rejectsNonCanonicalSnapshotBytes_data()
    {
        QTest::addColumn<QByteArray>("bytes");
        const auto object = QJsonDocument::fromJson(snapshotBytes()).object();
        QTest::newRow("pretty-printed")
            << QJsonDocument(object).toJson(QJsonDocument::Indented);
        auto trailing = snapshotBytes();
        trailing.append(' ');
        QTest::newRow("trailing-byte") << trailing;
        auto duplicate = snapshotBytes();
        const QByteArray revision = QByteArrayLiteral("\"revision\":\"7\"");
        const auto position = duplicate.indexOf(revision);
        QVERIFY(position >= 0);
        duplicate.replace(
            position,
            revision.size(),
            QByteArrayLiteral("\"revision\":\"7\",\"revision\":\"7\"")
        );
        QTest::newRow("duplicate-key") << duplicate;
    }

    void rejectsNonCanonicalSnapshotBytes()
    {
        QFETCH(QByteArray, bytes);
        service_.setSnapshotBytes(bytes);
        QVERIFY(service_.start());

        HyprShelld::CompositorClient client(bus_, nullptr);
        QTRY_COMPARE_WITH_TIMEOUT(service_.snapshotCallCount(), 1, 3000);
        QTest::qWait(20);
        QCOMPARE(client.available(), false);
        QCOMPARE(client.catalogAvailable(), false);
        QCOMPARE(client.appearanceAvailable(), false);
    }

    void revisionExhaustionDisablesNewAppearanceAndRecoveryMutations()
    {
        service_.setRevision(std::numeric_limits<qulonglong>::max());
        QVERIFY(service_.start());

        HyprShelld::CompositorClient client(bus_, nullptr);
        QTRY_VERIFY_WITH_TIMEOUT(client.available(), 3000);
        QTRY_VERIFY_WITH_TIMEOUT(client.catalogAvailable(), 3000);
        QCOMPARE(client.appearanceAvailable(), false);
        QCOMPARE(client.recoveryAvailable(), false);
        QCOMPARE(
            client.revisionToken(),
            QStringLiteral("18446744073709551615")
        );
    }

    void savesAndAppliesAppearanceAcrossPropertiesBeforeReplies()
    {
        QVERIFY(service_.start());
        HyprShelld::CompositorClient client(bus_, nullptr);
        QTRY_VERIFY_WITH_TIMEOUT(client.appearanceAvailable(), 3000);
        auto values = client.appearanceValues();
        values.insert(QStringLiteral("hyprland.general.border_size"), 6);
        QStringList observedOperations;
        connect(
            &client,
            &HyprShelld::CompositorClient::busyOperationChanged,
            this,
            [&client, &observedOperations] {
                observedOperations.append(client.busyOperation());
            }
        );

        client.saveAppearance(values);
        QTRY_COMPARE_WITH_TIMEOUT(client.revision(), 8ULL, 3000);
        QTRY_VERIFY_WITH_TIMEOUT(!client.busy(), 3000);
        QTRY_VERIFY_WITH_TIMEOUT(client.appearanceAvailable(), 3000);
        QCOMPARE(client.appliedRevision(), 8ULL);
        QCOMPARE(client.applyState(), QStringLiteral("current"));
        QCOMPARE(client.requiredActivation(), QStringLiteral("none"));
        QCOMPARE(
            client.appearanceValues().value(
                QStringLiteral("hyprland.general.border_size")
            ).toInt(),
            6
        );
        QCOMPARE(service_.replaceCallCount(), 1);
        QCOMPARE(service_.applyCallCount(), 1);
        QVERIFY(observedOperations.contains(QStringLiteral("appearance-save")));
        QVERIFY(observedOperations.contains(QStringLiteral("appearance-apply")));
        QCOMPARE(client.busyOperation(), QString{});
        QVERIFY(client.lastErrorName().isEmpty());
    }

    void retriesAnExactLostReplaceWithoutIncrementingTwice()
    {
        service_.setAmbiguousFirstReplace(true);
        QVERIFY(service_.start());
        HyprShelld::CompositorClient client(bus_, nullptr);
        QTRY_VERIFY_WITH_TIMEOUT(client.appearanceAvailable(), 3000);
        auto values = client.appearanceValues();
        values.insert(QStringLiteral("hyprland.decoration.rounding"), 9);

        client.saveAppearance(values);
        QTRY_VERIFY_WITH_TIMEOUT(!client.busy(), 3000);
        QTRY_COMPARE_WITH_TIMEOUT(client.revision(), 8ULL, 3000);
        QTRY_VERIFY_WITH_TIMEOUT(client.appearanceAvailable(), 3000);
        QCOMPARE(service_.replaceCallCount(), 2);
        QCOMPARE(service_.applyCallCount(), 1);
        QCOMPARE(client.appliedRevision(), 8ULL);
    }

    void retriesAnAmbiguousApplyOnlyForTheExactSavedRevision()
    {
        service_.setAmbiguousFirstApplyWithoutProperties(true);
        QVERIFY(service_.start());
        HyprShelld::CompositorClient client(bus_, nullptr);
        QTRY_VERIFY_WITH_TIMEOUT(client.appearanceAvailable(), 3000);
        auto values = client.appearanceValues();
        values.insert(
            QStringLiteral("hyprland.decoration.shadow.enabled"), false
        );

        client.saveAppearance(values);
        QTRY_VERIFY_WITH_TIMEOUT(!client.busy(), 3000);
        QTRY_VERIFY_WITH_TIMEOUT(client.appearanceAvailable(), 3000);
        QCOMPARE(client.revision(), 8ULL);
        QCOMPARE(client.appliedRevision(), 8ULL);
        QCOMPARE(service_.applyCallCount(), 2);
    }

    void acceptsExactCurrentPropertiesBeforeAnAmbiguousApplyReply()
    {
        service_.setAmbiguousFirstApplyWithProperties(true);
        QVERIFY(service_.start());
        HyprShelld::CompositorClient client(bus_, nullptr);
        QTRY_VERIFY_WITH_TIMEOUT(client.appearanceAvailable(), 3000);
        auto values = client.appearanceValues();
        values.insert(
            QStringLiteral("hyprland.decoration.blur.enabled"), false
        );

        client.saveAppearance(values);
        QTRY_VERIFY_WITH_TIMEOUT(!client.busy(), 3000);
        QTRY_VERIFY_WITH_TIMEOUT(client.appearanceAvailable(), 3000);
        QCOMPARE(client.revision(), 8ULL);
        QCOMPARE(client.appliedRevision(), 8ULL);
        QCOMPARE(service_.applyCallCount(), 1);
        QVERIFY(client.lastErrorName().isEmpty());
    }

    void retainsSavedAppearanceAfterApplyFailureAndRetriesExplicitly()
    {
        service_.setFailNextApply(true);
        QVERIFY(service_.start());
        HyprShelld::CompositorClient client(bus_, nullptr);
        QTRY_VERIFY_WITH_TIMEOUT(client.appearanceAvailable(), 3000);
        auto values = client.appearanceValues();
        values.insert(QStringLiteral("hyprland.animations.enabled"), false);

        client.saveAppearance(values);
        QTRY_VERIFY_WITH_TIMEOUT(!client.busy(), 3000);
        QTRY_COMPARE_WITH_TIMEOUT(client.revision(), 8ULL, 3000);
        QTRY_VERIFY_WITH_TIMEOUT(client.retryApplyAvailable(), 3000);
        QCOMPARE(client.appliedRevision(), baselineRevision);
        QCOMPARE(client.applyState(), QStringLiteral("retained"));
        QCOMPARE(client.requiredActivation(), QStringLiteral("reload"));
        QCOMPARE(
            client.lastErrorName(),
            QStringLiteral("org.hyprshelld.Compositor1.Error.ApplyFailed")
        );
        QVERIFY(client.recoveryAvailable());

        client.retryApply();
        QTRY_VERIFY_WITH_TIMEOUT(!client.busy(), 3000);
        QTRY_VERIFY_WITH_TIMEOUT(client.appearanceAvailable(), 3000);
        QCOMPARE(client.appliedRevision(), 8ULL);
        QCOMPARE(service_.applyCallCount(), 2);
        QVERIFY(client.lastErrorName().isEmpty());
    }

    void recoversTheWholeLastWorkingConfigurationAsANewRevision()
    {
        service_.setFailNextApply(true);
        QVERIFY(service_.start());
        HyprShelld::CompositorClient client(bus_, nullptr);
        QTRY_VERIFY_WITH_TIMEOUT(client.appearanceAvailable(), 3000);
        auto values = client.appearanceValues();
        values.insert(QStringLiteral("hyprland.general.resize_on_border"), true);
        client.saveAppearance(values);
        QTRY_VERIFY_WITH_TIMEOUT(client.recoveryAvailable(), 3000);

        client.recoverConfiguration();
        QTRY_VERIFY_WITH_TIMEOUT(!client.busy(), 3000);
        QTRY_COMPARE_WITH_TIMEOUT(client.revision(), 9ULL, 3000);
        QTRY_VERIFY_WITH_TIMEOUT(client.appearanceAvailable(), 3000);
        QCOMPARE(client.appliedRevision(), 9ULL);
        QCOMPARE(service_.recoverCallCount(), 1);
        QCOMPARE(
            client.appearanceValues().value(
                QStringLiteral("hyprland.general.resize_on_border")
            ).toBool(),
            false
        );
    }

    void neverRetriesAnAmbiguousRecoveryReply()
    {
        service_.setFailNextApply(true);
        service_.setAmbiguousRecover(true);
        QVERIFY(service_.start());
        HyprShelld::CompositorClient client(bus_, nullptr);
        QTRY_VERIFY_WITH_TIMEOUT(client.appearanceAvailable(), 3000);
        auto values = client.appearanceValues();
        values.insert(QStringLiteral("hyprland.general.snap.enabled"), true);
        client.saveAppearance(values);
        QTRY_VERIFY_WITH_TIMEOUT(client.recoveryAvailable(), 3000);

        client.recoverConfiguration();
        QTRY_VERIFY_WITH_TIMEOUT(!client.busy(), 3000);
        QTRY_COMPARE_WITH_TIMEOUT(client.revision(), 9ULL, 3000);
        QTRY_VERIFY_WITH_TIMEOUT(client.available(), 3000);
        QCOMPARE(client.appliedRevision(), 9ULL);
        QCOMPARE(client.applyState(), QStringLiteral("current"));
        QCOMPARE(service_.recoverCallCount(), 1);
        QCOMPARE(
            client.lastErrorName(),
            QStringLiteral("org.freedesktop.DBus.Error.NoReply")
        );
    }

    void rejectsPartialAppearanceMapsWithoutCallingTheService()
    {
        QVERIFY(service_.start());
        HyprShelld::CompositorClient client(bus_, nullptr);
        QTRY_VERIFY_WITH_TIMEOUT(client.appearanceAvailable(), 3000);
        auto values = client.appearanceValues();
        values.remove(QStringLiteral("hyprland.animations.enabled"));

        client.saveAppearance(values);
        QCOMPARE(client.busy(), false);
        QCOMPARE(service_.replaceCallCount(), 0);
        QCOMPARE(
            client.lastErrorName(),
            QStringLiteral(
                "org.hyprshelld.Client.Compositor.Error.InvalidAppearance"
            )
        );
    }

    void ownerLossClearsAppearanceBusyStateAndOperation()
    {
        service_.setHoldReplaces(true);
        QVERIFY(service_.start());
        HyprShelld::CompositorClient client(bus_, nullptr);
        QTRY_VERIFY_WITH_TIMEOUT(client.appearanceAvailable(), 3000);
        auto values = client.appearanceValues();
        values.insert(QStringLiteral("hyprland.general.snap.enabled"), true);
        client.saveAppearance(values);
        QTRY_COMPARE_WITH_TIMEOUT(service_.heldReplaceCount(), 1, 3000);
        QCOMPARE(client.busy(), true);
        QCOMPARE(client.busyOperation(), QStringLiteral("appearance-save"));

        service_.stop();
        QTRY_VERIFY_WITH_TIMEOUT(!client.busy(), 3000);
        QCOMPARE(client.busyOperation(), QString{});
        QCOMPARE(client.available(), false);
    }

    void recoversOnlyTheCallingClientsPendingConfirmation()
    {
        service_.setPendingBehavior(FakeCompositor::PendingBehavior::Success);
        service_.setAwaitingConfirmation();
        QVERIFY(service_.start());

        HyprShelld::CompositorClient client(bus_, nullptr);
        QTRY_VERIFY_WITH_TIMEOUT(client.available(), 3000);
        QTRY_COMPARE_WITH_TIMEOUT(service_.pendingCallCount(), 1, 3000);
        QCOMPARE(
            client.displayConfirmationState(),
            QStringLiteral("awaiting-confirmation")
        );
        QCOMPARE(client.displayConfirmationRevision(), previewRevision);
        QCOMPARE(client.displayConfirmationDeadlineMs(), previewDeadlineMs);
        QCOMPARE(client.displayConfirmationGeneration(), previewGeneration);
        QCOMPARE(client.displayConfirmationOwned(), true);

        service_.setPendingBehavior(
            FakeCompositor::PendingBehavior::NoDisplayConfirmation
        );
        service_.replaceAwaitingConfirmationWithForeignTuple();
        QTRY_COMPARE_WITH_TIMEOUT(service_.pendingCallCount(), 2, 3000);
        QTRY_VERIFY_WITH_TIMEOUT(client.available(), 3000);
        QCOMPARE(client.displayConfirmationOwned(), false);
        QCOMPARE(
            client.displayConfirmationGeneration(),
            QString(64, QLatin1Char('1'))
        );

        service_.stop();
        QTRY_VERIFY_WITH_TIMEOUT(!client.available(), 3000);
        QCOMPARE(client.displayConfirmationState(), QStringLiteral("idle"));
        QCOMPARE(client.displayConfirmationRevision(), 0ULL);
        QCOMPARE(client.displayConfirmationDeadlineMs(), 0ULL);
        QVERIFY(client.displayConfirmationGeneration().isEmpty());
        QCOMPARE(client.displayConfirmationOwned(), false);
        QCOMPARE(client.managementState(), QStringLiteral("unmanaged"));
    }

    void keepsForeignPendingConfirmationLockedAndAvailable()
    {
        service_.setPendingBehavior(
            FakeCompositor::PendingBehavior::NoDisplayConfirmation
        );
        service_.setAwaitingConfirmation();
        QVERIFY(service_.start());

        HyprShelld::CompositorClient client(bus_, nullptr);
        QTRY_VERIFY_WITH_TIMEOUT(client.available(), 3000);
        QTRY_COMPARE_WITH_TIMEOUT(service_.pendingCallCount(), 1, 3000);
        QCOMPARE(
            client.displayConfirmationState(),
            QStringLiteral("awaiting-confirmation")
        );
        QCOMPARE(client.displayConfirmationOwned(), false);
        QCOMPARE(client.managementState(), QStringLiteral("preview"));
        QVERIFY(client.lastErrorName().isEmpty());
    }

    void failsHydrationOnUnexpectedPendingLookupErrors()
    {
        service_.setPendingBehavior(
            FakeCompositor::PendingBehavior::UnexpectedError
        );
        service_.setAwaitingConfirmation();
        QVERIFY(service_.start());

        HyprShelld::CompositorClient client(bus_, nullptr);
        QTRY_COMPARE_WITH_TIMEOUT(service_.pendingCallCount(), 1, 3000);
        QTRY_COMPARE_WITH_TIMEOUT(
            client.lastErrorName(),
            QStringLiteral("org.freedesktop.DBus.Error.UnknownMethod"),
            3000
        );
        QCOMPARE(client.available(), true);
        QCOMPARE(client.displayDiscoveryAvailable(), false);
        QCOMPARE(client.displayConfirmationOwned(), false);
    }

    void rejectsMalformedPreviewRepliesAndAcceptsTheExactTuple()
    {
        QVERIFY(service_.start());
        HyprShelld::CompositorClient client(bus_, nullptr);
        QTRY_VERIFY_WITH_TIMEOUT(client.available(), 3000);
        QTRY_VERIFY_WITH_TIMEOUT(client.displayDiscoveryAvailable(), 3000);
        QSignalSpy failures(
            &client,
            &HyprShelld::CompositorClient::operationFailed
        );
        QVERIFY(failures.isValid());

        const QVariantList output{
            QVariantMap{
                {QStringLiteral("id"), QStringLiteral("display-DP-1")},
                {QStringLiteral("selector"), QStringLiteral("DP-1")},
                {QStringLiteral("enabled"), true},
            }
        };
        const QList<QVariantList> malformedReplies{
            {
                QVariant::fromValue<qulonglong>(previewRevision),
                QString{},
                QVariant::fromValue<qulonglong>(previewDeadlineMs),
                previewGeneration,
            },
            {
                QVariant::fromValue<qulonglong>(previewRevision),
                QString(32, QLatin1Char('F')),
                QVariant::fromValue<qulonglong>(previewDeadlineMs),
                previewGeneration,
            },
            {
                QVariant::fromValue<qulonglong>(baselineRevision),
                confirmationToken,
                QVariant::fromValue<qulonglong>(previewDeadlineMs),
                previewGeneration,
            },
            {
                QVariant::fromValue<qulonglong>(previewRevision + 1),
                confirmationToken,
                QVariant::fromValue<qulonglong>(previewDeadlineMs),
                previewGeneration,
            },
            {
                QVariant::fromValue<qulonglong>(previewRevision),
                confirmationToken,
                QVariant::fromValue<qulonglong>(0),
                previewGeneration,
            },
            {
                QVariant::fromValue<qulonglong>(previewRevision),
                confirmationToken,
                QVariant::fromValue<qulonglong>(previewDeadlineMs),
                QStringLiteral("not-a-generation-digest"),
            },
            {
                QVariant::fromValue<qulonglong>(previewRevision),
                confirmationToken,
            },
        };

        for (const auto &reply : malformedReplies) {
            const auto expectedFailures = failures.size() + 1;
            service_.setPreviewReply(reply, false);
            client.previewDisplayConfiguration(output, 15);
            QVERIFY(client.busy());
            QTRY_COMPARE_WITH_TIMEOUT(failures.size(), expectedFailures, 3000);
            QCOMPARE(
                failures.last().at(0).toString(),
                QStringLiteral(
                    "org.hyprshelld.Client.Compositor.Error.InvalidReply"
                )
            );
            QTRY_VERIFY_WITH_TIMEOUT(!client.busy(), 3000);
            QTRY_VERIFY_WITH_TIMEOUT(client.available(), 3000);
            QCOMPARE(client.displayConfirmationOwned(), false);
        }

        service_.setPreviewReply(validPreviewReply(), true);
        client.previewDisplayConfiguration(output, 15);
        QTRY_VERIFY_WITH_TIMEOUT(!client.busy(), 3000);
        QTRY_COMPARE_WITH_TIMEOUT(
            client.displayConfirmationState(),
            QStringLiteral("awaiting-confirmation"),
            3000
        );
        QCOMPARE(client.displayConfirmationRevision(), previewRevision);
        QCOMPARE(client.displayConfirmationDeadlineMs(), previewDeadlineMs);
        QCOMPARE(client.displayConfirmationGeneration(), previewGeneration);
        QCOMPARE(client.displayConfirmationOwned(), true);
    }

    void ignoresLateAndDuplicateAsyncReplies()
    {
        service_.setHoldSnapshots(true);
        QVERIFY(service_.start());
        HyprShelld::CompositorClient client(bus_, nullptr);
        QTRY_COMPARE_WITH_TIMEOUT(service_.heldSnapshotCount(), 1, 3000);

        client.refresh();
        QTRY_COMPARE_WITH_TIMEOUT(service_.heldSnapshotCount(), 2, 3000);
        QVERIFY(service_.releaseNextSnapshot(
            {QByteArrayLiteral("malformed")},
            true
        ));
        QTest::qWait(50);
        QCOMPARE(client.available(), false);

        QVERIFY(service_.releaseNextSnapshot({}, true));
        QTRY_VERIFY_WITH_TIMEOUT(client.available(), 3000);
        QCOMPARE(client.revision(), baselineRevision);

        service_.setHoldSnapshots(false);
        service_.setHoldPreviews(true);
        service_.setPreviewReply(validPreviewReply(), false);
        const QVariantList output{
            QVariantMap{
                {QStringLiteral("id"), QStringLiteral("display-DP-1")},
                {QStringLiteral("selector"), QStringLiteral("DP-1")},
                {QStringLiteral("enabled"), true},
            }
        };
        QSignalSpy failures(
            &client,
            &HyprShelld::CompositorClient::operationFailed
        );
        client.previewDisplayConfiguration(output, 15);
        QTRY_COMPARE_WITH_TIMEOUT(service_.heldPreviewCount(), 1, 3000);
        QCOMPARE(client.busy(), true);

        service_.stop();
        QTRY_VERIFY_WITH_TIMEOUT(!client.available(), 3000);
        QCOMPARE(client.busy(), false);
        QCOMPARE(client.displayConfirmationState(), QStringLiteral("idle"));
        QCOMPARE(client.displayConfirmationOwned(), false);

        QVERIFY(service_.start());
        QTRY_VERIFY_WITH_TIMEOUT(client.available(), 3000);
        QVERIFY(service_.releaseNextPreview(true));
        QTest::qWait(100);
        QCOMPARE(client.displayConfirmationState(), QStringLiteral("idle"));
        QCOMPARE(client.displayConfirmationOwned(), false);
        QCOMPARE(failures.size(), 0);
    }

    void acceptsConfirmReplyAfterCommittingAndIdleProperties()
    {
        service_.setPendingBehavior(FakeCompositor::PendingBehavior::Success);
        service_.setAwaitingConfirmation();
        QVERIFY(service_.start());

        HyprShelld::CompositorClient client(bus_, nullptr);
        QTRY_VERIFY_WITH_TIMEOUT(client.available(), 3000);
        QTRY_VERIFY_WITH_TIMEOUT(client.displayConfirmationOwned(), 3000);
        QSignalSpy failures(
            &client,
            &HyprShelld::CompositorClient::operationFailed
        );
        QVERIFY(failures.isValid());

        client.confirmDisplayConfiguration();
        QVERIFY(client.busy());
        QTRY_VERIFY_WITH_TIMEOUT(!client.busy(), 3000);
        QTRY_COMPARE_WITH_TIMEOUT(
            client.displayConfirmationState(),
            QStringLiteral("idle"),
            3000
        );
        QTRY_VERIFY_WITH_TIMEOUT(client.available(), 3000);
        QCOMPARE(failures.size(), 0);
        QVERIFY(client.lastErrorName().isEmpty());
        QCOMPARE(client.displayConfirmationOwned(), false);
    }

    void acceptsRevertReplyAfterRevertingAndIdleProperties()
    {
        service_.setPendingBehavior(FakeCompositor::PendingBehavior::Success);
        service_.setAwaitingConfirmation();
        QVERIFY(service_.start());

        HyprShelld::CompositorClient client(bus_, nullptr);
        QTRY_VERIFY_WITH_TIMEOUT(client.available(), 3000);
        QTRY_VERIFY_WITH_TIMEOUT(client.displayConfirmationOwned(), 3000);
        QSignalSpy failures(
            &client,
            &HyprShelld::CompositorClient::operationFailed
        );
        QVERIFY(failures.isValid());

        client.revertDisplayConfiguration();
        QVERIFY(client.busy());
        QTRY_VERIFY_WITH_TIMEOUT(!client.busy(), 3000);
        QTRY_COMPARE_WITH_TIMEOUT(
            client.displayConfirmationState(),
            QStringLiteral("idle"),
            3000
        );
        QTRY_VERIFY_WITH_TIMEOUT(client.available(), 3000);
        QCOMPARE(failures.size(), 0);
        QVERIFY(client.lastErrorName().isEmpty());
        QCOMPARE(client.displayConfirmationOwned(), false);
        QCOMPARE(client.revision(), baselineRevision);
    }

private:
    QDBusConnection bus_ = QDBusConnection::connectToBus(
        QDBusConnection::SessionBus,
        QStringLiteral("compositor-client-test")
    );
    QDBusConnection serviceBus_ = QDBusConnection::connectToBus(
        QDBusConnection::SessionBus,
        QStringLiteral("compositor-service-test")
    );
    FakeCompositor service_{serviceBus_};
};

QTEST_GUILESS_MAIN(CompositorClientTest)

#include "compositor_client_test.moc"
