#include <hyprland/src/SharedDefs.hpp>
#include <hyprland/src/Compositor.hpp>
#include <hyprland/src/desktop/view/LayerSurface.hpp>
#include <hyprland/src/output/Monitor.hpp>
#include <hyprland/src/plugins/PluginAPI.hpp>
#include <hyprland/src/protocols/LayerShell.hpp>
#include <hyprland/src/render/Renderer.hpp>
#include <hyprland/src/render/pass/RectPassElement.hpp>
#include <hyprland/src/state/MonitorState.hpp>

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cmath>
#include <iomanip>
#include <map>
#include <mutex>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

using json = nlohmann::json;

HANDLE                g_handle = nullptr;
SP<SHyprCtlCommand>   g_statusCommand;
SP<SHyprCtlCommand>   g_applyCommand;
SP<SHyprCtlCommand>   g_clearCommand;
SP<SHyprCtlCommand>   g_debugOverlayCommand;
SP<SHyprCtlCommand>   g_materialCommand;
CHyprSignalListener   g_renderStageListener;
std::mutex            g_stateMutex;
std::string           g_lastApplyStatus = "none";
std::string           g_lastError;
std::string           g_lastDebugOverlayRenderStatus = "disabled";
std::string           g_lastMaterialRenderStatus = "disabled";
std::string           g_materialMode = "off";
uint64_t              g_generation = 0;
uint64_t              g_applyCount  = 0;
bool                  g_debugOverlayEnabled = false;

struct RectSummary {
    double x      = 0.0;
    double y      = 0.0;
    double width  = 0.0;
    double height = 0.0;
};

struct RadiiSummary {
    double topLeft     = 0.0;
    double topRight    = 0.0;
    double bottomRight = 0.0;
    double bottomLeft  = 0.0;
};

struct MonitorSummary {
    std::string name;
    RectSummary logical;
    RectSummary framebuffer;
    std::string scaleKind;
    int64_t     id = -1;
    double      scale = 0.0;
    int         transform = 0;
    bool        hasLogical = false;
    bool        hasFramebuffer = false;
    bool        fractionalScale = false;
    bool        transformSupported = true;
};

struct DescriptorSummary {
    std::string id;
    std::string namespaceName;
    std::string monitor;
    std::string layer;
    std::string shapeType;
    std::string materialPreset;
    std::string tintColor;
    std::string debugName;
    RectSummary logical;
    RadiiSummary radius;
    int version = 0;
    uint64_t sequence = 0;
    bool materialEnabled = true;
    double opacity = 0.0;
    double frost = 0.0;
    double refractionStrength = 0.0;
    double tintOpacity = 0.0;
    double rimOpacity = 0.0;
    double highlightOpacity = 0.0;
    double shadowInnerOpacity = 0.0;
    double shadowOuterOpacity = 0.0;
};

struct SurfaceCandidate {
    std::string              debugID;
    std::string              namespaceName;
    std::string              monitor;
    std::string              layer;
    std::string              scaleKind;
    RectSummary              hyprlandGeometry;
    RectSummary              surfaceSize;
    RectSummary              monitorLogical;
    RectSummary              monitorFramebuffer;
    std::vector<std::string> matchedDescriptorIDs;
    int64_t                  monitorID = -1;
    double                   scale = 0.0;
    int                      monitorTransform = 0;
    bool                     hasGeometry = false;
    bool                     hasSurfaceSize = false;
    bool                     hasMonitorGeometry = false;
    bool                     hasMonitorFramebuffer = false;
    bool                     mapped = false;
    bool                     visible = false;
    bool                     fractionalScale = false;
    bool                     transformSupported = true;
};

struct DescriptorMatch {
    std::string      status;
    std::string      reason;
    std::vector<int> candidateIndexes;
};

struct CoordinateAnalysis {
    std::string              status = "unknown";
    std::string              confidence = "low";
    std::string              relation = "unknown";
    std::string              scaleKind = "invalid";
    std::string              framebufferRounding = "not-computed";
    std::vector<std::string> warnings;
    RectSummary              descriptorLogical;
    RectSummary              surfaceLogical;
    RectSummary              monitorLogical;
    RectSummary              computedMonitorLocalLogical;
    RectSummary              computedFramebuffer;
    RectSummary              computedFramebufferRounded;
    RectSummary              delta;
    RectSummary              deltaFramebuffer;
    double                   scale = 0.0;
    int                      monitorTransform = 0;
    bool                     hasSurfaceLogical = false;
    bool                     hasMonitorLogical = false;
    bool                     hasScale = false;
    bool                     fractionalScale = false;
    bool                     transformSupported = true;
};

struct DebugOverlayDescriptor {
    std::string              status;
    std::string              reason;
    std::vector<std::string> warnings;
    RectSummary              rectUsed;
    RectSummary              surfaceRectUsed;
    RectSummary              globalRectUsed;
    bool                     drawable = false;
    bool                     hasSurfaceRect = false;
    bool                     mismatch = false;
};

struct MaterialDescriptor {
    std::string              status;
    std::string              reason;
    std::vector<std::string> warnings;
    RectSummary              rectUsed;
    RectSummary              globalRectUsed;
    std::string              renderStage = "RENDER_POST_WINDOWS";
    std::string              mode = "off";
    std::string              tintColorRequested;
    std::string              colorUsed;
    std::string              effectiveBlurSource;
    std::string              effectiveBlurControl;
    CHyprColor               color = CHyprColor(0.92F, 0.95F, 1.0F, 0.0F);
    double                   radiusRequested = 0.0;
    double                   radiusUsed = 0.0;
    double                   opacityRequested = 0.0;
    double                   tintOpacityRequested = 0.0;
    double                   requestedFrost = 0.0;
    double                   blurAlphaUsed = 0.0;
    double                   alphaUsed = 0.0;
    bool                     drawable = false;
    bool                     rounded = false;
    bool                     blurEnabled = false;
    bool                     perSurfaceBlurSupported = false;
    std::string              perSurfaceBlurSupport;
};

struct MaterialColorResolution {
    std::string tintColorRequested;
    std::string colorUsed;
    CHyprColor  color = CHyprColor(0.92F, 0.95F, 1.0F, 0.0F);
    double      opacityRequested = 0.0;
    double      tintOpacityRequested = 0.0;
    double      alphaUsed = 0.0;
};

std::map<std::string, DescriptorSummary> g_descriptors;

std::string trim(std::string value) {
    auto isSpace = [](unsigned char c) { return std::isspace(c) != 0; };
    value.erase(value.begin(), std::find_if(value.begin(), value.end(), [&](unsigned char c) { return !isSpace(c); }));
    value.erase(std::find_if(value.rbegin(), value.rend(), [&](unsigned char c) { return !isSpace(c); }).base(), value.end());
    return value;
}

std::string removePrefix(std::string value, const std::string& prefix) {
    value = trim(std::move(value));
    if (value.starts_with(prefix))
        return trim(value.substr(prefix.size()));
    return value;
}

