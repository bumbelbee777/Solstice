# Python simulation for the homogeneous RACS toy and comparison to minisuperspace analytic mapping.
# This will:
# 1. Compute analytic single-mode solution Z0(s, lambda, b).
# 2. Numerically compute the root-subtree amplitude by iterating the convolution recursion on a finite-depth homogeneous tree.
# 3. Compare numeric root Z(theta) to analytic Z0 * exp(i s theta) and plot results.
#
# Run-time parameters (feel free to change):
# - b: branching factor
# - lam: kernel parameter lambda
# - s_list: list of integer angular modes to test
# - depth_N: number of generations to iterate (tree depth)
# - M: number of discretization points on the circle for theta
#
# Notes:
# - We use FFT-based circular convolution to evaluate I(theta) = (1/2pi) ∫ K(θ-θ') Z(θ') dθ' 
#   which discretizes to conv = (1/M) * ifft( fft(K) * fft(Z) ).
# - The kernel is K(φ) = exp(i λ cos φ). 
# - Analytic solution (single-mode ansatz) Z(θ) = Z0 * exp(i s θ), where
#     Z0 = ( i^s * J_s(λ) )^(b/(1-b))
#   (principal branch chosen for complex power).

import numpy as np
from scipy.special import jv as Jv
import matplotlib.pyplot as plt

# Parameters (change as you like)
b = 3                 # branching factor
lam = 2.5             # lambda parameter in kernel K_lambda(phi) = exp(i lambda cos phi)
s_list = [0, 1, 2, 5] # angular modes to test
depth_N = 6           # number of generations (tree depth)
M = 512               # theta discretization points (power of two for FFT)

# Discretize theta
theta = 2*np.pi*np.arange(M)/M

# Precompute kernel array K(θ) = exp(i lambda cos θ) sampled on grid
K = np.exp(1j * lam * np.cos(theta))

# FFT of kernel (for convolution)
K_fft = np.fft.fft(K)

def analytic_Z0(lam, b, s):
    """Compute analytic Z0 = (i^s * J_s(lam))^(b/(1-b)).
       We pick the principal complex power branch via numpy** operator.
    """
    Js = Jv(s, lam)
    base = (1j**s) * Js
    power = b / (1.0 - b)
    # handle possible zero or very small Js
    if np.abs(base) < 1e-16:
        return 0.0 + 0.0j
    return base**power

def numeric_root_Z(lambda_param, b, s, depth_N, M, K_fft):
    """Compute numeric Z_root by iterating from leaves with initial single-mode Z_leaf = exp(i s theta)."""
    # initial leaf profile (single-mode)
    Z = np.exp(1j * s * theta)
    # iterate depth_N times upwards
    for _ in range(depth_N):
        # convolution via FFT: conv = (1/M) * ifft( fft(K) * fft(Z) )
        Z_fft = np.fft.fft(Z)
        conv = np.fft.ifft(K_fft * Z_fft)
        # raise to branching power
        Z = conv**b
    return Z

# Run tests and store results
results = []
for s in s_list:
    Z0 = analytic_Z0(lam, b, s)
    Z_num = numeric_root_Z(lam, b, s, depth_N, M, K_fft)
    # analytic function on theta: Z0 * exp(i s theta)
    Z_analytic_theta = Z0 * np.exp(1j * s * theta)
    # compute errors
    diff = Z_num - Z_analytic_theta
    l2_err = np.linalg.norm(diff) / np.linalg.norm(Z_analytic_theta)
    max_err = np.max(np.abs(diff))
    # phase slope check: unwrap phase and perform linear fit to extract slope
    phase_num = np.unwrap(np.angle(Z_num))
    coeffs = np.polyfit(theta, phase_num, 1)
    phase_slope = coeffs[0]  # should be close to s
    # log Z0 numeric estimate: take theta where exp(i s theta) known, compute
    # choose theta index 0 and divide: est_Z0 = Z_num[0] / exp(i s * 0) = Z_num[0]
    est_Z0 = Z_num[0]  # because exp(i s * 0) = 1
    results.append({
        's': s,
        'Z0_analytic': Z0,
        'Z0_numeric_est': est_Z0,
        'l2_rel_err': l2_err,
        'max_abs_err': max_err,
        'phase_slope_numeric': phase_slope
    })

# Print results in a readable table
print("Parameters: b = {}, lambda = {}, depth = {}, M = {}".format(b, lam, depth_N, M))
print()
for r in results:
    print("s = {:2d} | Z0_analytic = {:.4e}{:+.4e}j | Z0_numeric_est = {:.4e}{:+.4e}j | l2_rel_err = {:.3e} | max_err = {:.3e} | phase_slope = {:.6f}".format(
        r['s'],
        r['Z0_analytic'].real, r['Z0_analytic'].imag,
        r['Z0_numeric_est'].real, r['Z0_numeric_est'].imag,
        r['l2_rel_err'], r['max_abs_err'], r['phase_slope_numeric']
    ))

# Plot results for the first mode in s_list
s0 = s_list[0]
# find corresponding result and numeric arrays
Z_num_s0 = numeric_root_Z(lam, b, s0, depth_N, M, K_fft)
Z0_s0 = analytic_Z0(lam, b, s0)
Z_analytic_s0 = Z0_s0 * np.exp(1j * s0 * theta)

plt.figure(figsize=(10,4))
plt.subplot(1,2,1)
plt.plot(theta, np.abs(Z_num_s0), label='|Z_num|')
plt.plot(theta, np.abs(Z_analytic_s0), label='|Z_analytic|', linestyle='--')
plt.title(f"Mode s={s0}: Magnitude vs theta")
plt.xlabel("theta")
plt.ylabel("|Z|")
plt.legend()

plt.subplot(1,2,2)
plt.plot(theta, np.angle(Z_num_s0), label='arg(Z_num)')
plt.plot(theta, np.angle(Z_analytic_s0), label='arg(Z_analytic)', linestyle='--')
plt.title(f"Mode s={s0}: Phase vs theta")
plt.xlabel("theta")
plt.ylabel("phase")
plt.legend()

plt.tight_layout()
plt.show()

# Also show analytic Z0 (magnitude & phase) vs s for s up to 20
s_range = np.arange(0,21)
Z0_vals = np.array([analytic_Z0(lam, b, int(s)) for s in s_range])

plt.figure(figsize=(10,4))
plt.subplot(1,2,1)
plt.plot(s_range, np.abs(Z0_vals))
plt.title("Analytic |Z0| vs s")
plt.xlabel("s")
plt.ylabel("|Z0|")
plt.subplot(1,2,2)
plt.plot(s_range, np.angle(Z0_vals))
plt.title("Analytic arg(Z0) vs s")
plt.xlabel("s")
plt.ylabel("arg(Z0)")
plt.tight_layout()
plt.show()

# End of script. You can change parameters above and re-run to explore parameter space.

