#include <Engine/DirLight.hpp>
#include <Engine/Buffer.hpp>
#include <Engine/ShadowMap.hpp>
#include <Engine/ShaderProgram.hpp>
#include <Engine/GBuffer.hpp>
#include <Engine/Camera.hpp>

Engine::DirLight::DirLight(const EngineDevice &EngineDevice, ShaderProgram *program)
	: Light(EngineDevice, program)
{
	_lightInfoBuffer->createStore(D3D11_BIND_CONSTANT_BUFFER, NULL, sizeof _lightInfo, D3D11_USAGE_DYNAMIC);

	D3D11_INPUT_ELEMENT_DESC layout[] =
	{
		{ "IN_POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
	};
	_EngineDevice.Device->CreateInputLayout(layout, ARRAYSIZE(layout), _program->getEntryBufferPointer(), _program->getEntryBytecodeLength(), &_pInputLayout);
}

Engine::DirLight::~DirLight(void)
{
	_pInputLayout->Release();
}

void Engine::DirLight::setColor(const XMVECTOR &color)
{
	XMStoreFloat3(&_lightInfo.color, color);
}

void Engine::DirLight::setDirection(const XMVECTOR &dir)
{
	XMStoreFloat3(&_lightInfo.direction, dir);
}

void Engine::DirLight::setShadowMapping(const BOOL &shadow)
{
	_lightInfo.withShadowMapping = shadow;
}

XMVECTOR Engine::DirLight::getColor(void) const
{
	return XMLoadFloat3(&_lightInfo.color);
}

XMVECTOR Engine::DirLight::getDirection(void) const
{
	return XMLoadFloat3(&_lightInfo.direction);
}

void Engine::DirLight::position(const XMVECTOR &position, const FLOAT &dim)
{
	XMVECTOR dir = XMVectorSet(_lightInfo.direction.x, _lightInfo.direction.y, _lightInfo.direction.z, 0.0f);

	*_VPMatrix = XMMatrixLookAtRH(position - dir, position, XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f)) *
		XMMatrixOrthographicOffCenterRH(-dim, dim, -dim, dim, -dim, dim);
}

void Engine::DirLight::display(GBuffer *gbuf, Camera *cam)
{	
	gbuf->setLightState();

	_EngineDevice.DeviceContext->VSSetShader(_program->getVertexShader(), NULL, 0);
	_EngineDevice.DeviceContext->HSSetShader(_program->getHullShader(), NULL, 0);
	_EngineDevice.DeviceContext->DSSetShader(_program->getDomainShader(), NULL, 0);
	_EngineDevice.DeviceContext->GSSetShader(_program->getGeometryShader(), NULL, 0);
	_EngineDevice.DeviceContext->PSSetShader(_program->getPixelShader(), NULL, 0);

	ID3D11ShaderResourceView *gshr[]
	{
		gbuf->getShaderResourceView(GBUF_NORMAL),
		gbuf->getShaderResourceView(GBUF_MATERIAL),
		gbuf->getShaderResourceView(GBUF_DEPTH),
		gbuf->getShaderResourceView(GBUF_STENCIL),
	};
	_EngineDevice.DeviceContext->PSSetShaderResources(0, ARRAYSIZE(gshr), gshr);

	struct
	{
		XMMATRIX shadowMatrix;
		XMMATRIX IVPMatrix;
		XMUINT2 __declspec(align(16)) screen;
		XMVECTOR __declspec(align(16)) camPosition;
	} mainInfo;

	// ShadowMap
	if (_lightInfo.withShadowMapping == TRUE)
	{
		ID3D11ShaderResourceView *shadowResourceView = _shadow->getShaderResourceView();
		ID3D11SamplerState *shadowSampler = _shadow->getSamplerComparisonState();
		_EngineDevice.DeviceContext->PSSetShaderResources(ARRAYSIZE(gshr), 1, &shadowResourceView);
		_EngineDevice.DeviceContext->PSSetSamplers(0, 1, &shadowSampler);

		mainInfo.shadowMatrix = *_VPMatrix;
	}
	mainInfo.IVPMatrix = XMMatrixInverse(NULL, cam->getVPMatrix());
	mainInfo.screen.x = gbuf->getWidth(), mainInfo.screen.y = gbuf->getHeight();
	mainInfo.camPosition = cam->getPositionCamera();

	_mainInfoBuffer->updateStoreMap(&mainInfo);
	_lightInfoBuffer->updateStoreMap(&_lightInfo);

	ID3D11Buffer *buf[] =
	{
		_mainInfoBuffer->getBuffer(),
		_lightInfoBuffer->getBuffer(),
	};
	_EngineDevice.DeviceContext->PSSetConstantBuffers(0, ARRAYSIZE(buf), buf);

	UINT stride = 2 * sizeof(FLOAT), offset = 0;
	ID3D11Buffer *drawBuf[] =
	{
		_vertexBuffer->getBuffer(),
	};
	_EngineDevice.DeviceContext->IASetVertexBuffers(0, 1, &drawBuf[0], &stride, &offset);
	_EngineDevice.DeviceContext->IASetInputLayout(_pInputLayout);
	_EngineDevice.DeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
	_EngineDevice.DeviceContext->Draw(4, 0);
}