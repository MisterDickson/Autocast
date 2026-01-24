#include <SPI.h>

#include "BluetoothA2DPSink.h"

#define CS_PIN 5

BluetoothA2DPSink a2dp_sink;
int last_volume, current_volume;


struct TrackInfo {
  String title = "";
  String artist = "";
} currentTrack;

const uint16_t character_patterns[] = {
  0b0000000000000000, /* (space) */
  0b0000000100001001, /* ! */
  0b0000000000011000, /* " */
  0b0101010010011001, /* # */
  0b0101010010110011, /* $ */
  0b0001111011010111, /* % */
  0b0111100000110100, /* & */
  0b0000000000010000, /* ' */
  0b0000100001000000, /* ( */
  0b0000001000000100, /* ) */
  0b0001111011010100, /* * */
  0b0001010010010000, /* + */
  0b0000001000000000, /* , */
  0b0001000010000000, /* - */
  0b0000000100000000, /* . */
  0b0000001001000000, /* / */
  0b0110001001101011, /* 0 */
  0b0000000001001001, /* 1 */
  0b0111000010101000, /* 2 */
  0b0100000010101001, /* 3 */
  0b0001000010001011, /* 4 */
  0b0101100000100010, /* 5 */
  0b0111000010100011, /* 6 */
  0b0000000000101001, /* 7 */
  0b0111000010101011, /* 8 */
  0b0101000010101011, /* 9 */
  0b0000010000010000, /* : */
  0b0000001000010000, /* ; */
  0b0001100001000000, /* < */
  0b0101000010000000, /* = */
  0b0000001010000100, /* > */
  0b0000010110101000, /* ? */
  0b0110000010111010, /* @ */
  0b0011000010101011, /* A */
  0b0100010010111001, /* B */
  0b0110000000100010, /* C */
  0b0100010000111001, /* D */
  0b0111000000100010, /* E */
  0b0011000000100010, /* F */
  0b0110000010100011, /* G */
  0b0011000010001011, /* H */
  0b0100010000110000, /* I */
  0b0110000000001001, /* J */
  0b0011100001000010, /* K */
  0b0110000000000010, /* L */
  0b0010000001001111, /* M */
  0b0010100000001111, /* N */
  0b0110000000101011, /* O */
  0b0011000010101010, /* P */
  0b0110100000101011, /* Q */
  0b0011100010101010, /* R */
  0b0101000010100011, /* S */
  0b0000010000110000, /* T */
  0b0110000000001011, /* U */
  0b0010001001000010, /* V */
  0b0010101000001011, /* W */
  0b0000101001000100, /* X */
  0b0101000010001011, /* Y */
  0b0100001001100000, /* Z */
  0b0110000000100010, /* [ */
  0b0000100000000100, /* \ */
  0b0100000000101001, /* ] */
  0b0000101000000000, /* ^ */
  0b0100000000000000, /* _ */
  0b0000000000000100, /* ` */
  0b0111010000000000, /* a */
  0b0111100000000010, /* b */
  0b0111000010000000, /* c */
  0b0100001010001001, /* d */
  0b0111001000000000, /* e */
  0b0001010011000000, /* f */
  0b0100000011001001, /* g */
  0b0011010000000010, /* h */
  0b0000010000000000, /* i */
  0b0010001000010000, /* j */
  0b0000110001010000, /* k */
  0b0010000000000010, /* l */
  0b0011010010000001, /* m */
  0b0011010000000000, /* n */
  0b0111000010000001, /* o */
  0b0011000000000110, /* p */
  0b0000000011001001, /* q */
  0b0011000000000000, /* r */
  0b0100100010000000, /* s */
  0b0111000000000010, /* t */
  0b0110000000000001, /* u */
  0b0010001000000000, /* v */
  0b0010101000000001, /* w */
  0b0000101001000100, /* x */
  0b0100000010011001, /* y */
  0b0101001000000000, /* z */
  0b0101001000100100, /* { */
  0b0000010000010000, /* | */
  0b0100100011100000, /* } */
  0b0001001011000000, /* ~ */
  0b0000000000000000, /* (del) */
};

