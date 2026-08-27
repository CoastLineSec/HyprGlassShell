#include "compositord/compositor2_dbus_types.h"

#include <QDir>
#include <QDBusMetaType>
#include <QFile>
#include <QFileInfo>
#include <QStringList>
#include <QXmlStreamReader>
#include <QtTest>

namespace {

QString attribute(const QXmlStreamReader &xml, const char16_t *name)
{
    return xml.attributes().value(QStringView(name)).toString();
}

QStringList describeContract(const QString &path, QString &error)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        error = file.errorString();
        return {};
    }

    QXmlStreamReader xml(&file);
    QStringList description;
    QString memberKind;
    QString memberName;

    while (!xml.atEnd()) {
        xml.readNext();

        if (xml.isStartElement()) {
            const auto element = xml.name();

            if (element == u"node") {
                description.append(
                    QStringLiteral("node=%1").arg(attribute(xml, u"name"))
                );
            } else if (element == u"interface") {
                description.append(
                    QStringLiteral("interface=%1").arg(attribute(xml, u"name"))
                );
            } else if (element == u"property") {
                memberKind = QStringLiteral("property");
                memberName = attribute(xml, u"name");
                description.append(
                    QStringLiteral("property=%1:%2:%3")
                        .arg(
                            memberName,
                            attribute(xml, u"type"),
                            attribute(xml, u"access")
                        )
                );
            } else if (element == u"method" || element == u"signal") {
                memberKind = element.toString();
                memberName = attribute(xml, u"name");
                description.append(
                    QStringLiteral("%1=%2").arg(memberKind, memberName)
                );
            } else if (element == u"arg") {
                description.append(
                    QStringLiteral("arg=%1:%2:%3:%4:%5")
                        .arg(
                            memberKind,
                            memberName,
                            attribute(xml, u"name"),
                            attribute(xml, u"type"),
                            attribute(xml, u"direction")
                        )
                );
            } else if (element == u"annotation") {
                description.append(
                    QStringLiteral("annotation=%1:%2:%3:%4")
                        .arg(
                            memberKind,
                            memberName,
                            attribute(xml, u"name"),
                            attribute(xml, u"value")
                        )
                );
            }
        } else if (xml.isEndElement()) {
            const auto element = xml.name();
            if (element == u"property" || element == u"method" || element == u"signal") {
                memberKind.clear();
                memberName.clear();
            }
        }
    }

    if (xml.hasError()) {
        error = xml.errorString();
        return {};
    }

    return description;
}

void compareContract(const QString &path, const QStringList &expected)
{
    QString error;
    const auto actual = describeContract(path, error);

    QVERIFY2(error.isEmpty(), qPrintable(error));
    QCOMPARE(actual, expected);
}

QString readTextFile(const QString &path, QString &error)
{
    error.clear();
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        error = file.errorString();
        return {};
    }

    return QString::fromUtf8(file.readAll());
}

QString methodDocumentation(
    const QString &path,
    const QString &methodName,
    QString &error
)
{
    const auto document = readTextFile(path, error);
    if (!error.isEmpty()) {
        return {};
    }

    const auto marker = QStringLiteral("<method name=\"%1\"").arg(methodName);
    const auto methodOffset = document.indexOf(marker);
    if (methodOffset < 0) {
        error = QStringLiteral("Method %1 was not found").arg(methodName);
        return {};
    }

    const auto commentEnd = document.lastIndexOf(
        QStringLiteral("-->"), methodOffset
    );
    const auto commentStart = document.lastIndexOf(
        QStringLiteral("<!--"), commentEnd
    );
    if (commentStart < 0 || commentEnd < commentStart) {
        error = QStringLiteral("Method %1 has no documentation").arg(methodName);
        return {};
    }

    const auto between = document.mid(
        commentEnd + 3, methodOffset - commentEnd - 3
    );
    if (!between.trimmed().isEmpty()) {
        error = QStringLiteral("Method %1 documentation is not adjacent")
                    .arg(methodName);
        return {};
    }

    return document.mid(
        commentStart + 4, commentEnd - commentStart - 4
    ).simplified();
}

} // namespace

class DbusContractTest final : public QObject {
    Q_OBJECT

private slots:
    void coordinatorContract()
    {
        compareContract(
            QStringLiteral(HYPRSHELLD_COORDINATOR_XML),
            {
                QStringLiteral("node=/org/hyprshelld/Coordinator1"),
                QStringLiteral("interface=org.hyprshelld.Coordinator1"),
                QStringLiteral("property=Healthy:b:read"),
                QStringLiteral("annotation=property:Healthy:org.freedesktop.DBus.Property.EmitsChangedSignal:true"),
                QStringLiteral("property=FailedUnits:as:read"),
                QStringLiteral("annotation=property:FailedUnits:org.freedesktop.DBus.Property.EmitsChangedSignal:true"),
                QStringLiteral("property=FailureSummary:s:read"),
                QStringLiteral("annotation=property:FailureSummary:org.freedesktop.DBus.Property.EmitsChangedSignal:true"),
                QStringLiteral("method=RestartComponent"),
                QStringLiteral("arg=method:RestartComponent:unitName:s:in"),
            }
        );
    }

    void configContract()
    {
        compareContract(
            QStringLiteral(HYPRSHELLD_CONFIG_XML),
            {
                QStringLiteral("node=/org/hyprshelld/Config1"),
                QStringLiteral("interface=org.hyprshelld.Config1"),
                QStringLiteral("property=BarHeight:u:read"),
                QStringLiteral("annotation=property:BarHeight:org.freedesktop.DBus.Property.EmitsChangedSignal:true"),
                QStringLiteral("property=ShellBorderEnabled:b:read"),
                QStringLiteral("annotation=property:ShellBorderEnabled:org.freedesktop.DBus.Property.EmitsChangedSignal:true"),
                QStringLiteral("property=ShellBorderWidth:u:read"),
                QStringLiteral("annotation=property:ShellBorderWidth:org.freedesktop.DBus.Property.EmitsChangedSignal:true"),
                QStringLiteral("property=ShellBorderRadius:u:read"),
                QStringLiteral("annotation=property:ShellBorderRadius:org.freedesktop.DBus.Property.EmitsChangedSignal:true"),
                QStringLiteral("property=SyncHyprlandWindowBorders:b:read"),
                QStringLiteral("annotation=property:SyncHyprlandWindowBorders:org.freedesktop.DBus.Property.EmitsChangedSignal:true"),
                QStringLiteral("property=ShellInnerSpacing:u:read"),
                QStringLiteral("annotation=property:ShellInnerSpacing:org.freedesktop.DBus.Property.EmitsChangedSignal:true"),
                QStringLiteral("property=ShellOuterSpacing:u:read"),
                QStringLiteral("annotation=property:ShellOuterSpacing:org.freedesktop.DBus.Property.EmitsChangedSignal:true"),
                QStringLiteral("property=SyncHyprlandWindowSpacing:b:read"),
                QStringLiteral("annotation=property:SyncHyprlandWindowSpacing:org.freedesktop.DBus.Property.EmitsChangedSignal:true"),
                QStringLiteral("property=AppearanceMode:s:read"),
                QStringLiteral("annotation=property:AppearanceMode:org.freedesktop.DBus.Property.EmitsChangedSignal:true"),
                QStringLiteral("property=Revision:t:read"),
                QStringLiteral("annotation=property:Revision:org.freedesktop.DBus.Property.EmitsChangedSignal:true"),
                QStringLiteral("property=RecoveryState:s:read"),
                QStringLiteral("annotation=property:RecoveryState:org.freedesktop.DBus.Property.EmitsChangedSignal:const"),
                QStringLiteral("method=SetBarHeight"),
                QStringLiteral("arg=method:SetBarHeight:height:u:in"),
                QStringLiteral("arg=method:SetBarHeight:revision:t:out"),
                QStringLiteral("method=ResetBarHeight"),
                QStringLiteral("arg=method:ResetBarHeight:revision:t:out"),
                QStringLiteral("method=SetSharedBorder"),
                QStringLiteral("arg=method:SetSharedBorder:enabled:b:in"),
                QStringLiteral("arg=method:SetSharedBorder:width:u:in"),
                QStringLiteral("arg=method:SetSharedBorder:radius:u:in"),
                QStringLiteral("arg=method:SetSharedBorder:syncHyprlandWindowBorders:b:in"),
                QStringLiteral("arg=method:SetSharedBorder:revision:t:out"),
                QStringLiteral("method=ResetSharedBorder"),
                QStringLiteral("arg=method:ResetSharedBorder:revision:t:out"),
                QStringLiteral("method=SetSharedSpacing"),
                QStringLiteral("arg=method:SetSharedSpacing:inner:u:in"),
                QStringLiteral("arg=method:SetSharedSpacing:outer:u:in"),
                QStringLiteral("arg=method:SetSharedSpacing:syncHyprlandWindowSpacing:b:in"),
                QStringLiteral("arg=method:SetSharedSpacing:revision:t:out"),
                QStringLiteral("method=ResetSharedSpacing"),
                QStringLiteral("arg=method:ResetSharedSpacing:revision:t:out"),
                QStringLiteral("method=SetAppearanceMode"),
                QStringLiteral("arg=method:SetAppearanceMode:mode:s:in"),
                QStringLiteral("arg=method:SetAppearanceMode:revision:t:out"),
                QStringLiteral("method=ResetAppearanceMode"),
                QStringLiteral("arg=method:ResetAppearanceMode:revision:t:out"),
            }
        );
    }

