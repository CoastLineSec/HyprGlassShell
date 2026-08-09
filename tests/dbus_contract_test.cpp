#include <QFile>
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
                QStringLiteral("property=Revision:t:read"),
                QStringLiteral("annotation=property:Revision:org.freedesktop.DBus.Property.EmitsChangedSignal:true"),
                QStringLiteral("property=RecoveryState:s:read"),
                QStringLiteral("annotation=property:RecoveryState:org.freedesktop.DBus.Property.EmitsChangedSignal:const"),
                QStringLiteral("method=SetBarHeight"),
                QStringLiteral("arg=method:SetBarHeight:height:u:in"),
                QStringLiteral("arg=method:SetBarHeight:revision:t:out"),
                QStringLiteral("method=ResetBarHeight"),
                QStringLiteral("arg=method:ResetBarHeight:revision:t:out"),
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
                QStringLiteral("method=GetSurfacePlan"),
                QStringLiteral("arg=method:GetSurfacePlan:expectedSurfacePlanRevision:t:in"),
                QStringLiteral("arg=method:GetSurfacePlan:surfacePlan:ay:out"),
                QStringLiteral("arg=method:GetSurfacePlan:surfacePlanDigest:s:out"),
            }
        );
    }
};

QTEST_APPLESS_MAIN(DbusContractTest)

#include "dbus_contract_test.moc"
