// Ryan Joseph Talalai
// CMPEN 431 FA22

#include <iostream>
#include <sstream>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <sys/stat.h>
#include <unistd.h>
#include <algorithm>
#include <fstream>
#include <map>
#include <math.h>
#include <fcntl.h>
#include <vector>
#include <iterator>

#include "431project.h"

using namespace std;

/*
 * Enter your PSU IDs here to select the appropriate scanning order.
 */
#define PSU_ID_SUM (979387969)

// 979387969 mod 24 = 1

// Index numbers of exploration categories in respective order
		
// BP -------------> CACHE --------------------------------------> FPU ---> CORE		[based on id number]
// 12 -> 13 -> 14 -> 2 -> 3 -> 4 -> 5 -> 6 -> 7 -> 8 -> 9 -> 10 -> 11 ----> 0 -> 1

/*
 * Some global variables to track heuristic progress.
 * 
 * Feel free to create more global variables to track progress of your
 * heuristic.
 */
 
unsigned int currentlyExploringDim = 0;
bool isDSEComplete = false;
bool firstConfig = true;

bool traversalList[15] = { false, false, false, false, false, false, false, false, false, false, false, false, false, false, false };
bool finishedState[15] = { true, true, true, true, true, true, true, true, true, true, true, true, true, true, true };
int orderMap[15] = { 12,13,14,2,3,4,5,6,7,8,9,10,11,0,1 };		// BP -> CACHE -> FPU -> CORE


/*
 * Given a half-baked configuration containing cache properties, generate
 * latency parameters in configuration string. You will need information about
 * how different cache paramters affect access latency.
 * 
 * Returns a string similar to "1 1 1"
 */
std::string generateCacheLatencyParams(string halfBackedConfig) {

	std::stringstream latency;
	
	int il1size = getil1size(halfBackedConfig);
	int dl1size = getdl1size(halfBackedConfig);
	int ul2size = getl2size(halfBackedConfig);
	
	int il1assoc_index = extractConfigParam(halfBackedConfig, 6);
	int dl1assoc_index = extractConfigParam(halfBackedConfig, 4);
	int ul2assoc_index = extractConfigParam(halfBackedConfig, 9);
	
	int il1_late = log2(il1size/1024) + il1assoc_index - 1;
	int dl1_late = log2(dl1size/1024) + dl1assoc_index - 1;
	int ul2_late = log2(ul2size/1024) + ul2assoc_index - 5;

	latency << dl1_late << " " << il1_late << " " << ul2_late;

	return latency.str();
}

/*
 * Returns 1 if configuration is valid, else 0
 */
int validateConfiguration(std::string configuration) {

	int il1size = getdl1size(configuration);
	int il1block_size = 8 << extractConfigParam(configuration, 2);
	int ifq = (1 << extractConfigParam(configuration, 0)) * 8;
	int dl1size = getil1size(configuration);
	int ul2size = getl2size(configuration);
	int ul2block_size = 16 << extractConfigParam(configuration, 8);	

	if(il1size < 2048 || 65536 < il1size)
		return 0;
	
	if(il1block_size < ifq)
		return 0;
	
	if(dl1size < 2048 || 65536 < dl1size)
		return 0;
	
	if(ul2size < (il1size + dl1size) * 2)
		return 0;
	
	if(ul2size < 32768 || 1048576 < ul2size)
		return 0;
	
	if(ul2block_size < il1block_size * 2)
		return 0;
	
	return isNumDimConfiguration(configuration);
}

/*
 * Given the current best known configuration, the current configuration,
 * and the globally visible map of all previously investigated configurations,
 * suggest a previously unexplored design point. You will only be allowed to
 * investigate 1000 design points in a particular run, so choose wisely.
 *
 * In the current implementation, we start from the leftmost dimension and
 * explore all possible options for this dimension and then go to the next
 * dimension until the rightmost dimension.
 */
std::string generateNextConfigurationProposal(std::string currentconfiguration,
		std::string bestEXECconfiguration, std::string bestEDPconfiguration,
		int optimizeforEXEC, int optimizeforEDP) {

	//
	// Some interesting variables in 431project.h include:
	//
	// 1. GLOB_dimensioncardinality
	// 2. GLOB_baseline
	// 3. NUM_DIMS
	// 4. NUM_DIMS_DEPENDENT
	// 5. GLOB_seen_configurations
	
	std::string nextconfiguration = currentconfiguration;
	// Continue if proposed configuration is invalid or has been seen/checked before.
	while (!validateConfiguration(nextconfiguration) ||
		GLOB_seen_configurations[nextconfiguration]) {

		// Check if DSE has been completed before and return current
		// configuration.
		if(isDSEComplete) {
			return currentconfiguration;
		}

		std::stringstream ss;

		string bestConfig;
		if (optimizeforEXEC == 1)
			bestConfig = bestEXECconfiguration;

		if (optimizeforEDP == 1)
			bestConfig = bestEDPconfiguration;

		// Fill in the dimensions already-scanned with the already-selected best
		// value.
		for (int dim = 0; dim < orderMap[currentlyExploringDim]; ++dim) {
			ss << extractConfigParam(bestConfig, dim) << " ";
		}

	
		// Handling for currently exploring dimension. This is a very dumb
		// implementation.
		int nextValue = extractConfigParam(nextconfiguration, orderMap[currentlyExploringDim]) + 1;
		
		if (firstConfig) {
			nextValue = 0;
			firstConfig = false;
		}

		if (nextValue >= GLOB_dimensioncardinality[orderMap[currentlyExploringDim]]) {
			nextValue = GLOB_dimensioncardinality[orderMap[currentlyExploringDim]] - 1;
			traversalList[orderMap[currentlyExploringDim]] = true;
			firstConfig = true;
		}
		
		ss << nextValue << " ";
		
		// Fill in remaining independent params with 0.
		for (int dim = (orderMap[currentlyExploringDim] + 1);
				dim < (NUM_DIMS - NUM_DIMS_DEPENDENT); ++dim) {
			ss << extractConfigParam(bestConfig, dim) << " ";;
		}

		//
		// Last NUM_DIMS_DEPENDENT3 configuration parameters are not independent.
		// They depend on one or more parameters already set. Determine the
		// remaining parameters based on already decided independent ones.
		//
		string configSoFar = ss.str();

		// Populate this object using corresponding parameters from config.
		ss << generateCacheLatencyParams(configSoFar);

		// Configuration is ready now.
		nextconfiguration = ss.str();
		
		// Make sure we start exploring next dimension in next iteration.
				
		if (traversalList[orderMap[currentlyExploringDim]]) {
			currentlyExploringDim++;
		}

		isDSEComplete = true;
		for(int i = 0; i < 15; i++){
			if(traversalList[i] != finishedState[i]){
				isDSEComplete = false;
			}
		}
	}
	return nextconfiguration;
}