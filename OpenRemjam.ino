/**
 * OpenRemjam -- ultra-low latency audio streaming solution for Teensy 4.1 
 * 
 * License: MIT
 * 
 * Hardware requirements:
 * - Teensy 4.1
 * - Teensy Audio shield for Teensy 4.x (rev D) (https://www.pjrc.com/store/teensy3_audio.html)
 * - Ethernet Kit for Teensy 4.1 (https://www.pjrc.com/store/ethernet_kit.html)
 * 
 * Software requirements:
 * - Arduino 1.8.19 or newer
 * - Teensyduino 1.56 or newer with installed libraries:
 *   + Audio
 *   + FNET 
 *   + NativeEthernet
 *   + NativeEthernetUdp
 *   + SerialFlash
 *   
 * !!!!!!!!!!!!!! IMPORTANT !!!!!!!!!!!!!!!!!!!!!!!
 * Before you start:
 * - Enable loopback interface: append line "#define FNET_CFG_LOOPBACK (1)" to C:\Program Files (x86)\Arduino\hardware\teensy\avr\libraries\FNET\src\fnet_user_config.h
 * - Patch NetiveEthernet.c: Append line "fnet_netif_set_default(fnet_netif_get_by_name("eth0"));" after existing line "//              Serial.println("SUCCESS: Network Interface is configurated!");" in "C:\Program Files (x86)\Arduino\hardware\teensy\avr\libraries\NativeEthernet\src\NativeEthernet.cpp"
 * - For ultra-low latency: adjust value of AUDIO_BLOCK_SAMPLES from 128 to 16 (line 54 of C:\Program Files (x86)\Arduino\hardware\teensy\avr\cores\teensy4\AudioStream.h)
 * - If you use more than one Teensy on your local network, each one needs a unique (!) MAC address: edit line 37 of this file, before you upload your sketch!
 */

#include <Audio.h>
#include <fnet.h>
#include <NativeEthernet.h>
#include <NativeEthernetUdp.h>
#include "NetworkJitterBufferPlayQueue.h"
#include "QueueController.h"

// Ethernet MAC (use a locally unique one!)
byte mac[] = {
  0x00, 0xAA, 0xBB, 0xCC, 0xDE, 0x04
};

EthernetUDP Udp;

uint8_t recv_buf[OPENREMJAM_MONO_PACKET_SIZE];
uint8_t send_buf[OPENREMJAM_MONO_PACKET_SIZE]; 

// set up audio input:
AudioControlSGTL5000 shield;
AudioInputI2S i2s_in;
// downmix stereo line input to mono and feed it into rec_queue:
AudioMixer4 input_mixer;
AudioRecordQueue rec_queue;
AudioConnection input_to_mixer_0(i2s_in, 0, input_mixer, 0);
AudioConnection input_to_mixer_1(i2s_in, 1, input_mixer, 1);
AudioConnection mixer_to_rec_queue(input_mixer, rec_queue);

// qc cares for audio output:
QueueController qc;

// subindex is the position of an audio block within a network block
int subindex = 0;

// seqno is the sequence number of a network block. The receiver uses it for detecting packet loss and reordering.
uint32_t seqno = 0;

void setup() {
  pinMode(13, OUTPUT); // LED output;
  Serial.begin(9600); // parameter value doesn't matter -- Teensy always uses USB full speed
  AudioMemory(16 * OPENREMJAM_AUDIO_BLOCKS_PER_NETWORK_BLOCK + 10);    // each of the 16 queues needs up to OPENREMJAM_AUDIO_BLOCKS_PER_NETWORK_BLOCK audio blocks, plus 10 blocks headroom (e.g. for audio input)
  shield.enable();

  Serial.println("Initializing Ethernet with DHCP:");
  if (Ethernet.begin(mac) == 0) {
    Serial.println("Failed to configure Ethernet using DHCP");
    if (Ethernet.hardwareStatus() == EthernetNoHardware) {
      Serial.println("Ethernet shield was not found.  Sorry, can't run without hardware. :(");
    } else if (Ethernet.linkStatus() == LinkOFF) {
      Serial.println("Ethernet cable is not connected.");
    }
    // no point in carrying on, so do nothing forevermore:
    while (true) {
      Serial.println("Please reboot.");
      delay(1);
    }
  }
  Udp.begin(OPENREMJAM_DEFAULT_UDP_PORT);
  Serial.print("Ethernet adapter is ready! Local IP address: ");
  Serial.println(Ethernet.localIP());

  /********************** Set output volume *******************/
  shield.volume(0.3);

  /********************** Setup queues: ***********************/
  // Loopback (makes you hear your local audio signal)
  qc.getQueue(0)->setIP(IPAddress(127,0,0,1));
  qc.getQueue(0)->setPort(OPENREMJAM_DEFAULT_UDP_PORT);

  qc.getQueue(1)->setIP(IPAddress(192,168,178,34));
  qc.getQueue(1)->setPort(OPENREMJAM_DEFAULT_UDP_PORT);

  // start recording samples:
  rec_queue.begin();

  // print configuration information:
  Serial.printf("Audio block size: %d samples\r\n", AUDIO_BLOCK_SAMPLES);
  Serial.printf("Number of audio blocks per datatgram: %d\r\n", OPENREMJAM_AUDIO_BLOCKS_PER_NETWORK_BLOCK);
  Serial.printf("One network packet corresponds to %d microseconds.\r\n", OPENREMJAM_MONO_PACKET_DURATION_US);
}

void loop() {
      // process locally recorded samples
    if (rec_queue.available() > 0) {
        digitalWrite(13, HIGH);
        uint8_t *bufptr = (uint8_t *)rec_queue.readBuffer();
        memcpy(&send_buf[subindex*AUDIO_BLOCK_SAMPLES*2], bufptr, AUDIO_BLOCK_SAMPLES *2);
        rec_queue.freeBuffer();
        subindex++;
        
        if (subindex == OPENREMJAM_AUDIO_BLOCKS_PER_NETWORK_BLOCK) { // we have one a full packet now!
            // set seqno:
            memcpy( &send_buf[subindex*AUDIO_BLOCK_SAMPLES*2], &seqno, 4);
            seqno++;
            // we have a new block. Send it to all of our remote hosts:
            for (int i = 0; i < 16; ++i) {
                if (qc.getQueue(i)->getPort() != 0) {
                  Udp.beginPacket(qc.getQueue(i)->getIP(), qc.getQueue(i)->getPort());
                  Udp.write(send_buf, OPENREMJAM_MONO_PACKET_SIZE);
                  Udp.endPacket();
                }
            }
            subindex=0;
        }
    }
    digitalWrite(13, LOW);

    // receive incomming packets:
    if (Udp.parsePacket() == OPENREMJAM_MONO_PACKET_SIZE) {
        // we have received something that looks valid...

        // look up queue index
        int qi = qc.getQueueIndexByIP(Udp.remoteIP(), Udp.remotePort());

        if (qi < 0) {
            // we don't have a queue for this remote host, yet.
            qi = qc.getFreeAutoconnectQueueIndex(); // find a suitable queue!
            if (qi >= 0) {
                qc.getQueue(qi)->setIP(Udp.remoteIP());
                qc.getQueue(qi)->setPort(Udp.remotePort());
            } else {
                Serial.println("No free autoconnect queues!");
            }
        }

        if (qi >= 0) {
            Udp.read(recv_buf,sizeof(recv_buf));
            qc.getQueue(qi)->enqueue(recv_buf);
        }
    }
    // maintain IP configuration using DHCP
    Ethernet.maintain();
}
