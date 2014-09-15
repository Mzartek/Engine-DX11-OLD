#include <Engine/Screen.hpp>
#include <Engine/ShaderProgram.hpp>
#include <Engine/GBuffer.hpp>
#include <Engine/Renderer.hpp>

engine::Screen::Screen()
	: _pInputLayout(NULL), _pVertexBuffer(NULL), _pScreenColorBuffer(NULL)
{
}

engine::Screen::~Screen()
{
	if (_pInputLayout) _pInputLayout->Release();
	if (_pVertexBuffer) _pVertexBuffer->Release();
	if (_pScreenColorBuffer) _pScreenColorBuffer->Release();
}

void engine::Screen::config(ShaderProgram *backgroundProgram, ShaderProgram *directProgram, ID3D11Device *pd3dDevice)
{
	HRESULT hr;
	D3D11_BUFFER_DESC bd;
	D3D11_SUBRESOURCE_DATA data;

	if (_pInputLayout) _pInputLayout->Release();
	if (_pVertexBuffer) _pVertexBuffer->Release();
	if (_pScreenColorBuffer) _pScreenColorBuffer->Release();
	
	_backgroundProgram = backgroundProgram;
	_directProgram = directProgram;

	// Create Input Layout
	D3D11_INPUT_ELEMENT_DESC layout[] =
	{
		{ "IN_POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
	};
	hr = pd3dDevice->CreateInputLayout(layout, ARRAYSIZE(layout),
		_directProgram->getEntryBufferPointer(), _directProgram->getEntryBytecodeLength(),
		&_pInputLayout);
	if (FAILED(hr))
	{
		MessageBox(NULL, "Failed to create Input Layout", "Screen", MB_OK);
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
	bd.Usage = D3D11_USAGE_IMMUTABLE;
	bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	bd.CPUAccessFlags = 0;
	bd.MiscFlags = 0;
	bd.StructureByteStride = 0;
	data.pSysMem = vertex;
	hr = pd3dDevice->CreateBuffer(&bd, &data, &_pVertexBuffer);
	if (FAILED(hr))
	{
		MessageBox(NULL, "Failed to create Vertex Buffer", "Screen", MB_OK);
		exit(1);
	}

	// Create Screen Color Buffer
	bd.ByteWidth = sizeof XMFLOAT4;
	bd.Usage = D3D11_USAGE_DEFAULT;
	bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	bd.CPUAccessFlags = 0;
	hr = pd3dDevice->CreateBuffer(&bd, NULL, &_pScreenColorBuffer);
	if (FAILED(hr))
	{
		MessageBox(NULL, "Failed to create Screen Buffer", "Screen", MB_OK);
		exit(1);
	}
}

void engine::Screen::background(GBuffer *gbuf)
{
	gbuf->setBackgroundState();

	// Shader
	gbuf->getContext()->VSSetShader(_backgroundProgram->getVertexShader(), NULL, 0);
	gbuf->getContext()->GSSetShader(_backgroundProgram->getGeometryShader(), NULL, 0);
	gbuf->getContext()->PSSetShader(_backgroundProgram->getPixelShader(), NULL, 0);

	// Texture
	ID3D11ShaderResourceView *pshr[] =
	{
		gbuf->getShaderResourceView(GBUF_MATERIAL),
		gbuf->getShaderResourceView(GBUF_LIGHT),
	};
	gbuf->getContext()->PSSetShaderResources(0, ARRAYSIZE(pshr), pshr);

	// Vertex Buffer
	UINT stride = 2 * sizeof(FLOAT), offset = 0;
	gbuf->getContext()->IASetVertexBuffers(0, 1, &_pVertexBuffer, &stride, &offset);
	gbuf->getContext()->IASetInputLayout(_pInputLayout);
	gbuf->getContext()->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
	gbuf->getContext()->Draw(4, 0);

	gbuf->clearLight();
}

void engine::Screen::display(Renderer *renderer, GBuffer *gbuf, const FLOAT &r, const FLOAT &g, const FLOAT &b, const FLOAT &a)
{
	renderer->setState();

	// Constant Buffer
	XMFLOAT4 color(r, g, b, a);
	renderer->getContext()->UpdateSubresource(_pScreenColorBuffer, 0, NULL, &color, 0, 0);
	renderer->getContext()->PSSetConstantBuffers(0, 1, &_pScreenColorBuffer);

	// Shader
	renderer->getContext()->VSSetShader(_directProgram->getVertexShader(), NULL, 0);
	renderer->getContext()->GSSetShader(_directProgram->getGeometryShader(), NULL, 0);
	renderer->getContext()->PSSetShader(_directProgram->getPixelShader(), NULL, 0);

	// Texture
	ID3D11ShaderResourceView *pshr[] =
	{
		gbuf->getShaderResourceView(GBUF_BACKGROUND),
	};
	renderer->getContext()->PSSetShaderResources(0, ARRAYSIZE(pshr), pshr);

	// Vertex Buffer
	UINT stride = 2 * sizeof(FLOAT), offset = 0;
	renderer->getContext()->IASetVertexBuffers(0, 1, &_pVertexBuffer, &stride, &offset);
	renderer->getContext()->IASetInputLayout(_pInputLayout);
	renderer->getContext()->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
	renderer->getContext()->Draw(4, 0);
}
