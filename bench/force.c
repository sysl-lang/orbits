/* The same work as `orbits --bench STEPS`, in C: Yoshida steps of one day over the 11-body system
 * from the J2000 epoch, with the force loop written the way system.sysl writes it.
 *
 *     cc -O2 -o force bench/force.c -lm && ./force 200000
 *
 * It prints the energy and Earth x the sysl program prints. The two agree to about twelve figures
 * rather than bit for bit: both fuse a multiply feeding an add, but clang leaves some of those apart
 * depending on how it vectorizes, and sysl fuses every one.
 */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

typedef struct { double x, y, z; } vec3;

enum { N = 11 };

static const double gm_km[N] = {132712440041.279419, 22031.868551, 324858.592000, 398600.435507,
    4902.800118, 42828.375816, 126712764.100000, 37940584.841800, 5794556.400000, 6836527.100580,
    975.500000};

static const double state[N][6] = {
    {-7.137179161607904e-03, -2.795997495567612e-03, 2.062985061910257e-04, 5.378460339618783e-06, -7.406916207973516e-06, -9.434293432137470e-08},
    {-1.372307845370601e-01, -4.500836156309240e-01, -2.439200845186075e-02, 2.137177412835578e-02, -6.455396660044872e-03, -2.487957757771483e-03},
    {-7.254394755069969e-01, -3.545030569537367e-02, 4.122048053303724e-02, 8.034959761149407e-04, -2.030262561844178e-02, -3.235458586621571e-04},
    {-1.842722784343177e-01, 9.644456892709630e-01, 2.022132246085051e-04, -1.720224660838933e-02, -3.166189060532839e-03, 1.064592514002072e-08},
    {-1.862215600840044e-01, 9.626075632308897e-01, 4.446711984905560e-04, -1.683057613107253e-02, -3.588367637063646e-03, -6.634893538849906e-06},
    {1.383578742584679e+00, -1.621231564790753e-02, -3.426136426966323e-02, 6.768779813732038e-04, 1.517984117494233e-02, 3.015574196696519e-04},
    {3.994039981964449e+00, 2.935780109470535e+00, -1.015789781275531e-01, -4.562935066413099e-03, 6.435799094013335e-03, 7.548517732342305e-05},
    {6.399271680371201e+00, 6.567193619416989e+00, -3.688701277386623e-01, -4.286973413503631e-03, 3.882908781806891e-03, 1.028535064646593e-04},
    {1.442471943522018e+01, -1.373711746626653e+01, -2.379353892930592e-01, 2.683484051759769e-03, 2.665288979504713e-03, -2.486640648937235e-05},
    {1.680490978889128e+01, -2.499455888678026e+01, 1.274291784264387e-01, 2.584652719387356e-03, 1.769493903808153e-03, -9.600395792023743e-05},
    {-9.882489409085188e+00, -2.796159262368592e+01, 5.850652214992730e+00, 3.034129306353885e-03, -1.545193083508789e-03, -7.122969936787857e-04},
};

static double gm[N];
static vec3 pos[N], vel[N], acc[N];

static void accelerations(void) {
    for (int i = 0; i < N; i++) acc[i] = (vec3){0, 0, 0};
    for (int i = 0; i < N; i++) {
        vec3 pi = pos[i], ai = acc[i];
        for (int j = i + 1; j < N; j++) {
            vec3 d = {pos[j].x - pi.x, pos[j].y - pi.y, pos[j].z - pi.z};
            double r2 = d.x * d.x + d.y * d.y + d.z * d.z;
            double inv = 1.0 / (r2 * sqrt(r2));
            double mj = gm[j] * inv, mi = gm[i] * inv;
            ai.x += d.x * mj; ai.y += d.y * mj; ai.z += d.z * mj;
            acc[j].x -= d.x * mi; acc[j].y -= d.y * mi; acc[j].z -= d.z * mi;
        }
        acc[i] = ai;
    }
}

static void leapfrog(double dt) {
    double half = 0.5 * dt;
    for (int i = 0; i < N; i++) {
        vel[i].x += acc[i].x * half; vel[i].y += acc[i].y * half; vel[i].z += acc[i].z * half;
        pos[i].x += vel[i].x * dt; pos[i].y += vel[i].y * dt; pos[i].z += vel[i].z * dt;
    }
    accelerations();
    for (int i = 0; i < N; i++) {
        vel[i].x += acc[i].x * half; vel[i].y += acc[i].y * half; vel[i].z += acc[i].z * half;
    }
}

int main(int argc, char **argv) {
    int steps = argc > 1 ? atoi(argv[1]) : 200000;
    const double au = 149597870.7, day = 86400.0;
    const double w1 = 1.0 / (2.0 - 1.2599210498948732), w0 = 1.0 - 2.0 * w1;

    for (int i = 0; i < N; i++) {
        gm[i] = gm_km[i] * (day * day) / (au * au * au);
        pos[i] = (vec3){state[i][0], state[i][1], state[i][2]};
        vel[i] = (vec3){state[i][3], state[i][4], state[i][5]};
    }
    accelerations();

    struct timespec t0, t1;
    clock_gettime(CLOCK_MONOTONIC, &t0);
    for (int s = 0; s < steps; s++) {
        leapfrog(w1);
        leapfrog(w0);
        leapfrog(w1);
    }
    clock_gettime(CLOCK_MONOTONIC, &t1);

    double ke = 0, pe = 0;
    for (int i = 0; i < N; i++) {
        ke += 0.5 * gm[i] * (vel[i].x * vel[i].x + vel[i].y * vel[i].y + vel[i].z * vel[i].z);
        for (int j = i + 1; j < N; j++) {
            double dx = pos[j].x - pos[i].x, dy = pos[j].y - pos[i].y, dz = pos[j].z - pos[i].z;
            pe -= gm[i] * gm[j] / sqrt(dx * dx + dy * dy + dz * dz);
        }
    }
    long us = (t1.tv_sec - t0.tv_sec) * 1000000L + (t1.tv_nsec - t0.tv_nsec) / 1000;
    printf("%d steps in %ld us; energy %.17g; earth x %.16g\n", steps, us, ke + pe, pos[3].x);
    return 0;
}
