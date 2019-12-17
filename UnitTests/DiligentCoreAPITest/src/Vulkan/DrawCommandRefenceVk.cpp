/*     Copyright 2019 Diligent Graphics LLC
 *  
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 * 
 *     http://www.apache.org/licenses/LICENSE-2.0
 * 
 *  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 *  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 *  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT OF ANY PROPRIETARY RIGHTS.
 *
 *  In no event and under no legal theory, whether in tort (including negligence), 
 *  contract, or otherwise, unless required by applicable law (such as deliberate 
 *  and grossly negligent acts) or agreed to in writing, shall any Contributor be
 *  liable for any damages, including any direct, indirect, special, incidental, 
 *  or consequential damages of any character arising as a result of this License or 
 *  out of the use or inability to use the software (including but not limited to damages 
 *  for loss of goodwill, work stoppage, computer failure or malfunction, or any and 
 *  all other commercial damages or losses), even if such Contributor has been advised 
 *  of the possibility of such damages.
 */

#include "Vulkan/TestingEnvironmentVk.h"
#include "Vulkan/TestingSwapChainVk.h"

#include "DeviceContextVk.h"

namespace Diligent
{

namespace Testing
{

static const char* VSSource = R"(
#version 430 core

#ifndef GL_ES
out gl_PerVertex
{
	vec4 gl_Position;
};
#endif

layout(location = 0) out vec3 out_Color;

void main()
{
    vec4 Pos[4];
    Pos[0] = vec4(-0.5, -0.5, 0.0, 1.0);
    Pos[1] = vec4(-0.5, +0.5, 0.0, 1.0);
    Pos[2] = vec4(+0.5, -0.5, 0.0, 1.0);
    Pos[3] = vec4(+0.5, +0.5, 0.0, 1.0);

    vec3 Col[4];
    Col[0] = vec3(1.0, 0.0, 0.0);
    Col[1] = vec3(0.0, 1.0, 0.0);
    Col[2] = vec3(0.0, 0.0, 1.0);
    Col[3] = vec3(1.0, 1.0, 1.0);
    
    gl_Position = Pos[gl_VertexIndex];
    out_Color = Col[gl_VertexIndex];
}
)";

static const char* PSSource = R"(
#version 430 core

layout(location = 0) in  vec3 in_Color;
layout(location = 0) out vec4 out_Color;

void main()
{
    out_Color = vec4(in_Color, 1.0);
}
)";

