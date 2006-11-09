require 'test/unit'
$:.unshift File.expand_path( File.dirname( __FILE__ ), "../ext" )
require 'fastthread'

class TestQueue < Test::Unit::TestCase
  def test_queue
    q = Queue.new
    s = ""
    t = Thread.new do
      for c in "a".."f"
        q << c
      end
    end
    6.times do
      s << q.shift
    end
    assert_equal "abcdef", s
  end
end 

