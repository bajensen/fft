int controls(){
	while(SDL_PollEvent(&event))
	switch(event.type){
		case SDL_VIDEORESIZE:
			reshape(event.resize.w,event.resize.h,90);
			break;
		case SDL_QUIT: //STOP
			return -2; break;
		case SDL_KEYDOWN:{
			switch(event.key.keysym.sym){
				case SDLK_p: fftpause=!fftpause; break;
				case SDLK_r: doRender=!doRender; break;
			}	
		}break;
		case SDL_KEYUP:break;
		case SDL_MOUSEMOTION:{
			mousex=event.motion.x;
			mousey=event.motion.y;
			if(dragging){
				startp=draginitpos-(mousex-draginitx)/zoom;
				if(startp<0)startp=0;
			}
		}break;
		case SDL_MOUSEBUTTONUP:{
			switch(event.button.button){
				case SDL_BUTTON_LEFT: 
					dragendx=event.button.x;
					dragendpos=startp;
					dragging=false; 
					break;
				case SDL_BUTTON_WHEELUP: zoom++; break;
				case SDL_BUTTON_WHEELDOWN: zoom--; if(zoom<1)zoom=1; break;
			}
		}break;
		case SDL_MOUSEBUTTONDOWN:{
			switch(event.button.button){
				case SDL_BUTTON_LEFT:
					draginitx=event.button.x;
					draginitpos=startp;
					dragging=true; 
					break;
			}
		}break;
	}
	return 0;
}
