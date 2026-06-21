package hyprglass

import (
	"bytes"
	"encoding/json"
	"errors"
	"fmt"
	"math"
	"regexp"
	"strings"
)

const (
	DescriptorSchemaVersion = 1

	PluginName = "hgs-hyprglass"
)

type DescriptorSet struct {
	Version     int          `json:"version"`
	Generation  uint64       `json:"generation,omitempty"`
	Descriptors []Descriptor `json:"descriptors"`
}

type Descriptor struct {
	Version  int        `json:"version,omitempty"`
	ID       string     `json:"id"`
	Kind     string     `json:"kind,omitempty"`
	Surface  SurfaceRef `json:"surface"`
	Geometry Geometry   `json:"geometry"`
	Shape    Shape      `json:"shape"`
	Material Material   `json:"material"`
	Debug    Debug      `json:"debug,omitempty"`
	Sequence uint64     `json:"sequence,omitempty"`
}

type SurfaceRef struct {
	Namespace string  `json:"namespace"`
	Address   string  `json:"address,omitempty"`
	Layer     string  `json:"layer,omitempty"`
	Role      string  `json:"role,omitempty"`
	Monitor   Monitor `json:"monitor"`
}

type Monitor struct {
	Name string `json:"name,omitempty"`
	ID   *int   `json:"id,omitempty"`
}

type Geometry struct {
	Logical     Rect    `json:"logical"`
	Monitor     *Rect   `json:"monitor,omitempty"`
	Framebuffer *Rect   `json:"framebuffer,omitempty"`
	Scale       float64 `json:"scale,omitempty"`
}

type Rect struct {
	X      float64 `json:"x"`
	Y      float64 `json:"y"`
	Width  float64 `json:"width"`
	Height float64 `json:"height"`
}

type Shape struct {
	Type   string `json:"type"`
	Radius Radii  `json:"radius"`
}

type Radii struct {
	TopLeft     float64 `json:"topLeft"`
	TopRight    float64 `json:"topRight"`
	BottomRight float64 `json:"bottomRight"`
	BottomLeft  float64 `json:"bottomLeft"`
}

type Material struct {
	Enabled            bool       `json:"enabled"`
	Preset             string     `json:"preset"`
	Opacity            float64    `json:"opacity"`
	Frost              float64    `json:"frost"`
	Saturation         float64    `json:"saturation"`
	ContrastBias       float64    `json:"contrastBias"`
	Tint               Tint       `json:"tint"`
	Refraction         Refraction `json:"refraction"`
	Rim                Rim        `json:"rim"`
	Highlight          Highlight  `json:"highlight"`
	Reflection         Reflection `json:"reflection"`
	Shadow             Shadow     `json:"shadow"`
	SuppressFullscreen bool       `json:"suppressFullscreen"`
}

type Tint struct {
	Mode    string  `json:"mode"`
	Color   string  `json:"color,omitempty"`
	Opacity float64 `json:"opacity"`
}

type Refraction struct {
	Strength            float64 `json:"strength"`
	EdgeWidth           float64 `json:"edgeWidth"`
	Displacement        float64 `json:"displacement"`
	ChromaticAberration float64 `json:"chromaticAberration"`
}

type Rim struct {
	Opacity float64 `json:"opacity"`
	Width   float64 `json:"width"`
}

type Highlight struct {
	Opacity float64 `json:"opacity"`
	Angle   float64 `json:"angle"`
	Spread  float64 `json:"spread"`
}

type Reflection struct {
	Opacity float64 `json:"opacity"`
	Angle   float64 `json:"angle"`
	Offset  float64 `json:"offset"`
	Blur    float64 `json:"blur"`
}

type Shadow struct {
	InnerOpacity float64 `json:"innerOpacity"`
	OuterOpacity float64 `json:"outerOpacity"`
	Radius       float64 `json:"radius"`
}

type Debug struct {
	Name        string `json:"name,omitempty"`
	ShowBounds  bool   `json:"showBounds,omitempty"`
	ShowSamples bool   `json:"showSamples,omitempty"`
}

