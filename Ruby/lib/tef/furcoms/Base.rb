

# TheElectricFursuits Ruby namespace.
# @see https://github.com/TheElectricFursuits/
module TEF
	# Module for all classes related to the FurComs communication bus.
	# @see https://github.com/TheElectricFursuits/tef-FurComs
	module FurComs
		# Base class for a FurComs bus.
		#
		# This base class represents a single, bidirectional connection
		# point to a FurComs bus. It provides function prototypes for sending
		# and receiving messages that shall be overloaded by actual implementations.
		#
		# @author The System (Neira)
		#
		# @see Serial
		# @see MQTT
		class Base
			# Initialize an empty base class.
			#
			# Note that this class cannot be used for communication!
			def initialize()
				@message_procs = [];
			end

			# Send a message to the FurComs bus.
			#
			# This will send a message onto topic, using the given
			# priority and chip_id (defaulting both to 0).
			#
			# @param topic [String] Topic to send the message onto
			# @param message [String] Binary string of data to send, expected
			#   to be ASCII-8 encoded, can contain any character (including null)
			def send_message(topic, message, priority: 0, chip_id: 0) end

			# Add a message callback.
			#
			# Calling this function with a block will add a callback to received
			# messages. Any new message that is received will be passed to the
			# callback.
			# @param topic_filter [String, RegExp] Optional topic filter to use,
			#   either a String (for exact match) or Regexp
			# @yieldparam data [String] Raw data received from the FurComs bus.
			#   ASCII-8 encoded, may contain any binary character.
			# @yieldparam topic [String] Topic string that this message was received on.
			def on_message(topic_filter = nil, &block)
				o_msg = { topic: topic_filter, block: block };
				@message_procs << o_msg;

				o_msg
			end

			# @private
			# Internal function used merely to cleanly hand out received data
			# to message callbacks.
			private def handout_data(topic, data)
				x_logd("Data received on #{topic}: #{data}")

				@message_procs.each do |callback|
					begin
						if filter = callback[:topic]
							if filter.is_a? String
								next unless topic == filter
							elsif filter.is_a? RegExp
								next unless filter.match(topic)
							end
						end

						callback[:block].call(data, topic)
					rescue => e
						x_logf("Error in callback #{callback[:block]}: #{e}");
					end
				end
			end
		end
	end
end
