require 'observer'
require 'runit/testsuite'
require 'runit/robserver'
require 'runit/error'
require 'runit/assert'
require 'runit/setuppable'
require 'runit/teardownable'

module RUNIT
  class TestCase
    alias_method :extend_test, :extend

    include Assert
    include Observable

    include Setuppable
    extend Setuppable
    include Teardownable
    extend Teardownable

    @@test_classes = []

    def initialize(method, name=self.class.name)
      @method = method
      @name = name
    end

    def setup
    end
    private :setup

    def teardown
    end
    private :teardown

    def name
      @name + "#" +  @method
    end

    def method_name
      @method
    end

    def count_test_cases
      1
    end

    def run(result)
      result.start_test(self)
      begin
        run_bare
      rescue AssertionFailedError
        result.add_failure($@, $!, self.class)
      rescue StandardError, ScriptError
        result.add_error($@, $!, self.class)
      end
      result.end_test(self)
    end
  
    def run_bare
      setup
      invoke_setup(@method)
      begin 
        send(@method)
      ensure
        begin
          invoke_teardown(@method)
          teardown
        rescue
          changed
          notify_observers(RObserver::ADD_ERROR, $@, $!, self.class)
        end
      end
    end
        
    def ==(other)
      other.class == self.class && other.method_name && self.method_name
    end

    def TestCase.suite
      TestSuite.new(self_test_cases)
    end

    def TestCase.inherited(sub)
      @@test_classes.push sub
    end

    def TestCase.self_test_cases
      instance_methods(true).sort.collect {|m|
	new(m, name) if /^test/ =~ m
      }.compact
    end

    def TestCase.test_cases
      if self == TestCase
        TestSuite.new(*@@test_classes)
      else
        suite
      end
    end


    class<<TestCase
      alias test_methods test_cases
      alias all_suite test_cases
      private :self_test_cases
    end

    def TestCase.test_classes
      @@test_classes
    end
  end
end