    void componentManagerContract()
    {
        compareContract(
            QStringLiteral(HYPRSHELLD_COMPONENT_MANAGER_XML),
            {
                QStringLiteral("node=/org/hyprshelld/ComponentManager1"),
                QStringLiteral("interface=org.hyprshelld.ComponentManager1"),
                QStringLiteral("property=CatalogDigest:s:read"),
                QStringLiteral("annotation=property:CatalogDigest:org.freedesktop.DBus.Property.EmitsChangedSignal:true"),
                QStringLiteral("method=ListComponents"),
                QStringLiteral("arg=method:ListComponents:componentIds:as:out"),
                QStringLiteral("arg=method:ListComponents:catalogDigest:s:out"),
                QStringLiteral("method=GetComponent"),
                QStringLiteral("arg=method:GetComponent:componentId:s:in"),
                QStringLiteral("arg=method:GetComponent:expectedCatalogDigest:s:in"),
                QStringLiteral("arg=method:GetComponent:manifestVersion:u:out"),
                QStringLiteral("arg=method:GetComponent:componentType:s:out"),
                QStringLiteral("arg=method:GetComponent:version:s:out"),
                QStringLiteral("arg=method:GetComponent:name:s:out"),
                QStringLiteral("arg=method:GetComponent:description:s:out"),
                QStringLiteral("arg=method:GetComponent:authorNames:as:out"),
                QStringLiteral("arg=method:GetComponent:authorEmails:as:out"),
                QStringLiteral("arg=method:GetComponent:authorHomepages:as:out"),
                QStringLiteral("arg=method:GetComponent:license:s:out"),
                QStringLiteral("arg=method:GetComponent:homepage:s:out"),
                QStringLiteral("arg=method:GetComponent:source:s:out"),
                QStringLiteral("arg=method:GetComponent:issues:s:out"),
                QStringLiteral("arg=method:GetComponent:componentApiVersion:s:out"),
                QStringLiteral("arg=method:GetComponent:runtimeKind:s:out"),
                QStringLiteral("arg=method:GetComponent:runtimeFactory:s:out"),
                QStringLiteral("arg=method:GetComponent:runtimeEntryPoint:s:out"),
                QStringLiteral("arg=method:GetComponent:runtimeArguments:as:out"),
                QStringLiteral("arg=method:GetComponent:settingsSchema:ay:out"),
                QStringLiteral("arg=method:GetComponent:capabilityIds:as:out"),
                QStringLiteral("arg=method:GetComponent:capabilityReasons:as:out"),
                QStringLiteral("arg=method:GetComponent:dependencyIds:as:out"),
                QStringLiteral("arg=method:GetComponent:dependencyVersionRequirements:as:out"),
                QStringLiteral("arg=method:GetComponent:packageDigest:s:out"),
                QStringLiteral("arg=method:GetComponent:origin:s:out"),
                QStringLiteral("arg=method:GetComponent:removable:b:out"),
                QStringLiteral("method=GetDeclarativeRuntime"),
                QStringLiteral("arg=method:GetDeclarativeRuntime:componentId:s:in"),
                QStringLiteral("arg=method:GetDeclarativeRuntime:expectedPackageDigest:s:in"),
                QStringLiteral("arg=method:GetDeclarativeRuntime:expectedCatalogDigest:s:in"),
                QStringLiteral("arg=method:GetDeclarativeRuntime:definition:ay:out"),
                QStringLiteral("method=BeginPackageInspection"),
                QStringLiteral("arg=method:BeginPackageInspection:packageFile:h:in"),
                QStringLiteral("arg=method:BeginPackageInspection:inspectionToken:s:out"),
                QStringLiteral("method=CancelPackageInspection"),
                QStringLiteral("arg=method:CancelPackageInspection:inspectionToken:s:in"),
                QStringLiteral("method=InstallInspectedPackage"),
                QStringLiteral("arg=method:InstallInspectedPackage:inspectionToken:s:in"),
                QStringLiteral("arg=method:InstallInspectedPackage:expectedArchiveDigest:s:in"),
                QStringLiteral("arg=method:InstallInspectedPackage:expectedCatalogDigest:s:in"),
                QStringLiteral("arg=method:InstallInspectedPackage:componentId:s:out"),
                QStringLiteral("arg=method:InstallInspectedPackage:packageDigest:s:out"),
                QStringLiteral("arg=method:InstallInspectedPackage:catalogDigest:s:out"),
                QStringLiteral("method=RemovePackage"),
                QStringLiteral("arg=method:RemovePackage:componentId:s:in"),
                QStringLiteral("arg=method:RemovePackage:expectedPackageDigest:s:in"),
                QStringLiteral("arg=method:RemovePackage:expectedCatalogDigest:s:in"),
                QStringLiteral("arg=method:RemovePackage:catalogDigest:s:out"),
                QStringLiteral("signal=PackageInspectionFinished"),
                QStringLiteral("arg=signal:PackageInspectionFinished:inspectionToken:s:"),
                QStringLiteral("arg=signal:PackageInspectionFinished:review:ay:"),
                QStringLiteral("arg=signal:PackageInspectionFinished:errorCode:s:"),
                QStringLiteral("arg=signal:PackageInspectionFinished:errorMessage:s:"),
            }
        );
    }

    void componentConfigContract()
    {
        compareContract(
            QStringLiteral(HYPRSHELLD_COMPONENT_CONFIG_XML),
            {
                QStringLiteral("node=/org/hyprshelld/Config1/Components"),
                QStringLiteral("interface=org.hyprshelld.ComponentConfig1"),
                QStringLiteral("property=Available:b:read"),
                QStringLiteral("annotation=property:Available:org.freedesktop.DBus.Property.EmitsChangedSignal:true"),
                QStringLiteral("property=CatalogAvailable:b:read"),
                QStringLiteral("annotation=property:CatalogAvailable:org.freedesktop.DBus.Property.EmitsChangedSignal:true"),
                QStringLiteral("property=Revision:t:read"),
                QStringLiteral("annotation=property:Revision:org.freedesktop.DBus.Property.EmitsChangedSignal:true"),
                QStringLiteral("property=CatalogDigest:s:read"),
                QStringLiteral("annotation=property:CatalogDigest:org.freedesktop.DBus.Property.EmitsChangedSignal:true"),
                QStringLiteral("property=LoadState:s:read"),
                QStringLiteral("annotation=property:LoadState:org.freedesktop.DBus.Property.EmitsChangedSignal:true"),
                QStringLiteral("method=GetSnapshot"),
                QStringLiteral("arg=method:GetSnapshot:snapshot:ay:out"),
                QStringLiteral("arg=method:GetSnapshot:revision:t:out"),
                QStringLiteral("arg=method:GetSnapshot:catalogDigest:s:out"),
                QStringLiteral("method=ReplaceSnapshot"),
                QStringLiteral("arg=method:ReplaceSnapshot:expectedRevision:t:in"),
                QStringLiteral("arg=method:ReplaceSnapshot:expectedCatalogDigest:s:in"),
                QStringLiteral("arg=method:ReplaceSnapshot:candidateSnapshot:ay:in"),
                QStringLiteral("arg=method:ReplaceSnapshot:revision:t:out"),
            }
        );
    }

    void componentRuntimeContract()
    {
        compareContract(
            QStringLiteral(HYPRSHELLD_COMPONENT_RUNTIME_XML),
            {
                QStringLiteral("node=/org/hyprshelld/Coordinator1/Components"),
                QStringLiteral("interface=org.hyprshelld.ComponentRuntime1"),
                QStringLiteral("property=SurfacePlanRevision:t:read"),
                QStringLiteral("annotation=property:SurfacePlanRevision:org.freedesktop.DBus.Property.EmitsChangedSignal:true"),
                QStringLiteral("property=SurfacePlanDigest:s:read"),
                QStringLiteral("annotation=property:SurfacePlanDigest:org.freedesktop.DBus.Property.EmitsChangedSignal:true"),
                QStringLiteral("property=SurfacePlanState:s:read"),
                QStringLiteral("annotation=property:SurfacePlanState:org.freedesktop.DBus.Property.EmitsChangedSignal:true"),
                QStringLiteral("property=RuntimeHealthRevision:t:read"),
                QStringLiteral("annotation=property:RuntimeHealthRevision:org.freedesktop.DBus.Property.EmitsChangedSignal:true"),
                QStringLiteral("property=ThirdPartySafeMode:b:read"),
                QStringLiteral("annotation=property:ThirdPartySafeMode:org.freedesktop.DBus.Property.EmitsChangedSignal:true"),
                QStringLiteral("method=GetSurfacePlan"),
                QStringLiteral("arg=method:GetSurfacePlan:expectedSurfacePlanRevision:t:in"),
                QStringLiteral("arg=method:GetSurfacePlan:surfacePlan:ay:out"),
                QStringLiteral("arg=method:GetSurfacePlan:surfacePlanDigest:s:out"),
                QStringLiteral("method=ListComponentRuntimeStates"),
                QStringLiteral("arg=method:ListComponentRuntimeStates:expectedRuntimeHealthRevision:t:in"),
                QStringLiteral("arg=method:ListComponentRuntimeStates:componentIds:as:out"),
                QStringLiteral("arg=method:ListComponentRuntimeStates:packageDigests:as:out"),
                QStringLiteral("arg=method:ListComponentRuntimeStates:states:as:out"),
                QStringLiteral("arg=method:ListComponentRuntimeStates:reasons:as:out"),
                QStringLiteral("arg=method:ListComponentRuntimeStates:failureCounts:au:out"),
                QStringLiteral("annotation=method:ListComponentRuntimeStates:org.qtproject.QtDBus.QtTypeName.Out4:QList<uint>"),
                QStringLiteral("method=RetryComponent"),
                QStringLiteral("arg=method:RetryComponent:componentId:s:in"),
                QStringLiteral("arg=method:RetryComponent:expectedPackageDigest:s:in"),
                QStringLiteral("arg=method:RetryComponent:expectedRuntimeHealthRevision:t:in"),
                QStringLiteral("arg=method:RetryComponent:runtimeHealthRevision:t:out"),
                QStringLiteral("method=ActivationStable"),
                QStringLiteral("arg=method:ActivationStable:instanceId:s:in"),
                QStringLiteral("arg=method:ActivationStable:componentId:s:in"),
                QStringLiteral("arg=method:ActivationStable:packageDigest:s:in"),
                QStringLiteral("arg=method:ActivationStable:surfacePlanRevision:t:in"),
                QStringLiteral("method=AuthorizeSurfacePlan"),
                QStringLiteral("arg=method:AuthorizeSurfacePlan:surfacePlanRevision:t:in"),
                QStringLiteral("arg=method:AuthorizeSurfacePlan:accepted:b:out"),
                QStringLiteral("method=CancelSurfacePlanAuthorization"),
                QStringLiteral("arg=method:CancelSurfacePlanAuthorization:surfacePlanRevision:t:in"),
                QStringLiteral("arg=method:CancelSurfacePlanAuthorization:cancelled:b:out"),
                QStringLiteral("method=ActivationFailed"),
                QStringLiteral("arg=method:ActivationFailed:instanceId:s:in"),
                QStringLiteral("arg=method:ActivationFailed:componentId:s:in"),
                QStringLiteral("arg=method:ActivationFailed:packageDigest:s:in"),
                QStringLiteral("arg=method:ActivationFailed:surfacePlanRevision:t:in"),
                QStringLiteral("arg=method:ActivationFailed:reason:s:in"),
            }
        );
    }

