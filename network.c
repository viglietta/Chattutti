#include "main.h"

//#define NETWORK_TEST

UDPsocket socket;
UDPpacket *packetin,*packetout;

#ifdef NETWORK_TEST
#include <time.h>
static void CopyPacket(UDPpacket *dest,UDPpacket *source){
    memcpy(&dest->address,&source->address,sizeof(IPaddress));
    dest->channel=source->channel;
    dest->len=source->len;
    dest->maxlen=source->maxlen;
    dest->status=source->status;
    memcpy(dest->data,source->data,NETWORK_PACKET_DATA_SIZE*4);
}

static void SendPacket(SDL_bool flush){
    #define PACKET_POOL_SIZE 8
    #define DUPLICATE_PACKET 1
    #define LOSE_PACKET 1
    int i,j;
    static UDPpacket *pp[PACKET_POOL_SIZE];
    static UDPpacket *temp;
    static SDL_bool first=SDL_TRUE;
    if(first){
        for(i=0;i<PACKET_POOL_SIZE;i++){
            pp[i]=SDLNet_AllocPacket(NETWORK_PACKET_DATA_SIZE*4);
            if(!pp[i])exit(EXIT_FAILURE);
            pp[i]->len=0;
        }
        temp=SDLNet_AllocPacket(NETWORK_PACKET_DATA_SIZE*4);
        if(!temp)exit(EXIT_FAILURE);
        srand(time(NULL));
        first=SDL_FALSE;
    }
    if(flush){
        for(i=0;i<PACKET_POOL_SIZE;i++)pp[i]->len=0;
    }
    else if(rand()%LOSE_PACKET==0){
        i=rand()%PACKET_POOL_SIZE;
        CopyPacket(temp,packetout);
        CopyPacket(packetout,pp[i]);
        CopyPacket(pp[i],temp);
        if(packetout->len)SDLNet_UDP_Send(socket,-1,packetout);
        if(rand()%DUPLICATE_PACKET){
            j=rand()%PACKET_POOL_SIZE;
            CopyPacket(packetout,pp[j]);
            if(i!=j)CopyPacket(pp[i],pp[j]);
            if(packetout->len)SDLNet_UDP_Send(socket,-1,packetout);
        }
    }
}
#endif // NETWORK_TEST

static inline Sint32 SeqDiff(Sint32 new,Sint32 stored){ // sequence wrap-around
    Sint32 d=new-stored;
    if(d>0){
        if(d<0x8000)return(d);
        return(d-0x10000);
    }
    if(d<0){
        if(d>-0x8000)return(d);
        return(d+0x10000);
    }
    return(0);
}

static void ConnectionReset(Connection *c,Uint32 t){
    int i;
    c->sequence=0;
    c->ack=0xFFFF;
    c->ackbits=0;
    c->send_delay=NETWORK_LOW_DELAY;
    c->last_receive_time=c->last_send_time=t;
    c->good_congestion_time=c->good_mode_time=t;
    c->good_threshold=NETWORK_GOOD_THRESHOLD_MIN;
    c->rtt=c->send_delay*3.0f;
    c->packlost=0.0f;
    for(i=0;i<NETWORK_SEQUENCE_BUFFER;i++){
        PacketInfo *info=c->seqbufferin+i;
        info->sequence=0xFFFFFFFF;
        info->acked=SDL_TRUE;
        info->len=info->msgnum=0;
        info=c->seqbufferout+i;
        info->sequence=0xFFFFFFFF;
        info->acked=SDL_TRUE;
        info->len=info->msgnum=0;
    }
    c->msgseq=c->msgack=c->msgqueue=c->msgread=0;
    for(i=0;i<NETWORK_MESSAGE_BUFFER;i++){
        MessageInfo *msginfo=c->msgbufferin+i;
        msginfo->sequence=0xFFFFFFFF;
        msginfo->active=SDL_FALSE;
        msginfo->len=0;
        msginfo=c->msgbufferout+i;
        msginfo->sequence=0xFFFFFFFF;
        msginfo->active=SDL_FALSE;
        msginfo->len=0;
    }
}

