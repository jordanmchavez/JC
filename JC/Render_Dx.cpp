#pragma once

#include "JC/Render.h"

#include "JC/Config.h"
#include "JC/Log.h"
#include "JC/Window.h"

#include <d3d12.h>
#include <d3dcompiler.h>
#include <dxgi1_6.h>

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "dxgi.lib")

// TODO: remove me
#include "JC/FS.h"

namespace JC::Render {

//--------------------------------------------------------------------------------------------------

static constexpr u32 Frames = 3;

static Arena*                     temp;
static Arena*                     perm;
static Log*                       log;
static ID3D12Debug*               d3d12Debug;
static ID3D12Device*              d3d12Device;
static ID3D12CommandQueue*        d3d12CommandQueue;
static IDXGISwapChain3*           dxgiSwapChain3;
static u32                        swapChainWidth;
static u32                        swapChainHeight;
static ID3D12DescriptorHeap*      d3d12RtvDescriptorHeap;
static ID3D12DescriptorHeap*      d3d12SrvDescriptorHeap;
static UINT                       rtvDescriptorSize;
static ID3D12Resource*            d3d12BackBuffers[Frames];
static ID3D12CommandAllocator*    d3d12CommandAllocator;
static ID3D12RootSignature*       d3d12RootSignature;
static ID3D12PipelineState*       d3d12PipelineState;
static ID3D12GraphicsCommandList* d3d12GraphicsCommandList;
static ID3D12Resource*            d3d12VertexBuffer;
static D3D12_VERTEX_BUFFER_VIEW   d3d12VertexBufferView;
static ID3D12Resource*            d3d12Texture;

static ID3D12Fence*               d3d12Fence;
static UINT                       fenceValue;
static HANDLE                     fenceHandle;
static u32                        frameIdx;

//--------------------------------------------------------------------------------------------------

template <class... A>
Err* MakeHrErr(ArenaSrcLoc arenaSl, HRESULT hr, s8 fn, A... args) {
	return VMakeErr(arenaSl.arena, 0, arenaSl.sl, ErrCode { .ns = "d3d", .code = (i32)hr }, MakeVArgs("fn", fn, args...));
}

#define CheckHr(expr) \
	if (const HRESULT hr = expr; hr != S_OK) { \
		return VMakeErr(temp, 0, SrcLoc::Here(), ErrCode { .ns = "d3d", .code = (i32)hr }, MakeVArgs("expr", #expr)); \
	}
//--------------------------------------------------------------------------------------------------

Res<> Init(const InitInfo* initInfo) {
	temp = initInfo->temp;
	perm = initInfo->perm;
	log  = initInfo->log;

	UINT dxgiFactoryFlags = 0;

	#if defined RenderDebug
		CheckHr(D3D12GetDebugInterface(IID_PPV_ARGS(&d3d12Debug)));
		d3d12Debug->EnableDebugLayer();
		dxgiFactoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
	#endif	// RenderDebug

	IDXGIFactory6* dxgiFactory6 = 0;
	CheckHr(CreateDXGIFactory2(dxgiFactoryFlags, IID_PPV_ARGS(&dxgiFactory6)));

	IDXGIAdapter1* dxgiAdapter1 = 0;
	// TODO: check for 
	for (UINT adapterIndex = 0; SUCCEEDED(dxgiFactory6->EnumAdapterByGpuPreference(adapterIndex, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, IID_PPV_ARGS(&dxgiAdapter1))); adapterIndex++) {
		DXGI_ADAPTER_DESC1 dxgiAdapterDesc1;
		dxgiAdapter1->GetDesc1(&dxgiAdapterDesc1);
		if (!(dxgiAdapterDesc1.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)) {
			break;
		}
		dxgiAdapter1->Release();
	}

	CheckHr(D3D12CreateDevice(dxgiAdapter1, D3D_FEATURE_LEVEL_12_0, IID_PPV_ARGS(&d3d12Device)));

	const D3D12_COMMAND_QUEUE_DESC d3d12CommandQueueDesc = {
		.Type     = D3D12_COMMAND_LIST_TYPE_DIRECT,
		.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL,
		.Flags    = D3D12_COMMAND_QUEUE_FLAG_NONE,
		.NodeMask = 0,
	};
	CheckHr(d3d12Device->CreateCommandQueue(&d3d12CommandQueueDesc, IID_PPV_ARGS(&d3d12CommandQueue)));
	constexpr DXGI_FORMAT dxgiSwapChainFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
	DXGI_SWAP_CHAIN_DESC1 dxgiSwapChainDesc1 = {
		.Width       = initInfo->width,
		.Height      = initInfo->height,
		.Format      = dxgiSwapChainFormat,
		.Stereo      = FALSE,
		.SampleDesc  = { .Count = 1, .Quality = 0 },
		.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT,
		.BufferCount = 3,
		.Scaling     = DXGI_SCALING_STRETCH,
		.SwapEffect  = DXGI_SWAP_EFFECT_FLIP_DISCARD,
		.AlphaMode   = DXGI_ALPHA_MODE_UNSPECIFIED,
		.Flags       = 0,
	};
	IDXGISwapChain1* dxgiSwapChain1 = 0;
	CheckHr(dxgiFactory6->CreateSwapChainForHwnd(
		d3d12CommandQueue,
		(HWND)initInfo->windowPlatformInfo->hwnd,
		&dxgiSwapChainDesc1,
		0,
		0,
		&dxgiSwapChain1
	));
	CheckHr(dxgiSwapChain1->QueryInterface(IID_PPV_ARGS(&dxgiSwapChain3)));
	CheckHr(dxgiFactory6->MakeWindowAssociation((HWND)initInfo->windowPlatformInfo->hwnd, DXGI_MWA_NO_ALT_ENTER));
	swapChainWidth  = initInfo->width;
	swapChainHeight = initInfo->height;

	frameIdx = dxgiSwapChain3->GetCurrentBackBufferIndex();

	{
		const D3D12_DESCRIPTOR_HEAP_DESC d3d12DescriptorHeapDesc = {
			.Type           = D3D12_DESCRIPTOR_HEAP_TYPE_RTV,
			.NumDescriptors = 3,
			.Flags          = D3D12_DESCRIPTOR_HEAP_FLAG_NONE,
			.NodeMask       = 0,
		};
		CheckHr(d3d12Device->CreateDescriptorHeap(&d3d12DescriptorHeapDesc, IID_PPV_ARGS(&d3d12RtvDescriptorHeap)));
	}
	
	{
		const D3D12_DESCRIPTOR_HEAP_DESC d3d12DescriptorHeapDesc = {
			.Type           = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
			.NumDescriptors = 1,
			.Flags          = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE,
			.NodeMask       = 0,
		};
		CheckHr(d3d12Device->CreateDescriptorHeap(&d3d12DescriptorHeapDesc, IID_PPV_ARGS(&d3d12SrvDescriptorHeap)));
	}
	
	rtvDescriptorSize = d3d12Device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	D3D12_CPU_DESCRIPTOR_HANDLE d3d12CpuDescriptorHandle = d3d12RtvDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
	for (u32 i = 0; i < Frames; i++) {
		CheckHr(dxgiSwapChain3->GetBuffer(i, IID_PPV_ARGS(&d3d12BackBuffers[i])));
		d3d12Device->CreateRenderTargetView(d3d12BackBuffers[i], 0, d3d12CpuDescriptorHandle);
		d3d12CpuDescriptorHandle.ptr += rtvDescriptorSize;
	}

	CheckHr(d3d12Device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&d3d12CommandAllocator)));







	// LoadAssets() begins here

	D3D12_FEATURE_DATA_ROOT_SIGNATURE d3d12FeatureDataRootSignature = { .HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_1 };
	if (FAILED(d3d12Device->CheckFeatureSupport(D3D12_FEATURE_ROOT_SIGNATURE, &d3d12FeatureDataRootSignature, sizeof(d3d12FeatureDataRootSignature)))) {
		d3d12FeatureDataRootSignature.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_0;
	}
	const D3D12_DESCRIPTOR_RANGE1 d3d12DescriptorRange1 = {
		.RangeType                         = D3D12_DESCRIPTOR_RANGE_TYPE_SRV,
		.NumDescriptors                    = 1,
		.BaseShaderRegister                = 0,
		.RegisterSpace                     = 0,
		.Flags                             = D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC,
		.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND,	// 0xffffffff
	};
	const D3D12_ROOT_PARAMETER1 d3d12RootParameter1 = {
		.ParameterType           = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE,
		.DescriptorTable         = {
			.NumDescriptorRanges = 1,
			.pDescriptorRanges   = &d3d12DescriptorRange1,
		},
		.ShaderVisibility        = D3D12_SHADER_VISIBILITY_ALL,
	};
	const D3D12_STATIC_SAMPLER_DESC d3d12StaticSamplerDesc = {
		.Filter           = D3D12_FILTER_MIN_MAG_MIP_POINT,
		.AddressU         = D3D12_TEXTURE_ADDRESS_MODE_BORDER,
		.AddressV         = D3D12_TEXTURE_ADDRESS_MODE_BORDER,
		.AddressW         = D3D12_TEXTURE_ADDRESS_MODE_BORDER,
		.MipLODBias       = 0.0f,
		.MaxAnisotropy    = 0,
		.ComparisonFunc   = D3D12_COMPARISON_FUNC_NEVER,
		.BorderColor      = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK,
		.MinLOD           = 0.0f,
		.MaxLOD           = D3D12_FLOAT32_MAX,
		.ShaderRegister   = 0,
		.RegisterSpace    = 0,
		.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL,
	};
	const D3D12_VERSIONED_ROOT_SIGNATURE_DESC d3d12VersionedRootSignatureDesc = {
		.Version                = D3D_ROOT_SIGNATURE_VERSION_1_1,
		.Desc_1_1               = {
			.NumParameters      = 1,
			.pParameters        = &d3d12RootParameter1,
			.NumStaticSamplers  = 1,
			.pStaticSamplers    = &d3d12StaticSamplerDesc,
			.Flags              = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT,
		},
	};
	ID3DBlob* d3dSignatureBlob = 0;	// TODO: free me
	ID3DBlob* d3dErrorBlob = 0;	// TODO: free me
	// TODO: what are the differences between the various root signature versions?
	CheckHr(D3D12SerializeVersionedRootSignature(
		&d3d12VersionedRootSignatureDesc,
		&d3dSignatureBlob,
		&d3dErrorBlob
	));
	CheckHr(d3d12Device->CreateRootSignature(
		0,
		d3dSignatureBlob->GetBufferPointer(),
		d3dSignatureBlob->GetBufferSize(),
		IID_PPV_ARGS(&d3d12RootSignature)
	));
	if (d3dSignatureBlob) { d3dSignatureBlob->Release(); }
	if (d3dErrorBlob) { d3dErrorBlob->Release(); }


	// TODO: we're not destroying our temp resources in this call, like dxgi factory, on error
	Span<u8> shaderData = {};
	if (Res<> r = FS::ReadAll(temp, "Shaders/shader.hlsl").To(shaderData); !r) {
		return r;
	}
	#if defined RenderDebug
		constexpr UINT compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
	#else	// RenderDebug
		constexpr UINT compileFlags = 0;
	#endif	// RenderDebug
	ID3DBlob* d3dVertexShaderCodeBlob = 0;
	ID3DBlob* d3dVertexShaderErrorBlob = 0;
	if (const HRESULT hr = D3DCompile(
		shaderData.data,
		shaderData.len,
		"Shaders/shader.hlsl",
		0,
		0,
		"VSMain",
		"vs_5_0",
		compileFlags,
		0,
		&d3dVertexShaderCodeBlob,
		&d3dVertexShaderErrorBlob
	); !SUCCEEDED(hr)) {
		const s8 errorStr((const char*)d3dVertexShaderErrorBlob->GetBufferPointer(), d3dVertexShaderErrorBlob->GetBufferSize());
		return MakeHrErr(temp, hr, "D3DCompile", "errorstr", errorStr);
	}

	ID3DBlob* d3dPixelShaderCodeBlob = 0;
	ID3DBlob* d3dPixelShaderErrorBlob = 0;
	if (const HRESULT hr = D3DCompile(
		shaderData.data,
		shaderData.len,
		"Shaders/shader.hlsl",
		0,
		0,
		"PSMain",
		"ps_5_0",
		compileFlags,
		0,
		&d3dPixelShaderCodeBlob,
		&d3dPixelShaderErrorBlob
	); !SUCCEEDED(hr)) {
		const s8 errorStr((const char*)d3dVertexShaderErrorBlob->GetBufferPointer(), d3dVertexShaderErrorBlob->GetBufferSize());
		return MakeHrErr(temp, hr, "D3DCompile", "errorstr", errorStr);
	}

	const D3D12_RENDER_TARGET_BLEND_DESC d3d12RenderTargetBlendDesc = {
		.BlendEnable            = FALSE,
		.LogicOpEnable          = FALSE,
		.SrcBlend               = D3D12_BLEND_ONE,
		.DestBlend              = D3D12_BLEND_ZERO,
		.BlendOp                = D3D12_BLEND_OP_ADD,
		.SrcBlendAlpha          = D3D12_BLEND_ONE,
		.DestBlendAlpha         = D3D12_BLEND_ZERO,
		.BlendOpAlpha           = D3D12_BLEND_OP_ADD,
		.LogicOp                = D3D12_LOGIC_OP_NOOP,
		.RenderTargetWriteMask  = D3D12_COLOR_WRITE_ENABLE_ALL,
	};
	const D3D12_INPUT_ELEMENT_DESC d3d12InputElementDescs[2] = {
		{
			.SemanticName         = "POSITION",
			.SemanticIndex        = 0,
			.Format               = DXGI_FORMAT_R32G32B32_FLOAT,
			.InputSlot            = 0,
			.AlignedByteOffset    = 0,
			.InputSlotClass       = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
			.InstanceDataStepRate = 0,
		},
		{
			.SemanticName         = "TEXCOORD",
			.SemanticIndex        = 0,
			.Format               = DXGI_FORMAT_R32G32_FLOAT,
			.InputSlot            = 0,
			.AlignedByteOffset    = 12,
			.InputSlotClass       = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
			.InstanceDataStepRate = 0,
		},
	};
	const D3D12_GRAPHICS_PIPELINE_STATE_DESC d3d12GraphicsPipelineStateDesc = {
		.pRootSignature             = d3d12RootSignature,
		.VS                         = { d3dVertexShaderCodeBlob->GetBufferPointer(), d3dVertexShaderCodeBlob->GetBufferSize() },
		.PS                         = { d3dPixelShaderCodeBlob->GetBufferPointer(),  d3dPixelShaderCodeBlob->GetBufferSize() },
		.DS                         = {},
		.HS                         = {},
		.GS                         = {},
		.StreamOutput               = {},
		.BlendState                 = {
			.AlphaToCoverageEnable  = FALSE,
			.IndependentBlendEnable = FALSE,
			.RenderTarget           = { d3d12RenderTargetBlendDesc },
		},
		.SampleMask                 = U32Max,
		.RasterizerState            = {
			.FillMode               = D3D12_FILL_MODE_SOLID,
			.CullMode               = D3D12_CULL_MODE_NONE,
			.FrontCounterClockwise  = FALSE,
			.DepthBias              = 0,
			.DepthBiasClamp         = 0.0F,
			.SlopeScaledDepthBias   = 0.0F,
			.DepthClipEnable        = FALSE,
			.MultisampleEnable      = FALSE,
			.AntialiasedLineEnable  = FALSE,
			.ForcedSampleCount      = 0,
			.ConservativeRaster     = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF,
		},
		.DepthStencilState          = {
			.DepthEnable            = FALSE,
			.DepthWriteMask         = D3D12_DEPTH_WRITE_MASK_ALL,
			.DepthFunc              = D3D12_COMPARISON_FUNC_LESS,
			.StencilEnable          = FALSE,
			.StencilReadMask        = 0,
			.StencilWriteMask       = 0,
			.FrontFace              = {
				.StencilFailOp      = D3D12_STENCIL_OP_KEEP,
				.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP,
				.StencilPassOp      = D3D12_STENCIL_OP_KEEP,
				.StencilFunc        = D3D12_COMPARISON_FUNC_NEVER,
			},
			.BackFace               = {
				.StencilFailOp      = D3D12_STENCIL_OP_KEEP,
				.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP,
				.StencilPassOp      = D3D12_STENCIL_OP_KEEP,
				.StencilFunc        = D3D12_COMPARISON_FUNC_NEVER,
			},
		},
		.InputLayout                = {
			.pInputElementDescs     = d3d12InputElementDescs,
			.NumElements            = LenOf(d3d12InputElementDescs),
		},
		.IBStripCutValue            = D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_DISABLED,
		.PrimitiveTopologyType      = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE,
		.NumRenderTargets           = 1,
		.RTVFormats                 = { dxgiSwapChainFormat },
		.DSVFormat                  = DXGI_FORMAT_UNKNOWN,
		.SampleDesc                 = { .Count = 1, .Quality = 0 },
		.NodeMask                   = 0,
		.CachedPSO                  = {
			.pCachedBlob            = 0,
			.CachedBlobSizeInBytes  = 0,
		},
		.Flags                      = D3D12_PIPELINE_STATE_FLAG_NONE,
	};
	CheckHr(d3d12Device->CreateGraphicsPipelineState(&d3d12GraphicsPipelineStateDesc, IID_PPV_ARGS(&d3d12PipelineState)));


	CheckHr(d3d12Device->CreateCommandList(
		0,
		D3D12_COMMAND_LIST_TYPE_DIRECT,
		d3d12CommandAllocator,
		d3d12PipelineState,
		IID_PPV_ARGS(&d3d12GraphicsCommandList)
	));

	struct Vertex {
		float pos[3];
		float uv[2];
	};
	constexpr Vertex vertices[3] = {
		{ .pos = {  0.0f,  0.5f, 0.0f }, .uv = { 0.5f, 0.0f } },
		{ .pos = { -0.5f, -0.5f, 0.0f }, .uv = { 1.0f, 1.0f } },
		{ .pos = {  0.5f, -0.5f, 0.0f }, .uv = { 0.0f, 1.0f } },
	};
	{
		const D3D12_HEAP_PROPERTIES d3d12HeapProperties = {
			.Type                 = D3D12_HEAP_TYPE_UPLOAD,
			.CPUPageProperty      = D3D12_CPU_PAGE_PROPERTY_UNKNOWN,
			.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN,
			.CreationNodeMask     = 0,
			.VisibleNodeMask      = 0,

		};
		const D3D12_RESOURCE_DESC d3d12ResourceDesc = {
			.Dimension        = D3D12_RESOURCE_DIMENSION_BUFFER,
			.Alignment        = 0,	// 64kb
			.Width            = sizeof(vertices),
			.Height           = 1,
			.DepthOrArraySize = 1,
			.MipLevels        = 1,
			.Format           = DXGI_FORMAT_UNKNOWN,
			.SampleDesc       = { .Count = 1, .Quality = 0 },
			.Layout           = D3D12_TEXTURE_LAYOUT_ROW_MAJOR,
			.Flags            = D3D12_RESOURCE_FLAG_NONE,
		};
		CheckHr(d3d12Device->CreateCommittedResource(
			&d3d12HeapProperties,
			D3D12_HEAP_FLAG_NONE,
			&d3d12ResourceDesc,
			D3D12_RESOURCE_STATE_COMMON,	// TODO: GENERIC_READ is a combined flag, can we do better?
			0,	// optimized clear value, must be 0 for buffers
			IID_PPV_ARGS(&d3d12VertexBuffer)
		));
	}

	{
		void* mappedPtr = 0;
		CheckHr(d3d12VertexBuffer->Map(0, 0, &mappedPtr));
		MemCpy(mappedPtr, &vertices, sizeof(vertices));
		d3d12VertexBuffer->Unmap(0, 0);
	}

	d3d12VertexBufferView = {
		.BufferLocation = d3d12VertexBuffer->GetGPUVirtualAddress(),
		.SizeInBytes = sizeof(vertices),
		.StrideInBytes = sizeof(Vertex),
	};

	constexpr u32 TextureWidth  = 256;
	constexpr u32 TextureHeight = 256;
	D3D12_PLACED_SUBRESOURCE_FOOTPRINT d3d12PlacedSubresourceFootprint = {};
	u32 textureUploadNumRows = 0;
	u64 textureUploadRowSize = 0;
	u64 textureUploadSize = 0;
	{
		const D3D12_HEAP_PROPERTIES d3d12HeapProperties = {
			.Type                 = D3D12_HEAP_TYPE_DEFAULT,
			.CPUPageProperty      = D3D12_CPU_PAGE_PROPERTY_UNKNOWN,
			.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN,
			.CreationNodeMask     = 0,
			.VisibleNodeMask      = 0,

		};
		const D3D12_RESOURCE_DESC d3d12ResourceDesc = {
			.Dimension        = D3D12_RESOURCE_DIMENSION_TEXTURE2D,
			.Alignment        = 0,	// 64kb
			.Width            = TextureWidth,
			.Height           = TextureHeight,
			.DepthOrArraySize = 1,
			.MipLevels        = 1,
			.Format           = DXGI_FORMAT_R8G8B8A8_UNORM,
			.SampleDesc       = { .Count = 1, .Quality = 0 },
			.Layout           = D3D12_TEXTURE_LAYOUT_UNKNOWN,
			.Flags            = D3D12_RESOURCE_FLAG_NONE,
		};
		CheckHr(d3d12Device->CreateCommittedResource(
			&d3d12HeapProperties,
			D3D12_HEAP_FLAG_NONE,
			&d3d12ResourceDesc,
			D3D12_RESOURCE_STATE_COPY_DEST,
			0,	// optimized clear value, must be 0 for buffers
			IID_PPV_ARGS(&d3d12Texture)
		));
		d3d12Device->GetCopyableFootprints(&d3d12ResourceDesc, 0, 1, 0, &d3d12PlacedSubresourceFootprint, &textureUploadNumRows, &textureUploadRowSize, &textureUploadSize);
	}

	ID3D12Resource* d3d12UploadBuffer = 0;
	{
		const D3D12_HEAP_PROPERTIES d3d12HeapProperties = {
			.Type                 = D3D12_HEAP_TYPE_UPLOAD,
			.CPUPageProperty      = D3D12_CPU_PAGE_PROPERTY_UNKNOWN,
			.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN,
			.CreationNodeMask     = 0,
			.VisibleNodeMask      = 0,

		};
		const D3D12_RESOURCE_DESC d3d12ResourceDesc = {
			.Dimension        = D3D12_RESOURCE_DIMENSION_BUFFER,
			.Alignment        = 0,	// 64kb
			.Width            = textureUploadSize,
			.Height           = 1,
			.DepthOrArraySize = 1,
			.MipLevels        = 1,
			.Format           = DXGI_FORMAT_UNKNOWN,
			.SampleDesc       = { .Count = 1, .Quality = 0 },
			.Layout           = D3D12_TEXTURE_LAYOUT_ROW_MAJOR,
			.Flags            = D3D12_RESOURCE_FLAG_NONE,
		};
		CheckHr(d3d12Device->CreateCommittedResource(
			&d3d12HeapProperties,
			D3D12_HEAP_FLAG_NONE,
			&d3d12ResourceDesc,
			D3D12_RESOURCE_STATE_COMMON,	// TODO: can we do better? this is an or'd bitmask
			0,	// optimized clear value, must be 0 for buffers
			IID_PPV_ARGS(&d3d12UploadBuffer)
		));

		u32* textureData = temp->AllocT<u32>(256 * 256);
		for (u32 y = 0; y < 256; y++) {
			for (u32 x = 0; x < 256; x++) {
				if (x % 16 == 0 || y % 16 == 0) {
					textureData[y * 256 + x] = 0;
				} else {
					const u32 r = x;
					const u32 g = 0xff;
					const u32 b = y;
					textureData[y * 256 + x] = r | (g << 8) | (b << 16) | 0xff000000;
				}
			}
		}

		{
			void* mappedPtr = 0;
			CheckHr(d3d12UploadBuffer->Map(0, 0, &mappedPtr));
			for (u32 y = 0; y < 256; y++) {
				MemCpy(
					((u8*)mappedPtr) + y * d3d12PlacedSubresourceFootprint.Footprint.RowPitch,
					textureData + y * 256,
					256 * 4
				);
			}
			d3d12UploadBuffer->Unmap(0, 0);
		}
		
		const D3D12_TEXTURE_COPY_LOCATION d3d12DstTextureCopyLocation = {
			.pResource        = d3d12Texture,
			.Type             = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX,
			.SubresourceIndex = 0,
		};
		const D3D12_TEXTURE_COPY_LOCATION d3d12SrcTextureCopyLocation = {
			.pResource       = d3d12UploadBuffer,
			.Type            = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT,
			.PlacedFootprint = d3d12PlacedSubresourceFootprint,
		};
		d3d12GraphicsCommandList->CopyTextureRegion(
			&d3d12DstTextureCopyLocation,
			0,
			0,
			0,
			&d3d12SrcTextureCopyLocation,
			0
		);
	}

	const D3D12_RESOURCE_BARRIER d3d12ResourceBarrier = {
		.Type            = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION,
		.Flags           = D3D12_RESOURCE_BARRIER_FLAG_NONE,
		.Transition      = {
			.pResource   = d3d12Texture,
			.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,
			.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST,
			.StateAfter  = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,	// TODO: why D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE?
		},
	};
	d3d12GraphicsCommandList->ResourceBarrier(1, &d3d12ResourceBarrier);

	const D3D12_SHADER_RESOURCE_VIEW_DESC d3d12ShaderResourceViewDesc = {
		.Format                   = DXGI_FORMAT_R8G8B8A8_UNORM,
		.ViewDimension            = D3D12_SRV_DIMENSION_TEXTURE2D,
		.Shader4ComponentMapping  = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING,
		.Texture2D                = {
			.MostDetailedMip      = 0,
			.MipLevels            = 1,
			.PlaneSlice           = 0,
			.ResourceMinLODClamp  = 0.0f,
		},
	};
	d3d12Device->CreateShaderResourceView(d3d12Texture, &d3d12ShaderResourceViewDesc, d3d12SrvDescriptorHeap->GetCPUDescriptorHandleForHeapStart());

	CheckHr(d3d12GraphicsCommandList->Close());

	ID3D12CommandList* d3d12CommandLists[] = { d3d12GraphicsCommandList };
	d3d12CommandQueue->ExecuteCommandLists(LenOf(d3d12CommandLists), d3d12CommandLists);

	CheckHr(d3d12Device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&d3d12Fence)));
	fenceValue = 1;
	fenceHandle = CreateEventW(0, FALSE, FALSE, 0);

	dxgiFactory6->Release();


	d3d12CommandQueue->Signal(d3d12Fence, fenceValue);
	const UINT oldFenceValue = fenceValue;
	fenceValue++;

	if (d3d12Fence->GetCompletedValue() < oldFenceValue) {
		CheckHr(d3d12Fence->SetEventOnCompletion(oldFenceValue, fenceHandle));
		WaitForSingleObject(fenceHandle, INFINITE);
	}

	frameIdx = dxgiSwapChain3->GetCurrentBackBufferIndex();


	return Ok();
}

