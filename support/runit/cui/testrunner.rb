require 'runit/testresult'
require 'runit/robserver'

module RUNIT
  module CUI
    class TestRunner
      include RObserver
      @@quiet_mode = false
      def initialize(io=STDERR, wait=false)
        @io = io
        @wait = wait
      end
  
      def TestRunner.quiet_mode=(flag)
        @@quiet_mode = flag
      end
     
      def TestRunner.run(test, io=STDERR, wait = false)
        r = TestRunner.new(io, wait)
        r.run(test)
      end
  
      def run(test)
        result = RUNIT::TestResult.new
        result.add_observer(self)
        test.run(result)
	print_running_time(result.running_time)
        print_result(result, test.count_test_cases)
	print_waiting_message
	result
      end

      def print_waiting_message
        if @wait
          STDERR.print "<RETURN> to Continue "
          STDIN.gets
        end
      end

      def print_running_time(rtm)
        @io.print "\nTime: #{rtm}\n"
      end

      def start_test(t)
        @io.print "\n", t.name ,  " " if !@@quiet_mode
      end
  
      def add_failure(at, err)
        @io.print("F")
      end
  
      def add_error(at, err)
        @io.print("E")
      end
  
      def end_test(t)
        @io.print(".")
      end
  
      def print_result(result, count)
        print_header(result, count)
        print_failures(result)
        print_errors(result)
      end
  
      def print_header(result, count)
        if result.succeed?
          @io.puts "OK (#{result.run_tests}/#{count} tests  #{result.run_asserts} asserts)"
        else
          @io.puts "FAILURES!!!"
          @io.puts "Test Results:"
          @io.print " Run: #{result.run_tests}/#{count}(#{result.run_asserts} asserts)"
          @io.print " Failures: #{result.failure_size}"
          @io.puts  " Errors: #{result.error_size}"
        end
      end
    
      def print_failures(result)
        @io.puts "Failures: #{result.failure_size}" if result.failure_size > 0
        result.failures.each do |f|
	  @io.print f.at[0], ": ", f.err, " (", f.err.class, ")\n"
	  for at in f.at[1..-1]
	    @io.print "\tfrom ", at, "\n"
	  end
        end
      end
    
      def print_errors(result)
        @io.puts "Errors: #{result.error_size}" if result.error_size > 0
        result.errors.each do |e|
	  @io.print e.at[0], ": ", e.err, " (", e.err.class, ")\n"
	  for at in e.at[1..-1]
	    @io.print "\tfrom ", at, "\n"
	  end
        end
      end

    end
  end
end

