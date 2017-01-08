#define SERVER_IP "88.198.124.71"
#define SERVER_PORT 25612

#define NETWORK_MAX_CONNECTIONS 16
#define NETWORK_EXPECTED_CONNECTIONS 128 // hard-coded into hash map
#define NETWORK_SEQUENCE_BUFFER 1024
#define NETWORK_MESSAGE_BUFFER 1024 // must be the same on client and server

#define NETWORK_DISCONNECTION_TIMEOUT 10000 //3000
#define NETWORK_RECONNECTION_TIME 1000

#define NETWORK_RTT_SMOOTH 0.1f
#define NETWORK_PL_SMOOTH 0.025f
#define NETWORK_RTT_THRESHOLD 250.0f // milliseconds
#define NETWORK_PL_THRESHOLD 0.3f // percentage
#define NETWORK_LAST_PACKET_THRESHOLD 500 // milliseconds
#define NETWORK_GOOD_THRESHOLD_MIN 1000 // milliseconds
#define NETWORK_GOOD_THRESHOLD_MAX (NETWORK_GOOD_THRESHOLD_MIN*64)
#define NETWORK_GOOD_REWARD 10000 // milliseconds
#define NETWORK_GOOD_PUNISH 10000 // milliseconds
#define NETWORK_LOW_DELAY 30
#define NETWORK_HIGH_DELAY 100
#define NETWORK_MESSAGE_RESEND 100 // milliseconds

#define NETWORK_PACKET_DATA_SIZE 300 // in 32-bit words; must be at most 300
#define NETWORK_MESSAGE_SIZE 256 // in bytes; must be at most 256
#define NETWORK_MESSAGE_NUMBER_BITS 6 // max number of messages in a packet is 2^this

typedef struct{
    Uint32 data[NETWORK_PACKET_DATA_SIZE];
    Uint32 sequence; // need 32 bits because 0xFFFFFFFF represents empty entry
    SDL_bool acked;
    Uint32 time;
    Uint16 len;
    Uint32 msgnum; // number of messages contained in packet
    Uint16 msg[1<<NETWORK_MESSAGE_NUMBER_BITS]; // sequence numbers of messages contained in packet, in increasing order
}PacketInfo;

typedef struct{
    Uint8 data[NETWORK_MESSAGE_SIZE];
    Uint32 sequence;
    SDL_bool active; // message sent or received
    Uint32 time; // last time this message was sent or time it was received
    Uint16 len;
}MessageInfo;

typedef struct{
    IPaddress ip;
    Uint16 sequence;
    Uint16 ack; // ack sent to client
    Uint32 ackbits; // redundant acks sent to client
    PacketInfo seqbufferin[NETWORK_SEQUENCE_BUFFER];
    PacketInfo seqbufferout[NETWORK_SEQUENCE_BUFFER];

    Uint32 send_delay; // time between packets sent
    Uint32 last_receive_time;
    Uint32 last_send_time;
    Uint32 good_congestion_time; // last time congestion has been high
    Uint32 good_mode_time; // last time good mode was entered
    Uint32 good_threshold; // time congestion has to be low before switching from bad to good mode
    float rtt; // round-trip time estimate
    float packlost; // percentage of packets sent and lost

    Uint16 msgseq; // next message to send
    Uint16 msgack; // oldest unacked message sent
    Uint16 msgqueue; // send queue size
    Uint16 msgread; // sequence number of next received message to read
    MessageInfo msgbufferin[NETWORK_MESSAGE_BUFFER];
    MessageInfo msgbufferout[NETWORK_MESSAGE_BUFFER];
}Connection;

#ifdef SERVER
Hash connections;
#else // SERVER
Connection *connection;
#endif // SERVER

typedef void(*TCReadFun)();
typedef void(*TCProcFun)();
typedef void(*TCWriteFun)();
typedef void(*MsgProcFun)(Connection*,void*,Uint32);

void InitNetwork();
void UpdateNetwork(TCReadFun tr,TCProcFun tp,TCWriteFun tw,MsgProcFun mp);
void DoneNetwork();

SDL_bool EnqueueMessage(Connection *connection,void *data,Uint32 len);
