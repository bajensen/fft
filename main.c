//TO COMPILE: gcc -o main main.c -lm -lpulse -lpulse-simple -lfftw3 -lSDL -lGL -lGLU
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <math.h>

#include <fcntl.h>
#include <termios.h>

#include <pulse/simple.h>
#include <pulse/error.h>


#include "oglinit.h"
#include "font.h"

#include <limits.h>

#include <fftw3.h>

#define true 1
#define false 0

#define NUMBINS 1024
#define BUFSIZE 1024

#define WH 360
#define WW 1024

#define MAXSHORT 65535

int ret = 1;
int *outGraph;
int fftpause=0;
int mousex,mousey;
char *tmpbuf;
int posx=0;

int selp=1,selpp=0,selpt=30;
int beatCount=0;
int beatButton=0;
int beatCurrent=10;

int dragging=false;
int draginitx,draginitpos,dragendx,dragendpos;
int zoom=1,startp=0, vw=WW;
int color;
int fd;

int doRender=1;

unsigned long count = 1;

int running=1;

#include "controls.h"

void HSBtoColor(float h, float s, float v){
        if(h>=1)h-=1;

        int i=h*6;
        float f=h*6-i;
        float q=v*(1-f);
        float t=v*(1-(1-f));
        
        //int ov=v*255;
        //int oq=q*255;
        //int ot=t*255;

        switch(i){//below is obviously rgb
                case 0:glColor3f(v,t,0);break;
                case 1:glColor3f(q,v,0);break;
                case 2:glColor3f(0,v,t);break;
                case 3:glColor3f(0,q,v);break;
                case 4:glColor3f(t,0,v);break;
                case 5:glColor3f(v,0,q);break;
        }
}

void render(){
	int i;
		
	glClear(GL_COLOR_BUFFER_BIT);
	
	glColor3f(1.0,1.0,1.0);
	
	glBegin(GL_QUADS);
	//glBegin(GL_LINES);
		
		for( i = startp ; i < vw+startp ; i++ ) {
			HSBtoColor((float)outGraph[i]/(float)WH,1.0,1.0);
			glVertex2f(i*zoom+zoom-startp*zoom,WH);
			glVertex2f(i*zoom-startp*zoom,WH);
			glVertex2f(i*zoom-startp*zoom,WH-outGraph[i]);
			glVertex2f(i*zoom+zoom-startp*zoom,WH-outGraph[i]);
			//glVertex2f(i,WH);
		}
		
	glEnd();
	
	//thresholds
	glBegin(GL_QUADS);
		glColor3f(0.75,0,0);
		i=selp;
		glVertex2f(i*zoom+zoom-startp*zoom,WH);
		glVertex2f(i*zoom-startp*zoom,WH);
		glVertex2f(i*zoom-startp*zoom,WH-selpt);
		glVertex2f(i*zoom+zoom-startp*zoom,WH-selpt);
	glEnd();
	
	
	//right bar
	glBegin(GL_QUADS);
		HSBtoColor((float)outGraph[posx]/(float)WH,1.0,1.0);
		glVertex2f(WW-50,WH);
		glVertex2f(WW,WH);
		glVertex2f(WW,WH-outGraph[posx]);
		glVertex2f(WW-50,WH-outGraph[posx]);
	glEnd();
	
	
	glColor3f(1.0,1.0,1.0);
	sprintf(tmpbuf,"SEL=%3d:%03d Z=%d SP=%d",selp,selpt,zoom,startp);
	text(10,32,tmpbuf);
	
	SDL_GL_SwapBuffers();
}

