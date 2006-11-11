require 'test/unit'
$:.unshift File.expand_path( File.join( File.dirname( __FILE__ ), "../ext" ) )
require 'fastthread'

class TestCondVar < Test::Unit::TestCase
  def test_signal
    s = ""
    m = Mutex.new
    cv = ConditionVariable.new
    ready = false

    t = Thread.new do
      nil until m.synchronize { ready }
      m.synchronize { s << "b" }
      cv.signal
    end

    m.synchronize do
      s << "a"
      ready = true
      cv.wait m
      assert m.locked?
      s << "c"
    end

    t.join

    assert_equal "abc", s
  end

  def test_signal_order
    m = Mutex.new
    cv = ConditionVariable.new
    ready = false
    s = ""

    threads = ("a".."f").map do |c|
      thread = Thread.new do
        m.synchronize do
          ready = true
          cv.wait m
          s << c
          ready = true
        end
      end
      nil until m.synchronize { ready }
      thread
    end

    threads.each_with_index do |thread, index|
      assert thread.alive?
      ready = false
      cv.signal
      nil until m.synchronize { ready }
      assert_equal( index + 1, m.synchronize { s.size } )
    end

    assert_equal "abcdef", s
  end
end 

