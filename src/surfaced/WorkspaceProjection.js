.pragma library

function valuesOf(model) {
    if (!model)
        return [];
    if (Array.isArray(model))
        return model;
    if (model.values !== undefined)
        return Array.from(model.values || []);
    return Array.from(model);
}

function displayName(workspace) {
    const rawName = String(workspace && workspace.name || "");
    if (rawName.startsWith("name:"))
        return rawName.slice(5);
    return rawName.length > 0
        ? rawName
        : String(workspace && workspace.id !== undefined
            ? workspace.id
            : "");
}

function isNormalWorkspaceOnOutput(workspace, outputName) {
    return workspace
        && workspace.monitorName === String(outputName || "")
        && !String(workspace.name || "").startsWith("special:");
}

function compareWorkspaces(left, right) {
    const leftId = Number(left.id);
    const rightId = Number(right.id);
    const leftNumbered = leftId >= 0;
    const rightNumbered = rightId >= 0;

    if (leftNumbered && rightNumbered)
        return leftId - rightId;
    if (leftNumbered !== rightNumbered)
        return leftNumbered ? -1 : 1;

    const byName = displayName(left).localeCompare(displayName(right));
    return byName !== 0 ? byName : leftId - rightId;
}

function monitorForOutput(snapshot, outputName) {
    if (!snapshot)
        return null;
    const monitors = valuesOf(snapshot.monitors);
    const name = String(outputName || "");
    for (let index = 0; index < monitors.length; ++index) {
        if (monitors[index] && monitors[index].name === name)
            return monitors[index];
    }
    return null;
}

function find(snapshot, outputName, workspaceId) {
    if (!snapshot)
        return null;
    const workspaces = valuesOf(snapshot.workspaces);
    for (let index = 0; index < workspaces.length; ++index) {
        const workspace = workspaces[index];
        if (isNormalWorkspaceOnOutput(workspace, outputName)
                && Number(workspace.id) === Number(workspaceId)) {
            return workspace;
        }
    }
    return null;
}

function outputAvailable(snapshot, outputName) {
    const monitor = monitorForOutput(snapshot, outputName);
    if (!monitor
            || !Number.isInteger(monitor.activeWorkspaceId)
            || String(monitor.activeWorkspaceName || "").length === 0) {
        return false;
    }

    const activeWorkspace = find(
        snapshot,
        outputName,
        monitor.activeWorkspaceId
    );
    return activeWorkspace !== null
        && activeWorkspace.name === monitor.activeWorkspaceName;
}

function clientsForWorkspace(snapshot, workspaceId) {
    if (!snapshot)
        return [];
    return valuesOf(snapshot.clients).filter(client => {
        return client
            && client.mapped === true
            && Number(client.workspaceId) === Number(workspaceId);
    });
}

function initialFor(label) {
    const normalized = String(label || "").trim();
    return normalized.length > 0 ? normalized.charAt(0).toUpperCase() : "?";
}

function applicationDescriptor(client, index, resolveApplication) {
    const rawId = String(client && client.applicationId || "");
    const resolved = resolveApplication
        ? (resolveApplication(rawId) || {})
        : {};
    const resolvedKey = String(resolved.key || rawId || "application")
        .toLowerCase();
    const label = String(resolved.label || rawId || "Application");
    const activationKey = String(client && client.address || "");

    return {
        key: resolvedKey,
        itemKey: resolvedKey + ":" + (activationKey || String(index)),
        activationKey: activationKey,
        label: label,
        iconSource: String(resolved.iconSource || ""),
        fallbackInitial: String(
            resolved.fallbackInitial || initialFor(label)
        ),
        active: Boolean(client && client.active),
        count: 1,
        activatable: activationKey.length > 0
    };
}

function projectApplications(clients, active, resolveApplication) {
    const source = valuesOf(clients);
    const projected = [];

    for (let index = 0; index < source.length; ++index) {
        const descriptor = applicationDescriptor(
            source[index],
            index,
            resolveApplication
        );

        if (active) {
            projected.push(descriptor);
            continue;
        }

        let existingIndex = -1;
        for (let candidate = 0; candidate < projected.length; ++candidate) {
            if (projected[candidate].key === descriptor.key) {
                existingIndex = candidate;
                break;
            }
        }

        if (existingIndex < 0) {
            descriptor.itemKey = descriptor.key;
            projected.push(descriptor);
            continue;
        }

        const existing = projected[existingIndex];
        const count = existing.count + 1;
        if (descriptor.active && !existing.active) {
            descriptor.itemKey = descriptor.key;
            descriptor.count = count;
            projected[existingIndex] = descriptor;
        } else {
            existing.count = count;
        }
    }

    if (!active)
        return projected;

    return projected.filter(application => application.active).concat(
        projected.filter(application => !application.active)
    );
}

function project(snapshot, outputName, options) {
    if (!outputAvailable(snapshot, outputName))
        return [];

    const settings = options || {};
    const monitor = monitorForOutput(snapshot, outputName);
    const resolveApplication = settings.resolveApplication;
    let projected = valuesOf(snapshot.workspaces)
        .filter(workspace => {
            return isNormalWorkspaceOnOutput(workspace, outputName);
        })
        .sort(compareWorkspaces)
        .map(workspace => {
            const id = Number(workspace.id);
            const active = id === monitor.activeWorkspaceId;
            const clients = clientsForWorkspace(snapshot, id);
            const applications = settings.showApplications === false
                ? []
                : projectApplications(
                    clients,
                    active,
                    resolveApplication
                );
            const name = displayName(workspace);
            return {
                key: "workspace:" + String(id),
                workspaceId: id,
                name: name,
                numberLabel: id >= 0 ? String(id) : name,
                active: active,
                urgent: Boolean(workspace.urgent),
                occupied: Number(workspace.windowCount) > 0
                    || clients.length > 0,
                applications: applications
            };
        });

    if (settings.occupiedOnly)
        projected = projected.filter(entry => entry.active || entry.occupied);

    return projected;
}

function adjacentWorkspaceId(entries, activeWorkspaceId, direction) {
    const workspaces = valuesOf(entries).filter(entry => {
        return entry
            && entry.workspaceId !== null
            && entry.workspaceId !== undefined
            && Number.isFinite(Number(entry.workspaceId));
    });
    const activeIndex = workspaces.findIndex(entry => {
        return Number(entry.workspaceId) === Number(activeWorkspaceId);
    });
    if (activeIndex < 0)
        return null;

    const step = direction < 0 ? -1 : 1;
    const targetIndex = activeIndex + step;
    if (targetIndex < 0 || targetIndex >= workspaces.length)
        return null;
    return Number(workspaces[targetIndex].workspaceId);
}
