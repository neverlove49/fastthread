require 'rake'
require 'rake/testtask'
require 'rake/gempackagetask'

Rake::TestTask.new do |task|
  task.libs << 'test'
  task.test_files = Dir.glob( 'test/test*.rb' )
  task.verbose = true
end

gemspec = Gem::Specification.new do |gemspec|
  gemspec.name = "fastthread"
  gemspec.version = "0.1"
  gemspec.platform = Gem::Platform::RUBY
  gemspec.author = "MenTaLguY <mental@rydia.net>"
  gemspec.summary = "Optimized replacement for thread.rb primitives"
  gemspec.test_file = 'test/test_all.rb'
  gemspec.extensions = Dir.glob( 'ext/**/extconf.rb' )
  gemspec.files = %w( Rakefile ) +
                  Dir.glob( 'test/*.rb' ) +
                  Dir.glob( 'ext/**/*.{c,rb}' )
  gemspec.require_path = 'ext'
end

Rake::GemPackageTask.new( gemspec ) do |task|
  task.gem_spec = gemspec
  task.need_tar = true
end

