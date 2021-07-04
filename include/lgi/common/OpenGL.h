#ifndef _GOPENGL_H_
#define _GOPENGL_H_

#include <gl/gl.h>
#include <gl/glu.h>

class LOpenGL : public LView
{
	struct LOpenGLPriv *d;

public:
	LOpenGL();
	~LOpenGL();

	void OnCreate();
	void SwapBuffers();
};

#endif