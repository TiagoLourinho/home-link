#include <atomic>
#include <cerrno>
#include <csignal>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <thread>

// SocketCAN
#include <linux/can.h>
#include <linux/can/raw.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <unistd.h>

// OPC-UA
#include <open62541/server.h>
#include <open62541/server_config_default.h>

// CANopen
static constexpr int SENSOR_ID = 1;
static constexpr const char *PARAMETER_NAME = "Temperature";
static constexpr int TPDO1_ID = 0x180 + SENSOR_ID;

std::atomic<bool> running(true);

void signal_handler(int signal) {
  std::cout << "\nShutting down..." << std::endl;
  running = false;
}

int setup_can_socket(const char *interface) {
  int sock = socket(PF_CAN, SOCK_RAW, CAN_RAW);
  if (sock < 0) {
    std::cerr << "Failed to create CAN socket" << std::endl;
    return -1;
  }

  struct ifreq ifr;
  std::strncpy(ifr.ifr_name, interface, IFNAMSIZ);
  ioctl(sock, SIOCGIFINDEX, &ifr);

  struct sockaddr_can addr {};
  addr.can_family = AF_CAN;
  addr.can_ifindex = ifr.ifr_ifindex;

  if (bind(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
    std::cerr << "Failed to bind CAN socket" << std::endl;
    return -1;
  }

  // Make it wake up periodically to check if still running
  struct timeval tv;
  tv.tv_sec = 1;
  tv.tv_usec = 0;
  setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

  return sock;
}

int main() {
  std::signal(SIGINT, signal_handler);

  // OPC-UA server setup
  UA_Server *server = UA_Server_new();
  UA_ServerConfig_setDefault(UA_Server_getConfig(server));

  // Add a variable node for temperature
  UA_VariableAttributes attr = UA_VariableAttributes_default;
  UA_Double temperature = 0.0;
  UA_Variant_setScalar(&attr.value, &temperature, &UA_TYPES[UA_TYPES_DOUBLE]);
  attr.displayName = UA_LOCALIZEDTEXT((char *)"en-US", (char *)PARAMETER_NAME);
  attr.dataType = UA_TYPES[UA_TYPES_DOUBLE].typeId;
  attr.accessLevel = UA_ACCESSLEVELMASK_READ;

  UA_NodeId nodeId = UA_NODEID_STRING(1, (char *)PARAMETER_NAME);
  UA_NodeId parentNodeId = UA_NODEID_NUMERIC(0, UA_NS0ID_OBJECTSFOLDER);
  UA_NodeId parentReferenceNodeId = UA_NODEID_NUMERIC(0, UA_NS0ID_ORGANIZES);
  UA_QualifiedName browseName = UA_QUALIFIEDNAME(1, (char *)PARAMETER_NAME);

  UA_Server_addVariableNode(
      server, nodeId, parentNodeId, parentReferenceNodeId, browseName,
      UA_NODEID_NUMERIC(0, UA_NS0ID_BASEDATAVARIABLETYPE), attr, NULL, NULL);

  // Run OPC-UA server in background thread
  std::thread server_thread([&]() {
    UA_Boolean ua_running = true;

    // Helper thread to check the global running
    std::thread stop_thread([&]() {
      while (running) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
      }
      ua_running = false;
    });

    UA_Server_run(server, &ua_running);
    stop_thread.join();
  });

  // CAN socket setup
  int can_sock = setup_can_socket("vcan0");
  if (can_sock < 0) {
    running = false;
    server_thread.join();
    return 1;
  }

  // Main loop
  struct can_frame frame;
  while (running) {
    int nbytes = read(can_sock, &frame, sizeof(frame));
    if (nbytes < 0) {
      if (errno == EAGAIN || errno == EWOULDBLOCK) {
        // Timeout, just check running and loop again
        continue;
      }
      std::cerr << "CAN read error" << std::endl;
      break;
    }

    if (frame.can_id == TPDO1_ID && frame.can_dlc >= 2) {
      int16_t raw;
      std::memcpy(&raw, frame.data, sizeof(raw));
      double temp =
          raw / 10.0; // Sensor multiplied by 10 to keep decimal precision

      std::cout << "Temperature received in C++ bridge: " << temp << " °C"
                << std::endl;

      // Update OPC-UA node
      UA_Variant value;
      UA_Variant_setScalar(&value, &temp, &UA_TYPES[UA_TYPES_DOUBLE]);
      UA_Server_writeValue(server, nodeId, value);
    }
  }

  running = false;
  server_thread.join();
  UA_Server_delete(server);
  close(can_sock);
  return 0;
}