const String ascii_ify(const String unicode_str) {
  String ret = unicode_str;

  // German
  ret.replace("ä", "ae");
  ret.replace("ö", "oe");
  ret.replace("ü", "ue");
  ret.replace("Ä", "AE");
  ret.replace("Ö", "OE");
  ret.replace("Ü", "UE");
  ret.replace("ß", "sz");
  ret.replace("ẞ", "SZ");

  // Accented vowels (Western / Central Europe)
  ret.replace("à", "a");
  ret.replace("á", "a");
  ret.replace("â", "a");
  ret.replace("ã", "a");
  ret.replace("å", "a");
  ret.replace("À", "A");
  ret.replace("Á", "A");
  ret.replace("Â", "A");
  ret.replace("Ã", "A");
  ret.replace("Å", "A");

  ret.replace("è", "e");
  ret.replace("é", "e");
  ret.replace("ê", "e");
  ret.replace("ë", "e");
  ret.replace("È", "E");
  ret.replace("É", "E");
  ret.replace("Ê", "E");
  ret.replace("Ë", "E");

  ret.replace("ì", "i");
  ret.replace("í", "i");
  ret.replace("î", "i");
  ret.replace("ï", "i");
  ret.replace("Ì", "I");
  ret.replace("Í", "I");
  ret.replace("Î", "I");
  ret.replace("Ï", "I");

  ret.replace("ò", "o");
  ret.replace("ó", "o");
  ret.replace("ô", "o");
  ret.replace("õ", "o");
  ret.replace("Ò", "O");
  ret.replace("Ó", "O");
  ret.replace("Ô", "O");
  ret.replace("Õ", "O");

  ret.replace("ù", "u");
  ret.replace("ú", "u");
  ret.replace("û", "u");
  ret.replace("Ù", "U");
  ret.replace("Ú", "U");
  ret.replace("Û", "U");

  // Nordic / French
  ret.replace("æ", "ae");
  ret.replace("Æ", "AE");
  ret.replace("œ", "oe");
  ret.replace("Œ", "OE");
  ret.replace("ø", "o");
  ret.replace("Ø", "O");

  // Slavic / Eastern Europe
  ret.replace("č", "c");
  ret.replace("ć", "c");
  ret.replace("ç", "c");
  ret.replace("Č", "C");
  ret.replace("Ć", "C");
  ret.replace("Ç", "C");

  ret.replace("š", "s");
  ret.replace("ś", "s");
  ret.replace("Š", "S");
  ret.replace("Ś", "S");

  ret.replace("ž", "z");
  ret.replace("ź", "z");
  ret.replace("ż", "z");
  ret.replace("Ž", "Z");
  ret.replace("Ź", "Z");
  ret.replace("Ż", "Z");

  ret.replace("ñ", "n");
  ret.replace("Ñ", "N");

  ret.replace("ý", "y");
  ret.replace("ÿ", "y");
  ret.replace("Ý", "Y");

  // Cyrillic
  ret.replace("А", "A");
  ret.replace("Б", "B");
  ret.replace("В", "V");
  ret.replace("Г", "G");
  ret.replace("Д", "D");
  ret.replace("Е", "E");
  ret.replace("Ё", "Yo");
  ret.replace("Ж", "Zh");
  ret.replace("З", "Z");
  ret.replace("И", "I");
  ret.replace("Й", "Y");
  ret.replace("К", "K");
  ret.replace("Л", "L");
  ret.replace("М", "M");
  ret.replace("Н", "N");
  ret.replace("О", "O");
  ret.replace("П", "P");
  ret.replace("Р", "R");
  ret.replace("С", "S");
  ret.replace("Т", "T");
  ret.replace("У", "U");
  ret.replace("Ф", "F");
  ret.replace("Х", "Kh");
  ret.replace("Ц", "Ts");
  ret.replace("Ч", "Ch");
  ret.replace("Ш", "Sh");
  ret.replace("Щ", "Shch");
  ret.replace("Ы", "Y");
  ret.replace("Э", "E");
  ret.replace("Ю", "Yu");
  ret.replace("Я", "Ya");

  ret.replace("а", "a");
  ret.replace("б", "b");
  ret.replace("в", "v");
  ret.replace("г", "g");
  ret.replace("д", "d");
  ret.replace("е", "e");
  ret.replace("ё", "yo");
  ret.replace("ж", "zh");
  ret.replace("з", "z");
  ret.replace("и", "i");
  ret.replace("й", "y");
  ret.replace("к", "k");
  ret.replace("л", "l");
  ret.replace("м", "m");
  ret.replace("н", "n");
  ret.replace("о", "o");
  ret.replace("п", "p");
  ret.replace("р", "r");
  ret.replace("с", "s");
  ret.replace("т", "t");
  ret.replace("у", "u");
  ret.replace("ф", "f");
  ret.replace("х", "kh");
  ret.replace("ц", "ts");
  ret.replace("ч", "ch");
  ret.replace("ш", "sh");
  ret.replace("щ", "shch");
  ret.replace("ы", "y");
  ret.replace("э", "e");
  ret.replace("ю", "yu");
  ret.replace("я", "ya");

  // Ligatures / oddballs
  ret.replace("ð", "d");
  ret.replace("Ð", "D");
  ret.replace("þ", "th");
  ret.replace("Þ", "TH");
  ret.replace("ł", "l");
  ret.replace("Ł", "L");

  return ret;
}

