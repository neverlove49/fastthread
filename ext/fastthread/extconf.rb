version_components = RUBY_VERSION.split('.').map { |c| c.to_i }

need_fastthread = ( !defined? RUBY_ENGINE )
need_fastthread &= ( RUBY_PLATFORM != 'java' )
need_fastthread &= version_components[0..1] == [1, 8]

if need_fastthread
  require 'mkmf'
  create_makefile('fastthread')
else
  require 'rbconfig'
  File.open('Makefile', 'w') do |stream|
    RbConfig::CONFIG.each do |key, value|
      stream.puts "#{key} = #{value.to_s.strip}"
    end
    stream.puts
    stream << <<EOS
RUBYARCHDIR = $(sitearchdir)$(target_prefix)

default:

install:
	mkdir -p $(RUBYARCHDIR)
	touch $(RUBYARCHDIR)/fastthread.rb

EOS
  end
end
