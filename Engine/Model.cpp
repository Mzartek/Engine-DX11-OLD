#include <Engine/Model.hpp>
#include <assimp/postprocess.h>
#include <assimp/Importer.hpp>
#include <assimp/scene.h>

engine::Model::Model(void)
{
	_tD3DObject = NULL;
	_pMVPMatrixBuffer = NULL;
	_pNormalMatrixBuffer = NULL;
	_pInputLayout = NULL;
	_ModelMatrix = (XMMATRIX *)_aligned_malloc(sizeof *_ModelMatrix, 16);
	_gProgram = NULL;
	_smProgram = NULL;
	_pd3dDevice = NULL;
	_pContext = NULL;

	matIdentity();
}

engine::Model::~Model(void)
{
	UINT i;
	if(_tD3DObject != NULL && isMirror == FALSE)
	{
		for (i = 0; i < _tD3DObject->size(); i++)
			delete (*_tD3DObject)[i];
		delete _tD3DObject;
	}

	_aligned_free(_ModelMatrix);

	if (_pInputLayout)
		_pInputLayout->Release();
	if (_pNormalMatrixBuffer)
		_pNormalMatrixBuffer->Release();
	if (_pMVPMatrixBuffer)
		_pMVPMatrixBuffer->Release();
}

void engine::Model::initD3DObjectArray(void)
{
	UINT i;
	if(_tD3DObject != NULL && isMirror == FALSE)
	{
		for (i = 0; i < _tD3DObject->size(); i++)
			delete (*_tD3DObject)[i];
		delete _tD3DObject;
	}
	isMirror = FALSE;
	_tD3DObject = new std::vector<D3DObject *>;
}

void engine::Model::initD3DObjectMirror(Model *m)
{
	UINT i;
	if (_tD3DObject != NULL && isMirror == FALSE)
	{
		for (i = 0; i < _tD3DObject->size(); i++)
			delete (*_tD3DObject)[i];
		delete _tD3DObject;
	}
	isMirror = TRUE;
	_tD3DObject = m->_tD3DObject;
}

void engine::Model::config(ShaderProgram *gProgram, ShaderProgram *smProgram, ID3D11Device *pd3dDevice, ID3D11DeviceContext *pContext)
{
	HRESULT hr;
	D3D11_BUFFER_DESC bd;

	_gProgram = gProgram;
	_smProgram = smProgram;
	_pd3dDevice = pd3dDevice;
	_pContext = pContext;

	if (_pMVPMatrixBuffer)
		_pMVPMatrixBuffer->Release();
	if (_pNormalMatrixBuffer)
		_pNormalMatrixBuffer->Release();
	if (_pInputLayout)
		_pInputLayout->Release();

	bd.Usage = D3D11_USAGE_DEFAULT;
	bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	bd.CPUAccessFlags = 0;
	bd.MiscFlags = 0;
	bd.StructureByteStride = 0;

	// MVPMatrix Buffer
	bd.ByteWidth = sizeof XMMATRIX;
	hr = _pd3dDevice->CreateBuffer(&bd, NULL, &_pMVPMatrixBuffer);
	if (FAILED(hr))
	{
		MessageBox(NULL, "Failed to create MVPMatrix Buffer", "Model", MB_OK);
		exit(1);
	}

	// NormalMatrix Buffer
	bd.ByteWidth = sizeof XMMATRIX;
	hr = _pd3dDevice->CreateBuffer(&bd, NULL, &_pNormalMatrixBuffer);
	if (FAILED(hr))
	{
		MessageBox(NULL, "Failed to create NormalMatrix Buffer", "Model", MB_OK);
		exit(1);
	}

	// Create and set the input layout
	D3D11_INPUT_ELEMENT_DESC layout[] =
	{
		{ "IN_POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "IN_TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 3 * sizeof(FLOAT), D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "IN_NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 5 * sizeof(FLOAT), D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "IN_TANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 8 * sizeof(FLOAT), D3D11_INPUT_PER_VERTEX_DATA, 0 },
	};
	hr = _pd3dDevice->CreateInputLayout(layout, ARRAYSIZE(layout),
		gProgram->getEntryBufferPointer(), gProgram->getEntryBytecodeLength(),
		&_pInputLayout);
	if (FAILED(hr))
	{
		MessageBox(NULL, "Failed to create Input Layout", "D3DObject", MB_OK);
		exit(1);
	}
}

