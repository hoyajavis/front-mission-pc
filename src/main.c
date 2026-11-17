#include "boot_loader.h"
#include "config.h"
#include "gui_menu.h"
#include "input.h"
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

    FmMouseState mouse;
    fm_input_init(&mouse);

    uint8_t *pixels = (uint8_t *)calloc(1, 512 * 480 * 4);
    bool running = true;
    while (running) {
        int cur_w = win_w, cur_h = win_h;
        SDL_GetWindowSize(window, &cur_w, &cur_h);
        bool widescreen = (cfg.display_mode != 2);

        SDL_Event ev;
        while (SDL_PollEvent(&ev)) {
            if (ev.type == SDL_QUIT) running = false;
            if (ev.type == SDL_KEYDOWN && ev.key.keysym.sym == SDLK_ESCAPE) running = false;
            if (ev.type == SDL_MOUSEMOTION) {
                fm_input_on_mouse_move(&mouse, ev.motion.x, ev.motion.y, cur_w, cur_h, widescreen);
            } else if (ev.type == SDL_MOUSEBUTTONDOWN || ev.type == SDL_MOUSEBUTTONUP) {
                bool down = (ev.type == SDL_MOUSEBUTTONDOWN);
                int btn = (ev.button.button == SDL_BUTTON_LEFT) ? 1 :
                          (ev.button.button == SDL_BUTTON_MIDDLE) ? 2 :
                          (ev.button.button == SDL_BUTTON_RIGHT) ? 3 : 0;
                if (btn != 0) {
                    fm_input_on_mouse_button(&mouse, btn, down, ev.button.x, ev.button.y, cur_w, cur_h, widescreen);
                }
            }
        }

        uint16_t joypad = 0;
        fm_input_apply_pre_frame(&mouse, FM_INPUT_MODE_TACTICAL, NULL, snes->ram, &joypad);
        snes->input1->currentState = joypad;

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
