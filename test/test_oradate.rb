# Low-level API
require 'oci8'
require 'runit/testcase'
require 'runit/cui/testrunner'
require './config'

class TestOraDate < RUNIT::TestCase

  YEAR_CHECK_TARGET = [-4712, -1, 1, 1192, 1868, 2002, 9999]
  MONTH_CHECK_TARGET = [1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12]
  DAY_CHECK_TARGET = [1, 10, 20, 31] # days of January.
  HOUR_CHECK_TARGET = [0, 6, 12, 18, 23]
  MINUTE_CHECK_TARGET = [0, 15, 30, 45, 59]
  SECOND_CHECK_TARGET = [0, 15, 30, 45, 59]

  def setup
    @env, @svc, @stmt = setup_lowapi()
  end

  def check_oradate(target, year, month, day, hour, minute, second)
    assert_equal(year, target.year)
    assert_equal(month, target.month)
    assert_equal(day, target.day)
    assert_equal(hour, target.hour)
    assert_equal(minute, target.minute)
    assert_equal(second, target.second)
  end

  def test_new()
    check_oradate(OraDate.new(), 1, 1, 1, 0, 0, 0)
  end

  def test_set_year
    @stmt.prepare("BEGIN :year := TO_NUMBER(TO_CHAR(:date, 'SYYYY'), '9999'); END;")
    date_in = @stmt.bindByName(":date", OraDate)
    year_out = @stmt.bindByName(":year", Fixnum)
    date = OraDate.new()
    YEAR_CHECK_TARGET.each do |i|
      # set year
      date.year = i
      # check result via oracle.
      date_in.set(date)
      @stmt.execute(@svc)
      assert_equal(i, year_out.get())
    end
  end

  def test_get_year
    @stmt.prepare("BEGIN :date := TO_DATE(TO_CHAR(:year, '0999'), 'SYYYY'); END;")
    year_in = @stmt.bindByName(":year", Fixnum)
    date_out = @stmt.bindByName(":date", OraDate)
    YEAR_CHECK_TARGET.each do |i|
      # set date via oracle.
      year_in.set(i)
      @stmt.execute(@svc)
      # check OraDate#year
      assert_equal(i, date_out.get.year)
    end
  end

  def test_set_month
    @stmt.prepare("BEGIN :month := TO_NUMBER(TO_CHAR(:date, 'MM'), '99'); END;")
    date_in = @stmt.bindByName(":date", OraDate)
    month_out = @stmt.bindByName(":month", Fixnum)
    date = OraDate.new()
    MONTH_CHECK_TARGET.each do |i|
      # set month
      date.month = i
      # check result via oracle.
      date_in.set(date)
      @stmt.execute(@svc)
      assert_equal(i, month_out.get())
    end
  end

  def test_get_month
    @stmt.prepare("BEGIN :date := TO_DATE(TO_CHAR(:month, '99'), 'MM'); END;")
    month_in = @stmt.bindByName(":month", Fixnum)
    date_out = @stmt.bindByName(":date", OraDate)
    MONTH_CHECK_TARGET.each do |i|
      # set date via oracle.
      month_in.set(i)
      @stmt.execute(@svc)
      # check OraDate#month
      assert_equal(i, date_out.get.month)
    end
  end

  def test_set_day
    @stmt.prepare("BEGIN :day := TO_NUMBER(TO_CHAR(:date, 'DD'), '99'); END;")
    date_in = @stmt.bindByName(":date", OraDate)
    day_out = @stmt.bindByName(":day", Fixnum)
    date = OraDate.new()
    DAY_CHECK_TARGET.each do |i|
      # set day
      date.day = i
      # check result via oracle.
      date_in.set(date)
      @stmt.execute(@svc)
      assert_equal(i, day_out.get())
    end
  end

  def test_get_day
    @stmt.prepare("BEGIN :date := TO_DATE('200101' || TO_CHAR(:day, 'FM00'), 'YYYYMMDD'); END;")
    day_in = @stmt.bindByName(":day", Fixnum)
    date_out = @stmt.bindByName(":date", OraDate)
    DAY_CHECK_TARGET.each do |i|
      # set date via oracle.
      day_in.set(i)
      @stmt.execute(@svc)
      # check OraDate#day
      assert_equal(i, date_out.get.day)
    end
  end

  def test_set_hour
    @stmt.prepare("BEGIN :hour := TO_NUMBER(TO_CHAR(:date, 'HH24'), '99'); END;")
    date_in = @stmt.bindByName(":date", OraDate)
    hour_out = @stmt.bindByName(":hour", Fixnum)
    date = OraDate.new()
    HOUR_CHECK_TARGET.each do |i|
      # set hour
      date.hour = i
      # check result via oracle.
      date_in.set(date)
      @stmt.execute(@svc)
      assert_equal(i, hour_out.get())
    end
  end

  def test_get_hour
    @stmt.prepare("BEGIN :date := TO_DATE(TO_CHAR(:hour, '99'), 'HH24'); END;")
    hour_in = @stmt.bindByName(":hour", Fixnum)
    date_out = @stmt.bindByName(":date", OraDate)
    HOUR_CHECK_TARGET.each do |i|
      # set date via oracle.
      hour_in.set(i)
      @stmt.execute(@svc)
      # check OraDate#hour
      assert_equal(i, date_out.get.hour)
    end
  end

  def test_set_minute
    @stmt.prepare("BEGIN :minute := TO_NUMBER(TO_CHAR(:date, 'MI'), '99'); END;")
    date_in = @stmt.bindByName(":date", OraDate)
    minute_out = @stmt.bindByName(":minute", Fixnum)
    date = OraDate.new()
    MINUTE_CHECK_TARGET.each do |i|
      # set minute
      date.minute = i
      # check result via oracle.
      date_in.set(date)
      @stmt.execute(@svc)
      assert_equal(i, minute_out.get())
    end
  end

  def test_get_minute
    @stmt.prepare("BEGIN :date := TO_DATE(TO_CHAR(:minute, '99'), 'MI'); END;")
    minute_in = @stmt.bindByName(":minute", Fixnum)
    date_out = @stmt.bindByName(":date", OraDate)
    MINUTE_CHECK_TARGET.each do |i|
      # set date via oracle.
      minute_in.set(i)
      @stmt.execute(@svc)
      # check OraDate#minute
      assert_equal(i, date_out.get.minute)
    end
  end

  def test_set_second
    @stmt.prepare("BEGIN :second := TO_NUMBER(TO_CHAR(:date, 'SS'), '99'); END;")
    date_in = @stmt.bindByName(":date", OraDate)
    second_out = @stmt.bindByName(":second", Fixnum)
    date = OraDate.new()
    SECOND_CHECK_TARGET.each do |i|
      # set second
      date.second = i
      # check result via oracle.
      date_in.set(date)
      @stmt.execute(@svc)
      assert_equal(i, second_out.get())
    end
  end

  def test_get_second
    @stmt.prepare("BEGIN :date := TO_DATE(TO_CHAR(:second, '99'), 'SS'); END;")
    second_in = @stmt.bindByName(":second", Fixnum)
    date_out = @stmt.bindByName(":date", OraDate)
    SECOND_CHECK_TARGET.each do |i|
      # set date via oracle.
      second_in.set(i)
      @stmt.execute(@svc)
      # check OraDate#second
      assert_equal(i, date_out.get.second)
    end
  end

  def test_compare
    d1 = OraDate.new(2003,03,15,18,55,35)
    d2 = OraDate.new(2003,03,15,18,55,35)
    assert_equal(d1, d2)
    assert_operator(d1, :<, OraDate.new(2004,03,15,18,55,35))
    assert_operator(d1, :<, OraDate.new(2003,04,15,18,55,35))
    assert_operator(d1, :<, OraDate.new(2003,03,16,18,55,35))
    assert_operator(d1, :<, OraDate.new(2003,03,15,19,55,35))
    assert_operator(d1, :<, OraDate.new(2003,03,15,18,56,35))
    assert_operator(d1, :<, OraDate.new(2003,03,15,18,55,36))

    assert_operator(OraDate.new(2002,03,15,18,55,35), :<, d1)
    assert_operator(OraDate.new(2003,02,15,18,55,35), :<, d1)
    assert_operator(OraDate.new(2003,03,14,18,55,35), :<, d1)
    assert_operator(OraDate.new(2003,03,15,17,55,35), :<, d1)
    assert_operator(OraDate.new(2003,03,15,18,54,35), :<, d1)
    assert_operator(OraDate.new(2003,03,15,18,55,34), :<, d1)
  end

  def test_to_time
    year, month, day, hour, minute, second = [2003,03,15,18,55,35]
    dt = OraDate.new(year, month, day, hour, minute, second)
    tm = Time.local(year, month, day, hour, minute, second)
    assert_equal(tm, dt.to_time)

    year, month, day, hour, minute, second = [1900,1,1,0,0,0]
    dt = OraDate.new(year, month, day, hour, minute, second)
    assert_exception(RangeError) { dt.to_time }
  end

  def test_to_date
    [[2003,03,15], [1900,1,1], [-4712, 1, 1]].each do |year, month, day|
      odt = OraDate.new(year, month, day)
      rdt = Date.new(year, month, day)
      assert_equal(rdt, odt.to_date)
    end
  end

  def teardown
    @stmt.free()
    @svc.logoff()
    @env.free()
  end
end

if $0 == __FILE__
  RUNIT::CUI::TestRunner.run(TestOraDate.suite())
end