int main(int argc, char*argv[]) {

	int i;
	/* The sample type to use */
	static const pa_sample_spec ss = {
		.format = PA_SAMPLE_S16LE,
		.rate = 48000,
		.channels = 1
	};
	
	tmpbuf=calloc(1024,sizeof(char));
	
	double *ddata;
	ddata=calloc(BUFSIZE,sizeof(double));
	fftw_complex	*data, *fft_result, *ifft_result;
	fftw_plan	   plan_forward,plan_backward,plan_aretosee;
	
	outGraph=calloc(BUFSIZE,sizeof(int));
	
	data		= ( fftw_complex* ) fftw_malloc( sizeof( fftw_complex ) * BUFSIZE );
	fft_result  = ( fftw_complex* ) fftw_malloc( sizeof( fftw_complex ) * BUFSIZE );
    ifft_result = ( fftw_complex* ) fftw_malloc( sizeof( fftw_complex ) * BUFSIZE );
	
	plan_aretosee = fftw_plan_dft_r2c_1d( BUFSIZE, ddata, ifft_result, FFTW_ESTIMATE);
	
	plan_forward  = fftw_plan_dft_1d( BUFSIZE, data, fft_result, FFTW_FORWARD, FFTW_ESTIMATE );
    plan_backward = fftw_plan_dft_1d( BUFSIZE, fft_result, ifft_result, FFTW_BACKWARD, FFTW_ESTIMATE );
	
	pa_simple *s = NULL;
	int error;

	/* Create the recording stream */
	if (!(s = pa_simple_new(NULL, argv[0], PA_STREAM_RECORD, NULL, "record", &ss, NULL, NULL, &error))) {
		fprintf(stderr, __FILE__": pa_simple_new() failed: %s\n", pa_strerror(error));
		goto finish;
	}
	
	initSDLOpenGL(WW,WH,90,"SpectroFFT");
	initfont();
	
	while (true) {
		short buf[BUFSIZE];

		/* Record some data ... */
		if (pa_simple_read(s, buf, sizeof(buf), &error) < 0) {
			fprintf(stderr, __FILE__": pa_simple_read() failed: %s\n", pa_strerror(error));
			goto finish;
		}
		
		if(!fftpause){
			//FILL FFT DATA
			for( i = 0 ; i < BUFSIZE ; i++ ) {
				//Hanning
				double m = 0.5 * (1 - cos(2*M_PI*i/BUFSIZE-1));
				ddata[i]=m * (double)buf[i];
				
				//data[i][0] = m * (double)buf[i];
				//data[i][1] = 0.0;
			}
		
			//fftw_execute( plan_forward );
			//fftw_execute( plan_backward );
			fftw_execute( plan_aretosee );
		
			for( i = 0 ; i < NUMBINS ; i++ ) {
				double mag,magdb;
				//OLD
				/*
				mag=ifft_result[i][0];
				outGraph[i]=abs(mag/10000);///10000;
				*/
				
				//NEW
				mag=sqrt(pow(ifft_result[i][0],2)+pow(ifft_result[i][1],2));
				magdb = mag>0?100*log10(mag/10000):0;
				outGraph[i]=abs(mag/10000);///10000;
				//fprintf(stdout, "%d: \n",outGraph[i]);
				/*fprintf( stdout, "fft_result[%d] = { %2.2f, %2.2f }\n",
							i, fft_result[i][0], fft_result[i][1] );*/
			}
		}
		
		

		posx=mousex/zoom+startp;
		if(posx>NUMBINS)posx=NUMBINS;
		if(posx<0)posx=0;
		

		beatCount++;
		beatButton++;
		if(beatCount>=beatCurrent){
			printf("%d ",beatCurrent);
			fflush(stdout);
			beatCount=0;
		}
		
		
		if(doRender)render();
		if(controls()==-2)goto finish;
		
		int ndiff=30;
		unsigned char temp;
		if(outGraph[selp]>selpt){
			//changeColor();
		}
		selpp=outGraph[selp];
		

	}

	running=0;


	ret = 0;

finish:

	//fftw_destroy_plan( plan_forward );
	
	//fftw_free( data );
	//fftw_free( fft_result );
	
	free(outGraph);
	
	if (s)pa_simple_free(s);

	return ret;
}