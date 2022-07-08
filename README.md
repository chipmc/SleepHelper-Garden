# SleepHelper-Garden

A useful tool to keep your garden watered.  I have a zone in my Rachio watering system dedicated to my garden.  The Boron will send a webhook to the Rachio system using their API to initiate a watering session for the plants.

See full description on the Particle Community Forum: https://community.particle.io/t/first-application-for-sleephelper-garden-watering/62863

 To keep the main application manageable, I have broken out configuration into focused files
 1) device_pinout - Pin definitions and connections for your device
 2) storage_object - Define what variables are needed to capture the sustem and current status
 3) take_measurements - This is the set of activities executed each time the device wakes
 4) sleep_helper_config - Define the sleep / wake / report cycle - the full behaviour of your device
 5) particle_fn - For any particle variables / functions that you want to expose in the console

* Revision history
* v0.01 - Began with the generic Sleep-Helper-Demo code
* v0.02 - Initial funtional code - to start long term testing of this codebase
* v0.03 - Added solar power configuration
