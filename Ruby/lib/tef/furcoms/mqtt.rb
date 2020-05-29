
require_relative 'base.rb'

require 'xasin_logger'

module TEF
	module FurComs
		# FurComs MQTT wrapper
		#
		# This class will connect to a MQTT Broker, and use it as a bridge
		# to a FurComs hardware connection.
		# This allows multiple different systems to access different busses remotely,
		# and has the added benefit of allowing Read/Write restrictions using
		# the MQTT built-in authentication.
		class MQTT < Base
			include XasLogger::Mix

			# Initialize a new FurComs MQTT Bridge.
			#
			# This will subscribe to the topic: topic + 'Received/#', and will
			# being relaying messages to the attached callbacks immediately.
			# Messages it wants to send will be sent to topic + 'Send/#{msg_topic}'
			#
			# If no MQTT to FurComs is attached to the bus, the message will be lost!
			#
			# @param mqtt The MQTT handler. Must support subscribe_to and publish_to.
			#   A fitting gem would be mqtt-sub_handler.
			# @param topic [String] Topic base. Must end with /
			def initialize(mqtt, topic = 'FurComs/ttyACM0/')
				super();

				@mqtt = mqtt;
				@mqtt_topic = topic;

				@mqtt.subscribe_to topic + 'Received/#' do |data, msg_topic|
					msg_topic = msg_topic.join('/')
					next unless msg_topic =~ /^[\w\s\/]*$/

					handout_data(msg_topic, data);
				end

				init_x_log(@mqtt_topic)
			end

			# (see Base#send_message)
			def send_message(topic, data, priority: 0, chip_id: 0)
				unless topic =~ /^[\w\s\/]*$/
					raise ArgumentError, 'Topic includes invalid characters!'
				end
				if (topic.length + data.length) > 250
					raise ArgumentError, 'Message packet length exceeded!'
				end

				x_logd("Sending '#{topic}': '#{data}'")

				@mqtt.publish_to @mqtt_topic + "Send/#{topic}", data
			end
		end
	end
end
