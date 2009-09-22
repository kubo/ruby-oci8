#
# Specs for OCI8::Cursor when fetching date, timestamp, timestamp with
# time zone and tiemstamp with local time zone as Time
#
require File.join(File.dirname(__FILE__), 'spec_helper.rb')

module FetchTimeHelper
  def before_all
    @conn = get_oracle_connection()
    @default_timezone = @conn.select_one("select sessiontimezone from dual")[0]
  end

  def after_all
    @conn.logoff
  end
end


describe OCI8::Cursor, "when fetching a date as Time" do
  include FetchTimeHelper

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

  it "should get a Time if the session time zone is local" do
    result = fetch_a_date('2009-10-11 12:13:14')
    result.should eql(Time.local(2009, 10, 11, 12, 13, 14))
    result.utc_offset.should eql(Time.now.utc_offset)
  end

  it "should get a Time if the session time zone is UTC" do
    begin
      @conn.exec("alter session set time_zone = '00:00'")
      result = fetch_a_date('2009-10-11 12:13:14')
      result.should eql(Time.utc(2009, 10, 11, 12, 13, 14))
      result.utc_offset.should eql(0)
    ensure
      @conn.exec("alter session set time_zone = '#{@default_timezone}'")
    end
  end

  it "should get a DateTime if the session time zone is neither local nor UTC prior to ruby 1.9.2" do
    if RUBY_VERSION < '1.9.2'
      begin
        ['+09:00', '-05:00', '-07:00'].each do |timezone|
          next if timezone == @default_timezone
          @conn.exec("alter session set time_zone = '#{timezone}'")
          result = fetch_a_date('2009-10-11 12:13:14')
          result.should eql(DateTime.parse("2009-10-11T12:13:14#{timezone}"))
          result.zone.should eql(timezone)
        end
      ensure
        @conn.exec("alter session set time_zone = '#{@default_timezone}'")
      end
    end
  end

  it "should get a Time in any session time zone after ruby 1.9.2" do
    if RUBY_VERSION >= '1.9.2'
      begin
        timezone = ['+09:00', '-05:00'].detect {|tz| tz != @default_timezone}
        @conn.exec("alter session set time_zone = '#{timezone}'")
        result = fetch_a_date('2009-10-11 12:13:14')
        result.should eql(Time.new(2009, 10, 11, 12, 13, 14, timezone))
        result.utc_offset.should eql(Time.new.getlocal(timezone).utc_offset)
      ensure
        @conn.exec("alter session set time_zone = '#{@default_timezone}'")
      end
    end
  end

  it "should get a Time if the year is in the valid year range, otherwise a DateTime" do
    ['1888', '1971', '1972', '2038', '2039', '9999', '-4711', '-1', '0001', '0138', '0139'].each do |year|
      result = fetch_a_date("#{year}-01-01", 'syyyy-mm-dd')
      expected_val = nil
      if RUBY_VERSION >= '1.9.2'
        expected_val = Time.new(year, 1, 1, 0, 0, 0, @default_timezone)
      else
        begin
          expected_val = Time.local(year)
        rescue ArgumentError
        end
        if expected_val.nil? ||
            expected_val.year != year.to_i ||
            expected_val.strftime('%z') != @default_timezone.gsub(/:/, '') then
          expected_val = DateTime.parse("#{year}-01-01T00:00#{@default_timezone}")
        end
      end
      result.should eql(expected_val)
    end
  end
end

