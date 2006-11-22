require 'oci8'
require 'test/unit'
require './config'

class TestDateTime < Test::Unit::TestCase

  def timezone_string(tzh, tzm)
    if tzh >= 0
      format("+%02d:%02d", tzh, tzm)
    else
      format("-%02d:%02d", -tzh, -tzm)
    end
  end

  def setup
    @conn = OCI8.new($dbuser, $dbpass, $dbname)
    @local_timezone = timezone_string(*((::Time.now.utc_offset / 60).divmod 60))
  end

  def teardown
    @conn.logoff
  end

  def test_timestamp_get
    cursor = @conn.parse("BEGIN :out := TO_TIMESTAMP(:in, 'YYYY-MM-DD HH24:MI:SS.FF'); END;")
    cursor.bind_param(:out, nil, DateTime)
    cursor.bind_param(:in, nil, String, 36)
    ['2005-12-31 23:59:59.999999000',
     '2006-01-01 00:00:00.000000000'].each do |date|
      cursor[:in] = date
      cursor.exec
      assert_equal(DateTime.parse(date + @local_timezone), cursor[:out])
    end
  end

  def test_timestamp_set
    cursor = @conn.parse("BEGIN :out := TO_CHAR(:in, 'YYYY-MM-DD HH24:MI:SS.FF'); END;")
    cursor.bind_param(:out, nil, String, 33)
    cursor.bind_param(:in, nil, DateTime)
    ['2005-12-31 23:59:59.999999000',
     '2006-01-01 00:00:00.000000000'].each do |date|
      cursor[:in] = DateTime.parse(date + @local_timezone)
      cursor.exec
      assert_equal(date, cursor[:out])
    end
  end

  def test_timestamp_tz_get
    cursor = @conn.parse("BEGIN :out := TO_TIMESTAMP_TZ(:in, 'YYYY-MM-DD HH24:MI:SS.FF TZH:TZM'); END;")
    cursor.bind_param(:out, nil, DateTime)
    cursor.bind_param(:in, nil, String, 36)
    ['2005-12-31 23:59:59.999999000 +08:30',
     '2006-01-01 00:00:00.000000000 -08:30'].each do |date|
      cursor[:in] = date
      cursor.exec
      assert_equal(DateTime.parse(date), cursor[:out])
    end
  end

  def test_timestamp_tz_set
    cursor = @conn.parse("BEGIN :out := TO_CHAR(:in, 'YYYY-MM-DD HH24:MI:SS.FF TZH:TZM'); END;")
    cursor.bind_param(:out, nil, String, 36)
    cursor.bind_param(:in, nil, DateTime)
    ['2005-12-31 23:59:59.999999000 +08:30',
     '2006-01-01 00:00:00.000000000 -08:30'].each do |date|
      cursor[:in] = DateTime.parse(date)
      cursor.exec
      assert_equal(date, cursor[:out])
    end
  end

  def test_interval_ym_get
    cursor = @conn.parse(<<-EOS)
DECLARE
  ts1 TIMESTAMP;
  ts2 TIMESTAMP;
BEGIN
  ts1 := TO_TIMESTAMP(:in1, 'YYYY-MM-DD');
  ts2 := TO_TIMESTAMP(:in2, 'YYYY-MM-DD');
  :out := (ts1 - ts2) YEAR TO MONTH;
END;
EOS
    cursor.bind_param(:out, nil, :interval_ym)
    cursor.bind_param(:in1, nil, String, 36)
    cursor.bind_param(:in2, nil, String, 36)
    [['2006-01-01', '2004-03-01'],
     ['2006-01-01', '2005-03-01'],
     ['2006-01-01', '2006-03-01'],
     ['2006-01-01', '2007-03-01']
    ].each do |date1, date2|
      cursor[:in1] = date1
      cursor[:in2] = date2
      cursor.exec
      assert_equal(DateTime.parse(date1), DateTime.parse(date2) >> cursor[:out])
    end
  end

  def test_interval_ym_set
    cursor = @conn.parse(<<-EOS)
DECLARE
  ts1 TIMESTAMP;
