# Low-level API
require 'oci8'
require 'runit/testcase'
require 'runit/cui/testrunner'
require './config'

class TestBindTime < RUNIT::TestCase

  YEAR_CHECK_TARGET = [1971, 1989, 2002, 2037]
  MON_CHECK_TARGET = [1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12]
  DAY_CHECK_TARGET = [1, 10, 20, 31] # days of January.
  HOUR_CHECK_TARGET = [0, 6, 12, 18, 23]
  MIN_CHECK_TARGET = [0, 15, 30, 45, 59]
  SEC_CHECK_TARGET = [0, 15, 30, 45, 59]

  def setup
    @env, @svc, @stmt = setup_lowapi()
  end

  def test_set_year
    @stmt.prepare("BEGIN :year := TO_NUMBER(TO_CHAR(:time, 'SYYYY'), '9999'); END;")
    time_in = @stmt.bindByName(":time", Time)
    year_out = @stmt.bindByName(":year", Fixnum)
    YEAR_CHECK_TARGET.each do |i|
      # set year
      time = Time.local(i, 1)
      # check result via oracle.
      time_in.set(time)
      @stmt.execute(@svc)
      assert_equal(i, year_out.get())
    end
  end

  def test_get_year
    @stmt.prepare("BEGIN :time := TO_DATE(TO_CHAR(:year, '0999'), 'SYYYY'); END;")
    year_in = @stmt.bindByName(":year", Fixnum)
    time_out = @stmt.bindByName(":time", Time)
    YEAR_CHECK_TARGET.each do |i|
      # set time via oracle.
      year_in.set(i)
      @stmt.execute(@svc)
      # check Time#year
      assert_equal(i, time_out.get.year)
    end
  end

  def test_set_mon
    @stmt.prepare("BEGIN :mon := TO_NUMBER(TO_CHAR(:time, 'MM'), '99'); END;")
    time_in = @stmt.bindByName(":time", Time)
    mon_out = @stmt.bindByName(":mon", Fixnum)
    MON_CHECK_TARGET.each do |i|
      # set mon
      time = Time.local(2001, i)
      # check result via oracle.
      time_in.set(time)
      @stmt.execute(@svc)
      assert_equal(i, mon_out.get())
    end
  end

  def test_get_mon
    @stmt.prepare("BEGIN :time := TO_DATE(TO_CHAR(:mon, '99'), 'MM'); END;")
    mon_in = @stmt.bindByName(":mon", Fixnum)
    time_out = @stmt.bindByName(":time", Time)
    MON_CHECK_TARGET.each do |i|
      # set time via oracle.
      mon_in.set(i)
      @stmt.execute(@svc)
      # check Time#mon
      assert_equal(i, time_out.get.mon)
    end
  end

  def test_set_day
    @stmt.prepare("BEGIN :day := TO_NUMBER(TO_CHAR(:time, 'DD'), '99'); END;")
    time_in = @stmt.bindByName(":time", Time)
    day_out = @stmt.bindByName(":day", Fixnum)
    DAY_CHECK_TARGET.each do |i|
      # set day
      time = Time.local(2001, 1, i)
      # check result via oracle.
      time_in.set(time)
      @stmt.execute(@svc)
      assert_equal(i, day_out.get())
    end
  end

  def test_get_day
    @stmt.prepare("BEGIN :time := TO_DATE('200101' || TO_CHAR(:day, 'FM00'), 'YYYYMMDD'); END;")
    day_in = @stmt.bindByName(":day", Fixnum)
    time_out = @stmt.bindByName(":time", Time)
    DAY_CHECK_TARGET.each do |i|
      # set time via oracle.
      day_in.set(i)
      @stmt.execute(@svc)
      # check Time#day
      assert_equal(i, time_out.get.day)
    end
  end

  def test_set_hour
    @stmt.prepare("BEGIN :hour := TO_NUMBER(TO_CHAR(:time, 'HH24'), '99'); END;")
    time_in = @stmt.bindByName(":time", Time)
    hour_out = @stmt.bindByName(":hour", Fixnum)
    HOUR_CHECK_TARGET.each do |i|
      # set hour
      time = Time.local(2001, 1, 1, i)
      # check result via oracle.
      time_in.set(time)
      @stmt.execute(@svc)
      assert_equal(i, hour_out.get())
    end
  end

  def test_get_hour
    @stmt.prepare("BEGIN :time := TO_DATE(TO_CHAR(:hour, '99'), 'HH24'); END;")
    hour_in = @stmt.bindByName(":hour", Fixnum)
    time_out = @stmt.bindByName(":time", Time)
    HOUR_CHECK_TARGET.each do |i|
      # set time via oracle.
      hour_in.set(i)
      @stmt.execute(@svc)
      # check Time#hour
      assert_equal(i, time_out.get.hour)
    end
  end

  def test_set_min
    @stmt.prepare("BEGIN :min := TO_NUMBER(TO_CHAR(:time, 'MI'), '99'); END;")
    time_in = @stmt.bindByName(":time", Time)
    min_out = @stmt.bindByName(":min", Fixnum)
    MIN_CHECK_TARGET.each do |i|
      # set min
      time = Time.local(2001, 1, 1, 0, i)
      # check result via oracle.
      time_in.set(time)
      @stmt.execute(@svc)
      assert_equal(i, min_out.get())
    end
  end

  def test_get_min
    @stmt.prepare("BEGIN :time := TO_DATE(TO_CHAR(:min, '99'), 'MI'); END;")
    min_in = @stmt.bindByName(":min", Fixnum)
    time_out = @stmt.bindByName(":time", Time)
    MIN_CHECK_TARGET.each do |i|
      # set time via oracle.
      min_in.set(i)
      @stmt.execute(@svc)
      # check Time#min
      assert_equal(i, time_out.get.min)
    end
  end

  def test_set_sec
    @stmt.prepare("BEGIN :sec := TO_NUMBER(TO_CHAR(:time, 'SS'), '99'); END;")
    time_in = @stmt.bindByName(":time", Time)
    sec_out = @stmt.bindByName(":sec", Fixnum)
    SEC_CHECK_TARGET.each do |i|
      # set sec
      time = Time.local(2001, 1, 1, 0, 0, i)
      # check result via oracle.
      time_in.set(time)
      @stmt.execute(@svc)
      assert_equal(i, sec_out.get())
    end
  end

  def test_get_sec
    @stmt.prepare("BEGIN :time := TO_DATE(TO_CHAR(:sec, '99'), 'SS'); END;")
    sec_in = @stmt.bindByName(":sec", Fixnum)
    time_out = @stmt.bindByName(":time", Time)
    SEC_CHECK_TARGET.each do |i|
      # set time via oracle.
      sec_in.set(i)
      @stmt.execute(@svc)
      # check Time#sec
      assert_equal(i, time_out.get.sec)
    end
  end

  def teardown
    @stmt.free()
    @svc.logoff()
    @env.free()
  end
end

if $0 == __FILE__
  RUNIT::CUI::TestRunner.run(TestBindTime.suite())
end
