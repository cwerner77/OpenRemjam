#include "QueueController.h"

QueueController::QueueController()
    : queue{NetworkJitterBufferPlayQueue(), NetworkJitterBufferPlayQueue(),
            NetworkJitterBufferPlayQueue(), NetworkJitterBufferPlayQueue(),
            NetworkJitterBufferPlayQueue(), NetworkJitterBufferPlayQueue(),
            NetworkJitterBufferPlayQueue(), NetworkJitterBufferPlayQueue(),
            NetworkJitterBufferPlayQueue(), NetworkJitterBufferPlayQueue(),
            NetworkJitterBufferPlayQueue(), NetworkJitterBufferPlayQueue(),
            NetworkJitterBufferPlayQueue(), NetworkJitterBufferPlayQueue(),
            NetworkJitterBufferPlayQueue(), NetworkJitterBufferPlayQueue()},
      mixer{AudioMixer4(), AudioMixer4(), AudioMixer4(), AudioMixer4(),
            AudioMixer4()},
      i2s_out{AudioOutputI2S()},
      con{
          AudioConnection(queue[0], 0, mixer[0], 0),
          AudioConnection(queue[1], 0, mixer[0], 1),
          AudioConnection(queue[2], 0, mixer[0], 2),
          AudioConnection(queue[3], 0, mixer[0], 3),
          AudioConnection(queue[4], 0, mixer[1], 0),
          AudioConnection(queue[5], 0, mixer[1], 1),
          AudioConnection(queue[6], 0, mixer[1], 2),
          AudioConnection(queue[7], 0, mixer[1], 3),
          AudioConnection(queue[8], 0, mixer[2], 0),
          AudioConnection(queue[9], 0, mixer[2], 1),
          AudioConnection(queue[10], 0, mixer[2], 2),
          AudioConnection(queue[11], 0, mixer[2], 3),
          AudioConnection(queue[12], 0, mixer[3], 0),
          AudioConnection(queue[13], 0, mixer[3], 1),
          AudioConnection(queue[14], 0, mixer[3], 2),
          AudioConnection(queue[15], 0, mixer[3], 3),
          AudioConnection(mixer[0], 0, mixer[4], 0),
          AudioConnection(mixer[1], 0, mixer[4], 1),
          AudioConnection(mixer[2], 0, mixer[4], 2),
          AudioConnection(mixer[3], 0, mixer[4], 3),
          AudioConnection(mixer[4], 0, i2s_out, 0),
          AudioConnection(mixer[4], 0, i2s_out, 1),
      },
      gain{1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0,
           1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0},
      autoconnect{true}, autodisconnect{true} {}




int QueueController::getQueueIndexByIP(IPAddress ip, uint16_t port) {
    for (int i = 0; i < 16; ++i) {
        if (!queue[i].hasIP6() && queue[i].getIPv4() == ip &&
            queue[i].getPort() == port)
            return i;
    }
    return -1;
}

int QueueController::getQueueIndexByIPv4(fnet_ip4_addr_t ip, uint16_t port) {
    for (int i = 0; i < 16; ++i) {
        if (!queue[i].hasIP6() && queue[i].getIPv4() == ip &&
            queue[i].getPort() == port)
            return i;
    }
    return -1;
}

int QueueController::getQueueIndexByIPv6(fnet_ip6_addr_t &ip6, uint16_t port) {
    for (int i = 0; i < 16; ++i) {
        if ((queue[i].hasIP6()) && (queue[i].getPort() == port) &&
            (FNET_IP6_ADDR_EQUAL(queue[i].getIP6Ptr(), &ip6)))
            return i;
    }
    return -1;
}

int QueueController::getFreeAutoconnectQueueIndex() {
    if (!getAutoconnect()) return -1;
    return getFreeQueueIndex();
}

int QueueController::getFreeQueueIndex() {
    // i == 0 is reserved for localhost/loopback 
    for (int i = 1; i < 16; ++i) {
        if (queue[i].getPort() == 0)
            return i;
    }
    return -1;
}

NetworkJitterBufferPlayQueue *QueueController::getQueue(int j) {
    if (j >= 0 && j < 16)
        return &queue[j];
    Serial.println("Warning: getQueue returning nullptr!");
    return nullptr;
}

float QueueController::getGain(int i) { return gain[i]; }

void QueueController::setGain(int i, float f) {
    gain[i] = f;
    mixer[i / 4].gain(i % 4, f);
}

void QueueController::setAutoconnect(boolean val) {
    autoconnect = val;
}

boolean QueueController::getAutoconnect() { return autoconnect; }

void QueueController::setAutodisconnect(boolean val) {
    autodisconnect = val;
}

boolean QueueController::getAutodisconnect() {
    return autodisconnect;
}

void QueueController::printInfo(int i) {
    if (i >= 0 && i < 16) {
        Serial.printf("#%2i: %39s:%5i - gain: %3f, max_buffers: %2i, prefill: %2i\r\n",
              i,
              fnet_inet_ntop(getQueue(i)->getSockaddrPtr()->sa_family, &getQueue(i)->getSockaddrPtr()->sa_data, ipv6_print_buffer, sizeof(ipv6_print_buffer)),
              getQueue(i)->getPort(),
              getGain(i),
              getQueue(i)->getMaxBuffers(),
              getQueue(i)->getPrefill());
    }
}
