#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_heap_caps.h"
#include "display_animations.h"
#include "device_config.h"

static const char *TAG = "DISPLAY_ANIM";

// Animation state
static animation_context_t anim_ctx = {0};
static SemaphoreHandle_t anim_mutex;

// Particle system
#define MAX_PARTICLES 50
static particle_t particles[MAX_PARTICLES];

// Starfield
#define MAX_STARS 100
static star_t stars[MAX_STARS];

// Frame buffer (simplified - actual implementation would use DMA)
static uint16_t *framebuffer = NULL;

// Color utilities
#define RGB565(r, g, b) (((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3))

// HSV to RGB565 conversion
static uint16_t hsv_to_rgb565(uint16_t h, uint8_t s, uint8_t v) {
    uint8_t r, g, b;
    uint8_t region, remainder, p, q, t;

    if (s == 0) {
        r = g = b = v;
    } else {
        region = h / 43;
        remainder = (h - (region * 43)) * 6;

        p = (v * (255 - s)) >> 8;
        q = (v * (255 - ((s * remainder) >> 8))) >> 8;
        t = (v * (255 - ((s * (255 - remainder)) >> 8))) >> 8;

        switch (region) {
            case 0: r = v; g = t; b = p; break;
            case 1: r = q; g = v; b = p; break;
            case 2: r = p; g = v; b = t; break;
            case 3: r = p; g = q; b = v; break;
            case 4: r = t; g = p; b = v; break;
            default: r = v; g = p; b = q; break;
        }
    }

    return RGB565(r, g, b);
}

// Initialize particle
static void init_particle(particle_t *p, float x, float y) {
    p->x = x;
    p->y = y;
    p->vx = ((float)rand() / RAND_MAX - 0.5f) * 4.0f;
    p->vy = ((float)rand() / RAND_MAX - 0.5f) * 4.0f - 2.0f;
    p->color = hsv_to_rgb565(rand() % 360, 255, 255);
    p->life = 100;
    p->active = true;
}

// Initialize stars
static void init_starfield(void) {
    for (int i = 0; i < MAX_STARS; i++) {
        stars[i].x = (float)(rand() % DISPLAY_WIDTH);
        stars[i].y = (float)(rand() % DISPLAY_HEIGHT);
        stars[i].z = (float)(rand() % 100 + 1);
        stars[i].speed = (float)(rand() % 3 + 1) * 0.5f;
    }
}

// Draw a pixel (bounds-checked)
static void draw_pixel(uint16_t x, uint16_t y, uint16_t color) {
    if (x < DISPLAY_WIDTH && y < DISPLAY_HEIGHT && framebuffer != NULL) {
        framebuffer[y * DISPLAY_WIDTH + x] = color;
    }
}

// Draw a filled circle
static void draw_circle_filled(int16_t x0, int16_t y0, int16_t r, uint16_t color) {
    for (int16_t y = -r; y <= r; y++) {
        for (int16_t x = -r; x <= r; x++) {
            if (x * x + y * y <= r * r) {
                draw_pixel(x0 + x, y0 + y, color);
            }
        }
    }
}

// Draw a line (kept for potential future use)
__attribute__((unused))
static void draw_line(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t color) {
    int16_t steep = abs(y1 - y0) > abs(x1 - x0);
    if (steep) {
        int16_t temp;
        temp = x0; x0 = y0; y0 = temp;
        temp = x1; x1 = y1; y1 = temp;
    }

    if (x0 > x1) {
        int16_t temp;
        temp = x0; x0 = x1; x1 = temp;
        temp = y0; y0 = y1; y1 = temp;
    }

    int16_t dx = x1 - x0;
    int16_t dy = abs(y1 - y0);
    int16_t err = dx / 2;
    int16_t ystep = (y0 < y1) ? 1 : -1;

    for (; x0 <= x1; x0++) {
        if (steep) {
            draw_pixel(y0, x0, color);
        } else {
            draw_pixel(x0, y0, color);
        }
        err -= dy;
        if (err < 0) {
            y0 += ystep;
            err += dx;
        }
    }
}

// Clear screen
static void clear_screen(uint16_t color) {
    if (framebuffer != NULL) {
        for (int i = 0; i < DISPLAY_WIDTH * DISPLAY_HEIGHT; i++) {
            framebuffer[i] = color;
        }
    }
}

