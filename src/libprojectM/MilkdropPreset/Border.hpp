#pragma once

#include "PresetState.hpp"

#include <Renderer/Mesh.hpp>

namespace libprojectM {
namespace MilkdropPreset {


/**
 * @brief Renders a border around the screen.
 */
class Border
{
public:
    Border() = delete;

    explicit Border(PresetState& presetState);

    /**
     * Draws the border.
     * @param presetPerFrameContext The per-frame context variables.
     */
    void Draw(const PerFrameContext& presetPerFrameContext);

private:
    static constexpr size_t BorderVertexCount = 8; //!< Number of vertices of a single border.
    static constexpr size_t BorderIndexCount = 24; //!< Number of indices of a single border.

    PresetState& m_presetState; //!< The global preset state.
    Renderer::Mesh m_borderMesh; //!< The border geometry.
};

} // namespace MilkdropPreset
} // namespace libprojectM
