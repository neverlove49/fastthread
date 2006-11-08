require 'thread'
require 'benchmark'
require 'thread'

n = 1_000_000
m = Mutex.new

Benchmark.bm do |x|
  x.report( "Thread.critical=:" ) { n.times {
    Thread.critical = true
    begin
      nil
    ensure
      Thread.critical = false
    end
  } }
  x.report( "Thread.critical= (saved):" ) { n.times {
    saved = Thread.critical
    Thread.critical = true
    begin
      nil
    ensure
      Thread.critical = saved
    end
  } }
  x.report( "Thread.exclusive:" ) { n.times { Thread.exclusive { nil } } }
  x.report( "Mutex#lock/unlock:" ) { n.times {
    m.lock
    begin
      nil
    ensure
      m.unlock
    end
  } }
  x.report( "Mutex#synchronize:" ) { n.times { m.synchronize { nil } } }
end

