#include "main.h"

int winx=800;
int winy=600;

#define leftmargin 10
#define rightmargin 5
#define bottombar 50
#define linedist 2
#define separator 220
#define text1size 20
#define text2asize 36
#define text2bsize 44
#define cursize 28

#define lightcol 255
#define barcol (256-16)
#define separatorcol (256-64)
#define textcol 0
#define curcol 0

#define curperiod 1000
#define curshow 600
#define savetime 10000

SDL_Window *window;
SDL_Renderer *renderer;
TTF_Font *font1,*font2a,*font2b;
SDL_AudioDeviceID audiodevice;
SDL_AudioSpec wavespec;
Uint8 *chatwave,*bellwave;
Uint32 chatwavelen,bellwavelen;
Uint8 *audiopos=NULL;
Uint32 audiolen=0;
SDL_bool quit=SDL_FALSE;
SDL_TimerID TimerID;
//SDL_mutex *update_mutex;
//SDL_bool to_update; // used for UpdateCallback
#ifdef WIN32
SDL_mutex *refresh_mutex; // used for EventFilter
#endif // WIN32
FILE *file;

#define MAX_INPUT_LEN 200
int inputlen=0,inputcur=0,curlen=0,curpos=0,inputleft=0;
char inputmsg[MAX_INPUT_LEN];
int curshift[MAX_INPUT_LEN];
Uint32 lastedit=0;
Uint32 lastblink=-10000;
Uint32 lastsave=0;
char filename[1024];
SDL_bool afk=SDL_FALSE;

Uint32 numclients=0;
Uint8 names[NETWORK_MAX_CONNECTIONS][16];
Uint8 states[NETWORK_MAX_CONNECTIONS];
Uint32 mousex[NETWORK_MAX_CONNECTIONS];
Uint32 mousey[NETWORK_MAX_CONNECTIONS];
Uint32 buttons[NETWORK_MAX_CONNECTIONS];
Uint32 times[NETWORK_MAX_CONNECTIONS];
SDL_Color colors[NETWORK_MAX_CONNECTIONS];

#define histmax 1024
Uint8 histcolors[histmax][3];
char histnames[histmax][16];
char histmsg[histmax][256];
Uint16 histcurr=0,histsize=0,histoldest=0,histbottom=0;


void AudioCallback(void *data,Uint8 *stream,int len){
    SDL_memset(stream,0,len);
	if(!audiolen)return;
	len=(len>audiolen?audiolen:len);
    //SDL_MixAudioFormat(stream,audiopos,wavespec.format,len,SDL_MIX_MAXVOLUME/2);
    SDL_memcpy(stream,audiopos,len);
    audiopos+=len;
    audiolen-=len;
}

Uint32 UpdateCallback(Uint32 t,void *p){
//    if(SDL_LockMutex(update_mutex)==0){
//        if(!to_update){
//            to_update=SDL_TRUE;
//            SDL_UnlockMutex(update_mutex);

            SDL_Event e;
            SDL_UserEvent u;

            u.type=SDL_USEREVENT;
            u.code=0;
            u.data1=NULL;
            u.data2=NULL;

            e.type=SDL_USEREVENT;
            e.user=u;

            SDL_PushEvent(&e);
//        }
//        else SDL_UnlockMutex(update_mutex);
//    }
//    else exit(EXIT_FAILURE);
    return(t);
}

void Init(SDL_bool vsynch){
    SDL_Init(SDL_INIT_VIDEO|SDL_INIT_EVENTS|SDL_INIT_TIMER|SDL_INIT_AUDIO);
    window=SDL_CreateWindow("Chattutti",SDL_WINDOWPOS_UNDEFINED,SDL_WINDOWPOS_UNDEFINED,winx,winy,SDL_WINDOW_SHOWN|SDL_WINDOW_RESIZABLE);
    renderer=SDL_CreateRenderer(window,-1,SDL_RENDERER_SOFTWARE);//SDL_RENDERER_ACCELERATED);//|SDL_RENDERER_PRESENTVSYNC);
    if(TTF_Init()==-1)exit(EXIT_FAILURE);
    font1=TTF_OpenFont("data/font1.ttf",text1size);
    if(!font1)exit(EXIT_FAILURE);
    font2a=TTF_OpenFont("data/font2.ttf",text2asize);
    if(!font2a)exit(EXIT_FAILURE);
    font2b=TTF_OpenFont("data/font2.ttf",text2bsize);
    if(!font2b)exit(EXIT_FAILURE);
    sprintf(filename,"logs_%s.txt",name);
    file=fopen(filename,"a");
    if(!file)exit(EXIT_FAILURE);
    if(!SDL_LoadWAV("data/chattutti.wav",&wavespec,&chatwave,&chatwavelen))exit(EXIT_FAILURE);
    if(!SDL_LoadWAV("data/bell.wav",&wavespec,&bellwave,&bellwavelen))exit(EXIT_FAILURE);
	wavespec.callback=AudioCallback;
	wavespec.userdata=NULL;
	audiodevice=SDL_OpenAudioDevice(NULL,0,&wavespec,NULL,SDL_AUDIO_ALLOW_ANY_CHANGE);
	if(audiodevice<0)exit(EXIT_FAILURE);
	SDL_PauseAudioDevice(audiodevice,0);
    lastsave=lastedit=SDL_GetTicks();
    InitNetwork();
    SDL_StartTextInput();
    SDL_ShowCursor(SDL_DISABLE);
    #ifdef WIN32
    refresh_mutex=SDL_CreateMutex();
    #endif // WIN32
//    update_mutex=SDL_CreateMutex();
//    to_update=SDL_FALSE;
    TimerID=SDL_AddTimer(UpdateTime,UpdateCallback,NULL);
}

