module RUNIT
  class TestSuite < Array
    
    def initialize(*test_classes)
      test_classes.each do |k|
        if k.kind_of?(Array)
          concat k
        else
          concat k.suite
        end
      end
    end

    def run(result)
      each do |t|
        t.run(result)
      end
    end

    def count_test_cases
      sum = 0
      each do |test|
	sum += test.count_test_cases
      end
      sum
    end

    def add(test)
      if test.kind_of?(Array) 
	concat test
      else
	push test
      end
    end
    alias add_test add

    def extend_test(*mod)
      each do |t|
        t.extend_test(*mod)
      end
    end
  end
end
