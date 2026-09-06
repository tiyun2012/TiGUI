# MyUI — Milestone 1 (Completed)

Milestone 1 establishes the low-level Windows + DirectX 12 foundation for the
UI framework.

## Scope

```text
Win32 Window
    ↓
DXGI Factory
    ↓
D3D12 Device
    ↓
Command Queue
    ↓
Command Allocator + Command List
    ↓
Swap Chain
    ↓
Back Buffers + RTVs
    ↓
Resource Barrier
    ↓
Clear Render Target
    ↓
Present
    ↓
Fence Synchronization
```

The application opens a native 1280x720 window and clears it to a dark color.

## Intentionally NOT included

These are future milestones:

- UI IDs and persistent widget state
- DrawList / DrawCommand
- layout
- widgets
- text rendering
- docking
- tabs
- panels
- viewport UI
- workspaces
- themes / appearance system

## Build workflow

Use `make.bat` as the normal project entry point.

### Debug

```powershell
.\make.bat
```

or

```powershell
.\make.bat debug
```

### Release

```powershell
.\make.bat release
```

### Clean

```powershell
.\make.bat clean
```

### Rebuild

```powershell
.\make.bat rebuild
```

or

```powershell
.\make.bat rebuild release
```

### Help

```powershell
.\make.bat help
```

## Direct CMake equivalent

The `make.bat` script wraps:

```powershell
cmake -S . -B build -G "Visual Studio 18 2026" -A x64
cmake --build build --config Debug
```

## D3D12 concepts in this milestone

### Device

`ID3D12Device` creates D3D12 resources and objects.

### Command queue

`ID3D12CommandQueue` submits recorded command lists to the GPU.

### Command allocator

`ID3D12CommandAllocator` owns memory used while recording a command list.

### Command list

`ID3D12GraphicsCommandList` records GPU operations.

### Swap chain

The swap chain owns the back buffers displayed in the Win32 window.

### RTV

A Render Target View descriptor tells D3D12 that a resource is being used as a
render target.

### Resource states

The back buffer changes state explicitly:

```text
PRESENT
   ↓
RENDER_TARGET
   ↓
Clear / future Draw commands
   ↓
PRESENT
```

### Fence

The fence synchronizes the CPU with completion of GPU work.

Milestone 1 waits for the GPU after each frame. It is intentionally simple, not
our final performance architecture.

## Status

**Milestone 1: COMPLETE**
