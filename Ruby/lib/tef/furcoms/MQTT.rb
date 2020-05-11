
require_relative 'Base.rb'

require 'xasin_logger'

module TEF
	module FurComs
		class MQTT < Base
			include XasLogger::Mix

			def initialize(mqtt, topic = 'FurComs/ttyACM0/')
				super();

				@mqtt = mqtt;
				@mqtt_topic = topic;

				@mqtt.subscribe_to topic + 'Received/#' do |data, topic|
					topic = topic.join('/')
					next unless topic =~ /^[\w\s\/]*$/

					handout_data(topic, data);
				end

				init_x_log(@mqtt_topic)
			end

			def send_message(topic, data, priority: 0, chip_id: 0)
				unless topic =~ /^[\w\s\/]*$/
					raise ArgumentError, "Topic includes invalid characters!"
				end
				if (topic.length + data.length) > 250
					raise ArgumentError, "Message packet length exceeded!"
				end

				x_logd("Sending '#{topic}': '#{data}'")

				@mqtt.publish_to @mqtt_topic + "Send/#{topic}", data
			end
		end
	end
end