// === IDLE ANIMATIONS ===

// Starfield animation
void anim_draw_starfield(uint32_t frame) {
    clear_screen(RGB565(0, 0, 0));

    for (int i = 0; i < MAX_STARS; i++) {
        // Move star closer
        stars[i].z -= stars[i].speed;
        if (stars[i].z <= 0) {
            stars[i].x = (float)(rand() % DISPLAY_WIDTH);
            stars[i].y = (float)(rand() % DISPLAY_HEIGHT);
            stars[i].z = 100.0f;
        }

        // Project to 2D
        float px = (stars[i].x - DISPLAY_WIDTH / 2) * (100.0f / stars[i].z) + DISPLAY_WIDTH / 2;
        float py = (stars[i].y - DISPLAY_HEIGHT / 2) * (100.0f / stars[i].z) + DISPLAY_HEIGHT / 2;

        // Draw star with brightness based on distance
        uint8_t brightness = (uint8_t)(255 - stars[i].z * 2.5f);
        uint16_t color = RGB565(brightness, brightness, brightness);

        if (px >= 0 && px < DISPLAY_WIDTH && py >= 0 && py < DISPLAY_HEIGHT) {
            draw_pixel((uint16_t)px, (uint16_t)py, color);
            // Draw bigger stars when closer
            if (stars[i].z < 30) {
                draw_pixel((uint16_t)px + 1, (uint16_t)py, color);
                draw_pixel((uint16_t)px, (uint16_t)py + 1, color);
            }
        }
    }
}

// Wave pattern animation
void anim_draw_wave_pattern(uint32_t frame, uint16_t color) {
    clear_screen(RGB565(0, 0, 20));

    for (int x = 0; x < DISPLAY_WIDTH; x += 3) {
        float y1 = DISPLAY_HEIGHT / 2 + sinf((x + frame * 2) * 0.02f) * 40;
        float y2 = DISPLAY_HEIGHT / 2 + sinf((x + frame * 2) * 0.02f + M_PI/3) * 30;
        float y3 = DISPLAY_HEIGHT / 2 + sinf((x + frame * 2) * 0.02f + 2*M_PI/3) * 20;

        draw_circle_filled(x, (int16_t)y1, 2, color);
        draw_circle_filled(x, (int16_t)y2, 2, RGB565(0, 100, 200));
        draw_circle_filled(x, (int16_t)y3, 2, RGB565(0, 50, 150));
    }
}

// Rainbow cycle animation
void anim_draw_rainbow_cycle(uint32_t frame) {
    for (int y = 0; y < DISPLAY_HEIGHT; y++) {
        uint16_t hue = (y * 360 / DISPLAY_HEIGHT + frame * 2) % 360;
        uint16_t color = hsv_to_rgb565(hue, 255, 200);

        for (int x = 0; x < DISPLAY_WIDTH; x++) {
            draw_pixel(x, y, color);
        }
    }
}

// === PLAYBACK ANIMATIONS ===

// Equalizer bars animation
void anim_draw_equalizer(uint32_t frame, float intensity) {
    clear_screen(RGB565(0, 0, 0));

    int bar_width = DISPLAY_WIDTH / 20;
    int bar_spacing = 2;

    for (int i = 0; i < 20; i++) {
        // Generate pseudo-random height based on frame and intensity
        float height_factor = sinf(frame * 0.1f + i * 0.5f) * 0.5f + 0.5f;
        int height = (int)(height_factor * intensity * DISPLAY_HEIGHT * 0.7f);

        uint16_t color;
        if (height > DISPLAY_HEIGHT * 0.6f) {
            color = RGB565(255, 0, 0);  // Red for high peaks
        } else if (height > DISPLAY_HEIGHT * 0.4f) {
            color = RGB565(255, 255, 0);  // Yellow for medium
        } else {
            color = RGB565(0, 255, 0);  // Green for low
        }

        int x = i * (bar_width + bar_spacing);
        for (int y = DISPLAY_HEIGHT - height; y < DISPLAY_HEIGHT; y++) {
            for (int w = 0; w < bar_width; w++) {
                draw_pixel(x + w, y, color);
            }
        }
    }
}

