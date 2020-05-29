
Gem::Specification.new do |s|
	s.name        = 'tef-furcoms'
	s.version     = '0.1.1'
	s.date        = '2020-05-19'
	s.summary     = 'Ruby-Gem for TheElectricFursuit FurComs'
	s.homepage    = 'https://github.com/TheElectricFursuits/tef-FurComs'
	s.description = 'Communication interfaces for TEF FurComs-compatible lines'
	s.authors     = %w[TheSystem Neira]
	s.files       = [Dir.glob('{bin,lib}/**/*'), 'README.md'].flatten
	s.license     = 'GPL-3.0'

	s.add_runtime_dependency 'mqtt-sub_handler', '~> 0.1'
	s.add_runtime_dependency 'serialport', '~> 1.3'
	s.add_runtime_dependency 'xasin-logger', '~> 0.1'

	s.add_development_dependency 'rubocop', '~> 0.77.0'
end