type Status struct {
	Version                      int                `json:"version"`
	Plugin                       string             `json:"plugin"`
	PluginLoaded                 bool               `json:"pluginLoaded"`
	Available                    bool               `json:"available"`
	Build                        *BuildStatus       `json:"build,omitempty"`
	Capabilities                 *CapabilityStatus  `json:"capabilities,omitempty"`
	CompositorRendering          bool               `json:"compositorRendering"`
	Generation                   uint64             `json:"generation,omitempty"`
	ApplyCount                   uint64             `json:"applyCount,omitempty"`
	DescriptorCount              int                `json:"descriptorCount"`
	MatchedCount                 int                `json:"matchedDescriptorCount,omitempty"`
	UnmatchedCount               int                `json:"unmatchedDescriptorCount,omitempty"`
	AmbiguousCount               int                `json:"ambiguousDescriptorCount,omitempty"`
	SkippedCount                 int                `json:"skippedDescriptorCount,omitempty"`
	ErrorCount                   int                `json:"errorDescriptorCount,omitempty"`
	CandidateCount               int                `json:"candidateSurfaceCount,omitempty"`
	MonitorCount                 int                `json:"monitorCount,omitempty"`
	FractionalScaleMonitors      int                `json:"fractionalScaleMonitorCount,omitempty"`
	TransformedMonitors          int                `json:"transformedMonitorCount,omitempty"`
	SupportedTransformMonitors   int                `json:"supportedTransformMonitorCount,omitempty"`
	UnsupportedTransformMonitors int                `json:"unsupportedTransformMonitorCount,omitempty"`
	StaleDescriptorCount         int                `json:"staleDescriptorCount,omitempty"`
	CoordinateAligned            int                `json:"coordinateAlignedCount,omitempty"`
	CoordinateNear               int                `json:"coordinateNearCount,omitempty"`
	CoordinateMismatch           int                `json:"coordinateMismatchedCount,omitempty"`
	CoordinateUnknown            int                `json:"coordinateUnknownCount,omitempty"`
	HasPayload                   bool               `json:"hasPayload"`
	LastApplyStatus              string             `json:"lastApplyStatus,omitempty"`
	LastError                    string             `json:"lastError,omitempty"`
	Reason                       string             `json:"reason,omitempty"`
	Descriptors                  []DescriptorStatus `json:"descriptors,omitempty"`
	CandidateSurfaces            []SurfaceStatus    `json:"candidateSurfaces,omitempty"`
	Monitors                     []MonitorStatus    `json:"monitors,omitempty"`
	Warnings                     []string           `json:"warnings,omitempty"`
	DebugOverlay                 DebugOverlayStatus `json:"debugOverlay,omitempty"`
	Material                     MaterialStatus     `json:"material,omitempty"`
}

type BuildStatus struct {
	ID            string `json:"id,omitempty"`
	PluginVersion string `json:"pluginVersion,omitempty"`
	GitCommit     string `json:"gitCommit,omitempty"`
	BuildTime     string `json:"buildTime,omitempty"`
	BuildType     string `json:"buildType,omitempty"`
}

type CapabilityStatus struct {
	Materials    []string          `json:"materials,omitempty"`
	DebugOverlay bool              `json:"debugOverlay,omitempty"`
	NativeBlur   bool              `json:"nativeBlur,omitempty"`
	RenderStages map[string]string `json:"renderStages,omitempty"`
}

type DescriptorStatus struct {
	ID           string                       `json:"id"`
	Namespace    string                       `json:"namespace,omitempty"`
	Monitor      string                       `json:"monitor,omitempty"`
	Layer        string                       `json:"layer,omitempty"`
	Logical      Rect                         `json:"logical,omitempty"`
	Shape        Shape                        `json:"shape,omitempty"`
	Material     DescriptorMaterialInfo       `json:"material,omitempty"`
	Version      int                          `json:"version,omitempty"`
	Sequence     uint64                       `json:"sequence,omitempty"`
	DebugName    string                       `json:"debugName,omitempty"`
	Surface      SurfaceMatchStatus           `json:"surfaceMatch,omitempty"`
	Coordinate   CoordinateStatus             `json:"coordinate,omitempty"`
	DebugOverlay DebugOverlayDescriptorStatus `json:"debugOverlay,omitempty"`
	MaterialPass MaterialDescriptorStatus     `json:"compositorMaterial,omitempty"`
}

type DescriptorMaterialInfo struct {
	Preset             string  `json:"preset,omitempty"`
	Opacity            float64 `json:"opacity,omitempty"`
	Frost              float64 `json:"frost,omitempty"`
	RefractionStrength float64 `json:"refractionStrength,omitempty"`
	TintColor          string  `json:"tintColor,omitempty"`
	TintOpacity        float64 `json:"tintOpacity,omitempty"`
	RimOpacity         float64 `json:"rimOpacity,omitempty"`
	HighlightOpacity   float64 `json:"highlightOpacity,omitempty"`
	ShadowInnerOpacity float64 `json:"shadowInnerOpacity,omitempty"`
	ShadowOuterOpacity float64 `json:"shadowOuterOpacity,omitempty"`
}

type SurfaceMatchStatus struct {
	Status             string          `json:"status,omitempty"`
	Matched            bool            `json:"matched"`
	MatchReason        string          `json:"matchReason,omitempty"`
	DebugID            string          `json:"debugID,omitempty"`
	Namespace          string          `json:"namespace,omitempty"`
	Monitor            string          `json:"monitor,omitempty"`
	MonitorID          int64           `json:"monitorID,omitempty"`
	Layer              string          `json:"layer,omitempty"`
	HyprlandGeometry   Rect            `json:"hyprlandGeometry,omitempty"`
	SurfaceSize        Rect            `json:"surfaceSize,omitempty"`
	Scale              float64         `json:"scale,omitempty"`
	ScaleKind          string          `json:"scaleKind,omitempty"`
	FractionalScale    bool            `json:"fractionalScale,omitempty"`
	MonitorTransform   int             `json:"monitorTransform,omitempty"`
	TransformSupported bool            `json:"transformSupported,omitempty"`
	Mapped             bool            `json:"mapped,omitempty"`
	Visible            bool            `json:"visible,omitempty"`
	Candidates         []SurfaceStatus `json:"candidates,omitempty"`
}