uint16_t pattern(char c) {
  if ((unsigned char)c < 32 || (unsigned char)c > 127)
    return 0b0001000010000000; /* - */

  return character_patterns[(unsigned char)c - 32];
}

void display_text(const char* str) {
  size_t len = strlen(str);
  digitalWrite(CS_PIN, LOW);
  for (int i = len - 1; i >= 0; i--) {
    SPI.transfer16(pattern(str[i]));
  }
  digitalWrite(CS_PIN, HIGH);
}

void update_display(const String& top_line, const String& bottom_line, uint16_t interval_ms) {
  static String prev_top = "";
  static String prev_bottom = "";
  static unsigned long last_scroll_time = 0;
  static uint16_t top_offset = 0;
  static uint16_t bottom_offset = 0;
  const uint8_t DISPLAY_WIDTH = 14;
  const uint8_t SEPARATOR_SPACES = 2;

  // Check if content has changed - reset scroll if so
  if (top_line != prev_top || bottom_line != prev_bottom) {
    prev_top = top_line;
    prev_bottom = bottom_line;
    top_offset = 0;
    bottom_offset = 0;
    last_scroll_time = millis();
  }

  // Helper lambda to prepare a line for display
  auto prepare_line = [&](const String& line, uint16_t& offset) -> String {
    String display_text;
    if (line.length() <= DISPLAY_WIDTH) {
      display_text = line;
      while (display_text.length() < DISPLAY_WIDTH) {
        display_text += ' ';
      }
    } else {
      String ring_buffer = line;
      for (uint8_t i = 0; i < SEPARATOR_SPACES; i++) {
        ring_buffer += ' ';
      }
      ring_buffer += line;

      for (uint8_t i = 0; i < DISPLAY_WIDTH; i++) {
        display_text += ring_buffer[(offset + i) % ring_buffer.length()];
      }
    }
    return display_text;
  };

  // Check if it's time to scroll
  unsigned long current_time = millis();
  if (current_time - last_scroll_time >= interval_ms) {
    last_scroll_time = current_time;

    // Advance scroll position for lines longer than display width
    if (top_line.length() > DISPLAY_WIDTH) {
      top_offset = (top_offset + 1) % (top_line.length() + SEPARATOR_SPACES);
    }
    if (bottom_line.length() > DISPLAY_WIDTH) {
      bottom_offset = (bottom_offset + 1) % (bottom_line.length() + SEPARATOR_SPACES);
    }
  }

  String top_display = prepare_line(top_line, top_offset);
  String bottom_display = prepare_line(bottom_line, bottom_offset);
  display_text((top_display + bottom_display).c_str());
}

void avrc_metadata_callback(uint8_t id, const uint8_t* text) {
  String metadata = String((char*)text);

  switch (id) {
    case ESP_AVRC_MD_ATTR_TITLE:
      metadata.replace(" • Lossless", "");
      currentTrack.title = ascii_ify(metadata);
      break;
    case ESP_AVRC_MD_ATTR_ARTIST:
      metadata.replace(" • Lossless", "");
      currentTrack.artist = ascii_ify(metadata);
      break;
  }
}

