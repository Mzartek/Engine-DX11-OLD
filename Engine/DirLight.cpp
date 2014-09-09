#include <Engine/DirLight.hpp>

engine::DirLight::DirLight(void)
{
}

engine::DirLight::~DirLight(void)
{
}

void engine::DirLight::config(ShaderProgram *program, ID3D11Device *pd3dDevice, ID3D11DeviceContext *pContext)
{
	HRESULT hr;
	D3D11_BUFFER_DESC bd;
	D3D11_SUBRESOURCE_DATA data;

	_pContext = pContext;

	bd.Usage = D3D11_USAGE_DEFAULT;
	bd.CPUAccessFlags = 0;
	bd.MiscFlags = 0;
	bd.StructureByteStride = 0;

	// ShadowMatrix Buffer
	bd.ByteWidth = sizeof XMMATRIX;
	bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	hr = pd3dDevice->CreateBuffer(&bd, NULL, &_pShadowMatrixBuffer);
	if (FAILED(hr))
	{
		MessageBox(NULL, "Failed to create ShadowMatrix Buffer", "DirLight", MB_OK);
		exit(1);
	}

	// IVPMatrix Buffer
	bd.ByteWidth = sizeof XMMATRIX;
	bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	hr = pd3dDevice->CreateBuffer(&bd, NULL, &_pIVPMatrixBuffer);
	if (FAILED(hr))
	{
		MessageBox(NULL, "Failed to create IVPMatrix Buffer", "DirLight", MB_OK);
		exit(1);
	}

	// Screen Buffer
	bd.ByteWidth = 16;
	bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	hr = pd3dDevice->CreateBuffer(&bd, NULL, &_pScreenBuffer);
	if (FAILED(hr))
	{
		MessageBox(NULL, "Failed to create Screen Buffer", "DirLight", MB_OK);
		exit(1);
	}

	// Camera Buffer
	bd.ByteWidth = 16;
	bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	hr = pd3dDevice->CreateBuffer(&bd, NULL, &_pCameraBuffer);
	if (FAILED(hr))
	{
		MessageBox(NULL, "Failed to create Camera Buffer", "DirLight", MB_OK);
		exit(1);
	}

	// LightInfo Buffer
	bd.ByteWidth = sizeof _lightInfo;
	bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	hr = pd3dDevice->CreateBuffer(&bd, NULL, &_pLightInfoBuffer);
	if (FAILED(hr))
	{
		MessageBox(NULL, "Failed to create LightInfo Buffer", "DirLight", MB_OK);
		exit(1);
	}

	// Create and set the input layout
	D3D11_INPUT_ELEMENT_DESC layout[] =
	{
		{ "IN_POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
	};
	hr = pd3dDevice->CreateInputLayout(layout, ARRAYSIZE(layout),
		program->getEntryBufferPointer(), program->getEntryBytecodeLength(),
		&_pInputLayout);
	if (FAILED(hr))
	{
		MessageBox(NULL, "Failed to create Input Layout", "D3DObject", MB_OK);
		exit(1);
	}

	// Create Vertex Buffer
	FLOAT vertex[] = {
		-1, -1,
		1, -1,
		-1, 1,
		1, 1,
	};
	bd.ByteWidth = sizeof vertex;
	bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	data.pSysMem = vertex;
	data.SysMemPitch = 0;
	data.SysMemSlicePitch = 0;
	hr = pd3dDevice->CreateBuffer(&bd, &data, &_pVertexBuffer);
	if (FAILED(hr))
	{
		MessageBox(NULL, "Failed to create Vertex Buffer", "D3DObject", MB_OK);
		exit(1);
	}

	_program = program;
}

void engine::DirLight::setColor(const XMFLOAT3 &color)
{
	_lightInfo.color = color;
	_pContext->UpdateSubresource(_pLightInfoBuffer, 0, NULL, &_lightInfo, 0, 0);
}

void engine::DirLight::setDirection(const XMFLOAT3 &dir)
{
	_lightInfo.direction = dir;
	_pContext->UpdateSubresource(_pLightInfoBuffer, 0, NULL, &_lightInfo, 0, 0);
}

XMFLOAT3 engine::DirLight::getColor(void) const
{
	return _lightInfo.color;
}

XMFLOAT3 engine::DirLight::getDirection(void) const
{
	return _lightInfo.direction;
}

void engine::DirLight::activateShadowMapping(const BOOL &shadow)
{
	_lightInfo.withShadowMapping = shadow;
	_pContext->UpdateSubresource(_pLightInfoBuffer, 0, NULL, &_lightInfo, 0, 0);
}

void engine::DirLight::position(const XMFLOAT3 &position, const FLOAT &dim)
{
	XMVECTOR pos = XMVectorSet(position.x, position.y, position.z, 0.0f);
	XMVECTOR dir = XMVectorSet(_lightInfo.direction.x, _lightInfo.direction.y, _lightInfo.direction.z, 0.0f);

	*_VPMatrix = XMMatrixLookAtRH(pos - dir, pos, XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f)) *
		XMMatrixOrthographicOffCenterRH(-dim, dim, -dim, dim, -dim, dim);
}

