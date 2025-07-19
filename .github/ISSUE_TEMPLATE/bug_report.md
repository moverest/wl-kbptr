---
name: Bug report
about: Create a report to help us improve
title: ''
labels: ''
assignees: ''

---

First describe the bug.

You should be able to answer the following questions:
 - How to reproduce the bug?
 - What is going wrong?
 - What is the expected behaviour?

It is helpful to provide screenshots and screencasts.

Then give the following information:

- **Version** of `wl-kbptr` used. Use `wl-kbptr --version` to get this. Ideally, check if the issue is present in `main` and use the commit hash as the version and state the enabled features.
- **Compositor** used and its version. This is important as compositors have their own implementation of the Wayland compositor even if they can share code or use the same library.
- **Distribution** used. Whilst it might not always be relevant, distributions can package the program differently or apply patches.
- You **display settings** (optional). Sometimes some bug only happen with some display settings. You can get this information with `wlr-randr` on `wlroots` compositors.

If you are not sure about something (e.g. how to get a piece of information, or how to reproduce the bug), don't worry just state what you are able to provide and we'll try to investigate together.