type SurfaceStatus struct {
	DebugID              string   `json:"debugID,omitempty"`
	Namespace            string   `json:"namespace,omitempty"`
	Monitor              string   `json:"monitor,omitempty"`
	MonitorID            int64    `json:"monitorID,omitempty"`
	Layer                string   `json:"layer,omitempty"`
	HyprlandGeometry     Rect     `json:"hyprlandGeometry,omitempty"`
	SurfaceSize          Rect     `json:"surfaceSize,omitempty"`
	MonitorLogical       Rect     `json:"monitorLogical,omitempty"`
	MonitorFramebuffer   Rect     `json:"monitorFramebuffer,omitempty"`
	MonitorTransform     int      `json:"monitorTransform,omitempty"`
	Scale                float64  `json:"scale,omitempty"`
	ScaleKind            string   `json:"scaleKind,omitempty"`
	FractionalScale      bool     `json:"fractionalScale,omitempty"`
	TransformSupported   bool     `json:"transformSupported,omitempty"`
	Mapped               bool     `json:"mapped,omitempty"`
	Visible              bool     `json:"visible,omitempty"`
	MatchedDescriptorIDs []string `json:"matchedDescriptorIds,omitempty"`
}

type MonitorStatus struct {
	ID                 int64   `json:"id,omitempty"`
	Name               string  `json:"name,omitempty"`
	Scale              float64 `json:"scale,omitempty"`
	ScaleKind          string  `json:"scaleKind,omitempty"`
	FractionalScale    bool    `json:"fractionalScale,omitempty"`
	Transform          int     `json:"transform,omitempty"`
	TransformSupported bool    `json:"transformSupported,omitempty"`
	LogicalPosition    Point   `json:"logicalPosition,omitempty"`
	LogicalSize        Size    `json:"logicalSize,omitempty"`
	FramebufferSize    Size    `json:"framebufferSize,omitempty"`
}

type Point struct {
	X float64 `json:"x"`
	Y float64 `json:"y"`
}

type Size struct {
	Width  float64 `json:"width"`
	Height float64 `json:"height"`
}

type CoordinateStatus struct {
	Status                      string    `json:"status,omitempty"`
	Confidence                  string    `json:"confidence,omitempty"`
	Relation                    string    `json:"relation,omitempty"`
	DescriptorLogical           SpaceRect `json:"descriptorLogical,omitempty"`
	SurfaceLogical              SpaceRect `json:"surfaceLogical,omitempty"`
	MonitorLogical              Rect      `json:"monitorLogical,omitempty"`
	ComputedMonitorLocalLogical SpaceRect `json:"computedMonitorLocalLogical,omitempty"`
	ComputedFramebuffer         SpaceRect `json:"computedFramebuffer,omitempty"`
	ComputedFramebufferRounded  SpaceRect `json:"computedFramebufferRounded,omitempty"`
	Delta                       Rect      `json:"delta,omitempty"`
	DeltaFramebuffer            Rect      `json:"deltaFramebuffer,omitempty"`
	Scale                       float64   `json:"scale,omitempty"`
	ScaleKind                   string    `json:"scaleKind,omitempty"`
	FractionalScale             bool      `json:"fractionalScale,omitempty"`
	FramebufferRounding         string    `json:"framebufferRounding,omitempty"`
	MonitorTransform            int       `json:"monitorTransform,omitempty"`
	TransformSupported          bool      `json:"transformSupported,omitempty"`
	Warnings                    []string  `json:"warnings,omitempty"`
}

type SpaceRect struct {
	Space  string  `json:"space,omitempty"`
	X      float64 `json:"x"`
	Y      float64 `json:"y"`
	Width  float64 `json:"width"`
	Height float64 `json:"height"`
}

type SpacePoint struct {
	Space string  `json:"space,omitempty"`
	X     float64 `json:"x"`
	Y     float64 `json:"y"`
}

type EdgeLensBandStatus struct {
	Enabled         bool        `json:"enabled,omitempty"`
	DestinationRect SpaceRect   `json:"destinationRect,omitempty"`
	SourceRect      SpaceRect   `json:"sourceRect,omitempty"`
	OffsetPx        *SpacePoint `json:"offsetPx,omitempty"`
}

type UVCandidateSliceStatus struct {
	Index                int         `json:"index,omitempty"`
	CandidateName        string      `json:"candidateName,omitempty"`
	Formula              string      `json:"formula,omitempty"`
	InBounds             bool        `json:"inBounds,omitempty"`
	DestinationSliceRect SpaceRect   `json:"destinationSliceRect,omitempty"`
	SourceSliceRect      SpaceRect   `json:"sourceSliceRect,omitempty"`
	SourceUVTopLeft      *SpacePoint `json:"sourceUvTopLeft,omitempty"`
	SourceUVTopRight     *SpacePoint `json:"sourceUvTopRight,omitempty"`
	SourceUVBottomRight  *SpacePoint `json:"sourceUvBottomRight,omitempty"`
	SourceUVBottomLeft   *SpacePoint `json:"sourceUvBottomLeft,omitempty"`
}

