# ThermalPlume — Rocket Plume Lab

A real-time 3D thermal plume simulation demo for the Solstice engine. A stationary rocket engine on a test stand fires a momentum- and temperature-driven exhaust plume computed by the engine's Navier-Stokes solver (NSSolver) with Boussinesq buoyancy. Temperature-colored billboard particles overlay the fluid volume for dramatic visual effect.

## Thesis Roots

This demo draws inspiration from Bouchra Squalli Houssaini's 1998 doctoral thesis at the Universite d'Evry-Val d'Essonne:

> *Etude experimentale et numerique de la structure d'un panache thermique dans un canal vertical*
> ([theses.fr/1998EVRY0001](https://theses.fr/1998EVRY0001))

The thesis studied 2D thermal plumes developing in a vertical channel using both experimental measurements and numerical simulation of natural convection (Boussinesq incompressible Navier-Stokes). ThermalPlume extends this into a 3D rocket-engine context: the heated plate becomes a rocket nozzle, the vertical channel can be toggled as optional confinement walls, and the plume is visualized through raymarched temperature fields and exhaust particle effects.

## Building

ThermalPlume is built as part of the Solstice engine. From the repository root:

```bash
cmake --preset x64-release
cmake --build out/build/x64-release --target ThermalPlume
```

The binary is placed in `out/build/x64-release/bin/`.

## Controls

### Keyboard

| Key | Action |
|-----|--------|
| **Space** | Start / Pause simulation |
| **R** | Reset simulation to idle |
| **I** | Toggle engine ignition |
| **C** | Cycle camera mode (Free Fly / Side Ortho / Plume Chase) |
| **WASD** | Move camera (Free Fly mode, when mouse locked) |
| **Space** (held, Free Fly) | Move up |
| **Shift** (held, Free Fly) | Move down |
| **F** | Toggle fullscreen |
| **Esc** | Quit |

### Mouse

| Input | Action |
|-------|--------|
| **Right-click** | Toggle mouse lock (enables camera look) |
| **Move** (locked) | Look around (Free Fly mode) |

## ImGui Panel — Rocket Plume Lab

### Simulation
- **Start / Pause / Reset** buttons with state indicator (Idle / Running / Paused)

### Engine Controls
- **Throttle** (0–1): scales exhaust velocity and temperature injection
- **Chamber Temperature** (500–4000 K): maps to fluid temperature at nozzle exit
- **Exhaust Velocity** (50–500 m/s): downward velocity forcing at nozzle cells
- **Nozzle Diameter**: adjusts injection footprint on the fluid grid
- **Expansion Ratio**: visual/parametric (affects nozzle geometry scale)
- **Fuel Flow Rate**: heat release in the combustion zone
- **Ignite / Shutdown**: toggles nozzle injection (orthogonal to sim run state)

### Environment
- **Gravity**: Boussinesq buoyancy strength (0–20 m/s²)
- **Ambient Temperature**: reference for buoyancy coupling
- **Turbulence**: random perturbation on injection velocity

### Visualization
- **Draw Mode**: Raymarch (temperature) / Raymarch (Schlieren) / Isosurface / Velocity lines
- **Iso Level / Velocity Stride / Scale**: fine-tune overlays
- **Particles / Volume / Walls**: toggle visibility
- **Camera**: combo box alternative to the C key

### Stats
- Estimated thrust (fuel flow × exhaust velocity × throttle)
- Max plume temperature and height from fluid grid scan
- Approximate Reynolds number
- FPS, grid resolution, active particle count

## Architecture

```
Main.cxx                    Entry point — constructs ThermalPlumeGame, calls Run()
ThermalPlumeGame.hxx/cxx    GameBase subclass: scene, fluid, particles, camera, UI
ExhaustParticleSystem.hxx/cxx  ParticleSystem subclass with temperature-color ramp
```

The game uses a three-state machine (Idle → Running → Paused) for simulation control. The nozzle fires independently of the sim state via the Ignite/Shutdown toggle, allowing parameter tuning before starting.

## Technical Notes

- NSSolver with Boussinesq buoyancy and localized hot-wall boundary conditions
- Anisotropic grid (Nx×Ny×Nz) with configurable cell size
- Per-frame nozzle inlet forcing: velocity + temperature injection at grid top
- Exhaust particles coupled to fluid velocity via `FluidSimulation::SampleVelocity`
- Temperature-to-color ramp: white-hot → orange → red → smoke gray
- Billboard particle rendering on bgfx view 2 (same as scene pass)
- Three camera modes: Free Fly (perspective), Side Ortho (thesis view), Plume Chase (auto-orbit)