describe OCI8::Cursor, "when fetching a timestamp as Time" do
  include FetchTimeHelper

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

  it "should get a Time whose precision is microsecond if the ruby is 1.8" do
    result = fetch_a_timestamp('2009-10-11 12:13:14.123456789', 'yyyy-mm-dd hh24:mi:ss.ff9')
    result.usec.should eql(123456)
    result = fetch_a_timestamp('2009-10-11 12:13:59.999999999', 'yyyy-mm-dd hh24:mi:ss.ff9')
    result.usec.should eql(999999)
  end

  it "should get a Time whose precision is nanosecond if the ruby is 1.9" do
    next unless Time.public_method_defined? :nsec
    result = fetch_a_timestamp('2009-10-11 12:13:14.123456789', 'yyyy-mm-dd hh24:mi:ss.ff9')
    result.nsec.should eql(123456789)
    result = fetch_a_timestamp('2009-10-11 12:13:59.999999999', 'yyyy-mm-dd hh24:mi:ss.ff9')
    result.nsec.should eql(999999999)
  end

  it "should get a Time if the session time zone is local" do
    result = fetch_a_timestamp('2009-10-11 12:13:14')
    result.should eql(Time.local(2009, 10, 11, 12, 13, 14))
    result.utc_offset.should eql(Time.now.utc_offset)
  end

  it "should get a Time if the session time zone is UTC" do
    begin
      @conn.exec("alter session set time_zone = '00:00'")
      result = fetch_a_timestamp('2009-10-11 12:13:14')
      result.should eql(Time.utc(2009, 10, 11, 12, 13, 14))
      result.utc_offset.should eql(0)
    ensure
      @conn.exec("alter session set time_zone = '#{@default_timezone}'")
    end
  end

  it "should get a DateTime if the session time zone is neither local nor UTC prior to ruby 1.9.2" do
    if RUBY_VERSION < '1.9.2'
      begin
        timezone = ['+09:00', '-05:00'].detect {|tz| tz != @default_timezone}
        @conn.exec("alter session set time_zone = '#{timezone}'")
        result = fetch_a_timestamp('2009-10-11 12:13:14')
        result.should eql(DateTime.parse("2009-10-11T12:13:14#{timezone}"))
        result.zone.should eql(timezone)
      ensure
        @conn.exec("alter session set time_zone = '#{@default_timezone}'")
      end
    end
  end

  it "should get a Time in any session time zone after ruby 1.9.2" do
    if RUBY_VERSION >= '1.9.2'
      begin
        timezone = ['+09:00', '-05:00'].detect {|tz| tz != @default_timezone}
        @conn.exec("alter session set time_zone = '#{timezone}'")
        result = fetch_a_timestamp('2009-10-11 12:13:14')
        result.should eql(Time.new(2009, 10, 11, 12, 13, 14, timezone))
        result.utc_offset.should eql(Time.new.getlocal(timezone).utc_offset)
      ensure
        @conn.exec("alter session set time_zone = '#{@default_timezone}'")
      end
    end
  end

  it "should get a Time if the year is in the valid year range, otherwise a DateTime" do
    ['1888', '1971', '1972', '2038', '2039', '9999', '-4711', '-1', '0001', '0138', '0139'].each do |year|
      result = fetch_a_timestamp("#{year}-01-01", 'syyyy-mm-dd')
      expected_val = nil
      if RUBY_VERSION >= '1.9.2'
        expected_val = Time.new(year, 1, 1, 0, 0, 0, @default_timezone)
      else
        begin
          expected_val = Time.local(year)
        rescue ArgumentError
        end
        if expected_val.nil? ||
            expected_val.year != year.to_i ||
            expected_val.strftime('%z') != @default_timezone.gsub(/:/, '') then
          expected_val = DateTime.parse("#{year}-01-01T00:00#{@default_timezone}")
        end
      end
      result.should eql(expected_val)
    end
  end
end

