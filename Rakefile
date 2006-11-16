require 'rake'
require 'rake/testtask'
require 'rake/gempackagetask'
require 'rake/rdoctask'

Rake::TestTask.new do |task|
  task.libs << 'test'
  task.test_files = Dir.glob( 'test/test*.rb' )
  task.verbose = true
end

Rake::RDocTask.new do |task|
  task.rdoc_dir = 'doc/rdoc'
end

gemspec = Gem::Specification.new do |gemspec|
  gemspec.name = "fastthread"
  gemspec.version = "0.0.1"
  gemspec.platform = Gem::Platform::RUBY
  gemspec.author = "MenTaLguY <mental@rydia.net>"
  gemspec.summary = "Optimized version of primitives from thread.rb"
  gemspec.has_rdoc = true
  gemspec.test_file = 'test/test_all.rb'
  gemspec.extensions = Dir.glob( 'ext/**/extconf.rb' )
  #gemspec.extra_rdoc_files = [ 'README' ]
  gemspec.files = %w( Rakefile ) +
                  Dir.glob( 'test/*.rb' ) +
                  Dir.glob( 'ext/**/*.{c,rb}' )
  gemspec.require_path = 'lib'
  gemspec.bindir = 'bin'
end

Rake::GemPackageTask.new( gemspec ) do |task|
  task.gem_spec = gemspec
  task.need_tar = true
end

