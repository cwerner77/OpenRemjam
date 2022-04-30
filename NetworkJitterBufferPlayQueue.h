#pragma once

#define OPENREMJAM_AUDIO_BLOCKS_PER_NETWORK_BLOCK (8)     // default: 8 (good choice if AUDIO_BLOCK_SAMPLES has value 16)
#define OPENREMJAM_PLAY_QUEUE_SIZE (10)                   // default: 10
#define OPENREMJAM_DEFAULT_UDP_PORT (9000)                // default: 9000

// DO NOT CHANGE THESE:
#define OPENREMJAM_PLAY_QUEUE_MAX_LENGTH (OPENREMJAM_PLAY_QUEUE_SIZE - 1)
#define OPENREMJAM_MONO_PACKET_SIZE (AUDIO_BLOCK_SAMPLES * OPENREMJAM_AUDIO_BLOCKS_PER_NETWORK_BLOCK * 2 + 4)
#define OPENREMJAM_MONO_PACKET_DURATION_US (AUDIO_BLOCK_SAMPLES * OPENREMJAM_AUDIO_BLOCKS_PER_NETWORK_BLOCK * 1000000 / 44100)

#include "NativeEthernet.h"
#include "Audio.h"
#include "fnet.h"


/**
 * @brief Packet on the network. Consists of samples of one or more audio blocks
 *
 */
typedef struct network_block_struct {
  int16_t samples[AUDIO_BLOCK_SAMPLES * OPENREMJAM_AUDIO_BLOCKS_PER_NETWORK_BLOCK];
  uint32_t seqno;
  uint32_t timestamp;
} network_block_t;


/**
 * @brief Jitter buffer queue. Receives audio samples from the network and plays them out continuously, mitigating network jitter.
 * 
 */
class NetworkJitterBufferPlayQueue : public AudioStream {
  public:

    /**
     * @brief Construct a new NetworkJitterBufferPlayQueue object
     * 
     */
    NetworkJitterBufferPlayQueue(void);
    
    /**
     * @brief Set the remote IPv4 address
     * 
     * @param a address
     */
    void setIPv4(fnet_ip4_addr_t a);

    /**
     * @brief Get the remote IPv4 address
     * 
     * @return fnet_ip4_addr_t 
     */
    fnet_ip4_addr_t getIPv4();

    /**
     * @brief Set the remote IPv4 address (arduino style)
     * 
     * @param a address
     */
    void setIP(IPAddress a);

    /**
     * @brief Get the remote IPv4 address (arduino style)
     * 
     * @return IPAddress  address
     */
    IPAddress getIP();

    /**
     * @brief Set the remote sockaddr of this queue
     * 
     * @param val sockaddr
     */
    void setSockaddr(struct fnet_sockaddr &val);


    /**
     * @brief Get a pointer to the remote sockaddr of this queue
     * 
     * @return struct fnet_sockaddr* pointer
     */
    struct fnet_sockaddr *getSockaddrPtr();

    /**
     * @brief Get a pointer to the remote IPv6 address of this queue
     * 
     * @return fnet_ip6_addr_t* pointer
     */
    fnet_ip6_addr_t *getIP6Ptr();

    /**
     * @brief Get a pointer to the remote IPv4 address of this queue
     * 
     * @return fnet_ip4_addr_t* pointer
     */
    fnet_ip4_addr_t *getIP4Ptr();

    /**
     * @brief Is the remote address of this queue IPv6?
     * 
     * @return boolean 
     */
    boolean hasIP6();

    /**
     * @brief Set the remote port number. A non-zero value sets this queue active.
     * 
     * @param port port number
     */
    void setPort(uint16_t port);

    /**
     * @brief Get the remote port number of this queue. A non-zero value indicates that this queue is active.
     * 
     * @return uint16_t 
     */
    uint16_t getPort();

    /**
     * @brief Enqueue one new network packet into this queue
     * 
     * @param buffer source buffer
     */
    void enqueue(uint8_t* buffer);

    /**
     * @brief Dequeue the oldest network packet from this queue
     * 
     */
    void dequeue(void);

    /**
     * @brief Get the current sequence number
     * 
     * @return uint32_t sequence number
     */
    uint32_t getSeqno(void);

    /**
     * @brief Get the count of played audio blocks
     * 
     * @return uint32_t count
     */
    uint32_t getCount(void);

    /**
     * @brief Get the current queue length
     * 
     * @return int32_t length
     */
    int32_t getQueueLength();

    /**
     * @brief Print statistic information
     * 
     */
    void printStatistics();

    /**
     * @brief Resets statistics 
     * 
     */
    void resetStatistics();

    /**
     * @brief Set the maximum count of network blocks in this queue
     * 
     * @param val count
     */
    void setMaxBuffers(uint8_t val);

    /**
     * @brief Get the maximum count of network blocks in this queue
     * 
     * @return uint8_t count
     */
    uint8_t getMaxBuffers();

    /**
     * @brief Set the number of network blocks to be queued, before playback starts
     * 
     * @param val number
     */
    void setPrefill(uint8_t val);

    /**
     * @brief Get the prefill block count
     * 
     * @return uint8_t count
     */
    uint8_t getPrefill();

    /**
     * @brief This is the update function of this auto output stream (plays one audio block)
     * 
     */
    virtual void update(void);

  private:

    /**
     * @brief A NetworkJitterBufferQueue is in state stopped, syncing, playing, or recovering
     * 
     */
    enum class State {
      stopped,
      syncing,
      playing,
      recovering
    };
    // we don't need volatile here, because update() is called by software interrupt, i.e. it cannot interrupt our code!
    State state;

    fnet_sockaddr sa; // port #, IP version, IPv4 or IPv6 address -- this struct has it all :-)
    network_block_t queue[OPENREMJAM_PLAY_QUEUE_MAX_LENGTH];
    int32_t max_buffers;          // used number of elements of the queue
    int32_t prefill;              // fill prefill blocks before start playing or after underrun

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
