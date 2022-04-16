#define OPENREMJAM_AUDIO_BLOCKS_PER_NETWORK_BLOCK (8)     // default: 8 (if AUDIO_BLOCK_SAMPLES has value 16)
#define OPENREMJAM_PLAY_QUEUE_SIZE (10)                   // default: 10
#define OPENREMJAM_DEFAULT_UDP_PORT (9000)                // default: 9000

/* DO NOT EDIT BELOW THIS LINE */

#pragma once

#include "NativeEthernet.h"
#include "Audio.h"
#include "fnet.h"

#define OPENREMJAM_PLAY_QUEUE_MAX_LENGTH (OPENREMJAM_PLAY_QUEUE_SIZE - 1)
#define OPENREMJAM_MONO_PACKET_SIZE (AUDIO_BLOCK_SAMPLES * OPENREMJAM_AUDIO_BLOCKS_PER_NETWORK_BLOCK * 2 + 4)
#define OPENREMJAM_MONO_PACKET_DURATION_US (AUDIO_BLOCK_SAMPLES * OPENREMJAM_AUDIO_BLOCKS_PER_NETWORK_BLOCK * 1000000 / 44100)

typedef struct network_block_struct {
  int16_t samples[AUDIO_BLOCK_SAMPLES * OPENREMJAM_AUDIO_BLOCKS_PER_NETWORK_BLOCK];
  uint32_t seqno;
  uint32_t timestamp;
} network_block_t;

class NetworkJitterBufferPlayQueue : public AudioStream {
  public:
    NetworkJitterBufferPlayQueue(void);
    
    void setIPv4(fnet_ip4_addr_t a);
    fnet_ip4_addr_t getIPv4();
    void setIP(IPAddress);
    IPAddress getIP();
    void setSockaddr(struct fnet_sockaddr &val);
    struct fnet_sockaddr *getSockaddrPtr();
    fnet_ip6_addr_t *getIP6Ptr();
    fnet_ip4_addr_t *getIP4Ptr();
    boolean hasIP6();
    void setPort(uint16_t port);
    uint16_t getPort();

    void enqueue(uint8_t* buffer);   // copy packet into queue, if possible
    void dequeue(void);              // increment used_tail
    uint32_t getSeqno(void);
    uint32_t getCount(void);
    uint32_t getQueueLength();
    void printStatistics();
    void resetStatistics();

    void setMaxBuffers(uint8_t val);
    uint8_t getMaxBuffers();
    void setPrefill(uint8_t val);
    uint8_t getPrefill();

    //void playBuffer(void);
    // void stop(void);
    // bool isPlaying(void) { return playing; }
    virtual void update(void);

  private:
    // we don't need volatile here, because update() is called by software interrupt, i.e. it cannot interrupt our code!

    enum class State {
      stopped,
      syncing,
      playing,
      recovering
    };
    State state;

    fnet_sockaddr sa; // port #, IP version, IPv4 or IPv6 address -- this struct has it all :-)
    network_block_t queue[OPENREMJAM_PLAY_QUEUE_MAX_LENGTH];
    uint32_t max_buffers;         // used number of elements of the queue
    uint32_t prefill;             // fill prefill blocks before start playing or after underrun

    uint32_t free_head;           // this index points to the first free element that can be filled with new data
    uint32_t used_tail;           // this index currently used for playing
    uint32_t subindex;            // [0...UNISON_AUDIO_BLOCKS_PER_NETWORK_BLOCK-1], point to the portion of data to be played next

                                  // statistics:
    uint32_t count;               // count played audio blocks
    uint32_t late_packets;        // increment, if an incoming packet has a too small seqno to be enqueued
    uint32_t early_packets;       // increment, if an incoming packet has a too big seqno to be enqueued
    uint32_t recoveries_success;  // increment, if sync recovery has been successful
    uint32_t recoveries_failed;   // increment, if sync recovery has failed (after timeout)
    
    uint32_t recoveryStart;       // timestamp of entering state recovery in millis

    fnet_char_t ipv6_print_buffer[FNET_IP6_ADDR_STR_SIZE];

    // helper functions:
    void placePacketIntoIndex(network_block_t * packet, uint32_t index); // just put the packet into index, if packet==NULL: generate empty packet
    void placePacketIntoFreeHead(network_block_t * packet); // places a packet at queue[free_head] and advance FreeHead;
    uint32_t nextIndex(uint32_t index);
    uint32_t prevIndex(uint32_t index);
    uint32_t nthIndexAfter(uint32_t index, uint32_t n);
    bool checkPacketContinuityWithPrevious(uint32_t index);
    void switchState(State s);
    bool recoveryTimeout();
};