type DebugOverlayStatus struct {
	Enabled                 bool     `json:"enabled"`
	RenderHookInstalled     bool     `json:"renderHookInstalled"`
	DrawableDescriptorCount int      `json:"drawableDescriptorCount,omitempty"`
	SkippedDescriptorCount  int      `json:"skippedDescriptorCount,omitempty"`
	LastRenderStatus        string   `json:"lastRenderStatus,omitempty"`
	Warnings                []string `json:"warnings,omitempty"`
}

type MaterialStatus struct {
	Enabled                 bool        `json:"enabled"`
	Mode                    string      `json:"mode,omitempty"`
	RenderHookInstalled     bool        `json:"renderHookInstalled"`
	RenderStage             string      `json:"renderStage,omitempty"`
	DrawableDescriptorCount int         `json:"drawableDescriptorCount,omitempty"`
	SkippedDescriptorCount  int         `json:"skippedDescriptorCount,omitempty"`
	ShaderDescriptorCount   int         `json:"shaderDescriptorCount,omitempty"`
	FallbackDescriptorCount int         `json:"fallbackDescriptorCount,omitempty"`
	LastRenderStatus        string      `json:"lastRenderStatus,omitempty"`
	ShaderCaptureTransforms []int       `json:"shaderCaptureSupportedTransforms,omitempty"`
	CaptureSamplingStatus   string      `json:"transformedCaptureSamplingStatus,omitempty"`
	FallbackBackend         string      `json:"fallbackBackend,omitempty"`
	NativeBlurEnabled       bool        `json:"nativeBlurEnabled,omitempty"`
	EffectiveBlurControl    string      `json:"effectiveBlurControl,omitempty"`
	PerSurfaceBlurSupported bool        `json:"perSurfaceBlurSupported,omitempty"`
	PerSurfaceBlurSupport   string      `json:"perSurfaceBlurSupport,omitempty"`
	CaptureEnabled          bool        `json:"captureEnabled,omitempty"`
	CaptureStage            string      `json:"captureStage,omitempty"`
	CaptureReady            bool        `json:"captureReady,omitempty"`
	CapturedMonitorCount    int         `json:"capturedMonitorCount,omitempty"`
	SampledDescriptorCount  int         `json:"sampledDescriptorCount,omitempty"`
	LastCaptureStatus       string      `json:"lastCaptureStatus,omitempty"`
	LastCaptureError        string      `json:"lastCaptureError,omitempty"`
	SelfSamplingRisk        bool        `json:"selfSamplingRisk,omitempty"`
	CaptureBackend          string      `json:"captureBackend,omitempty"`
	SourceCroppingSupported bool        `json:"sourceCroppingSupported,omitempty"`
	SourceCroppingMode      string      `json:"sourceCroppingMode,omitempty"`
	UVMappingType           string      `json:"uvMappingType,omitempty"`
	SourceMapDebugEnabled   bool        `json:"sourceMapDebugEnabled,omitempty"`
	CTexSupportsUVCorners   bool        `json:"ctexSupportsCustomUvCorners,omitempty"`
	CTexSupportsUVRect      bool        `json:"ctexSupportsAxisAlignedUvRect,omitempty"`
	CTexCanRotateSourceCrop bool        `json:"ctexCanRotateSourceCrop,omitempty"`
	CaptureBackedPolicy     string      `json:"captureBackedTransformPolicy,omitempty"`
	TransformSourceMapping  string      `json:"transformSourceMapping,omitempty"`
	SourceMappingEvidence   string      `json:"sourceMappingEvidence,omitempty"`
	FractionalSourceMapping string      `json:"fractionalSourceMapping,omitempty"`
	ShaderEnabled           bool        `json:"shaderEnabled,omitempty"`
	ShaderBackend           string      `json:"shaderBackend,omitempty"`
	ShaderCompiled          bool        `json:"shaderCompiled,omitempty"`
	ShaderReady             bool        `json:"shaderReady,omitempty"`
	ShaderError             string      `json:"shaderError,omitempty"`
	SDFMaskEnabled          bool        `json:"sdfMaskEnabled,omitempty"`
	RefractionDebugEnabled  bool        `json:"refractionDebugEnabled,omitempty"`
	UVOrientationEnabled    bool        `json:"uvOrientationEnabled,omitempty"`
	RoundedMaskEnabled      bool        `json:"roundedMaskEnabled,omitempty"`
	ImplementedTransforms   []int       `json:"implementedShaderTransforms,omitempty"`
	SupportedTransforms     []int       `json:"supportedShaderTransforms,omitempty"`
	FailedTransforms        []int       `json:"failedShaderTransforms,omitempty"`
	TargetTransforms        []int       `json:"targetShaderTransforms,omitempty"`
	CandidateCount          int         `json:"candidateCount,omitempty"`
	ActiveCandidateSet      string      `json:"activeCandidateSet,omitempty"`
	DisplacementStrengthPx  float64     `json:"displacementStrengthPx,omitempty"`
	DisplacementEnabled     bool        `json:"displacementEnabled,omitempty"`
	DisplacementOffsetPx    *SpacePoint `json:"displacementOffsetPx,omitempty"`
	EdgeLensEnabled         bool        `json:"edgeLensEnabled,omitempty"`
	EdgeLensBackend         string      `json:"edgeLensBackend,omitempty"`
	EdgeWidthPx             float64     `json:"edgeWidthPx,omitempty"`
	LensOffsetPx            float64     `json:"lensOffsetPx,omitempty"`
	TransformPolicy         string      `json:"transformPolicy,omitempty"`
	RefractionEnabled       bool        `json:"refractionEnabled,omitempty"`
	Warnings                []string    `json:"warnings,omitempty"`
}

