#pragma once

#include "JC/Render.h"

#include "JC/Config.h"
#include "JC/Log.h"

#include <d3d12.h>
#include <dxgi1_6.h>

namespace JC::Render {

//--------------------------------------------------------------------------------------------------

static Arena*                     temp;
static Arena*                     perm;
static Log*                       log;
static ID3D12Debug*               d3d12Debug;
static ID3D12Device*              d3d12Device;
static ID3D12CommandQueue*        d3d12CommandQueue;
static IDXGISwapChain3*           dxgiSwapChain3;
static ID3D12Resource*            ;
static ID3D12CommandAllocator*    ;
static ID3D12DescriptorHeap*      ;
static ID3D12PipelineState*       ;
static ID3D12GraphicsCommandList* ;

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

	CheckHr(dxgiFactory6->CreateSwapChainForHwnd(
		d3d12CommandQueue,
		initInfo->hwnd,
		&dxgiSwapChainDesc,
		&dxgiSwapChainFullscreenDesc,
		&dxgiSwapChain3


	
	dxgiFactory6->Release();

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

}	// namespace JC::Render