void Quit(){
    SDL_RemoveTimer(TimerID);
    #ifdef WIN32
    SDL_DestroyMutex(refresh_mutex);
    #endif // WIN32
//    SDL_DestroyMutex(update_mutex);
    SDL_StopTextInput();
    DoneNetwork();
    fclose(file);
    SDL_PauseAudioDevice(audiodevice,1);
    SDL_CloseAudioDevice(audiodevice);
    SDL_FreeWAV(bellwave);
    SDL_FreeWAV(chatwave);
    TTF_CloseFont(font1);
    TTF_CloseFont(font2a);
    TTF_CloseFont(font2b);
    TTF_Quit();
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
}

#define record_length 64

Uint32 last_time,total_time;
Uint32 record[record_length];
double accumulator_time;
int rec;

void InitState(){
    int i;
    last_time=SDL_GetTicks();
    total_time=0;
    for(i=0;i<record_length;i++)total_time+=(record[i]=1000/60);
    accumulator_time=0.0;
    rec=0;
}

void UpdateState(){
    Uint32 current_time=SDL_GetTicks();
    Uint32 delta_time=current_time-last_time;
    last_time=current_time;

    if(current_time-lastsave>savetime){
        fflush(file);
        lastsave=current_time;
    }

total_time=total_time-record[rec]+delta_time;
record[rec++]=delta_time;
if(rec>=record_length)rec=0;
}

#define UNKNOWN_UNICODE 0xFFFD
static Uint32 UTF8_getch(const char **src, size_t *srclen)
{
    const Uint8 *p = *(const Uint8**)src;
    size_t left = 0;
    SDL_bool underflow = SDL_FALSE;
    Uint32 ch = UNKNOWN_UNICODE;

    if (*srclen == 0) {
        return UNKNOWN_UNICODE;
    }
    if (p[0] >= 0xFC) {
        if ((p[0] & 0xFE) == 0xFC) {
            ch = (Uint32) (p[0] & 0x01);
            left = 5;
        }
    } else if (p[0] >= 0xF8) {
        if ((p[0] & 0xFC) == 0xF8) {
            ch = (Uint32) (p[0] & 0x03);
            left = 4;
        }
    } else if (p[0] >= 0xF0) {
        if ((p[0] & 0xF8) == 0xF0) {
            ch = (Uint32) (p[0] & 0x07);
            left = 3;
        }
    } else if (p[0] >= 0xE0) {
        if ((p[0] & 0xF0) == 0xE0) {
            ch = (Uint32) (p[0] & 0x0F);
            left = 2;
        }
    } else if (p[0] >= 0xC0) {
        if ((p[0] & 0xE0) == 0xC0) {
            ch = (Uint32) (p[0] & 0x1F);
            left = 1;
        }
    } else {
        if ((p[0] & 0x80) == 0x00) {
            ch = (Uint32) p[0];
        }
    }
    ++*src;
    --*srclen;
    while (left > 0 && *srclen > 0) {
        ++p;
        if ((p[0] & 0xC0) != 0x80) {
            ch = UNKNOWN_UNICODE;
            break;
        }
        ch <<= 6;
        ch |= (p[0] & 0x3F);
        ++*src;
        --*srclen;
        --left;
    }
    if (left > 0) {
        underflow = SDL_TRUE;
    }
    if (underflow ||
        (ch >= 0xD800 && ch <= 0xDFFF) ||
        (ch == 0xFFFE || ch == 0xFFFF) || ch > 0x10FFFF) {
        ch = UNKNOWN_UNICODE;
    }
    return ch;
}

