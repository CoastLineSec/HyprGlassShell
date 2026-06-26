.pragma library

// Multi-provider VPN status adapters. The UNIVERSAL detection (tunnel interfaces via
// networkd) lives in the QML; this module only adds rich per-provider detail by
// probing whichever provider CLI is installed and parsing its `status` output into a
// common shape: { provider, connected, status, rows: [{label, value}] } (or null).
//
// Verified: NordVPN. The others are best-effort from each CLI's documented format and
// are easy to correct in one place when a real sample comes in. Each parser only runs
// when that CLI is actually installed, so it never affects users without it.

// One shell probe: detect the installed CLI (first match wins) and emit
// "PROVIDER=<id>" followed by that CLI's raw status output.
function probeScript() {
    return [
        'if command -v nordvpn >/dev/null 2>&1; then echo "PROVIDER=nordvpn"; nordvpn status 2>/dev/null;',
        'elif command -v mullvad >/dev/null 2>&1; then echo "PROVIDER=mullvad"; mullvad status 2>/dev/null;',
        'elif command -v protonvpn-cli >/dev/null 2>&1; then echo "PROVIDER=protonvpn"; protonvpn-cli status 2>/dev/null;',
        'elif command -v protonvpn >/dev/null 2>&1; then echo "PROVIDER=protonvpn"; protonvpn status 2>/dev/null;',
        'elif command -v expressvpn >/dev/null 2>&1; then echo "PROVIDER=expressvpn"; expressvpn status 2>/dev/null;',
        'elif command -v cyberghostvpn >/dev/null 2>&1; then echo "PROVIDER=cyberghost"; cyberghostvpn --status 2>/dev/null;',
        'elif command -v piactl >/dev/null 2>&1; then echo "PROVIDER=pia";',
        '  echo "connectionstate: $(piactl get connectionstate 2>/dev/null)";',
        '  echo "region: $(piactl get region 2>/dev/null)";',
        '  echo "vpnip: $(piactl get vpnip 2>/dev/null)";',
        '  echo "pubip: $(piactl get pubip 2>/dev/null)";',
        'elif command -v surfshark-vpn >/dev/null 2>&1; then echo "PROVIDER=surfshark"; surfshark-vpn status 2>/dev/null;',
        'fi'
    ].join(' ');
}

function _row(label, value) {
    return {
        "label": label,
        "value": value
    };
}

function _kv(lines) {
    var o = {};
    for (var i = 0; i < lines.length; i++) {
        var idx = lines[i].indexOf(":");
        if (idx > 0)
            o[lines[i].substring(0, idx).trim().toLowerCase()] = lines[i].substring(idx + 1).trim();
    }
    return o;
}

function parse(raw) {
    if (!raw)
        return null;
    var lines = raw.split("\n").map(function (l) {
        return l.trim();
    }).filter(function (l) {
        return l.length > 0;
    });
    if (lines.length === 0)
        return null;

    var provider = "";
    if (lines[0].indexOf("PROVIDER=") === 0) {
        provider = lines[0].substring(9).trim();
        lines = lines.slice(1);
    }

    switch (provider) {
    case "nordvpn":
        return _nord(lines);
    case "mullvad":
        return _mullvad(lines);
    case "protonvpn":
        return _proton(lines);
    case "expressvpn":
        return _express(lines);
    case "cyberghost":
        return _cyberghost(lines);
    case "pia":
        return _pia(lines);
    case "surfshark":
        return _surfshark(lines);
    default:
        return null;
    }
}

// --- NordVPN (verified) ---
function _nord(lines) {
    var k = _kv(lines);
    var connected = (k["status"] || "").toLowerCase() === "connected";
    var loc = [k["city"], k["country"]].filter(Boolean).join(", ");
    var rows = [];
    if (k["server"])
        rows.push(_row("Server", k["server"] + (loc ? "  ·  " + loc : "")));
    if (k["ip"])
        rows.push(_row("IP", k["ip"]));
    var tech = [k["current technology"], k["current protocol"]].filter(Boolean).join(" / ");
    if (tech)
        rows.push(_row("Protocol", tech));
    if (k["uptime"])
        rows.push(_row("Uptime", k["uptime"]));
    if (k["transfer"])
        rows.push(_row("Transfer", k["transfer"]));
    return {
        "provider": "NordVPN",
        "connected": connected,
        "status": k["status"] || (connected ? "Connected" : "Disconnected"),
        "rows": rows
    };
}

