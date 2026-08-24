.pragma library

function emptySnapshot() {
    return {
        revision: 0,
        configProvider: "unknown",
        usingLua: false,
        monitors: [],
        workspaces: [],
        clients: []
    };
}

function isObject(value) {
    return value !== null
        && typeof value === "object"
        && !Array.isArray(value);
}

function isInteger(value) {
    return typeof value === "number"
        && Number.isFinite(value)
        && Math.floor(value) === value;
}

function normalizeAddress(value) {
    if (typeof value !== "string")
        return "";

    const digits = value.trim().replace(/^0x/i, "");
    if (!/^[0-9a-fA-F]{1,16}$/.test(digits))
        return "";
    return "0x" + digits.toLowerCase();
}

function parseArray(payload, label) {
    let value;
    try {
        value = JSON.parse(String(payload || ""));
    } catch (error) {
        return {
            ok: false,
            error: "Hyprland returned malformed " + label + " JSON."
        };
    }

    if (!Array.isArray(value)) {
        return {
            ok: false,
            error: "Hyprland " + label + " response is not an array."
        };
    }
    return { ok: true, value: value };
}

function completeArrayPayload(payload) {
    try {
        return Array.isArray(JSON.parse(String(payload || "")));
    } catch (error) {
        return false;
    }
}

function parseStatus(payload) {
    const text = String(payload || "").trim();
    if (text === "unknown request") {
        return {
            ok: true,
            configProvider: "legacy",
            usingLua: false
        };
    }

    let value;
    try {
        value = JSON.parse(text);
    } catch (error) {
        return {
            ok: false,
            error: "Hyprland returned malformed status JSON."
        };
    }

    if (!isObject(value)
            || typeof value.configProvider !== "string"
            || (value.configProvider !== "lua"
                && value.configProvider !== "hyprlang")) {
        return {
            ok: false,
            error: "Hyprland returned an unknown configuration provider."
        };
    }

    return {
        ok: true,
        configProvider: value.configProvider,
        usingLua: value.configProvider === "lua"
    };
}

function completeResponse(command, payload) {
    if (command === "j/status")
        return parseStatus(payload).ok;
    return completeArrayPayload(payload);
}

function normalizeMonitors(rawMonitors) {
    const monitors = [];
    const names = {};
    let skipped = 0;

    for (let index = 0; index < rawMonitors.length; ++index) {
        const monitor = rawMonitors[index];
        const activeWorkspace = isObject(monitor)
            ? monitor.activeWorkspace
            : null;
        const specialWorkspace = isObject(monitor)
            ? monitor.specialWorkspace
            : null;
        const specialWorkspaceActive = isObject(specialWorkspace)
            && specialWorkspace.id !== 0;
        if (!isObject(monitor)
                || !isInteger(monitor.id)
                || typeof monitor.name !== "string"
                || monitor.name.length === 0
                || names[monitor.name]
                || typeof monitor.focused !== "boolean"
                || !isObject(activeWorkspace)
                || !isInteger(activeWorkspace.id)
                || typeof activeWorkspace.name !== "string"
                || activeWorkspace.name.length === 0
                || !isObject(specialWorkspace)
                || !isInteger(specialWorkspace.id)
                || typeof specialWorkspace.name !== "string"
                || (specialWorkspaceActive
                    && (specialWorkspace.name.length === 0
                        || specialWorkspace.name.indexOf("special:") !== 0))
                || (!specialWorkspaceActive
                    && specialWorkspace.name.length !== 0)) {
            ++skipped;
            continue;
        }

        names[monitor.name] = true;
        monitors.push({
            id: monitor.id,
            name: monitor.name,
            focused: monitor.focused,
            activeWorkspaceId: activeWorkspace.id,
            activeWorkspaceName: activeWorkspace.name,
            specialWorkspaceId: specialWorkspace.id,
            specialWorkspaceName: specialWorkspace.name,
            visibleWorkspaceId: specialWorkspaceActive
                ? specialWorkspace.id
                : activeWorkspace.id,
            visibleWorkspaceName: specialWorkspaceActive
                ? specialWorkspace.name
                : activeWorkspace.name
        });
    }

    return { values: monitors, skipped: skipped };
}

