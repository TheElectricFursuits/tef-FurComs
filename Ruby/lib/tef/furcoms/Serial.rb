
require_relative 'Base.rb'

require 'serialport'
require 'xasin_logger'

module TEF
	module FurComs

		# USB-To-Serial FurComs Bridge.
		#
		# This class will handle connecting to a FurComs bus using a standard
		# USB-To-UART adapter, such as an FTDI-Chip, in order to send and
		# receive messages.
		#
		# @note This class provides limited FurComs compatibility. Arbitration
		#  handling is not possible, which may cause frequent collisions in busy
		#  bus conditions. It is recommended to use a STM32 processor
		#  that provides translation between FurComs and the computer.
		class Serial < Base
			include XasLogger::Mix

			# Initialize a Serial bridge class.
			#
			# This will open the given Serial port and begin reading/writing
			# onto the FurComs bus.
			# @note This class can not provide full arbitration handling. This may
			#   cause issues in busy bus conditions!
			# @todo Add graceful handling of controller disconnect/reconnect.
			def initialize(port = '/dev/ttyACM0')
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
						c = @port.getbyte # Blocks until byte is available on bus.

						if c == 0 # 0x00 Indicates STOP
							decode_data_string rx_buffer
							rx_buffer = ''
						elsif had_esc # ESC had been received, decode next byte.
							case c
							when 0xDC
								rx_buffer += "\0"
							when 0xDD
								rx_buffer += "\xDB"
							end
							had_esc = false
						elsif c == 0xDB # SLIP ESC
							had_esc = true
						else # No special treatment needed, add byte.
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

			# @private
			# Internal function to append/prepend the neccessary framing
			# to be a FurComs message.
			# This framing is:
			# - 0x00 START
			# - priority and chip_id fields
			# - 0xFF latency
			# - 24-bit arbitration collission map.
			# - 0xFF latency
			# - Modified SLIP-Encoded message data
			# - 0x00 STOP
			private def generate_furcom_message(priority, chip_id, data_str)
				priority = ([[-60, priority].max, 60].min + 64) * 2 + 1;
				chip_id = 0x1 | 0x100 | ((chip_id & 0xEF) << 9) | ((chip_id >> 6) & 0xEF);

				out_data = [0x00, priority, chip_id, 0xFF, 0xFE, 0xFF, 0xFF, 0xFF]
				out_data += data_str + [0]

				out_data
			end

			# (see Base#send_message)
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