#include <math.h>
void hsv2rgb(Uint8 h,Uint8 v,Uint8 *r,Uint8 *g,Uint8 *b){
    double i;
    double cx=h/256.0;
    double px=fabsf(modf(cx+1.0,&i)*6.0-3.0)-1.0;
    if(px<0.0)px=0.0;
    if(px>1.0)px=1.0;
    int x=(int)(px*v);
    if(x==256)x=255;
    *r=(Uint8)x;
    px=fabsf(modf(cx+2.0/3.0,&i)*6.0-3.0)-1.0;
    if(px<0.0)px=0.0;
    if(px>1.0)px=1.0;
    x=(int)(px*v);
    if(x==256)x=255;
    *g=(Uint8)x;
    px=fabsf(modf(cx+1.0/3.0,&i)*6.0-3.0)-1.0;
    if(px<0.0)px=0.0;
    if(px>1.0)px=1.0;
    x=(int)(px*v);
    if(x==256)x=255;
    *b=(Uint8)x;
}

void StringToColor(Uint8 *str,SDL_Color *c){
    int n;
    unsigned long hash=5381;
    while((n=*str++))hash=((hash<<5)+hash)+n;
    hsv2rgb(hash^hash>>8^hash>>16^hash>>24,192,&c->r,&c->g,&c->b);
    c->a=255;
}

void RenderInput(){
    char str[1024];
    SDL_Surface *sur;
    SDL_Texture *t;
    SDL_Rect s,r;
    int wid,curx=0;

    if(separator+leftmargin>=winx-rightmargin)return;

    memcpy(str,inputmsg,inputcur);
    str[inputcur]=0;
    if(curpos>0)TTF_SizeUTF8(font1,str,&wid,NULL);
    else wid=0;

    if(inputlen>0){
        memcpy(str,inputmsg,inputlen);
        str[inputlen]=0;
        sur=TTF_RenderUTF8_Shaded(font1,str,(SDL_Color){textcol,textcol,textcol,255},(SDL_Color){barcol,barcol,barcol,255});
        t=SDL_CreateTextureFromSurface(renderer,sur);

        int inputright=inputleft+winx-rightmargin-separator-leftmargin;
        if(wid<inputleft){
            inputright-=inputleft-wid;
            inputleft=wid;
        }
        if(wid>inputright){
            inputleft+=wid-inputright;
            inputright=wid;
        }
        if(inputleft>0 && sur->w<inputright){
            int d;
            if(inputleft<inputright-sur->w)d=inputleft;
            else d=inputright-sur->w;
            inputleft-=d;
            inputright-=d;
        }

        s.x=inputleft;
        s.y=0;
        s.w=inputright-inputleft+1;
        if(s.x+s.w>sur->w)s.w=sur->w-s.x;
        s.h=sur->h;

        r.x=separator+leftmargin;
        r.y=winy-bottombar/2-sur->h/2;
        r.w=s.w;
        r.h=s.h;

        SDL_FreeSurface(sur);
        SDL_RenderCopy(renderer,t,&s,&r);
        SDL_DestroyTexture(t);

        curx=r.x+wid-s.x;
    }
    else curx=separator+leftmargin;

    if(!afk){
        SDL_SetRenderDrawColor(renderer,curcol,curcol,curcol,255);
        if((last_time-lastedit)%curperiod<curshow)SDL_RenderDrawLine(renderer,curx,winy-bottombar/2-cursize/2,curx,winy-bottombar/2+cursize/2);
    }
}

int NextTextStop(TTF_Font *font,char *str,int wid){
    int i=0,j=-1,k,w;
    SDL_bool ns;
    char c;
    while(str[i]){
        ns=SDL_FALSE;
        while(str[i] && str[i]!=0xA && ((str[i]!=' ' && str[i]!='\t')|| !ns)){
            if(str[i]!=' ' && str[i]!='\t')ns=SDL_TRUE;
            i++;
        }
        c=str[i];
        str[i]=0;
        if(i)TTF_SizeUTF8(font,str,&w,NULL);
        else w=0;
        str[i]=c;
        if(w>wid){
            if(j>-1)i=j;
            break;
        }
        if(str[i] && str[i]!=0xA)j=i++;
        else break;
    }
    if(str[i]==0 || str[i]==0xA){
        str[i]=0;
        return(i+1);
    }
    k=i;
    while(1){
        i++;
        if(str[i]==0 || str[i]==0xA)break;
        if(str[i]!=' ' && str[i]!='\t'){
            i=k;
            break;
        }
    }
    str[i]=0;
    return(i+1);
}

