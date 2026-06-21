#include <hyprland/src/SharedDefs.hpp>
#include <hyprland/src/Compositor.hpp>
#include <hyprland/src/desktop/view/LayerSurface.hpp>
#include <hyprland/src/output/Monitor.hpp>
#include <hyprland/src/plugins/PluginAPI.hpp>
#include <hyprland/src/protocols/LayerShell.hpp>
#include <hyprland/src/render/Renderer.hpp>
#include <hyprland/src/render/OpenGL.hpp>
#include <hyprland/src/render/pass/BorderPassElement.hpp>
#include <hyprland/src/render/pass/RectPassElement.hpp>
#include <hyprland/src/render/pass/TexPassElement.hpp>
#include <hyprland/src/state/MonitorState.hpp>

#include <nlohmann/json.hpp>

#include <algorithm>
#include <array>
#include <cctype>
#include <chrono>
#include <cstdint>
#include <ctime>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <map>
#include <mutex>
#include <set>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#ifndef HGS_HYPRGLASS_PLUGIN_VERSION
#define HGS_HYPRGLASS_PLUGIN_VERSION "0.1.0"
#endif

#ifndef HGS_HYPRGLASS_BUILD_ID
#define HGS_HYPRGLASS_BUILD_ID "unknown"
#endif

#ifndef HGS_HYPRGLASS_BUILD_TIME
#define HGS_HYPRGLASS_BUILD_TIME "unknown"
#endif

#ifndef HGS_HYPRGLASS_GIT_COMMIT
#define HGS_HYPRGLASS_GIT_COMMIT "unknown"
#endif

#ifndef HGS_HYPRGLASS_BUILD_TYPE
#define HGS_HYPRGLASS_BUILD_TYPE "unknown"
#endif

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
std::string           g_lastBackdropCaptureStatus = "disabled";
std::string           g_lastBackdropCaptureError;
std::string           g_materialMode = "off";
uint64_t              g_generation = 0;
uint64_t              g_applyCount  = 0;
uint64_t              g_captureGeneration = 0;
bool                  g_debugOverlayEnabled = false;




SP<CShader>  g_fluidGlassShader;
bool         g_fluidGlassShaderCompileAttempted = false;
bool         g_fluidGlassShaderCompiled = false;
std::string  g_fluidGlassShaderError;

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
    std::string              rectUsedSpace = "monitor-framebuffer-pixels-rounded";
    std::string              surfaceRectUsedSpace = "monitor-framebuffer-pixels-rounded";
    std::string              drawMapping;
    std::vector<std::string> drawWarnings;
    int                      drawTransform = 0;
    bool                     drawTransformSupported = true;
    bool                     drawable = false;
    bool                     hasSurfaceRect = false;
    bool                     mismatch = false;
};

struct EdgeLensBandSummary {
    bool        enabled = false;
    RectSummary destination;
    RectSummary source;
    double      offsetX = 0.0;
    double      offsetY = 0.0;
};

struct UvCandidateSliceSummary {
    int         index = 0;
    std::string candidateName;
    std::string formula;
    bool        inBounds = false;
    RectSummary destinationSliceRect;
    RectSummary sourceSliceRect;
    RectSummary sourceUvTopLeft;
    RectSummary sourceUvTopRight;
    RectSummary sourceUvBottomRight;
    RectSummary sourceUvBottomLeft;
};

struct CaptureQuadCandidate {
    std::string name;
    std::string formula;
    std::string notes;
    std::string space = "capture-texture-pixels";
    RectSummary topLeft;
    RectSummary topRight;
    RectSummary bottomRight;
    RectSummary bottomLeft;
    RectSummary bounds;
    RectSummary uvTopLeft;
    RectSummary uvTopRight;
    RectSummary uvBottomRight;
    RectSummary uvBottomLeft;
    int         transform = 0;
    bool        axisAligned = false;
    bool        inBounds = false;
    std::string confidence = "diagnostic";
};

struct MaterialDescriptor {
    std::string              status;
    std::string              reason;
    std::vector<std::string> warnings;
    RectSummary              rectUsed;
    RectSummary              globalRectUsed;
    std::string              rectUsedSpace = "monitor-framebuffer-pixels-rounded";
    std::string              renderStage = "RENDER_POST_WINDOWS";
    std::string              mode = "off";
    std::string              tintColorRequested;
    std::string              colorUsed;
    std::string              effectiveBlurSource;
    std::string              effectiveBlurControl;
    std::string              rimTechnique = "disabled";
    std::string              rimColorUsed;
    std::string              innerEdgeTechnique = "disabled";
    std::string              innerEdgeColorUsed;
    std::string              drawMapping;
    std::string              descriptorId;
    std::string              captureMonitor;
    std::string              captureStage = "RENDER_POST_WINDOWS";
    std::string              captureStatus = "not-attempted";
    std::string              captureError;
    std::string              captureBackend;
    std::string              sourceMapping;
    std::string              sourceMappingEvidence;
    std::string              sourceCroppingMode = "ctex-axis-aligned-uv-rect";
    std::string              uvMappingType = "axis-aligned-uv-rect-two-corner";
    std::string              shaderBackend = "ctex-pass-custom-uv";
    std::string              shaderError;
    std::string              sdfTechnique = "disabled";
    std::string              shaderSourceMapTechnique = "disabled";
    std::string              edgeLensBackend = "ctex-pass-band-offsets";
    std::string              transformPolicy = "renderer-projected-band-offsets";
    std::string              roundedCornerStrategy = "full-rounded-base-plus-corner-inset-bands";
    std::string              backendUsed;
    std::string              fallbackReason;
    std::vector<std::string> drawWarnings;
    CHyprColor               color = CHyprColor(0.92F, 0.95F, 1.0F, 0.0F);
    RectSummary              sourceBackdropRect;
    RectSummary              destinationRect;
    RectSummary              textureSize;
    RectSummary              sourceExtent;
    RectSummary              sourceUvRect;
    RectSummary              sourceUvTopLeft;
    RectSummary              sourceUvTopRight;
    RectSummary              sourceUvBottomRight;
    RectSummary              sourceUvBottomLeft;
    RectSummary              centerRegion;
    CaptureQuadCandidate     selectedSourceQuad;
    std::vector<CaptureQuadCandidate> sourceMappingCandidates;
    EdgeLensBandSummary      topBand;
    EdgeLensBandSummary      bottomBand;
    EdgeLensBandSummary      leftBand;
    EdgeLensBandSummary      rightBand;
    std::vector<UvCandidateSliceSummary> candidateSlices;
    double                   monitorScale = 1.0;
    double                   radiusRequested = 0.0;
    double                   radiusUsed = 0.0;
    double                   opacityRequested = 0.0;
    double                   tintOpacityRequested = 0.0;
    double                   requestedFrost = 0.0;
    double                   blurAlphaUsed = 0.0;
    double                   alphaUsed = 0.0;
    double                   tintOverlayAlphaUsed = 0.0;
    double                   rimAlphaUsed = 0.0;
    double                   rimExpansionPx = 0.0;
    double                   innerEdgeAlphaUsed = 0.0;
    double                   innerEdgeInsetPx = 0.0;
    double                   highlightAlphaUsed = 0.0;
    double                   shadowAlphaUsed = 0.0;
    double                   displacementOffsetX = 0.0;
    double                   displacementOffsetY = 0.0;
    double                   edgeWidthPx = 0.0;
    double                   lensOffsetPx = 0.0;
    double                   sdfDisplacementStrengthPx = 0.0;
    double                   sdfEdgeWidthPx = 0.0;
    int                      drawTransform = 0;
    int                      passCount = 0;
    bool                     drawable = false;
    bool                     rounded = false;
    bool                     blurEnabled = false;
    bool                     tintOverlayEnabled = false;
    bool                     rimEnabled = false;
    bool                     innerEdgeEnabled = false;
    bool                     highlightEnabled = false;
    bool                     highlightTransformSafe = true;
    bool                     shadowEnabled = false;
    bool                     drawTransformSupported = true;
    bool                     perSurfaceBlurSupported = false;
    bool                     captureEnabled = false;
    bool                     captureAttempted = false;
    bool                     captureReady = false;
    bool                     captureTextureReady = false;
    bool                     textureReady = false;
    bool                     sampled = false;
    bool                     descriptorRendered = false;
    bool                     descriptorUsedCapture = false;
    bool                     renderedFromStaleCapture = false;
    bool                     shaderEnabled = false;
    bool                     shaderCompiled = false;
    bool                     shaderReady = true;
    bool                     displacementEnabled = false;
    bool                     edgeLensEnabled = false;
    bool                     edgeLensSupported = false;
    bool                     sdfMaskEnabled = false;
    bool                     uvOrientationEnabled = false;
    bool                     refractionDebugEnabled = false;
    bool                     transformShaderSupported = true;
    bool                     roundedMaskEnabled = false;
    bool                     refractionEnabled = false;
    bool                     selfSamplingRisk = true;
    bool                     transformCaptureSupported = false;
    bool                     transformCaptureDiagnosticEnabled = false;
    bool                     shaderUsesFourCornerUV = false;
    bool                     distortionUsesLocalPixelSpace = false;
    bool                     distortionUsesSourceQuadBasis = false;
    bool                     distortionClampedToSourceQuad = false;
    uint64_t                 captureGeneration = 0;
    std::string              perSurfaceBlurSupport;
};

struct BackdropCaptureRecord {
    std::string descriptorId;
    std::string monitor;
    std::string status = "not-attempted";
    std::string error;
    std::string backend = "hyprland-renderer-temp-fb-texture-copy";
    std::string sourceMapping = "monitor-framebuffer-axis-aligned-uv";
    std::string sourceCroppingMode = "ctex-axis-aligned-uv-rect";
    std::string uvMappingType = "axis-aligned-uv-rect";
    RectSummary sourceBackdropRect;
    RectSummary destinationRect;
    RectSummary textureSize;
    RectSummary sourceExtent;
    RectSummary sourceUvRect;
    RectSummary sourceUvTopLeft;
    RectSummary sourceUvTopRight;
    RectSummary sourceUvBottomRight;
    RectSummary sourceUvBottomLeft;
    std::vector<UvCandidateSliceSummary> candidateSlices;
    CaptureQuadCandidate selectedSourceQuad;
    std::vector<CaptureQuadCandidate> sourceMappingCandidates;
    std::string selectedSourceMappingCandidate;
    uint64_t    captureGeneration = 0;
    bool        captureAttempted = false;
    bool        transformCaptureDiagnosticEnabled = false;
    bool        shaderUsesFourCornerUV = false;
    bool        captureReady = false;
    bool        captureTextureReady = false;
    bool        textureReady = false;
    bool        sampled = false;
    bool        descriptorRendered = false;
    bool        descriptorUsedCapture = false;
    bool        renderedFromStaleCapture = false;
    bool        selfSamplingRisk = true;
};

struct MonitorBackdropCaptureRecord {
    std::string monitor;
    std::string status = "not-attempted";
    std::string error;
    std::string backend = "hyprland-renderer-temp-fb-texture-copy";
    std::string captureCopyMethod = "tex-pass-existing-projection";
    std::string captureCopyProjection = "unknown";
    RectSummary textureSize;
    RectSummary sourceExtent;
    uint64_t    captureGeneration = 0;
    bool        captureAttempted = false;
    bool        captureFaithfulCopyAttempted = false;
    bool        captureCopyProjectionRestored = false;
    bool        captureReady = false;
    bool        captureTextureReady = false;
    bool        textureReady = false;
    bool        selfSamplingRisk = true;
};


bool positiveExtent(const RectSummary& rect) {
    return rect.width > 0.0 && rect.height > 0.0;
}

bool monitorCaptureRecordReady(const MonitorBackdropCaptureRecord& record) {
    return record.captureReady && record.captureTextureReady && record.textureReady && positiveExtent(record.textureSize) && positiveExtent(record.sourceExtent);
}

bool captureRecordReady(const BackdropCaptureRecord& record) {
    return record.captureReady && record.captureTextureReady && record.textureReady && positiveExtent(record.textureSize) && positiveExtent(record.sourceExtent);
}

void copyMonitorCaptureStateToDescriptorRecord(BackdropCaptureRecord& record, const MonitorBackdropCaptureRecord& monitorRecord) {
    record.captureAttempted = monitorRecord.captureAttempted;
    record.captureGeneration = monitorRecord.captureGeneration;
    record.backend = monitorRecord.backend;
    record.sourceExtent = monitorRecord.sourceExtent;
    record.textureSize = monitorRecord.textureSize;
    record.captureTextureReady = monitorRecord.captureTextureReady && positiveExtent(monitorRecord.textureSize);
    record.textureReady = monitorRecord.textureReady && positiveExtent(monitorRecord.textureSize);
    record.captureReady = monitorCaptureRecordReady(monitorRecord);
    record.selfSamplingRisk = !record.captureReady || monitorRecord.selfSamplingRisk;
}

std::string selectedSourceMappingCandidateName(int transform);
const CaptureQuadCandidate* findCaptureQuadCandidate(const std::vector<CaptureQuadCandidate>& candidates, std::string_view name);

struct ShaderSourceUvMapping {
    bool        supported = false;
    bool        inBounds = true;
    bool        diagnosticOnly = true;
    std::string mapping;
    std::string error;
    std::string selectedCandidateName;
    RectSummary topLeft;
    RectSummary topRight;
    RectSummary bottomRight;
    RectSummary bottomLeft;
    RectSummary bounds;
    CaptureQuadCandidate selectedQuad;
    std::vector<CaptureQuadCandidate> candidates;
};

ShaderSourceUvMapping shaderSourceUvMappingFor(const MaterialDescriptor& material, double sourceWidth, double sourceHeight);

std::map<std::string, BackdropCaptureRecord>       g_backdropCaptureRecords;
std::map<std::string, MonitorBackdropCaptureRecord> g_backdropMonitorCaptureRecords;
std::map<std::string, SP<Render::IFramebuffer>>    g_backdropCaptureFramebuffers;

struct MaterialColorResolution {
    std::string tintColorRequested;
    std::string colorUsed;
    CHyprColor  color = CHyprColor(0.92F, 0.95F, 1.0F, 0.0F);
    double      opacityRequested = 0.0;
    double      tintOpacityRequested = 0.0;
    double      alphaUsed = 0.0;
};

struct DrawRectMapping {
    RectSummary              rect;
    std::string              space;
    std::string              mapping;
    std::vector<std::string> warnings;
    int                      transform = 0;
    bool                     supported = false;
};

std::map<std::string, DescriptorSummary> g_descriptors;

json buildInfoToJSON() {
    return {
        {"id", HGS_HYPRGLASS_BUILD_ID},
        {"pluginVersion", HGS_HYPRGLASS_PLUGIN_VERSION},
        {"gitCommit", HGS_HYPRGLASS_GIT_COMMIT},
        {"buildTime", HGS_HYPRGLASS_BUILD_TIME},
        {"buildType", HGS_HYPRGLASS_BUILD_TYPE},
    };
}

json capabilitiesToJSON() {
    return {
        {"materials", json::array({"flat", "blur-native", "glass-v1", "fluid-glass"})},
        {"debugOverlay", true},
        {"nativeBlur", true},
        {"fluidGlass", true},
        {"renderStages", {
            {"material", "RENDER_POST_WINDOWS"},
            {"debugOverlay", "RENDER_LAST_MOMENT"},
        }},
    };
}

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

uint64_t nextCaptureGeneration() {
    std::lock_guard guard(g_stateMutex);
    return ++g_captureGeneration;
}

