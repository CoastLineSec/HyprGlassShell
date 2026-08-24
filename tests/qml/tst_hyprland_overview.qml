import QtQuick
import QtQuick.Window
import QtTest
import "../../src/settings" as Settings

TestCase {
    name: "HyprlandOverviewPage"
    when: windowShown

    Component {
        id: overviewComponent

        Window {
            width: 920
            height: 760
            visible: true

            property alias page: overview

            Settings.HyprlandOverviewPage {
                id: overview

                anchors.fill: parent
            }
        }
    }

    Component {
        id: graphicComponent

        Settings.HyprlandCategoryGraphic {}
    }

    function catalogFixture() {
        return [
            {
                id: "hyprland.decoration.rounding",
                module: "decoration"
            },
            {
                id: "hyprland.decoration.rounding"
            },
            {
                id: "hyprland.animations.enabled"
            },
            {
                id: "fixture.cursor",
                path: "cursor:zoom_factor"
            },
            {
                id: "hyprland.input.sensitivity",
                module: "input"
            },
            {
                id: "hyprland.gestures.workspace_swipe_distance"
            },
            {
                id: "hyprland.general.layout",
                module: "general"
            },
            {
                id: "hyprland.dwindle.preserve_split"
            },
            {
                id: "hyprland.master.mfact",
                module: "master"
            },
            {
                id: "hyprland.group.auto_group",
                module: "group"
            },
            {
                id: "hyprland.layout.single_window_aspect_ratio"
            },
            {
                id: "hyprland.scrolling.column_width"
            },
            {
                id: "fixture.monitor",
                catalogModule: "monitors"
            },
            {
                id: "fixture.workspace",
                module: "workspaces"
            },
            {
                id: "fixture.rule",
                module: "rules"
            },
            {
                id: "hyprland.binds.drag_threshold"
            },
            {
                id: "hyprland.debug.disable_logs"
            },
            {
                id: "hyprland.experimental.xx_color_management_v4"
            },
            {
                id: "hyprland.opengl.nvidia_anti_flicker"
            },
            {
                id: "hyprland.quirks.prefer_hdr"
            },
            {
                id: "hyprland.render.direct_scanout"
            },
            {
                id: "hyprland.xwayland.enabled"
            },
            {
                id: "hyprland.ecosystem.no_update_news"
            },
            {
                id: "hyprland.misc.session_lock_xray"
            }
        ];
    }

    function test_categoryTaxonomyAndRoutes() {
        const window = createTemporaryObject(overviewComponent, this);
        verify(window !== null);
        const page = window.page;

        compare(page.categories.length, 12);
        compare(page.categories.map(category => category.id), ["appearance", "input", "devices", "windows", "displays", "workspaces", "rules", "shortcuts", "system", "session", "environment", "permissions"]);
        compare(page.categoryById("appearance").pageId, "appearance");
        compare(page.categoryById("input").pageId, "input");
        compare(page.categoryById("devices").pageId, "input-devices");
        compare(page.categoryById("windows").pageId, "windows");
        compare(page.categoryById("displays").pageId, "displays");
        compare(page.categoryById("workspaces").pageId, "workspaces");
        compare(page.categoryById("rules").pageId, "rules");
        compare(page.categoryById("shortcuts").pageId, "bindings");
        compare(page.categoryById("system").pageId, "advanced");
        compare(page.categoryById("session").pageId, "advanced");
        compare(page.categoryById("environment").pageId, "environment");
        compare(page.categoryById("permissions").pageId, "permissions");

        verify(findChild(page, "hyprlandOverviewHero") !== null);
        verify(findChild(page, "hyprlandCoverageStatusArea") !== null);
        verify(findChild(page, "hyprlandCategoryCard-appearance") !== null);
        verify(findChild(page, "hyprlandCategoryCard-session") !== null);
        verify(findChild(page, "hyprlandCategoryCard-permissions") !== null);
    }

    function test_catalogCountsUseModuleOwnershipAndDeduplicateIds() {
        const window = createTemporaryObject(overviewComponent, this);
        verify(window !== null);
        const page = window.page;
        page.allOptions = catalogFixture();

        compare(page.loadedCatalogOptionCount, 23);
        const appearanceCard = findChild(
            page, "hyprlandCategoryCard-appearance"
        );
        verify(appearanceCard !== null);
        tryCompare(appearanceCard, "optionCount", 3);
        compare(appearanceCard.coverageLabel, "3 catalog options");
        compare(page.optionCountForCategory("appearance"), 3);
        compare(page.optionCountForCategory("input"), 2);
        compare(page.optionCountForCategory("devices"), 0);
        compare(page.optionCountForCategory("windows"), 6);
        compare(page.optionCountForCategory("displays"), 1);
        compare(page.optionCountForCategory("workspaces"), 1);
        compare(page.optionCountForCategory("rules"), 1);
        compare(page.optionCountForCategory("shortcuts"), 1);
        compare(page.optionCountForCategory("system"), 7);
        compare(page.optionCountForCategory("session"), 1);
        compare(page.optionCountForCategory("environment"), 0);
        compare(page.optionCountForCategory("permissions"), 0);
        compare(page.optionCountForCategory("unknown"), 0);

        page.allOptions = {
            first: [
                {
                    id: "hyprland.animations.enabled"
                }
            ],
            second: [
                {
                    id: "hyprland.input.sensitivity"
                }
            ]
        };
        compare(page.loadedCatalogOptionCount, 2);
        compare(page.optionCountForCategory("appearance"), 1);
        compare(page.optionCountForCategory("input"), 1);

        page.allOptions = {
            0: {
                id: "hyprland.binds.drag_threshold"
            },
            1: {
                id: "hyprland.ecosystem.no_update_news"
            },
            length: 2
        };
        compare(page.loadedCatalogOptionCount, 2);
        compare(page.optionCountForCategory("shortcuts"), 1);
        compare(page.optionCountForCategory("session"), 1);
    }

    function test_reviewedCoverageContractAndResponsiveGrid() {
        const window = createTemporaryObject(overviewComponent, this);
        verify(window !== null);
        const page = window.page;

        compare(page.reviewedHyprlandVersion, "0.56.2");
        compare(page.reviewedOptionCount, 353);
        tryCompare(page, "cardColumnCount", 2);

        window.width = 620;
        tryCompare(page, "cardColumnCount", 1);

        page.allOptions = catalogFixture();
        verify(page.coverageSummary.indexOf("23") >= 0);
        verify(page.coverageSummary.indexOf("353") >= 0);
        verify(!page.coverageComplete);
    }

    function test_activationEmitsCategoryAndCurrentSurface() {
        const window = createTemporaryObject(overviewComponent, this);
        verify(window !== null);
        const page = window.page;
        const categories = [];
        const surfaces = [];

        function rememberCategory(categoryId) {
            categories.push(categoryId);
        }
        function rememberSurface(pageId) {
            surfaces.push(pageId);
        }
        page.openCategoryRequested.connect(rememberCategory);
        page.openSurfaceRequested.connect(rememberSurface);

        verify(page.activateCategory("appearance"));
        compare(categories.length, 1);
        compare(categories[0], "appearance");
        compare(surfaces.length, 0);

        verify(page.activateCategory("rules"));
        compare(categories.length, 1);
        compare(surfaces.length, 1);
        compare(surfaces[0], "rules");

        verify(!page.activateCategory("not-a-category"));
        compare(categories.length, 1);
        compare(surfaces.length, 1);

        page.openCategoryRequested.disconnect(rememberCategory);
        page.openSurfaceRequested.disconnect(rememberSurface);
    }

    function test_allGraphicKindsInstantiate() {
        const graphic = createTemporaryObject(graphicComponent, this);
        verify(graphic !== null);
        verify(graphic.implicitWidth >= 120);
        verify(graphic.implicitHeight >= 88);

        for (const kind of ["appearance", "input", "windows", "displays", "workspaces", "rules", "shortcuts", "system", "security"]) {
            graphic.kind = kind;
            wait(0);
            compare(graphic.kind, kind);
        }
    }
}
