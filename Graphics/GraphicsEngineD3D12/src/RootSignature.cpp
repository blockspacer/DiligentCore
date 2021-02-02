/*
 *  Copyright 2019-2021 Diligent Graphics LLC
 *  Copyright 2015-2019 Egor Yusov
 *  
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *  
 *      http://www.apache.org/licenses/LICENSE-2.0
 *  
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
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

#include "pch.h"

#include "RootSignature.hpp"
#include "CommandContext.hpp"
#include "RenderDeviceD3D12Impl.hpp"
#include "TextureD3D12Impl.hpp"
#include "TopLevelASD3D12Impl.hpp"
#include "D3D12TypeConversions.hpp"
#include "HashUtils.hpp"

namespace Diligent
{

RootSignature::RootSignature()
{
}

void RootSignature::Create(RenderDeviceD3D12Impl* pDeviceD3D12Impl, PIPELINE_TYPE PipelineType, IPipelineResourceSignature** ppSignatures, Uint32 SignatureCount)
{
    VERIFY(m_SignatureCount == 0 && m_pd3d12RootSignature == nullptr,
           "This root signature is already initialized");

    for (Uint32 i = 0; i < SignatureCount; ++i)
    {
        auto* pSignature = ValidatedCast<PipelineResourceSignatureD3D12Impl>(ppSignatures[i]);
        VERIFY(pSignature != nullptr, "Pipeline resource signature at index ", i, " is null. This error should've been caught by ValidatePipelineResourceSignatures.");

        const Uint8 Index = pSignature->GetDesc().BindingIndex;

#ifdef DILIGENT_DEBUG
        VERIFY(Index < m_Signatures.size(),
               "Pipeline resource signature specifies binding index ", Uint32{Index}, " that exceeds the limit (", m_Signatures.size() - 1,
               "). This error should've been caught by ValidatePipelineResourceSignatureDesc.");

        VERIFY(m_Signatures[Index] == nullptr,
               "Pipeline resource signature '", pSignature->GetDesc().Name, "' at index ", Uint32{Index},
               " conflicts with another resource signature '", m_Signatures[Index]->GetDesc().Name,
               "' that uses the same index. This error should've been caught by ValidatePipelineResourceSignatures.");

        for (Uint32 s = 0, StageCount = pSignature->GetNumActiveShaderStages(); s < StageCount; ++s)
        {
            const auto ShaderType = pSignature->GetActiveShaderStageType(s);
            VERIFY(IsConsistentShaderType(ShaderType, PipelineType),
                   "Pipeline resource signature '", pSignature->GetDesc().Name, "' at index ", Uint32{Index},
                   " has shader stage '", GetShaderTypeLiteralName(ShaderType), "' that is not compatible with pipeline type '",
                   GetPipelineTypeString(PipelineType), "'.");
        }
#endif

        m_SignatureCount    = std::max<Uint8>(m_SignatureCount, Index + 1);
        m_Signatures[Index] = pSignature;
    }

    D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc = {};
    rootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

    std::array<Uint32, MAX_RESOURCE_SIGNATURES> FirstRootIndex = {};
    Uint32                                      TotalParams    = 0;

    for (Uint32 s = 0; s < m_SignatureCount; ++s)
    {
        auto& pSignature = m_Signatures[s];
        if (pSignature != nullptr)
        {
            auto& RootParams = pSignature->m_RootParams;

            FirstRootIndex[s] = TotalParams;
            TotalParams += RootParams.GetNumRootTables() + RootParams.GetNumRootViews();
        }
    }

    std::vector<D3D12_ROOT_PARAMETER, STDAllocatorRawMem<D3D12_ROOT_PARAMETER>> D3D12Parameters(TotalParams, D3D12_ROOT_PARAMETER{}, STD_ALLOCATOR_RAW_MEM(D3D12_ROOT_PARAMETER, GetRawAllocator(), "Allocator for vector<D3D12_ROOT_PARAMETER>"));

    for (Uint32 s = 0; s < m_SignatureCount; ++s)
    {
        auto& pSignature = m_Signatures[s];
        if (pSignature != nullptr)
        {
            auto& RootParams = pSignature->m_RootParams;
            for (Uint32 rt = 0; rt < RootParams.GetNumRootTables(); ++rt)
            {
                const auto&                 RootTable = RootParams.GetRootTable(rt);
                const D3D12_ROOT_PARAMETER& SrcParam  = RootTable;
                const Uint32                RootIndex = FirstRootIndex[s] + RootTable.GetLocalRootIndex();
                VERIFY(SrcParam.ParameterType == D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE && SrcParam.DescriptorTable.NumDescriptorRanges > 0, "Non-empty descriptor table is expected");
                D3D12Parameters[RootIndex] = SrcParam;
            }
            for (Uint32 rv = 0; rv < RootParams.GetNumRootViews(); ++rv)
            {
                const auto&                 RootView  = RootParams.GetRootView(rv);
                const D3D12_ROOT_PARAMETER& SrcParam  = RootView;
                const Uint32                RootIndex = FirstRootIndex[s] + RootView.GetLocalRootIndex();
                VERIFY(SrcParam.ParameterType == D3D12_ROOT_PARAMETER_TYPE_CBV, "Root CBV is expected");
                D3D12Parameters[RootIndex] = SrcParam;
            }
        }
    }

    rootSignatureDesc.NumParameters = static_cast<UINT>(D3D12Parameters.size());
    rootSignatureDesc.pParameters   = D3D12Parameters.size() ? D3D12Parameters.data() : nullptr;
    /*
    UINT TotalD3D12StaticSamplers = 0;
    for (const auto& ImtblSam : m_ImmutableSamplers)
    {
        TotalD3D12StaticSamplers += ImtblSam.ArraySize;
    }
    rootSignatureDesc.NumStaticSamplers = TotalD3D12StaticSamplers;
    rootSignatureDesc.pStaticSamplers   = nullptr;
    std::vector<D3D12_STATIC_SAMPLER_DESC, STDAllocatorRawMem<D3D12_STATIC_SAMPLER_DESC>> D3D12StaticSamplers(STD_ALLOCATOR_RAW_MEM(D3D12_STATIC_SAMPLER_DESC, GetRawAllocator(), "Allocator for vector<D3D12_STATIC_SAMPLER_DESC>"));
    D3D12StaticSamplers.reserve(TotalD3D12StaticSamplers);
    if (!m_ImmutableSamplers.empty())
    {
        for (size_t s = 0; s < m_ImmutableSamplers.size(); ++s)
        {
            const auto& ImtblSmplr = m_ImmutableSamplers[s];
            const auto& SamDesc    = ImtblSmplr.Desc;
            for (UINT ArrInd = 0; ArrInd < ImtblSmplr.ArraySize; ++ArrInd)
            {
                D3D12StaticSamplers.emplace_back(
                    D3D12_STATIC_SAMPLER_DESC //
                    {
                        FilterTypeToD3D12Filter(SamDesc.MinFilter, SamDesc.MagFilter, SamDesc.MipFilter),
                        TexAddressModeToD3D12AddressMode(SamDesc.AddressU),
                        TexAddressModeToD3D12AddressMode(SamDesc.AddressV),
                        TexAddressModeToD3D12AddressMode(SamDesc.AddressW),
                        SamDesc.MipLODBias,
                        SamDesc.MaxAnisotropy,
                        ComparisonFuncToD3D12ComparisonFunc(SamDesc.ComparisonFunc),
                        BorderColorToD3D12StaticBorderColor(SamDesc.BorderColor),
                        SamDesc.MinLOD,
                        SamDesc.MaxLOD,
                        ImtblSmplr.ShaderRegister + ArrInd,
                        ImtblSmplr.RegisterSpace,
                        ShaderTypeToD3D12ShaderVisibility(ImtblSmplr.ShaderType) //
                    }                                                            //
                );
            }
        }
        rootSignatureDesc.pStaticSamplers = D3D12StaticSamplers.data();

        VERIFY_EXPR(D3D12StaticSamplers.size() == TotalD3D12StaticSamplers);
    }
    */

    CComPtr<ID3DBlob> signature;
    CComPtr<ID3DBlob> error;

    HRESULT hr = D3D12SerializeRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, &signature, &error);
    if (error)
    {
        LOG_ERROR_MESSAGE("Error: ", (const char*)error->GetBufferPointer());
    }
    CHECK_D3D_RESULT_THROW(hr, "Failed to serialize root signature");

    auto* pd3d12Device = pDeviceD3D12Impl->GetD3D12Device();

    hr = pd3d12Device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), __uuidof(m_pd3d12RootSignature), reinterpret_cast<void**>(static_cast<ID3D12RootSignature**>(&m_pd3d12RootSignature)));
    CHECK_D3D_RESULT_THROW(hr, "Failed to create root signature");
}