function normalizeWorkspaces(rawWorkspaces) {
    const workspaces = [];
    const ids = {};
    let skipped = 0;

    for (let index = 0; index < rawWorkspaces.length; ++index) {
        const workspace = rawWorkspaces[index];
        const id = isObject(workspace) ? workspace.id : null;
        const idKey = String(id);
        if (!isObject(workspace)
                || !isInteger(id)
                || ids[idKey]
                || typeof workspace.name !== "string"
                || workspace.name.length === 0
                || typeof workspace.monitor !== "string"
                || workspace.monitor.length === 0
                || !isInteger(workspace.monitorID)
                || !isInteger(workspace.windows)
                || workspace.windows < 0
                || typeof workspace.hasfullscreen !== "boolean"
                || typeof workspace.ispersistent !== "boolean") {
            ++skipped;
            continue;
        }

        ids[idKey] = true;
        workspaces.push({
            id: id,
            name: workspace.name,
            monitorName: workspace.monitor,
            monitorId: workspace.monitorID,
            windowCount: workspace.windows,
            hasFullscreen: workspace.hasfullscreen,
            persistent: workspace.ispersistent,
            lastWindowAddress: normalizeAddress(workspace.lastwindow),
            urgent: false
        });
    }

    return { values: workspaces, skipped: skipped };
}

function normalizeClients(rawClients) {
    const clients = [];
    const addresses = {};
    let skipped = 0;

    for (let index = 0; index < rawClients.length; ++index) {
        const client = rawClients[index];
        const workspace = isObject(client) ? client.workspace : null;
        const address = isObject(client)
            ? normalizeAddress(client.address)
            : "";
        if (!isObject(client)
                || address.length === 0
                || addresses[address]
                || !isObject(workspace)
                || !isInteger(workspace.id)
                || typeof workspace.name !== "string"
                || !isInteger(client.monitor)
                || typeof client.mapped !== "boolean"
                || typeof client.hidden !== "boolean"
                || typeof client.visible !== "boolean"
                || typeof client.floating !== "boolean"
                || !isInteger(client.fullscreen)
                || client.fullscreen < 0
                || client.fullscreen > 2
                || typeof client.fullscreenHandler !== "string"
                || typeof client.class !== "string"
                || typeof client.initialClass !== "string"
                || !isInteger(client.focusHistoryID)
                || client.focusHistoryID < -1) {
            ++skipped;
            continue;
        }

        addresses[address] = true;
        clients.push({
            address: address,
            workspaceId: workspace.id,
            workspaceName: workspace.name,
            monitorId: client.monitor,
            applicationId: client.class || client.initialClass,
            initialApplicationId: client.initialClass,
            title: typeof client.title === "string" ? client.title : "",
            mapped: client.mapped,
            hidden: client.hidden,
            visible: client.visible,
            floating: client.floating,
            fullscreenMode: client.fullscreen,
            fullscreenHandler: client.fullscreenHandler,
            focusHistoryId: client.focusHistoryID,
            active: client.focusHistoryID === 0,
            urgent: false
        });
    }

    return { values: clients, skipped: skipped };
}

function copyAddressSet(addresses) {
    const copy = {};
    if (!isObject(addresses))
        return copy;

    const keys = Object.keys(addresses);
    for (let index = 0; index < keys.length; ++index) {
        const address = normalizeAddress(keys[index]);
        if (address.length > 0 && addresses[keys[index]]) {
            const generations = Number(addresses[keys[index]]);
            copy[address] = Number.isInteger(generations) && generations > 0
                ? generations
                : 3;
        }
    }
    return copy;
}

function clearWorkspaceUrgency(addresses, snapshot, workspaceId, name) {
    const hasId = isInteger(workspaceId);
    const workspaceName = String(name || "");
    for (let index = 0; index < snapshot.clients.length; ++index) {
        const client = snapshot.clients[index];
        if ((hasId && client.workspaceId === workspaceId)
                || (!hasId
                    && workspaceName.length > 0
                    && client.workspaceName === workspaceName)) {
            delete addresses[client.address];
        }
    }
}

