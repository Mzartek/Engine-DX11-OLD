#include <Engine/ShadowMap.hpp>

engine::ShadowMap::ShadowMap()
{
	// Texture
	_pTexture = NULL;
	// Shader Resource View
	_pShaderResourceView = NULL;
	// View
	_pDepthView = NULL;
	// State
	_pDepthState = NULL;
	_pRasterizerState = NULL;
	_pSamplerComparisonState = NULL;
}

engine::ShadowMap::~ShadowMap()
{
	// Texture
	if (_pTexture) _pTexture->Release();
	// Shader Resource View
	if (_pShaderResourceView) _pShaderResourceView->Release();
	// View
	if (_pDepthView) _pDepthView->Release();
	// State
	if (_pDepthState) _pDepthState->Release();
	if (_pRasterizerState) _pRasterizerState->Release();
	if (_pSamplerComparisonState) _pSamplerComparisonState->Release();
}

void engine::ShadowMap::config(const UINT &width, const UINT &height, ID3D11Device *pd3dDevice, ID3D11DeviceContext *pContext)
{
	HRESULT hr;

	// Texture
	if (_pTexture) _pTexture->Release();
	// Shader Resource View
	if (_pShaderResourceView) _pShaderResourceView->Release();
	// View
	if (_pDepthView) _pDepthView->Release();
	// State
	if (_pDepthState) _pDepthState->Release();
	if (_pRasterizerState) _pRasterizerState->Release();
	if (_pSamplerComparisonState) _pSamplerComparisonState->Release();

	_width = width;
	_height = height;
	_pd3dDevice = pd3dDevice;
	_pContext = pContext;

	D3D11_TEXTURE2D_DESC descTexture;
	descTexture.Width = _width;
	descTexture.Height = _height;
	descTexture.MipLevels = 1;
	descTexture.ArraySize = 1;
	descTexture.SampleDesc.Count = 1;
	descTexture.SampleDesc.Quality = 0;
	descTexture.Usage = D3D11_USAGE_DEFAULT;
	descTexture.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_DEPTH_STENCIL;
	descTexture.CPUAccessFlags = 0;
	descTexture.MiscFlags = 0;

	D3D11_SHADER_RESOURCE_VIEW_DESC descShaderResourceView;
	descShaderResourceView.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
	descShaderResourceView.Texture2D.MostDetailedMip = 0;
	descShaderResourceView.Texture2D.MipLevels = 1;

	D3D11_DEPTH_STENCIL_VIEW_DESC descDepthView;
	descDepthView.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
	descDepthView.Flags = 0;
	descDepthView.Texture2D.MipSlice = 0;
	
	descTexture.Format = DXGI_FORMAT_R24G8_TYPELESS;
	descShaderResourceView.Format = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
	descDepthView.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	hr = _pd3dDevice->CreateTexture2D(&descTexture, NULL, &_pTexture);
	if (FAILED(hr))
	{
		MessageBox(NULL, "Failed to create Depth Texture", "GBuffer", NULL);
		exit(1);
	}
	hr = _pd3dDevice->CreateShaderResourceView(_pTexture, &descShaderResourceView, &_pShaderResourceView);
	if (FAILED(hr))
	{
		MessageBox(NULL, "Failed to create Depth Resource View", "GBuffer", NULL);
		exit(1);
	}
	hr = _pd3dDevice->CreateDepthStencilView(_pTexture, &descDepthView, &_pDepthView);
	if (FAILED(hr))
	{
		MessageBox(NULL, "Failed to create Depth Render View", "GBuffer", NULL);
		exit(1);
	}
	
	// State
	D3D11_DEPTH_STENCIL_DESC descDepth;
	descDepth.DepthEnable = TRUE;
	descDepth.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
	descDepth.DepthFunc = D3D11_COMPARISON_LESS;
	descDepth.StencilEnable = FALSE;
	hr = _pd3dDevice->CreateDepthStencilState(&descDepth, &_pDepthState);
	if (FAILED(hr))
	{
		MessageBox(NULL, "Failed to create DepthStencil State", "ShadowMap", NULL);
		exit(1);
	}

	D3D11_RASTERIZER_DESC descRasterizer;
	descRasterizer.FillMode = D3D11_FILL_SOLID;
	descRasterizer.CullMode = D3D11_CULL_NONE;
	descRasterizer.FrontCounterClockwise = TRUE;
	descRasterizer.DepthBias = 0;
	descRasterizer.DepthBiasClamp = 0.0f;
	descRasterizer.SlopeScaledDepthBias = 0.0f;
	descRasterizer.DepthClipEnable = TRUE;
	descRasterizer.ScissorEnable = FALSE;
	descRasterizer.MultisampleEnable = FALSE;
	descRasterizer.AntialiasedLineEnable = FALSE;
	hr = _pd3dDevice->CreateRasterizerState(&descRasterizer, &_pRasterizerState);
	if (FAILED(hr))
	{
		MessageBox(NULL, "Failed to create Rasterizer State", "ShadowMap", NULL);
		exit(1);
	}

	D3D11_SAMPLER_DESC descSampler;
	descSampler.Filter = D3D11_FILTER_COMPARISON_MIN_POINT_MAG_MIP_LINEAR;
	descSampler.AddressU = D3D11_TEXTURE_ADDRESS_BORDER;
	descSampler.AddressV = D3D11_TEXTURE_ADDRESS_BORDER;
	descSampler.AddressW = D3D11_TEXTURE_ADDRESS_BORDER;
	descSampler.ComparisonFunc = D3D11_COMPARISON_LESS_EQUAL;
	descSampler.MipLODBias = 0.0f;
	descSampler.MaxAnisotropy = 1;
	descSampler.BorderColor[0] = 1.0f;
	descSampler.BorderColor[1] = 1.0f;
	descSampler.BorderColor[2] = 1.0f;
	descSampler.BorderColor[3] = 1.0f;
	descSampler.MinLOD = 0;
	descSampler.MaxLOD = D3D11_FLOAT32_MAX;
	hr = pd3dDevice->CreateSamplerState(&descSampler, &_pSamplerComparisonState);
	if (FAILED(hr))
	{
		MessageBox(NULL, "Error while creating the SamplerState", "ShadowMap", MB_OK);
		exit(1);
	}

	// Create the Viewport
	_VP.TopLeftX = 0.0f;
	_VP.TopLeftY = 0.0f;
	_VP.Width = (FLOAT)_width;
	_VP.Height = (FLOAT)_height;
	_VP.MinDepth = 0.0f;
	_VP.MaxDepth = 1.0f;
}

ID3D11ShaderResourceView *engine::ShadowMap::getShaderResourceView(void) const
{
	return _pShaderResourceView;
}

ID3D11SamplerState *engine::ShadowMap::getSamplerComparisonState(void) const
{
	return _pSamplerComparisonState;
}

void engine::ShadowMap::setState(void) const
{
	_pContext->OMSetRenderTargets(0, NULL, _pDepthView);
	_pContext->OMSetDepthStencilState(_pDepthState, 0);
	_pContext->OMSetBlendState(NULL, NULL, 0xFFFFFFFF);
	_pContext->RSSetState(_pRasterizerState);
	_pContext->RSSetViewports(1, &_VP);
}

void engine::ShadowMap::clear(void) const
{
	_pContext->ClearDepthStencilView(_pDepthView, D3D11_CLEAR_DEPTH, 1.0f, 0);
}