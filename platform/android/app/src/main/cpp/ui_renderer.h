
#ifndef MUEANG_UI_RENDERER_H
#define MUEANG_UI_RENDERER_H

#include <GLES2/gl2.h>

typedef struct {
    GLuint program;
    GLint position;
    GLint color;
    int ready;
} UIRenderer;

int UIRenderer_Init(UIRenderer *ui);

void UIRenderer_Draw(
    UIRenderer *ui,
    int width,
    int height,
    float time_seconds
);

void UIRenderer_Release(UIRenderer *ui);

#endif