type DebugOverlayDescriptorStatus struct {
	Drawable        bool      `json:"drawable"`
	Status          string    `json:"status,omitempty"`
	Reason          string    `json:"reason,omitempty"`
	RectUsed        SpaceRect `json:"rectUsed,omitempty"`
	SurfaceRectUsed SpaceRect `json:"surfaceRectUsed,omitempty"`
	GlobalRectUsed  SpaceRect `json:"globalRectUsed,omitempty"`
	DrawTransform   int       `json:"drawTransform,omitempty"`
	DrawSupported   bool      `json:"drawTransformSupported,omitempty"`
	DrawMapping     string    `json:"drawMapping,omitempty"`
	DrawWarnings    []string  `json:"drawWarnings,omitempty"`
	Mismatch        bool      `json:"mismatch,omitempty"`
	Warnings        []string  `json:"warnings,omitempty"`
}

type MaterialDescriptorStatus struct {
	Drawable                  bool                     `json:"drawable"`
	Status                    string                   `json:"status,omitempty"`
	Reason                    string                   `json:"reason,omitempty"`
	Mode                      string                   `json:"mode,omitempty"`
	RenderStage               string                   `json:"renderStage,omitempty"`
	RectUsed                  SpaceRect                `json:"rectUsed,omitempty"`
	GlobalRectUsed            SpaceRect                `json:"globalRectUsed,omitempty"`
	DrawTransform             int                      `json:"drawTransform,omitempty"`
	DrawSupported             bool                     `json:"drawTransformSupported,omitempty"`
	DrawMapping               string                   `json:"drawMapping,omitempty"`
	DrawWarnings              []string                 `json:"drawWarnings,omitempty"`
	Rounded                   bool                     `json:"rounded,omitempty"`
	RadiusRequested           float64                  `json:"radiusRequested,omitempty"`
	RadiusUsed                float64                  `json:"radiusUsed,omitempty"`
	Round                     int                      `json:"round,omitempty"`
	RequestedFrost            float64                  `json:"requestedFrost,omitempty"`
	BlurAlphaUsed             float64                  `json:"blurAlphaUsed,omitempty"`
	EffectiveBlurSource       string                   `json:"effectiveBlurSource,omitempty"`
	EffectiveBlurControl      string                   `json:"effectiveBlurControl,omitempty"`
	PerSurfaceBlurSupported   bool                     `json:"perSurfaceBlurSupported,omitempty"`
	PerSurfaceBlurSupport     string                   `json:"perSurfaceBlurSupport,omitempty"`
	RequestedTintColor        string                   `json:"requestedTintColor,omitempty"`
	RequestedTintOpacity      float64                  `json:"requestedTintOpacity,omitempty"`
	RequestedOpacity          float64                  `json:"requestedOpacity,omitempty"`
	TintColorRequested        string                   `json:"tintColorRequested,omitempty"`
	ColorUsed                 string                   `json:"colorUsed,omitempty"`
	OpacityRequested          float64                  `json:"opacityRequested,omitempty"`
	TintOpacityRequested      float64                  `json:"tintOpacityRequested,omitempty"`
	AlphaUsed                 float64                  `json:"alphaUsed,omitempty"`
	BlurEnabled               bool                     `json:"blurEnabled,omitempty"`
	BackendUsed               string                   `json:"backendUsed,omitempty"`
	FallbackReason            string                   `json:"fallbackReason,omitempty"`
	TransformCaptureSupported bool                     `json:"transformCaptureSupported,omitempty"`
	TintOverlayEnabled        bool                     `json:"tintOverlayEnabled,omitempty"`
	TintOverlayAlphaUsed      float64                  `json:"tintOverlayAlphaUsed,omitempty"`
	RimEnabled                bool                     `json:"rimEnabled,omitempty"`
	RimTechnique              string                   `json:"rimTechnique,omitempty"`
	RimAlphaUsed              float64                  `json:"rimAlphaUsed,omitempty"`
	RimExpansionPx            float64                  `json:"rimExpansionPx,omitempty"`
	RimColorUsed              string                   `json:"rimColorUsed,omitempty"`
	InnerEdgeEnabled          bool                     `json:"innerEdgeEnabled,omitempty"`
	InnerEdgeTechnique        string                   `json:"innerEdgeTechnique,omitempty"`
	InnerEdgeAlphaUsed        float64                  `json:"innerEdgeAlphaUsed,omitempty"`
	InnerEdgeInsetPx          float64                  `json:"innerEdgeInsetPx,omitempty"`
	InnerEdgeColorUsed        string                   `json:"innerEdgeColorUsed,omitempty"`
	HighlightEnabled          bool                     `json:"highlightEnabled,omitempty"`
	HighlightAlphaUsed        float64                  `json:"highlightAlphaUsed,omitempty"`
	HighlightTransformSafe    bool                     `json:"highlightTransformSafe,omitempty"`
	ShadowEnabled             bool                     `json:"shadowEnabled,omitempty"`
	ShadowAlphaUsed           float64                  `json:"shadowAlphaUsed,omitempty"`
	CaptureEnabled            bool                     `json:"captureEnabled,omitempty"`
	CaptureStage              string                   `json:"captureStage,omitempty"`
	CaptureReady              bool                     `json:"captureReady,omitempty"`
	CaptureStatus             string                   `json:"captureStatus,omitempty"`
	CaptureMonitor            string                   `json:"captureMonitor,omitempty"`
	CaptureBackend            string                   `json:"captureBackend,omitempty"`
	TextureReady              bool                     `json:"textureReady,omitempty"`
	TextureSize               SpaceRect                `json:"textureSize,omitempty"`
	SourceBackdropRect        SpaceRect                `json:"sourceBackdropRect,omitempty"`
	DestinationRect           SpaceRect                `json:"destinationRect,omitempty"`
	SourceExtent              SpaceRect                `json:"sourceExtent,omitempty"`
	SourceUVRect              SpaceRect                `json:"sourceUvRect,omitempty"`
	SourceUVTopLeft           *SpacePoint              `json:"sourceUvTopLeft,omitempty"`
	SourceUVTopRight          *SpacePoint              `json:"sourceUvTopRight,omitempty"`
	SourceUVBottomRight       *SpacePoint              `json:"sourceUvBottomRight,omitempty"`
	SourceUVBottomLeft        *SpacePoint              `json:"sourceUvBottomLeft,omitempty"`
	SourceMapping             string                   `json:"sourceMapping,omitempty"`
	SourceCroppingMode        string                   `json:"sourceCroppingMode,omitempty"`
	UVMappingType             string                   `json:"uvMappingType,omitempty"`
	TransformSourceMapping    string                   `json:"transformSourceMapping,omitempty"`
	SourceMappingEvidence     string                   `json:"sourceMappingEvidence,omitempty"`
	SourceMapDebugEnabled     bool                     `json:"sourceMapDebugEnabled,omitempty"`
	CTexSupportsUVCorners     bool                     `json:"ctexSupportsCustomUvCorners,omitempty"`
	CTexSupportsUVRect        bool                     `json:"ctexSupportsAxisAlignedUvRect,omitempty"`
	CTexCanRotateSourceCrop   bool                     `json:"ctexCanRotateSourceCrop,omitempty"`
	MonitorScale              float64                  `json:"monitorScale,omitempty"`
	Sampled                   bool                     `json:"sampled,omitempty"`
	SelfSamplingRisk          bool                     `json:"selfSamplingRisk,omitempty"`
	ShaderEnabled             bool                     `json:"shaderEnabled,omitempty"`
	ShaderBackend             string                   `json:"shaderBackend,omitempty"`
	ShaderCompiled            bool                     `json:"shaderCompiled,omitempty"`
	ShaderReady               bool                     `json:"shaderReady,omitempty"`
	ShaderError               string                   `json:"shaderError,omitempty"`
	SDFMaskEnabled            bool                     `json:"sdfMaskEnabled,omitempty"`
	SDFTechnique              string                   `json:"sdfTechnique,omitempty"`
	ShaderSourceMapTechnique  string                   `json:"shaderSourceMapTechnique,omitempty"`
	UVOrientationEnabled      bool                     `json:"uvOrientationEnabled,omitempty"`
	RefractionDebugEnabled    bool                     `json:"refractionDebugEnabled,omitempty"`
	DisplacementStrengthPx    float64                  `json:"displacementStrengthPx,omitempty"`
	TransformShaderSupported  bool                     `json:"transformShaderSupported,omitempty"`
	ShaderTransformSupported  bool                     `json:"shaderTransformSupported,omitempty"`
	ShaderTransformValidation string                   `json:"shaderTransformValidation,omitempty"`
	ImplementedTransforms     []int                    `json:"implementedShaderTransforms,omitempty"`
	SupportedTransforms       []int                    `json:"supportedShaderTransforms,omitempty"`
	FailedTransforms          []int                    `json:"failedShaderTransforms,omitempty"`
	TargetTransforms          []int                    `json:"targetShaderTransforms,omitempty"`
	CandidateCount            int                      `json:"candidateCount,omitempty"`
	ActiveCandidateSet        string                   `json:"activeCandidateSet,omitempty"`
	CandidateSlices           []UVCandidateSliceStatus `json:"candidateSlices,omitempty"`
	DisplacementEnabled       bool                     `json:"displacementEnabled,omitempty"`
	DisplacementOffsetPx      *SpacePoint              `json:"displacementOffsetPx,omitempty"`
	EdgeLensEnabled           bool                     `json:"edgeLensEnabled,omitempty"`
	EdgeLensSupported         bool                     `json:"edgeLensSupported,omitempty"`
	EdgeLensBackend           string                   `json:"edgeLensBackend,omitempty"`
	EdgeWidthPx               float64                  `json:"edgeWidthPx,omitempty"`
	LensOffsetPx              float64                  `json:"lensOffsetPx,omitempty"`
	CenterRegion              SpaceRect                `json:"centerRegion,omitempty"`
	TopBand                   EdgeLensBandStatus       `json:"topBand,omitempty"`
	BottomBand                EdgeLensBandStatus       `json:"bottomBand,omitempty"`
	LeftBand                  EdgeLensBandStatus       `json:"leftBand,omitempty"`
	RightBand                 EdgeLensBandStatus       `json:"rightBand,omitempty"`
	RoundedCornerStrategy     string                   `json:"roundedCornerStrategy,omitempty"`
	TransformPolicy           string                   `json:"transformPolicy,omitempty"`
	RoundedMaskEnabled        bool                     `json:"roundedMaskEnabled,omitempty"`
	RefractionEnabled         bool                     `json:"refractionEnabled,omitempty"`
	LastCaptureError          string                   `json:"lastCaptureError,omitempty"`
	PassCount                 int                      `json:"passCount,omitempty"`
	Warnings                  []string                 `json:"warnings,omitempty"`
}