BEGIN
  ts1 := TO_TIMESTAMP(:in1, 'YYYY-MM-DD');
  :out := TO_CHAR(ts1 + :in2, 'YYYY-MM-DD');
END;
EOS
    cursor.bind_param(:out, nil, String, 36)
    cursor.bind_param(:in1, nil, String, 36)
    cursor.bind_param(:in2, nil, :interval_ym)
    [['2006-01-01', -22],
     ['2006-01-01', -10],
     ['2006-01-01',  +2],
     ['2006-01-01', +12]
    ].each do |date, interval|
      cursor[:in1] = date
      cursor[:in2] = interval
      cursor.exec
      assert_equal(DateTime.parse(date) >> interval, DateTime.parse(cursor[:out]))
    end
  end

  def test_interval_ds_get
    cursor = @conn.parse(<<-EOS)
DECLARE
  ts1 TIMESTAMP;
  ts2 TIMESTAMP;
BEGIN
  ts1 := TO_TIMESTAMP(:in1, 'YYYY-MM-DD HH24:MI:SS.FF');
  ts2 := TO_TIMESTAMP(:in2, 'YYYY-MM-DD HH24:MI:SS.FF');
  :out := (ts1 - ts2) DAY TO SECOND(9);
END;
EOS
    cursor.bind_param(:out, nil, :interval_ds)
    cursor.bind_param(:in1, nil, String, 36)
    cursor.bind_param(:in2, nil, String, 36)
    [['2006-01-01', '2004-03-01'],
     ['2006-01-01', '2005-03-01'],
     ['2006-01-01', '2006-03-01'],
     ['2006-01-01', '2007-03-01'],
     ['2006-01-01', '2006-01-01 23:00:00'],
     ['2006-01-01', '2006-01-01 00:59:00'],
     ['2006-01-01', '2006-01-01 00:00:59'],
     ['2006-01-01', '2006-01-01 00:00:00.999999'],
     ['2006-01-01', '2006-01-01 23:59:59.999999'],
     ['2006-01-01', '2005-12-31 23:00:00'],
     ['2006-01-01', '2005-12-31 00:59:00'],
     ['2006-01-01', '2005-12-31 00:00:59'],
     ['2006-01-01', '2005-12-31 00:00:00.999999'],
     ['2006-01-01', '2005-12-31 23:59:59.999999']
    ].each do |date1, date2|
      cursor[:in1] = date1
      cursor[:in2] = date2
      cursor.exec
      assert_equal(DateTime.parse(date1) - DateTime.parse(date2), cursor[:out])
    end
  end

  def test_interval_ds_set
    cursor = @conn.parse(<<-EOS)
DECLARE
  ts1 TIMESTAMP;
BEGIN
  ts1 := TO_TIMESTAMP(:in1, 'YYYY-MM-DD HH24:MI:SS.FF');
  :out := TO_CHAR(ts1 + :in2, 'YYYY-MM-DD HH24:MI:SS.FF');
END;
EOS
    cursor.bind_param(:out, nil, String, 36)
    cursor.bind_param(:in1, nil, String, 36)
    cursor.bind_param(:in2, nil, :interval_ds)
    [['2006-01-01', -22],
     ['2006-01-01', -10],
     ['2006-01-01',  +2],
     ['2006-01-01', +12],
     ['2006-01-01', -1.to_r / 24], # one hour
     ['2006-01-01', -1.to_r / (24*60)], # one minute
     ['2006-01-01', -1.to_r / (24*60*60)], # one second
     ['2006-01-01', -999999.to_r / (24*60*60*1000000)], # 0.999999 seconds
     ['2006-01-01', +1.to_r / 24], # one hour
     ['2006-01-01', +1.to_r / (24*60)], # one minute
     ['2006-01-01', +1.to_r / (24*60*60)], # one second
     ['2006-01-01', +999999.to_r / (24*60*60*1000000)] # 0.999999 seconds
    ].each do |date, interval|
      cursor[:in1] = date
      cursor[:in2] = interval
      cursor.exec
      assert_equal(DateTime.parse(date) + interval, DateTime.parse(cursor[:out]))
    end
  end

end # TestOCI8
