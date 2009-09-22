#
# Specs for OCI8::Cursor when fetching date, timestamp, timestamp with
# time zone and tiemstamp with local time zone as DateTime
#
require File.join(File.dirname(__FILE__), 'spec_helper.rb')

module FetchDateTimeHelper
  def before_all
    @conn = get_oracle_connection()
    @default_timezone = @conn.select_one("select sessiontimezone from dual")[0]
    OCI8::BindType::Mapping[:date] = OCI8::BindType::DateTime
    OCI8::BindType::Mapping[:timestamp] = OCI8::BindType::DateTime
    OCI8::BindType::Mapping[:timestamp_tz] = OCI8::BindType::DateTime
    OCI8::BindType::Mapping[:timestamp_ltz] = OCI8::BindType::DateTime
  end

  def after_all
    OCI8::BindType::Mapping[:date] = OCI8::BindType::Time
    OCI8::BindType::Mapping[:timestamp] = OCI8::BindType::Time
    OCI8::BindType::Mapping[:timestamp_tz] = OCI8::BindType::Time
    OCI8::BindType::Mapping[:timestamp_ltz] = OCI8::BindType::Time
    @conn.logoff
  end

end

describe OCI8::Cursor, "when fetching a date as DateTime" do
  include FetchDateTimeHelper

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

  it "should get a DateTime of local time if the session time zone is local" do
    result = fetch_a_date('2009-10-11 12:13:14')
    expected_val = DateTime.parse("2009-10-11T12:13:14#{@default_timezone}")
    result.should eql(expected_val)
    result.offset.should eql(expected_val.offset)
  end

  it "should get a DateTime of any session time zone" do
    begin
      ['+09:00', '+00:00', '-05:00'].each do |timezone|
        @conn.exec("alter session set time_zone = '#{timezone}'")
        result = fetch_a_date('2009-10-11 12:13:14')
        expected_val = DateTime.parse("2009-10-11T12:13:14#{timezone}")
        result.should eql(expected_val)
        result.offset.should eql(expected_val.offset)
      end
    ensure
      @conn.exec("alter session set time_zone = '#{@default_timezone}'")
    end
  end
end

describe OCI8::Cursor, "when fetching a timestamp as DateTime" do
  include FetchDateTimeHelper

  before :all do
    before_all
  end

  after :all do
    after_all
  end

  def fetch_a_timestamp(str, fmt = 'yyyy-mm-dd hh24:mi:ss.ff9')
    cursor = @conn.exec("select to_timestamp(:1, :2) from dual", str, fmt)
    begin
      cursor.column_metadata[0].data_type.should eql(:timestamp)
      cursor.fetch[0]
    ensure
      cursor.close
    end
  end

  it "should get a DateTime of local time if the session time zone is local" do
    result = fetch_a_timestamp('2009-10-11 12:13:14.123456789')
    expected_val = DateTime.parse("2009-10-11T12:13:14.123456789#{@default_timezone}")
    result.should eql(expected_val)
    result.offset.should eql(expected_val.offset)
  end

  it "should get a DateTime of any session time zone" do
    begin
      ['+09:00', '+00:00', '-05:00'].each do |timezone|
        @conn.exec("alter session set time_zone = '#{timezone}'")
        result = fetch_a_timestamp('2009-10-11 12:13:14.999999999')
        expected_val = DateTime.parse("2009-10-11T12:13:14.999999999#{timezone}")
        result.should eql(expected_val)
        result.offset.should eql(expected_val.offset)
      end
    ensure
      @conn.exec("alter session set time_zone = '#{@default_timezone}'")
    end
  end
end

describe OCI8::Cursor, "when fetching a timestamp with time zone as DateTime" do
  include FetchDateTimeHelper

  before :all do
    before_all
  end

  after :all do
    after_all
  end

  # fetch a timestamp with time zone
  def fetch_a_timestamp_tz(str, fmt = 'yyyy-mm-dd hh24:mi:ss.ff9 tzh:tzm')
    cursor = @conn.exec("select to_timestamp_tz(:1, :2) from dual", str, fmt)
    begin
      cursor.column_metadata[0].data_type.should eql(:timestamp_tz)
      cursor.fetch[0]
    ensure
      cursor.close
    end
  end

  it "should get a DateTime of local time if the time zone is local" do
    result = fetch_a_timestamp_tz("2009-10-11 12:13:14.123456789")
    expected_val = DateTime.parse("2009-10-11T12:13:14.123456789#{@default_timezone}")
    result.should eql(expected_val)
    result.offset.should eql(expected_val.offset)
  end

  it "should get a DateTime of any session time zone" do
    ['+09:00', '+00:00', '-05:00'].each do |timezone|
      result = fetch_a_timestamp_tz("2009-10-11 12:13:14.999999999 #{timezone}")
      expected_val = DateTime.parse("2009-10-11T12:13:14.999999999#{timezone}")
      result.should eql(expected_val)
      result.offset.should eql(expected_val.offset)
    end
  end
end

describe OCI8::Cursor, "when fetching a timestamp with local time zone as DateTime" do
  include FetchDateTimeHelper

  before :all do
    before_all
  end

  after :all do
    after_all
  end

  # fetch a timestamp with local time zone
  def fetch_a_timestamp_ltz(str, fmt = 'yyyy-mm-dd hh24:mi:ss.ff9 tzh:tzm')
    cursor = @conn.exec("select cast(to_timestamp_tz(:1, :2) as timestamp(9) with local time zone) from dual", str, fmt)
    begin
      cursor.column_metadata[0].data_type.should eql(:timestamp_ltz)
      timestamp = cursor.fetch[0]
    ensure
      cursor.close
    end
  end

  it "should get a DateTime of local time if the time zone is local" do
    result = fetch_a_timestamp_ltz("2009-10-11 12:13:14.123456789")
    expected_val = DateTime.parse("2009-10-11T12:13:14.123456789#{@default_timezone}")
    result.should eql(expected_val)
    result.offset.should eql(expected_val.offset)
  end

  it "should get a DateTime of any session time zone" do
    begin
      ['+09:00', '+00:00', '-05:00'].each do |timezone|
        @conn.exec("alter session set time_zone = '#{timezone}'")
        result = fetch_a_timestamp_ltz("2009-10-11 12:13:14.999999999 #{timezone}")
        expected_val = DateTime.parse("2009-10-11T12:13:14.999999999#{timezone}")
        result.should eql(expected_val)
        result.offset.should eql(expected_val.offset)
      end
    ensure
      @conn.exec("alter session set time_zone = '#{@default_timezone}'")
    end
  end

end
