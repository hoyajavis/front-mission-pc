#include "boot_loader.h"
#include "config.h"
#include "gui_menu.h"
#include "snes.h"

#include <SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char **argv) {
    (void)argc;
    (void)argv;

    FmUserConfig cfg;
    fm_config_init_defaults(&cfg);
    fm_config_load(&cfg, "front_mission.ini");

    FmBootConfig boot_cfg;
    fm_config_to_boot_config(&cfg, &boot_cfg);

    FmBootResult rom_res;
    if (!fm_boot_load_rom(&boot_cfg, &rom_res)) {
        fprintf(stderr, "Boot error: %s\n", rom_res.error_msg);
        return 1;
    }

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) != 0) {
        fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        fm_boot_free_result(&rom_res);
        return 1;
    }

    int win_w = 800 * cfg.window_scale;
    int win_h = 448 * cfg.window_scale;
    SDL_Window *window = SDL_CreateWindow(
        "Front Mission (SNES) - Native PC Port",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        win_w, win_h, SDL_WINDOW_RESIZABLE
    );
    SDL_Renderer *renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    SDL_Texture *texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, 512, 224);

    Snes *snes = snes_init();
    snes_loadRom(snes, rom_res.rom_data, (int)rom_res.rom_size);
    snes_setPixelFormat(snes, pixelFormatXRGB);

    uint8_t *pixels = (uint8_t *)calloc(1, 512 * 480 * 4);
    bool running = true;
    while (running) {
        SDL_Event ev;
        while (SDL_PollEvent(&ev)) {
            if (ev.type == SDL_QUIT) running = false;
            if (ev.type == SDL_KEYDOWN && ev.key.keysym.sym == SDLK_ESCAPE) running = false;
        }
        snes_runFrame(snes);
        snes_setPixels(snes, pixels);
        SDL_UpdateTexture(texture, NULL, pixels, 512 * 4);
        SDL_RenderClear(renderer);
        SDL_RenderCopy(renderer, texture, NULL, NULL);
        SDL_RenderPresent(renderer);
    }

    free(pixels);
    snes_free(snes);
    SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    fm_boot_free_result(&rom_res);
    return 0;
}
