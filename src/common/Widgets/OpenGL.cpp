#include "Lgi.h"
#include "LOpenGL.h"

struct LOpenGLPriv
{
	#ifdef WIN32
	HDC hDC;
	HGLRC hRC;
	#endif
};

LOpenGL::LOpenGL()
{
	d = new LOpenGLPriv;
}

LOpenGL::~LOpenGL()
{
	#ifdef WIN32
	wglMakeCurrent(NULL, NULL);
	wglDeleteContext(d->hRC);
	ReleaseDC(Handle(), d->hDC);
	#endif

	DeleteObj(d);
}

void LOpenGL::SwapBuffers()
{
	#ifdef WIN32
	::SwapBuffers(d->hDC);
	#endif
}

void LOpenGL::OnCreate()
{
	#ifdef WIN32
	d->hDC = GetDC(Handle());

	PIXELFORMATDESCRIPTOR pfd;
	ZeroMemory( &pfd, sizeof( pfd ) );
	pfd.nSize = sizeof( pfd );
	pfd.nVersion = 1;
	pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
	pfd.iPixelType = PFD_TYPE_RGBA;
	pfd.cColorBits = 24;
	pfd.cDepthBits = 16;
	pfd.iLayerType = PFD_MAIN_PLANE;
	int iFormat = ChoosePixelFormat(d->hDC, &pfd);
	SetPixelFormat(d->hDC, iFormat, &pfd);

	d->hRC = wglCreateContext(d->hDC);
	wglMakeCurrent(d->hDC, d->hRC);
	#endif
}