// --- Mullvad (best-effort) ---
function _mullvad(lines) {
    var first = lines[0] || "";
    var connected = /^connect/i.test(first);
    var status = first.split(" ")[0] || (connected ? "Connected" : "Disconnected");
    var relay = "";
    var loc = "";
    var ip = "";
    for (var i = 0; i < lines.length; i++) {
        var ln = lines[i];
        var m;
        if ((m = ln.match(/^Relay:\s*(.+)$/i)))
            relay = m[1];
        if ((m = ln.match(/appears to be from:\s*([^.]+)\.?\s*(?:IP:\s*([0-9a-fA-F:.]+))?/i))) {
            loc = m[1].trim();
            if (m[2])
                ip = m[2];
        }
        if ((m = ln.match(/^Connected to\s+(\S+)\s+in\s+(.+)$/i))) {
            relay = m[1];
            loc = m[2];
        }
    }
    var rows = [];
    if (relay)
        rows.push(_row("Relay", relay));
    if (loc)
        rows.push(_row("Location", loc));
    if (ip)
        rows.push(_row("IP", ip));
    return {
        "provider": "Mullvad",
        "connected": connected,
        "status": status,
        "rows": rows
    };
}

// --- Proton VPN (best-effort) ---
function _proton(lines) {
    var k = _kv(lines);
    var statusVal = k["status"] || "";
    var connected = /connected/i.test(statusVal) && !/disconnected/i.test(statusVal);
    if (!statusVal)
        connected = lines.some(function (l) {
            return /\bconnected\b/i.test(l) && !/disconnected/i.test(l);
        });
    var rows = [];
    if (k["server"])
        rows.push(_row("Server", k["server"]));
    if (k["country"])
        rows.push(_row("Country", k["country"]));
    if (k["protocol"])
        rows.push(_row("Protocol", k["protocol"]));
    if (k["ip"])
        rows.push(_row("IP", k["ip"]));
    if (k["server load"])
        rows.push(_row("Server load", k["server load"]));
    if (k["kill switch"])
        rows.push(_row("Kill switch", k["kill switch"]));
    return {
        "provider": "Proton VPN",
        "connected": connected,
        "status": connected ? "Connected" : (statusVal || "Disconnected"),
        "rows": rows
    };
}

// --- ExpressVPN (best-effort) ---
function _express(lines) {
    var joined = lines.join(" ");
    var connected = /\bconnected to\b/i.test(joined) && !/not connected/i.test(joined);
    var rows = [];
    var m = joined.match(/Connected to\s+([^.]+?)(?:\.|$)/i);
    if (m)
        rows.push(_row("Location", m[1].trim()));
    return {
        "provider": "ExpressVPN",
        "connected": connected,
        "status": connected ? "Connected" : "Not connected",
        "rows": rows
    };
}

// --- CyberGhost (best-effort) ---
function _cyberghost(lines) {
    var joined = lines.join("\n");
    var connected = /connected/i.test(joined) && !/no vpn connection|not connected|disconnected/i.test(joined);
    var rows = [];
    for (var i = 0; i < lines.length; i++) {
        var m = lines[i].match(/^(country|city|server|ip|protocol):\s*(.+)$/i);
        if (m)
            rows.push(_row(m[1], m[2]));
    }
    return {
        "provider": "CyberGhost",
        "connected": connected,
        "status": connected ? "Connected" : "Disconnected",
        "rows": rows
    };
}

// --- Private Internet Access (piactl) ---
function _pia(lines) {
    var k = _kv(lines);
    var connected = (k["connectionstate"] || "").toLowerCase() === "connected";
    var rows = [];
    if (k["region"])
        rows.push(_row("Region", k["region"]));
    if (k["vpnip"])
        rows.push(_row("VPN IP", k["vpnip"]));
    if (k["pubip"])
        rows.push(_row("Public IP", k["pubip"]));
    return {
        "provider": "Private Internet Access",
        "connected": connected,
        "status": k["connectionstate"] || "Disconnected",
        "rows": rows
    };
}

// --- Surfshark (best-effort) ---
function _surfshark(lines) {
    var joined = lines.join("\n");
    var connected = /connected/i.test(joined) && !/not connected|disconnected|no connection/i.test(joined);
    var rows = [];
    for (var i = 0; i < lines.length; i++) {
        var m = lines[i].match(/^(country|city|server|ip|location):\s*(.+)$/i);
        if (m)
            rows.push(_row(m[1], m[2]));
    }
    return {
        "provider": "Surfshark",
        "connected": connected,
        "status": connected ? "Connected" : "Disconnected",
        "rows": rows
    };
}
