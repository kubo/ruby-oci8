require 'runit/version'
require 'runit/error'

module RUNIT
  module Assert
    @@run_asserts = 0
    @@skip_failure = false

    def Assert.skip_failure=(arg)
      @@skip_failure = arg
    end

    def Assert.skip_failure?
      @@skip_failure
    end

    def Assert.run_asserts
      @@run_asserts
    end
   
    def edit_message(msg)
      (msg != "")? msg + " " : msg
    end
    private :edit_message

    def build_message(msg, *args, &proc)
      str_args = args.collect {|arg|
	to_string(arg)
      }
      if block_given?
        edit_message(msg).concat proc.call(*str_args)
      else
        msg
      end
    end
    private :build_message
	
    def to_string(obj)
      case obj
      when Symbol
	obj.id2name
      when String
	obj
      else
	obj.inspect
      end
    end
    private :to_string

    def float_to_str(f, e)
      return f.to_s if e == 0
      i = 0
      while 1 > e
	i += 1
	e *= 10
      end
      i += 1
      sprintf("%.#{i}f", f)
    end
    private :float_to_str

    def raise_assertion_error(message)
      if Assert.skip_failure?
        changed
        err = AssertionFailedError.new(message)
        err.set_backtrace(caller)
        notify_observers(RObserver::ADD_FAILURE, caller, err, type) 
      else
        raise AssertionFailedError, message
      end
    end
    private :raise_assertion_error

    def setup_assert
      @@run_asserts += 1
    end

    def assert(condition, message="")
      setup_assert
      if !condition.instance_of?(TrueClass) && !condition.instance_of?(FalseClass)
	raise TypeError, "1st argument <#{condition}> type should be TrueClass or FalseClass."
      end
      if !condition
	msg = build_message(message, condition) {|arg|
	  "The condition is <#{arg}:#{condition.type.inspect}>"
	}
	raise_assertion_error(msg)
      end
    end

    def assert_equal(expected, actual, message="")
      setup_assert
      if expected != actual
	msg = build_message(message, expected, actual) {|arg1, arg2|
	  "expected:<#{arg1}> but was:<#{arg2}>"
	}
	raise_assertion_error(msg)
      end
    end
    alias assert_equals assert_equal

    def assert_operator(obj1, op, obj2, message="")
      setup_assert
      if !obj1.send(op, obj2)
	msg = build_message(message, obj1, obj2) {|arg1, arg2|
	  "The condition is not satisfied: #{arg1} #{op.to_s} #{arg2}"
	}
	raise_assertion_error(msg)
      end
    end

    def assert_equal_float(expected, actual, e, message="")
      setup_assert
      if e < 0.0
        raise ArgumentError, "#{e}: 3rd argument should be 0 or greater than 0."
      end 
      if (expected - actual).abs > e
	msg = build_message(message) {
	  "expected:<#{float_to_str(expected, e)}> but was:<#{float_to_str(actual, e)}>"
	}
	raise_assertion_error(msg)
      end
    end

    def assert_same(expected, actual, message="")
      setup_assert
      if !actual.equal?(expected)
	msg = build_message(message, actual, expected) {|arg1, arg2|
	  "<#{arg1}:#{actual.type.inspect}> is not same object: <#{arg2}:#{expected.type.inspect}>"
	}
	raise_assertion_error(msg)
      end
    end

    def assert_send(obj, method, *args)
      setup_assert
      if !obj.send(method, *args) 
	msg = "Assertion failed: #{obj.type.inspect}(#{obj.inspect})##{to_string(method)}"
	if args.size > 0
	  strargs = args.collect {|arg|
	    "<#{to_string(arg).inspect}:#{arg.type.inspect}>"
	  }.join(", ")
	  msg.concat "(#{strargs})"
	end
	raise_assertion_error(msg)
      end
    end

    def assert_nil(obj, message="")
      setup_assert
      if !obj.nil?
	msg = build_message(message, obj) {|arg|
	  "<#{arg}:#{obj.type.inspect}> is not nil"
	}
	raise_assertion_error(msg)
      end
    end

    def assert_not_nil(obj, message="")
      setup_assert
      if obj.nil?
	msg = build_message(message) {
	  "object is nil"
	}
	raise_assertion_error(msg)
      end
    end

    def assert_respond_to(method, obj, message="")
      setup_assert
      if !obj.respond_to?(method)
	msg = build_message(message, obj, method) {|arg1, arg2|
	  "<#{arg1}:#{obj.type.inspect}> does not respond to <#{arg2}>"
	}
	raise_assertion_error(msg)
      end
    end

    def assert_kind_of(c, obj, message="")
      setup_assert
      if !obj.kind_of?(c)
	msg = build_message(message, obj, c) {|arg1, arg2|
	  "<#{arg1}:#{obj.type.inspect}> is not kind of <#{arg2}>"
	}
	raise_assertion_error(msg)
      end
    end

    def assert_instance_of(c, obj, message="")
      setup_assert
      if !obj.instance_of?(c)
	msg = build_message(message, obj, c) {|arg1, arg2|
	  "<#{arg1}:#{obj.type.inspect}> is not instance of <#{arg2}>"
	}
	raise_assertion_error(msg)
      end
    end

    def assert_match(str, re, message="")
      setup_assert
      if re !~ str
	msg = build_message(message, re, str) {|arg1, arg2|
	  "<#{arg1}> not match <#{arg2}>"
	}
	raise_assertion_error(msg)
      end
      return Regexp.last_match
    end
    alias assert_matches assert_match

    def assert_not_match(str, re, message="")
      setup_assert
      if re =~ str
	match = Regexp.last_match
	msg = build_message(message, re, str) {|arg1, arg2|
	  "<#{arg1}> matches '#{match[0]}' of <#{arg2}>"
	}
	raise_assertion_error(msg)
      end
    end

    def assert_exception(exception, message="")
      setup_assert
      exception_raised = true
      err = ""
      ret = nil
      begin
	yield
	exception_raised = false
	err = 'NO EXCEPTION RAISED'
      rescue Exception
	if $!.instance_of?(exception)
	  exception_raised = true
	  ret = $!
	else
	  exception_raised = false
	  err = $!.type
	end
      end
      if !exception_raised
	msg = build_message(message, exception, err) {|arg1, arg2|
	  "expected:<#{arg1}> but was:<#{arg2}>"
	}
	raise_assertion_error(msg)
      end
      ret
    end

    def assert_no_exception(*arg)
      setup_assert
      message = ""
      if arg[arg.size-1].instance_of?(String)
	message = arg.pop
      end
      err = nil
      exception_raised = false
      begin
	yield
      rescue Exception
	if arg.include?($!.type) || arg.size == 0
	  exception_raised = true
	  err = $!
	else
	  raise $!.type, $!.message, $!.backtrace
	end
      end
      if exception_raised 
	msg = build_message(message, err) {|arg|
	  "Exception raised:#{arg}"
	}
	raise_assertion_error(msg)
      end
    end

    def assert_fail(message)
      setup_assert
      raise_assertion_error message
    end

  end
end
