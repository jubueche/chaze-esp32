//#define WRITING
//#define READING
//#define ERASING
//#define WRITECALIBRATION
#define READCALIBRATION
//#define SIMPLEFULLERASE

#define DEBUG

#include "ChazeFlashtraining.h"

Flashtraining training;

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  Serial.println("Please send a character to start ...");
  while (!Serial.available());
  //zum Testen
  float floaty = 11.1111;
  byte arr[4];

  long l = *(long*) &floaty;
  arr[0] = l >> 24;             //linkeste Bits von l
  arr[1] = l >> 16;
  arr[2] = l >> 8;
  arr[3] = l;                   //rechteste Bits von l
  Serial.print("floaty: ");
  Serial.println(floaty);
  Serial.print("long-Wert von floaty: ");
  Serial.println(l);
  Serial.println(arr[0]);
  Serial.println(arr[1]);
  Serial.println(arr[2]);
  Serial.println(arr[3]);

  long l_regenerated = 16777216 * arr[0] + 65536 * arr[1] + 256 * arr[2] + arr[3];
  float floaty_regenerated = *(float*) &l_regenerated;
  Serial.print("l_regenerated: ");
  Serial.println(l_regenerated);
  Serial.print("floaty_regenerated: ");
  Serial.println(floaty_regenerated);

  Serial.println("\n#######################################\nGESAMTTEST:\n\n");   //Setting: see defines at the very top of the sketch

#ifdef WRITING

  for (int trainingnr = 0; trainingnr < 10; trainingnr++) {

    Serial.print("STATE: "); Serial.println(training.get_STATE());
    Serial.print("Start new training: "); Serial.println((training.start_new_training() == true) ? "successful" : "error");
    Serial.print("STATE: "); Serial.println(training.get_STATE());

    for (int i = 0; i < 1000; i++) {
      Serial.print("Frame "); Serial.print(i); Serial.print(": ");
      Serial.println((training.write_training_cycle_pressure(i, 11.1111) ? "pressure successfully written" : "error"));
      Serial.println((training.write_training_cycle_IMU(i, 11.1111, 22.2222, 33.3333, 44.4444, 55.5555, 66.6666, 77.7777) ? "IMU successfully written" : "error"));
    }
    Serial.print("STATE: "); Serial.println(training.get_STATE());
    Serial.print("Stop training: ");
    Serial.println((training.stop_training() == 1) ? "successful" : "error");
    Serial.print("STATE: "); Serial.println(training.get_STATE());

  }

#endif


#ifdef READING
  Serial.print("STATE: "); Serial.println(training.get_STATE());
  Serial.print("Start reading data: "); Serial.println((training.start_reading_data() == true) ? "successful" : "error");
  Serial.print("STATE: "); Serial.println(training.get_STATE());

  byte bufff[512];
  while (training.get_all_data_bufferwise(bufff)) {
    for (int ie = 0; ie < 512; ie++) {
      Serial.print(bufff[ie]); Serial.print(" ");
    }
    Serial.println();
    Serial.print("STATE: "); Serial.println(training.get_STATE());
  }
  Serial.print("STATE: "); Serial.println(training.get_STATE());
#endif

#ifdef ERASING
  Serial.print("STATE: "); Serial.println(training.get_STATE());
  Serial.print("Start erasing data: "); Serial.println((training.start_delete_all_trainings() == true) ? "successful" : "error");
  Serial.print("STATE: "); Serial.println(training.get_STATE());
  while (training.get_STATE() != 1) {
    training.please_call_every_loop();
  }
  Serial.print("STATE: "); Serial.println(training.get_STATE());

#endif


#ifdef WRITECALIBRATION
  if (training.writeCalibration(2, 2))Serial.println("\nWriting Calibration successful.");
  else Serial.println("\nError in Writing Calibration.");
#endif


#ifdef READCALIBRATION
  for (int irc = 0; irc < 256; irc++) {
    Serial.print("storageaddress "); Serial.print(irc); Serial.print(": "); Serial.println(training.readCalibration(irc), 7);
  }
  Serial.println("\nDone.");
#endif


#ifdef SIMPLEFULLERASE

  pinMode(SPIFlash_SUPPLY, OUTPUT);
  digitalWrite(SPIFlash_SUPPLY, HIGH);
  pinMode(SPIFlash_RESET, OUTPUT);
  digitalWrite(SPIFlash_RESET, HIGH);
  pinMode(SPIFlash_WP, OUTPUT);
  digitalWrite(SPIFlash_WP, HIGH);
  pinMode(SPIFlash_HOLD, OUTPUT);
  digitalWrite(SPIFlash_HOLD, HIGH);

  SPIFlash flash;
  Serial.println(flash.begin(SPIFlash_CS) ? "Init successful" : "Init failed");
  //Serial.print("\nReleasing all sectors from protection: ... ");
  //if (flash.ASP_release_all_sectors())Serial.println("Successful.\n"); else Serial.println("Error.\n");
  Serial.println(flash.eraseChip() ? "Full Erase successful" : "Full Erase failed");

#endif

}

void loop() {

}
