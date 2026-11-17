/*
 * Adapted from UltraOS kernel/demo/fireworks.c (ctx-switching).
 * https://github.com/UltraOS/Ultra/blob/ctx-switching/kernel/demo/fireworks.c
 *
 * MIT License
 *
 * Copyright (c) 2022-2023 UltraOS
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <errno.h>
#include <pmos/ipc.h>
#include <pmos/memory.h>
#include <pmos/ports.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <time.h>
#include <unistd.h>

#define FP_ONE ((i64)1 << 16)

#define FRAMES_PER_SEC 60
#define MS_PER_SEC 1000
#define NS_PER_MS 1000000ULL
#define STATS_PERIOD_MS 2000
#define DRAIN_POLL_PERIOD_MS 100

// A dot is the screen height divided by this, but at least one pixel
#define DOTS_PER_SCREEN_HEIGHT 384

/*
 * Speeds are in percent of the screen width or height per second, and gravity
 * is in percent of the screen height per second squared.
 */
#define GRAVITY_PCT 5

#define ROCKETS_PER_VOLLEY_MIN 1
#define ROCKETS_PER_VOLLEY_MAX 20
#define VOLLEY_INTERVAL_MIN_MS 2000
#define VOLLEY_INTERVAL_MAX_MS 4000

#define ROCKET_SIDEWAYS_SPEED_MAX_PCT 40
#define ROCKET_RISE_SPEED_MIN_PCT 50
#define ROCKET_RISE_SPEED_MAX_PCT 100
#define ROCKET_LIFETIME_MIN_MS 500
#define ROCKET_LIFETIME_MAX_MS 1000

#define PARTICLES_PER_ROCKET_MIN 100
#define PARTICLES_PER_ROCKET_MAX 200
#define PARTICLE_SPEED_MIN_PCT 12
#define PARTICLE_SPEED_MAX_PCT 25
#define PARTICLE_LIFETIME_MIN_MS 2000
#define PARTICLE_LIFETIME_MAX_MS 3000

struct color {
    u8 r, g, b;
};

#define BACKGROUND_COLOR ((struct color){0, 0, 0})

struct screen {
    volatile u8 *pixels;
    u32 width, height, pitch;
    u32 dot_size;
    u8 r_shift, g_shift, b_shift;
};

/*
 * A moving dot on the screen. Its position is in pixels and its velocity in
 * pixels per second, both stored as 48.16 fixed point numbers.
 */
struct body {
    i64 x, y;
    i64 vx, vy;
    struct color color;
};

struct particle_seed {
    i64 x, y;
    i64 speed;
};

static struct screen s_screen;
static i64 s_gravity;
static pthread_attr_t s_worker_attr;
static pthread_mutex_t s_pixels_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t s_stats_mutex = PTHREAD_MUTEX_INITIALIZER;
static u64 s_rand_state;
static u64 s_worst_lateness_ns;
static atomic_uint s_num_alive;
static atomic_uint s_num_spawned;
static atomic_uint s_num_failed;
static atomic_bool s_stop_stats;

static u64 monotonic_ns()
{
    struct timespec now = {0};
    clock_gettime(CLOCK_MONOTONIC, &now);
    return (u64)now.tv_sec * MS_PER_SEC * NS_PER_MS + now.tv_nsec;
}

static void sleep_ms(u32 ms)
{
    struct timespec delay = {
        .tv_sec = ms / MS_PER_SEC,
        .tv_nsec = (ms % MS_PER_SEC) * NS_PER_MS,
    };
    while (nanosleep(&delay, &delay) == -1 && errno == EINTR) {
    }
}

// SplitMix64 by Steele, Lea and Flood (OOPSLA 2014)
static i64 rand_between(i64 first, i64 last)
{
    u64 z;
    pthread_mutex_lock(&s_stats_mutex);
    z = s_rand_state;
    s_rand_state += 0x9E3779B97F4A7C15ull;
    pthread_mutex_unlock(&s_stats_mutex);
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
    z ^= z >> 31;
    return first + (i64)(z % (last - first + 1));
}

// A random bright color, with every channel at least half of its maximum
static struct color rand_color(void)
{
    struct color color;

    color.r = rand_between(UINT8_MAX / 2, UINT8_MAX);
    color.g = rand_between(UINT8_MAX / 2, UINT8_MAX);
    color.b = rand_between(UINT8_MAX / 2, UINT8_MAX);

    return color;
}

// 'pct' percent of 'length' pixels, as a 48.16 fixed point number
static i64 percent_of(u32 length, u32 pct)
{
    return (i64)length * pct * FP_ONE / 100;
}

