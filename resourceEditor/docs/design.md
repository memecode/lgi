# Resource Editor (`lgires`) — Design Analysis

## Scope
This document describes the design of the Lgi Resource Editor located at `/home/matthew/code/lgi/trunk/resourceEditor`. It is based on the current source tree, project files, and bundled resources.

Key entry points:
- `src/LgiResApp.cpp` — application bootstrap, main window, file load/save, import/compare flows.
- `src/LgiResEdit.h` — shared data model, base abstractions, IDs, and cross-module interfaces.
- `src/LgiRes_Dialog.{h,cpp}` — dialog resource editing and control representations.
- `src/LgiRes_String.{h,cpp}` — string group model/UI and localization data.
- `src/LgiRes_Menu.{h,cpp}` — menu resource editing.
- `src/LgiRes_Css.cpp` — CSS/style resource editing.
- `src/LgiRes_ControlTree.cpp` — control-tree editor for hierarchical controls.
- `src/LgiRes_TableLayout.cpp` — table layout editing behavior for dialog layouts.
- `src/Search.cpp` — search/find across resources.
- `src/ShowLanguages.cpp` / language UI in strings/dialogs — language visibility/selection.
- `src/translate/googleTranslate.cpp` — optional translation plugin integration.
- `resources/lgires.lr8` and `resources/resdefs.h` — editor’s own resources and generated control IDs.

## High-level architecture

`lgires` is a desktop GUI application built on the LGI framework (`LDocApp`, `LView`, `LTree`, `LLayout`, `LDom`/XML utilities, etc.). It edits LGI resource files (`.lr8` / XML-based resource content) and supports interoperability with Win32 `.rc` scripts.

The application is organized as a document-like editor:
1. **Main application shell** (`AppWnd` in `LgiResApp.cpp`) manages file operations, menus, status, global language state, and object lifecycle.
2. **Object browser** (`ObjContainer`) presents resource categories in a tree: Style/CSS, Dialogs, Strings, Menus.
3. **Content editor area** hosts the currently selected resource’s specialized UI.
4. **Property/field editor** (`FieldView`) shows editable fields for the selected object via a generic `FieldSource`/`FieldTree` serialization mechanism.
5. **Resource model objects** (`Resource` subclasses) implement type-specific behavior, persistence, testing, and UI creation.

This yields a classic *model + specialized editor view + shared property inspector* structure, with XML serialization as the persistence boundary.

## Core abstractions and data model

### `Resource` hierarchy (`LgiResEdit.h`)
`Resource` is the base for editable resource objects. It provides:
- ownership back-reference to `AppWnd`
- resource type (`TYPE_CSS`, `TYPE_DIALOG`, `TYPE_STRING`, `TYPE_MENU`)
- association with a tree item (`ObjTreeItem`)
- selection/system-object state
- common operations: `Create`, `Test`, `Read`, `Write`, `CreateUI`, context menu/commands, clipboard hooks, language updates

Concrete/related types:
- `ResFolder` — non-editable category node in the object tree.
- `ResCss` — style resource (`<style name="...">...</style>`).
- `ResDialog` — dialog resource and its visual control editor.
- `ResStringGroup` — collection of localized strings/defines/IDs.
- `ResMenu` — menu tree resource with associated string group.

### View/update coupling
- `ResourceItem` / `ResourceView` form a lightweight observer relationship so model changes can refresh attached views (`UpdateAllViews`).
- `FieldSource` / `FieldView` provide a generic property-editing pipeline. `FieldTree` serializes fields between object state, UI controls, and `LDom` storage depending on mode (`UiToObj`, `ObjToUi`, `ObjToStore`, `StoreToObj`).
- `M_OBJECT_CHANGED` is posted to notify field views of changes.

This design avoids hand-coding separate property panels per object type; each editable object exposes fields through `GetFields`/`Serialize`.

### Localization model
Localization is centered on `ResString` / `StrLang` (`LgiRes_String.*`):
- A `ResString` has a global `Ref`, numeric control `Id`, symbolic `Define`, optional tag, and a list of language variants (`StrLang` items).
- `ResStringGroup` manages strings, language columns/visibility, sorting, uniqueness (`UniqueRef`, `UniqueId`), and persistence.
- Dialog/menu controls reference strings (for captions/text) and may create or share string entries/symbols.
- `AppWnd` tracks available/visible/current languages and propagates language changes (`OnShowLanguages`, `OnLanguagesChange`).

The design treats text as a first-class localizable resource rather than embedding literals directly in UI definitions.

## Main UI composition

### Application window (`AppWnd`)
`AppWnd` (subclass of `LDocApp<LOptionsFile>`) builds:
- menu/commands (file, edit, view/languages, tools, help)
- horizontal split (`LBox`) containing:
  - left vertical area: object tree (`ObjContainer`) + field/property view (`FieldView`)
  - right/content area: selected resource editor (`ContentView`, wrapped by `ResFrame`)
- optional shortcuts window (`ShortCutView`) showing accelerators/IDs from dialogs/menus

Key responsibilities:
- file open/save dispatch by extension (`.lr8/.xml`, `.rc` import/open path)
- object creation/insertion/deletion and selection routing
- language management and menu updates
- search/compare/import language dialogs
- dirty-state propagation on edits

### Object browser (`ObjContainer`)
A tree with fixed top-level folders for Style, Dialog, String, Menu resources. Each resource becomes an `ObjTreeItem`; selection calls `AppWnd::OnResourceSelect`. Context menus support create/delete/rename/import operations depending on folder/resource type.

