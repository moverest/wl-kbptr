# wl-kbptr

`wl-kbptr` &mdash; short for Wayland Keyboard Pointer &mdash; is a utility to help move the mouse pointer with the keyboard.

See [Supported compositors](#supported-compositors) section for compatibility.

## Modes

To enable to select a target and click, it has four different modes:
- [`floating`](#floating-mode) &mdash; which uses arbitrary areas either given by the user or detected,
- [`tile`](#tile-mode) &mdash; which uses a grid to select areas,
- [`bisect`](#bisect-mode) &mdash; which enables to bisect an area,
- and [`click`](#click-mode) &mdash; which triggers a click in the middle of an area.

These are set with the `modes` configuration field and can be chained, e.g. `wl-kbptr -o modes=tile,bisect`.

### Floating mode
![Demo](https://github.com/user-attachments/assets/9561549c-96cb-4b35-8c81-a96c6e2d725f)

The `floating` mode uses arbitrary selection areas that can be passed by the user through the standard input. Each input line represents an area defined with the `wxh+x+y` format.

The areas can also be automatically detected with by setting the `mode_floating.source` to `detect`, e.g. `wl-kbptr -o modes=floating,click -o mode_floating.source=detect`. This requires the `wl-kbptr` binary to be built with the `opencv` feature &mdash; `meson build -Dopencv=enabled` &mdash; and the compositor to support the [`wlr-screencopy-unstable-v1`](https://wayland.app/protocols/wlr-screencopy-unstable-v1) protocol.


### Tile mode
![Demo](https://github.com/user-attachments/assets/0c7de4fb-f525-428d-8051-941dc64417d8)

The `tile` mode displays a grid. To select an area, simply type the label associated with the tile you want to select.

### Bisect mode
![Demo](https://github.com/user-attachments/assets/ea1305e9-d798-4aa6-9bc1-6227dcbcff54)

The `bisect` mode enables to bisect a given area. At any point the cursor can be moved at the location marked by the red marker by pressing `Enter` or `Space`. A left, right and middle click can be made by pressing the `g`, `h` and `b` keys respectively on a QWERTY keyboard layout. Note that other layout will use the same keys positions, e.g. `i`, `d`, and `x` with a Dvorak keyboard layout.


### Click mode

The `click` mode simply triggers a click in the middle of the selection area.

## Supported compositors

For `wl-kbptr` to work, it requires the following protocols:
 - [`wlr-layer-shell-unstable-v1`](https://wayland.app/protocols/wlr-layer-shell-unstable-v1) for the program to be displays on top,
 - [`wlr-virtual-pointer-unstable-v1`](https://wayland.app/protocols/wlr-virtual-pointer-unstable-v1) to control the mouse pointer,
 - and [`wlr-screencopy-unstable-v1`](https://wayland.app/protocols/wlr-screencopy-unstable-v1) (optional) to capture the screen for target detection in the `floating` mode.

Here are the compositors with which it has been tested:

| Compositor | Supported | Notes |
| ---------- | --------- | ----- |
| Sway       | ✅        | -     |
| Hyprland   | ✅        | -     |
| Niri       | ✅        | An initial area needs to be specified with `-r` or `--restrict`, e.g. `-r 10000x10000+0+0`. |
| KDE        | ❗        | The compositor doesn't support the [`wlr-virtual-pointer-unstable-v1`](https://wayland.app/protocols/wlr-virtual-pointer-unstable-v1) and [`wlr-screencopy-unstable-v1`](https://wayland.app/protocols/wlr-screencopy-unstable-v1) protocols. It can still work with the `--print-only` option and the cursor pointer can then be moved with `ydotool` or similar. |
| Mutter     | ❌        | The compositor doesn't support any of the required protocols. |

## Installation

### Arch Linux

If you are using Arch Linux, you can install the [`wl-kbptr` AUR package](https://aur.archlinux.org/packages/wl-kbptr).

You can build and install the package with:

```bash
curl 'https://aur.archlinux.org/cgit/aur.git/tree/PKGBUILD?h=wl-kbptr' -O PKGBUILD
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
meson build
meson compile -C build
```

If you want to build the target detection feature (see [floating mode](floating-mode)), you need to enable the `opencv` feature:

```bash
meson build -Dopencv=enabled
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
# If you do not use cursor timeout or cursor:hide_on_key_press, you can delete its respective cals
bind=,escape,exec,hyprctl keyword cursor:inactive_timeout 3; hyprctl keyword cursor:hide_on_key_press true; hyprctl dispatch submap reset 

submap = reset

# Entrypoint
# If you do not use cursor timeout or cursor:hide_on_key_press, you can delete its respective cals
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
