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
/// Declaration of Diligent::PipelineResourceSignatureD3D12Impl class

#include <array>

#include "PipelineResourceSignatureBase.hpp"
#include "SRBMemoryAllocator.hpp"

namespace Diligent
{

class RenderDeviceD3D12Impl;
class ShaderResourceCacheD3D12;
class ShaderVariableManagerD3D12;

/// Implementation of the Diligent::PipelineResourceSignatureD3D12Impl class
class PipelineResourceSignatureD3D12Impl final : public PipelineResourceSignatureBase<IPipelineResourceSignature, RenderDeviceD3D12Impl>
{
    friend class RootSignature;

public:
    using TPipelineResourceSignatureBase = PipelineResourceSignatureBase<IPipelineResourceSignature, RenderDeviceD3D12Impl>;

    PipelineResourceSignatureD3D12Impl(IReferenceCounters*                  pRefCounters,
                                       RenderDeviceD3D12Impl*               pDevice,
                                       const PipelineResourceSignatureDesc& Desc,
                                       bool                                 bIsDeviceInternal = false);
    ~PipelineResourceSignatureD3D12Impl();

    enum class CacheContentType
    {
        Signature = 0, // only static resources
        SRB       = 1  // in SRB
    };

    struct ResourceAttribs
    {
    private:
        static constexpr Uint32 _SpaceBits      = 1;
        static constexpr Uint32 _BindPointBits  = 16;
        static constexpr Uint32 _RootIndexBits  = 16;
        static constexpr Uint32 _SamplerIndBits = 16;

    public:
        static constexpr Uint32 InvalidSamplerInd = (1u << _SamplerIndBits) - 1;
        static constexpr Uint32 InvalidRootIndex  = (1u << _RootIndexBits) - 1;
        static constexpr Uint32 InvalidBindPoint  = (1u << _BindPointBits) - 1;

        // clang-format off
        const Uint32  BindPoint            : _BindPointBits;       // 
        const Uint32  RootIndex            : _RootIndexBits;       // Root index
        const Uint32  SamplerInd           : _SamplerIndBits;      // Index in m_Desc.Resources and m_pResourceAttribs
        const Uint32  Space                : _SpaceBits;           // Space index (0 or 1)
        const Uint32  SRBCacheOffset;                              // Offset in the SRB resource cache
        const Uint32  StaticCacheOffset;                           // Offset in the static resource cache
        // clang-format on

        ResourceAttribs(Uint32 _BindPoint,
                        Uint32 _RootIndex,
                        Uint32 _SamplerInd,
                        Uint32 _Space,
                        Uint32 _SRBCacheOffset,
                        Uint32 _StaticCacheOffset) noexcept :
            // clang-format off
            BindPoint        {_BindPoint        },
            RootIndex        {_RootIndex        },   
            SamplerInd       {_SamplerInd       },
            Space            {_Space            },
            SRBCacheOffset   {_SRBCacheOffset   },
            StaticCacheOffset{_StaticCacheOffset}
        // clang-format on
        {
            VERIFY(BindPoint == _BindPoint, "Bind point (", _BindPoint, ") exceeds maximum representable value");
            VERIFY(RootIndex == _RootIndex, "Root index (", _RootIndex, ") exceeds maximum representable value");
            VERIFY(SamplerInd == _SamplerInd, "Sampler index (", _SamplerInd, ") exceeds maximum representable value");
            VERIFY(Space == _Space, "Space (", Space, ") exceeds maximum representable value");
        }

        Uint32 CacheOffset(CacheContentType CacheType) const
        {
            return CacheType == CacheContentType::SRB ? SRBCacheOffset : StaticCacheOffset;
        }
    };

    const ResourceAttribs& GetResourceAttribs(Uint32 ResIndex) const
    {
        VERIFY_EXPR(ResIndex < m_Desc.NumResources);
        return m_pResourceAttribs[ResIndex];
    }

    const PipelineResourceDesc& GetResourceDesc(Uint32 ResIndex) const
    {
        VERIFY_EXPR(ResIndex < m_Desc.NumResources);
        return m_Desc.Resources[ResIndex];
    }

    virtual void DILIGENT_CALL_TYPE CreateShaderResourceBinding(IShaderResourceBinding** ppShaderResourceBinding,
                                                                bool                     InitStaticResources) override final;

    virtual IShaderResourceVariable* DILIGENT_CALL_TYPE GetStaticVariableByName(SHADER_TYPE ShaderType, const Char* Name) override final;

    virtual IShaderResourceVariable* DILIGENT_CALL_TYPE GetStaticVariableByIndex(SHADER_TYPE ShaderType, Uint32 Index) override final;

    virtual Uint32 DILIGENT_CALL_TYPE GetStaticVariableCount(SHADER_TYPE ShaderType) const override final;

    virtual void DILIGENT_CALL_TYPE BindStaticResources(Uint32            ShaderFlags,
                                                        IResourceMapping* pResourceMapping,
                                                        Uint32            Flags) override final;

