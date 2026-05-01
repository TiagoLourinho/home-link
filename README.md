# home-link

Simulated home sensors communicating over CANopen and OPC-UA, with a C++ bridge and Python tooling on Linux

## Architecture

![Architecture diagram](casalink_architecture.svg)

A temperature sensor is simulated as a CANopen node broadcasting data over a
virtual CAN bus (`vcan0`). A C++ bridge service reads those frames and exposes
the values as OPC-UA nodes. A Python client connects to the OPC-UA server and
displays a live plot of the temperature over time.

| Layer       | Technology                            |
| ----------- | ------------------------------------- |
| Mock sensor | Python, `python-can`, `canopen`       |
| Virtual bus | Linux `vcan` kernel module            |
| Bridge      | C++, `socketcan`, `open62541`         |
| Live plot   | Python, `opcua-asyncio`, `matplotlib` |