    void compositorContract()
    {
        compareContract(
            QStringLiteral(HYPRSHELLD_COMPOSITOR_XML),
            {
                QStringLiteral("node=/org/hyprshelld/Compositor1"),
                QStringLiteral("interface=org.hyprshelld.Compositor1"),
                QStringLiteral("property=Available:b:read"),
                QStringLiteral("annotation=property:Available:org.freedesktop.DBus.Property.EmitsChangedSignal:true"),
                QStringLiteral("property=Writable:b:read"),
                QStringLiteral("annotation=property:Writable:org.freedesktop.DBus.Property.EmitsChangedSignal:true"),
                QStringLiteral("property=Revision:t:read"),
                QStringLiteral("annotation=property:Revision:org.freedesktop.DBus.Property.EmitsChangedSignal:true"),
                QStringLiteral("property=LoadState:s:read"),
                QStringLiteral("annotation=property:LoadState:org.freedesktop.DBus.Property.EmitsChangedSignal:true"),
                QStringLiteral("property=ManagementState:s:read"),
                QStringLiteral("annotation=property:ManagementState:org.freedesktop.DBus.Property.EmitsChangedSignal:true"),
                QStringLiteral("property=EntrypointDigest:s:read"),
                QStringLiteral("annotation=property:EntrypointDigest:org.freedesktop.DBus.Property.EmitsChangedSignal:true"),
                QStringLiteral("property=CatalogDigest:s:read"),
                QStringLiteral("annotation=property:CatalogDigest:org.freedesktop.DBus.Property.EmitsChangedSignal:true"),
                QStringLiteral("property=ActionCatalogDigest:s:read"),
                QStringLiteral("annotation=property:ActionCatalogDigest:org.freedesktop.DBus.Property.EmitsChangedSignal:true"),
                QStringLiteral("property=AppliedRevision:t:read"),
                QStringLiteral("annotation=property:AppliedRevision:org.freedesktop.DBus.Property.EmitsChangedSignal:true"),
                QStringLiteral("property=ApplyState:s:read"),
                QStringLiteral("annotation=property:ApplyState:org.freedesktop.DBus.Property.EmitsChangedSignal:true"),
                QStringLiteral("property=RequiredActivation:s:read"),
                QStringLiteral("annotation=property:RequiredActivation:org.freedesktop.DBus.Property.EmitsChangedSignal:true"),
                QStringLiteral("property=GenerationDigest:s:read"),
                QStringLiteral("annotation=property:GenerationDigest:org.freedesktop.DBus.Property.EmitsChangedSignal:true"),
                QStringLiteral("property=DisplayConfirmationState:s:read"),
                QStringLiteral("annotation=property:DisplayConfirmationState:org.freedesktop.DBus.Property.EmitsChangedSignal:true"),
                QStringLiteral("property=DisplayConfirmationRevision:t:read"),
                QStringLiteral("annotation=property:DisplayConfirmationRevision:org.freedesktop.DBus.Property.EmitsChangedSignal:true"),
                QStringLiteral("property=DisplayConfirmationDeadlineMs:t:read"),
                QStringLiteral("annotation=property:DisplayConfirmationDeadlineMs:org.freedesktop.DBus.Property.EmitsChangedSignal:true"),
                QStringLiteral("property=DisplayConfirmationGeneration:s:read"),
                QStringLiteral("annotation=property:DisplayConfirmationGeneration:org.freedesktop.DBus.Property.EmitsChangedSignal:true"),
                QStringLiteral("property=SharedBorderSyncState:s:read"),
                QStringLiteral("annotation=property:SharedBorderSyncState:org.freedesktop.DBus.Property.EmitsChangedSignal:true"),
                QStringLiteral("property=SharedBorderSourceRevision:t:read"),
                QStringLiteral("annotation=property:SharedBorderSourceRevision:org.freedesktop.DBus.Property.EmitsChangedSignal:true"),
                QStringLiteral("property=SharedBorderSyncError:s:read"),
                QStringLiteral("annotation=property:SharedBorderSyncError:org.freedesktop.DBus.Property.EmitsChangedSignal:true"),
                QStringLiteral("property=SharedSpacingSyncState:s:read"),
                QStringLiteral("annotation=property:SharedSpacingSyncState:org.freedesktop.DBus.Property.EmitsChangedSignal:true"),
                QStringLiteral("property=SharedSpacingSourceRevision:t:read"),
                QStringLiteral("annotation=property:SharedSpacingSourceRevision:org.freedesktop.DBus.Property.EmitsChangedSignal:true"),
                QStringLiteral("property=SharedSpacingSyncError:s:read"),
                QStringLiteral("annotation=property:SharedSpacingSyncError:org.freedesktop.DBus.Property.EmitsChangedSignal:true"),
                QStringLiteral("method=GetSnapshot"),
                QStringLiteral("arg=method:GetSnapshot:snapshot:ay:out"),
                QStringLiteral("arg=method:GetSnapshot:revision:t:out"),
                QStringLiteral("arg=method:GetSnapshot:catalogDigest:s:out"),
                QStringLiteral("arg=method:GetSnapshot:actionCatalogDigest:s:out"),
                QStringLiteral("method=GetOptionCatalog"),
                QStringLiteral("arg=method:GetOptionCatalog:optionCatalog:ay:out"),
                QStringLiteral("arg=method:GetOptionCatalog:catalogDigest:s:out"),
                QStringLiteral("method=GetActionCatalog"),
                QStringLiteral("arg=method:GetActionCatalog:actionCatalog:ay:out"),
                QStringLiteral("arg=method:GetActionCatalog:actionCatalogDigest:s:out"),
                QStringLiteral("arg=method:GetActionCatalog:configSchema:ay:out"),
                QStringLiteral("arg=method:GetActionCatalog:configSchemaDigest:s:out"),
                QStringLiteral("method=ReplaceSnapshot"),
                QStringLiteral("arg=method:ReplaceSnapshot:expectedRevision:t:in"),
                QStringLiteral("arg=method:ReplaceSnapshot:expectedCatalogDigest:s:in"),
                QStringLiteral("arg=method:ReplaceSnapshot:expectedActionCatalogDigest:s:in"),
                QStringLiteral("arg=method:ReplaceSnapshot:candidateSnapshot:ay:in"),
                QStringLiteral("arg=method:ReplaceSnapshot:revision:t:out"),
                QStringLiteral("method=Apply"),
                QStringLiteral("arg=method:Apply:expectedRevision:t:in"),
                QStringLiteral("arg=method:Apply:expectedCatalogDigest:s:in"),
                QStringLiteral("arg=method:Apply:expectedActionCatalogDigest:s:in"),
                QStringLiteral("arg=method:Apply:appliedRevision:t:out"),
                QStringLiteral("arg=method:Apply:generationDigest:s:out"),
                QStringLiteral("method=Recover"),
                QStringLiteral("arg=method:Recover:expectedRevision:t:in"),
                QStringLiteral("arg=method:Recover:expectedCatalogDigest:s:in"),
                QStringLiteral("arg=method:Recover:expectedActionCatalogDigest:s:in"),
                QStringLiteral("arg=method:Recover:revision:t:out"),
                QStringLiteral("arg=method:Recover:appliedRevision:t:out"),
                QStringLiteral("arg=method:Recover:generationDigest:s:out"),
                QStringLiteral("method=AdoptManagedConfiguration"),
                QStringLiteral("arg=method:AdoptManagedConfiguration:expectedRevision:t:in"),
                QStringLiteral("arg=method:AdoptManagedConfiguration:expectedCatalogDigest:s:in"),
                QStringLiteral("arg=method:AdoptManagedConfiguration:expectedActionCatalogDigest:s:in"),
                QStringLiteral("arg=method:AdoptManagedConfiguration:expectedEntrypointDigest:s:in"),
                QStringLiteral("arg=method:AdoptManagedConfiguration:appliedRevision:t:out"),
                QStringLiteral("arg=method:AdoptManagedConfiguration:generationDigest:s:out"),
                QStringLiteral("arg=method:AdoptManagedConfiguration:entrypointDigest:s:out"),
                QStringLiteral("method=GetConnectedDisplays"),
                QStringLiteral("arg=method:GetConnectedDisplays:topology:ay:out"),
                QStringLiteral("arg=method:GetConnectedDisplays:observedAtMs:t:out"),
                QStringLiteral("method=GetConnectedInputDevices"),
                QStringLiteral("arg=method:GetConnectedInputDevices:inventory:ay:out"),
                QStringLiteral("arg=method:GetConnectedInputDevices:observedAtMs:t:out"),
                QStringLiteral("method=PreviewDisplayConfiguration"),
                QStringLiteral("arg=method:PreviewDisplayConfiguration:expectedRevision:t:in"),
                QStringLiteral("arg=method:PreviewDisplayConfiguration:expectedCatalogDigest:s:in"),
                QStringLiteral("arg=method:PreviewDisplayConfiguration:expectedActionCatalogDigest:s:in"),
                QStringLiteral("arg=method:PreviewDisplayConfiguration:profile:ay:in"),
                QStringLiteral("arg=method:PreviewDisplayConfiguration:timeoutSeconds:u:in"),
                QStringLiteral("arg=method:PreviewDisplayConfiguration:previewRevision:t:out"),
                QStringLiteral("arg=method:PreviewDisplayConfiguration:confirmationToken:s:out"),
                QStringLiteral("arg=method:PreviewDisplayConfiguration:deadlineMs:t:out"),
                QStringLiteral("arg=method:PreviewDisplayConfiguration:generationDigest:s:out"),
                QStringLiteral("method=GetPendingDisplayConfirmation"),
                QStringLiteral("arg=method:GetPendingDisplayConfirmation:confirmationToken:s:out"),
                QStringLiteral("arg=method:GetPendingDisplayConfirmation:previewRevision:t:out"),
                QStringLiteral("arg=method:GetPendingDisplayConfirmation:deadlineMs:t:out"),
                QStringLiteral("arg=method:GetPendingDisplayConfirmation:generationDigest:s:out"),
                QStringLiteral("method=ConfirmDisplayConfiguration"),
                QStringLiteral("arg=method:ConfirmDisplayConfiguration:confirmationToken:s:in"),
                QStringLiteral("arg=method:ConfirmDisplayConfiguration:revision:t:out"),
                QStringLiteral("arg=method:ConfirmDisplayConfiguration:generationDigest:s:out"),
                QStringLiteral("method=RevertDisplayConfiguration"),
                QStringLiteral("arg=method:RevertDisplayConfiguration:confirmationToken:s:in"),
                QStringLiteral("arg=method:RevertDisplayConfiguration:revision:t:out"),
                QStringLiteral("method=RetrySharedBorderSync"),
                QStringLiteral("method=RetrySharedSpacingSync"),
            }
        );
    }

