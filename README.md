# wl-kbptr

`wl-kbptr`, short for Wayland Keyboard Pointer, is a utility to control the mouse pointer with the keyboard.

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

To start the utility with a key shortcut, a binding needs to be set.

### Sway

```
swaymsg bindsym '$mod+d' exec wl-kbptr
```

## Dependencies

- `xkbcommon`
- `cairo`
- `wayland-client`
- `wayland-protocols`


## License

[GPL-3.0](./LICENSE)
