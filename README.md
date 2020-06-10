# cbonsai

`cbonsai` is a bonsai tree generator, written in `C` using `ncurses`. It intelligently creates, colors, and positions a bonsai tree, and is entirely configurable via CLI options-- see [usage](#usage). There are 2 modes of operation: `static` (the default), and `live`. See [modes](#modes) for details.

`cbonsai` is fairly new and is always looking for ideas for improvement. Feel free to open an issue if you've got an idea or a bug.

## Installation

At this time, only manual installation is possible.

## Usage

```
cbonsai
```

## Modes

### Static

`static` mode is the default: the user only sees the final, completed tree as a picture.

### Live

`live` mode displays each "step" of growth and waits a little bit, so that the user can watch the tree being grown step by step.

## How it Works

`cbonsai` starts by drawing the base onto the screen, which is basically just a static string of characters. To generate the actual tree, `cbonsai` uses a ~~bunch of if statements~~ homemade algorithm to decide how the tree should grow every step. Shoots to the left and right are generated as the main trunk grows. As any branch dies, it branches out into a bunch of leaves.

`cbonsai` has rules for which character and color it should use for each tiny branch piece, depending on things like what type of branch it is and what direction it's facing.

The algorithm is tweaked to look best at the default size, so larger sized trees may not be as bonsai-like.

## Inspiration

This project wouldn't be here if it weren't for its *roots*! `cbonsai` is a newer version of  [bonsai.sh](https://gitlab.com/jallbrit/bonsai.sh), which was written in `bash`. `cbonsai` is now written in `C` with additional improvements and continuing development. `bonsai.sh` itself is a port of [this bonsai tree generator](http://andai.tv/bonsai/), written in JS.