static u32 pixel_value(struct color color)
{
    struct screen *screen = &s_screen;
    u32 value;

    value = (u32)color.r << screen->r_shift;
    value |= (u32)color.g << screen->g_shift;
    value |= (u32)color.b << screen->b_shift;

    return value;
}

static volatile u32 *screen_row(u32 y)
{
    return (volatile u32 *)(s_screen.pixels + (size_t)y * s_screen.pitch);
}

static void plot(i64 x, i64 y, struct color color)
{
    struct screen *screen = &s_screen;
    u32 value, dx, dy;

    if (x < 0 || y < 0)
        return;
    if (x + screen->dot_size > screen->width)
        return;
    if (y + screen->dot_size > screen->height)
        return;

    value = pixel_value(color);
    pthread_mutex_lock(&s_pixels_mutex);

    for (dy = 0; dy < screen->dot_size; dy++) {
        volatile u32 *row = screen_row(y + dy);

        for (dx = 0; dx < screen->dot_size; dx++)
            row[x + dx] = value;
    }
    pthread_mutex_unlock(&s_pixels_mutex);
}

static void sleep_for_a_frame(u32 ms)
{
    u64 before = monotonic_ns();
    sleep_ms(ms);
    u64 slept_ns = monotonic_ns() - before;
    u64 wanted_ns = ms * NS_PER_MS;

    if (slept_ns > wanted_ns) {
        u64 lateness_ns = slept_ns - wanted_ns;
        pthread_mutex_lock(&s_stats_mutex);
        if (lateness_ns > s_worst_lateness_ns)
            s_worst_lateness_ns = lateness_ns;
        pthread_mutex_unlock(&s_stats_mutex);
    }
}

static void body_fly(struct body *body, u32 lifetime_ms)
{
    u32 frame_ms, elapsed_ms = 0, frame = 0;
    i64 x, y;

    while (elapsed_ms < lifetime_ms) {
        x = body->x / FP_ONE;
        y = body->y / FP_ONE;

        /*
         * Frame number 'frame' ends frame / FRAMES_PER_SEC seconds after the
         * first frame started, rounded down to a whole millisecond.
         */
        frame++;
        frame_ms = frame * MS_PER_SEC / FRAMES_PER_SEC - elapsed_ms;

        plot(x, y, body->color);
        sleep_for_a_frame(frame_ms);
        plot(x, y, BACKGROUND_COLOR);

        body->x += body->vx * frame_ms / MS_PER_SEC;
        body->y += body->vy * frame_ms / MS_PER_SEC;
        body->vy += s_gravity * frame_ms / MS_PER_SEC;

        elapsed_ms += frame_ms;
    }
}

static void *particle_thread(void *user)
{
    struct particle_seed *seed = user;
    struct body body;
    i64 dx, dy;

    /*
     * Pick a random point inside the unit circle. Its direction and its
     * distance from the center become the direction and the speed of the
     * particle.
     */
    do {
        dx = rand_between(-FP_ONE, FP_ONE);
        dy = rand_between(-FP_ONE, FP_ONE);
    } while (dx * dx + dy * dy > FP_ONE * FP_ONE);

    body.x = seed->x;
    body.y = seed->y;
    body.vx = dx * seed->speed / FP_ONE;
    body.vy = dy * seed->speed / FP_ONE;
    body.color = rand_color();

    free(seed);

    body_fly(
        &body,
        rand_between(PARTICLE_LIFETIME_MIN_MS, PARTICLE_LIFETIME_MAX_MS)
    );
    atomic_fetch_sub(&s_num_alive, 1);
    return nullptr;
}

static bool spawn(void *(*entry)(void *), void *arg)
{
    pthread_t thread;
    atomic_fetch_add(&s_num_alive, 1);
    if (pthread_create(&thread, &s_worker_attr, entry, arg) != 0) {
        atomic_fetch_sub(&s_num_alive, 1);
        atomic_fetch_add(&s_num_failed, 1);
        return false;
    }

    atomic_fetch_add(&s_num_spawned, 1);
    return true;
}

