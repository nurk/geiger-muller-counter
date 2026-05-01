# OLED Display Layout

128 × 64 pixel SSD1306, updated every 250 ms.

## Screen regions

```
pixel y
  0  ┌────────────────────────────┐
     │ Total : 1234567            │  ← total lifetime pulse count
 10  │ Up    : 02:14:37           │  ← uptime  (or  1d 02:14:37)
 20  │ CPM   : 23*              ^1│  ← rolling CPM / graph scale (* = warming up)
     │                            │
 33  ├────────────────────────────┤  ← divider
 36  │▁  ▂▁▃ ▂  ▁▃▂▄▃▂▁▂▃▂▄▃▂▁▂▃▂▄│  ← 5-minute CPM graph (28 px tall)
 64  └────────────────────────────┘
```

## Stats area (y 0 – 42)

| Line | y  | Content | Example |
|------|----|---------|---------|
| 1 | 0  | `Total : ` + value at x=48 | `Total : 1234567` |
| 2 | 10 | `Up    : ` + value at x=48 | `Up    : 02:14:37` |
| 3 | 20 | `CPM   : ` + value at x=48, `^N` right-aligned | `CPM   : 23*        ^1` |

Labels are padded to 8 characters (including colon and space) so the colon lands at
x=36, the space at x=42, and all values start at x=48. At size-1 (6 px/char):

```
Total : 1234567
Up    : 02:14:37
CPM   : 23*          ^1
```

> **`^N` scale label** — right-aligned on the CPM line. N is the pulse count of the
> tallest 1-second bucket in the graph window. It indicates the full-scale height of
> the graph. At background levels this is typically `^1` (one pulse in the busiest second).

> **Warming-up asterisk (`*`)** — shown next to the CPM value for the first 5 minutes
> (300 seconds) while the rolling window is not yet fully populated.
> Once the window is full the asterisk disappears and the CPM value is fully accurate.

## Graph area (y 36 – 63, 28 px tall)

- Spans the full 128 px width.
- Covers the last **5 minutes** (300 one-second buckets).
- Each pixel column represents ~2.3 seconds of data.
- **Left = oldest**, **right = most recent**.
- Auto-scales vertically to the tallest column in view.
- Scale reference (`^N`) is shown right-aligned on the CPM line in the stats area.

### Example — background radiation (~20 CPM, J321 tube)

```
y=36 ┤                                                                     ┤
     │                  █                      █                           │
     │                  █                      █                           │
     │        █         █     █       █        █       █      █            │
     │        █         █     █       █        █       █      █            │
     │   █    █    █    █     █   █   █   █    █  █    █  █   █   █    █   │
     │   █    █    █    █     █   █   █   █    █  █    █  █   █   █    █   │
     │   █  █ █  █ █  █ █  █  █   █   █   █ █  █  █  █ █  █   █   █  █ █  █│
y=63 └─────────────────────────────────────────────────────────────────────┘
      ↑ 5 min ago                                                  now ↑
```

### Example — elevated source nearby (~200 CPM)

```
Total : 5821
Up    : 00:12:44
CPM   : 187              ^8
───────────────────────────
███████████████████████████

```

Full graph view:

```
y=36 ┤                                                                       ┤
     │█ ██ █ ██  █ ██ ███ █ ██ ██ █ █ ███ ██ █ ███ ██ ██ ███ ██ ███ ██ ███   │
     │█ ██ █ ██  █ ██ ███ █ ██ ██ █ █ ███ ██ █ ███ ██ ██ ███ ██ ███ ██ ███   │
     │███████████████████████████████████████████████████████████████████████│
     │███████████████████████████████████████████████████████████████████████│
     │███████████████████████████████████████████████████████████████████████│
     │███████████████████████████████████████████████████████████████████████│
y=63 └───────────────────────────────────────────────────────────────────────┘
```

## Full screen examples

### Normal background — first minute (warming up)

```
┌────────────────────────────┐
│ Total : 18                 │
│ Up    : 00:00:47           │
│ CPM   : 23*              ^1│
├────────────────────────────┤
│         ▁  ▁   ▂ ▁  ▁  ▁▁  │
└────────────────────────────┘
```

### Normal background — after 5 minutes (window full)

```
┌────────────────────────────┐
│ Total : 1847               │
│ Up    : 01:32:10           │
│ CPM   : 21               ^1│
├────────────────────────────┤
│▁ ▂▁▁▂▁▃▂▁▂▁▂▃▁▂▁▁▂▁▂▂▁▃▁▂▁▂│
└────────────────────────────┘
```

### Long uptime

```
┌────────────────────────────┐
│ Total : 2891044            │
│ Up    : 4d 09:22:05        │
│ CPM   : 19               ^1│
├────────────────────────────┤
│▂▁▂▁▁▂▁▂▁▃▁▂▁▂▁▁▂▁▂▂▁▃▁▂▁▂▁▂│
└────────────────────────────┘
```
