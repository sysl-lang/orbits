orbits
======

The solar system as a 3D Newtonian n-body problem, integrated in f64 and checked against JPL
Horizons.

![The orbits at J2000: Mercury to Mars on the left, Jupiter to Pluto on the right](orbits.png)

The Sun, the eight planets, the Moon and Pluto, from their Horizons state vectors at J2000
(2000-01-01 12:00 TDB), in barycentric ecliptic J2000 coordinates, AU and days. Mars through Pluto
are system barycentres, since their GM is the planet's and its moons' together.

```
git clone https://github.com/sysl-lang/quaternion ../quaternion
sysl run .                      # writes orbits.png
sysl run . -- --compare         # each body's distance from Horizons at +30 d, +1 y, +10 y
sysl run . -- --bench 1000000   # times the force loop
sysl test .
```

`quaternion` is not released yet, so it is a `path` dependency on a sibling checkout. cairo needs
`brew install cairo` (or `libcairo2-dev`).

What is in it
-------------

| file | what |
|---|---|
| `system.sysl` | the bodies, the pairwise force loop, energy, angular momentum, barycentre |
| `integrate.sysl` | RK4, leapfrog (velocity Verlet) and Yoshida's 4th-order symplectic composition, switchable |
| `kepler.sysl` | Kepler's equation (elliptic and hyperbolic) by Newton, and elements ↔ state vectors |
| `render.sysl` | a perspective camera oriented by a quaternion, drawing the tracks with cairo |
| `horizons.sysl` | the Horizons fixtures, with the exact query; the raw replies are in `fixtures/horizons/` |
| `bench/force.c` | the same benchmark in C |

The tests, and why their tolerances widen with time
---------------------------------------------------

`sysl test .` runs 23 tests against oracles that share no code with the program: Horizons for where
the bodies are, conservation laws, the closed-form two-body solution, Kepler's equation solved by
bisection, and an independent PNG decoder (sysl-lang/png) for the image cairo writes.

Against Horizons, integrating with Yoshida at 0.05 day:

| after | every body within | Jupiter to Pluto within |
|---|---|---|
| 30 days | 2e-7 AU | 1e-9 AU |
| 1 year | 2e-6 AU | 2e-8 AU |
| 10 years | 4e-5 AU | 2e-6 AU |

The integration error is far below these: Yoshida and RK4 agree with each other to 1e-9 AU at ten
years. What sets the floor is what the model leaves out, and its effect grows with time:

- **General relativity.** Mercury's perihelion advances 43″ a century more than Newton says. It is
  the largest error at every date: 6e-8 AU at 30 days, 1.2e-5 AU at ten years.
- **The asteroids.** DE441 integrates Ceres, Vesta, Pallas and hundreds more; they pull the inner
  planets by about 1e-6 AU over a decade (the Earth is 4e-6 AU off at ten years, and a test asserts
  it is off by that much and no more).
- **Point masses.** The Earth and Moon are not spheres to each other, and the Sun's oblateness and
  the tides are absent.

The outer planets feel almost none of this, which is why they get a tolerance a hundred times
tighter.

The rest:

- **Conservation over 1000 years** (one-day steps, the Moon left out): Yoshida's relative energy
  error stays below 1e-7 and leapfrog's below 1e-5; both conserve angular momentum to 1e-13. RK4's
  energy error is over 100× Yoshida's and grows with time, and it loses angular momentum.
- **Reversibility**: leapfrog 20,000 steps forward and 20,000 back returns within a few ulps per
  step; RK4 does not.
- **Momentum**: the barycentre moves in a straight line.
- **Kepler**: against bisection, elliptic and hyperbolic, near-parabolic (e = 0.999, e = 1.0001) and
  huge anomalies; NaN outside the domain (e = 1, e < 0, NaN), as `sysl.math` answers; circular,
  equatorial, polar and retrograde conventions; elements → state → elements round trips.
- **Two-body**: a circular, an eccentric (e = 0.5), a nearly parabolic (e = 0.95) and a hyperbolic
  orbit integrated against the closed form, and halving the step divides the error by 16, which is
  the check that Yoshida is fourth order.
- **The picture**: the camera's projection, chosen pixels (the Sun, the Earth and Jupiter where the
  camera puts them at J2000), and the PNG decoded by `png` agreeing with what cairo drew.

Speed
-----

The force loop against `bench/force.c` at `-O2`, one million Yoshida steps of the 11-body system
(three force evaluations a step, 55 pairs each), on an Apple M-series machine:

| | time |
|---|---|
| C, `cc -O2` | 0.27 s |
| C, `cc -O2 -ffp-contract=off` | 0.30 s |
| sysl, `optimization = "2"` | 0.36 s |

sysl is 1.2× the C program that does the same arithmetic. Against `-ffp-contract=off` the final
state is **bit-identical**: sysl does not fuse a multiply and an add into an FMA, so it rounds every
operation as written, and clang's default does fuse them.

License
-------

ISC.
