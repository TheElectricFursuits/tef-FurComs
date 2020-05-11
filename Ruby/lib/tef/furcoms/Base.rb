

module TEF
	module FurComs
		class Message
			attr_reader :priority
			attr_reader :source_id

			attr_reader :topic
			attr_reader :data

			def initialize()
			end
		end

		class Base
			def initialize()
				@message_procs = [];
			end

			def send_message(topic, message, priority: 0, chip_id: 0) end

			def on_message(topic_filter = nil, &block)
				@message_procs << { topic: topic_filter, block: block };
			end

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