static Connection *ConnectionNew(IPaddress *ip,Uint32 t){
    Connection *c=malloc(sizeof(Connection));
    memcpy(&c->ip,ip,sizeof(IPaddress));
    ConnectionReset(c,t);
    return(c);
}

static void ConnectionFree(Connection *c){
    free(c);
}

SDL_bool EnqueueMessage(Connection *connection,void *data,Uint32 len){ // len>0
    if(connection->msgqueue>=NETWORK_MESSAGE_BUFFER)return(SDL_FALSE);
    if(len>NETWORK_MESSAGE_SIZE)return(SDL_FALSE);
    MessageInfo *msginfo=connection->msgbufferout+connection->msgseq%NETWORK_MESSAGE_BUFFER;
    msginfo->sequence=connection->msgseq;
    msginfo->active=SDL_FALSE;
    msginfo->time=0; // irrelevant because message has not been sent yet
    msginfo->len=len;
    memcpy(msginfo->data,data,len);
    connection->msgseq++;
    connection->msgqueue++;
    return(SDL_TRUE);
}

static SDL_bool ProcessMessage(Connection *connection,MsgProcFun mp){
    MessageInfo *msginfo=connection->msgbufferin+connection->msgread%NETWORK_MESSAGE_BUFFER;
    if(msginfo->active){
        msginfo->active=SDL_FALSE;
        mp(connection,msginfo->data,msginfo->len);
        connection->msgread++;
        return(SDL_TRUE);
    }
    else return(SDL_FALSE);
}

#ifdef SERVER
SDL_bool CheckConnect(){
    Uint32 msgseq;
    if(!ReadBits(&msgseq,16))return(SDL_FALSE);
    Uint32 len;
    if(!ReadBits(&len,8))return(SDL_FALSE);
    if(len>15)return(SDL_FALSE);
    Uint8 character;
    if(!ReadBytes(&character,1))return(SDL_FALSE);
    if(character!=255)return(SDL_FALSE);
    if(!ReadBytes(&character,1))return(SDL_FALSE);
    return(SDL_TRUE);
}

#else //SERVER
void EnqueueConnect(){
    Uint8 c[19];
    c[0]=255;
    c[1]=color.r;
    c[2]=color.g;
    c[3]=color.b;
    memcpy(c+4,name,namelen);
    if(!EnqueueMessage(connection,c,namelen+4))exit(EXIT_FAILURE);
}

void ResetNetwork(){ // just clear packet fields without deallocating them!!!
#ifdef NETWORK_TEST
SendPacket(SDL_TRUE);
SDL_Delay(1000);
#endif // NETWORK_TEST
    UDPsocket s=SDLNet_UDP_Open(0);
    if(!s)exit(EXIT_FAILURE);
    SDLNet_UDP_Close(socket);
    SDLNet_FreePacket(packetin);
    SDLNet_FreePacket(packetout);
    packetin=packetout=NULL;
    socket=s;
    SDL_Delay(1000); // NOT GOOD!
    packetin=SDLNet_AllocPacket(NETWORK_PACKET_DATA_SIZE*4);
    if(!packetin)exit(EXIT_FAILURE);
    packetout=SDLNet_AllocPacket(NETWORK_PACKET_DATA_SIZE*4);
    if(!packetout)exit(EXIT_FAILURE);
    memset(&packetin->address,0,sizeof(IPaddress));
    memset(&packetout->address,0,sizeof(IPaddress));
    if(SDLNet_ResolveHost(&packetout->address,SERVER_IP,SERVER_PORT)==-1)exit(EXIT_FAILURE);
    ConnectionReset(connection,last_time);
    EnqueueConnect();
}
#endif // SERVER

