//Particle Functions
#include "Particle.h"
#include "sleep_helper_config.h"

// Battery conect information - https://docs.particle.io/reference/device-os/firmware/boron/#batterystate-
const char* batteryContext[7] = {"Unknown","Not Charging","Charging","Charged","Discharging","Fault","Diconnected"};

void sleepHelperConfig() {

    SleepHelper::instance()
        .withMinimumCellularOffTime(5min)                                                           // 
        .withMaximumTimeToConnect(11min)
        .withTimeConfig("EST5EDT,M3.2.0/02:00:00,M11.1.0/02:00:00")
        .withEventHistory("/usr/events.txt", "eh")
        .withDataCaptureFunction([](SleepHelper::AppCallbackState &state) {
            if (Time.isValid()) {

                delay(2000);

                takeMeasurements();                 // Collect data from the sensors

                if (current.wateringState == 1) {
                    char data[64];
                    Log.info("Sending webhook to start watering");
                    snprintf(data, sizeof(data), "{\"duration\":%i}",sysStatus.wateringDuration);
                    Particle.publish("Rachio-WaterGarden", data, PRIVATE);
                }

                SleepHelper::instance().addEvent([](JSONWriter &writer) {
                    writer.name("t").value((int) Time.now());
                    writer.name("bs").value(current.batteryState);
                    writer.name("c").value(current.internalTempC);
                    writer.name("sm").value(current.soilMoisture);
                    writer.name("st").value(current.soilTempC);
                    writer.name("ws").value(current.wateringState);
                });
            }
            return false;
        })
        .withSleepReadyFunction([](SleepHelper::AppCallbackState &, system_tick_t) {
            if (sysStatus.enableSleep) return false;// Boolean set by Particle.function - If sleep is enabled return false
            else return true;                       // If we need to delay sleep, return true
        })
        .withAB1805_WDT(ab1805)                     // Stop the watchdog before sleep or reset, and resume after wake
        .withPublishQueuePosixRK()                  // Manage both internal publish queueing and PublishQueuePosixRK
        ;

    // Full wake and publish
    // Every 60 minutes from 5:00 AM to 10:00 PM local time 
    SleepHelper::instance().getScheduleFull()
        .withMinuteOfHour(60, LocalTimeRange(LocalTimeHMS("05:00:00"), LocalTimeHMS("21:59:59")));

    // Data capture every 15 minutes during these same hours 
    SleepHelper::instance().getScheduleDataCapture()
        .withMinuteOfHour(15, LocalTimeRange(LocalTimeHMS("05:00:00"), LocalTimeHMS("21:59:59")));
}
