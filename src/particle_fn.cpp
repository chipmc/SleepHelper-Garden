//Particle Functions
#include "Particle.h"
#include "particle_fn.h"

char wateringThresholdPctStr[8] = " ";
char wateringDurationStr[16] = " ";
char heatThresholdStr[8] = " ";
char wakeTimeStr[8] = " ";
char sleepTimeStr[8] = " ";

/**
 * @brief Initializes the Particle functions and variables
 * 
 * @details If new particles of functions are defined, they need to be initialized here
 * 
 */
void particleInitialize() {
  // Battery conect information - https://docs.particle.io/reference/device-os/firmware/boron/#batterystate-
  const char* batteryContext[8] = {"Unknown","Not Charging","Charging","Charged","Discharging","Fault","Diconnected"};

  Log.info("Initializing Particle functions and variables");     // Note: Don't have to be connected but these functions need to in first 30 seconds
  Particle.variable("Internal Temp", internalTempStr);
  Particle.variable("Wake Time", wakeTimeStr);
  Particle.variable("Sleep Time", sleepTimeStr);
  Particle.variable("Sleep Enabled",(sysStatus.enableSleep) ? "Yes" : "No");
  Particle.variable("Release",currentPointRelease);   
  Particle.variable("Signal", signalStr);
  Particle.variable("stateOfChg", current.stateOfCharge);
  Particle.variable("BatteryContext",batteryContext[current.batteryState]);
  Particle.variable("SoilMoisture", soilMoistureStr);   
  Particle.variable("Soil Temp", soilTempStr);
  Particle.variable("WateringPct",wateringThresholdPctStr);
  Particle.variable("WateringDuration",wateringDurationStr);
  Particle.variable("Heat Threshold",heatThresholdStr);

  Particle.function("Enable Sleep", setEnableSleep);
  // Particle.function("Set Wake Time", setWakeTime);
  // Particle.function("Set Sleep Time", setSleepTime);
  Particle.function("Set Water Threshold",setWaterThreshold);
  Particle.function("Set Heat Threshold",setHeatThreshold);
  Particle.function("Set Water Duration",setWaterDuration);

  if (!digitalRead(BUTTON_PIN)) sysStatus.enableSleep = false;     // If the user button is held down while resetting - diable sleep

  takeMeasurements();                               // Initialize sensor values

  makeUpStringMessages();                           // Initialize the string messages needed for the Particle Variables
}

/**
 * @brief Sets the closing time of the facility.
 *
 * @details Extracts the integer from the string passed in, and sets the closing time of the facility
 * based on this input. Fails if the input is invalid.
 *
 * @param command A string indicating what the closing hour of the facility is in 24-hour time.
 * Inputs outside of "0" - "24" will cause the function to return 0 to indicate an invalid entry.
 *
 * @return 1 if able to successfully take action, 0 if invalid command
 */
int setWakeTime(String command)
{
  char * pEND;
  char data[64];
  int tempTime = strtol(command,&pEND,10);                             // Looks for the first integer and interprets it
  if ((tempTime < 0) || (tempTime > 23)) return 0;                     // Make sure it falls in a valid range or send a "fail" result
  sysStatus.wakeTime = tempTime;
  snprintf(data, sizeof(data), "Wake time set to %i",sysStatus.wakeTime);
  Log.info(data);
  if (Particle.connected()) {
    Particle.publish("Time",data, PRIVATE);
  }
  return 1;
}

/**
 * @brief Sets the closing time of the facility.
 *
 * @details Extracts the integer from the string passed in, and sets the closing time of the facility
 * based on this input. Fails if the input is invalid.
 *
 * @param command A string indicating what the closing hour of the facility is in 24-hour time.
 * Inputs outside of "0" - "24" will cause the function to return 0 to indicate an invalid entry.
 *
 * @return 1 if able to successfully take action, 0 if invalid command
 */
int setSleepTime(String command)
{
  char * pEND;
  char data[64];
  int tempTime = strtol(command,&pEND,10);                       // Looks for the first integer and interprets it
  if ((tempTime < 0) || (tempTime > 24)) return 0;   // Make sure it falls in a valid range or send a "fail" result
  sysStatus.sleepTime = tempTime;
  snprintf(data, sizeof(data), "Sleep time set to %i",sysStatus.sleepTime);
  Log.info(data);
  if (Particle.connected()) {
    Particle.publish("Time",data, PRIVATE);
  }
  return 1;
}

/**
 * @brief Toggles the device into low power mode based on the input command.
 *
 * @details If the command is "1", sets the device into low power mode. If the command is "0",
 * sets the device into normal mode. Fails if neither of these are the inputs.
 *
 * @param command A string indicating whether to set the device into low power mode or into normal mode.
 * A "1" indicates low power mode, a "0" indicates normal mode. Inputs that are neither of these commands
 * will cause the function to return 0 to indicate an invalid entry.
 *
 * @return 1 if able to successfully take action, 0 if invalid command
 */
