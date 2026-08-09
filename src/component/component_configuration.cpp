#include "component_configuration.h"

#include "strict_json.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QRegularExpression>

#include <algorithm>
#include <cmath>
#include <limits>

namespace HyprShelld::Components {
namespace {

constexpr double maximumExactDormantNumber = 9007199254740991.0;

constexpr int maximumConfigurationDepth = 32;
constexpr qsizetype maximumComponents = 512;
constexpr qsizetype maximumInstances = 1024;
constexpr qsizetype maximumBarLayouts = 32;
constexpr qsizetype maximumRegionInstances = 1024;

void addError(
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

void rejectUnknownFields(
    const QJsonObject &object,
    const QSet<QString> &allowed,
    const QString &path,
    ValidationErrors &errors
)
{
    for (auto iterator = object.constBegin(); iterator != object.constEnd();
         ++iterator) {
        if (!allowed.contains(iterator.key())) {
            addError(
                errors,
                path + QLatin1Char('.') + iterator.key(),
                QStringLiteral("component-config.unknown-field"),
                QStringLiteral("Unknown field: %1").arg(iterator.key())
            );
        }
    }
}

bool parseRevision(const QJsonValue &value, quint64 &revision)
{
    if (!value.isString()) {
        return false;
    }
    const auto text = value.toString();
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

bool isLayoutId(const QString &value)
{
    static const QRegularExpression expression(QStringLiteral(
        "^[a-z][a-z0-9]*(?:-[a-z0-9]+)*$"
    ));
    return value.size() <= 64 && expression.match(value).hasMatch();
}

bool isSettingKey(const QString &value)
{
    static const QRegularExpression expression(
        QStringLiteral("^[a-z][A-Za-z0-9]{0,63}$")
    );
    return expression.match(value).hasMatch();
}

bool hasControl(const QString &value)
{
    return std::ranges::any_of(value, [](const QChar character) {
        return character.category() == QChar::Other_Control;
    });
}

void validateDormantSettingValue(
    const QJsonValue &value,
    const QString &path,
    ValidationErrors &errors,
    const int depth = 0
)
{
    if (depth > 8) {
        addError(
            errors,
            path,
            QStringLiteral("component-config.settings-depth-limit"),
            QStringLiteral("A dormant setting value is nested too deeply.")
        );
        return;
    }
    if (value.isString()) {
        const auto text = value.toString();
        if (text.size() > 4096 || hasControl(text)
            || text != text.normalized(QString::NormalizationForm_C)) {
            addError(
                errors,
                path,
                QStringLiteral("component-config.invalid-setting-string"),
                QStringLiteral("Setting strings must be bounded normalized plain text.")
            );
        }
        return;
    }
    if (value.isDouble()) {
        const auto number = value.toDouble();
        if (!std::isfinite(number)
            || std::abs(number) > maximumExactDormantNumber) {
            addError(
                errors,
                path,
                QStringLiteral("component-config.invalid-setting-number"),
                QStringLiteral("Dormant numeric settings must stay within the exact JSON-safe range.")
            );
        }
        return;
    }
    if (value.isArray()) {
        const auto array = value.toArray();
        if (array.size() > 64) {
            addError(
                errors,
                path,
                QStringLiteral("component-config.settings-array-limit"),
                QStringLiteral("A setting array may contain at most 64 values.")
            );
            return;
        }
        for (qsizetype index = 0; index < array.size(); ++index) {
            validateDormantSettingValue(
                array.at(index),
                path + QStringLiteral("[%1]").arg(index),
                errors,
                depth + 1
            );
        }
        return;
    }
    if (value.isObject()) {
        const auto object = value.toObject();
        if (object.size() > 64) {
            addError(
                errors,
                path,
                QStringLiteral("component-config.settings-object-limit"),
                QStringLiteral("A setting object may contain at most 64 fields.")
            );
            return;
        }
        for (auto iterator = object.constBegin(); iterator != object.constEnd();
             ++iterator) {
            if (iterator.key().size() > 64 || hasControl(iterator.key())) {
                addError(
                    errors,
                    path + QLatin1Char('.') + iterator.key(),
                    QStringLiteral("component-config.invalid-nested-setting-key"),
                    QStringLiteral("Nested setting keys must be bounded plain text.")
                );
            }
            validateDormantSettingValue(
                iterator.value(),
                path + QLatin1Char('.') + iterator.key(),
                errors,
                depth + 1
            );
        }
    }
}

void validateDormantSettings(
    const QJsonObject &settings,
    const QString &path,
    ValidationErrors &errors
)
{
    if (settings.size() > 128) {
        addError(
            errors,
            path,
            QStringLiteral("component-config.settings-count-limit"),
            QStringLiteral("A settings object may contain at most 128 keys.")
        );
        return;
    }
    for (auto iterator = settings.constBegin(); iterator != settings.constEnd();
         ++iterator) {
        if (!isSettingKey(iterator.key())) {
            addError(
                errors,
                path + QLatin1Char('.') + iterator.key(),
                QStringLiteral("component-config.invalid-setting-key"),
                QStringLiteral("Setting keys must use bounded lower camel-case ASCII.")
            );
        }
        validateDormantSettingValue(
            iterator.value(),
            path + QLatin1Char('.') + iterator.key(),
            errors
        );
    }
}

ValidationResult<QStringList> parseStringArray(
    const QJsonValue &value,
    const QString &path,
    const qsizetype maximumCount,
    const bool requireSortedUnique
)
{
    ValidationResult<QStringList> result;
    if (!value.isArray() || value.toArray().size() > maximumCount) {
        addError(
            result.errors,
            path,
            QStringLiteral("component-config.invalid-array"),
            QStringLiteral("A bounded string array is required.")
        );
        return result;
    }

    QString previous;
    const auto array = value.toArray();
    for (qsizetype index = 0; index < array.size(); ++index) {
        if (!array.at(index).isString()) {
            addError(
                result.errors,
                path + QStringLiteral("[%1]").arg(index),
                QStringLiteral("component-config.string-required"),
                QStringLiteral("A string is required.")
            );
            continue;
        }
        const auto text = array.at(index).toString();
        if (requireSortedUnique && index > 0 && text <= previous) {
            addError(
                result.errors,
                path + QStringLiteral("[%1]").arg(index),
                QStringLiteral("component-config.array-not-sorted-unique"),
                QStringLiteral("Values must be sorted and unique.")
            );
        }
        previous = text;
        if (!result.value.has_value()) {
            result.value.emplace();
        }
        result.value->append(text);
    }
    if (!result.errors.isEmpty()) {
        result.value.reset();
    } else if (!result.value.has_value()) {
        result.value = QStringList();
    }
    return result;
}

QJsonArray stringArray(const QStringList &values)
{
    QJsonArray result;
    for (const auto &value : values) {
        result.append(value);
    }
    return result;
}

} // namespace

bool isFullSha256Digest(const QString &digest)
{
    static const QRegularExpression expression(
        QStringLiteral("^[0-9a-f]{64}$")
    );
    return expression.match(digest).hasMatch();
}

bool isLowercaseUuidV4(const QString &uuid)
{
    static const QRegularExpression expression(QStringLiteral(
        "^[0-9a-f]{8}-[0-9a-f]{4}-4[0-9a-f]{3}-[89ab][0-9a-f]{3}-[0-9a-f]{12}$"
    ));
    return expression.match(uuid).hasMatch();
}

ValidationResult<ComponentConfiguration> parseComponentConfiguration(
    const QByteArrayView bytes,
    const ConfigurationCatalog &catalog
)
{
    ValidationResult<ComponentConfiguration> result;
    const auto parsed = parseStrictJsonObject(
        bytes,
        {.maximumBytes = maximumComponentConfigurationBytes,
         .maximumDepth = maximumConfigurationDepth}
    );
    if (!parsed) {
        result.errors = parsed.errors;
        return result;
    }

    const auto &root = *parsed.value;
    rejectUnknownFields(
        root,
        {QStringLiteral("formatVersion"), QStringLiteral("revision"),
         QStringLiteral("components"), QStringLiteral("instances"),
         QStringLiteral("layouts")},
        QStringLiteral("$"),
        result.errors
    );

    const auto formatVersion = root.value(QStringLiteral("formatVersion"));
    const auto versionNumber = formatVersion.toDouble(-1.0);
    if (formatVersion.isDouble() && std::isfinite(versionNumber)
        && std::floor(versionNumber) == versionNumber
        && versionNumber > 1.0) {
        addError(
            result.errors,
            QStringLiteral("$.formatVersion"),
            QStringLiteral("component-config.unsupported-format"),
            QStringLiteral("This component configuration format is newer than supported.")
        );
    } else if (!formatVersion.isDouble() || versionNumber != 1.0) {
        addError(
            result.errors,
            QStringLiteral("$.formatVersion"),
            QStringLiteral("component-config.invalid-format"),
            QStringLiteral("Component configuration format version 1 is required.")
        );
    }

    ComponentConfiguration configuration;
    if (!parseRevision(root.value(QStringLiteral("revision")), configuration.revision)) {
        addError(
            result.errors,
            QStringLiteral("$.revision"),
            QStringLiteral("component-config.invalid-revision"),
            QStringLiteral("revision must be a canonical unsigned decimal string.")
        );
    }

    const auto componentsValue = root.value(QStringLiteral("components"));
    if (!componentsValue.isObject()
        || componentsValue.toObject().size() > maximumComponents) {
        addError(
            result.errors,
            QStringLiteral("$.components"),
            QStringLiteral("component-config.invalid-components"),
            QStringLiteral("components must be an object with at most 512 records.")
        );
    } else {
        const auto components = componentsValue.toObject();
        for (auto iterator = components.constBegin(); iterator != components.constEnd();
             ++iterator) {
            const auto path = QStringLiteral("$.components.") + iterator.key();
            if (!isValidComponentId(iterator.key()) || !iterator.value().isObject()) {
                addError(
                    result.errors,
                    path,
                    QStringLiteral("component-config.invalid-component-record"),
                    QStringLiteral("A valid component ID and object record are required.")
                );
                continue;
            }
            const auto object = iterator.value().toObject();
            rejectUnknownFields(
                object,
                {QStringLiteral("packageDigest"), QStringLiteral("enabled"),
                 QStringLiteral("grantedCapabilities"), QStringLiteral("settings")},
                path,
                result.errors
            );

            DesiredComponent desired;
            const auto packageDigest = object.value(QStringLiteral("packageDigest"));
            const auto enabled = object.value(QStringLiteral("enabled"));
            const auto settings = object.value(QStringLiteral("settings"));
            if (!packageDigest.isString()
                || !isFullSha256Digest(packageDigest.toString())) {
                addError(
                    result.errors,
                    path + QStringLiteral(".packageDigest"),
                    QStringLiteral("component-config.invalid-package-digest"),
                    QStringLiteral("A lowercase SHA-256 package digest is required.")
                );
            } else {
                desired.packageDigest = packageDigest.toString();
            }
            if (!enabled.isBool()) {
                addError(
                    result.errors,
                    path + QStringLiteral(".enabled"),
                    QStringLiteral("component-config.boolean-required"),
                    QStringLiteral("enabled must be boolean.")
                );
            } else {
                desired.enabled = enabled.toBool();
            }
            const auto grants = parseStringArray(
                object.value(QStringLiteral("grantedCapabilities")),
                path + QStringLiteral(".grantedCapabilities"),
                64,
                true
            );
            if (grants) {
                desired.grantedCapabilities = *grants.value;
                for (const auto &grant : desired.grantedCapabilities) {
                    if (!isValidCapabilityId(grant)) {
                        addError(
                            result.errors,
                            path + QStringLiteral(".grantedCapabilities"),
                            QStringLiteral("component-config.invalid-capability"),
                            QStringLiteral("Capability IDs must use canonical dotted lowercase syntax.")
                        );
                    }
                }
            } else {
                result.errors += grants.errors;
            }
            if (!settings.isObject()) {
                addError(
                    result.errors,
                    path + QStringLiteral(".settings"),
                    QStringLiteral("component-config.settings-object-required"),
                    QStringLiteral("settings must be an object.")
                );
            } else {
                desired.settings = settings.toObject();
                validateDormantSettings(
                    desired.settings,
                    path + QStringLiteral(".settings"),
                    result.errors
                );
            }

            const auto catalogEntry = catalog.entries.constFind(iterator.key());
            if (catalogEntry != catalog.entries.cend()
                && desired.packageDigest == catalogEntry->packageDigest) {
                if (catalogEntry->origin == ComponentOrigin::System
                    && !desired.grantedCapabilities.isEmpty()) {
                    addError(
                        result.errors,
                        path + QStringLiteral(".grantedCapabilities"),
                        QStringLiteral("component-config.builtin-grants-forbidden"),
                        QStringLiteral("Built-in components do not store user grants.")
                    );
                }
                for (const auto &grant : desired.grantedCapabilities) {
                    if (!catalogEntry->requestedCapabilities.contains(grant)) {
                        addError(
                            result.errors,
                            path + QStringLiteral(".grantedCapabilities"),
                            QStringLiteral("component-config.unrequested-capability"),
                            QStringLiteral("A grant was not requested by this package.")
                        );
                    }
                }
                const auto normalized = normalizeSettings(
                    catalogEntry->settingsSchema,
                    SettingScope::Component,
                    desired.settings,
                    true
                );
                if (!normalized) {
                    result.errors += normalized.errors;
                } else {
                    desired.settings = *normalized.value;
                }
            }
            configuration.components.insert(iterator.key(), std::move(desired));
        }
    }

    const auto instancesValue = root.value(QStringLiteral("instances"));
    if (!instancesValue.isObject()
        || instancesValue.toObject().size() > maximumInstances) {
        addError(
            result.errors,
            QStringLiteral("$.instances"),
            QStringLiteral("component-config.invalid-instances"),
            QStringLiteral("instances must be an object with at most 1024 records.")
        );
    } else {
        const auto instances = instancesValue.toObject();
        for (auto iterator = instances.constBegin(); iterator != instances.constEnd();
             ++iterator) {
            const auto path = QStringLiteral("$.instances.") + iterator.key();
            if (!isLowercaseUuidV4(iterator.key()) || !iterator.value().isObject()) {
                addError(
                    result.errors,
                    path,
                    QStringLiteral("component-config.invalid-instance-record"),
                    QStringLiteral("A lowercase UUIDv4 and object record are required.")
                );
                continue;
            }
            const auto object = iterator.value().toObject();
            rejectUnknownFields(
                object,
                {QStringLiteral("componentId"), QStringLiteral("enabled"),
                 QStringLiteral("settings")},
                path,
                result.errors
            );
            ComponentInstance instance;
            const auto componentId = object.value(QStringLiteral("componentId"));
            const auto enabled = object.value(QStringLiteral("enabled"));
            const auto settings = object.value(QStringLiteral("settings"));
            if (!componentId.isString()
                || !configuration.components.contains(componentId.toString())) {
                addError(
                    result.errors,
                    path + QStringLiteral(".componentId"),
                    QStringLiteral("component-config.unknown-component-reference"),
                    QStringLiteral("The instance must reference a desired component record.")
                );
            } else {
                instance.componentId = componentId.toString();
            }
            if (!enabled.isBool()) {
                addError(
                    result.errors,
                    path + QStringLiteral(".enabled"),
                    QStringLiteral("component-config.boolean-required"),
                    QStringLiteral("enabled must be boolean.")
                );
            } else {
                instance.enabled = enabled.toBool();
            }
            if (!settings.isObject()) {
                addError(
                    result.errors,
                    path + QStringLiteral(".settings"),
                    QStringLiteral("component-config.settings-object-required"),
                    QStringLiteral("settings must be an object.")
                );
            } else {
                instance.settings = settings.toObject();
                validateDormantSettings(
                    instance.settings,
                    path + QStringLiteral(".settings"),
                    result.errors
                );
            }

            const auto desired = configuration.components.constFind(instance.componentId);
            const auto catalogEntry = catalog.entries.constFind(instance.componentId);
            if (desired != configuration.components.cend()
                && catalogEntry != catalog.entries.cend()
                && desired->packageDigest == catalogEntry->packageDigest) {
                if (catalogEntry->type != ComponentType::BarWidget) {
                    addError(
                        result.errors,
                        path + QStringLiteral(".componentId"),
                        QStringLiteral("component-config.unsupported-instance-type"),
                        QStringLiteral("The current visual host supports only bar-widget instances.")
                    );
                }
                const auto normalized = normalizeSettings(
                    catalogEntry->settingsSchema,
                    SettingScope::Instance,
                    instance.settings,
                    true
                );
                if (!normalized) {
                    result.errors += normalized.errors;
                } else {
                    instance.settings = *normalized.value;
                }
            }
            configuration.instances.insert(iterator.key(), std::move(instance));
        }
    }

    const auto layoutsValue = root.value(QStringLiteral("layouts"));
    if (!layoutsValue.isObject()) {
        addError(
            result.errors,
            QStringLiteral("$.layouts"),
            QStringLiteral("component-config.layouts-object-required"),
            QStringLiteral("layouts must be an object.")
        );
    } else {
        const auto layouts = layoutsValue.toObject();
        rejectUnknownFields(
            layouts,
            {QStringLiteral("bars"), QStringLiteral("desktops")},
            QStringLiteral("$.layouts"),
            result.errors
        );
        const auto desktops = layouts.value(QStringLiteral("desktops"));
        if (!desktops.isObject() || !desktops.toObject().isEmpty()) {
            addError(
                result.errors,
                QStringLiteral("$.layouts.desktops"),
                QStringLiteral("component-config.desktops-not-supported"),
                QStringLiteral("Desktop component layouts are not supported yet.")
            );
        }

        const auto barsValue = layouts.value(QStringLiteral("bars"));
        if (!barsValue.isObject() || barsValue.toObject().size() > maximumBarLayouts) {
            addError(
                result.errors,
                QStringLiteral("$.layouts.bars"),
                QStringLiteral("component-config.invalid-bar-layouts"),
                QStringLiteral("bars must contain at most 32 layouts.")
            );
        } else {
            const auto bars = barsValue.toObject();
            for (auto iterator = bars.constBegin(); iterator != bars.constEnd();
                 ++iterator) {
                const auto path = QStringLiteral("$.layouts.bars.") + iterator.key();
                if (!isLayoutId(iterator.key()) || !iterator.value().isObject()) {
                    addError(
                        result.errors,
                        path,
                        QStringLiteral("component-config.invalid-bar-layout"),
                        QStringLiteral("A valid layout ID and object are required.")
                    );
                    continue;
                }
                const auto object = iterator.value().toObject();
                rejectUnknownFields(
                    object,
                    {QStringLiteral("outputs"), QStringLiteral("regions")},
                    path,
                    result.errors
                );
                BarLayout layout;
                const auto outputsValue = object.value(QStringLiteral("outputs"));
                if (!outputsValue.isObject()) {
                    addError(
                        result.errors,
                        path + QStringLiteral(".outputs"),
                        QStringLiteral("component-config.invalid-output-selector"),
                        QStringLiteral("outputs must be an object.")
                    );
                } else {
                    const auto outputs = outputsValue.toObject();
                    const auto mode = outputs.value(QStringLiteral("mode"));
                    if (!mode.isString()
                        || (mode.toString() != QStringLiteral("all")
                            && mode.toString() != QStringLiteral("named"))) {
                        addError(
                            result.errors,
                            path + QStringLiteral(".outputs.mode"),
                            QStringLiteral("component-config.invalid-output-mode"),
                            QStringLiteral("Output mode must be all or named.")
                        );
                    } else {
                        layout.outputs.mode = mode.toString();
                    }
                    if (layout.outputs.mode == QStringLiteral("all")) {
                        rejectUnknownFields(
                            outputs,
                            {QStringLiteral("mode")},
                            path + QStringLiteral(".outputs"),
                            result.errors
                        );
                    } else {
                        rejectUnknownFields(
                            outputs,
                            {QStringLiteral("mode"), QStringLiteral("names")},
                            path + QStringLiteral(".outputs"),
                            result.errors
                        );
                        const auto names = parseStringArray(
                            outputs.value(QStringLiteral("names")),
                            path + QStringLiteral(".outputs.names"),
                            32,
                            true
                        );
                        if (!names || names.value->isEmpty()) {
                            if (!names) {
                                result.errors += names.errors;
                            } else {
                                addError(
                                    result.errors,
                                    path + QStringLiteral(".outputs.names"),
                                    QStringLiteral("component-config.empty-output-names"),
                                    QStringLiteral("named output mode requires at least one name.")
                                );
                            }
                        } else {
                            for (const auto &name : *names.value) {
                                if (name.isEmpty() || name.size() > 128
                                    || name != name.trimmed() || hasControl(name)
                                    || name != name.normalized(QString::NormalizationForm_C)) {
                                    addError(
                                        result.errors,
                                        path + QStringLiteral(".outputs.names"),
                                        QStringLiteral("component-config.invalid-output-name"),
                                        QStringLiteral("Output names must be bounded normalized plain text.")
                                    );
                                }
                            }
                            layout.outputs.names = *names.value;
                        }
                    }
                }

                const auto regionsValue = object.value(QStringLiteral("regions"));
                if (!regionsValue.isObject()) {
                    addError(
                        result.errors,
                        path + QStringLiteral(".regions"),
                        QStringLiteral("component-config.invalid-regions"),
                        QStringLiteral("regions must be an object.")
                    );
                } else {
                    const auto regions = regionsValue.toObject();
                    rejectUnknownFields(
                        regions,
                        {QStringLiteral("start"), QStringLiteral("center"),
                         QStringLiteral("end")},
                        path + QStringLiteral(".regions"),
                        result.errors
                    );
                    const auto start = parseStringArray(
                        regions.value(QStringLiteral("start")),
                        path + QStringLiteral(".regions.start"),
                        maximumRegionInstances,
                        false
                    );
                    const auto center = parseStringArray(
                        regions.value(QStringLiteral("center")),
                        path + QStringLiteral(".regions.center"),
                        maximumRegionInstances,
                        false
                    );
                    const auto end = parseStringArray(
                        regions.value(QStringLiteral("end")),
                        path + QStringLiteral(".regions.end"),
                        maximumRegionInstances,
                        false
                    );
                    if (start) layout.start = *start.value; else result.errors += start.errors;
                    if (center) layout.center = *center.value; else result.errors += center.errors;
                    if (end) layout.end = *end.value; else result.errors += end.errors;
                }
                configuration.bars.insert(iterator.key(), std::move(layout));
            }
        }
    }

    QSet<QString> placed;
    for (auto layout = configuration.bars.constBegin();
         layout != configuration.bars.constEnd(); ++layout) {
        for (const auto &region : {layout->start, layout->center, layout->end}) {
            for (const auto &instanceId : region) {
                if (!isLowercaseUuidV4(instanceId)
                    || !configuration.instances.contains(instanceId)) {
                    addError(
                        result.errors,
                        QStringLiteral("$.layouts.bars.%1.regions").arg(layout.key()),
                        QStringLiteral("component-config.unknown-instance-placement"),
                        QStringLiteral("Every placement must reference an existing instance.")
                    );
                } else if (placed.contains(instanceId)) {
                    addError(
                        result.errors,
                        QStringLiteral("$.layouts.bars.%1.regions").arg(layout.key()),
                        QStringLiteral("component-config.duplicate-instance-placement"),
                        QStringLiteral("An instance may appear in exactly one placement.")
                    );
                }
                placed.insert(instanceId);
            }
        }
    }
    for (auto instance = configuration.instances.constBegin();
         instance != configuration.instances.constEnd(); ++instance) {
        if (!placed.contains(instance.key())) {
            addError(
                result.errors,
                QStringLiteral("$.instances.") + instance.key(),
                QStringLiteral("component-config.unplaced-instance"),
                QStringLiteral("Every visual instance requires exactly one placement.")
            );
        }
    }

    if (result.errors.isEmpty()) {
        result.value = std::move(configuration);
    }
    return result;
}

QByteArray serializeComponentConfiguration(
    const ComponentConfiguration &configuration
)
{
    QJsonObject components;
    for (auto iterator = configuration.components.constBegin();
         iterator != configuration.components.constEnd(); ++iterator) {
        components.insert(iterator.key(), QJsonObject{
            {QStringLiteral("packageDigest"), iterator->packageDigest},
            {QStringLiteral("enabled"), iterator->enabled},
            {QStringLiteral("grantedCapabilities"),
             stringArray(iterator->grantedCapabilities)},
            {QStringLiteral("settings"), iterator->settings},
        });
    }

    QJsonObject instances;
    for (auto iterator = configuration.instances.constBegin();
         iterator != configuration.instances.constEnd(); ++iterator) {
        instances.insert(iterator.key(), QJsonObject{
            {QStringLiteral("componentId"), iterator->componentId},
            {QStringLiteral("enabled"), iterator->enabled},
            {QStringLiteral("settings"), iterator->settings},
        });
    }

    QJsonObject bars;
    for (auto iterator = configuration.bars.constBegin();
         iterator != configuration.bars.constEnd(); ++iterator) {
        QJsonObject outputs{{QStringLiteral("mode"), iterator->outputs.mode}};
        if (iterator->outputs.mode == QStringLiteral("named")) {
            outputs.insert(
                QStringLiteral("names"),
                stringArray(iterator->outputs.names)
            );
        }
        bars.insert(iterator.key(), QJsonObject{
            {QStringLiteral("outputs"), outputs},
            {QStringLiteral("regions"), QJsonObject{
                {QStringLiteral("start"), stringArray(iterator->start)},
                {QStringLiteral("center"), stringArray(iterator->center)},
                {QStringLiteral("end"), stringArray(iterator->end)},
            }},
        });
    }

    const QJsonObject root{
        {QStringLiteral("formatVersion"), 1},
        {QStringLiteral("revision"), QString::number(configuration.revision)},
        {QStringLiteral("components"), components},
        {QStringLiteral("instances"), instances},
        {QStringLiteral("layouts"), QJsonObject{
            {QStringLiteral("bars"), bars},
            {QStringLiteral("desktops"), QJsonObject()},
        }},
    };
    auto bytes = QJsonDocument(root).toJson(QJsonDocument::Compact);
    bytes.append('\n');
    return bytes;
}

} // namespace HyprShelld::Components
