#pragma once
#include "HDLDevice.hpp"
#include "HDLDevicePort.hpp"
#include "HDLPortType.hpp"
#include "HDLTiming.hpp"
#include <iostream>
#include <string>
#include <vector>
using namespace std;

namespace ElasticC {
namespace HDLGen {
struct ClockInfo {
public:
  double frequency = 50e6; // frequency in Hz
  double phase = 0;        // phase in degrees

  // Get position of first rising edge in seconds
  double GetRisingEdgePos();
  double GetFallingEdgePos();
};

class HDLSignal {
public:
  HDLSignal(string _name, HDLPortType *_type);
  string name;
  HDLPortType *sigType;
  vector<HDLDevicePort *> connectedPorts;
  ClockInfo clockInfo; // clock type signals only

  // Connect another signal to this one, replacing all instances of this signal
  // with the passed one
  void ConnectToSignal(HDLSignal *other);
  // Connect to a device port
  void ConnectToPort(HDLDevicePort *port);
  // Generate a VHDL signal definition
  void GenerateVHDL(ostream &vhdl);

  // Timing and latency data
  HDLSignal *clockDomain = nullptr;
  // Clock to valid delay in ns
  HDLTimingValue<double> timing_delay;
  // Pipeline latency from input to signal
  HDLTimingValue<int> pipeline_latency;
  // Force no pipelining to occur to this signal
  bool dont_pipeline = false;
};

struct HDLBitSlice {
  HDLBitSlice(int _high, int _low) : high(_high), low(_low){};
  int low, high;
  inline int width() { return (high - low) + 1; }
};

} // namespace HDLGen
} // namespace ElasticC