function reconcileUrgency(addresses, snapshot) {
    const reconciled = copyAddressSet(addresses);
    const currentAddresses = {};
    let focusedWorkspaceId = null;

    for (let index = 0; index < snapshot.monitors.length; ++index) {
        const monitor = snapshot.monitors[index];
        if (monitor.focused) {
            focusedWorkspaceId = monitor.activeWorkspaceId;
            break;
        }
    }

    for (let index = 0; index < snapshot.clients.length; ++index) {
        const client = snapshot.clients[index];
        currentAddresses[client.address] = true;
        if (client.active || client.workspaceId === focusedWorkspaceId)
            delete reconciled[client.address];
    }

    const urgentAddresses = Object.keys(reconciled);
    for (let index = 0; index < urgentAddresses.length; ++index) {
        const address = urgentAddresses[index];
        if (currentAddresses[address])
            continue;
        const remaining = Number(reconciled[address]);
        if (remaining > 1)
            reconciled[address] = remaining - 1;
        else
            delete reconciled[address];
    }
    return reconciled;
}

function applyUrgency(snapshot, addresses) {
    const urgentWorkspaces = {};
    const clients = snapshot.clients.map(client => {
        const urgent = Boolean(addresses[client.address]);
        if (urgent)
            urgentWorkspaces[String(client.workspaceId)] = true;
        return Object.assign({}, client, { urgent: urgent });
    });
    const workspaces = snapshot.workspaces.map(workspace => {
        return Object.assign({}, workspace, {
            urgent: Boolean(urgentWorkspaces[String(workspace.id)])
        });
    });
    return {
        revision: snapshot.revision,
        monitors: snapshot.monitors,
        workspaces: workspaces,
        clients: clients
    };
}

function buildSnapshot(
    workspacePayload,
    monitorPayload,
    clientPayload,
    urgentAddresses,
    revision
) {
    const parsedWorkspaces = parseArray(workspacePayload, "workspace");
    if (!parsedWorkspaces.ok)
        return parsedWorkspaces;
    const parsedMonitors = parseArray(monitorPayload, "monitor");
    if (!parsedMonitors.ok)
        return parsedMonitors;
    const parsedClients = parseArray(clientPayload, "client");
    if (!parsedClients.ok)
        return parsedClients;

    const monitors = normalizeMonitors(parsedMonitors.value);
    const workspaces = normalizeWorkspaces(parsedWorkspaces.value);
    const clients = normalizeClients(parsedClients.value);
    const skipped = monitors.skipped + workspaces.skipped + clients.skipped;
    if (skipped > 0) {
        return {
            ok: false,
            error: "Hyprland returned incomplete workspace state."
        };
    }
    if (monitors.values.length === 0) {
        return {
            ok: false,
            error: "Hyprland returned no valid monitors."
        };
    }

    const monitorsByName = {};
    const workspacesById = {};
    for (let index = 0; index < monitors.values.length; ++index)
        monitorsByName[monitors.values[index].name] = monitors.values[index];
    for (let index = 0; index < workspaces.values.length; ++index) {
        const workspace = workspaces.values[index];
        const monitor = monitorsByName[workspace.monitorName];
        if (!monitor || monitor.id !== workspace.monitorId) {
            return {
                ok: false,
                error: "Hyprland returned a workspace without its monitor."
            };
        }
        workspacesById[String(workspace.id)] = workspace;
    }
    for (let index = 0; index < monitors.values.length; ++index) {
        const monitor = monitors.values[index];
        const active = workspacesById[String(monitor.activeWorkspaceId)];
        if (!active
                || active.monitorName !== monitor.name
                || active.name !== monitor.activeWorkspaceName) {
            return {
                ok: false,
                error: "Hyprland returned incomplete active workspace state."
            };
        }
        if (monitor.specialWorkspaceId !== 0) {
            const special = workspacesById[
                String(monitor.specialWorkspaceId)
            ];
            if (!special
                    || special.monitorName !== monitor.name
                    || special.name !== monitor.specialWorkspaceName) {
                return {
                    ok: false,
                    error: "Hyprland returned incomplete special workspace state."
                };
            }
        }
    }
    for (let index = 0; index < clients.values.length; ++index) {
        const client = clients.values[index];
        const workspace = workspacesById[String(client.workspaceId)];
        if (!workspace
                || (client.mapped
                    && (client.workspaceName !== workspace.name
                        || client.monitorId !== workspace.monitorId))) {
            return {
                ok: false,
                error: "Hyprland returned a client outside its workspace."
            };
        }
        if (client.fullscreenMode !== 0
                && client.fullscreenHandler !== "default"
                && client.fullscreenHandler !== "scrolling") {
            return {
                ok: false,
                error: "Hyprland returned an unknown fullscreen handler."
            };
        }
        if (client.fullscreenMode !== 0
                && client.floating
                && client.fullscreenHandler !== "default") {
            return {
                ok: false,
                error: "Hyprland returned an inconsistent fullscreen handler."
            };
        }
    }
    const normalized = {
        revision: revision,
        monitors: monitors.values,
        workspaces: workspaces.values,
        clients: clients.values
    };
    const reconciledUrgency = reconcileUrgency(
        urgentAddresses,
        normalized
    );

    return {
        ok: true,
        snapshot: applyUrgency(normalized, reconciledUrgency),
        urgentAddresses: reconciledUrgency,
        skipped: 0
    };
}

