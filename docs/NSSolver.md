# NSSolver Navier-Stokes Solver Overview

This note presents the solver as a numerical approximation of incompressible Navier-Stokes dynamics in a closed rectangular domain, with optional thermal buoyancy under a Boussinesq model. The emphasis is on the PDE, the discrete operators, and the splitting error structure.

## 1) Governing equations

Let $$\Omega \subset \mathbb{R}^3$$ be a rectangular cavity, and $$t \ge 0$$.

For constant-density incompressible flow:

$$
\nabla \cdot \mathbf{u} = 0
$$

$$
\frac{\partial \mathbf{u}}{\partial t} + (\mathbf{u} \cdot \nabla)\mathbf{u} = -\frac{1}{\rho}\nabla p + \nu \nabla^2 \mathbf{u} + \mathbf{f}
$$

For a passive scalar $$s$$ (e.g., smoke concentration):

$$
\frac{\partial s}{\partial t} + (\mathbf{u} \cdot \nabla)s = \kappa_s \nabla^2 s
$$

For temperature $$T$$ under Boussinesq coupling:

$$
\frac{\partial T}{\partial t} + (\mathbf{u} \cdot \nabla)T = \kappa_T \nabla^2 T
$$

$$
\mathbf{f}_b = g\beta(T - T_{\mathrm{ref}})\,\hat{\mathbf{e}}_b
$$

where $$\hat{\mathbf{e}}_b$$ is the chosen buoyancy direction.

## 2) Nondimensional viewpoint

With characteristic length $$L$$, velocity $$U$$, temperature scale $$\Delta T$$:

$$
Re = \frac{UL}{\nu}, \qquad
Pr = \frac{\nu}{\kappa_T}, \qquad
Ra = \frac{g\beta \Delta T L^3}{\nu \kappa_T} = Gr\,Pr
$$

The solver accepts physical parameters $$(\nu,\kappa_T)$$ directly, and can also be configured from $$(Ra,Pr,\Delta T,L)$$ by algebraic inversion of the above relations.

## 3) Spatial discretization

- Domain uses an anisotropic Cartesian mesh with spacings $$(h_x,h_y,h_z)$$.
- Velocity is stored on a staggered MAC arrangement (face-centered components), while scalar fields ($$p,s,T$$) are cell-centered.
- Ghost layers enforce boundary conditions and allow centered stencils up to the wall.

The 3D anisotropic 7-point Laplacian at a cell center is:

$$
\nabla_h^2 \phi_{i,j,k} = \frac{\phi_{i+1,j,k}-2\phi_{i,j,k}+\phi_{i-1,j,k}}{h_x^2} + \frac{\phi_{i,j+1,k}-2\phi_{i,j,k}+\phi_{i,j-1,k}}{h_y^2} + \frac{\phi_{i,j,k+1}-2\phi_{i,j,k}+\phi_{i,j,k-1}}{h_z^2}
$$

Discrete divergence and gradient are the compatible MAC pair, so that the pressure projection is a discrete Helmholtz-Hodge decomposition.

## 4) Time advancement as operator splitting

One time step of size $$\Delta t$$ is written as sub-operators.

**(a) Viscous solve for velocity (implicit diffusion)**

$$
(I - \nu \Delta t \nabla_h^2)\,\mathbf{u}^{*} = \mathbf{u}^n
$$

**(b) Advection of velocity (semi-Lagrangian transport)** to obtain $\mathbf{u}^{**}$.

**\(c) Projection** to enforce incompressibility

$$
\nabla_h^2 p^{n+1} = \frac{\rho}{\Delta t}\nabla_h\cdot \mathbf{u}^{**}
$$

$$
\mathbf{u}^{n+1} = \mathbf{u}^{**} - \frac{\Delta t}{\rho}\nabla_h p^{n+1}
$$

**(d) Scalar/temperature transport** with diffusion and advection in analogous split form.

**(e) Buoyancy forcing** (if enabled), as an explicit increment to momentum.

This is first-order accurate in time under Lie splitting; stabilization choices prioritize robustness for moderate-to-large $$\Delta t$$ over strict high-order temporal accuracy.

## 5) Advection model

Primary transport uses backtraced characteristics:

$$
\mathbf{x}_d = \mathbf{x} - \Delta t\,\mathbf{u}(\mathbf{x},t^n), \qquad q^{n+1}(\mathbf{x}) \approx q^n(\mathbf{x}_d)
$$

with interpolation of $$q^n$$ at the departure point.

This semi-Lagrangian form is unconditionally stable in the CFL sense, but introduces numerical diffusion. A predictor-corrector variant (MacCormack type) can reduce truncation diffusion in smooth regions at the cost of possible local overshoot unless limited.

## 6) Linear elliptic solves

Diffusion and pressure Poisson subproblems both reduce to sparse SPD systems on the structured grid:

$$
a_x = \frac{\Delta t\,\gamma}{h_x^2},\quad a_y = \frac{\Delta t\,\gamma}{h_y^2},\quad a_z = \frac{\Delta t\,\gamma}{h_z^2},\quad c = 1 + 2(a_x+a_y+a_z)
$$

with $$\gamma=\nu$$ (or scalar diffusivity) for Helmholtz solves, and the corresponding Poisson coefficients for pressure.

Relaxation sweeps (Gauss-Seidel family) are used as stationary iterations. Convergence speed is grid- and mode-dependent; low-frequency error components decay slowly without multigrid acceleration.

## 7) Boundary conditions and physical interpretation

In the cavity configuration:

- Normal velocity at solid walls is zero (impermeability).
- Tangential treatment is consistent with a simple no-through-flow closure in ghost-cell form.
- Scalar fields use homogeneous normal-gradient style closure by default.

At corners/edges, ghost values are blended from adjacent faces to keep the discrete stencil well-defined and avoid undefined corner states.

## 8) Consistency, stability, and error profile

- **Projection:** after the Poisson solve and gradient subtraction, the divergence is reduced to solver tolerance (not identically zero in finite precision).
- **Advection:** dominant error is dissipative unless corrected; phase accuracy and small-scale retention 
