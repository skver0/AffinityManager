# CPU Affinity Manager

This program allows you to manage CPU affinity for specific processes on your system, ensuring that certain processes run on specific CPU cores. It can be configured through a simple `config.ini` file and provides a system tray icon for easy access to the configuration and settings.

## Requirements

- **Windows XP** or newer (compiled for Windows XP), works on Windows 2000 with BWC kernel extensions and VC++ 2019 runtime
- **Visual Studio 2019** for building the project

## Installation

1. Download the latest release from the [Releases](https://github.com/skver0/AffinityManager/releases) page.
2. Extract the ZIP archive to a folder on your system.
3. Configure the `config.ini` file as needed (see Configuration section below).
4. Run `AffinityManager.exe` to start the program.
5. (Optional) Create a shortcut to `AffinityManager.exe` in the Startup folder to run the program on system startup.

## Compilation

1. Clone this repository or download the ZIP archive of the project.
2. Open the solution file `AffinityManager.sln` in Visual Studio 2019.
3. Set the build configuration to `Release` or `Debug`.
4. Build the project in Visual Studio (`Build -> Build Solution`).
5. The program will output an executable file `AffinityManager.exe` in the `Debug` or `Release` folder.

## Configuration

The program reads from the `config.ini` file to determine which processes to manage and their corresponding CPU core assignments.

### Example `config.ini`

```ini
C:\Program Files\ExampleApp\example.exe=0,1
anotherapp.exe=2,3
```

In this example:
- `example.exe` will be assigned to CPU cores 0 and 1.
- `anotherapp.exe` will be assigned to CPU cores 2 and 3.

### Process Path and Core List
- **Process Path:** Full path to the executable of the process you want to manage.
- **Core List:** A comma-separated list of CPU cores (0-based index) to assign to the process.

### Log File
All operations, including affinity assignments, errors, and warnings, are logged in the `affinity.log` file. This log can be used for debugging or tracking activity.

## Usage

1. **Run the Program:**
   - Double-click on `AffinityManager.exe` to run the program. It will start in the system tray, where it will monitor processes and apply CPU affinity as configured in `config.ini`.

2. **Tray Menu:**
   - Right-click the system tray icon to access the tray menu.
   - **Reload Config:** Reload the `config.ini` file to apply any changes.
   - **Exit:** Close the program.

3. **Monitor Processes:**
   - The program will continuously monitor running processes and set their CPU affinity based on the configuration. You can stop the program by clicking "Exit" in the tray menu or closing it from the task manager.

## Building and Debugging

The solution is configured for Visual Studio 2019. You can open and build the project using the provided `.sln` solution file. Debug output will be shown in a console window when compiled in Debug mode.

Though you can just compile main.cpp with any C++ compiler, the project file is provided for convenience.

### Debugging Console
If you need to debug, the program attaches a debug console (`AllocConsole`) that outputs logs to the console as well as to the `affinity.log` file.

## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.
