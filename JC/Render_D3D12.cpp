#pragma once

#include "JC/Render.h"

#include "JC/Bit.h"
#include "JC/Config.h"
#include "JC/HandleArray.h"
#include "JC/Log.h"
#include "JC/Math.h"
#include "JC/Window.h"

#include <d3d12.h>
#include <d3dcompiler.h>
#include <dxgi1_6.h>

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "dx2gi.lib")

// TODO: remove me
#include "JC/FS.h"

namespace JC::Render {

//--------------------------------------------------------------------------------------------------

template <class... A>
Err* MakeHrErr(ArenaSrcLoc arenaSl, HRESULT hr, s8 fn, A... args) {
	return VMakeErr(arenaSl.arena, 0, arenaSl.sl, ErrCode { .ns = "d3d", .code = (i32)hr }, MakeVArgs("fn", fn, args...));
}

#define CheckHr(expr) \
	if (const HRESULT hr = expr; hr != S_OK) { \
		return VMakeErr(temp, 0, SrcLoc::Here(), ErrCode { .ns = "d3d", .code = (i32)hr }, MakeVArgs("expr", #expr)); \
	}

#define ReleaseDx(x) if (x) { x->Release(); x = 0; }

//--------------------------------------------------------------------------------------------------

struct Descriptor {
	D3D12_CPU_DESCRIPTOR_HANDLE d3d12CpuDescriptorHandle = {};
	D3D12_GPU_DESCRIPTOR_HANDLE d3d12GpuDescriptorHandle = {};
	U32                         idx                      = 0;
};

struct DescriptorHeap {
	ID3D12DescriptorHeap*       d3d12DescriptorHeap      = 0;
	D3D12_DESCRIPTOR_HEAP_TYPE  d3d12DescriptorHeapType  = {};
	U32                         handleInc                = 0;
	U32                         len                      = 0;
	U32                         cap                      = 0;
	D3D12_CPU_DESCRIPTOR_HANDLE d3d12CpuDescriptorHandle = {};
	D3D12_GPU_DESCRIPTOR_HANDLE d3d12GpuDescriptorHandle = {};

	Res<> Init(D3D12_DESCRIPTOR_HEAP_TYPE d3d12DescriptorHeapType, U32 cap, D3D12_DESCRIPTOR_HEAP_FLAGS d3d12DescriptorHeapFlags) {
		const D3D12_DESCRIPTOR_HEAP_DESC d3d12DescriptorHeapDesc = {
			.Type           = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
			.NumDescriptors = cap,
			.Flags          = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE,
			.NodeMask       = 0,
		};
		CheckHr(d3d12Device->CreateDescriptorHeap(&d3d12DescriptorHeapDesc, IID_PPV_ARGS(&d3d12DescriptorHeap)));
		d3d12DescriptorHeapType  = d3d12DescriptorHeapType;
		handleInc                = d3d12Device->GetDescriptorHandleIncrementSize(d3d12DescriptorHeapType);
		len                      = 0;
		cap                      = cap;
		d3d12CpuDescriptorHandle = d3d12DescriptorHeap->GetCPUDescriptorHandleForHeapStart();
		d3d12GpuDescriptorHandle = d3d12DescriptorHeap->GetGPUDescriptorHandleForHeapStart();

		return Ok();
	}

	Descriptor Alloc() {
		Assert(len < cap);

		const U32 idx = len++;
		return Descriptor {
			.d3d12CpuDescriptorHandle = d3d12CpuDescriptorHandle.ptr + (idx * handleInc),
			.d3d12GpuDescriptorHandle = d3d12GpuDescriptorHandle.ptr + (idx * handleInc),
			.idx                      = idx,
		};
	}
};

//--------------------------------------------------------------------------------------------------

Res<ID3D12Resource*> CreateBuffer(U32 size, D3D12_HEAP_TYPE d3d12HeapType) {
	const D3D12_HEAP_PROPERTIES d3d12HeapProperties = {
		.Type                 = D3D12_HEAP_TYPE_DEFAULT,
		.CPUPageProperty      = D3D12_CPU_PAGE_PROPERTY_UNKNOWN,
		.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN,
		.CreationNodeMask     = 0,
		.VisibleNodeMask      = 0,
	};
	const D3D12_RESOURCE_DESC d3d12ResourceDesc = {
		.Dimension        = D3D12_RESOURCE_DIMENSION_BUFFER,
		.Alignment        = 0,	// 64kb
		.Width            = size,
		.Height           = 1,
		.DepthOrArraySize = 1,
		.MipLevels        = 1,
		.Format           = DXGI_FORMAT_UNKNOWN,
		.SampleDesc       = { .Count = 1, .Quality = 0 },
		.Layout           = D3D12_TEXTURE_LAYOUT_ROW_MAJOR,
		.Flags            = D3D12_RESOURCE_FLAG_NONE,
	};
	ID3D12Resource* d3d12Buffer = 0;
	CheckHr(d3d12Device->CreateCommittedResource(
		&d3d12HeapProperties,
		D3D12_HEAP_FLAG_NONE,
		&d3d12ResourceDesc,
		D3D12_RESOURCE_STATE_COPY_DEST,
		0,	// optimized clear value, must be 0 for buffers
		IID_PPV_ARGS(&d3d12Buffer)
	));

	return d3d12Buffer;
}

//--------------------------------------------------------------------------------------------------

struct UploadMem {
	void* ptr  = 0;
	U32   size = 0;
};

struct UploadArena {
	ID3D12Resource* d3d12Buffer = 0;
	u8*             mappedPtr   = 0;
	U32             used        = 0;
	U32             cap         = 0;

	Res<> Init(U32 cap) {
		if (Res<> r = CreateBuffer(cap, D3D12_HEAP_TYPE_UPLOAD).To(d3d12Buffer); !r) {
			return r;
		}

		D3D12_RANGE d3d12Range = { .Begin = 0, .End = 0 };
		if (HRESULT hr = d3d12Buffer->Map(0, &d3d12Range, (void**)&mappedPtr); FAILED(hr)) {
			d3d12Buffer->Release();
			d3d12Buffer = 0;
		}

		used = 0;
		cap = cap;

		return Ok();
	}

	UploadMem Alloc(U32 size, U32 align) {
		const U32 alignedUsed = AlignUp(used, align);
		Assert(alignedUsed + size < cap);
		const UploadMem uploadMem = {
			.ptr  = mappedPtr + alignedUsed,
			.size = size,
		};
		used = alignedUsed + size;
		return uploadMem;
	}

	void Reset() {
		used = 0;
	}

	void Shutdown() {
		if (d3d12Buffer) {
			d3d12Buffer->Release();
			d3d12Buffer= 0;
		}
	}
};

//--------------------------------------------------------------------------------------------------

static constexpr U32 MaxFrames = 3;
static constexpr DXGI_FORMAT dxgiSwapChainFormat = DXGI_FORMAT_R8G8B8A8_UNORM;

struct Scene {
	Mat4 wvp;
};

struct Frame {
	U64                        fenceValue                   = 0;
	ID3D12CommandAllocator*    d3d12CommandAllocator        = 0;
	ID3D12GraphicsCommandList* d3d12CommandList             = 0;
	D3D12_RESOURCE_STATES      d3d12BackBufferResourceState = {};
	ID3D12Resource*            d3d12BackBuffer              = 0;
	Descriptor                 backBufferDescriptor         = {};
	UploadArena                uploadArena                  = {};
};

static Arena*                     temp;
static Arena*                     perm;
static Log*                       log;
static IDXGIFactory6*             dxgiFactory6;
static ID3D12Device*              d3d12Device;
static IDXGISwapChain3*           d3d12SwapChain3;
static U32                        swapChainWidth;
static U32                        swapChainHeight;
static Frame                      frames[MaxFrames];
static ID3D12CommandQueue*        d3d12CommandQueue;
static ID3D12RootSignature*       d3d12BindlessRootSignature;
static DescriptorHeap             cbvSrvUavDescriptorHeap;
static DescriptorHeap             rtvDescriptorHeap;

static ID3D12PipelineState*       d3d12PipelineState;
static ID3D12Resource*            d3d12VertexBuffer;
static D3D12_VERTEX_BUFFER_VIEW   d3d12VertexBufferView;
static ID3D12Resource*            d3d12Texture;
static ID3D12Resource*            d3d12ConstantBuffers[MaxFrames];
static void*                      constantBufferMappedPtrs[MaxFrames];
static ID3D12Fence*               d3d12Fence;
static UINT                       fenceValue;
static HANDLE                     fenceHandle;
static U32                        frameId3d12;
static Scene                      scene;

//--------------------------------------------------------------------------------------------------

Res<> InitDevice() {
	UINT dxgiFactoryFlags = 0;

	#if defined RenderDebug
		ID3D12Debug* d3d12Debug = 0;
		CheckHr(D3D12GetDebugInterface(IID_PPV_ARGS(&d3d12Debug)));
		d3d12Debug->EnableDebugLayer();
		dxgiFactoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
		ID3D12Debug1* d3d12Debug1;
		CheckHr(d3d12Debug->QueryInterface(IID_PPV_ARGS(&d3d12Debug1)));
		d3d12Debug1->SetEnableGPUBasedValidation(true);
		d3d12Debug1->Release();
		d3d12Debug->Release();
	#endif	// RenderDebug

	CheckHr(CreateDXGIFactory2(dxgiFactoryFlags, IID_PPV_ARGS(&dxgiFactory6)));

	IDXGIAdapter1* d3d12Adapter1 = 0;
	// TODO: check for 
	for (UINT adapterIndex = 0; SUCCEEDED(dxgiFactory6->EnumAdapterByGpuPreference(adapterIndex, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, IID_PPV_ARGS(&d3d12Adapter1))); adapterIndex++) {
		DXGI_ADAPTER_DESC1 d3d12AdapterDesc1;
		d3d12Adapter1->GetDesc1(&d3d12AdapterDesc1);
		if (!(d3d12AdapterDesc1.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)) {
			break;
		}
		d3d12Adapter1->Release();
	}

	CheckHr(D3D12CreateDevice(d3d12Adapter1, D3D_FEATURE_LEVEL_12_0, IID_PPV_ARGS(&d3d12Device)));

	return Ok();
}

//--------------------------------------------------------------------------------------------------

Res<> InitInfoQueue() {
	ID3D12InfoQueue* d3d12InfoQueue = 0;
	CheckHr(d3d12Device->QueryInterface(IID_PPV_ARGS(&d3d12InfoQueue)));
	d3d12InfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, TRUE);
	d3d12InfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, TRUE);
	d3d12InfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, TRUE);
	d3d12InfoQueue->Release();
	return Ok();
}