// Synchronized circles for quintet
void anim_draw_circles_sync(uint32_t frame, song_type_t type) {
    clear_screen(RGB565(10, 10, 30));

    uint16_t color;
    int num_circles;

    switch (type) {
        case SONG_TYPE_QUINTET:
            color = RGB565(0x33, 0xFF, 0x33);  // Green
            num_circles = 5;
            break;
        case SONG_TYPE_DUET:
            color = RGB565(0xFF, 0xFF, 0x33);  // Yellow
            num_circles = 2;
            break;
        default:
            color = RGB565(0xCC, 0x33, 0xCC);  // Purple
            num_circles = 1;
            break;
    }

    for (int i = 0; i < num_circles; i++) {
        float angle = (2.0f * M_PI * i / num_circles) + frame * 0.02f;
        int cx = DISPLAY_WIDTH / 2 + cosf(angle) * 80;
        int cy = DISPLAY_HEIGHT / 2 + sinf(angle) * 60;

        // Pulsing radius
        int radius = 20 + sinf(frame * 0.1f + i * M_PI / num_circles) * 10;

        // Draw expanding rings
        for (int r = 0; r < radius; r += 5) {
            uint8_t alpha = 255 - (r * 255 / radius);
            uint16_t ring_color = RGB565(
                (color >> 11) * alpha / 255,
                ((color >> 5) & 0x3F) * alpha / 255,
                (color & 0x1F) * alpha / 255
            );
            draw_circle_filled(cx, cy, radius - r, ring_color);
        }
    }
}

// Particle system animation
void anim_draw_particles(uint32_t frame, uint16_t color) {
    clear_screen(RGB565(0, 0, 0));

    // Spawn new particles periodically
    if (frame % 5 == 0) {
        for (int i = 0; i < MAX_PARTICLES; i++) {
            if (!particles[i].active) {
                init_particle(&particles[i], DISPLAY_WIDTH / 2, DISPLAY_HEIGHT / 2);
                break;
            }
        }
    }

    // Update and draw particles
    for (int i = 0; i < MAX_PARTICLES; i++) {
        if (particles[i].active) {
            // Update position
            particles[i].x += particles[i].vx;
            particles[i].y += particles[i].vy;
            particles[i].vy += 0.1f;  // Gravity
            particles[i].life--;

            // Draw particle
            if (particles[i].x >= 0 && particles[i].x < DISPLAY_WIDTH &&
                particles[i].y >= 0 && particles[i].y < DISPLAY_HEIGHT) {

                uint8_t fade = particles[i].life * 255 / 100;
                uint16_t faded_color = RGB565(
                    (particles[i].color >> 11) * fade / 255,
                    ((particles[i].color >> 5) & 0x3F) * fade / 255,
                    (particles[i].color & 0x1F) * fade / 255
                );

                draw_circle_filled((int16_t)particles[i].x, (int16_t)particles[i].y,
                                 particles[i].life / 25, faded_color);
            }

            // Deactivate dead particles
            if (particles[i].life <= 0 ||
                particles[i].x < 0 || particles[i].x >= DISPLAY_WIDTH ||
                particles[i].y >= DISPLAY_HEIGHT) {
                particles[i].active = false;
            }
        }
    }
}

// Spiral animation
void anim_draw_spiral(uint32_t frame, uint16_t color) {
    clear_screen(RGB565(0, 0, 0));

    float cx = DISPLAY_WIDTH / 2;
    float cy = DISPLAY_HEIGHT / 2;

    for (int i = 0; i < 200; i++) {
        float angle = i * 0.1f + frame * 0.05f;
        float radius = i * 0.8f;

        float x = cx + cosf(angle) * radius;
        float y = cy + sinf(angle) * radius;

        if (x >= 0 && x < DISPLAY_WIDTH && y >= 0 && y < DISPLAY_HEIGHT) {
            uint16_t hue = (i * 2 + frame) % 360;
            uint16_t spiral_color = hsv_to_rgb565(hue, 255, 255);
            draw_circle_filled((int16_t)x, (int16_t)y, 3, spiral_color);
        }
    }
}