int RenderText(TTF_Font *font,char *str,int x,int y,Uint8 *fg,int bg,int halign,int valign){
    SDL_Surface *s;
    SDL_Texture *t;
    SDL_Rect r;

    int i=0;
    if(str[0]==0)return(TTF_FontHeight(font));
    while(str[i++])if(i>MAX_INPUT_LEN)return(TTF_FontHeight(font));

    SDL_Color c;
    c.a=255;
    if(fg){
        c.r=fg[0];
        c.g=fg[1];
        c.b=fg[2];
    }
    else c.r=c.g=c.b=textcol;

    s=TTF_RenderUTF8_Shaded(font,str,c,(SDL_Color){bg,bg,bg,255});
    if(!s)return(TTF_FontHeight(font));
    t=SDL_CreateTextureFromSurface(renderer,s);

    switch(halign){
    case 0:
        r.x=x;
        break;
    case 1:
        r.x=x-s->w/2;
        break;
    default:
        r.x=x-s->w+1;
        break;
    }
    switch(valign){
    case 0:
        r.y=y;
        break;
    case 1:
        r.y=y-s->h/2;
        break;
    default:
        r.y=y-s->h+1;
        break;
    }
    r.w=s->w;
    r.h=s->h;
    SDL_FreeSurface(s);
    SDL_RenderCopy(renderer,t,NULL,&r);
    SDL_DestroyTexture(t);
    return(r.h);
}

int RenderTextWrap(TTF_Font *font,char *str,int x,int y,Uint8 *fg,int bg,int halign){
    SDL_Surface *s;
    SDL_Texture *t;
    SDL_Rect r;
    char strt[256];

    int i=0,j=0,h=0;
    if(str[0]==0)return(TTF_FontHeight(font));
    while(str[i++])if(i>MAX_INPUT_LEN)return(TTF_FontHeight(font));

    SDL_Color c;
    c.a=255;
    if(fg){
        c.r=fg[0];
        c.g=fg[1];
        c.b=fg[2];
    }
    else c.r=c.g=c.b=textcol;

    memcpy(strt,str,i);
    for(j=0;j<i;j++)if(strt[j]==0xD)strt[j]=' ';

    j=0;
    while(j<i){
        j+=NextTextStop(font,strt+j,winx-x);
        h+=TTF_FontHeight(font);
        if(j<i)h+=linedist;
    }
    y-=h;
    j=0;
    while(j<i){
        s=TTF_RenderUTF8_Shaded(font,strt+j,c,(SDL_Color){bg,bg,bg,255});
        while(strt[j++]);

        if(s){
            t=SDL_CreateTextureFromSurface(renderer,s);
            switch(halign){
            case 0:
                r.x=x;
                break;
            case 1:
                r.x=x-s->w/2;
                break;
            default:
                r.x=x-s->w+1;
                break;
            }
            r.y=y;
            r.w=s->w;
            r.h=s->h;
            SDL_FreeSurface(s);
            SDL_RenderCopy(renderer,t,NULL,&r);
            SDL_DestroyTexture(t);
        }
        else r.h=TTF_FontHeight(font);
        y+=r.h;
        if(j<i)y+=linedist;
    }
    return(h);
}

void RenderBubble(int i,SDL_bool solid){
    SDL_Surface *s;
    SDL_Texture *t;
    SDL_Rect r;
    TTF_Font *f;
    Uint8 a;

    if(names[i][0]==0)return;

    if(solid){
        if(states[i]!=1)return;
        a=255;
        f=font2b;
    }
    else{
        if(states[i]==1)return;
        a=128;
        if(times[i]>1000)a-=(times[i]-1000)*128/9000;
        f=font2a;
    }

    s=TTF_RenderUTF8_Blended(f,(char*)names[i],colors[i]);

    t=SDL_CreateTextureFromSurface(renderer,s);
    SDL_SetTextureBlendMode(t,SDL_BLENDMODE_BLEND);
    SDL_SetTextureAlphaMod(t,a);

    r.x=mousex[i]-s->w/2;
    r.y=winy-mousey[i]-s->h/2;
    r.w=s->w;
    r.h=s->h;

    if(r.x>winx-r.w)r.x=winx-r.w;
    if(r.y>winy-r.h-bottombar)r.y=winy-r.h-bottombar;
    if(r.x<0)r.x=0;
    if(r.y<0)r.y=0;

    SDL_FreeSurface(s);
    SDL_RenderCopy(renderer,t,NULL,&r);
    SDL_DestroyTexture(t);
}

