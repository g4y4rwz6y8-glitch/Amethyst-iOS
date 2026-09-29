#include <GLES3/gl3.h>
#include <unordered_map>
#include <vector>
#include <cstdlib>

#define MG_MAX_TEX_UNITS 32
#define MG_TARGET_2D 0
#define MG_TARGET_3D 1
#define MG_TARGET_CUBE 2
#define MG_TARGET_COUNT 3

struct MGTextureState {
    GLuint bound_textures[MG_MAX_TEX_UNITS][MG_TARGET_COUNT] = {{0}};
    GLint active_unit = 0;
};

static thread_local MGTextureState s_TexState;

static inline int TargetToIndex(GLenum target) {
    switch (target) {
        case GL_TEXTURE_2D: return MG_TARGET_2D;
        case GL_TEXTURE_3D: return MG_TARGET_3D;
        case GL_TEXTURE_CUBE_MAP: return MG_TARGET_CUBE;
        default: return -1;
    }
}

extern "C" {

void mg_glActiveTexture(GLenum texture) {
    s_TexState.active_unit = texture - GL_TEXTURE0;
    glActiveTexture(texture);
}

void mg_glBindTexture(GLenum target, GLuint texture) {
    int idx = TargetToIndex(target);
    if (idx >= 0 && s_TexState.active_unit >= 0 && s_TexState.active_unit < MG_MAX_TEX_UNITS) {
        s_TexState.bound_textures[s_TexState.active_unit][idx] = texture;
    }
    glBindTexture(target, texture);
}

// Intercept glDeleteTextures to eliminate stale texture pointers during resource pack reloads
void mg_glDeleteTextures(GLsizei n, const GLuint* textures) {
    if (n <= 0 || !textures) return;

    for (GLsizei i = 0; i < n; ++i) {
        GLuint tex = textures[i];
        if (tex == 0) continue;

        // Scrub all cached bindings across all texture units
        for (int u = 0; u < MG_MAX_TEX_UNITS; ++u) {
            for (int t = 0; t < MG_TARGET_COUNT; ++t) {
                if (s_TexState.bound_textures[u][t] == tex) {
                    s_TexState.bound_textures[u][t] = 0;

                    // If deleted texture is currently bound to the active unit, reset hardware state
                    if (s_TexState.active_unit == u) {
                        GLenum glTarget = (t == MG_TARGET_2D) ? GL_TEXTURE_2D :
                                          (t == MG_TARGET_3D) ? GL_TEXTURE_3D : GL_TEXTURE_CUBE_MAP;
                        glBindTexture(glTarget, 0);
                    }
                }
            }
        }
    }

    // Call underlying driver deletion
    glDeleteTextures(n, textures);

    // On iOS MetalANGLE, synchronously flush deallocation commands
    #if defined(__APPLE__)
    glFlush();
    #endif
}

} // extern "C"
