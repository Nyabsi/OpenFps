# OpenFps

OpenFps is an performance monitoring and utility tool for OpenVR compatible runtimes for aiding to identify performance issues.

## Features

- CPU & GPU frametime metrics
- Real-time FPS display
- Reprojected & Dropped frame counter
- Display device battery percentages
	- You can use device tracker roles to identify individual devices
- Customizable
	- You can select which hand the overlay is displayed in
	- You can adjust the scale of the overlay window
	- You can change the overlay mounting position

## Usage

W.I.P

## Building

> [!IMPORTANT]
> You need [Vulkan SDK](https://vulkan.lunarg.com/) to build this project, make sure you have it downloaded before proceeding

Make sure you have the required libraries fetched through `git submodule`

```sh
git submodule init && git submodule update
```

Build it through Visual Studio on Windows or if you're using an Unix-like system

```sh
cmake -B build .
cmake --build build
```

## License

This project is licensed under `Source First License 1.1` which can be found from the root of this project named `LICENSE.md`