### Resource editing surfaces
- **Dialog editor**: visual design surface with selectable/movable controls, goober handles, snapping/grid behavior, tool palette (`ResDialogUi`/toolbar), tab order editing, and control-specific field editing.
- **String editor**: list/table-like UI for string groups and language variants; supports copy/paste text, language operations, uniqueness/repair.
- **Menu editor**: tree-based menu/submenu/item editing; items carry `ResString` references and properties like separator/enabled/shortcut.
- **CSS editor**: simple form (`ResCssUi`) for style name/content.
- **Control tree / table layout**: specialized editors for hierarchical controls and table-based dialog layout/cell management.

`ResFrame` hosts the active resource view and intercepts clipboard/delete key handling, forwarding to the resource.

## Persistence and interoperability

### Native resource format
The editor reads/writes LGI resource XML (`.lr8`/XML). `AppWnd::LoadLgi`:
- parses XML into `LXmlTag` tree
- creates resource objects by tag (`style`, `dialog`, `string-group`, `menu`)
- runs `SerialiseContext::PostLoad` for repairs (e.g., duplicate/fixed control IDs)
- scans languages across string groups to populate the view-language menu

`AppWnd::SaveLgi`:
- validates with `TestLgi`
- backs up existing file as `.bak`
- writes string groups first, then other resources (so references can resolve on load)
- writes `Offset="1"` root attribute to distinguish coordinate/layout semantics from older files
- generates/updates `resdefs.h` with `#define` mappings for control IDs/symbols

### Win32 `.rc` import
`LoadWin32` parses `.rc` text to import dialogs, menus, and string tables. It tokenizes lines, tracks preprocessor-like defines/includes to resolve IDs, converts dialog/control geometry (with scaling/offset constants), and merges multi-language variants where matching resources exist. This makes the editor a bridge between Win32 resources and LGI’s resource model.

### Other file operations
- **Compare**: `ResCompare` loads two XML resource files and reports structural/attribute/string differences.
- **Import language**: imports translations from another LGI resource file by matching refs/IDs and merging language strings.
- **Options**: persisted via `LDocApp<LOptionsFile>`; e.g., visible languages (`OPT_ShowLanguages`).

## Search and navigation

`SearchThread` (`Search.cpp`) scans loaded resources for matches by:
- text content (in a specific language or across items)
- define name
- missing translation in a target language
- string `Ref` / control `Id`

Results (`LListItem` subclasses) can navigate back to the owning object/control/string via `AppWnd::GotoObject`, selecting the relevant tree node and UI element.

`ShortCutView` collects shortcut/accelerator information from menus and dialog controls (e.g., `&` mnemonics) for inspection and navigation.

## Translation integration

`LAutoTranslate` defines an engine interface; `src/translate/googleTranslate.cpp` registers a Google Translate-backed engine. It attempts to load a dynamic plugin (`libGoogleTranslatePlugin.so`) from the app install path and delegates translation requests through `LTranslationPlugin`. The engine exposes `DomGoogleCloudProject` from app options, indicating configuration-driven external translation support rather than hardcoded credentials.

## Build and platform packaging

The project is cross-platform:
- **CMake** builds the `LgiRes` executable, links LGI, includes `src/` and `resources/`, and packages platform-specific icons/resources (`CMakeLists.txt`).
- Native IDE/project files exist for Windows (`win/*.sln`, `.vcxproj`), macOS (`mac/LgiRes.xcodeproj`, plist, assets), Linux/Haiku makefiles.
- The app’s own UI/resources are stored in `resources/lgires.lr8`; generated symbol definitions are in `resources/resdefs.h`.

## Notable design characteristics

1. **Resource-type polymorphism with shared infrastructure**: a common `Resource` contract plus specialized editors keeps file/object management centralized in `AppWnd`.
2. **Generic field/property editing**: `FieldSource`/`FieldTree`/`FieldView` reduce duplication for object inspectors.
3. **Localization-first string model**: IDs, refs, defines, and per-language text are explicit, enabling translation workflows and Win32/LGI mapping.
4. **XML-centric persistence with migration hooks**: load/post-load repair and format markers (`Offset`) support compatibility and data cleanup.
5. **Interoperability tooling**: `.rc` import, language import, and file comparison are integrated into the editor, not separate utilities.
6. **LGI framework dependence**: UI, events, serialization helpers, file dialogs, and controls are deeply tied to LGI abstractions; portability is achieved through LGI/platform projects rather than a framework-neutral core.

## Risks / areas to inspect when modifying

- **ID/ref uniqueness**: string refs, control IDs, and defines are central to serialization, generated headers, and cross-resource references. Changes should preserve `UniqueRef`/`UniqueId` behavior and post-load repair.
- **Load order assumptions**: string groups are written/loaded before dependent resources; altering format order can break reference resolution.
- **Legacy coordinate/layout compatibility**: the `Offset` marker and dialog/control positioning logic must remain consistent with existing `.lr8` files.
- **Language propagation**: adding language features requires updating both model storage (`ResStringGroup`/`StrLang`) and UI refresh paths (`OnShowLanguages`, menu state).
- **Large, stateful source files**: `LgiResApp.cpp` and `LgiRes_Dialog.cpp` contain substantial logic; localized changes can affect unrelated workflows (file I/O, import, selection, clipboard, painting).
