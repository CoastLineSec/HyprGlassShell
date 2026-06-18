-- Minimal Hyprland (Lua) session for greetd — replace _HGS_PATH_ with your HGS checkout.
-- Copy to `/etc/greetd/hgs-hypr.lua` alongside `greet-hyprland.sh`.

hl.env("HGS_RUN_GREETER", "1")

hl.on("hyprland.start", function()
	hl.exec_cmd('sh -c "qs -p _HGS_PATH_; hyprctl dispatch exit"')
end)