enum display_information_t { NOT_CONNECTED,
                             NOW_PLAYING,
                             VOLUME_OVERLAY } display_information = NOT_CONNECTED;

void connection_state_changed(esp_a2d_connection_state_t state, void* ptr) {

  if (state == ESP_A2D_CONNECTION_STATE_CONNECTED) {
    display_information = NOW_PLAYING;
  } else if (state == ESP_A2D_CONNECTION_STATE_DISCONNECTED) {
    display_information = NOT_CONNECTED;
  }
}

void audio_state_changed(esp_a2d_audio_state_t state, void* ptr) {
  switch (state) {
    case ESP_A2D_AUDIO_STATE_STARTED:
      display_information = NOW_PLAYING;
      break;
    default:
      /**/
      break;
  }
}

void setup() {
  i2s_pin_config_t pin_config = {
    .bck_io_num = 26,    // Bit clock
    .ws_io_num = 25,     // Word select (LR clock)
    .data_out_num = 22,  // Data out
    .data_in_num = I2S_PIN_NO_CHANGE
  };

  a2dp_sink.set_pin_config(pin_config);

  pinMode(CS_PIN, OUTPUT);
  digitalWrite(CS_PIN, HIGH);

  SPI.begin(18, 19, 23, CS_PIN);

  Serial.begin(115200);
  a2dp_sink.set_on_connection_state_changed(connection_state_changed);
  a2dp_sink.set_avrc_metadata_callback(avrc_metadata_callback);
  a2dp_sink.set_on_audio_state_changed(audio_state_changed);
  a2dp_sink.start("Mazda 323");
  last_volume = current_volume = a2dp_sink.get_volume();
}

String top_display_content;
String bottom_display_content;

uint16_t overlay_duration_ms = 1000;
unsigned long timestamp = 0;
bool overlay_on = false;

void loop() {

  current_volume = a2dp_sink.get_volume();

  if (current_volume != last_volume) {
    display_information = VOLUME_OVERLAY;
    last_volume = current_volume;
    timestamp = millis();
  }

  switch (display_information) {
    case NOT_CONNECTED:
      top_display_content = "   Ready to";
      bottom_display_content = "   connect";
      break;

    case NOW_PLAYING:
      top_display_content = currentTrack.title;
      bottom_display_content = currentTrack.artist;

      if (top_display_content.length() < 1 && bottom_display_content.length() < 1) {
        top_display_content = "    Device";
        bottom_display_content = "   connected";
      }

      break;


    case VOLUME_OVERLAY:
      if (overlay_on) {
        if (millis() - timestamp > overlay_duration_ms) {
          overlay_on = false;
          display_information = NOW_PLAYING;
        }
      } else {
        overlay_on = true;
        timestamp = millis();
      }

      top_display_content = "Volume";

      bottom_display_content = "[";

      for (int i = 0; i < current_volume; i += 11) {
        bottom_display_content += "-";
      }

      for (int i = bottom_display_content.length(); i < 13; i++) {
        bottom_display_content += " ";
      }

      bottom_display_content += "]";
      break;
  }

  update_display(top_display_content, bottom_display_content, 300);


  // handle playback control events



  /*
  if (Serial.available() > 0 && isConnected) {
    char cmd = Serial.read();

    switch (cmd) {
      case 'p':
        a2dp_sink.play();
        break;

      case 's':
        a2dp_sink.pause();
        break;

      case 'n':
        a2dp_sink.next();
        break;

      case 'b':
        a2dp_sink.previous();
        break;

      case '+':
        a2dp_sink.set_volume(a2dp_sink.get_volume() + 10);
        break;

      case '-':
        a2dp_sink.set_volume(a2dp_sink.get_volume() - 10);
        break;

      case '?':
        Serial.printf("Current volume: %d\n", a2dp_sink.get_volume());
        break;
    }
  } else if (Serial.available() > 0 && !isConnected) {
    Serial.read();  // Clear buffer
  }*/

  delay(10);
}