//--------------------------------------------------------------------------------------------------

Res<> InitSwapChain(U32 width, U32 height, HWND hwnd) {
	DXGI_SWAP_CHAIN_DESC1 d3d12SwapChainDesc1 = {
		.Width       = width,
		.Height      = height,
		.Format      = dxgiSwapChainFormat,
		.Stereo      = FALSE,
		.SampleDesc  = { .Count = 1, .Quality = 0 },
		.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT,
		.BufferCount = MaxFrames,
		.Scaling     = DXGI_SCALING_STRETCH,
		.SwapEffect  = DXGI_SWAP_EFFECT_FLIP_DISCARD,
		.AlphaMode   = DXGI_ALPHA_MODE_UNSPECIFIED,
		.Flags       = 0,
	};
	IDXGISwapChain1* d3d12SwapChain1 = 0;
	CheckHr(dxgiFactory6->CreateSwapChainForHwnd(
		d3d12CommandQueue,
		hwnd,
		&d3d12SwapChainDesc1,
		0,
		0,
		&d3d12SwapChain1
	));
	CheckHr(d3d12SwapChain1->QueryInterface(IID_PPV_ARGS(&d3d12SwapChain3)));
	swapChainWidth  = width;
	swapChainHeight = height;

	frameId3d12 = d3d12SwapChain3->GetCurrentBackBufferIndex();

	for (U32 i = 0; i < MaxFrames; i++) {
		CheckHr(d3d12SwapChain3->GetBuffer(i, IID_PPV_ARGS(&frames[i].d3d12BackBuffer)));
		const D3D12_RENDER_TARGET_VIEW_DESC d3d12RenderTargetViewDesc = {
			.Format         = dxgiSwapChainFormat,
			.ViewDimension  = D3D12_RTV_DIMENSION_TEXTURE2D,
			.Texture2D      = {
				.MipSlice   = 0,
				.PlaneSlice = 0,
			},
		};
		frames[i].backBufferDescriptor = rtvDescriptorHeap.Alloc();
		d3d12Device->CreateRenderTargetView(frames[i].d3d12BackBuffer, 0, frames[i].backBufferDescriptor.d3d12CpuDescriptorHandle);
	}

	CheckHr(dxgiFactory6->MakeWindowAssociation(hwnd, DXGI_MWA_NO_ALT_ENTER));	// TODO: does this actualyl do anythign? tst without t
}