void engine::Model::createD3DObject(const UINT &sizeVertexArray, const FLOAT *vertexArray,
	const UINT &sizeIndexArray, const UINT *indexArray,
	const TCHAR *colorTexture, const TCHAR *NMTexture,
	const XMFLOAT4 &ambient, const XMFLOAT4 &diffuse, const XMFLOAT4 &specular, const FLOAT &shininess)
{
	D3DObject *newone = new D3DObject(_pd3dDevice);
	ID3D11Texture2D *pColorTex, *pNMTex;
	ID3D11ShaderResourceView *pColorSHR, *pNMSHR;
	ID3D11SamplerState *psam;

	loadTextureFromFile(colorTexture, &pColorTex, &pColorSHR, &psam, _pd3dDevice, _pContext);
	loadTextureFromFile(NMTexture, &pNMTex, &pNMSHR, NULL, _pd3dDevice, _pContext);

	newone->setColorTexture(pColorTex, pColorSHR, psam);
	newone->setNMTexture(pNMTex, pNMSHR);
	newone->setAmbient(ambient, _pContext);
	newone->setDiffuse(diffuse, _pContext);
	newone->setSpecular(specular, _pContext);
	newone->setShininess(shininess, _pContext);
	newone->load(sizeVertexArray, vertexArray,
		sizeIndexArray, indexArray,
		_pd3dDevice);
  
	_tD3DObject->push_back(newone);
}

static std::string getDir(const TCHAR *file)
{
	UINT size, i;
	std::string path;

	for (size = i = 0; file[i] != '\0'; i++)
		if (file[i] == '/')
			size = i + 1;

	path.insert(0, file, 0, size);

	return path;
}

void engine::Model::loadFromFile(const TCHAR *szFileName)
{
	Assimp::Importer Importer;
	UINT i, j;

	if (_tD3DObject != NULL && isMirror == FALSE)
		_tD3DObject->clear();

	const aiScene *pScene = Importer.ReadFile(szFileName, aiProcess_Triangulate | aiProcess_GenSmoothNormals | aiProcess_CalcTangentSpace | aiProcess_JoinIdenticalVertices | aiProcess_OptimizeMeshes);
	if (!pScene)
	{
		std::string mes = "Failed to load File: ";
		mes.append(szFileName);
		MessageBox(NULL, mes.c_str(), "Model", MB_OK);
		exit(1);
	}

	std::vector<Vertex> vertices;
	std::vector<UINT> indices;
	const aiVector3D Zero3D(0.0f, 0.0f, 0.0f);
	const aiVector3D *pPos;
	const aiVector3D *pTexCoord;
	const aiVector3D *pNormal;
	const aiVector3D *pTangent;
	for (i = 0; i<pScene->mNumMeshes; i++)
	{
		// Vertex Buffer
		for (j = 0; j<pScene->mMeshes[i]->mNumVertices; j++)
		{
			pPos = &(pScene->mMeshes[i]->mVertices[j]);
			pTexCoord = pScene->mMeshes[i]->HasTextureCoords(0) ? &(pScene->mMeshes[i]->mTextureCoords[0][j]) : &Zero3D;
			pNormal = pScene->mMeshes[i]->HasNormals() ? &(pScene->mMeshes[i]->mNormals[j]) : &Zero3D;
			pTangent = pScene->mMeshes[i]->HasTangentsAndBitangents() ? &(pScene->mMeshes[i]->mTangents[j]) : &Zero3D;

			Vertex v = 
			{
				XMFLOAT3(pPos->x, pPos->y, pPos->z),
				XMFLOAT2(pTexCoord->x, pTexCoord->y),
				XMFLOAT3(pNormal->x, pNormal->y, pNormal->z),
				XMFLOAT3(pTangent->x, pTangent->y, pTangent->z),
			};

			vertices.push_back(v);
		}

		// Index Buffer
		for (j = 0; j < pScene->mMeshes[i]->mNumFaces; j++)
		{
			indices.push_back(pScene->mMeshes[i]->mFaces[j].mIndices[0]);
			indices.push_back(pScene->mMeshes[i]->mFaces[j].mIndices[1]);
			indices.push_back(pScene->mMeshes[i]->mFaces[j].mIndices[2]);
		}

		aiString path;
		std::string dir, colorPath, NMPath;
		dir = getDir(szFileName);
		if (pScene->mMaterials[pScene->mMeshes[i]->mMaterialIndex]->GetTexture(aiTextureType_DIFFUSE, 0, &path, NULL, NULL, NULL, NULL, NULL) == AI_SUCCESS)
		{
			colorPath += dir + path.C_Str();
			NMPath += dir + "NM_" + path.C_Str();
		}
		else
		{
			colorPath = "resources/none.png";
			NMPath = "resources/NM_none.png";
		}

		aiColor4D mat_ambient;
		aiColor4D mat_diffuse;
		aiColor4D mat_specular;
		FLOAT mat_shininess;
		FLOAT opacity;

		pScene->mMaterials[pScene->mMeshes[i]->mMaterialIndex]->Get(AI_MATKEY_COLOR_AMBIENT, mat_ambient);
		pScene->mMaterials[pScene->mMeshes[i]->mMaterialIndex]->Get(AI_MATKEY_COLOR_DIFFUSE, mat_diffuse);
		pScene->mMaterials[pScene->mMeshes[i]->mMaterialIndex]->Get(AI_MATKEY_COLOR_SPECULAR, mat_specular);
		pScene->mMaterials[pScene->mMeshes[i]->mMaterialIndex]->Get(AI_MATKEY_SHININESS, mat_shininess);
		pScene->mMaterials[pScene->mMeshes[i]->mMaterialIndex]->Get(AI_MATKEY_OPACITY, opacity);
		mat_ambient.a = opacity;
		mat_diffuse.a = opacity;
		mat_specular.a = opacity;

		createD3DObject(vertices.size() * sizeof(Vertex), (FLOAT *)&vertices[0],
			indices.size() * sizeof(UINT), &indices[0],
			colorPath.c_str(), NMPath.c_str(),
			XMFLOAT4(mat_ambient.r, mat_ambient.g, mat_ambient.b, mat_ambient.a), XMFLOAT4(mat_diffuse.r, mat_diffuse.g, mat_diffuse.b, mat_diffuse.a), XMFLOAT4(mat_specular.r, mat_specular.g, mat_specular.b, mat_specular.a),
			mat_shininess);

		vertices.clear();
		indices.clear();
	}
}

