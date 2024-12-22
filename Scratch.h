#include "JC/Common.h"

#include "JC/Array.h"
#include "JC/Math.h"

namespace JC {

//--------------------------------------------------------------------------------------------------
64 32-bit dwords

so 4 float matrices

each 32-bit entry can either be a:
- constnat
- descriptor
- table

enum D3D12_ROOT_PARAMETER_TYPE {
	D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS,
	D3D12_ROOT_PARAMETER_TYPE_CBV,
	D3D12_ROOT_PARAMETER_TYPE_SRV,
	D3D12_ROOT_PARAMETER_TYPE_UAV
	D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE = 0,
};

enum D3D12_DESCRIPTOR_RANGE_TYPE {
	D3D12_DESCRIPTOR_RANGE_TYPE_SRV = 0,
	D3D12_DESCRIPTOR_RANGE_TYPE_UAV,
	D3D12_DESCRIPTOR_RANGE_TYPE_CBV,
	D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER
};

struct D3D12_DESCRIPTOR_RANGE1 {
	D3D12_DESCRIPTOR_RANGE_TYPE  RangeType;
	UINT                         NumDescriptors;
	UINT                         BaseShaderRegister;
	UINT                         RegisterSpace;
	D3D12_DESCRIPTOR_RANGE_FLAGS Flags;
	UINT                         OffsetInDescriptorsFromTableStart;
}

struct D3D12_ROOT_PARAMETER1 {
	D3D12_ROOT_PARAMETER_TYPE ParameterType;
	union {
		D3D12_ROOT_CONSTANTS Constants {
			UINT ShaderRegister;
			UINT RegisterSpace;
			UINT Num32BitValues;
		};
		D3D12_ROOT_DESCRIPTOR1 Descriptor {
			UINT                        ShaderRegister;
			UINT                        RegisterSpace;
			D3D12_ROOT_DESCRIPTOR_FLAGS Flags;
		};
		D3D12_ROOT_DESCRIPTOR_TABLE1 DescriptorTable {
			UINT                          NumDescriptorRanges;
			const D3D12_DESCRIPTOR_RANGE1 *pDescriptorRanges;
		};
	};
	D3D12_SHADER_VISIBILITY   ShaderVisibility;
};

struct D3D12_ROOT_SIGNATURE_DESC1 {
	UINT                            NumParameters;
	const D3D12_ROOT_PARAMETER1     *pParameters;
	UINT                            NumStaticSamplers;
	const D3D12_STATIC_SAMPLER_DESC *pStaticSamplers;
	D3D12_ROOT_SIGNATURE_FLAGS      Flags;
};

struct D3D12_VERSIONED_ROOT_SIGNATURE_DESC {
	D3D_ROOT_SIGNATURE_VERSION Version;
	D3D12_ROOT_SIGNATURE_DESC1 Desc_1_1;
};
//--------------------------------------------------------------------------------------------------

}	// namespace JC