//--------------------------------------------------------------------------------------------------

Res<> InitCommands() {
	const D3D12_COMMAND_QUEUE_DESC d3d12CommandQueueDesc = {
		.Type     = D3D12_COMMAND_LIST_TYPE_DIRECT,
		.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL,
		.Flags    = D3D12_COMMAND_QUEUE_FLAG_NONE,
		.NodeMask = 0,
	};
	CheckHr(d3d12Device->CreateCommandQueue(&d3d12CommandQueueDesc, IID_PPV_ARGS(&d3d12CommandQueue)));

	for (U32 i = 0; i < MaxFrames; i++) {
		CheckHr(d3d12Device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&frames[i].d3d12CommandAllocator)));
		CheckHr(d3d12Device->CreateCommandList(
			0,
			D3D12_COMMAND_LIST_TYPE_DIRECT,
			frames[i].d3d12CommandAllocator,
			0,
			IID_PPV_ARGS(&frames[i].d3d12CommandList)
		));
		frames[i].d3d12CommandList->Close();
	}

	return Ok();
}

//--------------------------------------------------------------------------------------------------

Res<> InitDescriptors() {
	// TODO: view d3d hardware tiers and limits
	if (Res<> r = cbvSrvUavDescriptorHeap.Init(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 4096, D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE ); !r) { return r; }
	if (Res<> r = cbvSrvUavDescriptorHeap.Init(D3D12_DESCRIPTOR_HEAP_TYPE_RTV,         4096, D3D12_DESCRIPTOR_HEAP_FLAG_NONE           ); !r) { return r; }

	return Ok();
}

