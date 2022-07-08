//Particle Functions
#include "Particle.h"
#include "take_measurements.h"
#include <math.h>

FuelGauge fuelGauge;                                // Needed to address issue with updates in low battery state 

char internalTempStr[16] = " ";                       // External as this can be called as a Particle variable
char soilTempStr[16] = " ";                           // External as this can be called as a Particle variable
char soilMoistureStr[16] = " ";                       // External as this can be called as a Particle variable
char signalStr[64] = " ";

/**
 * @brief This code collects temperature data from the TMP-36
 * 
 * @details Uses an analog input and the appropriate scaling
 * 
 * @returns Returns true if succesful and puts the data into the current object
 * 
 */
bool takeMeasurements() { 

    digitalWrite(SOIL_POWER_PIN, HIGH);             // Power up the soil sensor
    delay(100);                                     // Recommendation from Jay Ham at CSU - have not tested to see if it can be shorter

    // Temperature inside the enclosure
    current.internalTempC = tmp36TemperatureC(analogRead(TMP36_SENSE_PIN));
    snprintf(internalTempStr,sizeof(internalTempStr), "%4.2f C", current.internalTempC);
    Log.info("Internal Temperature is %s",internalTempStr);

    // Soil Temperature
    current.soilTempC = soilTemperarureC(analogRead(SOIL_TEMP_PIN));
    snprintf(soilTempStr,sizeof(soilTempStr), "%4.2f C", current.soilTempC);
    Log.info("Soil Temperature is %s",soilTempStr);

    // Soil Moisture
    current.soilMoisture = map(analogRead(SOIL_MOISTURE_PIN),0,3722,0,100);        // Sensor puts out 0-3V for 0% to 100% soil moisuture
    snprintf(soilMoistureStr,sizeof(soilMoistureStr), "%4.2f%%", current.soilMoisture);
    Log.info("Soil Moisture is %s",soilMoistureStr);

    digitalWrite(SOIL_POWER_PIN, LOW);              // Analog measurements complete power down the soil sensor

    if (current.soilMoisture < sysStatus.wateringThresholdPct) current.wateringState = 1;         // If the soil is too dry, water
    else if (current.soilTempC > 38) current.wateringState = 1;                                   // If the soil it too hot, water           
    else current.wateringState = 0;                                                               // Else, don't

    batteryState();

    isItSafeToCharge();

    getSignalStrength();

    return 1;

}

/**
 * @brief tmp36TemperatureC
 * 
 * @details generic code that convers the analog value of the TMP-36 sensors to degrees c - Assume 3.3V Power
 * 
 * @returns Returns the temperature in C as a float value
 * 
 */
float tmp36TemperatureC (int adcValue) { 
    // Analog inputs have values from 0-4095, or
    // 12-bit precision. 0 = 0V, 4095 = 3.3V, 0.0008 volts (0.8 mV) per unit
    // The temperature sensor docs use millivolts (mV), so use 3300 as the factor instead of 3.3.
    float mV = ((float)adcValue) * 3300 / 4095;

    // According to the TMP36 docs:
    // Offset voltage 500 mV, scaling 10 mV/deg C, output voltage at 25C = 750 mV (77F)
    // The offset voltage is subtracted from the actual voltage, allowing negative temperatures
    // with positive voltages.

    // Example value=969 mV=780.7 tempC=28.06884765625 tempF=82.52392578125

    // With the TMP36, with the flat side facing you, the pins are:
    // Vcc | Analog Out | Ground
    // You must put a 0.1 uF capacitor between the analog output and ground or you'll get crazy
    // inaccurate values!
    return (mV - 500) / 10;
}

/**
 * @brief Soil Temperarure calculation from the soil moisture probe
 * 
 * @details Uses a thermistor based sensor on the Colorado State Soil sensor - Assume 3.3V power
 * Analog read value is passed to the function - needs to be converted to voltage.
 * 
 * @returns Returns the soil temperature in C
 * 
 */
float soilTemperarureC (int adcValue) {
  // c1, c2, c3 are calibration coefficients for a particular thermistor
  float Vex = 2500.0;                               // circuit excitation in mV
  float Vo = (((float)adcValue) * 3300 / 4095);
  float c1 = 0.901747748E-03, c2=2.489190310E-04, c3 = 2.043213857E-07; // Murata NCP18XH103F03RB
  float logRt,Rt,T;
  float R = 10000.0;                                // fixed resistor, 10K

  Rt=R/(Vex/Vo-1);            // calc thermistor resistance, therm next to gnd
  
  if (Rt>0) {
    logRt = log(Rt);                                //calc log of R
    T = ( 1.0 / (c1 + c2*logRt + c3*logRt*logRt*logRt ) ) - 273.15; // Steinhart Hart, 3nd order, Celcius
  } 
  else T=-99.9;                                     // Invalid Reading

  delay(1000);
  Log.info("Vo: %4.2f mV  Rt= %4.2f mV  T = %4.2f", Vo, Rt, T);

  return T;
}


/**
 * @brief In this function, we will measure the battery state of charge and the current functional state
 * 
 * @details One factor that is an issue today is the accurace of the state of charge if the device is waking
 * from sleep.  In order to help with this, there is a test for enable sleep and an additional delay.
 * 
 * @return true  - If the battery has a charge over 60%
 * @return false - Less than 60% indicates a low battery condition
 */
bool batteryState() {
    current.batteryState = System.batteryState();                      // Call before isItSafeToCharge() as it may overwrite the context

  if (sysStatus.enableSleep) {                                        // Need to take these steps if we are sleeping
    fuelGauge.quickStart();                                            // May help us re-establish a baseline for SoC
    delay(500);
  }

  current.stateOfCharge = int(fuelGauge.getSoC());                   // Assign to system value

  if (current.stateOfCharge > 60) return true;
  else return false;
}

/**
 * @brief Checks to see if the temperature is in the range to support charging
 * 
 * @details Will enable or disable charging based on the current temperature
 * 
 * @link https://batteryuniversity.com/learn/article/charging_at_high_and_low_temperatures @endlink
 * 
 */
bool isItSafeToCharge()                             // Returns a true or false if the battery is in a safe charging range.
{
  PMIC pmic(true);
  if (current.internalTempC < 0 || current.internalTempC > 37 )  {  // Reference: (32 to 113 but with safety)
    pmic.disableCharging();                         // It is too cold or too hot to safely charge the battery
    current.batteryState = 1;                       // Overwrites the values from the batteryState API to reflect that we are "Not Charging"
    return false;
  }
  else {
    pmic.enableCharging();                          // It is safe to charge the battery
    return true;
  }
}

/**
 * @brief Get the Signal Strength values and make up a string for use in the console
 * 
 * @details Provides data on the signal strength and quality
 * 
 */
void getSignalStrength() {
  const char* radioTech[10] = {"Unknown","None","WiFi","GSM","UMTS","CDMA","LTE","IEEE802154","LTE_CAT_M1","LTE_CAT_NB1"};
  // New Signal Strength capability - https://community.particle.io/t/boron-lte-and-cellular-rssi-funny-values/45299/8
  CellularSignal sig = Cellular.RSSI();

  auto rat = sig.getAccessTechnology();

  //float strengthVal = sig.getStrengthValue();
  float strengthPercentage = sig.getStrength();

  //float qualityVal = sig.getQualityValue();
  float qualityPercentage = sig.getQuality();

  snprintf(signalStr,sizeof(signalStr), "%s S:%2.0f%%, Q:%2.0f%% ", radioTech[rat], strengthPercentage, qualityPercentage);
}