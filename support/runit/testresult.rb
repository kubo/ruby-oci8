require 'observer'
require 'runit/error'
require 'runit/robserver'
require 'runit/testfailure'

module RUNIT
  class TestResultItem
    attr_reader :name
    attr_reader :failures
    attr_reader :errors
    attr_accessor :run_asserts

    def initialize(name)
      @failures = []
      @errors = []
      @name = name
      @run_asserts = 0
    end

    def add_failure(failure)
      @failures.push failure
    end

    def add_error(error)
      @errors.push error
    end
  end

  class TestResult
    include Observable
    include RObserver

    def initialize
      @result = nil
      @results = []
      @test_start_time = nil
      @test_end_time = nil
      @start_assert_size = 0
    end

    def items
      @results
    end
  
    def run_tests
      @results.size
    end

    def add_failure(at, err, testclass)
      fail = TestFailure.new(at, err, testclass)
      @result.add_failure(fail)
      changed
      notify_observers(RObserver::ADD_FAILURE, at, err)
    end
  
    def add_error(at, err, testclass)
      error = TestFailure.new(at, err, testclass)
      @result.add_error(error)
      changed
      notify_observers(RObserver::ADD_ERROR, at, err)
    end
  
    def assert_size
      size = 0
      @results.each do |i|
        size += i.run_asserts
      end
      size
    end
    alias run_asserts assert_size

    def failure_size
      failures.size
    end

    def error_size
      errors.size
    end

    def failures
      @results.collect{|r|
	r.failures
      }.flatten
    end

    def errors
      @results.collect{|r|
        r.errors
      }.flatten
    end

    def succeed?
      failure_size == 0 && error_size == 0
    end

    def running_time
      if @test_end_time && @test_start_time
        @test_end_time - @test_start_time
      else
        0
      end
    end
  
    def start_test(testcase)
      testcase.add_observer(self)
      @result = TestResultItem.new(testcase.name)
      @results.push @result
      @start_assert_size = Assert.run_asserts
      @test_start_time = Time.now if !@test_start_time
      changed
      notify_observers(RObserver::START_TEST, testcase)
    end
  
    def end_test(testcase)
      changed
      notify_observers(RObserver::END_TEST, testcase)
      @test_end_time = Time.now
      @result.run_asserts = Assert.run_asserts - @start_assert_size
    end
  end
end  