void InitNetwork(){
    if(SDLNet_Init()<0)exit(EXIT_FAILURE);
    packetin=SDLNet_AllocPacket(NETWORK_PACKET_DATA_SIZE*4);
    if(!packetin)exit(EXIT_FAILURE);
    packetout=SDLNet_AllocPacket(NETWORK_PACKET_DATA_SIZE*4);
    if(!packetout)exit(EXIT_FAILURE);
    memset(&packetin->address,0,sizeof(IPaddress));
    memset(&packetout->address,0,sizeof(IPaddress));
    #ifdef SERVER
    HashNew(&connections,NETWORK_EXPECTED_CONNECTIONS*2,HashMap8,sizeof(IPaddress));
    socket=SDLNet_UDP_Open(SERVER_PORT);
    #else // SERVER
    if(SDLNet_ResolveHost(&packetout->address,SERVER_IP,SERVER_PORT)==-1)exit(EXIT_FAILURE);
    connection=ConnectionNew(&packetout->address,last_time);
    EnqueueConnect();
    socket=SDLNet_UDP_Open(0);
    #endif // SERVER
    if(!socket)exit(EXIT_FAILURE);
}

static void ProcessAck(Connection *connection,Uint16 ack,Uint32 t){
    int i;
    PacketInfo *info=connection->seqbufferout+ack%NETWORK_SEQUENCE_BUFFER;
    if(info->sequence==ack){
        if(!info->acked){
            info->acked=SDL_TRUE;
            connection->rtt+=(t-info->time-connection->rtt)*NETWORK_RTT_SMOOTH;
            for(i=0;i<info->msgnum;i++){
                MessageInfo *msginfo=connection->msgbufferout+info->msg[i]%NETWORK_MESSAGE_BUFFER;
                msginfo->sequence=0xFFFFFFFF;
            }
        }
    }
}

static inline void ProcessPacket(TCReadFun tr,TCProcFun tp,Uint32 t){
    Uint16 i;
    Uint32 n;

    #ifndef SERVER
    if(memcmp(&packetin->address,&connection->ip,sizeof(IPaddress)))return;
    #endif // SERVER

    if(packetin->len&3)return;

    ResetBitPacker(packetin->data,packetin->len>>2);

    Uint32 sequence,ack,ackbits,msgnum,msgseq,len;
    if(!ReadBits(&sequence,16))return;
    if(!ReadBits(&ack,16))return;
    if(!ReadBits(&ackbits,32))return;

// READ TIME-CRITICAL DATA
    tr();

    if(!ReadBits(&msgnum,NETWORK_MESSAGE_NUMBER_BITS))return;

    #ifdef SERVER
    SaveBitPacker();
    Connection *connection=HashSearch(&connections,&packetin->address);
    if(!connection && connections.items.tot<NETWORK_MAX_CONNECTIONS){
        if(!msgnum)return;
        if(!CheckConnect())return;
        connection=ConnectionNew(&packetin->address,t);
        HashAdd(&connections,&connection->ip,connection);
#ifndef GRAPHICS
Uint32 host=SDLNet_Read32(&connection->ip.host);
Uint16 port=SDLNet_Read16(&connection->ip.port);
printf("Client connected: %d.%d.%d.%d:%d\n",host>>24,(host>>16)&255,(host>>8)&255,host&255,port);
#endif // GRAPHICS
    }
    if(!connection)return;
    LoadBitPacker();
    #endif // SERVER

    connection->last_receive_time=t;

    PacketInfo *info=connection->seqbufferin+sequence%NETWORK_SEQUENCE_BUFFER;
    if(info->sequence==sequence)return;

    int diff=SeqDiff(sequence,connection->ack);
    if(!diff)return;

// CHECK IF PACKET HAS ERRORS AND POSSIBLY DISCARD IT

//  PROCESS TIME-CRITICAL DATA
    if(diff>0)tp();

    info->sequence=sequence;
    info->len=packetin->len;
    memcpy(info->data,packetin->data,packetin->len);

for(i=0;i<msgnum;i++){

    // read sequence, read length, read message
    if(!ReadBits(&msgseq,16))return;
    if(!ReadBits(&len,8))return;
    if(++len>NETWORK_MESSAGE_SIZE)return;

    MessageInfo *msginfo=connection->msgbufferin+msgseq%NETWORK_MESSAGE_BUFFER;
    if(SeqDiff(msgseq,connection->msgread)>=0 && !msginfo->active){
        msginfo->sequence=msgseq;
        msginfo->active=SDL_TRUE;
        msginfo->time=t;
        msginfo->len=len;
        if(!ReadBytes(msginfo->data,len))return;
    }
    else if(!ReadBytes(NULL,len))return;
}

    if(diff>0){
        for(i=connection->ack+1;i!=sequence;i++)connection->seqbufferin[i%NETWORK_SEQUENCE_BUFFER].sequence=0xFFFFFFFF;
        connection->ack=sequence;
        connection->ackbits<<=1;
        connection->ackbits|=1;
        if(diff-1>=32)connection->ackbits=0;
        else connection->ackbits<<=diff-1;
    }
    else if(-diff-1<32)connection->ackbits|=1<<(-diff-1);

    ProcessAck(connection,ack,t);
    for(i=0,n=1;i<32;i++,n<<=1)if(ackbits&n)ProcessAck(connection,((Uint16)ack)-i-1,t);

    SDL_bool b;
    do{
        MessageInfo *msginfo=connection->msgbufferout+connection->msgack%NETWORK_MESSAGE_BUFFER;
        if((b=msginfo->sequence==0xFFFFFFFF && connection->msgqueue)){
            connection->msgack++;
            connection->msgqueue--;
        }
    }while(b);
}