int setEnableSleep(String command)                                   // This is where we can put the device into low power mode if needed
{
  char data[64];
  if (command != "1" && command != "0") return 0;                     // Before we begin, let's make sure we have a valid input
  if (command == "1") {                                               // Command calls for enabling sleep
    sysStatus.enableSleep = true;
  }
  else {                                                             // Command calls for disabling sleep
    sysStatus.enableSleep = false;
  }
  snprintf(data, sizeof(data), "Enable sleep is %s", (sysStatus.enableSleep) ? "true" : "false");
  Log.info(data);
  if (Particle.connected()) {
    Particle.publish("Mode",data, PRIVATE);
  }
  return 1;
}

/**
 * @brief Let's you set the threashold for watering
 * 
 * @details Input the watering threshold in percent from 0 (bone dry) to 100 (soaking)
 *
 * @param command A string indicating what soil moisture precentage would be the minimum before watering is initiated.  A value of 0 disables watering
 * 
 * @return 1 if able to successfully take action, 0 if invalid command
 */
int setWaterThreshold(String command)                                  // This is the amount of time in seconds we will wait before starting a new session
{
  char * pEND;
  float tempThreshold = strtof(command,&pEND);                         // Looks for the first float and interprets it
  if ((tempThreshold < 0.0) | (tempThreshold > 100.0)) return 0;       // Make sure it falls in a valid range or send a "fail" result
  sysStatus.wateringThresholdPct = tempThreshold;                      // debounce is how long we must space events to prevent overcounting
  makeUpStringMessages();

  if (Particle.connected()) {                                         // Publish result if feeling verbose
    if (sysStatus.wateringThresholdPct == 0) Particle.publish("System","Watering function disabled",PRIVATE);
    else Particle.publish("Threshold",wateringThresholdPctStr, PRIVATE);
  }
  return 1;                                                            // Returns 1 to let the user know if was reset
}

/**
 * @brief Let's you set the temperature threashold for watering in degrees C
 * 
 * @details Input the watering threshold in percent from 0 (freezing) to 100 (boiling)
 *
 * @param command A string indicating the temperature in C above which we will water to keep cool.  A value of 100 disables temp watering.
 * 
 * @return 1 if able to successfully take action, 0 if invalid command
 */
int setHeatThreshold(String command)                                  // This is the amount of time in seconds we will wait before starting a new session
{
  char * pEND;
  float tempThreshold = strtof(command,&pEND);                         // Looks for the first float and interprets it
  if ((tempThreshold < 0.0) | (tempThreshold > 100.0)) return 0;       // Make sure it falls in a valid range or send a "fail" result
  sysStatus.heatThreshold = tempThreshold;                      // debounce is how long we must space events to prevent overcounting
  makeUpStringMessages();

  if (Particle.connected()) {                                         // Publish result if feeling verbose
    if (sysStatus.heatThreshold == 100) Particle.publish("System","Heat watering function disabled",PRIVATE);
    else Particle.publish("Heat",wateringThresholdPctStr, PRIVATE);
  }
  return 1;                                                            // Returns 1 to let the user know if was reset
}

/**
 * @brief Let's you set the duration of the watering
 * 
 * @details Input the watering duration in seconds from 0 1000 seconds
 *
 * @param Pass the wating duration in seconds.
 * 
 * @return 1 if able to successfully take action, 0 if invalid command
 */
int setWaterDuration(String command)                                   // This is the amount of time in seconds we will wait before starting a new session
{
  char * pEND;
  float tempValue = strtol(command,&pEND,10);                          // Looks for the first float and interprets it
  if ((tempValue < 0) | (tempValue > 1000)) return 0;                  // Make sure it falls in a valid range or send a "fail" result
  sysStatus.wateringDuration = tempValue;                              // debounce is how long we must space events to prevent overcounting
  makeUpStringMessages();
  if (Particle.connected()) {                                     // Publish result if feeling verbose
    Particle.publish("Duration",wateringDurationStr, PRIVATE);
  }
  return 1;                                                            // Returns 1 to let the user know if was reset
}

 /**
  * @brief Simple Function to construct the strings that make the console easier to read
  * 
  * @details Looks at all the system setting values and creates the appropriate strings.  Note that this 
  * is a little inefficient but it cleans up a fair bit of code.
  * 
  */
void makeUpStringMessages() {

  if (sysStatus.wakeTime == 0 && sysStatus.sleepTime == 24) {                         // Special case for 24 hour operations
    snprintf(wakeTimeStr, sizeof(wakeTimeStr), "NA");
    snprintf(sleepTimeStr, sizeof(sleepTimeStr), "NA");
  }
  else {
    snprintf(wakeTimeStr, sizeof(wakeTimeStr), "%i:00", sysStatus.wakeTime);           // Open and Close Times
    snprintf(sleepTimeStr, sizeof(sleepTimeStr), "%i:00", sysStatus.sleepTime);
  }

  // Watering Strings
  snprintf(wateringDurationStr,sizeof(wateringDurationStr),"%isec",sysStatus.wateringDuration);
  snprintf(wateringThresholdPctStr,sizeof(wateringThresholdPctStr),"%2.1f %%",sysStatus.wateringThresholdPct);
  snprintf(heatThresholdStr,sizeof(heatThresholdStr),"%2.1f %%",sysStatus.heatThreshold);

  return;
}