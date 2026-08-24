# Fjord Personal Alpha

This document defines the first usable Fjord release. The technical gates must
pass first.

## Goal

Give one Omarchy user a fast secondary browser for real work, communication,
and media. Chrome remains the default browser and fallback.

Fjord is not a public release and does not promise Chrome compatibility.

## Identity

| Item | Value |
| --- | --- |
| Codename | Fjord |
| Binary | `fjord` |
| App ID | `io.github.PabloMerino.Fjord` |
| Language | English |
| License | Apache-2.0 |

The name and app ID may change before a public release.

## Main Window

- One main, undecorated browser window
- Hyprland handles placement, size, and workspace choice
- Fjord remembers size only
- A temporary login popup is allowed when a site needs one
- A second Fjord launch shows a warning and exits
- Closing the last tab exits Fjord
- Quitting with several tabs asks for confirmation
- Quitting with active downloads asks before cancelling them

## Tabs

Tabs live in a full-height sidebar on the left.

| Mode | Size | Contents |
| --- | --- | --- |
| Expanded | About 240 px | Icon, title, loading state, audio state, close button |
| Collapsed | About 44 px | Icon, state marker, title tooltip |

Expanded is the default. The choice is stored in `config.toml`.

The bottom of the sidebar contains New Tab and the sidebar toggle. The active
or hovered expanded tab shows its close button. An audio icon can mute that tab.

Fjord supports new, close, switch, duplicate, and reopen-closed tabs. It keeps
active tabs alive after they open. Restored background tabs wait until selected.
Tab reordering is not part of the alpha.

Useful defaults:

- `Ctrl+T`: new tab
- `Ctrl+W`: close tab
- `Ctrl+Shift+T`: reopen closed tab
- `Ctrl+Tab`: next tab in sidebar order
- `Ctrl+1` to `Ctrl+8`: select a numbered tab
- `Ctrl+9`: select the last tab

Closing the active tab returns to the previously used tab. Middle-click closes
a tab.

## Toolbar And Navigation

The toolbar sits above webpage content, to the right of the sidebar. It has:

- Back and Forward buttons
- One button that is Reload when idle and Stop while loading
- A full address field
- A site information button
- An overflow menu
- A thin loading line

The toolbar stays visible in fullscreen. The sidebar hides and returns after
fullscreen ends.

New tabs show `fjord://newtab` and focus the address field. Google is the
default search engine and can be changed. Suggestions use only open tabs and
local history. Page zoom is stored per website.

Fjord opens local files only when the user chooses them. A webpage cannot open
local files by itself. Links such as `mailto:` ask before opening another desktop
application and can remember the answer.

## Commands

Buttons, menus, accessibility actions, shortcuts, and the command palette use
the same commands.

| Action | Default key |
| --- | --- |
| Focus address | `Ctrl+L` |
| Find in page | `Ctrl+F` |
| Back / Forward | `Alt+Left` / `Alt+Right` |
| Reload | `Ctrl+R` |
| Zoom | `Ctrl++`, `Ctrl+-`, `Ctrl+0` |
| Command palette | `Ctrl+K` |

The command palette searches actions, open tabs, and history. The menu opens a
keybindings guide that shows active shortcuts. Users may override shortcuts in
TOML and restart Fjord to apply changes.

## Built-In Pages And Prompts

`fjord://newtab`, `fjord://error`, `fjord://crash`, and
`fjord://keybindings` use local HTML and CSS without JavaScript. They use the
same colors and font as Fjord's native interface.

Settings, downloads, site information, permissions, and clear-data controls
use native Fjord panels. Webpage requests such as alerts, select menus,
permissions, authentication, file selection, and context menus also use native
Fjord panels.

## Saved Data

WPE WebKit stores cookies and website data. Fjord stores sessions, history,
recently closed tabs, permission choices, zoom, user-agent overrides, external
link choices, and download records.

Fjord restores the last session. It loads the active tab first. History and
download records stay until cleared. Users can clear history, download records,
permissions, and website data separately.

Fjord stores no passwords and has no private mode in this alpha.

## Files, Media, And Permissions

- Downloads go to the normal Downloads folder and never open automatically.
- The menu shows download progress and recent downloads.
- File chooser uploads, desktop file drops, and text/HTML/image clipboard work.
- Camera, microphone, location, notifications, and clipboard access ask first.
- Allowed notifications use the desktop notification service.
- Screen sharing uses the desktop portal, the system screen-sharing service.
- Media follows WebKit's normal autoplay rules.

## Safety

Before real-account use, Fjord must verify WPE's sandbox, TLS checks, private
data directories, safe permission prompts, and redacted logs.

Bad certificates show an error page. A user may make a one-time exception.
Fjord does not save that exception. The site information panel shows connection
state, permissions, and website data controls.

## Appearance And Accessibility

Fjord reads the active Omarchy theme when it starts. It uses JetBrains Mono for
browser text, small bundled icons, opaque surfaces, and limited motion.

Keyboard use, visible focus, accessible names, roles, and state are required.
Full screen-reader testing comes later.

## Acceptance

The alpha is ready when the technical gates pass and these workflows complete:

- GitHub, Linear, AWS Console, and Slack web
- Gmail, Calendar, Drive, and Docs
- ChatGPT with streaming, rich text, clipboard, and uploads
- Google Meet with camera, microphone, notifications, and screen sharing
- YouTube playback, seeking, audio, mute, and fullscreen

Known small visual differences are acceptable. Broken core workflows are not.
