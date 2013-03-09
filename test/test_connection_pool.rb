require 'oci8'
require 'test/unit'
require File.dirname(__FILE__) + '/config'

class TestConnectionPool < Test::Unit::TestCase

  def create_pool(min, max, incr)
    OCI8::ConnectionPool.new(min, max, incr, $dbuser, $dbpass, $dbname)
  rescue OCIError
    raise if $!.code != 12516 && $!.code != 12520
    sleep(5)
    OCI8::ConnectionPool.new(min, max, incr, $dbuser, $dbpass, $dbname)
  end

  def test_connect
    pool = create_pool(1, 5, 3)
    assert_equal(1, pool.min)
    assert_equal(5, pool.max)
    assert_equal(3, pool.incr)
  end

  def test_reinitialize
    pool = create_pool(1, 5, 3)
    pool.reinitialize(2, 6, 4)
    assert_equal(2, pool.min)
    assert_equal(6, pool.max)
    assert_equal(4, pool.incr)
  end

  def test_busy_and_open_count
    check_busy_and_open_count(1, 5, 3)
    check_busy_and_open_count(2, 4, 1)
  end

  def check_busy_and_open_count(min_cnt, max_cnt, incr_cnt)
    msg = "create_pool(#{min_cnt}, #{max_cnt}, #{incr_cnt})"
    # Create a connection pool.
    pool = create_pool(min_cnt, max_cnt, incr_cnt)
    assert_equal(min_cnt, pool.open_count, msg)
    assert_equal(0, pool.busy_count, msg)

    non_blocking = true

    # Create connections from the pool.
    conns = []
    max_cnt.times do |cnt|
      conn = OCI8.new($dbuser, $dbpass, pool)
      if cnt == 0
        unless conn.non_blocking?
          non_blocking = false
          assert_raise(RuntimeError) do
            # This should raise "Could not set non-blocking mode to a connection allocated from OCI8::ConnectionPool."
            conn.non_blocking = true
          end
        end
      end
      conns << conn
    end
    assert_equal(min_cnt, pool.open_count, msg)
    assert_equal(0, pool.busy_count, msg)

    # Execute blocking SQL statements sequentially.
    max_cnt.times do |n|
      thread = Thread.start do
        conns[n].exec "BEGIN DBMS_LOCK.SLEEP(1); END;"
      end
      sleep(0.5)
      assert_equal(min_cnt, pool.open_count, msg)
      assert_equal(non_blocking ? 1 : 0, pool.busy_count, msg)
      thread.join
    end
    assert_equal(min_cnt, pool.open_count, msg)
    assert_equal(0, pool.busy_count, msg)

    # Execute blocking SQL statements parallel to increment open_count.
    threads = []
    (min_cnt + 1).times do |n|
      threads << Thread.start do
        conns[n].exec "BEGIN DBMS_LOCK.SLEEP(5); END;"
      end
    end
    sleep(0.5)
    assert_equal(non_blocking ? (min_cnt + incr_cnt) : min_cnt, pool.open_count, msg)
    assert_equal(non_blocking ? (min_cnt + 1) : 0, pool.busy_count, msg)

    # Execute blocking SQL statements parallel up to maximum.
    (min_cnt + 1).upto(max_cnt - 1) do |n|
      threads << Thread.start do
        conns[n].exec "BEGIN DBMS_LOCK.SLEEP(4); END;"
      end
    end
    sleep(0.5)
    assert_equal(non_blocking ? max_cnt : min_cnt, pool.open_count, msg)
    assert_equal(non_blocking ? max_cnt : 0, pool.busy_count, msg)

    # 
    threads.each do |thr|
      thr.join
    end
    assert_equal(non_blocking ? max_cnt : min_cnt, pool.open_count, msg)
    assert_equal(0, pool.busy_count, msg)

    # Set timeout
    pool.timeout = 1
    sleep(1.5)
    assert_equal(non_blocking ? max_cnt : min_cnt, pool.open_count, msg) # open_count doesn't shrink.
    assert_equal(0, pool.busy_count, msg)
    conns[0].ping # make a network roundtrip.
    sleep(0.5)
    # open_count shrinks.
    # The decrement count depends on Oracle version.
    assert_operator(pool.open_count, :<, max_cnt, msg)
    assert_equal(0, pool.busy_count, msg)

    # Close all conections.
    conns.each do | conn |
      conn.logoff
    end
    assert_equal(min_cnt, pool.open_count, msg)
    assert_equal(0, pool.busy_count, msg)
  end

  def test_nowait
  end
end