void engine::Model::sortD3DObject(void)
{
	qsort(&(*_tD3DObject)[0], _tD3DObject->size(), sizeof (*_tD3DObject)[0], comparD3DObject);
}

void engine::Model::matIdentity(void)
{
	*_ModelMatrix = XMMatrixIdentity();
}

void engine::Model::matTranslate(const FLOAT &x, const FLOAT &y, const FLOAT &z)
{
	*_ModelMatrix = XMMatrixTranslation(x, y, z) * *_ModelMatrix;
}

void engine::Model::matRotate(const FLOAT &angle, const FLOAT &x, const FLOAT &y, const FLOAT &z)
{
	*_ModelMatrix = XMMatrixRotationAxis(XMVectorSet(x, y, z, 1.0f), angle * ((FLOAT)XM_PI / 180)) * *_ModelMatrix;
}

void engine::Model::matScale(const FLOAT &x, const FLOAT &y, const FLOAT &z)
{
	*_ModelMatrix = XMMatrixScaling(x, y, z) * *_ModelMatrix;
}

XMFLOAT3 engine::Model::getPosition(void) const
{
	XMFLOAT3 tmp;
	tmp.x = XMVectorGetW(_ModelMatrix->r[0]);
	tmp.y = XMVectorGetW(_ModelMatrix->r[1]);
	tmp.z = XMVectorGetW(_ModelMatrix->r[2]);

	return tmp;
}

engine::D3DObject *engine::Model::getD3DObject(UINT num) const
{
	if(num>=_tD3DObject->size())
	{
		MessageBox(NULL, "Bad num Object!", "Error", MB_OK);
		exit(1);
	}
	return (*_tD3DObject)[num];
}
  
void engine::Model::display(GBuffer *g, Camera *cam) const
{
	UINT i;
	XMMATRIX MVPMatrix = *_ModelMatrix * cam->getVPMatrix();
	XMMATRIX NormalMatrix = XMMatrixTranspose(XMMatrixInverse(NULL, *_ModelMatrix));

	g->setGeometryConfig();

	g->getContext()->UpdateSubresource(_pMVPMatrixBuffer, 0, NULL, &MVPMatrix, 0, 0);
	g->getContext()->UpdateSubresource(_pNormalMatrixBuffer, 0, NULL, &NormalMatrix, 0, 0);

	g->getContext()->VSSetShader(_gProgram->getVertexShader(), NULL, 0);
	g->getContext()->GSSetShader(_gProgram->getGeometryShader(), NULL, 0);
	g->getContext()->PSSetShader(_gProgram->getPixelShader(), NULL, 0);
	g->getContext()->IASetInputLayout(_pInputLayout);
	g->getContext()->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	g->getContext()->VSSetConstantBuffers(0, 1, &_pMVPMatrixBuffer);
	g->getContext()->VSSetConstantBuffers(1, 1, &_pNormalMatrixBuffer);
  
	for (i = 0; i<_tD3DObject->size(); i++)
		if ((*_tD3DObject)[i]->getTransparency() == 1.0f)
			(*_tD3DObject)[i]->display(g->getContext());
}

void engine::Model::displayShadowMap(Light *l) const
{
	UINT i;

	XMMATRIX MVPMatrix = *_ModelMatrix * l->getVPMatrix();
	l->getShadowMap()->getContext()->UpdateSubresource(_pMVPMatrixBuffer, 0, NULL, &MVPMatrix, 0, 0);

	l->getShadowMap()->getContext()->VSSetShader(_smProgram->getVertexShader(), NULL, 0);
	l->getShadowMap()->getContext()->GSSetShader(_smProgram->getGeometryShader(), NULL, 0);
	l->getShadowMap()->getContext()->PSSetShader(_smProgram->getPixelShader(), NULL, 0);
	l->getShadowMap()->getContext()->IASetInputLayout(_pInputLayout);
	l->getShadowMap()->getContext()->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	l->getShadowMap()->getContext()->VSSetConstantBuffers(0, 1, &_pMVPMatrixBuffer);

	for (i = 0; i<_tD3DObject->size(); i++)
		if ((*_tD3DObject)[i]->getTransparency() == 1.0f)
			(*_tD3DObject)[i]->displayShadow(l->getShadowMap()->getContext());

	l->getShadowMap()->executeDeferredContext();
}