void engine::DirLight::display(GBuffer *gbuf, Camera *cam)
{
	XMMATRIX tmp;

	gbuf->setLightConfig();

	gbuf->getContext()->VSSetShader(_program->getVertexShader(), NULL, 0);
	gbuf->getContext()->GSSetShader(_program->getGeometryShader(), NULL, 0);
	gbuf->getContext()->PSSetShader(_program->getPixelShader(), NULL, 0);
	gbuf->getContext()->IASetInputLayout(_pInputLayout);
	gbuf->getContext()->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

	ID3D11ShaderResourceView *gshr[]
	{
		gbuf->getShaderResourceView(GBUF_NORMAL),
		gbuf->getShaderResourceView(GBUF_MATERIAL),
		gbuf->getShaderResourceView(GBUF_DEPTH),
		gbuf->getShaderResourceView(GBUF_STENCIL),
	};
	gbuf->getContext()->PSSetShaderResources(0, ARRAYSIZE(gshr), gshr);

	// ShadowMap
	if (_lightInfo.withShadowMapping == TRUE)
	{
		ID3D11ShaderResourceView *shadowResourceView = _shadow->getShaderResourceView();
		ID3D11SamplerState *shadowSampler = _shadow->getSamplerComparisonState();
		gbuf->getContext()->PSSetShaderResources(ARRAYSIZE(gshr), 1, &shadowResourceView);
		gbuf->getContext()->PSSetSamplers(0, 1, &shadowSampler);

		gbuf->getContext()->UpdateSubresource(_pShadowMatrixBuffer, 0, NULL, _VPMatrix, 0, 0);
		gbuf->getContext()->PSSetConstantBuffers(0, 1, &_pShadowMatrixBuffer);
	}

	tmp = XMMatrixInverse(NULL, cam->getVPMatrix());
	gbuf->getContext()->UpdateSubresource(_pIVPMatrixBuffer, 0, NULL, &tmp, 0, 0);
	gbuf->getContext()->PSSetConstantBuffers(1, 1, &_pIVPMatrixBuffer);

	XMUINT2 screen(gbuf->getWidth(), gbuf->getHeight());
	gbuf->getContext()->UpdateSubresource(_pScreenBuffer, 0, NULL, &screen, 0, 0);
	gbuf->getContext()->PSSetConstantBuffers(2, 1, &_pScreenBuffer);

	XMFLOAT3 pos = cam->getPositionCamera();
	gbuf->getContext()->UpdateSubresource(_pCameraBuffer, 0, NULL, &pos, 0, 0);
	gbuf->getContext()->PSSetConstantBuffers(3, 1, &_pCameraBuffer);

	gbuf->getContext()->PSSetConstantBuffers(4, 1, &_pLightInfoBuffer);

	UINT stride = 2 * sizeof(FLOAT), offset = 0;
	gbuf->getContext()->IASetVertexBuffers(0, 1, &_pVertexBuffer, &stride, &offset);

	gbuf->getContext()->Draw(4, 0);
}