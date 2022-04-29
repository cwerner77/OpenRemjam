# OpenRemjam – ultra-low latency audio streaming solution for Teensy 4.1

## License

MIT (see LICENSE file)
 
## Hardware requirements

- Teensy 4.1
- Teensy Audio shield for Teensy 4.x (rev D) (https://www.pjrc.com/store/teensy3_audio.html)
- Ethernet Kit for Teensy 4.1 (https://www.pjrc.com/store/ethernet_kit.html)

## Software requirements

- Arduino 1.8.19 or newer
- Teensyduino 1.56 or newer with installed libraries:
    - Audio
    - Entropy
    - FNET 
    - NativeEthernet
    - NativeEthernetUdp
    - SerialFlash

- External Libraries
    - CmdParser (https://www.arduino.cc/reference/en/libraries/cmdparser/)
   
## Getting started

- Enable loopback interface: in file `C:\Program Files (x86)\Arduino\hardware\teensy\avr\libraries\FNET\src\fnet_user_config.h` append line

        #define FNET_CFG_LOOPBACK (1)

- Make NativeEthernet use ETH0 as defaut (instead of loopback interface): in file
  `C:\Program Files (x86)\Arduino\hardware\teensy\avr\libraries\NativeEthernet\src\NativeEthernet.cpp` after the existing line

        //              Serial.println("SUCCESS: Network Interface is configurated!");

  append the new line

        fnet_netif_set_default(fnet_netif_get_by_name("eth0"));

- For low latency: adjust value of AUDIO_BLOCK_SAMPLES from 128 to 16 in line 54 of `C:\Program Files (x86)\Arduino\hardware\teensy\avr\cores\teensy4\AudioStream.h`

        #define AUDIO_BLOCK_SAMPLES  16

- After uploading your sketch, you should see the LEDs of your Ethernet Kit blinking. After a while, also the orange LED of your Teensy should light up at low intensity. This indicates that your Teensy is sampling audio data.
- Open the serial monitor to see debug output.

## Available commands in serial monitor

- Show available queues

        Syntax:  SHOW
        Example: SHOW

- Connect to a remote host.

        Syntax:  CONNECT <queue-id> <ip> <port> 
        Example: CONNECT 1 192.168.178.23 9000

- Disconnect a queue. To avoid unintended reconnects due to inconning traffic from the remote host, this also disables the auto-connect feature for all queues!

        Syntax:  DISCONNECT <queue-id>
        Example: DISCONNECT 1