LocalRootSignature::LocalRootSignature(const char* pCBName, Uint32 ShaderRecordSize) :
    m_pName{pCBName},
    m_ShaderRecordSize{ShaderRecordSize}
{
    VERIFY_EXPR((m_pName != nullptr) == (m_ShaderRecordSize > 0));
}

bool LocalRootSignature::IsShaderRecord(const D3DShaderResourceAttribs& CB)
{
    if (m_ShaderRecordSize > 0 &&
        CB.GetInputType() == D3D_SIT_CBUFFER &&
        strcmp(m_pName, CB.Name) == 0)
    {
        return true;
    }
    return false;
}

ID3D12RootSignature* LocalRootSignature::Create(ID3D12Device* pDevice)
{
    if (m_ShaderRecordSize == 0 || m_BindPoint == InvalidBindPoint)
        return nullptr;

    D3D12_ROOT_SIGNATURE_DESC d3d12RootSignatureDesc = {};
    D3D12_ROOT_PARAMETER      d3d12Params            = {};

    d3d12Params.ParameterType            = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
    d3d12Params.ShaderVisibility         = D3D12_SHADER_VISIBILITY_ALL;
    d3d12Params.Constants.Num32BitValues = m_ShaderRecordSize / 4;
    d3d12Params.Constants.RegisterSpace  = 0;
    d3d12Params.Constants.ShaderRegister = m_BindPoint;

    d3d12RootSignatureDesc.Flags         = D3D12_ROOT_SIGNATURE_FLAG_LOCAL_ROOT_SIGNATURE;
    d3d12RootSignatureDesc.NumParameters = 1;
    d3d12RootSignatureDesc.pParameters   = &d3d12Params;

    CComPtr<ID3DBlob> signature;
    auto              hr = D3D12SerializeRootSignature(&d3d12RootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, &signature, nullptr);
    CHECK_D3D_RESULT_THROW(hr, "Failed to serialize local root signature");

    hr = pDevice->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&m_pd3d12RootSignature));
    CHECK_D3D_RESULT_THROW(hr, "Failed to create D3D12 local root signature");

    return m_pd3d12RootSignature;
}

} // namespace Diligent