// Fireworks animation
void anim_draw_fireworks(uint32_t frame) {
    clear_screen(RGB565(0, 0, 10));

    // Launch new firework periodically
    if (frame % 30 == 0) {
        int x = rand() % DISPLAY_WIDTH;
        int y = DISPLAY_HEIGHT - 20;

        for (int i = 0; i < 30; i++) {
            if (!particles[i].active) {
                particles[i].x = x;
                particles[i].y = y;
                particles[i].vx = ((float)rand() / RAND_MAX - 0.5f) * 8.0f;
                particles[i].vy = -((float)rand() / RAND_MAX * 5.0f + 5.0f);
                particles[i].color = hsv_to_rgb565(rand() % 360, 255, 255);
                particles[i].life = 50;
                particles[i].active = true;
            }
        }
    }

    // Update and draw particles
    for (int i = 0; i < MAX_PARTICLES; i++) {
        if (particles[i].active) {
            particles[i].x += particles[i].vx;
            particles[i].y += particles[i].vy;
            particles[i].vy += 0.2f;  // Gravity
            particles[i].life--;

            // Trail effect
            for (int t = 0; t < 3; t++) {
                float tx = particles[i].x - particles[i].vx * t * 0.3f;
                float ty = particles[i].y - particles[i].vy * t * 0.3f;

                if (tx >= 0 && tx < DISPLAY_WIDTH && ty >= 0 && ty < DISPLAY_HEIGHT) {
                    uint8_t fade = (particles[i].life * (3 - t)) / 3 * 255 / 50;
                    uint16_t trail_color = RGB565(
                        (particles[i].color >> 11) * fade / 255,
                        ((particles[i].color >> 5) & 0x3F) * fade / 255,
                        (particles[i].color & 0x1F) * fade / 255
                    );
                    draw_pixel((uint16_t)tx, (uint16_t)ty, trail_color);
                }
            }

            if (particles[i].life <= 0 || particles[i].y >= DISPLAY_HEIGHT) {
                particles[i].active = false;
            }
        }
    }
}

// Draw Tinkercademy logo (simplified version)
void display_draw_tinkercademy_logo(void) {
    clear_screen(RGB565(30, 30, 60));

    // Draw "TINKER" text (simplified)
    // const char *text = "TINKERCADEMY";
    // int text_width = strlen(text) * 12;
    // int x_start = (DISPLAY_WIDTH - text_width) / 2;
    // int y_pos = DISPLAY_HEIGHT / 2 - 20;
    // TODO: Implement text rendering when font system is added

    // Draw mascot representation (simplified circles and shapes)
    int mascot_x = DISPLAY_WIDTH / 2;
    int mascot_y = DISPLAY_HEIGHT / 2 + 40;

    // Body
    draw_circle_filled(mascot_x, mascot_y, 20, RGB565(255, 200, 0));
    // Head
    draw_circle_filled(mascot_x, mascot_y - 25, 15, RGB565(255, 220, 0));
    // Eyes
    draw_circle_filled(mascot_x - 5, mascot_y - 25, 3, RGB565(0, 0, 0));
    draw_circle_filled(mascot_x + 5, mascot_y - 25, 3, RGB565(0, 0, 0));

    // Draw animated glow effect
    uint32_t frame = xTaskGetTickCount() / 10;
    for (int i = 0; i < 3; i++) {
        int radius = 30 + i * 10 + (int)(sinf(frame * 0.05f) * 5);
        uint8_t brightness = 100 - i * 30;

        // Draw glow as expanding circles with decreasing brightness
        for (int r = radius - 2; r <= radius; r++) {
            if (r > 0) {
                // Simple glow effect using multiple circles
                uint16_t glow = RGB565(brightness/2, brightness/2, brightness);
                // Would implement circle outline drawing here
                // For now, just draw some points around the circle
                for (int angle = 0; angle < 360; angle += 10) {
                    float rad = angle * M_PI / 180.0f;
                    int gx = mascot_x + (int)(cosf(rad) * r);
                    int gy = mascot_y - 10 + (int)(sinf(rad) * r);
                    if (gx >= 0 && gx < DISPLAY_WIDTH && gy >= 0 && gy < DISPLAY_HEIGHT) {
                        draw_pixel(gx, gy, glow);
                    }
                }
            }
        }
    }
}