//--------------------------------------------------------------------------------------------------

Res<> InitUploadArenas() {
	for (U32 i = 0; i < MaxFrames; i++) {
		frames[i].uploadArena.Init(64 * 1024);
	}
	return Ok();
}

//--------------------------------------------------------------------------------------------------

Res<> InitRootSignature() {

	const D3D12_ROOT_PARAMETER1 d3d12RootParameters[4] = {
		{
			.ParameterType      = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS,
			.Constants          = {
				.ShaderRegister = 0,
				.RegisterSpace  = 0,
				.Num32BitValues = 58,	// 64*32 - 2*32 per root cbv
			},
			.ShaderVisibility   = D3D12_SHADER_VISIBILITY_ALL,
		},
		{
			.ParameterType      = D3D12_ROOT_PARAMETER_TYPE_CBV,
			.Descriptor         = {
				.ShaderRegister = 1,
				.RegisterSpace  = 0,
				.Flags          = D3D12_ROOT_DESCRIPTOR_FLAG_DATA_VOLATILE,
			},
			.ShaderVisibility   = D3D12_SHADER_VISIBILITY_ALL,
		},
		{
			.ParameterType      = D3D12_ROOT_PARAMETER_TYPE_CBV,
			.Descriptor         = {
				.ShaderRegister = 2,
				.RegisterSpace  = 0,
				.Flags          = D3D12_ROOT_DESCRIPTOR_FLAG_DATA_VOLATILE,
			},
			.ShaderVisibility   = D3D12_SHADER_VISIBILITY_ALL,
		},
		{
			.ParameterType      = D3D12_ROOT_PARAMETER_TYPE_CBV,
			.Descriptor         = {
				.ShaderRegister = 3,
				.RegisterSpace  = 0,
				.Flags          = D3D12_ROOT_DESCRIPTOR_FLAG_DATA_VOLATILE,
			},
			.ShaderVisibility   = D3D12_SHADER_VISIBILITY_ALL,
		},
	};
	const D3D12_STATIC_SAMPLER_DESC d3d12StaticSamplerDescs[] = {
		{
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
			.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL,
		},
	};
	const D3D12_VERSIONED_ROOT_SIGNATURE_DESC d3d12VersionedRootSignatureDesc = {
		.Version                = D3D_ROOT_SIGNATURE_VERSION_1_1,
		.Desc_1_1               = {
			.NumParameters      = LenOf(d3d12RootParameters),
			.pParameters        = d3d12RootParameters,
			.NumStaticSamplers  = LenOf(d3d12StaticSamplerDescs),
			.pStaticSamplers    = d3d12StaticSamplerDescs,
			.Flags              = D3D12_ROOT_SIGNATURE_FLAG_CBV_SRV_UAV_HEAP_DIRECTLY_INDEXED,
		},
	};
	ID3DBlob* d3dSignatureBlob = 0;
	// TODO: what are the differences between the various root signature versions?
	CheckHr(D3D12SerializeVersionedRootSignature(&d3d12VersionedRootSignatureDesc, &d3dSignatureBlob, 0));
	Defer { d3dSignatureBlob->Release(); };
	CheckHr(d3d12Device->CreateRootSignature(
		0,
		d3dSignatureBlob->GetBufferPointer(),
		d3dSignatureBlob->GetBufferSize(),
		IID_PPV_ARGS(&d3d12BindlessRootSignature)
	));

	return Ok();
}

