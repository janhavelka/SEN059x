# SEN0591 and SEN0592 source survey

Distributor and software registry records were checked on 20 November 2026 using the DFRobot part numbers. The [source inventory](ORIGINAL_SOURCES.md) lists the original manuals, examples and drawings for each model.

| Source | SEN0591 (3 m) | SEN0592 (5 m) |
| --- | --- | --- |
| Mouser order code | [426-SEN0591](https://www.mouser.com/en/ProductDetail/DFRobot/SEN0591?qs=HoCaDK9Nz5eQi7Rj5fSpUw%3D%3D) | [426-SEN0592](https://www.mouser.com/en/ProductDetail/DFRobot/SEN0592?qs=ZcfC38r4Pounvvw2TbpJ0Q%3D%3D) |
| TME order code | [DF-SEN0591](https://www.tme.eu/en/details/df-sen0591/distance-sensors/dfrobot/sen0591/) | [DF-SEN0592](https://www.tme.eu/en/details/df-sen0592/distance-sensors/dfrobot/sen0592/) |

DigiKey's “Datasheet” for 1738-SEN0591-ND opens the DFRobot HTML overview. Mouser lists a separate one-page EU declaration for SEN0591. No standalone manufacturer **technical datasheet PDF** surfaced for either part in the manufacturer and distributor records checked here. [DigiKey listing](https://www.digikey.com/en/products/detail/dfrobot/SEN0591/25558522) · [DigiKey's datasheet destination](https://wiki.dfrobot.com/sen0591/) · [Mouser's SEN0591 declaration link](https://www.mouser.com/catalog/additional/DFRobot_SEN0591.pdf)

Use the manufacturer's operating temperature range of −25 to +65 °C for both profiles. TME lists −25 to +60 °C for SEN0591 and −30 to +80 °C for SEN0592. The latter matches DFRobot's **storage** range. [DFRobot SEN0591](https://www.dfrobot.com/product-2728.html) · [DFRobot SEN0592](https://www.dfrobot.com/product-2729.html) · [TME SEN0591](https://www.tme.eu/en/details/df-sen0591/distance-sensors/dfrobot/sen0591/) · [TME SEN0592](https://www.tme.eu/en/details/df-sen0592/distance-sensors/dfrobot/sen0592/)

## Software registries

The repository name covers the two RS485 models above. Nearby DFRobot part numbers include an I²C laser sensor (SEN0590) and UART underwater ultrasonic sensors (SEN0598 and SEN0599), each with its own protocol. [SEN0590](https://wiki.dfrobot.com/sen0590/) · [SEN0598](https://wiki.dfrobot.com/sen0598/) · [SEN0599](https://wiki.dfrobot.com/sen0599/)

The lookups used `SEN0591`, `SEN0592` and `SEN059x` individually. A package published under an unrelated name can fall outside a SKU search.

| Database and fields checked | Result |
| --- | --- |
| [Arduino Library Manager index](https://downloads.arduino.cc/libraries/library_index.json.gz), case-insensitive text across release metadata | Zero matches for all three terms. The index includes the manufacturer's generic `DFRobot_RTU` library. |
| PlatformIO Registry search API | Zero matches for [SEN0591](https://api.registry.platformio.org/v3/search?query=SEN0591), [SEN0592](https://api.registry.platformio.org/v3/search?query=SEN0592) and [SEN059x](https://api.registry.platformio.org/v3/search?query=SEN059x). Related packages include [DFRobot_RTU](https://registry.platformio.org/libraries/dfrobot/DFRobot_RTU), [ModbusMaster](https://registry.platformio.org/libraries/4-20ma/ModbusMaster) and [modbus-esp8266](https://registry.platformio.org/libraries/emelianov/modbus-esp8266). |
| ESP Component Registry | Zero matches for [SEN0591](https://components.espressif.com/components?q=SEN0591), [SEN0592](https://components.espressif.com/components?q=SEN0592) and [SEN059x](https://components.espressif.com/components?q=SEN059x). Espressif publishes its serial Modbus stack as [espressif/esp-modbus](https://components.espressif.com/components/espressif/esp-modbus). |
| GitHub public repository names, descriptions and READMEs | [SEN0591](https://api.github.com/search/repositories?q=SEN0591+in:name,description,readme) returned zero. [SEN0592](https://api.github.com/search/repositories?q=SEN0592+in:name,description,readme) returned an ESPHome integration. [SEN059x](https://api.github.com/search/repositories?q=SEN059x+in:name,description,readme) returned this repository. Searching file contents for [SEN0591](https://github.com/search?q=SEN0591&type=code) identified two additional applications. |
| [PyPI project-name index](https://pypi.org/simple/) and [ROS distribution metadata](https://github.com/ros/rosdistro) | No PyPI project name contained `sen059`. The Humble, Jazzy, Kilted, Lyrical and Rolling `distribution.yaml` files contained none of the three terms. |

## Device-specific community code

- **SEN0591, Indoor_LogisticsRobot.** Its C++ ROS 2 node reads `0x0101` from two units at addresses 1 and 2 over the same serial port and checks reply CRCs. [SEN0591 node](https://github.com/DeveloperKSH/Indoor_LogisticsRobot/blob/20fc43ce84158edc166df315918d2cf6526fabc7/z8015/src/sonar/src/sen0591.cpp)
- **SEN0591, GulliLuft.** This Arduino application has a register table and a polled RS485 transaction state machine. Its setup searches baud rates and can reconfigure the sensor. [RS485 distance interface](https://github.com/tokschi/GulliLuft/blob/02b3189f9efa9e5ebcaa7a2322a454374e706f61/include/RS485_distance.h)
- **SEN0592, dfrobot_sen0592.** An ESPHome configuration reads holding register `0x0101` in millimetres on an Atom Lite. The accompanying ROS 2 node reads the result over HTTP. Its message metadata specifies infrared and a 50 m maximum, both inconsistent with the ultrasonic 5 m model. [ESPHome configuration](https://github.com/karelkalina/dfrobot_sen0592/blob/0acb0d275d1035e4ff96a078416d8f8cd624a02d/firmware/atomlite-distance.yaml) · [ROS 2 node](https://github.com/karelkalina/dfrobot_sen0592/blob/0acb0d275d1035e4ff96a078416d8f8cd624a02d/ros_node/sen0592_driver/sen0592_node.py) · [Manufacturer specifications](https://wiki.dfrobot.com/sen0592/)