var colorPattern = regexp.MustCompile(`^#(?:[0-9a-fA-F]{6}|[0-9a-fA-F]{8})$`)

func DecodeDescriptorSet(data []byte) (DescriptorSet, error) {
	data = bytes.TrimSpace(data)
	if len(data) == 0 {
		return DescriptorSet{}, errors.New("empty descriptor payload")
	}

	var envelope struct {
		Version     int             `json:"version"`
		Generation  uint64          `json:"generation"`
		Descriptors json.RawMessage `json:"descriptors"`
	}
	if err := json.Unmarshal(data, &envelope); err != nil {
		return DescriptorSet{}, err
	}

	var set DescriptorSet
	if len(envelope.Descriptors) > 0 {
		if err := json.Unmarshal(data, &set); err != nil {
			return DescriptorSet{}, err
		}
	} else {
		var descriptor Descriptor
		if err := json.Unmarshal(data, &descriptor); err != nil {
			return DescriptorSet{}, err
		}
		set = DescriptorSet{
			Version:    descriptor.Version,
			Generation: descriptor.Sequence,
			Descriptors: []Descriptor{
				descriptor,
			},
		}
	}

	set.Normalize()
	return set, nil
}

func (s *DescriptorSet) Normalize() {
	if s.Version == 0 {
		s.Version = DescriptorSchemaVersion
	}
	for i := range s.Descriptors {
		if s.Descriptors[i].Version == 0 {
			s.Descriptors[i].Version = s.Version
		}
		if s.Descriptors[i].Shape.Type == "" {
			s.Descriptors[i].Shape.Type = "rounded_rect"
		}
		if s.Descriptors[i].Material.Preset == "" {
			s.Descriptors[i].Material.Preset = "clear"
		}
		if s.Descriptors[i].Material.Tint.Mode == "" {
			s.Descriptors[i].Material.Tint.Mode = "neutral"
		}
	}
}

