#pragma once

#include "PerFrameContext.hpp"
#include "PresetState.hpp"

#include <Renderer/Mesh.hpp>

#include <array>

namespace libprojectM {
namespace MilkdropPreset {

/**
 * @brief Renders a video "echo" (ghost image) effect and gamma adjustments.
 */
class VideoEcho
{
public:
	VideoEcho() = delete;
    explicit VideoEcho(const PresetState& presetState);

	void Draw();

private:
    /**
     * Collects the quads for the video echo effect, including any gamma adjustment.
     */
    void AddVideoEchoQuads();

    /**
     * Collects the quads for the gamma adjustment only.
     */
    void AddGammaAdjustmentQuads();

    /**
     * Appends a single screen-sized quad to the mesh.
     * @param uvs The texture coordinates of the four corners.
     * @param brightness A multiplier for the corner colors.
     */
    void AddQuad(const std::array<Renderer::TextureUV, 4>& uvs, float brightness);

    const PresetState& m_presetState; //!< The global preset state.

    std::array<std::array<float, 3>, 4> m_shade; // !< Random, changing color values for the four corners
    std::array<Renderer::Point, 4> m_quadVertices; //!< Corner positions of the screen-sized quad.
    Renderer::Mesh m_echoMesh; //!< The video echo/gamma adj mesh
    Renderer::Sampler m_sampler{GL_CLAMP_TO_EDGE, GL_LINEAR};
};

} // namespace MilkdropPreset
} // namespace libprojectM
