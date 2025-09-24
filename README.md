# megatron

Terminal file browser for videos. Navigate folders and play videos with VLC.

## Install

```bash
make install
```

Requires Go and VLC.

## Setup

Set your video folder:
```bash
megatron -c
```
Write only one line with the path to your video directory.

Example config.txt:
```
/home/myuser/Filmoteca
```

## Usage

```bash
megatron
```

**Controls:**
- Arrow keys or `hjkl` - navigate
- Enter - play video
- `q` - quit

Plays `.mp4`, `.mkv`, `.avi`, `.mov`, `.flv`, `.wmv` files.

## Remove

```bash
make remove
```
