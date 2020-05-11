
require_relative 'Base.rb'

require 'serialport'
require 'xasin_logger'

module TEF
	module FurComs
		class Serial < Base
			include XasLogger::Mix

			def initialize(port)
				super();

				@port = SerialPort.new(port);
				@port.baud = 115200;
				@port.sync = true;

				start_thread();

				init_x_log("FurComs #{port}")
				x_logi("Ready!");
			end

			private def decode_data_string(data)
				return if(data.length() < 9)
				payload = data[8..-1]
				topic, _sep, payload = payload.partition("\0")

				# Filter out unsafe topics
				return unless topic =~ /^[\w\s\/]*$/

				handout_data(topic, payload);
			end

			private def start_thread()
				@rx_thread = Thread.new() do
					had_esc = false;
					rx_buffer = '';
					c = 0;

					loop do
						c = @port.getbyte
						if c == 0
							decode_data_string rx_buffer
							rx_buffer = ''
						elsif had_esc
							case c
							when 0xDC
								rx_buffer += "\0"
							when 0xDD
								rx_buffer += "\xDB"
							end
							had_esc = false
						elsif c == 0xDB
							had_esc = true
						else
							rx_buffer += c.chr
						end
					end
				end

				@rx_thread.abort_on_exception = true
			end

			private def slip_encode_data(data_str)
				data_str.bytes.map do |b|
					if b == 0
						[0xDB, 0xDC]
					elsif b == 0xDB
						[0xDB, 0xDD]
					else
						b
					end
				end.flatten
			end

			private def generate_furcom_message(priority, chip_id, data_str)
				priority = ([[-60, priority].max, 60].min + 64) * 2 + 1;
				chip_id = 0x1 | 0x100 | ((chip_id & 0xEF) << 9) | ((chip_id >> 6) & 0xEF);

				out_data = [0x00, priority, chip_id, 0xFF, 0xFE, 0xFF, 0xFF, 0xFF]
				out_data += data_str + [0]

				out_data
			end

			def send_message(topic, message, priority: 0, chip_id: 0)
				unless topic =~ /^[\w\s\/]*$/
					raise ArgumentError, "Topic includes invalid characters!"
				end
				if (topic.length + message.length) > 250
					raise ArgumentError, "Message packet length exceeded!"
				end

				x_logd("Sending '#{topic}': '#{message}'")

				escaped_str = slip_encode_data "#{topic}\0#{message}"
				out_data = generate_furcom_message priority, chip_id, escaped_str;

				@port.write(out_data.pack("C2S<C*"))

				@port.flush
			end
		end
	end
end
