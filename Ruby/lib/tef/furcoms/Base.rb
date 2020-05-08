

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
		end
	end
end
