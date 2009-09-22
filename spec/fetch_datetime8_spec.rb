#
# Specs for OCI8::Cursor when fetching date, timestamp, timestamp with
# time zone and tiemstamp with local time zone as DateTime by Oracle 8.x API
#
require File.join(File.dirname(__FILE__), 'spec_helper.rb')

module FetchDateTime8Helper
  def before_all
    @conn = get_oracle_connection()
    @default_timezone = @conn.select_one("select sessiontimezone from dual")[0]
    OCI8::BindType::Mapping[:date] = OCI8::BindType::DateTimeViaOCIDate
    OCI8::BindType::Mapping[:timestamp] = OCI8::BindType::DateTimeViaOCIDate
    OCI8::BindType::Mapping[:timestamp_tz] = OCI8::BindType::DateTimeViaOCIDate
    OCI8::BindType::Mapping[:timestamp_ltz] = OCI8::BindType::DateTimeViaOCIDate
  end

  def after_all
    OCI8::BindType::Mapping[:date] = OCI8::BindType::Time
    OCI8::BindType::Mapping[:timestamp] = OCI8::BindType::Time
    OCI8::BindType::Mapping[:timestamp_tz] = OCI8::BindType::Time
    OCI8::BindType::Mapping[:timestamp_ltz] = OCI8::BindType::Time
    @conn.logoff
  end

  @@local_timezone = DateTime.now.zone

  def local_timezone
    @@local_timezone
  end
end

describe OCI8::Cursor, "when fetching a date as DateTime by Oracle 8.x API" do
  include FetchDateTime8Helper

  before :all do
    before_all
  end

  after :all do
    after_all
  end

  def fetch_a_date(str, fmt = 'yyyy-mm-dd hh24:mi:ss')
    cursor = @conn.exec("select to_date(:1, :2) from dual", str, fmt)
    begin
      cursor.column_metadata[0].data_type.should eql(:date)
      cursor.fetch[0]
    ensure
      cursor.close
    end
  end

  it "should get a DateTime whose time zone is OCI8::BindType.default_timezone" do
    begin
      [:utc, :local].each do |tz|
        OCI8::BindType.default_timezone = tz
        result = fetch_a_date('2009-10-11 12:13:14')
        expected_val = DateTime.parse("2009-10-11T12:13:14#{tz == :utc ? '+00:00' : local_timezone}")
        result.should eql(expected_val)
        result.offset.to_r.should eql(expected_val.offset.to_r)
      end
    ensure
      OCI8::BindType.default_timezone = :local
    end
  end
end

describe OCI8::Cursor, "when fetching a timestamp as DateTime by Oracle 8.x API" do
  include FetchDateTime8Helper

  before :all do
    before_all
  end

  after :all do
    after_all
  end

  def fetch_a_timestamp(str, fmt = 'yyyy-mm-dd hh24:mi:ss')
    cursor = @conn.exec("select to_timestamp(:1, :2) from dual", str, fmt)
    begin
      cursor.column_metadata[0].data_type.should eql(:timestamp)
      cursor.fetch[0]
    ensure
      cursor.close
    end
  end

  it "should get a DateTime whose time zone is OCI8::BindType.default_timezone" do
    begin
      [:utc, :local].each do |tz|
        OCI8::BindType.default_timezone = tz
        result = fetch_a_timestamp('2009-10-11 12:13:14')
        expected_val = DateTime.parse("2009-10-11T12:13:14#{tz == :utc ? '+00:00' : local_timezone}")
        result.should eql(expected_val)
        result.offset.to_r.should eql(expected_val.offset.to_r)
      end
    ensure
      OCI8::BindType.default_timezone = :local
    end
  end

  it "should get a DateTime whose fractional seconds are truncated" do
    begin
      ['2009-10-11 12:13:14',
       '2009-10-11 12:13:14.000000001',
       '2009-10-11 12:13:14.999999999'].each do |time_str|
        result = fetch_a_timestamp(time_str, 'yyyy-mm-dd hh24:mi:ss.ff9')
        expected_val = DateTime.parse("2009-10-11 12:13:14#{local_timezone}")
        result.should eql(expected_val)
        result.offset.to_r.should eql(expected_val.offset.to_r)
      end
    ensure
      OCI8::BindType.default_timezone = :local
    end
  end
