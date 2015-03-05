//TO COMPILE: gcc -o main main.c -lm -lpulse -lpulse-simple -lfftw3 -lSDL -lGL -lGLU -lGLEW
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <math.h>

#include <fcntl.h>
#include <termios.h>

#include <pulse/simple.h>
#include <pulse/error.h>

#define GL_GLEXT_PROTOTYPES
#include <GL/glu.h>

#include "oglinit.h"
#include "font.h"

#include <limits.h>

#include <fftw3.h>

#define true 1
#define false 0

#define PROGRAM_NAME "SpectroFFT"

#define BUFSIZE 1024
#define BINSIZE 2048

#define WH 500
#define WW 1024

#define MAXSHORT 65535

int ret = 1;
int *outGraph;
int fftpause=0;
int mousex,mousey;
char *tmpbuf;
int posx=0;

int dragging=false;
int draginitx,draginitpos,dragendx,dragendpos;
int zoom=2, speed=1;
int magnitude=10000;
int startp=0, vw=BINSIZE;
int color;
int fd;

int doRender=1;

unsigned long count = 1;

#include "controls.h"

void HSBtoColor(float h, float s, float v){
	if(h>=1)h-=1;

	int i=h*6;
	float f=h*6-i;
	float q=v*(1-f);
	float t=v*(1-(1-f));
	
	switch(i){//below is obviously rgb
		case 0:glColor3f(v,t,0);break;
		case 1:glColor3f(q,v,0);break;
		case 2:glColor3f(0,v,t);break;
		case 3:glColor3f(0,q,v);break;
		case 4:glColor3f(t,0,v);break;
		case 5:glColor3f(v,0,q);break;
	}
}

char pingPong = 0;
GLuint FBO0,TEX0,RBO0;
GLuint FBO1,TEX1,RBO1;

void render(){
	int i;
		
	glClear(GL_COLOR_BUFFER_BIT);
	
	GLuint prevTex,curTex,curFbo;
	if(pingPong){
		prevTex=TEX0;
		curTex=TEX1;
		curFbo=FBO1;
	}else{
		prevTex=TEX1;
		curTex=TEX0;
		curFbo=FBO0;
	}pingPong = !pingPong;

	glBindFramebuffer(GL_FRAMEBUFFER,curFbo);

		glColor3f(1,1,1);
		glEnable(GL_TEXTURE_2D);
		glBindTexture(GL_TEXTURE_2D,prevTex);
		glBegin(GL_QUADS);
			glTexCoord2f(0,1);glVertex2f(00,00+speed);
			glTexCoord2f(1,1);glVertex2f(WW,00+speed);
			glTexCoord2f(1,0);glVertex2f(WW,WH+speed);
			glTexCoord2f(0,0);glVertex2f(00,WH+speed);
		glEnd();
		glBindTexture(GL_TEXTURE_2D,0);
		glDisable(GL_TEXTURE_2D);
		
		/*
		glBegin(GL_LINES);
		for(i=startp;i<vw+startp;++i){
			#define min(a,b)((a)<(b)?(a):(b))
			HSBtoColor((float)outGraph[i]/2000.0,1,min(1,(float)outGraph[i]/(float)WH*30));
			//glVertex2f(i,WH);
			glVertex2f((i-startp)*zoom,0);
			glVertex2f((i-startp)*zoom+zoom,0);
		}glEnd();
		*/
		glBegin(GL_QUADS);
		for(i=startp;i<vw+startp;++i){
			#define min(a,b)((a)<(b)?(a):(b))
			HSBtoColor((float)outGraph[i]/2000.0,1,min(1,(float)outGraph[i]/(float)WH*30));
			//glVertex2f(i,WH);
			glVertex2f((i-startp)*zoom,0);
			glVertex2f((i-startp)*zoom+zoom,0);
			glVertex2f((i-startp)*zoom+zoom,speed);
			glVertex2f((i-startp)*zoom,speed);
		}glEnd();
	glBindTexture(GL_TEXTURE_2D,curTex);
	glGenerateMipmap(GL_TEXTURE_2D);
	glBindTexture(GL_TEXTURE_2D,0);
	glBindFramebuffer(GL_FRAMEBUFFER,0);

	glColor3f(1,1,1);
	glEnable(GL_TEXTURE_2D);
	glBindTexture(GL_TEXTURE_2D,curTex);
	glBegin(GL_QUADS);
		glTexCoord2f(0,0);glVertex2f(00,00);
		glTexCoord2f(1,0);glVertex2f(WW,00);
		glTexCoord2f(1,1);glVertex2f(WW,WH);
		glTexCoord2f(0,1);glVertex2f(00,WH);
	glEnd();
	glBindTexture(GL_TEXTURE_2D,0);
	glDisable(GL_TEXTURE_2D);
	
	SDL_GL_SwapBuffers();
}

