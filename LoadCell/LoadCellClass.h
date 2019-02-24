#pragma once

#include "Config.h"
#include <NIDAQmx.h>
#include <ftconfig.h>
//#include <DspFilters\Dsp.h>

#include <array>



#ifndef DEBUG
#define DEBUG false
#endif

#ifndef FILENAME
#define FILENAME (strrchr(__FILE__, '\\') \
  ? strrchr(__FILE__, '\\') + 1 : __FILE__)
#endif

#ifndef DBGVAR
#define DBGVAR(os, var) \
  if (DEBUG) (os) << "DBG " << FILENAME << "(" << __LINE__ << ") "\
    << __FUNCTION__ << #var << ": " << (var) << "\n"
#endif

#ifndef DBGPRINT
#define DBGPRINT(M, ...) \
  fprintf(stderr, "DBG %s(%d) " M "\n", \
    FILENAME, __LINE__, ##__VA_ARGS__)
#endif

#define DAQmxErrChk(functionCall) if (DAQmxFailed(error = (functionCall))) goto Error;


template<size_t stride, size_t N, class T, size_t count = N / stride>
std::array<T*, count> make_2d(T *raw)
{
	std::array<T*, count> retval;
	for (size_t i = 0; i < count; ++i)
		retval[i] = raw + i*stride;
	return retval;
}


struct CallbackPackage
{
	float64 *buffer;
	float *voltages;
	float* raw_voltages;
	bool filtering_enabled;
	//Dsp::SimpleFilter <Dsp::Butterworth::LowPass<FILTER_ORDER_NUMBER>, DOF>* filter;
	bool channel_averaging_enabled;
};



class LoadCellClass
{
public:
	//Constructs an instance of the class
	LoadCellClass();


	//Destrcuts an instance of the class
	~LoadCellClass();



	// Calibrates, transforms, starts and biases in that order.
	// The equivalent of calling the 4 functions:
	//   SetCalibration(calibration_path)
	//   SetTransformation(transformation)
	//   Start(sample_rate, channel)
	//   Bias()
	// Parameters:
	//   calibration_file_path: the name and path of the calibration file
	//   transformation: displacements and rotations in the order
	//                     Dx, Dy, Dz, Rx, Ry, Rz
	//   sample_rate: sampling rate (Hz)
	//   channel: hardware channel e.g. "Dev1/ai0:5"
	//   bias: set bias to current voltage readings
	// Return Values:
	//   0: SUCCESS
	//   1: ERROR_CALIBRATION
	//   2: ERROR_DAQ_START
	//   3: ERROR_TRANSFORMATION
	int Initialize(const std::string calibration_path = "",
		const float* transformation = NULL,
		const float sample_rate = 0,
		const std::string channel = "",
		const bool set_filter = true);



	// Starts the DAQmx device
	// Parameters:
	//   sample_rate: sampling rate (Hz)
	//   channel: hardware channel e.g. "Dev1/ai0:5"
	// Return Values:
	//   0: SUCCESS
	//   2: ERROR_DAQ_START
	int StartDAQmx(const float sample_rate, const std::string channel);



	// Stops the DAQmx device
	void StopDAQmx();



	// Set double butterworth filter parameters
	// Parameters:
	//   order_number: 3 (default)
	//   cutoff_freq: 50 (default)
	//   enable_filter: Activates filtering
	void SetFilter(unsigned int order_number = 3,
		unsigned int cutoff_freq = 50,
		bool enable_filter = true);

	// Enables filtering for load cell values
	// Parameters:
	//   state: true == ON (default), false == OFF
	void EnableFilter(bool state = true);



	// Loads calibration info for a transducer. Units are N, and N-m.
	// Parameters:
	//   calibration_file_path: the name and path of the calibration file
	// Return Values:
	//   0: SUCCESS
	//   1: ERROR_CALIBRATION
	int SetCalibration(const std::string calibration_file_path);



	// Sets voltages with raw voltage load cell values.
	// Parameters:
	//   voltages: array of raw voltage values (6 elements)
	// Return Values:
	//   0: SUCCESS
	//   4: ERROR_DAQ_NOT_STARTED
	int GetVoltages(float *voltages) const;
	int GetRawVoltages(float* raw_voltages) const;



	// Converts the raw voltage readings into forces and torques which are then
	// set in the input parameter loads (6 elements).
	// Loads are set via:
	//   Fx, Fy, Fz, Mx, My, Mz
	// Parameters:
	//   loads: array of force-torque values (6 elements)
	// Return Values:
	//   0: SUCCESS
	//   4: ERROR_DAQ_NOT_STARTED
	int GetLoads(float *loads);
	int GetRawLoads(float *loads);



	// Performs a 6-axis translation/rotation on the transducer's coordinate
	// system. First the translation is applied using the load cell's native
	// coordinate system. Then a XYZ Euler rotation sequence is applied.
	// Translation units are in mm and rotation units are in deg.
	// Note:
	//   Rx is the rotation about the X axis also known as Gamma.
	// Parameters:
	//   transform: displacements and rotations in the order
	//              Dx, Dy, Dz, Rx, Ry, Rz
	// Return Values:
	//   0: SUCCESS
	//   3: ERROR_TRANSFORMATION
	int SetTransformation(const float* transform);



	// Stores a voltage reading to be subtracted from subsequent readings,
	// effectively "zeroing" the transducer output.
	// Uses the current voltage reading as the bias values.
	// Return Values:
	//   0: Successs
	//   4: ERROR_DAQ_NOT_STARTED
	int SetBias();



	// Sets the input parameter bias to the recorded biased voltage.
	// Return Values:
	//   0: SUCCESS
	//   5: ERROR_BIAS_NOT_SET
	int GetBias(float* bias) const;



	// Gets the current sampling rate. 0 indicates not active.
	void GetSamplingRate(float& rate) const;



	// Set a lower bounded force threshold for handling electrical jitter.
	// Intended for use with "ExceedsLowerForceThreshold()".
	// Parameters:
	//  lower_threshold: array of absolute threshold load values.
	void SetLowerForceThreshold(const float* lower_threshold);



	// Check if any load exceed the lower threshold.
	// Returns:
	//   false: None of the forces exceeds the lower force threshold.
	//          If load cell has not been started or lower threshold has not
	//          been set returns false with an error message printed to stderr.
	//   true: Any of the forces exceeds the lower force threshold.
	bool ExceedsLowerForceThreshold();



	// Check if the load cell is currently operating.
	// Returns:
	//   true: There is a task running.
	//   false: No task running.
	bool IsActive();


private:

	// handles channel for DAQmx
	TaskHandle  task_handle_;
	// transducer properties read from calibration file
	Calibration *cal_;
	// handle for callback data package
	CallbackPackage callback_;
	// voltage buffer
	float64 buffer_[SAMPLES_PER_CHANNEL*DOF];
	// processed voltage values
	float voltages_[DOF];
	// raw voltages
	float raw_voltages_[DOF];
	// lower threshold to reduce jitter
	float lower_threshold_[DOF];
	// flag to indicate if lower threshold has been set
	bool lower_threshold_set_flag;
	// biased voltage value
	float bias_[DOF];
	// flag to indicate if bias has been set
	bool bias_set_flag;
	// sample rate (hz)
	float sample_rate_;
	// filter state
	bool filter_enabled_ = false;
	// double butterworth lowpass filter
	//Dsp::SimpleFilter <Dsp::Butterworth::LowPass<FILTER_ORDER_NUMBER>, DOF> filter_;
};