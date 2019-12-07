#include "include/SPIFlash.h"

SPIFlash flash;
uint8_t testBuffer[512];
uint8_t readBuffer[512];

void setup() {
  //turn main circuit on
  pinMode(A0, OUTPUT);
  digitalWrite(A0, HIGH);
  ledcSetup(2, 5000, 8);
  ledcAttachPin(33, 2);  //BLUE
  ledcWrite(2, 20);
  
  //pull up SPI reset Pin
  pinMode(13, OUTPUT);
  digitalWrite(13, HIGH);
  //pull up SPI WP#
  pinMode(32, OUTPUT);
  digitalWrite(32, HIGH);
  //pull up SPI HOLD#
  pinMode(14, OUTPUT);
  digitalWrite(14, HIGH);

  Serial.begin(115200);
  while (!Serial.available()) ;
  Serial.println("Initialising Flash memory");
  flash.begin(15);

  //Testbuffer schreiben
  for (int i = 0; i < 512; i++) {
    testBuffer[i] = 169 + i % 10;
  }

  
  //##########################################################################


  uint32_t JEDEC = flash.getJEDECID();
  Serial.print("JEDEC: "); Serial.print(uint8_t(JEDEC >> 16)); Serial.print(" "); Serial.print(uint8_t(JEDEC >> 8)); Serial.print(" "); Serial.println(uint8_t(JEDEC >> 0));
  Serial.print("Capacity: "); Serial.println(flash.getCapacity());
  Serial.print("MaxPage: "); Serial.println(flash.getMaxPage());

  //Serial.print("\nReleasing all sectors from protection: ... ");
  //if (flash.ASP_release_all_sectors())Serial.println("Successful.\n"); else Serial.println("Error.\n");

  //FULL ERASING
  //Serial.print("\nErasing all ... ");
  //flash.eraseChip();
  //Serial.println("Done.");

  //PART ERASING
  Serial.print("\nErasing Pages 0-511 (256kB sector): ... "); if (!flash.eraseBlock256K(0)) Serial.println("Error.");
  while (true) { if (flash.CheckErasing_inProgress() != 3) { if (flash.CheckErasing_inProgress() == 0) { Serial.println("Successful."); break;} else { Serial.println("Error."); break; }}}

  //WRITING 2 Pages
  Serial.print("Writing testBuffer to flash ... \n");
  for (int i = 0; i < 2; i++) {
    if (flash.writeByteArray(i * 512, testBuffer, 512)) {
      Serial.print("Page ");
      Serial.print(i);
      Serial.println(" successfully written.");
    } else Serial.println("Error in Page writing.");
  }

  read_eight_pages();

  //WRITING Single Bytes
  Serial.print("\n\nWriting single bytes to flash ... \n");
  Serial.print(flash.writeByte(512*2 + 3, 69));
  Serial.print(flash.writeByte(512*2 + 0, 1));
  Serial.print(flash.writeByte(512*2 + 1, 2));
  Serial.print(flash.writeByte(512*2 + 2, 3));
  Serial.print(flash.writeByte(512*2 + 0, 42));
  Serial.print(flash.writeByte(512*2 + 5, 52));

  read_eight_pages();

  //PART ERASING
  //Serial.print("\nErasing Pages 0-511 (256kB sector): ... "); if (!flash.eraseBlock256K(0)) Serial.println("Error.");
  //while (true) {
  //  if (flash.CheckErasing_inProgress() != 3) {
  //    if (flash.CheckErasing_inProgress() == 0) {
  //      Serial.println("Successful.");
  //      break;
  //    } else {
  //      Serial.println("Error.");
  //      break;
  //    }
  //  }
  //}
}


void read_eight_pages(){
  //READING
  Serial.println("\n\nReading 8 Pages from flash to readBuffer:");
  for (int i = 0; i < 8; i++) {
    Serial.print("\nPage "); Serial.print(i);    Serial.print(": ");
    flash.readByteArray(i * 512, readBuffer, 512);
    for (int i = 0; i < 512; i++) {
      Serial.print(readBuffer[i]); Serial.print(" ");
    }
  }
  Serial.println("\nDone.\n");
}


void loop() {
}
