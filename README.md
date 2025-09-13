# <img src="https://github.com/Nyabsi/OpenFps/blob/master/resources/icon.png" height="24" width="24"> OpenFps

OpenFps is an performance monitoring and utility tool for OpenVR compatible runtimes for aiding to identify performance issues.

![showcase](https://github.com/user-attachments/assets/a00a6ab2-e0c9-4b56-9152-b53b1bbffc56)

## Features

- CPU & GPU frame time metrics
- Real-time FPS display
- Reprojected & Dropped frame counter
- VRAM monitoring
	- See how much VRAM your application is using
- Display device battery percentages
	- Hint: Assign the appropriate tracker roles for your device to identify your trackers
- Accessibility
	- Select between Left/Right hand for displaying the overlay
	- Adjust the scale of the overlay to match your preference
	- Change the mounting position of the overlay (Above, Wrist, Below)
- SteamVR Resolution Adjustment
	- Adjust your SteamVR resolution straight from OpenFps to reduce friction
- Display Color Adjustment
	- Apply colors filters, brightness control directly from OpenFps universally
 
> [!WARNING]
> Display Color Adjustment is only available for native SteamVR headsets.

## Usage

Head over to [Downloads](https://github.com/Nyabsi/OpenFps/releases) to install the latest version of OpenFps.

To be able to read the graphs efficiently, please look at [Reading the Graph](https://github.com/Nyabsi/OpenFps/wiki/Reading-the-Graph) to understand what the colours imply.

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
