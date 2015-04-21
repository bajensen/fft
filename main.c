//TO COMPILE: gcc -o main main.c -lm -lpulse -lpulse-simple -lfftw3 -lSDL -lGL -lGLU -lGLEW -llirc_client -lpthread
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

void initFBO();

#include "oglinit.h"
#include "font.h"

#include <limits.h>

#include <fftw3.h>


#define true 1
#define false 0

#define PROGRAM_NAME "SpectroFFT"

#define BUFSIZE 1024
#define BINSIZE 2048

#define HISTORYSIZE 256

#define WH 500
#define WW 1024

#define MAXSHORT 65535


//////////////////////////////////////////////////////////////////////////////////////
/// LIRC CONFIGURATION////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////
#include <lirc/lirc_client.h>
lirc_cmd_ctx ctx;
unsigned long count = 1;
int color;
int fd;
int running=1;
int doChangeColor=0;
double sel_val=0, sel_val_prev=0,sel_val_max=0;
double sel_val_thresh=1.25;
int sel_skip=1, sel_skip_count=0;
int beat=0;
double history[HISTORYSIZE];
int historyStart=0,historyEnd=0;

int send_packet(lirc_cmd_ctx* ctx);
void changeColor();

void *worker_func(void *c){
	lirc_command_init(&ctx, "SEND_ONCE COLORFUL ON %lu\n",count);
	lirc_command_reply_to_stdout(&ctx);
	send_packet(&ctx);

	printf("WORKER THREAD STARTED!\n");
	int *cc = (int *)c;
	printf("cc: %d\n\n",*cc);

	do{
		if(*cc==1){
			changeColor();
		}
		*cc=0;
		usleep(10000);
	}while(running);

	lirc_command_init(&ctx, "SEND_ONCE COLORFUL OFF %lu\n",count);
	lirc_command_reply_to_stdout(&ctx);
	send_packet(&ctx);
}

int send_packet(lirc_cmd_ctx* ctx){
	int r;

	do {
		r = lirc_command_run(ctx, fd);
		if (r != 0 && r != EAGAIN) {
			fprintf(stderr,
			        "Error running command: %s\n", strerror(r));
		}
	} while  (r == EAGAIN);
	return r == 0 ? 0 : -1;
}
void changeColor(){
	int r;
	switch(color){
		case 0:r = lirc_command_init(&ctx, "SEND_ONCE COLORFUL RED %lu\n",count);break;
		case 1:r = lirc_command_init(&ctx, "SEND_ONCE COLORFUL GREEN %lu\n",count);break;
		case 2:r = lirc_command_init(&ctx, "SEND_ONCE COLORFUL BLUE %lu\n",count);break;
		case 3:r = lirc_command_init(&ctx, "SEND_ONCE COLORFUL ORANGE %lu\n",count);break;
		case 4:r = lirc_command_init(&ctx, "SEND_ONCE COLORFUL CYAN %lu\n",count);break;
		case 5:r = lirc_command_init(&ctx, "SEND_ONCE COLORFUL YELLOW %lu\n",count);break;
		case 6:r = lirc_command_init(&ctx, "SEND_ONCE COLORFUL PURPLE %lu\n",count);break;
		case 7:r = lirc_command_init(&ctx, "SEND_ONCE COLORFUL SKYBLUE %lu\n",count);break;
		case 8:r = lirc_command_init(&ctx, "SEND_ONCE COLORFUL PINK %lu\n",count);break;
	}

	lirc_command_reply_to_stdout(&ctx);
	if (send_packet(&ctx) == -1) {
		//exit(EXIT_FAILURE);
	}

	color++;
	if(color>8)color=0;

}
int sel_pos=0;
void setDetector(int xp){
	int x;
	if(xp>=0){
		sel_val_max=0;
		sel_pos=sel_pos<BINSIZE?xp:BINSIZE;
		for(x=0;x<HISTORYSIZE;x++)history[x]=0;
	}
}
//////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////
/// END LIRC ADD ONS

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

int doRender=1;

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
			glTexCoord2f(1,1);glVertex2f(ww,00+speed);
			glTexCoord2f(1,0);glVertex2f(ww,wh+speed);
			glTexCoord2f(0,0);glVertex2f(00,wh+speed);
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
			HSBtoColor((float)outGraph[i]/2000.0,1,min(1,(float)outGraph[i]/(float)wh*30));
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
		glTexCoord2f(1,0);glVertex2f(ww,00);
		glTexCoord2f(1,1);glVertex2f(ww,wh);
		glTexCoord2f(0,1);glVertex2f(00,wh);
	glEnd();
	glBindTexture(GL_TEXTURE_2D,0);
	glDisable(GL_TEXTURE_2D);

	//draw black for top detector
	if(doChangeColor)glColor3f(0.5,0.5,0.5);
	else glColor3f(0.0,0.0,0.0);
	glBegin(GL_QUADS);
	glVertex2f(0,0);
	glVertex2f(ww,0);
	glVertex2f(ww,10);
	glVertex2f(0,10);
	glEnd();

	if(beat){
		glColor3f(1.0,1.0,1.0);
		glBegin(GL_QUADS);
		glVertex2f(0,0);
		glVertex2f(ww,0);
		glVertex2f(ww,10);
		glVertex2f(0,10);
		glEnd();
	}

	glBegin(GL_QUADS);
	HSBtoColor((float)outGraph[sel_pos]/1500.0,1,min(1,(float)outGraph[sel_pos]/(float)wh*30));
	glVertex2f((sel_pos-startp)*zoom,0);
	glVertex2f((sel_pos-startp)*zoom+zoom,0);
	glVertex2f((sel_pos-startp)*zoom+zoom,10);
	glVertex2f((sel_pos-startp)*zoom,10);
	glEnd();

	glBegin(GL_LINES);
	glColor3f(1.0,1.0,1.0);
	historyEnd = historyStart;
	i=0;
	do{
		glVertex2f(ww-i*3,wh-history[historyEnd]/2.0);
		historyEnd = (historyEnd + 1)%HISTORYSIZE;
		i++;
		glVertex2f(ww-i*3,wh-history[historyEnd]/2.0);
	}while(historyEnd != historyStart);

	glVertex2f(ww,wh-sel_val_max/2.0);
	glVertex2f(ww-i*3,wh-sel_val_max/2.0);

	glVertex2f(ww,wh-sel_val_max/sel_val_thresh/2.0);
	glVertex2f(ww-i*3,wh-sel_val_max/sel_val_thresh/2.0);
	glEnd();

	int len=sprintf(tmpbuf,"Start Pos=%d Zoom=%d Speed=%d SEL_POS=%d SKIP=%d:%d THRESH=%f",startp,zoom,speed,sel_pos,sel_skip,sel_skip_count,sel_val_thresh);
	text(wh-10-len*6,10,tmpbuf);

	SDL_GL_SwapBuffers();
}