void RenderMyBubble(SDL_bool solid){
    SDL_Surface *s;
    SDL_Texture *t;
    SDL_Rect r;
    TTF_Font *f;
    Uint8 a;
    int x,y,wx,wy;

    if(name[0]==0)return;

    if(solid){
        if(last_time-lastblink>=150)return;
        a=255;
        f=font2b;
    }
    else{
        if(last_time-lastblink<150)return;
        a=128;
        f=font2a;
    }

    s=TTF_RenderUTF8_Blended(f,(char*)name,color);

    t=SDL_CreateTextureFromSurface(renderer,s);
    SDL_SetTextureBlendMode(t,SDL_BLENDMODE_BLEND);
    SDL_SetTextureAlphaMod(t,a);

    SDL_GetGlobalMouseState(&x,&y);
    SDL_GetWindowPosition(window,&wx,&wy);
    x-=wx;
    y-=wy;

    r.x=x-s->w/2;
    r.y=y-s->h/2;
    r.w=s->w;
    r.h=s->h;

    if(r.x>winx-r.w)r.x=winx-r.w;
    if(r.y>winy-r.h-bottombar)r.y=winy-r.h-bottombar;
    if(r.x<0)r.x=0;
    if(r.y<0)r.y=0;

    SDL_FreeSurface(s);
    SDL_RenderCopy(renderer,t,NULL,&r);
    SDL_DestroyTexture(t);
}

void Render(){
    SDL_Rect r;

    if(SDL_GetWindowFlags(window)&SDL_WINDOW_MINIMIZED)return;
    SDL_GetWindowSize(window,&winx,&winy);

    SDL_SetRenderDrawColor(renderer,lightcol,lightcol,lightcol,255);
    SDL_RenderClear(renderer);

    r.x=0;
    r.y=winy-bottombar;
    r.w=winx;
    r.h=bottombar;

    SDL_SetRenderDrawColor(renderer,barcol,barcol,barcol,255);
    SDL_RenderFillRect(renderer,&r);

    Uint8 c[3];
    c[0]=color.r;
    c[1]=color.g;
    c[2]=color.b;
    RenderText(font1,(char*)name,separator-leftmargin,winy-bottombar/2,c,barcol,2,1);

    RenderInput();

    Uint16 i=histbottom;
    if(histsize){
        int y=winy-bottombar-leftmargin;
        while(y>0){
            y-=RenderTextWrap(font1,histmsg[i],separator+leftmargin,y,NULL,lightcol,0);
            RenderText(font1,histnames[i],separator-leftmargin,y,histcolors[i],lightcol,2,0);
            y-=linedist;
            if(i==histoldest)break;
            i--;
            i%=histmax;
        }
    }

    SDL_SetRenderDrawColor(renderer,separatorcol,separatorcol,separatorcol,255);
    SDL_RenderDrawLine(renderer,separator,0,separator,winy-1);
    SDL_RenderDrawLine(renderer,0,winy-bottombar,winx-1,winy-bottombar);

    SDL_SetRenderDrawBlendMode(renderer,SDL_BLENDMODE_BLEND);
    for(i=0;i<numclients;i++)RenderBubble(i,SDL_FALSE);
    RenderMyBubble(SDL_FALSE);
    for(i=0;i<numclients;i++)RenderBubble(i,SDL_TRUE);
    RenderMyBubble(SDL_TRUE);
    SDL_SetRenderDrawBlendMode(renderer,SDL_BLENDMODE_NONE);

    SDL_RenderPresent(renderer);
}

void FlushEvents(){
	SDL_Event event;
	while(SDL_PollEvent(&event));
}

void WindowEvent(SDL_WindowEvent *win){
    if(win->event==SDL_WINDOWEVENT_FOCUS_GAINED){
        afk=SDL_FALSE;
        lastedit=last_time;
    }
    if(win->event==SDL_WINDOWEVENT_FOCUS_LOST)afk=SDL_TRUE;
//    if(win->event==SDL_WINDOWEVENT_SIZE_CHANGED || win->event==SDL_WINDOWEVENT_RESIZED){
//        winx=win->data1;
//        winy=win->data2;
//    }
}

void AddKey(char *c){
    int i,j=0;
    while(c[j++]);
    if(j>0 && inputlen+j<=MAX_INPUT_LEN){
        j=0;
        while(c[j]){
            for(i=inputlen;i>inputcur;i--)inputmsg[i]=inputmsg[i-1];
            inputmsg[inputcur++]=c[j++];
            inputlen++;
        }
        for(i=MAX_INPUT_LEN-1;i>curpos;i--)curshift[i]=curshift[i-1];
        curshift[curpos++]=j;
        curlen++;
        lastedit=lastblink=last_time;
    }
}