std::string toLower(std::string value) {
    std::ranges::transform(value, value.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return value;
}

std::string getString(const json& object, std::string_view key) {
    auto it = object.find(key);
    if (it == object.end() || !it->is_string())
        return {};
    return it->get<std::string>();
}

double getDouble(const json& object, std::string_view key) {
    auto it = object.find(key);
    if (it == object.end() || !it->is_number())
        return 0.0;
    return it->get<double>();
}

uint64_t getUInt64(const json& object, std::string_view key) {
    auto it = object.find(key);
    if (it == object.end() || !it->is_number_unsigned())
        return 0;
    return it->get<uint64_t>();
}

int getInt(const json& object, std::string_view key) {
    auto it = object.find(key);
    if (it == object.end() || !it->is_number_integer())
        return 0;
    return it->get<int>();
}

bool getBool(const json& object, std::string_view key, bool defaultValue = false) {
    auto it = object.find(key);
    if (it == object.end() || !it->is_boolean())
        return defaultValue;
    return it->get<bool>();
}

const json* getObject(const json& object, std::string_view key) {
    auto it = object.find(key);
    if (it == object.end() || !it->is_object())
        return nullptr;
    return &*it;
}

RectSummary parseRect(const json* object) {
    if (!object)
        return {};
    return {
        .x      = getDouble(*object, "x"),
        .y      = getDouble(*object, "y"),
        .width  = getDouble(*object, "width"),
        .height = getDouble(*object, "height"),
    };
}

RectSummary rectFromBox(const CBox& box) {
    return {
        .x      = box.x,
        .y      = box.y,
        .width  = box.w,
        .height = box.h,
    };
}

RectSummary rectFromSize(const Vector2D& size) {
    return {
        .x      = 0.0,
        .y      = 0.0,
        .width  = size.x,
        .height = size.y,
    };
}

RectSummary rectFromPositionAndSize(const Vector2D& position, const Vector2D& size) {
    return {
        .x      = position.x,
        .y      = position.y,
        .width  = size.x,
        .height = size.y,
    };
}

bool isFractionalScale(double scale) {
    if (!std::isfinite(scale) || scale <= 0.0)
        return false;
    return std::abs(scale - std::round(scale)) > 0.0001;
}

std::string scaleKindFor(double scale) {
    if (!std::isfinite(scale) || scale <= 0.0)
        return "invalid";
    return isFractionalScale(scale) ? "fractional" : "integer";
}

bool isTransformSupported(int transform) {
    return transform == 0;
}

RectSummary multiplyRect(const RectSummary& rect, double factor) {
    return {
        .x      = rect.x * factor,
        .y      = rect.y * factor,
        .width  = rect.width * factor,
        .height = rect.height * factor,
    };
}

RectSummary roundRect(const RectSummary& rect) {
    return {
        .x      = std::round(rect.x),
        .y      = std::round(rect.y),
        .width  = std::round(rect.width),
        .height = std::round(rect.height),
    };
}

MonitorSummary monitorToSummary(const PHLMONITOR& monitor) {
    MonitorSummary summary;
    if (!monitor)
        return summary;

    summary.name      = monitor->m_name;
    summary.id        = monitor->m_id;
    summary.scale     = monitor->m_scale;
    summary.transform = static_cast<int>(monitor->m_transform);
    summary.scaleKind = scaleKindFor(summary.scale);
    summary.fractionalScale = isFractionalScale(summary.scale);
    summary.transformSupported = isTransformSupported(summary.transform);

    if (monitor->m_size.x > 0 || monitor->m_size.y > 0) {
        summary.logical    = rectFromPositionAndSize(monitor->m_position, monitor->m_size);
        summary.hasLogical = true;
    }

    if (monitor->m_pixelSize.x > 0 || monitor->m_pixelSize.y > 0) {
        summary.framebuffer    = rectFromSize(monitor->m_pixelSize);
        summary.hasFramebuffer = true;
    }

    return summary;
}

RadiiSummary parseRadii(const json* object) {
    if (!object)
        return {};
    return {
        .topLeft     = getDouble(*object, "topLeft"),
        .topRight    = getDouble(*object, "topRight"),
        .bottomRight = getDouble(*object, "bottomRight"),
        .bottomLeft  = getDouble(*object, "bottomLeft"),
    };
}

std::string layerName(uint32_t layer) {
    switch (layer) {
    case 0: return "background";
    case 1: return "bottom";
    case 2: return "top";
    case 3: return "overlay";
    default: return "unknown";
    }
}

std::string pointerDebugString(const void* pointer) {
    std::ostringstream out;
    out << "0x" << std::hex << std::uintptr_t(pointer);
    return out.str();
}

json rectToJSON(const RectSummary& rect) {
    return {
        {"x", rect.x},
        {"y", rect.y},
        {"width", rect.width},
        {"height", rect.height},
    };
}

json positionToJSON(const RectSummary& rect) {
    return {
        {"x", rect.x},
        {"y", rect.y},
    };
}

json sizeToJSON(const RectSummary& rect) {
    return {
        {"width", rect.width},
        {"height", rect.height},
    };
}

json rectWithSpaceToJSON(std::string_view space, const RectSummary& rect) {
    json out = rectToJSON(rect);
    out["space"] = space;
    return out;
}

json radiiToJSON(const RadiiSummary& radius) {
    return {
        {"topLeft", radius.topLeft},
        {"topRight", radius.topRight},
        {"bottomRight", radius.bottomRight},
        {"bottomLeft", radius.bottomLeft},
    };
}

json monitorToJSON(const MonitorSummary& monitor) {
    json out = {
        {"name", monitor.name},
        {"scale", monitor.scale},
        {"scaleKind", monitor.scaleKind},
        {"fractionalScale", monitor.fractionalScale},
        {"transform", monitor.transform},
        {"transformSupported", monitor.transformSupported},
    };
    if (monitor.id >= 0)
        out["id"] = monitor.id;
    if (monitor.hasLogical) {
        out["logicalPosition"] = positionToJSON(monitor.logical);
        out["logicalSize"] = sizeToJSON(monitor.logical);
    }
    if (monitor.hasFramebuffer)
        out["framebufferSize"] = sizeToJSON(monitor.framebuffer);
    return out;
}

json surfaceCandidateToJSON(const SurfaceCandidate& candidate) {
    json out = {
        {"debugID", candidate.debugID},
        {"namespace", candidate.namespaceName},
        {"monitor", candidate.monitor},
        {"layer", candidate.layer},
        {"mapped", candidate.mapped},
        {"visible", candidate.visible},
        {"scale", candidate.scale},
        {"scaleKind", candidate.scaleKind},
        {"fractionalScale", candidate.fractionalScale},
        {"transformSupported", candidate.transformSupported},
        {"matchedDescriptorIds", candidate.matchedDescriptorIDs},
    };
    if (candidate.monitorID >= 0)
        out["monitorID"] = candidate.monitorID;
    if (candidate.hasGeometry)
        out["hyprlandGeometry"] = rectToJSON(candidate.hyprlandGeometry);
    if (candidate.hasSurfaceSize)
        out["surfaceSize"] = rectToJSON(candidate.surfaceSize);
    if (candidate.hasMonitorGeometry) {
        out["monitorLogical"] = rectToJSON(candidate.monitorLogical);
        out["monitorTransform"] = candidate.monitorTransform;
    }
    if (candidate.hasMonitorFramebuffer)
        out["monitorFramebuffer"] = rectToJSON(candidate.monitorFramebuffer);
    return out;
}

json surfaceMatchToJSON(const DescriptorMatch& match, const std::vector<SurfaceCandidate>& candidates) {
    json out = {
        {"status", match.status},
        {"matched", match.status == "matched"},
        {"matchReason", match.reason},
    };

    if (match.candidateIndexes.size() == 1) {
        const auto& candidate = candidates[match.candidateIndexes.front()];
        out["debugID"] = candidate.debugID;
        out["monitor"] = candidate.monitor;
        out["layer"] = candidate.layer;
        out["namespace"] = candidate.namespaceName;
        out["mapped"] = candidate.mapped;
        out["visible"] = candidate.visible;
        out["scale"] = candidate.scale;
        out["scaleKind"] = candidate.scaleKind;
        out["fractionalScale"] = candidate.fractionalScale;
        out["monitorTransform"] = candidate.monitorTransform;
        out["transformSupported"] = candidate.transformSupported;
        if (candidate.monitorID >= 0)
            out["monitorID"] = candidate.monitorID;
        if (candidate.hasGeometry)
            out["hyprlandGeometry"] = rectToJSON(candidate.hyprlandGeometry);
        if (candidate.hasSurfaceSize)
            out["surfaceSize"] = rectToJSON(candidate.surfaceSize);
    } else if (!match.candidateIndexes.empty()) {
        json ambiguous = json::array();
        for (const int index : match.candidateIndexes)
            ambiguous.push_back(surfaceCandidateToJSON(candidates[index]));
        out["candidates"] = ambiguous;
    }

    return out;
}

double maxAbsDelta(const RectSummary& delta) {
    return std::max({std::abs(delta.x), std::abs(delta.y), std::abs(delta.width), std::abs(delta.height)});
}

void addWarning(std::vector<std::string>& warnings, const std::string& warning) {
    if (std::ranges::find(warnings, warning) == warnings.end())
        warnings.push_back(warning);
}

bool rectValid(const RectSummary& rect) {
    return std::isfinite(rect.x) && std::isfinite(rect.y) && std::isfinite(rect.width) && std::isfinite(rect.height) && rect.width > 0.0 && rect.height > 0.0;
}

double clampDouble(double value, double min, double max) {
    if (!std::isfinite(value))
        return min;
    return std::clamp(value, min, max);
}

uint8_t hexNibble(char c) {
    if (c >= '0' && c <= '9')
        return static_cast<uint8_t>(c - '0');
    if (c >= 'a' && c <= 'f')
        return static_cast<uint8_t>(c - 'a' + 10);
    if (c >= 'A' && c <= 'F')
        return static_cast<uint8_t>(c - 'A' + 10);
    return 0;
}

uint8_t hexByte(std::string_view value, size_t offset) {
    return static_cast<uint8_t>((hexNibble(value[offset]) << 4) | hexNibble(value[offset + 1]));
}

bool parseHexColor(std::string_view value, CHyprColor& color) {
    if (!(value.size() == 7 || value.size() == 9) || value.front() != '#')
        return false;

    for (size_t i = 1; i < value.size(); ++i) {
        if (!std::isxdigit(static_cast<unsigned char>(value[i])))
            return false;
    }

    const auto r = static_cast<float>(hexByte(value, 1)) / 255.0F;
    const auto g = static_cast<float>(hexByte(value, 3)) / 255.0F;
    const auto b = static_cast<float>(hexByte(value, 5)) / 255.0F;
    const auto a = value.size() == 9 ? static_cast<float>(hexByte(value, 7)) / 255.0F : 1.0F;
    color = CHyprColor(r, g, b, a);
    return true;
}

std::string colorToHexRGB(const CHyprColor& color) {
    auto channel = [](float value) {
        return static_cast<int>(std::round(clampDouble(value, 0.0, 1.0) * 255.0));
    };

    std::ostringstream out;
    out << '#'
        << std::uppercase << std::hex << std::setfill('0')
        << std::setw(2) << channel(color.r)
        << std::setw(2) << channel(color.g)
        << std::setw(2) << channel(color.b);
    return out.str();
}

double uniformRadiusFromDescriptor(const RadiiSummary& radius) {
    return std::min({radius.topLeft, radius.topRight, radius.bottomRight, radius.bottomLeft});
}

MaterialColorResolution resolveMaterialColor(const DescriptorSummary& descriptor, std::vector<std::string>& warnings) {
    MaterialColorResolution resolved;
    resolved.tintColorRequested = descriptor.tintColor;
    resolved.opacityRequested = descriptor.opacity;
    resolved.tintOpacityRequested = descriptor.tintOpacity;

    CHyprColor baseColor(0.92F, 0.95F, 1.0F, 1.0F);
    if (!descriptor.tintColor.empty()) {
        CHyprColor tint;
        if (parseHexColor(descriptor.tintColor, tint)) {
            baseColor = tint;
        } else {
            addWarning(warnings, "descriptor tint color is invalid; using fallback material color");
        }
    }

    const double opacity = clampDouble(descriptor.opacity, 0.0, 1.0);
    const double tintOpacity = clampDouble(descriptor.tintOpacity, 0.0, 1.0);
    const double colorAlpha = clampDouble(baseColor.a, 0.0, 1.0);
    const double alpha = clampDouble((opacity + tintOpacity * (1.0 - opacity)) * colorAlpha, 0.0, 1.0);

    resolved.color = CHyprColor(baseColor.r, baseColor.g, baseColor.b, static_cast<float>(alpha));
    resolved.colorUsed = colorToHexRGB(baseColor);
    resolved.alphaUsed = alpha;
    return resolved;
}

bool rectContains(const RectSummary& outer, const RectSummary& inner, double tolerance) {
    if (!rectValid(outer) || !rectValid(inner))
        return false;

    return inner.x >= outer.x - tolerance &&
        inner.y >= outer.y - tolerance &&
        inner.x + inner.width <= outer.x + outer.width + tolerance &&
        inner.y + inner.height <= outer.y + outer.height + tolerance;
}

CoordinateAnalysis analyzeCoordinates(const DescriptorSummary& descriptor, const DescriptorMatch& match, const std::vector<SurfaceCandidate>& candidates) {
    CoordinateAnalysis analysis;
    analysis.descriptorLogical = descriptor.logical;

    if (match.status != "matched") {
        analysis.status = match.status.empty() ? "unknown" : match.status;
        analysis.confidence = "low";
        if (!match.reason.empty())
            addWarning(analysis.warnings, match.reason);
        return analysis;
    }

    if (match.candidateIndexes.size() != 1) {
        analysis.status = match.candidateIndexes.empty() ? "unknown" : "ambiguous";
        analysis.confidence = "low";
        addWarning(analysis.warnings, match.candidateIndexes.empty() ? "matched descriptor has no candidate surface" : "multiple candidate surfaces matched");
        return analysis;
    }

    const auto& candidate = candidates[match.candidateIndexes.front()];
    analysis.scale = candidate.scale;
    analysis.monitorTransform = candidate.monitorTransform;
    analysis.scaleKind = candidate.scaleKind.empty() ? scaleKindFor(candidate.scale) : candidate.scaleKind;
    analysis.fractionalScale = candidate.fractionalScale;
    analysis.transformSupported = candidate.transformSupported;
    analysis.hasSurfaceLogical = candidate.hasGeometry;
    analysis.hasMonitorLogical = candidate.hasMonitorGeometry;
    analysis.hasScale = candidate.scale > 0.0;

    if (!candidate.hasGeometry) {
        analysis.status = "unknown";
        analysis.confidence = "low";
        addWarning(analysis.warnings, "Hyprland surface geometry unavailable");
        return analysis;
    }
    if (!candidate.hasMonitorGeometry) {
        analysis.status = "unknown";
        analysis.confidence = "low";
        addWarning(analysis.warnings, "monitor logical geometry unavailable");
        return analysis;
    }
    if (candidate.scale <= 0.0) {
        analysis.status = "unknown";
        analysis.confidence = "low";
        addWarning(analysis.warnings, "monitor scale unavailable");
        return analysis;
    }

    analysis.surfaceLogical = candidate.hyprlandGeometry;
    analysis.monitorLogical = candidate.monitorLogical;
    analysis.computedMonitorLocalLogical = {
        .x      = candidate.hyprlandGeometry.x - candidate.monitorLogical.x,
        .y      = candidate.hyprlandGeometry.y - candidate.monitorLogical.y,
        .width  = candidate.hyprlandGeometry.width,
        .height = candidate.hyprlandGeometry.height,
    };
    analysis.computedFramebuffer = multiplyRect(analysis.computedMonitorLocalLogical, candidate.scale);
    analysis.computedFramebufferRounded = roundRect(analysis.computedFramebuffer);
    analysis.delta = {
        .x      = descriptor.logical.x - analysis.computedMonitorLocalLogical.x,
        .y      = descriptor.logical.y - analysis.computedMonitorLocalLogical.y,
        .width  = descriptor.logical.width - analysis.computedMonitorLocalLogical.width,
        .height = descriptor.logical.height - analysis.computedMonitorLocalLogical.height,
    };
    analysis.deltaFramebuffer = multiplyRect(analysis.delta, candidate.scale);
    analysis.framebufferRounding = analysis.fractionalScale ? "nearest-integer-diagnostic" : "none-needed";

    const double largestDelta = maxAbsDelta(analysis.delta);
    const double nearTolerance = std::max(2.0, candidate.scale);
    const bool descriptorMatchesSurface = largestDelta <= 1.0;
    const bool descriptorNearSurface = largestDelta <= nearTolerance;
    const bool descriptorInsideSurface = rectContains(analysis.computedMonitorLocalLogical, descriptor.logical, nearTolerance);

    if (descriptorMatchesSurface) {
        analysis.status = "aligned";
        analysis.relation = "matches-layer-surface";
    } else if (descriptorNearSurface) {
        analysis.status = "near";
        analysis.relation = "near-layer-surface";
    } else if (descriptorInsideSurface) {
        analysis.status = "aligned";
        analysis.relation = "contained-in-layer-surface";
    } else {
        analysis.status = "mismatched";
        analysis.relation = "outside-layer-surface";
    }

    analysis.confidence = analysis.status == "aligned" || analysis.status == "near" ? "medium" : "low";

    addWarning(analysis.warnings, "surface logicalBox appears to be global-layout logical coordinates");
    addWarning(analysis.warnings, "framebuffer rect is computed from monitor-local logical rect multiplied by monitor scale");
    if (analysis.fractionalScale)
        addWarning(analysis.warnings, "fractional monitor scale is active; framebuffer rounding is diagnostic and has not been live-tested");
    else
        addWarning(analysis.warnings, "fractional scale is not active for this descriptor; fractional scale remains untested");
    if (analysis.relation == "contained-in-layer-surface")
        addWarning(analysis.warnings, "descriptor logical rect is contained inside the matched layer surface; delta reports inset from layer-surface bounds");
    if (candidate.monitorTransform != 0)
        addWarning(analysis.warnings, "monitor transform is non-normal; framebuffer conversion does not yet account for rotation");

    return analysis;
}

json coordinateAnalysisToJSON(const CoordinateAnalysis& analysis) {
    json out = {
        {"status", analysis.status},
        {"confidence", analysis.confidence},
        {"relation", analysis.relation},
        {"descriptorLogical", rectWithSpaceToJSON("monitor-local-logical", analysis.descriptorLogical)},
        {"warnings", analysis.warnings},
    };

    if (analysis.hasSurfaceLogical)
        out["surfaceLogical"] = rectWithSpaceToJSON("global-layout-logical", analysis.surfaceLogical);
    if (analysis.hasMonitorLogical)
        out["monitorLogical"] = rectToJSON(analysis.monitorLogical);
    if (analysis.hasScale) {
        out["scale"] = analysis.scale;
        out["scaleKind"] = analysis.scaleKind;
        out["fractionalScale"] = analysis.fractionalScale;
        out["framebufferRounding"] = analysis.framebufferRounding;
        out["computedMonitorLocalLogical"] = rectWithSpaceToJSON("monitor-local-logical", analysis.computedMonitorLocalLogical);
        out["computedFramebuffer"] = rectWithSpaceToJSON("monitor-framebuffer", analysis.computedFramebuffer);
        out["computedFramebufferRounded"] = rectWithSpaceToJSON("monitor-framebuffer-pixels-rounded", analysis.computedFramebufferRounded);
        out["delta"] = rectToJSON(analysis.delta);
        out["deltaFramebuffer"] = rectToJSON(analysis.deltaFramebuffer);
    }
    out["monitorTransform"] = analysis.monitorTransform;
    out["transformSupported"] = analysis.transformSupported;

    return out;
}

DebugOverlayDescriptor analyzeDebugOverlayDrawable(
    bool overlayEnabled,
    const DescriptorSummary& descriptor,
    const DescriptorMatch& match,
    const CoordinateAnalysis& coordinate,
    const std::vector<SurfaceCandidate>& candidates
) {
    DebugOverlayDescriptor overlay;
    overlay.status = "skipped";
    overlay.reason = "debug overlay disabled";

    if (!overlayEnabled)
        return overlay;

    if (match.status != "matched") {
        overlay.reason = match.reason.empty() ? "descriptor is not matched" : match.reason;
        return overlay;
    }
    if (match.candidateIndexes.size() != 1) {
        overlay.reason = match.candidateIndexes.empty() ? "matched descriptor has no candidate surface" : "multiple candidate surfaces matched";
        return overlay;
    }

    const auto& candidate = candidates[match.candidateIndexes.front()];
    if (!candidate.mapped) {
        overlay.reason = "surface not mapped";
        return overlay;
    }
    if (!candidate.visible) {
        overlay.reason = "surface not visible";
        return overlay;
    }
    if (!candidate.hasMonitorGeometry) {
        overlay.reason = "monitor geometry unavailable";
        return overlay;
    }
    if (candidate.scale <= 0.0) {
        overlay.reason = "monitor scale unavailable";
        return overlay;
    }
    if (candidate.monitorTransform != 0) {
        overlay.reason = "monitor transform unsupported";
        addWarning(overlay.warnings, "monitor transform is non-normal; debug overlay is skipped for this descriptor");
        return overlay;
    }
    if (coordinate.status == "unknown" || coordinate.status == "error" || coordinate.status == "unmatched" || coordinate.status == "ambiguous" || coordinate.status == "skipped") {
        overlay.reason = "coordinate " + coordinate.status;
        return overlay;
    }
    if (!rectValid(descriptor.logical)) {
        overlay.reason = "descriptor logical rect invalid";
        return overlay;
    }
    if (!rectValid(coordinate.computedMonitorLocalLogical)) {
        overlay.reason = "computed surface rect invalid";
        return overlay;
    }

    overlay.drawable = true;
    overlay.status = "drawable";
    overlay.reason = "coordinate " + coordinate.status;
    overlay.rectUsed = descriptor.logical;
    overlay.surfaceRectUsed = coordinate.computedMonitorLocalLogical;
    overlay.globalRectUsed = {
        .x      = descriptor.logical.x + candidate.monitorLogical.x,
        .y      = descriptor.logical.y + candidate.monitorLogical.y,
        .width  = descriptor.logical.width,
        .height = descriptor.logical.height,
    };
    overlay.hasSurfaceRect = true;
    overlay.mismatch = coordinate.status == "mismatched";
    overlay.warnings = coordinate.warnings;
    if (overlay.mismatch)
        addWarning(overlay.warnings, "descriptor rect and matched surface rect differ");

    return overlay;
}

MaterialDescriptor analyzeMaterialDrawable(
    const std::string& materialMode,
    const DescriptorSummary& descriptor,
    const DescriptorMatch& match,
    const CoordinateAnalysis& coordinate,
    const std::vector<SurfaceCandidate>& candidates
) {
    MaterialDescriptor material;
    material.mode = materialMode;
    material.status = "skipped";
    material.reason = "material mode off";

    if (materialMode == "off")
        return material;
    if (materialMode != "flat" && materialMode != "blur-native") {
        material.reason = "material mode unsupported";
        return material;
    }
    if (!descriptor.materialEnabled) {
        material.reason = "descriptor material disabled";
        return material;
    }
    if (match.status != "matched") {
        material.reason = match.reason.empty() ? "descriptor is not matched" : match.reason;
        return material;
    }
    if (match.candidateIndexes.size() != 1) {
        material.reason = match.candidateIndexes.empty() ? "matched descriptor has no candidate surface" : "multiple candidate surfaces matched";
        return material;
    }

    const auto& candidate = candidates[match.candidateIndexes.front()];
    if (!candidate.mapped) {
        material.reason = "surface not mapped";
        return material;
    }
    if (!candidate.visible) {
        material.reason = "surface not visible";
        return material;
    }
    if (!candidate.hasMonitorGeometry) {
        material.reason = "monitor geometry unavailable";
        return material;
    }
    if (candidate.scale <= 0.0) {
        material.reason = "monitor scale unavailable";
        return material;
    }
    if (candidate.monitorTransform != 0) {
        material.reason = "monitor transform unsupported";
        addWarning(material.warnings, "monitor transform is non-normal; compositor material is skipped for this descriptor");
        return material;
    }
    if (coordinate.status == "unknown" || coordinate.status == "error" || coordinate.status == "unmatched" || coordinate.status == "ambiguous" || coordinate.status == "skipped" || coordinate.status == "mismatched") {
        material.reason = "coordinate " + coordinate.status;
        return material;
    }
    if (!rectValid(descriptor.logical)) {
        material.reason = "descriptor logical rect invalid";
        return material;
    }

    material.drawable = true;
    material.status = "drawable";
    material.reason = "coordinate " + coordinate.status;
    material.rectUsed = descriptor.logical;
    material.globalRectUsed = {
        .x      = descriptor.logical.x + candidate.monitorLogical.x,
        .y      = descriptor.logical.y + candidate.monitorLogical.y,
        .width  = descriptor.logical.width,
        .height = descriptor.logical.height,
    };
    material.warnings = coordinate.warnings;

    material.radiusRequested = uniformRadiusFromDescriptor(descriptor.radius);
    const double maxRadius = std::min(descriptor.logical.width, descriptor.logical.height) / 2.0;
    material.radiusUsed = clampDouble(material.radiusRequested, 0.0, maxRadius);
    material.rounded = material.radiusUsed > 0.0;
    if (descriptor.radius.topLeft != descriptor.radius.topRight || descriptor.radius.topRight != descriptor.radius.bottomRight || descriptor.radius.bottomRight != descriptor.radius.bottomLeft)
        addWarning(material.warnings, "flat material uses a single uniform corner radius from the minimum descriptor corner radius");

    const auto resolvedColor = resolveMaterialColor(descriptor, material.warnings);
    material.tintColorRequested = resolvedColor.tintColorRequested;
    material.colorUsed = resolvedColor.colorUsed;
    material.color = resolvedColor.color;
    material.opacityRequested = resolvedColor.opacityRequested;
    material.tintOpacityRequested = resolvedColor.tintOpacityRequested;
    material.alphaUsed = resolvedColor.alphaUsed;

    material.requestedFrost = clampDouble(descriptor.frost, 0.0, 1.0);
    if (materialMode == "blur-native") {
        material.blurAlphaUsed = material.requestedFrost;
        material.blurEnabled = material.blurAlphaUsed > 0.01;
        material.effectiveBlurSource = "native-hyprland";
        material.effectiveBlurControl = "global-kernel-per-surface-alpha";
        material.perSurfaceBlurSupported = false;
        material.perSurfaceBlurSupport = "alpha-only";
        addWarning(material.warnings, "native Hyprland blur kernel uses global decoration:blur config; descriptor frost maps to CRectPassElement blurA alpha only");
        if (material.alphaUsed >= 1.0)
            addWarning(material.warnings, "native blur requires translucent material alpha in CRectPassElement; resolved alpha is opaque");
    }

    return material;
}

json debugOverlayDescriptorToJSON(const DebugOverlayDescriptor& overlay) {
    json out = {
        {"drawable", overlay.drawable},
        {"status", overlay.status},
        {"reason", overlay.reason},
        {"warnings", overlay.warnings},
    };

    if (overlay.drawable) {
        out["rectUsed"] = rectWithSpaceToJSON("monitor-local-logical", overlay.rectUsed);
        out["globalRectUsed"] = rectWithSpaceToJSON("global-layout-logical", overlay.globalRectUsed);
        out["mismatch"] = overlay.mismatch;
        if (overlay.hasSurfaceRect)
            out["surfaceRectUsed"] = rectWithSpaceToJSON("monitor-local-logical", overlay.surfaceRectUsed);
    }

    return out;
}

json materialDescriptorToJSON(const MaterialDescriptor& material) {
    json out = {
        {"drawable", material.drawable},
        {"status", material.status},
        {"reason", material.reason},
        {"mode", material.mode},
        {"warnings", material.warnings},
    };

    if (material.drawable) {
        out["rectUsed"] = rectWithSpaceToJSON("monitor-local-logical", material.rectUsed);
        out["globalRectUsed"] = rectWithSpaceToJSON("global-layout-logical", material.globalRectUsed);
        out["renderStage"] = material.renderStage;
        out["rounded"] = material.rounded;
        out["radiusRequested"] = material.radiusRequested;
        out["radiusUsed"] = material.radiusUsed;
        out["round"] = static_cast<int>(std::round(material.radiusUsed));
        out["requestedFrost"] = material.requestedFrost;
        out["requestedTintColor"] = material.tintColorRequested;
        out["requestedTintOpacity"] = material.tintOpacityRequested;
        out["requestedOpacity"] = material.opacityRequested;
        out["tintColorRequested"] = material.tintColorRequested;
        out["colorUsed"] = material.colorUsed;
        out["opacityRequested"] = material.opacityRequested;
        out["tintOpacityRequested"] = material.tintOpacityRequested;
        out["alphaUsed"] = material.alphaUsed;
        out["blurEnabled"] = material.blurEnabled;
        if (material.mode == "blur-native") {
            out["blurAlphaUsed"] = material.blurAlphaUsed;
            out["effectiveBlurSource"] = material.effectiveBlurSource;
            out["effectiveBlurControl"] = material.effectiveBlurControl;
            out["perSurfaceBlurSupported"] = material.perSurfaceBlurSupported;
            out["perSurfaceBlurSupport"] = material.perSurfaceBlurSupport;
        }
    }

    return out;
}

json descriptorToJSON(
    bool overlayEnabled,
    const std::string& materialMode,
    const DescriptorSummary& descriptor,
    const DescriptorMatch& match,
    const CoordinateAnalysis& coordinate,
    const std::vector<SurfaceCandidate>& candidates
) {
    const auto debugOverlay = analyzeDebugOverlayDrawable(overlayEnabled, descriptor, match, coordinate, candidates);
    const auto material = analyzeMaterialDrawable(materialMode, descriptor, match, coordinate, candidates);
    json out = {
        {"id", descriptor.id},
        {"namespace", descriptor.namespaceName},
        {"monitor", descriptor.monitor},
        {"layer", descriptor.layer},
        {"logical", rectToJSON(descriptor.logical)},
        {"shape", {
            {"type", descriptor.shapeType},
            {"radius", radiiToJSON(descriptor.radius)},
        }},
        {"material", {
            {"preset", descriptor.materialPreset},
            {"opacity", descriptor.opacity},
            {"frost", descriptor.frost},
            {"refractionStrength", descriptor.refractionStrength},
            {"tintColor", descriptor.tintColor},
            {"tintOpacity", descriptor.tintOpacity},
            {"rimOpacity", descriptor.rimOpacity},
            {"highlightOpacity", descriptor.highlightOpacity},
            {"shadowInnerOpacity", descriptor.shadowInnerOpacity},
            {"shadowOuterOpacity", descriptor.shadowOuterOpacity},
        }},
        {"surfaceMatch", surfaceMatchToJSON(match, candidates)},
        {"coordinate", coordinateAnalysisToJSON(coordinate)},
        {"debugOverlay", debugOverlayDescriptorToJSON(debugOverlay)},
        {"compositorMaterial", materialDescriptorToJSON(material)},
    };

    if (descriptor.version != 0)
        out["version"] = descriptor.version;
    if (descriptor.sequence != 0)
        out["sequence"] = descriptor.sequence;
    if (!descriptor.debugName.empty())
        out["debugName"] = descriptor.debugName;

    return out;
}

bool parseDescriptor(const json& value, DescriptorSummary& out, std::string& error) {
    if (!value.is_object()) {
        error = "descriptor must be an object";
        return false;
    }

    out.id = getString(value, "id");
    if (trim(out.id).empty()) {
        error = "descriptor is missing required id";
        return false;
    }

    out.version  = getInt(value, "version");
    out.sequence = getUInt64(value, "sequence");

    const json* surface = getObject(value, "surface");
    if (surface) {
        out.namespaceName = getString(*surface, "namespace");
        out.layer = getString(*surface, "layer");

        if (const json* monitor = getObject(*surface, "monitor"))
            out.monitor = getString(*monitor, "name");
    }

    if (const json* geometry = getObject(value, "geometry"))
        out.logical = parseRect(getObject(*geometry, "logical"));

    if (const json* shape = getObject(value, "shape")) {
        out.shapeType = getString(*shape, "type");
        out.radius = parseRadii(getObject(*shape, "radius"));
    }

    if (const json* material = getObject(value, "material")) {
        out.materialEnabled = getBool(*material, "enabled", true);
        out.materialPreset = getString(*material, "preset");
        out.opacity = getDouble(*material, "opacity");
        out.frost = getDouble(*material, "frost");

        if (const json* refraction = getObject(*material, "refraction"))
            out.refractionStrength = getDouble(*refraction, "strength");
        if (const json* tint = getObject(*material, "tint")) {
            out.tintColor = getString(*tint, "color");
            out.tintOpacity = getDouble(*tint, "opacity");
        }
        if (const json* rim = getObject(*material, "rim"))
            out.rimOpacity = getDouble(*rim, "opacity");
        if (const json* highlight = getObject(*material, "highlight"))
            out.highlightOpacity = getDouble(*highlight, "opacity");
        if (const json* shadow = getObject(*material, "shadow")) {
            out.shadowInnerOpacity = getDouble(*shadow, "innerOpacity");
            out.shadowOuterOpacity = getDouble(*shadow, "outerOpacity");
        }
    }

    if (const json* debug = getObject(value, "debug"))
        out.debugName = getString(*debug, "name");

    return true;
}

bool parseDescriptorPayload(const std::string& payload, std::map<std::string, DescriptorSummary>& out, std::string& error) {
    json root;
    try {
        root = json::parse(payload);
    } catch (const json::parse_error& e) {
        error = std::string("malformed JSON: ") + e.what();
        return false;
    }

    if (!root.is_object()) {
        error = "descriptor payload must be a JSON object";
        return false;
    }

    std::vector<json> descriptors;
    auto descriptorsIt = root.find("descriptors");
    if (descriptorsIt != root.end()) {
        if (!descriptorsIt->is_array()) {
            error = "descriptors must be an array";
            return false;
        }
        for (const auto& descriptor : *descriptorsIt)
            descriptors.push_back(descriptor);
    } else {
        descriptors.push_back(root);
    }

    if (descriptors.empty()) {
        error = "descriptor payload must contain at least one descriptor";
        return false;
    }

    for (const auto& value : descriptors) {
        DescriptorSummary descriptor;
        if (!parseDescriptor(value, descriptor, error))
            return false;
        out[descriptor.id] = std::move(descriptor);
    }

    return true;
}

std::vector<MonitorSummary> discoverMonitors() {
    std::vector<MonitorSummary> monitors;

    const auto& monitorState = State::monitorState();
    if (!monitorState)
        return monitors;

    for (const auto& monitor : monitorState->monitors()) {
        if (!monitor)
            continue;
        monitors.push_back(monitorToSummary(monitor));
    }

    return monitors;
}

std::vector<SurfaceCandidate> discoverLayerSurfaces() {
    std::vector<SurfaceCandidate> candidates;

    if (!g_pCompositor)
        return candidates;

    for (const auto& layerSurface : g_pCompositor->m_layers) {
        if (!layerSurface)
            continue;

        SurfaceCandidate candidate;
        candidate.debugID       = pointerDebugString(layerSurface.get());
        candidate.namespaceName = layerSurface->m_namespace;
        candidate.layer         = layerName(layerSurface->m_layer);
        candidate.mapped        = layerSurface->m_mapped;
        candidate.visible       = layerSurface->visible();

        if (const auto monitor = layerSurface->m_monitor.lock()) {
            const auto summary = monitorToSummary(monitor);
            candidate.monitor   = summary.name;
            candidate.monitorID = summary.id;
            candidate.scale     = summary.scale;
            candidate.scaleKind = summary.scaleKind;
            candidate.fractionalScale = summary.fractionalScale;
            candidate.monitorTransform = summary.transform;
            candidate.transformSupported = summary.transformSupported;
            candidate.monitorLogical = summary.logical;
            candidate.monitorFramebuffer = summary.framebuffer;
            candidate.hasMonitorGeometry = summary.hasLogical;
            candidate.hasMonitorFramebuffer = summary.hasFramebuffer;
        }

        if (const auto logicalBox = layerSurface->logicalBox()) {
            candidate.hyprlandGeometry = rectFromBox(*logicalBox);
            candidate.hasGeometry      = true;
        } else if (!layerSurface->m_geometry.empty()) {
            candidate.hyprlandGeometry = rectFromBox(layerSurface->m_geometry);
            candidate.hasGeometry      = true;
        }

        if (const auto layerResource = layerSurface->m_layerSurface.lock()) {
            if (candidate.namespaceName.empty())
                candidate.namespaceName = layerResource->m_layerNamespace;
            if (candidate.monitor.empty())
                candidate.monitor = layerResource->m_monitor;
            candidate.mapped = candidate.mapped || layerResource->m_mapped;
            if (layerResource->m_size.x > 0 || layerResource->m_size.y > 0) {
                candidate.surfaceSize = rectFromSize(layerResource->m_size);
                candidate.hasSurfaceSize = true;
            }
        }

        candidates.push_back(std::move(candidate));
    }

    return candidates;
}

bool candidateMatchesDescriptor(const SurfaceCandidate& candidate, const DescriptorSummary& descriptor) {
    if (!descriptor.namespaceName.empty() && candidate.namespaceName != descriptor.namespaceName)
        return false;
    if (!descriptor.monitor.empty() && candidate.monitor != descriptor.monitor)
        return false;
    if (!descriptor.layer.empty() && candidate.layer != descriptor.layer)
        return false;
    return true;
}

bool monitorExists(const std::vector<SurfaceCandidate>& candidates, const std::string& monitor) {
    return std::ranges::any_of(candidates, [&](const auto& candidate) { return candidate.monitor == monitor; });
}

std::vector<int> filterCandidates(const std::vector<SurfaceCandidate>& candidates, const std::vector<int>& indexes, const auto& predicate) {
    std::vector<int> filtered;
    for (const int index : indexes) {
        if (predicate(candidates[index]))
            filtered.push_back(index);
    }
    return filtered;
}

DescriptorMatch matchDescriptorToSurfaces(const DescriptorSummary& descriptor, const std::vector<SurfaceCandidate>& candidates) {
    if (descriptor.namespaceName.empty())
        return {.status = "skipped", .reason = "descriptor namespace empty", .candidateIndexes = {}};
    if (candidates.empty())
        return {.status = "unmatched", .reason = "no layer-shell surfaces discovered", .candidateIndexes = {}};

    std::vector<int> allIndexes;
    allIndexes.reserve(candidates.size());
    for (size_t i = 0; i < candidates.size(); ++i)
        allIndexes.push_back(static_cast<int>(i));

    auto namespaceMatches = filterCandidates(candidates, allIndexes, [&](const auto& candidate) {
        return candidate.namespaceName == descriptor.namespaceName;
    });
    if (namespaceMatches.empty()) {
        const bool namespaceUnavailable = std::ranges::any_of(candidates, [](const auto& candidate) { return candidate.namespaceName.empty(); });
        return {
            .status = "unmatched",
            .reason = namespaceUnavailable ? "namespace unavailable from Hyprland API" : "no layer surface with namespace",
            .candidateIndexes = {},
        };
    }

    auto currentMatches = namespaceMatches;
    if (!descriptor.monitor.empty()) {
        auto monitorMatches = filterCandidates(candidates, currentMatches, [&](const auto& candidate) {
            return candidate.monitor == descriptor.monitor;
        });
        if (monitorMatches.empty()) {
            return {
                .status = "unmatched",
                .reason = monitorExists(candidates, descriptor.monitor) ? "no layer surface with namespace on monitor" : "monitor not found",
                .candidateIndexes = {},
            };
        }
        currentMatches = std::move(monitorMatches);
    }

    if (!descriptor.layer.empty()) {
        auto layerMatches = filterCandidates(candidates, currentMatches, [&](const auto& candidate) {
            return candidate.layer == descriptor.layer;
        });
        if (layerMatches.empty())
            return {.status = "unmatched", .reason = "layer mismatch", .candidateIndexes = {}};
        currentMatches = std::move(layerMatches);
    }

    if (currentMatches.size() > 1)
        return {.status = "ambiguous", .reason = "multiple candidate surfaces matched", .candidateIndexes = std::move(currentMatches)};

    if (currentMatches.empty())
        return {.status = "unmatched", .reason = "no matching layer surface", .candidateIndexes = {}};

    const auto& candidate = candidates[currentMatches.front()];
    if (!candidate.mapped)
        return {.status = "skipped", .reason = "surface not mapped", .candidateIndexes = std::move(currentMatches)};

    return {.status = "matched", .reason = "namespace+monitor+layer", .candidateIndexes = std::move(currentMatches)};
}

std::map<std::string, DescriptorMatch> matchDescriptorsToSurfaces(const std::map<std::string, DescriptorSummary>& descriptors, std::vector<SurfaceCandidate>& candidates) {
    std::map<std::string, DescriptorMatch> matches;
    for (const auto& [id, descriptor] : descriptors) {
        auto match = matchDescriptorToSurfaces(descriptor, candidates);
        for (auto& candidate : candidates) {
            if (candidateMatchesDescriptor(candidate, descriptor))
                candidate.matchedDescriptorIDs.push_back(id);
        }
        matches.emplace(id, std::move(match));
    }
    return matches;
}

struct StatusSnapshot {
    std::map<std::string, DescriptorSummary> descriptors;
    std::string lastApplyStatus;
    std::string lastError;
    std::string lastDebugOverlayRenderStatus;
    std::string lastMaterialRenderStatus;
    std::string materialMode;
    uint64_t generation = 0;
    uint64_t applyCount = 0;
    bool debugOverlayEnabled = false;
};

StatusSnapshot activeStatusSnapshot() {
    std::lock_guard guard(g_stateMutex);
    return {
        .descriptors = g_descriptors,
        .lastApplyStatus = g_lastApplyStatus,
        .lastError = g_lastError,
        .lastDebugOverlayRenderStatus = g_lastDebugOverlayRenderStatus,
        .lastMaterialRenderStatus = g_lastMaterialRenderStatus,
        .materialMode = g_materialMode,
        .generation = g_generation,
        .applyCount = g_applyCount,
        .debugOverlayEnabled = g_debugOverlayEnabled,
    };
}

bool materialModeEnabled(std::string_view mode) {
    return mode == "flat" || mode == "blur-native";
}

std::string materialRenderStageFor(std::string_view mode) {
    return materialModeEnabled(mode) ? "RENDER_POST_WINDOWS" : "disabled";
}

int countMatchStatus(const std::map<std::string, DescriptorMatch>& matches, std::string_view status) {
    int count = 0;
    for (const auto& [_, match] : matches) {
        if (match.status == status)
            ++count;
    }
    return count;
}

std::map<std::string, CoordinateAnalysis> analyzeDescriptorCoordinates(
    const std::map<std::string, DescriptorSummary>& descriptors,
    const std::map<std::string, DescriptorMatch>& matches,
    const std::vector<SurfaceCandidate>& candidates
) {
    std::map<std::string, CoordinateAnalysis> coordinates;
    for (const auto& [id, descriptor] : descriptors)
        coordinates.emplace(id, analyzeCoordinates(descriptor, matches.at(id), candidates));
    return coordinates;
}

int countCoordinateStatus(const std::map<std::string, CoordinateAnalysis>& coordinates, std::string_view status) {
    int count = 0;
    for (const auto& [_, coordinate] : coordinates) {
        if (coordinate.status == status)
            ++count;
    }
    return count;
}

int countFractionalScaleMonitors(const std::vector<MonitorSummary>& monitors) {
    return static_cast<int>(std::ranges::count_if(monitors, [](const auto& monitor) { return monitor.fractionalScale; }));
}

int countTransformedMonitors(const std::vector<MonitorSummary>& monitors) {
    return static_cast<int>(std::ranges::count_if(monitors, [](const auto& monitor) { return monitor.transform != 0; }));
}

int countUnsupportedTransformMonitors(const std::vector<MonitorSummary>& monitors) {
    return static_cast<int>(std::ranges::count_if(monitors, [](const auto& monitor) { return !monitor.transformSupported; }));
}

int countPotentiallyStaleDescriptors(const std::map<std::string, DescriptorMatch>& matches) {
    return static_cast<int>(std::ranges::count_if(matches, [](const auto& item) {
        const auto& match = item.second;
        return match.status == "unmatched";
    }));
}

std::vector<std::string> collectCoordinateWarnings(const std::map<std::string, CoordinateAnalysis>& coordinates) {
    std::vector<std::string> warnings;
    for (const auto& [_, coordinate] : coordinates) {
        for (const auto& warning : coordinate.warnings)
            addWarning(warnings, warning);
    }
    return warnings;
}

std::vector<std::string> collectStatusWarnings(
    const std::vector<MonitorSummary>& monitors,
    const std::map<std::string, DescriptorMatch>& matches,
    const std::map<std::string, CoordinateAnalysis>& coordinates
) {
    auto warnings = collectCoordinateWarnings(coordinates);
    if (countFractionalScaleMonitors(monitors) > 0)
        addWarning(warnings, "fractional scale monitor present; HyprGlass framebuffer alignment remains diagnostic until live-tested");
    else
        addWarning(warnings, "no fractional scale monitors detected; fractional scale remains untested");
    if (countUnsupportedTransformMonitors(monitors) > 0)
        addWarning(warnings, "one or more transformed monitors are unsupported and descriptors there are skipped");
    if (countPotentiallyStaleDescriptors(matches) > 0)
        addWarning(warnings, "one or more descriptors have no matched layer surface and are non-drawable; this can indicate stale descriptors after shell exit");
    return warnings;
}

int countDrawableDebugOverlays(
    bool overlayEnabled,
    const std::map<std::string, DescriptorSummary>& descriptors,
    const std::map<std::string, DescriptorMatch>& matches,
    const std::map<std::string, CoordinateAnalysis>& coordinates,
    const std::vector<SurfaceCandidate>& candidates
) {
    int count = 0;
    for (const auto& [id, descriptor] : descriptors) {
        if (analyzeDebugOverlayDrawable(overlayEnabled, descriptor, matches.at(id), coordinates.at(id), candidates).drawable)
            ++count;
    }
    return count;
}

std::vector<std::string> collectDebugOverlayWarnings(
    bool overlayEnabled,
    const std::map<std::string, DescriptorSummary>& descriptors,
    const std::map<std::string, DescriptorMatch>& matches,
    const std::map<std::string, CoordinateAnalysis>& coordinates,
    const std::vector<SurfaceCandidate>& candidates
) {
    std::vector<std::string> warnings;
    for (const auto& [id, descriptor] : descriptors) {
        const auto overlay = analyzeDebugOverlayDrawable(overlayEnabled, descriptor, matches.at(id), coordinates.at(id), candidates);
        for (const auto& warning : overlay.warnings)
            addWarning(warnings, warning);
    }
    return warnings;
}

int countDrawableMaterials(
    const std::string& materialMode,
    const std::map<std::string, DescriptorSummary>& descriptors,
    const std::map<std::string, DescriptorMatch>& matches,
    const std::map<std::string, CoordinateAnalysis>& coordinates,
    const std::vector<SurfaceCandidate>& candidates
) {
    int count = 0;
    for (const auto& [id, descriptor] : descriptors) {
        if (analyzeMaterialDrawable(materialMode, descriptor, matches.at(id), coordinates.at(id), candidates).drawable)
            ++count;
    }
    return count;
}

std::vector<std::string> collectMaterialWarnings(
    const std::string& materialMode,
    const std::map<std::string, DescriptorSummary>& descriptors,
    const std::map<std::string, DescriptorMatch>& matches,
    const std::map<std::string, CoordinateAnalysis>& coordinates,
    const std::vector<SurfaceCandidate>& candidates
) {
    std::vector<std::string> warnings;
    if (materialMode == "flat")
        addWarning(warnings, "flat material uses RENDER_POST_WINDOWS; render ordering is diagnostic until visually verified");
    if (materialMode == "blur-native") {
        addWarning(warnings, "blur-native material uses RENDER_POST_WINDOWS and Hyprland native blur; render ordering is diagnostic until visually verified");
        addWarning(warnings, "blur-native frost maps to CRectPassElement blurA alpha; effective blur kernel strength still comes from global Hyprland decoration:blur settings");
    }
    for (const auto& [id, descriptor] : descriptors) {
        const auto material = analyzeMaterialDrawable(materialMode, descriptor, matches.at(id), coordinates.at(id), candidates);
        for (const auto& warning : material.warnings)
            addWarning(warnings, warning);
    }
    return warnings;
}

const SurfaceCandidate* matchedCandidate(const DescriptorMatch& match, const std::vector<SurfaceCandidate>& candidates) {
    if (match.candidateIndexes.size() != 1)
        return nullptr;
    const int index = match.candidateIndexes.front();
    if (index < 0 || static_cast<size_t>(index) >= candidates.size())
        return nullptr;
    return &candidates[index];
}

void updateLastDebugOverlayRenderStatus(const std::string& status) {
    std::lock_guard guard(g_stateMutex);
    g_lastDebugOverlayRenderStatus = status;
}

void updateLastMaterialRenderStatus(const std::string& status) {
    std::lock_guard guard(g_stateMutex);
    g_lastMaterialRenderStatus = status;
}

void drawDebugLine(const RectSummary& rect, const CHyprColor& color) {
    if (!g_pHyprRenderer || !rectValid(rect))
        return;

    CRectPassElement::SRectData data;
    data.box = CBox(rect.x, rect.y, rect.width, rect.height);
    data.color = color;
    data.round = 0;
    data.blur = false;
    data.xray = false;
    g_pHyprRenderer->m_renderPass.add(makeUnique<CRectPassElement>(std::move(data)));
}

void drawMaterialRect(const RectSummary& rect, const CHyprColor& color, int round, bool blur, double blurAlpha) {
    if (!g_pHyprRenderer || !rectValid(rect))
        return;

    CRectPassElement::SRectData data;
    data.box = CBox(rect.x, rect.y, rect.width, rect.height);
    data.color = color;
    data.round = std::max(0, round);
    data.roundingPower = 2.0F;
    data.blur = blur;
    data.blurA = static_cast<float>(clampDouble(blurAlpha, 0.0, 1.0));
    data.xray = false;
    g_pHyprRenderer->m_renderPass.add(makeUnique<CRectPassElement>(std::move(data)));
}

void drawDebugOutline(const RectSummary& rect, const CHyprColor& color, double thickness) {
    if (!rectValid(rect))
        return;

    thickness = std::max(1.0, thickness);
    const double clampedHorizontal = std::min(thickness, rect.height);
    const double clampedVertical = std::min(thickness, rect.width);

    drawDebugLine({.x = rect.x, .y = rect.y, .width = rect.width, .height = clampedHorizontal}, color);
    drawDebugLine({.x = rect.x, .y = rect.y + rect.height - clampedHorizontal, .width = rect.width, .height = clampedHorizontal}, color);
    drawDebugLine({.x = rect.x, .y = rect.y, .width = clampedVertical, .height = rect.height}, color);
    drawDebugLine({.x = rect.x + rect.width - clampedVertical, .y = rect.y, .width = clampedVertical, .height = rect.height}, color);
}

void drawMismatchMarker(const RectSummary& rect) {
    if (!rectValid(rect))
        return;

    constexpr double markerSize = 12.0;
    drawDebugLine({.x = rect.x, .y = rect.y, .width = std::min(markerSize, rect.width), .height = std::min(markerSize, rect.height)}, CHyprColor(1.0F, 0.25F, 0.0F, 0.85F));
}

void renderDebugOverlay(eRenderStage stage) {
    if (stage != RENDER_LAST_MOMENT)
        return;

    auto snapshot = activeStatusSnapshot();
    if (!snapshot.debugOverlayEnabled)
        return;
    if (!g_pHyprRenderer) {
        updateLastDebugOverlayRenderStatus("renderer unavailable");
        return;
    }

    const auto currentMonitor = g_pHyprRenderer->renderData().pMonitor.lock();
    if (!currentMonitor) {
        updateLastDebugOverlayRenderStatus("current monitor unavailable");
        return;
    }

    auto candidates = discoverLayerSurfaces();
    auto matches = matchDescriptorsToSurfaces(snapshot.descriptors, candidates);
    auto coordinates = analyzeDescriptorCoordinates(snapshot.descriptors, matches, candidates);
    const int drawableCount = countDrawableDebugOverlays(snapshot.debugOverlayEnabled, snapshot.descriptors, matches, coordinates, candidates);
    if (drawableCount == 0) {
        updateLastDebugOverlayRenderStatus("no drawable descriptors");
        return;
    }
    if (currentMonitor->m_transform != WL_OUTPUT_TRANSFORM_NORMAL)
        return;

    for (const auto& [id, descriptor] : snapshot.descriptors) {
        const auto& match = matches.at(id);
        const auto* candidate = matchedCandidate(match, candidates);
        if (!candidate || candidate->monitor != currentMonitor->m_name)
            continue;

        const auto overlay = analyzeDebugOverlayDrawable(snapshot.debugOverlayEnabled, descriptor, match, coordinates.at(id), candidates);
        if (!overlay.drawable)
            continue;

        const CHyprColor descriptorColor = overlay.mismatch ? CHyprColor(1.0F, 0.0F, 1.0F, 1.0F) : CHyprColor(0.0F, 1.0F, 0.0F, 1.0F);
        const CHyprColor surfaceColor = overlay.mismatch ? CHyprColor(1.0F, 0.85F, 0.0F, 1.0F) : CHyprColor(0.0F, 0.9F, 1.0F, 1.0F);
        drawDebugOutline(overlay.rectUsed, descriptorColor, 8.0);
        if (overlay.hasSurfaceRect)
            drawDebugOutline(overlay.surfaceRectUsed, surfaceColor, 4.0);
        if (overlay.mismatch)
            drawMismatchMarker(overlay.rectUsed);
    }

    updateLastDebugOverlayRenderStatus("ok");
}

void renderCompositorMaterial(eRenderStage stage) {
    if (stage != RENDER_POST_WINDOWS)
        return;

    auto snapshot = activeStatusSnapshot();
    if (!materialModeEnabled(snapshot.materialMode))
        return;
    if (!g_pHyprRenderer) {
        updateLastMaterialRenderStatus("renderer unavailable");
        return;
    }

    const auto currentMonitor = g_pHyprRenderer->renderData().pMonitor.lock();
    if (!currentMonitor) {
        updateLastMaterialRenderStatus("current monitor unavailable");
        return;
    }
    if (currentMonitor->m_transform != WL_OUTPUT_TRANSFORM_NORMAL)
        return;

    auto candidates = discoverLayerSurfaces();
    auto matches = matchDescriptorsToSurfaces(snapshot.descriptors, candidates);
    auto coordinates = analyzeDescriptorCoordinates(snapshot.descriptors, matches, candidates);
    const int drawableCount = countDrawableMaterials(snapshot.materialMode, snapshot.descriptors, matches, coordinates, candidates);
    if (drawableCount == 0) {
        updateLastMaterialRenderStatus("no drawable descriptors");
        return;
    }

    int renderedCount = 0;
    for (const auto& [id, descriptor] : snapshot.descriptors) {
        const auto& match = matches.at(id);
        const auto* candidate = matchedCandidate(match, candidates);
        if (!candidate || candidate->monitor != currentMonitor->m_name)
            continue;

        const auto material = analyzeMaterialDrawable(snapshot.materialMode, descriptor, match, coordinates.at(id), candidates);
        if (!material.drawable)
            continue;

        drawMaterialRect(material.rectUsed, material.color, static_cast<int>(std::round(material.radiusUsed)), material.blurEnabled, material.blurAlphaUsed);
        ++renderedCount;
    }

    if (renderedCount > 0)
        updateLastMaterialRenderStatus("ok");
}

void renderHyprGlass(eRenderStage stage) {
    renderCompositorMaterial(stage);
    renderDebugOverlay(stage);
}

void damageAllMonitors() {
    if (!g_pHyprRenderer)
        return;

    const auto& monitorState = State::monitorState();
    if (!monitorState)
        return;

    for (const auto& monitor : monitorState->monitors()) {
        if (monitor)
            g_pHyprRenderer->damageMonitor(monitor);
    }
}

json debugOverlayStatusToJSON(
    const StatusSnapshot& snapshot,
    const std::map<std::string, DescriptorMatch>& matches,
    const std::map<std::string, CoordinateAnalysis>& coordinates,
    const std::vector<SurfaceCandidate>& candidates
) {
    const int drawableCount = countDrawableDebugOverlays(snapshot.debugOverlayEnabled, snapshot.descriptors, matches, coordinates, candidates);
    return {
        {"enabled", snapshot.debugOverlayEnabled},
        {"renderHookInstalled", static_cast<bool>(g_renderStageListener)},
        {"drawableDescriptorCount", drawableCount},
        {"skippedDescriptorCount", static_cast<int>(snapshot.descriptors.size()) - drawableCount},
        {"lastRenderStatus", snapshot.debugOverlayEnabled ? snapshot.lastDebugOverlayRenderStatus : "disabled"},
        {"warnings", collectDebugOverlayWarnings(snapshot.debugOverlayEnabled, snapshot.descriptors, matches, coordinates, candidates)},
    };
}

json materialStatusToJSON(
    const StatusSnapshot& snapshot,
    const std::map<std::string, DescriptorMatch>& matches,
    const std::map<std::string, CoordinateAnalysis>& coordinates,
    const std::vector<SurfaceCandidate>& candidates
) {
    const int drawableCount = countDrawableMaterials(snapshot.materialMode, snapshot.descriptors, matches, coordinates, candidates);
    json out = {
        {"enabled", snapshot.materialMode != "off"},
        {"mode", snapshot.materialMode},
        {"renderHookInstalled", static_cast<bool>(g_renderStageListener)},
        {"renderStage", materialRenderStageFor(snapshot.materialMode)},
        {"drawableDescriptorCount", drawableCount},
        {"skippedDescriptorCount", static_cast<int>(snapshot.descriptors.size()) - drawableCount},
        {"lastRenderStatus", snapshot.materialMode != "off" ? snapshot.lastMaterialRenderStatus : "disabled"},
        {"warnings", collectMaterialWarnings(snapshot.materialMode, snapshot.descriptors, matches, coordinates, candidates)},
    };
    if (snapshot.materialMode == "blur-native") {
        out["nativeBlurEnabled"] = true;
        out["effectiveBlurControl"] = "global-kernel-per-surface-alpha";
        out["perSurfaceBlurSupported"] = false;
        out["perSurfaceBlurSupport"] = "alpha-only";
    }
    return out;
}

std::string normalStatus() {
    auto snapshot = activeStatusSnapshot();
    auto monitors = discoverMonitors();
    auto candidates = discoverLayerSurfaces();
    auto matches = matchDescriptorsToSurfaces(snapshot.descriptors, candidates);
    auto coordinates = analyzeDescriptorCoordinates(snapshot.descriptors, matches, candidates);

    std::ostringstream out;
    out << "hgs-hyprglass\n";
    out << "  pluginLoaded: true\n";
    out << "  available: true\n";
    out << "  compositorRendering: " << (snapshot.materialMode != "off" ? "true" : "false") << "\n";
    out << "  materialMode: " << snapshot.materialMode << "\n";
    out << "  materialRenderStage: " << materialRenderStageFor(snapshot.materialMode) << "\n";
    out << "  materialDrawableCount: " << countDrawableMaterials(snapshot.materialMode, snapshot.descriptors, matches, coordinates, candidates) << "\n";
    out << "  materialLastRenderStatus: " << (snapshot.materialMode != "off" ? snapshot.lastMaterialRenderStatus : "disabled") << "\n";
    out << "  generation: " << snapshot.generation << "\n";
    out << "  applyCount: " << snapshot.applyCount << "\n";
    out << "  descriptorCount: " << snapshot.descriptors.size() << "\n";
    out << "  matchedDescriptorCount: " << countMatchStatus(matches, "matched") << "\n";
    out << "  unmatchedDescriptorCount: " << countMatchStatus(matches, "unmatched") << "\n";
    out << "  ambiguousDescriptorCount: " << countMatchStatus(matches, "ambiguous") << "\n";
    out << "  candidateSurfaceCount: " << candidates.size() << "\n";
    out << "  monitorCount: " << monitors.size() << "\n";
    out << "  fractionalScaleMonitorCount: " << countFractionalScaleMonitors(monitors) << "\n";
    out << "  transformedMonitorCount: " << countTransformedMonitors(monitors) << "\n";
    out << "  unsupportedTransformMonitorCount: " << countUnsupportedTransformMonitors(monitors) << "\n";
    out << "  staleDescriptorCount: " << countPotentiallyStaleDescriptors(matches) << "\n";
    out << "  coordinateAlignedCount: " << countCoordinateStatus(coordinates, "aligned") << "\n";
    out << "  coordinateNearCount: " << countCoordinateStatus(coordinates, "near") << "\n";
    out << "  coordinateMismatchedCount: " << countCoordinateStatus(coordinates, "mismatched") << "\n";
    out << "  coordinateUnknownCount: " << countCoordinateStatus(coordinates, "unknown") << "\n";
    out << "  debugOverlayEnabled: " << (snapshot.debugOverlayEnabled ? "true" : "false") << "\n";
    out << "  debugOverlayDrawableCount: " << countDrawableDebugOverlays(snapshot.debugOverlayEnabled, snapshot.descriptors, matches, coordinates, candidates) << "\n";
    out << "  debugOverlayLastRenderStatus: " << (snapshot.debugOverlayEnabled ? snapshot.lastDebugOverlayRenderStatus : "disabled") << "\n";
    out << "  hasPayload: " << (!snapshot.descriptors.empty() ? "true" : "false") << "\n";
    out << "  lastApplyStatus: " << snapshot.lastApplyStatus << "\n";
    if (!snapshot.lastError.empty())
        out << "  lastError: " << snapshot.lastError << "\n";
    for (const auto& [id, descriptor] : snapshot.descriptors) {
        const auto& match = matches.at(id);
        const auto& coordinate = coordinates.at(id);
        out << "  descriptor: " << id << " surface=" << match.status << " coordinate=" << coordinate.status << " reason=\"" << match.reason << "\"";
        if (!descriptor.namespaceName.empty())
            out << " namespace=" << descriptor.namespaceName;
        if (!descriptor.monitor.empty())
            out << " monitor=" << descriptor.monitor;
        out << "\n";
    }
    return out.str();
}

std::string jsonStatus() {
    auto snapshot = activeStatusSnapshot();
    auto monitors = discoverMonitors();
    auto candidates = discoverLayerSurfaces();
    auto matches = matchDescriptorsToSurfaces(snapshot.descriptors, candidates);
    auto coordinates = analyzeDescriptorCoordinates(snapshot.descriptors, matches, candidates);

    json descriptors = json::array();
    for (const auto& [id, descriptor] : snapshot.descriptors)
        descriptors.push_back(descriptorToJSON(snapshot.debugOverlayEnabled, snapshot.materialMode, descriptor, matches.at(id), coordinates.at(id), candidates));

    json candidateSurfaces = json::array();
    for (const auto& candidate : candidates)
        candidateSurfaces.push_back(surfaceCandidateToJSON(candidate));

    json monitorStatuses = json::array();
    for (const auto& monitor : monitors)
        monitorStatuses.push_back(monitorToJSON(monitor));

    json status = {
        {"version", 1},
        {"plugin", "hgs-hyprglass"},
        {"pluginLoaded", true},
        {"available", true},
        {"compositorRendering", snapshot.materialMode != "off"},
        {"generation", snapshot.generation},
        {"applyCount", snapshot.applyCount},
        {"descriptorCount", snapshot.descriptors.size()},
        {"matchedDescriptorCount", countMatchStatus(matches, "matched")},
        {"unmatchedDescriptorCount", countMatchStatus(matches, "unmatched")},
        {"ambiguousDescriptorCount", countMatchStatus(matches, "ambiguous")},
        {"skippedDescriptorCount", countMatchStatus(matches, "skipped")},
        {"errorDescriptorCount", countMatchStatus(matches, "error")},
        {"candidateSurfaceCount", candidates.size()},
        {"monitorCount", monitors.size()},
        {"fractionalScaleMonitorCount", countFractionalScaleMonitors(monitors)},
        {"transformedMonitorCount", countTransformedMonitors(monitors)},
        {"unsupportedTransformMonitorCount", countUnsupportedTransformMonitors(monitors)},
        {"staleDescriptorCount", countPotentiallyStaleDescriptors(matches)},
        {"coordinateAlignedCount", countCoordinateStatus(coordinates, "aligned")},
        {"coordinateNearCount", countCoordinateStatus(coordinates, "near")},
        {"coordinateMismatchedCount", countCoordinateStatus(coordinates, "mismatched")},
        {"coordinateUnknownCount", countCoordinateStatus(coordinates, "unknown")},
        {"hasPayload", !snapshot.descriptors.empty()},
        {"lastApplyStatus", snapshot.lastApplyStatus},
        {"descriptors", descriptors},
        {"candidateSurfaces", candidateSurfaces},
        {"monitors", monitorStatuses},
        {"warnings", collectStatusWarnings(monitors, matches, coordinates)},
        {"debugOverlay", debugOverlayStatusToJSON(snapshot, matches, coordinates, candidates)},
        {"material", materialStatusToJSON(snapshot, matches, coordinates, candidates)},
    };
    if (!snapshot.lastError.empty())
        status["lastError"] = snapshot.lastError;
    return status.dump();
}

std::string applyDescriptorPayload(std::string payload) {
    payload = trim(payload);
    if (payload.empty()) {
        std::lock_guard guard(g_stateMutex);
        g_lastApplyStatus = "rejected";
        g_lastError = "missing descriptor JSON payload";
        return "error: missing descriptor JSON payload\n";
    }
    if (payload.front() != '{') {
        std::lock_guard guard(g_stateMutex);
        g_lastApplyStatus = "rejected";
        g_lastError = "descriptor payload must be a JSON object";
        return "error: descriptor payload must be a JSON object\n";
    }

    std::map<std::string, DescriptorSummary> parsed;
    std::string error;
    if (!parseDescriptorPayload(payload, parsed, error)) {
        std::lock_guard guard(g_stateMutex);
        g_lastApplyStatus = "rejected";
        g_lastError = std::move(error);
        return "error: " + g_lastError + "\n";
    }

    bool shouldDamage = false;
    {
        std::lock_guard guard(g_stateMutex);
        g_descriptors = std::move(parsed);
        g_lastApplyStatus = "accepted";
        g_lastError.clear();
        ++g_generation;
        ++g_applyCount;
        shouldDamage = g_debugOverlayEnabled || g_materialMode != "off";
    }
    if (shouldDamage)
        damageAllMonitors();
    return "ok\n";
}

std::string clearDescriptors() {
    bool shouldDamage = false;
    {
        std::lock_guard guard(g_stateMutex);
        g_descriptors.clear();
        g_lastApplyStatus = "cleared";
        g_lastError.clear();
        ++g_generation;
        shouldDamage = g_debugOverlayEnabled || g_materialMode != "off";
    }
    if (shouldDamage)
        damageAllMonitors();
    return "ok\n";
}

std::string debugOverlayCommandStatus(eHyprCtlOutputFormat format) {
    auto snapshot = activeStatusSnapshot();
    if (format == FORMAT_JSON) {
        json status = {
            {"enabled", snapshot.debugOverlayEnabled},
            {"renderHookInstalled", static_cast<bool>(g_renderStageListener)},
            {"lastRenderStatus", snapshot.debugOverlayEnabled ? snapshot.lastDebugOverlayRenderStatus : "disabled"},
        };
        return status.dump();
    }
    return std::string("debugOverlay: ") + (snapshot.debugOverlayEnabled ? "on" : "off") + "\n";
}

std::string setDebugOverlayEnabled(bool enabled) {
    {
        std::lock_guard guard(g_stateMutex);
        g_debugOverlayEnabled = enabled;
        g_lastDebugOverlayRenderStatus = enabled ? "pending" : "disabled";
    }
    damageAllMonitors();
    return std::string("debugOverlay: ") + (enabled ? "on" : "off") + "\n";
}

std::string materialCommandStatus(eHyprCtlOutputFormat format) {
    auto snapshot = activeStatusSnapshot();
    if (format == FORMAT_JSON) {
        json status = {
            {"enabled", snapshot.materialMode != "off"},
            {"mode", snapshot.materialMode},
            {"renderHookInstalled", static_cast<bool>(g_renderStageListener)},
            {"renderStage", materialRenderStageFor(snapshot.materialMode)},
            {"lastRenderStatus", snapshot.materialMode != "off" ? snapshot.lastMaterialRenderStatus : "disabled"},
        };
        if (snapshot.materialMode == "blur-native") {
            status["nativeBlurEnabled"] = true;
            status["effectiveBlurControl"] = "global-kernel-per-surface-alpha";
            status["perSurfaceBlurSupported"] = false;
            status["perSurfaceBlurSupport"] = "alpha-only";
        }
        return status.dump();
    }
    return std::string("material: ") + snapshot.materialMode + "\n";
}

std::string setMaterialMode(const std::string& mode) {
    {
        std::lock_guard guard(g_stateMutex);
        g_materialMode = mode;
        g_lastMaterialRenderStatus = mode == "off" ? "disabled" : "pending";
    }
    damageAllMonitors();
    return std::string("material: ") + mode + "\n";
}

std::string hyprglassStatusRequest(eHyprCtlOutputFormat format, std::string) {
    return format == FORMAT_JSON ? jsonStatus() : normalStatus();
}

std::string hyprglassApplyRequest(eHyprCtlOutputFormat, std::string request) {
    return applyDescriptorPayload(removePrefix(std::move(request), "hyprglass-apply-json"));
}

std::string hyprglassClearRequest(eHyprCtlOutputFormat, std::string) {
    return clearDescriptors();
}

std::string hyprglassDebugOverlayRequest(eHyprCtlOutputFormat format, std::string request) {
    const std::string mode = toLower(removePrefix(std::move(request), "hyprglass-debug-overlay"));
    if (mode.empty() || mode == "status")
        return debugOverlayCommandStatus(format);
    if (mode == "on" || mode == "enable" || mode == "enabled" || mode == "true" || mode == "1")
        return setDebugOverlayEnabled(true);
    if (mode == "off" || mode == "disable" || mode == "disabled" || mode == "false" || mode == "0")
        return setDebugOverlayEnabled(false);
    if (mode == "toggle") {
        const bool enabled = activeStatusSnapshot().debugOverlayEnabled;
        return setDebugOverlayEnabled(!enabled);
    }
    return "error: expected on, off, toggle, or status\n";
}

std::string hyprglassMaterialRequest(eHyprCtlOutputFormat format, std::string request) {
    const std::string mode = toLower(removePrefix(std::move(request), "hyprglass-material"));
    if (mode.empty() || mode == "status")
        return materialCommandStatus(format);
    if (mode == "off" || mode == "disable" || mode == "disabled" || mode == "false" || mode == "0")
        return setMaterialMode("off");
    if (mode == "flat")
        return setMaterialMode("flat");
    if (mode == "blur-native")
        return setMaterialMode("blur-native");
    return "error: expected off, flat, blur-native, or status\n";
}

}

