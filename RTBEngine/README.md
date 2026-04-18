# RTBEngine

A 3D game engine written in **C++17**, compiled as `RTBEngine.dll` with an import library (`RTBEngine.lib`). RTBEngine provides a complete set of subsystems for building games and interactive real-time applications on Windows: an Entity-Component-System architecture, OpenGL rendering with shadow mapping, Bullet-based rigid-body physics, FMOD spatial audio, SDL2 input, skeletal animation, Lua-based scene serialization, a macro-driven reflection system, and an ImGui-compatible UI framework.

This document covers every subsystem in depth — public API, internal design, data flow, and usage patterns.

---

## Table of Contents

1. [Project Structure](#1-project-structure)
2. [Build System](#2-build-system)
3. [Third-Party Libraries](#3-third-party-libraries)
4. [Architecture Overview](#4-architecture-overview)
5. [Core Subsystem](#5-core-subsystem)
   - 5.1 [ApplicationConfig](#51-applicationconfig)
   - 5.2 [Application](#52-application)
   - 5.3 [Window](#53-window)
   - 5.4 [Logger](#54-logger)
   - 5.5 [ResourceManager](#55-resourcemanager)
6. [ECS Subsystem](#6-ecs-subsystem)
   - 6.1 [Component](#61-component)
   - 6.2 [Transform](#62-transform)
   - 6.3 [GameObject](#63-gameobject)
   - 6.4 [Scene](#64-scene)
   - 6.5 [SceneManager](#65-scenemanager)
   - 6.6 [Prefab](#66-prefab)
   - 6.7 [PrefabRegistry](#67-prefabregistry)
7. [Built-in Components](#7-built-in-components)
   - 7.1 [MeshRenderer](#71-meshrenderer)
   - 7.2 [CameraComponent](#72-cameracomponent)
   - 7.3 [LightComponent](#73-lightcomponent)
   - 7.4 [RigidBodyComponent](#74-rigidbodycomponent)
   - 7.5 [BoxColliderComponent](#75-boxcollidercomponent)
   - 7.6 [AudioSourceComponent](#76-audiosourcecomponent)
   - 7.7 [FreeLookCamera](#77-freelookcamera)
   - 7.8 [SphereColliderComponent](#78-spherecollidercomponent)
8. [Rendering Subsystem](#8-rendering-subsystem)
   - 8.1 [Camera](#81-camera)
   - 8.2 [Shader](#82-shader)
   - 8.3 [Material](#83-material)
   - 8.4 [Vertex and Mesh](#84-vertex-and-mesh)
   - 8.5 [Texture](#85-texture)
   - 8.6 [FrameBuffer](#86-framebuffer)
   - 8.7 [ModelLoader](#87-modelloader)
   - 8.8 [Skybox](#88-skybox)
   - 8.9 [ShadowMap](#89-shadowmap)
   - 8.10 [Lighting — DirectionalLight](#810-lighting--directionallight)
   - 8.11 [Lighting — PointLight](#811-lighting--pointlight)
   - 8.12 [Lighting — SpotLight](#812-lighting--spotlight)
   - 8.13 [Rendering Pipeline](#813-rendering-pipeline)
   - 8.14 [Frustum](#814-frustum)
   - 8.15 [FbxBinding](#815-fbxbinding)
9. [Physics Subsystem](#9-physics-subsystem)
   - 9.1 [PhysicsWorld](#91-physicsworld)
   - 9.2 [RigidBody](#92-rigidbody)
   - 9.3 [Colliders](#93-colliders)
   - 9.4 [PhysicsSystem](#94-physicssystem)
   - 9.5 [CollisionInfo](#95-collisioninfo)
   - 9.6 [Physics Lifecycle](#96-physics-lifecycle)
   - 9.7 [PhysicsUtils](#97-physicsutils)
10. [Audio Subsystem](#10-audio-subsystem)
    - 10.1 [AudioSystem](#101-audiosystem)
    - 10.2 [AudioClip](#102-audioclip)
11. [Input Subsystem](#11-input-subsystem)
    - 11.1 [InputManager](#111-inputmanager)
    - 11.2 [KeyCode](#112-keycode)
    - 11.3 [MouseButton](#113-mousebutton)
12. [Animation Subsystem](#12-animation-subsystem)
    - 12.1 [Bone](#121-bone)
    - 12.2 [Skeleton](#122-skeleton)
    - 12.3 [AnimationClip](#123-animationclip)
    - 12.4 [Animator](#124-animator)
13. [Math Library](#13-math-library)
    - 13.1 [Vector2](#131-vector2)
    - 13.2 [Vector3](#132-vector3)
    - 13.3 [Vector4](#133-vector4)
    - 13.4 [Quaternion](#134-quaternion)
    - 13.5 [Matrix4](#135-matrix4)
    - 13.6 [Color](#136-color)
14. [Reflection System](#14-reflection-system)
    - 14.1 [PropertyType and PropertyFlags](#141-propertytype-and-propertyflags)
    - 14.2 [PropertyInfo](#142-propertyinfo)
    - 14.3 [TypeInfo](#143-typeinfo)
    - 14.4 [TypeRegistry](#144-typeregistry)
    - 14.5 [PropertyMacros](#145-propertymacros)
    - 14.6 [Writing a Reflectable Component](#146-writing-a-reflectable-component)
    - 14.7 [TypeInfoBuilder](#147-typeinfobuilder)
15. [Scripting and Serialization](#15-scripting-and-serialization)
    - 15.1 [ComponentRegistry](#151-componentregistry)
    - 15.2 [SceneLoader](#152-sceneloader)
    - 15.3 [SceneSaver](#153-scenesaver)
    - 15.4 [ScriptManager](#154-scriptmanager)
    - 15.5 [Latent Actions](#155-latent-actions)
    - 15.6 [PrefabLoader](#156-prefabloader)
    - 15.7 [PrefabSaver](#157-prefabsaver)
    - 15.8 [Scene Serialization Infrastructure](#158-scene-serialization-infrastructure)
16. [UI Subsystem](#16-ui-subsystem)
    - 16.1 [RectTransform](#161-recttransform)
    - 16.2 [UIElement](#162-uielement)
    - 16.3 [Canvas](#163-canvas)
    - 16.4 [CanvasSystem](#164-canvassystem)
    - 16.5 [UIButton](#165-uibutton)
    - 16.6 [UIText](#166-uitext)
    - 16.7 [UIImage](#167-uiimage)
    - 16.8 [UIPanel](#168-uipanel)
    - 16.9 [UIContainer](#169-uicontainer)
    - 16.10 [UIRenderContext](#1610-uirendercontext)
    - 16.11 [EventSystem](#1611-eventsystem)
17. [DLL Boundary Safety](#17-dll-boundary-safety)
18. [Code Conventions](#18-code-conventions)

---

## 1. Project Structure

```
RTBEngine/
├── RTBEngine.sln                  Visual Studio solution
├── SetupDeps.bat                  Downloads and builds all third-party libraries
├── BuildSDK.bat                   Compiles the engine and packages the SDK
│
├── RTBEngine/                     Engine DLL project
│   ├── RTBEngine.h                Master include: pulls in the entire public API
│   ├── RTBEngineAPI.h             RTB_API macro (dllexport / dllimport)
│   │
│   ├── Engine/
│   │   ├── Core/
│   │   │   ├── Application.h / .cpp
│   │   │   ├── Window.h / .cpp
│   │   │   ├── Logger.h / .cpp
│   │   │   ├── ResourceManager.h / .cpp
│   │   │   └── ApplicationConfig.h
│   │   │
│   │   ├── ECS/
│   │   │   ├── Component.h
│   │   │   ├── Transform.h / .cpp
│   │   │   ├── GameObject.h / .cpp
│   │   │   ├── Scene.h / .cpp
│   │   │   ├── SceneManager.h / .cpp
│   │   │   ├── MeshRenderer.h / .cpp
│   │   │   ├── CameraComponent.h / .cpp
│   │   │   ├── LightComponent.h / .cpp
│   │   │   ├── RigidBodyComponent.h / .cpp
│   │   │   ├── BoxColliderComponent.h / .cpp
│   │   │   ├── AudioSourceComponent.h / .cpp
│   │   │   ├── FreeLookCamera.h / .cpp
│   │   │   ├── MissingComponent.h
│   │   │   ├── Prefab.h / .cpp
│   │   │   ├── PrefabRegistry.h / .cpp
│   │   │   └── SphereColliderComponent.h / .cpp
│   │   │
│   │   ├── Rendering/
│   │   │   ├── Camera.h / .cpp
│   │   │   ├── Shader.h / .cpp
│   │   │   ├── Material.h / .cpp
│   │   │   ├── Mesh.h / .cpp
│   │   │   ├── Texture.h / .cpp
│   │   │   ├── FrameBuffer.h / .cpp
│   │   │   ├── ShadowMap.h / .cpp
│   │   │   ├── ModelLoader.h / .cpp
│   │   │   ├── Skybox.h / .cpp
│   │   │   ├── Vertex.h
│   │   │   ├── Font.h / .cpp
│   │   │   ├── Cubemap.h / .cpp
│   │   │   ├── Frustum.h / .cpp
│   │   │   ├── FbxBinding.h / .cpp
│   │   │   └── Lighting/
│   │   │       ├── Light.h
│   │   │       ├── DirectionalLight.h / .cpp
│   │   │       ├── PointLight.h / .cpp
│   │   │       └── SpotLight.h / .cpp
│   │   │
│   │   ├── Physics/
│   │   │   ├── PhysicsWorld.h / .cpp
│   │   │   ├── PhysicsSystem.h / .cpp
│   │   │   ├── RigidBody.h / .cpp
│   │   │   ├── Collider.h
│   │   │   ├── BoxCollider.h / .cpp
│   │   │   ├── SphereCollider.h / .cpp
│   │   │   ├── CollisionInfo.h
│   │   │   └── PhysicsUtils.h
│   │   │
│   │   ├── Audio/
│   │   │   ├── AudioSystem.h / .cpp
│   │   │   └── AudioClip.h / .cpp
│   │   │
│   │   ├── Input/
│   │   │   ├── InputManager.h / .cpp
│   │   │   ├── KeyCode.h
│   │   │   └── MouseButton.h
│   │   │
│   │   ├── Animation/
│   │   │   ├── Bone.h
│   │   │   ├── Skeleton.h / .cpp
│   │   │   ├── AnimationClip.h / .cpp
│   │   │   └── Animator.h / .cpp
│   │   │
│   │   ├── Math/
│   │   │   ├── Vectors/
│   │   │   │   ├── Vector2.h
│   │   │   │   ├── Vector3.h
│   │   │   │   └── Vector4.h
│   │   │   ├── Quaternions/
│   │   │   │   └── Quaternion.h
│   │   │   └── Matrix/
│   │   │       └── Matrix4.h
│   │   │   ├── Color.h
│   │   │   └── Math.h
│   │   │
│   │   ├── UI/
│   │   │   ├── Canvas.h / .cpp
│   │   │   ├── UIElement.h / .cpp
│   │   │   ├── RectTransform.h / .cpp
│   │   │   ├── CanvasSystem.h / .cpp
│   │   │   └── Elements/
│   │   │       ├── UIButton.h / .cpp
│   │   │       ├── UIText.h / .cpp
│   │   │       ├── UIImage.h / .cpp
│   │   │       ├── UIPanel.h / .cpp
│   │   │       └── UIContainer.h / .cpp
│   │   │   ├── UIRenderContext.h / .cpp
│   │   │   └── EventSystem/
│   │   │       ├── IEventSystemHandler.h
│   │   │       ├── PointerEventData.h
│   │   │       ├── IPointerClickHandler.h
│   │   │       ├── IPointerDownHandler.h
│   │   │       ├── IPointerUpHandler.h
│   │   │       ├── IPointerEnterHandler.h
│   │   │       └── IPointerExitHandler.h
│   │   │
│   │   ├── Scripting/
│   │   │   ├── ComponentRegistry.h / .cpp
│   │   │   ├── SceneLoader.h / .cpp
│   │   │   ├── SceneSaver.h / .cpp
│   │   │   ├── ScriptManager.h / .cpp
│   │   │   ├── PrefabLoader.h / .cpp
│   │   │   ├── PrefabSaver.h / .cpp
│   │   │   ├── SceneComponentConfigurator.h / .cpp
│   │   │   ├── SceneParsingUtils.h / .cpp
│   │   │   ├── ScenePropertySerializer.h / .cpp
│   │   │   ├── SceneReflectionUtils.h / .cpp
│   │   │   └── SceneLuaBindings.h / .cpp
│   │   │
│   │   └── Reflection/
│   │       ├── TypeInfo.h / .cpp
│   │       ├── PropertyMacros.h
│   │       └── TypeInfoBuilder.h
│   │
│   ├── Assets/                    Runtime assets (audio, fonts, models, scenes, textures)
│   └── Default/                   Built-in engine assets (shaders, fonts, models, textures)
│
└── RTBEngine_SDK/                 Generated by BuildSDK.bat
    ├── Include/RTBEngine/         Public headers (mirrors Engine/ structure)
    └── Lib/                       RTBEngine.lib
```

---

## 2. Build System

### First-Time Setup

```batch
SetupDeps.bat
```

This script downloads, configures, and builds all third-party libraries into `RTBEngine/ThirdParty/`. It only needs to be run once. Libraries built include: SDL2, GLEW, Bullet, Assimp, Lua, and LuaBridge. FMOD is expected to be installed separately via the FMOD Studio installer.

### Building the Engine and SDK

```batch
BuildSDK.bat
```

This script:
1. Compiles `RTBEngine.dll` and `RTBEngine.lib` in both Debug and Release configurations.
2. Copies all public headers into `RTBEngine_SDK/Include/RTBEngine/`.
3. Copies the compiled `.lib` into `RTBEngine_SDK/Lib/`.

Consuming projects (like the editor) reference the SDK rather than the engine source.

Whenever the public ABI changes, rebuild the engine and regenerate the SDK before rebuilding `GameScripts.dll`. That includes changes in exported headers, reflection metadata, scene loading, input, or any script-facing API that lives in the SDK copy. Old SDK copies or stale script binaries can keep outdated macro expansions and crash on load.

### Visual Studio Solution

- **Solution**: `RTBEngine.sln`
- **Configurations**: `Debug | x64`, `Release | x64`
- **Toolset**: MSVC v145 (Visual Studio 2026)
- **C++ Standard**: `/std:c++17`
- **Output**: `RTBEngine.dll` plus `RTBEngine.lib`

### Preprocessor Defines

| Define | Meaning |
|--------|---------|
| `RTB_EXPORTS` | Defined when compiling the engine DLL; activates `__declspec(dllexport)` |
| `RTB_API` | Expands to `__declspec(dllexport)` or `__declspec(dllimport)` depending on context |

---

## 3. Third-Party Libraries

| Library | Version | Location | Purpose |
|---------|---------|----------|---------|
| SDL2 | 2.32.10 | `ThirdParty/SDL2-2.32.10/` | Window creation, OpenGL context, input events, platform abstraction |
| GLEW | 2.1.0 | `ThirdParty/glew-2.1.0/` | OpenGL extension loading |
| Bullet Physics | 3.25 | `ThirdParty/bullet3-3.25/` | Rigid-body dynamics, broadphase, collision detection |
| Assimp | 5.4.3 | `ThirdParty/assimp/` | 3D model import: FBX, OBJ, GLTF, DAE, and more |
| FMOD | — | `ThirdParty/fmod/` | Real-time audio engine with 3D spatialization |
| ImGui | 1.92.5 | `ThirdParty/imgui/` | Immediate-mode debug/editor GUI |
| Lua | 5.4.8 | `ThirdParty/lua/` | Embedded scripting for scene files |
| LuaBridge | — | `ThirdParty/luabridge/` | C++ ↔ Lua binding without Lua C API boilerplate |
| stb_image | — | `ThirdParty/stb/` | PNG/JPG/BMP image loading |

---

## 4. Architecture Overview

RTBEngine is organized around three concepts:

1. **Application** — owns the platform layer (window, OpenGL context, device), drives the main loop, and coordinates all subsystems.
2. **Scene / SceneManager** — manages the active collection of `GameObject` instances. Scene loading now goes through deferred requests so the active scene can be replaced at a safe point without leaving subsystems half torn down.
3. **Component** — the unit of behavior. Every distinct feature of a `GameObject` (rendering, physics, audio, scripts) is a `Component`. Components interact through their owner `GameObject` and the global singleton subsystems.

### Main Loop

```
Application::Run()
  loop:
    deltaTime = ComputeDelta()
    ProcessInput()            ← SDL events → InputManager
    SceneManager::ProcessPendingSceneLoad()
    Update(deltaTime)         ← Scene::Update → all GameObjects → all Components
                              ← PhysicsSystem::Update (fixed step)
                              ← AudioSystem::Update
    SceneManager::ProcessPendingSceneLoad()
    RenderShadowPass()        ← depth-only render for shadow-casting lights
    RenderGeometryPass()      ← full lit render with shadow texture
    ImGui::Render()           ← editor/debug overlay
    Window::SwapBuffers()
```

### Subsystem Initialization Order

Inside `Application::Initialize()`:

```
1. Window::Create()           SDL2 window + OpenGL 4.1 context
2. GLEW::Init()               Load OpenGL extensions
3. ResourceManager::Init()    Load default assets (shaders, textures, meshes)
4. AudioSystem::Init()        FMOD system
5. ImGui::Init()              Context, SDL2 + OpenGL backends
6. SceneManager callbacks     Register onSceneUnloading / onSceneLoaded
```

Physics is initialized per-scene in `InitializePhysicsForScene()`, which is called from the `onSceneLoaded` callback. This ensures that Bullet bodies are created after the scene is fully populated.
Scene requests can be queued during gameplay and are processed at safe points around the frame update so a scene change requested from `OnStart` or `OnUpdate` can complete before physics runs again.

### Shutdown Order (reverse of init)

```
SceneManager::UnloadCurrentScene()
AudioSystem::Shutdown()
ImGui::Shutdown()
ResourceManager::Clear()
Window::Destroy()
```

---

## 5. Core Subsystem

### 5.1 ApplicationConfig

`Engine/Core/ApplicationConfig.h` — Plain data struct passed to the `Application` constructor. No methods.

```cpp
struct ApplicationConfig {
    std::string title        = "RTBEngine App";
    int         width        = 1280;
    int         height       = 720;
    bool        fullscreen   = false;
    bool        vsync        = true;
    int         targetFPS    = 60;
    std::string startScene   = "";
};
```

| Field | Description |
|-------|-------------|
| `title` | Window title bar text |
| `width` / `height` | Initial window resolution in pixels |
| `fullscreen` | Start in fullscreen mode |
| `vsync` | Enable SDL vertical sync (limits to monitor refresh) |
| `targetFPS` | Frame cap when vsync is disabled |
| `startScene` | Path (relative to working directory) to the `.lua` scene loaded at startup |

### 5.2 Application

`Engine/Core/Application.h` — Central object that owns every subsystem and drives the main loop.

**Constructor and lifecycle:**

```cpp
explicit Application(const ApplicationConfig& config);

bool Initialize();   // Must be called before Run()
void Run();          // Blocks until RequestExit() is called
void Shutdown();     // Releases all resources; called automatically after Run() returns
```

`Initialize()` returns `false` if any critical step fails (window creation, GLEW init, shader compilation). The caller should treat a `false` return as a fatal error and exit.

**Input and update:**

```cpp
void ProcessInput();            // Polls SDL events; dispatches to InputManager and Window
void Update(float deltaTime);   // Advances scene, physics, and audio
static void RequestQuit();      // Script-facing quit request; consumed by the host/editor
static bool IsQuitRequested();
static bool ConsumeQuitRequest();
static void ClearQuitRequest();
```

`Update` follows this internal order:
1. `SceneManager::ProcessPendingSceneLoad()` — resolves queued scene transitions before gameplay work for the frame.
2. `Scene::Update(deltaTime)` — calls `Component::OnUpdate` on every active component.
3. `SceneManager::ProcessPendingSceneLoad()` — runs again after component updates so a scene change requested from `OnStart` or `OnUpdate` takes effect before physics.
4. `PhysicsSystem::Update(scene, deltaTime)` — syncs transforms to Bullet bodies, steps the simulation, dispatches collision callbacks.
5. `AudioSystem::Update()` — advances the FMOD system (required each frame).

`Scene::Update` actually has two passes at the component level: it calls `OnStart()` once for enabled components on the first frame, and then it calls `OnUpdate()` only on components that are both enabled and have `SetUpdateTickEnabled(true)`. This is the mechanism that lets event-driven scripts stay alive while opting out of idle per-frame work.

`RequestQuit()` is the script-facing path for asking the host to stop play mode or close the runtime. The host consumes that request with `ConsumeQuitRequest()` and can clear it explicitly with `ClearQuitRequest()` when a play session ends or is cancelled.

**Rendering:**

```cpp
void Render();
void RenderShadowPass(ECS::Scene* scene);
void RenderGeometryPass(ECS::Scene* scene, Rendering::Camera* camera);
```

`Render()` orchestrates:
1. Collects lights from the scene via `Scene::CollectLights()`.
2. For each shadow-casting `DirectionalLight`, calls `RenderShadowPass`.
3. Calls `RenderGeometryPass` with the active camera.
4. Renders the `Skybox` after opaque geometry.
5. Renders ImGui.
6. Calls `Window::SwapBuffers()`.

`RenderShadowPass` binds the light's `ShadowMap` framebuffer, sets the depth-only shader, and renders every `MeshRenderer` with `RenderSceneDepthOnly()`.

`RenderGeometryPass` binds the screen framebuffer, uploads all light data to the lit shader, uploads the shadow map texture and light-space matrix, then calls `Scene::Render(camera)` which delegates to each `MeshRenderer`.

**Physics helpers:**

```cpp
void InitializePhysicsForScene(ECS::Scene* scene);
void ResetPhysics();
```

`InitializePhysicsForScene` iterates every `GameObject` in the scene, finds pairs of `RigidBodyComponent` + `BoxColliderComponent`, and calls `PhysicsSystem::InitializeCollider` to create the Bullet shape and rigid body. It then adds each body to `PhysicsWorld`.

`ResetPhysics` removes all bodies from the `PhysicsWorld` and resets the `PhysicsSystem` collision state. It is registered with `SceneManager::SetOnSceneUnloading`, guaranteeing it fires **before** the scene's `GameObject` destructors run. This prevents the Bullet broadphase from accessing freed memory.

**Scene load callbacks (registered in Initialize):**

```cpp
sceneMgr.SetOnSceneUnloading([this](ECS::Scene*) {
    ResetPhysics();   // ← fires first, while GameObjects still exist
});

sceneMgr.SetOnSceneLoaded([this](ECS::Scene* scene) {
    InitializePhysicsForScene(scene);   // ← fires after scene is populated
    if (auto* cam = scene->GetActiveCamera())
        cam->SetAspectRatio((float)config.width / config.height);
});
```

**Accessors:**

```cpp
Window*                GetWindow();
void*                  GetImGuiContext();
const ApplicationConfig& GetConfig() const;
bool                   IsRunning() const;
void                   RequestExit();
void                   SetIsRunning(bool value);
```

### 5.3 Window

`Engine/Core/Window.h` — Wraps an SDL2 window with an OpenGL 4.1 Core Profile context.

```cpp
bool Create(const std::string& title, int width, int height,
            bool fullscreen, bool vsync);
void Destroy();
void SwapBuffers();
```

**Mouse control:**

```cpp
void SetMouseRelativeMode(bool enabled);  // Locks cursor to window (fps-style capture)
void SetCursorVisible(bool visible);      // Shows or hides the OS cursor
void SetMousePosition(int x, int y);      // Warps the cursor
```

**Window state:**

```cpp
int  GetWidth() const;
int  GetHeight() const;
bool IsFullscreen() const;
void SetFullscreen(bool fullscreen);
```

**Resize callback:**

```cpp
using ResizeCallback = std::function<void(int width, int height)>;
void SetResizeCallback(ResizeCallback callback);
```

The callback fires from inside `ProcessInput` whenever an `SDL_WINDOWEVENT_RESIZED` event is received. The `Application` uses it to update the OpenGL viewport and the camera aspect ratio.

**Low-level handles (for ImGui and advanced use):**

```cpp
SDL_Window*   GetSDLWindow() const;
SDL_GLContext GetGLContext() const;
```

### 5.4 Logger

`Engine/Core/Logger.h` — Thread-safe singleton. All engine subsystems write through this class. The editor's Console panel subscribes to it via a callback.

**Log levels:**

```cpp
enum class LogLevel { Info, Warning, Error };
```

**LogMessage struct** (stored in history and passed to callbacks):

```cpp
struct LogMessage {
    LogLevel    level;
    std::string message;
    std::string timestamp;   // formatted HH:MM:SS
};
```

**Core API:**

```cpp
// std::string overloads (use inside the engine, where CRT is shared):
void Log(LogLevel level, const std::string& message);
void Info(const std::string& message);
void Warning(const std::string& message);
void Error(const std::string& message);

// const char* overloads (ABI-safe for script DLL boundaries):
void Log(LogLevel level, const char* message);
void Info(const char* message);
void Warning(const char* message);
void Error(const char* message);
```

**History and callbacks:**

```cpp
void AddCallback(LogCallback callback);     // LogCallback = std::function<void(const LogMessage&)>
const std::vector<LogMessage>& GetLogs() const;
void Clear();
```

**Convenience macros** (resolve to the `Instance().Info/Warning/Error()` calls):

```cpp
RTB_INFO(msg)
RTB_WARN(msg)
RTB_ERROR(msg)
```

These macros work with both `std::string` and `const char*` arguments. When calling from a script DLL, always pass `const char*` — see [DLL Boundary Safety](#17-dll-boundary-safety).

**Singleton access:**

```cpp
static Logger& GetInstance();
```

**Thread safety**: `Log()` acquires a `std::mutex` before writing to the message list and invoking callbacks. Callbacks must not themselves call `Log()` on the same thread (would deadlock).

### 5.5 ResourceManager

`Engine/Core/ResourceManager.h` — Singleton asset cache with lazy loading. Assets are identified by their file path (relative to the working directory). Once loaded, a pointer is returned and the asset is kept alive for the lifetime of the manager (until `Clear()` is called).

**Singleton access:**

```cpp
static ResourceManager& GetInstance();
```

**Shader management:**

```cpp
Rendering::Shader* LoadShader(const std::string& name,
                               const std::string& vertexPath,
                               const std::string& fragmentPath);
Rendering::Shader* GetShader(const std::string& name);
```

Shaders are keyed by a logical name (not path) to allow the same GLSL files to be exposed under multiple names with different compilation options in the future.

**Texture management:**

```cpp
Rendering::Texture* LoadTexture(const std::string& path);
Rendering::Texture* GetTexture(const std::string& path);
std::string         GetTexturePath(const Rendering::Texture* texture) const;
```

`GetTexturePath` is the reverse lookup used by `SceneSaver` when serializing scene files. If the texture was not loaded from a file (e.g., created programmatically), it returns an empty string.

**Mesh / Model management:**

```cpp
// Single-mesh models (OBJ files, simple primitives):
Rendering::Mesh*              LoadModel(const std::string& path);
Rendering::Mesh*              GetModel(const std::string& path);

// Multi-mesh models (FBX files with multiple submeshes):
std::vector<Rendering::Mesh*> LoadModelMeshes(const std::string& path);
std::vector<Rendering::Mesh*> GetModelMeshes(const std::string& path);

std::string GetMeshPath(const Rendering::Mesh* mesh) const;
```

**Audio clip management:**

```cpp
Audio::AudioClip* LoadAudioClip(const std::string& path, bool stream = false);
Audio::AudioClip* GetAudioClip(const std::string& path);
std::string       GetAudioClipPath(const Audio::AudioClip* clip) const;
```

The `stream` parameter tells FMOD to stream the file from disk instead of loading it entirely into memory. Use streaming for long music tracks; keep short sound effects in memory.

**Font management:**

```cpp
Rendering::Font* LoadFont(const std::string& path,
                           const int* sizes, int numSizes);
Rendering::Font* GetFont(const std::string& path, int size);
Rendering::Font* GetDefaultFont();
```

**Cubemap management:**

```cpp
Rendering::Cubemap* LoadCubemapAsset(const std::string& path);
Rendering::Cubemap* GetCubemap(const std::string& path);
std::string         GetCubemapPath(const Rendering::Cubemap* cubemap) const;
```

Cubemap assets are stored as `.cubemap` files — a simple text format that lists the paths to the six face textures (Right, Left, Top, Bottom, Front, Back).

**Default assets** (always available, loaded during `Initialize()`):

```cpp
Rendering::Mesh*    GetDefaultCubeMesh();
Rendering::Mesh*    GetDefaultSphereMesh();
Rendering::Mesh*    GetDefaultPlaneMesh();
Rendering::Texture* GetDefaultTexture();       // 1×1 white pixel
Rendering::Skybox*  GetDefaultSkybox();
Rendering::Font*    GetDefaultFont();
```

**Cache management:**

```cpp
void Initialize();   // Load all default assets
void Clear();        // Destroy all cached assets
```

---

## 6. ECS Subsystem

### 6.1 Component

`Engine/ECS/Component.h` — Abstract base class for all behaviors. Every feature that can be attached to a `GameObject` extends this class.

**Lifecycle hooks** (called by `GameObject::Update` / `Scene::Update`):

```cpp
virtual void OnAwake();                          // Fired inside AddComponent() — props not yet set
virtual void OnStart();                          // Fired on first Update() after scene load
virtual void OnUpdate(float deltaTime);          // Every frame while active and owner is active
virtual void OnFixedUpdate(float fixedDelta);    // Fixed physics timestep
virtual void OnDestroy();                        // Owner removed or scene unloaded
virtual void OnParentChanged(GameObject* oldParent, GameObject* newParent); // Fired by SetParent()
```

**Collision callbacks** (fired by `PhysicsSystem` when rigid bodies interact):

```cpp
virtual void OnCollisionEnter(const Physics::CollisionInfo& collision);
virtual void OnCollisionStay(const Physics::CollisionInfo& collision);
virtual void OnCollisionExit(const Physics::CollisionInfo& collision);
virtual void OnTriggerEnter(const Physics::CollisionInfo& collision);
virtual void OnTriggerStay(const Physics::CollisionInfo& collision);
virtual void OnTriggerExit(const Physics::CollisionInfo& collision);
```

Collision callbacks are only dispatched when the `GameObject` has a `RigidBodyComponent`. All six have empty default implementations so subclasses can override only what they need.

**Editor callback:**

```cpp
virtual void OnValidate();   // Called by Inspector when a reflected property is modified
```

Use `OnValidate` to sync reflected proxy members to private fields, clamp values, or trigger dependent updates.

**Reflection interface:**

```cpp
virtual const char*                    GetTypeName() const  { return "Component"; }
virtual const Reflection::TypeInfo*    GetTypeInfo()  const { return nullptr; }
```

These are overridden by the `RTB_COMPONENT(ClassName)` macro. Non-reflected components return `nullptr` from `GetTypeInfo()`.

**Owner and enable state:**

```cpp
void           SetOwner(ECS::GameObject* owner);
ECS::GameObject* GetOwner() const;

void SetEnabled(bool enabled);
bool IsEnabled() const;

void SetUpdateTickEnabled(bool enabled);
bool IsUpdateTickEnabled() const;
```

A component does not receive `OnUpdate` or `OnFixedUpdate` when disabled. `OnDestroy` is still called even on disabled components when the owner is destroyed.

`SetUpdateTickEnabled(false)` is a lighter-weight switch than `SetEnabled(false)`:

- `SetEnabled(false)` disables the component as a whole. In the current engine implementation it blocks `OnUpdate`, `OnFixedUpdate`, and also prevents `OnStart` from being called during the owning `GameObject`'s first update pass.
- `SetUpdateTickEnabled(false)` only suppresses `OnUpdate`. The component remains enabled and can still participate in other systems that address it directly, such as editor validation, fixed-step logic, or UI/physics event dispatch.

This split exists for event-driven scripts. A component such as a UI animation controller can stay "alive" for pointer events, but keep its per-frame cost at zero while idle. When work starts, it re-enables its update tick; when the work finishes, it disables the tick again.

> **Critical**: `OnAwake` fires inside `AddComponent()`, **before** `SceneReflectionUtils` assigns reflected property values from the scene file. Never read reflected `GameObjectRef` or asset references in `OnAwake`. Use `OnStart` instead.

### 6.2 Transform

`Engine/ECS/Transform.h` — Stores and manipulates a `GameObject`'s local-space position, rotation, and scale.

**Setters:**

```cpp
void SetPosition(const Math::Vector3& pos);
void SetRotation(const Math::Quaternion& rot);
void SetEulerAngles(const Math::Vector3& eulerDegrees);
void SetScale(const Math::Vector3& scale);
```

**Getters:**

```cpp
const Math::Vector3&    GetPosition()    const;
const Math::Quaternion& GetRotation()    const;
Math::Vector3           GetEulerAngles() const;   // Converts quaternion to Euler (degrees)
const Math::Vector3&    GetScale()       const;
```

**Mutation:**

```cpp
void Translate(const Math::Vector3& delta);          // Adds to position in local space
void Rotate(const Math::Quaternion& rotation);       // Multiplies current rotation
void RotateEuler(const Math::Vector3& eulerDelta);   // Euler-angle rotation delta (degrees)
```

**Direction vectors** (derived from the current rotation):

```cpp
Math::Vector3 GetForward() const;   // -Z in local space, rotated to world
Math::Vector3 GetRight()   const;   //  +X in local space, rotated to world
Math::Vector3 GetUp()      const;   //  +Y in local space, rotated to world
```

**Matrix:**

```cpp
Math::Matrix4 GetModelMatrix() const;
```

Returns the TRS matrix: `Translation * Rotation * Scale`. Used by renderers and physics sync code.

### 6.3 GameObject

`Engine/ECS/GameObject.h` — Named container that owns a `Transform` and a list of `Component` instances. Can participate in a parent-child hierarchy.

**Identity:**

```cpp
const std::string& GetName()      const;
const char*        GetNameCStr()  const;   // ABI-safe: returns name.c_str()
void               SetName(const std::string& name);

uint64_t GetUUID() const;
void     SetUUID(uint64_t uuid);
```

UUIDs are assigned by `SceneLoader` when deserializing a scene file. They are used for `GameObjectRef` resolution (deferred UUID-to-pointer mapping).

**Transform access:**

```cpp
Transform&       GetTransform();
const Transform& GetTransform() const;
```

The `Transform` is always present and is not a `Component` — it is a direct member.

**World-space transforms** (computed by walking up the parent chain):

```cpp
Math::Vector3    GetWorldPosition() const;
Math::Quaternion GetWorldRotation() const;
Math::Vector3    GetWorldScale()    const;
Math::Matrix4    GetWorldMatrix()   const;
```

**Component management:**

```cpp
template<typename T>
T* AddComponent();          // Creates a T, calls T::OnAwake(), returns raw pointer

template<typename T>
T* GetComponent();          // Returns first T found, or nullptr

template<typename T>
bool HasComponent() const;

template<typename T>
void RemoveComponent();     // Calls T::OnDestroy(), removes from list

const std::vector<std::unique_ptr<Component>>& GetComponents() const;
```

`AddComponent<T>()` uses `std::make_unique<T>()`, sets the owner pointer, then immediately calls `OnAwake()`. Because this happens before the scene is fully loaded, reflected properties are not yet populated at this point.

**Hierarchy:**

```cpp
void             SetParent(GameObject* parent);   // Calls OnParentChanged on all components
void             AddChild(GameObject* child);
void             RemoveChild(GameObject* child);
GameObject*      GetParent() const;
const std::vector<GameObject*>& GetChildren() const;

// Global counter incremented on every AddChild/RemoveChild:
static uint32_t  GetHierarchyVersion();

// Depth-first component search (checks self first, then descendants):
template<typename T>
T* GetComponentInChildren(int maxDepth = -1);   // -1 = unlimited depth
```

**Activation:**

```cpp
void SetActive(bool active);
bool IsActive() const;
```

An inactive `GameObject` does not receive `Update`, `FixedUpdate`, or `Render` calls. Its children are also effectively deactivated.

**Prefab support:**

```cpp
const std::string& GetPrefabName() const;
void SetPrefabName(const std::string& name);
bool IsPrefabInstance() const;   // Returns true when prefabName is non-empty
```

**Transient flag:**

```cpp
void SetTransient(bool transient);
bool IsTransient() const;
```

Transient GameObjects are excluded from scene serialization. Used for editor-only objects (e.g., gizmo handles).

**Lifecycle (called by Scene):**

```cpp
void Update(float deltaTime);    // Calls OnStart once, then OnUpdate on enabled components with update tick enabled
void FixedUpdate();              // Calls OnFixedUpdate on all enabled components
void Render(Rendering::Camera* camera);  // Calls MeshRenderer / Animator / etc.
void Start();                    // Calls OnStart on all components (once, at first Update)
```

`GameObject::Update` now has two distinct gates:

1. The one-time start phase checks `IsEnabled()` only. This preserves the existing rule that update-tick sleeping does not suppress initialization.
2. The per-frame phase checks both `IsEnabled()` and `IsUpdateTickEnabled()`. A component can therefore remain enabled for events and ownership/lifecycle purposes while opting out of steady-state frame work.

`FixedUpdate` is unchanged in this iteration and still depends only on `IsEnabled()`.

### 6.4 Scene

`Engine/ECS/Scene.h` — Owns a flat list of `GameObject` unique pointers. Manages the active camera and aggregates lighting information.

**GameObject management:**

```cpp
GameObject* AddGameObject(const std::string& name);
void        RemoveGameObject(GameObject* gameObject);
GameObject* FindGameObject(const std::string& name) const;
GameObject* FindGameObjectByUUID(uint64_t uuid) const;

const std::vector<std::unique_ptr<GameObject>>& GetGameObjects() const;
```

`AddGameObject` creates a new `GameObject` via `std::make_unique`, stores it in the scene's vector, and returns a raw observer pointer. The scene owns all `GameObject` instances; raw pointers are safe as long as `RemoveGameObject` has not been called.

**Lifecycle (called by Application):**

```cpp
void Update(float deltaTime);    // Propagates to all active root GameObjects
void FixedUpdate();
void Render(Rendering::Camera* camera);
```

Update is called on all root-level `GameObject` instances (those without a parent). Each `GameObject::Update` recurses into its children.

**Camera:**

```cpp
void        SetMainCamera(GameObject* cameraObject);   // Must have CameraComponent
Camera*     GetMainCamera()   const;    // Returns the raw Camera from CameraComponent
GameObject* GetActiveCameraObject() const;
Camera*     GetActiveCamera() const;
```

**Lighting:**

```cpp
std::vector<Rendering::Lighting::Light*> CollectLights() const;
const std::vector<Rendering::Lighting::Light*>& GetLights() const;
```

`CollectLights()` scans every `GameObject` for a `LightComponent` and returns their underlying `Light*` pointers. Called by `Application::Render` before the geometry pass.

**Skybox:**

```cpp
void SetSkyboxCubemap(Rendering::Cubemap* cubemap);
void SetSkyboxEnabled(bool enabled);
bool IsSkyboxEnabled() const;
Rendering::Skybox* GetSkybox() const;
```

Each scene has its own `Skybox` instance. When rendering, `Application` checks `IsSkyboxEnabled()` and calls `Skybox::Render(camera)` after the geometry pass.

### 6.5 SceneManager

`Engine/ECS/SceneManager.h` — Singleton. Handles loading and unloading scenes, and fires lifecycle callbacks so subsystems can respond.

```cpp
static SceneManager& GetInstance();
```

**Loading:**

```cpp
bool RequestSceneLoad(const char* path);
bool ProcessPendingSceneLoad();
void ClearPendingSceneLoad();
bool LoadScene(const std::string& path);
void UnloadCurrentScene();
```

`RequestSceneLoad` copies the requested path into an internal pending slot and returns immediately. `ProcessPendingSceneLoad()` performs the transition at a safe point in the frame. Empty paths are ignored with a warning, and a newer request replaces the older pending one.

`LoadScene` performs the actual transition safely:
1. The current scene is moved to a temporary local and `activeScene` / `activeScenePath` are cleared.
2. The previous dirty flag is preserved so it can be restored if loading fails.
3. `SceneLoader::Load(path)` parses the Lua file and builds the new scene.
4. If loading fails, the previous scene, path, and dirty state are restored.
5. If loading succeeds, `onSceneUnloading` fires, the old scene is destroyed, the new scene becomes active, dirty state is cleared, and `onSceneLoaded` fires.

`UnloadCurrentScene()` and any path that tears scripts down also clear pending scene requests so a stale request cannot resurrect after shutdown.

**State:**

```cpp
bool         HasActiveScene()     const;
ECS::Scene*  GetActiveScene()     const;   // Returns nullptr if no scene loaded
const std::string& GetActiveScenePath() const;
```

**Callbacks:**

```cpp
using SceneCallback = std::function<void(ECS::Scene*)>;
void SetOnSceneLoaded(SceneCallback callback);
void SetOnSceneUnloading(SceneCallback callback);
```

Only one callback of each type can be registered. Registering a second one replaces the first.

**Dirty tracking** (used by the editor to show unsaved-changes indicator):

```cpp
void MarkSceneDirty();
void ClearSceneDirty();
bool IsSceneDirty() const;
```

Dirty state is automatically set when scene content is modified through the editor Inspector or Hierarchy panels, and cleared on save.

**Instantiation:**

```cpp
GameObject* Instantiate(const std::string& name = "GameObject", GameObject* parent = nullptr);
GameObject* Instantiate(const ECS::Prefab& prefab, GameObject* parent = nullptr);
```

`Instantiate` creates a new `GameObject` in the active scene. The second overload uses a `Prefab` to create a fully configured copy with all component snapshots applied.

### 6.6 Prefab

`Engine/ECS/Prefab.h` — Captures a snapshot of a `GameObject` and its component data. Can instantiate independent copies with the same configuration.

**ComponentSnapshot:**

```cpp
struct ComponentSnapshot {
    std::string typeName;                                 // Component class name
    std::vector<uint8_t> rawData;                         // POD property bytes
    std::unordered_map<size_t, std::string> stringData;   // String properties by offset
    std::unordered_map<size_t, std::string> ptrPathData;  // Asset/GO ref paths by offset
};
```

**Creation and instantiation:**

```cpp
static std::unique_ptr<Prefab> CreateFromGameObject(const GameObject* source);

GameObject* Instantiate(GameObject* parent,
                        std::vector<GameObject*>& outChildren) const;
GameObject* Instantiate(GameObject* parent = nullptr) const;
```

`CreateFromGameObject` recursively snapshots the source's transform, all components (via `SnapshotComponent`), and all children as child prefabs. `Instantiate` creates new `GameObject` instances and applies the snapshots via `ApplySnapshot`.

**Transform:**

```cpp
void SetPosition(const Math::Vector3& pos);
void SetRotation(const Math::Quaternion& rot);
void SetScale(const Math::Vector3& scl);
const Math::Vector3&    GetPosition() const;
const Math::Quaternion& GetRotation() const;
const Math::Vector3&    GetScale()    const;
```

**Static helpers:**

```cpp
static void SnapshotComponent(ComponentSnapshot& snap, const Component* comp);
static void ApplySnapshot(Component* target, const ComponentSnapshot& snap);
```

### 6.7 PrefabRegistry

`Engine/ECS/PrefabRegistry.h` — Singleton that manages loading, caching, and hot-reloading of prefab assets.

```cpp
static PrefabRegistry& GetInstance();

void LoadAll(const std::string& directory);   // Load all .prefab files in directory
void Register(const std::string& filePath);   // Load and register a single prefab
void Unload(const std::string& name);
void Reload(const std::string& name);         // Re-read from disk
void Clear();

Prefab* Get(const std::string& name) const;
Prefab* GetByPath(const std::string& filePath) const;
bool    Has(const std::string& name) const;

std::function<void(const std::string&)> onPrefabChanged;   // Notification callback
```

---

## 7. Built-in Components

All built-in components follow the full lifecycle protocol and support the reflection system. Their reflected proxy members are listed with the `Ref` suffix — these are what the Inspector reads and writes.

### 7.1 MeshRenderer

`Engine/ECS/MeshRenderer.h` — Renders one or more meshes using a single `Material`.

**Single-mesh workflow:**

```cpp
auto* mr = go->AddComponent<MeshRenderer>();
mr->SetMesh(resourceMgr.GetModel("Assets/Models/Crate.obj"));
mr->SetTexture(resourceMgr.GetTexture("Assets/Textures/crate.png"));
```

**Multi-mesh workflow** (for FBX files with submeshes):

```cpp
mr->SetMeshes(resourceMgr.GetModelMeshes("Assets/Models/Character.fbx"));

// Per-mesh materials (loaded from ModelData by ModelLoader):
mr->SetMeshMaterials(modelData.materials);
```

**Material control:**

```cpp
Material*  GetMaterial();
void       SetShader(Rendering::Shader* shader);
void       SetTexture(Rendering::Texture* texture);
void       SetColor(const Math::Vector4& color);
```

**Per-mesh materials** (when a model has embedded materials):

```cpp
bool        HasMeshMaterials() const;
Material*   GetMeshMaterial(int index) const;
void        SetMeshMaterials(const std::vector<Material*>& materials);
```

**Mesh access:**

```cpp
Rendering::Mesh*               GetMesh()   const;
const std::vector<Rendering::Mesh*>& GetMeshes() const;
void SetMesh(Rendering::Mesh* mesh);
void SetMeshes(const std::vector<Rendering::Mesh*>& meshes);
```

**Reflected properties (Proxy):**

```cpp
std::string meshRef;      // Path to mesh asset (synced to SetMesh in OnValidate)
std::string textureRef;   // Path to texture asset
Math::Vector4 colorRef;   // RGBA tint color
```

**Lifecycle:**

- `OnAwake`: initializes `Material` with the default lit shader and default white texture.
- `OnValidate`: called by Inspector when `meshRef`, `textureRef`, or `colorRef` changes; reloads the asset from `ResourceManager`.
- `OnUpdate`: not used (rendering happens in `OnUpdate`'s sibling call `Render(camera)`).

The actual draw call is issued by `GameObject::Render(camera)`, which calls `MeshRenderer`'s internal render method.

### 7.2 CameraComponent

`Engine/ECS/CameraComponent.h` — Wraps a `Rendering::Camera` and can designate itself as the scene's main camera.

**Camera properties:**

```cpp
void SetFOV(float fovDegrees);          float GetFOV()   const;
void SetNearClip(float near);           float GetNearClip() const;
void SetFarClip(float far);             float GetFarClip() const;
void SetOrthographicSize(float size);   float GetOrthographicSize() const;

enum class ProjectionType { Perspective, Orthographic };
void SetProjectionType(ProjectionType type);
ProjectionType GetProjectionType() const;
```

**Scene camera designation:**

```cpp
void SetAsMain();   // Registers this camera with Scene::SetMainCamera
bool IsMain() const;
```

**Transform sync:**

```cpp
void SetSyncWithTransform(bool sync);
bool GetSyncWithTransform() const;
```

When sync is enabled (default), `OnUpdate` copies the owner `GameObject`'s world position and rotation to the internal `Camera` every frame. Disable sync if you want to drive the camera directly (e.g., in a cinematic controller).

**Reflected properties (Proxy):**

```cpp
float fovRef               = 60.0f;
float nearClipRef          = 0.1f;
float farClipRef           = 1000.0f;
int   projectionTypeRef    = 0;      // 0=Perspective, 1=Orthographic
float orthographicSizeRef  = 5.0f;
```

### 7.3 LightComponent

`Engine/ECS/LightComponent.h` — Attaches a dynamic `Rendering::Light` (Directional, Point, or Spot) to a `GameObject`. The component syncs the light's world position and direction with the owner's `Transform` each frame.

**Light type:**

```cpp
enum class LightType { Directional = 0, Point = 1, Spot = 2 };
void      SetLightType(LightType type);
LightType GetLightType() const;
```

Changing `lightType` destroys the existing `Light` object and creates a new one of the requested subclass. All property values (color, intensity, etc.) are reset to defaults on type change.

**Common properties:**

```cpp
void SetColor(const Math::Vector3& color);      Math::Vector3 GetColor() const;
void SetIntensity(float intensity);             float GetIntensity() const;
```

**Point and Spot specific:**

```cpp
void  SetRange(float range);        float GetRange() const;
```

**Spot specific:**

```cpp
void  SetSpotAngle(float degrees);       float GetSpotAngle()      const;  // outer cone
void  SetSpotInnerAngle(float degrees);  float GetSpotInnerAngle() const;  // inner cone (soft edge)
```

**Sync flags:**

```cpp
bool syncPosition  = true;   // Copy world position to light each frame
bool syncDirection = true;   // Copy world forward vector to light each frame (Directional/Spot)
```

**Reflected properties (Proxy):**

```cpp
int           lightTypeRef   = 0;     // LightType enum index
Math::Vector3 colorRef       = {1,1,1};
float         intensityRef   = 1.0f;
float         rangeRef       = 10.0f;
float         spotAngleRef   = 30.0f;
float         spotInnerAngleRef = 15.0f;
```

**Lifecycle:**

- `OnAwake`: creates the initial `Light` object (default type: `Point`).
- `OnStart`: registers the light with `Scene::CollectLights`.
- `OnUpdate`: syncs position/direction from `GetWorldPosition()` / `GetWorldMatrix().GetForward()`.
- `OnValidate`: rebuilds the light object if `lightTypeRef` changed; updates color, intensity, range.
- `OnDestroy`: unregisters the light from the scene.

### 7.4 RigidBodyComponent

`Engine/ECS/RigidBodyComponent.h` — Adds Bullet Physics simulation to a `GameObject`. Works in conjunction with `BoxColliderComponent` (or other colliders) to define the shape.

**Body type:**

```cpp
enum class BodyType { Static = 0, Dynamic = 1, Kinematic = 2 };
```

- **Static**: mass = 0; never moves; other objects collide with it.
- **Dynamic**: fully simulated; affected by gravity and forces.
- **Kinematic**: moved by code (not physics forces); other dynamic bodies collide with it.

**Runtime control** (after `OnStart`, when the Bullet body exists):

```cpp
Physics::RigidBody* GetRigidBody() const;
```

Access the underlying `RigidBody` to apply forces, set velocities, or lock axes:

```cpp
rb->GetRigidBody()->ApplyCentralImpulse({0, 10, 0});
rb->GetRigidBody()->SetLinearVelocity({5, 0, 0});
rb->GetRigidBody()->SetAngularFactor({0, 1, 0});  // lock X and Z rotation
rb->GetRigidBody()->SetLinearFactor({1, 1, 0});   // lock Z movement
```

**Reflected properties (Proxy):**

```cpp
float massRef        = 1.0f;
float frictionRef    = 0.5f;
float restitutionRef = 0.0f;   // Bounciness (0 = no bounce, 1 = perfect bounce)
int   bodyTypeRef    = 1;      // BodyType enum index (default: Dynamic)
```

**Lifecycle:**

- `OnAwake`: stores initial values but does not create the Bullet body (no collider or physics world yet).
- `OnStart`: Bullet body is created by `PhysicsSystem::InitializeCollider` before `OnStart` fires.
- `OnUpdate`: syncs `Transform` from the Bullet body's world transform (for Dynamic bodies).
- `OnValidate`: updates mass, friction, restitution on the existing Bullet body if it exists.
- `OnDestroy`: removes the Bullet body from `PhysicsWorld`.

### 7.5 BoxColliderComponent

`Engine/ECS/BoxColliderComponent.h` — Defines a box-shaped collision volume for the sibling `RigidBodyComponent`.

**Size and offset:**

```cpp
void          SetSize(const Math::Vector3& halfExtents);
Math::Vector3 GetSize() const;

void          SetCenter(const Math::Vector3& center);
Math::Vector3 GetCenter() const;
```

Size is specified as **half-extents** (half the full side length in each axis), which is Bullet's native convention. A unit cube uses `{0.5f, 0.5f, 0.5f}`.

**Reflected properties (Proxy):**

```cpp
Math::Vector3 sizeRef   = {0.5f, 0.5f, 0.5f};
Math::Vector3 centerRef = {0.0f, 0.0f, 0.0f};
```

The collider is realized when `PhysicsSystem::InitializeCollider(go, boxCollider)` is called from `Application::InitializePhysicsForScene`. At that point, a `btBoxShape` is created from `sizeRef` and attached to the `btRigidBody`.

### 7.6 AudioSourceComponent

`Engine/ECS/AudioSourceComponent.h` — Plays audio clips through the FMOD engine. Supports positional 3D audio when the owning `GameObject` has a `Transform`.

**Clip management:**

```cpp
void              SetClip(Audio::AudioClip* clip);
Audio::AudioClip* GetClip() const;
```

**Playback control:**

```cpp
void Play();
void Stop();
void Pause();
void Resume();
bool IsPlaying() const;
```

**Properties:**

```cpp
void  SetVolume(float volume);     float GetVolume()  const;  // 0.0–1.0
void  SetPitch(float pitch);       float GetPitch()   const;  // 0.5–2.0 typical
void  SetLoop(bool loop);          bool  GetLoop()    const;
void  SetPlayOnStart(bool play);   bool  GetPlayOnStart() const;
```

**Reflected properties (Proxy):**

```cpp
std::string audioClipRef  = "";
float       volumeRef     = 1.0f;
float       pitchRef      = 1.0f;
bool        loopRef       = false;
bool        playOnStartRef= false;
```

**Lifecycle:**

- `OnStart`: if `playOnStartRef` is true, calls `Play()`.
- `OnUpdate`: updates the FMOD channel's 3D position from `GetWorldPosition()`.
- `OnDestroy`: stops and releases the FMOD channel.

### 7.7 FreeLookCamera

`Engine/ECS/FreeLookCamera.h` — A convenience component that implements first-person camera controls using `InputManager`. Designed for runtime use (not the editor).

Reads `KeyCode::W/A/S/D/E/Q` for movement and the mouse delta for look. Speed and sensitivity are configurable. Internally drives the owner `GameObject`'s `Transform` directly.

### 7.8 SphereColliderComponent

`Engine/ECS/SphereColliderComponent.h` — Defines a sphere-shaped collision volume. Counterpart to `BoxColliderComponent`.

**Size:**

```cpp
void  SetRadius(float r);
float GetRadius() const;
```

**Offset:**

```cpp
void          SetCenterOffset(const Math::Vector3& offset);
Math::Vector3 GetCenterOffset() const;
```

**Trigger mode:**

```cpp
void SetIsTrigger(bool trigger);
bool IsTrigger() const;
```

When `isTrigger` is true, the collider generates trigger events (`OnTriggerEnter/Stay/Exit`) instead of physical collisions.

**Internal (used by PhysicsSystem):**

```cpp
Physics::SphereCollider* GetSphereCollider() const;
void SetBulletCollisionObject(btCollisionObject* obj);
btCollisionObject* GetBulletCollisionObject() const;
```

**Reflected properties (Proxy):**

```cpp
float         radius       = 0.5f;
Math::Vector3 centerOffset = {0, 0, 0};
bool          isTrigger    = false;
```

---

## 8. Rendering Subsystem

### 8.1 Camera

`Engine/Rendering/Camera.h` — A standalone (non-component) perspective or orthographic camera. Owned by `CameraComponent`; also used directly by the editor's `SceneViewPanel`.

**Projection type:**

```cpp
enum class ProjectionType { Perspective, Orthographic };
void SetProjectionType(ProjectionType type);
```

**Projection parameters:**

```cpp
void SetFOV(float degrees);
void SetAspectRatio(float aspect);      // width / height
void SetNearPlane(float near);
void SetFarPlane(float far);
void SetOrthographicSize(float size);   // Half-height of orthographic view
```

**Position and orientation:**

```cpp
void SetPosition(const Math::Vector3& pos);
void SetRotation(float pitchDegrees, float yawDegrees);

const Math::Vector3& GetPosition() const;
float GetPitch() const;
float GetYaw()   const;
```

**Free-look movement helpers** (advance the position along local axes):

```cpp
void Move(const Math::Vector3& delta);
void MoveForward(float distance);
void MoveRight(float distance);
void MoveUp(float distance);
void Rotate(float deltaPitch, float deltaYaw);  // Clamps pitch to ±89°
```

**Matrices:**

```cpp
Math::Matrix4 GetViewMatrix()           const;
Math::Matrix4 GetProjectionMatrix()     const;
Math::Matrix4 GetViewProjectionMatrix() const;   // Projection * View
```

**Direction vectors** (derived from pitch/yaw, not stored):

```cpp
Math::Vector3 GetForward() const;
Math::Vector3 GetRight()   const;
Math::Vector3 GetUp()      const;
```

**Frustum culling:**

```cpp
const Rendering::Frustum& GetFrustum();
```

Returns the camera's view frustum, lazily recomputed when the view or projection matrix changes. Used by the rendering pipeline to skip objects outside the camera's view.

### 8.2 Shader

`Engine/Rendering/Shader.h` — Compiles, links, and caches GLSL programs. Provides typed uniform setters.

**Loading:**

```cpp
bool LoadFromFiles(const std::string& vertexPath,
                   const std::string& fragmentPath);
bool LoadFromStrings(const std::string& vertexSrc,
                     const std::string& fragmentSrc);
```

Both methods compile the shaders, link the program, and log any compilation errors via `Logger`.

**Binding:**

```cpp
void Bind()   const;
void Unbind() const;
```

**Uniform setters:**

```cpp
void SetBool(const std::string& name, bool value)                  const;
void SetInt(const std::string& name, int value)                    const;
void SetFloat(const std::string& name, float value)                const;
void SetVector2(const std::string& name, const Math::Vector2& v)   const;
void SetVector3(const std::string& name, const Math::Vector3& v)   const;
void SetVector4(const std::string& name, const Math::Vector4& v)   const;
void SetMatrix4(const std::string& name, const Math::Matrix4& m)   const;
```

Uniform locations are cached in an `std::unordered_map<std::string, GLint>` after the first lookup to avoid repeated `glGetUniformLocation` calls.

**State:**

```cpp
bool IsCompiled()    const;
GLuint GetProgramID() const;
```

### 8.3 Material

`Engine/Rendering/Material.h` — Groups a `Shader`, a `Texture`, and surface properties (color, shininess). Provides a single `Bind()` call that activates the shader and uploads all uniforms.

```cpp
void SetShader(Rendering::Shader* shader);
void SetTexture(Rendering::Texture* texture);
void SetColor(const Math::Vector4& color);
void SetDiffuseColor(const Math::Vector3& color);
void SetShininess(float shininess);

Rendering::Shader*  GetShader()    const;
Rendering::Texture* GetTexture()   const;
Math::Vector4       GetColor()     const;
float               GetShininess() const;

void Bind();     // Binds shader, uploads uniforms, binds texture to unit 0
void Unbind();
```

When `Bind()` is called, it uploads:
- `u_Color` — the RGBA color multiplier.
- `u_Shininess` — the specular shininess exponent.
- `u_Texture` — the diffuse texture (unit 0).

### 8.4 Vertex and Mesh

**Vertex** (`Engine/Rendering/Vertex.h`):

```cpp
struct Vertex {
    Math::Vector3 position;
    Math::Vector3 normal;
    Math::Vector2 texCoords;
    Math::Vector3 tangent;
    Math::Vector3 bitangent;

    // Skeletal animation:
    int   boneIDs[4]     = {-1,-1,-1,-1};
    float boneWeights[4] = { 0, 0, 0, 0};
};
```

All attributes are uploaded to the GPU as a single interleaved buffer. The bone ID/weight attributes are used by the skinning shader when `Animator` is present.

**Mesh** (`Engine/Rendering/Mesh.h`):

```cpp
Mesh(const std::vector<Vertex>& vertices,
     const std::vector<unsigned int>& indices);
```

Internally creates a VAO, VBO, and EBO and uploads the data. After construction the CPU-side vectors are no longer needed.

```cpp
void Draw() const;   // glDrawElements(GL_TRIANGLES, indexCount, ...)
```

**Bounding box (AABB)**:

```cpp
Math::Vector3 GetAABBMin()    const;
Math::Vector3 GetAABBMax()    const;
Math::Vector3 GetAABBSize()   const;   // max - min
Math::Vector3 GetAABBCenter() const;   // (min + max) / 2
```

The AABB is computed from vertex positions at construction time and is used by the editor's object picking raycast.

**Material index** (for multi-mesh models with per-mesh materials):

```cpp
void SetMaterialIndex(int index);
int  GetMaterialIndex() const;
```

When a model is loaded with `LoadModelMeshes`, each `Mesh` is assigned the index of its corresponding material in the `LoadedMaterial` array. `MeshRenderer` uses this to select the correct `Material*` from `meshMaterials`.

**GPU state:**

```cpp
int GetVertexCount() const;
int GetIndexCount()  const;
GLuint GetVAO()      const;
```

### 8.5 Texture

`Engine/Rendering/Texture.h` — Wraps an OpenGL 2D texture object.

**Loading:**

```cpp
bool LoadFromFile(const std::string& path);

// For embedded textures from model files:
bool LoadFromMemory(const unsigned char* data, int width, int height, int channels);
bool LoadFromCompressedMemory(const unsigned char* data, size_t size);

// For shadow maps:
bool CreateDepthTexture(int width, int height);
```

`LoadFromFile` uses `stb_image` to decode PNG, JPG, BMP, and TGA files. It automatically detects the number of channels and selects the appropriate OpenGL internal format (`GL_RGB`, `GL_RGBA`, etc.).

**Sampling parameters:**

```cpp
enum class FilterMode { Nearest, Linear, LinearMipmapLinear };
enum class WrapMode   { Repeat, ClampToEdge, ClampToBorder };

void SetFilter(FilterMode minFilter, FilterMode magFilter);
void SetWrap(WrapMode wrapS, WrapMode wrapT);
void GenerateMipmaps();
```

**Binding:**

```cpp
void Bind(unsigned int slot = 0) const;   // glActiveTexture(GL_TEXTURE0 + slot)
void Unbind() const;
```

**Properties:**

```cpp
int    GetWidth()    const;
int    GetHeight()   const;
int    GetChannels() const;
GLuint GetID()       const;
bool   IsLoaded()    const;
```

### 8.6 FrameBuffer

`Engine/Rendering/FrameBuffer.h` — Off-screen render target with a color attachment and a depth/stencil renderbuffer.

```cpp
bool Create(int width, int height);
void Destroy();
void Bind()   const;   // glBindFramebuffer(GL_FRAMEBUFFER, fbo)
void Unbind() const;   // glBindFramebuffer(GL_FRAMEBUFFER, 0)
void Resize(int newWidth, int newHeight);

GLuint GetColorAttachment() const;   // OpenGL texture ID, used as ImGui image
int GetWidth()  const;
int GetHeight() const;
```

Used by the editor's `SceneViewPanel` and `GameViewPanel` to render the 3D scene and display it as a texture inside an ImGui window.

### 8.7 ModelLoader

`Engine/Rendering/ModelLoader.h` — Static class. Wraps Assimp to load 3D model files.

**Embedded texture data** (for models with textures baked into the file):

```cpp
struct EmbeddedTexture {
    std::string name;
    bool isCompressed;

    // Compressed (PNG/JPG embedded):
    std::vector<unsigned char> compressedData;

    // Uncompressed (raw RGBA):
    std::vector<unsigned char> rawData;
    int width;
    int height;
    int channels;
};
```

**Loaded material data:**

```cpp
struct LoadedMaterial {
    std::string name;
    Math::Vector4 diffuseColor  = {1,1,1,1};
    std::string   diffuseTexturePath;   // File path or embedded texture name
    float         opacity       = 1.0f;
};
```

**Full model data** (returned by `LoadModelWithAnimations`):

```cpp
struct ModelData {
    std::vector<Rendering::Mesh*>      meshes;
    Animation::Skeleton*               skeleton;
    std::vector<Animation::AnimationClip*> animations;
    std::vector<LoadedMaterial>        materials;
    std::vector<EmbeddedTexture>       embeddedTextures;
    std::string                        modelDirectory;
};
```

**Loading methods:**

```cpp
// Simple: returns flat list of meshes (no animation data):
static std::vector<Rendering::Mesh*> LoadModel(const std::string& path);

// Full: returns meshes + skeleton + animations + materials:
static ModelData LoadModelWithAnimations(const std::string& path);
```

`LoadModelWithAnimations` processes:
1. All Assimp meshes → `Rendering::Mesh` with bone ID/weight attributes.
2. The armature hierarchy → `Animation::Skeleton` with `Bone` nodes.
3. All Assimp animations → `Animation::AnimationClip` with per-bone keyframe tracks.
4. All Assimp materials → `LoadedMaterial` with diffuse color and texture path.
5. Embedded textures → `EmbeddedTexture` raw or compressed data.

The `ResourceManager` uses the simple `LoadModel` for quick mesh-only loading and the full version when `Animator` is involved.

### 8.8 Skybox

`Engine/Rendering/Skybox.h` — Renders a cubemap skybox using a dedicated cube mesh and shader. Drawn last in the render pass with depth write disabled and depth test set to `GL_LEQUAL` so it only fills pixels not covered by scene geometry.

```cpp
void Initialize(Rendering::Cubemap* cubemap, Rendering::Shader* shader);
void Render(Rendering::Camera* camera);

void SetCubemap(Rendering::Cubemap* cubemap);
Rendering::Cubemap* GetCubemap() const;

void SetEnabled(bool enabled);
bool IsEnabled() const;
```

The view matrix passed to the skybox shader has its translation component stripped (only the rotation matters) so the skybox appears infinitely far away regardless of camera position.

### 8.9 ShadowMap

`Engine/Rendering/ShadowMap.h` — A `FrameBuffer` specialized for depth-only rendering. Used by `DirectionalLight` to capture a shadow depth map.

```cpp
bool Create(int width = 1024, int height = 1024);
void Bind()   const;
void Unbind() const;

Rendering::Texture* GetDepthTexture() const;
Math::Matrix4       GetLightSpaceMatrix() const;

void SetLightSpaceMatrix(const Math::Matrix4& lsm);
int  GetWidth()  const;
int  GetHeight() const;
```

The light-space matrix is computed by `DirectionalLight::GetLightSpaceMatrix(sceneCenter, sceneRadius)` and stored here for later upload to the geometry-pass shader as `u_LightSpaceMatrix`.

### 8.10 Lighting — DirectionalLight

`Engine/Rendering/Lighting/DirectionalLight.h` — Simulates a sun-like light source with parallel rays.

```cpp
void SetDirection(const Math::Vector3& dir);    // Should be normalized
const Math::Vector3& GetDirection() const;

void SetColor(const Math::Vector3& color);
void SetIntensity(float intensity);

void SetCastShadows(bool castShadows);
bool GetCastShadows() const;

void SetShadowMapResolution(int resolution);
int  GetShadowMapResolution() const;

void SetShadowBias(float bias);         // Prevents shadow acne
float GetShadowBias() const;

ShadowMap* GetShadowMap() const;

// Computes orthographic light-space matrix for shadow map rendering:
Math::Matrix4 GetLightSpaceMatrix(const Math::Vector3& sceneCenter,
                                   float sceneRadius) const;

void ApplyToShader(Rendering::Shader* shader) const;
```

`ApplyToShader` uploads:
- `u_DirLight.direction`
- `u_DirLight.color`
- `u_DirLight.intensity`
- `u_DirLight.castShadows`
- `u_ShadowMap` (texture unit 1)
- `u_LightSpaceMatrix`
- `u_ShadowBias`

### 8.11 Lighting — PointLight

`Engine/Rendering/Lighting/PointLight.h` — An omnidirectional point light with physical attenuation.

```cpp
void SetPosition(const Math::Vector3& pos);
void SetColor(const Math::Vector3& color);
void SetIntensity(float intensity);
void SetRange(float range);

// Attenuation coefficients (Ogre / Unity convention):
void SetAttenuation(float constant, float linear, float quadratic);

void ApplyToShader(Rendering::Shader* shader, int index) const;
```

The index-based `ApplyToShader` uploads to array uniforms `u_PointLights[index].*`. The maximum number of point lights is defined by the shader (`#define MAX_POINT_LIGHTS 16` in the default lit shader).

Uploaded uniforms per light:
- `u_PointLights[i].position`
- `u_PointLights[i].color`
- `u_PointLights[i].intensity`
- `u_PointLights[i].range`
- `u_PointLights[i].constant`, `.linear`, `.quadratic`

### 8.12 Lighting — SpotLight

`Engine/Rendering/Lighting/SpotLight.h` — A cone-shaped light, like a flashlight or stage spotlight.

```cpp
void SetPosition(const Math::Vector3& pos);
void SetDirection(const Math::Vector3& dir);    // Normalized
void SetColor(const Math::Vector3& color);
void SetIntensity(float intensity);
void SetRange(float range);

// Cone angles (degrees → stored as cosine):
void SetCutOff(float innerDegrees, float outerDegrees);

void SetAttenuation(float constant, float linear, float quadratic);

void ApplyToShader(Rendering::Shader* shader, int index) const;
```

The soft edge is computed in the shader as `smoothstep(outerCos, innerCos, dot(lightDir, fragDir))`.

### 8.13 Rendering Pipeline

The full render pipeline executed each frame by `Application::Render()`:

```
1. Scene::CollectLights()
   └── Scans LightComponents, returns Light* list

2. For each DirectionalLight with CastShadows == true:
   a. ShadowMap::Bind()
   b. glViewport(0, 0, shadowRes, shadowRes)
   c. Upload u_LightSpaceMatrix to depth shader
   d. RenderSceneDepthOnly(scene)          ← draws all MeshRenderers with depth-only shader
   e. ShadowMap::Unbind()
   f. Restore main viewport

3. FrameBuffer::Bind() (if rendering to texture, e.g. editor)
   glClear(COLOR | DEPTH)

4. Upload all lights to lit shader:
   └── DirectionalLight::ApplyToShader
   └── PointLight::ApplyToShader(index)
   └── SpotLight::ApplyToShader(index)
   └── u_PointLightCount, u_SpotLightCount

5. Scene::Render(camera)
   └── For each root GameObject:
       └── GameObject::Render(camera)
           └── MeshRenderer draws mesh(es) with material bound

6. Skybox::Render(camera)
   └── GL_LEQUAL depth test, no depth write
   └── View matrix stripped of translation

7. ImGui::Render()

8. Window::SwapBuffers()
```

### 8.14 Frustum

`Engine/Rendering/Frustum.h` — View frustum culling. Extracts the six clipping planes from a view-projection matrix and tests axis-aligned bounding boxes for visibility.

```cpp
struct Plane {
    Math::Vector3 normal;
    float         distance = 0.0f;
};

void ExtractPlanes(const Math::Matrix4& viewProjection);
bool IsAABBVisible(const Math::Vector3& aabbMin, const Math::Vector3& aabbMax) const;
```

`ExtractPlanes` decomposes the combined view-projection matrix into Left, Right, Bottom, Top, Near, and Far planes. `IsAABBVisible` returns `true` if the AABB is at least partially inside all six planes.

**Static utility:**

```cpp
static void TransformAABB(const Math::Matrix4& worldMatrix,
                          const Math::Vector3& localMin,
                          const Math::Vector3& localMax,
                          Math::Vector3& outWorldMin,
                          Math::Vector3& outWorldMax);
```

Transforms a local-space AABB to world space, computing the enclosing axis-aligned bounding box of the transformed corners.

### 8.15 FbxBinding

`Engine/Rendering/FbxBinding.h` — Utility for building per-mesh materials and GameObject hierarchies from `ModelData` loaded by `ModelLoader`.

**Context and result:**

```cpp
struct FbxBindingContext {
    Core::ResourceManager& resources;
    std::string            modelPath;
    const ModelData&       modelData;
};

struct FbxBindingResult {
    std::vector<Mesh*>     meshes;
    std::vector<Material*> meshMaterials;           // Per-mesh materials
    std::vector<Texture*>  embeddedTextureObjects;  // Textures from embedded data
};
```

**Material builder:**

```cpp
inline FbxBindingResult BuildMeshesAndMaterials(const FbxBindingContext& ctx);
```

For each mesh in `modelData`, creates or reuses a `Material` based on the FBX material name. Texture resolution follows a priority chain: `.texture` asset file on disk → `.png`/`.jpg` asset → embedded texture data from the FBX file. When a diffuse texture is present, the diffuse color is forced to white to prevent FBX-exported tint colors from darkening the texture.

**Hierarchy builder:**

```cpp
ECS::GameObject* BuildFbxHierarchy(ECS::Scene* scene,
                                    const ModelData& modelData,
                                    const std::string& fbxPath,
                                    Core::ResourceManager& resources);
```

Creates a root `GameObject` (named after the FBX file) with an `Animator` if the model has animation data, then creates child `GameObject` instances for each mesh with a `MeshRenderer` configured with the appropriate material.

---

## 9. Physics Subsystem

### 9.1 PhysicsWorld

`Engine/Physics/PhysicsWorld.h` — Singleton. Owns the Bullet `btDiscreteDynamicsWorld` and all its required components (broadphase, dispatcher, solver, collision config).

```cpp
static PhysicsWorld& GetInstance();

bool Initialize();
void Step(float deltaTime);   // Calls btDynamicsWorld::stepSimulation
void Cleanup();               // Destroys all Bullet objects
```

**Body management:**

```cpp
void AddRigidBody(btRigidBody* body);
void RemoveRigidBody(btRigidBody* body);

void AddCollisionObject(btCollisionObject* obj);
void RemoveCollisionObject(btCollisionObject* obj);
```

`RemoveRigidBody` also calls `RemoveCollisionObject` internally since `btRigidBody` extends `btCollisionObject`.

**Gravity:**

```cpp
void          SetGravity(const Math::Vector3& gravity);
Math::Vector3 GetGravity() const;
```

Default gravity: `{0, -9.81f, 0}`.

**Low-level access:**

```cpp
btDiscreteDynamicsWorld* GetDynamicsWorld() const;
btDispatcher*            GetDispatcher()    const;
```

The dynamics world pointer is used by `PhysicsSystem` to iterate manifolds for collision detection.

### 9.2 RigidBody

`Engine/Physics/RigidBody.h` — Wraps a `btRigidBody`. Manages the Bullet motion state and provides a high-level API for common operations.

**Body type:**

```cpp
enum class BodyType { Static, Dynamic, Kinematic };
void SetBodyType(BodyType type);
BodyType GetBodyType() const;
```

Changing to `Static` sets mass to 0. Changing to `Kinematic` sets mass to 0 and adds `CF_KINEMATIC_OBJECT` flag. Changing to `Dynamic` restores the stored mass.

**Physical properties:**

```cpp
void SetMass(float mass);         float GetMass()         const;
void SetFriction(float f);        float GetFriction()     const;
void SetRestitution(float r);     float GetRestitution()  const;
```

**Velocity and forces (Dynamic only):**

```cpp
void SetLinearVelocity(const Math::Vector3& vel);
void SetAngularVelocity(const Math::Vector3& vel);
Math::Vector3 GetLinearVelocity()  const;
Math::Vector3 GetAngularVelocity() const;

void ApplyForce(const Math::Vector3& force, const Math::Vector3& relPos);
void ApplyCentralForce(const Math::Vector3& force);
void ApplyImpulse(const Math::Vector3& impulse, const Math::Vector3& relPos);
void ApplyCentralImpulse(const Math::Vector3& impulse);
void ApplyTorque(const Math::Vector3& torque);
void ApplyTorqueImpulse(const Math::Vector3& torque);

void ClearForces();
```

**Axis constraints:**

```cpp
void SetLinearFactor(const Math::Vector3& factor);   // {1,1,0} = lock Z movement
void SetAngularFactor(const Math::Vector3& factor);  // {0,1,0} = only Y rotation
```

**Transform sync:**

```cpp
void SetPosition(const Math::Vector3& pos);
void SetRotation(const Math::Quaternion& rot);
Math::Vector3    GetPosition() const;
Math::Quaternion GetRotation() const;
```

**Owner:**

```cpp
void             SetOwner(ECS::GameObject* owner);
ECS::GameObject* GetOwner() const;
```

The owner pointer is stored in the `btRigidBody::setUserPointer()` field so that `PhysicsSystem` can retrieve the `GameObject` from a Bullet collision pair.

**Low-level:**

```cpp
btRigidBody* GetBulletRigidBody() const;
void         SetBulletRigidBody(btRigidBody* body);
```

### 9.3 Colliders

**Base class** (`Engine/Physics/Collider.h`):

```cpp
enum class ColliderType { Box, Sphere, Capsule, Mesh };

ColliderType GetType() const;

void SetCenter(const Math::Vector3& center);
Math::Vector3 GetCenter() const;

btCollisionShape* GetCollisionShape() const;
```

The Bullet shape is created lazily when `GetCollisionShape()` is first called.

**BoxCollider** (`Engine/Physics/BoxCollider.h`):

```cpp
void          SetHalfExtents(const Math::Vector3& halfExtents);
Math::Vector3 GetHalfExtents() const;
```

Creates a `btBoxShape(halfExtents)`.

**SphereCollider** (`Engine/Physics/SphereCollider.h`):

```cpp
void  SetRadius(float radius);
float GetRadius() const;
```

Creates a `btSphereShape(radius)`.

### 9.4 PhysicsSystem

`Engine/Physics/PhysicsSystem.h` — Per-scene update system. Called from `Application::Update`.

```cpp
void Update(ECS::Scene* scene, float deltaTime);
void InitializeCollider(ECS::GameObject* go, ECS::BoxColliderComponent* boxCollider);
void Reset();
```

**`Update` internal flow:**

```
1. For each Dynamic RigidBodyComponent:
   sync Transform ← Bullet body position/rotation

2. PhysicsWorld::Step(deltaTime)
   (Bullet advances the simulation)

3. Collect collision manifolds from PhysicsWorld::GetDispatcher()
4. For each manifold with contact points:
   a. Retrieve GameObject A and B from btRigidBody::getUserPointer()
   b. Classify as Enter / Stay / Exit based on CollisionPair tracking set
   c. Dispatch OnCollisionEnter/Stay/Exit to all components on A and B
```

**CollisionPair:**

```cpp
struct CollisionPair {
    ECS::GameObject* a;
    ECS::GameObject* b;
    bool operator<(const CollisionPair& other) const;
};

std::set<CollisionPair> activeCollisions;   // private
std::set<CollisionPair> previousCollisions; // private
```

At the end of each update, `previousCollisions` = `activeCollisions`. Next frame, pairs in `previous` but not in `active` generate `OnCollisionExit` events.

**`InitializeCollider`** creates the Bullet rigid body:

```
1. Get BoxCollider size and center from BoxColliderComponent
2. Create btBoxShape(halfExtents)
3. Create btDefaultMotionState with initial world transform
4. Compute inertia for dynamic bodies (btBoxShape::calculateLocalInertia)
5. Create btRigidBody with mass, motion state, shape
6. Set friction, restitution, body type flags
7. Store owner pointer in btRigidBody::setUserPointer
8. Add to PhysicsWorld
```

### 9.5 CollisionInfo

`Engine/Physics/CollisionInfo.h` — Passed to all collision callbacks.

```cpp
struct CollisionInfo {
    ECS::GameObject* other;          // The other object in the collision
    Math::Vector3    contactPoint;   // World-space contact position
    Math::Vector3    contactNormal;  // Normal pointing from other toward self
    float            penetrationDepth;
};
```

### 9.6 Physics Lifecycle

The correct order of physics operations across a deferred scene reload:

```
1. User requests scene load (SceneManager::RequestSceneLoad)
2. onSceneUnloading fires → Application::ResetPhysics()
   └── PhysicsWorld removes all btRigidBody objects
   └── PhysicsSystem::Reset() clears collision tracking
3. activeScene.reset() — all GameObjects destroyed
   └── ~BoxColliderComponent deletes btCollisionObject safely (already removed)
4. SceneLoader::Load — new Scene created, GameObjects and components added
5. onSceneLoaded fires → Application::InitializePhysicsForScene(newScene)
   └── For each RigidBodyComponent + BoxColliderComponent pair:
       PhysicsSystem::InitializeCollider(go, boxCollider)
```

The deferred path matters because scene changes can now be requested from gameplay and then completed later by `SceneManager::ProcessPendingSceneLoad()`. When play mode stops, any queued request is cleared before the editor restores the pre-play scene.

**Critical**: steps 2 and 3 must happen in that order. If `activeScene.reset()` runs before `ResetPhysics()`, the `btDiscreteDynamicsWorld` will access freed broadphase proxies in its internal structures when the bodies are removed, causing an access violation (`0xC0000005`).

### 9.7 PhysicsUtils

`Engine/Physics/PhysicsUtils.h` — Inline conversion functions between RTBEngine math types and Bullet Physics types. Used internally by `PhysicsSystem` and `RigidBody`.

```cpp
namespace RTBEngine::Physics::PhysicsUtils {

    inline btVector3    ToBullet(const Math::Vector3& v);
    inline btQuaternion ToBullet(const Math::Quaternion& q);

    inline Math::Vector3    FromBullet(const btVector3& v);
    inline Math::Quaternion FromBullet(const btQuaternion& q);

}
```

---

## 10. Audio Subsystem

### 10.1 AudioSystem

`Engine/Audio/AudioSystem.h` — Singleton. Wraps an `FMOD::System` instance.

```cpp
static AudioSystem& GetInstance();

bool Initialize(int maxChannels = 512);
void Update();      // Must be called every frame
void Shutdown();

FMOD::System* GetFMODSystem() const;
```

`GetFMODSystem()` exposes the raw FMOD handle for advanced use (3D listener positioning, DSP effects, channel groups, etc.).

Setting up the FMOD 3D listener (typically done in `Application::Update` or a CameraComponent):

```cpp
FMOD_VECTOR pos      = { camPos.x, camPos.y, camPos.z };
FMOD_VECTOR vel      = { 0, 0, 0 };
FMOD_VECTOR forward  = { fwd.x, fwd.y, fwd.z };
FMOD_VECTOR up       = { up.x,  up.y,  up.z  };
AudioSystem::GetInstance().GetFMODSystem()->set3DListenerAttributes(0, &pos, &vel, &forward, &up);
```

### 10.2 AudioClip

`Engine/Audio/AudioClip.h` — Wraps an `FMOD::Sound`.

```cpp
bool LoadFromFile(const std::string& path, bool stream = false);

bool          IsLoaded()    const;
const std::string& GetFilePath() const;
float         GetLength()   const;   // Duration in seconds
FMOD::Sound*  GetSound()    const;
```

`AudioSourceComponent` calls `AudioSystem::GetFMODSystem()->playSound(clip->GetSound(), ...)` to play the audio on a channel.

---

## 11. Input Subsystem

### 11.1 InputManager

`Engine/Input/InputManager.h` — Singleton. Processes `SDL_Event` structures and exposes a per-frame snapshot of keyboard and mouse state.

```cpp
static InputManager& GetInstance();

void ProcessEvent(const SDL_Event& event);   // Call inside the SDL event loop
void Update();                               // Call once per frame to flush just-pressed/released
```

The public header keeps SDL behind a forward declaration so gameplay and script code can include the engine headers without inheriting SDL includes. SDL stays in the implementation file.

**Keyboard:**

```cpp
bool IsKeyPressed(KeyCode key)      const;   // Held down this frame
bool IsKeyJustPressed(KeyCode key)  const;   // First frame it was pressed
bool IsKeyJustReleased(KeyCode key) const;   // First frame it was released
```

**Mouse buttons:**

```cpp
bool IsMouseButtonPressed(MouseButton button)      const;
bool IsMouseButtonJustPressed(MouseButton button)  const;
bool IsMouseButtonJustReleased(MouseButton button) const;
```

**Mouse position and movement:**

```cpp
int   GetMouseX()      const;   // Screen-space X pixel
int   GetMouseY()      const;   // Screen-space Y pixel
int   GetMouseDeltaX() const;   // Pixels moved since last frame
int   GetMouseDeltaY() const;
float GetScrollDelta() const;   // Vertical scroll wheel
```

**Mouse control:**

```cpp
void SetMouseRelativeMode(bool enabled);   // Locks cursor and returns only deltas
void SetMousePosition(int x, int y);
```

### 11.2 KeyCode

`Engine/Input/KeyCode.h` — Enum wrapping SDL keycodes. Values match `SDLK_*` constants.

Selected values:

```
KeyCode::W, A, S, D, Q, E         Movement
KeyCode::Space                     Jump / confirm
KeyCode::Escape                    Cancel / quit
KeyCode::LeftShift, LeftControl    Modifiers
KeyCode::F1 … F12                  Function keys
KeyCode::Alpha0 … Alpha9           Number row
KeyCode::Delete, Backspace
KeyCode::Up, Down, Left, Right     Arrow keys
```

### 11.3 MouseButton

`Engine/Input/MouseButton.h` — Enum for mouse buttons.

```
MouseButton::Left
MouseButton::Right
MouseButton::Middle
MouseButton::X1       // Extra button 1 (side button)
MouseButton::X2       // Extra button 2
```

---

## 12. Animation Subsystem

### 12.1 Bone

`Engine/Animation/Bone.h` — Represents a single joint in a skeleton.

```cpp
struct Bone {
    std::string  name;
    int          parentIndex;       // -1 for root bones
    Math::Matrix4 offsetMatrix;     // Inverse bind-pose transform
};
```

`offsetMatrix` transforms from mesh space to bone local space. Used by the skinning shader to compute final vertex positions.

### 12.2 Skeleton

`Engine/Animation/Skeleton.h` — The full bone hierarchy for a skinned mesh.

```cpp
void AddBone(const Bone& bone);
const Bone& GetBone(int index) const;
int         GetBoneIndex(const std::string& name) const;   // -1 if not found
int         GetBoneCount() const;

void          SetGlobalInverseTransform(const Math::Matrix4& m);
Math::Matrix4 GetGlobalInverseTransform() const;

void CalculateBoneTransforms(
    const std::vector<Math::Matrix4>& localTransforms,
    std::vector<Math::Matrix4>& outFinalTransforms
) const;
```

`CalculateBoneTransforms` walks the bone hierarchy, multiplying each bone's local transform by its parent's accumulated transform, then by the offset matrix and the global inverse, to produce the final GPU-ready skinning matrices.

### 12.3 AnimationClip

`Engine/Animation/AnimationClip.h` — A named animation that stores per-bone keyframe tracks.

**Keyframe types:**

```cpp
struct VectorKey {
    double        time;
    Math::Vector3 value;
};

struct QuatKey {
    double           time;
    Math::Quaternion value;
};
```

**Per-bone track:**

```cpp
struct BoneAnimation {
    std::string           boneName;
    std::vector<VectorKey> positionKeys;
    std::vector<QuatKey>   rotationKeys;
    std::vector<VectorKey> scaleKeys;
};
```

**Clip properties:**

```cpp
const std::string& GetName()              const;
double             GetDuration()          const;   // In ticks
double             GetTicksPerSecond()    const;
float              GetDurationInSeconds() const;
```

**Interpolation:**

```cpp
bool GetBoneTransform(const std::string& boneName,
                      float timeSeconds,
                      Math::Matrix4& outMatrix) const;
```

Internally performs:
1. Binary search for surrounding keys at `timeSeconds`.
2. Linear interpolation of position (`lerp`) and scale (`lerp`).
3. Spherical linear interpolation of rotation (`slerp`).
4. Assembles TRS matrix.

### 12.4 Animator

`Engine/ECS/Animator.h` — Component that plays `AnimationClip`s and produces per-frame bone transform arrays for GPU upload.

**Skeleton and clips:**

```cpp
void SetSkeleton(Animation::Skeleton* skeleton);
Animation::Skeleton* GetSkeleton() const;

void AddClip(const std::string& name, Animation::AnimationClip* clip);
Animation::AnimationClip* GetClip(const std::string& name) const;
```

**Playback:**

```cpp
void Play(const std::string& clipName);
void Stop();
void Pause();
void Resume();

bool IsPlaying() const;
bool IsPaused()  const;
float GetCurrentTime()       const;   // Seconds within current clip
const std::string& GetCurrentClipName() const;
```

**Speed and looping:**

```cpp
void SetSpeed(float speed);     float GetSpeed()   const;
void SetLooping(bool loop);     bool  GetLooping() const;
```

**Bone transforms** (upload to skinning shader):

```cpp
const std::vector<Math::Matrix4>& GetBoneTransforms() const;
```

Returns a vector of `GetSkeleton()->GetBoneCount()` matrices. The skinning shader reads them as `uniform mat4 u_BoneMatrices[MAX_BONES]`.

**Mesh loading:**

```cpp
void SetMeshes(const std::vector<Rendering::Mesh*>& meshes);
const std::vector<Rendering::Mesh*>& GetMeshes() const;
```

**Clip management:**

```cpp
std::vector<std::string> GetClipNames() const;
Rendering::Mesh* GetFirstMesh() const;
```

**Bone GameObjects** (creates a visual hierarchy of empty GameObjects that mirror the skeleton):

```cpp
void CreateBoneGameObjects(ECS::Scene* scene, ECS::GameObject* parent);
void SyncBoneGameObjects();
ECS::GameObject* GetBoneGameObject(const std::string& boneName) const;
ECS::GameObject* GetBoneGameObject(int boneIndex) const;
bool AreBoneGOsCreated() const;
```

Bone GameObjects are created once (usually by the editor or scene loader) and synced each frame by `SyncBoneGameObjects()`, which updates each bone GO's transform to match the current animation pose.

**Additional reflected properties:**

```cpp
std::string additionalModels;   // Semicolon-separated paths to additional FBX models
std::string defaultClip;        // Clip to play on start (alternative to currentClipName)
```

**Reflected properties (Proxy):**

```cpp
std::string modelRef;         // Path to FBX/DAE model
std::string currentClipName;  // Name of the clip to play
float       speedRef    = 1.0f;
bool        playingRef  = false;
bool        loopingRef  = true;
```

**Lifecycle:**

- `OnAwake`: initializes the bone transform buffer.
- `OnStart`: if `playingRef` is true, calls `Play(currentClipName)`.
- `OnUpdate(dt)`: advances `currentTime` by `dt * speed`, loops if `looping`, calls `AnimationClip::GetBoneTransform` for each bone, calls `Skeleton::CalculateBoneTransforms`, uploads results to the skinning shader via `MeshRenderer`'s shader.

---

## 13. Math Library

All types are in `RTBEngine::Math`. Headers are in `Engine/Math/`.

### 13.1 Vector2

`float x, y`

```cpp
Vector2(float x = 0, float y = 0);
Vector2 operator+(const Vector2&) const;
Vector2 operator-(const Vector2&) const;
Vector2 operator*(float scalar)   const;
Vector2 operator/(float scalar)   const;
Vector2& operator+=(const Vector2&);
Vector2& operator-=(const Vector2&);
bool operator==(const Vector2&) const;
bool operator!=(const Vector2&) const;

float   Dot(const Vector2& other) const;
float   Length()        const;
float   LengthSquared() const;
Vector2 Normalized()    const;
void    Normalize();

static Vector2 Zero();
static Vector2 One();
```

### 13.2 Vector3

`float x, y, z`

```cpp
Vector3(float x = 0, float y = 0, float z = 0);

// Arithmetic operators: +, -, * (scalar), * (component-wise), /, +=, -=, *=, /=, unary -
// Comparison: ==, !=

float   Dot(const Vector3& other)   const;
Vector3 Cross(const Vector3& other) const;
float   Length()        const;
float   LengthSquared() const;
Vector3 Normalized()    const;
void    Normalize();

// Static direction helpers:
static Vector3 Zero();
static Vector3 One();
static Vector3 Up();       // { 0, 1, 0}
static Vector3 Down();     // { 0,-1, 0}
static Vector3 Left();     // {-1, 0, 0}
static Vector3 Right();    // { 1, 0, 0}
static Vector3 Forward();  // { 0, 0,-1}
static Vector3 Back();     // { 0, 0, 1}
```

### 13.3 Vector4

`float x, y, z, w`

```cpp
Vector4(float x = 0, float y = 0, float z = 0, float w = 0);

// Arithmetic: +, -, *, /, +=, -=, *=
// Dot product, length, normalize
// Swizzles: xyz() → Vector3
```

Also used as RGBA color where `{r, g, b, a}` maps to `{x, y, z, w}`.

### 13.4 Quaternion

`float x, y, z, w`

```cpp
Quaternion(float x = 0, float y = 0, float z = 0, float w = 1);

Quaternion operator*(const Quaternion& other) const;   // Composition
Vector3    operator*(const Vector3& v)         const;   // Rotate vector

float      Dot(const Quaternion& other)     const;
float      Length()                         const;
Quaternion Normalized()                     const;
Quaternion Conjugate()                      const;
Quaternion Inverse()                        const;

Vector3    ToEulerAngles() const;     // Returns degrees (pitch, yaw, roll)
Matrix4    ToMatrix4()     const;

static Quaternion Identity();
static Quaternion FromEuler(float pitchDeg, float yawDeg, float rollDeg);
static Quaternion FromAxisAngle(const Vector3& axis, float angleDeg);
static Quaternion LookRotation(const Vector3& forward, const Vector3& up = Vector3::Up());
static Quaternion Slerp(const Quaternion& a, const Quaternion& b, float t);
```

### 13.5 Matrix4

`float m[4][4]` (column-major, matching OpenGL convention)

```cpp
Matrix4();   // Identity
Matrix4(float m00, float m01, ..., float m33);

Matrix4 operator*(const Matrix4& other) const;
Vector4 operator*(const Vector4& v)     const;
Vector3 operator*(const Vector3& v)     const;   // w=1 implied

Matrix4 Transposed() const;
Matrix4 Inversed()   const;

const float* Data() const;    // Raw pointer for glUniformMatrix4fv

// Decomposition:
Vector3    GetTranslation() const;
Quaternion GetRotation()    const;
Vector3    GetScale()       const;
Vector3    GetForward()     const;
Vector3    GetRight()       const;
Vector3    GetUp()          const;

// Factory:
static Matrix4 Identity();
static Matrix4 Translation(const Vector3& t);
static Matrix4 Rotation(const Quaternion& q);
static Matrix4 Scale(const Vector3& s);
static Matrix4 TRS(const Vector3& t, const Quaternion& r, const Vector3& s);
static Matrix4 LookAt(const Vector3& eye, const Vector3& center, const Vector3& up);
static Matrix4 Perspective(float fovDeg, float aspect, float nearZ, float farZ);
static Matrix4 Orthographic(float left, float right, float bottom, float top,
                             float nearZ, float farZ);
```

### 13.6 Color

`Engine/Math/Color.h` — RGBA color struct with implicit `Vector4` conversion. Provides predefined color constants.

```cpp
struct Color {
    float r, g, b, a;

    Color();                                       // Default: white (1,1,1,1)
    Color(float r, float g, float b, float a = 1.0f);
    explicit Color(float value);                   // Grayscale (value, value, value, 1)
    Color(const Vector4& v);                       // Implicit from Vector4
    operator Vector4() const;                      // Implicit to Vector4

    static Color White();    // {1, 1, 1, 1}
    static Color Black();    // {0, 0, 0, 1}
    static Color Red();      // {1, 0, 0, 1}
    static Color Green();    // {0, 1, 0, 1}
    static Color Blue();     // {0, 0, 1, 1}
    static Color Clear();    // {0, 0, 0, 0}
};
```

---

## 14. Reflection System

The reflection system allows the editor Inspector to display and edit component properties without knowing their concrete types at compile time. It also drives the serializer (`SceneSaver`) and deserializer (`SceneLoader`).

### 14.1 PropertyType and PropertyFlags

`Engine/Reflection/TypeInfo.h`

```cpp
enum class PropertyType {
    Bool,
    Int,
    Float,
    Double,
    String,
    Vector2,
    Vector3,
    Vector4,
    Quaternion,
    Color,
    Enum,
    AssetRef,       // Typed asset path string, typically serialized as Assets/...
    TextureRef,     // Texture asset path
    AudioClipRef,   // AudioClip asset path
    MeshRef,        // Mesh asset path
    FontRef,        // Font asset path
    GameObjectRef,  // Pointer to a GameObject (resolved by UUID at load time)
    ComponentRef,   // Pointer to a specific component type
};

enum class PropertyFlags : uint32_t {
    None             = 0,
    Serialize        = 1 << 0,   // Include in scene file
    HideInInspector  = 1 << 1,   // Don't show in Inspector
    ReadOnly         = 1 << 2,   // Show but don't allow editing
};
```

### 14.2 PropertyInfo

```cpp
struct PropertyInfo {
    std::string name;
    std::string displayName;
    PropertyType type;
    size_t offset;               // Byte offset from the start of the actual reflected object
    size_t size;
    PropertyFlags flags;

    std::optional<Range> range;
    std::optional<std::string> tooltip;
    std::optional<std::string> category;
    std::vector<std::string> enumNames;
    std::string assetType;          // Only used when type == AssetRef
    std::string componentTypeName;  // Only used when type == ComponentRef

    void* GetMutableData(void* objectBase) const;
    const void* GetData(const void* objectBase) const;

    void* GetMutableData(ECS::Component* component) const;
    const void* GetData(const ECS::Component* component) const;
};
```

`PropertyInfo::offset` is still the core piece of metadata, but it is no longer applied directly to `Component*`.

The runtime now resolves property addresses through `PropertyInfo::GetData(...)` / `GetMutableData(...)`, which:

1. Ask the component for its real dynamic object via `GetActualObject()`
2. Apply the offset relative to that actual object

This is what makes reflected properties safe for components that participate in multiple inheritance, such as UI controls implementing pointer-handler interfaces.

The offset itself is computed by `GetMemberOffset<OwnerClass, DeclaringClass>(...)`, which supports inherited properties by distinguishing:

- `OwnerClass`: the final reflected type being registered
- `DeclaringClass`: the class where the member is declared

Example:

```cpp
GetMemberOffset<UIText, UIElement>(&UIElement::anchorMin)
```

This computes the offset of `UIElement::anchorMin` inside a full `UIText` object.

For typed asset references, `assetType` tells editor tooling which logical asset family the string belongs to. A property registered as `RTB_PROPERTY_FBX(idleAnimationFbx)` is still stored in the component as a plain `std::string`, but the Inspector can restrict drag-and-drop and browsing to `.fbx` files while preserving the logical `Assets/...` path.

### 14.3 TypeInfo

```cpp
class TypeInfo {
public:
    using FactoryFunc = ECS::Component*(*)(void* context);
    using DestroyFunc = void(*)(ECS::Component*, void* context);

    TypeInfo() = default;
    TypeInfo(const char* typeName, FactoryFunc factory = nullptr);

    // Component creation/destruction (ABI-safe across DLL boundaries):
    ECS::Component* Create() const;
    void Destroy(ECS::Component* c) const;
    void SetFactory(FactoryFunc fn, void* ctx);
    void SetDestroyer(DestroyFunc fn, void* ctx);

    // Property registry:
    void AddProperty(const PropertyInfo& prop);
    void AddPropertyPOD(const char* name, PropertyType type, size_t offset, size_t size, PropertyFlags flags);
    void AddPropertyPODRange(const char* name, PropertyType type, size_t offset, size_t size, PropertyFlags flags, float rangeMin, float rangeMax);
    void AddPropertyPODEnum(const char* name, size_t offset, size_t size, PropertyFlags flags, const char* const* enumNames, int enumCount);
    void AddPropertyPODTyped(const char* name, PropertyType type, size_t offset, size_t size, PropertyFlags flags, const char* extraTypeName);
    const std::vector<PropertyInfo>& GetProperties() const;
    const PropertyInfo* GetProperty(const std::string& name) const;

    // Filtered views:
    std::vector<const PropertyInfo*> GetInspectorProperties()  const;  // !HideInInspector
    std::vector<const PropertyInfo*> GetSerializableProperties() const; // Serialize flag set
};
```

The runtime stores factory and destroy callbacks as raw function pointers plus opaque context instead of `std::function`. This keeps the reflection metadata POD-friendly across module boundaries and avoids allocator / CRT ownership issues when types come from `GameScripts.dll`.

### 14.4 TypeRegistry

Singleton that maps type name strings to `TypeInfo` objects.

```cpp
static TypeRegistry& GetInstance();

void RegisterType(const std::string& typeName, const TypeInfo& info);
void UnregisterType(const std::string& typeName);
bool HasType(const std::string& typeName) const;
const TypeInfo* GetTypeInfo(const std::string& typeName) const;
ECS::Component* CreateComponent(const std::string& name) const;

std::vector<std::string> GetRegisteredTypes() const;
void ForEachType(void(*callback)(const char* typeName, const TypeInfo* info, void* userData), void* userData) const;
```

All engine built-in components are registered at startup. Script components (from `GameScripts.dll`) register themselves via `RTB_REGISTER_COMPONENT` which runs as a static initializer when the DLL is loaded.

### 14.5 PropertyMacros

`Engine/Reflection/PropertyMacros.h` — Macro system that generates the `TypeInfo` registration boilerplate.

**In the class header:**

```cpp
float speedRef = 5.0f;

RTB_COMPONENT(MyComponent)
```

`RTB_COMPONENT(ClassName)` generates:

```cpp
// Generated inside the class:
virtual const char* GetTypeName() const override { return "MyComponent"; }
virtual void* GetActualObject() override { return this; }
virtual const void* GetActualObject() const override { return this; }
virtual const Reflection::TypeInfo* GetTypeInfo() const override;
static const Reflection::TypeInfo& StaticTypeInfo();
static Reflection::TypeInfo& MutableTypeInfo();
```

**In the .cpp implementation file:**

```cpp
using ThisClass = MyComponent;

RTB_REGISTER_COMPONENT(MyComponent)
    RTB_PROPERTY_RANGE(speedRef, 0.0f, 100.0f)
    RTB_PROPERTY_GAMEOBJECT(targetRef)
RTB_END_REGISTER(MyComponent)
```

`RTB_REGISTER_COMPONENT(ClassName)` defines a static registrar object. Its constructor:

1. Retrieves the type's `MutableTypeInfo()`
2. Sets factory and destroy callbacks for the owning module
3. Registers each reflected property
4. Registers the final `TypeInfo` in `TypeRegistry`

Inside that registrar:

- `RTBCurrentClass` is the final reflected type being registered
- `ThisClass` is the class that declares the property currently being added

For regular properties both are the same type:

```cpp
using ThisClass = MyComponent;

RTB_REGISTER_COMPONENT(MyComponent)
    RTB_PROPERTY(speedRef) // GetMemberOffset<MyComponent, MyComponent>(&MyComponent::speedRef)
RTB_END_REGISTER(MyComponent)
```

For inherited properties, rebind `ThisClass` inside a scoped block. Alternatively, use `RTB_PROPERTY_NESTED_HIDDEN` for fields that live inside an inline sub-object (e.g. `UIElement::rectTransform.anchorMin`):

```cpp
// Registers a Serialize+HideInInspector field inside an inline sub-object.
// Computes the combined offset at registration time using a stack probe.
RTB_PROPERTY_NESTED_HIDDEN(rectTransform, RectTransform, anchorMin)
RTB_PROPERTY_NESTED_HIDDEN(rectTransform, RectTransform, sizeDelta)
```

When these macros are compiled inside `GameScripts.dll`, the registration path uses POD descriptors instead of moving `TypeInfo` or STL containers across the DLL boundary. The engine reconstructs its own runtime metadata on its side, which keeps the bridge stable even when the script DLL is rebuilt separately.

For properties inherited from a base class that declares them directly, rebind `ThisClass` inside a scoped block:

```cpp
using ThisClass = UIButton;

RTB_REGISTER_COMPONENT(UIButton)
    RTB_PROPERTY(label)
    { using ThisClass = UIElement; RTB_PROPERTY(isVisible) }
RTB_END_REGISTER(UIButton)
```

This pattern works across deeper inheritance chains as long as `OwnerClass*` can be converted to `DeclaringClass*`.

**Full property macro reference:**

| Macro | Generated PropertyType | Inspector Widget |
|-------|----------------------|-----------------|
| `RTB_PROPERTY(name)` | Auto-detected from member type | Type-appropriate |
| `RTB_PROPERTY_RANGE(name, min, max)` | Float or Int | Slider |
| `RTB_PROPERTY_COLOR(name)` | Color | RGBA picker |
| `RTB_PROPERTY_ENUM(name, "A", "B", ...)` | Enum | Dropdown |
| `RTB_PROPERTY_TEXTURE(name)` | TextureRef | Path + asset browser button |
| `RTB_PROPERTY_AUDIOCLIP(name)` | AudioClipRef | Path + asset browser button |
| `RTB_PROPERTY_MESH(name)` | MeshRef | Path + asset browser button |
| `RTB_PROPERTY_FONT(name)` | FontRef | Path + asset browser button |
| `RTB_PROPERTY_GAMEOBJECT(name)` | GameObjectRef | Drag-and-drop from Hierarchy |
| `RTB_PROPERTY_COMPONENT(name, TypeName)` | ComponentRef | Drag-and-drop from Hierarchy |
| `RTB_PROPERTY_ASSET_PATH(name, "fbx")` | AssetRef | Typed asset slot + filtered browser |
| `RTB_PROPERTY_FBX(name)` | AssetRef | FBX-only drag-and-drop slot |
| `RTB_PROPERTY_HIDDEN(name)` | (any) | Not shown, but serialized |
| `RTB_PROPERTY_READONLY(name)` | (any) | Shown, greyed out |
| `RTB_PROPERTY_SERIALIZED(name)` | (any) | Serialized, not shown |
| `RTB_PROPERTY_NESTED_HIDDEN(outer, InnerType, inner)` | Auto-detected from inner field | Serialized, hidden — for inline sub-objects |

`RTB_PROPERTY_ASSET_PATH` and the convenience alias `RTB_PROPERTY_FBX` are the recommended way to expose typed script-side asset slots. They keep the underlying value as a serialized logical path while letting the editor provide an asset picker and filtered drag-and-drop workflow instead of a raw text field.

### 14.6 Writing a Reflectable Component

Complete example: a reflected UI component with its own properties plus inherited reflected layout properties.

**Header** (`Engine/UI/Elements/UIText.h`):

```cpp
#pragma once
#include "UIElement.h"

class UIText : public UIElement {
public:
    std::string text;
    Math::Vector4 color = Math::Vector4(1, 1, 1, 1);
    float fontSize = 16.0f;

    RTB_COMPONENT(UIText)
};
```

**Implementation** (`Engine/UI/Elements/UIText.cpp`):

```cpp
#include "UIText.h"

using ThisClass = UIText;

RTB_REGISTER_COMPONENT(UIText)
    RTB_PROPERTY(text)
    RTB_PROPERTY_COLOR(color)
    RTB_PROPERTY(fontSize)
    { using ThisClass = UIElement; RTB_PROPERTY(isVisible) }
    RTB_PROPERTY_NESTED_HIDDEN(rectTransform, RectTransform, anchorMin)
    RTB_PROPERTY_NESTED_HIDDEN(rectTransform, RectTransform, anchorMax)
    RTB_PROPERTY_NESTED_HIDDEN(rectTransform, RectTransform, anchoredPosition)
    RTB_PROPERTY_NESTED_HIDDEN(rectTransform, RectTransform, sizeDelta)
    RTB_PROPERTY_NESTED_HIDDEN(rectTransform, RectTransform, rotation)
    RTB_PROPERTY_NESTED_HIDDEN(rectTransform, RectTransform, scale)
RTB_END_REGISTER(UIText)
```

Recommended registration pattern for UI components:

- `using ThisClass = UIText` for properties declared directly on the subclass
- `{ using ThisClass = UIElement; ... }` for properties declared on `UIElement` itself (e.g. `isVisible`)
- `RTB_PROPERTY_NESTED_HIDDEN` for all transform fields — they live inside the inline `rectTransform` member, not directly on `UIElement`

### 14.7 TypeInfoBuilder

`Engine/Reflection/TypeInfoBuilder.h` — Template class providing a fluent API for constructing `TypeInfo` instances programmatically, as an alternative to the property macros.

```cpp
template<typename T>
class TypeInfoBuilder {
public:
    explicit TypeInfoBuilder(const char* typeName);

    template<typename PropType>
    TypeInfoBuilder& Property(const char* name, PropType T::* member,
                              PropertyFlags flags = PropertyFlags::None);

    template<typename PropType>
    TypeInfoBuilder& PropertyWithRange(const char* name, PropType T::* member,
                                       float min, float max,
                                       PropertyFlags flags = PropertyFlags::None);

    template<typename PropType>
    TypeInfoBuilder& PropertyWithTooltip(const char* name, PropType T::* member,
                                         const char* tooltip,
                                         PropertyFlags flags = PropertyFlags::None);

    template<typename EnumType>
    TypeInfoBuilder& PropertyEnum(const char* name, EnumType T::* member,
                                   std::initializer_list<const char*> enumNames,
                                   PropertyFlags flags = PropertyFlags::None);

    template<typename AssetType>
    TypeInfoBuilder& PropertyAsset(const char* name, AssetType* T::* member,
                                    const char* assetTypeName,
                                    PropertyFlags flags = PropertyFlags::None);

    TypeInfoBuilder& Category(const char* categoryName);

    TypeInfo  Build();
    TypeInfo& GetInfo();
};
```

**Usage example:**

```cpp
TypeInfoBuilder<MyComponent> builder("MyComponent");
builder
    .Property("enabled", &MyComponent::enabledRef)
    .PropertyWithRange("speed", &MyComponent::speedRef, 0.0f, 100.0f)
    .PropertyEnum("mode", &MyComponent::modeRef, {"Auto", "Manual", "Custom"})
    .PropertyAsset("texture", &MyComponent::texturePtr, "Texture");
TypeInfo info = builder.Build();
```

`TypeInfoBuilder` currently uses `GetMemberOffset<T, T>(member)`, so it is ideal when the property is declared directly on `T`.

For inherited members where the declaring type differs from the owning reflected type, the macro-based registration style is usually clearer because it can express:

```cpp
GetMemberOffset<OwnerClass, DeclaringClass>(&DeclaringClass::member)
```

Template specializations for `MakePropertyInfo` are provided for `Vector2`, `Vector3`, `Vector4`, and `Quaternion` to ensure correct `PropertyType` mapping.

---

## 15. Scripting and Serialization

### 15.1 ComponentRegistry

`Engine/Scripting/ComponentRegistry.h` — Singleton. Stores factory functions for component types. Unlike `TypeRegistry` (which handles reflection metadata), this registry is focused on runtime instantiation.

```cpp
static ComponentRegistry& GetInstance();

void             RegisterComponent(const std::string& typeName,
                                   std::function<ECS::Component*()> factory);
ECS::Component*  CreateComponent(const std::string& typeName) const;
bool             HasComponent(const std::string& typeName) const;

// Resolve the registered TypeInfo for a type name (queries TypeRegistry):
const Reflection::TypeInfo* GetComponentTypeInfo(const std::string& typeName) const;

// Destroy a component through its registered TypeInfo Destroy function.
// Use instead of raw delete to keep allocation/deallocation in the same module:
void DestroyComponent(const std::string& typeName, ECS::Component* component) const;

void RegisterBuiltInComponents();   // Called by Application::Initialize()
```

Built-in types registered: `MeshRenderer`, `CameraComponent`, `LightComponent`, `RigidBodyComponent`, `BoxColliderComponent`, `SphereColliderComponent`, `AudioSourceComponent`, `FreeLookCamera`, `Canvas`, `UIText`, `UIImage`, `UIPanel`, `UIButton`, `UIContainer`.

Script DLLs typically register through `RTB_REGISTER_COMPONENT` static initializers, which call both `ComponentRegistry::RegisterComponent` and `TypeRegistry::RegisterType`.

### 15.2 SceneLoader

`Engine/Scripting/SceneLoader.h` — Reads `.lua` scene files and populates an `ECS::Scene`.

Scene files use a Lua table format:

```lua
return {
  gameObjects = {
    {
      name = "Cube",
      uuid = 12345678,
      active = true,
      transform = {
        position = {x=0, y=1, z=0},
        rotation = {x=0, y=0, z=0, w=1},
        scale    = {x=1, y=1, z=1}
      },
      components = {
        {
          type = "MeshRenderer",
          properties = {
            meshRef    = "Assets/Models/Cube.obj",
            textureRef = "Assets/Textures/stone.png",
            colorRef   = {x=1, y=1, z=1, w=1}
          }
        }
      },
      children = { ... }
    }
  }
}
```

**Load sequence:**

```
1. Open Lua state, execute scene file
2. Iterate gameObjects table recursively (handles parent-child nesting)
3. For each object:
   a. scene.AddGameObject(name)
   b. Set UUID, active flag
   c. Apply transform (position, rotation, scale)
   d. For each component entry:
      i.  registeredTypeInfo = ComponentRegistry::GetComponentTypeInfo(type)
      ii. ComponentRegistry::CreateComponent(type)
      iii. gameObject.AddComponent(component, registeredTypeInfo)
      iv. SceneReflectionUtils::ApplyLuaTableToComponent(L, tableIndex, type, component)
      v.  component.OnValidate()
      vi. Duplicates destroyed via ComponentRegistry::DestroyComponent (never raw delete)
4. Deferred UUID resolution:
   For each GameObjectRef property:
      Resolve UUID -> GameObject* and write through the reflected property accessor
5. Set parent-child relationships from hierarchy
6. SceneManager stores the new scene as active
```

### 15.3 SceneSaver

`Engine/Scripting/SceneSaver.h` — Serializes the active `Scene` to a `.lua` file.

For each `GameObject`, iterates its components. For each component, calls `TypeInfo::GetSerializableProperties()` and reads each proxy member value via its `offset`, then formats it as Lua table syntax.

`GameObjectRef` properties are serialized as the target `GameObject`'s UUID (an integer), not a pointer. On load, `SceneLoader` performs UUID-to-pointer resolution after all objects are created.

### 15.4 ScriptManager

`Engine/Scripting/ScriptManager.h` — Singleton that loads and unloads `GameScripts.dll` at runtime, triggering component type registration.

```cpp
static ScriptManager& GetInstance();

bool LoadScripts(const std::string& dllPath);   // Loads DLL, calls RTBScripts_RegisterAll
void UnloadScripts();                            // Unloads DLL, unregisters script types
bool IsLoaded() const;
const std::string& GetLoadedPath() const;
```

When `LoadScripts` is called, it loads the DLL and invokes the exported `RTBScripts_RegisterAll` function, which bridges each script component's type information across the DLL boundary using POD structs (no STL crossing). Each type is registered with both `TypeRegistry` and `ComponentRegistry`.

The bridge uses two plain-data descriptors: `RTBScriptTypeDesc` for the type-level factory and destroy callbacks, and `RTBPropertyDesc` for each reflected property. `TypeInfo`, `PropertyInfo`, `std::string`, and `std::vector` stay on the engine side and are rebuilt there from the descriptors.

### 15.5 Latent Actions

`Engine/Scripting/LatentActions.h` — Header-only, frame-driven latent action system for component-owned coroutine-style flows in C++17.

This API exists to replace "always do a little work in `OnUpdate`" patterns with explicit short-lived sequences. It is intentionally not C++20 coroutines and intentionally not a global scheduler.

**Public API:**

```cpp
struct LatentActionHandle {
    std::uint64_t id = 0;
    bool IsValid() const;
    explicit operator bool() const;
};

class LatentSequence {
public:
    LatentSequence& Call(std::function<void()> callback);
    LatentSequence& Wait(float seconds);
    LatentSequence& Tween(float durationSec,
                          std::function<void(float)> onUpdate,
                          std::function<void()> onComplete = {});
};

class LatentActionRunner {
public:
    LatentActionHandle Play(LatentSequence sequence);
    void Stop(LatentActionHandle handle);
    void StopAll();
    bool HasActiveActions() const;
    void Tick(float deltaTime);
};
```

**Why it is header-only:**

- No new engine `.cpp` file or Visual Studio project change is required.
- The full implementation is visible to both the engine and `GameScripts.dll`.
- Most importantly, the runner can live entirely inside the component that owns the work, which keeps captured callbacks in the same module that created them.

**Why there is no global scheduler in the engine DLL:**

`LatentSequence` stores callbacks as `std::function`. For script components, those callbacks usually capture `this` and other script-owned state. If a global engine-side scheduler owned those `std::function` objects, allocation/destruction and captured callable types would cross the `GameScripts.dll` boundary. That is exactly the kind of ownership pattern the engine avoids for ABI safety.

The intended pattern is:

1. A component owns a `LatentActionRunner` as a normal member.
2. The component builds sequences locally.
3. The component ticks its own runner from `OnUpdate`.
4. When the runner becomes idle, the component disables its own update tick.

This keeps lifetime, allocation, callback destruction, and captured state in one module.

**Execution model:**

- `Play(sequence)` returns a `LatentActionHandle`. Invalid handle `0` means the sequence was empty.
- `Tick(deltaTime)` clamps negative delta to `0`.
- The runner advances each active action step by step.
- If a step finishes early and there is leftover time in the current frame, that remaining time is immediately applied to the next step.
- Multiple zero-duration steps can therefore complete in one frame.
- New actions started while the runner is already ticking are stored in `pendingAdds` and flushed after the current iteration.
- `Stop(handle)` and `StopAll()` mark actions as stopped instead of erasing them immediately. Cleanup is deferred until it is safe.
- A safety limit (`kMaxStepAdvancesPerTick`) stops pathological infinite loops caused by sequences made only of instant-complete steps.

**Step semantics:**

- `Call(callback)` executes immediately, then advances to the next step in the same tick.
- `Wait(seconds)` accumulates elapsed time and advances once the requested duration has been reached.
- `Tween(duration, onUpdate, onComplete)` computes normalized progress in the `[0, 1]` range, calls `onUpdate(progress)` each tick, then calls `onComplete()` exactly once when the duration finishes.
- `Wait(0)` and `Tween(0, ...)` complete immediately. `Tween(0, ...)` still emits `onUpdate(1.0f)` before `onComplete()`.

**Recommended ownership pattern for scripts:**

```cpp
class MyComponent : public RTBEngine::ECS::Component {
public:
    void OnUpdate(float deltaTime) override
    {
        latentRunner.Tick(deltaTime);
        if (!latentRunner.HasActiveActions()) {
            SetUpdateTickEnabled(false);
        }
    }

    void StartAnimation()
    {
        activeHandle = latentRunner.Play(
            RTBEngine::Scripting::LatentSequence()
                .Call([this]() { /* setup */ })
                .Tween(0.2f, [this](float t) { /* animate */ })
                .Call([this]() { /* finalize */ }));

        SetUpdateTickEnabled(activeHandle.IsValid());
    }

private:
    RTBEngine::Scripting::LatentActionRunner latentRunner;
    RTBEngine::Scripting::LatentActionHandle activeHandle;
};
```

This is the professional event-driven pattern used by `ButtonStyle`: pointer events start or replace a transition, `OnUpdate` exists only to tick the active transition, and idle buttons do not consume steady-state update work.

**Concrete flow used by `ButtonStyle`:**

1. `OnAwake()` initializes the cached visual state and disables the update tick.
2. `OnStart()` and `OnValidate()` refresh references, capture the base transform, force the normal visual state, apply it immediately, and keep the component asleep.
3. Pointer handlers such as `OnPointerEnter()` and `OnPointerDown()` call `StartTransition(nextState)`.
4. `StartTransition()` snapshots the current visual state, resolves the exact target for the new state, stops the previous latent action, and starts a new `Tween`.
5. `OnUpdate()` does not contain button logic anymore; it only calls `latentRunner.Tick(deltaTime)`.
6. When the runner becomes idle, `ButtonStyle` disables its own update tick again.
7. `OnDestroy()` stops any in-flight transition so no latent callback can outlive the component instance.

This is what removes the old "every frame, keep lerping toward a target forever" behavior. The button now does work only while a transition actually exists, and every transition finishes on an exact final value instead of asymptotically approaching it.

### 15.6 PrefabLoader

`Engine/Scripting/PrefabLoader.h` — Static utility for loading prefab files.

```cpp
class PrefabLoader {
public:
    PrefabLoader() = delete;
    static std::unique_ptr<ECS::Prefab> Load(const std::string& filePath);
};
```

Reads a `.lua` prefab file and constructs a `Prefab` with all component snapshots and child prefabs.

### 15.7 PrefabSaver

`Engine/Scripting/PrefabSaver.h` — Static utility for saving prefab files.

```cpp
class PrefabSaver {
public:
    PrefabSaver() = delete;
    static bool Save(const ECS::Prefab& prefab, const std::string& filePath);
};
```

Serializes a `Prefab` to a `.lua` file in the same format used by `SceneLoader` for individual `GameObject` entries.

### 15.8 Scene Serialization Infrastructure

The scene loading and saving pipeline relies on several specialized utility classes:

**SceneComponentConfigurator** (`Engine/Scripting/SceneComponentConfigurator.h`) — Contains per-component-type configuration functions called during scene loading. Handles built-in components that require special setup beyond reflection (e.g., `ConfigureMeshRenderer` loads mesh and texture assets, `ConfigureAnimator` loads the FBX model and clips).

**SceneParsingUtils** (`Engine/Scripting/SceneParsingUtils.h`) — Lua table reading helpers: `ReadOptionalString`, `ReadOptionalInt`, `ReadOptionalFloat`, `ReadOptionalBool`, `ReadOptionalVector2/3/4`, `ReadOptionalQuaternion`. Also provides `ValidateSceneTable` for structural validation.

**ScenePropertySerializer** (`Engine/Scripting/ScenePropertySerializer.h`) — Functions for writing component properties to Lua scene files: `WriteComponent`, `WriteProperty`. Includes format helpers for all math types and utility functions for path normalization.

**SceneReflectionUtils** (`Engine/Scripting/SceneReflectionUtils.h`) — Two overloads:
- `ApplyLuaTableToComponent(L, tableIndex, componentTypeName, component)` — preferred; uses the registered type name to look up TypeInfo, ensuring script components from `GameScripts.dll` use their registered metadata rather than the virtual method result.
- `ApplyLuaTableToComponent(L, tableIndex, component)` — convenience wrapper; forwards to the above using `component->GetTypeName()`.

**SceneLuaBindings** (`Engine/Scripting/SceneLuaBindings.h`) — `SetupLuaBindings(L)` — registers C++ engine types with the Lua state so scene files can reference engine constants and functions.

---

`PropertyType::AssetRef` round-trips through the scene and prefab pipeline as a plain logical path string. Files therefore keep values such as `Assets/Models/AnimationsPlayer/great sword walk.fbx` exactly as authored, while the editor can still treat the field as a typed FBX slot through reflection metadata.

## 16. UI Subsystem

The UI subsystem provides a component-based 2D UI framework that runs on top of the 3D scene. It is separate from ImGui (which is only for the editor). The runtime UI uses OpenGL quads rendered in screen space.

### 16.1 RectTransform

`Engine/UI/RectTransform.h` — 2D layout component. Owns all transform data for a UI element and is the single source of truth — `UIElement` delegates to it directly.

**Public serializable fields** (use `Set*()` methods at runtime to also trigger `SetDirty()`):

```cpp
Math::Vector2 anchorMin        = {0.0f, 0.0f};  // (0,0) = bottom-left, (1,1) = top-right
Math::Vector2 anchorMax        = {0.0f, 0.0f};
Math::Vector2 pivot            = {0.5f, 0.5f};  // (0.5, 0.5) = center
Math::Vector2 anchoredPosition = {0.0f, 0.0f};
Math::Vector2 sizeDelta        = {100.0f, 100.0f};
Math::Vector2 scale            = {1.0f, 1.0f};
float         rotation         = 0.0f;
```

**Dirty flag** — marks for world transform recalculation:

```cpp
void SetDirty();
bool IsDirty() const;
void ClearDirty();
```

**World transform** (calculated by `Canvas::UpdateRectTransforms` only when dirty):

```cpp
void CalculateWorldTransform(const Math::Vector2& parentWorldPos,
                              const Math::Vector2& parentWorldSize,
                              const Math::Vector2& parentLossyScale);

Math::Vector2 GetWorldPosition() const;
Math::Vector2 GetWorldSize()     const;
Math::Vector4 GetWorldRect()     const;   // (x, y, width, height) in screen pixels
Math::Vector2 GetLossyScale()    const;
```

### 16.2 UIElement

`Engine/UI/UIElement.h` — Base class for all UI components. Extends `Component`. Owns a `RectTransform` as an inline member.

```cpp
virtual void Render() = 0;

bool IsVisible()       const;
void SetVisible(bool v);

bool IsRaycastTarget() const;
void SetRaycastTarget(bool t);

RectTransform*       GetRectTransform();
const RectTransform* GetRectTransform() const;
```

**Transform accessors** — delegate to the inline `RectTransform`:

```cpp
Math::Vector2 GetAnchorMin() / SetAnchorMin(const Math::Vector2&)
Math::Vector2 GetAnchorMax() / SetAnchorMax(const Math::Vector2&)
Math::Vector2 GetPivot()     / SetPivot(const Math::Vector2&)
Math::Vector2 GetAnchoredPosition() / SetAnchoredPosition(const Math::Vector2&)
Math::Vector2 GetSizeDelta() / SetSizeDelta(const Math::Vector2&)
float         GetRotation()  / SetRotation(float degrees)
Math::Vector2 GetScale()     / SetScale(const Math::Vector2&)
```

Each setter delegates to `rectTransform.Set*()` then calls `PropagateDirtyToChildren()`.

**Public reflected fields:**

```cpp
bool isVisible     = true;
bool raycastTarget = true;
```

Transform fields are serialized through the inline `rectTransform` member using `RTB_PROPERTY_NESTED_HIDDEN` in each subclass registration — there are no proxy transform fields on `UIElement` itself.

### 16.3 Canvas

`Engine/UI/Canvas.h` — Root container. Extends `Component`. Must be on the root `GameObject` of a UI hierarchy.

**Render modes:**

```cpp
enum class RenderMode {
    ScreenSpaceOverlay,   // Drawn on top of 3D, ignores camera
    ScreenSpaceCamera,    // Follows a camera, can have depth
    WorldSpace            // Placed in 3D world space
};

void SetRenderMode(RenderMode mode);
RenderMode GetRenderMode() const;
```

**Canvas size:**

```cpp
void          SetCanvasSize(const Math::Vector2& size);
Math::Vector2 GetCanvasSize() const;
```

In `ScreenSpaceOverlay` mode, canvas size is automatically set to the window dimensions each frame.

**Sort order** (for multi-canvas layering):

```cpp
void SetSortOrder(int order);
int  GetSortOrder() const;
```

Higher sort order renders on top.

**Rendering:**

```cpp
void RenderCanvas(const Math::Vector2& screenSize);
void PrepareForHitTest(const Math::Vector2& screenSize);
```

`RenderCanvas` collects all `UIElement` children recursively, sorts them by depth, and calls `Render()` on each visible one.

**Element collection:**

```cpp
const std::vector<UIElement*>& GetUIElements() const;
```

Returns a cached list of all `UIElement` descendants. Rebuilt only when `GameObject::GetHierarchyVersion()` changes — zero cost on frames where the hierarchy is unchanged.

### 16.4 CanvasSystem

`Engine/UI/CanvasSystem.h` — Global system that manages all active canvases and dispatches pointer input events.

```cpp
static CanvasSystem& GetInstance();

void RegisterCanvas(Canvas* canvas);
void UnregisterCanvas(Canvas* canvas);

void Update(const Math::Vector2& screenSize);
void RenderAll(const Math::Vector2& screenSize);

// Mouse input (called by GameViewPanel in Play mode):
void OnMouseMove(float x, float y);
void OnMouseDown(int button, float x, float y);
void OnMouseUp(int button, float x, float y);
```

`CanvasSystem` processes input events through `PrepareForHitTest()` and walks the element tree in reverse render order to find the front-most `raycastTarget` element under the cursor.

### 16.5 UIButton

`Engine/UI/Elements/UIButton.h` — A clickable rectangular region with optional text label.

```cpp
void SetOnClick(std::function<void()> callback);
void SetLabel(const std::string& label);
void SetNormalColor(const Math::Vector4& color);
void SetHoveredColor(const Math::Vector4& color);
void SetPressedColor(const Math::Vector4& color);
```

`Render()` draws a colored quad (color changes based on hover/press state) and overlays the label text using the default font.

### 16.6 UIText

`Engine/UI/Elements/UIText.h` — Renders a text string.

```cpp
void SetText(const std::string& text);
void SetFontSize(int size);
void SetColor(const Math::Vector4& color);
void SetAlignment(TextAlignment alignment);   // Left, Center, Right

enum class TextAlignment { Left, Center, Right };
```

Uses `Font` (FreeType/stb_truetype) to rasterize glyphs into a texture atlas and renders quads for each character.

**Reflected properties (Proxy):**

```cpp
std::string textRef;
int         fontSizeRef = 14;
Math::Vector4 colorRef  = {1,1,1,1};
int         alignmentRef = 0;
```

### 16.7 UIImage

`Engine/UI/Elements/UIImage.h` — Renders a textured quad.

```cpp
void SetTexture(Rendering::Texture* texture);
void SetColor(const Math::Vector4& tint);   // Multiplied with texture color
```

**Reflected properties (Proxy):**

```cpp
std::string   textureRef;
Math::Vector4 colorRef = {1,1,1,1};
```

### 16.8 UIPanel

`Engine/UI/Elements/UIPanel.h` — A solid-colored or textured background rectangle. Used as a container backdrop.

```cpp
void SetColor(const Math::Vector4& color);
void SetTexture(Rendering::Texture* texture);   // Optional background texture
```

### 16.9 UIContainer

`Engine/UI/Elements/UIContainer.h` — A layout-only UI element. Extends `UIElement` with an empty `Render()` implementation. Used to group child elements without drawing anything itself.

### 16.10 UIRenderContext

`Engine/UI/UIRenderContext.h` — Static rendering context that controls where UI elements draw to. Enables rendering UI into off-screen framebuffers (e.g., the editor's Game View panel).

```cpp
struct UIRenderContext {
    static ImDrawList*   CurrentDrawList;   // Target draw list (nullptr = BackgroundDrawList)
    static Math::Vector2 Offset;            // Position offset for all UI drawing
    static bool          IsValid;           // Whether context is active

    static void Begin(ImDrawList* drawList, const Math::Vector2& offset);
    static void End();
    static ImDrawList* GetDrawList();   // Returns CurrentDrawList or fallback
};
```

Before rendering canvases to a framebuffer, call `Begin()` with the framebuffer's draw list and position offset. After rendering, call `End()`. All `UIElement::Render()` calls use `GetDrawList()` to obtain the correct target.

### 16.11 EventSystem

`Engine/UI/EventSystem/` — Interface-based pointer event system for UI elements. Components implement these interfaces to receive input events from `CanvasSystem`.

**PointerEventData** (`PointerEventData.h`):

```cpp
struct PointerEventData {
    Math::Vector2    position;       // Current pointer screen position
    Math::Vector2    delta;          // Movement since last event
    ECS::GameObject* pointerEnter;   // Object currently under pointer
    ECS::GameObject* pointerPress;   // Object that received the press event
    int              button = 0;     // Mouse button index
};
```

**Handler interfaces** (each extends `IEventSystemHandler`):

| Interface | Method | Fires when |
|-----------|--------|------------|
| `IPointerClickHandler` | `OnPointerClick(const PointerEventData&)` | Pointer pressed and released on same element |
| `IPointerDownHandler` | `OnPointerDown(const PointerEventData&)` | Pointer button pressed on element |
| `IPointerUpHandler` | `OnPointerUp(const PointerEventData&)` | Pointer button released on element |
| `IPointerEnterHandler` | `OnPointerEnter(const PointerEventData&)` | Pointer enters element bounds |
| `IPointerExitHandler` | `OnPointerExit(const PointerEventData&)` | Pointer leaves element bounds |

`CanvasSystem` performs hit-testing via `PrepareForHitTest()`, walking the element tree in reverse render order to find the front-most `raycastTarget` element under the pointer. It then dispatches the appropriate events by `dynamic_cast`-ing each component on the hit `GameObject` to the handler interfaces.

Pointer-event dispatch is independent from `SetUpdateTickEnabled()`. This is important for event-driven UI scripts: a component can keep its update tick disabled while idle and still receive `OnPointerEnter`, `OnPointerExit`, `OnPointerDown`, `OnPointerUp`, and `OnPointerClick`. That distinction is what makes low-overhead animated controls such as `ButtonStyle` possible.

---

## 17. DLL Boundary Safety

RTBEngine exports its API via `RTB_API` (`__declspec(dllexport/import)`). Script DLLs (`GameScripts.dll`) are compiled separately from the engine and may be rebuilt against copied SDK headers. Even when both modules use the DLL runtime (`/MD` or `/MDd`), C++ STL ownership across that boundary is fragile because Debug/Release, iterator-debug settings, toolset version, or stale SDK copies can make binary layout or allocator ownership differ. The rule is therefore: **`std::string` and any heap-allocated STL types must not cross the DLL boundary by value or ownership**.

The current script bridge follows that rule explicitly. `GameScripts.dll` publishes `RTBScriptTypeDesc` and `RTBPropertyDesc` records, and the engine rebuilds the runtime metadata from those POD descriptors on its side of the boundary.

### The Problem

```cpp
// In Connector.cpp (GameScripts.dll, compiled separately from the engine):
std::string name = targetRef->GetName();   // GetName() returns std::string from engine's heap
RTB_INFO("Name: " + name);                // operator+ allocates in script's heap
// Risk: ownership and STL layout cross the module boundary.
```

### The Solution

**Pattern 1**: Use `const char*` accessors for string data:

```cpp
// Engine side adds ABI-safe accessor:
const char* GetNameCStr() const { return name.c_str(); }

// Script side uses it:
const char* name = targetRef->GetNameCStr();
```

**Pattern 2**: Use `snprintf` into a stack buffer:

```cpp
char buf[256];
snprintf(buf, sizeof(buf), "Connected to: %s", targetRef->GetNameCStr());
RTB_INFO(buf);   // const char* overload — no heap allocation
```

**Pattern 3**: Logger `const char*` overloads (added explicitly for this purpose):

```cpp
void Logger::Info(const char* message);
void Logger::Warning(const char* message);
void Logger::Error(const char* message);
```

These overloads convert to `std::string` inside the engine's CRT — safe because the engine always owns that string's memory.

### What Is Safe Across the Boundary

| Passes safely | Does NOT pass safely |
|--------------|---------------------|
| `const char*` | `std::string` (by value or reference) |
| `int`, `float`, `bool`, all POD types | `std::vector<T>` |
| Raw pointers (`T*`) | `std::shared_ptr<T>` |
| `Math::Vector2/3/4` (all-float structs) | `std::function<>` with captures |
| `Math::Matrix4` | Any STL container |

Also unsafe across the boundary: `TypeInfo`, `PropertyInfo`, and any script-facing reflection object that owns STL state. Those stay on the engine side and are reconstructed from POD descriptors.

`LatentActionRunner` follows this rule by design: it is meant to be owned by the same module that creates the callbacks. Script components store their own runner, so captured lambdas are not transferred to an engine-global queue.

### C4251 Warning Suppression

The `RTB_API` annotation on classes with STL member variables triggers MSVC C4251 ("class needs to have DLL-interface"). This is suppressed per-class with:

```cpp
#pragma warning(push)
#pragma warning(disable: 4251)
class RTB_API MyClass {
    std::vector<int> items;   // Would trigger C4251
};
#pragma warning(pop)
```

All engine classes with STL members have this suppression applied.

---

## 18. Code Conventions

### Naming

| Element | Convention | Example |
|---------|-----------|---------|
| Namespace | PascalCase | `RTBEngine::Rendering` |
| Class | PascalCase | `MeshRenderer`, `PhysicsWorld` |
| Method | PascalCase | `GetComponent()`, `OnUpdate()` |
| Member variable | camelCase | `isActive`, `deltaTime`, `owner` |
| Parameter | camelCase | `deltaTime`, `newParent` |
| Macro | UPPER_SNAKE_CASE | `RTB_COMPONENT`, `RTB_PROPERTY_FLOAT` |

### Header Structure

```cpp
#pragma once                           // Always first, never #ifndef
#include "DirectDependency.h"          // Project headers first
#include <standard_header>             // STL and third-party after

namespace RTBEngine {
    namespace SubSystem {

        class OtherClass;              // Forward declarations before class

        class ClassName {
        public:
            ClassName();
            virtual ~ClassName();

            ClassName(const ClassName&) = delete;             // Always deleted
            ClassName& operator=(const ClassName&) = delete;  // Always deleted

            //Public interface grouped by purpose
            void Method();
            Type GetValue() const { return member; }
            void SetValue(Type v) { member = v; }

        private:
            Type member;
        };

    }   // No closing annotation
}       // No closing annotation
```

### Comments

- All comments in **English only**.
- Section headers: `//Section name` (no space after `//`).
- Explanatory comments: `// One space then text`.
- No block comments (`/* */`) except for complex math.
- No redundant comments — names must be self-documenting.

### Memory Management

```cpp
// Ownership — always unique_ptr:
std::unique_ptr<RigidBody> body = std::make_unique<RigidBody>();

// Non-owning reference — raw pointer:
RigidBody* bodyRef = body.get();   // Does not extend lifetime

// No raw new/delete except for third-party API requirements:
btRigidBody* btBody = new btRigidBody(info);   // Bullet requires raw new
```

### Virtual Methods

- Always `override` on overridden methods.
- Base class empty implementations defined in-header as `{}`.
- Base class destructors always `virtual`.

### Component Lifecycle Hook Order

The following order must always be preserved in component declarations:

```cpp
void OnAwake() override;
void OnStart() override;
void OnUpdate(float deltaTime) override;
void OnFixedUpdate(float fixedDeltaTime) override;
void OnDestroy() override;
```

### Reflected Proxy Convention

- Reflected proxy members always have the `Ref` suffix: `speedRef`, `targetRef`, `colorRef`.
- They are declared `public` (the Inspector and serializer access them through reflection metadata).
- They are grouped under the comment `// Reflected properties (Proxy)`.
- `RTB_COMPONENT(ClassName)` macro is placed last in the public section.
- Private non-reflected copies of the same data (if needed) use the plain name: `speed`, `target`.
