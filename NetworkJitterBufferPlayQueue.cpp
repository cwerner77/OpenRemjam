#include "NetworkJitterBufferPlayQueue.h"

NetworkJitterBufferPlayQueue::NetworkJitterBufferPlayQueue()
    : AudioStream(0, NULL), state(State::stopped), sa{AF_INET, 0, 0, {}}, queue{},  max_buffers{7},
      prefill(3), free_head(0), used_tail(0), subindex(0), count(0), late_packets(),
      early_packets(0), recoveries_success(0), recoveries_failed(0), recoveryStart(0) {}

void NetworkJitterBufferPlayQueue::setIPv4(fnet_ip4_addr_t a) {
    fnet_sockaddr_in* sa_ptr = (fnet_sockaddr_in*) &sa; // re-use this struct for IPv4 (it's comaptible!)   
    sa_ptr->sin_family = AF_INET; // this is an IPv4 address
    sa_ptr->sin_addr.s_addr = a;
}

fnet_ip4_addr_t NetworkJitterBufferPlayQueue::getIPv4() {
    if (sa.sa_family==AF_INET6)
        return 0;
    fnet_sockaddr_in* sa_ptr = (fnet_sockaddr_in*) &sa; // re-use this struct for IPv4 (it's comaptible!)      
    return sa_ptr->sin_addr.s_addr;
}

void NetworkJitterBufferPlayQueue::setIP(IPAddress a) {
    setIPv4(a);
}

IPAddress NetworkJitterBufferPlayQueue::getIP() {
    fnet_ip4_addr_t a = getIPv4();
    return IPAddress(a);
}

void NetworkJitterBufferPlayQueue::setSockaddr(struct fnet_sockaddr &a) {
    sa = a;
    if (!getPort()) {
        switchState(State::stopped);
    } else {
        switchState(State::syncing);
    }
}

struct fnet_sockaddr *NetworkJitterBufferPlayQueue::getSockaddrPtr() {
    return &sa;
}

fnet_ip6_addr_t *NetworkJitterBufferPlayQueue::getIP6Ptr() {
    fnet_sockaddr_in6* sa_ptr = (fnet_sockaddr_in6*) &sa; // re-use this struct for IPv6 (it's comaptible!)
    return &(sa_ptr->sin6_addr.s6_addr);
}

fnet_ip4_addr_t *NetworkJitterBufferPlayQueue::getIP4Ptr() {
    fnet_sockaddr_in* sa_ptr = (fnet_sockaddr_in*) &sa; // re-use this struct for IPv6 (it's comaptible!)
    return &(sa_ptr->sin_addr.s_addr);
}

boolean NetworkJitterBufferPlayQueue::hasIP6() { return sa.sa_family==AF_INET6;}

void NetworkJitterBufferPlayQueue::setPort(uint16_t p) {
    sa.sa_port = fnet_htons(p);
    if (p) {
        switchState(State::syncing);
    } else {
        switchState(State::stopped);
    }
}

uint16_t NetworkJitterBufferPlayQueue::getPort() {
  return fnet_ntohs(sa.sa_port);
}

uint32_t NetworkJitterBufferPlayQueue::getCount() { return count; }

uint32_t NetworkJitterBufferPlayQueue::getQueueLength() {
    // example: free_head 0...5
    //          used_tail 0...5
    //          size      6
    // used_tail=2, free_head=2 -> 0 (empty, zeroed blocks will be played)
    // used_tail=2, free_head=3  -> 1 (one network buffer is being played)
    // used_tail=5, free_head=2  -> 3 (2 + 6 - 5)
    if (free_head >= used_tail)
        return (free_head - used_tail);
    else
        return (free_head + max_buffers - used_tail);
}

void NetworkJitterBufferPlayQueue::printStatistics() {
    Serial.printf("Remote host:             %s\r\n", fnet_inet_ntop(sa.sa_family, &sa.sa_data, ipv6_print_buffer, sizeof(ipv6_print_buffer)));
    Serial.printf("Port:                    %d\r\n", getPort());
    Serial.printf("Queue length (subindex): %lu (%lu)\r\n", getQueueLength(),subindex);
    Serial.printf("Early/late packets:      %lu / %lu\r\n", early_packets, late_packets);
    Serial.printf("Recoveries succ/fail:    %lu / %lu\r\n", recoveries_success, recoveries_failed);
    Serial.printf("Mem:                     %d\r\n", AudioMemoryUsage());
    Serial.printf("===============================\r\n");
}

void NetworkJitterBufferPlayQueue::resetStatistics() {
    count = 0;
    subindex = 0;
    late_packets = 0;
    early_packets = 0;
    recoveries_success = 0;
    recoveries_failed = 0;
}

void NetworkJitterBufferPlayQueue::setMaxBuffers(uint8_t val) {
    if (val <= OPENREMJAM_PLAY_QUEUE_MAX_LENGTH && val >= 2) {
        setPrefill(val / 2); // adjust prefill as well!
        max_buffers = val;
    } else {
        Serial.printf("setMaxBuffers: invalid value!");
    }
}