    virtual bool DILIGENT_CALL_TYPE IsCompatibleWith(const IPipelineResourceSignature* pPRS) const override final
    {
        VERIFY_EXPR(pPRS != nullptr);
        return IsCompatibleWith(*ValidatedCast<const PipelineResourceSignatureD3D12Impl>(pPRS));
    }

    bool IsCompatibleWith(const PipelineResourceSignatureD3D12Impl& Other) const;

    bool IsIncompatibleWith(const PipelineResourceSignatureD3D12Impl& Other) const
    {
        return GetHash() != Other.GetHash();
    }

    SRBMemoryAllocator& GetSRBMemoryAllocator()
    {
        return m_SRBMemAllocator;
    }

    void InitResourceCache(ShaderResourceCacheD3D12& ResourceCache,
                           IMemoryAllocator&         CacheMemAllocator,
                           const char*               DbgPipelineName) const;

    void InitializeStaticSRBResources(ShaderResourceCacheD3D12& ResourceCache) const;

    static String GetPrintName(const PipelineResourceDesc& ResDesc, Uint32 ArrayInd);

private:
    class RootParameter
    {
    public:
        RootParameter(D3D12_ROOT_PARAMETER_TYPE     ParameterType,
                      Uint32                        RootIndex,
                      UINT                          Register,
                      UINT                          RegisterSpace,
                      D3D12_SHADER_VISIBILITY       Visibility,
                      SHADER_RESOURCE_VARIABLE_TYPE VarType) noexcept;

        RootParameter(D3D12_ROOT_PARAMETER_TYPE     ParameterType,
                      Uint32                        RootIndex,
                      UINT                          Register,
                      UINT                          RegisterSpace,
                      UINT                          NumDwords,
                      D3D12_SHADER_VISIBILITY       Visibility,
                      SHADER_RESOURCE_VARIABLE_TYPE VarType) noexcept;

        RootParameter(D3D12_ROOT_PARAMETER_TYPE     ParameterType,
                      Uint32                        RootIndex,
                      UINT                          NumRanges,
                      D3D12_DESCRIPTOR_RANGE*       pRanges,
                      D3D12_SHADER_VISIBILITY       Visibility,
                      SHADER_RESOURCE_VARIABLE_TYPE VarType) noexcept;

        RootParameter(const RootParameter& RP) noexcept;

        RootParameter(const RootParameter&    RP,
                      UINT                    NumRanges,
                      D3D12_DESCRIPTOR_RANGE* pRanges) noexcept;

        RootParameter& operator=(const RootParameter&) = delete;
        RootParameter& operator=(RootParameter&&) = delete;

        void SetDescriptorRange(UINT                        RangeIndex,
                                D3D12_DESCRIPTOR_RANGE_TYPE Type,
                                UINT                        Register,
                                UINT                        Count,
                                UINT                        Space                = 0,
                                UINT                        OffsetFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND);

        SHADER_RESOURCE_VARIABLE_TYPE GetShaderVariableType() const { return m_ShaderVarType; }

        Uint32 GetDescriptorTableSize() const;

        D3D12_SHADER_VISIBILITY   GetShaderVisibility() const { return m_RootParam.ShaderVisibility; }
        D3D12_ROOT_PARAMETER_TYPE GetParameterType() const { return m_RootParam.ParameterType; }

        Uint32 GetLocalRootIndex() const { return m_RootIndex; }

        operator const D3D12_ROOT_PARAMETER&() const { return m_RootParam; }

        bool operator==(const RootParameter& rhs) const;
        bool operator!=(const RootParameter& rhs) const { return !(*this == rhs); }

        size_t GetHash() const;

    private:
        SHADER_RESOURCE_VARIABLE_TYPE m_ShaderVarType       = static_cast<SHADER_RESOURCE_VARIABLE_TYPE>(-1);
        D3D12_ROOT_PARAMETER          m_RootParam           = {};
        Uint32                        m_DescriptorTableSize = 0;
        Uint32                        m_RootIndex           = static_cast<Uint32>(-1);
    };

    class RootParamsManager
    {
    public:
        RootParamsManager(IMemoryAllocator& MemAllocator);

        // clang-format off
        RootParamsManager           (const RootParamsManager&) = delete;
        RootParamsManager& operator=(const RootParamsManager&) = delete;
        RootParamsManager           (RootParamsManager&&)      = delete;
        RootParamsManager& operator=(RootParamsManager&&)      = delete;
        // clang-format on

        Uint32 GetNumRootTables() const { return m_NumRootTables; }
        Uint32 GetNumRootViews() const { return m_NumRootViews; }

        const RootParameter& GetRootTable(Uint32 TableInd) const
        {
            VERIFY_EXPR(TableInd < m_NumRootTables);
            return m_pRootTables[TableInd];
        }

        RootParameter& GetRootTable(Uint32 TableInd)
        {
            VERIFY_EXPR(TableInd < m_NumRootTables);
            return m_pRootTables[TableInd];
        }