BackdropCaptureRecord backdropCaptureRecordFor(const std::string& descriptorId) {
    std::lock_guard guard(g_stateMutex);
    auto it = g_backdropCaptureRecords.find(descriptorId);
    if (it == g_backdropCaptureRecords.end()) {
        BackdropCaptureRecord record;
        record.descriptorId = descriptorId;
        return record;
    }
    return it->second;
}

void updateBackdropCaptureRecord(const BackdropCaptureRecord& record) {
    std::lock_guard guard(g_stateMutex);
    g_backdropCaptureRecords[record.descriptorId] = record;
    g_lastBackdropCaptureStatus = record.status;
    g_lastBackdropCaptureError = record.error;
}

MonitorBackdropCaptureRecord monitorBackdropCaptureRecordFor(const std::string& monitor) {
    std::lock_guard guard(g_stateMutex);
    auto it = g_backdropMonitorCaptureRecords.find(monitor);
    if (it == g_backdropMonitorCaptureRecords.end()) {
        MonitorBackdropCaptureRecord record;
        record.monitor = monitor;
        return record;
    }
    return it->second;
}

void updateMonitorBackdropCaptureRecord(const MonitorBackdropCaptureRecord& record) {
    std::lock_guard guard(g_stateMutex);
    g_backdropMonitorCaptureRecords[record.monitor] = record;
    g_lastBackdropCaptureStatus = record.status;
    g_lastBackdropCaptureError = record.error;
}





struct FluidGlassShaderStatus {
    bool        compileAttempted = false;
    bool        compiled = false;
    bool        ready = false;
    std::string backend = "hyprland-cshader-gles-custom-pass";
    std::string error;
};


FluidGlassShaderStatus fluidGlassShaderStatus() {
    std::lock_guard guard(g_stateMutex);
    return {
        .compileAttempted = g_fluidGlassShaderCompileAttempted,
        .compiled = g_fluidGlassShaderCompiled,
        .ready = g_fluidGlassShaderCompiled && g_fluidGlassShaderError.empty(),
        .backend = "hyprland-cshader-gles-custom-pass",
        .error = g_fluidGlassShaderError,
    };
}


