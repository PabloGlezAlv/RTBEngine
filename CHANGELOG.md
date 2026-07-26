# Changelog — RTBEngine

**Current version:** `0.11.0`

API documentation: [`README.md`](README.md)

**Compatibility:** use with **RTBEngineEditor 0.11.x**. After SDK or Script Bridge ABI changes: rebuild the engine → `BuildSDK.bat` → `GameScripts`.

---

## Protocols and ABI

| Protocol | Version | Location |
|----------|---------|----------|
| RTBN (gameplay UDP) | **4** | `RTBEngine/Engine/Online/OnlineMessageCodec.h` |
| Script Bridge ABI | **4** | `RTBEngine/Engine/Scripting/ScriptBridgeABI.h` |
| NavMesh sidecar (`.navmesh`) | **1** | `RTBEngine/Engine/Navigation/NavMeshFile.cpp` |

---

## [0.11.0] — 2026

### Added
- **`VolumeComponent`**: localized post-process volumes with per-effect overrides (`overrideDistanceFog`, `overrideVolumetricFog`) for distance fog and volumetric fog / god rays.
- **`VolumeStack`** + **`VolumeProfile`**: priority-sorted volume blending into per-frame `FogFrameState` (global project defaults + box volumes with blend distance and weight).
- **`VolumetricFogPass`**: fullscreen ray-marched volumetric fog pass driven by `VolumeStack` state (OpenGL and Vulkan).

### Changed
- Volume override model simplified to **one toggle per effect** (not per parameter); enabling an effect in a volume overrides the whole distance-fog or volumetric-fog block when blended.
- **OpenGL DDGI** software ray trace budget tuned for editor playability: 32 probes/frame, 24 rays/probe, 4096 triangles max; triangle upload cached by scene signature (same idea as Vulkan AS cache).

### Fixed
- **OpenGL volumetric fog**: removed redundant dual enable gate in C++ and shader; scene depth uses `Depth32F` + `ClampToEdge`; scene color/depth textures unbound after the pass to avoid feedback loops.
- **OpenGL DDGI**: software probe updates no longer stall typical scenes at ~6 FPS (was dominated by per-frame full-scene triangle uploads and excessive probe ray counts).

## [0.10.0] — 2026

### Added
- **Vulkan RHI backend** (`VulkanRenderDevice`): SDL window, deferred draw recording, swapchain present, ImGui overlay support.
- **Graphics abstraction layer** (`IRenderDevice`, `RenderDevice`, `RenderDeviceFactory`) shared by OpenGL and Vulkan backends.
- **DDGI** (Dynamic Diffuse Global Illumination): `DDGISystem`, `DDGIVolume`, probe irradiance atlas, `OpenGLDDGIUpdater` and `VulkanDDGIUpdater` with software ray-traced probe updates.
- **`LightingProjectSettings`**: per-project shadow map resolution and DDGI volume settings (persisted to project lighting file).
- **GitHub Actions** release workflow and CI scripts; ThirdParty link/runtime binaries tracked via Git LFS.

### Changed
- Lighting UBO and light types route GPU uploads through `IRenderDevice`.
- `LightComponent` syncs transform and reflected properties with the active light instance.
- Assimp library name selected via `Directory.Build.props` (`RTBAssimpToolset` for vc143/vc145).

### Fixed
- Guard against invalid GPU resource IDs in RHI code paths.

## [0.9.0] — 2026

### Added
- **`MeshRenderer` GPU instancing** (generic): `SetInstances`, `SetInstanceColors`, `ClearInstances`, `RenderInstanced`. One renderer can draw N copies without N GameObjects.
- Shared **`MeshDrawSubmit`** backend for single and instanced opaque draws.
- Per-instance color support in `basic.vert` / `basic.frag` (`uUseInstanceColor`).
- Generic ECS components: **`LocalTransform`**, **`VisualLink`** (scene proxy link).
- Generic **`EcsSimulationStats`** (alive entity count + simulation phase ms) via `Application::GetEcsSimulationStats()`.
- **`ScriptManager::InitializeGameEcs`**: loads game-owned ECS systems from GameScripts (`RTBScripts_InitializeEcs`).

### Changed
- Hybrid ECS frame order stays: Scene `Update` → ECS `Simulation` → Fixed/Physics → Scene `LateUpdate` → ECS `Presentation` → Render.
- Opaque scene pass skips renderers with explicit instance buffers, then draws them with `RenderInstanced()`.
- Engine ECS core is infrastructure-only: game simulation (projectiles, swarm benchmarks) registers from **GameScripts**, not from engine defaults.

