import time
import math
import canopen

OD = "src/temp_sensor.eds"
NODE_ID = 1
PARAMETER_NAME = "Temperature Value"


def main():
    network = canopen.Network()
    network.connect(channel="vcan0", bustype="socketcan")

    node = canopen.LocalNode(NODE_ID, OD)
    network.add_node(node)

    print(f"Temperature sensor started on node ID {NODE_ID:#04x}")

    t = 0
    try:
        while True:
            temperature = 22.0 + 4.0 + math.sin(t)

            # Encode as integer (multiply by 10 to keep 1 decimal of precision)
            raw = int(temperature * 10)
            data = raw.to_bytes(2, byteorder="little", signed=True)

            node.sdo[PARAMETER_NAME].raw = raw  # type: ignore (done to keep the OD consistent)
            network.send_message(0x180 + NODE_ID, data)

            print(f"Sent temperature: {temperature:.1f} °C")
            t += 0.1
            time.sleep(1)
    except KeyboardInterrupt:
        print("Stopping sensor")
    finally:
        network.disconnect()


if __name__ == "__main__":
    main()
