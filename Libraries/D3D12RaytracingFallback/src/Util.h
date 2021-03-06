//*********************************************************
//
// Copyright (c) Microsoft. All rights reserved.
// This code is licensed under the MIT License (MIT).
// THIS CODE IS PROVIDED *AS IS* WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING ANY
// IMPLIED WARRANTIES OF FITNESS FOR A PARTICULAR
// PURPOSE, MERCHANTABILITY, OR NON-INFRINGEMENT.
//
//*********************************************************
#pragma once

#define ThrowInternalFailure(expression) ThrowFailure(expression, L"Unexpected internal Failure: " #expression)

inline void ThrowFailure(HRESULT hr, LPCWSTR errorString = nullptr)
{
    if (FAILED(hr))
    {
        if (errorString)
        {
            OutputDebugString(L"\n");
            OutputDebugString(L"D3D12 Raytracing Fallback Error: ");
            OutputDebugString(errorString);
            OutputDebugString(L"\n");
        }
        throw _com_error(hr);
    }
}

#define SizeOfInUint32(obj) ((sizeof(obj) - 1) / sizeof(UINT32) + 1)

template <typename T>
T DivideAndRoundUp(T dividend, T divisor) { return (dividend - 1) / divisor + 1; }

__forceinline uint8_t Log2(uint64_t value)
{
    unsigned long mssb; // most significant set bit
    unsigned long lssb; // least significant set bit

                        // If perfect power of two (only one set bit), return index of bit.  Otherwise round up
                        // fractional log by adding 1 to most signicant set bit's index.
    if (_BitScanReverse64(&mssb, value) > 0 && _BitScanForward64(&lssb, value) > 0)
        return uint8_t(mssb + (mssb == lssb ? 0 : 1));
    else
        return 0;
}

template <typename T> T AlignPowerOfTwo(T value)
{
    return value == 0 ? 0 : 1 << Log2(value);
}

static void CreateRootSignatureHelper(ID3D12Device *pDevice, D3D12_VERSIONED_ROOT_SIGNATURE_DESC &desc, ID3D12RootSignature **ppRootSignature)
{
    CComPtr<ID3DBlob> pRootSignatureBlob;
    ThrowInternalFailure(::D3D12SerializeVersionedRootSignature(&desc, &pRootSignatureBlob, nullptr));

    ThrowInternalFailure(pDevice->CreateRootSignature(1, pRootSignatureBlob->GetBufferPointer(), pRootSignatureBlob->GetBufferSize(),
        IID_PPV_ARGS(ppRootSignature)));
}

#define COMPILED_SHADER(bytecodeArray) CD3DX12_SHADER_BYTECODE((void*)bytecodeArray, sizeof(bytecodeArray))

static void CreatePSOHelper(
    ID3D12Device *pDevice, 
    UINT nodeMask, 
    ID3D12RootSignature *pRootSignature, 
    const D3D12_SHADER_BYTECODE &byteCode, 
    ID3D12PipelineState **ppPSO)
{
    D3D12_COMPUTE_PIPELINE_STATE_DESC psoDesc = {};
    psoDesc.NodeMask = nodeMask;
    psoDesc.pRootSignature = pRootSignature;
    psoDesc.CS = byteCode;
    ThrowFailure(pDevice->CreateComputePipelineState(&psoDesc, IID_PPV_ARGS(ppPSO)));
}

static bool IsVertexBufferFormatSupported(DXGI_FORMAT format)
{
    switch (format)
    {
    case DXGI_FORMAT_R32G32B32_FLOAT:
    case DXGI_FORMAT_R32G32B32A32_FLOAT:
        return true;
    default:
        return false;
    }
}

static bool IsIndexBufferFormatSupported(DXGI_FORMAT format)
{
    switch (format)
    {
    case DXGI_FORMAT_R32_UINT:
    case DXGI_FORMAT_R16_UINT:
    case DXGI_FORMAT_UNKNOWN:
        return true;
    default:
        return false;
    }
}

template<typename RAYTRACING_ACCELERATION_STRUCTURE_DESC>
static const D3D12_RAYTRACING_GEOMETRY_DESC &GetGeometryDesc(const typename RAYTRACING_ACCELERATION_STRUCTURE_DESC &desc, UINT geometryIndex)
{
    switch (desc.DescsLayout)
    {
    case D3D12_ELEMENTS_LAYOUT_ARRAY:
        return desc.pGeometryDescs[geometryIndex];
    case D3D12_ELEMENTS_LAYOUT_ARRAY_OF_POINTERS:
        return *desc.ppGeometryDescs[geometryIndex];
    default:
        ThrowFailure(E_INVALIDARG, L"Unexpected value for D3D12_ELEMENTS_LAYOUT");
        return *(D3D12_RAYTRACING_GEOMETRY_DESC *)nullptr;
    }
}

static UINT GetTriangleCountFromGeometryDesc(const D3D12_RAYTRACING_GEOMETRY_DESC &geometryDesc)
{
    if (geometryDesc.Type == D3D12_RAYTRACING_GEOMETRY_TYPE_PROCEDURAL_PRIMITIVE_AABBS)
    {
        ThrowFailure(E_NOTIMPL,
            L"Intersection shaders are not currently supported. This error was thrown due to the use of D3D12_RAYTRACING_GEOMETRY_TYPE_PROCEDURAL_PRIMITIVE_AABBS");
    }

    const D3D12_RAYTRACING_GEOMETRY_TRIANGLES_DESC &triangles = geometryDesc.Triangles;
    if (!IsIndexBufferFormatSupported(triangles.IndexFormat))
    {
        ThrowFailure(E_NOTIMPL, L"Unsupported index buffer format provided");
    }

    const bool bNullIndexBuffer = (triangles.IndexFormat == DXGI_FORMAT_UNKNOWN);
    const UINT vertexCount = bNullIndexBuffer ? triangles.VertexCount : triangles.IndexCount;
    if (vertexCount % 3 != 0)
    {
        ThrowFailure(E_INVALIDARG, bNullIndexBuffer ?
            L"Invalid vertex count provided, must be a multiple of 3 when there is no index buffer since geometry is always a triangle list" :
            L"Invalid index count provided, must be a multiple of 3 since geometry is always a triangle list"
        );
    }
    return vertexCount / 3;
}

template<typename RAYTRACING_ACCELERATION_STRUCTURE_DESC>
static UINT GetTotalTriangleCount(const typename RAYTRACING_ACCELERATION_STRUCTURE_DESC &desc)
{
    UINT totalTriangles = 0;
    for (UINT elementIndex = 0; elementIndex < desc.NumDescs; elementIndex++)
    {
        const D3D12_RAYTRACING_GEOMETRY_DESC &geometryDesc = GetGeometryDesc(desc, elementIndex);
        totalTriangles += GetTriangleCountFromGeometryDesc(geometryDesc);
    }
    return totalTriangles;
}