uint8_t NetworkJitterBufferPlayQueue::getMaxBuffers() { return max_buffers; }

void NetworkJitterBufferPlayQueue::setPrefill(uint8_t val) { prefill = val; }

uint8_t NetworkJitterBufferPlayQueue::getPrefill() { return prefill; }


/***** HELPER ****/

uint32_t NetworkJitterBufferPlayQueue::nextIndex(uint32_t index) {
    return (index + 1) % max_buffers;
}

uint32_t NetworkJitterBufferPlayQueue::prevIndex(uint32_t index) {
    return ((index==0) ? (max_buffers - 1) : (index - 1));
}

uint32_t NetworkJitterBufferPlayQueue::nthIndexAfter(uint32_t index, uint32_t n) {
    return (index + n) % max_buffers;
}

void NetworkJitterBufferPlayQueue::placePacketIntoIndex(network_block_t * packet, uint32_t index) {
    if (!packet) {
        // generate empty packet with consecutive seqno and current timestamp:
        memset(&queue[index], 0, OPENREMJAM_MONO_PACKET_SIZE-4);
        queue[index].seqno=queue[prevIndex(index)].seqno + 1;
        queue[index].timestamp=micros();
        Serial.printf("Index: %d, Generated seqno: %d, currently playing seqno: %d\r\n", index, queue[index].seqno, queue[used_tail].seqno);
    } else {
        memcpy(&queue[index], packet, OPENREMJAM_MONO_PACKET_SIZE + 4); // +4 because of timestamp!
    }
}

bool NetworkJitterBufferPlayQueue::checkPacketContinuityWithPrevious(uint32_t index) {
    if (queue[prevIndex(index)].seqno + 1 != queue[index].seqno) {
        Serial.printf("prev seqno: %d, my seqno: %d\r\n", queue[prevIndex(index)].seqno, queue[index].seqno);
        return false;
    }
    
    // previous timestamp should be at least ~1 ms smaller than current (@128 samples per packet)
    if (queue[prevIndex(index)].timestamp + OPENREMJAM_MONO_PACKET_DURATION_US/3 > queue[index].timestamp) {
        Serial.printf("prev timestamp: %d, my timestamp: %d\r\n", queue[prevIndex(index)].timestamp, queue[index].timestamp);
        return false;
    }
    
    // previous timestamp should be no more ~4 ms smaller than current (@128 samples per packet)
    if (queue[prevIndex(index)].timestamp + OPENREMJAM_MONO_PACKET_DURATION_US*4/3 < queue[index].timestamp) {
        Serial.printf("prev timestamp: %d, my timestamp: %d\r\n", queue[prevIndex(index)].timestamp, queue[index].timestamp);
        return false;
    }
    return true;
}

void NetworkJitterBufferPlayQueue::switchState(State s) {
    switch(state) {
        case (State::stopped):
            if (s==State::syncing) {
                state=s;
                Serial.println("switchState() -- new state: syncing");
            } else {
                Serial.println("WARNING: switchState() -- invalid transition from state stopped!");
            }
            break;
        case (State::syncing):
            if (s==State::stopped) {
                state=s;
                resetStatistics();
                Serial.println("switchState() -- new state: stopped");
            } else if (s==State::playing) {
                state=s;
                Serial.println("switchState() -- new state: playing");
            } else {
                Serial.println("WARNING: switchState() -- invalid transition from state syncing!");
            }
            break;
        case (State::playing):
            if (s==State::stopped) {
                state=s;
                resetStatistics();
                Serial.println("switchState() -- new state: stopped");
            } else if (s==State::recovering) {
                state=s;
                recoveryStart = millis();
                Serial.println("switchState() -- new state: recovering");
            } else {
                Serial.println("WARNING: switchState() -- invalid transition from state playing!");
            }
            break;
        case (State::recovering):
            if (s==State::stopped) {
                state=s;
                resetStatistics();
                Serial.println("switchState() -- new state: stopped");
            } else if (s==State::syncing) {
                state=s;
                recoveries_failed++;
                Serial.println("switchState() -- new state: syncing");
            } else if (s==State::playing) {
                state=s;
                recoveries_success++;
                Serial.println("switchState() -- new state: playing");
            } else {
                Serial.println("WARNING: switchState() -- invalid transition from state playing!");
            }
            break;
        default:
            Serial.println("WARNING: switchState() -- invalid state!");
            break;
    }
}

bool NetworkJitterBufferPlayQueue::recoveryTimeout() {
    if (millis() - recoveryStart > 1000) return true;
    return false;
}

/**** HELPER END ***/

