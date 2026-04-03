import time
import synnax as sy
import paho.mqtt.client as mqtt


# CLOSED IS "MOSFET OFF" (LOW POWER / NO CURRENT)
# OPEN IS "MOSFET ON" (HIGH POWER / CURRENT / ACTUATION)
CLOSED = 0
OPEN = 1


############################################
######## FLIGHT SYSTEM CONFIGURATIONS ######
############################################


NUM_CHANNELS = 7
SAFE_STATES = [
    CLOSED,  #1 - Abort
    CLOSED,  #2 - QD
    CLOSED,  #3 - Vent
    CLOSED,  #4 - Ignite
    CLOSED,  #5 - Fill
    CLOSED,  #6 - Dump
    CLOSED,  #7 - MPV
]


PROTECTED_CHANNELS = [4, 7]  # MPV, Ignite 1


############################################
######## MQTT CONFIGURATIONS (NEW CONTROLS BOX) ######
############################################


# ←←← CHANGE THIS IP WHEN YOUR BROKER IS RUNNING ←←←
MQTT_BROKER = "192.168.0.100"
MQTT_PORT = 1883
MQTT_TOPIC = "switchbox/commands"


# ←←← SET THIS TO True ONLY WHEN YOU HAVE THE BROKER RUNNING ←←←
MQTT_ENABLED = True
#IMPORTANT!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!! - Change to True when using MQTT


# Mapping from Synnax channel → position in the 10-byte packet
MAP_SYN_TO_PACKET = {
    1: 1,  # Abort
    2: 2,  # QD
    3: 3,  # Vent
    4: 4,  # IgniteM
    5: 5,  # Fill
    6: 6,  # Dump
    7: 7,  # MPV
}


##############################
##############################
LOOP = sy.Loop(sy.Rate.HZ * 7.5)


# keep track of states of each Synnax channel
channel_states = SAFE_STATES.copy()


# Connect to the Synnax server
client = sy.Synnax(
    host="localhost",
    port=9091,
    username="synnax",
    password="seldon",
    secure=False
)


# Create VIRTUAL command channels
cmd_channels = [
    sy.Channel(name=f"controls_cmd_{i + 1}", data_type=sy.DataType.UINT8, virtual=True)
    for i in range(NUM_CHANNELS)
]
cmd_channels = client.channels.create(cmd_channels, retrieve_if_name_exists=True)


# State channels
state_time = client.channels.create(
    name="controls_state_time", is_index=True,
    data_type=sy.DataType.TIMESTAMP, retrieve_if_name_exists=True
)
state_channels = [
    sy.Channel(name=f"controls_state_{i + 1}", data_type=sy.DataType.UINT8, index=state_time.key)
    for i in range(NUM_CHANNELS)
]
state_channels = client.channels.create(state_channels, retrieve_if_name_exists=True)


ARMED = 0
arm_time = client.channels.create(name="controls_arm_time", is_index=True,
                                  data_type=sy.DataType.TIMESTAMP, retrieve_if_name_exists=True)
arm_cmd = client.channels.create(name="controls_arm_cmd", data_type=sy.DataType.UINT8,
                                 virtual=True, retrieve_if_name_exists=True)
arm_state = client.channels.create(name="controls_arm_state", data_type=sy.DataType.UINT8,
                                   index=arm_time.key, retrieve_if_name_exists=True)


# MQTT client (will stay None if MQTT_ENABLED = False)
mqtt_client = None




def build_packet(states):
    packet_list = ['0'] * 10
    packet_list[0] = 'A'
    packet_list[9] = 'Z'
    packet_list[1] = '0'          # abortValve always safe for now


    for syn_ch, packet_idx in MAP_SYN_TO_PACKET.items():
        if 1 <= syn_ch <= NUM_CHANNELS:
            packet_list[packet_idx] = str(states[syn_ch - 1])


    packet_list[8] = str(ARMED)


    return "".join(packet_list)




