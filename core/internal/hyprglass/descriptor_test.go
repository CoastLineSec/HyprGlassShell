package hyprglass

import (
	"strings"
	"testing"
)

func TestDecodeDescriptorSetValidatesNestedContract(t *testing.T) {
	payload := []byte(`{
		"version": 1,
		"generation": 42,
		"descriptors": [{
			"id": "bar/main/left",
			"kind": "bar-pill",
			"surface": {
				"namespace": "hgs-bar",
				"layer": "top",
				"role": "bar",
				"monitor": {"name": "DP-2"}
			},
			"geometry": {
				"logical": {"x": 12, "y": 8, "width": 280, "height": 40},
				"monitor": {"x": 12, "y": 8, "width": 280, "height": 40},
				"framebuffer": {"x": 24, "y": 16, "width": 560, "height": 80},
				"scale": 2
			},
			"shape": {
				"type": "capsule",
				"radius": {"topLeft": 20, "topRight": 20, "bottomRight": 20, "bottomLeft": 20}
			},
			"material": {
				"enabled": true,
				"preset": "clear",
				"opacity": 0.28,
				"frost": 0.1,
				"saturation": 1.2,
				"contrastBias": 0.1,
				"tint": {"mode": "theme", "color": "#bda6ff", "opacity": 0.2},
				"refraction": {"strength": 0.6, "edgeWidth": 18, "displacement": 20, "chromaticAberration": 0.08},
				"rim": {"opacity": 0.45, "width": 1.5},
				"highlight": {"opacity": 0.32, "angle": 315, "spread": 80},
				"reflection": {"opacity": 0.22, "angle": 30, "offset": 12, "blur": 16},
				"shadow": {"innerOpacity": 0.18, "outerOpacity": 0.24, "radius": 42},
				"suppressFullscreen": true
			},
			"debug": {"name": "main bar", "showBounds": true},
			"sequence": 9
		}]
	}`)

	set, err := DecodeDescriptorSet(payload)
	if err != nil {
		t.Fatalf("DecodeDescriptorSet returned error: %v", err)
	}
	if err := set.Validate(); err != nil {
		t.Fatalf("Validate returned error: %v", err)
	}

	if set.Generation != 42 {
		t.Fatalf("generation = %d, want 42", set.Generation)
	}
	if got := set.Descriptors[0].Surface.Monitor.Name; got != "DP-2" {
		t.Fatalf("monitor name = %q, want DP-2", got)
	}
}

func TestDecodeDescriptorSetAcceptsSingleDescriptor(t *testing.T) {
	payload := []byte(`{
		"version": 1,
		"id": "modal/settings",
		"surface": {
			"namespace": "hgs-settings",
			"monitor": {"name": "eDP-1"}
		},
		"geometry": {
			"logical": {"x": 100, "y": 100, "width": 800, "height": 600}
		},
		"shape": {
			"type": "rounded_rect",
			"radius": {"topLeft": 24, "topRight": 24, "bottomRight": 24, "bottomLeft": 24}
		},
		"material": {
			"enabled": true,
			"preset": "regular",
			"opacity": 0.4,
			"frost": 0.35,
			"saturation": 1,
			"contrastBias": 0,
			"tint": {"mode": "neutral", "opacity": 0},
			"refraction": {"strength": 0.4, "edgeWidth": 12, "displacement": 12, "chromaticAberration": 0},
			"rim": {"opacity": 0.4, "width": 1},
			"highlight": {"opacity": 0.3, "angle": 315, "spread": 60},
			"reflection": {"opacity": 0.2, "angle": 0, "offset": 0, "blur": 12},
			"shadow": {"innerOpacity": 0.1, "outerOpacity": 0.2, "radius": 32}
		},
		"sequence": 7
	}`)

	set, err := DecodeDescriptorSet(payload)
	if err != nil {
		t.Fatalf("DecodeDescriptorSet returned error: %v", err)
	}
	if len(set.Descriptors) != 1 {
		t.Fatalf("descriptor count = %d, want 1", len(set.Descriptors))
	}
	if set.Generation != 7 {
		t.Fatalf("generation = %d, want 7", set.Generation)
	}
	if err := set.Validate(); err != nil {
		t.Fatalf("Validate returned error: %v", err)
	}
}

func TestValidateRejectsUnsafeDescriptor(t *testing.T) {
	set := DescriptorSet{
		Version: DescriptorSchemaVersion,
		Descriptors: []Descriptor{
			{
				ID: "broken",
				Surface: SurfaceRef{
					Namespace: "hgs",
					Monitor:   Monitor{Name: "DP-2"},
				},
				Geometry: Geometry{
					Logical: Rect{Width: 0, Height: 20},
				},
				Shape: Shape{
					Type: "triangle",
				},
				Material: Material{
					Preset:  "clear",
					Opacity: 2,
					Frost:   0,
					Tint: Tint{
						Mode:    "manual",
						Color:   "purple",
						Opacity: 0.2,
					},
				},
			},
		},
	}
	set.Normalize()

	err := set.Validate()
	if err == nil {
		t.Fatal("Validate returned nil, want validation errors")
	}
	for _, want := range []string{
		"geometry.logical",
		"shape.type",
		"material.opacity",
		"material.tint.color",
	} {
		if !strings.Contains(err.Error(), want) {
			t.Fatalf("error %q does not contain %q", err.Error(), want)
		}
	}
}