func (s DescriptorSet) Validate() error {
	var validation validationErrors

	if s.Version != DescriptorSchemaVersion {
		validation.add("version", "expected %d, got %d", DescriptorSchemaVersion, s.Version)
	}
	if len(s.Descriptors) == 0 {
		validation.add("descriptors", "at least one descriptor is required")
	}
	for i, descriptor := range s.Descriptors {
		prefix := fmt.Sprintf("descriptors[%d]", i)
		validateDescriptor(prefix, descriptor, &validation)
	}

	if validation.empty() {
		return nil
	}
	return validation
}

func (s DescriptorSet) JSON() ([]byte, error) {
	normalized := s
	normalized.Normalize()
	if err := normalized.Validate(); err != nil {
		return nil, err
	}
	return json.MarshalIndent(normalized, "", "  ")
}

func validateDescriptor(prefix string, descriptor Descriptor, validation *validationErrors) {
	if descriptor.Version != DescriptorSchemaVersion {
		validation.add(prefix+".version", "expected %d, got %d", DescriptorSchemaVersion, descriptor.Version)
	}
	if strings.TrimSpace(descriptor.ID) == "" {
		validation.add(prefix+".id", "is required")
	}
	if strings.TrimSpace(descriptor.Surface.Namespace) == "" {
		validation.add(prefix+".surface.namespace", "is required")
	}
	if strings.TrimSpace(descriptor.Surface.Monitor.Name) == "" && descriptor.Surface.Monitor.ID == nil {
		validation.add(prefix+".surface.monitor", "name or id is required")
	}
	validateRect(prefix+".geometry.logical", descriptor.Geometry.Logical, true, validation)
	validateOptionalRect(prefix+".geometry.monitor", descriptor.Geometry.Monitor, validation)
	validateOptionalRect(prefix+".geometry.framebuffer", descriptor.Geometry.Framebuffer, validation)
	if descriptor.Geometry.Scale != 0 && !positiveFinite(descriptor.Geometry.Scale) {
		validation.add(prefix+".geometry.scale", "must be a positive finite number when set")
	}
	validateShape(prefix+".shape", descriptor.Shape, validation)
	validateMaterial(prefix+".material", descriptor.Material, validation)
}

