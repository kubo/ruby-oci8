require 'date'

class OCI8

  module BindType

    # call-seq:
    #   OCI8::BindType.default_timezone -> :local or :utc
    #
    # Returns the default time zone when using Oracle 8.x client.
    # The value is unused when using Oracle 9i or upper client.
    #
    # See also: OCI8::BindType::Time
    def self.default_timezone
      OCI8::BindType::Util.default_timezone
    end

    # call-seq:
    #   OCI8::BindType.default_timezone = :local or :utc
    #
    # Sets the default time zone when using Oracle 8.x client.
    # The value is unused when using Oracle 9i or upper client.
    #
    # See also: OCI8::BindType::Time
    def self.default_timezone=(tz)
      OCI8::BindType::Util.default_timezone = tz
    end

    module Util # :nodoc:

      @@datetime_fsec_base = (1 / ::DateTime.parse('0001-01-01 00:00:00.000000001').sec_fraction).to_i

      @@default_timezone = :local
      begin
        Time.new(2001, 1, 1, 0, 0, 0, '+00:00')
        @@time_new_accepts_timezone = true  # after ruby 1.9.2
      rescue ArgumentError
        @@time_new_accepts_timezone = false # prior to ruby 1.9.2
      end

      begin
        # 2001-01-01 00:00:59.999
        ::DateTime.civil(2001, 1, 1, 0, 0, Rational(59_999, 1000), 0)
        @@datetime_has_fractional_second_bug = false
      rescue ArgumentError
        @@datetime_has_fractional_second_bug = true
      end

      def self.default_timezone
        @@default_timezone
      end

      def self.default_timezone=(tz)
        if tz != :local and tz != :utc
          raise ArgumentError, "expected :local or :utc but #{tz}"
        end
        @@default_timezone = tz
      end

      private

      def datetime_to_array(val, datatype)
        return nil if val.nil?

        # year
        year = val.year
        # month
        if val.respond_to? :mon
          month = val.mon
        elsif val.respond_to? :month
          month = val.month
        else
          raise "expect Time, Date or DateTime but #{val.class}"
        end
        # day
        if val.respond_to? :mday
          day = val.mday
        elsif val.respond_to? :day
          day = val.day
        else
          raise "expect Time, Date or DateTime but #{val.class}"
        end
        # hour
        if val.respond_to? :hour
          hour = val.hour
        else
          hour = 0
        end
        # minute
        if val.respond_to? :min
          minute = val.min
        else
          minute = 0
        end
        # second
        if val.respond_to? :sec
          sec = val.sec
        else
          sec = 0
        end
        return [year, month, day, hour, minute, sec] if datatype == :date

        # fractional second
        if val.respond_to? :sec_fraction
          fsec = (val.sec_fraction * @@datetime_fsec_base).to_i
        elsif val.respond_to? :nsec
          fsec = val.nsec
        elsif val.respond_to? :usec
          fsec = val.usec * 1000
        else
          fsec = 0
        end
        return [year, month, day, hour, minute, sec, fsec, nil, nil] if datatype == :timestamp

        # time zone
        if val.respond_to? :offset
          # DateTime
          tz_min = (val.offset * 1440).to_i
        elsif val.respond_to? :utc_offset
          # Time
          tz_min = val.utc_offset / 60
        else
          tz_hour = nil
          tz_min = nil
        end
        if tz_min
          if tz_min < 0
            tz_min = - tz_min
            tz_hour = - (tz_min / 60)
            tz_min = (tz_min % 60)
          else
            tz_hour = tz_min / 60
            tz_min = tz_min % 60
          end
        end
        [year, month, day, hour, minute, sec, fsec, tz_hour, tz_min]
      end

      def array_to_datetime(ary, timezone)
        return nil if ary.nil?

        year, month, day, hour, minute, sec, nsec, tz_hour, tz_min = ary
        sec += nsec.to_r / 1000000000 if nsec and nsec != 0
        if tz_hour and tz_min
          offset = tz_hour.to_r / 24 + tz_min.to_r / 1440
        else
          if @@default_timezone == :local
            if ::DateTime.respond_to? :local_offset
              offset = ::DateTime.local_offset # Use a method defined by active support.
            else
              # Do as active support does.
              offset = ::Time.local(2007).utc_offset.to_r / 86400
            end
          else
            offset = 0
          end
        end

        if @@datetime_has_fractional_second_bug and sec >= 59 and nsec != 0
          # convert to a DateTime via a String as a workaround
          if offset >= 0
            sign = ?+
          else
            sign = ?-
            offset = - offset;
          end
          tz_min = (offset * 1440).to_i
          tz_hour, tz_min = tz_min.divmod 60
          time_str = format("%04d-%02d-%02dT%02d:%02d:%02d.%09d%c%02d:%02d",
                            year, month, day, hour, minute, sec, nsec, sign, tz_hour, tz_min)
          ::DateTime.parse(time_str)
        else
          ::DateTime.civil(year, month, day, hour, minute, sec, offset)
        end
      end

      if @@time_new_accepts_timezone

        # after ruby 1.9.2
        def array_to_time(ary, timezone)
          return nil if ary.nil?

          year, month, day, hour, minute, sec, nsec, tz_hour, tz_min = ary
          nsec ||= 0

          if timezone
            usec = (nsec == 0) ? 0 : nsec.to_r / 1000
            ::Time.send(timezone, year, month, day, hour, minute, sec, usec)
          else
            sec += nsec.to_r / 1_000_000_000 if nsec != 0
            utc_offset = tz_hour * 3600 + tz_min * 60
            ::Time.new(year, month, day, hour, minute, sec, utc_offset)
          end
        end

      else

        # prior to ruby 1.9.2
        def array_to_time(ary, timezone)
          return nil if ary.nil?

          year, month, day, hour, minute, sec, nsec, tz_hour, tz_min = ary
          nsec ||= 0
          usec = (nsec == 0) ? 0 : nsec.to_r / 1000
          begin
            if timezone
              return ::Time.send(timezone, year, month, day, hour, minute, sec, usec)
            else
              if tz_hour == 0 and tz_min == 0
                tm = ::Time.utc(year, month, day, hour, minute, sec, usec)
                # Time.utc(99, ...) returns a time object the year of which is 1999.
                # 'tm.year == year' checks such cases.
                return tm if tm.year == year
              else
                tm = ::Time.local(year, month, day, hour, minute, sec, usec)
                return tm if tm.utc_offset == tz_hour * 3600 + tz_min * 60 and tm.year == year
              end
            end
          rescue StandardError
          end
          array_to_datetime(ary, timezone)
        end

      end
    end

    #--
    # OCI8::BindType::DateTime
    #++
    # This is a helper class to select or bind Oracle data types such as
    # <tt>DATE</tt>, <tt>TIMESTAMP</tt>, <tt>TIMESTAMP WITH TIME ZONE</tt>
    # and <tt>TIMESTAMP WITH LOCAL TIME ZONE</tt>. The retrieved value
    # is a \DateTime.
    #
    # === How to select \DataTime values.
    #
    # <tt>DATE</tt>, <tt>TIMESTAMP</tt>, <tt>TIMESTAMP WITH TIME ZONE</tt>
    # and <tt>TIMESTAMP WITH LOCAL TIME ZONE</tt> are selected as a \Time
    # by default. You change the behaviour by explicitly calling
    # OCI8::Cursor#define as follows:
    #
    #   cursor = conn.parse("SELECT hiredate FROM emp")
    #   cursor.define(1, nil, DateTime)
    #   cursor.exec()
    #
    # Otherwise, you can change the default mapping for all queries.
    #
    #   # Changes the mapping for DATE
    #   OCI8::BindType::Mapping[OCI8::SQLT_DAT] = OCI8::BindType::DateTime
    #   
    #   # Changes the mapping for TIMESTAMP
    #   OCI8::BindType::Mapping[OCI8::SQLT_TIMESTAMP] = OCI8::BindType::DateTime
    #   
    #   # Changes the mapping for TIMESTAMP WITH TIME ZONE
    #   OCI8::BindType::Mapping[OCI8::SQLT_TIMESTAMP_TZ] = OCI8::BindType::DateTime
    #   
    #   # Changes the mapping for TIMESTAMP WITH LOCAL TIME ZONE
    #   OCI8::BindType::Mapping[OCI8::SQLT_TIMESTAMP_LTZ] = OCI8::BindType::DateTime
    #
    # === Note for default time zone
    #
    # The retrieved value's time zone is determined by the session time zone
    # if its data type is <tt>DATE</tt>, <tt>TIMESTAMP</tt> or <tt>TIMESTAMP
    # WITH LOCAL TIME ZONE</tt>.
    #
    # The session time zone is same with local machine's by default.
    # It is changed by the following SQL.
    #
    #   ALTER SESSION SET TIME_ZONE='-05:00'
    #
    # === Note for Oracle 8.x client
    #
    # Timestamp data types and session time zone are new features in
    # Oracle 9i. This class is available only to fetch or bind <tt>DATE</tt>
    # when using Oracle 8.x client.
    #
    # The retrieved value's time zone is determined not by the session
    # time zone, but by the OCI8::BindType.default_timezone
    # The time zone can be changed as follows:
    #
    #  OCI8::BindType.default_timezone = :local
    #  # or
    #  OCI8::BindType.default_timezone = :utc
    #
    # If you are in the regions where daylight saving time is adopted,
    # you should use OCI8::BindType::Time.
    #
    class DateTime < OCI8::BindType::OCITimestampTZ
      include OCI8::BindType::Util

      def set(val) # :nodoc:
        super(datetime_to_array(val, :timestamp_tz))
      end

      def get() # :nodoc:
        array_to_datetime(super(), nil)
      end
    end

    class LocalDateTime < OCI8::BindType::OCITimestamp
      include OCI8::BindType::Util

      def set(val) # :nodoc:
        super(datetime_to_array(val, :timestamp))
      end

      def get() # :nodoc:
        array_to_datetime(super(), :local)
      end
    end

    class UTCDateTime < OCI8::BindType::OCITimestamp
      include OCI8::BindType::Util

      def set(val) # :nodoc:
        super(datetime_to_array(val, :timestamp))
      end

      def get() # :nodoc:
        array_to_datetime(super(), :utc)
      end
    end

    #--
    # OCI8::BindType::Time
    #++
    # This is a helper class to select or bind Oracle data types such as
    # <tt>DATE</tt>, <tt>TIMESTAMP</tt>, <tt>TIMESTAMP WITH TIME ZONE</tt>
    # and <tt>TIMESTAMP WITH LOCAL TIME ZONE</tt>. The retrieved value
    # is a \Time.
    #
    # === How to select \Time values.
    #
    # <tt>DATE</tt>, <tt>TIMESTAMP</tt>, <tt>TIMESTAMP WITH TIME ZONE</tt>
    # and <tt>TIMESTAMP WITH LOCAL TIME ZONE</tt> are selected as a \Time
    # by default. If the default behaviour is changed, you can select it
    # as a \Time by explicitly calling OCI8::Cursor#define as follows:
    #
    #   cursor = conn.parse("SELECT hiredate FROM emp")
    #   cursor.define(1, nil, Time)
    #   cursor.exec()
    #
    # === Note for ruby prior to 1.9.2
    #
    # If the retrieved value cannot be represented by \Time, it become
    # a \DateTime. The fallback is done only when the ruby is before 1.9.2
    # and one of the following conditions are met.
    # - The timezone part is neither local nor utc.
    # - The time is out of the time_t[http://en.wikipedia.org/wiki/Time_t].
    #
    # If the retrieved value has the precision of fractional second more
    # than 6, the fractional second is truncated to microsecond, which
    # is the precision of standard \Time class.
    #
    # To avoid this fractional second truncation:
    # - Upgrade to ruby 1.9.2, whose \Time precision is nanosecond.
    # - Otherwise, change the defalt mapping to use \DateTime as follows.
    #    OCI8::BindType::Mapping[OCI8::SQLT_TIMESTAMP] = OCI8::BindType::DateTime
    #    OCI8::BindType::Mapping[OCI8::SQLT_TIMESTAMP_TZ] = OCI8::BindType::DateTime
    #    OCI8::BindType::Mapping[OCI8::SQLT_TIMESTAMP_LTZ] = OCI8::BindType::DateTime
    #
    # === Note for default time zone
    #
    # The retrieved value's time zone is determined by the session time zone
    # if its data type is <tt>DATE</tt>, <tt>TIMESTAMP</tt> or <tt>TIMESTAMP
    # WITH LOCAL TIME ZONE</tt>.
    #
    # The session time zone is same with local machine's by default.
    # It is changed by the following SQL.
    #
    #   ALTER SESSION SET TIME_ZONE='-05:00'
    #
    # === Note for Oracle 8.x client
    #
    # Timestamp data types and session time zone are new features in
    # Oracle 9i. This class is available only to fetch or bind <tt>DATE</tt>
    # when using Oracle 8.x client.
    #
    # The retrieved value's time zone is determined not by the session
    # time zone, but by the OCI8::BindType.default_timezone
    # The time zone can be changed as follows:
    #
    #  OCI8::BindType.default_timezone = :local
    #  # or
    #  OCI8::BindType.default_timezone = :utc
    #
    class Time < OCI8::BindType::OCITimestampTZ
      include OCI8::BindType::Util

      def set(val) # :nodoc:
        super(datetime_to_array(val, :timestamp_tz))
      end

      def get() # :nodoc:
        array_to_time(super(), nil)
      end
    end

    class LocalTime < OCI8::BindType::OCITimestamp
      include OCI8::BindType::Util

      def set(val) # :nodoc:
        super(datetime_to_array(val, :timestamp))
      end

      def get() # :nodoc:
        array_to_time(super(), :local)
      end
    end

    class UTCTime < OCI8::BindType::OCITimestamp
      include OCI8::BindType::Util

      def set(val) # :nodoc:
        super(datetime_to_array(val, :timestamp))
      end

      def get() # :nodoc:
        array_to_time(super(), :utc)
      end
    end

    #--
    # OCI8::BindType::IntervalYM
    #++
    #
    # This is a helper class to select or bind Oracle data type
    # <tt>INTERVAL YEAR TO MONTH</tt>. The retrieved value is
    # the number of months between two timestamps.
    #
    # The value can be applied to \DateTime#>> to shift months.
    # It can be applied to \Time#months_since if activisupport has
    # been loaded.
    #
    # === How to select <tt>INTERVAL YEAR TO MONTH</tt>
    #
    # <tt>INTERVAL YEAR TO MONTH</tt> is selected as an Integer.
    #
    #   conn.exec("select (current_timestamp - hiredate) year to month from emp") do |hired_months|
    #     puts "hired_months = #{hired_months}"
    #   end
    #
    # == How to bind <tt>INTERVAL YEAR TO MONTH</tt>
    #
    # You cannot bind a bind variable as <tt>INTERVAL YEAR TO MONTH</tt> implicitly.
    # It must be bound explicitly by OCI8::Cursor#bind_param.
    #
    #   # output bind variable
    #   cursor = conn.parse(<<-EOS)
    #     BEGIN
    #       :interval := (:ts1 - :ts2) YEAR TO MONTH;
    #     END;
    #   EOS
    #   cursor.bind_param(:interval, nil, :interval_ym)
    #   cursor.bind_param(:ts1, DateTime.parse('1969-11-19 06:54:35 00:00'))
    #   cursor.bind_param(:ts2, DateTime.parse('1969-07-20 20:17:40 00:00'))
    #   cursor.exec
    #   cursor[:interval] # => 4 (months)
    #   cursor.close
    #
    #   # input bind variable
    #   cursor = conn.parse(<<-EOS)
    #     BEGIN
    #       :ts1 := :ts2 + :interval;
    #     END;
    #   EOS
    #   cursor.bind_param(:ts1, nil, DateTime)
    #   cursor.bind_param(:ts2, Date.parse('1969-11-19'))
    #   cursor.bind_param(:interval, 4, :interval_ym)
    #   cursor.exec
    #   cursor[:ts1].strftime('%Y-%m-%d') # => 1970-03-19
    #   cursor.close
    #
    class IntervalYM < OCI8::BindType::OCIIntervalYM
      def set(val) # :nodoc:
        unless val.nil?
          val = [val / 12, val % 12]
        end
        super(val)
      end
      def get() # :nodoc:
        val = super()
        return nil if val.nil?
        year, month = val
        year * 12 + month
      end
    end # OCI8::BindType::IntervalYM

    #--
    # OCI8::BindType::IntervalDS
    #++
    #
    # (new in 2.0)
    #
    # This is a helper class to select or bind Oracle data type
    # <tt>INTERVAL DAY TO SECOND</tt>. The retrieved value is
    # the number of seconds between two typestamps as a \Float.
    #
    # Note that it is the number days as a \Rational if
    # OCI8::BindType::IntervalDS.unit is :day or the ruby-oci8
    # version is prior to 2.0.3.
    #
    # == How to bind <tt>INTERVAL DAY TO SECOND</tt>
    #
    # You cannot bind a bind variable as <tt>INTERVAL DAY TO SECOND</tt>
    # implicitly. It must be bound explicitly by OCI8::Cursor#bind_param.
    #
    #   # output bind variable
    #   cursor = conn.parse(<<-EOS)
    #     BEGIN
    #       :interval := (:ts1 - :ts2) DAY TO SECOND(9);
    #     END;
    #   EOS
    #   cursor.bind_param(:interval, nil, :interval_ds)
    #   cursor.bind_param(:ts1, DateTime.parse('1969-11-19 06:54:35 00:00'))
    #   cursor.bind_param(:ts2, DateTime.parse('1969-07-20 20:17:40 00:00'))
    #   cursor.exec
    #   cursor[:interval] # => 10492615.0 seconds
    #   cursor.close
    #
    #   # input bind variable
    #   cursor = conn.parse(<<-EOS)
    #     BEGIN
    #       :ts1 := :ts2 + :interval;
    #     END;
    #   EOS
    #   cursor.bind_param(:ts1, nil, DateTime)
    #   cursor.bind_param(:ts2, DateTime.parse('1969-07-20 20:17:40 00:00'))
    #   cursor.bind_param(:interval, 10492615.0, :interval_ds)
    #   cursor.exec
    #   cursor[:ts1].strftime('%Y-%m-%d %H:%M:%S') # => 1969-11-19 06:54:35
    #   cursor.close
    #
    class IntervalDS < OCI8::BindType::OCIIntervalDS
      @@hour = 1 / 24.to_r
      @@minute = @@hour / 60
      @@sec = @@minute / 60
      @@fsec = @@sec / 1000000000
      @@unit = :second

      # call-seq:
      #   OCI8::BindType::IntervalDS.unit -> :second or :day
      #
      # (new in 2.0.3)
      #
      # Retrieves the unit of interval.
      def self.unit
        @@unit
      end

      # call-seq:
      #   OCI8::BindType::IntervalDS.unit = :second or :day
      #
      # (new in 2.0.3)
      #
      # Changes the unit of interval. :second is the default.
      def self.unit=(val)
        case val
        when :second, :day
          @@unit = val
        else
          raise 'unit should be :second or :day'
        end
      end

      def set(val) # :nodoc:
        unless val.nil?
          if val < 0
            is_minus = true
            val = -val
          else
            is_minus = false
          end
          if @@unit == :second
            day, val = val.divmod 86400
            hour, val = val.divmod 3600
            minute, val = val.divmod 60
            sec, val = val.divmod 1
          else
            day, val = val.divmod 1
            hour, val = (val * 24).divmod 1
            minute, val = (val * 60).divmod 1
            sec, val = (val * 60).divmod 1
          end
          fsec, val = (val * 1000000000).divmod 1
          if is_minus
            day = - day
            hour = - hour
            minute = - minute
            sec = - sec
            fsec = - fsec
          end
          val = [day, hour, minute, sec, fsec]
        end
        super(val)
      end

      def get() # :nodoc:
        val = super()
        return nil if val.nil?
        day, hour, minute, sec, fsec = val
        if @@unit == :second
          fsec = fsec / 1000000000.0
          day * 86400 + hour * 3600 + minute * 60 + sec + fsec
        else
          day + (hour * @@hour) + (minute * @@minute) + (sec * @@sec) + (fsec * @@fsec)
        end
      end
    end # OCI8::BindType::IntervalDS
  end # OCI8::BindType
end # OCI8
