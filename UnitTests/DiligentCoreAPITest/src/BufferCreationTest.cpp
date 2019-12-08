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

#if D3D11_SUPPORTED
#    include "CreateObjFromNativeResD3D11.h"
#endif

#if D3D12_SUPPORTED
#    include "CreateObjFromNativeResD3D12.h"
#endif

#if GL_SUPPORTED || GLES_SUPPORTED
#    include "CreateObjFromNativeResGL.h"
#endif

#if VULKAN_SUPPORTED
#    include "CreateObjFromNativeResVK.h"
#endif

#include "TestingEnvironment.h"

#include "gtest/gtest.h"

using namespace Diligent;

namespace
{

class BufferCreationTest : public ::testing::Test
{
protected:
    static void SetUpTestSuite()
    {
        auto* pEnv    = TestingEnvironment::GetInstance();
        auto* pDevice = pEnv->GetDevice();

        const auto DevCaps = pDevice->GetDeviceCaps();
        switch (DevCaps.DevType)
        {
#if D3D11_SUPPORTED
            case DeviceType::D3D11:
                pCreateObjFromNativeRes.reset(new TestCreateObjFromNativeResD3D11(pDevice));
                break;

#endif

#if D3D12_SUPPORTED
            case DeviceType::D3D12:
                pCreateObjFromNativeRes.reset(new TestCreateObjFromNativeResD3D12(pDevice));
                break;
#endif

#if GL_SUPPORTED || GLES_SUPPORTED
            case DeviceType::OpenGL:
            case DeviceType::OpenGLES:
                pCreateObjFromNativeRes.reset(new TestCreateObjFromNativeResGL(pDevice));
                break;
#endif

#if VULKAN_SUPPORTED
            case DeviceType::Vulkan:
                pCreateObjFromNativeRes.reset(new TestCreateObjFromNativeResVK(pDevice));
                break;
#endif

            default: UNEXPECTED("Unexpected device type");
        }
    }

    static void TearDownTestSuite()
    {
        pCreateObjFromNativeRes.reset();
        TestingEnvironment::GetInstance()->Reset();
    }