//--------------------------------------------------------------------------------------------------

#define ReleaseDx(x) if (x) { x->Release(); x = 0; }

void Shutdown() {
	ReleaseDx(d3d12CommandQueue);
	ReleaseDx(d3d12Device);
	ReleaseDx(d3d12Debug);
}

//--------------------------------------------------------------------------------------------------

Res<> BeginFrame() {
	CheckHr(d3d12CommandAllocator->Reset());
	CheckHr(d3d12GraphicsCommandList->Reset(d3d12CommandAllocator, d3d12PipelineState));

	d3d12GraphicsCommandList->SetGraphicsRootSignature(d3d12RootSignature);

	d3d12GraphicsCommandList->SetDescriptorHeaps(1, &d3d12SrvDescriptorHeap);
	d3d12GraphicsCommandList->SetGraphicsRootDescriptorTable(0, d3d12SrvDescriptorHeap->GetGPUDescriptorHandleForHeapStart());

	const D3D12_VIEWPORT d3d12Viewport = {
		.TopLeftX = 0.0f,
		.TopLeftY = 0.0f,
		.Width    = (float)swapChainWidth,
		.Height   = (float)swapChainHeight,
		.MinDepth = D3D12_MIN_DEPTH,	// 0.0f
		.MaxDepth = D3D12_MAX_DEPTH,	// 1.0f
	};
	d3d12GraphicsCommandList->RSSetViewports(1, &d3d12Viewport);
	const D3D12_RECT d3d12ScissorRect = {
		.left   = 0,
		.top    = 0,
		.right  = (LONG)swapChainWidth,
		.bottom = (LONG)swapChainHeight,	
	};
	d3d12GraphicsCommandList->RSSetScissorRects(1, &d3d12ScissorRect);

	{
		const D3D12_RESOURCE_BARRIER d3d12ResourceBarrier = {
			.Type            = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION,
			.Flags           = D3D12_RESOURCE_BARRIER_FLAG_NONE,
			.Transition      = {
				.pResource   = d3d12BackBuffers[frameIdx],
				.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,
				.StateBefore = D3D12_RESOURCE_STATE_PRESENT,
				.StateAfter  = D3D12_RESOURCE_STATE_RENDER_TARGET,
			},
		};
		d3d12GraphicsCommandList->ResourceBarrier(1, &d3d12ResourceBarrier);
	}

	D3D12_CPU_DESCRIPTOR_HANDLE d3d12CpuDescriptorHandle = d3d12RtvDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
	d3d12CpuDescriptorHandle.ptr += frameIdx * rtvDescriptorSize;
	d3d12GraphicsCommandList->OMSetRenderTargets(1, &d3d12CpuDescriptorHandle, FALSE, 0);

	const f32 clearColor[4] = { 0.0f, 0.2f, 0.4f, 1.0f };
	d3d12GraphicsCommandList->ClearRenderTargetView(d3d12CpuDescriptorHandle, clearColor, 0, 0);

	d3d12GraphicsCommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	d3d12GraphicsCommandList->IASetVertexBuffers(0, 1, &d3d12VertexBufferView);
	d3d12GraphicsCommandList->DrawInstanced(3, 1, 0, 0);

	{
		const D3D12_RESOURCE_BARRIER d3d12ResourceBarrier = {
			.Type            = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION,
			.Flags           = D3D12_RESOURCE_BARRIER_FLAG_NONE,
			.Transition      = {
				.pResource   = d3d12BackBuffers[frameIdx],
				.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,
				.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET,
				.StateAfter  = D3D12_RESOURCE_STATE_PRESENT,
			},
		};
		d3d12GraphicsCommandList->ResourceBarrier(1, &d3d12ResourceBarrier);
	}

	CheckHr(d3d12GraphicsCommandList->Close());

	ID3D12CommandList* d3d12CommandLists[] = { d3d12GraphicsCommandList };
	d3d12CommandQueue->ExecuteCommandLists(LenOf(d3d12CommandLists), d3d12CommandLists);

	CheckHr(dxgiSwapChain3->Present(1, 0));

	d3d12CommandQueue->Signal(d3d12Fence, fenceValue);
	const UINT oldFenceValue = fenceValue;
	fenceValue++;

	if (d3d12Fence->GetCompletedValue() < oldFenceValue) {
		CheckHr(d3d12Fence->SetEventOnCompletion(oldFenceValue, fenceHandle));
		WaitForSingleObject(fenceHandle, INFINITE);
	}

	frameIdx = dxgiSwapChain3->GetCurrentBackBufferIndex();

	return Ok();
}

//--------------------------------------------------------------------------------------------------

Res<> EndFrame() {

	return Ok();
}

//--------------------------------------------------------------------------------------------------

}	// namespace JC::Render