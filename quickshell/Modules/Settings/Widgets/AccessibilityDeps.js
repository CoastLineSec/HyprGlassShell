.pragma library

// Accessibility features that rely on an external program. Package names vary
// by distribution, so each tool maps a detected distro family to its package.
var TOOLS = {
    "orca": {
        name: "Orca screen reader",
        bin: "orca",
        pkg: {
            "arch": "orca",
            "debian": "gnome-orca",
            "fedora": "orca",
            "suse": "orca",
            "default": "orca"
        }
    },
    "osk": {
        name: "on-screen keyboard",
        bin: "wvkbd-mobintl",
        pkg: {
            "arch": "wvkbd",
            "debian": "wvkbd",
            "fedora": "wvkbd",
            "suse": "wvkbd",
            "default": "wvkbd (or squeekboard)"
        }
    }
};

function binary(key) {
    return TOOLS[key] ? TOOLS[key].bin : "";
}

// Map an os-release "ID ID_LIKE" line to a package family.
function family(idLine) {
    var s = (idLine || "").toLowerCase();
    if (s.indexOf("arch") !== -1 || s.indexOf("manjaro") !== -1 || s.indexOf("endeavour") !== -1)
        return "arch";
    if (s.indexOf("debian") !== -1 || s.indexOf("ubuntu") !== -1 || s.indexOf("mint") !== -1 || s.indexOf("pop") !== -1)
        return "debian";
    if (s.indexOf("fedora") !== -1 || s.indexOf("rhel") !== -1 || s.indexOf("centos") !== -1 || s.indexOf("nobara") !== -1)
        return "fedora";
    if (s.indexOf("suse") !== -1)
        return "suse";
    return "default";
}

function packageName(key, fam) {
    var t = TOOLS[key];
    if (!t)
        return "";
    return t.pkg[fam] || t.pkg["default"];
}

function message(key, fam) {
    var t = TOOLS[key];
    if (!t)
        return "";
    return "Dependency missing: " + t.name + ". Install the “" + packageName(key, fam) + "” package to use this setting.";
}
