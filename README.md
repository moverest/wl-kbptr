# wl-kbptr

`wl-kbptr` &mdash; short for Wayland Keyboard Pointer &mdash; is a utility enabling the control the mouse pointer with the keyboard.

![demo](https://user-images.githubusercontent.com/19509728/235551237-c19a53ba-475f-4670-b9ec-2a84138094c9.gif)

Note that your compositor needs to support the [`wlr-layer-shell-unstable-v1`](https://wayland.app/protocols/wlr-layer-shell-unstable-v1) and [`wlr-virtual-pointer-unstable-v1`](https://wayland.app/protocols/wlr-virtual-pointer-unstable-v1) protocols.

## Installation

If you are using Arch Linux, you can install the [`wl-kbptr` AUR package](https://aur.archlinux.org/packages/wl-kbptr).

Otherwise, you can build it with:

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
    bindsym g mode default, exec 'wl-kbptr; swaymsg mode Mouse'

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

## Dependencies

- `xkbcommon`
- `cairo`
- `wayland-client`
- `wayland-protocols`


## License

[GPL-3.0](./LICENSE)