static inline void BuildPacket(Connection *connection,TCWriteFun tw,Uint32 t){
    Uint16 i;

    PacketInfo *info=connection->seqbufferout+connection->sequence%NETWORK_SEQUENCE_BUFFER;
    info->sequence=connection->sequence;
    info->acked=SDL_FALSE;
    info->time=t;
    info->msgnum=0;

    ResetBitPacker(info->data,NETWORK_PACKET_DATA_SIZE);

    WriteBits(connection->sequence,16);
    WriteBits(connection->ack,16);
    WriteBits(connection->ackbits,32);

// WRITE TIME-CRITICAL DATA
    tw();

    int left=LeftBitPacker();

    for(i=0;i<connection->msgqueue && info->msgnum<(1<<NETWORK_MESSAGE_NUMBER_BITS)-1;i++){
        Uint16 sequence=connection->msgack+i;
        MessageInfo *msginfo=connection->msgbufferout+sequence%NETWORK_MESSAGE_BUFFER;
        if(msginfo->sequence==sequence && (!msginfo->active || t-msginfo->time>NETWORK_MESSAGE_RESEND)&& left>=(msginfo->len+3)<<3){ // +3 because of sequence (2) and len (1)
            msginfo->active=SDL_TRUE;
            msginfo->time=t;
            info->msg[info->msgnum++]=sequence;
            left-=(msginfo->len+3)<<3; // +3 because of sequence (2) and len (1)
        }
    }

    WriteBits(info->msgnum,NETWORK_MESSAGE_NUMBER_BITS);
    for(i=0;i<info->msgnum;i++){
        MessageInfo *msginfo=connection->msgbufferout+info->msg[i]%NETWORK_MESSAGE_BUFFER;
        WriteBits(msginfo->sequence,16);
        WriteBits(msginfo->len-1,8);
        WriteBytes(msginfo->data,msginfo->len);
    }

    FlushBitPacker();
    info->len=UsedBitPacker()>>3;

    connection->packlost+=((connection->seqbufferout[(connection->sequence-32)%NETWORK_SEQUENCE_BUFFER].acked?0.0f:1.0f)-connection->packlost)*NETWORK_PL_SMOOTH; // maybe increase rtt instead
    connection->sequence++;

// PUT THIS STUFF IN AN EXTERNAL FUNCTION
memcpy(&packetout->address,&connection->ip,sizeof(IPaddress));
packetout->len=info->len;
memcpy(packetout->data,info->data,packetout->len);
}