    void compositor2AuthorityContract()
    {
        const auto path = QFileInfo(
            QStringLiteral(HYPRSHELLD_COMPOSITOR_XML)
        ).dir().filePath(QStringLiteral("org.hyprshelld.Compositor2.xml"));
        const auto expected = QString::fromLatin1(R"dbus(
node=/org/hyprshelld/Compositor2
interface=org.hyprshelld.Compositor2
property=Available:b:read
annotation=property:Available:org.freedesktop.DBus.Property.EmitsChangedSignal:true
property=Writable:b:read
annotation=property:Writable:org.freedesktop.DBus.Property.EmitsChangedSignal:true
property=AuthorityId:s:read
annotation=property:AuthorityId:org.freedesktop.DBus.Property.EmitsChangedSignal:true
property=Revision:t:read
annotation=property:Revision:org.freedesktop.DBus.Property.EmitsChangedSignal:true
property=LoadState:s:read
annotation=property:LoadState:org.freedesktop.DBus.Property.EmitsChangedSignal:true
property=ManagementState:s:read
annotation=property:ManagementState:org.freedesktop.DBus.Property.EmitsChangedSignal:true
property=EntrypointDigest:s:read
annotation=property:EntrypointDigest:org.freedesktop.DBus.Property.EmitsChangedSignal:true
property=CatalogDigest:s:read
annotation=property:CatalogDigest:org.freedesktop.DBus.Property.EmitsChangedSignal:true
property=ActionCatalogDigest:s:read
annotation=property:ActionCatalogDigest:org.freedesktop.DBus.Property.EmitsChangedSignal:true
property=AppliedRevision:t:read
annotation=property:AppliedRevision:org.freedesktop.DBus.Property.EmitsChangedSignal:true
property=ApplyState:s:read
annotation=property:ApplyState:org.freedesktop.DBus.Property.EmitsChangedSignal:true
property=RequiredActivation:s:read
annotation=property:RequiredActivation:org.freedesktop.DBus.Property.EmitsChangedSignal:true
property=GenerationDigest:s:read
annotation=property:GenerationDigest:org.freedesktop.DBus.Property.EmitsChangedSignal:true
property=DisplayConfirmationState:s:read
annotation=property:DisplayConfirmationState:org.freedesktop.DBus.Property.EmitsChangedSignal:true
property=DisplayConfirmationRevision:t:read
annotation=property:DisplayConfirmationRevision:org.freedesktop.DBus.Property.EmitsChangedSignal:true
property=DisplayConfirmationDeadlineMs:t:read
annotation=property:DisplayConfirmationDeadlineMs:org.freedesktop.DBus.Property.EmitsChangedSignal:true
property=DisplayConfirmationGeneration:s:read
annotation=property:DisplayConfirmationGeneration:org.freedesktop.DBus.Property.EmitsChangedSignal:true
property=SharedBorderSyncState:s:read
annotation=property:SharedBorderSyncState:org.freedesktop.DBus.Property.EmitsChangedSignal:true
property=SharedBorderSourceRevision:t:read
annotation=property:SharedBorderSourceRevision:org.freedesktop.DBus.Property.EmitsChangedSignal:true
property=SharedBorderSyncError:s:read
annotation=property:SharedBorderSyncError:org.freedesktop.DBus.Property.EmitsChangedSignal:true
property=SharedSpacingSyncState:s:read
annotation=property:SharedSpacingSyncState:org.freedesktop.DBus.Property.EmitsChangedSignal:true
property=SharedSpacingSourceRevision:t:read
annotation=property:SharedSpacingSourceRevision:org.freedesktop.DBus.Property.EmitsChangedSignal:true
property=SharedSpacingSyncError:s:read
annotation=property:SharedSpacingSyncError:org.freedesktop.DBus.Property.EmitsChangedSignal:true
method=GetSnapshot
arg=method:GetSnapshot:snapshot:ay:out
arg=method:GetSnapshot:authorityId:s:out
arg=method:GetSnapshot:revision:t:out
arg=method:GetSnapshot:catalogDigest:s:out
arg=method:GetSnapshot:actionCatalogDigest:s:out
method=GetOptionCatalog
arg=method:GetOptionCatalog:optionCatalog:ay:out
arg=method:GetOptionCatalog:authorityId:s:out
arg=method:GetOptionCatalog:revision:t:out
arg=method:GetOptionCatalog:catalogDigest:s:out
arg=method:GetOptionCatalog:actionCatalogDigest:s:out
method=GetActionCatalog
arg=method:GetActionCatalog:actionCatalog:ay:out
arg=method:GetActionCatalog:configSchema:ay:out
arg=method:GetActionCatalog:authorityId:s:out
arg=method:GetActionCatalog:revision:t:out
arg=method:GetActionCatalog:catalogDigest:s:out
arg=method:GetActionCatalog:actionCatalogDigest:s:out
arg=method:GetActionCatalog:configSchemaDigest:s:out
method=ReplaceSnapshot
arg=method:ReplaceSnapshot:expectedAuthorityId:s:in
arg=method:ReplaceSnapshot:expectedRevision:t:in
arg=method:ReplaceSnapshot:expectedCatalogDigest:s:in
arg=method:ReplaceSnapshot:expectedActionCatalogDigest:s:in
arg=method:ReplaceSnapshot:candidateSnapshot:ay:in
arg=method:ReplaceSnapshot:revision:t:out
method=Apply
arg=method:Apply:expectedAuthorityId:s:in
arg=method:Apply:expectedRevision:t:in
arg=method:Apply:expectedCatalogDigest:s:in
arg=method:Apply:expectedActionCatalogDigest:s:in
arg=method:Apply:appliedRevision:t:out
arg=method:Apply:generationDigest:s:out
method=Recover
arg=method:Recover:expectedAuthorityId:s:in
arg=method:Recover:expectedRevision:t:in
arg=method:Recover:expectedCatalogDigest:s:in
arg=method:Recover:expectedActionCatalogDigest:s:in
arg=method:Recover:revision:t:out
arg=method:Recover:appliedRevision:t:out
arg=method:Recover:generationDigest:s:out
method=AdoptManagedConfiguration
arg=method:AdoptManagedConfiguration:expectedAuthorityId:s:in
arg=method:AdoptManagedConfiguration:expectedRevision:t:in
arg=method:AdoptManagedConfiguration:expectedCatalogDigest:s:in
arg=method:AdoptManagedConfiguration:expectedActionCatalogDigest:s:in
arg=method:AdoptManagedConfiguration:expectedEntrypointDigest:s:in
arg=method:AdoptManagedConfiguration:appliedRevision:t:out
arg=method:AdoptManagedConfiguration:generationDigest:s:out
arg=method:AdoptManagedConfiguration:entrypointDigest:s:out
method=GetConnectedDisplays
arg=method:GetConnectedDisplays:topology:ay:out
arg=method:GetConnectedDisplays:observedAtMs:t:out
method=GetConnectedInputDevices
arg=method:GetConnectedInputDevices:inventory:ay:out
arg=method:GetConnectedInputDevices:observedAtMs:t:out
method=PreviewDisplayConfiguration
arg=method:PreviewDisplayConfiguration:expectedAuthorityId:s:in
arg=method:PreviewDisplayConfiguration:expectedRevision:t:in
arg=method:PreviewDisplayConfiguration:expectedCatalogDigest:s:in
arg=method:PreviewDisplayConfiguration:expectedActionCatalogDigest:s:in
arg=method:PreviewDisplayConfiguration:profile:ay:in
arg=method:PreviewDisplayConfiguration:timeoutSeconds:u:in
arg=method:PreviewDisplayConfiguration:previewRevision:t:out
arg=method:PreviewDisplayConfiguration:confirmationToken:s:out
arg=method:PreviewDisplayConfiguration:deadlineMs:t:out
arg=method:PreviewDisplayConfiguration:generationDigest:s:out
method=GetPendingDisplayConfirmation
arg=method:GetPendingDisplayConfirmation:authorityId:s:out
arg=method:GetPendingDisplayConfirmation:confirmationToken:s:out
arg=method:GetPendingDisplayConfirmation:previewRevision:t:out
arg=method:GetPendingDisplayConfirmation:deadlineMs:t:out
arg=method:GetPendingDisplayConfirmation:generationDigest:s:out
method=ConfirmDisplayConfiguration
arg=method:ConfirmDisplayConfiguration:expectedAuthorityId:s:in
arg=method:ConfirmDisplayConfiguration:expectedPreviewRevision:t:in
arg=method:ConfirmDisplayConfiguration:confirmationToken:s:in
arg=method:ConfirmDisplayConfiguration:revision:t:out
arg=method:ConfirmDisplayConfiguration:generationDigest:s:out
method=RevertDisplayConfiguration
arg=method:RevertDisplayConfiguration:expectedAuthorityId:s:in
arg=method:RevertDisplayConfiguration:expectedPreviewRevision:t:in
arg=method:RevertDisplayConfiguration:confirmationToken:s:in
arg=method:RevertDisplayConfiguration:revision:t:out
method=RetrySharedBorderSync
arg=method:RetrySharedBorderSync:expectedAuthorityId:s:in
arg=method:RetrySharedBorderSync:expectedRevision:t:in
arg=method:RetrySharedBorderSync:expectedCatalogDigest:s:in
arg=method:RetrySharedBorderSync:expectedActionCatalogDigest:s:in
method=RetrySharedSpacingSync
arg=method:RetrySharedSpacingSync:expectedAuthorityId:s:in
arg=method:RetrySharedSpacingSync:expectedRevision:t:in
arg=method:RetrySharedSpacingSync:expectedCatalogDigest:s:in
arg=method:RetrySharedSpacingSync:expectedActionCatalogDigest:s:in
method=GetRestartPlan
arg=method:GetRestartPlan:expectedAuthorityId:s:in
arg=method:GetRestartPlan:expectedRevision:t:in
arg=method:GetRestartPlan:expectedCatalogDigest:s:in
arg=method:GetRestartPlan:expectedActionCatalogDigest:s:in
arg=method:GetRestartPlan:operationKind:s:in
arg=method:GetRestartPlan:requestId:s:in
arg=method:GetRestartPlan:plan:ay:out
arg=method:GetRestartPlan:planId:s:out
arg=method:GetRestartPlan:planDigest:s:out
arg=method:GetRestartPlan:disclosureVersion:u:out
method=AuthorizeRestart
arg=method:AuthorizeRestart:expectedAuthorityId:s:in
arg=method:AuthorizeRestart:expectedRevision:t:in
arg=method:AuthorizeRestart:expectedCatalogDigest:s:in
arg=method:AuthorizeRestart:expectedActionCatalogDigest:s:in
arg=method:AuthorizeRestart:requestId:s:in
arg=method:AuthorizeRestart:planId:s:in
arg=method:AuthorizeRestart:planDigest:s:in
arg=method:AuthorizeRestart:disclosureVersion:u:in
arg=method:AuthorizeRestart:operationId:s:out
arg=method:AuthorizeRestart:status:ay:out
method=GetActiveRestart
arg=method:GetActiveRestart:active:b:out
arg=method:GetActiveRestart:authorityId:s:out
arg=method:GetActiveRestart:revision:t:out
arg=method:GetActiveRestart:operationId:s:out
arg=method:GetActiveRestart:status:ay:out
method=GetRestartOperation
arg=method:GetRestartOperation:operationId:s:in
arg=method:GetRestartOperation:found:b:out
arg=method:GetRestartOperation:availability:s:out
arg=method:GetRestartOperation:authorityId:s:out
arg=method:GetRestartOperation:revision:t:out
arg=method:GetRestartOperation:payloadDigest:s:out
arg=method:GetRestartOperation:payload:ay:out
method=LookupRestartRequest
arg=method:LookupRestartRequest:requestId:s:in
arg=method:LookupRestartRequest:found:b:out
arg=method:LookupRestartRequest:availability:s:out
arg=method:LookupRestartRequest:operationId:s:out
arg=method:LookupRestartRequest:authorityId:s:out
arg=method:LookupRestartRequest:revision:t:out
arg=method:LookupRestartRequest:payloadDigest:s:out
arg=method:LookupRestartRequest:payload:ay:out
method=GetRestartResults
arg=method:GetRestartResults:afterResultSequence:t:in
arg=method:GetRestartResults:limit:u:in
arg=method:GetRestartResults:unreadResults:a(tstssay):out
arg=method:GetRestartResults:nextResultSequence:t:out
arg=method:GetRestartResults:hasMore:b:out
arg=method:GetRestartResults:hasLatestAcknowledged:b:out
arg=method:GetRestartResults:latestSequence:t:out
arg=method:GetRestartResults:latestAuthorityId:s:out
arg=method:GetRestartResults:latestRevision:t:out
arg=method:GetRestartResults:latestOperationId:s:out
arg=method:GetRestartResults:latestResultDigest:s:out
arg=method:GetRestartResults:latestStatus:ay:out
annotation=method:GetRestartResults:org.qtproject.QtDBus.QtTypeName.Out0:HyprShelld::Compositor2::RestartResultRows
method=AcknowledgeRestartResult
arg=method:AcknowledgeRestartResult:authorityId:s:in
arg=method:AcknowledgeRestartResult:revision:t:in
arg=method:AcknowledgeRestartResult:operationId:s:in
arg=method:AcknowledgeRestartResult:resultDigest:s:in
method=GetRepairStatus
arg=method:GetRepairStatus:status:ay:out
arg=method:GetRepairStatus:repairId:s:out
arg=method:GetRepairStatus:observedAuthorityKind:s:out
arg=method:GetRepairStatus:observedAuthorityId:s:out
arg=method:GetRepairStatus:observedRevision:t:out
arg=method:GetRepairStatus:statusDigest:s:out
method=GetExactPriorRestorePlan
arg=method:GetExactPriorRestorePlan:expectedRepairId:s:in
arg=method:GetExactPriorRestorePlan:expectedAuthorityKind:s:in
arg=method:GetExactPriorRestorePlan:expectedAuthorityId:s:in
arg=method:GetExactPriorRestorePlan:expectedRevision:t:in
arg=method:GetExactPriorRestorePlan:expectedStatusDigest:s:in
arg=method:GetExactPriorRestorePlan:requestId:s:in
arg=method:GetExactPriorRestorePlan:plan:ay:out
arg=method:GetExactPriorRestorePlan:planId:s:out
arg=method:GetExactPriorRestorePlan:planDigest:s:out
arg=method:GetExactPriorRestorePlan:disclosureVersion:u:out
method=RestoreExactPrior
arg=method:RestoreExactPrior:expectedRepairId:s:in
arg=method:RestoreExactPrior:expectedAuthorityKind:s:in
arg=method:RestoreExactPrior:expectedAuthorityId:s:in
arg=method:RestoreExactPrior:expectedRevision:t:in
arg=method:RestoreExactPrior:expectedStatusDigest:s:in
arg=method:RestoreExactPrior:requestId:s:in
arg=method:RestoreExactPrior:planId:s:in
arg=method:RestoreExactPrior:planDigest:s:in
arg=method:RestoreExactPrior:disclosureVersion:u:in
arg=method:RestoreExactPrior:restoredAuthorityKind:s:out
arg=method:RestoreExactPrior:restoredAuthorityId:s:out
arg=method:RestoreExactPrior:restoredRevision:t:out
arg=method:RestoreExactPrior:repairResultId:s:out
arg=method:RestoreExactPrior:repairResultDigest:s:out
method=GetResetPlan
arg=method:GetResetPlan:expectedRepairId:s:in
arg=method:GetResetPlan:expectedAuthorityKind:s:in
arg=method:GetResetPlan:expectedAuthorityId:s:in
arg=method:GetResetPlan:expectedRevision:t:in
arg=method:GetResetPlan:expectedStatusDigest:s:in
arg=method:GetResetPlan:requestId:s:in
arg=method:GetResetPlan:plan:ay:out
arg=method:GetResetPlan:planId:s:out
arg=method:GetResetPlan:planDigest:s:out
arg=method:GetResetPlan:disclosureVersion:u:out
method=ResetAuthorityToDefaults
arg=method:ResetAuthorityToDefaults:expectedRepairId:s:in
arg=method:ResetAuthorityToDefaults:expectedAuthorityKind:s:in
arg=method:ResetAuthorityToDefaults:expectedAuthorityId:s:in
arg=method:ResetAuthorityToDefaults:expectedRevision:t:in
arg=method:ResetAuthorityToDefaults:expectedStatusDigest:s:in
arg=method:ResetAuthorityToDefaults:requestId:s:in
arg=method:ResetAuthorityToDefaults:planId:s:in
arg=method:ResetAuthorityToDefaults:planDigest:s:in
arg=method:ResetAuthorityToDefaults:disclosureVersion:u:in
arg=method:ResetAuthorityToDefaults:newAuthorityKind:s:out
arg=method:ResetAuthorityToDefaults:newAuthorityId:s:out
arg=method:ResetAuthorityToDefaults:newRevision:t:out
arg=method:ResetAuthorityToDefaults:backupId:s:out
arg=method:ResetAuthorityToDefaults:repairResultId:s:out
arg=method:ResetAuthorityToDefaults:repairResultDigest:s:out
method=LookupRepairRequest
arg=method:LookupRepairRequest:requestId:s:in
arg=method:LookupRepairRequest:found:b:out
arg=method:LookupRepairRequest:availability:s:out
arg=method:LookupRepairRequest:repairId:s:out
arg=method:LookupRepairRequest:repairResultId:s:out
arg=method:LookupRepairRequest:outcome:s:out
arg=method:LookupRepairRequest:authorityKind:s:out
arg=method:LookupRepairRequest:authorityId:s:out
arg=method:LookupRepairRequest:revision:t:out
arg=method:LookupRepairRequest:payloadDigest:s:out
arg=method:LookupRepairRequest:payload:ay:out
method=GetRepairResults
arg=method:GetRepairResults:afterResultSequence:t:in
arg=method:GetRepairResults:limit:u:in
arg=method:GetRepairResults:unreadResults:a(tsssssstsay):out
arg=method:GetRepairResults:nextResultSequence:t:out
arg=method:GetRepairResults:hasMore:b:out
arg=method:GetRepairResults:hasLatestAcknowledged:b:out
arg=method:GetRepairResults:latestSequence:t:out
arg=method:GetRepairResults:latestRepairResultId:s:out
arg=method:GetRepairResults:latestRepairId:s:out
arg=method:GetRepairResults:latestRequestId:s:out
arg=method:GetRepairResults:latestOutcome:s:out
arg=method:GetRepairResults:latestAuthorityKind:s:out
arg=method:GetRepairResults:latestAuthorityId:s:out
arg=method:GetRepairResults:latestRevision:t:out
arg=method:GetRepairResults:latestResultDigest:s:out
arg=method:GetRepairResults:latestResult:ay:out
annotation=method:GetRepairResults:org.qtproject.QtDBus.QtTypeName.Out0:HyprShelld::Compositor2::RepairResultRows
method=AcknowledgeRepairResult
arg=method:AcknowledgeRepairResult:repairId:s:in
arg=method:AcknowledgeRepairResult:repairResultId:s:in
arg=method:AcknowledgeRepairResult:authorityKind:s:in
arg=method:AcknowledgeRepairResult:authorityId:s:in
arg=method:AcknowledgeRepairResult:revision:t:in
arg=method:AcknowledgeRepairResult:resultDigest:s:in
)dbus").trimmed().split(u'\n');
        compareContract(path, expected);
    }

