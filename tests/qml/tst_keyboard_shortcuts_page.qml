import QtQuick
import QtQuick.Window
import QtTest
import "../../src/settings" as Settings

TestCase {
    id: testCase

    name: "KeyboardShortcutsPage"
    when: windowShown

    readonly property var sampleRows: [
        {
            ordinal: 1,
            sourceLine: 8,
            section: "Application Launchers",
            chord: "SUPER + T",
            action: "hl.dsp.exec_cmd(\"hgs-terminal\")",
            sourceText:
                "hl.bind(\"SUPER + T\", hl.dsp.exec_cmd(\"hgs-terminal\"))",
            options: ({})
        },
        {
            ordinal: 6,
            sourceLine: 13,
            section: "Application Launchers",
            chord: "SUPER + comma",
            action:
                "hl.dsp.exec_cmd(\"hgs ipc call settings focusOrToggle\")",
            sourceText:
                "hl.bind(\"SUPER + comma\", hl.dsp.exec_cmd(\"hgs ipc call settings focusOrToggle\"))",
            options: ({})
        },
        {
            ordinal: 50,
            sourceLine: 73,
            section: "Column Navigation",
            chord: "SUPER + Home",
            action: "hl.dsp.focus({ window = \"first\" })",
            sourceText:
                "hl.bind(\"SUPER + Home\", hl.dsp.focus({ window = \"first\" }))",
            options: ({})
        },
        {
            ordinal: 51,
            sourceLine: 74,
            section: "Column Navigation",
            chord: "SUPER + End",
            action: "hl.dsp.focus({ window = \"last\" })",
            sourceText:
                "hl.bind(\"SUPER + End\", hl.dsp.focus({ window = \"last\" }))",
            options: ({})
        },
        {
            ordinal: 105,
            sourceLine: 150,
            section:
                "Move/resize windows with mainMod + LMB/RMB and dragging",
            chord: "SUPER + mouse:272",
            action: "hl.dsp.window.drag()",
            sourceText:
                "hl.bind(\"SUPER + mouse:272\", hl.dsp.window.drag(), { mouse = true, description = \"Move window\" })",
            options: ({ mouse: true, description: "Move window" })
        }
    ]

    Component {
        id: pageWindowComponent

        Window {
            width: 423
            height: 480
            visible: true

            property alias page: shortcutPage

            Settings.KeyboardShortcutsPage {
                id: shortcutPage

                anchors.fill: parent
                referenceAvailable: true
                referenceErrorMessage: ""
                referenceSourceDigest: "source-receipt"
                referenceArtifactDigest: "artifact-receipt"
                referenceRowCount: testCase.sampleRows.length
                sourceRows: testCase.sampleRows
            }
        }
    }

    Component {
        id: actualReferenceWindowComponent

        Window {
            width: 640
            height: 600
            visible: true

            property alias page: shortcutPage

            Settings.KeyboardShortcutsPage {
                id: shortcutPage

                anchors.fill: parent
                contentTopMargin: 12
            }
        }
    }

    function test_actualSingletonProvidesAllPinnedRowsUnchanged() {
        const testWindow = createTemporaryObject(
            actualReferenceWindowComponent, this
        );
        verify(testWindow !== null);
        const page = testWindow.page;
        waitForRendering(page);

        compare(page.referenceAvailable, true);
        compare(page.referenceRowCount, 117);
        compare(page.filteredRows.length, 117);
        compare(
            page.referenceSourceDigest,
            "47bbde429980d2fa9817c88915cac595ec887573802ed162980613f576b9979d"
        );
        compare(
            page.referenceArtifactDigest,
            "ee9f5cbe19e4deea91d4640725c14df4153bec6c1e68b11d8770e39c866fc7ba"
        );
        compare(page.filteredRows[0].ordinal, 1);
        compare(page.filteredRows[0].chord, "SUPER + T");
        compare(page.filteredRows[5].ordinal, 6);
        compare(page.filteredRows[5].chord, "SUPER + comma");
        compare(
            page.filteredRows[5].action,
            "hl.dsp.exec_cmd(\"hgs ipc call settings focusOrToggle\")"
        );
        compare(page.filteredRows[49].ordinal, 50);
        compare(page.filteredRows[49].chord, "SUPER + Home");
        compare(page.filteredRows[50].ordinal, 51);
        compare(page.filteredRows[50].chord, "SUPER + End");
        compare(page.filteredRows[104].ordinal, 105);
        compare(page.filteredRows[104].chord, "SUPER + mouse:272");
        compare(
            page.optionsText(page.filteredRows[104]),
            "{ mouse = true, description = \"Move window\" }"
        );
        compare(page.filteredRows[116].ordinal, 117);
        compare(page.filteredRows[116].chord, "SUPER + SHIFT + P");
        compare(
            page.filteredRows[116].action,
            "hl.dsp.dpms({ action = \"toggle\" })"
        );
        compare(findChild(page, "keyboardShortcutList").count, 117);
    }

    function test_noticeMakesImmutableLegacyScopeExplicit() {
        const testWindow = createTemporaryObject(pageWindowComponent, this);
        verify(testWindow !== null);
        const page = testWindow.page;
        waitForRendering(page);

        const title = findChild(page, "keyboardShortcutsTitle");
        const noticeTitle = findChild(page, "legacyShortcutNoticeTitle");
        const noticeBody = findChild(page, "legacyShortcutNoticeBody");
        const resultCount = findChild(page, "keyboardShortcutResultCount");
        const list = findChild(page, "keyboardShortcutList");
        verify(title !== null);
        verify(noticeTitle !== null);
        verify(noticeBody !== null);
        verify(resultCount !== null);
        verify(list !== null);
        compare(title.text, "Keyboard Shortcuts");
        verify(String(noticeTitle.text).includes("Legacy reference"));
        verify(String(noticeTitle.text).includes("not current keybindings"));
        verify(String(noticeBody.text).includes("immutable"));
        verify(String(noticeBody.text).includes("source order"));
        verify(String(noticeBody.text).includes("does not read your active"));
        verify(String(noticeBody.text).includes("cannot edit, activate"));
        compare(resultCount.text, "5 of 5 legacy rows");
        compare(list.count, 5);
        compare(findChild(page, "saveKeyboardShortcutsButton"), null);
        compare(findChild(page, "resetKeyboardShortcutsButton"), null);
        compare(findChild(page, "applyKeyboardShortcutsButton"), null);
    }

    function test_searchRetainsSourceOrderAndLiteralLegacyRows() {
        const testWindow = createTemporaryObject(pageWindowComponent, this);
        verify(testWindow !== null);
        const page = testWindow.page;
        waitForRendering(page);

        compare(page.filteredRows.length, 5);
        compare(page.filteredRows[0].ordinal, 1);
        compare(page.filteredRows[1].ordinal, 6);
        compare(page.filteredRows[2].ordinal, 50);
        compare(page.filteredRows[3].ordinal, 51);
        compare(page.filteredRows[4].ordinal, 105);

        page.searchQuery = "hl.dsp.focus";
        tryCompare(page, "visibleRowCount", 2);
        compare(page.filteredRows[0].ordinal, 50);
        compare(page.filteredRows[0].chord, "SUPER + Home");
        compare(
            page.filteredRows[0].action,
            "hl.dsp.focus({ window = \"first\" })"
        );
        compare(page.filteredRows[1].ordinal, 51);
        compare(page.filteredRows[1].chord, "SUPER + End");
        compare(
            page.filteredRows[1].action,
            "hl.dsp.focus({ window = \"last\" })"
        );

        page.searchQuery = "comma";
        tryCompare(page, "visibleRowCount", 1);
        compare(page.filteredRows[0].ordinal, 6);
        compare(page.filteredRows[0].chord, "SUPER + comma");
        compare(
            page.filteredRows[0].action,
            "hl.dsp.exec_cmd(\"hgs ipc call settings focusOrToggle\")"
        );
        compare(page.optionsText(page.filteredRows[0]), "{}");

        page.searchQuery = "mouse = true";
        tryCompare(page, "visibleRowCount", 1);
        compare(page.filteredRows[0].ordinal, 105);
        compare(
            page.optionsText(page.filteredRows[0]),
            "{ mouse = true, description = \"Move window\" }"
        );
    }

    function test_compactPageWrapsWithoutHorizontalOverflow() {
        const testWindow = createTemporaryObject(pageWindowComponent, this);
        verify(testWindow !== null);
        const page = testWindow.page;
        waitForRendering(page);

        compare(page.width, 423);
        compare(page.height, 480);
        const search = findChild(page, "keyboardShortcutSearch");
        const list = findChild(page, "keyboardShortcutList");
        verify(search !== null);
        verify(list !== null);
        verify(search.width <= page.width);
        verify(search.height >= 44);
        verify(list.width <= page.width);
        verify(list.height > 0);
        compare(
            search.Accessible.name,
            "Search the legacy shortcut reference"
        );
        verify(String(search.Accessible.description).includes("read-only"));

        page.searchQuery = "comma";
        tryCompare(list, "count", 1);
        list.positionViewAtIndex(0, ListView.Beginning);
        wait(0);
        const card = list.itemAtIndex(0);
        verify(card !== null);
        compare(card.shortcutOrdinal, 6);
        compare(card.width, list.width);
        const chord = findChild(card, "keyboardShortcutChord");
        const action = findChild(card, "keyboardShortcutAction");
        const options = findChild(card, "keyboardShortcutOptions");
        const referenceState = findChild(
            card, "keyboardShortcutReferenceState"
        );
        verify(chord !== null);
        verify(action !== null);
        verify(options !== null);
        verify(referenceState !== null);
        compare(chord.text, "SUPER + comma");
        verify(String(action.text).includes("settings focusOrToggle"));
        compare(options.text, "Options: {}");
        verify(String(referenceState.text).includes("Reference only"));
        verify(String(referenceState.text).includes("not editable or active"));
        compare(chord.textFormat, Text.PlainText);
        compare(action.textFormat, Text.PlainText);
        compare(options.textFormat, Text.PlainText);
        compare(referenceState.textFormat, Text.PlainText);
        verify(chord.width <= card.width);
        verify(action.width <= card.width);
        verify(options.width <= card.width);
    }

    function test_verificationFailureNeverClaimsCurrentState() {
        const testWindow = createTemporaryObject(pageWindowComponent, this);
        verify(testWindow !== null);
        const page = testWindow.page;
        page.referenceAvailable = false;
        page.referenceErrorMessage = "artifact digest mismatch";
        waitForRendering(page);

        const error = findChild(page, "keyboardShortcutReferenceError");
        const list = findChild(page, "keyboardShortcutList");
        verify(error !== null);
        verify(list !== null);
        compare(error.visible, true);
        verify(String(error.text).includes("could not be verified"));
        verify(String(error.text).includes("artifact digest mismatch"));
        compare(list.visible, false);
        verify(!String(error.text).includes("current shortcut"));
    }
}
