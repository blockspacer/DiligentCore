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
#include "PipelineResourceSignatureD3D12Impl.hpp"
#include "ShaderResourceCacheD3D12.hpp"
#include "ShaderVariableD3D12.hpp"
#include "RenderDeviceD3D12Impl.hpp"
#include "ShaderResourceBindingD3D12Impl.hpp"

namespace Diligent
{

PipelineResourceSignatureD3D12Impl::RootParameter::RootParameter(D3D12_ROOT_PARAMETER_TYPE     ParameterType,
                                                                 Uint32                        RootIndex,
                                                                 UINT                          Register,
                                                                 UINT                          RegisterSpace,
                                                                 D3D12_SHADER_VISIBILITY       Visibility,
                                                                 SHADER_RESOURCE_VARIABLE_TYPE VarType) noexcept :
    // clang-format off
    m_RootIndex    {RootIndex},
    m_ShaderVarType{VarType  }
// clang-format on
{
    VERIFY(ParameterType == D3D12_ROOT_PARAMETER_TYPE_CBV || ParameterType == D3D12_ROOT_PARAMETER_TYPE_SRV || ParameterType == D3D12_ROOT_PARAMETER_TYPE_UAV, "Unexpected parameter type - verify argument list");
    m_RootParam.ParameterType             = ParameterType;
    m_RootParam.ShaderVisibility          = Visibility;
    m_RootParam.Descriptor.ShaderRegister = Register;
    m_RootParam.Descriptor.RegisterSpace  = RegisterSpace;
}

PipelineResourceSignatureD3D12Impl::RootParameter::RootParameter(D3D12_ROOT_PARAMETER_TYPE     ParameterType,
                                                                 Uint32                        RootIndex,
                                                                 UINT                          Register,
                                                                 UINT                          RegisterSpace,
                                                                 UINT                          NumDwords,
                                                                 D3D12_SHADER_VISIBILITY       Visibility,
                                                                 SHADER_RESOURCE_VARIABLE_TYPE VarType) noexcept :
    // clang-format off
    m_RootIndex    {RootIndex},
    m_ShaderVarType{VarType  }
// clang-format on
{
    VERIFY(ParameterType == D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS, "Unexpected parameter type - verify argument list");
    m_RootParam.ParameterType            = ParameterType;
    m_RootParam.ShaderVisibility         = Visibility;
    m_RootParam.Constants.Num32BitValues = NumDwords;
    m_RootParam.Constants.ShaderRegister = Register;
    m_RootParam.Constants.RegisterSpace  = RegisterSpace;
}

PipelineResourceSignatureD3D12Impl::RootParameter::RootParameter(D3D12_ROOT_PARAMETER_TYPE     ParameterType,
                                                                 Uint32                        RootIndex,
                                                                 UINT                          NumRanges,
                                                                 D3D12_DESCRIPTOR_RANGE*       pRanges,
                                                                 D3D12_SHADER_VISIBILITY       Visibility,
                                                                 SHADER_RESOURCE_VARIABLE_TYPE VarType) noexcept :
    // clang-format off
    m_RootIndex    {RootIndex},
    m_ShaderVarType{VarType  }
// clang-format on
{
    VERIFY(ParameterType == D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE, "Unexpected parameter type - verify argument list");
    VERIFY_EXPR(pRanges != nullptr);
    m_RootParam.ParameterType                       = ParameterType;
    m_RootParam.ShaderVisibility                    = Visibility;
    m_RootParam.DescriptorTable.NumDescriptorRanges = NumRanges;
    m_RootParam.DescriptorTable.pDescriptorRanges   = pRanges;
#ifdef DILIGENT_DEBUG
    for (Uint32 r = 0; r < NumRanges; ++r)
        pRanges[r].RangeType = static_cast<D3D12_DESCRIPTOR_RANGE_TYPE>(-1);
#endif
}

PipelineResourceSignatureD3D12Impl::RootParameter::RootParameter(const RootParameter& RP) noexcept :
    // clang-format off
    m_RootParam          {RP.m_RootParam          },
    m_DescriptorTableSize{RP.m_DescriptorTableSize},
    m_ShaderVarType      {RP.m_ShaderVarType      },
    m_RootIndex          {RP.m_RootIndex          }
// clang-format on
{
    VERIFY(m_RootParam.ParameterType != D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE, "Use another constructor to copy descriptor table");
}

PipelineResourceSignatureD3D12Impl::RootParameter::RootParameter(const RootParameter&    RP,
                                                                 UINT                    NumRanges,
                                                                 D3D12_DESCRIPTOR_RANGE* pRanges) noexcept :
    // clang-format off
    m_RootParam          {RP.m_RootParam          },
    m_DescriptorTableSize{RP.m_DescriptorTableSize},
    m_ShaderVarType      {RP.m_ShaderVarType      },
    m_RootIndex          {RP.m_RootIndex          }
// clang-format on
{
    VERIFY(m_RootParam.ParameterType == D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE, "Root parameter is expected to be a descriptor table");
    VERIFY(NumRanges >= m_RootParam.DescriptorTable.NumDescriptorRanges, "New table must be larger than source one");
    auto& DstTbl               = m_RootParam.DescriptorTable;
    DstTbl.NumDescriptorRanges = NumRanges;
    DstTbl.pDescriptorRanges   = pRanges;
    const auto& SrcTbl         = RP.m_RootParam.DescriptorTable;
    memcpy(pRanges, SrcTbl.pDescriptorRanges, SrcTbl.NumDescriptorRanges * sizeof(D3D12_DESCRIPTOR_RANGE));
#ifdef DILIGENT_DEBUG
    {
        Uint32 dbgTableSize = 0;
        for (Uint32 r = 0; r < SrcTbl.NumDescriptorRanges; ++r)
        {
            const auto& Range = SrcTbl.pDescriptorRanges[r];
            dbgTableSize      = std::max(dbgTableSize, Range.OffsetInDescriptorsFromTableStart + Range.NumDescriptors);
        }
        VERIFY(dbgTableSize == m_DescriptorTableSize, "Incorrect descriptor table size");

        for (Uint32 r = SrcTbl.NumDescriptorRanges; r < DstTbl.NumDescriptorRanges; ++r)
            pRanges[r].RangeType = static_cast<D3D12_DESCRIPTOR_RANGE_TYPE>(-1);
    }
#endif
}

void PipelineResourceSignatureD3D12Impl::RootParameter::SetDescriptorRange(UINT                        RangeIndex,
                                                                           D3D12_DESCRIPTOR_RANGE_TYPE Type,
                                                                           UINT                        Register,
                                                                           UINT                        Count,
                                                                           UINT                        Space,
                                                                           UINT                        OffsetFromTableStart)
{
    VERIFY(m_RootParam.ParameterType == D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE, "Incorrect parameter table: descriptor table is expected");
    auto& Tbl = m_RootParam.DescriptorTable;
    VERIFY(RangeIndex < Tbl.NumDescriptorRanges, "Invalid descriptor range index");
    D3D12_DESCRIPTOR_RANGE& range = const_cast<D3D12_DESCRIPTOR_RANGE&>(Tbl.pDescriptorRanges[RangeIndex]);
    VERIFY(range.RangeType == static_cast<D3D12_DESCRIPTOR_RANGE_TYPE>(-1), "Descriptor range has already been initialized. m_DescriptorTableSize may be updated incorrectly");
    range.RangeType                         = Type;
    range.NumDescriptors                    = Count;
    range.BaseShaderRegister                = Register;
    range.RegisterSpace                     = Space;
    range.OffsetInDescriptorsFromTableStart = OffsetFromTableStart;
    m_DescriptorTableSize                   = std::max(m_DescriptorTableSize, OffsetFromTableStart + Count);
}

Uint32 PipelineResourceSignatureD3D12Impl::RootParameter::GetDescriptorTableSize() const
{
    VERIFY(m_RootParam.ParameterType == D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE, "Incorrect parameter table: descriptor table is expected");
    return m_DescriptorTableSize;
}

bool PipelineResourceSignatureD3D12Impl::RootParameter::operator==(const RootParameter& rhs) const
{
    if (m_ShaderVarType != rhs.m_ShaderVarType ||
        m_DescriptorTableSize != rhs.m_DescriptorTableSize ||
        m_RootIndex != rhs.m_RootIndex)
        return false;

    if (m_RootParam.ParameterType != rhs.m_RootParam.ParameterType ||
        m_RootParam.ShaderVisibility != rhs.m_RootParam.ShaderVisibility)
        return false;

    switch (m_RootParam.ParameterType)
    {
        case D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE:
        {
            const auto& tbl0 = m_RootParam.DescriptorTable;
            const auto& tbl1 = rhs.m_RootParam.DescriptorTable;
            if (tbl0.NumDescriptorRanges != tbl1.NumDescriptorRanges)
                return false;
            for (UINT r = 0; r < tbl0.NumDescriptorRanges; ++r)
            {
                const auto& rng0 = tbl0.pDescriptorRanges[r];
                const auto& rng1 = tbl1.pDescriptorRanges[r];
                if (memcmp(&rng0, &rng1, sizeof(rng0)) != 0)
                    return false;
            }
        }
        break;

        case D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS:
        {
            const auto& cnst0 = m_RootParam.Constants;
            const auto& cnst1 = rhs.m_RootParam.Constants;
            if (memcmp(&cnst0, &cnst1, sizeof(cnst0)) != 0)
                return false;
        }
        break;

        case D3D12_ROOT_PARAMETER_TYPE_CBV:
        case D3D12_ROOT_PARAMETER_TYPE_SRV:
        case D3D12_ROOT_PARAMETER_TYPE_UAV:
        {
            const auto& dscr0 = m_RootParam.Descriptor;
            const auto& dscr1 = rhs.m_RootParam.Descriptor;
            if (memcmp(&dscr0, &dscr1, sizeof(dscr0)) != 0)
                return false;
        }
        break;

        default: UNEXPECTED("Unexpected root parameter type");
    }

    return true;
}

size_t PipelineResourceSignatureD3D12Impl::RootParameter::GetHash() const
{
    size_t hash = ComputeHash(m_ShaderVarType, m_DescriptorTableSize, m_RootIndex);
    HashCombine(hash, m_RootParam.ParameterType, m_RootParam.ShaderVisibility);

    switch (m_RootParam.ParameterType)
    {
        case D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE:
        {
            const auto& tbl = m_RootParam.DescriptorTable;
            HashCombine(hash, tbl.NumDescriptorRanges);
            for (UINT r = 0; r < tbl.NumDescriptorRanges; ++r)
            {
                const auto& rng = tbl.pDescriptorRanges[r];
                HashCombine(hash, rng.BaseShaderRegister, rng.NumDescriptors, rng.OffsetInDescriptorsFromTableStart, rng.RangeType, rng.RegisterSpace);
            }
        }
        break;

        case D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS:
        {
            const auto& cnst = m_RootParam.Constants;
            HashCombine(hash, cnst.Num32BitValues, cnst.RegisterSpace, cnst.ShaderRegister);
        }
        break;

        case D3D12_ROOT_PARAMETER_TYPE_CBV:
        case D3D12_ROOT_PARAMETER_TYPE_SRV:
        case D3D12_ROOT_PARAMETER_TYPE_UAV:
        {
            const auto& dscr = m_RootParam.Descriptor;
            HashCombine(hash, dscr.RegisterSpace, dscr.ShaderRegister);
        }
        break;

        default: UNEXPECTED("Unexpected root parameter type");
    }

    return hash;
}


PipelineResourceSignatureD3D12Impl::RootParamsManager::RootParamsManager(IMemoryAllocator& MemAllocator) :
    m_MemAllocator{MemAllocator},
    m_pMemory{nullptr, STDDeleter<void, IMemoryAllocator>(MemAllocator)}
{}

size_t PipelineResourceSignatureD3D12Impl::RootParamsManager::GetRequiredMemorySize(Uint32 NumExtraRootTables,
                                                                                    Uint32 NumExtraRootViews,
                                                                                    Uint32 NumExtraDescriptorRanges) const
{
    return sizeof(RootParameter) * (m_NumRootTables + NumExtraRootTables + m_NumRootViews + NumExtraRootViews) + sizeof(D3D12_DESCRIPTOR_RANGE) * (m_TotalDescriptorRanges + NumExtraDescriptorRanges);
}

D3D12_DESCRIPTOR_RANGE* PipelineResourceSignatureD3D12Impl::RootParamsManager::Extend(Uint32 NumExtraRootTables,
                                                                                      Uint32 NumExtraRootViews,
                                                                                      Uint32 NumExtraDescriptorRanges,
                                                                                      Uint32 RootTableToAddRanges)
{
    VERIFY(NumExtraRootTables > 0 || NumExtraRootViews > 0 || NumExtraDescriptorRanges > 0, "At least one root table, root view or descriptor range must be added");
    auto MemorySize = GetRequiredMemorySize(NumExtraRootTables, NumExtraRootViews, NumExtraDescriptorRanges);
    VERIFY_EXPR(MemorySize > 0);
    auto* pNewMemory = ALLOCATE_RAW(m_MemAllocator, "Memory buffer for root tables, root views & descriptor ranges", MemorySize);
    memset(pNewMemory, 0, MemorySize);

    // Note: this order is more efficient than views->tables->ranges
    auto* pNewRootTables          = reinterpret_cast<RootParameter*>(pNewMemory);
    auto* pNewRootViews           = pNewRootTables + (m_NumRootTables + NumExtraRootTables);
    auto* pCurrDescriptorRangePtr = reinterpret_cast<D3D12_DESCRIPTOR_RANGE*>(pNewRootViews + m_NumRootViews + NumExtraRootViews);

    // Copy existing root tables to new memory
    for (Uint32 rt = 0; rt < m_NumRootTables; ++rt)
    {
        const auto& SrcTbl      = GetRootTable(rt);
        auto&       D3D12SrcTbl = static_cast<const D3D12_ROOT_PARAMETER&>(SrcTbl).DescriptorTable;
        auto        NumRanges   = D3D12SrcTbl.NumDescriptorRanges;
        if (rt == RootTableToAddRanges)
        {
            VERIFY(NumExtraRootTables == 0 || NumExtraRootTables == 1, "Up to one descriptor table can be extended at a time");
            NumRanges += NumExtraDescriptorRanges;
        }
        new (pNewRootTables + rt) RootParameter(SrcTbl, NumRanges, pCurrDescriptorRangePtr);
        pCurrDescriptorRangePtr += NumRanges;
    }

    // Copy existing root views to new memory
    for (Uint32 rv = 0; rv < m_NumRootViews; ++rv)
    {
        const auto& SrcView = GetRootView(rv);
        new (pNewRootViews + rv) RootParameter(SrcView);
    }

    m_pMemory.reset(pNewMemory);
    m_NumRootTables += NumExtraRootTables;
    m_NumRootViews += NumExtraRootViews;
    m_TotalDescriptorRanges += NumExtraDescriptorRanges;
    m_pRootTables = m_NumRootTables != 0 ? pNewRootTables : nullptr;
    m_pRootViews  = m_NumRootViews != 0 ? pNewRootViews : nullptr;

    return pCurrDescriptorRangePtr;
}

void PipelineResourceSignatureD3D12Impl::RootParamsManager::AddRootView(D3D12_ROOT_PARAMETER_TYPE     ParameterType,
                                                                        Uint32                        RootIndex,
                                                                        UINT                          Register,
                                                                        D3D12_SHADER_VISIBILITY       Visibility,
                                                                        SHADER_RESOURCE_VARIABLE_TYPE VarType)
{
    auto* pRangePtr = Extend(0, 1, 0);
    VERIFY_EXPR((char*)pRangePtr == (char*)m_pMemory.get() + GetRequiredMemorySize(0, 0, 0));
    new (m_pRootViews + m_NumRootViews - 1) RootParameter(ParameterType, RootIndex, Register, 0u, Visibility, VarType);
}

void PipelineResourceSignatureD3D12Impl::RootParamsManager::AddRootTable(Uint32                        RootIndex,
                                                                         D3D12_SHADER_VISIBILITY       Visibility,
                                                                         SHADER_RESOURCE_VARIABLE_TYPE VarType,
                                                                         Uint32                        NumRangesInNewTable)
{
    auto* pRangePtr = Extend(1, 0, NumRangesInNewTable);
    VERIFY_EXPR((char*)(pRangePtr + NumRangesInNewTable) == (char*)m_pMemory.get() + GetRequiredMemorySize(0, 0, 0));
    new (m_pRootTables + m_NumRootTables - 1) RootParameter(D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE, RootIndex, NumRangesInNewTable, pRangePtr, Visibility, VarType);
}

void PipelineResourceSignatureD3D12Impl::RootParamsManager::AddDescriptorRanges(Uint32 RootTableInd, Uint32 NumExtraRanges)
{
    auto* pRangePtr = Extend(0, 0, NumExtraRanges, RootTableInd);
    VERIFY_EXPR((char*)pRangePtr == (char*)m_pMemory.get() + GetRequiredMemorySize(0, 0, 0));
}

bool PipelineResourceSignatureD3D12Impl::RootParamsManager::operator==(const RootParamsManager& RootParams) const
{
    if (m_NumRootTables != RootParams.m_NumRootTables ||
        m_NumRootViews != RootParams.m_NumRootViews)
        return false;

    for (Uint32 rv = 0; rv < m_NumRootViews; ++rv)
    {
        const auto& RV0 = GetRootView(rv);
        const auto& RV1 = RootParams.GetRootView(rv);
        if (RV0 != RV1)
            return false;
    }

    for (Uint32 rv = 0; rv < m_NumRootTables; ++rv)
    {
        const auto& RT0 = GetRootTable(rv);
        const auto& RT1 = RootParams.GetRootTable(rv);
        if (RT0 != RT1)
            return false;
    }

    return true;
}


namespace
{

D3D12_DESCRIPTOR_RANGE_TYPE GetDescriptorRangeType(SHADER_RESOURCE_TYPE ResType)
{
    static_assert(SHADER_RESOURCE_TYPE_LAST == SHADER_RESOURCE_TYPE_ACCEL_STRUCT, "Please update the switch below to handle the new resource type");

    switch (ResType)
    {
        // clang-format off
        case SHADER_RESOURCE_TYPE_CONSTANT_BUFFER: return D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
        case SHADER_RESOURCE_TYPE_TEXTURE_SRV:     return D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
        case SHADER_RESOURCE_TYPE_BUFFER_SRV:      return D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
        case SHADER_RESOURCE_TYPE_TEXTURE_UAV:     return D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
        case SHADER_RESOURCE_TYPE_BUFFER_UAV:      return D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
        case SHADER_RESOURCE_TYPE_SAMPLER:         return D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER;
        case SHADER_RESOURCE_TYPE_ACCEL_STRUCT:    return D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
        // clang-format on
        case SHADER_RESOURCE_TYPE_INPUT_ATTACHMENT:
        default:
            UNEXPECTED("Unknown resource type");
            return static_cast<D3D12_DESCRIPTOR_RANGE_TYPE>(~0u);
    }
}

inline Uint32 GetSpaceId(SHADER_RESOURCE_VARIABLE_TYPE VarType)
{
    return VarType == SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC ? 1 : 0;
}

} // namespace


PipelineResourceSignatureD3D12Impl::PipelineResourceSignatureD3D12Impl(IReferenceCounters*                  pRefCounters,
                                                                       RenderDeviceD3D12Impl*               pDevice,
                                                                       const PipelineResourceSignatureDesc& Desc,
                                                                       bool                                 bIsDeviceInternal) :
    TPipelineResourceSignatureBase{pRefCounters, pDevice, Desc, bIsDeviceInternal},
    m_RootParams{GetRawAllocator()},
    m_SRBMemAllocator{GetRawAllocator()}
{
    try
    {
        m_SrvCbvUavRootTablesMap.fill(InvalidRootTableIndex);
        m_SamplerRootTablesMap.fill(InvalidRootTableIndex);

        FixedLinearAllocator MemPool{GetRawAllocator()};

        // Reserve at least 1 element because m_pResourceAttribs must hold a pointer to memory
        MemPool.AddSpace<ResourceAttribs>(std::max(1u, Desc.NumResources));

        ReserveSpaceForDescription(MemPool, Desc);

        Uint32 StaticResourceCount = 0; // The total number of static resources in all stages
                                        // accounting for array sizes.

        std::array<Uint32, 2> CacheSizes = {};

        SHADER_TYPE StaticResStages = SHADER_TYPE_UNKNOWN; // Shader stages that have static resources
        for (Uint32 i = 0; i < Desc.NumResources; ++i)
        {
            const auto& ResDesc = Desc.Resources[i];
            const auto  SpaceId = GetSpaceId(ResDesc.VarType);

            m_ShaderStages |= ResDesc.ShaderStages;

            if (ResDesc.VarType == SHADER_RESOURCE_VARIABLE_TYPE_STATIC)
            {
                StaticResStages |= ResDesc.ShaderStages;
                StaticResourceCount += ResDesc.ArraySize;
            }

            CacheSizes[SpaceId] += ResDesc.ArraySize;
        }

        m_NumShaderStages = static_cast<Uint8>(PlatformMisc::CountOneBits(static_cast<Uint32>(m_ShaderStages)));
        if (m_ShaderStages != SHADER_TYPE_UNKNOWN)
        {
            m_PipelineType = PipelineTypeFromShaderStages(m_ShaderStages);
            DEV_CHECK_ERR(m_PipelineType != PIPELINE_TYPE_INVALID, "Failed to deduce pipeline type from shader stages");
        }

        int StaticVarStageCount = 0; // The number of shader stages that have static variables
        for (; StaticResStages != SHADER_TYPE_UNKNOWN; ++StaticVarStageCount)
        {
            const auto StageBit             = ExtractLSB(StaticResStages);
            const auto ShaderTypeInd        = GetShaderTypePipelineIndex(StageBit, m_PipelineType);
            m_StaticVarIndex[ShaderTypeInd] = static_cast<Int8>(StaticVarStageCount);
        }
        if (StaticVarStageCount > 0)
        {
            MemPool.AddSpace<ShaderResourceCacheD3D12>(1);
            MemPool.AddSpace<ShaderVariableManagerD3D12>(StaticVarStageCount);
        }

        MemPool.Reserve();

        m_pResourceAttribs = MemPool.Allocate<ResourceAttribs>(std::max(1u, m_Desc.NumResources));

        // The memory is now owned by PipelineResourceSignatureD3D12Impl and will be freed by Destruct().
        auto* Ptr = MemPool.ReleaseOwnership();
        VERIFY_EXPR(Ptr == m_pResourceAttribs);
        (void)Ptr;

        CopyDescription(MemPool, Desc);

        if (StaticVarStageCount > 0)
        {
            m_pResourceCache = MemPool.Construct<ShaderResourceCacheD3D12>(CacheContentType::Signature);
            m_StaticVarsMgrs = MemPool.Allocate<ShaderVariableManagerD3D12>(StaticVarStageCount);

            m_pResourceCache->Initialize(GetRawAllocator(), 1, &StaticResourceCount);
        }

        CreateLayout(CacheSizes);

        if (StaticVarStageCount > 0)
        {
            const SHADER_RESOURCE_VARIABLE_TYPE AllowedVarTypes[] = {SHADER_RESOURCE_VARIABLE_TYPE_STATIC};

            for (Uint32 i = 0; i < m_StaticVarIndex.size(); ++i)
            {
                Int8 Idx = m_StaticVarIndex[i];
                if (Idx >= 0)
                {
                    VERIFY_EXPR(Idx < StaticVarStageCount);
                    const auto ShaderType = GetShaderTypeFromPipelineIndex(i, GetPipelineType());
                    new (m_StaticVarsMgrs + Idx) ShaderVariableManagerD3D12{*this, *m_pResourceCache};
                    m_StaticVarsMgrs[Idx].Initialize(*this, GetRawAllocator(), AllowedVarTypes, _countof(AllowedVarTypes), ShaderType);
                }
            }
        }

        m_Hash = CalculateHash();
    }
    catch (...)
    {
        Destruct();
        throw;
    }
}

void PipelineResourceSignatureD3D12Impl::CreateLayout(const CacheOffsetsType& CacheSizes)
{ /*
    CacheOffsetsType CacheOffsets      = {};
    Uint32           StaticCacheOffset = 0;

    for (Uint32 i = 0; i < m_Desc.NumResources; ++i)
    {
        const auto& ResDesc    = m_Desc.Resources[i];
        const auto  SpaceId    = GetSpaceId(ResDesc.VarType);
        const auto  CacheGroup = GetResourceCacheGroup(ResDesc);

        Uint32                      RootIndex           = ResourceAttribs::InvalidRootIndex;
        Uint32                      Offset              = ~0u;
        Uint32                      BindPoint           = ResourceAttribs::InvalidBindPoint;
        D3D12_DESCRIPTOR_RANGE_TYPE DescriptorRangeType = GetDescriptorRangeType(ResDesc.ResourceType);
        
        // runtime sized array must be in separate space
        if (ResDesc.Flags & PIPELINE_RESOURCE_FLAG_RUNTIME_ARRAY)
        {
            Space     = m_NumSpaces++;
            BindPoint = 0;
        }
        else
        {
            Space     = 0;
            BindPoint = m_NumResources[RangeType];
            m_NumResources[RangeType] += ArraySize;
        }

        //if (ResDesc.ShaderStages & (ResDesc.ShaderStages - 1))
        //    AllocateResourceSlot(ResDesc.ShaderStages, ShaderVisibility[s], VarType, DescriptorRangeType, ResDesc.ArraySize, BindPoint, Space, RootIndex, Offset);
        //else
        //    AllocateResourceSlot(ShaderTypes[s], ShaderVisibility[s], VarType, DescriptorRangeType, ResDesc.ArraySize, BindPoint, Space, RootIndex, Offset);
    }*/
}

// http://diligentgraphics.com/diligent-engine/architecture/d3d12/shader-resource-layout#Initializing-Shader-Resource-Layouts-and-Root-Signature-in-a-Pipeline-State-Object
void PipelineResourceSignatureD3D12Impl::AllocateResourceSlot(SHADER_TYPE                   ShaderType,
                                                              D3D12_SHADER_VISIBILITY       ShaderVisibility,
                                                              SHADER_RESOURCE_VARIABLE_TYPE VariableType,
                                                              D3D12_DESCRIPTOR_RANGE_TYPE   RangeType,
                                                              Uint32                        ArraySize,
                                                              Uint32                       BindPoint,
                                                              Uint32                       Space,
                                                              Uint32&                       RootIndex,           // Output parameter
                                                              Uint32&                       OffsetFromTableStart // Output parameter
)
{
    if (RangeType == D3D12_DESCRIPTOR_RANGE_TYPE_CBV && ArraySize == 1)
    {
        // Allocate single CBV directly in the root signature

        // Get the next available root index past all allocated tables and root views
        RootIndex            = m_RootParams.GetNumRootTables() + m_RootParams.GetNumRootViews();
        OffsetFromTableStart = 0;

        // Add new root view to existing root parameters
        m_RootParams.AddRootView(D3D12_ROOT_PARAMETER_TYPE_CBV, RootIndex, BindPoint, Space, ShaderVisibility, VariableType);
    }
    else
    {
        const auto ShaderInd = GetShaderTypePipelineIndex(ShaderType, GetPipelineType());
        // Use the same table for static and mutable resources. Treat both as static
        const auto RootTableType = (VariableType == SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC) ? SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC : SHADER_RESOURCE_VARIABLE_TYPE_STATIC;
        const auto TableIndKey   = ShaderInd * SHADER_RESOURCE_VARIABLE_TYPE_NUM_TYPES + RootTableType;
        // Get the table array index (this is not the root index!)
        auto& RootTableArrayInd = ((RangeType == D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER) ? m_SamplerRootTablesMap : m_SrvCbvUavRootTablesMap)[TableIndKey];
        if (RootTableArrayInd == InvalidRootTableIndex)
        {
            // Root table has not been assigned to this combination yet

            // Get the next available root index past all allocated tables and root views
            RootIndex = m_RootParams.GetNumRootTables() + m_RootParams.GetNumRootViews();
            VERIFY_EXPR(m_RootParams.GetNumRootTables() < 255);
            RootTableArrayInd = static_cast<Uint8>(m_RootParams.GetNumRootTables());
            // Add root table with one single-descriptor range
            m_RootParams.AddRootTable(RootIndex, ShaderVisibility, RootTableType, 1);
        }
        else
        {
            // Add a new single-descriptor range to the existing table at index RootTableArrayInd
            m_RootParams.AddDescriptorRanges(RootTableArrayInd, 1);
        }

        // Reference to either existing or just added table
        auto& CurrParam = m_RootParams.GetRootTable(RootTableArrayInd);
        RootIndex       = CurrParam.GetLocalRootIndex();

        const auto& d3d12RootParam = static_cast<const D3D12_ROOT_PARAMETER&>(CurrParam);

        VERIFY(d3d12RootParam.ShaderVisibility == ShaderVisibility, "Shader visibility is not correct");

        // Descriptors are tightly packed, so the next descriptor offset is the
        // current size of the table
        OffsetFromTableStart = CurrParam.GetDescriptorTableSize();

        // New just added range is the last range in the descriptor table
        Uint32 NewDescriptorRangeIndex = d3d12RootParam.DescriptorTable.NumDescriptorRanges - 1;
        CurrParam.SetDescriptorRange(NewDescriptorRangeIndex,
                                     RangeType,           // Range type (CBV, SRV, UAV or SAMPLER)
                                     BindPoint,           // Shader register
                                     Space,               // Shader register space
                                     ArraySize,           // Number of registers used (1 for non-array resources)
                                     0,                   // Register space. Always 0 for now
                                     OffsetFromTableStart // Offset in descriptors from the table start
        );
    }
}

PipelineResourceSignatureD3D12Impl::~PipelineResourceSignatureD3D12Impl()
{
    Destruct();
}

void PipelineResourceSignatureD3D12Impl::Destruct()
{
    TPipelineResourceSignatureBase::Destruct();

    if (m_pResourceAttribs == nullptr)
        return; // memory is not allocated

    auto& RawAllocator = GetRawAllocator();

    if (m_StaticVarsMgrs != nullptr)
    {
        for (size_t i = 0; i < m_StaticVarIndex.size(); ++i)
        {
            auto Idx = m_StaticVarIndex[i];
            if (Idx >= 0)
            {
                m_StaticVarsMgrs[Idx].Destroy(RawAllocator);
                m_StaticVarsMgrs[Idx].~ShaderVariableManagerD3D12();
            }
        }
        m_StaticVarIndex.fill(-1);
        m_StaticVarsMgrs = nullptr;
    }

    if (m_pResourceCache != nullptr)
    {
        m_pResourceCache->~ShaderResourceCacheD3D12();
        m_pResourceCache = nullptr;
    }

    if (void* pRawMem = m_pResourceAttribs)
    {
        RawAllocator.Free(pRawMem);
        m_pResourceAttribs = nullptr;
    }
}

bool PipelineResourceSignatureD3D12Impl::IsCompatibleWith(const PipelineResourceSignatureD3D12Impl& Other) const
{
    // AZ TODO
    return false;
}

void PipelineResourceSignatureD3D12Impl::CreateShaderResourceBinding(IShaderResourceBinding** ppShaderResourceBinding,
                                                                     bool                     InitStaticResources)
{
    auto& SRBAllocator     = m_pDevice->GetSRBAllocator();
    auto  pResBindingD3D12 = NEW_RC_OBJ(SRBAllocator, "ShaderResourceBindingD3D12Impl instance", ShaderResourceBindingD3D12Impl)(this, false);
    if (InitStaticResources)
        pResBindingD3D12->InitializeStaticResources(nullptr);
    pResBindingD3D12->QueryInterface(IID_ShaderResourceBinding, reinterpret_cast<IObject**>(ppShaderResourceBinding));
}

Uint32 PipelineResourceSignatureD3D12Impl::GetStaticVariableCount(SHADER_TYPE ShaderType) const
{
    const auto VarMngrInd = GetStaticVariableCountHelper(ShaderType, m_StaticVarIndex);
    if (VarMngrInd < 0)
        return 0;

    auto& StaticVarMgr = m_StaticVarsMgrs[VarMngrInd];
    return StaticVarMgr.GetVariableCount();
}

IShaderResourceVariable* PipelineResourceSignatureD3D12Impl::GetStaticVariableByName(SHADER_TYPE ShaderType, const Char* Name)
{
    const auto VarMngrInd = GetStaticVariableByNameHelper(ShaderType, Name, m_StaticVarIndex);
    if (VarMngrInd < 0)
        return nullptr;

    auto& StaticVarMgr = m_StaticVarsMgrs[VarMngrInd];
    return StaticVarMgr.GetVariable(Name);
}

IShaderResourceVariable* PipelineResourceSignatureD3D12Impl::GetStaticVariableByIndex(SHADER_TYPE ShaderType, Uint32 Index)
{
    const auto VarMngrInd = GetStaticVariableByIndexHelper(ShaderType, Index, m_StaticVarIndex);
    if (VarMngrInd < 0)
        return nullptr;

    auto& StaticVarMgr = m_StaticVarsMgrs[VarMngrInd];
    return StaticVarMgr.GetVariable(Index);
}

void PipelineResourceSignatureD3D12Impl::BindStaticResources(Uint32            ShaderFlags,
                                                             IResourceMapping* pResMapping,
                                                             Uint32            Flags)
{
    const auto PipelineType = GetPipelineType();
    for (Uint32 ShaderInd = 0; ShaderInd < m_StaticVarIndex.size(); ++ShaderInd)
    {
        const auto VarMngrInd = m_StaticVarIndex[ShaderInd];
        if (VarMngrInd >= 0)
        {
            // ShaderInd is the shader type pipeline index here
            const auto ShaderType = GetShaderTypeFromPipelineIndex(ShaderInd, PipelineType);
            if (ShaderFlags & ShaderType)
            {
                m_StaticVarsMgrs[VarMngrInd].BindResources(pResMapping, Flags);
            }
        }
    }
}

size_t PipelineResourceSignatureD3D12Impl::CalculateHash() const
{
    // AZ TODO
    return 0;
}

void PipelineResourceSignatureD3D12Impl::InitResourceCache(ShaderResourceCacheD3D12& ResourceCache,
                                                           IMemoryAllocator&         CacheMemAllocator,
                                                           const char*               DbgPipelineName) const
{
    // AZ TODO
}

void PipelineResourceSignatureD3D12Impl::InitializeStaticSRBResources(ShaderResourceCacheD3D12& ResourceCache) const
{
    // AZ TODO
}

String PipelineResourceSignatureD3D12Impl::GetPrintName(const PipelineResourceDesc& ResDesc, Uint32 ArrayInd)
{
    // AZ TODO
    return {};
}

} // namespace Diligent