def send_mqtt_command(states):
    """Prints the packet every time and only sends if MQTT_ENABLED"""
    packet = build_packet(states)
    print(f"Packet ready: {packet}")


    if not MQTT_ENABLED:
        # print("   → MQTT disabled for testing (no actual send)") # Muted to prevent spam at 7.5Hz
        return False


    global mqtt_client
    if mqtt_client is None or not mqtt_client.is_connected():
        # print("   → MQTT not connected (no actual send)") # Muted to prevent spam at 7.5Hz
        return False


    try:
        mqtt_client.publish(MQTT_TOPIC, packet, qos=0)
        # print("   → Sent successfully over MQTT") # Muted to prevent spam at 7.5Hz
        return True
    except Exception as e:
        print(f"   → Publish failed: {e}")
        return False




def main():
    global ARMED
    print("Setting initial safe states...")
    send_mqtt_command(SAFE_STATES)


    with client.open_streamer([f"controls_cmd_{i + 1}" for i in range(NUM_CHANNELS)] + ["controls_arm_cmd"]) as reader:
        with client.open_writer(
            start=sy.TimeStamp.now(),
            channels=[f"controls_state_{i + 1}" for i in range(NUM_CHANNELS)] +
                     ["controls_state_time", "controls_arm_time", "controls_arm_state"],
            enable_auto_commit=True
        ) as writer:


            # Write initial safe states
            frame = {"controls_state_time": sy.TimeStamp.now()}
            for i, state in enumerate(channel_states):
                frame[f"controls_state_{i+1}"] = state
            writer.write(frame)


            print("\n" + "="*60)
            print("READY — Flip any valve in the Synnax console")
            print("Sending continuous 10-byte packets at 5Hz.")
            print("="*60 + "\n")


            while LOOP.wait():
                # Read with a 0 timeout so we don't block the 7.5Hz loop waiting for data
                frame = reader.read(timeout=0)
                
                # Only update states if we actually received new commands
                if frame is not None:
                    commands = channel_states.copy()
                    for i in range(NUM_CHANNELS):
                        ch = frame[f"controls_cmd_{i + 1}"]
                        if len(ch) > 0:
                            commands[i] = int(ch[-1])


                    arm_ch = frame["controls_arm_cmd"]
                    if len(arm_ch) > 0:
                        ARMED = int(arm_ch[-1])


                    # MPV requires ARMED
                    if not ARMED:
                        commands[6] = SAFE_STATES[6]


                    # IGNITE requires BOTH ARMED and MPV open
                    if not ARMED or commands[6] != OPEN:
                        commands[3] = SAFE_STATES[3]


                    channel_states[:] = commands[:]


                # --- THIS NOW HAPPENS EVERY CYCLE ---
                
                # Send the current known state via MQTT
                send_mqtt_command(channel_states)


                # Write the current known state back to Synnax
                out_frame = {"controls_state_time": sy.TimeStamp.now()}
                for i, state in enumerate(channel_states):
                    out_frame[f"controls_state_{i+1}"] = state
                writer.write(out_frame)


                out_arm_frame = {
                    "controls_arm_time": sy.TimeStamp.now(),
                    "controls_arm_state": ARMED
                }
                writer.write(out_arm_frame)




if __name__ == "__main__":
    print("=" * 70)
    print("Synnax → Controls Box TEST MODE")
    print("MQTT_ENABLED =", MQTT_ENABLED)
    if MQTT_ENABLED:
        print(f"Will try to connect to {MQTT_BROKER}:{MQTT_PORT}")
    else:
        print("MQTT is DISABLED")
    print("=" * 70)


    # Only try to connect if enabled
    if MQTT_ENABLED:
        mqtt_client = mqtt.Client(client_id="SynnaxControls")
        connected = False
        while not connected:
            try:
                mqtt_client.connect(MQTT_BROKER, MQTT_PORT, keepalive=60)
                mqtt_client.loop_start()
                print(f"✅ Connected to MQTT broker at {MQTT_BROKER}")
                connected = True
            except Exception as e:
                print(f"⚠️ Connection failed: {e}")
                print("   Retrying in 3 seconds... (Ctrl+C to quit)")
                time.sleep(3)


    print("Setup complete.\n")
    try:
        main()
    except KeyboardInterrupt:
        print("\nShutting down...")
        if mqtt_client is not None:
            mqtt_client.loop_stop()
            mqtt_client.disconnect()