static void explode(struct body *rocket)
{
    struct particle_seed *seed;
    u32 i, num_particles;
    i64 speed;

    num_particles = rand_between(
        PARTICLES_PER_ROCKET_MIN, PARTICLES_PER_ROCKET_MAX
    );
    speed = rand_between(
        percent_of(s_screen.height, PARTICLE_SPEED_MIN_PCT),
        percent_of(s_screen.height, PARTICLE_SPEED_MAX_PCT)
    );

    for (i = 0; i < num_particles; i++) {
        seed = malloc(sizeof(*seed));
        if (seed == nullptr) {
            atomic_fetch_add(&s_num_failed, 1);
            continue;
        }

        seed->x = rocket->x;
        seed->y = rocket->y;
        seed->speed = speed;

        if (!spawn(particle_thread, seed))
            free(seed);
    }
}

static void *rocket_thread(void *)
{
    struct screen *screen = &s_screen;
    struct body body;
    i64 max_vx;

    max_vx = percent_of(screen->width, ROCKET_SIDEWAYS_SPEED_MAX_PCT);

    body.x = (i64)screen->width / 2 * FP_ONE;
    body.y = ((i64)screen->height - screen->dot_size) * FP_ONE;
    body.vx = rand_between(-max_vx, max_vx);
    body.vy = -rand_between(
        percent_of(screen->height, ROCKET_RISE_SPEED_MIN_PCT),
        percent_of(screen->height, ROCKET_RISE_SPEED_MAX_PCT)
    );
    body.color = rand_color();

    body_fly(
        &body, rand_between(ROCKET_LIFETIME_MIN_MS, ROCKET_LIFETIME_MAX_MS)
    );
    explode(&body);

    atomic_fetch_sub(&s_num_alive, 1);
    return nullptr;
}

static void *stats_thread(void *)
{
    while (!atomic_load(&s_stop_stats)) {
        sleep_ms(STATS_PERIOD_MS);
        if (atomic_load(&s_stop_stats))
            break;

        u64 worst_ns;
        pthread_mutex_lock(&s_stats_mutex);
        worst_ns = s_worst_lateness_ns;
        s_worst_lateness_ns = 0;
        pthread_mutex_unlock(&s_stats_mutex);
        printf("fireworks: %u threads, %u spawned, %u failed, worst wakeup %llu us late\n",
               atomic_load(&s_num_alive), atomic_load(&s_num_spawned), atomic_load(&s_num_failed),
               (unsigned long long)(worst_ns / 1000));
    }
    return nullptr;
}

static void launch_volleys(unsigned runtime_seconds)
{
    u64 end_ns = monotonic_ns() + (u64)runtime_seconds * MS_PER_SEC * NS_PER_MS;
    do {
        u32 num_rockets = rand_between(ROCKETS_PER_VOLLEY_MIN, ROCKETS_PER_VOLLEY_MAX);
        for (u32 i = 0; i < num_rockets; ++i)
            spawn(rocket_thread, nullptr);

        sleep_ms(rand_between(VOLLEY_INTERVAL_MIN_MS, VOLLEY_INTERVAL_MAX_MS));
    } while (runtime_seconds == 0 || monotonic_ns() < end_ns);

    while (atomic_load(&s_num_alive) != 0)
        sleep_ms(DRAIN_POLL_PERIOD_MS);
}

static bool valid_channel(u8 bits, u8 shift)
{
    return bits == 8 && shift <= 24;
}

static int request_framebuffer(IPC_Framebuffer_Reply *fb)
{
    static const char loader_name[] = "/pmos/loader";
    right_request_t loader = get_right_by_name(loader_name, sizeof(loader_name) - 1, 0);
    if (loader.result)
        return -loader.result;

    ports_request_t port = create_port(TASK_ID_SELF, 0);
    if (port.result) {
        delete_right(loader.right);
        return -port.result;
    }

    IPC_Framebuffer_Request request = {.type = IPC_Framebuffer_Request_NUM, .flags = 0};
    right_request_t sent = send_message_right(loader.right, port.port, &request,
                                               sizeof(request), NULL, 0);
    delete_right(loader.right);
    if (sent.result) {
        pmos_delete_port(port.port);
        return -sent.result;
    }

    Message_Descriptor desc;
    unsigned char *message;
    result_t result = get_message(&desc, &message, port.port, NULL, NULL);
    pmos_delete_port(port.port);
    if (result)
        return -result;

    bool valid = desc.size >= sizeof(*fb);
    if (valid)
        memcpy(fb, message, sizeof(*fb));
    free(message);
    return valid ? 0 : EPROTO;
}

