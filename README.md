# <img src="https://github.com/Nyabsi/openvr-metrics/blob/master/resources/icon.png" height="24" width="24"> OpenVR Metrics

OpenVR Metrics is an performance monitoring and utility tool for OpenVR compatible runtimes for aiding to identify performance issues.

## Features

- **Per-Process Monitoring**  
  - Provides detailed insight into resource consumption of individual applications
  - CPU and GPU frame time metrics
  - Real-time FPS display
  - Reprojected and dropped frame counters
- **Resource Monitoring (Per-Process)**  
  - CPU and GPU usage
  - Dedicated and shared VRAM usage
  - Video encoding utilization
- **Process List**
  - View applications currently consuming system resources
- **Display Device Battery Levels**
  - Assign tracker roles to identify connected devices
- **Accessibility**
  - Select overlay for left or right hand
  - Adjust overlay scale and mounting position (Above, Wrist, Below)
- **SteamVR Resolution Adjustment**
  - Modify SteamVR resolution directly within OpenVR Metrics
- **Display Color Adjustment**  
  - Apply color filters and brightness controls universally via OpenVR Metrics

## Usage

Head over to [Downloads](https://github.com/Nyabsi/OpenVR Metrics/releases) to install the latest version of OpenVR Metrics.

To be able to read the graphs efficiently, please look at [Reading the Graph](https://github.com/Nyabsi/OpenVR Metrics/wiki/Reading-the-Graph) to understand what the colours imply.

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
