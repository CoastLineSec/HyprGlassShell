#include "surface_plan.h"

#include "builtin_component_defaults.h"
#include "component_contract.h"
#include "strict_json.h"

#include <QCryptographicHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QRegularExpression>
#include <QSet>
#include <QtEndian>

#include <array>
#include <limits>

namespace HyprShelld::Components {
namespace {

constexpr auto surfacePlanFormatVersion = 1;

const QRegularExpression &digestPattern()
{
    static const QRegularExpression pattern(
        QStringLiteral("^[0-9a-f]{64}$")
    );
    return pattern;
}

const QRegularExpression &uuidV4Pattern()
{
    static const QRegularExpression pattern(QStringLiteral(
        "^[0-9a-f]{8}-[0-9a-f]{4}-4[0-9a-f]{3}-[89ab][0-9a-f]{3}-[0-9a-f]{12}$"
    ));
    return pattern;
}

const QRegularExpression &layoutIdPattern()
{
    static const QRegularExpression pattern(
        QStringLiteral("^[a-z][a-z0-9]*(?:-[a-z0-9]+)*$")
    );
    return pattern;
}

const QRegularExpression &factoryPattern()
{
    static const QRegularExpression pattern(
        QStringLiteral("^[a-z][a-z0-9]*(?:-[a-z0-9]+)*$")
    );
    return pattern;
}

void appendError(
    ValidationErrors &errors,
    QString path,
    QString code,
    QString message
)
{
    errors.append({
        .path = std::move(path),
        .code = std::move(code),
        .message = std::move(message),
    });
}

bool requireExactKeys(
    const QJsonObject &object,
    QStringList expected,
    const QString &path,
    ValidationErrors &errors
)
{
    auto actual = object.keys();
    actual.sort();
    expected.sort();
    if (actual == expected) {
        return true;
    }

    appendError(
        errors,
        path,
        QStringLiteral("surface-plan.closed-object"),
        QStringLiteral("The object contains missing or unsupported fields.")
    );
    return false;
}

bool parseUnsignedRevision(const QString &text, quint64 &revision)
{
    if (text.isEmpty() || (text.size() > 1 && text.front() == u'0')) {
        return false;
    }
    for (const auto character : text) {
        if (character < u'0' || character > u'9') {
            return false;
        }
    }

    bool converted = false;
    revision = text.toULongLong(&converted, 10);
    return converted;
}

QString childPath(const QString &parent, const QString &child)
{
    return parent + QLatin1Char('.') + child;
}

bool parseStringArray(
    const QJsonValue &value,
    const QString &path,
    QStringList &result,
    ValidationErrors &errors
)
{
    if (!value.isArray()) {
        appendError(
            errors,
            path,
            QStringLiteral("surface-plan.array-required"),
            QStringLiteral("Expected an array of instance IDs.")
        );
        return false;
    }

    const auto array = value.toArray();
    if (array.size() > maximumSurfacePlanInstances) {
        appendError(
            errors,
            path,
            QStringLiteral("surface-plan.instance-limit"),
            QStringLiteral("The region contains too many instances.")
        );
        return false;
    }

    result.clear();
    result.reserve(array.size());
    for (qsizetype index = 0; index < array.size(); ++index) {
        const auto entry = array.at(index);
        const auto entryPath = path + QLatin1Char('[')
            + QString::number(index) + QLatin1Char(']');
        if (!entry.isString()
            || !uuidV4Pattern().match(entry.toString()).hasMatch()) {
            appendError(
                errors,
                entryPath,
                QStringLiteral("surface-plan.invalid-instance-id"),
                QStringLiteral("The instance ID must be a canonical lowercase UUIDv4.")
            );
            return false;
        }
        result.append(entry.toString());
    }
    return true;
}

bool parseInstance(
    const QJsonObject &object,
    const QString &path,
    SurfaceInstance &instance,
    ValidationErrors &errors
)
{
    if (!requireExactKeys(
            object,
            {
                QStringLiteral("componentId"),
                QStringLiteral("componentType"),
                QStringLiteral("packageDigest"),
                QStringLiteral("runtime"),
                QStringLiteral("settings"),
            },
            path,
            errors
        )) {
        return false;
    }

    const auto componentId = object.value(QStringLiteral("componentId"));
    const auto componentType = object.value(QStringLiteral("componentType"));
    const auto packageDigest = object.value(QStringLiteral("packageDigest"));
    const auto runtimeValue = object.value(QStringLiteral("runtime"));
    const auto settingsValue = object.value(QStringLiteral("settings"));

    if (!componentId.isString() || !isValidComponentId(componentId.toString())) {
        appendError(
            errors,
            childPath(path, QStringLiteral("componentId")),
            QStringLiteral("surface-plan.invalid-component-id"),
            QStringLiteral("The component ID is invalid.")
        );
        return false;
    }
    if (!componentType.isString()
        || componentType.toString() != QStringLiteral("bar-widget")) {
        appendError(
            errors,
            childPath(path, QStringLiteral("componentType")),
            QStringLiteral("surface-plan.unsupported-component-type"),
            QStringLiteral("Only bar-widget instances are supported.")
        );
        return false;
    }
    if (!packageDigest.isString()
        || !digestPattern().match(packageDigest.toString()).hasMatch()) {
        appendError(
            errors,
            childPath(path, QStringLiteral("packageDigest")),
            QStringLiteral("surface-plan.invalid-package-digest"),
            QStringLiteral("The package digest must be lowercase SHA-256.")
        );
        return false;
    }
    if (!runtimeValue.isObject()) {
        appendError(
            errors,
            childPath(path, QStringLiteral("runtime")),
            QStringLiteral("surface-plan.runtime-object-required"),
            QStringLiteral("The runtime must be an object.")
        );
        return false;
    }

    const auto runtime = runtimeValue.toObject();
    const auto runtimePath = childPath(path, QStringLiteral("runtime"));
    if (!requireExactKeys(
            runtime,
            {QStringLiteral("kind"), QStringLiteral("factory")},
            runtimePath,
            errors
        )) {
        return false;
    }
    const auto kind = runtime.value(QStringLiteral("kind"));
    const auto factory = runtime.value(QStringLiteral("factory"));
    if (!kind.isString() || kind.toString() != QStringLiteral("builtin-v1")) {
        appendError(
            errors,
            childPath(runtimePath, QStringLiteral("kind")),
            QStringLiteral("surface-plan.unsupported-runtime"),
            QStringLiteral("Only builtin-v1 is supported.")
        );
        return false;
    }
    if (!factory.isString()
        || factory.toString().size() > 64
        || !factoryPattern().match(factory.toString()).hasMatch()) {
        appendError(
            errors,
            childPath(runtimePath, QStringLiteral("factory")),
            QStringLiteral("surface-plan.invalid-factory"),
            QStringLiteral("The built-in factory key is invalid.")
        );
        return false;
    }
    if (!settingsValue.isObject()
        || settingsValue.toObject().size() > 128) {
        appendError(
            errors,
            childPath(path, QStringLiteral("settings")),
            QStringLiteral("surface-plan.invalid-settings"),
            QStringLiteral("The instance settings object is invalid.")
        );
        return false;
    }

    if (componentId.toString() != QLatin1StringView(workspaceSwitcherId)
        || factory.toString() != QLatin1StringView(workspaceSwitcherFactory)
        || !isValidWorkspaceSwitcherSettings(settingsValue.toObject())) {
        appendError(
            errors,
            path,
            QStringLiteral("surface-plan.unsupported-factory-instance"),
            QStringLiteral("The built-in instance is not supported by this host.")
        );
        return false;
    }

    instance = {
        .componentId = componentId.toString(),
        .componentType = componentType.toString(),
        .packageDigest = packageDigest.toString(),
        .runtimeKind = kind.toString(),
        .factory = factory.toString(),
        .settings = settingsValue.toObject(),
    };
    return true;
}

bool parseBarLayout(
    const QJsonObject &object,
    const QString &path,
    SurfaceBarLayout &layout,
    ValidationErrors &errors
)
{
    if (!requireExactKeys(
            object,
            {QStringLiteral("outputs"), QStringLiteral("regions")},
            path,
            errors
        )) {
        return false;
    }

    const auto outputsValue = object.value(QStringLiteral("outputs"));
    const auto regionsValue = object.value(QStringLiteral("regions"));
    if (!outputsValue.isObject() || !regionsValue.isObject()) {
        appendError(
            errors,
            path,
            QStringLiteral("surface-plan.layout-object-required"),
            QStringLiteral("The output selector and regions must be objects.")
        );
        return false;
    }

    const auto outputs = outputsValue.toObject();
    const auto outputsPath = childPath(path, QStringLiteral("outputs"));
    if (!requireExactKeys(
            outputs,
            {QStringLiteral("mode")},
            outputsPath,
            errors
        )) {
        return false;
    }
    const auto outputMode = outputs.value(QStringLiteral("mode"));
    if (!outputMode.isString()
        || outputMode.toString() != QStringLiteral("all")) {
        appendError(
            errors,
            childPath(outputsPath, QStringLiteral("mode")),
            QStringLiteral("surface-plan.unsupported-output-selector"),
            QStringLiteral("Only the all-output selector is supported.")
        );
        return false;
    }

    const auto regions = regionsValue.toObject();
    const auto regionsPath = childPath(path, QStringLiteral("regions"));
    if (!requireExactKeys(
            regions,
            {
                QStringLiteral("start"),
                QStringLiteral("center"),
                QStringLiteral("end"),
            },
            regionsPath,
            errors
        )) {
        return false;
    }

    QStringList start;
    QStringList center;
    QStringList end;
    if (!parseStringArray(
            regions.value(QStringLiteral("start")),
            childPath(regionsPath, QStringLiteral("start")),
            start,
            errors
        )
        || !parseStringArray(
            regions.value(QStringLiteral("center")),
            childPath(regionsPath, QStringLiteral("center")),
            center,
            errors
        )
        || !parseStringArray(
            regions.value(QStringLiteral("end")),
            childPath(regionsPath, QStringLiteral("end")),
            end,
            errors
        )) {
        return false;
    }

    layout = {
        .outputMode = outputMode.toString(),
        .start = std::move(start),
        .center = std::move(center),
        .end = std::move(end),
    };
    return true;
}

ValidationErrors validateRelationships(const SurfacePlan &plan)
{
    ValidationErrors errors;
    QSet<QString> placed;

    auto layoutIds = plan.barLayouts.keys();
    layoutIds.sort();
    for (const auto &layoutId : layoutIds) {
        const auto &layout = plan.barLayouts.value(layoutId);
        const std::array<std::pair<QString, QStringList>, 3> regions {{
            {QStringLiteral("start"), layout.start},
            {QStringLiteral("center"), layout.center},
            {QStringLiteral("end"), layout.end},
        }};

        for (const auto &[regionName, instanceIds] : regions) {
            for (qsizetype index = 0; index < instanceIds.size(); ++index) {
                const auto &instanceId = instanceIds.at(index);
                const auto path = QStringLiteral("$.layouts.bars.%1.regions.%2[%3]")
                    .arg(layoutId, regionName)
                    .arg(index);
                if (!plan.instances.contains(instanceId)) {
                    appendError(
                        errors,
                        path,
                        QStringLiteral("surface-plan.dangling-instance"),
                        QStringLiteral("The placed instance does not exist.")
                    );
                } else if (placed.contains(instanceId)) {
                    appendError(
                        errors,
                        path,
                        QStringLiteral("surface-plan.duplicate-placement"),
                        QStringLiteral("A visual instance may be placed only once.")
                    );
                } else {
                    placed.insert(instanceId);
                }
            }
        }
    }

    auto instanceIds = plan.instances.keys();
    instanceIds.sort();
    for (const auto &instanceId : instanceIds) {
        if (!placed.contains(instanceId)) {
            appendError(
                errors,
                QStringLiteral("$.instances.%1").arg(instanceId),
                QStringLiteral("surface-plan.unplaced-instance"),
                QStringLiteral("Every effective visual instance must be placed.")
            );
        }
    }
    return errors;
}

QJsonArray stringArray(const QStringList &values)
{
    QJsonArray array;
    for (const auto &value : values) {
        array.append(value);
    }
    return array;
}

QJsonObject instanceObject(const SurfaceInstance &instance)
{
    return {
        {QStringLiteral("componentId"), instance.componentId},
        {QStringLiteral("componentType"), instance.componentType},
        {QStringLiteral("packageDigest"), instance.packageDigest},
        {
            QStringLiteral("runtime"),
            QJsonObject{
                {QStringLiteral("kind"), instance.runtimeKind},
                {QStringLiteral("factory"), instance.factory},
            }
        },
        {QStringLiteral("settings"), instance.settings},
    };
}

QJsonObject layoutObject(const SurfaceBarLayout &layout)
{
    return {
        {
            QStringLiteral("outputs"),
            QJsonObject{{QStringLiteral("mode"), layout.outputMode}}
        },
        {
            QStringLiteral("regions"),
            QJsonObject{
                {QStringLiteral("start"), stringArray(layout.start)},
                {QStringLiteral("center"), stringArray(layout.center)},
                {QStringLiteral("end"), stringArray(layout.end)},
            }
        },
    };
}

} // namespace

ValidationResult<SurfacePlan> parseSurfacePlan(QByteArrayView bytes)
{
    const auto parsed = parseStrictJsonObject(
        bytes,
        {
            .maximumBytes = maximumSurfacePlanBytes,
            .maximumDepth = 32,
        }
    );
    if (!parsed) {
        return {.errors = parsed.errors};
    }

    ValidationResult<SurfacePlan> result;
    const auto &root = *parsed.value;
    if (!requireExactKeys(
            root,
            {
                QStringLiteral("formatVersion"),
                QStringLiteral("catalogDigest"),
                QStringLiteral("configurationRevision"),
                QStringLiteral("instances"),
                QStringLiteral("layouts"),
            },
            QStringLiteral("$"),
            result.errors
        )) {
        return result;
    }

    const auto formatVersion = root.value(QStringLiteral("formatVersion"));
    if (!formatVersion.isDouble()
        || formatVersion.toInteger(-1) != surfacePlanFormatVersion
        || formatVersion.toDouble() != surfacePlanFormatVersion) {
        appendError(
            result.errors,
            QStringLiteral("$.formatVersion"),
            QStringLiteral("surface-plan.unsupported-format"),
            QStringLiteral("Only surface plan format version 1 is supported.")
        );
        return result;
    }

    const auto catalogDigest = root.value(QStringLiteral("catalogDigest"));
    if (!catalogDigest.isString()
        || !digestPattern().match(catalogDigest.toString()).hasMatch()) {
        appendError(
            result.errors,
            QStringLiteral("$.catalogDigest"),
            QStringLiteral("surface-plan.invalid-catalog-digest"),
            QStringLiteral("The catalog digest must be lowercase SHA-256.")
        );
        return result;
    }

    const auto configurationRevision = root.value(
        QStringLiteral("configurationRevision")
    );
    quint64 revision = 0;
    if (!configurationRevision.isString()
        || !parseUnsignedRevision(configurationRevision.toString(), revision)) {
        appendError(
            result.errors,
            QStringLiteral("$.configurationRevision"),
            QStringLiteral("surface-plan.invalid-configuration-revision"),
            QStringLiteral("The configuration revision is not canonical unsigned-64 text.")
        );
        return result;
    }

    const auto instancesValue = root.value(QStringLiteral("instances"));
    const auto layoutsValue = root.value(QStringLiteral("layouts"));
    if (!instancesValue.isObject() || !layoutsValue.isObject()) {
        appendError(
            result.errors,
            QStringLiteral("$"),
            QStringLiteral("surface-plan.object-required"),
            QStringLiteral("Instances and layouts must be objects.")
        );
        return result;
    }

    const auto instances = instancesValue.toObject();
    if (instances.size() > maximumSurfacePlanInstances) {
        appendError(
            result.errors,
            QStringLiteral("$.instances"),
            QStringLiteral("surface-plan.instance-limit"),
            QStringLiteral("The plan contains too many instances.")
        );
        return result;
    }

    SurfacePlan plan;
    plan.catalogDigest = catalogDigest.toString();
    plan.configurationRevision = revision;
    for (auto iterator = instances.begin(); iterator != instances.end(); ++iterator) {
        if (!uuidV4Pattern().match(iterator.key()).hasMatch()
            || !iterator.value().isObject()) {
            appendError(
                result.errors,
                QStringLiteral("$.instances.%1").arg(iterator.key()),
                QStringLiteral("surface-plan.invalid-instance"),
                QStringLiteral("The instance key or value is invalid.")
            );
            return result;
        }

        SurfaceInstance instance;
        if (!parseInstance(
                iterator.value().toObject(),
                QStringLiteral("$.instances.%1").arg(iterator.key()),
                instance,
                result.errors
            )) {
            return result;
        }
        plan.instances.insert(iterator.key(), std::move(instance));
    }

    const auto layouts = layoutsValue.toObject();
    if (!requireExactKeys(
            layouts,
            {QStringLiteral("bars")},
            QStringLiteral("$.layouts"),
            result.errors
        )) {
        return result;
    }
    const auto barsValue = layouts.value(QStringLiteral("bars"));
    if (!barsValue.isObject()) {
        appendError(
            result.errors,
            QStringLiteral("$.layouts.bars"),
            QStringLiteral("surface-plan.object-required"),
            QStringLiteral("Bar layouts must be an object.")
        );
        return result;
    }

    const auto bars = barsValue.toObject();
    if (bars.size() > maximumSurfacePlanBarLayouts) {
        appendError(
            result.errors,
            QStringLiteral("$.layouts.bars"),
            QStringLiteral("surface-plan.layout-limit"),
            QStringLiteral("The plan contains too many bar layouts.")
        );
        return result;
    }
    for (auto iterator = bars.begin(); iterator != bars.end(); ++iterator) {
        if (!layoutIdPattern().match(iterator.key()).hasMatch()
            || !iterator.value().isObject()) {
            appendError(
                result.errors,
                QStringLiteral("$.layouts.bars.%1").arg(iterator.key()),
                QStringLiteral("surface-plan.invalid-layout"),
                QStringLiteral("The bar layout key or value is invalid.")
            );
            return result;
        }

        SurfaceBarLayout layout;
        if (!parseBarLayout(
                iterator.value().toObject(),
                QStringLiteral("$.layouts.bars.%1").arg(iterator.key()),
                layout,
                result.errors
            )) {
            return result;
        }
        plan.barLayouts.insert(iterator.key(), std::move(layout));
    }

    result.errors.append(validateRelationships(plan));
    if (result.errors.isEmpty()) {
        result.value = std::move(plan);
    }
    return result;
}

ValidationResult<SurfacePlanArtifact> makeSurfacePlanArtifact(
    const SurfacePlan &plan
)
{
    QJsonObject instances;
    auto instanceIds = plan.instances.keys();
    instanceIds.sort();
    for (const auto &instanceId : instanceIds) {
        instances.insert(instanceId, instanceObject(plan.instances.value(instanceId)));
    }

    QJsonObject bars;
    auto layoutIds = plan.barLayouts.keys();
    layoutIds.sort();
    for (const auto &layoutId : layoutIds) {
        bars.insert(layoutId, layoutObject(plan.barLayouts.value(layoutId)));
    }

    const QJsonObject root {
        {QStringLiteral("formatVersion"), surfacePlanFormatVersion},
        {QStringLiteral("catalogDigest"), plan.catalogDigest},
        {
            QStringLiteral("configurationRevision"),
            QString::number(plan.configurationRevision)
        },
        {QStringLiteral("instances"), instances},
        {
            QStringLiteral("layouts"),
            QJsonObject{{QStringLiteral("bars"), bars}}
        },
    };

    auto bytes = QJsonDocument(root).toJson(QJsonDocument::Compact);
    bytes.append('\n');
    auto parsed = parseSurfacePlan(QByteArrayView(bytes));
    if (!parsed) {
        return {.errors = parsed.errors};
    }

    const auto digest = surfacePlanDigest(QByteArrayView(bytes));
    return {
        .value = SurfacePlanArtifact{
            .plan = std::move(*parsed.value),
            .bytes = std::move(bytes),
            .digest = digest,
            .revision = surfacePlanRevision(digest),
        },
    };
}

QString surfacePlanDigest(QByteArrayView bytes)
{
    return QString::fromLatin1(
        QCryptographicHash::hash(bytes, QCryptographicHash::Sha256).toHex()
    );
}

quint64 surfacePlanRevision(const QString &digest)
{
    const auto bytes = QByteArray::fromHex(digest.toLatin1());
    if (bytes.size() != QCryptographicHash::hashLength(
            QCryptographicHash::Sha256
        )) {
        return 0;
    }

    auto revision = qFromBigEndian<quint64>(
        reinterpret_cast<const uchar *>(bytes.constData())
    );
    if (revision == 0) {
        revision = 1;
    }
    return revision;
}

} // namespace HyprShelld::Components
