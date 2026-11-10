#include "VideoEcho.hpp"

#include <Renderer/BlendMode.hpp>

namespace libprojectM {
namespace MilkdropPreset {

VideoEcho::VideoEcho(const PresetState& presetState)
    : m_presetState(presetState)
    , m_echoMesh(Renderer::VertexBufferUsage::DynamicDraw, true, true)
{
    m_echoMesh.SetRenderPrimitiveType(Renderer::Mesh::PrimitiveType::TriangleStrip);
}

void VideoEcho::Draw()
{
    float const aspect = m_presetState.renderContext.viewportSizeX / static_cast<float>(m_presetState.renderContext.viewportSizeY * m_presetState.renderContext.invAspectY);
    float aspectMultX = 1.0f;
    float aspectMultY = 1.0f;

    if (aspect > 1)
    {
        aspectMultY = aspect;
    }
    else
    {
        aspectMultX = 1.0f / aspect;
    }

    float const fOnePlusInvWidth = 1.0f + 1.0f / static_cast<float>(m_presetState.renderContext.viewportSizeX);
    float const fOnePlusInvHeight = 1.0f + 1.0f / static_cast<float>(m_presetState.renderContext.viewportSizeY);
    m_quadVertices = {{{-fOnePlusInvWidth * aspectMultX, fOnePlusInvHeight * aspectMultY},
                       {fOnePlusInvWidth * aspectMultX, fOnePlusInvHeight * aspectMultY},
                       {-fOnePlusInvWidth * aspectMultX, -fOnePlusInvHeight * aspectMultY},
                       {fOnePlusInvWidth * aspectMultX, -fOnePlusInvHeight * aspectMultY}}};

    for (int i = 0; i < 4; i++)
    {
        auto const indexFloat = static_cast<float>(i);
        m_shade[i][0] = 0.6f + 0.3f * sinf(m_presetState.renderContext.time * 30.0f * 0.0143f + 3 + indexFloat * 21 + m_presetState.hueRandomOffsets[3]);
        m_shade[i][1] = 0.6f + 0.3f * sinf(m_presetState.renderContext.time * 30.0f * 0.0107f + 1 + indexFloat * 13 + m_presetState.hueRandomOffsets[1]);
        m_shade[i][2] = 0.6f + 0.3f * sinf(m_presetState.renderContext.time * 30.0f * 0.0129f + 6 + indexFloat * 9 + m_presetState.hueRandomOffsets[2]);

        float const max = std::max(m_shade[i][0], std::max(m_shade[i][1], m_shade[i][2]));

        for (int k = 0; k < 3; k++)
        {
            m_shade[i][k] /= max;
            m_shade[i][k] = 0.5f + 0.5f * m_shade[i][k];
        }
    }

    // Updating a buffer which is still in use by a previous draw call can stall the GPU pipeline, so
    // first collect the quads of all passes, then upload them once and draw them in order.
    m_echoMesh.Vertices().Get().clear();
    m_echoMesh.Colors().Get().clear();
    m_echoMesh.UVs().Get().clear();

    if (m_presetState.videoEchoAlpha > 0.001f)
    {
        AddVideoEchoQuads();
    }
    else
    {
        AddGammaAdjustmentQuads();
    }

    const auto vertexCount = m_echoMesh.Vertices().Size();
    if (m_echoMesh.Indices().Size() != vertexCount)
    {
        m_echoMesh.Indices().Resize(vertexCount);
        m_echoMesh.Indices().MakeContinuous();
    }

    m_echoMesh.Update();

    auto shader = m_presetState.texturedShader.lock();
    shader->Bind();
    shader->SetUniformMat4x4("vertex_transformation", PresetState::orthogonalProjection);
    shader->SetUniformInt("texture_sampler", 0);

    auto mainTexture = m_presetState.mainTexture.lock();
    if (mainTexture)
    {
        mainTexture->Bind(0);
        m_sampler.Bind(0);
    }

    // The first quad replaces the image, all following quads are added on top.
    Renderer::BlendMode::Set(true, Renderer::BlendMode::Function::One, Renderer::BlendMode::Function::Zero);

    for (size_t firstVertex = 0; firstVertex < vertexCount; firstVertex += 4)
    {
        m_echoMesh.Draw(firstVertex, 4);

        if (firstVertex == 0)
        {
            Renderer::BlendMode::SetBlendFunction(Renderer::BlendMode::Function::One, Renderer::BlendMode::Function::One);
        }
    }

    Renderer::BlendMode::SetBlendActive(false);
    Renderer::Mesh::Unbind();
    Renderer::Shader::Unbind();

    if (mainTexture)
    {
        mainTexture->Unbind(0);
        Renderer::Sampler::Unbind(0);
    }
}

void VideoEcho::AddVideoEchoQuads()
{
    auto const videoEchoZoom = m_presetState.videoEchoZoom;
    auto const videoEchoAlpha = m_presetState.videoEchoAlpha;
    auto const videoEchoOrientation = m_presetState.videoEchoOrientation % 4;
    auto const gammaAdj = m_presetState.gammaAdj;

    for (int pass = 0; pass < 2; pass++)
    {
        float const zoom = (pass == 0) ? 1.0f : videoEchoZoom;

        float const tempLow = 0.5f - 0.5f / zoom;
        float const tempHigh = 0.5f + 0.5f / zoom;

        std::array<Renderer::TextureUV, 4> uvs{{{tempLow, tempLow},
                                                {tempHigh, tempLow},
                                                {tempLow, tempHigh},
                                                {tempHigh, tempHigh}}};

        // Flipping
        if (pass == 1)
        {
            for (auto& uv : uvs)
            {
                if (videoEchoOrientation % 2 == 1)
                {
                    uv.SetU(1.0f - uv.U());
                }
                if (videoEchoOrientation >= 2)
                {
                    uv.SetV(1.0f - uv.V());
                }
            }
        }

        float const mix = (pass == 1) ? videoEchoAlpha : 1.0f - videoEchoAlpha;

        AddQuad(uvs, mix);

        if (gammaAdj > 0.001f)
        {
            int const redrawCount = static_cast<int>(gammaAdj - 0.0001f);

            for (int redraw = 0; redraw < redrawCount; redraw++)
            {
                float gamma;
                if (redraw == redrawCount - 1)
                {
                    gamma = gammaAdj - static_cast<float>(static_cast<int>(gammaAdj - 0.0001f));
                }
                else
                {
                    gamma = 1.0f;
                }

                AddQuad(uvs, gamma * mix);
            }
        }
    }
}

void VideoEcho::AddGammaAdjustmentQuads()
{
    const std::array<Renderer::TextureUV, 4> uvs{{{0.0f, 0.0f},
                                                  {1.0f, 0.0f},
                                                  {0.0f, 1.0f},
                                                  {1.0f, 1.0f}}};

    auto const gammaAdj = m_presetState.gammaAdj;
    int const redrawCount = static_cast<int>(gammaAdj - 0.0001f) + 1;

    for (int redraw = 0; redraw < redrawCount; redraw++)
    {
        float gamma;
        if (redraw == redrawCount - 1)
        {
            gamma = gammaAdj - static_cast<float>(redraw);
        }
        else
        {
            gamma = 1.0f;
        }

        AddQuad(uvs, gamma);
    }
}

void VideoEcho::AddQuad(const std::array<Renderer::TextureUV, 4>& uvs, float brightness)
{
    auto& vertices = m_echoMesh.Vertices().Get();
    auto& colors = m_echoMesh.Colors().Get();
    auto& meshUVs = m_echoMesh.UVs().Get();

    for (int vertex = 0; vertex < 4; vertex++)
    {
        vertices.push_back(m_quadVertices[vertex]);
        colors.emplace_back(brightness * m_shade[vertex][0],
                            brightness * m_shade[vertex][1],
                            brightness * m_shade[vertex][2],
                            1.0f);
        meshUVs.push_back(uvs[vertex]);
    }
}

} // namespace MilkdropPreset
} // namespace libprojectM
