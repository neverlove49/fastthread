require 'thread'
require 'benchmark'
require 'optimized_locking'

class Mutex
  def noop
    begin
      nil
    ensure
      nil
    end
  end
end

n = 1_000_000
m = Mutex.new
om = OptimizedMutex.new

Benchmark.bm do |x|
  x.report( "optimized open-coded lock:" ) { n.times {
    om.lock
    begin
      nil
    ensure
      om.unlock
    end
  } }
  x.report( "brazen open-coded critical:" ) { n.times {
    Thread.critical = true
    begin
      nil
    ensure
      Thread.critical = false
    end
  } }
  x.report( "optimized synchronize:" ) { n.times { om.synchronize { nil } } }
  x.report( "timid open-coded critical:" ) { n.times {
    saved = Thread.critical
    Thread.critical = true
    begin
      nil
    ensure
      Thread.critical = saved
    end
  } }
  x.report( "exclusive:" ) { n.times { Thread.exclusive { nil } } }
  x.report( "open-coded lock:" ) { n.times {
    m.lock
    begin
      nil
    ensure
      m.unlock
    end
  } }
  x.report( "synchronize:" ) { n.times { m.synchronize { nil } } }
end