static inline void ControlCongestion(Connection *connection,Uint32 t){
    SDL_bool bad=(connection->rtt>NETWORK_RTT_THRESHOLD)||(connection->packlost>NETWORK_PL_THRESHOLD)||(t-connection->last_receive_time>NETWORK_LAST_PACKET_THRESHOLD); // use different parameters for good and bad mode
    if(bad)connection->good_congestion_time=t;
    if(connection->send_delay==NETWORK_LOW_DELAY){ // currently in good mode
        if(bad){
            connection->send_delay=NETWORK_HIGH_DELAY;
            if((t-connection->good_mode_time<NETWORK_GOOD_PUNISH)&&(connection->good_threshold<NETWORK_GOOD_THRESHOLD_MAX))connection->good_threshold<<=1;
        }
        else if((t-connection->good_congestion_time>NETWORK_GOOD_REWARD)&&(connection->good_threshold>NETWORK_GOOD_THRESHOLD_MIN)){
            connection->good_threshold>>=1;
            connection->good_congestion_time=t;
        }
    }
    else{ // currently in bad mode
        if(t-connection->good_congestion_time>connection->good_threshold){
            connection->send_delay=NETWORK_LOW_DELAY;
            connection->good_congestion_time=connection->good_mode_time=t;
            while(t-connection->last_send_time>=connection->send_delay)connection->last_send_time+=connection->send_delay;
        }
    }
}

void UpdateNetwork(TCReadFun tr,TCProcFun tp,TCWriteFun tw,MsgProcFun mp){
    int res;
    while((res=SDLNet_UDP_Recv(socket,packetin)))if(res==1)ProcessPacket(tr,tp,last_time);
    #ifdef SERVER
    int i;
    Connection *connection;
    for(i=0;i<connections.items.tot;i++){
        connection=connections.items.items[i];
        if(last_time-connection->last_receive_time>=NETWORK_DISCONNECTION_TIMEOUT){
#ifndef GRAPHICS
Uint32 host=SDLNet_Read32(&connection->ip.host);
Uint16 port=SDLNet_Read16(&connection->ip.port);
printf("Client disconnected: %d.%d.%d.%d:%d\n",host>>24,(host>>16)&255,(host>>8)&255,host&255,port);
#endif // GRAPHICS
            HashDelete(&connections,&connection->ip);
            ConnectionFree(connection);
            continue;
        }
    #else // SERVER
    if(last_time-connection->last_receive_time>=NETWORK_DISCONNECTION_TIMEOUT){
        ResetNetwork();
        return;
    }
    #endif // SERVER
        ControlCongestion(connection,last_time);
        while(last_time-connection->last_send_time>=connection->send_delay){
            connection->last_send_time+=connection->send_delay;
            BuildPacket(connection,tw,last_time);
#ifdef NETWORK_TEST
SendPacket(SDL_FALSE);
#else
        SDLNet_UDP_Send(socket,-1,packetout);
#endif // NETWORK_TEST
        }
    #ifdef SERVER
    }
    for(i=0;i<connections.items.tot;i++){
        connection=connections.items.items[i];
    #endif // SERVER
        while(ProcessMessage(connection,mp));
    #ifdef SERVER
    }
    #endif // SERVER
}

void DoneNetwork(){
    SDLNet_UDP_Close(socket);
    socket=NULL;
    SDLNet_FreePacket(packetin);
    SDLNet_FreePacket(packetout);
    packetin=packetout=NULL;
    #ifdef SERVER
    int i;
    for(i=0;i<connections.items.tot;i++)ConnectionFree(connections.items.items[i]);
    HashFree(&connections);
    #else // SERVER
    ConnectionFree(connection);
    #endif // SERVER
    SDLNet_Quit();
}
