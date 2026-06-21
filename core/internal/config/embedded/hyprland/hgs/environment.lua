-- Environment Variable Configuration

-- Warning: Do not manually edit configuration files
-- Configuration files are maintained by HGS settings
-- Add manual configurations to ~/.config/hypr/hgs/custom.lua

local hgs_home = os.getenv("HOME") or ""
local hgs_path = os.getenv("PATH") or ""
local hgs_user_bin = hgs_home ~= "" and (hgs_home .. "/.local/bin") or ""

if hgs_user_bin ~= "" and not string.find(":" .. hgs_path .. ":", ":" .. hgs_user_bin .. ":", 1, true) then
    hgs_path = hgs_user_bin .. ":" .. hgs_path
end

if hgs_path ~= "" then
    hl.env("PATH", hgs_path)
end

hl.env("XCURSOR_SIZE", "24")
hl.env("HYPRCURSOR_SIZE", "24")