void NetworkJitterBufferPlayQueue::enqueue(uint8_t * buffer) {
    network_block_t* packet = (network_block_t*) buffer; // IMPORTANT: buffer must be large enough to add timestamp!
    packet->timestamp = micros();
    //Serial.printf("enqued - seqno: %d\r\n",packet->seqno);

    uint32_t seqno_delta = 0;

    switch (state) {
        case State::stopped:
            break; // don't do anything
        case State::syncing:
            //Serial.println("Sync");
            //Playback is stopped.
            //Queue must be filled with exactly n==prefill packets.
            //They must have consecutive sequence numbers and sound time stamps (no burst arrival, no reordering)
            
            placePacketIntoIndex(packet, free_head);

            if (free_head != used_tail) { // do we have other packets already?
                // check if this packets has consecutive seqnos and sound timestaps with respect to the previous
                if (checkPacketContinuityWithPrevious(free_head)) {
                    //Serial.println("Check passed");
                    if (getQueueLength() == prefill) switchState(State::playing);
                } else {
                    //Serial.println("New start");
                    // use this packet as a new starting for syncing
                    used_tail = free_head;
                }
            }
            free_head = nextIndex(free_head);
            break;
        case State::recovering: // fall trough!
            //Serial.println("Recovering");
        case State::playing:
            //Serial.println("Playing");
            //Serial.printf("used_tail has seqno: %d (%d)\r\n", queue[used_tail].seqno, queue[used_tail].timestamp);
            //Serial.printf("new packet has seqno: %d (%d)\r\n",packet->seqno, packet->timestamp);
            seqno_delta = packet->seqno - queue[used_tail].seqno;
            if (seqno_delta < 1) {
                //Serial.printf("Late packet -- max_buffers: %d, used_tail has index: %d (seqno: %d), free_head has index: %d, queue length: %d, seqno: %d, seqno_delta: %d\r\n", max_buffers, used_tail, queue[used_tail].seqno, free_head, getQueueLength(), packet->seqno, seqno_delta);
                late_packets++;
            } else if (seqno_delta > max_buffers - 1) {
                //Serial.printf("Early packet -- max_buffers: %d, used_tail has index: %d (seqno: %d), free_head has index: %d, queue length: %d, seqno: %d, seqno_delta: %d\r\n", max_buffers, used_tail, queue[used_tail].seqno, free_head, getQueueLength(), packet->seqno, seqno_delta);
                early_packets++;
            } else {
                // create zero-padded packets if neccessary
                if (seqno_delta > getQueueLength()) {
                    // how many zero-padded packets do we need?
                    int bogus_cnt = seqno_delta - getQueueLength();
                    //Serial.printf("Creating %i bogus packet(s)\r\n", bogus_cnt);
                    for (int i=0 ; i<bogus_cnt; ++i) {
                        placePacketIntoIndex(nullptr, free_head);
                        free_head = nextIndex(free_head);
                    }
                    // place the packet:
                    placePacketIntoIndex(packet, free_head);
                    free_head = nextIndex(free_head);
                } else if (seqno_delta == getQueueLength()) {
                    placePacketIntoIndex(packet, free_head);
                    free_head = nextIndex(free_head);
                } else {
                    // late arriving packet, free head has been advanced already
                    placePacketIntoIndex(packet, nthIndexAfter(used_tail, seqno_delta));
                }
                
                if (state==State::recovering && getQueueLength() == prefill) switchState(State::playing);
            }
            break;
        default:
            Serial.println("WARNING: enqueue() -- invalid state!");
            break;
    }
}

void NetworkJitterBufferPlayQueue::dequeue() {
    used_tail = (used_tail + 1) % max_buffers;
    //Serial.printf("used_tail: %i\r\n", used_tail);
}

void NetworkJitterBufferPlayQueue::update(void) {
    audio_block_t* block;

    switch (state) {
        case State::stopped:
        case State::syncing:
            return; // no audio playback!
            break;
        case State::playing:
            block = allocate();
            if (!block) {
                Serial.println("Error: update() -- could not allocate audio block!");
                return;
            }
            memcpy(block->data,&queue[used_tail].samples[subindex*AUDIO_BLOCK_SAMPLES], AUDIO_BLOCK_SAMPLES*sizeof(int16_t));
            subindex = (subindex + 1) % OPENREMJAM_AUDIO_BLOCKS_PER_NETWORK_BLOCK;
            if (!subindex) {
                if (used_tail!=free_head) {
                    dequeue();
                } else {
                    switchState(State::recovering);
                }
            }
            transmit(block);
            release(block);
            break;
        case State::recovering:
            block = allocate();
            if (!block) {
                Serial.println("Error: update() -- could not allocate audio block!");
                return;
            }
            memset(block->data, 0, AUDIO_BLOCK_SAMPLES*sizeof(int16_t));
            subindex = (subindex + 1) % OPENREMJAM_AUDIO_BLOCKS_PER_NETWORK_BLOCK;
            if (!subindex) {
                if (recoveryTimeout()) {
                    switchState(State::syncing);
                } else {
                    queue[used_tail].seqno++; // just increment seqno for recovery
                }
            }
            transmit(block);
            release(block);
            break;
        default:
            Serial.println("WARNING: update() -- invalid state!");
            break;
    }
    
    if (++count % 10000 == 0) {
        printStatistics();
    }
}