    void compositor2ResultRowMarshallingContract()
    {
        using namespace HyprShelld::Compositor2;

        const auto restartRowType = qDBusRegisterMetaType<RestartResultRow>();
        const auto restartRowsType = qDBusRegisterMetaType<RestartResultRows>();
        const auto repairRowType = qDBusRegisterMetaType<RepairResultRow>();
        const auto repairRowsType = qDBusRegisterMetaType<RepairResultRows>();

        QCOMPARE(
            QByteArray(QDBusMetaType::typeToSignature(restartRowType)),
            QByteArrayLiteral("(tstssay)")
        );
        QCOMPARE(
            QByteArray(QDBusMetaType::typeToSignature(restartRowsType)),
            QByteArrayLiteral("a(tstssay)")
        );
        QCOMPARE(
            QByteArray(QDBusMetaType::typeToSignature(repairRowType)),
            QByteArrayLiteral("(tsssssstsay)")
        );
        QCOMPARE(
            QByteArray(QDBusMetaType::typeToSignature(repairRowsType)),
            QByteArrayLiteral("a(tsssssstsay)")
        );

        const RestartResultRows restartRows{{
            .sequence = 7,
            .authorityId = QStringLiteral("0123456789abcdef0123456789abcdef"),
            .revision = 11,
            .operationId = QStringLiteral("11111111111111111111111111111111"),
            .resultDigest = QString(64, QLatin1Char('a')),
            .status = QByteArrayLiteral("{\"outcome\":\"complete\"}\n"),
        }};
        const RepairResultRows repairRows{{
            .sequence = 13,
            .repairResultId = QStringLiteral("22222222222222222222222222222222"),
            .repairId = QStringLiteral("33333333333333333333333333333333"),
            .requestId = QStringLiteral("44444444444444444444444444444444"),
            .outcome = QStringLiteral("restored"),
            .authorityKind = QStringLiteral("v2"),
            .authorityId = QStringLiteral("55555555555555555555555555555555"),
            .revision = 17,
            .resultDigest = QString(64, QLatin1Char('b')),
            .result = QByteArrayLiteral("{\"outcome\":\"restored\"}\n"),
        }};

        const auto restartVariant = QVariant::fromValue(restartRows);
        const auto repairVariant = QVariant::fromValue(repairRows);
        QCOMPARE(restartVariant.metaType(), restartRowsType);
        QCOMPARE(repairVariant.metaType(), repairRowsType);
        QVERIFY(qvariant_cast<RestartResultRows>(restartVariant) == restartRows);
        QVERIFY(qvariant_cast<RepairResultRows>(repairVariant) == repairRows);
    }


