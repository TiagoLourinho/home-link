import asyncio
import threading
import matplotlib.pyplot as plt
import matplotlib.animation as animation
from asyncua import Client

OPC_UA_URL = "opc.tcp://localhost:4840"
NODE_ID = "ns=1;s=Temperature"

timestamps = []
temperatures = []
latest_temp = None


async def poll_temperature():
    global latest_temp
    async with Client(url=OPC_UA_URL) as client:
        print(f"Connected to OPC-UA server at {OPC_UA_URL}")
        node = client.get_node(NODE_ID)
        while True:
            latest_temp = await node.read_value()
            print(f"Temperature received in python monitor: {latest_temp}")
            await asyncio.sleep(1)


def opcua_thread():
    asyncio.run(poll_temperature())


def main():
    thread = threading.Thread(target=opcua_thread, daemon=True)
    thread.start()

    fig, ax = plt.subplots()
    ax.set_title("Live Temperature")
    ax.set_xlabel("Sample")
    ax.set_ylabel("Temperature (°C)")
    ax.grid(True, linestyle="--", alpha=0.7)
    (line,) = ax.plot([], [], "b-o", markerfacecolor="red", markersize=10)

    def update(frame):
        if latest_temp is not None:
            temperatures.append(latest_temp)
            timestamps.append(len(temperatures))
            line.set_data(timestamps, temperatures)
            ax.relim()
            ax.autoscale_view()
        return (line,)

    anim = animation.FuncAnimation(fig, update, interval=1000, cache_frame_data=False)
    plt.show()


if __name__ == "__main__":
    main()
