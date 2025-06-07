# wl-kbptr

`wl-kbptr` &mdash; short for Wayland Keyboard Pointer &mdash; is a utility to help move the mouse pointer with the keyboard.

See [Supported compositors](#supported-compositors) section for compatibility.

## Modes

To enable to select a target and click, it has four different modes:
- [`floating`](#floating-mode) &mdash; which uses arbitrary areas either given by the user or detected,
- [`tile`](#tile-mode) &mdash; which uses a grid to select areas,
- [`bisect`](#bisect-mode) &mdash; which enables to bisect an area,
- [`split`](#split-mode) &mdash; which enables to successively split an area,
- and [`click`](#click-mode) &mdash; which triggers a click in the middle of an area.

These are set with the `modes` configuration field and can be chained, e.g. `wl-kbptr -o modes=tile,bisect`.

Note that if you make a mistake &mdash; e.g. select the wrong area &mdash; you can always go back on step by pressing the `Backspace` key. This even works between modes.

### Floating mode
[Floating Mode Demo](https://github.com/user-attachments/assets/1598128b-e03b-4d06-b47a-bc8a0021f4da)

The `floating` mode uses arbitrary selection areas that can be passed by the user through the standard input. Each input line represents an area defined with the `wxh+x+y` format.

#### Auto-detection
The areas can also be automatically detected with `mode_floating.source` configuration set to `detect`, e.g. `wl-kbptr -o modes=floating,click -o mode_floating.source=detect`.

This requires the `wl-kbptr` binary to be built with the `opencv` feature and the compositor to support the [`wlr-screencopy-unstable-v1`](https://wayland.app/protocols/wlr-screencopy-unstable-v1) protocol &mdash; see the [supported compositors](#supported-compositors) section and [build instructions](#from-sources) for details. Whilst it doesn't noticeably change the size of the program itself, OpenCV is a 100 MB+ dependency which is not ideal if you want a very small system which is why this is an optional feature.

Most distributions will package the program with the option enabled. If not, they will usually provide two packages. You can check if the binary you have has been built with it with `wl-kbptr --version` &mdash; it should print `opencv` if supported.

### Tile mode
[Tile Mode Demo](https://github.com/user-attachments/assets/d8c9c8dc-2733-4835-9d82-d0f5b093c382)

The `tile` mode displays a grid. To select an area, simply type the label associated with the tile you want to select.

### Bisect mode
[Bisect Mode Demo](https://github.com/user-attachments/assets/8f8f7fb4-1bb9-4180-9eda-78ee1ff14181)

The `bisect` mode enables to bisect a given area. At any point the cursor can be moved at the location marked by the red marker by pressing `Enter` or `Space`.

A left, right and middle click can be made by pressing the `g`, `h` and `b` keys respectively on a QWERTY keyboard layout. Note that other layout will use the same keys positions, e.g. `i`, `d`, and `x` with a Dvorak keyboard layout.

### Split mode
[Split Mode Demo](https://github.com/user-attachments/assets/760fa154-ce50-47b4-8f9a-26c5ac79a55b)

The `split` mode enables to successively split an area with the arrow keys.

Just like the `bisect` mode, a left, right and middle click can be made by pressing the `g`, `h` and `b` keys respectively on a QWERTY keyboard layout.

### Click mode

The `click` mode simply triggers a click in the middle of the selection area.

## Supported compositors

For `wl-kbptr` to work, it requires the following protocols:
 - [`wlr-layer-shell-unstable-v1`](https://wayland.app/protocols/wlr-layer-shell-unstable-v1) for the program to display on top,
 - [`wlr-virtual-pointer-unstable-v1`](https://wayland.app/protocols/wlr-virtual-pointer-unstable-v1) to control the mouse pointer,
 - and [`wlr-screencopy-unstable-v1`](https://wayland.app/protocols/wlr-screencopy-unstable-v1) (optional) to capture the screen for target detection in the `floating` mode.

Here are the compositors with which it has been tested:

| Compositor |     | Notes |
| ---------- | --- | ----- |
| [Sway](https://swaywm.org) | ✅ | - |
| [Hyprland](https://hyprland.org) | ✅ | - |
| [Niri](https://github.com/YaLTeR/niri) | ✅ | - |
| [dwl](https://codeberg.org/dwl/dwl) | ✅ | - |
| [KWin](https://github.com/KDE/kwin) | ❗ | The compositor doesn't support the [`wlr-virtual-pointer-unstable-v1`](https://wayland.app/protocols/wlr-virtual-pointer-unstable-v1) and [`wlr-screencopy-unstable-v1`](https://wayland.app/protocols/wlr-screencopy-unstable-v1) protocols. It can still work with the `--print-only` option and the mouse pointer can then be moved with `ydotool` or similar. |
| [Mutter](https://mutter.gnome.org) | ❌ | The compositor doesn't support any of the required protocols. |

## Installation

### Arch Linux

If you are using Arch Linux, you can install the [`wl-kbptr` AUR package](https://aur.archlinux.org/packages/wl-kbptr).

You can build and install the package with:

```bash
curl 'https://aur.archlinux.org/cgit/aur.git/tree/PKGBUILD?h=wl-kbptr' -o PKGBUILD
makepkg -si
```

### NixOS

If you are using NixOS, you can install the [`wl-kbptr` package](https://search.nixos.org/packages?query=wl-kbptr). Nix can also be used to install `wl-kbptr` on other distributions.

### Chimera Linux

If you are using Chimera Linux, you can install the [`wl-kbptr` package](https://pkgs.chimera-linux.org/package/current/contrib/x86_64/wl-kbptr) which is available in the [contrib repository](https://chimera-linux.org/docs/apk#repositories).

```bash
apk add chimera-repo-contrib
apk add wl-kbptr
```
### Fedora

As of now there is no official package through dnf. So you will need to install the dependencies and then run the steps described in the **From sources** section.

```bash
sudo dnf install -y \
    meson \
    wayland-protocols-devel \
    gcc \
    libwayland-client  \
    cmake \
    wayland-devel \
    libxkbcommon-devel \
    cairo-devel
```

### From sources

You can build from sources with:

```bash
meson setup build --buildtype=release
meson compile -C build
```

If you want to build the target detection feature (see [floating mode](#floating-mode)), you need to enable the `opencv` feature:

```bash
meson setup build --buildtype=release -Dopencv=enabled
meson compile -C build
```

Then install with:

```bash
meson install -C build
```

## Setting the bindings

### Sway

```
mode Mouse {
    bindsym a mode default, exec 'wl-kbptr-sway-active-win; swaymsg mode Mouse'
    bindsym Shift+a mode default, exec 'wl-kbptr; swaymsg mode Mouse'

    # Mouse move
    bindsym h seat seat0 cursor move -15 0
    bindsym j seat seat0 cursor move 0 15
    bindsym k seat seat0 cursor move 0 -15
    bindsym l seat seat0 cursor move 15 0

    # Left button
    bindsym s seat seat0 cursor press button1
    bindsym --release s seat seat0 cursor release button1

    # Middle button
    bindsym d seat seat0 cursor press button2
    bindsym --release d seat seat0 cursor release button2

    # Right button
    bindsym f seat seat0 cursor press button3
    bindsym --release f seat seat0 cursor release button3

    bindsym Escape mode default
}

bindsym $mod+g exec wl-kbptr-sway-active-win -o modes=floating','click -o mode_floating.source=detect
bindsym $mod+Shift+g mode Mouse
```

### Hyprland

```
# Cursor submap (similar to the Mouse mode in Sway)
submap=cursor

# Jump cursor to a position
bind=,a,exec,hyprctl dispatch submap reset && wl-kbptr && hyprctl dispatch submap cursor

# Cursor movement
binde=,j,exec,wlrctl pointer move 0 10
binde=,k,exec,wlrctl pointer move 0 -10
binde=,l,exec,wlrctl pointer move 10 0
binde=,h,exec,wlrctl pointer move -10 0

# Left button
bind=,s,exec,wlrctl pointer click left
# Middle button
bind=,d,exec,wlrctl pointer click middle
# Right button
bind=,f,exec,wlrctl pointer click right

# Scroll up and down
binde=,e,exec,wlrctl pointer scroll 10 0
binde=,r,exec,wlrctl pointer scroll -10 0

# Scroll left and right
binde=,t,exec,wlrctl pointer scroll 0 -10
binde=,g,exec,wlrctl pointer scroll 0 10

# Exit cursor submap
# If you do not use cursor timeout or cursor:hide_on_key_press, you can delete its respective calls.
bind=,escape,exec,hyprctl keyword cursor:inactive_timeout 3; hyprctl keyword cursor:hide_on_key_press true; hyprctl dispatch submap reset 

submap = reset

# Entrypoint
# If you do not use cursor timeout or cursor:hide_on_key_press, you can delete its respective calls.
bind=$mainMod,g,exec,hyprctl keyword cursor:inactive_timeout 0; hyprctl keyword cursor:hide_on_key_press false; hyprctl dispatch submap cursor
```

## Configuration

`wl-kbptr` can be configured with a configuration file. See [`config.example`](./config.example) for an example and run `wl-kbptr --help-config` for help.

## Dependencies

- [`xkbcommon`](https://xkbcommon.org)
- [`cairo`](https://cairographics.org)
- [`wayland`](https://wayland.freedesktop.org)
- [`wayland-protocols`](https://gitlab.freedesktop.org/wayland/wayland-protocols)
- With the `opencv` feature enabled:
  - C++ compiler
  - [`OpenCV`](https://opencv.org)
  - [`Pixman`](https://www.pixman.org)


## License

[GPL-3.0](./LICENSE)