void initFBO(){
	glGenTextures(1,&TEX0);
	glBindTexture(GL_TEXTURE_2D,TEX0);
	glTexParameterf(GL_TEXTURE_2D,GL_GENERATE_MIPMAP,GL_TRUE);
	glTexParameterf(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);
	glTexParameterf(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
	glTexParameterf(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
	glTexParameterf(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);
	glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,ww,wh,0,GL_RGBA,GL_UNSIGNED_BYTE,0);
	glBindTexture(GL_TEXTURE_2D,0);
	glGenFramebuffers(1,&FBO0);
	glBindFramebuffer(GL_FRAMEBUFFER,FBO0);
	glGenRenderbuffers(1,&RBO0);
	glBindRenderbuffer(GL_RENDERBUFFER,RBO0);
	glRenderbufferStorage(GL_RENDERBUFFER,GL_DEPTH_COMPONENT,ww,wh);
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
	glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,ww,wh,0,GL_RGBA,GL_UNSIGNED_BYTE,0);
	glBindTexture(GL_TEXTURE_2D,0);
	glGenFramebuffers(1,&FBO1);
	glBindFramebuffer(GL_FRAMEBUFFER,FBO1);
	glGenRenderbuffers(1,&RBO1);
	glBindRenderbuffer(GL_RENDERBUFFER,RBO1);
	glRenderbufferStorage(GL_RENDERBUFFER,GL_DEPTH_COMPONENT,ww,wh);
	glBindRenderbuffer(GL_RENDERBUFFER,0);
	glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,TEX1,0);
	glFramebufferRenderbuffer(GL_FRAMEBUFFER,GL_DEPTH_ATTACHMENT,GL_RENDERBUFFER,RBO1);
	glBindFramebuffer(GL_FRAMEBUFFER,0);
}

int main(int argc, char*argv[]) {

	ww=WW;
	wh=WH;
//LIRC ADDODNS
	pthread_t work_thread;
	running=1;
	fd = lirc_get_local_socket("/var/run/lirc/lircd", 0);
	if (fd < 0) {
		fprintf(stderr, "could not open socket: %s\n", strerror(-fd));
		//exit(EXIT_FAILURE);
		printf("Light control will be unavailable\n");
	}else{
		int status = fcntl(fd, F_SETFL, fcntl(fd, F_GETFL, 0) | O_NONBLOCK);
		///////////////////// THREADS ///////////////////////////////
		/* create a second thread which executes inc_x(&x) */
		if(pthread_create(&work_thread, NULL, worker_func, &doChangeColor)) {
			fprintf(stderr, "Error creating thread\n");
			return 1;
		}
	}
///////////////////////////////

	glewInit();
	initSDLOpenGL(ww,wh,90,"SpectroFFT");
	initfont();

	initFBO();

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

	initSDLOpenGL(ww,wh,90,PROGRAM_NAME);
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


		//beat detection
		sel_val=outGraph[sel_pos];
		double tmpthresh = sel_val_max/sel_val_thresh;
		if(sel_val > tmpthresh && sel_val_prev < tmpthresh){
			if(sel_skip_count>=sel_skip){
				doChangeColor=1;
				beat=1;
				sel_skip_count=0;
			}
			sel_skip_count++;
		}

		sel_val_prev = sel_val;

		sel_val_max = 0;
		historyEnd = historyStart;
		do{
			if(history[historyEnd] > sel_val_max) sel_val_max = history[historyEnd];
			historyEnd = (historyEnd + 1)%HISTORYSIZE;
		}while(historyEnd != historyStart);

		history[historyStart]=outGraph[sel_pos];
		historyStart = (historyStart + 1)%HISTORYSIZE;

		if(doRender)render();
		if(controls()==-2)goto finish;
		beat = 0;
	}
////////////////////////////////////////////////////////////
	running=0;
	if(pthread_join(work_thread, NULL)) {
		fprintf(stderr, "Error joining thread\n");
		return 0;
	}
	close(fd);
///////////////////////////////////////////////////////////
	ret = 0;

finish:
	free(outGraph);
	if (s)pa_simple_free(s);
	return ret;
}