const relevantEvents = {
    workspace: true,
    workspacev2: true,
    focusedmon: true,
    focusedmonv2: true,
    monitoradded: true,
    monitoraddedv2: true,
    monitorremoved: true,
    monitorremovedv2: true,
    createworkspace: true,
    createworkspacev2: true,
    destroyworkspace: true,
    destroyworkspacev2: true,
    moveworkspace: true,
    moveworkspacev2: true,
    renameworkspace: true,
    changeworkspaceid: true,
    openwindow: true,
    closewindow: true,
    movewindow: true,
    movewindowv2: true,
    changefloatingmode: true,
    fullscreen: true,
    activespecial: true,
    activespecialv2: true,
    minimized: true,
    pin: true,
    togglegroup: true,
    moveintogroup: true,
    moveoutofgroup: true,
    activewindow: true,
    activewindowv2: true,
    urgent: true,
    configreloaded: true
};

function parseEvent(line) {
    const text = String(line || "").replace(/\r$/, "");
    const separator = text.indexOf(">>");
    if (separator < 0) {
        const name = text.trim();
        return {
            valid: false,
            relevant: Boolean(relevantEvents[name]),
            name: name,
            data: ""
        };
    }

    const name = text.slice(0, separator);
    return {
        valid: name.length > 0,
        relevant: Boolean(relevantEvents[name]),
        name: name,
        data: text.slice(separator + 2)
    };
}

function firstField(data) {
    const text = String(data || "");
    const comma = text.indexOf(",");
    return comma < 0 ? text : text.slice(0, comma);
}

function secondField(data) {
    const text = String(data || "");
    const firstComma = text.indexOf(",");
    if (firstComma < 0)
        return "";
    const remainder = text.slice(firstComma + 1);
    const secondComma = remainder.indexOf(",");
    return secondComma < 0
        ? remainder
        : remainder.slice(0, secondComma);
}

function applyUrgentEvent(addresses, event, snapshot) {
    const next = copyAddressSet(addresses);
    if (!event || !event.valid)
        return next;

    if (event.name === "urgent") {
        const address = normalizeAddress(firstField(event.data));
        if (address.length > 0)
            next[address] = 3;
    } else if (event.name === "closewindow"
            || event.name === "activewindowv2") {
        const address = normalizeAddress(firstField(event.data));
        if (address.length > 0)
            delete next[address];
    } else if (event.name === "workspacev2") {
        const id = Number(firstField(event.data));
        clearWorkspaceUrgency(
            next,
            snapshot,
            Number.isInteger(id) ? id : null,
            secondField(event.data)
        );
    } else if (event.name === "workspace") {
        clearWorkspaceUrgency(next, snapshot, null, event.data);
    } else if (event.name === "focusedmonv2") {
        const id = Number(secondField(event.data));
        clearWorkspaceUrgency(
            next,
            snapshot,
            Number.isInteger(id) ? id : null,
            ""
        );
    } else if (event.name === "focusedmon") {
        clearWorkspaceUrgency(
            next,
            snapshot,
            null,
            secondField(event.data)
        );
    }
    return next;
}

