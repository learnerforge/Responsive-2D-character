# src/ui/Menu.cpp — Context Menu Implementation

## File role

Implements the popup menu construction and the modal show loop.

## Menu layout

```
Animate                 (MF_STRING, id 1)
─────────────
Auto-start with Windows (MF_STRING | MF_CHECKED?, id 2)
Settings…               (MF_STRING, id 3)
─────────────
Quit                    (MF_STRING, id 4)
```

Command ids are private constants (`kIdAnimate=1 … kIdQuit=4`). Keep ids > 0
(0 = `TPM_RETURNCMD` returns 0 for a cancelled menu).

## Build + Show

```cpp
HMENU ui::Menu::Build() {
  HMENU h = CreatePopupMenu();
  UINT flagsAuto = MF_STRING | (autostartChecked_ ? MF_CHECKED : 0);
  AppendMenuW(h, MF_STRING, 1, L"Animate");
  AppendMenuW(h, flagsAuto, 2, L"Auto-start with Windows");
  AppendMenuW(h, MF_STRING, 3, L"Settings\u2026");
  AppendMenuW(h, MF_SEPARATOR, 0, nullptr);
  AppendMenuW(h, MF_STRING, 4, L"Quit");
  return h;
}

MenuChoice ui::Menu::Show(HWND owner, int x, int y) {
  HMENU h = Build();
  if (!h) return MenuChoice::None;

  UINT cmd = TrackPopupMenu(h, TPM_RETURNCMD | TPM_RIGHTBUTTON | TPM_NONOTIFY,
                            x, y, 0, owner, nullptr);
  DestroyMenu(h);                      // always destroy — no leak

  switch (cmd) {
    case 1: return MenuChoice::Animate;
    case 2: return MenuChoice::ToggleAutostart;
    case 3: return MenuChoice::Settings;
    case 4: return MenuChoice::Quit;
    default: return MenuChoice::None;
  }
}
```

### Flags rationale
- `TPM_RETURNCMD` → the selected id is the return value (0 = cancelled).
- `TPM_RIGHTBUTTON` → item selection is right-button aware (standard for
  context menus).
- `TPM_NONOTIFY` → no `WM_COMMAND` sent; we read the return value directly and
  avoid a `WM_COMMAND` handler in `App`'s WndProc.

## Position

`(x, y)` are the **screen** coordinates of the right-click (converted from the
client click via `ClientToScreen` by `App` before calling `Show`). The menu
appears anchored at the cursor.

## Modal-loop side effect

While the menu is open, `TrackPopupMenu` pumps messages, so:
- our `WM_TIMER` (30 Hz) keeps firing → the character continues animating,
- the menu receives and handles all input itself → no stray clicks leak to the
  character window.

## Error handling

| Condition | Behavior |
|---|---|
| `CreatePopupMenu` fails | return `None` |
| `TrackPopupMenu` returns 0 | treat as `None` (dismissed) |
| `DestroyMenu` | always called before returning |

## Performance / memory

- Allocates/frees one HMENU (~1–2 KB) per show — transient, immediately
  released.
- After the menu closes, `App` calls `TrimWorkingSet()` (see
  [WinUtil.cpp.md](../util/WinUtil.cpp.md)) to shed the transient pages.

## Testing

- Manual: right-click shows menu, each item works (Animate triggers hop,
  autostart toggles the HKCU key and the checkmark, Settings shows placeholder,
  Quit exits), Esc dismisses returning `None`.

## Related

- Header: [Menu.h.md](Menu.h.md)
- Autostart state: [Autostart.cpp.md](../util/Autostart.cpp.md)
- Trigger: [App.cpp.md](../App.cpp.md)