    void compositor2AuthorityDocumentationContract()
    {
        const auto compositor1Path = QStringLiteral(HYPRSHELLD_COMPOSITOR_XML);
        const auto compositor2Path = QFileInfo(compositor1Path).dir().filePath(
            QStringLiteral("org.hyprshelld.Compositor2.xml")
        );
        QString error;

        const auto compositor1 = readTextFile(compositor1Path, error).simplified();
        QVERIFY2(error.isEmpty(), qPrintable(error));
        QVERIFY(compositor1.contains(QStringLiteral(
            "never opens a revision-only mutation window"
        )));
        QVERIFY(compositor1.contains(QStringLiteral(
            "Available and Writable remain permanently false"
        )));
        QVERIFY(compositor1.contains(QStringLiteral(
            "no legacy desired-state property tuple is authoritative"
        )));
        QVERIFY(compositor1.contains(QStringLiteral(
            "Legacy sentinel value: permanently false once Compositor2 is present"
        )));
        QVERIFY(compositor1.contains(QStringLiteral(
            "exact readable-method allowlist is GetConnectedDisplays and GetConnectedInputDevices"
        )));
        QVERIFY(compositor1.contains(QStringLiteral(
            "Every other method, including all desired-state reads, catalog reads, confirmation-capability reads, and every mutation, permanently returns org.hyprshelld.Compositor1.Error.UpgradeRequired"
        )));
        QVERIFY(compositor1.contains(QStringLiteral(
            "org.hyprshelld.Compositor1.Error.UpgradeRequired"
        )));
        QVERIFY(!compositor1.contains(QStringLiteral(
            "runtime observations such as"
        )));
        QVERIFY(!compositor1.contains(QStringLiteral(
            "GetConnectedInputDevices may remain readable"
        )));
        QVERIFY(compositor1.contains(QStringLiteral(
            "must not fall back to this interface"
        )));
        QVERIFY(compositor1.contains(QStringLiteral(
            "does not widen this exhaustive allowlist"
        )));

        const auto compositor2 = readTextFile(compositor2Path, error).simplified();
        QVERIFY2(error.isEmpty(), qPrintable(error));
        QVERIFY(compositor2.contains(QStringLiteral(
            "This dormant interface is served by the existing org.hyprshelld.Compositor1 bus owner"
        )));
        QVERIFY(compositor2.contains(QStringLiteral(
            "store lease, writer, or D-Bus activation name"
        )));

        const auto replace = methodDocumentation(
            compositor2Path, QStringLiteral("ReplaceSnapshot"), error
        );
        QVERIFY2(error.isEmpty(), qPrintable(error));
        QVERIFY(replace.contains(QStringLiteral(
            "candidate must embed exactly expectedAuthorityId and expectedRevision"
        )));
        QVERIFY(replace.contains(QStringLiteral(
            "AuthorityId is checked before either catalog digest and before revision"
        )));
        QVERIFY(replace.contains(QStringLiteral(
            "current verified Config1 projection"
        )));
        QVERIFY(replace.contains(QStringLiteral(
            "exact unique final protected maximized-window rule"
        )));

        const auto optionCatalog = methodDocumentation(
            compositor2Path, QStringLiteral("GetOptionCatalog"), error
        );
        QVERIFY2(error.isEmpty(), qPrintable(error));
        QVERIFY(optionCatalog.contains(QStringLiteral(
            "one coherent retained authority tuple"
        )));
        QVERIFY(optionCatalog.contains(QStringLiteral(
            "authorityId, revision, catalogDigest, and actionCatalogDigest"
        )));

        const auto actionCatalog = methodDocumentation(
            compositor2Path, QStringLiteral("GetActionCatalog"), error
        );
        QVERIFY2(error.isEmpty(), qPrintable(error));
        QVERIFY(actionCatalog.contains(QStringLiteral(
            "action catalog is bounded to 1 MiB and the schema to 2 MiB"
        )));
        QVERIFY(actionCatalog.contains(QStringLiteral(
            "never reopens their source paths"
        )));
        QVERIFY(actionCatalog.contains(QStringLiteral(
            "one coherent retained authority tuple"
        )));

        const auto inputDevices = methodDocumentation(
            compositor2Path, QStringLiteral("GetConnectedInputDevices"), error
        );
        QVERIFY2(error.isEmpty(), qPrintable(error));
        QVERIFY(inputDevices.contains(QStringLiteral(
            "fresh authenticated `j/devices` query"
        )));
        QVERIFY(inputDevices.contains(QStringLiteral(
            "old observations are not cached as current"
        )));

        for (const auto &methodName : {
                 QStringLiteral("ConfirmDisplayConfiguration"),
                 QStringLiteral("RevertDisplayConfiguration")}) {
            const auto documentation = methodDocumentation(
                compositor2Path, methodName, error
            );
            QVERIFY2(error.isEmpty(), qPrintable(error));
            QVERIFY2(documentation.contains(QStringLiteral(
                         "expectedAuthorityId"
                     )), qPrintable(methodName));
            QVERIFY2(documentation.contains(QStringLiteral(
                         "expectedPreviewRevision"
                     )), qPrintable(methodName));
            QVERIFY2(documentation.contains(QStringLiteral(
                         "org.hyprshelld.Compositor2.Error.StaleAuthority"
                     )), qPrintable(methodName));
            QVERIFY2(documentation.contains(QStringLiteral(
                         "org.hyprshelld.Compositor2.Error.StaleRevision"
                     )), qPrintable(methodName));
            QVERIFY2(documentation.contains(QStringLiteral(
                         "catalog digest, and action-catalog digest"
                     )), qPrintable(methodName));
            QVERIFY2(documentation.contains(QStringLiteral(
                         "rechecked immediately before any Desired write"
                     )), qPrintable(methodName));
        }

        const auto restartPlan = methodDocumentation(
            compositor2Path, QStringLiteral("GetRestartPlan"), error
        );
        QVERIFY2(error.isEmpty(), qPrintable(error));
        QVERIFY(restartPlan.contains(QStringLiteral(
            "operationKind apply or recovery"
        )));
        QVERIFY(restartPlan.contains(QStringLiteral(
            "at most 64 unexpired plans and 4 MiB"
        )));
        QVERIFY(restartPlan.contains(QStringLiteral("PlanCapacity")));

        const auto authorizeRestart = methodDocumentation(
            compositor2Path, QStringLiteral("AuthorizeRestart"), error
        );
        QVERIFY2(error.isEmpty(), qPrintable(error));
        QVERIFY(authorizeRestart.contains(QStringLiteral(
            "prepared-authorized"
        )));
        QVERIFY(authorizeRestart.contains(QStringLiteral("StalePlan")));
        QVERIFY(authorizeRestart.contains(QStringLiteral(
            "request lookup precedes current-authority CAS"
        )));

        for (const auto &methodName : {
                 QStringLiteral("GetRestartOperation"),
                 QStringLiteral("LookupRestartRequest")}) {
            const auto documentation = methodDocumentation(
                compositor2Path, methodName, error
            );
            QVERIFY2(error.isEmpty(), qPrintable(error));
            QVERIFY2(documentation.contains(QStringLiteral(
                         "active, unread-result, latest-acknowledged"
                     )), qPrintable(methodName));
            QVERIFY2(documentation.contains(QStringLiteral("tombstone-only")),
                     qPrintable(methodName));
            QVERIFY2(documentation.contains(QStringLiteral("payloadDigest")),
                     qPrintable(methodName));
            QVERIFY2(documentation.contains(QStringLiteral("found is false")),
                     qPrintable(methodName));
        }

        const auto restartResults = methodDocumentation(
            compositor2Path, QStringLiteral("GetRestartResults"), error
        );
        QVERIFY2(error.isEmpty(), qPrintable(error));
        QVERIFY(restartResults.contains(QStringLiteral("limit is 1 through 64")));
        QVERIFY(restartResults.contains(QStringLiteral("at most 4 MiB")));
        QVERIFY(restartResults.contains(QStringLiteral(
            "latest acknowledged result is returned explicitly"
        )));

        const auto acknowledgeRestart = methodDocumentation(
            compositor2Path, QStringLiteral("AcknowledgeRestartResult"), error
        );
        QVERIFY2(error.isEmpty(), qPrintable(error));
        QVERIFY(acknowledgeRestart.contains(QStringLiteral("idempotent")));
        QVERIFY(acknowledgeRestart.contains(QStringLiteral(
            "retains that result as the explicitly queryable latest acknowledged"
        )));

        for (const auto &methodName : {
                 QStringLiteral("RetrySharedBorderSync"),
                 QStringLiteral("RetrySharedSpacingSync")}) {
            const auto documentation = methodDocumentation(
                compositor2Path, methodName, error
            );
            QVERIFY2(error.isEmpty(), qPrintable(error));
            QVERIFY2(documentation.contains(QStringLiteral(
                         "CAS errors"
                     )), qPrintable(methodName));
            QVERIFY2(documentation.contains(QStringLiteral(
                         "org.hyprshelld.Compositor2.Error.StaleAuthority"
                     )), qPrintable(methodName));
            QVERIFY2(documentation.contains(QStringLiteral(
                         "org.hyprshelld.Compositor2.Error.StaleRevision"
                     )), qPrintable(methodName));
        }

        const auto status = methodDocumentation(
            compositor2Path, QStringLiteral("GetRepairStatus"), error
        );
        QVERIFY2(error.isEmpty(), qPrintable(error));
        QVERIFY(status.contains(QStringLiteral(
            "observedAuthorityKind is exactly v1, v2, absent, or unreadable"
        )));
        QVERIFY(status.contains(QStringLiteral(
            "V1 carries its exact observed revision without an invented authority ID"
        )));
        QVERIFY(status.contains(QStringLiteral(
            "V2 carries its exact authority ID and revision"
        )));
        QVERIFY(status.contains(QStringLiteral(
            "absent and unreadable carry an empty ID and zero revision"
        )));
        QVERIFY(status.contains(QStringLiteral("degraded repair-only mode")));

        const auto restorePlan = methodDocumentation(
            compositor2Path, QStringLiteral("GetExactPriorRestorePlan"), error
        );
        QVERIFY2(error.isEmpty(), qPrintable(error));
        QVERIFY(restorePlan.contains(QStringLiteral(
            "complete prior authority is provably recoverable"
        )));
        QVERIFY(restorePlan.contains(QStringLiteral(
            "client-generated request ID before confirmation"
        )));
        QVERIFY(restorePlan.contains(QStringLiteral(
            "does not restore or mutate any state"
        )));

        const auto restore = methodDocumentation(
            compositor2Path, QStringLiteral("RestoreExactPrior"), error
        );
        QVERIFY2(error.isEmpty(), qPrintable(error));
        QVERIFY(restore.contains(QStringLiteral(
            "complete exact prior authority"
        )));
        QVERIFY(restore.contains(QStringLiteral(
            "permanent request tombstone"
        )));
        QVERIFY(restore.contains(QStringLiteral(
            "request ID, plan ID, plan digest, and disclosure version"
        )));
        QVERIFY(restore.contains(QStringLiteral(
            "restored authority kind, ID, and revision"
        )));
        QVERIFY(restore.contains(QStringLiteral(
            "repair-result ID and digest"
        )));
        QVERIFY(restore.contains(QStringLiteral("never starts Hyprland")));

        const auto resetPlan = methodDocumentation(
            compositor2Path, QStringLiteral("GetResetPlan"), error
        );
        QVERIFY2(error.isEmpty(), qPrintable(error));
        QVERIFY(resetPlan.contains(QStringLiteral(
            "only when no complete exact prior authority is provably restorable"
        )));
        QVERIFY(resetPlan.contains(QStringLiteral(
            "client-generated request ID before confirmation"
        )));
        QVERIFY(resetPlan.contains(QStringLiteral(
            "org.hyprshelld.Compositor2.Error.ExactPriorRequired"
        )));

        const auto reset = methodDocumentation(
            compositor2Path, QStringLiteral("ResetAuthorityToDefaults"), error
        );
        QVERIFY2(error.isEmpty(), qPrintable(error));
        QVERIFY(reset.contains(QStringLiteral(
            "complete descriptor-relative, fsynced archive"
        )));
        QVERIFY(reset.contains(QStringLiteral(
            "authority and revision, exact status and plan digests"
        )));
        QVERIFY(reset.contains(QStringLiteral(
            "request ID is supplied exactly once and precedes the plan tuple"
        )));
        QVERIFY(reset.contains(QStringLiteral(
            "expectedAuthorityKind v1, absent, or unreadable"
        )));
        QVERIFY(reset.contains(QStringLiteral(
            "no Applied or LastGood claim"
        )));
        QVERIFY(reset.contains(QStringLiteral(
            "repair-result ID and digest"
        )));
        QVERIFY(reset.contains(QStringLiteral("never starts Hyprland")));

        const auto lookupRepair = methodDocumentation(
            compositor2Path, QStringLiteral("LookupRepairRequest"), error
        );
        QVERIFY2(error.isEmpty(), qPrintable(error));
        QVERIFY(lookupRepair.contains(QStringLiteral(
            "active, unread-result, latest-acknowledged"
        )));
        QVERIFY(lookupRepair.contains(QStringLiteral("tombstone-only")));
        QVERIFY(lookupRepair.contains(QStringLiteral(
            "repairResultId is already reserved"
        )));
        QVERIFY(lookupRepair.contains(QStringLiteral(
            "never occupies an immutable result body or unread slot"
        )));
        QVERIFY(lookupRepair.contains(QStringLiteral("found is false")));

        const auto repairResults = methodDocumentation(
            compositor2Path, QStringLiteral("GetRepairResults"), error
        );
        QVERIFY2(error.isEmpty(), qPrintable(error));
        QVERIFY(repairResults.contains(QStringLiteral("limit is 1 through 64")));
        QVERIFY(repairResults.contains(QStringLiteral("at most 4 MiB")));
        QVERIFY(repairResults.contains(QStringLiteral(
            "latest acknowledged result is returned explicitly"
        )));

        const auto acknowledgeRepair = methodDocumentation(
            compositor2Path, QStringLiteral("AcknowledgeRepairResult"), error
        );
        QVERIFY2(error.isEmpty(), qPrintable(error));
        QVERIFY(acknowledgeRepair.contains(QStringLiteral("idempotent")));
        QVERIFY(acknowledgeRepair.contains(QStringLiteral(
            "retains that result as the explicitly queryable latest acknowledged"
        )));

        const auto contract = describeContract(compositor2Path, error);
        QVERIFY2(error.isEmpty(), qPrintable(error));
        QCOMPARE(
            contract.count(QStringLiteral(
                "arg=method:ResetAuthorityToDefaults:requestId:s:in"
            )),
            1
        );
        QVERIFY(contract.contains(QStringLiteral("method=GetRestartPlan")));
        QVERIFY(contract.contains(QStringLiteral("method=AuthorizeRestart")));
        QVERIFY(contract.contains(QStringLiteral("method=GetRestartResults")));
        QVERIFY(contract.contains(QStringLiteral("method=GetRepairResults")));
        for (const auto &forbidden : {
                 QStringLiteral("method=AdvanceRestartPhase"),
                 QStringLiteral("method=CancelRestart"),
                 QStringLiteral("method=RestartUnit")}) {
            QVERIFY2(!contract.contains(forbidden), qPrintable(forbidden));
        }

        const auto readmePath = QFileInfo(compositor2Path).dir().filePath(
            QStringLiteral("README.md")
        );
        const auto readme = readTextFile(readmePath, error).simplified();
        QVERIFY2(error.isEmpty(), qPrintable(error));
        QVERIFY(readme.contains(QStringLiteral(
            "`org.hyprshelld.Compositor1` destination at `/org/hyprshelld/Compositor2` with interface `org.hyprshelld.Compositor2`"
        )));
        QVERIFY(readme.contains(QStringLiteral(
            "one activation name, one compositord process, one store lease, and one writer"
        )));
        QVERIFY(readme.contains(QStringLiteral(
            "must not fall back to Compositor1 after `UpgradeRequired`"
        )));
        QVERIFY(readme.contains(QStringLiteral(
            "`Available` and `Writable` properties remain permanently false"
        )));
        QVERIFY(readme.contains(QStringLiteral(
            "no legacy desired-state property tuple is authoritative"
        )));
        QVERIFY(readme.contains(QStringLiteral(
            "exact readable-method allowlist is `GetConnectedDisplays` and `GetConnectedInputDevices`"
        )));
        QVERIFY(readme.contains(QStringLiteral(
            "Every other method, including all desired-state reads, catalog reads, confirmation-capability reads, and every mutation, permanently returns `org.hyprshelld.Compositor1.Error.UpgradeRequired`"
        )));
        QVERIFY(!readme.contains(QStringLiteral(
            "runtime observations may remain readable"
        )));
        QVERIFY(readme.contains(QStringLiteral(
            "reviewed Restart methods are part of that dormant ABI"
        )));
        QVERIFY(readme.contains(QStringLiteral(
            "no public phase-advance, Cancel, or RestartUnit method exists"
        )));
        QVERIFY(!readme.contains(QStringLiteral(
            "Restart authorization methods remain absent"
        )));
        QVERIFY(readme.contains(QStringLiteral(
            "`GetOptionCatalog` returns five values in this exact order"
        )));
        QVERIFY(readme.contains(QStringLiteral(
            "`GetActionCatalog` returns seven values in this exact order"
        )));
        QVERIFY(readme.contains(QStringLiteral(
            "Each getter returns one coherent retained in-memory authority tuple"
        )));
        QVERIFY(readme.contains(QStringLiteral(
            "`availability` is exactly `active`, `unread-result`, `latest-acknowledged`, or `tombstone-only`"
        )));
        QVERIFY(readme.contains(QStringLiteral(
            "`limit` is 1 through 64 and the complete reply is at most 4 MiB"
        )));
        QVERIFY(readme.contains(QStringLiteral(
            "latest acknowledged result is returned explicitly even when no unread result remains"
        )));
        QVERIFY(readme.contains(QStringLiteral(
            "`AcknowledgeRepairResult` binds the exact repair/repair-result/authority-kind"
        )));
        QVERIFY(readme.contains(QStringLiteral(
            "`observedAuthorityKind`, `observedAuthorityId`, `observedRevision`, and `statusDigest"
        )));
        QVERIFY(readme.contains(QStringLiteral(
            "kind is exactly `v1`, `v2`, `absent`, or `unreadable`"
        )));
        QVERIFY(readme.contains(QStringLiteral(
            "`GetExactPriorRestorePlan` has priority"
        )));
        QVERIFY(readme.contains(QStringLiteral(
            "request ID appears exactly once and precedes the plan fields"
        )));
        QVERIFY(readme.contains(QStringLiteral(
            "complete descriptor-relative, fsynced archive"
        )));
        QVERIFY(readme.contains(QStringLiteral(
            "No second repair daemon or writer exists"
        )));
    }