function findMonitor(snapshot, outputName) {
    if (!snapshot)
        return null;
    const name = String(outputName || "");
    for (let index = 0; index < snapshot.monitors.length; ++index) {
        if (snapshot.monitors[index].name === name)
            return snapshot.monitors[index];
    }
    return null;
}

function findWorkspace(snapshot, outputName, workspaceId) {
    if (!snapshot)
        return null;
    const name = String(outputName || "");
    const id = Number(workspaceId);
    for (let index = 0; index < snapshot.workspaces.length; ++index) {
        const workspace = snapshot.workspaces[index];
        if (workspace.monitorName === name && workspace.id === id)
            return workspace;
    }
    return null;
}

function findClient(snapshot, address) {
    if (!snapshot)
        return null;
    const normalized = normalizeAddress(address);
    for (let index = 0; index < snapshot.clients.length; ++index) {
        if (snapshot.clients[index].address === normalized)
            return snapshot.clients[index];
    }
    return null;
}

function visibleWorkspaceCoveringMode(snapshot, outputName) {
    const monitor = findMonitor(snapshot, outputName);
    if (!monitor)
        return 0;

    const workspace = findWorkspace(
        snapshot,
        outputName,
        monitor.visibleWorkspaceId
    );
    if (!workspace
            || workspace.name !== monitor.visibleWorkspaceName
            || !workspace.hasFullscreen) {
        return 0;
    }

    let candidates = [];
    let coveringPriority = Number.MAX_SAFE_INTEGER;
    for (let index = 0; index < snapshot.clients.length; ++index) {
        const client = snapshot.clients[index];
        if (!client.mapped
                || client.hidden
                || !client.visible
                || client.fullscreenMode === 0
                || client.workspaceId !== workspace.id
                || client.workspaceName !== workspace.name
                || client.monitorId !== monitor.id) {
            continue;
        }

        const priority = client.floating
            ? 0
            : client.fullscreenHandler === "default" ? 1 : 2;
        if (priority > coveringPriority)
            continue;
        if (priority < coveringPriority) {
            coveringPriority = priority;
            candidates = [];
        }
        candidates.push(client);
    }
    if (candidates.length === 1)
        return candidates[0].fullscreenMode;
    if (candidates.length === 0)
        return 0;

    const lastWindowMatches = candidates.filter(client => {
        return workspace.lastWindowAddress.length > 0
            && client.address === workspace.lastWindowAddress;
    });
    if (lastWindowMatches.length === 1)
        return lastWindowMatches[0].fullscreenMode;

    let lowestFocusHistory = Number.MAX_SAFE_INTEGER;
    for (let index = 0; index < candidates.length; ++index) {
        if (candidates[index].focusHistoryId >= 0) {
            lowestFocusHistory = Math.min(
                lowestFocusHistory,
                candidates[index].focusHistoryId
            );
        }
    }
    const focusMatches = candidates.filter(client => {
        return client.focusHistoryId >= 0
            && client.focusHistoryId === lowestFocusHistory;
    });
    return focusMatches.length === 1
        ? focusMatches[0].fullscreenMode
        : 0;
}

function validWorkspaceName(value) {
    return typeof value === "string"
        && value.length > 0
        && !/[\u0000-\u001f\u007f]/.test(value);
}

function luaString(value) {
    return "\"" + String(value)
        .replace(/\\/g, "\\\\")
        .replace(/\"/g, "\\\"")
        .replace(/\t/g, "\\t") + "\"";
}

function workspaceDispatch(usingLua, workspace) {
    if (!workspace || !isInteger(workspace.id) || workspace.id === 0)
        return "";

    let selector;
    if (workspace.id > 0) {
        selector = String(workspace.id);
    } else {
        if (!validWorkspaceName(workspace.name))
            return "";
        selector = workspace.name.indexOf("special:") === 0
            ? workspace.name
            : "name:" + workspace.name;
    }

    if (usingLua) {
        return "hl.dsp.focus({ workspace = "
            + luaString(selector) + " })";
    }
    return "workspace " + selector;
}

function windowDispatch(usingLua, address) {
    const normalized = normalizeAddress(address);
    if (normalized.length === 0)
        return "";
    const selector = "address:" + normalized;
    if (usingLua) {
        return "hl.dsp.focus({ window = "
            + luaString(selector) + " })";
    }
    return "focuswindow " + selector;
}
