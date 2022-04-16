// manages all input queues as well as our audio output via i2s

#pragma once

#include "Audio.h"
#include "NativeEthernet.h"

#include "NetworkJitterBufferPlayQueue.h"

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
    QueueController();
    int getQueueIndexByIP(IPAddress ip, uint16_t port);
    int getQueueIndexByIPv4(fnet_ip4_addr_t ip, uint16_t port);
    int getQueueIndexByIPv6(fnet_ip6_addr_t &ip6, uint16_t port);
    int getFreeAutoconnectQueueIndex();
    int getFreeQueueIndex();
    NetworkJitterBufferPlayQueue *getQueue(int i);
    void setGain(int i, float f);
    float getGain(int i);
    boolean getAutoconnect();
    void setAutoconnect(boolean val);
    boolean getAutodisconnect();
    void setAutodisconnect(boolean val);
    void printInfo(int i);
};