    static std::unique_ptr<CreateObjFromNativeResTestBase> pCreateObjFromNativeRes;
};

std::unique_ptr<CreateObjFromNativeResTestBase> BufferCreationTest::pCreateObjFromNativeRes;

TEST_F(BufferCreationTest, CreateVertexBuffer)
{
    auto* pEnv    = TestingEnvironment::GetInstance();
    auto* pDevice = pEnv->GetDevice();

    BufferDesc BuffDesc;
    BuffDesc.Name          = "Vertex buffer";
    BuffDesc.uiSizeInBytes = 256;
    BuffDesc.BindFlags     = BIND_VERTEX_BUFFER;

    BufferData InitData;
    InitData.DataSize = BuffDesc.uiSizeInBytes;
    std::vector<Uint8> DummyData(InitData.DataSize);
    InitData.pData = DummyData.data();
    RefCntAutoPtr<IBuffer> pBuffer;
    pDevice->CreateBuffer(BuffDesc, &InitData, &pBuffer);
    ASSERT_NE(pBuffer, nullptr);

    pCreateObjFromNativeRes->CreateBuffer(pBuffer);
}

TEST_F(BufferCreationTest, CreateIndexBuffer)
{
    auto* pEnv    = TestingEnvironment::GetInstance();
    auto* pDevice = pEnv->GetDevice();

    BufferDesc BuffDesc;
    BuffDesc.Name          = "Index";
    BuffDesc.uiSizeInBytes = 256;
    BuffDesc.BindFlags     = BIND_VERTEX_BUFFER;

    BufferData NullData;

    RefCntAutoPtr<IBuffer> pBuffer;
    pDevice->CreateBuffer(BuffDesc, &NullData, &pBuffer);
    ASSERT_NE(pBuffer, nullptr);

    pCreateObjFromNativeRes->CreateBuffer(pBuffer);
}

TEST_F(BufferCreationTest, CreateFormattedBuffer)
{
    auto* pEnv    = TestingEnvironment::GetInstance();
    auto* pDevice = pEnv->GetDevice();

    const auto& DevCaps = pDevice->GetDeviceCaps();
    if (!(DevCaps.bComputeShadersSupported && DevCaps.bIndirectRenderingSupported))
    {
        GTEST_SKIP();
    }

    BufferDesc BuffDesc;
    BuffDesc.Name              = "Formatted buffer";
    BuffDesc.uiSizeInBytes     = 256;
    BuffDesc.BindFlags         = BIND_UNORDERED_ACCESS | BIND_SHADER_RESOURCE;
    BuffDesc.Mode              = BUFFER_MODE_FORMATTED;
    BuffDesc.ElementByteStride = 16;

    RefCntAutoPtr<IBuffer> pBuffer;
    pDevice->CreateBuffer(BuffDesc, nullptr, &pBuffer);
    ASSERT_NE(pBuffer, nullptr);

    BufferViewDesc ViewDesc;
    ViewDesc.ViewType             = BUFFER_VIEW_SHADER_RESOURCE;
    ViewDesc.ByteOffset           = 32;
    ViewDesc.Format.NumComponents = 4;
    ViewDesc.Format.ValueType     = VT_FLOAT32;
    ViewDesc.Format.IsNormalized  = false;
    RefCntAutoPtr<IBufferView> pBufferSRV;
    pBuffer->CreateView(ViewDesc, &pBufferSRV);
    EXPECT_NE(pBufferSRV, nullptr);

    ViewDesc.ViewType = BUFFER_VIEW_UNORDERED_ACCESS;
    RefCntAutoPtr<IBufferView> pBufferUAV;
    pBuffer->CreateView(ViewDesc, &pBufferUAV);
    EXPECT_NE(pBufferUAV, nullptr);

    pCreateObjFromNativeRes->CreateBuffer(pBuffer);
}

TEST_F(BufferCreationTest, CreateStructuedBuffer)
{
    auto* pEnv    = TestingEnvironment::GetInstance();
    auto* pDevice = pEnv->GetDevice();

    const auto& DevCaps = pDevice->GetDeviceCaps();
    if (!DevCaps.bComputeShadersSupported)
    {
        GTEST_SKIP();
    }

    BufferDesc BuffDesc;
    BuffDesc.Name              = "Structured buffer";
    BuffDesc.uiSizeInBytes     = 256;
    BuffDesc.BindFlags         = BIND_SHADER_RESOURCE | BIND_UNORDERED_ACCESS;
    BuffDesc.Mode              = BUFFER_MODE_STRUCTURED;
    BuffDesc.ElementByteStride = 16;
    RefCntAutoPtr<IBuffer> pBuffer;
    pDevice->CreateBuffer(BuffDesc, nullptr, &pBuffer);
    ASSERT_NE(pBuffer, nullptr);

    pCreateObjFromNativeRes->CreateBuffer(pBuffer);
}

TEST_F(BufferCreationTest, CreateUniformBuffer)
{
    auto* pEnv    = TestingEnvironment::GetInstance();
    auto* pDevice = pEnv->GetDevice();

    BufferDesc BuffDesc;
    BuffDesc.Name          = "Uniform buffer";
    BuffDesc.uiSizeInBytes = 256;
    BuffDesc.BindFlags     = BIND_UNIFORM_BUFFER;
    RefCntAutoPtr<IBuffer> pBuffer;
    pDevice->CreateBuffer(BuffDesc, nullptr, &pBuffer);
    ASSERT_NE(pBuffer, nullptr);

    pCreateObjFromNativeRes->CreateBuffer(pBuffer);
}

TEST_F(BufferCreationTest, CreateRawBuffer)
{
    auto* pEnv    = TestingEnvironment::GetInstance();
    auto* pDevice = pEnv->GetDevice();

    BufferDesc BuffDesc;
    BuffDesc.Name              = "Raw buffer";
    BuffDesc.uiSizeInBytes     = 256;
    BuffDesc.BindFlags         = BIND_VERTEX_BUFFER | BIND_INDEX_BUFFER | BIND_UNORDERED_ACCESS | BIND_SHADER_RESOURCE;
    BuffDesc.Mode              = BUFFER_MODE_RAW;
    BuffDesc.ElementByteStride = 16;
    RefCntAutoPtr<IBuffer> pBuffer;
    pDevice->CreateBuffer(BuffDesc, nullptr, &pBuffer);
    ASSERT_NE(pBuffer, nullptr);

    BufferViewDesc ViewDesc;
    ViewDesc.ViewType             = BUFFER_VIEW_UNORDERED_ACCESS;
    ViewDesc.ByteOffset           = 32;
    ViewDesc.Format.NumComponents = 4;
    ViewDesc.Format.ValueType     = VT_FLOAT32;
    RefCntAutoPtr<IBufferView> pBufferUAV;
    pBuffer->CreateView(ViewDesc, &pBufferUAV);
    ASSERT_NE(pBufferUAV, nullptr);

    ViewDesc.ViewType = BUFFER_VIEW_SHADER_RESOURCE;
    RefCntAutoPtr<IBufferView> pBufferSRV;
    pBuffer->CreateView(ViewDesc, &pBufferSRV);
    ASSERT_NE(pBufferSRV, nullptr);

    pCreateObjFromNativeRes->CreateBuffer(pBuffer);
}

} // namespace