// Animation task
static void animation_task(void *pvParameters) {
    uint32_t frame = 0;
    uint32_t idle_animation = 0;

    while (1) {
        xSemaphoreTake(anim_mutex, portMAX_DELAY);

        if (anim_ctx.active) {
            frame++;

            // Select animation based on context
            if (anim_ctx.song_type != SONG_TYPE_SOLO) {
                // Playback animations
                switch (anim_ctx.song_type) {
                    case SONG_TYPE_QUINTET:
                        if (frame % 2 == 0) {
                            anim_draw_circles_sync(frame, SONG_TYPE_QUINTET);
                        } else {
                            // Convert 24-bit color to RGB565
                            uint16_t color565 = RGB565(0x33, 0xFF, 0x33);
                            anim_draw_particles(frame, color565);
                        }
                        break;

                    case SONG_TYPE_DUET:
                        anim_draw_circles_sync(frame, SONG_TYPE_DUET);
                        break;

                    default:  // SOLO
                        switch (frame / 100 % 4) {
                            case 0:
                                anim_draw_equalizer(frame, anim_ctx.beat_intensity);
                                break;
                            case 1: {
                                uint16_t color565 = RGB565(0xCC, 0x33, 0xCC);
                                anim_draw_spiral(frame, color565);
                                break;
                            }
                            case 2: {
                                uint16_t color565 = RGB565(0xCC, 0x33, 0xCC);
                                anim_draw_particles(frame, color565);
                                break;
                            }
                            case 3:
                                anim_draw_fireworks(frame);
                                break;
                        }
                        break;
                }
            } else {
                // Idle animations - cycle through different effects
                idle_animation = (frame / 300) % 4;  // Change every 15 seconds

                switch (idle_animation) {
                    case 0:
                        anim_draw_starfield(frame);
                        break;
                    case 1:
                        anim_draw_wave_pattern(frame, RGB565(0, 150, 255));
                        break;
                    case 2:
                        anim_draw_rainbow_cycle(frame);
                        break;
                    case 3:
                        display_draw_tinkercademy_logo();
                        break;
                }
            }
        } else {
            // Show static logo when completely idle
            if (frame % 100 == 0) {
                display_draw_tinkercademy_logo();
            }
        }

        xSemaphoreGive(anim_mutex);

        // Update display here (send framebuffer to LCD via SPI)
        // This would involve actual SPI transmission

        vTaskDelay(pdMS_TO_TICKS(33));  // ~30 FPS
    }
}

// Initialize animation system
void display_animations_init(void) {
    anim_mutex = xSemaphoreCreateMutex();

    // Allocate framebuffer
    framebuffer = heap_caps_malloc(DISPLAY_WIDTH * DISPLAY_HEIGHT * sizeof(uint16_t), MALLOC_CAP_DMA);
    if (framebuffer == NULL) {
        ESP_LOGE(TAG, "Failed to allocate framebuffer");
        return;
    }

    // Initialize subsystems
    init_starfield();
    memset(particles, 0, sizeof(particles));

    // Get device role for animation customization
    anim_ctx.device_role = device_config_get_role();

    // Start animation task
    xTaskCreate(animation_task, "animation_task", 8192, NULL, 5, NULL);

    ESP_LOGI(TAG, "Display animations initialized");
}

// Start idle animations
void display_animations_start_idle(void) {
    xSemaphoreTake(anim_mutex, portMAX_DELAY);
    anim_ctx.active = true;
    anim_ctx.song_type = SONG_TYPE_SOLO;  // Use solo to trigger idle anims
    anim_ctx.frame = 0;
    xSemaphoreGive(anim_mutex);
}

// Start playback animations
void display_animations_start_playback(song_type_t song_type) {
    xSemaphoreTake(anim_mutex, portMAX_DELAY);
    anim_ctx.active = true;
    anim_ctx.song_type = song_type;
    anim_ctx.frame = 0;
    anim_ctx.beat_intensity = 0.8f;
    xSemaphoreGive(anim_mutex);
}

// Stop animations
void display_animations_stop(void) {
    xSemaphoreTake(anim_mutex, portMAX_DELAY);
    anim_ctx.active = false;
    xSemaphoreGive(anim_mutex);
}

// Update beat intensity for reactive animations
void display_animations_update_beat(float intensity) {
    xSemaphoreTake(anim_mutex, portMAX_DELAY);
    anim_ctx.beat_intensity = intensity;
    xSemaphoreGive(anim_mutex);
}