static int map_screen(void **mapping)
{
    IPC_Framebuffer_Reply fb;
    int error = request_framebuffer(&fb);
    if (error)
        return error;
    if (fb.type != IPC_Framebuffer_Reply_NUM)
        return EPROTO;
    if (fb.result_code)
        return -fb.result_code;

    const size_t pixel_size = sizeof(u32);
    if (fb.framebuffer_bpp != 32 || fb.framebuffer_memory_model != 1 ||
        !valid_channel(fb.framebuffer_red_mask_size, fb.framebuffer_red_mask_shift) ||
        !valid_channel(fb.framebuffer_green_mask_size, fb.framebuffer_green_mask_shift) ||
        !valid_channel(fb.framebuffer_blue_mask_size, fb.framebuffer_blue_mask_shift))
        return ENOTSUP;

    if (!fb.framebuffer_width || !fb.framebuffer_height ||
        fb.framebuffer_pitch / pixel_size < fb.framebuffer_width ||
        fb.framebuffer_pitch % pixel_size || fb.framebuffer_addr % pixel_size)
        return EINVAL;

    const u64 page_size = getpagesize();
    const u64 start = fb.framebuffer_addr & ~(page_size - 1);
    const u64 offset = fb.framebuffer_addr - start;
    const u64 bytes = (u64)fb.framebuffer_pitch * fb.framebuffer_height;
    if (bytes > SIZE_MAX - offset - (page_size - 1))
        return EOVERFLOW;

    const size_t size = (offset + bytes + page_size - 1) & ~(page_size - 1);
    mem_request_ret_t map = create_phys_map_region(TASK_ID_SELF, nullptr, size,
                                      PROT_READ | PROT_WRITE | PHYS_REGION_TYPE_FRAMEBUFFER, start);
    if (map.result)
        return -map.result;

    *mapping = map.virt_addr;
    u32 dot_size = fb.framebuffer_height / DOTS_PER_SCREEN_HEIGHT;
    s_screen = (struct screen){
        .pixels = (volatile u8 *)map.virt_addr + offset,
        .width = fb.framebuffer_width,
        .height = fb.framebuffer_height,
        .pitch = fb.framebuffer_pitch,
        .dot_size = dot_size ? dot_size : 1,
        .r_shift = fb.framebuffer_red_mask_shift,
        .g_shift = fb.framebuffer_green_mask_shift,
        .b_shift = fb.framebuffer_blue_mask_shift,
    };
    return 0;
}

void test_fireworks(void)
{
    int error = pthread_attr_init(&s_worker_attr);
    if (error) {
        printf("fireworks: pthread_attr_init: %s\n", strerror(error));
        return;
    }
    error = pthread_attr_setdetachstate(&s_worker_attr, PTHREAD_CREATE_DETACHED);
    // pmOS defaults to 2 MiB per pthread. These workers only need a small stack.
    const size_t stack_size = 64 * 1024;
    if (!error)
        error = pthread_attr_setstacksize(&s_worker_attr,
                                          stack_size < PTHREAD_STACK_MIN ? PTHREAD_STACK_MIN : stack_size);
    if (error) {
        printf("fireworks: worker attributes: %s\n", strerror(error));
        pthread_attr_destroy(&s_worker_attr);
        return;
    }

    void *mapping;
    error = map_screen(&mapping);
    if (error) {
        printf("fireworks: framebuffer unavailable: %s\n", strerror(error));
        pthread_attr_destroy(&s_worker_attr);
        return;
    }

    printf("fireworks: %ux%u\n", s_screen.width, s_screen.height);
    for (u32 y = 0; y < s_screen.height; ++y) {
        volatile u32 *row = screen_row(y);
        for (u32 x = 0; x < s_screen.width; ++x)
            row[x] = pixel_value(BACKGROUND_COLOR);
    }

    s_rand_state = monotonic_ns();
    s_gravity = percent_of(s_screen.height, GRAVITY_PCT);
    s_worst_lateness_ns = 0;
    atomic_store(&s_num_spawned, 0);
    atomic_store(&s_num_failed, 0);
    atomic_store(&s_stop_stats, false);

    pthread_t stats;
    int stats_error = pthread_create(&stats, nullptr, stats_thread, nullptr);
    if (stats_error)
        printf("fireworks: stats thread: %s\n", strerror(stats_error));

    launch_volleys(0);
    atomic_store(&s_stop_stats, true);
    if (!stats_error)
        pthread_join(stats, nullptr);

    pthread_attr_destroy(&s_worker_attr);
    result_t result = release_region(TASK_ID_SELF, mapping);
    if (result)
        printf("fireworks: release framebuffer: %s\n", strerror((int)-result));

    printf("fireworks: done, %u spawned, %u failed\n", atomic_load(&s_num_spawned), atomic_load(&s_num_failed));
}
