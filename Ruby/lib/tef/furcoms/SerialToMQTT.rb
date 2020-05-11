
require_relative 'Serial.rb'

module TEF
	module FurComs
		class SerialToMQTT < Serial
			def initialize(port, mqtt, topic = nil)
				super(port);

				@mqtt = mqtt;
				@mqtt_topic = topic || "FurComs/#{port.sub('/dev/', '')}/";

				@mqtt.subscribe_to @mqtt_topic + 'Send/#' do |data, topic|
					topic = topic.join('/')

					next if data.length + topic.length > 250
					next unless topic =~ /^[\w\s\/]*$/

					self.send_message topic, data
				end
			end

			def handout_data(topic, data)
				super(topic, data);

				@mqtt.publish_to @mqtt_topic + "Received/#{topic}", data
			end
		end
	end
end
