/**
 * @file particle_fn.h
 * @author Chip McClelland (chip@seeinisghts.com)
 * @brief This file initilizes the Particle functions and variables needed for control from the console / API calls
 * @version 0.1
 * @date 2022-06-30
 * 
 */

// Particle functions
#ifndef PARTICLE_FN_H
#define PARTICLE_FN_H

#include "Particle.h"
#include "storage_objects.h"
#include "take_measurements.h"

// Variables
extern char currentPointRelease[6];
extern char wateringThresholdPctStr[8];
extern char heatThreshold[8];
extern char wateringDurationStr[16];
extern char wakeTimeStr[8];
extern char sleepTimeStr[8];

void particleInitialize();
int setWakeTime(String command); 
int setSleepTime(String command);
int setEnableSleep(String command);
int setWaterDuration(String command);
int setWaterThreshold(String command);
int setHeatThreshold(String command);
void makeUpStringMessages();

#endif