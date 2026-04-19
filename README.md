# Animura

**Animura** is a modular live wallpaper engine for Windows built with **Qt (QML + C++)**.  
It allows dynamic wallpaper modules to render directly on the desktop while providing a flexible **JSON‑driven settings system** and a clean QML interface for configuration.

Unlike traditional wallpaper changers, Animura runs real‑time wallpaper modules that can animate, react to mouse movement and span across multiple monitors.

---

# Features

- **Live wallpaper modules**
  - Load wallpaper modules dynamically from DLLs
  - Modules run in their own update loop

- **Desktop integration**
  - Attaches rendering window directly to the desktop layer
  - Works across **multiple monitors**

- **Dynamic settings system**
  - Module settings defined using **JSON schema**
  - UI generated automatically in QML
  - Validation for types, ranges, and allowed values

- **System tray support**
  - Minimize Animura to the tray
  - Restore window or quit from tray menu

- **Accent color integration**
  - Automatically reads Windows accent color for style

- **Wallpaper safety**
  - Restores the original wallpaper automatically when the module stops

---

# Architecture

Animura is built around a **plugin architecture**.

```
Animura
 ├─ QML UI
 │   └─ Settings editor generated from JSON schema
 │
 ├─ WallpaperController (C++)
 │   ├─ Module discovery
 │   ├─ Module loading/unloading
 │   ├─ Settings validation
 │   └─ Runtime management
 │
 └─ Wallpaper Modules (DLL)
     └─ Implement IWallpaperModule
```

Modules are loaded dynamically at runtime using `LoadLibrary` and expose a factory entry function.

---

# Module Interface

Each wallpaper module must implement the `IWallpaperModule` interface.

```cpp
class IWallpaperModule {
public:
    virtual ~IWallpaperModule() = default;

    virtual void run() = 0;
    virtual void stop() = 0;

    virtual HWND hwnd() const = 0;
};
```

---

# Module Metadata

Modules are placed inside the `/modules` folder

Example:

```
Animura
 └─ modules
    ├─ delaunay-flow
    │  ├─ module.dll
    │  ├─ module.json
    │  ├─ setings.json
    │  ├─ schema.json
    │  └─ preview.png
    └─ eclipse-frame
       ├─ module.dll
       ├─ module.json
       ├─ setings.json
       ├─ schema.json
       └─ preview.png
```

The **schema** defines how the UI is generated and how values are validated.

---

# Settings System

Animura validates settings against the schema before modules are loaded.

Supported schema constraints:

- `type`
  - `int`
  - `float`
  - `bool`
  - `string`

- `min` / `max`
- `options` (allowed values)

Nested objects are also supported.

---

# Rendering Model

Modules render directly to the **desktop layer** instead of changing the wallpaper image.

Advantages:

- Instant updates
- No Windows wallpaper fade animation
- Supports real-time rendering
- Works across multiple monitors

The desktop surface is acquired using utilities from: `wallpaper-host/desktop_utils.hpp`

---

# Technology Stack

- **C++20**
- **Qt 6 (QML + Qt Quick)**
- **Win32 API**
- **nlohmann/json**
- **glfw3**

---

# Project Structure

```
Animura/
 ├─ Source Files/
 │  ├─ qml/
 │  │  ├─ Components
 │  │  │  ├─ SettingsPanel.qml
 │  │  │  └─ WallpaperCard.qml
 │  │  └─ main.qml
 │  ├─ WallpaperController.cpp
 │  └─ main.cpp
 │
 ├─ Header Files/
 │  └─ WallpaperController.hpp
 │
 ├─ modules/
 │   └─ wallpaper module DLLs
 │
 └─ resources/
    ├─ icon.ico
    └─ qml.qrc
```

---

# Versioning

Animura follows:

```
Major.Minor.Patch.Build
```

Example:

```
1.1.0.0
```

- **Major** – breaking architecture changes
- **Minor** – new features
- **Patch** – bug fixes
- **Build** – internal revisions

---

# Goals

Animura is designed to be:

- **Modular** – anyone can build new wallpaper modules
- **Lightweight** – minimal overhead compared to game‑engine based wallpaper apps
- **Extensible** – JSON schema allows rapid UI creation
- **Stable** – original wallpaper is always restored

---

# License

This project is licensed under the [MIT License](LICENSE.txt).

---

# Status

Animura is currently in **early development**.

Core systems implemented:

- module loader
- wallpaper runtime
- schema validation
- dynamic settings UI
- system tray support