void DeleteKey(){
    int i,j;
    if(curpos>=curlen)return;
    j=curshift[curpos];
    for(i=inputcur;i<inputlen-j;i++)inputmsg[i]=inputmsg[i+j];
    for(i=curpos;i<curlen-1;i++)curshift[i]=curshift[i+1];
    inputlen-=j;
    curlen--;
    lastedit=lastblink=last_time;
}

void SendString(){
    Uint8 str[256];
    str[0]=254;
    memcpy(str+1,inputmsg,inputlen);
    EnqueueMessage(connection,str,inputlen+1);
}

void ScrollChat(int n){
    int i;
    if(histsize){
        if(n<0){
            for(i=0;i>n;i--){
                if(histbottom!=histoldest){
                    histbottom--;
                    histbottom%=histmax;
                }
            }
        }
        else{
            for(i=0;i<n;i++){
                if(histbottom!=histcurr){
                    histbottom++;
                    histbottom%=histmax;
                }
            }
        }
    }
}

SDL_bool keyv=SDL_FALSE;
SDL_bool keylc=SDL_FALSE;
SDL_bool keyrc=SDL_FALSE;
SDL_bool keyls=SDL_FALSE;
SDL_bool keyrs=SDL_FALSE;
SDL_bool keyins=SDL_FALSE;

void CheckCopy(){
    if((keyv&&keylc)||(keyv&&keyrc)||(keyins&&keyls)||(keyins&&keyrs)){
        if(!SDL_HasClipboardText())return;
        char *text=SDL_GetClipboardText();
        if(!text)return;
        const char *t1=text,*t2=text;
        size_t l1,l2;
        char c[32];
        l1=l2=strlen(t2);
        while(l2>0){
            UTF8_getch(&t2,&l2);
            memcpy(c,t1,l1-l2);
            c[l1-l2]=0;
            AddKey(c);
            t1=t2;
            l1=l2;
        }
        SDL_free(text);
    }
}

void KeyPressed(SDL_Keycode key){
    switch(key){
    case SDLK_ESCAPE:
        //quit=SDL_TRUE;
        if(curlen>0){
            inputcur=inputlen=curpos=curlen=0;
            lastedit=lastblink=last_time;
        }
        break;
    case SDLK_UP:
        ScrollChat(-1);
        break;
    case SDLK_DOWN:
        ScrollChat(1);
        break;
    case SDLK_PAGEUP:
        ScrollChat(-10);
        break;
    case SDLK_PAGEDOWN:
        ScrollChat(10);
        break;
    case SDLK_LEFT:
        if(curpos>0){
            inputcur-=curshift[--curpos];
            lastedit=last_time;
        }
        break;
    case SDLK_RIGHT:
        if(curpos<curlen){
            inputcur+=curshift[curpos++];
            lastedit=last_time;
        }
        break;
    case SDLK_HOME:
        if(curpos>0){
            inputcur=curpos=0;
            lastedit=last_time;
        }
        break;
    case SDLK_END:
        if(curpos<curlen){
            inputcur=inputlen;
            curpos=curlen;
            lastedit=last_time;
        }
        break;
    case SDLK_BACKSPACE:
        if(curpos>0){
            inputcur-=curshift[--curpos];
            DeleteKey();
        }
        break;
    case SDLK_DELETE:
        if(curpos<curlen)DeleteKey();
        break;
    case SDLK_RETURN:
        if(curpos>0){
            SendString();
            inputcur=inputlen=curpos=curlen=0;
            lastedit=lastblink=last_time;
        }
        break;
    case SDLK_v:
        keyv=SDL_TRUE;
        CheckCopy();
        break;
    case SDLK_LCTRL:
        keylc=SDL_TRUE;
        CheckCopy();
        break;
    case SDLK_RCTRL:
        keyrc=SDL_TRUE;
        CheckCopy();
        break;
    case SDLK_LSHIFT:
        keyls=SDL_TRUE;
        CheckCopy();
        break;
    case SDLK_RSHIFT:
        keyrs=SDL_TRUE;
        CheckCopy();
        break;
    case SDLK_INSERT:
        keyins=SDL_TRUE;
        CheckCopy();
        break;
    default:
        break;
    }
}

void KeyReleased(SDL_Keycode key){
    switch(key){
    case SDLK_v:
        keyv=SDL_FALSE;
        break;
    case SDLK_LCTRL:
        keylc=SDL_FALSE;
        break;
    case SDLK_RCTRL:
        keyrc=SDL_FALSE;
        break;
    case SDLK_LSHIFT:
        keyls=SDL_FALSE;
        break;
    case SDLK_RSHIFT:
        keyrs=SDL_FALSE;
        break;
    case SDLK_INSERT:
        keyins=SDL_FALSE;
        break;
    default:
        break;
    }
}

