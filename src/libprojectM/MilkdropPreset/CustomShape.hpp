#pragma once

#include "Constants.hpp"
#include "PresetState.hpp"
#include "ShapePerFrameContext.hpp"

#include <Renderer/Mesh.hpp>

#include <projectm-eval.h>

#include <vector>

namespace libprojectM {
namespace MilkdropPreset {

class PresetFileParser;

/**
 * @brief Renders a custom shape with or without a texture.
 *
 * The geometry of all instances is collected into one fill and one outline mesh, uploaded once per frame and then
 * drawn instance by instance, merging consecutive fills with the same render state into a single draw call.
 */
class CustomShape
{
public:
    CustomShape(PresetState& presetState);

    virtual ~CustomShape() = default;

    /**
     * @brief Loads the initial values and code from the preset file.
     * @param parsedFile The file parser with the preset data.
     * @param index The waveform index.
     */
    void Initialize(PresetFileParser& parsedFile, int index);

    /**
     * @brief Compiles all code blocks and runs the init expression.
     * @throws MilkdropCompileException Thrown if one of the code blocks couldn't be compiled.
     */
    void CompileCodeAndRunInitExpressions();

    /**
     * @brief Renders the shape.
     */
    void Draw();

private:
    /**
     * @brief Draw parameters of a single shape instance, referencing the fill and outline mesh buffers.
     */
    struct InstanceDrawInfo {
        bool additive{false};           //!< If true, the instance is drawn additively.
        bool textured{false};           //!< If true, the fill is drawn with a texture.
        uint32_t fillFirstIndex{};      //!< First fill mesh index of this instance.
        uint32_t fillIndexCount{};      //!< Number of fill mesh indices of this instance.
        Renderer::Color borderColor;    //!< The outline color.
        uint32_t outlineFirstVertex{};  //!< First outline mesh vertex of this instance.
        uint32_t outlineVertexCount{};  //!< Number of outline vertices per iteration.
        uint32_t outlineIterations{};   //!< Number of times the outline is drawn, 0 if there's no outline.
    };

    Renderer::Mesh m_outlineMesh; //!< The border/outline mesh for all shape instances.
    Renderer::Mesh m_fillMesh; //!< The color/texture mesh for all shape instances.
    std::vector<InstanceDrawInfo> m_instanceDrawInfo; //!< Draw parameters of each instance in the current frame.

    std::string m_image; //!< Texture filename to be rendered on this shape.

    int m_index{0};        //!< The custom shape index in the preset.
    bool m_enabled{false};      //!< If false, the shape isn't drawn.
    int m_sides{4};        //!< Number of sides (vertices)
    bool m_additive{false};     //!< Flag that specifies whether the shape should be drawn additive.
    bool m_thickOutline{false}; //!< If true, the shape is rendered with a thick line, otherwise a single-pixel line.
    bool m_textured{false};     //!< If true, the shape will be rendered with the given texture.
    int m_instances{1};    //!< Number of shape instances to render

    float m_x{0.5f};      //!< The shape x position.
    float m_y{0.5f};      //!< The shape y position.
    float m_radius{0.1f}; //!< The shape radius (1.0 fills the whole screen).
    float m_angle{0.0f};  //!< The shape rotation.

    float m_r{1.0f}; ///!< Red color value.
    float m_g{0.0f}; ///!< Green color value.
    float m_b{0.0f}; ///!< Blue color value.
    float m_a{1.0f}; ///!< Alpha color value.

    float m_r2{0.0f}; ///!< Second red color value.
    float m_g2{1.0f}; ///!< Second green color value.
    float m_b2{0.0f}; ///!< Second blue color value.
    float m_a2{0.0f}; ///!< Second alpha color value.

    float m_border_r{1.0f}; //!< Red color value.
    float m_border_g{1.0f}; //!< Green color value.
    float m_border_b{1.0f}; //!< Blue color value.
    float m_border_a{0.0f}; //!< Alpha color value

    float m_tex_ang{0.0f};  //!< Texture rotation angle.
    float m_tex_zoom{1.0f}; //!< Texture zoom value.

    std::string m_initCode;     //!< Init expression code, run once on preset load.
    std::string m_perFrameCode; //!< Per-frame expression code, run once per frame and instance.

    PRJM_EVAL_F m_tValuesAfterInitCode[TVarCount]{};

    PresetState& m_presetState; //!< The global preset state.
    ShapePerFrameContext m_perFrameContext;

    friend class ShapePerFrameContext;
};

} // namespace MilkdropPreset
} // namespace libprojectM
