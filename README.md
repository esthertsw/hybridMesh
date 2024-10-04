# 🧱 Dependencies

Ensure the listed versions are installed, as there are breaking changes in the later versions, as of August 2024.

**Software**

* ArduinoIDE

**Arduino Libraries**

- RadioLib by Jan Gromes - v6.6.0
- ArduinoJson by Benoit Blanchon - v6.21.5
- PainlessMesh by Coopdis et. al. - v1.5.0

**ArduinoIDE Boards Managers**

- esp32 by Espressif Systems - v2.0.17

# 🖥️ Devices Used

- LILYGO® TTGO T-Beam (Semtech SX1262 LoRa transceiver)

# 🏁 How to Run

1. In hybridMesh.ino, modify the `myNodeName` variable's value for each of the devices in the mesh, giving them a unique 2 character ID.
2. `destNode` may be modified to direct messages from each device to a specific node of choice.
3. For one of the nodes, uncomment either lines 137-141 or 145-149 to allow for the device to send an initiating message within the mesh, through LoRa or Wi-Fi.
4. Flash the devices through ArduinoIDE.
5. Messages received or sent can be monitored through the serial terminal at 115200 baud.

**Disclaimer**

This code has been developed to demonstrate the viability of having a network of devices whose nodes participate in LoRa and Wi-Fi mesh networks concurrently. It has not been tested rigorously enough to be used at scale.