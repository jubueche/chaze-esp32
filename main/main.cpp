#include "BNO055.h"


extern "C" void app_main()
{

	BNO055 bno = BNO055();

	if (!bno.begin())
	  {
	    printf("No connection.\n");
	  }

	while(1){
		imu::Quaternion quat = bno.getQuat();

		/* Display the quat data */
		printf("qW: %.04f qX: %.04f qY: %.04f qZ: %.04f\n",quat.w(),quat.y(),quat.x(), quat.z());

		uint8_t system, gyro, accel, mag = 0;
		bno.getCalibration(&system, &gyro, &accel, &mag);

		printf("CALIBRATION: Sys= %d Gyro= %d Accel= %d Mag= %d", system, gyro, accel, mag);
		vTaskDelay(100 / portTICK_PERIOD_MS);
	}
}

