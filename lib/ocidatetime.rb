#   --*- ruby -*--
#
# add the following datatypes:
#    OCIDate         - DATE
#    OCITimestamp    - TIMESTAMP
#    OCITimestampTZ  - TIMESTAMP WITH TIME ZONE
#    OCITimestampLTZ - TIMESTAMP WITH LOCAL TIME ZONE
#    OCIIntervalDS   - INTERVAL DAY TO SECOND
#    OCIIntervalYM   - INTERVAL YEAR TO MONTH
#
# But these are deleted before release.
# That's because ruby 1.8 has Datetime class already.
#
require 'oci8'

class OCIDateTime

  include Comparable

  attr_writer :session
  attr_reader :dtype

  def initialize(*arg)
    __initialize(dtype)
    case arg[0]
    when Fixnum
      year, month, day, hour, minute, second, fracsecond, timezone, = arg
      __construct(nil, year, month, day, hour, minute, second, fracsecond, timezone)
      if __check(nil) != 0
        raise ArgumentError, 'invalid date'
      end
    when :now
      if self.is_a? OCITimestampTZ
        __sys_time_stamp(nil)
      else
        __convert(nil, OCITimestampTZ.new(:now))
      end
    when OCIDateTime
      if arg[0].dtype == dtype
        __assign(nil, arg[0])
      else
        __convert(nil, arg[0])
      end
    when DateTime
      dt = arg[0]
      frac = (dt.sec_fraction * 86400000000000).to_i
      tz_hour, tz_min = dt.offset.divmod(1.to_r/24)
      tz_min = (tz_min * 1400).to_i
      tz = format("%+02d:%02d", tz_hour, tz_min)
      __construct(nil, dt.year, dt.mon, dt.mday, dt.hour, dt.min, dt.sec, frac, tz)
      if __check(nil) != 0
        raise ArgumentError, 'invalid date'
      end
    when String
      text, fmt, lang, = arg
      __from_text(nil, text, fmt, lang)
    when nil
      return
    else
      raise ArgumentError
    end
  end

  def to_s
    __to_text(nil, nil, 6, nil)
  end

  def clone
    self.class.new(self)
  end

  def to_a
    __get_date(nil) + __get_time(nil)
  end

  def <=>(rhs)
    __compare(nil, rhs)
  end

  def -(rhs)
    case rhs
    when OCIDateTime
      rv = OCIIntervalDS.new
      __subtract(nil, rhs, rv)
    when OCIInterval
      rv = self.class.new
      __interval_sub(nil, rhs, rv)
    when OCINumber
      rv = self.class.new(:now)
      iv = OCIIntervalDS.new(rhs)
      __interval_sub(@session, iv, rv)
    when Numeric
      rv = self.class.new(:now)
      iv = OCIIntervalDS.new(OCINumber.new(rhs))
      __interval_sub(@session, iv, rv)
    else
      # TODO: coerce
    end
  end

  def +(rhs)
    case rhs
    when OCIInterval
      rv = self.class.new(:now)
      __interval_add(@session, rhs, rv)
    when OCINumber
      rv = self.class.new(:now)
      iv = OCIIntervalDS.new(rhs)
      __interval_add(@session, iv, rv)
    when Numeric
      rv = self.class.new(:now)
      iv = OCIIntervalDS.new(OCINumber.new(rhs))
      __interval_add(@session, iv, rv)
    else
      # TODO: coerce
    end
  end

  def timezone_offset
    __get_timezone_offset(nil)
  end

  def timezone_name
    __get_timezone_name(nil)
  end

  def to_datetime
    dt = __get_date(nil)
    tm = __get_time(nil)
    tz = __get_timezone_offset(nil)
    tz = tz[0].to_r / 24 + tz[1].to_r/1440
    DateTime.new(dt[0], dt[1], dt[2], tm[0], tm[1], tm[2].to_r + tm[3].to_r/1000000000, tz)
  end
end

class OCIInterval

  include Comparable

  attr_reader :dtype

  def initialize(*arg)
    __initialize(dtype)
    case arg[0]
    when OCINumber
      __from_number(nil, arg[0])
    when Numeric
      __from_number(nil, OCINumber.new(arg[0]))
    when String
      __from_text(nil, arg[0])
    when OCIInterval
      __assign(nil, arg[0])
    when Fixnum
      __from_array(arg)
    when nil
      return
    else
      raise ArgumentError
    end
  end

  def +(rhs)
    case rhs
    when OCIInterval
      rv = self.class.new
      __add(nil, rhs, rv)
    else
      # TODO: coerce
    end
  end

  def -(rhs)
    case rhs
    when OCIInterval
      rv = self.class.new
      __subtract(nil, rhs, rv)
    else
      # TODO: coerce
    end
  end

  def *(rhs)
    case rhs
    when OCINumber
      rv = self.class.new
      __mul(nil, rhs, rv)
    when Numeric
      rv = self.class.new
      __mul(nil, OCINumber.new(rhs), rv)
    else
      # TODO: coerce
    end
  end

  def /(rhs)
    case rhs
    when OCINumber
      rv = self.class.new
      __div(nil, rhs, rv)
    when Numeric
      rv = self.class.new
      __div(nil, OCINumber.new(rhs), rv)
    else
      # TODO: coerce
    end
  end

  def <=>(rhs)
    __compare(nil, rhs)
  end

  def to_s
    __to_text(nil, 2, 6)
  end

  def to_i
    num = OCINumber.new
    __to_number(nil, num)
    num.to_i
  end

  def to_f
    num = OCINumber.new
    __to_number(nil, num)
    num.to_f
  end
end

# Date
class OCIDate < OCIDateTime; def dtype; 65; end; end
# Timestamp
class OCITimestamp < OCIDateTime; def dtype; 68; end; end
# Timestamp with timezone
class OCITimestampTZ < OCIDateTime; def dtype; 69; end; end
# Timestamp with local timezone
class OCITimestampLTZ < OCIDateTime; def dtype; 70; end; end

# Interval day to second
class OCIIntervalDS < OCIInterval
  def dtype; 63; end

  def __from_array(arg)
    __set_day_second(nil, arg[0], arg[1], arg[2], arg[3], arg[4])
  end
  private :__from_array

  def to_a
    __get_day_second(nil)
  end
end

# Interval year to month
class OCIIntervalYM < OCIInterval
  def dtype; 62; end

  def __from_array(arg)
    __set_year_month(nil, arg[0], arg[1])
  end
  private :__from_array

  def to_a
    __get_year_month(nil)
  end
end