    void compositorSharedVisualDocumentationContract()
    {
        const auto path = QStringLiteral(HYPRSHELLD_COMPOSITOR_XML);
        QString error;

        const auto xml = readTextFile(path, error).simplified();
        QVERIFY2(error.isEmpty(), qPrintable(error));
        QVERIFY(xml.contains(QStringLiteral(
            "zero while the Config1 source is unavailable"
        )));

        const auto replace = methodDocumentation(
            path, QStringLiteral("ReplaceSnapshot"), error
        );
        QVERIFY2(error.isEmpty(), qPrintable(error));
        QVERIFY(replace.contains(QStringLiteral("ControlledByHyprShelld")));
        QVERIFY(replace.contains(QStringLiteral(
            "current verified Config1 projection"
        )));
        QVERIFY(replace.contains(QStringLiteral(
            "retained last verified projection"
        )));
        QVERIFY(replace.contains(QStringLiteral(
            "current desired resolved values"
        )));
        QVERIFY(replace.contains(QStringLiteral(
            "exact unique final protected maximized-window rule"
        )));

        const auto apply = methodDocumentation(
            path, QStringLiteral("Apply"), error
        );
        QVERIFY2(error.isEmpty(), qPrintable(error));
        QVERIFY(apply.contains(QStringLiteral("ControlledByHyprShelld")));
        QVERIFY(apply.contains(
            QStringLiteral("currently verified, available Config1 shared-visual authority")
        ));
        QVERIFY(apply.contains(
            QStringLiteral("even when the last verified border or spacing policy was an override")
        ));
        QVERIFY(apply.contains(
            QStringLiteral("exact protected maximized-window rule")
        ));

        const auto adopt = methodDocumentation(
            path, QStringLiteral("AdoptManagedConfiguration"), error
        );
        QVERIFY2(error.isEmpty(), qPrintable(error));
        QVERIFY(adopt.contains(QStringLiteral("ControlledByHyprShelld")));
        QVERIFY(adopt.contains(
            QStringLiteral("currently verified, available Config1 authority")
        ));
        QVERIFY(adopt.contains(
            QStringLiteral("even when the last verified border or spacing policy was an override")
        ));
        QVERIFY(adopt.contains(
            QStringLiteral("exact protected maximized-window rule")
        ));

        const auto recover = methodDocumentation(
            path, QStringLiteral("Recover"), error
        );
        QVERIFY2(error.isEmpty(), qPrintable(error));
        QVERIFY(recover.contains(
            QStringLiteral("exempt from the shared-visual ownership gate")
        ));
        QVERIFY(recover.contains(
            QStringLiteral("shared-border and shared-spacing policies")
        ));
        QVERIFY(recover.contains(
            QStringLiteral("exact protected maximized-window rule")
        ));
        QVERIFY(!recover.contains(QStringLiteral("ControlledByHyprShelld")));

        const auto retry = methodDocumentation(
            path, QStringLiteral("RetrySharedBorderSync"), error
        );
        QVERIFY2(error.isEmpty(), qPrintable(error));
        QVERIFY(retry.contains(QStringLiteral("Explicitly retries")));
        QVERIFY(retry.contains(QStringLiteral(
            "effective Config1 shared policy or derived compositor border values change"
        )));
        QVERIFY(retry.contains(QStringLiteral("Revision-only change")));
        QVERIFY(retry.contains(QStringLiteral("does not retry")));

        const auto spacingRetry = methodDocumentation(
            path, QStringLiteral("RetrySharedSpacingSync"), error
        );
        QVERIFY2(error.isEmpty(), qPrintable(error));
        QVERIFY(spacingRetry.contains(QStringLiteral("Explicitly retries")));
        QVERIFY(spacingRetry.contains(QStringLiteral(
            "protected maximized-window rule"
        )));
        QVERIFY(spacingRetry.contains(QStringLiteral(
            "Revision-only source update"
        )));
        QVERIFY(spacingRetry.contains(QStringLiteral("without retrying")));

        const auto readmePath = QFileInfo(path).dir().filePath(
            QStringLiteral("README.md")
        );
        const auto readme = readTextFile(readmePath, error).simplified();
        QVERIFY2(error.isEmpty(), qPrintable(error));
        QVERIFY(readme.contains(QStringLiteral(
            "requires a currently verified, available Config1 shared-visual authority"
        )));
        QVERIFY(readme.contains(QStringLiteral(
            "even when the last verified border or spacing policy was an override"
        )));
        QVERIFY(readme.contains(QStringLiteral(
            "Recover is intentionally exempt from the shared-visual ownership gate"
        )));
        QVERIFY(readme.contains(QStringLiteral(
            "current verified Config1 projection, then the retained last verified projection, then the current desired resolved values"
        )));
        QVERIFY(readme.contains(QStringLiteral(
            "exact unique final protected maximized-window rule"
        )));
        QVERIFY(readme.contains(QStringLiteral(
            "SharedSpacingSyncError`, and `RetrySharedSpacingSync`"
        )));
        QVERIFY(readme.contains(QStringLiteral(
            "Config1 projection Revision-only change"
        )));
        QVERIFY(readme.contains(QStringLiteral("does not retry")));
    }

