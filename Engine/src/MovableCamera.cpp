#include <Engine/MovableCamera.hpp>

engine::MovableCamera::MovableCamera(void)
	: _speed(1), _atheta(0), _aphi(0)
{
	_vforward = (XMVECTOR *)_aligned_malloc(sizeof *_vforward, 16);
	_vleft = (XMVECTOR *)_aligned_malloc(sizeof *_vleft, 16);
}

engine::MovableCamera::~MovableCamera(void)
{
	_aligned_free(_vforward);
	_aligned_free(_vleft);
}

void engine::MovableCamera::setInitialAngle(const FLOAT &t, const FLOAT &p)
{
	_atheta = t;
	_aphi = p;

	if (_atheta > 360)
		_atheta -= 360;
	else if (_atheta < -360)
		_atheta += 360;
	if (_aphi > 89)
		_aphi = 89;
	else if (_aphi < -89)
		_aphi = -89;

	FLOAT tmp = (FLOAT)cos(_aphi*XM_PI / 180);
	*_vforward = XMVectorSetX(*_vforward, tmp*(FLOAT)sin(_atheta*XM_PI / 180));
	*_vforward = XMVectorSetY(*_vforward, (FLOAT)sin(_aphi*XM_PI / 180));
	*_vforward = XMVectorSetZ(*_vforward, tmp*(FLOAT)cos(_atheta*XM_PI / 180));

	*_vleft = XMVectorSetX(*_vleft, 1 * XMVectorGetZ(*_vforward));
	*_vleft = XMVectorSetY(*_vleft, 0);
	*_vleft = XMVectorSetZ(*_vleft, -(1 * XMVectorGetX(*_vforward)));
	*_vleft = XMVector3Normalize(*_vleft);

	*_ptarget = *_pcamera + *_vforward;
}

void engine::MovableCamera::setSpeed(const FLOAT &v)
{
	_speed = v;
}

XMFLOAT3 engine::MovableCamera::getForward(void) const
{
	XMFLOAT3 forward;
	XMStoreFloat3(&forward, *_vforward);

	return forward;
}

XMFLOAT3 engine::MovableCamera::getLeft(void) const
{
	XMFLOAT3 left;
	XMStoreFloat3(&left, *_vleft);

	return left;
}

void engine::MovableCamera::mouseMove(const INT &xrel, const INT &yrel)
{
	_atheta -= (FLOAT)xrel;
	_aphi -= (FLOAT)yrel;

	if (_atheta > 360)
		_atheta -= 360;
	else if (_atheta < -360)
		_atheta += 360;
	if (_aphi > 89)
		_aphi = 89;
	else if (_aphi < -89)
		_aphi = -89;

	FLOAT tmp = (FLOAT)cos(_aphi*XM_PI / 180);
	*_vforward = XMVectorSetX(*_vforward, tmp*(FLOAT)sin(_atheta*XM_PI / 180));
	*_vforward = XMVectorSetY(*_vforward, (FLOAT)sin(_aphi*XM_PI / 180));
	*_vforward = XMVectorSetZ(*_vforward, tmp*(FLOAT)cos(_atheta*XM_PI / 180));

	*_vleft = XMVectorSetX(*_vleft, 1 * XMVectorGetZ(*_vforward));
	*_vleft = XMVectorSetY(*_vleft, 0);
	*_vleft = XMVectorSetZ(*_vleft, -(1 * XMVectorGetX(*_vforward)));
	*_vleft = XMVector3Normalize(*_vleft);

	*_ptarget = *_pcamera + *_vforward;
}