describe OCI8::Cursor, "when fetching a timestamp with time zone as Time" do
  include FetchTimeHelper

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

  it "should get a Time whose precision is microsecond if the ruby is 1.8" do
    result = fetch_a_timestamp_tz("2009-10-11 12:13:14.123456789 #{DateTime.now.zone}", 'yyyy-mm-dd hh24:mi:ss.ff9 tzh:tzm')
    result.should be_a_kind_of(Time)
    result.usec.should eql(123456)
  end

  it "should get a Time whose precision is nanosecond if the ruby is 1.9" do
    next unless Time.public_method_defined? :nsec
    result = fetch_a_timestamp_tz("2009-10-11 12:13:14.123456789 #{DateTime.now.zone}", 'yyyy-mm-dd hh24:mi:ss.ff9 tzh:tzm')
    result.should be_a_kind_of(Time)
    result.nsec.should eql(123456789)
  end

  it "should get a Time if the time zone is local" do
    result = fetch_a_timestamp_tz("2009-10-11 12:13:14 #{DateTime.now.zone}")
    result.should eql(Time.local(2009, 10, 11, 12, 13, 14))
    result.utc_offset.should eql(Time.now.utc_offset)
  end

  it "should get a Time if the time zone is UTC" do
    result = fetch_a_timestamp_tz("2009-10-11 12:13:14 00:00")
    result.should eql(Time.utc(2009, 10, 11, 12, 13, 14))
    result.utc_offset.should eql(0)
  end

  it "should get a DateTime if the time zone is neither local nor UTC prior to ruby 1.9.2" do
    if RUBY_VERSION < '1.9.2'
      timezone = ['+09:00', '-05:00'].detect {|tz| tz != @default_timezone}
      result = fetch_a_timestamp_tz("2009-10-11 12:13:14 #{timezone}")
      result.should eql(DateTime.parse("2009-10-11T12:13:14#{timezone}"))
      result.zone.should eql(timezone)
    end
  end

  it "should get a Time in any time zone after ruby 1.9.2" do
    if RUBY_VERSION >= '1.9.2'
      timezone = ['+09:00', '-05:00'].detect {|tz| tz != DateTime.now.zone}
      result = fetch_a_timestamp_tz("2009-10-11 12:13:14 #{timezone}")
      result.should eql(Time.new(2009, 10, 11, 12, 13, 14, timezone))
      result.utc_offset.should eql(Time.new.getlocal(timezone).utc_offset)
    end
  end
end

describe OCI8::Cursor, "when fetching a timestamp with local time zone as Time" do
  include FetchTimeHelper

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

  it "should get a Time whose precision is microsecond if the ruby is 1.8" do
    result = fetch_a_timestamp_ltz('2009-10-11 12:13:14.123456789 00:00', 'yyyy-mm-dd hh24:mi:ss.ff9 tzh:tzm')
    result.should be_a_kind_of(Time)
    result.usec.should eql(123456)
  end

  it "should get a Time whose precision is nanosecond if the ruby is 1.9" do
    next unless Time.public_method_defined? :nsec
    result = fetch_a_timestamp_ltz('2009-10-11 12:13:14.123456789 00:00', 'yyyy-mm-dd hh24:mi:ss.ff9 tzh:tzm')
    result.should be_a_kind_of(Time)
    result.nsec.should eql(123456789)
  end

  it "should get a Time if the session time zone is local" do
    result = fetch_a_timestamp_ltz('2009-10-11 12:13:14 00:00')
    result.should eql(Time.utc(2009, 10, 11, 12, 13, 14))
    result.utc_offset.should eql(Time.now.utc_offset)
  end

  it "should get a Time if the session time zone is UTC" do
    begin
      @conn.exec("alter session set time_zone = '00:00'")
      result = fetch_a_timestamp_ltz('2009-10-11 12:13:14 00:00')
      result.should eql(Time.utc(2009, 10, 11, 12, 13, 14))
      result.utc_offset.should eql(0)
    ensure
      @conn.exec("alter session set time_zone = '#{@default_timezone}'")
    end
  end

  it "should get a DateTime if the session time zone is neither local nor UTC prior to ruby 1.9.2" do
    if RUBY_VERSION < '1.9.2'
      timezone = ['+09:00', '-05:00'].detect {|tz| tz != @default_timezone}
      begin
        @conn.exec("alter session set time_zone = '#{timezone}'")
        result = fetch_a_timestamp_ltz('2009-10-11 12:13:14 00:00')
        result.should eql(DateTime.parse("2009-10-11T12:13:14+00:00"))
        result.zone.should eql(timezone)
      ensure
        @conn.exec("alter session set time_zone = '#{@default_timezone}'")
      end
    end
  end

  it "should get a Time in any session time zone after ruby 1.9.2" do
    if RUBY_VERSION >= '1.9.2'
      timezone = ['+09:00', '-05:00'].detect {|tz| tz != @default_timezone}
      begin
        @conn.exec("alter session set time_zone = '#{timezone}'")
        result = fetch_a_timestamp_ltz('2009-10-11 12:13:14 00:00')
        result.should eql(Time.new(2009, 10, 11, 12, 13, 14, '+00:00'))
        result.utc_offset.should eql(Time.new.getlocal(timezone).utc_offset)
      ensure
        @conn.exec("alter session set time_zone = '#{@default_timezone}'")
      end
    end
  end
end
