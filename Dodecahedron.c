#include <SDL2/SDL.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#define WINDOW_WIDTH 800
#define WINDOW_HEIGHT 600
#define NUM_VERTICES 20
#define TOL 1e-6
#define MAX_PLANES 30

typedef struct {
    double x, y, z;
} Vec3;

typedef struct {
    Vec3 n;
    double d;
} Plane;

const double phi = (1.0 + sqrt(5.0)) / 2.0;
const double invphi = 1.0 / phi;
Vec3 baseVertices[NUM_VERTICES] = {
    {-1, -1, -1}, {-1, -1,  1}, {-1,  1, -1}, {-1,  1,  1},
    { 1, -1, -1}, { 1, -1,  1}, { 1,  1, -1}, { 1,  1,  1},
    { 0, -invphi, -phi}, { 0, -invphi,  phi},
    { 0,  invphi, -phi}, { 0,  invphi,  phi},
    {-phi, 0, -invphi}, {-phi, 0,  invphi},
    { phi, 0, -invphi}, { phi, 0,  invphi},
    {-invphi, -phi, 0}, {-invphi,  phi, 0},
    { invphi, -phi, 0}, { invphi,  phi, 0}
};

static double dot(Vec3 a, Vec3 b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

static Vec3 cross(Vec3 a, Vec3 b) {
    return (Vec3){
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x
    };
}

static double length(Vec3 v) {
    return sqrt(dot(v, v));
}

static Vec3 normalize(Vec3 v) {
    double len = length(v);
    if (len < TOL) return v;
    return (Vec3){ v.x / len, v.y / len, v.z / len };
}

static Vec3 add(Vec3 a, Vec3 b) {
    return (Vec3){ a.x + b.x, a.y + b.y, a.z + b.z };
}

static Vec3 subtract(Vec3 a, Vec3 b) {
    return (Vec3){ a.x - b.x, a.y - b.y, a.z - b.z };
}

static Vec3 scale(Vec3 v, double s) {
    return (Vec3){ v.x * s, v.y * s, v.z * s };
}

static Vec3 rotate(Vec3 v, double angle) {
    double cosA = cos(angle), sinA = sin(angle);
    double x = cosA * v.x + sinA * v.z;
    double z = -sinA * v.x + cosA * v.z;
    double y = v.y;
    double cosB = cos(angle * 0.5), sinB = sin(angle * 0.5);
    double y2 = cosB * y - sinB * z;
    double z2 = sinB * y + cosB * z;
    return (Vec3){ x, y2, z2 };
}


int computeBasePlanes(const Vec3 *vertices, int numVertices, Plane *planes, int maxPlanes) {
    int count = 0;
    for (int i = 0; i < numVertices; i++) {
        for (int j = i + 1; j < numVertices; j++) {
            for (int k = j + 1; k < numVertices; k++) {
                Vec3 v1 = subtract(vertices[j], vertices[i]);
                Vec3 v2 = subtract(vertices[k], vertices[i]);
                Vec3 n = cross(v1, v2);
                if (length(n) < TOL)
                    continue;
                n = normalize(n);
                double d = dot(n, vertices[i]);
                int allBelow = 1, allAbove = 1;
                for (int m = 0; m < numVertices; m++) {
                    double side = dot(n, vertices[m]) - d;
                    if (side > TOL) allBelow = 0;
                    if (side < -TOL) allAbove = 0;
                }
                if (!(allBelow || allAbove))
                    continue;
                if (allAbove) {
                    n = scale(n, -1);
                    d = -d;
                }
                // Skip duplicates
                int duplicate = 0;
                for (int p = 0; p < count; p++) {
                    if (fabs(dot(planes[p].n, n) - 1.0) < 1e-3 && fabs(planes[p].d - d) < 1e-3) {
                        duplicate = 1;
                        break;
                    }
                }
                if (!duplicate && count < maxPlanes) {
                    planes[count].n = n;
                    planes[count].d = d;
                    count++;
                }
            }
        }
    }
    return count;
}

int main(int argc, char* argv[]) {
    // Seed random generator (no longer used I like bloat)
    srand((unsigned int)SDL_GetTicks());

    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        fprintf(stderr, "SDL_Init Error: %s\n", SDL_GetError());
        return 1;
    }
    SDL_Window *window = SDL_CreateWindow("Dodecahedron",
                                          SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                          WINDOW_WIDTH, WINDOW_HEIGHT, SDL_WINDOW_SHOWN);
    if (!window) {
        fprintf(stderr, "SDL_CreateWindow Error: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }
    SDL_Renderer *renderer = SDL_CreateRenderer(window, -1,
                               SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!renderer) {
        fprintf(stderr, "SDL_CreateRenderer Error: %s\n", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }
    SDL_Texture *texture = SDL_CreateTexture(renderer,
                         SDL_PIXELFORMAT_ARGB8888,
                         SDL_TEXTUREACCESS_STREAMING,
                         WINDOW_WIDTH, WINDOW_HEIGHT);
    if (!texture) {
        fprintf(stderr, "SDL_CreateTexture Error: %s\n", SDL_GetError());
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    uint32_t *pixels = malloc(WINDOW_WIDTH * WINDOW_HEIGHT * sizeof(uint32_t));
    if (!pixels) {
        fprintf(stderr, "Failed to allocate pixel buffer\n");
        SDL_DestroyTexture(texture);
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    double modelScale = 0.5;
    Vec3 scaledVertices[NUM_VERTICES];
    for (int i = 0; i < NUM_VERTICES; i++) {
        scaledVertices[i] = scale(baseVertices[i], modelScale);
    }

    Plane basePlanes[MAX_PLANES];
    int numPlanes = computeBasePlanes(scaledVertices, NUM_VERTICES, basePlanes, MAX_PLANES);
    if (numPlanes != 12) {
        printf("Warning: Expected 12 planes, but got %d\n", numPlanes);
    }

    Vec3 camPos = { 0, 0, -5 };
    double scaleFactor = 300.0;  // Screen-space scaling (I'm Lazy)
    double halfWidth = WINDOW_WIDTH / 2.0;
    double halfHeight = WINDOW_HEIGHT / 2.0;

    Vec3 lightDir = normalize((Vec3){ 1, 1, -1 });

    Uint32 frameCount = 0;
    Uint32 lastDebugTime = SDL_GetTicks();

    int running = 1;
    SDL_Event event;
    while (running) {
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT)
                running = 0;
        }

        Uint32 currentTime = SDL_GetTicks();
        double angle = currentTime / 1000.0;

        // d should now remain unchanged
        Plane rotatedPlanes[MAX_PLANES];
        for (int i = 0; i < numPlanes; i++) {
            rotatedPlanes[i].n = rotate(basePlanes[i].n, angle);
            rotatedPlanes[i].d = basePlanes[i].d;
        }

        // for each pixel cast a ray and test intersection with the convex polyhedron
        for (int y = 0; y < WINDOW_HEIGHT; y++) {
            for (int x = 0; x < WINDOW_WIDTH; x++) {
                double u = (x - halfWidth) / scaleFactor;
                double v = (halfHeight - y) / scaleFactor;
                Vec3 rayDir = normalize((Vec3){ u, v, 5 });

                double tNear = -1e9;
                double tFar  =  1e9;
                int activePlaneIndex = -1;
                for (int i = 0; i < numPlanes; i++) {
                    double denom = dot(rotatedPlanes[i].n, rayDir);
                    if (fabs(denom) < TOL)
                        continue;
                    double t = (rotatedPlanes[i].d - dot(rotatedPlanes[i].n, camPos)) / denom;
                    if (denom < 0) {
                        if (t > tNear) {
                            tNear = t;
                            activePlaneIndex = i;
                        }
                    } else {
                        if (t < tFar)
                            tFar = t;
                    }
                }
                if (tNear > tFar || tFar < 0) {
                    pixels[y * WINDOW_WIDTH + x] = 0x00FF00; // bg R, G, B Currently: Green
                    continue;
                }
                double tHit = (tNear >= 0) ? tNear : tFar;
                Vec3 hitPoint = add(camPos, scale(rayDir, tHit));

                Vec3 surfNormal = (activePlaneIndex >= 0) ? rotatedPlanes[activePlaneIndex].n : (Vec3){0, 0, 1};
                double diff = dot(surfNormal, lightDir);
                if (diff < 0) diff = 0;
                int c = (int)(diff * 255);
                if (c > 255) c = 255;
                uint32_t color = 0x000000 | (c << 16) | (c << 8) | c; // light R, G, B flickers when changed idk why
                pixels[y * WINDOW_WIDTH + x] = color;
            }
        }

        SDL_UpdateTexture(texture, NULL, pixels, WINDOW_WIDTH * sizeof(uint32_t));
        SDL_RenderClear(renderer);
        SDL_RenderCopy(renderer, texture, NULL, NULL);
        SDL_RenderPresent(renderer);

        frameCount++;
        if (currentTime - lastDebugTime >= 1000) {
            double fps = frameCount * 1000.0 / (currentTime - lastDebugTime);
            printf("FPS: %.2f | Angle: %.2f rad | Frames: %u | Planes: %d\n", fps, angle, frameCount, numPlanes);
            lastDebugTime = currentTime;
            frameCount = 0;
        }
        SDL_Delay(1);
    }

    free(pixels);
    SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