    void compositorInputDeviceDiscoveryContract()
    {
        const auto path = QStringLiteral(HYPRSHELLD_COMPOSITOR_XML);
        QString error;
        const auto method = methodDocumentation(
            path, QStringLiteral("GetConnectedInputDevices"), error
        );
        QVERIFY2(error.isEmpty(), qPrintable(error));
        QVERIFY(method.contains(QStringLiteral(
            "fresh authenticated `j/devices` query"
        )));
        QVERIFY(method.contains(QStringLiteral(
            "Raw runtime addresses are retained only in the private opaque inventory fingerprint and never cross D-Bus"
        )));
        QVERIFY(method.contains(QStringLiteral(
            "independent of desired-state and managed-filesystem authority"
        )));
        QVERIFY(method.contains(QStringLiteral(
            "unavailable while a display confirmation capability still exists or its durable commit is active"
        )));
        QVERIFY(method.contains(QStringLiteral(
            "old observations are not cached as current"
        )));
        for (const auto &name : {
                 QStringLiteral("Unavailable"),
                 QStringLiteral("RuntimeUnavailable"),
                 QStringLiteral("UnsupportedVersion"),
                 QStringLiteral("VerificationFailed")}) {
            QVERIFY2(method.contains(
                         QStringLiteral("org.hyprshelld.Compositor1.Error.")
                         + name
                     ), qPrintable(name));
        }

        const auto readmePath = QFileInfo(path).dir().filePath(
            QStringLiteral("README.md")
        );
        const auto readme = readTextFile(readmePath, error).simplified();
        QVERIFY2(error.isEmpty(), qPrintable(error));
        QVERIFY(readme.contains(QStringLiteral(
            "separate authenticated read-only authority"
        )));
        QVERIFY(readme.contains(QStringLiteral(
            "configured reviewed Hyprland 0.56.x policy"
        )));
        QVERIFY(readme.contains(QStringLiteral(
            "canonical v1 inventory plus its observation time"
        )));
        QVERIFY(readme.contains(QStringLiteral(
            "random compositord-process epoch"
        )));
        QVERIFY(readme.contains(QStringLiteral(
            "does not require an available desired snapshot, managed entrypoint ownership, activation filesystem binding, or successful activation finalization"
        )));
        QVERIFY(readme.contains(QStringLiteral(
            "display confirmation capability still exists or its durable commit is active"
        )));
        QVERIFY(readme.contains(QStringLiteral(
            "terminal failed state without a retained capability does not permanently block later discovery"
        )));
        QVERIFY(readme.contains(QStringLiteral(
            "No old device inventory is cached or returned as current"
        )));
    }
};

QTEST_APPLESS_MAIN(DbusContractTest)

#include "dbus_contract_test.moc"
