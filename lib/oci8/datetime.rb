require 'date'

class OCI8

  module BindType

    module Util # :nodoc:

      @@datetime_fsec_base = (1 / ::DateTime.parse('0001-01-01 00:00:00.000000001').sec_fraction).to_i
      @@time_offset = ::Time.now.utc_offset
      @@datetime_offset = ::DateTime.now.offset

      private

      def datetime_to_array(val, full)
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
        return [year, month, day, hour, minute, sec] unless full

        # sec_fraction
        if val.respond_to? :sec_fraction
          fsec = (val.sec_fraction * @@datetime_fsec_base).to_i
        else
          fsec = 0
        end
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

      def ocidate_to_datetime(ary)
        year, month, day, hour, minute, sec = ary
        ::DateTime.civil(year, month, day, hour, minute, sec, @@datetime_offset)
      end

      def ocidate_to_time(ary)
        year, month, day, hour, minute, sec = ary
        begin
          ::Time.local(year, month, day, hour, minute, sec)
        rescue StandardError
          ocidate_to_datetime(ary)
        end
      end

      if OCI8.oracle_client_version >= ORAVER_9_0

        def ocitimestamp_to_datetime(ary)
          year, month, day, hour, minute, sec, fsec, tz_hour, tz_min = ary
          if sec >= 59 and fsec != 0
            # convert to a DateTime via a String as a last resort.
            if tz_hour >= 0 && tz_min >= 0
              sign = ?+
            else
              sign = ?-
              tz_hour = - tz_hour
              tz_min = - tz_min
            end
            time_str = format("%04d-%02d-%02dT%02d:%02d:%02d.%09d%c%02d:%02d",
                              year, month, day, hour, minute, sec, fsec, sign, tz_hour, tz_min)
            ::DateTime.parse(time_str)
          else
            sec += fsec.to_r / 1000000000
            offset = tz_hour.to_r / 24 + tz_min.to_r / 1440
            ::DateTime.civil(year, month, day, hour, minute, sec, offset)
          end
        end

        def ocitimestamp_to_time(ary)
          year, month, day, hour, minute, sec, fsec, tz_hour, tz_min = ary

          if tz_hour == 0 and tz_min == 0
            timezone = :utc
          elsif @@time_offset == tz_hour * 3600 + tz_min * 60
            timezone = :local
          end
          if timezone
            begin
              return ::Time.send(timezone, year, month, day, hour, minute, sec, fsec / 1000)
            rescue StandardError
            end
          end
          ocitimestamp_to_datetime(ary)
        end
      end
    end

    class DateTimeViaOCIDate < OCI8::BindType::OCIDate
      include OCI8::BindType::Util

      def set(val) # :nodoc:
        val &&= datetime_to_array(val, false)
        super(val)
      end

      def get() # :nodoc:
        val = super()
        val ? ocidate_to_datetime(val) : nil
      end
    end

    class TimeViaOCIDate < OCI8::BindType::OCIDate
      include OCI8::BindType::Util

      def set(val) # :nodoc:
        val &&= datetime_to_array(val, false)
        super(val)
      end

      def get() # :nodoc:
        val = super()
        val ? ocidate_to_time(val) : nil
      end
    end

    if OCI8.oracle_client_version >= ORAVER_9_0
      class DateTimeViaOCITimestamp < OCI8::BindType::OCITimestamp
        include OCI8::BindType::Util

        def set(val) # :nodoc:
          val &&= datetime_to_array(val, true)
          super(val)
        end

        def get() # :nodoc:
          val = super()
          val ? ocitimestamp_to_datetime(val) : nil
        end
      end

      class TimeViaOCITimestamp < OCI8::BindType::OCITimestamp
        include OCI8::BindType::Util

        def set(val) # :nodoc:
          val &&= datetime_to_array(val, true)
          super(val)
        end

        def get() # :nodoc:
          val = super()
          val ? ocitimestamp_to_time(val) : nil
        end
      end
    end


    #--
    # OCI8::BindType::DateTime
    #++
    # This is a helper class to bind ruby's
    # DateTime[http://www.ruby-doc.org/core/classes/DateTime.html]
    # object as Oracle's <tt>TIMESTAMP WITH TIME ZONE</tt> datatype.
    #
    # == Select
    #
    # The fetched value for a <tt>DATE</tt>, <tt>TIMESTAMP</tt>, <tt>TIMESTAMP WITH
    # TIME ZONE</tt> or <tt>TIMESTAMP WITH LOCAL TIME ZONE</tt> column
    # is a DateTime[http://www.ruby-doc.org/core/classes/DateTime.html].
    # The time zone part is a session time zone if the Oracle datatype doesn't
    # have time zone information. The session time zone is the client machine's
    # time zone by default.
    #
    # You can change the session time zone by executing the following SQL.
    #
    #   ALTER SESSION SET TIME_ZONE='-05:00'
    #
    # == Bind
    #
    # To bind a DateTime[http://www.ruby-doc.org/core/classes/DateTime.html]
    # value implicitly:
    #
    #   conn.exec("INSERT INTO lunar_landings(ship_name, landing_time) VALUES(:1, :2)",
    #             'Apollo 11',
    #             DateTime.parse('1969-7-20 20:17:40 00:00'))
    #
    # The bind variable <code>:2</code> is bound as <tt>TIMESTAMP WITH TIME ZONE</tt> on Oracle.
    #
    # To bind explicitly:
    #
    #   cursor = conn.exec("INSERT INTO lunar_landings(ship_name, landing_time) VALUES(:1, :2)")
    #   cursor.bind_param(':1', nil, String, 60)
    #   cursor.bind_param(':2', nil, DateTime)
    #   [['Apollo 11', DateTime.parse('1969-07-20 20:17:40 00:00'))],
    #    ['Apollo 12', DateTime.parse('1969-11-19 06:54:35 00:00'))],
    #    ['Apollo 14', DateTime.parse('1971-02-05 09:18:11 00:00'))],
    #    ['Apollo 15', DateTime.parse('1971-07-30 22:16:29 00:00'))],
    #    ['Apollo 16', DateTime.parse('1972-04-21 02:23:35 00:00'))],
    #    ['Apollo 17', DateTime.parse('1972-12-11 19:54:57 00:00'))]
    #   ].each do |ship_name, landing_time|
    #     cursor[':1'] = ship_name
    #     cursor[':2'] = landing_time
    #     cursor.exec
    #   end
    #   cursor.close
    #
    # On setting a object to the bind variable, you can use any object
    # which has at least three instance methods _year_, _mon_ (or _month_)
    # and _mday_ (or _day_). If the object responses to _hour_, _min_,
    # _sec_ or _sec_fraction_, the responsed values are used for hour,
    # minute, second or fraction of a second respectively.
    # If not, zeros are set. If the object responses to _offset_ or
    # _utc_offset_, it is used for time zone. If not, the session time
    # zone is used.
    #
    # The acceptable value are listed below.
    # _year_:: -4712 to 9999 [excluding year 0]
    # _mon_ (or _month_):: 0 to 12
    # _mday_ (or _day_):: 0 to 31 [depends on the month]
    # _hour_:: 0 to 23
    # _min_:: 0 to 59
    # _sec_:: 0 to 59
    # _sec_fraction_:: 0 to (999_999_999.to_r / (24*60*60* 1_000_000_000)) [999,999,999 nanoseconds]
    # _offset_:: (-12.to_r / 24) to (14.to_r / 24) [-12:00 to +14:00]
    # _utc_offset_:: -12*3600 <= utc_offset <= 24*3600 [-12:00 to +14:00]
    #
    # The output value of the bind varible is always a
    # DateTime[http://www.ruby-doc.org/core/classes/DateTime.html].
    #
    #   cursor = conn.exec("BEGIN :ts := current_timestamp; END")
    #   cursor.bind_param(:ts, nil, DateTime)
    #   cursor.exec
    #   cursor[:ts] # => a DateTime.
    #   cursor.close
    #
    class DateTime
      if OCI8.oracle_client_version >= ORAVER_9_0
        def self.create(con, val, param, max_array_size)
          DateTimeViaOCITimestamp.new(con, val, param, max_array_size)
        end
      else
        def self.create(con, val, param, max_array_size)
          DateTimeViaOCIDate.new(con, val, param, max_array_size)
        end
      end
    end

    class Time
      if OCI8.oracle_client_version >= ORAVER_9_0
        def self.create(con, val, param, max_array_size)
          TimeViaOCITimestamp.new(con, val, param, max_array_size)
        end
      else
        def self.create(con, val, param, max_array_size)
          TimeViaOCIDate.new(con, val, param, max_array_size)
        end
      end
    end

    if OCI8.oracle_client_version >= ORAVER_9_0
      #--
      # OCI8::BindType::IntervalYM
      #++
      #
      # This is a helper class to bind ruby's
      # Integer[http://www.ruby-doc.org/core/classes/Integer.html]
      # object as Oracle's <tt>INTERVAL YEAR TO MONTH</tt> datatype.
      #
      # == Select
      #
      # The fetched value for a <tt>INTERVAL YEAR TO MONTH</tt> column
      # is an Integer[http://www.ruby-doc.org/core/classes/Integer.html]
      # which means the months between two timestamps.
      #
      # == Bind
      #
      # You cannot bind as <tt>INTERVAL YEAR TO MONTH</tt> implicitly.
      # It must be bound explicitly with :interval_ym.
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
      # This is a helper class to bind ruby's
      # Rational[http://www.ruby-doc.org/core/classes/Rational.html]
      # object as Oracle's <tt>INTERVAL DAY TO SECOND</tt> datatype.
      #
      # == Select
      #
      # The fetched value for a <tt>INTERVAL DAY TO SECOND</tt> column
      # is a Rational[http://www.ruby-doc.org/core/classes/Rational.html]
      # or an Integer[http://www.ruby-doc.org/core/classes/Integer.html].
      # The value is usable to apply to
      # DateTime[http://www.ruby-doc.org/core/classes/DateTime.html]#+ and
      # DateTime[http://www.ruby-doc.org/core/classes/DateTime.html]#-.
      #
      # == Bind
      #
      # You cannot bind as <tt>INTERVAL YEAR TO MONTH</tt> implicitly.
      # It must be bound explicitly with :interval_ds.
      #
      #   # output
      #   ts1 = DateTime.parse('1969-11-19 06:54:35 00:00')
      #   ts2 = DateTime.parse('1969-07-20 20:17:40 00:00')
      #   cursor = conn.parse(<<-EOS)
      #     BEGIN
      #       :itv := (:ts1 - :ts2) DAY TO SECOND;
      #     END;
      #   EOS
      #   cursor.bind_param(:itv, nil, :interval_ds)
      #   cursor.bind_param(:ts1, ts1)
      #   cursor.bind_param(:ts2, ts2)
      #   cursor.exec
      #   cursor[:itv] # == ts1 - ts2
      #   cursor.close
      #
      #   # input
      #   ts2 = DateTime.parse('1969-07-20 20:17:40 00:00')
      #   itv = 121 + 10.to_r/24 + 36.to_r/(24*60) + 55.to_r/(24*60*60)
      #   # 121 days, 10 hours,    36 minutes,       55 seconds
      #   cursor = conn.parse(<<-EOS)
      #     BEGIN
      #       :ts1 := :ts2 + :itv;
      #     END;
      #   EOS
      #   cursor.bind_param(:ts1, nil, DateTime)
      #   cursor.bind_param(:ts2, ts2)
      #   cursor.bind_param(:itv, itv, :interval_ds)
      #   cursor.exec
      #   cursor[:ts1].strftime('%Y-%m-%d %H:%M:%S') # => 1969-11-19 06:54:35
      #   cursor.close
      #
      class IntervalDS < OCI8::BindType::OCIIntervalDS
        @@hour = 1 / 24.to_r
        @@minute = @@hour / 60
        @@sec = @@minute / 60
        @@fsec = @@sec / 1000000000

        def set(val) # :nodoc:
          unless val.nil?
            if val < 0
              is_minus = true
              val = -val
            else
              is_minus = false
            end
            day, val = val.divmod 1
            hour, val = (val * 24).divmod 1
            minute, val = (val * 60).divmod 1
            sec, val = (val * 60).divmod 1
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
          day + (hour * @@hour) + (minute * @@minute) + (sec * @@sec) + (fsec * @@fsec)
        end
      end # OCI8::BindType::IntervalDS
    end
  end # OCI8::BindType
end # OCI8
