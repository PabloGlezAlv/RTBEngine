# Changelog — RTBEngine

**Current version:** `0.8.0` (tag `0.8.0`)

API documentation: [`README.md`](README.md)

**Compatibility:** use with **RTBEngineEditor 0.8.x**. After SDK or Script Bridge ABI changes: rebuild the engine → `BuildSDK.bat` → `GameScripts`.

---

## Protocols and ABI

| Protocol | Version | Location |
|----------|---------|----------|
| RTBN (gameplay UDP) | **4** | `RTBEngine/Engine/Online/OnlineMessageCodec.h` |
| Script Bridge ABI | **4** | `RTBEngine/Engine/Scripting/ScriptBridgeABI.h` |
| NavMesh sidecar (`.navmesh`) | **1** | `RTBEngine/Engine/Navigation/NavMeshFile.cpp` |

---

## [0.8.0] — 2026

### Added
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
