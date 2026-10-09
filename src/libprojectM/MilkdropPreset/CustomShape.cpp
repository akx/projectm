#include "CustomShape.hpp"

#include "PresetFileParser.hpp"

#include <Renderer/BlendMode.hpp>
#include <Renderer/TextureManager.hpp>

#include <array>
#include <vector>

namespace libprojectM {
namespace MilkdropPreset {

CustomShape::CustomShape(PresetState& presetState)
    : m_outlineMesh(Renderer::VertexBufferUsage::StreamDraw)
    , m_fillMesh(Renderer::VertexBufferUsage::StreamDraw, true, true)
    , m_presetState(presetState)
    , m_perFrameContext(presetState.globalMemory, &presetState.globalRegisters)
{
    m_outlineMesh.SetRenderPrimitiveType(Renderer::Mesh::PrimitiveType::LineLoop);
    m_fillMesh.SetRenderPrimitiveType(Renderer::Mesh::PrimitiveType::Triangles);

    m_perFrameContext.RegisterBuiltinVariables();
}

void CustomShape::Initialize(PresetFileParser& parsedFile, int index)
{
    std::string const shapecodePrefix = "shapecode_" + std::to_string(index) + "_";

    m_index = index;
    m_enabled = parsedFile.GetBool(shapecodePrefix + "enabled", m_enabled);
    m_sides = parsedFile.GetInt(shapecodePrefix + "sides", m_sides);
    m_additive = parsedFile.GetBool(shapecodePrefix + "additive", m_additive);
    m_thickOutline = parsedFile.GetBool(shapecodePrefix + "thickOutline", m_thickOutline);
    m_textured = parsedFile.GetBool(shapecodePrefix + "textured", m_textured);
    m_instances = parsedFile.GetInt(shapecodePrefix + "num_inst", m_instances);
    m_x = parsedFile.GetFloat(shapecodePrefix + "x", m_x);
    m_y = parsedFile.GetFloat(shapecodePrefix + "y", m_y);
    m_radius = parsedFile.GetFloat(shapecodePrefix + "rad", m_radius);
    m_angle = parsedFile.GetFloat(shapecodePrefix + "ang", m_angle);
    m_tex_ang = parsedFile.GetFloat(shapecodePrefix + "tex_ang", m_tex_ang);
    m_tex_zoom = parsedFile.GetFloat(shapecodePrefix + "tex_zoom", m_tex_zoom);
    m_r = parsedFile.GetFloat(shapecodePrefix + "r", m_r);
    m_g = parsedFile.GetFloat(shapecodePrefix + "g", m_g);
    m_b = parsedFile.GetFloat(shapecodePrefix + "b", m_b);
    m_a = parsedFile.GetFloat(shapecodePrefix + "a", m_a);
    m_r2 = parsedFile.GetFloat(shapecodePrefix + "r2", m_r2);
    m_g2 = parsedFile.GetFloat(shapecodePrefix + "g2", m_g2);
    m_b2 = parsedFile.GetFloat(shapecodePrefix + "b2", m_b2);
    m_a2 = parsedFile.GetFloat(shapecodePrefix + "a2", m_a2);
    m_border_r = parsedFile.GetFloat(shapecodePrefix + "border_r", m_border_r);
    m_border_g = parsedFile.GetFloat(shapecodePrefix + "border_g", m_border_g);
    m_border_b = parsedFile.GetFloat(shapecodePrefix + "border_b", m_border_b);
    m_border_a = parsedFile.GetFloat(shapecodePrefix + "border_a", m_border_a);

    // projectM addition: texture name to use for rendering the shape
    m_image = parsedFile.GetString(shapecodePrefix + "image", "");
}

void CustomShape::CompileCodeAndRunInitExpressions()
{
    m_perFrameContext.LoadStateVariables(m_presetState, *this, 0);
    m_perFrameContext.EvaluateInitCode(m_presetState.customShapeInitCode[m_index], *this);

    for (int t = 0; t < TVarCount; t++)
    {
        m_tValuesAfterInitCode[t] = *m_perFrameContext.t_vars[t];
    }

    m_perFrameContext.CompilePerFrameCode(m_presetState.customShapePerFrameCode[m_index], *this);
}

void CustomShape::Draw()
{
    static constexpr float pi = 3.141592653589793f;

    if (!m_enabled)
    {
        return;
    }

    // Updating a buffer which is still in use by a previous draw call can stall the GPU pipeline, so
    // first evaluate all instances and collect their geometry, then upload it once and draw the
    // instances in order from the shared buffers.
    auto& fillVertices = m_fillMesh.Vertices().Get();
    auto& fillColors = m_fillMesh.Colors().Get();
    auto& fillUVs = m_fillMesh.UVs().Get();
    auto& fillIndices = m_fillMesh.Indices().Get();
    auto& outlineVertices = m_outlineMesh.Vertices().Get();

    fillVertices.clear();
    fillColors.clear();
    fillUVs.clear();
    fillIndices.clear();
    outlineVertices.clear();
    m_instanceDrawInfo.clear();

    // Textured shape, either main texture or texture from "image" key
    Renderer::TextureSamplerDescriptor imageTexture;
    if (!m_image.empty())
    {
        imageTexture = m_presetState.renderContext.textureManager->GetTexture(m_image);
    }
    const auto textureAspectY = imageTexture.Empty() ? m_presetState.renderContext.aspectY : 1.0f;

    // Need to use +/- 1.0 here instead of 2.0 used in Milkdrop to achieve the same rendering result.
    const auto incrementX = 1.0f / static_cast<float>(m_presetState.renderContext.viewportSizeX);
    const auto incrementY = 1.0f / static_cast<float>(m_presetState.renderContext.viewportSizeY);

    for (int instance = 0; instance < m_instances; instance++)
    {
        m_perFrameContext.LoadStateVariables(m_presetState, *this, instance);
        m_perFrameContext.ExecutePerFrameCode();

        int sides = static_cast<int>(*m_perFrameContext.sides);
        if (sides < 3)
        {
            sides = 3;
        }
        if (sides > 100)
        {
            sides = 100;
        }

        InstanceDrawInfo drawInfo;
        drawInfo.additive = static_cast<int>(*m_perFrameContext.additive) != 0;
        drawInfo.textured = static_cast<int>(*m_perFrameContext.textured) != 0;

        const auto firstVertex = static_cast<uint32_t>(fillVertices.size());

        const Renderer::Point center(static_cast<float>(*m_perFrameContext.x * 2.0 - 1.0),
                                     static_cast<float>(*m_perFrameContext.y * -2.0 + 1.0));

        // x = f*255.0 & 0xFF = (f*255.0) % 256
        // f' = x/255.0 = f % (256/255)
        // 1.0 -> 255 (0xFF)
        // 2.0 -> 254 (0xFE)
        // -1.0 -> 0x01

        const auto centerColor = Renderer::Color::Modulo(Renderer::Color(static_cast<float>(*m_perFrameContext.r),
                                                                         static_cast<float>(*m_perFrameContext.g),
                                                                         static_cast<float>(*m_perFrameContext.b),
                                                                         static_cast<float>(*m_perFrameContext.a)));

        const auto edgeColor = Renderer::Color::Modulo(Renderer::Color(static_cast<float>(*m_perFrameContext.r2),
                                                                       static_cast<float>(*m_perFrameContext.g2),
                                                                       static_cast<float>(*m_perFrameContext.b2),
                                                                       static_cast<float>(*m_perFrameContext.a2)));

        fillVertices.push_back(center);
        fillColors.push_back(centerColor);
        fillUVs.emplace_back(0.5f, 0.5f);

        for (int i = 1; i < sides + 1; i++)
        {
            const float cornerProgress = static_cast<float>(i - 1) / static_cast<float>(sides);
            const float angle = cornerProgress * pi * 2.0f + static_cast<float>(*m_perFrameContext.ang) + pi * 0.25f;

            // Todo: There's still some issue with aspect ratio here, as everything gets squashed horizontally if Y > x.
            fillVertices.emplace_back(center.X() + static_cast<float>(*m_perFrameContext.rad) * cosf(angle) * m_presetState.renderContext.aspectY,
                                      center.Y() + static_cast<float>(*m_perFrameContext.rad) * sinf(angle));
            fillColors.push_back(edgeColor);

            if (drawInfo.textured)
            {
                const float textureAngle = cornerProgress * pi * 2.0f + static_cast<float>(*m_perFrameContext.tex_ang) + pi * 0.25f;

                fillUVs.emplace_back(0.5f + 0.5f * cosf(textureAngle) / static_cast<float>(*m_perFrameContext.tex_zoom) * textureAspectY,
                                     1.0f - (0.5f - 0.5f * sinf(textureAngle) / static_cast<float>(*m_perFrameContext.tex_zoom))); // Vertical flip required!
            }
            else
            {
                fillUVs.emplace_back(0.0f, 0.0f);
            }
        }

        // Duplicate first edge vertex to close the fan.
        fillVertices.push_back(fillVertices[firstVertex + 1]);
        fillColors.push_back(fillColors[firstVertex + 1]);
        fillUVs.push_back(fillUVs[firstVertex + 1]);

        // Store the triangle fan as a triangle list, so consecutive instances can be drawn in one call.
        drawInfo.fillFirstIndex = static_cast<uint32_t>(fillIndices.size());
        for (int i = 1; i < sides + 1; i++)
        {
            fillIndices.push_back(firstVertex);
            fillIndices.push_back(firstVertex + i);
            fillIndices.push_back(firstVertex + i + 1);
        }
        drawInfo.fillIndexCount = static_cast<uint32_t>(fillIndices.size()) - drawInfo.fillFirstIndex;

        if (*m_perFrameContext.border_a > 0.0001f)
        {
            drawInfo.borderColor = Renderer::Color(static_cast<float>(*m_perFrameContext.border_r),
                                                   static_cast<float>(*m_perFrameContext.border_g),
                                                   static_cast<float>(*m_perFrameContext.border_b),
                                                   static_cast<float>(*m_perFrameContext.border_a));
            drawInfo.outlineFirstVertex = static_cast<uint32_t>(outlineVertices.size());
            drawInfo.outlineVertexCount = static_cast<uint32_t>(sides);
            drawInfo.outlineIterations = m_thickOutline ? 4 : 1;

            // If thick outline is used, draw the shape four times with slight offsets
            // (top left, top right, bottom right, bottom left).
            const std::array<Renderer::Point, 4> offsets{{{0.0f, 0.0f},
                                                          {incrementX, 0.0f},
                                                          {incrementX, incrementY},
                                                          {0.0f, incrementY}}};

            for (uint32_t iteration = 0; iteration < drawInfo.outlineIterations; iteration++)
            {
                for (int i = 0; i < sides; i++)
                {
                    const auto& point = fillVertices[firstVertex + 1 + i];
                    outlineVertices.emplace_back(point.X() + offsets[iteration].X(), point.Y() + offsets[iteration].Y());
                }
            }
        }

        m_instanceDrawInfo.push_back(drawInfo);
    }

    // Outline indices are continuous, only upload them if the size changed.
    if (m_outlineMesh.Indices().Size() != outlineVertices.size())
    {
        m_outlineMesh.Indices().Resize(outlineVertices.size());
        m_outlineMesh.Indices().MakeContinuous();
    }

    m_fillMesh.Update();
    m_outlineMesh.Update();

    Renderer::BlendMode::SetBlendActive(true);

    for (size_t instance = 0; instance < m_instanceDrawInfo.size(); instance++)
    {
        const auto& drawInfo = m_instanceDrawInfo[instance];

        // Merge the fills of following instances using the same state, as long as there's no outline in between.
        auto fillIndexCount = drawInfo.fillIndexCount;
        while (drawInfo.outlineIterations == 0 &&
               instance + 1 < m_instanceDrawInfo.size() &&
               m_instanceDrawInfo[instance + 1].additive == drawInfo.additive &&
               m_instanceDrawInfo[instance + 1].textured == drawInfo.textured)
        {
            instance++;
            fillIndexCount += m_instanceDrawInfo[instance].fillIndexCount;
            if (m_instanceDrawInfo[instance].outlineIterations > 0)
            {
                break;
            }
        }
        const auto& lastDrawInfo = m_instanceDrawInfo[instance];

        // Additive Drawing or Overwrite
        Renderer::BlendMode::SetBlendFunction(Renderer::BlendMode::Function::SourceAlpha,
                                              drawInfo.additive
                                                  ? Renderer::BlendMode::Function::One
                                                  : Renderer::BlendMode::Function::OneMinusSourceAlpha);

        if (drawInfo.textured)
        {
            auto shader = m_presetState.texturedShader.lock();
            shader->Bind();
            shader->SetUniformMat4x4("vertex_transformation", PresetState::orthogonalProjection);
            shader->SetUniformInt("texture_sampler", 0);

            if (!imageTexture.Empty())
            {
                imageTexture.Bind(0, *shader);
            }
            else
            {
                assert(!m_presetState.mainTexture.expired());
                m_presetState.mainTexture.lock()->Bind(0);
            }

            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        }
        else
        {
            // Untextured (creates a color gradient: center=r/g/b/a to border=r2/b2/g2/a2)
            auto shader = m_presetState.untexturedShader.lock();
            shader->Bind();
            shader->SetUniformMat4x4("vertex_transformation", PresetState::orthogonalProjection);
        }

        m_fillMesh.Draw(drawInfo.fillFirstIndex, fillIndexCount);

        glBindTexture(GL_TEXTURE_2D, 0);
        Renderer::Sampler::Unbind(0);

        if (lastDrawInfo.outlineIterations > 0)
        {
            auto shader = m_presetState.untexturedShader.lock();
            shader->Bind();
            shader->SetUniformMat4x4("vertex_transformation", PresetState::orthogonalProjection);
            shader->SetUniformFloat("vertex_point_size", 1.0f);

            glVertexAttrib4f(1,
                             lastDrawInfo.borderColor.R(),
                             lastDrawInfo.borderColor.G(),
                             lastDrawInfo.borderColor.B(),
                             lastDrawInfo.borderColor.A());
            glLineWidth(1);
#ifndef USE_GLES
            glEnable(GL_LINE_SMOOTH);
#endif

            for (uint32_t iteration = 0; iteration < lastDrawInfo.outlineIterations; iteration++)
            {
                m_outlineMesh.Draw(lastDrawInfo.outlineFirstVertex + iteration * lastDrawInfo.outlineVertexCount,
                                   lastDrawInfo.outlineVertexCount);
            }
        }
    }

    Renderer::Mesh::Unbind();
    Renderer::Shader::Unbind();

#ifndef USE_GLES
    glDisable(GL_LINE_SMOOTH);
#endif
    Renderer::BlendMode::SetBlendActive(false);
}

} // namespace MilkdropPreset
} // namespace libprojectM
