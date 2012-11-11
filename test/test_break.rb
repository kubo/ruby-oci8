# High-level API
require 'oci8'
require 'test/unit'
require 'timeout'
require File.dirname(__FILE__) + '/config'

class TestBreak < Test::Unit::TestCase

  def setup
    @conn = get_oci8_connection
  end

  def teardown
    @conn.logoff
  end

  def report(str)
    printf "%d: %s\n", (Time.now - $start_time), str
  end

  @@server_is_runing_on_windows = nil
  def server_is_runing_on_windows?
    if @@server_is_runing_on_windows.nil?
      @@server_is_runing_on_windows = false
      @conn.exec('select banner from v$version') do |row|
        @@server_is_runing_on_windows = true if row[0].include? 'Windows'
      end
    end
    @@server_is_runing_on_windows
  end

  PLSQL_DONE = 1
  OCIBREAK = 2
  SEND_BREAK = 3

  TIME_IN_PLSQL = 5
  TIME_TO_BREAK = 2

  def do_test_ocibreak(conn, expect)
    $start_time = Time.now

    th = Thread.start do 
      begin
        conn.exec("BEGIN DBMS_LOCK.SLEEP(#{TIME_IN_PLSQL}); END;")
        assert_equal(expect[PLSQL_DONE], (Time.now - $start_time).round, 'PLSQL_DONE')
      rescue OCIBreak
        assert_equal(expect[OCIBREAK], (Time.now - $start_time).round, 'OCIBREAK')
        assert_equal('Canceled by user request.', $!.to_s)
      end
    end

    sleep(0.3) # Wait until DBMS_LOCK.SLEEP is running.
    sleep(TIME_TO_BREAK - 0.3)
    assert_equal(expect[SEND_BREAK], (Time.now - $start_time).round, 'SEND_BREAK')
    conn.break()
    th.join
  end

  def test_blocking_mode
    @conn.non_blocking = false
    assert_equal(false, @conn.non_blocking?)
    expect = []
    expect[PLSQL_DONE] = TIME_IN_PLSQL
    expect[OCIBREAK]   = "Invalid status"
    if defined? Rubinius and Rubinius::VERSION >= "2.0"
      # Rubinius 2.0.0.
      # DBMS_LOCK.SLEEP blocks ruby threads which try to call C-API.
      expect[SEND_BREAK] = TIME_TO_BREAK
    else
      # MRI and Rubinius 1.2.4.
      # DBMS_LOCK.SLEEP blocks all ruby threads.
      expect[SEND_BREAK] = TIME_IN_PLSQL + TIME_TO_BREAK
    end
    do_test_ocibreak(@conn, expect)
  end

  def test_non_blocking_mode
    @conn.non_blocking = true
    assert_equal(true, @conn.non_blocking?)
    expect = []
    if server_is_runing_on_windows?
      if $oracle_server_version >= OCI8::ORAVER_9_0
        # raise after sleeping #{TIME_IN_PLSQL} seconds.
        expect[PLSQL_DONE] = "Invalid status"
        expect[OCIBREAK] = TIME_IN_PLSQL
      else
        expect[PLSQL_DONE] = TIME_IN_PLSQL
        expect[OCIBREAK] = "Invalid status"
      end
    else
      # raise immediately by OCI8#break.
      expect[PLSQL_DONE] = "Invalid status"
      expect[OCIBREAK] = TIME_TO_BREAK
    end
    expect[SEND_BREAK]   = TIME_TO_BREAK
    do_test_ocibreak(@conn, expect)
  end

  def test_timeout
    @conn.non_blocking = true
    start_time = Time.now

    if defined? Rubinius and Rubinius::VERSION < "2.0"
      # Rubinius 1.2.4
      expected = OCIBreak
    else
      # MRI and Rubinius 2.0.0
      expected = Timeout::Error
    end
    assert_raise(expected) do
      Timeout.timeout(1) do
        @conn.exec("BEGIN DBMS_LOCK.SLEEP(5); END;")
      end
    end
    if server_is_runing_on_windows?
      end_time = start_time + 5
    else
      end_time = start_time + 1
    end
    assert_in_delta(Time.now, end_time, 1)
    @conn.exec("BEGIN NULL; END;")
    assert_in_delta(Time.now, end_time, 1)
  end
end