APICALL EXPORT std::string PLUGIN_API_VERSION() {
    return HYPRLAND_API_VERSION;
}

APICALL EXPORT PLUGIN_DESCRIPTION_INFO PLUGIN_INIT(HANDLE handle) {
    g_handle = handle;

    SHyprCtlCommand statusCommand;
    statusCommand.name  = "hyprglass-status";
    statusCommand.exact = true;
    statusCommand.fn    = hyprglassStatusRequest;
    g_statusCommand = HyprlandAPI::registerHyprCtlCommand(g_handle, statusCommand);

    SHyprCtlCommand applyCommand;
    applyCommand.name  = "hyprglass-apply-json";
    applyCommand.exact = false;
    applyCommand.fn    = hyprglassApplyRequest;
    g_applyCommand = HyprlandAPI::registerHyprCtlCommand(g_handle, applyCommand);

    SHyprCtlCommand clearCommand;
    clearCommand.name  = "hyprglass-clear";
    clearCommand.exact = true;
    clearCommand.fn    = hyprglassClearRequest;
    g_clearCommand = HyprlandAPI::registerHyprCtlCommand(g_handle, clearCommand);

    SHyprCtlCommand debugOverlayCommand;
    debugOverlayCommand.name  = "hyprglass-debug-overlay";
    debugOverlayCommand.exact = false;
    debugOverlayCommand.fn    = hyprglassDebugOverlayRequest;
    g_debugOverlayCommand = HyprlandAPI::registerHyprCtlCommand(g_handle, debugOverlayCommand);

    SHyprCtlCommand materialCommand;
    materialCommand.name  = "hyprglass-material";
    materialCommand.exact = false;
    materialCommand.fn    = hyprglassMaterialRequest;
    g_materialCommand = HyprlandAPI::registerHyprCtlCommand(g_handle, materialCommand);

    if (Event::bus())
        g_renderStageListener = Event::bus()->m_events.render.stage.listen(renderHyprGlass);

    return {"hgs-hyprglass", "HyprGlassShell compositor-side glass material engine", "CoastLineSec", "0.1.0"};
}

APICALL EXPORT void PLUGIN_EXIT() {
    {
        std::lock_guard guard(g_stateMutex);
        g_debugOverlayEnabled = false;
        g_materialMode = "off";
        g_lastDebugOverlayRenderStatus = "disabled";
        g_lastMaterialRenderStatus = "disabled";
    }
    damageAllMonitors();
    g_renderStageListener.reset();
    if (g_materialCommand)
        HyprlandAPI::unregisterHyprCtlCommand(g_handle, g_materialCommand);
    if (g_debugOverlayCommand)
        HyprlandAPI::unregisterHyprCtlCommand(g_handle, g_debugOverlayCommand);
    if (g_clearCommand)
        HyprlandAPI::unregisterHyprCtlCommand(g_handle, g_clearCommand);
    if (g_applyCommand)
        HyprlandAPI::unregisterHyprCtlCommand(g_handle, g_applyCommand);
    if (g_statusCommand)
        HyprlandAPI::unregisterHyprCtlCommand(g_handle, g_statusCommand);
    g_clearCommand.reset();
    g_debugOverlayCommand.reset();
    g_materialCommand.reset();
    g_applyCommand.reset();
    g_statusCommand.reset();
    g_handle = nullptr;
}
