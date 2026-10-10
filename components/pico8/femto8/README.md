# femto8

femto8 is an open-source reimplementation of the [PICO-8](https://www.lexaloffle.com/pico-8.php) fantasy console, designed specifically for embedded systems with smaller CPUs and less memory. It provides a platform to play PICO-8 games on devices with resource constraints, making it ideal for environments with limited hardware capabilities.

## Acknowledgment

This project has been significantly influenced by the work of [Jacopo Santoni](https://github.com/Jakz) on [retro8](https://github.com/Jakz/retro8), a PICO-8 emulator designed for desktop platforms. We would like to express our gratitude to Jacopo Santoni for his contributions and efforts on retro8, which has been an invaluable resource for the development of femto8.

## Key Features

- Written in C for optimal performance on embedded systems.
- Compact and resource-efficient design, suitable for devices with smaller CPUs and limited memory.

## Screenshots

![](/images/screenshot1.png)

![](/images/screenshot2.png)

![](/images/screenshot3.png)

## Building

To build femto8 (desktop):

1. Clone the repository: `git clone https://github.com/benbaker76/femto8.git`
2. Navigate to the femto8 directory: `cd femto8`
3. Install SDL2 development files for your platform:
	- Debian/Ubuntu: `sudo apt install libsdl2-dev pkg-config`
	- Fedora: `sudo dnf install SDL2-devel pkgconfig`
	- macOS (Homebrew): `brew install sdl2 pkg-config`
	- MSYS2 (Windows): install `mingw-w64-*-SDL2` and `pkgconf` via pacman
4. Build a local binary: `make`

## Credits

- [benbaker76](https://github.com/benbaker76) - Author and maintainer of [femto8](https://github.com/benbaker76/femto8)
- [Jacopo Santoni](https://github.com/Jakz) - Author of [retro8](https://github.com/Jakz/retro8)
- [Lexaloffle](https://www.lexaloffle.com/) - Creator of PICO-8
- [atomice1](https://github.com/atomice1) - For updates and improvements
