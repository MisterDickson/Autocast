#include "BluetoothA2DPSink.h"

BluetoothA2DPSink a2dp_sink;
bool isConnected = false;

struct TrackInfo {
    String title = "";
    String artist = "";
} currentTrack;

void avrc_metadata_callback(uint8_t id, const uint8_t *text) {
    String metadata = String((char*)text);
    
    switch (id) {
        case ESP_AVRC_MD_ATTR_TITLE:
            currentTrack.title = metadata;
            break;
        case ESP_AVRC_MD_ATTR_ARTIST:
            currentTrack.artist = metadata;
            break;
    }
    
    Serial.println("\n--- Now Playing ---");
    Serial.println("Title:  " + currentTrack.title);
    Serial.println("Artist: " + currentTrack.artist);
    Serial.println("-------------------\n");
}

void connection_state_changed(esp_a2d_connection_state_t state, void *ptr) {
    if (state == ESP_A2D_CONNECTION_STATE_CONNECTED) {
        Serial.println("Device connected!");
        isConnected = true;
    } else if (state == ESP_A2D_CONNECTION_STATE_DISCONNECTED) {
        Serial.println("Device disconnected!");
        isConnected = false;
    }
}

void setup() {
    Serial.begin(115200);
    a2dp_sink.set_on_connection_state_changed(connection_state_changed);
    a2dp_sink.set_avrc_metadata_callback(avrc_metadata_callback);
    a2dp_sink.start("Mazda 323");
    Serial.println("Ready to connect");
}

void loop() {
    if (Serial.available() > 0 && isConnected) {
        char cmd = Serial.read();
        
        switch(cmd) {
            case 'p':
                Serial.println("Play");
                a2dp_sink.play();
                break;
                
            case 's':
                Serial.println("Pause");
                a2dp_sink.pause();
                break;
                
            case 'n':
                Serial.println("Next");
                a2dp_sink.next();
                break;
                
            case 'b':
                Serial.println("Previous");
                a2dp_sink.previous();
                break;
                
            case '+':
                Serial.println("Volume up");
                a2dp_sink.set_volume(a2dp_sink.get_volume() + 10);
                break;
                
            case '-':
                Serial.println("Volume down");
                a2dp_sink.set_volume(a2dp_sink.get_volume() - 10);
                break;
                
            case '?':
                Serial.printf("Current volume: %d\n", a2dp_sink.get_volume());
                break;
        }
    } else if (Serial.available() > 0 && !isConnected) {
        Serial.read(); // Clear buffer
        Serial.println("No device connected!");
    }
    
    delay(10);
}