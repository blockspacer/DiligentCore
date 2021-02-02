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

#pragma once

/// \file
/// Declaration of Diligent::RootSignature class
#include <array>
#include "BufferD3D12Impl.hpp"
#include "D3D12TypeConversions.hpp"
#include "ShaderResourceCacheD3D12.hpp"
#include "PipelineResourceSignatureD3D12Impl.hpp"
#include "PrivateConstants.h"
#include "ShaderResources.hpp"

namespace Diligent
{

class RenderDeviceD3D12Impl;
class PipelineResourceSignatureD3D12Impl;

/// Implementation of the Diligent::RootSignature class
class RootSignature
{
public:
    RootSignature();

    void Create(RenderDeviceD3D12Impl* pDeviceD3D12Impl, PIPELINE_TYPE PipelineType, IPipelineResourceSignature** ppSignatures, Uint32 SignatureCount);
    void InitResourceCache(RenderDeviceD3D12Impl* pDeviceD3D12Impl, class ShaderResourceCacheD3D12& ResourceCache, IMemoryAllocator& CacheMemAllocator) const;

    ID3D12RootSignature* GetD3D12RootSignature() const { return m_pd3d12RootSignature; }

    Uint32 GetSignatureCount() const { return m_SignatureCount; }

    PipelineResourceSignatureD3D12Impl* GetSignature(Uint32 index) const
    {
        VERIFY_EXPR(index < m_SignatureCount);
        return m_Signatures[index].RawPtr<PipelineResourceSignatureD3D12Impl>();
    }

    // Returns the index of the first space used by the given resource signature
    Uint32 GetFirstSpaceIndex(const PipelineResourceSignatureD3D12Impl* pPRS) const
    {
        VERIFY_EXPR(pPRS != nullptr);
        Uint32 Index = pPRS->GetDesc().BindingIndex;

        VERIFY_EXPR(Index < m_SignatureCount);
        VERIFY_EXPR(m_Signatures[Index] != nullptr);
        VERIFY_EXPR(!m_Signatures[Index]->IsIncompatibleWith(*pPRS));

        return m_FirstSpaceIndex[Index];
    }

private:
    CComPtr<ID3D12RootSignature> m_pd3d12RootSignature;

    // Index of the first descriptor set, for every resource signature.
    using FirstSpaceIndexArrayType             = std::array<Uint8, MAX_RESOURCE_SIGNATURES>;
    FirstSpaceIndexArrayType m_FirstSpaceIndex = {};

    // The number of resource signatures used by this root signature
    // (Maximum is MAX_RESOURCE_SIGNATURES)
    Uint8 m_SignatureCount = 0;

    using SignatureArray = std::array<RefCntAutoPtr<PipelineResourceSignatureD3D12Impl>, MAX_RESOURCE_SIGNATURES>;
    SignatureArray m_Signatures;
};


class LocalRootSignature
{
public:
    LocalRootSignature(const char* pCBName, Uint32 ShaderRecordSize);

    bool IsShaderRecord(const D3DShaderResourceAttribs& CB);

    ID3D12RootSignature* Create(ID3D12Device* pDevice);

private:
    static constexpr Uint32 InvalidBindPoint = ~0u;

    const char*                  m_pName            = nullptr;
    Uint32                       m_BindPoint        = InvalidBindPoint;
    const Uint32                 m_ShaderRecordSize = 0;
    CComPtr<ID3D12RootSignature> m_pd3d12RootSignature;
};

} // namespace Diligent