### Removed
- Engine-owned **`ProjectileSimulation`** / **`ProjectileComponents`** (moved to GameScripts).
- Engine-owned swarm simulation / swarm stats (test-only; lives in editor GameScripts).
- World APIs **`GetProjectileStats`** / **`GetSwarmStats`** (replaced by generic `GetSimulationStats`).

## [0.8.0] — 2026

### Added
- **Hybrid ECS** (`RTBEngine::ECS` under `Engine/ECS/`): sparse-set `World`, `Entity`, `SystemScheduler`. Authoring stays in `RTBEngine::Scene` (former legacy `ECS` namespace renamed). First vertical slice: projectile flight via pooled GameObject proxy + `ProjectileComponent` bridge.
- Global engine uniforms (`uTime`, etc.) and shader support.
- **ShaderAsset**: custom shaders as `.rtbshader` assets with reflected properties.
- Shadow pass with **GPU instancing** for identical static meshes.
- Generic **ObjectPool** for projectiles and VFX.
- Combat: `OverlapSphere`, `OverlapCapsuleSegment`, physics-based unified melee.
- **CountdownTimer** and global **Scheduler** (`Invoke`, `StartSequence`).
- **Animator**: key-based clips (`PlayKey`, `HasKey`, finish callbacks).
- **DataAsset** for serializable game data.
- **ComponentQuery** and gameplay APIs (features 01–08).

### Changed
- Bone/animator UBO; cached `Animator` in `MeshRenderer`; reusable render scratch buffers.
- Particles: lifecycle, trail alpha fade, optimized active list.
- Occludable objects, world-space Canvas (`faceCameraLockY`), list serialization on components.
- Audio: `PlayOneShot` on `AudioSource`.

## [0.7.2]

- Shadow pass and geometry pass with **GPU instancing**.
- Bone UBO for shader skinning.

## [0.7.1]

- Cached `IsActiveInHierarchy` with dirty propagation.
- O(1) `GetComponent` with stable cross-DLL TypeId.
- Cached component lists per scene (render and physics).
- Camera and lighting UBO (single upload per pass).
- O(1) free-list for pool slots.
- Transform dirty flag; UI updated once per frame.

### Fixed
- `BoxCollider` center offset; VAO/VBO/EBO release in `Mesh`.

## [0.7.0]

- **Navigation** subsystem: `NavGridComponent`, `NavAgentComponent`, A* pathfinding, `.navmesh` persistence.
- Basic **ParticleSystem**.
- Unified lifecycle: Awake → Validate → Start.
- Persistent bones on animated characters.

## [0.6.0]

- Player removal from online sessions.
- Online backend selection (LAN vs Relay).
- **RelayNetworkTransport** and HTTP matchmaking API.
- Online player profile; world-space Canvas and layout groups.

## [0.5.0]

- **Collision layers** (layer names and collision matrix).
- LAN online; identity and lobby.
- World-space text/canvas, joystick UI, time system.
- Trail renderer.

## [0.4.1]

- Invalid collider/rigidbody validation.
- Safe GameObject destruction and physics shutdown fixes.

## [0.4.0]

- Physics in editor scene (Play mode).
- Raycast API, capsule collider, collider offset.
- Engine event system, UISlider.
- Script Bridge ABI-safe (no SDL in public headers).

## [0.3.1]

- Explicit `Animator` references and draggable FBX slots in Inspector.
- Animation library compatibility and cleanup.

## [0.3.0]

- Latent Actions; safe property offsets with multiple inheritance.
- Frustum culling; RectTransform and UI in editor.

## [0.2.0]

- **Prefab** system (save/load, scene overrides).
- Multi-mesh and material support.
- Sphere collider; multi-clip Animator.
- Engine compiled as **DLL** with `RTB_API`.

## [0.1.0]

- Framebuffer for editor scene rendering.
- Component reflection, UUIDs for GameObjects.
- Lua scene saver/loader, cubemap, basic UI.
- Standalone build and editor integration.

---

## Third-party dependencies

| Library | Version |
|---------|---------|
| SDL2 | 2.32.10 |
| GLEW | 2.1.0 |
| Bullet Physics | 3.25 |
| Assimp | 5.4.3 |
| ImGui | 1.92.5 |
| Lua | 5.4.8 |
| FMOD | — (external install) |

---

Format: [Keep a Changelog](https://keepachangelog.com/en/1.1.0/) · [Semantic Versioning](https://semver.org/)