void MousePressed(SDL_MouseButtonEvent *button){
    Uint8 d=253;
    if(button->button==SDL_BUTTON_RIGHT){
        EnqueueMessage(connection,&d,1);
        lastblink=last_time;
    }
}

void MouseWheel(SDL_MouseWheelEvent *wheel){
    ScrollChat(-wheel->y);
}

//void MouseMoved(SDL_MouseMotionEvent *motion){
//    //if(motion->state && SDL_BUTTON_LMASK)rotate(motion->xrel,motion->yrel);
//    //CameraRotate(rad(motion->xrel)*0.1,rad(-motion->yrel)*0.1);
//}

void MainLoop();

void Events(){
    SDL_Event e;
    if(SDL_WaitEvent(&e)){
        switch(e.type){
        case SDL_QUIT:
            quit=SDL_TRUE;
            break;
        case SDL_USEREVENT:
            MainLoop();
            break;
        case SDL_WINDOWEVENT:
            WindowEvent(&e.window);
            break;
        case SDL_TEXTINPUT:
            AddKey(e.text.text);
            break;
        case SDL_KEYDOWN:
            KeyPressed(e.key.keysym.sym);
            break;
        case SDL_KEYUP:
            KeyReleased(e.key.keysym.sym);
            break;
        case SDL_MOUSEBUTTONDOWN:
            MousePressed(&e.button);
            break;
        case SDL_MOUSEWHEEL:
            MouseWheel(&e.wheel);
            break;
//        case SDL_MOUSEMOTION:
//            MouseMoved(&e.motion);
//            break;
        }
	}
}

Uint32 Tnumclients;
Uint8 Tnames[NETWORK_MAX_CONNECTIONS][16];
Uint8 Tstates[NETWORK_MAX_CONNECTIONS];
Uint32 Tmousex[NETWORK_MAX_CONNECTIONS];
Uint32 Tmousey[NETWORK_MAX_CONNECTIONS];
Uint32 Tbuttons[NETWORK_MAX_CONNECTIONS];
Uint32 Ttimes[NETWORK_MAX_CONNECTIONS];
SDL_Color Tcolors[NETWORK_MAX_CONNECTIONS];

void ReadTimeCritical(){
    int i,j;
    SDL_bool z;
    Uint32 s;
    ReadBits(&Tnumclients,8);
    for(i=0;i<Tnumclients;i++){
        ReadBits(&s,8);
        Tstates[i]=s;
        ReadBits(Tmousex+i,16);
        ReadBits(Tmousey+i,16);
        ReadBits(Tbuttons+i,32);
        ReadBits(Ttimes+i,16);
        ReadBits(&s,8);
        Tcolors[i].r=s;
        ReadBits(&s,8);
        Tcolors[i].g=s;
        ReadBits(&s,8);
        Tcolors[i].b=s;
        Tcolors[i].a=255;
        ReadBytes(Tnames[i],16);
        z=SDL_FALSE;
        for(j=0;j<16;j++)if(!Tnames[i][j])z=SDL_TRUE;
        if(!z)Tnames[i][0]=0;
    }
}

void ProcessTimeCritical(){
    numclients=Tnumclients;
    memcpy(names,Tnames,sizeof(names));
    memcpy(states,Tstates,sizeof(states));
    memcpy(mousex,Tmousex,sizeof(mousex));
    memcpy(mousey,Tmousey,sizeof(mousey));
    memcpy(times,Ttimes,sizeof(times));
    memcpy(colors,Tcolors,sizeof(colors));
}

void WriteTimeCritical(){
    if(afk)WriteBits(0,8);
    else{
        if(last_time-lastblink<150)WriteBits(1,8);
        else WriteBits(2,8);
    }
    int x,y,wx,wy;
    Uint32 b=SDL_GetGlobalMouseState(&x,&y);
    SDL_GetWindowPosition(window,&wx,&wy);
    x-=wx;
    y-=wy;
    y=winy-y;
    if(x<0)x=0;
    if(y<0)y=0;
    WriteBits((Uint16)x,16);
    WriteBits((Uint16)y,16);
    WriteBits(b,32);
}