func validateRect(path string, rect Rect, required bool, validation *validationErrors) {
	if !required && rect == (Rect{}) {
		return
	}
	if !finite(rect.X) || !finite(rect.Y) || !finite(rect.Width) || !finite(rect.Height) {
		validation.add(path, "all values must be finite")
		return
	}
	if rect.Width <= 0 || rect.Height <= 0 {
		validation.add(path, "width and height must be greater than zero")
	}
}

func validateOptionalRect(path string, rect *Rect, validation *validationErrors) {
	if rect == nil {
		return
	}
	validateRect(path, *rect, true, validation)
}

func validateShape(path string, shape Shape, validation *validationErrors) {
	switch shape.Type {
	case "rounded_rect", "capsule", "circle":
	default:
		validation.add(path+".type", "must be rounded_rect, capsule, or circle")
	}
	validateNonNegative(path+".radius.topLeft", shape.Radius.TopLeft, validation)
	validateNonNegative(path+".radius.topRight", shape.Radius.TopRight, validation)
	validateNonNegative(path+".radius.bottomRight", shape.Radius.BottomRight, validation)
	validateNonNegative(path+".radius.bottomLeft", shape.Radius.BottomLeft, validation)
}

func validateMaterial(path string, material Material, validation *validationErrors) {
	switch material.Preset {
	case "clear", "regular", "tinted", "fallback":
	default:
		validation.add(path+".preset", "must be clear, regular, tinted, or fallback")
	}
	validateUnit(path+".opacity", material.Opacity, validation)
	validateUnit(path+".frost", material.Frost, validation)
	validateRange(path+".saturation", material.Saturation, 0, 2, validation)
	validateRange(path+".contrastBias", material.ContrastBias, -1, 1, validation)

	switch material.Tint.Mode {
	case "neutral", "manual", "theme", "wallpaper":
	default:
		validation.add(path+".tint.mode", "must be neutral, manual, theme, or wallpaper")
	}
	if material.Tint.Color != "" && !colorPattern.MatchString(material.Tint.Color) {
		validation.add(path+".tint.color", "must be #RRGGBB or #RRGGBBAA")
	}
	validateUnit(path+".tint.opacity", material.Tint.Opacity, validation)

	validateRange(path+".refraction.strength", material.Refraction.Strength, 0, 4, validation)
	validateRange(path+".refraction.edgeWidth", material.Refraction.EdgeWidth, 0, 128, validation)
	validateRange(path+".refraction.displacement", material.Refraction.Displacement, 0, 128, validation)
	validateRange(path+".refraction.chromaticAberration", material.Refraction.ChromaticAberration, 0, 1, validation)
	validateUnit(path+".rim.opacity", material.Rim.Opacity, validation)
	validateRange(path+".rim.width", material.Rim.Width, 0, 32, validation)
	validateUnit(path+".highlight.opacity", material.Highlight.Opacity, validation)
	validateFinite(path+".highlight.angle", material.Highlight.Angle, validation)
	validateRange(path+".highlight.spread", material.Highlight.Spread, 0, 360, validation)
	validateUnit(path+".reflection.opacity", material.Reflection.Opacity, validation)
	validateFinite(path+".reflection.angle", material.Reflection.Angle, validation)
	validateRange(path+".reflection.offset", material.Reflection.Offset, -256, 256, validation)
	validateRange(path+".reflection.blur", material.Reflection.Blur, 0, 128, validation)
	validateUnit(path+".shadow.innerOpacity", material.Shadow.InnerOpacity, validation)
	validateUnit(path+".shadow.outerOpacity", material.Shadow.OuterOpacity, validation)
	validateRange(path+".shadow.radius", material.Shadow.Radius, 0, 256, validation)
}

func validateUnit(path string, value float64, validation *validationErrors) {
	validateRange(path, value, 0, 1, validation)
}

func validateNonNegative(path string, value float64, validation *validationErrors) {
	validateRange(path, value, 0, math.MaxFloat64, validation)
}

func validateFinite(path string, value float64, validation *validationErrors) {
	if !finite(value) {
		validation.add(path, "must be finite")
	}
}

func validateRange(path string, value, min, max float64, validation *validationErrors) {
	if !finite(value) || value < min || value > max {
		validation.add(path, "must be between %g and %g", min, max)
	}
}

func finite(value float64) bool {
	return !math.IsNaN(value) && !math.IsInf(value, 0)
}

func positiveFinite(value float64) bool {
	return finite(value) && value > 0
}

type validationErrors []string

func (v *validationErrors) add(path, format string, args ...any) {
	*v = append(*v, fmt.Sprintf("%s: %s", path, fmt.Sprintf(format, args...)))
}

func (v validationErrors) empty() bool {
	return len(v) == 0
}

func (v validationErrors) Error() string {
	return strings.Join(v, "\n")
}