//--------------------------------------------------------------------------------------------------

Res<> Init(const InitDesc* initDesc) {
	temp = initDesc->temp;
	perm = initDesc->perm;
	log  = initDesc->log;

	if (Res<> r = InitDevice(); !r) { return r; }
	if (Res<> r = InitInfoQueue(); !r) { return r; }
	if (Res<> r = InitSwapChain(initDesc->width, initDesc->height, (HWND)initDesc->windowPlatformDesc->hwnd); !r) { return r; }
	if (Res<> r = InitCommands(); !r) { return r; }
	if (Res<> r = InitDescriptors(); !r) { return r; }
	if (Res<> r = InitUploadArenas(); !r) { return r; }
	if (Res<> r = InitRootSignature(); !r) { return r; }

	// TODO: per-frame fences
	CheckHr(d3d12Device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&d3d12Fence)));
	fenceValue = 1;

	return Ok();
}

//--------------------------------------------------------------------------------------------------

struct PipelineObj {
	ID3D12PipelineState* d3d12PipelineState = 0;
};

static HandleArray<PipelineObj, Pipeline> pipelineObjs;

Res<Pipeline> CreateGraphicsPipeline(
	Shader              vertexShader,
	Shader              pixelShader,
	Span<TextureFormat> renderTargetFormats,
	TextureFormat       depthStencilFormat
) {
	ShaderObj* const vertexShaderObj = shaderObjs.Get(vertexShader);
	ShaderObj* const pixelShaderObj  = shaderObjs.Get(pixelShader);
	D3D12_GRAPHICS_PIPELINE_STATE_DESC d3d12GraphicsPipelineStateDesc = {
		.pRootSignature             = d3d12BindlessRootSignature,
		.VS                         = {
			.pShaderBytecode        = vertexShaderObj->d3dCodeBlob->GetBufferPointer(),
			.BytecodeLength         = vertexShaderObj->d3dCodeBlob->GetBufferSize(),
		},
		.PS                         = {
			.pShaderBytecode        = pixelShaderObj->d3dCodeBlob->GetBufferPointer(),
			.BytecodeLength         = pixelShaderObj->d3dCodeBlob->GetBufferSize(),
		},
		.DS                         = {},
		.HS                         = {},
		.GS                         = {},
		.StreamOutput               = {},
		.BlendState                 = {
			.AlphaToCoverageEnable  = FALSE,
			.IndependentBlendEnable = FALSE,
			.RenderTarget           = {},
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
			.pInputElementDescs     = 0,
			.NumElements            = 0,
		},
		.IBStripCutValue            = D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_DISABLED,
		.PrimitiveTopologyType      = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE,
		.NumRenderTargets           = 1,
		.RTVFormats                 = {},
		.DSVFormat                  = TextureFormatToDXGIFormat(depthStencilFormat),
		.SampleDesc                 = { .Count = 1, .Quality = 0 },
		.NodeMask                   = 0,
		.CachedPSO                  = {
			.pCachedBlob            = 0,
			.CachedBlobSizeInBytes  = 0,
		},
		.Flags                      = D3D12_PIPELINE_STATE_FLAG_NONE,
	};
	for (U64 i = 0; i < renderTargetFormats.len; i++) {
		d3d12GraphicsPipelineStateDesc.BlendState.RenderTarget[i] = {
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
		d3d12GraphicsPipelineStateDesc.RTVFormats[i] = TextureFormatToDXGIFormat(renderTargetFormats[i]);
	};
	CheckHr(d3d12Device->CreateGraphicsPipelineState(&d3d12GraphicsPipelineStateDesc, IID_PPV_ARGS(&d3d12PipelineState)));
}

//--------------------------------------------------------------------------------------------------

{
	{
		void* mappedPtr = 0;
		CheckHr(d3d12VertexUploadBuffer->Map(0, 0, &mappedPtr));
		MemCpy(mappedPtr, &vertices, sizeof(vertices));
		d3d12VertexBuffer->Unmap(0, 0);

		d3d12GraphicsCommandList->CopyBufferRegion(
			d3d12VertexBuffer,
			0,
			d3d12VertexUploadBuffer,
			0,
			sizeof(vertices)
		);
	}

	d3d12VertexBufferView = {
		.BufferLocation = d3d12VertexBuffer->GetGPUVirtualAddress(),
		.SizeInBytes = sizeof(vertices),
		.StrideInBytes = sizeof(Vertex),
	};

	constexpr U32 TextureWidth  = 256;
	constexpr U32 TextureHeight = 256;
	D3D12_PLACED_SUBRESOURCE_FOOTPRINT d3d12PlacedSubresourceFootprint = {};
	U32 textureUploadNumRows = 0;
	U64 textureUploadRowSize = 0;
	U64 textureUploadSize = 0;
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

		U32* textureData = temp->AllocT<U32>(256 * 256);
		for (U32 y = 0; y < 256; y++) {
			for (U32 x = 0; x < 256; x++) {
				if (x % 16 == 0 || y % 16 == 0) {
					textureData[y * 256 + x] = 0;
				} else {
					const U32 r = x;
					const U32 g = 0xff;
					const U32 b = y;
					textureData[y * 256 + x] = r | (g << 8) | (b << 16) | 0xff000000;
				}
			}
		}

		{
			void* mappedPtr = 0;
			CheckHr(d3d12UploadBuffer->Map(0, 0, &mappedPtr));
			for (U32 y = 0; y < 256; y++) {
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
	D3D12_CPU_DESCRIPTOR_HANDLE d3d12SrvCbvCpuDescriptorHandle = d3d12SrvCbvDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
	d3d12Device->CreateShaderResourceView(d3d12Texture, &d3d12ShaderResourceViewDesc, d3d12SrvCbvCpuDescriptorHandle);
	d3d12SrvCbvCpuDescriptorHandle.ptr += srvCbvDescriptorSize;

	for (U32 i = 0; i < MaxFrames; i++) {
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
			.Width            = AlignUp(sizeof(Scene), 256),
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
			D3D12_RESOURCE_STATE_GENERIC_READ,
			0,
			IID_PPV_ARGS(&d3d12ConstantBuffers[i])
		));

		D3D12_CONSTANT_BUFFER_VIEW_DESC d3d12ConstantBufferViewDesc = {
			.BufferLocation = d3d12ConstantBuffers[i]->GetGPUVirtualAddress(),
			.SizeInBytes    = (UINT)AlignUp(sizeof(Scene), 256),
		};
		d3d12Device->CreateConstantBufferView(&d3d12ConstantBufferViewDesc, d3d12SrvCbvCpuDescriptorHandle);
		d3d12SrvCbvCpuDescriptorHandle.ptr += srvCbvDescriptorSize;
		const D3D12_RANGE d3d12Range = { 0, 0 };
		CheckHr(d3d12ConstantBuffers[i]->Map(0, &d3d12Range, &constantBufferMappedPtrs[i]));
	}

	CheckHr(d3d12GraphicsCommandList->Close());

	ID3D12CommandList* d3d12CommandLists[] = { d3d12GraphicsCommandList };
	d3d12CommandQueue->ExecuteCommandLists(LenOf(d3d12CommandLists), d3d12CommandLists);

	fenceHandle = CreateEventW(0, FALSE, FALSE, 0);

	dxgiFactory6->Release();


	d3d12CommandQueue->Signal(d3d12Fence, fenceValue);
	const UINT oldFenceValue = fenceValue;
	fenceValue++;

	if (d3d12Fence->GetCompletedValue() < oldFenceValue) {
		CheckHr(d3d12Fence->SetEventOnCompletion(oldFenceValue, fenceHandle));
		WaitForSingleObject(fenceHandle, INFINITE);
	}

	frameId3d12 = d3d12SwapChain3->GetCurrentBackBufferIndex();


	return Ok();
}

//--------------------------------------------------------------------------------------------------

void Shutdown() {
	ReleaseDx(d3d12CommandQueue);
	ReleaseDx(d3d12Device);
}

//--------------------------------------------------------------------------------------------------

struct ShaderObj {
	ID3DBlob* d3dCodeBlob = 0;
};

static HandleArray<ShaderObj, Shader> shaderObjs;

Res<Shader> CreateShader(const void* data, U64 len, const char* entry) {
	#if defined RenderDebug
		constexpr UINT compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
	#else	// RenderDebug
		constexpr UINT compileFlags = 0;
	#endif	// RenderDebug
	ID3DBlob* d3dCodeBlob = 0;
	ID3DBlob* d3dErrorBlob = 0;
	if (const HRESULT hr = D3DCompile(
		data,
		len,
		0,
		0,
		0,
		entry,
		"vs_5_0",
		compileFlags,
		0,
		&d3dCodeBlob,
		&d3dErrorBlob
	); !SUCCEEDED(hr)) {
		ReleaseDx(d3dCodeBlob);
		const s8 errorStr((const char*)d3dErrorBlob->GetBufferPointer(), d3dErrorBlob->GetBufferSize());
		return MakeHrErr(temp, hr, "D3DCompile", "errorstr", errorStr);
	}
	ReleaseDx(d3dErrorBlob);

	const Shader shader = shaderObjs.Alloc();
	ShaderObj* const shaderObj = shaderObjs.Get(shader);
	shaderObj->d3dCodeBlob = d3dCodeBlob;
	return shader;
}

void DestroyShader(Shader shader) {
	if (shader.handle) {
		ShaderObj* const shaderObj = shaderObjs.Get(shader);
		ReleaseDx(shaderObj->d3dCodeBlob);
		shaderObjs.Free(shader);
	}
}

//--------------------------------------------------------------------------------------------------

Res<> BeginFrame() {
	scene.wvp = Mat4::Identity();
	MemCpy(constantBufferMappedPtr, &scene, sizeof(scene));

	CheckHr(d3d12CommandAllocator->Reset());
	CheckHr(d3d12GraphicsCommandList->Reset(d3d12CommandAllocator, d3d12PipelineState));

	d3d12GraphicsCommandList->SetGraphicsRootSignature(d3d12RootSignature);

	d3d12GraphicsCommandList->SetDescriptorHeaps(1, &d3d12SrvCbvDescriptorHeap);
	d3d12GraphicsCommandList->SetGraphicsRootDescriptorTable(1, d3d12SrvCbvDescriptorHeap->GetGPUDescriptorHandleForHeapStart());

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
				.pResource   = d3d12BackBuffers[frameId3d12],
				.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,
				.StateBefore = D3D12_RESOURCE_STATE_PRESENT,
				.StateAfter  = D3D12_RESOURCE_STATE_RENDER_TARGET,
			},
		};
		d3d12GraphicsCommandList->ResourceBarrier(1, &d3d12ResourceBarrier);
	}

	D3D12_CPU_DESCRIPTOR_HANDLE d3d12RtvCpuDescriptorHandle = d3d12RtvDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
	d3d12RtvCpuDescriptorHandle.ptr += frameId3d12 * rtvDescriptorSize;
	d3d12GraphicsCommandList->OMSetRenderTargets(1, &d3d12RtvCpuDescriptorHandle, FALSE, 0);

	const f32 clearColor[4] = { 0.0f, 0.2f, 0.4f, 1.0f };
	d3d12GraphicsCommandList->ClearRenderTargetView(d3d12RtvCpuDescriptorHandle, clearColor, 0, 0);

	d3d12GraphicsCommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	d3d12GraphicsCommandList->IASetVertexBuffers(0, 1, &d3d12VertexBufferView);
	d3d12GraphicsCommandList->DrawInstanced(3, 1, 0, 0);

	{
		const D3D12_RESOURCE_BARRIER d3d12ResourceBarrier = {
			.Type            = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION,
			.Flags           = D3D12_RESOURCE_BARRIER_FLAG_NONE,
			.Transition      = {
				.pResource   = d3d12BackBuffers[frameId3d12],
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

	CheckHr(d3d12SwapChain3->Present(1, 0));

	d3d12CommandQueue->Signal(d3d12Fence, fenceValue);
	const UINT oldFenceValue = fenceValue;
	fenceValue++;

	if (d3d12Fence->GetCompletedValue() < oldFenceValue) {
		CheckHr(d3d12Fence->SetEventOnCompletion(oldFenceValue, fenceHandle));
		WaitForSingleObject(fenceHandle, INFINITE);
	}

	frameId3d12 = d3d12SwapChain3->GetCurrentBackBufferIndex();

	return Ok();
}

//--------------------------------------------------------------------------------------------------

Res<> EndFrame() {

	return Ok();
}

//--------------------------------------------------------------------------------------------------

}	// namespace JC::Render