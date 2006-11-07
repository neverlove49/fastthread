require 'thread'
require 'benchmark'
require 'optimized-locking'

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
  x.report( "raw:" ) { n.times { nil } }
  x.report( "open-coded noop:" ) { n.times {
    begin
      nil
    ensure
      nil
    end
  } }
  x.report( "noop method:" ) { n.times { m.noop { nil } } }
  x.report( "optimized open-coded lock:" ) { n.times {
    om.lock
    begin
      nil
    ensure
      om.unlock
    end
  } }
  x.report( "optimize synchronize:" ) { n.times { om.synchronize { nil } } }
  x.report( "open-coded critical:" ) { n.times {
    saved = Thread.critical
    begin
      Thread.critical = true
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

