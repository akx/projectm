#include "Border.hpp"

#include "PerFrameContext.hpp"

#include <Renderer/BlendMode.hpp>

#include <array>
#include <vector>

namespace libprojectM {
namespace MilkdropPreset {

Border::Border(PresetState& presetState)
    : m_presetState(presetState)
    , m_borderMesh(Renderer::VertexBufferUsage::StreamDraw)
{
    m_borderMesh.SetRenderPrimitiveType(Renderer::Mesh::PrimitiveType::Triangles);
    m_borderMesh.SetVertexCount(2 * BorderVertexCount);

    // Two borders with 8 vertices each (4 outer and 4 inner corners), drawn as 8 triangles.
    std::vector<uint32_t> indices;
    for (uint32_t firstVertex = 0; firstVertex < 2 * BorderVertexCount; firstVertex += BorderVertexCount)
    {
        for (uint32_t index : {0, 1, 4,
                               1, 4, 5,
                               2, 3, 6,
                               3, 7, 6,
                               2, 0, 6,
                               0, 4, 6,
                               3, 7, 5,
                               1, 3, 5})
        {
            indices.push_back(firstVertex + index);
        }
    }
    m_borderMesh.Indices().Set(indices);
}

void Border::Draw(const PerFrameContext& presetPerFrameContext)
{
    // Draw Borders
    float const outerBorderSize = static_cast<float>(*presetPerFrameContext.ob_size);
    float const innerBorderSize = static_cast<float>(*presetPerFrameContext.ib_size);

    const std::array<Renderer::Color, 2> colors{{{static_cast<float>(*presetPerFrameContext.ob_r),
                                                  static_cast<float>(*presetPerFrameContext.ob_g),
                                                  static_cast<float>(*presetPerFrameContext.ob_b),
                                                  static_cast<float>(*presetPerFrameContext.ob_a)},
                                                 {static_cast<float>(*presetPerFrameContext.ib_r),
                                                  static_cast<float>(*presetPerFrameContext.ib_g),
                                                  static_cast<float>(*presetPerFrameContext.ib_b),
                                                  static_cast<float>(*presetPerFrameContext.ib_a)}}};

    // Most presets don't use borders, so don't touch any GL state if both are invisible.
    if (colors[0].A() <= 0.001f && colors[1].A() <= 0.001f)
    {
        return;
    }

    // Upload the vertices of both borders at once, then draw the visible ones.
    for (int border = 0; border < 2; border++)
    {
        float innerRadius = (border == 0) ? 1.0f - outerBorderSize : 1.0f - outerBorderSize - innerBorderSize;
        float outerRadius = (border == 0) ? 1.0f : 1.0f - outerBorderSize;

        const size_t firstVertex = border * BorderVertexCount;
        auto& vertices = m_borderMesh.Vertices();
        vertices[firstVertex + 0] = {outerRadius, outerRadius};
        vertices[firstVertex + 1] = {outerRadius, -outerRadius};
        vertices[firstVertex + 2] = {-outerRadius, outerRadius};
        vertices[firstVertex + 3] = {-outerRadius, -outerRadius};
        vertices[firstVertex + 4] = {innerRadius, innerRadius};
        vertices[firstVertex + 5] = {innerRadius, -innerRadius};
        vertices[firstVertex + 6] = {-innerRadius, innerRadius};
        vertices[firstVertex + 7] = {-innerRadius, -innerRadius};
    }

    m_borderMesh.Update();

    // No additive drawing for borders
    Renderer::BlendMode::Set(true, Renderer::BlendMode::Function::SourceAlpha, Renderer::BlendMode::Function::OneMinusSourceAlpha);

    auto shader = m_presetState.untexturedShader.lock();
    shader->Bind();
    shader->SetUniformMat4x4("vertex_transformation", PresetState::orthogonalProjection);
    shader->SetUniformFloat("vertex_point_size", 1.0f);

    for (int border = 0; border < 2; border++)
    {
        const auto& color = colors[border];
        if (color.A() > 0.001f)
        {
            glVertexAttrib4f(1, color.R(), color.G(), color.B(), color.A());
            m_borderMesh.Draw(border * BorderIndexCount, BorderIndexCount);
        }
    }

    Renderer::Mesh::Unbind();
    Renderer::Shader::Unbind();
    Renderer::BlendMode::SetBlendActive(false);
}

} // namespace MilkdropPreset
} // namespace libprojectM