end

describe OCI8::Cursor, "when fetching a timestamp with time zone as DateTime by Oracle 8.0 API" do
  include FetchDateTime8Helper

  before :all do
    before_all
  end

  after :all do
    after_all
  end

  # fetch a timestamp with time zone
  def fetch_a_timestamp_tz(str, fmt = 'yyyy-mm-dd hh24:mi:ss tzh:tzm')
    cursor = @conn.exec("select to_timestamp_tz(:1, :2) from dual", str, fmt)
    begin
      cursor.column_metadata[0].data_type.should eql(:timestamp_tz)
      cursor.fetch[0]
    ensure
      cursor.close
    end
  end

  it "should get a DateTime whose time zone is OCI8::BindType.default_timezone" do
    begin
      [:utc, :local].each do |tz|
        OCI8::BindType.default_timezone = tz
        result = fetch_a_timestamp_tz('2009-10-11 12:13:14 -07:00')
        expected_val = DateTime.parse("2009-10-11T12:13:14#{tz == :utc ? '+00:00' : local_timezone}")
        result.should eql(expected_val)
        result.offset.to_r.should eql(expected_val.offset.to_r)
      end
    ensure
      OCI8::BindType.default_timezone = :local
    end
  end

  it "should get a DateTime whose fractional seconds are truncated" do
    begin
      ['2009-10-11 12:13:14',
       '2009-10-11 12:13:14.000000001',
       '2009-10-11 12:13:14.999999999'].each do |time_str|
        result = fetch_a_timestamp_tz(time_str, 'yyyy-mm-dd hh24:mi:ss.ff9')
        expected_val = DateTime.parse("2009-10-11T12:13:14#{local_timezone}")
        result.should eql(expected_val)
        result.offset.to_r.should eql(expected_val.offset.to_r)
      end
    ensure
      OCI8::BindType.default_timezone = :local
    end
  end
end

describe OCI8::Cursor, "when fetching a timestamp with local time zone as DateTime by Oracle 8.x API" do
  include FetchDateTime8Helper

  before :all do
    before_all
  end

  after :all do
    after_all
  end

  # fetch a timestamp with local time zone
  def fetch_a_timestamp_ltz(str, fmt = 'yyyy-mm-dd hh24:mi:ss tzh:tzm')
    cursor = @conn.exec("select cast(to_timestamp_tz(:1, :2) as timestamp(9) with local time zone) from dual", str, fmt)
    begin
      cursor.column_metadata[0].data_type.should eql(:timestamp_ltz)
      timestamp = cursor.fetch[0]
    ensure
      cursor.close
    end
  end

  it "should get a DateTime whose time zone is OCI8::BindType.default_timezone" do
    begin
      [:utc, :local].each do |tz|
        OCI8::BindType.default_timezone = tz
        result = fetch_a_timestamp_ltz('2009-10-11 12:13:14')
        expected_val = DateTime.parse("2009-10-11T12:13:14#{tz == :utc ? '+00:00' : local_timezone}")
        result.should eql(expected_val)
        result.offset.to_r.should eql(expected_val.offset.to_r)
      end
    ensure
      OCI8::BindType.default_timezone = :local
    end
  end

  it "should get a DateTime whose fractional seconds are truncated" do
    begin
      ['2009-10-11 12:13:14',
       '2009-10-11 12:13:14.000000001',
       '2009-10-11 12:13:14.999999999'].each do |time_str|
        result = fetch_a_timestamp_ltz(time_str, 'yyyy-mm-dd hh24:mi:ss.ff9')
        expected_val = DateTime.parse("2009-10-11T12:13:14#{local_timezone}")
        result.should eql(expected_val)
        result.offset.to_r.should eql(expected_val.offset.to_r)
      end
    ensure
      OCI8::BindType.default_timezone = :local
    end
  end
end
