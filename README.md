# wltypometer

A tool to measure input latency on Wayland.

## How it works

1. You specify a rectangle to watch on the CLI.
2. During startup, pixels in this rectangles are copied.
3. A key press will be simulated.
4. The screen is recorded. When a frame is received, the rectangle's pixels are
   compared against the ones copied during startup.
5. When they differ, then the time elapsed between the key press being simulated and the frame being received is logged to stdout in nanoseconds.
6. A backspace key press is simulated to remove the character, and the tool
   keeps comparing the rectangle's pixels against the ones copied during
   startup.
7. When they match, it goes back to step 3.

## Building

Run these commands:

```sh
meson build/
ninja -C build/
```

An executable will be built at `build/wltypometer`.

## Running

Install [slurp](https://github.com/emersion/slurp) to make selecting a rectangle
on the screen easier.

Prepare the application whose input latency has to be measured, and run the
following command. You will have to choose the rectangle that must be watched:
use a small rectangle around the text cursor. Then, quickly focus the
application's window in order for the key presses to be sent to it.

```sh
build/wltypometer $(slurp && sleep 5)
```
