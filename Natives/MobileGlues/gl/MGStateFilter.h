#pragma once
#include <GLES3/gl3.h>
#include <cstdint>
#include <cstdlib>

enum MGStateBits : uint32_t {
    BIT_BLEND        = 1 << 0,
    BIT_DEPTH_TEST   = 1 << 1,
    BIT_CULL_FACE    = 1 << 2,
    BIT_SCISSOR_TEST = 1 << 3
};

struct MGDevicePipelineCache {
    uint32_t active_bits = 0;
    GLenum blend_s_rgb = GL_SRC_ALPHA;
    GLenum blend_d_rgb = GL_ONE_MINUS_SRC_ALPHA;
    GLenum blend_s_a   = GL_ONE;
    GLenum blend_d_a   = GL_ONE_MINUS_SRC_ALPHA;
};

static thread_local MGDevicePipelineCache s_DeviceCache;

// Bitmask-cached state toggles
inline void MG_EnableFilter(GLenum cap) {
    uint32_t bit = 0;
    if (cap == GL_BLEND) bit = BIT_BLEND;
    else if (cap == GL_DEPTH_TEST) bit = BIT_DEPTH_TEST;
    else if (cap == GL_CULL_FACE) bit = BIT_CULL_FACE;
    else if (cap == GL_SCISSOR_TEST) bit = BIT_SCISSOR_TEST;
    else { glEnable(cap); return; }

    if (!(s_DeviceCache.active_bits & bit)) {
        s_DeviceCache.active_bits |= bit;
        glEnable(cap);
    }
}

inline void MG_DisableFilter(GLenum cap) {
    uint32_t bit = 0;
    if (cap == GL_BLEND) bit = BIT_BLEND;
    else if (cap == GL_DEPTH_TEST) bit = BIT_DEPTH_TEST;
    else if (cap == GL_CULL_FACE) bit = BIT_CULL_FACE;
    else if (cap == GL_SCISSOR_TEST) bit = BIT_SCISSOR_TEST;
    else { glDisable(cap); return; }

    if (s_DeviceCache.active_bits & bit) {
        s_DeviceCache.active_bits &= ~bit;
        glDisable(cap);
    }
}

inline void MG_BlendFuncFilter(GLenum srgb, GLenum drgb, GLenum sa, GLenum da) {
    if (s_DeviceCache.blend_s_rgb != srgb || s_DeviceCache.blend_d_rgb != drgb ||
        s_DeviceCache.blend_s_a != sa || s_DeviceCache.blend_d_a != da) {
        
        s_DeviceCache.blend_s_rgb = srgb;
        s_DeviceCache.blend_d_rgb = drgb;
        s_DeviceCache.blend_s_a = sa;
        s_DeviceCache.blend_d_a = da;
        glBlendFuncSeparate(srgb, drgb, sa, da);
    }
}

// 4MB zero-allocation scratchpad for MultiDraw index arrays
class MGFrameScratchpad {
private:
    static constexpr size_t POOL_SIZE = 4 * 1024 * 1024;
    uint8_t* m_pool;
    size_t m_head = 0;

public:
    MGFrameScratchpad() { m_pool = (uint8_t*)malloc(POOL_SIZE); }
    ~MGFrameScratchpad() { free(m_pool); }

    void* Alloc(size_t size) {
        size_t aligned = (m_head + 7) & ~7;
        if (aligned + size > POOL_SIZE) {
            aligned = 0; // Wrap around for frame ring
        }
        m_head = aligned + size;
        return m_pool + aligned;
    }

    void FrameReset() { m_head = 0; }
};

static thread_local MGFrameScratchpad t_ScratchAllocator;
