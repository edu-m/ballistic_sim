#ifndef VECTOR_FONT_H
#define VECTOR_FONT_H

#include <SDL2/SDL.h>

static inline void draw_vector_char(SDL_Renderer* renderer, char c, float ox, float oy, float scale) {
    float strokes[80]; 
    int count = 0;

    #define LINE(x1,y1,x2,y2) do { strokes[count++]=(x1); strokes[count++]=(y1); strokes[count++]=(x2); strokes[count++]=(y2); } while(0)
    #define L2(x1,y1,x2,y2,x3,y3) do { LINE(x1,y1,x2,y2); LINE(x2,y2,x3,y3); } while(0)
    #define L3(x1,y1,x2,y2,x3,y3,x4,y4) do { L2(x1,y1,x2,y2,x3,y3); LINE(x3,y3,x4,y4); } while(0)
    #define L4(x1,y1,x2,y2,x3,y3,x4,y4,x5,y5) do { L3(x1,y1,x2,y2,x3,y3,x4,y4); LINE(x4,y4,x5,y5); } while(0)

    if (c >= 'a' && c <= 'z') c -= 32; 
    switch(c) {
        case 'A': L4(0,5, 0,1, 2,0, 4,1, 4,5); LINE(0,3,4,3); break;
        case 'B': L4(0,0, 3,0, 4,1, 3,2, 0,2); L3(3,2, 4,4, 3,5, 0,5); LINE(0,0,0,5); break;
        case 'C': L4(4,0, 1,0, 0,1, 0,4, 1,5); LINE(1,5,4,5); break;
        case 'D': L4(0,0, 3,0, 4,1, 4,4, 3,5); LINE(3,5,0,5); LINE(0,0,0,5); break;
        case 'E': L2(4,0, 0,0, 0,5); LINE(0,5,4,5); LINE(0,2,3,2); break;
        case 'F': L2(4,0, 0,0, 0,5); LINE(0,2,3,2); break;
        case 'G': L4(4,1, 4,0, 1,0, 0,1, 0,4); L4(0,4, 1,5, 4,5, 4,3, 2,3); break;
        case 'H': LINE(0,0,0,5); LINE(4,0,4,5); LINE(0,2,4,2); break;
        case 'I': LINE(0,0,4,0); LINE(2,0,2,5); LINE(0,5,4,5); break;
        case 'J': L3(4,0, 4,4, 3,5, 1,5); LINE(1,5,0,4); break;
        case 'K': LINE(0,0,0,5); L2(4,0, 0,2, 4,5); break;
        case 'L': L2(0,0, 0,5, 4,5); break;
        case 'M': L4(0,5, 0,0, 2,2, 4,0, 4,5); break;
        case 'N': L3(0,5, 0,0, 4,5, 4,0); break;
        case 'O': L4(1,0, 3,0, 4,1, 4,4, 3,5); L4(3,5, 1,5, 0,4, 0,1, 1,0); break;
        case 'P': L4(0,5, 0,0, 3,0, 4,1, 4,2); L2(4,2, 3,3, 0,3); break;
        case 'Q': L4(1,0, 3,0, 4,1, 4,4, 3,5); L4(3,5, 1,5, 0,4, 0,1, 1,0); LINE(2,3,4,5); break;
        case 'R': L4(0,5, 0,0, 3,0, 4,1, 4,2); L2(4,2, 3,3, 0,3); LINE(2,3,4,5); break;
        case 'S': L4(4,0, 0,0, 0,2, 4,3, 4,5); LINE(4,5,0,5); break;
        case 'T': LINE(0,0,4,0); LINE(2,0,2,5); break;
        case 'U': L4(0,0, 0,4, 1,5, 3,5, 4,4); LINE(4,4,4,0); break;
        case 'V': L2(0,0, 2,5, 4,0); break;
        case 'W': L4(0,0, 1,5, 2,3, 3,5, 4,0); break;
        case 'X': LINE(0,0,4,5); LINE(4,0,0,5); break;
        case 'Y': L2(0,0, 2,2, 4,0); LINE(2,2,2,5); break;
        case 'Z': L3(0,0, 4,0, 0,5, 4,5); break;
        case '0': L4(1,0, 3,0, 4,1, 4,4, 3,5); L4(3,5, 1,5, 0,4, 0,1, 1,0); LINE(4,0,0,5); break;
        case '1': L2(1,1, 2,0, 2,5); LINE(1,5,3,5); break;
        case '2': L4(0,1, 1,0, 3,0, 4,1, 4,2); L2(4,2, 0,5, 4,5); break;
        case '3': L4(0,0, 4,0, 2,2, 4,3, 4,4); L2(4,4, 3,5, 0,5); break;
        case '4': L3(3,5, 3,0, 0,3, 4,3); break;
        case '5': L4(4,0, 0,0, 0,2, 3,2, 4,3); L2(4,3, 4,4, 3,5); LINE(3,5,0,5); break;
        case '6': L4(4,0, 1,0, 0,1, 0,4, 1,5); L4(1,5, 3,5, 4,4, 4,2, 0,2); break;
        case '7': L2(0,0, 4,0, 1,5); break;
        case '8': L4(1,2, 0,1, 1,0, 3,0, 4,1); L4(4,1, 3,2, 4,3, 4,4, 3,5); L3(3,5, 1,5, 0,4, 1,2); LINE(1,2,3,2); break;
        case '9': L4(4,2, 0,2, 0,1, 1,0, 3,0); L4(3,0, 4,1, 4,4, 3,5, 0,5); break;
        case '-': LINE(0,2,4,2); break;
        case '.': LINE(2,4,2,5); break;
        case ':': LINE(2,1,2,2); LINE(2,4,2,5); break;
    }

    #undef LINE
    #undef L2
    #undef L3
    #undef L4

    for(int i = 0; i < count; i += 4) {
        SDL_RenderDrawLine(renderer, 
            (int)(ox + (strokes[i] * scale)),   (int)(oy + (strokes[i+1] * scale)),
            (int)(ox + (strokes[i+2] * scale)), (int)(oy + (strokes[i+3] * scale))
        );
    }
}

static inline void draw_vector_string(SDL_Renderer* renderer, const char* text, float x, float y, float scale) {
    float cursor_x = x;
    for (int i = 0; text[i] != '\0'; i++) {
        char c = text[i];
        if (c == ' ') {
            cursor_x += 6.0f * scale; 
            continue;
        }
        draw_vector_char(renderer, c, cursor_x, y, scale);
        cursor_x += 6.0f * scale; 
    }
}

#endif