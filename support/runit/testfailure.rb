module RUNIT
  SKIP_FILES = $:.collect{|path| /#{path}\/runit\//}.push /\/rubyunit\.rb:/

  class TestFailure
    attr_reader :at, :err
    def initialize(at, err, testclass)
      skip_trace = at.reject{|i|
        SKIP_FILES.find {|pat|
          pat =~ i
	}
      }
      @at = insert_class_name(skip_trace, testclass)
      @err = err
    end
    def to_s
      "#{@at[0]} #{@err}(#{@err.type})"
    end
    def insert_class_name(last_exception_backtrace, testclass)
      tns = testclass.inspect.split("::")
      last_exception_backtrace[0] += "(" + tns[tns.length-1] + ")"
      last_exception_backtrace
    end
    private :insert_class_name
  end
end
