require 'date'

class OCI8
  # Class representing an interval year to month.
  #
  #
  class IntervalYM
    def initialize(years, months = 0)
      case years
      when String
        unless arg[0] =~ /^([+-]?)(\d+)-(\d+)$/
          raise ArgumentError, 'invalid interval'
        end
        @months = $2.to_i * 12 + $3.to_i
        if $1 == '-'
          @months = -@months
        end
      when IntervalYM
        @months = years.to_i
      else
        @months = years * 12 + months
      end
    end
    def to_i
      @months
    end
    def to_a
      ary = @months.abs.divmod 12
      ary.collect! do |x| -x; end if @months < 0
      ary
    end
    def to_s
      y, m = @months.abs.divmod 12
      format('%c%02d-%02d', @months >= 0 ? ?+ : ?-, y, m)
    end
    def inspect
      y, m = @months.abs.divmod 12
      format('#<%s: %c%02d-%02d>', self.class, @months >= 0 ? ?+ : ?-, y, m)
    end
  end

  class IntervalDS
    def initialize(days, hours = 0, minutes = 0, seconds = 0, fseconds = 0)
      case days
      when String
        unless arg[0] =~ /^([+-]?)(\d+) (\d+):(\d+):(\d+)(?:\.(\d+))?$/
          raise ArgumentError, 'invalid interval'
        end
        days = $2.to_i
        hours = $3.to_i
        minutes = $4.to_i
        seconds = $5.to_i
        if $6.nil?
          fseconds = 0
        else
          fseconds = $6.to_i * 10 ** (9 - $6.length)
        end
        if $1 == '-'
          days = -days
          hours = -hours
          minutes = -minutes
          seconds = -seconds
          fseconds = -fseconds
        end
      when IntervalDS
        days = days.to_r
        hours = minutes = seconds = fseconds = 0
      end
      @days = days.to_r + hours.to_r / 24 + minutes.to_r / 1440 + seconds.to_r / 86400 + fseconds.to_r / 86400000000000
    end
    def to_r
      @days
    end
    def to_a
      v = @days.abs
      days, v = v.divmod 1
      v *= 24
      hours, v = v.divmod 1
      v *= 60
      minutes, v = v.divmod 1
      v *= 60
      seconds, v = v.divmod 1
      v *= 1000000000
      fseconds, v = v.divmod 1
      if @days >= 0
        [days, hours, minutes, seconds, fseconds]
      else
        [-days, -hours, -minutes, -seconds, -fseconds]
      end
    end
    def to_s
      v = @days.abs
      days, v = v.divmod 1
      v *= 24
      hours, v = v.divmod 1
      v *= 60
      minutes, v = v.divmod 1
      v *= 60
      seconds, v = v.divmod 1
      v *= 1000000000
      fseconds, v = v.divmod 1
      format('%c%02d %02d:%02d:%02d.%09d', @days >= 0 ? ?+ : ?-, days, hours, minutes, seconds, fseconds)
    end
    def inspect
      v = @days.abs
      days, v = v.divmod 1
      v *= 24
      hours, v = v.divmod 1
      v *= 60
      minutes, v = v.divmod 1
      v *= 60
      seconds, v = v.divmod 1
      v *= 1000000000
      fseconds, v = v.divmod 1
      format('#<%s: %c%02d %02d:%02d:%02d.%09d>', self.class, @days >= 0 ? ?+ : ?-, days, hours, minutes, seconds, fseconds)
    end
  end
end

# :stopdoc:
class Date
  alias :pre_add_interval :+
  def + (n)
    case n
    when OCI8::IntervalYM
      self >> n.to_i
    when OCI8::IntervalDS
      pre_add_interval(n.to_r)
    else
      pre_add_interval(n)
    end
  end
  alias :pre_sub_interval :-
  def - (n)
    case n
    when OCI8::IntervalYM
      self << n.to_i
    when OCI8::IntervalDS
      pre_sub_interval(n.to_r)
    else
      pre_sub_interval(n)
    end
  end
end
# :startdoc:
