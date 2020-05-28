
require_relative 'Serial.rb'

module TEF
	module FurComs
		# FurComs SerialToMQTT Bridge.
		#
		# This class will extend {FurComs::Serial} with a few internal functions
		# to bridge the FurComs bus connection to MQTT.
		#
		# @note This class suffers from the same restrictions as the {Serial} class.
		#   Use a different PC to FurComs connection if possible!
		#
		# @see Serial
		class SerialToMQTT < Serial
			# Initialize a Serial To MQTT bridge instance.
			#
			# This will open the given Serial port and begin reading/writing
			# onto the FurComs bus.
			# It will additionally subscribe to the MQTT 'topic', or 'FurComs/#{topic}/Send/#'
			# using the /dev/DEVICE part of the port argument if topic is left nil.
			# Any message received on this topic will be sent as FurComs messages.
			#
			# Received messages are first handed out to internal callbacks,
			# and will then be sent off to 'FurComs/#{topic}/Received/#{msg_topic}'
			#
			# @note This class can not provide full arbitration handling. This may
			#   cause issues in busy bus conditions!
			# @param port [String] Device port to use for the FurComs connection
			# @param mqtt MQTT Handler class. Must support subscribe_to
			#  and publish_to
			# @param topic [nil, String] MQTT topic to use for receiving and sending.
			#  must end with a '/'
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

			private def handout_data(topic, data)
				super(topic, data);

				@mqtt.publish_to @mqtt_topic + "Received/#{topic}", data
			end
		end
	end
end