int main(int argc, char*argv[]) {
	glewInit();
	initSDLOpenGL(WW,WH,90,"SpectroFFT");
	initfont();

	glGenTextures(1,&TEX0);
	glBindTexture(GL_TEXTURE_2D,TEX0);
	glTexParameterf(GL_TEXTURE_2D,GL_GENERATE_MIPMAP,GL_TRUE);
	glTexParameterf(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);
	glTexParameterf(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
	glTexParameterf(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
	glTexParameterf(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);
	glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,WW,WH,0,GL_RGBA,GL_UNSIGNED_BYTE,0);
	glBindTexture(GL_TEXTURE_2D,0);
	glGenFramebuffers(1,&FBO0);
	glBindFramebuffer(GL_FRAMEBUFFER,FBO0);
	glGenRenderbuffers(1,&RBO0);
	glBindRenderbuffer(GL_RENDERBUFFER,RBO0);
	glRenderbufferStorage(GL_RENDERBUFFER,GL_DEPTH_COMPONENT,WW,WH);
	glBindRenderbuffer(GL_RENDERBUFFER,0);
	glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,TEX0,0);
	glFramebufferRenderbuffer(GL_FRAMEBUFFER,GL_DEPTH_ATTACHMENT,GL_RENDERBUFFER,RBO0);
	glBindFramebuffer(GL_FRAMEBUFFER,0);

	glGenTextures(1,&TEX1);
	glBindTexture(GL_TEXTURE_2D,TEX1);
	glTexParameterf(GL_TEXTURE_2D,GL_GENERATE_MIPMAP,GL_TRUE);
	glTexParameterf(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);
	glTexParameterf(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
	glTexParameterf(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
	glTexParameterf(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);
	glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,WW,WH,0,GL_RGBA,GL_UNSIGNED_BYTE,0);
	glBindTexture(GL_TEXTURE_2D,0);
	glGenFramebuffers(1,&FBO1);
	glBindFramebuffer(GL_FRAMEBUFFER,FBO1);
	glGenRenderbuffers(1,&RBO1);
	glBindRenderbuffer(GL_RENDERBUFFER,RBO1);
	glRenderbufferStorage(GL_RENDERBUFFER,GL_DEPTH_COMPONENT,WW,WH);
	glBindRenderbuffer(GL_RENDERBUFFER,0);
	glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,TEX1,0);
	glFramebufferRenderbuffer(GL_FRAMEBUFFER,GL_DEPTH_ATTACHMENT,GL_RENDERBUFFER,RBO1);
	glBindFramebuffer(GL_FRAMEBUFFER,0);
	
	int i;
	
	//buffer for text stuff
	tmpbuf=calloc(1024,sizeof(char));
	
	//buffer for audio
	short buf[BUFSIZE];
	short dblbuf[BINSIZE];
	
	//this is the array containing the amplitudes to be drawn in a bar graph
	outGraph=calloc(BINSIZE,sizeof(int));
	
	//THE SETTINGS FOR PULSEAUDIO
	static const pa_sample_spec ss = {
		.format = PA_SAMPLE_S16LE,
		.rate = 41100,
		.channels = 1
	};
	
	//and array to contain the data sampled from
	double *ddata;
	ddata=calloc(BINSIZE,sizeof(double));
	
	//a bunch of fft stuff I don't understand
	fftw_complex *data, *fft_result, *ifft_result;
	fftw_plan plan_aretosee;
	
	data		= ( fftw_complex* ) fftw_malloc( sizeof( fftw_complex ) * BINSIZE );
	fft_result  = ( fftw_complex* ) fftw_malloc( sizeof( fftw_complex ) * BINSIZE );
    ifft_result = ( fftw_complex* ) fftw_malloc( sizeof( fftw_complex ) * BINSIZE );
	
	plan_aretosee = fftw_plan_dft_r2c_1d( BINSIZE, ddata, ifft_result, FFTW_ESTIMATE);
	
	pa_simple *s = NULL;
	int error;

	/* Create the recording stream */
	if(!(s = pa_simple_new(NULL,PROGRAM_NAME,PA_STREAM_RECORD,NULL,"record",&ss,NULL,NULL,&error))){
		fprintf(stderr, __FILE__": pa_simple_new() failed: %s\n", pa_strerror(error));
		goto finish;
	}
	
	initSDLOpenGL(WW,WH,90,PROGRAM_NAME);
	initfont();
	
	while(true){

		/* Record some audio data ... */
		if (pa_simple_read(s, buf, sizeof(buf), &error) < 0) {
			fprintf(stderr, __FILE__": pa_simple_read() failed: %s\n", pa_strerror(error));
			goto finish;
		}
		
		int x,y;
		for(x=0,y=0;x<BUFSIZE;x++,y+=2){
			dblbuf[y]=buf[x];
		}
		for(i=0;i<BINSIZE-2;i+=2){
			dblbuf[i+1]=(dblbuf[i]+dblbuf[i+2])/2;
		}
		
		//FILL FFT DATA
		for(i = 0; i<BINSIZE; i++){
			//Hanning
			double m = 0.5 * (1 - cos(2*M_PI*i/BINSIZE-1));
			ddata[i]=m * (double)dblbuf[i];
			
			//data[i][0] = m * (double)buf[i];
			//data[i][1] = 0.0;
		}
	
		fftw_execute( plan_aretosee );
	
		for(i = 0; i<BINSIZE; i++){
			double mag,magdb;
			mag=sqrt(pow(ifft_result[i][0],2)+pow(ifft_result[i][1],2));// mag=sqrt(real^2+img^2)
			outGraph[i]=abs(mag/magnitude);
		}

		posx=mousex/zoom+startp;
		if(posx>BINSIZE)posx=BINSIZE;
		if(posx<0)posx=0;
		

		if(doRender)render();
		if(controls()==-2)goto finish;
		
	}
	ret = 0;

finish:
	free(outGraph);
	if (s)pa_simple_free(s);
	return ret;
}
