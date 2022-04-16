#pragma once

#include "Audio.h"
#include "NativeEthernet.h"
#include "NetworkJitterBufferPlayQueue.h"

/**
 * @brief Manages all input queues as well as our audio output via i2s
 * 
 */
class QueueController {
  private:
    NetworkJitterBufferPlayQueue queue[16];
    AudioMixer4 mixer[5]; // we need 5 mixers and 22 connections to connect 16 queues
    AudioOutputI2S i2s_out;
    AudioConnection con[22];
    float gain[16]; // gain setting for each input;
    boolean autoconnect;
    boolean autodisconnect;
    fnet_char_t ipv6_print_buffer[FNET_IP6_ADDR_STR_SIZE];
  public:
    /**
     * @brief Construct a new QueueController object
     * 
     */
    QueueController();

    /**
     * @brief Get the queue index by Arduino-style IP address
     * 
     * @param ip IP address
     * @param port port number 
     * @return int queue index [0..15]
     */
    int getQueueIndexByIP(IPAddress ip, uint16_t port);

    /**
     * @brief Get the queue index by IPv4 address
     * 
     * @param ip IP address
     * @param port port number
     * @return int queue index [0..15]
     */
    int getQueueIndexByIPv4(fnet_ip4_addr_t ip, uint16_t port);

    /**
     * @brief Get the queue index by IPv6 address
     * 
     * @param ip6 
     * @param port 
     * @return int [0..15]
     */
    int getQueueIndexByIPv6(fnet_ip6_addr_t &ip6, uint16_t port);

    /**
     * @brief Get the index of an unused autoconnect queue 
     * 
     * @return int index [0..15]
     */
    int getFreeAutoconnectQueueIndex();

    /**
     * @brief Get the index of and unused queue
     * 
     * @return int index [0..15]
     */
    int getFreeQueueIndex();

    /**
     * @brief Get a pointer to a queue instance
     * 
     * @param i queue index [0..15]
     * @return NetworkJitterBufferPlayQueue* queue pointer
     */
    NetworkJitterBufferPlayQueue *getQueue(int i);

    /**
     * @brief Set the gain of a queue
     * 
     * @param i index of queue [0..15]
     * @param f gain -32767.0...32767.0 
     */
    void setGain(int i, float f);

    /**
     * @brief Get the current gain
     * 
     * @param i index of queue [0..15]
     * @return float gain -32767.0...32767.0 
     */
    float getGain(int i);
    
    /**
     * @brief Is autoconnect anabled?
     * 
     * @return boolean 
     */
    boolean getAutoconnect();

    /**
     * @brief Set autoconnect on (true) or off (false)
     * 
     * @param val
     */
    void setAutoconnect(boolean val);

    /**
     * @brief Is autodisconnect enabled?
     * 
     * @return boolean 
     */
    boolean getAutodisconnect();

    /**
     * @brief Set the autodisconnect on (true) or off (false)
     * 
     * @param val 
     */
    void setAutodisconnect(boolean val);

    /**
     * @brief Print status information of a queue
     * 
     * @param i index of queue [0..15]
     */
    void printInfo(int i);
};
