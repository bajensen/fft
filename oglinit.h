#include "SDL/SDL.h"
#include "SDL/SDL_opengl.h"
#include "SDL/SDL_thread.h"

SDL_Surface *screen;
SDL_Event event;
int oglflags=SDL_OPENGL|SDL_RESIZABLE;
int ww,wh,oglfov,fov;
int elapsed;
int tick=0;

void mode2d(){
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glOrtho(0,ww,wh,0,0,1);
	glMatrixMode(GL_MODELVIEW);
	glDisable(GL_DEPTH_TEST);
}

void mode3d(){
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	gluPerspective(oglfov,(GLfloat)ww/(GLfloat)wh,0.05,1000.0);
	glMatrixMode(GL_MODELVIEW);
	glEnable(GL_DEPTH_TEST);
}

void reshape(int w,int h,int fovt){
	ww=w;
	wh=h;
	fov=fovt;
	oglfov=(fov/ww)*wh;
	SDL_FreeSurface(screen);
	screen=SDL_SetVideoMode(ww,wh,32,oglflags);
	glViewport(0,0,ww,wh);
	mode2d();
}

void initSDLOpenGL(int w,int h,int fovt,const char title[]){
	SDL_Init(SDL_INIT_VIDEO);
	SDL_GL_SetAttribute(SDL_GL_RED_SIZE,8);
	SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE,8);
	SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE,8);
	SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE,8);
	SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE,24);
	SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER,1);

	reshape(w,h,fovt);

	SDL_WM_SetCaption(title,title);
	SDL_ShowCursor(SDL_ENABLE);//cursor
	//glClearColor(0.1,0.1,0.1,0);//background color
	glClearColor(0,0,0,0);//background color
	glPointSize(1);//point size
	mode2d();//initial projection mode

	glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
	glShadeModel(GL_FLAT);
	glEnable(GL_BLEND);
}

void updatetime(){
	int temp=SDL_GetTicks();
	elapsed=temp-tick;
	tick=temp;
}
