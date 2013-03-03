Gem::Specification.new do |s|
  s.name        = 'fastthread'
  s.version     = '1.0.7'
  s.platform    = Gem::Platform::RUBY
  s.authors     = ["MenTaLguY"]
  s.email       = ["mental@rydia.net"]
  s.homepage    = "https://github.com/mental/fastthread"
  s.summary     = %q{Legacy hotfix for Ruby concurrency}
  s.description = %q{Optimized replacement for thread.rb primitives}

  s.rubyforge_project = "fastthread"

  s.files         = `git ls-files -- ext/*`.split("\n")
  s.test_files    = `git ls-files -- test/*`.split("\n")
  s.executables   = `git ls-files -- bin/*`.split("\n").map{ |f| File.basename(f) }
  s.require_paths = ["ext"]
end