void updateFluidGlassShaderStatus(bool compiled, std::string error) {
    std::lock_guard guard(g_stateMutex);
    g_fluidGlassShaderCompileAttempted = true;
    g_fluidGlassShaderCompiled = compiled;
    g_fluidGlassShaderError = std::move(error);
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
    return transform >= 0 && transform <= 7;
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

json pointWithSpaceToJSON(std::string_view space, double x, double y) {
    return {
        {"x", x},
        {"y", y},
        {"space", space},
    };
}




json captureQuadCandidateToJSON(const CaptureQuadCandidate& quad) {
    return {
        {"name", quad.name},
        {"formula", quad.formula},
        {"notes", quad.notes},
        {"space", quad.space},
        {"transform", quad.transform},
        {"topLeft", pointWithSpaceToJSON(quad.space, quad.topLeft.x, quad.topLeft.y)},
        {"topRight", pointWithSpaceToJSON(quad.space, quad.topRight.x, quad.topRight.y)},
        {"bottomRight", pointWithSpaceToJSON(quad.space, quad.bottomRight.x, quad.bottomRight.y)},
        {"bottomLeft", pointWithSpaceToJSON(quad.space, quad.bottomLeft.x, quad.bottomLeft.y)},
        {"bounds", rectWithSpaceToJSON(quad.space, quad.bounds)},
        {"uvTopLeft", pointWithSpaceToJSON("normalized-uv", quad.uvTopLeft.x, quad.uvTopLeft.y)},
        {"uvTopRight", pointWithSpaceToJSON("normalized-uv", quad.uvTopRight.x, quad.uvTopRight.y)},
        {"uvBottomRight", pointWithSpaceToJSON("normalized-uv", quad.uvBottomRight.x, quad.uvBottomRight.y)},
        {"uvBottomLeft", pointWithSpaceToJSON("normalized-uv", quad.uvBottomLeft.x, quad.uvBottomLeft.y)},
        {"axisAligned", quad.axisAligned},
        {"inBounds", quad.inBounds},
        {"confidence", quad.confidence},
    };
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

std::string drawMappingForTransform(int transform) {
    switch (transform) {
    case 0: return "logical-to-framebuffer-normal";
    case 1: return "logical-to-framebuffer-renderer-projected-transform-1";
    case 2: return "logical-to-framebuffer-renderer-projected-transform-2";
    case 3: return "logical-to-framebuffer-renderer-projected-transform-3";
    case 4: return "logical-to-framebuffer-renderer-projected-transform-4";
    case 5: return "logical-to-framebuffer-renderer-projected-transform-5";
    case 6: return "logical-to-framebuffer-renderer-projected-transform-6";
    case 7: return "logical-to-framebuffer-renderer-projected-transform-7";
    default: return "unsupported-transform";
    }
}

std::string drawRectSpaceForTransform(int transform) {
    if (transform == 0)
        return "monitor-framebuffer-pixels-rounded";
    return "monitor-framebuffer-pixels-rounded-transform-" + std::to_string(transform);
}

DrawRectMapping mapLogicalRectToFramebuffer(const RectSummary& monitorLocalLogical, const SurfaceCandidate& candidate) {
    DrawRectMapping mapping;
    mapping.transform = candidate.monitorTransform;
    mapping.supported = isTransformSupported(candidate.monitorTransform);
    mapping.mapping = drawMappingForTransform(candidate.monitorTransform);
    mapping.space = drawRectSpaceForTransform(candidate.monitorTransform);

    if (!mapping.supported) {
        addWarning(mapping.warnings, "monitor transform is unsupported for compositor draw mapping");
        return mapping;
    }
    if (!rectValid(monitorLocalLogical)) {
        addWarning(mapping.warnings, "monitor-local logical rect is invalid");
        return mapping;
    }
    if (!candidate.hasMonitorGeometry || !rectValid(candidate.monitorLogical)) {
        addWarning(mapping.warnings, "monitor logical geometry unavailable for transform draw mapping");
        return mapping;
    }
    if (candidate.scale <= 0.0) {
        addWarning(mapping.warnings, "monitor scale unavailable for transform draw mapping");
        return mapping;
    }

    mapping.rect = roundRect(multiplyRect(monitorLocalLogical, candidate.scale));
    if (!rectValid(mapping.rect))
        addWarning(mapping.warnings, "transform draw mapping produced an invalid framebuffer rect");
    if (candidate.monitorTransform != 0)
        addWarning(mapping.warnings, "draw rect uses scaled monitor-local coordinates; Hyprland render pass projects the monitor transform");

    return mapping;
}

double clampDouble(double value, double min, double max) {
    if (!std::isfinite(value))
        return min;
    return std::clamp(value, min, max);
}

bool materialModeUsesNativeBlur(std::string_view mode) {
    return mode == "blur-native" || mode == "glass-v1";
}


bool materialModeUsesFluidGlass(std::string_view mode) {
    return mode == "fluid-glass";
}

bool materialModeUsesFluidShader(std::string_view mode) {
    return materialModeUsesFluidGlass(mode);
}

bool materialModeUsesSdfRefraction(std::string_view mode) {
    return materialModeUsesFluidShader(mode);
}








bool materialModeUsesBackdropCapture(std::string_view mode) {
    return materialModeUsesSdfRefraction(mode);
}







bool materialModeIsSupported(std::string_view mode) {
    return mode == "flat" || materialModeUsesNativeBlur(mode) || materialModeUsesFluidGlass(mode);
}

bool fluidGlassShaderSupportsTransform(int transform) {
    return transform >= 0 && transform <= 7;
}

std::string fluidGlassShaderTransformName(int transform) {
    if (transform == 0)
        return "shared-source-quad-currentDirectRect-transform-0";
    if (transform >= 0 && transform <= 7)
        return "shared-source-quad-diagnostic-transform-" + std::to_string(transform);
    return "shader-source-map-transform-" + std::to_string(transform) + "-unsupported";
}




bool fluidV0SupportsTransform(int transform) {
    return transform == 0;
}

bool fluidGlassSupportsTransform(int transform) {
    return fluidGlassShaderSupportsTransform(transform);
}

bool fluidShaderSupportsTransform(std::string_view mode, int transform) {
    if (materialModeUsesFluidGlass(mode))
        return fluidGlassSupportsTransform(transform);
    return fluidV0SupportsTransform(transform);
}




std::string fluidV0SourceMappingName(int transform) {
    if (transform == 0)
        return "fluid-v0-direct-source-uv-transform-0";
    if (transform >= 0 && transform <= 7)
        return "fluid-v0-transform-" + std::to_string(transform) + "-unvalidated-skipped";
    return "fluid-v0-transform-" + std::to_string(transform) + "-unsupported";
}

std::string fluidV0EvidenceForTransform(int transform) {
    if (transform == 0)
        return "transform 0 direct source UV mapping was live-validated through fluid-glass";
    return "transformed capture-backed sampling is shelved; fluid-v0-debug remains skipped for this transform";
}

std::string fluidV0ValidationStatus(int transform) {
    if (transform == 0)
        return "live-validated";
    if (transform >= 1 && transform <= 7)
        return "unvalidated-skipped";
    return "unsupported";
}

std::string fluidGlassSourceMappingName(int transform) {
    if (transform == 0)
        return "fluid-glass-direct-source-uv-transform-0";
    if (transform >= 0 && transform <= 7)
        return "fluid-glass-shared-source-quad-transform-" + std::to_string(transform);
    return "fluid-glass-transform-" + std::to_string(transform) + "-unsupported";
}

std::string fluidGlassEvidenceForTransform(int transform) {
    if (transform == 0)
        return "fluid-glass uses the validated direct source UV shader path on transform 0";
    if (transform >= 1 && transform <= 7)
        return "fluid-glass uses the selected shared display-to-capture quad when it is valid for this descriptor";
    return "unsupported transform";
}

std::string fluidGlassValidationStatus(int transform) {
    if (transform == 0)
        return "live-validated";
    if (transform >= 1 && transform <= 7)
        return "quad-mapped-if-valid";
    return "unsupported";
}

std::string fluidShaderSourceMappingName(std::string_view mode, int transform) {
    if (materialModeUsesFluidGlass(mode))
        return fluidGlassSourceMappingName(transform);
    return fluidV0SourceMappingName(transform);
}

std::string fluidShaderEvidenceForTransform(std::string_view mode, int transform) {
    if (materialModeUsesFluidGlass(mode))
        return fluidGlassEvidenceForTransform(transform);
    return fluidV0EvidenceForTransform(transform);
}

std::string fluidShaderValidationStatus(std::string_view mode, int transform) {
    if (materialModeUsesFluidGlass(mode))
        return fluidGlassValidationStatus(transform);
    return fluidV0ValidationStatus(transform);
}



double fluidV0DisplacementFor(double frost) {
    return clampDouble(1.5 + frost * 8.0, 1.5, 9.5);
}

double fluidV0EdgeWidthFor(const RectSummary& rect) {
    return clampDouble(std::min(rect.width, rect.height) * 0.30, 6.0, 18.0);
}




RectSummary rectFromXYWH(double x, double y, double width, double height) {
    return {
        .x = x,
        .y = y,
        .width = width,
        .height = height,
    };
}


RectSummary mapDrawRectToCaptureSource(const RectSummary& drawRect) {
    if (!rectValid(drawRect))
        return {};

    // The capture texture is copied in the monitor framebuffer UV basis. Draw
    // boxes are already monitor-local framebuffer rects; Hyprland projects the
    // destination transform separately when rendering the pass.
    return roundRect(drawRect);
}



double glassRimAlpha(double frost) {
    return clampDouble(0.024 + frost * 0.046, 0.024, 0.07);
}

double glassInnerEdgeAlpha(double frost) {
    return clampDouble(0.008 + frost * 0.020, 0.008, 0.028);
}

double glassHighlightAlpha(double frost) {
    return clampDouble(0.014 + frost * 0.032, 0.014, 0.046);
}

double glassTintOverlayAlpha(double frost, double tintOpacity) {
    return clampDouble(0.010 + frost * 0.018 + tintOpacity * 0.07, 0.010, 0.045);
}

CHyprColor withAlpha(const CHyprColor& color, double alpha) {
    return CHyprColor(color.r, color.g, color.b, static_cast<float>(clampDouble(alpha, 0.0, 1.0)));
}

CHyprColor mixColor(const CHyprColor& color, float r, float g, float b, double amount, double alpha) {
    const double t = clampDouble(amount, 0.0, 1.0);
    return CHyprColor(
        static_cast<float>(color.r + (r - color.r) * t),
        static_cast<float>(color.g + (g - color.g) * t),
        static_cast<float>(color.b + (b - color.b) * t),
        static_cast<float>(clampDouble(alpha, 0.0, 1.0))
    );
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

void configureGlassPolish(MaterialDescriptor& material, double strengthMultiplier = 1.0) {
    const double strength = clampDouble(strengthMultiplier, 0.0, 1.0);
    material.tintOverlayAlphaUsed = glassTintOverlayAlpha(material.requestedFrost, material.tintOpacityRequested) * strength;
    material.rimAlphaUsed = glassRimAlpha(material.requestedFrost) * strength;
    material.rimExpansionPx = 0.0;
    material.innerEdgeAlphaUsed = glassInnerEdgeAlpha(material.requestedFrost) * strength;
    material.innerEdgeInsetPx = 2.0;
    material.highlightAlphaUsed = glassHighlightAlpha(material.requestedFrost) * strength;
    material.shadowAlphaUsed = 0.0;

    material.tintOverlayEnabled = material.tintOverlayAlphaUsed > 0.001;
    material.rimEnabled = material.rimAlphaUsed > 0.001;
    material.rimTechnique = material.rimEnabled ? "rounded-border-pass" : "disabled";
    material.rimColorUsed = material.rimEnabled ? colorToHexRGB(mixColor(material.color, 1.0F, 1.0F, 1.0F, 0.52, 1.0)) : "";
    material.innerEdgeEnabled = material.innerEdgeAlphaUsed > 0.001 && material.rectUsed.width > material.innerEdgeInsetPx * 2.0 + 1.0 && material.rectUsed.height > material.innerEdgeInsetPx * 2.0 + 1.0;
    material.innerEdgeTechnique = material.innerEdgeEnabled ? "rounded-border-pass" : "disabled";
    material.innerEdgeColorUsed = material.innerEdgeEnabled ? colorToHexRGB(mixColor(material.color, 1.0F, 1.0F, 1.0F, 0.62, 1.0)) : "";
    material.highlightTransformSafe = material.drawTransformSupported && isTransformSupported(material.drawTransform);
    material.highlightEnabled = material.highlightTransformSafe && material.highlightAlphaUsed > 0.001 && material.rectUsed.width > material.radiusUsed * 2.0 + 8.0;
    material.shadowEnabled = false;

    if (material.tintOverlayEnabled)
        ++material.passCount;
    if (material.rimEnabled)
        ++material.passCount;
    if (material.innerEdgeEnabled)
        ++material.passCount;
    if (material.highlightEnabled)
        ++material.passCount;
}

void configureGlassV1Backend(MaterialDescriptor& material, std::string backend, std::string fallbackReason = {}) {
    material.backendUsed = std::move(backend);
    material.fallbackReason = std::move(fallbackReason);
    material.blurAlphaUsed = material.requestedFrost;
    material.blurEnabled = material.blurAlphaUsed > 0.01;
    material.effectiveBlurSource = "native-hyprland";
    material.effectiveBlurControl = "global-kernel-per-surface-alpha";
    material.perSurfaceBlurSupported = false;
    material.perSurfaceBlurSupport = "alpha-only";
    material.captureEnabled = false;
    material.captureReady = false;
    material.textureReady = false;
    material.sampled = false;
    material.shaderEnabled = false;
    material.shaderCompiled = false;
    material.shaderReady = false;
    material.shaderError.clear();
    material.sdfMaskEnabled = false;
    material.roundedMaskEnabled = false;
    material.refractionDebugEnabled = false;
    material.refractionEnabled = false;
    material.transformCaptureSupported = false;
    material.sourceMapping = "none-glass-v1-fallback";
    material.sourceMappingEvidence = "no capture source is sampled for glass-v1 fallback";
    material.sourceCroppingMode = "none";
    material.uvMappingType = "none";
    material.passCount = 1;
    configureGlassPolish(material, 1.0);
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
        addWarning(analysis.warnings, "fractional monitor scale is active; draw rects use rounded framebuffer coordinates");
    else
        addWarning(analysis.warnings, "fractional scale is not active for this descriptor; fractional scale remains untested");
    if (analysis.relation == "contained-in-layer-surface")
        addWarning(analysis.warnings, "descriptor logical rect is contained inside the matched layer surface; delta reports inset from layer-surface bounds");
    if (candidate.monitorTransform != 0 && candidate.transformSupported)
        addWarning(analysis.warnings, "monitor transform is non-normal; coordinate framebuffer rect is pre-transform and draw rect uses transform-aware mapping");
    else if (candidate.monitorTransform != 0)
        addWarning(analysis.warnings, "monitor transform is non-normal and unsupported for compositor draw mapping");

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
    if (!candidate.transformSupported) {
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

    const auto descriptorDraw = mapLogicalRectToFramebuffer(descriptor.logical, candidate);
    overlay.drawTransform = descriptorDraw.transform;
    overlay.drawTransformSupported = descriptorDraw.supported;
    overlay.drawMapping = descriptorDraw.mapping;
    overlay.drawWarnings = descriptorDraw.warnings;
    if (!descriptorDraw.supported || !rectValid(descriptorDraw.rect)) {
        overlay.reason = descriptorDraw.warnings.empty() ? "transform draw mapping failed" : descriptorDraw.warnings.front();
        return overlay;
    }

    const auto surfaceDraw = mapLogicalRectToFramebuffer(coordinate.computedMonitorLocalLogical, candidate);
    if (!surfaceDraw.supported || !rectValid(surfaceDraw.rect)) {
        overlay.reason = surfaceDraw.warnings.empty() ? "surface transform draw mapping failed" : surfaceDraw.warnings.front();
        for (const auto& warning : surfaceDraw.warnings)
            addWarning(overlay.drawWarnings, warning);
        return overlay;
    }

    overlay.drawable = true;
    overlay.status = "drawable";
    overlay.reason = "coordinate " + coordinate.status;
    overlay.rectUsed = descriptorDraw.rect;
    overlay.rectUsedSpace = descriptorDraw.space;
    overlay.surfaceRectUsed = surfaceDraw.rect;
    overlay.surfaceRectUsedSpace = surfaceDraw.space;
    overlay.globalRectUsed = {
        .x      = descriptor.logical.x + candidate.monitorLogical.x,
        .y      = descriptor.logical.y + candidate.monitorLogical.y,
        .width  = descriptor.logical.width,
        .height = descriptor.logical.height,
    };
    overlay.hasSurfaceRect = true;
    overlay.mismatch = coordinate.status == "mismatched";
    overlay.warnings = coordinate.warnings;
    for (const auto& warning : overlay.drawWarnings)
        addWarning(overlay.warnings, warning);
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
    material.descriptorId = descriptor.id;
    material.mode = materialMode;
    material.status = "skipped";
    material.reason = "material mode off";

    if (materialMode == "off")
        return material;
    if (!materialModeIsSupported(materialMode)) {
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
    if (!candidate.transformSupported) {
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

    const auto drawMapping = mapLogicalRectToFramebuffer(descriptor.logical, candidate);
    material.drawTransform = drawMapping.transform;
    material.drawTransformSupported = drawMapping.supported;
    material.drawMapping = drawMapping.mapping;
    material.drawWarnings = drawMapping.warnings;
    if (!drawMapping.supported || !rectValid(drawMapping.rect)) {
        material.reason = drawMapping.warnings.empty() ? "transform draw mapping failed" : drawMapping.warnings.front();
        return material;
    }

    material.drawable = true;
    material.status = "drawable";
    material.reason = "coordinate " + coordinate.status;
    material.rectUsed = drawMapping.rect;
    material.rectUsedSpace = drawMapping.space;
    material.destinationRect = material.rectUsed;
    material.sourceBackdropRect = mapDrawRectToCaptureSource(material.destinationRect);
    material.monitorScale = candidate.scale;
    material.globalRectUsed = {
        .x      = descriptor.logical.x + candidate.monitorLogical.x,
        .y      = descriptor.logical.y + candidate.monitorLogical.y,
        .width  = descriptor.logical.width,
        .height = descriptor.logical.height,
    };
    material.warnings = coordinate.warnings;
    for (const auto& warning : material.drawWarnings)
        addWarning(material.warnings, warning);

    material.radiusRequested = uniformRadiusFromDescriptor(descriptor.radius);
    const double maxRadius = std::min(material.rectUsed.width, material.rectUsed.height) / 2.0;
    material.radiusUsed = clampDouble(material.radiusRequested * candidate.scale, 0.0, maxRadius);
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
    material.passCount = 1;
    if (materialModeUsesFluidGlass(materialMode)) {
        const auto shaderStatus = fluidGlassShaderStatus();
        material.shaderEnabled = true;
        material.shaderBackend = shaderStatus.backend;
        material.shaderCompiled = shaderStatus.compiled;
        material.shaderReady = shaderStatus.ready;
        material.shaderError = shaderStatus.error;
        material.sdfMaskEnabled = material.rounded;
        material.roundedMaskEnabled = material.rounded;
        material.refractionDebugEnabled = true;
        material.refractionEnabled = true;
        material.distortionUsesLocalPixelSpace = true;
        material.distortionUsesSourceQuadBasis = true;
        material.distortionClampedToSourceQuad = true;
        material.sdfTechnique = "fluid-glass-rounded-sdf-edge-displacement";
        material.sdfDisplacementStrengthPx = fluidV0DisplacementFor(material.requestedFrost);
        material.sdfEdgeWidthPx = fluidV0EdgeWidthFor(material.destinationRect);
        material.transformPolicy = "fluid-glass uses capture-backed shader sampling when the selected shared display-to-capture quad is valid; otherwise it uses glass-v1 fallback";
        material.transformShaderSupported = material.drawTransformSupported && fluidShaderSupportsTransform(materialMode, material.drawTransform);
        material.transformCaptureSupported = material.transformShaderSupported;
        material.sourceMapping = fluidShaderSourceMappingName(materialMode, material.drawTransform);
        material.sourceMappingEvidence = fluidShaderEvidenceForTransform(materialMode, material.drawTransform);
        material.sourceCroppingMode = "custom-shader-corner-uv";
        material.uvMappingType = "custom-shader-corner-uv";
        material.passCount = 1;
        material.backendUsed = "fluid-shader";
        if (!candidate.hasMonitorFramebuffer || !rectValid(candidate.monitorFramebuffer)) {
            configureGlassV1Backend(material, "glass-v1-fallback", "Fluid Glass capture source extent unavailable; using glass-v1 fallback");
            material.reason = "coordinate " + coordinate.status + "; fluid-glass using glass-v1 fallback";
            material.transformPolicy = "fluid-glass fallback: capture source extent is unavailable for selected quad mapping";
            addWarning(material.warnings, "fluid-glass could not compute a capture source extent for this descriptor and drew glass-v1 fallback instead");
            return material;
        }

        const auto previewMapping = shaderSourceUvMappingFor(material, candidate.monitorFramebuffer.width, candidate.monitorFramebuffer.height);
        material.sourceMappingCandidates = previewMapping.candidates;
        material.selectedSourceQuad = previewMapping.selectedQuad;
        material.sourceUvTopLeft = previewMapping.topLeft;
        material.sourceUvTopRight = previewMapping.topRight;
        material.sourceUvBottomRight = previewMapping.bottomRight;
        material.sourceUvBottomLeft = previewMapping.bottomLeft;
        material.sourceUvRect = previewMapping.bounds;
        material.sourceExtent = rectFromXYWH(0.0, 0.0, candidate.monitorFramebuffer.width, candidate.monitorFramebuffer.height);
        material.shaderUsesFourCornerUV = true;
        material.transformCaptureDiagnosticEnabled = material.drawTransform != 0;
        material.transformShaderSupported = material.drawTransformSupported && previewMapping.supported;
        material.transformCaptureSupported = material.transformShaderSupported;
        material.sourceMapping = previewMapping.supported ? previewMapping.mapping : fluidGlassSourceMappingName(material.drawTransform);
        material.sourceMappingEvidence = previewMapping.supported ?
            "fluid-glass using quad-mapped capture source candidate: " + previewMapping.selectedCandidateName :
            "fluid-glass falling back because selected capture quad is invalid: " + (previewMapping.error.empty() ? "unknown" : previewMapping.error);

        if (!previewMapping.supported) {
            const std::string fallbackReason = previewMapping.error.empty() ? "Fluid Glass selected capture quad invalid; using glass-v1 fallback" : "Fluid Glass selected capture quad invalid: " + previewMapping.error;
            configureGlassV1Backend(material, "glass-v1-fallback", fallbackReason);
            material.reason = "coordinate " + coordinate.status + "; fluid-glass using glass-v1 fallback";
            material.transformPolicy = "fluid-glass fallback: selected shared display-to-capture quad was invalid or out of bounds";
            addWarning(material.warnings, fallbackReason);
            return material;
        }
        addWarning(material.warnings, "fluid-glass uses capture-backed Fluid Glass shader sampling with a shared display-to-capture source quad");
        if (!material.transformShaderSupported) {
            configureGlassV1Backend(material, "glass-v1-fallback", "Fluid Glass selected capture quad unavailable; using glass-v1 fallback");
            material.reason = "coordinate " + coordinate.status + "; fluid-glass using glass-v1 fallback";
            material.transformPolicy = "fluid-glass fallback: selected shared display-to-capture quad is unavailable";
            addWarning(material.warnings, "fluid-glass skipped capture-backed sampling because selected quad mapping was unavailable and drew glass-v1 fallback instead");
            return material;
        }
        if (shaderStatus.compileAttempted && !shaderStatus.ready) {
            configureGlassV1Backend(material, "glass-v1-fallback", "Fluid Glass shader unavailable; using glass-v1 fallback");
            material.reason = "coordinate " + coordinate.status + "; fluid-glass using glass-v1 fallback";
            addWarning(material.warnings, "fluid-glass shader was unavailable, so this descriptor uses glass-v1 fallback");
            return material;
        }
    }
    if (materialModeUsesBackdropCapture(materialMode)) {
        material.captureEnabled = true;
        material.captureMonitor = candidate.monitor;
        const auto record = backdropCaptureRecordFor(descriptor.id);
        material.captureStatus = record.status;
        material.captureError = record.error;
        material.captureBackend = record.backend;
        material.captureGeneration = record.captureGeneration;
        material.captureAttempted = record.captureAttempted;
        material.captureReady = captureRecordReady(record);
        material.captureTextureReady = record.captureTextureReady && positiveExtent(record.textureSize);
        material.textureReady = record.textureReady && positiveExtent(record.textureSize);
        material.textureSize = record.textureSize;
        material.sourceExtent = record.sourceExtent;
        material.sourceUvRect = record.sourceUvRect;
        material.sourceUvTopLeft = record.sourceUvTopLeft;
        material.sourceUvTopRight = record.sourceUvTopRight;
        material.sourceUvBottomRight = record.sourceUvBottomRight;
        material.sourceUvBottomLeft = record.sourceUvBottomLeft;
        material.sourceMapping = record.sourceMapping;
        material.sourceCroppingMode = record.sourceCroppingMode;
        material.uvMappingType = record.uvMappingType;
        material.candidateSlices = record.candidateSlices;
        material.selectedSourceQuad = record.selectedSourceQuad;
        material.sourceMappingCandidates = record.sourceMappingCandidates;
        if (!record.selectedSourceMappingCandidate.empty())
            material.sourceMappingEvidence = "fluid-glass selected shared source mapping candidate: " + record.selectedSourceMappingCandidate;
        material.transformCaptureDiagnosticEnabled = record.transformCaptureDiagnosticEnabled;
        material.shaderUsesFourCornerUV = record.shaderUsesFourCornerUV;
        material.sampled = record.sampled && material.captureReady;
        material.descriptorRendered = record.descriptorRendered && material.captureReady;
        material.descriptorUsedCapture = record.descriptorUsedCapture && material.captureReady;
        material.renderedFromStaleCapture = record.renderedFromStaleCapture && material.captureReady;
        material.selfSamplingRisk = !material.captureReady || record.selfSamplingRisk;
        material.blurEnabled = false;
        if (material.captureReady && material.textureReady) {
            material.reason = "coordinate " + coordinate.status + "; backdrop capture ready";
        } else {
            addWarning(material.warnings, "fluid-glass uses glass-v1 fallback until captureReady, textureReady, shaderReady, and selected capture quad validity are true");
            if (!material.captureError.empty())
                addWarning(material.warnings, material.captureError);
            else if (!material.captureAttempted)
                addWarning(material.warnings, "capture has not run for this descriptor yet; waiting for the next damaged frame");
        }
        if (material.captureStatus != "not-attempted" && material.captureStatus != "ok") {
            const std::string fallbackReason = material.captureError.empty() ? "Fluid Glass shader/capture render failed; using glass-v1 fallback" : "Fluid Glass shader/capture render failed: " + material.captureError;
            configureGlassV1Backend(material, "glass-v1-fallback", fallbackReason);
            material.reason = "coordinate " + coordinate.status + "; fluid-glass using glass-v1 fallback";
            material.transformPolicy = "fluid-glass fallback: shader render path reported " + material.captureStatus;
            addWarning(material.warnings, fallbackReason);
            return material;
        }
        if (material.captureStatus != "not-attempted" && (!material.captureReady || !material.textureReady)) {
            const std::string fallbackReason = "Fluid Glass capture unavailable; using glass-v1 fallback";
            configureGlassV1Backend(material, "glass-v1-fallback", fallbackReason);
            material.reason = "coordinate " + coordinate.status + "; fluid-glass using glass-v1 fallback";
            material.transformPolicy = "fluid-glass fallback: shader capture is used only when capture, texture, shader, and selected quad state are ready";
            addWarning(material.warnings, fallbackReason);
            return material;
        }
    }
    if (materialModeUsesNativeBlur(materialMode)) {
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
    if (materialMode == "glass-v1") {
        material.passCount = 1;
        configureGlassPolish(material, 1.0);
        addWarning(material.warnings, "glass-v1 rim uses CBorderPassElement rounded border pass; square outline layers remain disabled");
    }
    if (materialModeUsesFluidGlass(materialMode)) {
        configureGlassPolish(material, 0.82);
        addWarning(material.warnings, "fluid-glass uses the shared-quad Fluid Glass shader path plus conservative glass-v1 rounded polish overlays");
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
        out["rectUsed"] = rectWithSpaceToJSON(overlay.rectUsedSpace, overlay.rectUsed);
        out["globalRectUsed"] = rectWithSpaceToJSON("global-layout-logical", overlay.globalRectUsed);
        out["drawTransform"] = overlay.drawTransform;
        out["drawTransformSupported"] = overlay.drawTransformSupported;
        out["drawMapping"] = overlay.drawMapping;
        out["drawWarnings"] = overlay.drawWarnings;
        out["mismatch"] = overlay.mismatch;
        if (overlay.hasSurfaceRect)
            out["surfaceRectUsed"] = rectWithSpaceToJSON(overlay.surfaceRectUsedSpace, overlay.surfaceRectUsed);
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
        out["rectUsed"] = rectWithSpaceToJSON(material.rectUsedSpace, material.rectUsed);
        out["globalRectUsed"] = rectWithSpaceToJSON("global-layout-logical", material.globalRectUsed);
        out["drawTransform"] = material.drawTransform;
        out["drawTransformSupported"] = material.drawTransformSupported;
        out["drawMapping"] = material.drawMapping;
        out["drawWarnings"] = material.drawWarnings;
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
        out["passCount"] = material.passCount;
        if (!material.backendUsed.empty())
            out["backendUsed"] = material.backendUsed;
        if (!material.fallbackReason.empty())
            out["fallbackReason"] = material.fallbackReason;
        out["transformCaptureSupported"] = material.transformCaptureSupported;
        if (materialModeUsesNativeBlur(material.mode)) {
            out["blurAlphaUsed"] = material.blurAlphaUsed;
            out["effectiveBlurSource"] = material.effectiveBlurSource;
            out["effectiveBlurControl"] = material.effectiveBlurControl;
            out["perSurfaceBlurSupported"] = material.perSurfaceBlurSupported;
            out["perSurfaceBlurSupport"] = material.perSurfaceBlurSupport;
        }
        if (material.mode == "glass-v1" || materialModeUsesFluidShader(material.mode) || material.backendUsed == "glass-v1-fallback") {
            out["tintOverlayEnabled"] = material.tintOverlayEnabled;
            out["tintOverlayAlphaUsed"] = material.tintOverlayAlphaUsed;
            out["rimEnabled"] = material.rimEnabled;
            out["rimTechnique"] = material.rimTechnique;
            out["rimAlphaUsed"] = material.rimAlphaUsed;
            out["rimExpansionPx"] = material.rimExpansionPx;
            out["rimColorUsed"] = material.rimColorUsed;
            out["innerEdgeEnabled"] = material.innerEdgeEnabled;
            out["innerEdgeTechnique"] = material.innerEdgeTechnique;
            out["innerEdgeAlphaUsed"] = material.innerEdgeAlphaUsed;
            out["innerEdgeInsetPx"] = material.innerEdgeInsetPx;
            out["innerEdgeColorUsed"] = material.innerEdgeColorUsed;
            out["highlightEnabled"] = material.highlightEnabled;
            out["highlightAlphaUsed"] = material.highlightEnabled ? material.highlightAlphaUsed : 0.0;
            out["highlightTransformSafe"] = material.highlightTransformSafe;
            out["shadowEnabled"] = material.shadowEnabled;
            out["shadowAlphaUsed"] = material.shadowEnabled ? material.shadowAlphaUsed : 0.0;
        }
        if (materialModeUsesBackdropCapture(material.mode)) {
            out["captureEnabled"] = material.captureEnabled;
            out["captureStage"] = material.captureStage;
            out["captureAttempted"] = material.captureAttempted;
            out["captureGeneration"] = material.captureGeneration;
            out["captureReady"] = material.captureReady;
            out["captureStatus"] = material.captureStatus;
            out["captureMonitor"] = material.captureMonitor;
            out["captureBackend"] = material.captureBackend;
            out["captureTextureReady"] = material.captureTextureReady;
            out["textureReady"] = material.textureReady;
            out["textureSize"] = rectWithSpaceToJSON("pixels", material.textureSize);
            out["sourceBackdropRect"] = rectWithSpaceToJSON(material.rectUsedSpace, material.sourceBackdropRect);
            out["destinationRect"] = rectWithSpaceToJSON(material.rectUsedSpace, material.destinationRect);
            out["sourceExtent"] = rectWithSpaceToJSON("monitor-framebuffer-uv-basis", material.sourceExtent);
            out["sourceUvRect"] = rectWithSpaceToJSON("normalized-uv", material.sourceUvRect);
            out["sourceUvTopLeft"] = pointWithSpaceToJSON("normalized-uv", material.sourceUvTopLeft.x, material.sourceUvTopLeft.y);
            out["sourceUvTopRight"] = pointWithSpaceToJSON("normalized-uv", material.sourceUvTopRight.x, material.sourceUvTopRight.y);
            out["sourceUvBottomRight"] = pointWithSpaceToJSON("normalized-uv", material.sourceUvBottomRight.x, material.sourceUvBottomRight.y);
            out["sourceUvBottomLeft"] = pointWithSpaceToJSON("normalized-uv", material.sourceUvBottomLeft.x, material.sourceUvBottomLeft.y);
            out["sourceMapping"] = material.sourceMapping;
            out["sourceCroppingMode"] = material.sourceCroppingMode;
            out["uvMappingType"] = material.uvMappingType;
            out["transformCaptureSupportedForProduction"] = material.transformCaptureSupported;
            out["transformCaptureDiagnosticEnabled"] = material.transformCaptureDiagnosticEnabled;
            out["shaderUsesFourCornerUV"] = material.shaderUsesFourCornerUV;
            if (!material.selectedSourceQuad.name.empty()) {
                out["selectedSourceMappingCandidate"] = material.selectedSourceQuad.name;
                out["selectedSourceQuad"] = captureQuadCandidateToJSON(material.selectedSourceQuad);
                out["selectedUvQuad"] = {
                    {"topLeft", pointWithSpaceToJSON("normalized-uv", material.selectedSourceQuad.uvTopLeft.x, material.selectedSourceQuad.uvTopLeft.y)},
                    {"topRight", pointWithSpaceToJSON("normalized-uv", material.selectedSourceQuad.uvTopRight.x, material.selectedSourceQuad.uvTopRight.y)},
                    {"bottomRight", pointWithSpaceToJSON("normalized-uv", material.selectedSourceQuad.uvBottomRight.x, material.selectedSourceQuad.uvBottomRight.y)},
                    {"bottomLeft", pointWithSpaceToJSON("normalized-uv", material.selectedSourceQuad.uvBottomLeft.x, material.selectedSourceQuad.uvBottomLeft.y)},
                };
                out["selectedBounds"] = rectWithSpaceToJSON(material.selectedSourceQuad.space, material.selectedSourceQuad.bounds);
                out["selectedInBounds"] = material.selectedSourceQuad.inBounds;
            }
            if (!material.sourceMappingCandidates.empty()) {
                json candidateQuads = json::array();
                for (const auto& candidate : material.sourceMappingCandidates)
                    candidateQuads.push_back(captureQuadCandidateToJSON(candidate));
                out["candidateQuads"] = candidateQuads;
            }
            out["transformSourceMapping"] = material.sourceMapping;
            if (!material.sourceMappingEvidence.empty())
                out["sourceMappingEvidence"] = material.sourceMappingEvidence;
            out["monitorScale"] = material.monitorScale;
            out["sampled"] = material.sampled;
            out["descriptorSampled"] = material.sampled;
            out["descriptorRendered"] = material.descriptorRendered;
            out["descriptorUsedCapture"] = material.descriptorUsedCapture;
            out["renderedFromStaleCapture"] = material.renderedFromStaleCapture;
            if (!material.captureAttempted)
                out["captureWaitReason"] = "waiting-for-capture-frame";
            else if (material.captureAttempted && !material.captureReady) {
                if (!material.captureTextureReady || !material.textureReady)
                    out["captureWaitReason"] = material.captureError.empty() ? "capture-texture-not-ready" : material.captureError;
                else if (!positiveExtent(material.sourceExtent))
                    out["captureWaitReason"] = material.captureError.empty() ? "capture-source-extent-invalid" : material.captureError;
                else
                    out["captureWaitReason"] = material.captureError.empty() ? "capture-attempted-but-not-ready" : material.captureError;
            }
            if (materialModeUsesFluidGlass(material.mode)) {
                out["shaderEnabled"] = material.shaderEnabled;
                out["shaderBackend"] = material.shaderBackend;
                out["shaderCompiled"] = material.shaderCompiled;
                out["shaderReady"] = material.shaderReady;
                out["shaderError"] = material.shaderError;
                out["sdfMaskEnabled"] = material.sdfMaskEnabled;
                out["sdfTechnique"] = material.sdfTechnique;
                out["refractionDebugEnabled"] = material.refractionDebugEnabled;
                out["displacementStrengthPx"] = material.sdfDisplacementStrengthPx;
                out["edgeWidthPx"] = material.sdfEdgeWidthPx;
                out["distortionUsesLocalPixelSpace"] = material.distortionUsesLocalPixelSpace;
                out["distortionUsesSourceQuadBasis"] = material.distortionUsesSourceQuadBasis;
                out["distortionClampedToSourceQuad"] = material.distortionClampedToSourceQuad;
                out["transformShaderSupported"] = material.transformShaderSupported;
                out["shaderTransformSupported"] = material.transformShaderSupported;
                out["shaderTransformValidation"] = fluidShaderValidationStatus(material.mode, material.drawTransform);
                out["implementedShaderTransforms"] = json::array({0});
                out["supportedShaderTransforms"] = material.transformShaderSupported ? json::array({material.drawTransform}) : json::array();
                out["failedShaderTransforms"] = json::array();
                out["targetShaderTransforms"] = json::array({material.drawTransform});
                out["shaderCaptureTransformPolicy"] = "per-descriptor selected shared source quad";
                out["monitorTransform"] = material.drawTransform;
                out["transformSourceMapping"] = material.sourceMapping;
                out["sourceMappingEvidence"] = material.sourceMappingEvidence;
                out["transformPolicy"] = material.transformPolicy;
            }
            out["selfSamplingRisk"] = material.selfSamplingRisk;
            out["blurEnabled"] = false;
            out["refractionEnabled"] = material.refractionEnabled;
            if (!material.captureError.empty())
                out["lastCaptureError"] = material.captureError;
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
    std::string lastBackdropCaptureStatus;
    std::string lastBackdropCaptureError;
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
        .lastBackdropCaptureStatus = g_lastBackdropCaptureStatus,
        .lastBackdropCaptureError = g_lastBackdropCaptureError,
        .materialMode = g_materialMode,
        .generation = g_generation,
        .applyCount = g_applyCount,
        .debugOverlayEnabled = g_debugOverlayEnabled,
    };
}

bool materialModeEnabled(std::string_view mode) {
    return materialModeIsSupported(mode);
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

int countSupportedTransformMonitors(const std::vector<MonitorSummary>& monitors) {
    return static_cast<int>(std::ranges::count_if(monitors, [](const auto& monitor) { return monitor.transform != 0 && monitor.transformSupported; }));
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
        addWarning(warnings, "fractional scale monitor present; inspect rounded framebuffer coordinates and visual alignment");
    else
        addWarning(warnings, "no fractional scale monitors detected; fractional scale remains untested");
    if (countSupportedTransformMonitors(monitors) > 0)
        addWarning(warnings, "one or more transformed monitors use transform-aware framebuffer draw mapping");
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
    if (materialMode == "glass-v1") {
        addWarning(warnings, "glass-v1 material uses RENDER_POST_WINDOWS, Hyprland native blur, and simple rect pass overlays");
        addWarning(warnings, "glass-v1 frost maps to CRectPassElement blurA alpha; effective blur kernel strength still comes from global Hyprland decoration:blur settings");
        addWarning(warnings, "glass-v1 highlight uses the same renderer-projected rounded rect path on transforms 0-7");
    }
    if (materialMode == "fluid-glass") {
        addWarning(warnings, "fluid-glass uses capture-backed Fluid Glass shader sampling with selected shared display-to-capture source quads");
        addWarning(warnings, "fluid-glass uses glass-v1 fallback only when capture, shader, texture, or selected source quad state is unavailable");
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

void drawRoundedBorder(const RectSummary& rect, const CHyprColor& color, int round, int borderSize) {
    if (!g_pHyprRenderer || !rectValid(rect))
        return;

    CBorderPassElement::SBorderData data;
    data.box = CBox(rect.x, rect.y, rect.width, rect.height);
    data.grad1 = Config::CGradientValueData(color);
    data.hasGrad2 = false;
    data.a = 1.0F;
    data.round = std::max(0, round);
    data.outerRound = -1;
    data.borderSize = std::max(1, borderSize);
    data.roundingPower = 2.0F;
    g_pHyprRenderer->m_renderPass.add(makeUnique<CBorderPassElement>(data));
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

RectSummary insetRect(const RectSummary& rect, double inset) {
    if (!rectValid(rect))
        return {};

    const double safeInset = std::max(0.0, std::min({inset, (rect.width - 1.0) / 2.0, (rect.height - 1.0) / 2.0}));
    return {
        .x = rect.x + safeInset,
        .y = rect.y + safeInset,
        .width = rect.width - safeInset * 2.0,
        .height = rect.height - safeInset * 2.0,
    };
}

void drawGlassPolishOverlays(const MaterialDescriptor& material) {
    const int round = static_cast<int>(std::round(material.radiusUsed));
    if (material.tintOverlayEnabled)
        drawMaterialRect(material.rectUsed, withAlpha(material.color, material.tintOverlayAlphaUsed), round, false, 0.0);

    if (material.rimEnabled)
        drawRoundedBorder(material.rectUsed, mixColor(material.color, 1.0F, 1.0F, 1.0F, 0.52, material.rimAlphaUsed), round, 1);

    if (material.innerEdgeEnabled) {
        const RectSummary inner = insetRect(material.rectUsed, material.innerEdgeInsetPx);
        const int innerRound = static_cast<int>(std::round(clampDouble(material.radiusUsed - material.innerEdgeInsetPx, 0.0, std::min(inner.width, inner.height) / 2.0)));
        if (rectValid(inner))
            drawRoundedBorder(inner, mixColor(material.color, 1.0F, 1.0F, 1.0F, 0.62, material.innerEdgeAlphaUsed), innerRound, 1);
    }

    if (material.highlightEnabled) {
        RectSummary highlight = insetRect(material.rectUsed, 2.0);
        if (rectValid(highlight)) {
            const double horizontalInset = std::min(std::max(material.radiusUsed + 2.0, 2.0), (highlight.width - 1.0) / 2.0);
            highlight.x += horizontalInset;
            highlight.width -= horizontalInset * 2.0;
            highlight.height = std::min({highlight.height, std::max(2.0, material.rectUsed.height * 0.12), 7.0});
            const int highlightRound = static_cast<int>(std::round(clampDouble(material.radiusUsed / 3.0, 0.0, std::min(highlight.width, highlight.height) / 2.0)));
            if (rectValid(highlight))
                drawMaterialRect(highlight, CHyprColor(1.0F, 1.0F, 1.0F, static_cast<float>(material.highlightAlphaUsed)), highlightRound, false, 0.0);
        }
    }
}

void drawGlassV1Material(const MaterialDescriptor& material) {
    if (!material.drawable)
        return;

    const int round = static_cast<int>(std::round(material.radiusUsed));
    drawMaterialRect(material.rectUsed, material.color, round, material.blurEnabled, material.blurAlphaUsed);
    drawGlassPolishOverlays(material);
}

SP<Render::IFramebuffer> backdropCaptureFramebufferFor(const PHLMONITOR& monitor, const SP<Render::IFramebuffer>& sourceFB) {
    if (!g_pHyprRenderer || !monitor || !sourceFB)
        return nullptr;

    const std::string key = monitor->m_name;
    auto& framebuffer = g_backdropCaptureFramebuffers[key];
    if (!framebuffer)
        framebuffer = g_pHyprRenderer->createFB("hgs backdrop debug " + key);
    if (!framebuffer)
        return nullptr;

    const auto sourceTexture = sourceFB->getTexture();
    if (!sourceTexture || !sourceTexture->ok())
        return nullptr;

    const int width = static_cast<int>(std::round(sourceTexture->m_size.x));
    const int height = static_cast<int>(std::round(sourceTexture->m_size.y));
    if (width <= 0 || height <= 0)
        return nullptr;

    auto format = sourceFB->m_drmFormat;
    if (format == DRM_FORMAT_INVALID)
        format = DRM_FORMAT_ABGR8888;

    framebuffer->alloc(width, height, format);
    if (const auto imageDescription = sourceFB->imageDescription())
        framebuffer->setImageDescription(imageDescription);
    else
        framebuffer->setImageDescription(monitor->workBufferImageDescription());

    return framebuffer;
}

RectSummary textureSizeSummary(const SP<Render::ITexture>& texture) {
    if (!texture)
        return {};
    return {
        .x = 0.0,
        .y = 0.0,
        .width = texture->m_size.x,
        .height = texture->m_size.y,
    };
}














const char* fluidGlassVertexSource() {
    return R"HGS_SHADER(#version 320 es

uniform mat3 proj;

in vec2 pos;
in vec2 texcoord;

out vec2 v_texcoord;

void main() {
    gl_Position = vec4(proj * vec3(pos, 1.0), 1.0);
    v_texcoord = texcoord;
}
)HGS_SHADER";
}

const char* fluidGlassFragmentSource() {
    return R"HGS_SHADER(#version 320 es

precision highp float;

in vec2 v_texcoord;

uniform sampler2D tex;
uniform vec2 uSourceTopLeft;
uniform vec2 uSourceTopRight;
uniform vec2 uSourceBottomRight;
uniform vec2 uSourceBottomLeft;
uniform vec2 uDestinationSize;
uniform float uDisplacementPx;
uniform float uRadiusPx;
uniform float uEdgeWidthPx;
uniform float uAlpha;
uniform vec4 uTint;

layout(location = 0) out vec4 fragColor;

float roundedRectSdf(vec2 point, vec2 size, float radius) {
    vec2 halfSize = size * 0.5;
    float safeRadius = clamp(radius, 0.0, min(halfSize.x, halfSize.y));
    vec2 q = abs(point - halfSize) - (halfSize - vec2(safeRadius));
    return length(max(q, vec2(0.0))) + min(max(q.x, q.y), 0.0) - safeRadius;
}

vec2 sourceUvForLocal(vec2 local) {
    vec2 clampedLocal = clamp(local, vec2(0.0), vec2(1.0));
    vec2 topUv = mix(uSourceTopLeft, uSourceTopRight, clampedLocal.x);
    vec2 bottomUv = mix(uSourceBottomLeft, uSourceBottomRight, clampedLocal.x);
    return mix(topUv, bottomUv, clampedLocal.y);
}

void main() {
    vec2 localUv = clamp(v_texcoord, vec2(0.0), vec2(1.0));
    vec2 localPx = localUv * uDestinationSize;

    float sdf = roundedRectSdf(localPx, uDestinationSize, uRadiusPx);
    float mask = 1.0 - smoothstep(0.0, 1.25, sdf);
    if (mask <= 0.001)
        discard;

    float insideDistance = max(-sdf, 0.0);
    float edgeFactor = 1.0 - smoothstep(0.0, max(uEdgeWidthPx, 0.001), insideDistance);
    vec2 fromCenter = localPx - uDestinationSize * 0.5;
    vec2 direction = length(fromCenter) > 0.001 ? normalize(fromCenter) : vec2(0.0);
    vec2 localOffsetUv = (direction * uDisplacementPx * edgeFactor) / max(uDestinationSize, vec2(1.0));
    vec2 displacedLocalUv = clamp(localUv + localOffsetUv, vec2(0.0), vec2(1.0));
    vec2 displacedUv = sourceUvForLocal(displacedLocalUv);

    vec4 sampleColor = texture(tex, displacedUv);
    vec3 tinted = mix(sampleColor.rgb, uTint.rgb, clamp(uTint.a, 0.0, 1.0));
    fragColor = vec4(tinted, sampleColor.a * clamp(uAlpha, 0.0, 1.0) * mask);
}
)HGS_SHADER";
}

bool ensureFluidGlassShader() {
    if (g_fluidGlassShader && g_fluidGlassShaderCompiled)
        return true;
    if (g_fluidGlassShaderCompileAttempted && !g_fluidGlassShaderCompiled)
        return false;
    if (!Render::GL::g_pHyprOpenGL) {
        updateFluidGlassShaderStatus(false, "OpenGL renderer unavailable for fluid-glass");
        return false;
    }

    auto shader = makeShared<CShader>();
    if (!shader || !shader->createProgram(fluidGlassVertexSource(), fluidGlassFragmentSource(), true, true)) {
        g_fluidGlassShader.reset();
        updateFluidGlassShaderStatus(false, "fluid-glass shader compile/link failed");
        return false;
    }

    shader->setUsesCustomUV(true);
    g_fluidGlassShader = shader;
    updateFluidGlassShaderStatus(true, "");
    return true;
}


RectSummary uvPointForCapture(double x, double y, double sourceWidth, double sourceHeight) {
    if (sourceWidth <= 0.0 || sourceHeight <= 0.0)
        return {};
    return rectFromXYWH(clampDouble(x / sourceWidth, 0.0, 1.0), clampDouble(y / sourceHeight, 0.0, 1.0), 0.0, 0.0);
}

RectSummary uvBoundsForCorners(const RectSummary& topLeft, const RectSummary& topRight, const RectSummary& bottomRight, const RectSummary& bottomLeft) {
    const double minX = std::min({topLeft.x, topRight.x, bottomRight.x, bottomLeft.x});
    const double minY = std::min({topLeft.y, topRight.y, bottomRight.y, bottomLeft.y});
    const double maxX = std::max({topLeft.x, topRight.x, bottomRight.x, bottomLeft.x});
    const double maxY = std::max({topLeft.y, topRight.y, bottomRight.y, bottomLeft.y});
    return rectFromXYWH(minX, minY, std::max(0.0, maxX - minX), std::max(0.0, maxY - minY));
}

CaptureQuadCandidate selectedCaptureQuadCandidateForShaderSourceMap(
    const RectSummary& displayRect,
    const RectSummary& currentSourceRect,
    int transform,
    const RectSummary& sourceExtent,
    std::vector<CaptureQuadCandidate>& candidates,
    std::string& selectedName
);

ShaderSourceUvMapping shaderSourceUvMappingFor(const MaterialDescriptor& material, double sourceWidth, double sourceHeight) {
    ShaderSourceUvMapping mapping;
    mapping.mapping = fluidGlassShaderTransformName(material.drawTransform);
    mapping.diagnosticOnly = material.drawTransform != 0;

    if (!fluidGlassShaderSupportsTransform(material.drawTransform)) {
        mapping.error = "shader-source-map-debug transform " + std::to_string(material.drawTransform) + " is not validated";
        return mapping;
    }
    if (sourceWidth <= 0.0 || sourceHeight <= 0.0 || !rectValid(material.sourceBackdropRect) || !rectValid(material.destinationRect)) {
        mapping.error = "shader-source-map-debug source extent or source rect is invalid";
        return mapping;
    }

    const auto sourceExtent = rectFromXYWH(0.0, 0.0, sourceWidth, sourceHeight);
    mapping.selectedQuad = selectedCaptureQuadCandidateForShaderSourceMap(
        material.destinationRect,
        material.sourceBackdropRect,
        material.drawTransform,
        sourceExtent,
        mapping.candidates,
        mapping.selectedCandidateName
    );
    if (mapping.selectedCandidateName == "none" || mapping.selectedQuad.name.empty()) {
        mapping.error = "shader-source-map-debug source mapping candidate was not found";
        return mapping;
    }
    if (!mapping.selectedQuad.inBounds) {
        mapping.error = "shader-source-map-debug selected source mapping candidate is out of bounds: " + mapping.selectedCandidateName;
        return mapping;
    }

    mapping.topLeft = mapping.selectedQuad.uvTopLeft;
    mapping.topRight = mapping.selectedQuad.uvTopRight;
    mapping.bottomRight = mapping.selectedQuad.uvBottomRight;
    mapping.bottomLeft = mapping.selectedQuad.uvBottomLeft;

    mapping.bounds = uvBoundsForCorners(mapping.topLeft, mapping.topRight, mapping.bottomRight, mapping.bottomLeft);
    auto uvInBounds = [](const RectSummary& point) {
        constexpr double epsilon = 0.0001;
        return std::isfinite(point.x) && std::isfinite(point.y) &&
            point.x >= -epsilon && point.y >= -epsilon &&
            point.x <= 1.0 + epsilon && point.y <= 1.0 + epsilon;
    };
    mapping.inBounds = uvInBounds(mapping.topLeft) &&
        uvInBounds(mapping.topRight) &&
        uvInBounds(mapping.bottomRight) &&
        uvInBounds(mapping.bottomLeft);
    if (!mapping.inBounds) {
        mapping.error = "shader-source-map-debug UV mapping is out of bounds for candidate " + mapping.selectedCandidateName;
        return mapping;
    }

    mapping.mapping = "shared-source-quad:" + mapping.selectedCandidateName;
    mapping.supported = true;
    return mapping;
}









RectSummary transformedSourceRectCandidate(const RectSummary& rect, int transform, const RectSummary& extent) {
    if (!rectValid(rect) || !rectValid(extent) || transform < 0 || transform > 7)
        return {};

    CBox box(rect.x - extent.x, rect.y - extent.y, rect.width, rect.height);
    box.transform(static_cast<eTransform>(transform), extent.width, extent.height);
    auto out = rectFromBox(box);
    out.x += extent.x;
    out.y += extent.y;
    return roundRect(out);
}

RectSummary inverseTransformedSourceRectCandidate(const RectSummary& rect, int transform, const RectSummary& extent) {
    if (!rectValid(rect) || !rectValid(extent) || transform < 0 || transform > 7)
        return {};

    const auto inverse = Math::wlTransformToHyprutils(Math::invertTransform(static_cast<wl_output_transform>(transform)));
    CBox box(rect.x - extent.x, rect.y - extent.y, rect.width, rect.height);
    box.transform(inverse, extent.width, extent.height);
    auto out = rectFromBox(box);
    out.x += extent.x;
    out.y += extent.y;
    return roundRect(out);
}


RectSummary pointSummary(double x, double y) {
    return rectFromXYWH(x, y, 0.0, 0.0);
}

RectSummary boundsForQuad(const RectSummary& topLeft, const RectSummary& topRight, const RectSummary& bottomRight, const RectSummary& bottomLeft) {
    const double minX = std::min({topLeft.x, topRight.x, bottomRight.x, bottomLeft.x});
    const double minY = std::min({topLeft.y, topRight.y, bottomRight.y, bottomLeft.y});
    const double maxX = std::max({topLeft.x, topRight.x, bottomRight.x, bottomLeft.x});
    const double maxY = std::max({topLeft.y, topRight.y, bottomRight.y, bottomLeft.y});
    return rectFromXYWH(minX, minY, std::max(0.0, maxX - minX), std::max(0.0, maxY - minY));
}

bool pointInExtent(const RectSummary& point, const RectSummary& extent) {
    if (!rectValid(extent) || !std::isfinite(point.x) || !std::isfinite(point.y))
        return false;
    constexpr double epsilon = 0.5;
    return point.x >= extent.x - epsilon && point.y >= extent.y - epsilon &&
        point.x <= extent.x + extent.width + epsilon && point.y <= extent.y + extent.height + epsilon;
}

bool isAxisAlignedQuad(const RectSummary& topLeft, const RectSummary& topRight, const RectSummary& bottomRight, const RectSummary& bottomLeft) {
    constexpr double epsilon = 0.5;
    return std::abs(topLeft.y - topRight.y) <= epsilon &&
        std::abs(bottomLeft.y - bottomRight.y) <= epsilon &&
        std::abs(topLeft.x - bottomLeft.x) <= epsilon &&
        std::abs(topRight.x - bottomRight.x) <= epsilon;
}

CaptureQuadCandidate captureQuadFromPoints(
    std::string name,
    std::string formula,
    std::string notes,
    const RectSummary& topLeft,
    const RectSummary& topRight,
    const RectSummary& bottomRight,
    const RectSummary& bottomLeft,
    const RectSummary& sourceExtent,
    int transform = 0,
    std::string confidence = "diagnostic"
) {
    CaptureQuadCandidate quad;
    quad.name = std::move(name);
    quad.formula = std::move(formula);
    quad.notes = std::move(notes);
    quad.topLeft = topLeft;
    quad.topRight = topRight;
    quad.bottomRight = bottomRight;
    quad.bottomLeft = bottomLeft;
    quad.bounds = boundsForQuad(topLeft, topRight, bottomRight, bottomLeft);
    quad.uvTopLeft = uvPointForCapture(topLeft.x - sourceExtent.x, topLeft.y - sourceExtent.y, sourceExtent.width, sourceExtent.height);
    quad.uvTopRight = uvPointForCapture(topRight.x - sourceExtent.x, topRight.y - sourceExtent.y, sourceExtent.width, sourceExtent.height);
    quad.uvBottomRight = uvPointForCapture(bottomRight.x - sourceExtent.x, bottomRight.y - sourceExtent.y, sourceExtent.width, sourceExtent.height);
    quad.uvBottomLeft = uvPointForCapture(bottomLeft.x - sourceExtent.x, bottomLeft.y - sourceExtent.y, sourceExtent.width, sourceExtent.height);
    quad.transform = transform;
    quad.axisAligned = isAxisAlignedQuad(topLeft, topRight, bottomRight, bottomLeft);
    quad.inBounds = pointInExtent(topLeft, sourceExtent) && pointInExtent(topRight, sourceExtent) &&
        pointInExtent(bottomRight, sourceExtent) && pointInExtent(bottomLeft, sourceExtent);
    quad.confidence = std::move(confidence);
    return quad;
}

CaptureQuadCandidate captureQuadFromRect(
    std::string name,
    std::string formula,
    std::string notes,
    const RectSummary& rect,
    const RectSummary& sourceExtent,
    int transform = 0,
    std::string confidence = "diagnostic"
) {
    return captureQuadFromPoints(
        std::move(name),
        std::move(formula),
        std::move(notes),
        pointSummary(rect.x, rect.y),
        pointSummary(rect.x + rect.width, rect.y),
        pointSummary(rect.x + rect.width, rect.y + rect.height),
        pointSummary(rect.x, rect.y + rect.height),
        sourceExtent,
        transform,
        std::move(confidence)
    );
}

RectSummary transformCapturePoint(const RectSummary& point, int transform, const RectSummary& sourceExtent) {
    if (!rectValid(sourceExtent))
        return {};
    const auto transformed = Vector2D(point.x - sourceExtent.x, point.y - sourceExtent.y)
        .transform(static_cast<eTransform>(transform), Vector2D(sourceExtent.width, sourceExtent.height));
    return pointSummary(transformed.x + sourceExtent.x, transformed.y + sourceExtent.y);
}

bool transformSwapsSourceAxes(int transform) {
    return transform == 1 || transform == 3 || transform == 5 || transform == 7;
}

RectSummary displayExtentForPhysicalCaptureExtent(const RectSummary& sourceExtent, int transform) {
    if (!rectValid(sourceExtent))
        return {};

    if (transformSwapsSourceAxes(transform))
        return rectFromXYWH(sourceExtent.x, sourceExtent.y, sourceExtent.height, sourceExtent.width);

    return sourceExtent;
}

RectSummary preblurInverseTransformCapturePoint(const RectSummary& point, int transform, const RectSummary& sourceExtent) {
    const auto displayExtent = displayExtentForPhysicalCaptureExtent(sourceExtent, transform);
    if (!rectValid(displayExtent))
        return {};

    const double x = point.x - displayExtent.x;
    const double y = point.y - displayExtent.y;
    const double w = displayExtent.width;
    const double h = displayExtent.height;
    const auto   inverse = Math::wlTransformToHyprutils(Math::invertTransform(static_cast<wl_output_transform>(transform)));

    double outX = x;
    double outY = y;
    switch (static_cast<int>(inverse)) {
        case 0:
            outX = x;
            outY = y;
            break;
        case 1:
            outX = h - y;
            outY = x;
            break;
        case 2:
            outX = w - x;
            outY = h - y;
            break;
        case 3:
            outX = y;
            outY = w - x;
            break;
        case 4:
            outX = w - x;
            outY = y;
            break;
        case 5:
            outX = h - y;
            outY = w - x;
            break;
        case 6:
            outX = x;
            outY = h - y;
            break;
        case 7:
            outX = y;
            outY = x;
            break;
        default:
            return {};
    }

    return pointSummary(outX + sourceExtent.x, outY + sourceExtent.y);
}

RectSummary manualTransformCapturePoint(const RectSummary& point, int transform, const RectSummary& sourceExtent) {
    if (!rectValid(sourceExtent))
        return {};

    const double x = point.x - sourceExtent.x;
    const double y = point.y - sourceExtent.y;
    const double w = sourceExtent.width;
    const double h = sourceExtent.height;

    double outX = x;
    double outY = y;
    switch (transform) {
        case 0:
            outX = x;
            outY = y;
            break;
        case 1:
            outX = h - y;
            outY = x;
            break;
        case 2:
            outX = w - x;
            outY = h - y;
            break;
        case 3:
            outX = y;
            outY = w - x;
            break;
        case 4:
            outX = w - x;
            outY = y;
            break;
        case 5:
            outX = h - y;
            outY = w - x;
            break;
        case 6:
            outX = x;
            outY = h - y;
            break;
        case 7:
            outX = y;
            outY = x;
            break;
        default:
            return {};
    }

    return pointSummary(outX + sourceExtent.x, outY + sourceExtent.y);
}

CaptureQuadCandidate pointTransformQuadForRect(
    std::string name,
    std::string formula,
    std::string notes,
    const RectSummary& displayRect,
    int transform,
    const RectSummary& sourceExtent
) {
    const auto topLeft = pointSummary(displayRect.x, displayRect.y);
    const auto topRight = pointSummary(displayRect.x + displayRect.width, displayRect.y);
    const auto bottomRight = pointSummary(displayRect.x + displayRect.width, displayRect.y + displayRect.height);
    const auto bottomLeft = pointSummary(displayRect.x, displayRect.y + displayRect.height);
    return captureQuadFromPoints(
        std::move(name),
        std::move(formula),
        std::move(notes),
        transformCapturePoint(topLeft, transform, sourceExtent),
        transformCapturePoint(topRight, transform, sourceExtent),
        transformCapturePoint(bottomRight, transform, sourceExtent),
        transformCapturePoint(bottomLeft, transform, sourceExtent),
        sourceExtent,
        transform,
        "hyprutils-point-transform"
    );
}

CaptureQuadCandidate preblurInverseTransformQuadForRect(
    const RectSummary& displayRect,
    int transform,
    const RectSummary& sourceExtent
) {
    const auto topLeft = pointSummary(displayRect.x, displayRect.y);
    const auto topRight = pointSummary(displayRect.x + displayRect.width, displayRect.y);
    const auto bottomRight = pointSummary(displayRect.x + displayRect.width, displayRect.y + displayRect.height);
    const auto bottomLeft = pointSummary(displayRect.x, displayRect.y + displayRect.height);
    const auto displayExtent = displayExtentForPhysicalCaptureExtent(sourceExtent, transform);
    return captureQuadFromPoints(
        "preblurInverseTransformCandidate",
        "explicit point table for invertTransform(monitorTransform) over monitor transformed display extent",
        "Hyprland preblur-style candidate: descriptor points start in transformed display space and map through the inverse monitor transform into the physical currentFB/capture texture basis; transform 3 maps physicalX = captureW - displayY, physicalY = displayX",
        preblurInverseTransformCapturePoint(topLeft, transform, sourceExtent),
        preblurInverseTransformCapturePoint(topRight, transform, sourceExtent),
        preblurInverseTransformCapturePoint(bottomRight, transform, sourceExtent),
        preblurInverseTransformCapturePoint(bottomLeft, transform, sourceExtent),
        sourceExtent,
        transform,
        transform == 0 ? "validated-transform-0-baseline" :
                         "hyprland-preblur-inspired-inverse-transform-display-extent-" +
                             std::to_string(static_cast<int>(displayExtent.width)) + "x" +
                             std::to_string(static_cast<int>(displayExtent.height))
    );
}

CaptureQuadCandidate manualTransformQuadForRect(
    std::string name,
    std::string formula,
    std::string notes,
    const RectSummary& displayRect,
    int transform,
    const RectSummary& sourceExtent
) {
    const auto topLeft = pointSummary(displayRect.x, displayRect.y);
    const auto topRight = pointSummary(displayRect.x + displayRect.width, displayRect.y);
    const auto bottomRight = pointSummary(displayRect.x + displayRect.width, displayRect.y + displayRect.height);
    const auto bottomLeft = pointSummary(displayRect.x, displayRect.y + displayRect.height);
    return captureQuadFromPoints(
        std::move(name),
        std::move(formula),
        std::move(notes),
        manualTransformCapturePoint(topLeft, transform, sourceExtent),
        manualTransformCapturePoint(topRight, transform, sourceExtent),
        manualTransformCapturePoint(bottomRight, transform, sourceExtent),
        manualTransformCapturePoint(bottomLeft, transform, sourceExtent),
        sourceExtent,
        transform,
        "manual-point-transform"
    );
}

std::vector<CaptureQuadCandidate> displayToCaptureQuadCandidatesForRect(
    const RectSummary& displayRect,
    const RectSummary& currentSourceRect,
    int transform,
    const RectSummary& sourceExtent
) {
    std::vector<CaptureQuadCandidate> quads;
    if (!rectValid(displayRect) || !rectValid(currentSourceRect) || !rectValid(sourceExtent) || transform < 0 || transform > 7)
        return quads;

    quads.push_back(captureQuadFromRect(
        "currentDirectRect",
        "sourceBackdropRect = round(destinationRect)",
        "current source crop used by capture-backed modes",
        currentSourceRect,
        sourceExtent,
        transform,
        transform == 0 ? "validated-transform-0-baseline" : "diagnostic-direct-rect"
    ));

    quads.push_back(preblurInverseTransformQuadForRect(
        displayRect,
        transform,
        sourceExtent
    ));

    quads.push_back(pointTransformQuadForRect(
        "hyprutilsPointTransform",
        "Vector2D::transform(monitorTransform, sourceExtent)",
        "point-based display-to-capture candidate; preserves corner orientation",
        displayRect,
        transform,
        sourceExtent
    ));

    const auto inverse = Math::wlTransformToHyprutils(Math::invertTransform(static_cast<wl_output_transform>(transform)));
    quads.push_back(pointTransformQuadForRect(
        "hyprutilsPointInverseTransform",
        "Vector2D::transform(invertTransform(monitorTransform), sourceExtent)",
        "inverse point-transform candidate for comparison",
        displayRect,
        static_cast<int>(inverse),
        sourceExtent
    ));

    quads.push_back(manualTransformQuadForRect(
        "manualTransformCandidate",
        "manual display point transform table using monitorTransform",
        "manual point-transform candidate; preserves corner orientation and avoids CBox bounds collapse",
        displayRect,
        transform,
        sourceExtent
    ));

    quads.push_back(manualTransformQuadForRect(
        "manualInverseTransformCandidate",
        "manual display point transform table using invertTransform(monitorTransform)",
        "manual inverse point-transform candidate; preserves corner orientation and avoids CBox bounds collapse",
        displayRect,
        static_cast<int>(inverse),
        sourceExtent
    ));

    quads.push_back(captureQuadFromRect(
        "cboxDirectBounds",
        "CBox::transform(monitorTransform, sourceExtent) bounds only",
        "box-transform comparison only; loses corner orientation",
        transformedSourceRectCandidate(currentSourceRect, transform, sourceExtent),
        sourceExtent,
        transform,
        "comparison-only"
    ));

    quads.push_back(captureQuadFromRect(
        "cboxInverseBounds",
        "CBox::transform(invertTransform(monitorTransform), sourceExtent) bounds only",
        "inverse box-transform comparison only; loses corner orientation",
        inverseTransformedSourceRectCandidate(currentSourceRect, transform, sourceExtent),
        sourceExtent,
        transform,
        "comparison-only"
    ));

    return quads;
}


std::string sourceMappingCandidateOverrideValue() {
    return "";
}


std::string selectedSourceMappingCandidateName(int transform) {
    const auto override = sourceMappingCandidateOverrideValue();
    if (!override.empty())
        return override;
    if (const char* candidate = std::getenv("HGS_HYPRGLASS_SOURCE_MAPPING_CANDIDATE"); candidate && candidate[0] != '\0')
        return candidate;
    if (transform == 0)
        return "currentDirectRect";
    return "preblurInverseTransformCandidate";
}

const CaptureQuadCandidate* findCaptureQuadCandidate(const std::vector<CaptureQuadCandidate>& candidates, std::string_view name) {
    for (const auto& candidate : candidates) {
        if (candidate.name == name)
            return &candidate;
    }
    return nullptr;
}

CaptureQuadCandidate selectedCaptureQuadCandidateForShaderSourceMap(
    const RectSummary& displayRect,
    const RectSummary& currentSourceRect,
    int transform,
    const RectSummary& sourceExtent,
    std::vector<CaptureQuadCandidate>& candidates,
    std::string& selectedName
) {
    candidates = displayToCaptureQuadCandidatesForRect(displayRect, currentSourceRect, transform, sourceExtent);
    selectedName = selectedSourceMappingCandidateName(transform);

    if (const auto* selected = findCaptureQuadCandidate(candidates, selectedName); selected)
        return *selected;

    selectedName = transform == 0 ? "currentDirectRect" : "preblurInverseTransformCandidate";
    if (const auto* selected = findCaptureQuadCandidate(candidates, selectedName); selected)
        return *selected;

    selectedName = "none";
    return {};
}






void applyShaderSourceUvMappingToRecord(const ShaderSourceUvMapping& mapping, BackdropCaptureRecord& record) {
    record.sourceUvTopLeft = mapping.topLeft;
    record.sourceUvTopRight = mapping.topRight;
    record.sourceUvBottomRight = mapping.bottomRight;
    record.sourceUvBottomLeft = mapping.bottomLeft;
    record.sourceUvRect = mapping.bounds;
    record.sourceMapping = mapping.mapping;
    record.sourceCroppingMode = "custom-shader-corner-uv";
    record.uvMappingType = "custom-shader-corner-uv";
    record.selectedSourceMappingCandidate = mapping.selectedCandidateName;
    record.selectedSourceQuad = mapping.selectedQuad;
    record.sourceMappingCandidates = mapping.candidates;
    record.transformCaptureDiagnosticEnabled = mapping.diagnosticOnly;
    record.shaderUsesFourCornerUV = true;
}






bool drawFluidGlassShaderNow(
    const MaterialDescriptor& material,
    const SP<Render::ITexture>& captureTexture,
    double sourceWidth,
    double sourceHeight,
    const RectSummary& sourceUvRect,
    BackdropCaptureRecord& record
) {
    (void)sourceUvRect;
    if (!Render::GL::g_pHyprOpenGL) {
        record.status = "shader backend unavailable";
        record.error = "OpenGL renderer unavailable for fluid-glass";
        updateFluidGlassShaderStatus(false, record.error);
        return false;
    }
    if (!ensureFluidGlassShader()) {
        const auto shaderStatus = fluidGlassShaderStatus();
        record.status = "shader unavailable";
        record.error = shaderStatus.error.empty() ? "fluid-glass shader unavailable" : shaderStatus.error;
        return false;
    }
    if (!g_pHyprRenderer || !g_pHyprRenderer->m_renderData.pMonitor) {
        record.status = "current monitor unavailable";
        record.error = "current monitor unavailable for fluid-glass";
        return false;
    }
    if (!captureTexture || !captureTexture->ok()) {
        record.status = "capture texture unavailable";
        record.error = "capture texture unavailable for fluid-glass";
        return false;
    }
    if (sourceWidth <= 0.0 || sourceHeight <= 0.0 || !rectValid(material.destinationRect) || !rectValid(material.sourceBackdropRect)) {
        record.status = "source mapping invalid";
        record.error = "fluid-glass source or destination rect is invalid";
        return false;
    }
    const auto uvMapping = shaderSourceUvMappingFor(material, sourceWidth, sourceHeight);
    if (!uvMapping.supported) {
        record.status = "source mapping unsupported";
        record.error = uvMapping.error.empty() ? "fluid-glass source mapping unsupported" : uvMapping.error;
        record.selectedSourceMappingCandidate = uvMapping.selectedCandidateName;
        record.selectedSourceQuad = uvMapping.selectedQuad;
        record.sourceMappingCandidates = uvMapping.candidates;
        record.transformCaptureDiagnosticEnabled = uvMapping.diagnosticOnly;
        record.shaderUsesFourCornerUV = true;
        return false;
    }
    applyShaderSourceUvMappingToRecord(uvMapping, record);

    CBox box(material.destinationRect.x, material.destinationRect.y, material.destinationRect.width, material.destinationRect.height);
    CRegion drawDamage{g_pHyprRenderer->m_renderData.damage};
    drawDamage.intersect(box.x, box.y, box.width, box.height);
    if (drawDamage.empty()) {
        record.status = "ok";
        record.error.clear();
        return true;
    }

    CBox projectedBox = box;
    g_pHyprRenderer->m_renderData.renderModif.applyToBox(projectedBox);

    auto transform = captureTexture->m_transform;
    if (g_pHyprRenderer->monitorTransformEnabled()) {
        const auto monitor = g_pHyprRenderer->m_renderData.pMonitor.lock();
        if (monitor) {
            const auto monitorInverted = Math::wlTransformToHyprutils(Math::invertTransform(monitor->m_transform));
            transform = Math::composeTransform(monitorInverted, transform);
        }
    }

    const auto glMatrix = g_pHyprRenderer->projectBoxToTarget(projectedBox, transform);
    auto shader = Render::GL::g_pHyprOpenGL->useShader(g_fluidGlassShader);
    if (!shader || shader->program() == 0) {
        record.status = "shader unavailable";
        record.error = "fluid-glass shader program unavailable";
        updateFluidGlassShaderStatus(false, record.error);
        return false;
    }

    glActiveTexture(GL_TEXTURE0);
    captureTexture->bind();
    captureTexture->setTexParameter(GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    captureTexture->setTexParameter(GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    captureTexture->setTexParameter(GL_TEXTURE_MAG_FILTER, captureTexture->magFilter);
    captureTexture->setTexParameter(GL_TEXTURE_MIN_FILTER, captureTexture->minFilter);

    shader->setUniformMatrix3fv(SHADER_PROJ, 1, GL_TRUE, glMatrix.getMatrix());
    shader->setUniformInt(SHADER_TEX, 0);
    shader->setUniformFloat(SHADER_ALPHA, static_cast<float>(clampDouble(material.alphaUsed, 0.0, 1.0)));

    const GLuint program = shader->program();
    auto setUniform1f = [&](const char* name, float value) {
        const GLint loc = glGetUniformLocation(program, name);
        if (loc >= 0)
            glUniform1f(loc, value);
    };
    auto setUniform2f = [&](const char* name, float x, float y) {
        const GLint loc = glGetUniformLocation(program, name);
        if (loc >= 0)
            glUniform2f(loc, x, y);
    };
    auto setUniform4f = [&](const char* name, float x, float y, float z, float w) {
        const GLint loc = glGetUniformLocation(program, name);
        if (loc >= 0)
            glUniform4f(loc, x, y, z, w);
    };

    setUniform2f("uSourceTopLeft", static_cast<float>(uvMapping.topLeft.x), static_cast<float>(uvMapping.topLeft.y));
    setUniform2f("uSourceTopRight", static_cast<float>(uvMapping.topRight.x), static_cast<float>(uvMapping.topRight.y));
    setUniform2f("uSourceBottomRight", static_cast<float>(uvMapping.bottomRight.x), static_cast<float>(uvMapping.bottomRight.y));
    setUniform2f("uSourceBottomLeft", static_cast<float>(uvMapping.bottomLeft.x), static_cast<float>(uvMapping.bottomLeft.y));
    setUniform2f("uDestinationSize", static_cast<float>(material.destinationRect.width), static_cast<float>(material.destinationRect.height));
    setUniform1f("uDisplacementPx", static_cast<float>(material.sdfDisplacementStrengthPx));
    setUniform1f("uRadiusPx", static_cast<float>(material.radiusUsed));
    setUniform1f("uEdgeWidthPx", static_cast<float>(material.sdfEdgeWidthPx));
    setUniform1f("uAlpha", static_cast<float>(clampDouble(material.alphaUsed, 0.0, 1.0)));
    setUniform4f(
        "uTint",
        material.color.r,
        material.color.g,
        material.color.b,
        static_cast<float>(clampDouble(material.tintOpacityRequested * 0.45 + 0.025, 0.025, 0.16))
    );

    glBindVertexArray(shader->getUniformLocation(SHADER_SHADER_VAO));
    glBindBuffer(GL_ARRAY_BUFFER, shader->getUniformLocation(SHADER_SHADER_VBO));

    auto verts = Render::GL::fullVerts;
    verts[0].u = 0.0F;
    verts[0].v = 0.0F;
    verts[1].u = 0.0F;
    verts[1].v = 1.0F;
    verts[2].u = 1.0F;
    verts[2].v = 0.0F;
    verts[3].u = 1.0F;
    verts[3].v = 1.0F;
    glBufferData(GL_ARRAY_BUFFER, sizeof(verts), nullptr, GL_DYNAMIC_DRAW);
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(verts), verts.data());

    Render::GL::g_pHyprOpenGL->blend(true);
    drawDamage.forEachRect([](const auto& rect) {
        Render::GL::g_pHyprOpenGL->scissor(&rect, g_pHyprRenderer->m_renderData.transformDamage);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    });
    Render::GL::g_pHyprOpenGL->scissor(nullptr);
    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    captureTexture->unbind();

    record.status = "ok";
    record.error.clear();
    return true;
}

bool captureBackdropForCurrentMonitorNow() {
    MonitorBackdropCaptureRecord record;
    record.captureAttempted = true;
    record.captureGeneration = nextCaptureGeneration();
    if (!g_pHyprRenderer) {
        record.status = "renderer unavailable";
        record.error = "renderer unavailable";
        updateMonitorBackdropCaptureRecord(record);
        return false;
    }

    const auto currentMonitor = g_pHyprRenderer->renderData().pMonitor.lock();
    if (!currentMonitor) {
        record.status = "current monitor unavailable";
        record.error = "current monitor unavailable";
        updateMonitorBackdropCaptureRecord(record);
        return false;
    }
    record.monitor = currentMonitor->m_name;

    const auto sourceFB = g_pHyprRenderer->renderData().currentFB;
    if (!sourceFB || !sourceFB->isAllocated() || !sourceFB->getTexture()) {
        record.status = "source framebuffer unavailable";
        record.error = "current framebuffer texture unavailable";
        updateMonitorBackdropCaptureRecord(record);
        return false;
    }

    const auto captureFB = backdropCaptureFramebufferFor(currentMonitor, sourceFB);
    if (!captureFB || !captureFB->isAllocated()) {
        record.status = "capture framebuffer unavailable";
        record.error = "failed to allocate capture framebuffer";
        updateMonitorBackdropCaptureRecord(record);
        return false;
    }

    const auto sourceTexture = sourceFB->getTexture();
    if (!sourceTexture || !sourceTexture->ok()) {
        record.status = "source framebuffer texture unavailable";
        record.error = "current framebuffer texture unavailable";
        updateMonitorBackdropCaptureRecord(record);
        return false;
    }

    const CBox sourceBox = {0, 0, sourceTexture->m_size.x, sourceTexture->m_size.y};
    if (sourceBox.width <= 0 || sourceBox.height <= 0) {
        record.status = "monitor framebuffer geometry invalid";
        record.error = "current framebuffer texture size is invalid";
        updateMonitorBackdropCaptureRecord(record);
        return false;
    }
    record.sourceExtent = {
        .x = 0.0,
        .y = 0.0,
        .width = static_cast<double>(sourceBox.width),
        .height = static_cast<double>(sourceBox.height),
    };

    {
        auto guard = g_pHyprRenderer->bindTempFB(captureFB);
        const auto oldProjectionType = g_pHyprRenderer->m_renderData.projectionType;
        const auto oldFbSize = g_pHyprRenderer->m_renderData.fbSize;
        const auto oldTransformDamage = g_pHyprRenderer->m_renderData.transformDamage;

        record.captureFaithfulCopyAttempted = true;
        record.captureCopyMethod = "tex-pass-export-projection";
        record.captureCopyProjection = "RPT_EXPORT";

        g_pHyprRenderer->m_renderData.fbSize = Vector2D{static_cast<double>(sourceBox.width), static_cast<double>(sourceBox.height)};
        g_pHyprRenderer->setProjectionType(Render::RPT_EXPORT);
        g_pHyprRenderer->m_renderData.transformDamage = false;
        g_pHyprRenderer->setViewport(0, 0, sourceBox.width, sourceBox.height);
        g_pHyprRenderer->blend(false);

        CTexPassElement::SRenderData copyData;
        copyData.tex = sourceTexture;
        copyData.box = sourceBox;
        copyData.a = 1.0F;
        copyData.damage = CRegion{CBox(0, 0, sourceBox.width, sourceBox.height)};
        copyData.allowCustomUV = false;
        copyData.wrapX = WRAP_CLAMP_TO_EDGE;
        copyData.wrapY = WRAP_CLAMP_TO_EDGE;
        g_pHyprRenderer->draw(copyData, copyData.damage);

        g_pHyprRenderer->blend(true);
        g_pHyprRenderer->m_renderData.fbSize = oldFbSize;
        g_pHyprRenderer->m_renderData.transformDamage = oldTransformDamage;
        g_pHyprRenderer->setProjectionType(oldProjectionType);
        g_pHyprRenderer->setViewport(0, 0, static_cast<int>(currentMonitor->m_pixelSize.x), static_cast<int>(currentMonitor->m_pixelSize.y));
        record.captureCopyProjectionRestored = true;
    }

    const auto captureTexture = captureFB->getTexture();
    if (!captureTexture || !captureTexture->ok()) {
        record.status = "capture texture unavailable";
        record.error = "capture framebuffer texture was not ready after copy";
        updateMonitorBackdropCaptureRecord(record);
        return false;
    }

    record.captureReady = true;
    record.captureTextureReady = true;
    record.textureReady = true;
    record.selfSamplingRisk = false;
    record.status = "ok";
    record.error.clear();
    record.backend = "hyprland-renderer-temp-fb-texture-copy";
    record.textureSize = textureSizeSummary(captureTexture);
    updateMonitorBackdropCaptureRecord(record);
    return true;
}

































bool drawFluidGlassNow(const MaterialDescriptor& material) {
    BackdropCaptureRecord record;
    record.descriptorId = material.descriptorId;
    record.monitor = material.captureMonitor;
    record.sourceBackdropRect = material.sourceBackdropRect;
    record.destinationRect = material.destinationRect;

    if (!g_pHyprRenderer) {
        record.status = "renderer unavailable";
        record.error = "renderer unavailable";
        updateBackdropCaptureRecord(record);
        return false;
    }

    const auto currentMonitor = g_pHyprRenderer->renderData().pMonitor.lock();
    if (!currentMonitor) {
        record.status = "current monitor unavailable";
        record.error = "current monitor unavailable";
        updateBackdropCaptureRecord(record);
        return false;
    }
    record.monitor = currentMonitor->m_name;

    const auto monitorRecord = monitorBackdropCaptureRecordFor(record.monitor);
    copyMonitorCaptureStateToDescriptorRecord(record, monitorRecord);
    if (!captureRecordReady(record)) {
        record.status = monitorRecord.status.empty() ? "monitor capture not ready" : monitorRecord.status;
        record.error = monitorRecord.error.empty() ? "monitor backdrop capture was not ready before descriptor sampling" : monitorRecord.error;
        record.selfSamplingRisk = true;
        updateBackdropCaptureRecord(record);
        return false;
    }

    const auto sourceFB = g_backdropCaptureFramebuffers[record.monitor];
    if (!sourceFB || !sourceFB->isAllocated() || !sourceFB->getTexture()) {
        record.status = "capture framebuffer unavailable";
        record.error = "monitor capture framebuffer texture unavailable";
        copyMonitorCaptureStateToDescriptorRecord(record, monitorRecord);
        record.selfSamplingRisk = true;
        updateBackdropCaptureRecord(record);
        return false;
    }

    const auto captureTexture = sourceFB->getTexture();
    if (!captureTexture || !captureTexture->ok()) {
        record.status = "capture texture unavailable";
        record.error = "monitor capture texture unavailable";
        copyMonitorCaptureStateToDescriptorRecord(record, monitorRecord);
        record.selfSamplingRisk = true;
        updateBackdropCaptureRecord(record);
        return false;
    }

    const double sourceWidth = monitorRecord.sourceExtent.width > 0 ? monitorRecord.sourceExtent.width : captureTexture->m_size.x;
    const double sourceHeight = monitorRecord.sourceExtent.height > 0 ? monitorRecord.sourceExtent.height : captureTexture->m_size.y;
    record.sourceExtent = {
        .x = 0.0,
        .y = 0.0,
        .width = sourceWidth,
        .height = sourceHeight,
    };
    if (sourceWidth <= 0.0 || sourceHeight <= 0.0 || !rectValid(material.sourceBackdropRect) || !rectValid(material.destinationRect)) {
        record.status = "source mapping invalid";
        record.error = "backdrop source or destination rect is invalid";
        record.textureSize = textureSizeSummary(captureTexture);
        record.captureTextureReady = positiveExtent(record.textureSize);
        record.textureReady = record.captureTextureReady;
        record.captureReady = captureRecordReady(record);
        record.selfSamplingRisk = true;
        updateBackdropCaptureRecord(record);
        return false;
    }

    auto uvFor = [&](const RectSummary& source) {
        const double u0 = clampDouble(source.x / sourceWidth, 0.0, 1.0);
        const double v0 = clampDouble(source.y / sourceHeight, 0.0, 1.0);
        const double u1 = clampDouble((source.x + source.width) / sourceWidth, 0.0, 1.0);
        const double v1 = clampDouble((source.y + source.height) / sourceHeight, 0.0, 1.0);
        return RectSummary{
            .x = u0,
            .y = v0,
            .width = std::max(0.0, u1 - u0),
            .height = std::max(0.0, v1 - v0),
        };
    };

    auto setRecordUvCorners = [&](const RectSummary& uv) {
        record.sourceUvTopLeft = rectFromXYWH(uv.x, uv.y, 0.0, 0.0);
        record.sourceUvTopRight = rectFromXYWH(uv.x + uv.width, uv.y, 0.0, 0.0);
        record.sourceUvBottomRight = rectFromXYWH(uv.x + uv.width, uv.y + uv.height, 0.0, 0.0);
        record.sourceUvBottomLeft = rectFromXYWH(uv.x, uv.y + uv.height, 0.0, 0.0);
        record.uvMappingType = "axis-aligned-uv-rect-two-corner";
    };

    record.sourceUvRect = uvFor(material.sourceBackdropRect);
    setRecordUvCorners(record.sourceUvRect);

    if (!drawFluidGlassShaderNow(material, captureTexture, sourceWidth, sourceHeight, record.sourceUvRect, record)) {
        record.sourceMapping = record.selectedSourceMappingCandidate.empty() ?
            fluidShaderSourceMappingName(material.mode, material.drawTransform) :
            "shared-source-quad:" + record.selectedSourceMappingCandidate;
        record.sourceCroppingMode = "custom-shader-corner-uv";
        record.uvMappingType = "custom-shader-corner-uv";
        record.textureSize = textureSizeSummary(captureTexture);
        record.captureTextureReady = positiveExtent(record.textureSize);
        record.textureReady = record.captureTextureReady;
        record.captureReady = captureRecordReady(record);
        record.selfSamplingRisk = true;
        record.sampled = false;
        updateBackdropCaptureRecord(record);
        return false;
    }

    drawGlassPolishOverlays(material);

    record.textureSize = textureSizeSummary(captureTexture);
    record.captureTextureReady = positiveExtent(record.textureSize);
    record.textureReady = record.captureTextureReady;
    record.captureReady = captureRecordReady(record);
    record.selfSamplingRisk = false;
    record.status = "ok";
    record.error.clear();
    record.sourceMapping = record.selectedSourceMappingCandidate.empty() ?
        fluidShaderSourceMappingName(material.mode, material.drawTransform) :
        "shared-source-quad:" + record.selectedSourceMappingCandidate;
    record.sourceCroppingMode = "custom-shader-corner-uv";
    record.uvMappingType = "custom-shader-corner-uv";
    record.sampled = record.captureReady;
    record.descriptorRendered = record.captureReady && !record.renderedFromStaleCapture;
    record.descriptorUsedCapture = record.captureReady;
    updateBackdropCaptureRecord(record);
    return true;
}

class CFluidGlassMonitorCapturePassElement : public IPassElement {
  public:
    bool needsLiveBlur() override {
        return false;
    }

    bool needsPrecomputeBlur() override {
        return false;
    }

    const char* passName() override {
        return "HGSFluidGlassMonitorCapturePassElement";
    }

    ePassElementType type() override {
        return EK_CUSTOM;
    }

    std::optional<CBox> boundingBox() override {
        return std::nullopt;
    }

    CRegion opaqueRegion() override {
        return {};
    }

    std::vector<UP<IPassElement>> draw() override {
        captureBackdropForCurrentMonitorNow();
        return {};
    }
};

class CFluidGlassPassElement : public IPassElement {
  public:
    explicit CFluidGlassPassElement(MaterialDescriptor material_) : material(std::move(material_)) {}

    bool needsLiveBlur() override {
        return false;
    }

    bool needsPrecomputeBlur() override {
        return false;
    }

    const char* passName() override {
        return "HGSFluidGlassPassElement";
    }

    ePassElementType type() override {
        return EK_CUSTOM;
    }

    std::optional<CBox> boundingBox() override {
        if (!rectValid(material.destinationRect) || !g_pHyprRenderer || !g_pHyprRenderer->m_renderData.pMonitor || g_pHyprRenderer->m_renderData.pMonitor->m_scale <= 0)
            return std::nullopt;
        return CBox(material.destinationRect.x, material.destinationRect.y, material.destinationRect.width, material.destinationRect.height)
            .scale(1.0 / g_pHyprRenderer->m_renderData.pMonitor->m_scale)
            .round();
    }

    CRegion opaqueRegion() override {
        return {};
    }

    std::vector<UP<IPassElement>> draw() override {
        drawFluidGlassNow(material);
        return {};
    }

    MaterialDescriptor material;
};



void drawFluidGlassMaterial(const MaterialDescriptor& material) {
    if (!material.drawable || !g_pHyprRenderer)
        return;

    g_pHyprRenderer->m_renderPass.add(makeUnique<CFluidGlassPassElement>(material));
}






void drawFluidGlassMonitorCapture() {
    if (!g_pHyprRenderer)
        return;

    g_pHyprRenderer->m_renderPass.add(makeUnique<CFluidGlassMonitorCapturePassElement>());
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
    if (!isTransformSupported(static_cast<int>(currentMonitor->m_transform)))
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
    if (!isTransformSupported(static_cast<int>(currentMonitor->m_transform)))
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
    const bool shouldCaptureThisMonitor = materialModeUsesBackdropCapture(snapshot.materialMode) &&
        (!materialModeUsesFluidGlass(snapshot.materialMode) || fluidGlassSupportsTransform(static_cast<int>(currentMonitor->m_transform)));
    if (shouldCaptureThisMonitor)
        drawFluidGlassMonitorCapture();

    for (const auto& [id, descriptor] : snapshot.descriptors) {
        const auto& match = matches.at(id);
        const auto* candidate = matchedCandidate(match, candidates);
        if (!candidate || candidate->monitor != currentMonitor->m_name)
            continue;

        const auto material = analyzeMaterialDrawable(snapshot.materialMode, descriptor, match, coordinates.at(id), candidates);
        if (!material.drawable)
            continue;

        if (snapshot.materialMode == "glass-v1" || material.backendUsed == "glass-v1-fallback")
            drawGlassV1Material(material);
        else if (materialModeUsesBackdropCapture(snapshot.materialMode))
            drawFluidGlassMaterial(material);
        else
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
    if (materialModeUsesFluidGlass(snapshot.materialMode)) {
        int shaderDescriptorCount = 0;
        int fallbackDescriptorCount = 0;
        std::set<int> activeShaderTransforms;
        for (const auto& [id, descriptor] : snapshot.descriptors) {
            const auto material = analyzeMaterialDrawable(snapshot.materialMode, descriptor, matches.at(id), coordinates.at(id), candidates);
            if (!material.drawable)
                continue;
            if (material.backendUsed == "fluid-shader") {
                ++shaderDescriptorCount;
                activeShaderTransforms.insert(material.drawTransform);
            } else if (material.backendUsed == "glass-v1-fallback")
                ++fallbackDescriptorCount;
        }
        json activeTransforms = json::array();
        for (const auto transform : activeShaderTransforms)
            activeTransforms.push_back(transform);
        out["shaderCaptureSupportedTransforms"] = json::array({0});
        out["activeShaderCaptureTransforms"] = activeTransforms;
        out["shaderCaptureTransformPolicy"] = "transform 0 is the baseline; transformed descriptors may use fluid-shader only when the selected shared source quad is valid";
        out["transformedCaptureSamplingStatus"] = "per-descriptor-quad-mapped-when-selected-quad-valid";
        out["fallbackBackend"] = "glass-v1";
        out["shaderDescriptorCount"] = shaderDescriptorCount;
        out["fallbackDescriptorCount"] = fallbackDescriptorCount;
    }
    if (materialModeUsesNativeBlur(snapshot.materialMode)) {
        out["nativeBlurEnabled"] = true;
        out["effectiveBlurControl"] = "global-kernel-per-surface-alpha";
        out["perSurfaceBlurSupported"] = false;
        out["perSurfaceBlurSupport"] = "alpha-only";
    }
    if (materialModeUsesBackdropCapture(snapshot.materialMode)) {
        std::set<std::string> capturedMonitors;
        int sampledDescriptorCount = 0;
        int captureAttemptedDescriptorCount = 0;
        int renderedDescriptorCount = 0;
        int usedCaptureDescriptorCount = 0;
        int renderedFromStaleCaptureDescriptorCount = 0;
        bool anyCaptureAttempted = false;
        bool anyCaptureReady = false;
        bool anySelfSamplingRisk = false;
        uint64_t maxCaptureGeneration = 0;
        {
            std::lock_guard guard(g_stateMutex);
            for (const auto& [id, descriptor] : snapshot.descriptors) {
                auto it = g_backdropCaptureRecords.find(id);
                if (it == g_backdropCaptureRecords.end())
                    continue;
                const auto& record = it->second;
                if (record.captureAttempted) {
                    ++captureAttemptedDescriptorCount;
                    anyCaptureAttempted = true;
                }
                const bool recordReady = captureRecordReady(record);
                if (record.descriptorRendered && recordReady)
                    ++renderedDescriptorCount;
                if (record.descriptorUsedCapture && recordReady)
                    ++usedCaptureDescriptorCount;
                if (record.renderedFromStaleCapture && recordReady)
                    ++renderedFromStaleCaptureDescriptorCount;
                maxCaptureGeneration = std::max(maxCaptureGeneration, record.captureGeneration);
                if (record.sampled && recordReady) {
                    ++sampledDescriptorCount;
                    anyCaptureReady = true;
                    if (!record.monitor.empty())
                        capturedMonitors.insert(record.monitor);
                }
                anySelfSamplingRisk = anySelfSamplingRisk || record.selfSamplingRisk;
            }
            for (const auto& [monitor, record] : g_backdropMonitorCaptureRecords) {
                if (record.captureAttempted)
                    anyCaptureAttempted = true;
                maxCaptureGeneration = std::max(maxCaptureGeneration, record.captureGeneration);
                if (monitorCaptureRecordReady(record)) {
                    anyCaptureReady = true;
                    if (!monitor.empty())
                        capturedMonitors.insert(monitor);
                }
                anySelfSamplingRisk = anySelfSamplingRisk || record.selfSamplingRisk;
            }
        }
        out["captureEnabled"] = true;
        out["captureStage"] = "RENDER_POST_WINDOWS";
        out["captureAttempted"] = anyCaptureAttempted;
        out["captureGeneration"] = maxCaptureGeneration;
        out["captureReady"] = anyCaptureReady;
        out["capturedMonitorCount"] = capturedMonitors.size();
        out["captureAttemptedDescriptorCount"] = captureAttemptedDescriptorCount;
        out["renderedDescriptorCount"] = renderedDescriptorCount;
        out["usedCaptureDescriptorCount"] = usedCaptureDescriptorCount;
        out["renderedFromStaleCaptureDescriptorCount"] = renderedFromStaleCaptureDescriptorCount;
        out["sampledDescriptorCount"] = sampledDescriptorCount;
        out["lastCaptureStatus"] = snapshot.lastBackdropCaptureStatus;
        out["lastCaptureError"] = snapshot.lastBackdropCaptureError;
        out["selfSamplingRisk"] = anySelfSamplingRisk;
        out["captureBackend"] = "hyprland-renderer-temp-fb-texture-copy";
        out["sourceCroppingSupported"] = true;
        out["sourceCroppingMode"] = "custom-shader-corner-uv";
        out["uvMappingType"] = "custom-shader-corner-uv";
        out["ctexSupportsCustomUvCorners"] = false;
        out["ctexSupportsAxisAlignedUvRect"] = true;
        out["ctexCanRotateSourceCrop"] = false;
        out["transformSourceMapping"] = "fluid-glass-selected-shared-source-quad-per-descriptor";
        out["fractionalSourceMapping"] = "monitor-local-logical-times-scale-rounded";
        if (materialModeUsesFluidGlass(snapshot.materialMode)) {
            double displacementStrengthPx = 0.0;
            double edgeWidthPx = 0.0;
            for (const auto& [id, descriptor] : snapshot.descriptors) {
                const auto material = analyzeMaterialDrawable(snapshot.materialMode, descriptor, matches.at(id), coordinates.at(id), candidates);
                if (!material.drawable || !material.refractionDebugEnabled)
                    continue;
                displacementStrengthPx = material.sdfDisplacementStrengthPx;
                edgeWidthPx = material.sdfEdgeWidthPx;
                break;
            }
            const auto shaderStatus = fluidGlassShaderStatus();
            out["shaderEnabled"] = true;
            out["shaderBackend"] = shaderStatus.backend;
            out["shaderCompiled"] = shaderStatus.compiled;
            out["shaderReady"] = shaderStatus.ready;
            out["shaderError"] = shaderStatus.error;
            out["sdfMaskEnabled"] = true;
            out["refractionDebugEnabled"] = true;
            out["displacementStrengthPx"] = displacementStrengthPx;
            out["edgeWidthPx"] = edgeWidthPx;
            out["displacementFormula"] = "clamp(1.5 + frost * 8.0, 1.5, 9.5)";
            out["edgeWidthFormula"] = "clamp(min(width, height) * 0.30, 6, 18)";
            out["implementedShaderTransforms"] = json::array({0});
            out["supportedShaderTransforms"] = json::array({0});
            out["failedShaderTransforms"] = json::array();
            out["targetShaderTransforms"] = json::array({0});
            out["sourceMappingEvidence"] = "fluid-glass uses shared display-to-capture source quads; transformed descriptors are shader-backed only when their selected quad is valid";
            out["transformPolicy"] = "fluid-glass supports capture-backed shader sampling per descriptor when a selected shared source quad is valid, with glass-v1 fallback otherwise";
        }
        out["blurEnabled"] = false;
        out["refractionEnabled"] = materialModeUsesSdfRefraction(snapshot.materialMode);
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
    out << "  pluginVersion: " << HGS_HYPRGLASS_PLUGIN_VERSION << "\n";
    out << "  buildId: " << HGS_HYPRGLASS_BUILD_ID << "\n";
    out << "  buildTime: " << HGS_HYPRGLASS_BUILD_TIME << "\n";
    out << "  gitCommit: " << HGS_HYPRGLASS_GIT_COMMIT << "\n";
    out << "  buildType: " << HGS_HYPRGLASS_BUILD_TYPE << "\n";
    out << "  materialModesSupported: flat,blur-native,glass-v1,fluid-glass\n";
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
    out << "  supportedTransformMonitorCount: " << countSupportedTransformMonitors(monitors) << "\n";
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
        {"build", buildInfoToJSON()},
        {"capabilities", capabilitiesToJSON()},
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
        {"supportedTransformMonitorCount", countSupportedTransformMonitors(monitors)},
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
        g_backdropCaptureRecords.clear();
        g_backdropMonitorCaptureRecords.clear();
        if (materialModeUsesBackdropCapture(g_materialMode)) {
            g_lastBackdropCaptureStatus = "pending";
            g_lastBackdropCaptureError.clear();
        }
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
        g_backdropCaptureRecords.clear();
        g_backdropMonitorCaptureRecords.clear();
        g_lastBackdropCaptureStatus = materialModeUsesBackdropCapture(g_materialMode) ? "no descriptors" : g_lastBackdropCaptureStatus;
        g_lastBackdropCaptureError.clear();
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
        if (materialModeUsesFluidGlass(snapshot.materialMode)) {
            status["shaderCaptureSupportedTransforms"] = json::array({0});
            status["shaderCaptureTransformPolicy"] = "transform 0 is the baseline; transformed descriptors may use fluid-shader only when the selected shared source quad is valid";
            status["transformedCaptureSamplingStatus"] = "per-descriptor-quad-mapped-when-selected-quad-valid";
            status["fallbackBackend"] = "glass-v1";
        }
        if (materialModeUsesNativeBlur(snapshot.materialMode)) {
            status["nativeBlurEnabled"] = true;
            status["effectiveBlurControl"] = "global-kernel-per-surface-alpha";
            status["perSurfaceBlurSupported"] = false;
            status["perSurfaceBlurSupport"] = "alpha-only";
        }
        if (materialModeUsesBackdropCapture(snapshot.materialMode)) {
            status["captureEnabled"] = true;
            status["captureStage"] = "RENDER_POST_WINDOWS";
            status["lastCaptureStatus"] = snapshot.lastBackdropCaptureStatus;
            status["lastCaptureError"] = snapshot.lastBackdropCaptureError;
            status["captureBackend"] = "hyprland-renderer-temp-fb-texture-copy";
            status["sourceCroppingMode"] = "custom-shader-corner-uv";
            status["uvMappingType"] = "custom-shader-corner-uv";
            status["ctexSupportsCustomUvCorners"] = false;
            status["ctexSupportsAxisAlignedUvRect"] = true;
            status["ctexCanRotateSourceCrop"] = false;
            status["transformSourceMapping"] = "fluid-glass-selected-shared-source-quad-per-descriptor";
            status["fractionalSourceMapping"] = "monitor-local-logical-times-scale-rounded";
            if (materialModeUsesFluidGlass(snapshot.materialMode)) {
                const auto shaderStatus = fluidGlassShaderStatus();
                status["shaderEnabled"] = true;
                status["shaderBackend"] = shaderStatus.backend;
                status["shaderCompiled"] = shaderStatus.compiled;
                status["shaderReady"] = shaderStatus.ready;
                status["shaderError"] = shaderStatus.error;
                status["sdfMaskEnabled"] = true;
                status["refractionDebugEnabled"] = true;
                status["displacementStrengthPx"] = "clamp(1.5 + frost * 8.0, 1.5, 9.5)";
                status["edgeWidthPx"] = "clamp(min(width, height) * 0.30, 6, 18)";
                status["implementedShaderTransforms"] = json::array({0});
                status["supportedShaderTransforms"] = json::array({0});
                status["failedShaderTransforms"] = json::array();
                status["targetShaderTransforms"] = json::array({0});
                status["sourceMappingEvidence"] = "fluid-glass uses shared display-to-capture source quads; transformed descriptors are shader-backed only when their selected quad is valid";
                status["transformPolicy"] = "fluid-glass supports capture-backed shader sampling per descriptor when a selected shared source quad is valid, with glass-v1 fallback otherwise";
            }
            status["blurEnabled"] = false;
            status["refractionEnabled"] = materialModeUsesSdfRefraction(snapshot.materialMode);
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
        if (materialModeUsesBackdropCapture(mode)) {
            g_backdropCaptureRecords.clear();
            g_backdropMonitorCaptureRecords.clear();
            g_lastBackdropCaptureStatus = "pending";
            g_lastBackdropCaptureError.clear();
        } else if (mode == "off") {
            g_backdropCaptureRecords.clear();
            g_backdropMonitorCaptureRecords.clear();
            g_lastBackdropCaptureStatus = "disabled";
            g_lastBackdropCaptureError.clear();
        }
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
    if (mode == "glass-v1")
        return setMaterialMode("glass-v1");
    if (mode == "fluid-glass")
        return setMaterialMode("fluid-glass");
    return "error: expected off, flat, blur-native, glass-v1, fluid-glass, or status\n";
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

    return {"hgs-hyprglass", "HyprGlassShell compositor-side glass material engine", "CoastLineSec", HGS_HYPRGLASS_PLUGIN_VERSION};
}

APICALL EXPORT void PLUGIN_EXIT() {
    g_renderStageListener.reset();
    {
        std::lock_guard guard(g_stateMutex);
        g_debugOverlayEnabled = false;
        g_materialMode = "off";
        g_lastDebugOverlayRenderStatus = "disabled";
        g_lastMaterialRenderStatus = "disabled";
        g_backdropCaptureRecords.clear();
        g_backdropMonitorCaptureRecords.clear();
        g_lastBackdropCaptureStatus = "disabled";
        g_lastBackdropCaptureError.clear();
    }
    g_backdropCaptureFramebuffers.clear();
    g_fluidGlassShader.reset();
    g_fluidGlassShaderCompileAttempted = false;
    g_fluidGlassShaderCompiled = false;
    g_fluidGlassShaderError.clear();
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
