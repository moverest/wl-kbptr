# wl-kbptr

`wl-kbptr` &mdash; short for Wayland Keyboard Pointer &mdash; is a utility to help move the mouse pointer with the keyboard.

![demo](https://user-images.githubusercontent.com/19509728/235551237-c19a53ba-475f-4670-b9ec-2a84138094c9.gif)

It works in two phases: the screen is first in mapped with three letter labels and then the selected area is refined by bisecting it. At any point the cursor can be moved at the location marked by the red marker by pressing `Enter` or `Space`. A left, right and middle click can be made by pressing the `g`, `h` and `b` keys respectively.

Note that your compositor needs to support the [`wlr-layer-shell-unstable-v1`](https://wayland.app/protocols/wlr-layer-shell-unstable-v1) and [`wlr-virtual-pointer-unstable-v1`](https://wayland.app/protocols/wlr-virtual-pointer-unstable-v1) protocols.

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

### From sources

You can build from sources with:

```bash
meson build
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

bindsym $mod+g mode Mouse
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


## License

[GPL-3.0](./LICENSE)
