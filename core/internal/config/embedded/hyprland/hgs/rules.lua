-- Rules Configuration

-- Warning: Do not manually edit configuration files
-- Configuration files are maintained by HGS settings
-- Add manual configurations to ~/.config/hypr/hgs/custom.lua

hl.window_rule({
    name  = "fix-xwayland-drags",
    match = {
        class      = "^$",
        title      = "^$",
        xwayland   = true,
        float      = true,
        fullscreen = false,
        pin        = false,
    },

    no_focus = true,
})

