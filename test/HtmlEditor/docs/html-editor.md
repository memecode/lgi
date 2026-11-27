# HTML Editor test harness

`htmleditor` is a GUI test harness for the RTE2 rich text control (`src/common/Widgets/richText2`). It opens an HTML file in the editor. Command line options either run a headless export/dump and exit, or simulate input once the layout has settled and trace the result.

Build with `make -j 12` in `test/HtmlEditor`.

```
./htmleditor [file.html] [options]
```

## Options

| Option | Action |
|--------|--------|
| `--export=<file>` | Write the processed HTML to `<file>`, then exit. |
| `--layout[=<file>]` | Capture the runtime layout to a `.rte` file (default `runtime-layout.rte`), then exit. |
| `--nodes[=<file>]` | Dump the editor's node tree, with layout and colour data, to a JSON file (default `nodes.json`), then exit. Requires a `DEBUG_ACTIVITY` build. |
| `--drag=x1,y1,x2,y2` | Simulate a mouse drag in window coordinates, then trace the selected text and exit. |
| `--pos=x,y` | Simulate a single click in window coordinates, then trace the caret index and exit. |
| `--keys="<sequence>"` | Simulate a sequence of key presses, tracing the caret and handled state after each press, then exit. |

`--drag`, `--pos` and `--keys` run from a pulse after layout has settled, so the vertical scrollbar and reflow are in place first.

Do not write test output over `testcases/example-reply.html`. Use `--export` or `--nodes` with another file name.

## Key sequences (`--keys`)

The sequence is a comma-separated list of key presses:

```
./htmleditor testcases/example-reply.html --keys="ctrl+end,up*10,end,pageup"
```

- `*N` repeats a key N times, for example `up*10`.
- Modifiers `ctrl+`, `shift+` and `alt+` can be combined, for example `ctrl+shift+end`.
- Named keys: `home`, `end`, `up`, `down`, `left`, `right`, `pageup`, `pagedown`, `backspace`, `delete`, `return`, `tab`, `space`, `escape`.
- Single letters `a`-`z` and digits `0`-`9` are also accepted.
- Unknown keys are reported as `Unknown key '<name>'` and skipped.

Each press traces one line:

```
up x3 -> caret 2352 handled=1
```

`caret` is the global character index after the press, and `handled` shows whether the editor consumed the key.

The probe does not change the window size, so results depend on the default window height. Page keys use the visible content height, so PageUp and PageDown results change with the window size.

## Node dump (`--nodes`)

`--nodes` writes the block tree as JSON. Top-level nodes are in `blocks`. Container children are in `children` (which may be `null`). Each node has `bounds` with `x1`, `y1`, `x2` and `y2` as strings. Use it to inspect layout, such as table widths and cell boxes, and colour data.
