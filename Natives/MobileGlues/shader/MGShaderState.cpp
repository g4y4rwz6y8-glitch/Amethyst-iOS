#include <GLES3/gl3.h>
#include <unordered_map>
#include <string>
#include <vector>
#include <cstring>
#include <cstdio>

struct SamplerUnitMap {
    GLint location;
    GLint target_unit;
};

struct MGProgramCache {
    GLint loc_modelview = -1;
    GLint loc_projection = -1;
    GLint loc_normal_matrix = -1;
    GLint loc_alpha_ref = -1;
    std::vector<SamplerUnitMap> samplers;
};

// Global registry for program uniform locations
static std::unordered_map<GLuint, MGProgramCache> s_ProgramCache;

// Global matrix storage shadow
struct MatrixShadowState {
    float modelview[16];
    float projection[16];
    float normal[9];
    float alpha_test_ref = 0.1f;
    bool alpha_test_enabled = false;
} g_MGMatrixShadow;

void MG_RegisterProgram(GLuint program) {
    if (program == 0) return;

    MGProgramCache cache;
    cache.loc_modelview = glGetUniformLocation(program, "mg_ModelViewMatrix");
    if (cache.loc_modelview == -1) {
        cache.loc_modelview = glGetUniformLocation(program, "gbufferModelView");
    }
    if (cache.loc_modelview == -1) {
        cache.loc_modelview = glGetUniformLocation(program, "ModelViewMat"); // Minecraft 1.21 Core
    }

    cache.loc_projection = glGetUniformLocation(program, "mg_ProjectionMatrix");
    if (cache.loc_projection == -1) {
        cache.loc_projection = glGetUniformLocation(program, "gbufferProjection");
    }
    if (cache.loc_projection == -1) {
        cache.loc_projection = glGetUniformLocation(program, "ProjMat"); // Minecraft 1.21 Core
    }

    cache.loc_normal_matrix = glGetUniformLocation(program, "mg_NormalMatrix");
    cache.loc_alpha_ref = glGetUniformLocation(program, "alphaTestRef");

    // Inspect active samplers and auto-bind stripped layout(binding) declarations
    GLint totalUniforms = 0;
    glGetProgramiv(program, GL_ACTIVE_UNIFORMS, &totalUniforms);

    GLchar uName[256];
    for (GLint i = 0; i < totalUniforms; ++i) {
        GLsizei nameLen;
        GLint uSize;
        GLenum uType;
        glGetActiveUniform(program, i, sizeof(uName), &nameLen, &uSize, &uType, uName);

        if (uType == GL_SAMPLER_2D || uType == GL_SAMPLER_2D_SHADOW) {
            GLint loc = glGetUniformLocation(program, uName);
            if (loc == -1) continue;

            GLint unit = 0;
            // Iris / OptiFine shader conventions
            if (strcmp(uName, "gtexture") == 0 || strcmp(uName, "texture") == 0 || strcmp(uName, "Sampler0") == 0) {
                unit = 0;
            } else if (strcmp(uName, "lightmap") == 0 || strcmp(uName, "Sampler1") == 0 || strcmp(uName, "gaux1") == 0) {
                unit = 1;
            } else if (strcmp(uName, "normals") == 0 || strcmp(uName, "Sampler2") == 0 || strcmp(uName, "gaux2") == 0) {
                unit = 2;
            } else if (strcmp(uName, "specular") == 0 || strcmp(uName, "gaux3") == 0) {
                unit = 3;
            } else if (strcmp(uName, "shadowtex0") == 0 || strcmp(uName, "shadowcolor0") == 0) {
                unit = 4;
            } else {
                continue;
            }
            cache.samplers.push_back({loc, unit});
        }
    }

    s_ProgramCache[program] = cache;
}

void MG_UnregisterProgram(GLuint program) {
    s_ProgramCache.erase(program);
}

// Hook called whenever glUseProgram executes
void MG_SyncShaderState(GLuint program) {
    if (program == 0) return;

    auto it = s_ProgramCache.find(program);
    if (it == s_ProgramCache.end()) {
        MG_RegisterProgram(program);
        it = s_ProgramCache.find(program);
        if (it == s_ProgramCache.end()) return;
    }

    const MGProgramCache& entry = it->second;

    // 1. Force upload the current active ModelView and Proj matrices to avoid (0, 0, 0, 0) clip-space vertices
    if (entry.loc_modelview != -1) {
        glUniformMatrix4fv(entry.loc_modelview, 1, GL_FALSE, g_MGMatrixShadow.modelview);
    }
    if (entry.loc_projection != -1) {
        glUniformMatrix4fv(entry.loc_projection, 1, GL_FALSE, g_MGMatrixShadow.projection);
    }
    if (entry.loc_normal_matrix != -1) {
        glUniformMatrix3fv(entry.loc_normal_matrix, 1, GL_FALSE, g_MGMatrixShadow.normal);
    }

    // 2. Prevent player model alpha discard
    if (entry.loc_alpha_ref != -1) {
        float ref = g_MGMatrixShadow.alpha_test_enabled ? g_MGMatrixShadow.alpha_test_ref : 0.00392f;
        glUniform1f(entry.loc_alpha_ref, ref);
    }

    // 3. Re-link texture samplers to texture units
    for (const auto& binding : entry.samplers) {
        glUniform1i(binding.location, binding.target_unit);
    }
}

// Vertex Attribute Pointer Hook for Iris generic attribute 10 (mc_Entity)
extern "C" void mg_glVertexAttribIPointer(GLuint index, GLint size, GLenum type, GLsizei stride, const void *pointer) {
    // Forward directly to native OpenGL ES 3.0 integer attribute handler to prevent GL_INVALID_OPERATION
    glVertexAttribIPointer(index, size, type, stride, pointer);
}
