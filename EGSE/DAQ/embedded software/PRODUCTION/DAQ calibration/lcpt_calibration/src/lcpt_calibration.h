#ifndef CALIBRATOR_H
#define CALIBRATOR_H

#include <vector>

//Functions
//Computes and prints best fit
void computeBestFit(std::vector<float> yValsT);
//Computes best fit and assigns values to m and b
void computeBestFit(float& m, float& b);
//Starts calibration process
void startCalibration();
//Resets xVals and yVals
void clear();
//Removes most recent xVal and yVal
void removeLatest();
//Removes first occurent of xVal in xVals and its corresponding yVal
bool remove(float xVal);
//Adds xVal and yVal to the end of xVals and yVals
void add(float xVal, float yVal, float yVal2, float yVal3, float yVal4);
//Returns the size of xVals and yVals
int size();
//Prints the size
void sizePrint();
//Prints a table of the values
void printTable(std::vector<float> yValsT);
//Sorts the xVals into numerical order, mvoes appropriate yVals with their corresponding xVals
void sort();

#endif // CALIBRATOR_H