        const RootParameter& GetRootView(Uint32 ViewInd) const
        {
            VERIFY_EXPR(ViewInd < m_NumRootViews);
            return m_pRootViews[ViewInd];
        }

        RootParameter& GetRootView(Uint32 ViewInd)
        {
            VERIFY_EXPR(ViewInd < m_NumRootViews);
            return m_pRootViews[ViewInd];
        }

        void AddRootView(D3D12_ROOT_PARAMETER_TYPE     ParameterType,
                         Uint32                        RootIndex,
                         UINT                          Register,
                         D3D12_SHADER_VISIBILITY       Visibility,
                         SHADER_RESOURCE_VARIABLE_TYPE VarType);

        void AddRootTable(Uint32                        RootIndex,
                          D3D12_SHADER_VISIBILITY       Visibility,
                          SHADER_RESOURCE_VARIABLE_TYPE VarType,
                          Uint32                        NumRangesInNewTable = 1);

        void AddDescriptorRanges(Uint32 RootTableInd, Uint32 NumExtraRanges = 1);

        template <class TOperation>
        void ProcessRootTables(TOperation) const;

        bool operator==(const RootParamsManager& RootParams) const;

    private:
        size_t GetRequiredMemorySize(Uint32 NumExtraRootTables,
                                     Uint32 NumExtraRootViews,
                                     Uint32 NumExtraDescriptorRanges) const;

        D3D12_DESCRIPTOR_RANGE* Extend(Uint32 NumExtraRootTables,
                                       Uint32 NumExtraRootViews,
                                       Uint32 NumExtraDescriptorRanges,
                                       Uint32 RootTableToAddRanges = static_cast<Uint32>(-1));

        IMemoryAllocator&                                         m_MemAllocator;
        std::unique_ptr<void, STDDeleter<void, IMemoryAllocator>> m_pMemory;
        Uint32                                                    m_NumRootTables         = 0;
        Uint32                                                    m_NumRootViews          = 0;
        Uint32                                                    m_TotalDescriptorRanges = 0;
        RootParameter*                                            m_pRootTables           = nullptr;
        RootParameter*                                            m_pRootViews            = nullptr;
    };

    using CacheOffsetsType = std::array<Uint32, 2>;

    void CreateLayout(const CacheOffsetsType& CacheSizes);

    // Allocates root signature slot for the given resource.
    // For graphics and compute pipelines, BindPoint is the same as the original bind point.
    // For ray-tracing pipeline, BindPoint will be overriden. Bind points are then
    // remapped by PSO constructor.
    void AllocateResourceSlot(SHADER_TYPE                   ShaderType,
                              D3D12_SHADER_VISIBILITY       Visibility,
                              SHADER_RESOURCE_VARIABLE_TYPE VariableType,
                              D3D12_DESCRIPTOR_RANGE_TYPE   RangeType,
                              Uint32                        ArraySize,
                              Uint32                       BindPoint,
                              Uint32                       Space,
                              Uint32&                       RootIndex,
                              Uint32&                       OffsetFromTableStart);

    size_t CalculateHash() const;

    void Destruct();

private:
    static constexpr Uint8  InvalidRootTableIndex    = static_cast<Uint8>(-1);
    static constexpr Uint32 MAX_SPACES_PER_SIGNATURE = 128;

    ResourceAttribs* m_pResourceAttribs = nullptr; // [m_Desc.NumResources]

    SHADER_TYPE m_ShaderStages = SHADER_TYPE_UNKNOWN;

    std::array<Int8, MAX_SHADERS_IN_PIPELINE> m_StaticVarIndex = {-1, -1, -1, -1, -1, -1};
    static_assert(MAX_SHADERS_IN_PIPELINE == 6, "Please update the initializer list above");

    // The array below contains array index of a CBV/SRV/UAV root table
    // in m_RootParams (NOT the Root Index!), for every variable type
    // (static, mutable, dynamic) and every shader type,
    // or -1, if the table is not yet assigned to the combination
    std::array<Uint8, SHADER_RESOURCE_VARIABLE_TYPE_NUM_TYPES* MAX_SHADERS_IN_PIPELINE> m_SrvCbvUavRootTablesMap = {};
    // This array contains the same data for Sampler root table
    std::array<Uint8, SHADER_RESOURCE_VARIABLE_TYPE_NUM_TYPES* MAX_SHADERS_IN_PIPELINE> m_SamplerRootTablesMap = {};

    // Resource counters for every descriptor range type used to assign bind points
    // for ray-tracing shaders.
    std::array<Uint32, D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER + 1> m_NumResources = {};

    Uint32 m_NumSpaces = 0;

    // The number of shader stages that have resources.
    Uint8 m_NumShaderStages = 0;

    ShaderResourceCacheD3D12*   m_pResourceCache = nullptr;
    ShaderVariableManagerD3D12* m_StaticVarsMgrs = nullptr; // [m_NumShaderStages]

    RootParamsManager m_RootParams;

    SRBMemoryAllocator m_SRBMemAllocator;
};


} // namespace Diligent