void ExecuteMessage(Connection *connection,void *data,Uint32 len){
    int i;
    SDL_bool a,z;
    switch(((Uint8*)data)[0]){
    case 255: // connection
        audiopos=chatwave;
        audiolen=chatwavelen;
        break;
    case 254: // chat message
        a=SDL_FALSE;
        if(histsize){
            if(histcurr==histbottom){
                histbottom++;
                histbottom%=histmax;
                a=SDL_TRUE;
            }
            histcurr++;
            histcurr%=histmax;
        }
        data++;
        memcpy(histcolors[histcurr],data,3);
        data+=3;
        z=SDL_FALSE;
        for(i=0;i<16;i++)if(!((Uint8*)data)[i]){z=SDL_TRUE;break;}
        if(!z)((Uint8*)data)[0]=0;
        memcpy(histnames[histcurr],data,16);
        z=SDL_FALSE;
        for(i=0;i<MAX_INPUT_LEN;i++)if(!((Uint8*)data)[i+16]){z=SDL_TRUE;break;}
        if(!z)((Uint8*)data)[16]=0;
        memcpy(histmsg[histcurr],data+16,len-20);
        if(histsize<histmax)histsize++;
        else{
            if(histbottom==histoldest && !a){
                histbottom++;
                histbottom%=histmax;
            }
            histoldest++;
            histoldest%=histmax;
        }
        fprintf(file,"<%s> %s\n",(char*)data,(char*)data+16);
        break;
    case 253: // alert
        audiopos=bellwave;
        audiolen=bellwavelen;
        break;
    }
}

#ifdef WIN32
int EventFilter(void *data,SDL_Event *event){
    if(event->type==SDL_WINDOWEVENT){
        if(event->window.event==SDL_WINDOWEVENT_RESIZED || event->window.event==SDL_WINDOWEVENT_MOVED){
            if(SDL_LockMutex(refresh_mutex)==0){
                SDL_Event e;
                while(SDL_PollEvent(&e)){
                    switch(e.type){
                    case SDL_QUIT:
                        quit=SDL_TRUE;
                        break;
                    case SDL_WINDOWEVENT:
                        WindowEvent(&e.window);
                        break;
                    case SDL_TEXTINPUT:
                        AddKey(e.text.text);
                        break;
                    case SDL_KEYDOWN:
                        KeyPressed(e.key.keysym.sym);
                        break;
                    case SDL_KEYUP:
                        KeyReleased(e.key.keysym.sym);
                        break;
                    case SDL_MOUSEBUTTONDOWN:
                        MousePressed(&e.button);
                        break;
                    case SDL_MOUSEWHEEL:
                        MouseWheel(&e.wheel);
                        break;
            //        case SDL_MOUSEMOTION:
            //            MouseMoved(&e.motion);
            //            break;
                    }
                }
                MainLoop();
                SDL_UnlockMutex(refresh_mutex);
            }
            else exit(EXIT_FAILURE);
        }
    }
    return(1);
}
#endif // WIN32

#include <time.h>
void PickColor(){
    srand(time(NULL));
    color.a=255;
    Uint8 *str=name;
    Uint8 n;
    Uint8 hash=5381%256;
    while((n=*str++))hash=((hash<<5)+hash)+n;
    hsv2rgb(hash+rand()%256,192,&color.r,&color.g,&color.b);
}

void MainLoop(){
//    if(SDL_LockMutex(update_mutex)==0){
//        to_update=SDL_FALSE;
//        SDL_UnlockMutex(update_mutex);
//    }
//    else exit(EXIT_FAILURE);
    UpdateState();
    UpdateNetwork(ReadTimeCritical,ProcessTimeCritical,WriteTimeCritical,ExecuteMessage);
    Render();
}

int main(int argc,char *argv[]){
    if(argc!=2){
        printf("\nUsage: Chattutti nickname\n(max. 15 characters)\n\n");
        exit(EXIT_FAILURE);
    }
    else{
        int i;
        for(i=0;argv[1][i];i++){
            if(i>=15){
                printf("\nUsage: Chattutti nickname\n(max. 15 characters)\n\n");
                exit(EXIT_FAILURE);
            }
            name[i]=argv[1][i];
        }
        name[i]=0;
        namelen=i;
    }
    PickColor();
    Init(SDL_TRUE);
    #ifdef WIN32
    SDL_SetEventFilter(&EventFilter,NULL);
    #endif // WIN32
    FlushEvents();
    InitState();
    while(!quit){
        #ifdef WIN32
        if(SDL_LockMutex(refresh_mutex)==0){
        #endif // WIN32
            Events();
        #ifdef WIN32
            SDL_UnlockMutex(refresh_mutex);
        }
        else exit(EXIT_FAILURE);
        #endif // WIN32
    }
    Quit();
    return(EXIT_SUCCESS);
}
