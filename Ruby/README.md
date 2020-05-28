
# TheElectricFursuit FurComs

This gem is the link between a FurComs hardware bus and your ruby code!
It provides a standardized interface for sending and receiving messages, using
a large variety of channels to connect to a FurComs bus.

In its current state, it provides the following options:
- A direct FurComs connection via a USB to UART adapter.  
  This does not provide arbitration, but is perfect for testing, as
  well as read-only access.
- Two MQTT Briding classes to connect a Serial FurComs connection
  to a broker, and to access the FurComs bus via MQTT.

In upcoming versions there will be other options available, such as
using a STM32 as translator to the computer, providing true arbitration.

### Quickstart:

Let's give a short and sweet example. Here, we will try to connect to a FurComs bus using a USB to UART adapter connected to `/dev/ttyACM0` and send and receive a few messages.

```Ruby
require 'tef/furcoms.rb'

coms_interface = TEF::FurComs::Serial.new('/dev/ttyACM0');

coms_interface.on_message /^Topic_Regexp(\d)/ do |data, topic|
	puts "Got some data on #{topic}!"
end

coms_interface.send_message 'Topic_Regexp1234', 'Henlo yes!'
```

And that's all you need!

If you want to use MQTT you'll need to do the following instead:
```Ruby
require 'mqtt/sub_handler.rb' # Comes from the mqtt-sub_handler gem, which is not a dependency of this FurComs gem!

require 'tef/furcoms.rb'

mqtt = MQTT::SubHandler.new('your.broker.address');

coms_interface = TEF::FurComs::SerialToMQTT.new('/dev/ttyACM0', mqtt, 'FurComs/MyTopic/');
# Leaving out the topic will instead use 'FurComs/DEVNAME', in this case FurComs/ttyACM0

coms_over_mqtt = TEF::FurComs::MQTT.new(mqtt, 'FurComs/MyTopic/');
```

The SerialToMQTT and MQTT classes have the same interface functions as the Serial class, and can be used interchangeably!