void RenderDrawCommandRefenceVk(ISwapChain* pSwapChain)
{
    auto* pEnv     = TestingEnvironmentVk::GetInstance();
    auto  vkDevice = pEnv->GetVkDevice();
    auto* pContext = pEnv->GetDeviceContext();

    const auto& SCDesc = pSwapChain->GetDesc();

    VkResult res = VK_SUCCESS;
    (void)res;

    auto* pTestingSwapChainVk = ValidatedCast<TestingSwapChainVk>(pSwapChain);

    auto vkVSModule = pEnv->CreateShaderModule(SHADER_TYPE_VERTEX, VSSource, static_cast<int>(strlen(VSSource)));
    ASSERT_TRUE(vkVSModule != VK_NULL_HANDLE);
    auto vkPSModule = pEnv->CreateShaderModule(SHADER_TYPE_PIXEL, PSSource, static_cast<int>(strlen(PSSource)));
    ASSERT_TRUE(vkPSModule != VK_NULL_HANDLE);

    VkGraphicsPipelineCreateInfo PipelineCI = {};

    PipelineCI.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    PipelineCI.pNext = nullptr;

    VkPipelineShaderStageCreateInfo ShaderStages[2] = {};

    ShaderStages[0].sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    ShaderStages[0].stage  = VK_SHADER_STAGE_VERTEX_BIT;
    ShaderStages[0].module = vkVSModule;
    ShaderStages[0].pName  = "main";

    ShaderStages[1].sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    ShaderStages[1].stage  = VK_SHADER_STAGE_FRAGMENT_BIT;
    ShaderStages[1].module = vkPSModule;
    ShaderStages[1].pName  = "main";

    PipelineCI.pStages    = ShaderStages;
    PipelineCI.stageCount = _countof(ShaderStages);

    VkPipelineLayoutCreateInfo PipelineLayoutCI = {};

    PipelineLayoutCI.sType    = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    VkPipelineLayout vkLayout = VK_NULL_HANDLE;
    vkCreatePipelineLayout(vkDevice, &PipelineLayoutCI, nullptr, &vkLayout);
    ASSERT_TRUE(vkLayout != VK_NULL_HANDLE);
    PipelineCI.layout = vkLayout;

    VkPipelineVertexInputStateCreateInfo VertexInputStateCI = {};

    VertexInputStateCI.sType     = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    PipelineCI.pVertexInputState = &VertexInputStateCI;


    VkPipelineInputAssemblyStateCreateInfo InputAssemblyCI = {};

    InputAssemblyCI.sType                  = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    InputAssemblyCI.pNext                  = nullptr;
    InputAssemblyCI.flags                  = 0; // reserved for future use
    InputAssemblyCI.topology               = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
    InputAssemblyCI.primitiveRestartEnable = VK_FALSE;
    PipelineCI.pInputAssemblyState         = &InputAssemblyCI;


    VkPipelineTessellationStateCreateInfo TessStateCI = {};

    TessStateCI.sType             = VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_STATE_CREATE_INFO;
    TessStateCI.pNext             = nullptr;
    TessStateCI.flags             = 0; // reserved for future use
    PipelineCI.pTessellationState = &TessStateCI;


    VkPipelineViewportStateCreateInfo ViewPortStateCI = {};

    ViewPortStateCI.sType         = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    ViewPortStateCI.pNext         = nullptr;
    ViewPortStateCI.flags         = 0; // reserved for future use
    ViewPortStateCI.viewportCount = 1;

    VkViewport Viewport = {};
    Viewport.y          = static_cast<float>(SCDesc.Height);
    Viewport.width      = static_cast<float>(SCDesc.Width);
    Viewport.height     = -static_cast<float>(SCDesc.Height);
    Viewport.maxDepth   = 1;

    ViewPortStateCI.pViewports   = &Viewport;
    ViewPortStateCI.scissorCount = ViewPortStateCI.viewportCount; // the number of scissors must match the number of viewports (23.5)
                                                                  // (why the hell it is in the struct then?)

    VkRect2D ScissorRect      = {};
    ScissorRect.extent.width  = SCDesc.Width;
    ScissorRect.extent.height = SCDesc.Height;
    ViewPortStateCI.pScissors = &ScissorRect;

    PipelineCI.pViewportState = &ViewPortStateCI;

    VkPipelineRasterizationStateCreateInfo RasterizerStateCI = {};

    RasterizerStateCI.sType       = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    RasterizerStateCI.polygonMode = VK_POLYGON_MODE_FILL;
    RasterizerStateCI.cullMode    = VK_CULL_MODE_NONE;
    RasterizerStateCI.lineWidth   = 1;

    PipelineCI.pRasterizationState = &RasterizerStateCI;

    // Multisample state (24)
    VkPipelineMultisampleStateCreateInfo MSStateCI = {};

    MSStateCI.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    MSStateCI.pNext = nullptr;
    MSStateCI.flags = 0; // reserved for future use
    // If subpass uses color and/or depth/stencil attachments, then the rasterizationSamples member of
    // pMultisampleState must be the same as the sample count for those subpass attachments
    MSStateCI.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    MSStateCI.sampleShadingEnable  = VK_FALSE;
    MSStateCI.minSampleShading     = 0;               // a minimum fraction of sample shading if sampleShadingEnable is set to VK_TRUE.
    uint32_t SampleMask[]          = {0xFFFFFFFF, 0}; // Vulkan spec allows up to 64 samples
    MSStateCI.pSampleMask          = SampleMask;      // an array of static coverage information that is ANDed with
                                                      // the coverage information generated during rasterization (25.3)
    MSStateCI.alphaToCoverageEnable = VK_FALSE;       // whether a temporary coverage value is generated based on
                                                      // the alpha component of the fragment's first color output
    MSStateCI.alphaToOneEnable   = VK_FALSE;          // whether the alpha component of the fragment's first color output is replaced with one
    PipelineCI.pMultisampleState = &MSStateCI;


    VkPipelineDepthStencilStateCreateInfo DepthStencilStateCI = {};

    DepthStencilStateCI.sType     = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    PipelineCI.pDepthStencilState = &DepthStencilStateCI;


    VkPipelineColorBlendStateCreateInfo BlendStateCI = {};

    VkPipelineColorBlendAttachmentState Attachment = {};

    Attachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

    BlendStateCI.sType           = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    BlendStateCI.pAttachments    = &Attachment;
    BlendStateCI.attachmentCount = 1; //  must equal the colorAttachmentCount for the subpass
                                      // in which this pipeline is used.
    PipelineCI.pColorBlendState = &BlendStateCI;


    VkPipelineDynamicStateCreateInfo DynamicStateCI = {};

    DynamicStateCI.sType     = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    PipelineCI.pDynamicState = &DynamicStateCI;

    PipelineCI.renderPass         = pTestingSwapChainVk->GetRenderPass();
    PipelineCI.subpass            = 0;
    PipelineCI.basePipelineHandle = VK_NULL_HANDLE; // a pipeline to derive from
    PipelineCI.basePipelineIndex  = 0;              // an index into the pCreateInfos parameter to use as a pipeline to derive from

    VkPipeline vkPipeline = VK_NULL_HANDLE;
    res                   = vkCreateGraphicsPipelines(vkDevice, VK_NULL_HANDLE, 1, &PipelineCI, nullptr, &vkPipeline);
    ASSERT_GE(res, 0);
    ASSERT_TRUE(vkPipeline != VK_NULL_HANDLE);

    VkCommandBuffer vkCmdBuffer = pEnv->AllocateCommandBuffer();

    pTestingSwapChainVk->BeginRenderPass(vkCmdBuffer, VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
    vkCmdBindPipeline(vkCmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vkPipeline);
    vkCmdDraw(vkCmdBuffer, 4, 1, 0, 0);
    pTestingSwapChainVk->EndRenderPass(vkCmdBuffer);
    res = vkEndCommandBuffer(vkCmdBuffer);
    VERIFY(res >= 0, "Failed to end command buffer");

    RefCntAutoPtr<IDeviceContextVk> pContextVk{pContext, IID_DeviceContextVk};

    auto* pQeueVk = pContextVk->LockCommandQueue();
    auto  vkQueue = pQeueVk->GetVkQueue();

    VkSubmitInfo SubmitInfo       = {};
    SubmitInfo.sType              = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    SubmitInfo.pCommandBuffers    = &vkCmdBuffer;
    SubmitInfo.commandBufferCount = 1;
    vkQueueSubmit(vkQueue, 1, &SubmitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(vkQueue);

    pContextVk->UnlockCommandQueue();

    vkDestroyPipeline(vkDevice, vkPipeline, nullptr);
    vkDestroyPipelineLayout(vkDevice, vkLayout, nullptr);
    vkDestroyShaderModule(vkDevice, vkVSModule, nullptr);
    vkDestroyShaderModule(vkDevice, vkPSModule, nullptr);
}

} // namespace Testing

} // namespace Diligent